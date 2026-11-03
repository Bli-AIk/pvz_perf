"""Audit authored variants and native particles sharing one behavior per root.

PVZ_RESOURCE_PACKAGE selects the built main.pak64 and adjacent manifest.json.
The ParticleVariantSourceTests and ParticleVariantComparisonTests classes also
run before a new package exists.
"""
from __future__ import annotations

import hashlib
import io
import itertools
import json
import os
from pathlib import Path
import re
import struct
import sys
import unittest
import zipfile
import zlib

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import build_numeric_runtime_pak64 as pak
import definition_atlas
import runtime_asset_sources
import resource_build_config
from migrate_particle_frame_images import check_published_frames

PACKAGE = Path(os.environ.get(
    "PVZ_RESOURCE_PACKAGE", ROOT / "out/x64-win32-vulkan/cache/resources/main.pak64"))


def component_definitions() -> dict[str, dict]:
    """Read balanced DSL component bodies without confusing nested track braces."""
    result = {}
    declaration = re.compile(r"\bexport\s+component\s+([A-Za-z_]\w*)\s*\{")
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|[{}]')
    for path in sorted((ROOT / "assets/definitions/particles").rglob("*.dsl")):
        text = pak.clean_comments(path.read_text(encoding="utf-8-sig"))
        for match in declaration.finditer(text):
            depth = 1
            for token in tokens.finditer(text, match.end()):
                if token[0] == "{":
                    depth += 1
                elif token[0] == "}":
                    depth -= 1
                if depth == 0:
                    body = text[match.end():token.start()]
                    break
            else:
                raise ValueError(f"Unclosed component: {path}:{match[1]}")
            pak.require(match[1] not in result, f"Duplicate particle component: {match[1]}")
            source = re.findall(r'\bsource-name\s*:\s*"([^"]+)"\s*;', body)
            pak.require(len(source) == 1, f"Missing/duplicate source-name: {match[1]}")
            result[match[1]] = {
                "file": path, "body": body, "source": source[0],
                "path": pak.definition_path(f"particles/{source[0]}.xml"),
            }
    return result


def original_particle_paths() -> dict[str, str]:
    config = resource_build_config.load(ROOT / "assets/definitions/ResourceBuild.pvzbuild")
    result = {effect: pak.definition_path(f"particles/{name}.xml")
              for effect, name in config.definition_orders["particle"]}
    pak.require(len(result) == len(set(result.values())) == 106,
                "Original 106 particle definitions are incomplete or aliased")
    return result


def nonimage_bytes(name: str, record: bytes) -> bytes:
    """Retain every byte except atlas dimensions, image rectangles and outer CRC."""
    atlas = pak.parse_definition_atlas(name, record)
    pak.require(atlas is not None and pak.numeric_kind(name) == "particles",
                f"Expected an inline particle v5 record: {name}")
    canonical = bytearray(record)
    canonical[8:12] = bytes(4)     # CRC changes when any image rectangle changes.
    canonical[16:24] = bytes(8)    # Shared atlas width/height may differ.
    for offset in atlas["image_offsets"]:
        canonical[offset:offset + 24] = bytes(24)
    if struct.unpack_from("<I", record, 24)[0] == 0x35524449:
        # Frame counts can change with the replacement source. Remove both the
        # count and table while retaining all emitter motion and identity data.
        reader = pak.Reader(record[44:])
        count = struct.unpack_from("<I", record, 36)[0]
        spans = []
        def track():
            reader.take(reader.count(20, 100000) * 20)
        for _ in range(count):
            start = 44 + reader.at
            reader.take(reader.count(16, 100000) * 16)
            spans.append((start, 44 + reader.at))
            reader.take(16)
            for _ in range(22):
                track()
            for _ in range(2):
                for _ in range(reader.count(12, 4)):
                    reader.take(4)
                    track()
                    track()
            for _ in range(16):
                track()
        for start, end in reversed(spans):
            del canonical[start:end]
        canonical[4:8] = bytes(4)
        canonical[28:32] = bytes(4)  # Total frame count follows replacement sequences.
        canonical[40:44] = bytes(4)
    return bytes(canonical)


