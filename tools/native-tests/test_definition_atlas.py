"""Pixel and reference invariants for the offline definition atlas conversion."""
from pathlib import Path
import importlib.util
import io
import json
import os
import struct
import sys
import tempfile
import subprocess
import unittest
from unittest import mock
import zlib

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import definition_atlas as atlas
import build_numeric_runtime_pak64 as pak
import test_numeric_runtime_pak64 as fixtures
resources = fixtures.compiler  # Frozen NRSC1 pruning cases below are historical fixtures.


def envelope(cookie, payload):
    return struct.pack("<3I", cookie, len(payload), zlib.crc32(payload)) + payload


def reanim(images):
    body = struct.pack("<4IfI", 0x35544449, 11, 22, 33, 12.0, 1)
    body += struct.pack("<4I", 0, len(images), 32, 1) + bytes(32 * len(images))
    for image in images:
        body += struct.pack("<3I", image, 1, 7) + b"Caption"
    return envelope(0xDEADFED7, body + struct.pack("<I", 0))


def particle_images(emitters):
    body = b""
    for frames in emitters:
        body += struct.pack("<I", len(frames))
        body += b"".join(struct.pack("<I", handle) for handle in frames)
        body += struct.pack("<4I", 1, 0, 0, 0) + bytes(40 * 4)
    payload = struct.pack("<5I", 0x34524449, 0, 44, len(emitters), len(body)) + body
    payload += struct.pack("<2I", 0x33544449, len(emitters))
    payload += struct.pack(f"<{len(emitters)}I", *([0xFFFFFFFF] * len(emitters)))
    payload += struct.pack("<2I", 0x35544449, len(emitters))
    payload += bytes(4 * len(emitters))
    return envelope(0xDEADFED8, payload)


