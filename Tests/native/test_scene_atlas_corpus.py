"""Compare every scene sheet and local view against original source pixels."""
from __future__ import annotations

import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
import unittest
import zipfile

from PIL import Image
from atlas_uv_assertions import regions, assert_regions

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import build_numeric_runtime_pak64 as pak
import resource_build_config
import runtime_asset_sources
import scene_atlas as scene

PACKAGE = Path(os.environ.get(
    "PVZ_RESOURCE_PACKAGE", ROOT / "out/x64-win32-vulkan/cache/resources/main.pak64"))


def original_pixels(descriptor, files):
    """Independent Pillow oracle for the full image and blue-channel masks."""
    def read(name):
        key = name.casefold()
        candidates = [key] if PurePosixPath(key).suffix else [
            key + suffix for suffix in (".tga", ".jpg", ".png", ".gif")]
        for candidate in candidates:
            if candidate in files:
                with Image.open(files[candidate]) as decoded:
                    return decoded.convert("RGBA")
        return None

    image = read(descriptor.path)
    if descriptor.flags & 1024 and not (descriptor.alpha or descriptor.alpha_grid):
        path = PurePosixPath(descriptor.path)
        alpha = read(str(path.with_name("_" + path.name)))
        if alpha is None:
            alpha = read(str(path.with_name(path.stem + "_" + path.suffix)))
        if alpha is not None:
            if image is None:
                color = descriptor.color
                image = Image.new("RGBA", alpha.size,
                                  ((color >> 16) & 255, (color >> 8) & 255, color & 255, 255))
            if image.size != alpha.size:
                raise AssertionError("Original alpha mask dimensions differ")
            image.putalpha(alpha.getchannel("B"))
    if image is None:
        raise AssertionError("Original scene image is missing: " + descriptor.path)
    for name in (descriptor.alpha, descriptor.alpha_grid):
        if name:
            alpha = read(name)
            if alpha is None or alpha.size != image.size:
                raise AssertionError("Scene alpha mask must cover the complete source image")
            image.putalpha(alpha.getchannel("B"))
    return image


class SceneAtlasCorpusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report = json.loads(PACKAGE.with_name("manifest.json").read_text("utf-8"))
        cls.scenes = cls.report["scene_atlases"]
        cls.config = resource_build_config.load(ROOT / cls.report["build_config"])
        cls.files = runtime_asset_sources.inventory(ROOT)
        cls.modules, cls.sources = scene.prepare(
            [scene.parse(path.read_text("utf-8-sig")) for path in cls.config.sources["scene"]]
            + [scene.common_ui()], (ROOT / "assets/definitions/resources.json").read_bytes(), cls.files)

    def test_exact_scene_membership_and_native_static_rectangles(self):
        self.assertEqual(set(self.scenes), set(self.modules))
        self.assertEqual(set(self.scenes), {scene.record_path(module) for module in scene.MODULES})
        generated = ROOT / "pvzPerf/Sexy.TodLib/generated"
        native = (generated / "SceneAtlasData.cpp").read_text("utf-8")
        header = (generated / "SceneAtlasData.h").read_text("utf-8")
        for name, document in self.modules.items():
            report = self.scenes[name]
            self.assertEqual([row["name"] for row in report["sprites"]],
                             [row["name"] for row in document["sprites"]])
            self.assertEqual({row["source"] for row in report["images"]},
                             {row["source"] for row in document["sprites"]})
            self.assertEqual(len(report["images"]), len({row["source"] for row in report["images"]}))
            block = re.search(
                r"namespace " + document["name"] + r"Atlas \{(.*?)"
                r"(?=\nnamespace \w+Atlas \{|\nvoid LoadSceneAtlases\()", native, re.S)
            self.assertIsNotNone(block)
            array = re.search(r"const Sexy::TextureRegion Images\[Count\] = \{(.*?)\n\};", block[1], re.S)
            self.assertIsNotNone(array)
            assert_regions(self, regions(array[1]), [(row["region"], report["atlas_width"],
                           report["atlas_height"]) for row in report["sprites"]])
            self.assertIn(report["atlas_path"], block[1])
            self.assertIn(f"DefinitionBindAtlasPath(Texture, {report['atlas_width']}, {report['atlas_height']}", block[1])
            self.assertIn("DefinitionUnbindAtlas(Texture)", block[1])
        self.assertNotRegex(header + native, r"ImageSymbolId|mNumRows|mNumCols|GetCel|\.compiled")
        self.assertEqual([path.name for path in self.config.sources["scene"]],
                         sorted(path.name for path in self.config.sources["scene"]))

    def test_every_full_sheet_and_view_preserves_original_source_pixels(self):
        with zipfile.ZipFile(PACKAGE) as archive:
            for name, report in self.scenes.items():
                with Image.open(io.BytesIO(archive.read(report["atlas_path"]))) as decoded:
                    self.assertEqual((decoded.format, decoded.mode), ("PNG", "RGBA"))
                    atlas = decoded.convert("RGBA")
                self.assertEqual(atlas.size, (report["atlas_width"], report["atlas_height"]))
                sheets = {row["source"]: row for row in report["images"]}
                original = {}
                for key, row in sheets.items():
                    expected = original_pixels(self.sources[key], self.files)
                    original[key] = expected
                    x, y, width, height = row["region"]
                    self.assertEqual((width, height), expected.size)
                    self.assertEqual((row["rows"], row["cols"]), (1, 1))
                    self.assertEqual(atlas.crop((x, y, x+width, y+height)).tobytes(), expected.tobytes())
                    self.assertEqual(row["rgba_sha256"], hashlib.sha256(expected.tobytes()).hexdigest())
                    self.assertEqual(row["rgba_bytes"], expected.width * expected.height * 4)
                for authored, row in zip(self.modules[name]["sprites"], report["sprites"]):
                    expected = original[row["source"]]
                    local = authored.get("rect", [0, 0, expected.width, expected.height])
                    self.assertEqual(row["source_rect"], local)
                    sx, sy, width, height = local
                    ox, oy = sheets[row["source"]]["region"][:2]
                    self.assertEqual(row["region"], [ox+sx, oy+sy, width, height])
                    self.assertEqual(atlas.crop((ox+sx, oy+sy, ox+sx+width, oy+sy+height)).tobytes(),
                                     expected.crop((sx, sy, sx+width, sy+height)).tobytes())
                    self.assertEqual(row["scale"], authored["scale"])

    def test_packaged_atlases_and_static_extent_loading_have_complete_closure(self):
        scene_extents, sheets = pak.read_scene_atlas_metadata(ROOT)
        expected = {row["atlas_path"]: (row["atlas_width"], row["atlas_height"])
                    for row in self.scenes.values()}
        self.assertEqual(scene_extents, expected)
        expected_sheets = {(report["atlas_path"], row["source"], tuple(row["region"]), row["rgba_sha256"])
                           for report in self.scenes.values() for row in report["images"]}
        self.assertEqual({(row["atlas"], row["source"], row["rect"], row["rgba_sha256"])
                          for row in sheets}, expected_sheets)
        with zipfile.ZipFile(PACKAGE) as archive:
            available = set(archive.namelist())
            self.assertFalse(any(path.startswith("compiled/scene/") for path in available))
            numeric = {path for path in available if pak.numeric_kind(path)}
            audit = pak.source_audit(ROOT, {}, numeric)
            self.assertEqual(audit["scene_atlases"], expected)
            self.assertEqual(set(audit["static_atlas_extents"]),
                             {path for path in available if path.endswith(".atlas.png")})
            self.assertEqual(len(audit["static_atlas_extents"]),
                             self.report["atlas_memory"]["definition_atlas_count"])


if __name__ == "__main__":
    unittest.main()