class ParticleVariantSourceTests(unittest.TestCase):
    def test_inc_and_all_variant_declarations_match_inside_original_seven_files(self):
        variants = pak.particle_image_variants(ROOT)
        frame_sources = {row["path"]: row["source"] for row in json.loads(
            (ROOT / "assets/definitions/particle-frame-images.json").read_text(encoding="utf-8"))["images"]}
        definitions = component_definitions()
        original = original_particle_paths()
        self.assertEqual(len(variants), 56)
        self.assertEqual(len({row["base"] for row in variants}), 7)
        by_path = {row["path"]: name for name, row in definitions.items()
                   if "variant-of" not in row["body"]}
        self.assertTrue(set(original.values()) <= set(by_path))
        declared = {name for name, row in definitions.items()
                    if re.search(r"\bvariant-of\s*:", row["body"])}
        self.assertEqual(declared, {row["component"] for row in variants})
        original_files = set()
        for row in variants:
            with self.subTest(component=row["component"]):
                variant = definitions[row["component"]]
                base = definitions[by_path[original[row["base"]]]]
                original_files.add(base["file"])
                self.assertEqual(variant["file"], base["file"])
                self.assertEqual(variant["source"], row["component"].lower())
                self.assertEqual(variant["path"], row["path"])
                self.assertEqual(re.findall(r"\bvariant-of\s*:\s*(\w+)\s*;", variant["body"]),
                                 [by_path[original[row["base"]]]])
                images = re.search(r'\bimages\s*:\s*(\[[\s\S]*\])\s*;', variant["body"])
                self.assertIsNotNone(images)
                labels = re.findall(r'"([^"]+)"', images[1])
                self.assertTrue(labels)
                self.assertTrue(all(label == row["image"] or
                                    frame_sources.get(label) == row["image"]
                                    for label in labels))
                # The image variant cannot silently acquire motion overrides.
                properties = re.findall(r"([\w-]+)\s*:", variant["body"])
                self.assertCountEqual(properties, ["kind", "source-name", "variant-of", "images"])
                self.assertNotRegex(variant["body"], r"\b[Ee]mitter\s*\{")
        self.assertEqual(len(original_files), 7)
        self.assertFalse((ROOT / "assets/definitions/particles/particles/"
                         "ZombieImageVariants.particle.dsl").exists())

    def test_split_frame_pngs_exactly_match_logical_rgba_source_crops(self):
        checked = check_published_frames(ROOT)
        self.assertEqual(checked["decoded_png_frames"], 213)
        self.assertEqual(checked["source_sheets"], 36)
        self.assertGreater(checked["live_source_crops_compared"], 0)