class DefinitionAtlasTests(unittest.TestCase):
    def test_explicit_images_preserve_duplicate_order_mixed_sequences_and_source_pixels(self):
        with tempfile.TemporaryDirectory() as temporary:
            files, descriptors, images = {}, {}, {}
            for handle, size in ((2, (3, 7)), (3, (8, 3)), (4, (11, 5))):
                path = Path(temporary) / f"{handle}.png"
                images[handle] = Image.new("RGBA", size, (handle * 7, 19, 23, 97))
                images[handle].save(path)
                files[f"x/{handle}.png"] = path
                descriptors[handle] = atlas.ImageDescriptor(handle, f"x/{handle}", "", "", 0, 1, 1, 0xFFFFFF)
            sequence = [2, 3, 3, 2, 4]
            original = particle_images([sequence[:4], sequence[4:], []])
            offsets = atlas.image_offsets("particles", original[12:])
            self.assertEqual([struct.unpack_from("<I", original, 12 + o)[0] for o in offsets], sequence)
            data, png, report = atlas.compile_definition("compiled/particles/explicit.compiled", original,
                                                        descriptors, files)
            self.assertEqual(struct.unpack_from("<I", data, 24)[0], 0x35524449)
            parsed = pak.parse_definition_atlas("compiled/particles/explicit.compiled", data)
            self.assertEqual(parsed["regions"], [])
            self.assertEqual(parsed["image_fields"], [])
            blocks = {row["symbol"]: tuple(row["region"][:4]) for row in report["images"]}
            self.assertEqual(parsed["frame_fields4I"],
                             [blocks[handle] for handle in sequence])
            self.assertEqual(len(report["images"]), 3)
            self.assertEqual(len(report["frames"]), 5)
            self.assertIsNotNone(png)
            with Image.open(io.BytesIO(png)) as packed:
                for handle, (x, y, w, h) in blocks.items():
                    self.assertEqual(packed.crop((x, y, x + w, y + h)).tobytes(), images[handle].tobytes())
            pak.validate_numeric("compiled/particles/explicit.compiled", data, fixtures.REGISTRIES)
            root_name = "compiled/particles/explicit.compiled"
            rows = [{"width": parsed["width"], "height": parsed["height"],
                     "frame_counts": parsed["frame_counts"], "rectangles": parsed["frame_fields4I"]},
                    {"width": 0, "height": 0, "frame_counts": parsed["frame_counts"],
                     "rectangles": [(0, 0, 0, 0)] * 5}]
            combined, summary = atlas.compile_particle_variants(root_name, data, rows)
            appearance = pak.parse_definition_atlas(root_name, combined)
            self.assertEqual(struct.unpack_from("<I", combined)[0], 0xDEADFEDC)
            self.assertEqual(struct.unpack_from("<I", combined, 12)[0], 0x36564150)
            self.assertEqual(struct.unpack_from("<I", combined, 20)[0], 0x36524449)
            self.assertEqual(appearance["appearance_offset"], 20 + appearance["behavior_bytes"])
            self.assertEqual(appearance["slot_offsets"], [0, 4, 5])
            self.assertEqual(appearance["frame_counts"], [4, 1, 0])
            self.assertEqual(appearance["variant_rectangles"][0], parsed["frame_fields4I"])
            self.assertEqual(appearance["variant_rectangles"][1], [(0, 0, 0, 0)] * 5)
            self.assertEqual(summary, {"variant_count": 2, "slot_count": 5})
            pak.validate_numeric(root_name, combined, fixtures.REGISTRIES)
            for offset, value in ((16, 1), (40, 1), (44, 3),
                                  (appearance["appearance_offset"] + 4, 6),
                                  (appearance["variant_rectangle_offsets"][0][0], -1)):
                changed = bytearray(combined)
                struct.pack_into("<i", changed, offset, value)
                with self.subTest(v6_offset=offset), self.assertRaises(ValueError):
                    pak.validate_numeric(root_name, envelope(0xDEADFEDC, changed[12:]), fixtures.REGISTRIES)
            with self.assertRaisesRegex(ValueError, "v6 envelope"):
                pak.parse_definition_atlas(root_name, envelope(0xDEADFEDA, combined[12:]))
            collected = set()
            pak.validate_numeric("compiled/particles/explicit.compiled", original, fixtures.REGISTRIES,
                                 image_ids=collected)
            self.assertEqual(collected, {2, 3, 4})
            for offset, value in ((parsed["frame_offsets"][0], -1),
                                  (parsed["frame_offsets"][0] + 8, 0),
                                  (parsed["frame_offsets"][0] + 8, 0x7FFFFFFF),
                                  (28, 6),
                                  (44, 100000)):
                changed = bytearray(data)
                struct.pack_into("<i", changed, offset, value)
                with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                    pak.validate_numeric("compiled/particles/explicit.compiled",
                                         envelope(0xDEADFEDA, changed[12:]), fixtures.REGISTRIES)

    def test_explicit_variable_image_table_truncation_and_null_handles_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "sheet.png"
            Image.new("RGBA", (9, 5)).save(path)
            descriptors = {2: atlas.ImageDescriptor(2, "x/sheet", "", "", 0, 1, 1, 0xFFFFFF)}
            for frames in ([0], [2, 0]):
                with self.subTest(frames=frames), self.assertRaisesRegex(ValueError, "Null"):
                    atlas.compile_definition("compiled/particles/bad.compiled", particle_images([frames]),
                                             descriptors, {"x/sheet.png": path})
            payload = bytearray(particle_images([[2]])[12:])
            struct.pack_into("<I", payload, 20, 100000)
            with self.assertRaises(ValueError):
                atlas.image_offsets("particles", payload)
            with self.assertRaises(ValueError):
                atlas.image_offsets("particles", particle_images([[2]])[12:35])

    def test_direct_frame_image_paths_are_literal_rgba_and_reject_traversal(self):
        author = json.dumps({"format": "pvz-resources-authoring-v1", "manifest": {
            "tag": "ResourceManifest", "attrs": {}, "children": []}}).encode()
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "frame.png"
            source = Image.new("RGBA", (3, 2), (1, 2, 3, 17))
            source.save(path)
            label = "particles/frames/IMAGE_FRAME0.png"
            files = {label.lower(): path}
            descriptor = atlas.authoring_descriptors(author, files, {label})[label]
            self.assertEqual(descriptor.rows, 1)
            self.assertEqual(descriptor.cols, 1)
            self.assertEqual(atlas.logical_image(descriptor, files)[0].tobytes(), source.tobytes())
            for invalid in ("../frame.png", "particles/../frame.png", "/frame.png",
                            "C:/frame.png", "particles//frame.png", "particles/missing.png"):
                with self.subTest(path=invalid), self.assertRaises(ValueError):
                    atlas.authoring_descriptors(author, files, {invalid})

    def test_author_labels_resolve_and_compile_without_program_registry(self):
        def node(tag, **attrs):
            return {"tag": tag, "attrs": attrs, "children": []}
        author = {"format": "pvz-resources-authoring-v1", "manifest": {
            "tag": "ResourceManifest", "attrs": {}, "children": [
                {"tag": "Resources", "attrs": {"id": "IndependentAuthorGroup"}, "children": [
                    node("SetDefaults", path="images", idprefix="IMAGE_"),
                    node("Image", id="BASE", path="base", rows="2", cols="3", alphagrid="mask"),
                    node("SetDefaults", path="reanim", idprefix="IMAGE_REANIM_"),
                    node("Image", id="AUTHORED", path="authored", noalpha="true"),
                ]},
                {"tag": "Resources", "attrs": {"id": "AnotherIndependentGroup"}, "children": [
                    node("Image", id="INHERITED", path="inherited", alphacolor="123456"),
                    node("SetDefaults", idprefix="IMAGE_"),
                    node("Image", id="PROGRAM", path="!program"),
                ]},
            ]}}
        names = {"IMAGE_BASE", "IMAGE_REANIM_AUTHORED", "IMAGE_REANIM_INHERITED", "IMAGE_PROGRAM",
                 "IMAGE_REANIM_FALLBACK", "IMAGE_BURST"}
        with tempfile.TemporaryDirectory() as temporary:
            directory, files = Path(temporary), {}
            for label, size, color in (("images/base", (6, 4), (10, 20, 30, 87)),
                                       ("images/mask", (2, 2), (1, 2, 120, 0)),
                                       ("reanim/authored", (3, 2), (20, 30, 40, 77)),
                                       ("reanim/fallback", (2, 3), (30, 40, 50, 66)),
                                       ("images/fallback", (9, 9), (1, 1, 1, 1)),
                                       ("particles/burst", (3, 3), (40, 50, 60, 55))):
                path = directory / (label.replace("/", "_") + ".png")
                Image.new("RGBA", size, color).save(path)
                files[label + ".png"] = path
            with mock.patch.object(resources, "read_symbols", side_effect=AssertionError("registry read")), \
                 mock.patch.object(resources, "compile_manifest", side_effect=AssertionError("program manifest")):
                descriptors = atlas.authoring_descriptors(json.dumps(author).encode(), files, names)
                self.assertEqual(set(descriptors), names)
                self.assertEqual(descriptors["IMAGE_REANIM_FALLBACK"].path, "reanim/fallback")
                self.assertEqual(descriptors["IMAGE_BURST"].path, "particles/burst")
                self.assertEqual(descriptors["IMAGE_REANIM_INHERITED"].path, "reanim/inherited")
                self.assertEqual(descriptors["IMAGE_REANIM_INHERITED"].color, 0x123456)
                self.assertEqual(descriptors["IMAGE_REANIM_AUTHORED"].flags & 1024, 0)
                self.assertEqual(descriptors["IMAGE_BASE"].source_label, "IMAGE_BASE")
                with self.assertRaisesRegex(ValueError, "IMAGE_PROGRAM"):
                    atlas.logical_image(descriptors["IMAGE_PROGRAM"], files)
                image, _ = atlas.logical_image(descriptors["IMAGE_BASE"], files)
                self.assertEqual(set(image.getchannel("A").tobytes()), {120})
                source_names = {9: "IMAGE_BASE", 2: "IMAGE_REANIM_FALLBACK", 23: "IMAGE_BURST"}
                data, png, report = atlas.compile_definition(
                    "compiled/reanim/independent.compiled", reanim([9, 2, 9, 0]), descriptors, files,
                    source_names=source_names)
                self.assertIsNotNone(png)
                self.assertEqual({row["source"] for row in report["images"]},
                                 {"IMAGE_BASE", "IMAGE_REANIM_FALLBACK"})
                self.assertTrue(all("symbol" not in row for row in report["images"]))
                parsed = pak.parse_definition_atlas("compiled/reanim/independent.compiled", data)
                self.assertEqual(parsed["image_fields"][0], parsed["image_fields"][2])
                self.assertEqual(parsed["image_fields"][-1], (0,) * 6)
                pak.validate_numeric("compiled/reanim/independent.compiled", data,
                                     {**fixtures.REGISTRIES, "track": 11, "font": 33})
                with self.assertRaisesRegex(ValueError, "author image handle"):
                    atlas.compile_definition("compiled/reanim/bad.compiled", reanim([99]),
                                             descriptors, files, source_names=source_names)

    def test_nonrotated_pixels_and_all_extruded_edges_are_exact(self):
        images = []
        for index, size in enumerate(((3, 2), (2, 5), (7, 1), (1, 1))):
            image = Image.new("RGBA", size)
            image.putdata([(x * 17, y * 29, index * 51, (x + y + index) * 19)
                           for y in range(size[1]) for x in range(size[0])])
            images.append(image)
        compiler = Path(os.environ["PVZ_ATLAS_COMPILER"]) if os.environ.get("PVZ_ATLAS_COMPILER") else None
        packed, regions = atlas.pack(images, compiler=compiler)
        self.assertEqual(regions, atlas.pack(images, compiler=compiler)[1])
        occupied = set()
        for image, (x, y, width, height) in zip(images, regions):
            self.assertEqual(packed.crop((x, y, x + width, y + height)).tobytes(), image.tobytes())
            for yy in range(-2, height + 2):
                for xx in range(-2, width + 2):
                    self.assertNotIn((x + xx, y + yy), occupied)
                    occupied.add((x + xx, y + yy))
                    self.assertEqual(packed.getpixel((x + xx, y + yy)),
                                     image.getpixel((min(max(xx, 0), width - 1),
                                                     min(max(yy, 0), height - 1))))

    def test_auto_alpha_blue_channel_and_alpha_only_tint(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            Image.new("RGBA", (2, 1), (20, 30, 40, 5)).save(directory / "base.png")
            mask = Image.new("RGB", (2, 1))
            mask.putdata([(1, 2, 71), (4, 5, 190)])
            mask.save(directory / "_base.png")
            files = {"x/base.png": directory / "base.png", "x/_base.png": directory / "_base.png"}
            image, used = atlas.read_image("x/base", files)
            self.assertEqual(image.tobytes(), bytes([20, 30, 40, 71, 20, 30, 40, 190]))
            self.assertEqual(used, set(files))
            del files["x/base.png"]
            image, _ = atlas.read_image("x/base", files, color=0x123456)
            self.assertEqual(image.tobytes(), bytes([18, 52, 86, 71, 18, 52, 86, 190]))

    def test_inline_rectangles_zero_hash_caption_font_and_grid_preserved(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "sheet.png"
            Image.new("RGBA", (6, 4), (13, 37, 91, 117)).save(path)
            descriptor = atlas.ImageDescriptor(101, "x/sheet", "", "", 0, 2, 3, 0xFFFFFF)
            original = reanim([101, 0, 101])
            converted, png, report = atlas.compile_definition(
                "compiled/reanim/example.compiled", original, {101: descriptor}, {"x/sheet.png": path})
            cookie, size, crc = struct.unpack_from("<3I", converted)
            self.assertEqual(cookie, 0xDEADFED9)
            self.assertEqual(size, len(converted) - 12)
            self.assertEqual(crc, zlib.crc32(converted[12:]))
            magic, width, height = struct.unpack_from("<3I", converted, 12)
            self.assertEqual(magic, 0x35564152)
            parsed = pak.parse_definition_atlas("compiled/reanim/example.compiled", converted)
            x, y, w, h, rows, cols = parsed["regions"][0]
            self.assertEqual((w, h, rows, cols), (6, 4, 2, 3))
            body = converted[24:]
            offsets = atlas.image_offsets("reanim", original[12:])
            self.assertEqual(parsed["image_fields"], [(x, y, w, h, rows, cols), (0,) * 6,
                                                       (x, y, w, h, rows, cols)])
            expected = bytearray(original[12:])
            struct.pack_into("<I", expected, 8, 0)
            for offset in reversed(offsets):
                symbol = struct.unpack_from("<I", expected, offset)[0]
                expected[offset:offset + 4] = struct.pack("<6I", *((x, y, w, h, rows, cols)
                                                                  if symbol else (0,) * 6))
            self.assertEqual(body, expected)
            with Image.open(io.BytesIO(png)) as packed:
                self.assertEqual(packed.size, (width, height))
                self.assertEqual(packed.crop((x, y, x + w, y + h)).tobytes(),
                                 Image.open(path).tobytes())
            self.assertEqual(report["source_files"], ["x/sheet.png"])

    def test_empty_definition_has_no_atlas(self):
        output, png, report = atlas.compile_definition(
            "compiled/reanim/empty.compiled", reanim([0]), {}, {})
        self.assertIsNone(png)
        self.assertEqual(struct.unpack_from("<3I", output, 12), (0x35564152, 0, 0))
        self.assertEqual(pak.parse_definition_atlas("compiled/reanim/empty.compiled", output)["image_fields"],
                         [(0,) * 6])
        self.assertEqual(report["atlas_rgba_bytes"], 0)

    def test_null_particle_image_expands_to_six_zeros_and_updates_body_size(self):
        original = fixtures.particle(0)
        output, png, _ = atlas.compile_definition(
            "compiled/particles/empty.compiled", original, {}, {})
        self.assertIsNone(png)
        parsed = pak.parse_definition_atlas("compiled/particles/empty.compiled", output)
        self.assertEqual(parsed["image_fields"], [(0,) * 6])
        self.assertEqual(parsed["count"], 0)
        self.assertEqual(struct.unpack_from("<I", output, 40)[0],
                         struct.unpack_from("<I", original, 28)[0] + 20)
        pak.validate_numeric("compiled/particles/empty.compiled", output, fixtures.REGISTRIES)

    def test_reanimation_and_particle_share_pixels_without_compiled_path_or_indices(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            files, descriptors = {}, {}
            for symbol, size, color in ((2, (3, 2), (10, 20, 30, 40)),
                                        (101, (2, 4), (50, 60, 70, 80))):
                path = directory / f"{symbol}.png"
                Image.new("RGBA", size, color).save(path)
                files[f"x/{symbol}.png"] = path
                descriptors[symbol] = atlas.ImageDescriptor(
                    symbol, f"x/{symbol}", "", "", 0, 1, 1, 0xFFFFFF)
            layout = atlas.shelf_layout([(3, 2), (2, 4)])
            shared = "compiled/atlases/mixed_test.atlas.png"
            first, png, _ = atlas.compile_definition(
                "compiled/reanim/example.compiled", reanim([101]), descriptors, files,
                layout=layout, atlas_symbols=[2, 101], atlas_path=shared)
            second, other_png, _ = atlas.compile_definition(
                "compiled/particles/example.compiled", fixtures.particle(2), descriptors, files,
                layout=layout, atlas_symbols=[2, 101], atlas_path=shared, emit_png=False)
            explicit, explicit_png, explicit_report = atlas.compile_definition(
                "compiled/particles/images.compiled", particle_images([[2, 101, 2]]), descriptors, files,
                layout=layout, atlas_symbols=[2, 101], atlas_path=shared, emit_png=False)
            self.assertIsNotNone(png)
            self.assertIsNone(other_png)
            self.assertIsNone(explicit_png)
            parsed_explicit = pak.parse_definition_atlas("compiled/particles/images.compiled", explicit)
            self.assertEqual(parsed_explicit["frame_fields4I"],
                             [layout["regions"][index] for index in (0, 1, 0)])
            self.assertEqual(len(explicit_report["images"]), 2)
            self.assertEqual(len(explicit_report["frames"]), 3)
            with Image.open(io.BytesIO(png)) as packed:
                for name, data, symbol in (("compiled/reanim/example.compiled", first, 101),
                                          ("compiled/particles/example.compiled", second, 2)):
                    parsed = pak.parse_definition_atlas(name, data)
                    self.assertNotIn("path", parsed)
                    self.assertNotIn(shared.encode(), data)
                    self.assertEqual(parsed["count"], 1)
                    x, y, w, h, _, _ = parsed["regions"][0]
                    self.assertEqual(packed.crop((x, y, x + w, y + h)).tobytes(),
                                     Image.open(files[f"x/{symbol}.png"]).tobytes())
            references = set()
            pak.validate_numeric("compiled/reanim/example.compiled", first,
                                 {**fixtures.REGISTRIES, "track": 11, "font": 33}, references,
                                 atlas_path=shared)
            pak.validate_numeric("compiled/particles/example.compiled", second, fixtures.REGISTRIES,
                                 references, atlas_path=shared)
            pak.validate_numeric("compiled/particles/images.compiled", explicit, fixtures.REGISTRIES,
                                 references, atlas_path=shared)
            self.assertEqual(references, {("atlas", shared)})

    def test_invalid_envelope_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "compiler definition"):
            atlas.compile_definition("compiled/reanim/empty.compiled", reanim([0])[:-1], {}, {})

    def test_pruning_removes_physical_binding_but_keeps_code_ui_and_stable_slots(self):
        original = fixtures.resources({20: {"path": "reanim/unused"},
                                       879: {"path": "reanim/code"}})
        output, mapping, kinds, retired = resources.prune_definition_images(
            original, fixtures.REGISTRIES["image_slots"], {2})
        self.assertEqual(retired, {1})
        self.assertEqual(mapping, [resources.NONE, resources.NONE, 879])
        self.assertEqual(kinds[20], pak.RESOURCE_KIND_NONE)
        registries = {**fixtures.REGISTRIES, "resource_kinds": kinds, "image_slots": mapping}
        references = set()
        pak.validate_numeric(pak.RESOURCE_PATH, output, registries, references)
        self.assertIn(("image", "reanim/code"), references)
        self.assertNotIn(("image", "reanim/unused"), references)
        kept, mapping, kinds, retired = resources.prune_definition_images(
            fixtures.resources(), fixtures.REGISTRIES["image_slots"], set())
        self.assertEqual(retired, {2})  # Base images/Actual remains an authored UI root.
        self.assertEqual(mapping[1], 20)
        self.assertEqual(kinds[20], 0)

    def test_split_frame_provenance_prunes_only_used_parent_and_code_roots_survive(self):
        spec = importlib.util.spec_from_file_location(
            "atlas_pipeline_pruning", Path(atlas.__file__).parent / "resourcePipeline/src/build_resources.py")
        pipeline = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(pipeline)
        provenance = {"format": "pvz-particle-frame-images-v1", "images": [
            {"path": "particles/frames/used_r0_c0.png", "source": "IMAGE_PARENT_USED"},
            {"path": "particles/frames/unused_r0_c0.png", "source": "IMAGE_PARENT_UNUSED"},
        ]}
        referenced = {"compiled/particles/example.compiled": [
            "particles/frames/USED_r0_c0.png", "particles/frames/USED_r0_c0.png"]}
        candidates = pipeline.definition_image_pruning_sources(referenced, provenance)
        self.assertEqual(candidates, {"particles/frames/used_r0_c0.png", "image_parent_used"})
        self.assertNotIn("image_parent_unused", candidates)
        symbol_names = {1: "IMAGE_PARENT_USED", 2: "IMAGE_PARENT_UNUSED"}
        symbols = {symbol for symbol, name in symbol_names.items() if name.casefold() in candidates}
        original = fixtures.resources({20: {"path": "images/used-parent"},
                                       879: {"path": "images/unused-parent"}})
        output, mapping, kinds, retired = resources.prune_definition_images(
            original, fixtures.REGISTRIES["image_slots"], {2}, symbols)
        self.assertEqual(retired, {1})
        self.assertEqual(mapping[1], resources.NONE)
        self.assertEqual(mapping[2], 879)
        self.assertNotIn(b"images/used-parent", output)
        self.assertIn(b"images/unused-parent", output)
        # A real C++ root of the same parent always takes precedence.
        kept, mapping, _, retired = resources.prune_definition_images(
            original, fixtures.REGISTRIES["image_slots"], {1, 2}, symbols)
        self.assertEqual(retired, set())
        self.assertIn(b"images/used-parent", kept)
        self.assertEqual(mapping[1], 20)

    def test_v5_validator_requires_valid_inline_geometry_and_zero_registry_hash(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "image.png"
            Image.new("RGBA", (1, 1), (1, 2, 3, 4)).save(path)
            descriptor = atlas.ImageDescriptor(101, "x/image", "", "", 0, 1, 1, 0xFFFFFF)
            data, _, _ = atlas.compile_definition("compiled/reanim/example.compiled",
                                                 reanim([101]), {101: descriptor}, {"x/image.png": path})
            registries = {**fixtures.REGISTRIES, "track": 11, "font": 33}
            references, globals_ = set(), set()
            pak.validate_numeric("compiled/reanim/example.compiled", data, registries, references,
                                 image_ids=globals_, atlas_path="compiled/reanim/example.atlas.png")
            self.assertEqual(globals_, set())
            self.assertEqual(references, {("atlas", "compiled/reanim/example.atlas.png")})
            body_offset = 24
            region_offset = pak.parse_definition_atlas("compiled/reanim/example.compiled", data)["image_offsets"][0]
            for offset, value in ((body_offset + 8, 22),
                                  (region_offset + 8, 0),  # Partial zero region is invalid.
                                  (region_offset, 0)):  # Region x must include its extrusion.
                changed = bytearray(data)
                struct.pack_into("<I", changed, offset, value)
                changed = envelope(0xDEADFED9, changed[12:])
                with self.subTest(offset=offset), self.assertRaises(ValueError):
                    pak.validate_numeric("compiled/reanim/example.compiled", changed, registries)

    def test_particle_inline_rectangle_body_size_and_other_emitter_bytes_preserved(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "image.png"
            Image.new("RGBA", (67, 17), (17, 8, 42, 63)).save(path)
            descriptor = atlas.ImageDescriptor(2, "x/image", "", "", 0, 1, 4, 0xFFFFFF)
            original = fixtures.particle(2)
            data, _, _ = atlas.compile_definition("compiled/particles/example.compiled", original,
                                                 {2: descriptor}, {"x/image.png": path})
            expected = bytearray(original[12:])
            struct.pack_into("<I", expected, 4, 0)
            region = pak.parse_definition_atlas("compiled/particles/example.compiled", data)["regions"][0]
            original_size = struct.unpack_from("<I", expected, 16)[0]
            struct.pack_into("<I", expected, 16, original_size + 20)
            expected[20:24] = struct.pack("<6I", *region)
            self.assertEqual(data[24:], expected)
            pak.validate_numeric("compiled/particles/example.compiled", data, fixtures.REGISTRIES)

    def test_explicit_grid_blue_channel_repeats_and_preserves_remainder(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            Image.new("RGBA", (5, 2), (10, 11, 12, 87)).save(directory / "base.png")
            mask = Image.new("RGBA", (2, 2))
            mask.putdata([(2, 3, 4, 0), (5, 6, 7, 0), (8, 9, 10, 0), (11, 12, 13, 0)])
            mask.save(directory / "mask.png")
            files = {"x/base.png": directory / "base.png", "x/mask.png": directory / "mask.png"}
            descriptor = atlas.ImageDescriptor(1, "x/base", "", "x/mask", 1024, 1, 2, 0)
            image, used = atlas.logical_image(descriptor, files)
            self.assertEqual(image.getchannel("A").tobytes(), bytes([4, 7, 4, 7, 87, 10, 13, 10, 13, 87]))
            self.assertEqual(image.getpixel((0, 0))[:3], (10, 11, 12))
            self.assertEqual(used, set(files))


@unittest.skipUnless(os.environ.get("PVZ_ATLAS_COMPILER"), "Set PVZ_ATLAS_COMPILER for native layout tests")
class NativeAtlasLayoutTests(unittest.TestCase):
    def compiler(self):
        return Path(os.environ["PVZ_ATLAS_COMPILER"])

    def test_batch_is_deterministic_preserves_order_and_never_regresses_shelf(self):
        batches = [[], [(1, 1)], [(56, 96), (36, 56), (36, 36)],
                   [(28, 28)] * 16, [(124, 1)] * 4 + [(1, 124)] * 4,
                   [(4092, 1)], [(2044, 4092), (2044, 2044), (2044, 2044)]]
        first = atlas.native_layouts(self.compiler(), batches)
        second = atlas.native_layouts(self.compiler(), batches)
        for sizes, a, b in zip(batches, first, second):
            self.assertEqual({k: v for k, v in a.items() if k != "search_microseconds"},
                             {k: v for k, v in b.items() if k != "search_microseconds"})
            self.assertEqual([(w, h) for _, _, w, h in a["regions"]], sizes)
            if a["shelf_area"]:
                self.assertLessEqual(a["width"] * a["height"], a["shelf_area"])
            self.assertLessEqual(a["search_attempts"], 16288)
        self.assertLess(first[2]["width"] * first[2]["height"], first[2]["shelf_area"])
        self.assertEqual((first[-1]["width"], first[-1]["height"]), (4096, 4096))
        self.assertEqual(first[-1]["shelf_area"], 0)

    def test_padded_sources_exceeding_4096_are_rejected(self):
        with self.assertRaises(subprocess.CalledProcessError):
            atlas.native_layouts(self.compiler(), [[(4093, 1)]])

    def test_vertical_gap_filling_keeps_logical_orientation_and_padding(self):
        # Padded rectangles 60x100,40x60,40x40 exactly fill a 100x100 square:
        # a full-height column beside a second column split vertically.
        vertical = [(56, 96), (36, 56), (36, 36)]
        horizontal = [(h, w) for w, h in vertical]
        results = atlas.native_layouts(self.compiler(), [vertical, horizontal])
        for sizes, result in zip((vertical, horizontal), results):
            self.assertEqual((result["width"], result["height"]), (100, 100))
            self.assertEqual([(w, h) for _, _, w, h in result["regions"]], sizes)
            self.assertEqual(sum((w + 4) * (h + 4) for w, h in sizes), 10000)
        _, second, third = results[0]["regions"]
        self.assertEqual(second[0], third[0])
        self.assertTrue(second[1] + second[3] + 4 == third[1] or
                        third[1] + third[3] + 4 == second[1])
        _, second, third = results[1]["regions"]
        self.assertEqual(second[1], third[1])
        self.assertTrue(second[0] + second[2] + 4 == third[0] or
                        third[0] + third[2] + 4 == second[0])

    def test_two_dimensional_complementary_gaps_pack_without_wasted_pixels(self):
        # Each pair contributes one vertical strip and one horizontal shelf.
        sizes = [(116, 56), (56, 56), (56, 116), (56, 56)]
        result = atlas.native_layouts(self.compiler(), [sizes])[0]
        area = sum((w + 4) * (h + 4) for w, h in sizes)
        self.assertEqual(result["width"] * result["height"], area)
        self.assertEqual([(w, h) for _, _, w, h in result["regions"]], sizes)

    def test_input_permutations_preserve_geometric_layout_and_output_mapping(self):
        sizes = [(56, 96), (36, 56), (36, 36), (20, 76), (76, 20), (12, 44)]
        permutations = [sizes, list(reversed(sizes)), [sizes[i] for i in (2, 5, 0, 4, 1, 3)]]
        layouts = atlas.native_layouts(self.compiler(), permutations)
        normalized = []
        for source, layout in zip(permutations, layouts):
            self.assertEqual([(w, h) for _, _, w, h in layout["regions"]], source)
            normalized.append((layout["width"], layout["height"],
                               sorted((w, h, x, y) for x, y, w, h in layout["regions"])))
        self.assertEqual(normalized[0], normalized[1])
        self.assertEqual(normalized[0], normalized[2])

    def test_native_protocol_rejects_truncated_trailing_and_oversized_records(self):
        valid = struct.pack("<7I", 0x314B5041, 1, 1, 2, 1, 8, 8)
        cases = [valid[:-1], valid + b"x", struct.pack("<5I", 0x314B5041, 1, 1, 2, 513),
                 struct.pack("<7I", 0x314B5041, 1, 1, 2, 1, 16381, 1),
                 struct.pack("<3I", 0x314B5041, 1, 4097)]
        with tempfile.TemporaryDirectory() as temporary:
            source, target = Path(temporary) / "batch.bin", Path(temporary) / "layout.bin"
            for data in cases:
                source.write_bytes(data)
                completed = subprocess.run([str(self.compiler()), "pack-atlas-layout", "--input",
                                            str(source), "--output", str(target)], capture_output=True)
                self.assertNotEqual(completed.returncode, 0)
                self.assertFalse(target.exists())


if __name__ == "__main__":
    unittest.main()
