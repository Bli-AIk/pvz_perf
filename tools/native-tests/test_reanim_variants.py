"""Reanim appearance rows share one body and retain independent track choices."""
from pathlib import Path
import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import build_numeric_runtime_pak64 as pak
import definition_atlas as atlas
from test_numeric_runtime_pak64 import REGISTRIES

COMPILER = os.environ.get("PVZ_RESOURCE_COMPILER") or os.environ.get("PVZ_ATLAS_COMPILER")


def envelope(payload):
    return struct.pack("<3I", 0xDEADFEDB, len(payload), zlib.crc32(payload)) + payload


class ReanimVariantPackingTests(unittest.TestCase):
    def fixture(self):
        # Root sheet has two cells, explicit image:none inherits the previous
        # image, while variant none blanks only the appearance rectangle.
        body = struct.pack("<4IfI", 0x35544449, 11, 0, 33, 12, 2)
        for track, cells in ((0, (0, 1, 2)), (1, (0,))):
            body += struct.pack("<4I", track, len(cells), 32, 0)
            for cell in cells:
                body += struct.pack("<8f", 0, 0, 0, 0, 1, 1, cell, 1)
        body += struct.pack("<9I", 1, 0, 0, 0, 1, 0, 2, 0, 0)
        source = struct.pack("<3I", 0xDEADFED7, len(body), zlib.crc32(body)) + body
        variants = [
            {"name": "Root", "images": [["SHEET"], ["HEAD"]]},
            {"name": "Root_V00000001", "images": [["BODY"], ["HEAD"]]},
            {"name": "Root_V00000002", "images": [["SHEET"], [None]]},
            {"name": "Root_V00000003", "images": [["BODY"], [None]]},
        ]
        metadata = {"tracks": ["body", "head"], "variants": variants}
        descriptors = {
            label: atlas.ImageDescriptor(label, "", "", "", 0, 1, cols, 0xFFFFFF)
            for label, cols in (("SHEET", 2), ("HEAD", 1), ("BODY", 1))}
        symbols = ["SHEET", "HEAD", "BODY"]
        layout = {"width": 64, "height": 64,
                  "regions": [(2, 2, 20, 8), (2, 14, 4, 6), (2, 24, 13, 7)]}
        actions = {"actions": [{"track": "head", "option": 0}, {"track": "head", "option": 1},
                               {"track": "body", "option": 0}, {"track": "body", "option": 1}],
                   "transitions": {
                       row["name"]: [variants[i & 1]["name"], variants[(i & 1) | 2]["name"],
                                     variants[i & 2]["name"], variants[(i & 2) | 1]["name"]]
                       for i, row in enumerate(variants)}}
        data, report = atlas.compile_reanim_variants(
            "compiled/reanim/root.compiled", source, metadata,
            [{"symbols": symbols, "layout": layout}] * 4, {1: "SHEET", 2: "HEAD"},
            descriptors, actions)
        return data, report

    def test_flat_rows_cell_slots_blank_and_sorted_independent_actions(self):
        data, report = self.fixture()
        parsed = pak.parse_definition_atlas("compiled/reanim/root.compiled", data)
        self.assertEqual((report["variant_count"], report["slot_count"], report["action_count"]), (4, 4, 4))
        self.assertEqual(parsed["action_keys"], [(0, 0), (0, 1), (1, 0), (1, 1)])
        self.assertEqual(parsed["frame_slots"], [1, 2, 1, 3])
        self.assertEqual(parsed["variant_rectangles"][0],
                         [(0, 0, 0, 0), (2, 2, 10, 8), (12, 2, 10, 8), (2, 14, 4, 6)])
        self.assertEqual(parsed["variant_rectangles"][1][1:3], [(2, 24, 13, 7)] * 2)
        self.assertEqual(parsed["variant_rectangles"][2][3], (0, 0, 0, 0))
        self.assertEqual(struct.unpack_from("<I", data, parsed["body_offset"])[0], 0x36544449)
        self.assertEqual(struct.unpack_from("<I", data, 12)[0], 0x37564152)
        self.assertEqual(parsed["body_offset"], 20)
        self.assertEqual(parsed["appearance_offset"], 20 + parsed["behavior_bytes"])
        references = set()
        pak.validate_numeric("compiled/reanim/root.compiled", data, REGISTRIES, references,
                             atlas_paths=["compiled/shared.atlas.png"] * 4)
        self.assertEqual(references, {("atlas", "compiled/shared.atlas.png")})

    def test_bad_counts_transitions_slots_rectangles_and_trailing_behavior_rejected(self):
        data, _ = self.fixture()
        parsed = pak.parse_definition_atlas("compiled/reanim/root.compiled", data)
        for offset, value in ((16, 0), (parsed["appearance_offset"], 0),
                              (parsed["appearance_offset"] + 4, 0),
                              (parsed["variant_rectangle_offsets"][0][1], -1),
                              (parsed["body_offset"] + 8, 1)):
            changed = bytearray(data)
            struct.pack_into("<i", changed, offset, value)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                pak.validate_numeric("compiled/reanim/root.compiled", envelope(changed[12:]), REGISTRIES)
        with self.assertRaises(ValueError):
            pak.validate_numeric("compiled/reanim/root.compiled", envelope(data[12:] + b"tail"), REGISTRIES)


