"""Check every packaged atlas region against its offline source pixel digest."""
from __future__ import annotations

import hashlib
import io
import json
import os
from pathlib import Path
import re
import struct
import unittest
import zipfile
import sys

from PIL import Image
from atlas_uv_assertions import regions as uv_regions, assert_regions, REGION


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import build_numeric_runtime_pak64 as pak
import resource_build_config
PACKAGE = Path(os.environ.get(
    "PVZ_RESOURCE_PACKAGE", ROOT / "out/x64-win32-vulkan/cache/resources/main.pak64"))


class DefinitionAtlasCorpusTests(unittest.TestCase):
    def test_all_regions_preserve_source_pixels_and_grids(self):
        report = json.loads(PACKAGE.with_name("manifest.json").read_text(encoding="utf-8"))
        definitions = report["definition_atlases"]
        self.assertEqual(report["format"], "pvz-resource-build-static-v1")
        expected = {"reanim": 0, "particles": 0, "projectiles": 0, "fonts": 20, "resources": 1}
        config = resource_build_config.load(ROOT / report["build_config"])
        self.assertEqual(len(definitions), sum(len(config.definition_orders[kind])
                                               for kind in ("reanim", "particle")) + 2)
        regions_checked = 0
        physical_atlases = set()
        bindings = pak.read_definition_image_bindings(ROOT)
        variant_bindings = pak.read_definition_image_binding_rows(ROOT)
        checked_regions = set()
        generated = ROOT / "pvzPerf/Sexy.TodLib/generated"
        from test_particle_image_variants import ParticleVariantPackageTests
        ParticleVariantPackageTests.setUpClass()
        source_rectangles = {}
        for path in (generated / "definitions").glob("*Data*.cpp"):
            text = path.read_text(encoding="utf-8")
            for label, size, body in re.findall(
                    r"constinit const Sexy::TextureRegion (\w+)\[(\d+)\] = \{\n(.*?)\n\};", text, re.S):
                rectangles = uv_regions(body)
                self.assertEqual(len(rectangles), int(size), label)
                self.assertNotIn(label, source_rectangles)
                source_rectangles[label] = rectangles
        for kind, directory, prefix in (("reanim", "reanim", "Reanim"),
                                          ("particle", "particles", "Particle")):
            for index_, (_, name) in enumerate(config.definition_orders[kind]):
                definition = definitions[f"compiled/{directory}/{name.casefold()}.compiled"]
                expected_uv = [(rect, row["atlas_width"], row["atlas_height"])
                               for row in definition["variants"] for rect in row["frame_rectangles"]]
                if kind == "particle":
                    runtime_path = f"compiled/particle-runtime/{name.casefold()}.compiled"
                    _, rows = ParticleVariantPackageTests.runtime[runtime_path]
                    actual_uv = [uv for _,_,_,rects in rows for uv in rects]
                    self.assertNotIn(f"{prefix}{index_}Rects", source_rectangles)
                else:
                    actual_uv = source_rectangles.get(f"{prefix}{index_}Rects", [])
                assert_regions(self, actual_uv, expected_uv)
                self.assertTrue(all(len(row["frame_rectangles"]) == definition["slot_count"]
                                    for row in definition["variants"]))
        projectile_source = re.search(
            r"constinit ProjectileAtlasDefinition Projectiles = \{\n(.*?)\n\};",
            (generated / "GeneratedDefinitions.cpp").read_text(encoding="utf-8"), re.S)
        self.assertIsNotNone(projectile_source)
        projectile_lines = projectile_source[1].splitlines()
        projectile_report = definitions["compiled/projectiles/projectiles.compiled"]
        visuals = re.findall(r"\{(" + REGION + r"),([-+.\deE]+)f\}", projectile_lines[0])
        self.assertEqual(len(visuals), len(projectile_report["projectiles"]))
        for actual, expected_visual in zip(visuals, projectile_report["projectiles"]):
            assert_regions(self, uv_regions(actual[0]), [(expected_visual["rect"], projectile_report["atlas_width"], projectile_report["atlas_height"])])
            self.assertEqual(struct.pack("<f", float(actual[-1])), struct.pack("<f", expected_visual["scale"]))
        assert_regions(self, uv_regions(projectile_lines[1]), [(row["rect"], projectile_report["atlas_width"], projectile_report["atlas_height"])
                       for row in projectile_report["shadows"]])
        zombatar_report = definitions["compiled/zombatar/zombatar.compiled"]
        self.assertEqual(zombatar_report["region_count"], 209)
        zombatar_source = (generated / "ZombatarAtlasData.cpp").read_text("utf-8")
        assert_regions(self, uv_regions(zombatar_source), [(row["region"][:4], zombatar_report["atlas_width"], zombatar_report["atlas_height"])
                       for row in zombatar_report["images"]])
        atlas_images, owner_dimensions, shared_sources, source_metadata, owner_regions = {}, {}, {}, {}, {}
        with zipfile.ZipFile(PACKAGE) as archive:
            index = pak.archive_index(archive)
            available = {name for name, info in index.items() if not info.is_dir()}
            numeric = {name for name in available if pak.numeric_kind(name)}
            self.assertEqual({kind: sum(pak.numeric_kind(name) == kind for name in numeric)
                              for kind in expected}, expected)
            self.assertFalse({name for name in numeric
                              if pak.numeric_kind(name) in ("reanim", "particles", "projectiles")})
            self.assertEqual(set(bindings), set(definitions))
            self.assertEqual(set(report["files"]), available)
            rows = [(name, variant, row) for name, definition in definitions.items()
                    for variant, row in enumerate(definition.get("variants", [definition]))]
            rows.extend((name, 0, row) for name, row in report["scene_atlases"].items())
            for name, variant, definition in rows:
                with self.subTest(definition=name, variant=variant):
                    projectile = pak.numeric_kind(name) == "projectiles"
                    zombatar = name == "compiled/zombatar/zombatar.compiled"
                    scene = name in report["scene_atlases"]
                    width, height = definition["atlas_width"], definition["atlas_height"]
                    count = len(definition["images"])
                    if not scene:
                        self.assertEqual(definition["region_count"], len(definition["images"]))
                    if projectile:
                        self.assertNotIn("variants", definition)
                        self.assertEqual(definition["region_count"], 13)
                        self.assertEqual(len(definition["projectiles"]), 14)
                        self.assertEqual(len(definition["shadows"]), 2)
                        source_blocks = {image["source"]: tuple(image["region"][:4])
                                         for image in definition["images"]}
                        for slot, row in enumerate(definition["projectiles"]):
                            self.assertGreater(row["scale"], 0)
                            self.assertEqual(tuple(row["rect"]), source_blocks[row["image"]]
                                             if row["image"] else (0, 0, 0, 0))
                        for x, y, w, h in (row["rect"] for row in definition["shadows"]):
                            self.assertTrue(any(x >= ox and y >= oy and x + w <= ox + sw and y + h <= oy + sh
                                                for ox, oy, sw, sh in source_blocks.values()))
                        self.assertEqual(set(definition["source_files"]),
                                         {path for image in definition["images"] for path in image["files"]})
                    elif not zombatar and not scene:
                        regions = [image["region"][:4] for image in definition["images"]]
                        for x, y, w, h in definition["frame_rectangles"]:
                            if not w:
                                self.assertEqual((x, y, w, h), (0, 0, 0, 0))
                                continue
                            self.assertTrue(any(x >= ox and y >= oy and x + w <= ox + sw and y + h <= oy + sh
                                                for ox, oy, sw, sh in regions))
                        source_blocks = {image["source"]: image["region"] for image in definition["images"]}
                        for frame in definition["frames"]:
                            ox, oy, sw, sh, _, _ = source_blocks[frame["source"]]
                            sx, sy, fw, fh = frame["source_rect"]
                            self.assertGreaterEqual(sx, 0)
                            self.assertGreaterEqual(sy, 0)
                            self.assertGreater(fw, 0)
                            self.assertGreater(fh, 0)
                            self.assertLessEqual(sx + fw, sw)
                            self.assertLessEqual(sy + fh, sh)
                            self.assertEqual(tuple(frame["atlas_rect"]), (ox + sx, oy + sy, fw, fh))
                    atlas_name = definition["atlas_path"]
                    if not scene:
                        self.assertEqual(variant_bindings[name][variant], atlas_name)
                    if count == 0:
                        self.assertEqual(atlas_name, "")
                        self.assertEqual((width, height), (0, 0))
                        if not projectile and not zombatar and not scene:
                            self.assertTrue(all(not any(rect) for rect in definition["frame_rectangles"]))
                        continue
                    physical_atlases.add(atlas_name)
                    self.assertEqual(owner_dimensions.setdefault(atlas_name, (width, height)), (width, height))
                    if atlas_name not in atlas_images:
                        with Image.open(io.BytesIO(archive.read(index[atlas_name]))) as decoded:
                            self.assertEqual((decoded.format, decoded.mode), ("PNG", "RGBA"))
                            atlas_images[atlas_name] = decoded.convert("RGBA")
                    atlas = atlas_images[atlas_name]
                    self.assertEqual(atlas.size, (width, height))
                    for image in definition["images"]:
                        source = image["source"]
                        self.assertIsInstance(source, str)
                        self.assertTrue(source)
                        self.assertNotIn("symbol", image)
                        region = tuple(image["region"]) + ((1, 1) if scene else ())
                        x, y, w, h, rows, cols = region
                        self.assertEqual((w, h), (image["width"], image["height"]))
                        self.assertEqual((rows, cols), (image["rows"], image["cols"]))
                        metadata = (w, h, rows, cols, image["rgba_sha256"])
                        self.assertEqual(source_metadata.setdefault(source, metadata), metadata)
                        shared = (region, image["rgba_sha256"])
                        self.assertEqual(shared_sources.setdefault((atlas_name, source), shared), shared)
                        geometry = (x, y, w, h)
                        owner = owner_regions.setdefault(atlas_name, {})
                        self.assertEqual(owner.setdefault(geometry, image["rgba_sha256"]), image["rgba_sha256"])
                        self.assertGreater(rows, 0)
                        self.assertGreater(cols, 0)
                        self.assertGreaterEqual(x, 2)
                        self.assertGreaterEqual(y, 2)
                        self.assertLessEqual(x + w + 2, width)
                        self.assertLessEqual(y + h + 2, height)
                        identity = (atlas_name, geometry, source)
                        if identity in checked_regions:
                            continue
                        checked_regions.add(identity)
                        pixels = atlas.crop((x, y, x + w, y + h))
                        self.assertEqual(hashlib.sha256(pixels.tobytes()).hexdigest(),
                                         image["rgba_sha256"])
                        # Every pixel of the four two-pixel borders and corners
                        # must replicate the original logical image's RGBA edge.
                        for source_box, output_box in (
                            ((0, 0, w, 1), (x, y - 2, x + w, y)),
                            ((0, h - 1, w, h), (x, y + h, x + w, y + h + 2)),
                            ((0, 0, 1, h), (x - 2, y, x, y + h)),
                            ((w - 1, 0, w, h), (x + w, y, x + w + 2, y + h)),
                            ((0, 0, 1, 1), (x - 2, y - 2, x, y)),
                            ((w - 1, 0, w, 1), (x + w, y - 2, x + w + 2, y)),
                            ((0, h - 1, 1, h), (x - 2, y + h, x, y + h + 2)),
                            ((w - 1, h - 1, w, h), (x + w, y + h, x + w + 2, y + h + 2)),
                        ):
                            expected = pixels.crop(source_box).resize(
                                (output_box[2] - output_box[0], output_box[3] - output_box[1]),
                                Image.Resampling.NEAREST)
                            self.assertEqual(atlas.crop(output_box).tobytes(), expected.tobytes())
                        regions_checked += 1
            self.assertEqual({name for name in available if name.endswith(".atlas.png")}, physical_atlases)
        for path, owner in owner_regions.items():
            rectangles = []
            for x, y, w, h in owner:
                padded = (x - 2, y - 2, x + w + 2, y + h + 2)
                self.assertTrue(all(padded[2] <= old[0] or old[2] <= padded[0] or
                                    padded[3] <= old[1] or old[3] <= padded[1] for old in rectangles),
                                f"Cross-definition padded regions overlap in {path}")
                rectangles.append(padded)
        self.assertEqual(len(physical_atlases), report["atlas_memory"]["definition_atlas_count"])
        self.assertEqual(sum(width * height * 4 for width, height in owner_dimensions.values()),
                         report["atlas_memory"]["rgba_bytes"])
        self.assertGreater(regions_checked, 1000)


if __name__ == "__main__":
    unittest.main()
