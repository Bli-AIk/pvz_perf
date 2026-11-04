"""Whole-sheet scene packing, authored views, and native ownership contracts."""
from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import definition_atlas
import scene_atlas as scene
from static_definition_codegen import texture_region


SOURCE = '''
// Views may select arbitrary pixels; the original sheet remains intact.
export atlas Magnet {
    sprite IMAGE_First { image: "IMAGE_SHEET"; rect: [1, 1, 2, 3]; }
    sprite Second { image: "images/sheet.png"; rect: [5, 4, 3, 2]; scale: 0.6; }
    sprite Whole { image: "IMAGE_OTHER"; }
}
'''


class SceneAtlasTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.originals = {}
        self.files = {}
        for name, size in (("sheet", (8, 6)), ("other", (3, 2))):
            image = Image.new("RGBA", size)
            image.putdata([(17 + x * 19, 11 + y * 31, 9 + (x + y) * 13,
                            53 + (x * 23 + y * 17) % 203)
                           for y in range(size[1]) for x in range(size[0])])
            path = self.directory / (name + ".png")
            image.save(path)
            self.files["images/" + name + ".png"] = path
            self.originals[name] = image
        self.authoring = json.dumps({
            "format": "pvz-resources-authoring-v1",
            "manifest": {"tag": "ResourceManifest", "attrs": {}, "children": [
                {"tag": "Resources", "attrs": {"id": "Fixture"}, "children": [
                    {"tag": "Image", "attrs": {
                        "id": "IMAGE_SHEET", "path": "images/sheet", "rows": "2", "cols": "4"},
                     "children": []},
                    {"tag": "Image", "attrs": {
                        "id": "IMAGE_ALIAS", "path": "images/sheet", "rows": "3", "cols": "2"},
                     "children": []},
                    {"tag": "Image", "attrs": {
                        "id": "IMAGE_OTHER", "path": "images/other"}, "children": []},
                ]}]}}, separators=(",", ":")).encode()
        self.document = scene.parse(SOURCE)

    def prepare(self, documents=None):
        return scene.prepare(documents or [self.document], self.authoring, self.files)

    def packed(self, documents=None):
        modules, sources = self.prepare(documents)
        order = sorted(sources)
        logical = [definition_atlas.logical_image(sources[name], self.files)[0]
                   for name in order]
        layout = definition_atlas.shelf_layout([image.size for image in logical], 2)
        atlas, _ = definition_atlas.pack(logical, layout=layout)
        reports = {name: scene.compile_definition(
            document, sources, self.files, layout, order,
            "compiled/atlases/" + document["name"].lower() + ".atlas.png")
            for name, document in modules.items()}
        return atlas, reports, sources, layout, order

    def test_dsl_preserves_order_names_rectangles_and_scales(self):
        self.assertEqual(self.document["name"], "Magnet")
        self.assertEqual([row["name"] for row in self.document["sprites"]],
                         ["First", "Second", "Whole"])
        self.assertEqual(self.document["sprites"][0]["rect"], [1, 1, 2, 3])
        self.assertEqual([row["scale"] for row in self.document["sprites"]], [1, 0.6, 1])

    def test_duplicate_sprites_and_duplicate_fields_are_rejected(self):
        for text in (
            SOURCE.replace("sprite Second", "sprite First"),
            SOURCE.replace('image: "IMAGE_SHEET";',
                           'image: "IMAGE_SHEET"; image: "IMAGE_OTHER";'),
            SOURCE.replace("rect: [1, 1, 2, 3];",
                           "rect: [1, 1, 2, 3]; rect: [0, 0, 1, 1];"),
        ):
            with self.subTest(text=text), self.assertRaises(ValueError):
                scene.parse(text)

    def test_invalid_paths_syntax_and_rectangles_are_rejected(self):
        for path in ("../sheet.png", "/images/sheet.png", "C:/sheet.png",
                     "images/../sheet.png", "images/sheet.txt", "images//sheet.png"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                scene.parse(SOURCE.replace('"images/sheet.png"', json.dumps(path)))
        for rect in ("[-1, 0, 2, 3]", "[0, -1, 2, 3]", "[0, 0, 0, 3]",
                     "[0, 0, 2, -3]", "[0, 0, 2.5, 3]", "[0, 0, 2]"):
            with self.subTest(rect=rect), self.assertRaises(ValueError):
                scene.parse(SOURCE.replace("[1, 1, 2, 3]", rect))
        for text in (SOURCE + "trailing", SOURCE.replace("0.6", "0"),
                     SOURCE.replace("0.6", "-1"), SOURCE.replace("0.6", "1e999"),
                     SOURCE.replace("atlas Magnet", "atlas CommonUI"),
                     SOURCE.replace("image:", "rows:", 1)):
            with self.subTest(text=text), self.assertRaises(ValueError):
                scene.parse(text)

    def test_ui_scale_requires_an_explicit_layout_consumer(self):
        with self.assertRaisesRegex(ValueError, "Per-sprite scale"):
            scene.parse('export atlas Almanac { sprite Card { image: "images/card.png"; scale: 2; } }')

    def test_author_labels_paths_and_grid_aliases_deduplicate_one_full_sheet(self):
        document = copy.deepcopy(self.document)
        document["sprites"].append({"name": "Alias", "image": "IMAGE_ALIAS", "scale": 1})
        modules, sources = self.prepare([document])
        rows = modules[scene.record_path("Magnet")]["sprites"]
        self.assertEqual(len(sources), 2)
        self.assertEqual(rows[0]["source"], rows[1]["source"])
        self.assertEqual(rows[0]["source"], rows[3]["source"])
        self.assertTrue(all((descriptor.rows, descriptor.cols) == (1, 1)
                            for descriptor in sources.values()))

    def test_unknown_author_labels_and_duplicate_modules_are_rejected(self):
        changed = copy.deepcopy(self.document)
        changed["sprites"][0]["image"] = "IMAGE_UNKNOWN"
        with self.assertRaises(ValueError):
            self.prepare([changed])
        with self.assertRaises(ValueError):
            self.prepare([self.document, copy.deepcopy(self.document)])

    def test_same_path_conflicting_decode_recipes_are_not_silently_deduplicated(self):
        authoring = json.loads(self.authoring)
        authoring["manifest"]["children"][0]["children"][1]["attrs"]["alphacolor"] = "123456"
        self.authoring = json.dumps(authoring).encode()
        with self.assertRaisesRegex(ValueError, "Ambiguous|Conflicting"):
            self.prepare()
        document = copy.deepcopy(self.document)
        document["sprites"][1]["image"] = "IMAGE_ALIAS"
        with self.assertRaisesRegex(ValueError, "Conflicting"):
            self.prepare([document])

    def test_missing_source_and_changed_full_sheet_extents_are_rejected(self):
        modules, sources = self.prepare()
        order = sorted(sources)
        layout = definition_atlas.shelf_layout([(3, 2), (8, 6)], 2)
        with self.assertRaises((ValueError, KeyError)):
            scene.compile_definition(modules[scene.record_path("Magnet")],
                                     sources, {}, layout, order, "atlas.png")
        _, reports, sources, layout, order = self.packed()
        changed = copy.deepcopy(layout)
        x, y, width, height = changed["regions"][0]
        changed["regions"][0] = (x, y, width + 1, height)
        with self.assertRaises(ValueError):
            scene.compile_definition(modules[scene.record_path("Magnet")],
                                     sources, self.files, changed, order, "atlas.png")

    def test_views_outside_source_dimensions_are_rejected(self):
        for rectangle in ([7, 0, 2, 1], [0, 5, 1, 2], [0, 0, 9, 6]):
            document = copy.deepcopy(self.document)
            document["sprites"][0]["rect"] = rectangle
            with self.subTest(rectangle=rectangle), self.assertRaisesRegex(ValueError, "outside"):
                self.packed([document])

    def test_packed_sources_keep_every_original_pixel_including_unreferenced_areas(self):
        atlas, reports, sources, _, _ = self.packed()
        report = reports[scene.record_path("Magnet")]
        self.assertEqual(len(report["images"]), 2)
        for row in report["images"]:
            expected = self.originals[Path(sources[row["source"]].path).name]
            x, y, width, height = row["region"]
            self.assertEqual((width, height), expected.size)
            self.assertEqual(atlas.crop((x, y, x + width, y + height)).tobytes(),
                             expected.tobytes())
            self.assertEqual(row["rgba_sha256"], hashlib.sha256(expected.tobytes()).hexdigest())
            self.assertEqual(row["rgba_bytes"], width * height * 4)
            self.assertEqual(row["files"], [sources[row["source"]].path + ".png"])
        # The two views occupy 12 of the 48 sheet pixels; all 48 are packed.
        sheet = next(row for row in report["images"] if row["width"] == 8)
        self.assertEqual(sheet["rgba_bytes"], 48 * 4)

    def test_view_regions_add_packed_origin_without_changing_local_pixels(self):
        atlas, reports, sources, _, _ = self.packed()
        report = reports[scene.record_path("Magnet")]
        originals = {row["source"]: row for row in report["images"]}
        for row in report["sprites"]:
            x, y, width, height = row["source_rect"]
            ox, oy, _, _ = originals[row["source"]]["region"]
            self.assertGreater(ox, 0)
            self.assertGreater(oy, 0)
            self.assertEqual(row["region"], [ox + x, oy + y, width, height])
            original = self.originals[Path(sources[row["source"]].path).name]
            self.assertEqual(atlas.crop((ox+x, oy+y, ox+x+width, oy+y+height)).tobytes(),
                             original.crop((x, y, x+width, y+height)).tobytes())

    def test_generated_native_geometry_and_reverse_release_match_all_four_modules(self):
        documents = []
        for module in scene.MODULES:
            document = copy.deepcopy(self.document)
            document["name"] = module
            documents.append(document)
        _, reports, _, _, _ = self.packed(documents)
        generated = scene.generate(reports)
        self.assertEqual(set(generated), {"SceneAtlasData.h", "SceneAtlasData.cpp",
                                         "SceneAtlasBindings.h"})
        header = generated["SceneAtlasData.h"].decode()
        native = generated["SceneAtlasData.cpp"].decode()
        for module in scene.MODULES:
            report = reports[scene.record_path(module)]
            self.assertIn("namespace " + module + "Atlas", header)
            for index, row in enumerate(report["sprites"]):
                self.assertIn(f'{row["name"]} = {index},', header)
                self.assertIn(texture_region(row["region"], report["atlas_width"],
                                             report["atlas_height"]), native)
            self.assertIn(report["atlas_path"], native)
        self.assertIn("extern Sexy::GpuTexture* Texture", header)
        self.assertIn("extern const Sexy::TextureRegion Images[Count]", header)
        self.assertNotIn("inline constexpr int Width", header)
        for report in reports.values():
            self.assertIn(f'DefinitionBindAtlasPath(Texture, {report["atlas_width"]}, '
                          f'{report["atlas_height"]},', native)
        self.assertIn("DefinitionUnbindAtlas(Texture)", native)
        self.assertIn("void LoadSceneAtlases(bool landscapeUI)", native)
        self.assertIn("if (landscapeUI) LandscapeUIAtlas::Load();", native)
        self.assertIn("else DesktopUIAtlas::Load();", native)
        free_body = native.split("void FreeSceneAtlases() {", 1)[1].split("}", 1)[0]
        self.assertEqual(re.findall(r"(\w+)Atlas::Free\(\)", free_body), list(reversed(scene.MODULES)))
        self.assertNotRegex(header + native, r"ImageSymbolId|ResourceId|mNumRows|mNumCols|rows|cols|cel")
        self.assertNotIn("compiled/scene/", native)
        self.assertNotIn("SceneAtlasBindings.h", header + native)
        self.assertEqual(scene.generate(dict(reversed(list(reports.items())))), generated)

    def test_common_ui_includes_four_original_touch_frame_corners(self):
        names = [row["name"] for row in scene.common_ui()["sprites"]]
        self.assertEqual(len(names), 28)
        self.assertEqual(len(set(names)), 28)
        corners = scene.common_ui()["sprites"][-4:]
        self.assertEqual([row["rect"] for row in corners],
                         [[0, 0, 42, 36], [62, 0, 42, 36], [0, 108, 42, 36], [62, 108, 42, 36]])
        self.assertTrue(all(row["image"] == "IMAGE_TOUCH_SELECTION_FRAME_BLUE" for row in corners))
        self.assertEqual(sum(name.startswith("BUTTON_") for name in names), 6)
        self.assertEqual(sum(name.startswith("DIALOG_") for name in names), 13)
        self.assertEqual(sum(name.startswith("SEEDCHOOSER_BUTTON") for name in names), 5)


if __name__ == "__main__":
    unittest.main()