@unittest.skipUnless(COMPILER, "Set PVZ_RESOURCE_COMPILER")
class ReanimVariantCompilerTests(unittest.TestCase):
    def test_production_roots_compile_once_and_all_appearance_metadata_resolves(self):
        root = Path(__file__).resolve().parents[2]
        directory = Path(self.temporary.name)
        paths = sorted((root / "assets/definitions/reanim").rglob("*.dsl"))
        inputs = directory / "inputs.txt"
        inputs.write_text("\n".join(map(str, paths)) + "\n", encoding="utf-8")
        output = directory / "production"
        result = subprocess.run([
            COMPILER, "compile-v2", "--input-list", str(inputs), "--output", str(output),
            "--type-map", str(root / "tools/reanim_type_map.txt"),
            "--family-file", str(root / "assets/definitions/toolchain/reanim_track_families.txt"),
            "--font-registry", str(root / "assets/definitions/toolchain/font_symbol_ids.bin")],
            capture_output=True, text=True, errors="replace")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        metadata = json.loads((output / "reanim_variants.json").read_text())["roots"]
        self.assertEqual(len(metadata), 146)
        self.assertEqual(len(list(output.glob("*.source-record"))), 146)
        declarations = sum(len(row["variants"]) - 1 for row in metadata.values())
        self.assertGreater(declarations, 2500)
        self.assertLess(sum(path.stat().st_size for path in output.glob("*.source-record")), 64 * 1024 * 1024)
        # Exercise every authored transition without waiting for atlas proofs.
        # Synthetic bounded regions isolate behavior sharing and state tables.
        actions = json.loads((root / "assets/definitions/reanim-variant-actions.json").read_text())
        labels = (output / "image_sources.txt").read_text().splitlines()
        handles = dict(enumerate(labels, 1))
        descriptors = {label: atlas.ImageDescriptor(label, "", "", "", 0, 1, 1, 0xFFFFFF)
                       for label in labels}
        for name, definition in metadata.items():
            source = (output / f"{name}.source-record").read_bytes()
            symbols = sorted({label for row in definition["variants"] for track in row["images"]
                              for label in track if label is not None})
            layout = {"width": max(16, len(symbols) * 12 + 4), "height": 16,
                      "regions": [(2 + i * 12, 2, 8, 8) for i in range(len(symbols))]}
            packed, _ = atlas.compile_reanim_variants(
                f"compiled/reanim/{name.casefold()}.compiled", source, definition,
                [{"symbols": symbols, "layout": layout}] * len(definition["variants"]),
                handles, descriptors, actions.get(name))
            parsed = pak.parse_definition_atlas(f"compiled/reanim/{name.casefold()}.compiled", packed)
            self.assertEqual(parsed["variant_count"], len(definition["variants"]))
            self.assertEqual(struct.unpack_from("<I", packed, parsed["body_offset"])[0], 0x46544449)

    def compile(self, source, success=True):
        directory = Path(self.temporary.name)
        (directory / "input.dsl").write_text(source, encoding="utf-8")
        (directory / "types.txt").write_text("root 0\n", encoding="utf-8")
        (directory / "fonts.bin").write_bytes(struct.pack("<2I", 0x4953594D, 0))
        output = directory / "output"
        result = subprocess.run([COMPILER, "compile-v2", "--input", str(directory / "input.dsl"),
                                 "--output", str(output), "--type-map", str(directory / "types.txt"),
                                 "--font-registry", str(directory / "fonts.bin")], capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return output, result

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = ('export component Root { fps:12; Track {source-name:"body"; frame-count:3;'
                     'frame 0 {image:"A";} frame 1 {image:"B";} frame 2 {image:"A";}}'
                     'Track {source-name:"empty";frame-count:1;frame 0 {image:none;}}}')

    def test_forward_chain_metadata_enum_and_one_behavior_record(self):
        output, _ = self.compile(
            'export component Root_V00000002 { variant-of:Root_V00000001; images:[[none,"D"],[]];}'
            'export component Root_V00000001 { variant-of:Root; images:[["C","D"],[]];}' + self.root)
        self.assertEqual([path.name for path in output.glob("*.source-record")], ["Root.source-record"])
        metadata = json.loads((output / "reanim_variants.json").read_text())["roots"]["Root"]
        self.assertEqual([row["index"] for row in metadata["variants"]], [0, 1, 2])
        self.assertEqual(metadata["variants"][2]["images"], [[None, "D"], []])
        self.assertIn("Root_V00000002 = 2", (output / "ReanimVariantId.h").read_text())
        self.assertEqual((output / "image_sources.txt").read_text(), "A\nB\nC\nD\n")

    def test_strict_replacements_missing_parent_cycle_duplicate_and_behavior_override(self):
        cases = [
            ('export component V {variant-of:Root;images:[["X"],[]];}', "image count"),
            ('export component V {variant-of:Root;images:[["X","Y"]];}', "one images list"),
            ('export component V {variant-of:Missing;images:[];}', "missing"),
            ('export component V {variant-of:V;images:[];}', "cycle"),
            ('export component V {variant-of:Root;images:[["X","Y"],[]];fps:2;}', "behavior"),
        ]
        for source, message in cases:
            with self.subTest(message=message):
                _, result = self.compile(self.root + source, False)
                self.assertIn(message, result.stderr)


if __name__ == "__main__":
    unittest.main()