class ParticleVariantComparisonTests(unittest.TestCase):
    def test_parent_paths_match_runtime_canonical_utf8_rules(self):
        prefix = "compiled/particles/"
        for leaf in ("base", "a_b-2.3", "Éß"):
            self.assertTrue(pak.canonical_particle_path(prefix + leaf + ".compiled"))
        for leaf in ("", ".", "..", "Upper", "a b", "a+b", "a/b", "a\\b", "\0", "\ud800"):
            self.assertFalse(pak.canonical_particle_path(prefix + leaf + ".compiled"))

    @staticmethod
    def inherited_fixture(parent="compiled/particles/base.compiled", frames=((2, 2, 8, 8),)):
        path = parent.encode("utf-8")
        inner = struct.pack("<5I", 0x31564449, 123, len(path), len(frames), 1) + path
        inner += struct.pack("<I", len(frames)) + b"".join(struct.pack("<4i", *f) for f in frames)
        payload = struct.pack("<3I", 0x35564150, 64, 64) + inner
        return struct.pack("<3I", 0xDEADFEDA, len(payload), zlib.crc32(payload)) + payload

    def test_inheritance_contains_only_parent_reference_and_rectangles(self):
        name, parent = "compiled/particles/child.compiled", "compiled/particles/base.compiled"
        child = self.inherited_fixture()
        atlas = pak.parse_definition_atlas(name, child)
        self.assertEqual(atlas["parent"], parent)
        self.assertEqual(atlas["frame_counts"], [1])
        self.assertEqual(len(child), 44 + len(parent.encode()) + 4 + 16)
        pak.validate_numeric(name, child, {"emitter": 123})
        records = {name: child, parent: self.explicit_fixture([(2, 2, 8, 8)])}
        pak.validate_particle_inheritance(records)
        for invalid in (
            {name: child},
            {name: child, parent: self.inherited_fixture(name)},
            {name: child, parent: self.explicit_fixture([])},
        ):
            with self.assertRaises(ValueError):
                pak.validate_particle_inheritance(invalid)
        payload = child[12:] + b"behavior-tail"
        corrupt = struct.pack("<3I", 0xDEADFEDA, len(payload), zlib.crc32(payload)) + payload
        with self.assertRaises(ValueError):
            pak.parse_definition_atlas(name, corrupt)

    @staticmethod
    def explicit_fixture(frames, size=64):
        body = struct.pack("<I", len(frames)) + b"".join(struct.pack("<4i", *frame) for frame in frames)
        body += struct.pack("<4I", 1, 0, 5, 0) + bytes(22 * 4 + 2 * 4 + 16 * 4)
        inner = struct.pack("<5I", 0x35524449, len(frames), 123, 1, len(body)) + body
        inner += struct.pack("<6I", 0x33544449, 1, 0xFFFFFFFF, 0x35544449, 1, 7)
        payload = struct.pack("<3I", 0x35564150, size, size) + inner
        return struct.pack("<3I", 0xDEADFEDA, len(payload), zlib.crc32(payload)) + payload

    def test_comparison_strips_variable_image_sequence_preserves_motion_scalars(self):
        name = "compiled/particles/fixture.compiled"
        first = self.explicit_fixture([(2, 2, 8, 8)])
        second = self.explicit_fixture([(2, 2, 8, 8), (2, 2, 8, 8), (30, 30, 16, 16)], 128)
        self.assertEqual(nonimage_bytes(name, first), nonimage_bytes(name, second))
        self.assertEqual(pak.parse_definition_atlas(name, second)["total_frame_count"], 3)
        for offset in (48 + 16 * 3, len(second) - 4):
            changed = bytearray(second)
            changed[offset] ^= 1
            struct.pack_into("<I", changed, 8, zlib.crc32(changed[12:]))
            self.assertNotEqual(nonimage_bytes(name, first), nonimage_bytes(name, bytes(changed)))

    @staticmethod
    def fixture(region=(2, 2, 8, 8, 1, 1), size=64):
        body = struct.pack("<6I6I", *region, 1, 2, 3, 4, 5, 0)
        body += bytes(22 * 4 + 2 * 4 + 16 * 4)
        inner = struct.pack("<5I", 0x35504449, 0, 123, 1, len(body)) + body
        inner += struct.pack("<6I", 0x33544449, 1, 0xFFFFFFFF, 0x35544449, 1, 7)
        payload = struct.pack("<3I", 0x35564150, size, size) + inner
        return struct.pack("<3I", 0xDEADFEDA, len(payload), zlib.crc32(payload)) + payload

    def test_comparison_accepts_only_image_geometry_and_atlas_dimension_changes(self):
        name = "compiled/particles/fixture.compiled"
        base = self.fixture()
        changed = self.fixture((10, 12, 20, 24, 2, 2), 128)
        self.assertEqual(nonimage_bytes(name, base), nonimage_bytes(name, changed))
        parsed = pak.parse_definition_atlas(name, changed)
        # These mutations stand for an emitter parameter and the final name ID;
        # all motion tracks, field arrays and OnDuration links remain compared too.
        for offset in (parsed["image_offsets"][0] + 24, len(changed) - 4):
            with self.subTest(offset=offset):
                corrupt = bytearray(changed)
                corrupt[offset] ^= 1
                struct.pack_into("<I", corrupt, 8, zlib.crc32(corrupt[12:]))
                self.assertNotEqual(nonimage_bytes(name, base), nonimage_bytes(name, bytes(corrupt)))


class ParticleVariantPackageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.variants = pak.particle_image_variants(ROOT)
        cls.original = original_particle_paths()
        cls.report = json.loads(PACKAGE.with_name("manifest.json").read_text(encoding="utf-8"))
        cls.archive = zipfile.ZipFile(PACKAGE)
        cls.addClassCleanup(cls.archive.close)
        cls.index = pak.archive_index(cls.archive)
        cls.records = {name for name in cls.index if pak.numeric_kind(name) == "particles"}
        cls.bindings = pak.read_definition_image_bindings(ROOT)
        cls.variant_bindings = pak.read_definition_image_binding_rows(ROOT)
        cls.generated = ROOT / "pvzPerf/Sexy.TodLib/generated"
        cls.runtime = {}
        for path in cls.index:
            if not path.startswith("compiled/particle-runtime/"):
                continue
            reader = pak.Reader(cls.archive.read(path))
            magic, names, name_hash, emitters, variants, slots = reader.unpack("<6I")
            if magic != 0x31435450:
                raise AssertionError(f"Unexpected particle runtime format: {path}")
            headers = []
            for _ in range(emitters):
                headers.append(reader.unpack("<2I3i4I"))
                for _ in range(22):
                    reader.take(reader.count(24, 100000) * 24)
                for _ in range(2):
                    for _ in range(reader.count(12, 4)):
                        reader.word()
                        for _ in range(2):
                            reader.take(reader.count(24, 100000) * 24)
                for _ in range(16):
                    reader.take(reader.count(24, 100000) * 24)
            rows = []
            for _ in range(variants):
                width, height, length = reader.unpack("<3I")
                atlas_path = reader.take(length).decode("utf-8")
                regions = [reader.unpack("<4f2i") for _ in range(slots)]
                rows.append((width, height, atlas_path, regions))
            reader.finish()
            cls.runtime[path] = (headers, rows)

    def appearance(self, row):
        root = self.original[row["base"]]
        reports = self.report["definition_atlases"][root]["variants"]
        index = next(index for index, report in enumerate(reports) if report["name"] == row["component"])
        definition = self.report["definition_atlases"][root]
        metadata = reports[index]
        runtime_path = root.replace("compiled/particles/", "compiled/particle-runtime/")
        headers, rows = self.runtime[runtime_path]
        width, height, atlas_path, regions = rows[index]
        self.assertEqual((width, height, atlas_path),
                         (metadata["atlas_width"], metadata["atlas_height"], metadata["atlas_path"]))
        expected = []
        for x, y, w, h in metadata["frame_rectangles"]:
            values = (x / width, y / height, (x + w) / width, (y + h) / height) if width and height else (0.,) * 4
            expected.append(struct.unpack("<4f2i", struct.pack("<4f2i", *values, w, h)))
        self.assertEqual(regions, expected)
        return root, index, metadata, {
            "regions": [], "frame_fields4I": [tuple(rect) for rect in metadata["frame_rectangles"]],
            "width": metadata["atlas_width"], "height": metadata["atlas_height"],
            "total_frame_count": definition["slot_count"],
            "frame_counts": [header[1] for header in headers],
            "slot_offsets": [header[0] for header in headers]}

    def test_all_56_variants_and_original_106_defaults_are_present(self):
        self.assertEqual(len(self.variants), 56)
        expected = set(self.original.values())
        self.assertEqual(len(expected), 106)
        self.assertEqual(self.records, set())
        self.assertEqual(self.report["numeric_counts"]["particles"], 0)
        self.assertEqual(self.report["static_definitions"]["particle"]["definitions"], 106)
        self.assertTrue(expected <= set(self.bindings))
        self.assertTrue(expected <= set(self.report["definition_atlases"]))
        self.assertFalse({row["path"] for row in self.variants} & set(self.records))
        self.assertEqual(sum(len(self.report["definition_atlases"][name]["variants"]) for name in expected), 162)

    def test_every_root_has_one_compiled_behavior_and_complete_appearance_array(self):
        for row in self.variants:
            with self.subTest(component=row["component"]):
                name, index, metadata, variant = self.appearance(row)
                self.assertGreater(index, 0)
                self.assertEqual(len(variant["frame_fields4I"]), variant["total_frame_count"])
                self.assertEqual(sum(variant["frame_counts"]), variant["total_frame_count"])
                self.assertEqual(variant["slot_offsets"], list(
                    itertools.accumulate([0] + variant["frame_counts"][:-1])))
                self.assertGreater(len(variant["frame_fields4I"]), 0)
        # Every appearance borrows its root's behavior record.
        expected_runtime = {name.replace("compiled/particles/", "compiled/particle-runtime/")
                            for name in self.original.values()}
        self.assertEqual(set(self.runtime), expected_runtime)
        self.assertEqual(len(self.runtime), 106)
        self.assertEqual(list((self.generated / "definitions").glob("ParticleData*.cpp")), [])
        # GPU particle evaluation was retired; only CPU behavior records ship.
        self.assertFalse({name for name in self.index if name.startswith("compiled/gpu-particle/")})

    def test_every_variant_region_matches_the_declared_source_image_pixels(self):
        files = runtime_asset_sources.inventory(ROOT)
        descriptors = definition_atlas.authoring_descriptors(
            (ROOT / "assets/definitions/resources.json").read_bytes(), files,
            {image["source"] for row in self.variants for image in self.appearance(row)[2]["images"]})
        sources, atlases = {}, {}
        for row in self.variants:
            with self.subTest(component=row["component"], image=row["image"]):
                name, variant_index, metadata, atlas = self.appearance(row)
                self.assertEqual(metadata["region_count"], len(metadata["images"]))
                self.assertTrue(metadata["images"])
                self.assertEqual(atlas["regions"], [])
                self.assertEqual(atlas["frame_fields4I"],
                                 [tuple(frame["atlas_rect"]) for frame in metadata["frames"]])
                self.assertEqual((metadata["atlas_width"], metadata["atlas_height"]),
                                 (atlas["width"], atlas["height"]))
                path = metadata["atlas_path"]
                self.assertEqual(self.variant_bindings[name][variant_index], path)
                self.assertIn(path, self.index)
                if path not in atlases:
                    with Image.open(io.BytesIO(self.archive.read(self.index[path]))) as decoded:
                        self.assertEqual((decoded.format, decoded.mode), ("PNG", "RGBA"))
                        atlases[path] = decoded.convert("RGBA")
                packed = atlases[path]
                self.assertEqual(packed.size, (atlas["width"], atlas["height"]))
                blocks = {}
                for image in metadata["images"]:
                    label = image["source"]
                    descriptor = descriptors[label]
                    if label not in sources:
                        sources[label] = definition_atlas.logical_image(descriptor, files)[0]
                    source = sources[label]
                    region = tuple(image["region"])
                    x, y, width, height, rows, cols = region
                    self.assertEqual((width, height), source.size)
                    self.assertEqual((rows, cols), (descriptor.rows, descriptor.cols))
                    self.assertEqual(image["rgba_sha256"], hashlib.sha256(source.tobytes()).hexdigest())
                    self.assertEqual(packed.crop((x, y, x + width, y + height)).tobytes(),
                                     source.tobytes())
                    blocks[label] = region[:4]
                self.assertEqual(atlas["frame_fields4I"],
                                 [blocks[frame["source"]] for frame in metadata["frames"]])


if __name__ == "__main__":
    unittest.main()
