"""Pure Python fixtures only. No production assets are written or launched."""

from __future__ import annotations

import importlib.util
from contextlib import redirect_stderr, redirect_stdout
import io
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock
import warnings
import zipfile
import zlib


TOOL = Path(importlib.util.find_spec("build_numeric_runtime_pak64").origin)
SPEC = importlib.util.spec_from_file_location("numeric_pak64", TOOL)
pak = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pak)
_production_source_audit = pak.source_audit
pak.source_audit = lambda *args, **kwargs: _production_source_audit(
    *args, **kwargs, legacy_fixture=True)
COMPILER_SPEC = importlib.util.spec_from_file_location(
    "numeric_resource_compiler", Path(__file__).parent / "fixtures/legacy_numeric_resources.py")
compiler = importlib.util.module_from_spec(COMPILER_SPEC)
COMPILER_SPEC.loader.exec_module(compiler)
STAGE_SPEC = importlib.util.spec_from_file_location(
    "runtime_stage", TOOL.with_name("stage_runtime_package.py"))
stager = importlib.util.module_from_spec(STAGE_SPEC)
with mock.patch.dict(sys.modules, {"build_numeric_runtime_pak64": pak}):
    STAGE_SPEC.loader.exec_module(stager)
REGISTRIES = {"legacy_fixture": True, "track": 11, "image": 22, "font": 33, "emitter": 44, "slots": 880,
              "track_max": 9, "track_capacity": 256, "image_max": 2, "emitter_count": 3,
              "groups": 1, "reanim_types": 146, "attacher_range": [8, 10],
              "resource_kinds": [2] * 20 + [0] + [1] * 858,
              "image_slots": [0xFFFFFFFF, 20, 879], "legacy_image_slots": []}


def envelope(cookie, payload):
    return struct.pack("<3I", cookie, len(payload), zlib.crc32(payload)) + payload


def reanim():
    payload = struct.pack("<4IfI", 0x35544449, 11, 22, 33, 12.0, 1)
    payload += struct.pack("<4I8f2II", 0, 1, 32, 0, *([0.0] * 8), 0, 0, 0)
    return envelope(0xDEADFED7, payload)


def particle(image=1):
    body = struct.pack("<7I", image, 0, 0, 1, 0, 0, 0) + struct.pack("<40I", *([0] * 40))
    payload = struct.pack("<5I", 0x35504449, 22, 44, 1, len(body)) + body
    payload += struct.pack("<6I", 0x33544449, 1, 0xFFFFFFFF, 0x35544449, 1, 0)
    return envelope(0xDEADFED8, payload)


def font(image="data/glyph"):
    payload = struct.pack("<I", 0) + string(image)
    payload += struct.pack("<2I22iI", 0, 0, -1, *([0] * 12), 1, *([255] * 4), *([0] * 4), 0)
    return struct.pack("<4Ii3I", 0x544E464E, 1, len(payload), zlib.crc32(payload), 12, 0, 1, 0) + payload


def string(value):
    raw = value.encode()
    return struct.pack("<I", len(raw)) + raw


def resource_record(slot, **overrides):
    is_font, is_image = slot < 20, slot in (20, 879)
    fields = dict(symbol=1 if slot == 20 else 2 if slot == 879 else 0,
                  group=1 if slot < 879 else 0, type_=2 if is_font else 0 if is_image else 1,
                  flags=0 if is_font or is_image or slot == 21 else 1, reference=0xFFFFFFFF,
                  variant=0, path=f"data/Font{slot:02}.txt" if is_font else
                  "images/Actual" if is_image else "sounds/Actual" if slot == 21 else "",
                  alpha="", alpha_grid="", font_image="", tail=b"")
    fields.update(overrides)
    record = struct.pack("<7I", slot, *(fields[k] for k in
                         ("symbol", "group", "type_", "flags", "reference", "variant")))
    record += b"".join(string(fields[k]) for k in ("path", "alpha", "alpha_grid", "font_image"))
    if "body" in overrides:
        record += overrides["body"]
    elif fields["type_"] == 0:
        record += struct.pack("<iiII3i2I", 1, 1, 0xFFFFFF, 0, 0, 0, 0, 0, 0)
    elif fields["type_"] == 1:
        record += struct.pack("<di", 1.0, 0)
    else:
        record += struct.pack("<3I", 12, 0, 0)
    return record + fields["tail"]


def registries_with_holes(*slots):
    registries = {**REGISTRIES, "resource_kinds": REGISTRIES["resource_kinds"].copy(),
                  "image_slots": REGISTRIES["image_slots"].copy()}
    for slot in slots:
        registries["resource_kinds"][slot] = pak.RESOURCE_KIND_NONE
    registries["image_slots"] = [pak.RESOURCE_SLOT_NONE if slot in slots else slot
                                for slot in registries["image_slots"]]
    return registries


def resources(overrides=None, *, registries=REGISTRIES, record_slots=None, record_count=None):
    if record_slots is None:
        record_slots = [slot for slot in range(registries["slots"])
                        if slot >= len(registries["resource_kinds"])
                        or registries["resource_kinds"][slot] != pak.RESOURCE_KIND_NONE]
    records = []
    for slot in record_slots:
        record = resource_record(slot, **(overrides or {}).get(slot, {}))
        records.append(struct.pack("<I", len(record)) + record)
    payload = b"".join(records)
    return struct.pack("<9I", 0x4353524E, 1, 879, registries["image_max"] + 1,
                       registries["slots"], len(records) if record_count is None else record_count,
                       registries["image"],
                       zlib.crc32(payload), len(payload)) + payload


def reseal(name, data):
    data = bytearray(data)
    if pak.numeric_kind(name) in ("reanim", "particles"):
        struct.pack_into("<2I", data, 4, len(data) - 12, zlib.crc32(data[12:]))
    elif pak.numeric_kind(name) == "fonts":
        struct.pack_into("<2I", data, 8, len(data) - 32, zlib.crc32(data[32:]))
    else:
        struct.pack_into("<2I", data, 28, zlib.crc32(data[36:]), len(data) - 36)
    return bytes(data)


def effect(texture="lut.dds", spirv_opcode=0, source="effects/Fixture.hlsl", params=(),
           size_expr=(), pass_inputs=(0, 2), pass_outputs=(1,), samplers=(), style=0,
           output=1, format_=37):
    data = struct.pack("<2I", 0x58465356, 1) + string(source)
    data += struct.pack("<3I", 0, output, len(params))
    data += b"".join(struct.pack("<Q2I5f", *p) for p in params)
    data += struct.pack("<I", 3)
    for id_, path in ((0, ""), (1, ""), (2, texture)):
        data += struct.pack("<2I", id_, format_) + string(path)
        data += struct.pack("<I", len(size_expr))
        data += b"".join(struct.pack("<Ii", *p) for p in size_expr) + struct.pack("<I", 0)
    data += struct.pack("<I", len(samplers)) + b"".join(struct.pack("<2I", *s) for s in samplers)
    data += struct.pack("<8I", 1, 0, style, 1, 1, 1, 1, 1)
    for values in (pass_inputs, pass_outputs):
        data += struct.pack("<I", len(values))
        data += b"".join(struct.pack("<I", v) for v in values)
    words = [0x07230203, 0x10000, 0, 1, 0, (1 << 16) | spirv_opcode]
    return data + struct.pack("<I", len(words)) + struct.pack("<6I", *words)


class NonSeekable(io.BytesIO):
    def seekable(self):
        return False

    def seek(self, *args):
        raise OSError("fixture stream cannot seek")


class NumericResourceAuthoringTests(unittest.TestCase):
    def setUp(self):
        imports = mock.patch.dict(sys.modules, {"build_numeric_runtime_pak64": pak})
        imports.start()
        self.addCleanup(imports.stop)
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        generated = self.root / "pvzPerf/Sexy.TodLib/generated"
        generated.mkdir(parents=True)
        self.names = ["IMAGE_PRIMARY", "FONT_PRIMARY", "IMAGE_LEGACY", "SOUND_PRIMARY",
                      "FONT_REFERENCE", "FONT_PROGRAM", "IMAGE_ALIAS"]
        self.names += [f"SOUND_FIXTURE_{slot:03}" for slot in range(7, 879)]
        (generated / "ResourceId.h").write_text(
            "    enum class ResourceId : int\n    {\n"
            + "".join(f"        {name}_ID = {slot},\n" for slot, name in enumerate(self.names))
            + "    };\n", encoding="utf-8")
        (self.root / "pvzPerf/ResourceGroupId.h").write_text(
            "enum class ResourceGroupId { Invalid = 0,\n"
            + "".join(f"Group{group} = {group},\n" for group in range(1, 30))
            + "};\n", encoding="utf-8")
        self.sidecar = generated / "image_symbol_ids.bin"
        symbols = ["IMAGE_PRIMARY", "IMAGE_ALIAS", "IMAGE_EXTRA", "IMAGE_PROJECTILE", "IMAGE_SUN"]
        self.sidecar.write_bytes(struct.pack("<2I", 0x4953594D, len(symbols)) + b"".join(
            struct.pack("<2I", symbol, len(name)) + name.encode()
            for symbol, name in enumerate(symbols, 1)))
        (generated.parent / "ProgramImageSymbols.h").write_text(
            "enum class ImageSymbolId : std::uint32_t {\n    None = 0,\n"
            + "".join(f"    {name} = {identifier},\n"
                      for identifier, name in enumerate(symbols, 1))
            + "};\n", encoding="utf-8")

        def node(tag, **attrs):
            return {"tag": tag, "attrs": attrs, "children": []}

        nodes = [
            node("SetDefaults", path="images", idprefix="IMAGE_"),
            node("Image", id="PRIMARY", path="Actual", rows="2", cols="3", anim="pingpong",
                 alphacolor="A0B0C0", framedelay="6", begindelay="7", enddelay="8",
                 perframedelay="4, 5", framemap="0, 2", alphaimage="Alpha", alphagrid="Grid"),
            node("SetDefaults", path="data", idprefix="FONT_"),
            node("Font", id="PRIMARY", path="Font.txt", size="17", bold="", shadow="", tags="tag-a"),
            node("SetDefaults", path="images", idprefix="IMAGE_"),
            node("Image", id="LEGACY", path="!program"),
            node("SetDefaults", path="sounds", idprefix="SOUND_"),
            node("Sound", id="PRIMARY", path="Sound", volume="0.75", pan="-3"),
            node("SetDefaults", path="data", idprefix="FONT_"),
            node("Font", id="REFERENCE", path="!ref:FONT_PRIMARY", italic="", size="12"),
            node("Font", id="PROGRAM", path="!program"),
            node("SetDefaults", path="images", idprefix="IMAGE_"),
            node("Image", id="ALIAS", path="Actual", noalpha="", nopal=""),
            node("SetDefaults", path="sounds", idprefix="SOUND_"),
        ]
        nodes += [node("Sound", id=f"FIXTURE_{slot:03}", path="!program") for slot in range(7, 879)]
        self.author = {"format": "pvz-resources-authoring-v1", "manifest": {
            "tag": "ResourceManifest", "attrs": {}, "children": [
                {"tag": "Resources", "attrs": {"id": "Group1"}, "children": nodes}]}}
        self.archive_paths = ["images/Actual.png", "images/Extra.png"]

    def encoded(self, author=None):
        return json.dumps(self.author if author is None else author).encode()

    def compile(self, author=None):
        return compiler.compile_manifest(self.root, self.encoded(author), self.archive_paths,
                                         self.sidecar, {"tag-a": 1})

    def test_json_preserves_slots_flags_font_references_and_numeric_bodies(self):
        data, mapping, crc, names = self.compile()
        self.assertEqual(names, self.names)
        self.assertEqual(mapping, [compiler.NONE, 0, 6, 879, 880, 881])
        header = struct.unpack_from("<9I", data)
        self.assertEqual(header[:7], (compiler.MAGIC, 1, 879, 6, 882, 882, crc))
        self.assertEqual(header[7:], (zlib.crc32(data[36:]), len(data) - 36))
        reader = pak.Reader(data[36:])
        records = [reader.take(reader.word()) for _ in range(header[5])]
        reader.finish()
        self.assertEqual([struct.unpack_from("<I", record)[0] for record in records], list(range(882)))
        image = pak.Reader(records[0])
        self.assertEqual(image.unpack("<7I"), (0, 1, 1, 0, 1040, compiler.NONE, 0))
        self.assertEqual([image.path() for _ in range(4)],
                         ["images/Actual", "images/Alpha", "images/Grid", ""])
        self.assertEqual(image.unpack("<iiII3i"), (2, 3, 0xA0B0C0, 2, 6, 7, 8))
        self.assertEqual(image.vector(), (4, 5))
        self.assertEqual(image.vector(), (0, 2))
        image.finish()
        expected_font = struct.pack("<7I", 1, 0, 1, 2, 0, compiler.NONE, 0)
        expected_font += string("data/Font.txt") + string("") * 3 + struct.pack("<4I", 17, 9, 1, 1)
        self.assertEqual(records[1], expected_font)
        expected_sound = struct.pack("<7I", 3, 0, 1, 1, 0, compiler.NONE, 0)
        expected_sound += string("sounds/Sound") + string("") * 3 + struct.pack("<di", 0.75, -3)
        self.assertEqual(records[3], expected_sound)
        self.assertEqual(struct.unpack_from("<7I", records[4]), (4, 0, 1, 2, 4, 1, 0))
        self.assertEqual(struct.unpack_from("<7I", records[5]),
                         (5, 0, 1, 2, 1, compiler.NONE, 0))
        self.assertEqual(struct.unpack_from("<7I", records[6]),
                         (6, 2, 1, 0, 0, compiler.NONE, 0))
        for slot in (880, 881):
            self.assertEqual(struct.unpack_from("<7I", records[slot]),
                             (slot, slot - 876, 0, 0, 8192, compiler.NONE, 0))
        kinds = [0, 2, 0, 1, 2, 2, 0] + [1] * 872
        registries = {**REGISTRIES, "resource_kinds": kinds, "image_max": 5,
                      "image_slots": mapping, "slots": 882, "image": crc, "groups": 29,
                      "legacy_image_slots": [2]}
        sources = set()
        self.assertEqual(pak.validate_numeric(pak.RESOURCE_PATH, data, registries, font_sources=sources),
                         {"compiled/fonts/font.compiled"})
        self.assertEqual(sources, {"data/font.txt"})

    def test_defaults_are_ordered_and_carry_across_resource_groups(self):
        group = self.author["manifest"]["children"][0]
        next_group = {"tag": "Resources", "attrs": {"id": "Group2"},
                      "children": group["children"][3:]}
        group["children"] = group["children"][:3]
        self.author["manifest"]["children"].append(next_group)
        data, _, _, _ = self.compile()
        reader = pak.Reader(data[36:])
        reader.take(reader.word())
        record = pak.Reader(reader.take(reader.word()))
        self.assertEqual(record.unpack("<7I"), (1, 0, 2, 2, 0, compiler.NONE, 0))
        self.assertEqual(record.path(), "data/Font.txt")

    def compact_symbols(self, names):
        path = self.root / "pvzPerf/Sexy.TodLib/ProgramImageSymbols.h"
        path.write_text(
            "enum class ImageSymbolId : std::uint32_t {\n    None = 0,\n"
            + "".join(f"    {name} = {symbol},\n" for symbol, name in enumerate(names, 1))
            + "};\n", encoding="utf-8")
        return path

    def test_compact_symbols_keep_sparse_base_slots_and_dense_supplemental_slots(self):
        symbols = self.compact_symbols(["IMAGE_ALIAS", "IMAGE_EXTRA"])
        data, mapping, crc, names = compiler.compile_manifest(
            self.root, self.encoded(), self.archive_paths, symbols, {"tag-a": 1})
        self.assertEqual(names, self.names)
        self.assertEqual(mapping, [compiler.NONE, 6, 879])
        header = struct.unpack_from("<9I", data)
        self.assertEqual(header[:7], (compiler.MAGIC, 1, 879, 3, 880, 879, crc))
        reader = pak.Reader(data[36:])
        records = [reader.take(reader.word()) for _ in range(header[5])]
        reader.finish()
        slots = [struct.unpack_from("<I", record)[0] for record in records]
        self.assertEqual(slots, list(range(1, 880)))
        # Old ResourceId 0 is retired; slots 1..878 retain their old identities.
        # Historical slot 2 remains the explicitly allowed symbol-free image.
        self.assertEqual(struct.unpack_from("<2I", records[1]), (2, 0))
        self.assertEqual(struct.unpack_from("<2I", records[5]), (6, 1))
        self.assertEqual(struct.unpack_from("<2I", records[-1]), (879, 2))
        self.assertEqual(struct.unpack_from("<7I", records[3]), (4, 0, 1, 2, 4, 1, 0))

        pruned, new_mapping, kinds, removed = compiler.prune_definition_images(data, mapping, set())
        self.assertEqual(new_mapping, [compiler.NONE, 6, compiler.NONE])
        self.assertEqual(removed, {2})
        self.assertEqual(kinds[:7], [3, 2, 0, 1, 2, 2, 0])
        self.assertEqual(struct.unpack_from("<9I", pruned)[4:6], (879, 878))
        registries = {**REGISTRIES, "resource_kinds": kinds, "image_max": 2,
                      "image_slots": new_mapping, "slots": 879, "image": crc, "groups": 29,
                      "legacy_image_slots": [2]}
        self.assertEqual(pak.validate_numeric(pak.RESOURCE_PATH, pruned, registries),
                         {"compiled/fonts/font.compiled"})
        kept, kept_mapping, _, removed = compiler.prune_definition_images(data, mapping, {2})
        self.assertEqual(kept, data)
        self.assertEqual(kept_mapping, mapping)
        self.assertEqual(removed, set())

        (self.root / "pvzPerf/Resources.h").write_text("#pragma once\n", encoding="utf-8")
        compiler.emit_headers(self.root, new_mapping, crc, names, kinds)
        stable = (self.root / "pvzPerf/Sexy.TodLib/generated/ResourceId.h").read_text()
        for slot, name in enumerate(self.names):
            self.assertIn(f"{name}_ID = {slot},", stable)
        bindings = (self.root / "pvzPerf/Sexy.TodLib/generated/NumericResourceBindings.h").read_text()
        self.assertIn("NUMERIC_RESOURCE_SLOT_COUNT = 879;", bindings)
        self.assertIn("gImageSymbolResourceSlots[3]", bindings)
        compiler.emit_headers(self.root, mapping, crc, names)
        bindings = (self.root / "pvzPerf/Sexy.TodLib/generated/NumericResourceBindings.h").read_text()
        self.assertIn("NUMERIC_RESOURCE_SLOT_COUNT = 880;", bindings)
        self.assertIn("    3, 2, 0, 1, 2, 2, 0,", bindings)

    def test_compact_registry_rejects_live_legacy_image_globals_and_resource_ids(self):
        symbols = self.compact_symbols(["IMAGE_ALIAS", "IMAGE_EXTRA"])
        source = self.root / "pvzPerf/Consumer.cpp"
        for expression in ("IMAGE_PRIMARY", "ResourceId::IMAGE_PRIMARY_ID"):
            with self.subTest(expression=expression):
                source.write_text(f"auto image = {expression};\n", encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "Code image absent.*IMAGE_PRIMARY"):
                    compiler.compile_manifest(
                        self.root, self.encoded(), self.archive_paths, symbols, {"tag-a": 1})
        source.write_text("// IMAGE_PRIMARY\n/* ResourceId::IMAGE_PRIMARY_ID */\n"
                          "auto legacy = ResourceId::IMAGE_LEGACY_ID;\n", encoding="utf-8")
        compiler.compile_manifest(self.root, self.encoded(), self.archive_paths, symbols, {"tag-a": 1})

    def test_pruning_recompacts_supplemental_slots_without_filling_base_holes(self):
        symbols = self.compact_symbols(["IMAGE_ALIAS", "IMAGE_PROJECTILE", "IMAGE_EXTRA"])
        data, mapping, _, _ = compiler.compile_manifest(
            self.root, self.encoded(), self.archive_paths, symbols, {"tag-a": 1})
        self.assertEqual(mapping, [compiler.NONE, 6, 879, 880])
        pruned, mapping, kinds, removed = compiler.prune_definition_images(data, mapping, {3})
        self.assertEqual(mapping, [compiler.NONE, 6, compiler.NONE, 879])
        self.assertEqual(removed, {2})
        self.assertEqual(kinds[0], 3)
        header = struct.unpack_from("<9I", pruned)
        self.assertEqual(header[4:6], (880, 879))
        self.assertEqual(header[7:], (zlib.crc32(pruned[36:]), len(pruned) - 36))
        reader = pak.Reader(pruned[36:])
        records = [reader.take(reader.word()) for _ in range(header[5])]
        reader.finish()
        self.assertEqual(struct.unpack_from("<2I", records[-1]), (879, 3))

    def test_header_slot_count_handles_no_image_bindings(self):
        (self.root / "pvzPerf/Resources.h").write_text("#pragma once\n", encoding="utf-8")
        compiler.emit_headers(self.root, [compiler.NONE], 0, self.names)
        bindings = (self.root / "pvzPerf/Sexy.TodLib/generated/NumericResourceBindings.h").read_text()
        self.assertIn("NUMERIC_RESOURCE_SLOT_COUNT = 879;", bindings)

    def test_definition_only_images_path_is_pruned_but_code_and_quickplay_roots_survive(self):
        self.names[6] = "IMAGE_QUICKPLAY_BACK_BUTTON"
        enum_path = self.root / "pvzPerf/Sexy.TodLib/generated/ResourceId.h"
        enum_path.write_text(enum_path.read_text().replace("IMAGE_ALIAS", self.names[6]), encoding="utf-8")
        for node in self.author["manifest"]["children"][0]["children"]:
            if node["attrs"].get("id") == "ALIAS":
                node["attrs"]["id"] = "QUICKPLAY_BACK_BUTTON"
        symbols = self.compact_symbols(["IMAGE_PRIMARY", self.names[6], "IMAGE_EXTRA"])
        data, mapping, _, _ = compiler.compile_manifest(
            self.root, self.encoded(), self.archive_paths, symbols, {"tag-a": 1})
        self.assertEqual(mapping, [compiler.NONE, 0, 6, 879])

        # PRIMARY and QuickPlay even share the physical images/Actual path:
        # declaration identity, rather than directory/physical alias, controls
        # definition ownership. The independent QuickPlay root must survive.
        pruned, result, kinds, removed = compiler.prune_definition_images(
            data, mapping, set(), definition_image_symbols={1, 3})
        self.assertEqual(result, [compiler.NONE, compiler.NONE, 6, compiler.NONE])
        self.assertEqual(removed, {1, 3})
        self.assertEqual(kinds[0], 3)
        self.assertEqual(kinds[6], 0)
        self.assertEqual(struct.unpack_from("<9I", pruned)[4:6], (879, 878))

        _, result, kinds, removed = compiler.prune_definition_images(
            data, mapping, {1, 3}, definition_image_symbols={1, 3})
        self.assertEqual(result, mapping)
        self.assertEqual(kinds[0], 0)
        self.assertEqual(removed, set())

        # Existing consumers without source metadata keep the old images/ UI
        # protection, including the historical PRIMARY descriptor.
        _, result, _, removed = compiler.prune_definition_images(data, mapping, set())
        self.assertEqual(result, [compiler.NONE, 0, 6, compiler.NONE])
        self.assertEqual(removed, {3})

    def test_json_rejects_xml_and_has_no_compatibility_parser(self):
        for data in (b"<ResourceManifest/>", b"<?xml version='1.0'?><ResourceManifest/>"):
            with self.subTest(data=data):
                with self.assertRaises(ValueError):
                    compiler.compile_manifest(self.root, data, self.archive_paths, self.sidecar)

    def test_json_duplicate_members_and_duplicate_attributes_are_rejected(self):
        for data in (b'{"format":"pvz-resources-authoring-v1","format":"again","manifest":{}}',
                     b'{"attrs":{"path":"first","path":"second"}}'):
            with self.subTest(data=data):
                with self.assertRaisesRegex(ValueError, "Duplicate"):
                    compiler.read_authoring(data)

    def test_json_schema_rejects_unstructured_coerced_or_nested_author_data(self):
        changes = [
            lambda a: a.update(format="unknown"),
            lambda a: a.update(extra="discarded"),
            lambda a: a["manifest"].update(tag="Other"),
            lambda a: a["manifest"].update(children={}),
            lambda a: a["manifest"]["children"][0].update(tag="Other"),
            lambda a: a["manifest"]["children"][0].update(attrs={}),
            lambda a: a["manifest"]["children"][0]["children"][1].update(tag="Other"),
            lambda a: a["manifest"]["children"][0]["children"][1].update(children=[{}]),
            lambda a: a["manifest"]["children"][0]["children"][1]["attrs"].pop("path"),
            lambda a: a["manifest"]["children"][0]["children"][1]["attrs"].update(rows=2),
            lambda a: a["manifest"]["children"][0]["children"][1]["attrs"].update(noalpha=True),
            lambda a: a["manifest"]["children"][0]["children"][1]["attrs"].update(path="a\0b"),
        ]
        for change in changes:
            author = json.loads(self.encoded())
            change(author)
            with self.subTest(author=author["format"]):
                with self.assertRaises(ValueError):
                    self.compile(author)
        for author in ([], {}, {"format": "pvz-resources-authoring-v1", "manifest": None}):
            with self.assertRaises(ValueError):
                self.compile(author)

    def test_json_reordered_or_missing_saved_ids_and_unknown_group_fail(self):
        for action in ("reorder", "missing", "group"):
            author = json.loads(self.encoded())
            group = author["manifest"]["children"][0]
            if action == "reorder":
                group["children"][1]["attrs"]["id"] = "ALIAS"
            elif action == "missing":
                group["children"].pop()
            else:
                group["attrs"]["id"] = "Unknown"
            with self.subTest(action=action), self.assertRaisesRegex(ValueError, "ResourceId|approved"):
                self.compile(author)

    def test_json_system_font_and_unknown_control_token_are_rejected(self):
        for path in ("!sys:Arial", "!unknown", "!ref:FONT_MISSING"):
            author = json.loads(self.encoded())
            author["manifest"]["children"][0]["children"][3]["attrs"]["path"] = path
            with self.subTest(path=path), self.assertRaises((ValueError, KeyError)):
                self.compile(author)

    def test_default_json_cli_verify_is_read_only_and_old_xml_option_is_rejected(self):
        # This fixture omits a tag registry because the CLI has no tag argument.
        self.author["manifest"]["children"][0]["children"][3]["attrs"]["tags"] = ""
        data, _, _, _ = self.compile()
        author_path = self.root / "assets/definitions/resources.json"
        author_path.parent.mkdir(parents=True)
        author_path.write_bytes(self.encoded())
        archive_path, output = self.root / "input.pak64", self.root / "numeric.fixture"
        with zipfile.ZipFile(archive_path, "w") as archive:
            for path in self.archive_paths:
                archive.writestr(path, b"image fixture bytes")
        output.write_bytes(data)
        args = ["compile_numeric_resources.py", "--root", str(self.root), "--archive", str(archive_path),
                "--output", str(output), "--verify", "--headers"]
        with mock.patch("sys.argv", args), mock.patch.object(compiler, "emit_headers") as headers, \
                redirect_stdout(io.StringIO()):
            self.assertEqual(compiler.main(), 0)
        headers.assert_not_called()
        self.assertEqual(output.read_bytes(), data)
        explicit = self.root / "explicit-author.json"
        explicit.write_bytes(self.encoded())
        author_path.write_bytes(b"<ResourceManifest/>")
        with mock.patch("sys.argv", args + ["--authoring", str(explicit)]), \
                redirect_stdout(io.StringIO()):
            self.assertEqual(compiler.main(), 0)
        generate = ["compile_numeric_resources.py", "--root", str(self.root),
                    "--archive", str(archive_path), "--authoring", str(explicit)]
        with mock.patch("sys.argv", generate), redirect_stdout(io.StringIO()):
            self.assertEqual(compiler.main(), 0)
        self.assertEqual((self.root / "compiled/resources.compiled").read_bytes(), data)
        self.assertFalse((self.root / pak.LEGACY_RESOURCE_PATH).exists())
        with mock.patch("sys.argv", args), self.assertRaises(ValueError):
            compiler.main()
        self.assertEqual(output.read_bytes(), data)
        with mock.patch("sys.argv", ["compile_numeric_resources.py", "--xml", "old.xml"]), \
                redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as raised:
            compiler.main()
        self.assertEqual(raised.exception.code, 2)

    def test_retail_json_conversion_evidence_preserves_ordered_tree_and_attributes(self):
        directory = TOOL.parent / "test-data"
        # Git may check this text fixture out as CRLF on Windows; the archived
        # conversion evidence hashes its canonical LF bytes.
        data = (directory / "resources-authoring.json").read_bytes().replace(b"\r\n", b"\n")
        evidence = json.loads((directory / "resources-authoring-verification.json").read_bytes())
        self.assertEqual(evidence["authoring"]["sha256"], pak.digest(data))
        self.assertEqual(evidence["authoring"]["bytes"], len(data))
        manifest = compiler.read_authoring(data)

        def nodes(node):
            yield node
            for child in node["children"]:
                yield from nodes(child)

        ordered = list(nodes(manifest))
        self.assertEqual(evidence["group_count"], len(manifest["children"]))
        self.assertEqual(evidence["node_count"], len(ordered))
        self.assertEqual(evidence["attribute_count"], sum(len(n["attrs"]) for n in ordered))
        self.assertEqual(evidence["ordered_tree_sha256"],
                         pak.digest(json.dumps(manifest, ensure_ascii=True, separators=(",", ":")).encode()))
        self.assertTrue(evidence["deployed_numeric"]["equal_to_prior_compiler"])
        self.assertEqual(evidence["prior_xml_compiler"]["sha256"], evidence["deployed_numeric"]["sha256"])


class NumericRuntimePak64Tests(unittest.TestCase):
    def test_legacy_image_slot_without_shader_symbol_keeps_its_file_reference(self):
        registries = {**REGISTRIES, "resource_kinds": REGISTRIES["resource_kinds"].copy(),
                      "legacy_image_slots": [22]}
        registries["resource_kinds"][22] = 0
        references = set()
        data = resources({22: dict(type_=0, symbol=0, flags=0, path="images/Legacy")},
                         registries=registries)
        pak.validate_numeric(pak.RESOURCE_PATH, data, registries, references)
        self.assertIn(("image", "images/Legacy"), references)
        with self.assertRaisesRegex(ValueError, "Missing referenced image"):
            pak.physical_references("image", "images/Legacy", {"images/actual.png"})

    def test_active_mapped_image_cannot_drop_its_shader_symbol(self):
        with self.assertRaisesRegex(ValueError, "descriptor identity"):
            pak.validate_numeric(pak.RESOURCE_PATH, resources({20: dict(symbol=0)}), REGISTRIES)

    def test_dynamic_image_cannot_become_a_legacy_only_slot(self):
        with self.assertRaisesRegex(ValueError, "descriptor identity"):
            pak.validate_numeric(pak.RESOURCE_PATH, resources({879: dict(symbol=0)}), REGISTRIES)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "root"
        self.stage = self.base / "stage"
        self.source = self.base / "main.pak64"
        self.output = self.base / "release.numeric.pak64"
        self.entries = [("properties/LawnStrings.txt", b"Actual UI captions\xff\r\n"),
                        ("images/Actual.PNG", bytes(range(256))),
                        ("sounds/Actual.ogg", b"\0audio bytes\xfe"),
                        ("data/not-a-font.txt", b"Do not drop unrelated UI data"),
                        ("properties/ZombatarTOS.txt", b"Actual legal UI text"),
                        ("data/glyph.PNG", b"Fixture glyph image bytes")]
        self.entries.append(("properties/resources.xml", b"Obsolete XML bytes; never a manifest input\xff"))
        for kind, count, suffix, blob in (("reanim", 146, ".reanim", reanim()),
                                         ("particles", 106, ".xml", particle())):
            directory = "reanim" if kind == "reanim" else "particles"
            for i in range(count):
                name = f"{directory}/Asset{i:03}{suffix}"
                self.entries += [(name, b"<author-only/>"), ("compiled/" + name + ".compiled", b"old symbolic")]
                self.write(self.stage / pak.definition_path(name), blob)
        self.entries += [("particles/unused.trail", b"author trail"),
                         ("compiled/particles/unused.trail.compiled", b"old trail")]
        for i in range(6):
            self.entries += [(f"particles/LegacyOnly{i}.xml", b"unused author-only particle"),
                             (f"compiled/particles/LegacyOnly{i}.xml.compiled", b"unused old compiled")]
        for i in range(20):
            self.entries.append((f"data/Font{i:02}.txt", b"CreateLayer AuthorName;"))
            self.write(self.stage / f"compiled/fonts/Font{i:02}.compiled", font())
        self.write(self.stage / pak.RESOURCE_PATH, resources())
        self.write(self.stage / "effects/lut.dds", b"DDS actual texture bytes")
        self.write(self.stage / "effects/README.md", b"Do not package author prose")
        self.make_sources()
        self.make_archive()

    def write(self, path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data.encode() if isinstance(data, str) else data)

    def make_sources(self, registries=REGISTRIES):
        files = {
            "CMakeLists.txt": "project(Fixture)\n",
            "SexyAppFramework/graphics/vulkan/CMakeLists.txt":
                "target_compile_definitions(sexy_vulkan_effects PUBLIC SEXY_SCALE_EFFECT_AUTHORING=1)\n",
            "SexyAppFramework/graphics/d3d12/CMakeLists.txt":
                "target_compile_definitions(sexy_d3d12_effects PUBLIC SEXY_SCALE_EFFECT_AUTHORING=1)\n",
            "LawnApp.cpp": 'ParseResourcesFile("compiled/resources.compiled");\n',
            "Lawn/Fixture.cpp": '#if defined(SEXY_FONT_TEXT_COMPAT_TEST_ONLY)\nLoadDescriptor("data/Font00.txt");\n#endif\n',
            "Sexy.TodLib/Reanimator.cpp":
                'DefinitionCompiledPath(gReanimationParamArray[(int)theReanimType].mReanimFileName);\n'
                'DefinitionOpenCompiledFile(aV5Path.c_str());\n'
                '0x36544449u; 0x37564152u; ReanimationTryLoadDefinitionV6(theReanimType, aReanimDef);\n'
                + "".join(f'{{ ReanimationType::Fixture{i}, "reanim\\\\Asset{i:03}.reanim", 0 }},\n'
                          for i in range(146)),
            "Sexy.TodLib/TodParticle.cpp":
                'DefinitionCompiledPath(params[i].mParticleFileName);\n'
                'DefinitionOpenCompiledFile(path.c_str());\n'
                '0x36524449u; 0x36564150u; TodParticleTryLoadDefinitionV6(&gParticleDefArray[index], paths[index]);\n'
                + "".join(f'{{ ParticleEffect::Fixture{i}, "particles\\\\Asset{i:03}.xml" }},\n'
                          for i in range(106)),
            "Sexy.TodLib/TodCommon.cpp": "TodFontSymbolRegistryHash() { return 0x21u; }\n",
            "Sexy.TodLib/ReanimTrackId.h": "PVZ_REANIM_TRACK_TABLE_HASH = 11u;\n"
                "PVZ_REANIM_TRACK_ID_COUNT = 9;\n"
                "enum class ReanimTrackId { attacher = 8, top = 10 };\n"
                "static constexpr ReanimTrackIdRange attacher__{ReanimTrackId::attacher, ReanimTrackId::top};\n",
            "Sexy.TodLib/Reanimator.h": "kReanimActiveMaskCapacityBits = 256;\n",
            "Sexy.TodLib/generated/ImageSymbolId.h": f'IMAGE_SYMBOL_ID_MAX = {registries["image_max"]};\n',
            "ResourceGroupId.h": "RESOURCE_GROUP_COUNT = 1;\n",
            "Sexy.TodLib/generated/NumericResourceBindings.h":
                f'IMAGE_SYMBOL_REGISTRY_CRC = {registries["image"]}u;\n'
                + f'NUMERIC_RESOURCE_SLOT_COUNT = {registries["slots"]};\n'
                + f'unsigned char gNumericResourceKinds[{len(registries["resource_kinds"])}] = {{'
                + ",".join(map(str, registries["resource_kinds"])) + "};\n"
                + f'unsigned gImageSymbolResourceSlots[{len(registries["image_slots"])}] = {{'
                + ",".join(f"{v}u" for v in registries["image_slots"]) + "};\n",
            "Sexy.TodLib/generated/ParticleEmitterName.h": "kParticleEmitterNameTableHash = 44u;\n"
                "kParticleEmitterNameCount = 2u;\n",
            "SexyAppFramework/graphics/ImageFont.cpp":
                'ResolveCompiledFontPath(theFontDescFileName); "compiled/fonts/"; const std::string suffix = ".compiled"; 0x544E464Eu;\n',
            "SexyAppFramework/misc/ResourceManager.cpp":
                "return ParseNumericResourcesFile(theFilename); 0x4353524Eu;\n",
            "SexyAppFramework/effect/ScaleEffectCompiler.cpp": 'path+=".vfx"; 0x58465356u;\n',
            "SexyAppFramework/SexyAppBase.cpp":
                "sourceCompiler.CompileFile(sourcePath,{},metadata,error);\n"
                "sourceCompiler.CompileD3d12(sourcePath,{},metadata,error);\n"
                "metadata.mSourcePath=sourcePath;\n",
            "SexyAppFramework/graphics/vulkan/VulkanScalePipeline.cpp":
                "std::filesystem::path(effect.mSourcePath).parent_path()/texture.mSource;\n",
        }
        for name, content in files.items():
            base = self.root if name == "CMakeLists.txt" or name.startswith("CMake/") else self.root / "pvzPerf"
            self.write(base / name, content)

    def make_archive(self, entries=None, compression=zipfile.ZIP_STORED, descriptor=False, force_zip64=False):
        buffer = NonSeekable() if descriptor else io.BytesIO()
        with warnings.catch_warnings():
            warnings.simplefilter("ignore", UserWarning)
            with zipfile.ZipFile(buffer, "w", compression=compression) as archive:
                archive.comment = b"Original archive metadata"
                for name, data in self.entries if entries is None else entries:
                    info = zipfile.ZipInfo(name, (2009, 6, 3, 4, 5, 6))
                    info.compress_type = compression
                    info.create_system = 3
                    info.external_attr = 0o100644 << 16
                    info.internal_attr = 1
                    info.extra = struct.pack("<HH3s", 0xCAFE, 3, b"xyz")
                    info.comment = b"entry comment"
                    if force_zip64:
                        with archive.open(info, "w", force_zip64=True) as stream:
                            stream.write(data)
                    else:
                        archive.writestr(info, data)
        self.source.write_bytes(buffer.getvalue())

    def build(self, **kwargs):
        return pak.build(self.source, self.stage, self.output, self.root, **kwargs)

    def set_resource_schema(self, registries, overrides=None, **kwargs):
        self.make_sources(registries)
        self.write(self.stage / pak.RESOURCE_PATH, resources(overrides, registries=registries, **kwargs))
        image = next((symbol for symbol, slot in enumerate(registries["image_slots"])
                      if slot != pak.RESOURCE_SLOT_NONE), 0)
        for i in range(106):
            self.write(self.stage / f"compiled/particles/Asset{i:03}.compiled", particle(image))

    def assert_rejected(self, message=None):
        with self.assertRaises((ValueError, OSError, zipfile.BadZipFile)) as raised:
            self.build()
        if message:
            self.assertIn(message, str(raised.exception))
        self.assertFalse(self.output.exists())
        self.assertFalse(self.output.with_suffix(".manifest.json").exists())
        self.assertEqual(list(self.base.glob(".numeric-*")), [])

    def test_complete_package_retains_bytes_order_metadata_and_manifest(self):
        source_stamp = pak.fingerprint(self.source)
        result = self.build()
        self.assertEqual(pak.fingerprint(self.source), source_stamp)
        self.assertEqual(result["numeric_counts"], {"reanim": 146, "particles": 106, "fonts": 20, "resources": 1})
        self.assertEqual(result["font_author_sources_from_numeric_resources"],
                         [f"data/font{i:02}.txt" for i in range(20)])
        self.assertEqual(len(result["added"]), 274)
        self.assertEqual(len(result["removed"]), 539)
        self.assertEqual(len(result["retained"]), 6)
        self.assertEqual(json.loads(self.output.with_suffix(".manifest.json").read_text()), result)
        self.assertEqual(result["output"]["sha256"], pak.fingerprint(self.output)["sha256"])
        with zipfile.ZipFile(self.source) as original, zipfile.ZipFile(self.output) as output:
            self.assertEqual(output.namelist()[:5], [name for name, _ in self.entries[:5]])
            for row in result["retained"]:
                self.assertEqual(row["source_sha256"], row["output_sha256"])
                self.assertTrue(row["metadata_preserved"])
                self.assertEqual(pak.metadata(original.getinfo(row["name"])), pak.metadata(output.getinfo(row["name"])))
            self.assertFalse(any(n.endswith(".vfx") for n in output.namelist()))
            self.assertEqual(output.read("effects/lut.dds"), b"DDS actual texture bytes")
            self.assertNotIn("effects/README.md", output.namelist())
            self.assertFalse(any(name.casefold().endswith(".xml") for name in output.namelist()))
        refs = result["numeric_file_references"]
        self.assertEqual(refs[pak.RESOURCE_PATH], ["images/actual.png", "sounds/actual.ogg"])
        self.assertEqual(refs["compiled/fonts/font00.compiled"], ["data/glyph.png"])
        self.assertIn("NOT decoder", " ".join(result["verification_scope"]))

    def test_numeric_production_source_repackages_without_restoring_original_assets(self):
        first = self.build()
        self.source = self.output
        self.output = self.base / "repacked.pak64"
        result = self.build()
        self.assertEqual(result["removed"], [])
        self.assertEqual(result["added"], [])
        self.assertEqual(result["output"]["sha256"], first["output"]["sha256"])

    def test_font_and_manifest_names_migrate_offline_with_identical_bytes(self):
        self.build()
        entries = []
        with zipfile.ZipFile(self.output) as archive:
            for info in archive.infolist():
                name = info.filename
                if pak.numeric_kind(name) == "fonts":
                    name = name[:-len(".compiled")] + ".font.compiled"
                elif name == pak.RESOURCE_PATH:
                    name = pak.LEGACY_RESOURCE_PATH
                entries.append((name, archive.read(info)))
        self.make_archive(entries)
        self.stage = self.base / "all-clean-stage"
        report = stager.stage_runtime(self.source, self.stage)
        self.assertEqual(report["counts"], pak.COUNTS)
        self.output = self.base / "all-clean.pak64"
        result = self.build()
        self.assertEqual(len(result["removed"]), 21)
        self.assertEqual(len(result["added"]), 21)
        self.assertTrue(all(row["reason"] == "numeric_asset_rename" for row in result["removed"]))
        with zipfile.ZipFile(self.source) as old, zipfile.ZipFile(self.output) as new:
            self.assertIn(pak.RESOURCE_PATH, new.namelist())
            for row in result["removed"]:
                mapped = pak.migrated_numeric_path(row["name"])
                target = next(n for n in new.namelist() if n.casefold() == mapped)
                self.assertEqual(old.read(row["name"]), new.read(target))
            self.assertFalse(any(".font.compiled" in n or n == pak.LEGACY_RESOURCE_PATH
                                 for n in new.namelist()))

    def test_legacy_font_and_manifest_paths_are_forbidden_in_production_stage(self):
        for name, blob in (("compiled/fonts/Font00.font.compiled", font()),
                           (pak.LEGACY_RESOURCE_PATH, resources())):
            target = self.stage / name
            self.write(target, blob)
            with self.subTest(name=name):
                self.assert_rejected()
            target.unlink()
        with self.assertRaisesRegex(ValueError, "offline-only"):
            pak.font_path("compiled/fonts/Font00.font.compiled")

    def test_font_or_manifest_old_new_alias_collision_is_rejected(self):
        self.build()
        with zipfile.ZipFile(self.output) as archive:
            base_entries = [(i.filename, archive.read(i)) for i in archive.infolist()]
        self.output = self.base / "collision.pak64"
        for old_name, new_name in ((pak.LEGACY_RESOURCE_PATH, pak.RESOURCE_PATH),
                                   ("compiled/fonts/Font00.font.compiled",
                                    "compiled/fonts/Font00.compiled")):
            blob = next(data for name, data in base_entries if name == new_name)
            self.make_archive(base_entries + [(old_name, blob)])
            with self.subTest(old_name=old_name):
                self.assert_rejected("collision")

    def test_old_numeric_production_names_migrate_offline_with_identical_payloads(self):
        self.build()
        entries = []
        with zipfile.ZipFile(self.output) as archive:
            for info in archive.infolist():
                name = info.filename
                kind = pak.numeric_kind(name)
                if kind in ("reanim", "particles"):
                    suffix = ".reanim.v4.compiled" if kind == "reanim" else ".xml.v4.compiled"
                    name = name[:-len(".compiled")] + suffix
                entries.append((name, archive.read(info)))
        self.make_archive(entries)
        migrated_stage = self.base / "migrated-stage"
        report = stager.stage_runtime(self.source, migrated_stage)
        self.assertEqual(report["counts"], pak.COUNTS)
        for row in report["files"]:
            self.assertEqual(pak.fingerprint(migrated_stage / row["output"]),
                             {k: row[k] for k in ("bytes", "sha256")})
        self.stage = migrated_stage
        self.output = self.base / "migrated.pak64"
        result = self.build()
        self.assertEqual(len(result["removed"]), 252)
        self.assertEqual(len(result["added"]), 252)
        self.assertTrue(all(row["reason"] == "numeric_definition_rename" for row in result["removed"]))
        with zipfile.ZipFile(self.output) as archive:
            self.assertFalse(any(".xml" in n or ".v4." in n or ".reanim." in n
                                 for n in archive.namelist()))

    def test_new_stage_rejects_old_names_and_shader_binaries(self):
        for name, blob in (("compiled/reanim/Asset000.reanim.v4.compiled", reanim()),
                           ("compiled/particles/Asset000.xml.v4.compiled", particle()),
                           ("effects/unexpected.spv", b"spirv"),
                           ("shaders/game.dxil", b"dxil")):
            with self.subTest(name=name):
                target = self.stage / name
                self.write(target, blob)
                self.assert_rejected()
                target.unlink()

    def test_source_shader_binaries_removed_and_never_included_in_pak(self):
        self.entries.append(("shaders/", b""))
        for name in ("shaders/game.dxil", "shaders/game_vs.spv", "effects/builtin.spv"):
            self.entries.append((name, b"build-time shader"))
        self.make_archive()
        result = self.build()
        self.assertEqual(sum(row["reason"] == "external_shader" for row in result["removed"]), 4)
        with zipfile.ZipFile(self.output) as archive:
            self.assertFalse(any(pak.external_shader(n) for n in archive.namelist()))

    def test_stager_never_overwrites_inputs_or_existing_output(self):
        report = stager.stage_runtime(self.stage, self.base / "fresh")
        self.assertEqual(report["counts"], pak.COUNTS)
        with self.assertRaisesRegex(ValueError, "fresh staging"):
            stager.stage_runtime(self.stage, self.base / "fresh")
        with self.assertRaisesRegex(ValueError, "outside the input"):
            stager.stage_runtime(self.stage, self.stage / "nested")

    def test_stager_rejects_path_collisions_and_missing_corpus(self):
        path = self.stage / "compiled/reanim/Asset000.reanim.v4.compiled"
        self.write(path, reanim())
        with self.assertRaisesRegex(ValueError, "collision"):
            stager.stage_runtime(self.stage, self.base / "collision")
        path.unlink()
        (self.stage / "compiled/reanim/asset000.compiled").unlink()
        with self.assertRaisesRegex(ValueError, "numeric corpus"):
            stager.stage_runtime(self.stage, self.base / "incomplete")

    def test_obsolete_resource_xml_bytes_do_not_determine_font_removals(self):
        self.entries = [(name, b"\0not XML and not a font list\xff" if name == "properties/resources.xml"
                         else data) for name, data in self.entries]
        self.make_archive()
        result = self.build()
        fonts = {row["name"] for row in result["removed"] if row["reason"] == "font_dsl"}
        self.assertEqual(fonts, {f"data/Font{i:02}.txt" for i in range(20)})
        xml = next(row for row in result["removed"] if row["reason"] == "resource_xml")
        self.assertEqual(xml["sha256"], pak.digest(b"\0not XML and not a font list\xff"))

    def test_resource_xml_removal_entry_is_required_even_though_never_parsed(self):
        self.make_archive([(name, data) for name, data in self.entries
                           if name != "properties/resources.xml"])
        self.assert_rejected("removal-only entry")

    def test_unclassified_xml_source_is_rejected_instead_of_retained(self):
        for name in ("properties/Unexpected.xml", "data/Unexpected.XML", "images/Unexpected.xml"):
            with self.subTest(name=name):
                self.make_archive(self.entries + [(name, b"author-only XML bytes")])
                self.assert_rejected("Unclassified XML")

    def test_xml_author_files_anywhere_in_numeric_stage_are_rejected(self):
        for name in ("properties/resources.xml", "particles/Asset.xml", "compiled/Extra.xml",
                     "effects/Extra.XML"):
            with self.subTest(name=name):
                path = self.stage / name
                self.write(path, b"legacy XML must not enter numeric stage")
                self.assert_rejected("XML author file")
                path.unlink()

    def test_numeric_fonts_drive_source_removal_and_require_all_twenty_paths(self):
        self.write(self.stage / pak.RESOURCE_PATH, resources({0: dict(path="data/Missing.txt")}))
        self.assert_rejected("font paths")
        self.write(self.stage / pak.RESOURCE_PATH, resources({0: dict(flags=1, path="")}))
        self.assert_rejected("exactly 20")

    def test_font_source_extraction_ignores_references_program_and_inactive_fonts(self):
        sources = set()
        data = resources({1: dict(flags=1, path=""), 2: dict(flags=4, reference=0, path=""),
                          3: dict(flags=8192, path="data/Inactive.txt")})
        fonts = pak.validate_numeric(pak.RESOURCE_PATH, data, REGISTRIES, font_sources=sources)
        self.assertEqual(sources, {f"data/font{i:02}.txt" for i in range(20) if i not in (1, 2, 3)})
        self.assertEqual(fonts, {pak.font_path(path) for path in sources})

    def test_removed_xml_importer_gate_cannot_hide_parser_references(self):
        self.write(self.root / "pvzPerf/Lawn/Fixture.cpp",
                   "#ifdef SEXY_RESOURCE_XML_TEST_IMPORTER\nXMLParser parser;\n#endif\n")
        self.assert_rejected("XML importer")

    def test_reproducible_stored_archive(self):
        self.build()
        second = self.base / "second.pak64"
        pak.build(self.source, self.stage, second, self.root)
        self.assertEqual(self.output.read_bytes(), second.read_bytes())

    def test_data_descriptor_and_utf8_metadata_retained(self):
        self.entries.insert(0, ("images/\u00e9.png", b"unicode-named image"))
        self.make_archive(descriptor=True)
        result = self.build()
        self.assertTrue(all(row["metadata_preserved"] for row in result["retained"]))
        with zipfile.ZipFile(self.output) as output:
            self.assertTrue(output.getinfo("images/\u00e9.png").flag_bits & 8)
            self.assertTrue(output.getinfo("images/\u00e9.png").flag_bits & 2048)

    def test_input_output_alias_and_existing_outputs_rejected(self):
        with self.assertRaises(ValueError):
            pak.build(self.source, self.stage, self.source, self.root)
        self.output.write_bytes(b"user data")
        with self.assertRaises(ValueError):
            self.build()
        self.assertEqual(self.output.read_bytes(), b"user data")
        self.output.unlink()
        manifest = self.output.with_suffix(".manifest.json")
        manifest.write_bytes(b"user manifest")
        with self.assertRaises(ValueError):
            self.build()
        self.assertEqual(manifest.read_bytes(), b"user manifest")

    def test_runtime_name_stage_destination_and_manifest_alias_rejected(self):
        for output in (self.base / "separate/main.pak64", self.stage / "release.pak64",
                       self.root / "data/release.pak64", self.root / "images/release.pak64",
                       self.root / "sounds/release.pak64"):
            output.parent.mkdir(parents=True, exist_ok=True)
            with self.assertRaises(ValueError):
                pak.build(self.source, self.stage, output, self.root)
            self.assertFalse(output.exists())
        self.assert_rejected_with_manifest_alias()

    def assert_rejected_with_manifest_alias(self):
        with self.assertRaises(ValueError):
            self.build(manifest_path=self.output)

    def test_duplicates_casefold_and_separator_collisions_rejected(self):
        for name in ("images/Actual.PNG", "IMAGES/actual.png", "images\\Actual.PNG"):
            with self.subTest(name=name):
                self.make_archive(self.entries + [(name, b"collision")])
                self.assert_rejected("collision")

    def test_compressed_and_encrypted_input_rejected(self):
        self.make_archive(compression=zipfile.ZIP_DEFLATED)
        self.assert_rejected("non-stored")
        self.make_archive()
        raw = bytearray(self.source.read_bytes())
        central = raw.index(b"PK\x01\x02")
        struct.pack_into("<H", raw, 6, struct.unpack_from("<H", raw, 6)[0] | 1)
        struct.pack_into("<H", raw, central + 8, struct.unpack_from("<H", raw, central + 8)[0] | 1)
        self.source.write_bytes(raw)
        self.assert_rejected("Encrypted")

    def test_invalid_numeric_headers_rejected_without_fallback(self):
        for name in ("compiled/reanim/Asset000.compiled",
                     "compiled/particles/Asset000.compiled",
                     "compiled/fonts/Font00.compiled", pak.RESOURCE_PATH):
            with self.subTest(name=name):
                path = self.stage / name
                original = path.read_bytes()
                path.write_bytes(b"author data instead of numeric")
                self.assert_rejected()
                path.write_bytes(original)

    def test_stale_registry_crc_version_and_size_rejected(self):
        name = "compiled/reanim/Asset000.compiled"
        original = (self.stage / name).read_bytes()
        for offset in (0, 4, 8, 12, 16, 20, 24):
            with self.subTest(offset=offset):
                data = bytearray(original)
                data[offset] ^= 1
                if offset >= 12:
                    struct.pack_into("<I", data, 8, zlib.crc32(data[12:]))
                self.write(self.stage / name, data)
                self.assert_rejected()
        self.write(self.stage / name, original)

    def test_missing_extra_numeric_and_wrong_font_paths_rejected(self):
        path = self.stage / "compiled/fonts/Font00.compiled"
        original = path.read_bytes()
        path.unlink()
        self.assert_rejected("corpus")
        self.write(path, original)
        self.write(self.stage / "compiled/reanim/Unexpected.compiled", reanim())
        self.assert_rejected("corpus")

    def test_source_audit_allows_particle_suffix_only_in_parent_path_validator(self):
        path = self.root / "pvzPerf/Sexy.TodLib/TodParticle.cpp"
        original = path.read_text()
        self.write(path, original + '\nbool IsCanonicalParticlePath(std::string_view path) {\n'
                   'constexpr std::string_view prefix = "compiled/particles/", suffix = ".compiled";\n'
                   'return path.starts_with(prefix) && path.ends_with(suffix);\n}\n')
        numeric = {name for name in pak.stage_inventory(self.stage) if pak.numeric_kind(name)}
        pak.source_audit(self.root, {}, numeric)
        self.write(path, path.read_text() + '\nconst char* legacy = ".compiled";\n')
        with self.assertRaisesRegex(ValueError, "Legacy compiled suffix"):
            pak.source_audit(self.root, {}, numeric)

    def test_source_audit_blocks_author_reference_and_fallback(self):
        path = self.root / "pvzPerf/Lawn/Fixture.cpp"
        for text in ('p_fopen("properties/resources.xml", "rb");\n',
                     'p_fopen("particles/unused.trail", "rb");\n',
                     'DefinitionReadCompiledFile(somePath, schema, result);\n',
                     'LoadDescriptor(path);\n',
                     'p_fopen(("compiled/" + name + ".compiled").c_str(), "rb");\n',
                     '#if UNKNOWN\np_fopen("data/Font00.txt", "rb");\n#else\nok();\n#endif\n'):
            with self.subTest(text=text):
                self.write(path, text)
                self.assert_rejected()

    def test_legacy_only_particle_reference_blocks_removal(self):
        self.write(self.root / "pvzPerf/Lawn/Fixture.cpp", 'p_fopen("particles/LegacyOnly0.xml", "rb");\n')
        self.assert_rejected("production reference")

    def test_live_particle_table_cannot_reference_missing_numeric_asset(self):
        path = self.root / "pvzPerf/Sexy.TodLib/TodParticle.cpp"
        self.write(path, path.read_text() + '{ ParticleEffect::Unexpected, "particles\\\\LegacyOnly0.xml" },\n')
        self.assert_rejected("lacks numeric equivalent")

    def test_source_audit_allows_only_numeric_font_resolver(self):
        self.write(self.root / "pvzPerf/Lawn/Fixture.cpp", 'ImageFont font(&app, "data/Font00.txt");\n')
        self.build()

    def test_missing_numeric_route_and_enabled_test_importer_rejected(self):
        self.write(self.root / "pvzPerf/LawnApp.cpp", 'ParseResourcesFile("properties/resources.xml");\n')
        self.assert_rejected("route")
        self.make_sources()
        self.write(self.root / "CMakeLists.txt", "target_compile_definitions(game PRIVATE SEXY_FONT_TEXT_COMPAT_TEST_ONLY)\n")
        self.assert_rejected("disabled importer")

    def test_custom_scale_hlsl_startup_compiler_is_allowed(self):
        self.write(self.root / "CMakeLists.txt",
                   "target_compile_definitions(sexy_d3d12_effects PUBLIC SEXY_SCALE_EFFECT_AUTHORING=1)\n")
        result = self.build()
        self.assertNotIn("SEXY_SCALE_EFFECT_AUTHORING",
                         result["source_reference_audit"]["disabled_test_macros"])

    def test_old_particle_runtime_route_blocks_new_package(self):
        path = self.root / "pvzPerf/Sexy.TodLib/TodParticle.cpp"
        self.write(path, path.read_text().replace(
            "DefinitionCompiledPath(params[i].mParticleFileName)",
            'std::string(params[i].mParticleFileName) + ".v4.compiled"'))
        self.assert_rejected("route")

    def test_offline_authoring_target_cannot_link_into_runtime(self):
        text = "target_compile_definitions(compile_numeric_scale PRIVATE SEXY_SCALE_EFFECT_AUTHORING=1)\n"
        text += "target_compile_definitions(sexy_scale_authoring PUBLIC SEXY_SCALE_EFFECT_AUTHORING=1)\n"
        text += "target_link_libraries(sexy_vulkan_scale_compile_tests PRIVATE sexy_scale_authoring)\n"
        self.write(self.root / "CMakeLists.txt", text + "target_link_libraries(game PRIVATE sexy_scale_authoring)\n")
        self.assert_rejected("Production links")
        self.write(self.root / "CMakeLists.txt", text)
        self.build()

    def test_effect_missing_dependency_invalid_binary_and_debug_strings_rejected(self):
        # Offline legacy-container validation remains available for frozen evidence.
        self.write(self.stage / "effects/Fixture.hlsl.vfx", b"authored HLSL source")
        self.assert_rejected("VFX shader")
        for opcode in (2, 3, 4, 5, 6, 7, 330):
            with self.subTest(opcode=opcode):
                with self.assertRaisesRegex(ValueError, "source/debug"):
                    pak.effect_references("effects/Fixture.hlsl.vfx", effect(spirv_opcode=opcode))

    def test_effect_reference_in_source_only_retained(self):
        self.entries.append(("effects/lut.dds", b"DDS actual texture bytes"))
        self.make_archive()
        (self.stage / "effects/lut.dds").unlink()
        result = self.build()
        self.assertIn("effects/lut.dds", [r["name"] for r in result["retained"]])
        # Runtime rebasing also permits a safe DDS in another archive directory.
        self.entries.append(("images/lut.dds", b"DDS existing image-directory texture"))
        self.make_archive()
        result = pak.build(self.source, self.stage, self.base / "other-dds.pak64", self.root)
        self.assertEqual(result["effect_file_references"], {})
        self.assertIn("images/lut.dds", [r["name"] for r in result["retained"]])

    def test_absolute_effect_bake_path_preserved_and_dependencies_rebased(self):
        data = effect(source="C:/separate-author-machine/project/effects/Fixture.hlsl")
        self.entries.append(("effects/Fixture.hlsl.vfx", data))
        self.make_archive()
        result = self.build()
        self.assertEqual(result["effect_file_references"], {})
        with zipfile.ZipFile(self.output) as archive:
            self.assertNotIn("effects/Fixture.hlsl.vfx", archive.namelist())

    def test_effect_source_language_enum_is_not_source_text(self):
        data = bytearray(effect())
        struct.pack_into("<I", data, len(data) - 28, 8)
        struct.pack_into("<I", data, len(data) - 4, (3 << 16) | 3)
        data += struct.pack("<2I", 5, 600)
        self.assertEqual(pak.effect_references("effects/Fixture.hlsl.vfx", bytes(data)),
                         {"effects/lut.dds"})

    def external_effect_mirror(self, data=b"DDS actual texture bytes"):
        self.write(self.root / "effects/lut.dds", data)
        self.write(self.root / "effects/Fixture.hlsl", "// authored HLSL\n")
        self.write(self.root / "CMake/StageNumericEffects.cmake",
                   'file(GLOB_RECURSE assets "${SOURCE}/*.hlsl" "${SOURCE}/*.hlsli" "${SOURCE}/*.dds")\n'
                   'configure_file("${SOURCE}/${relative}" "${DESTINATION}/${relative}" COPYONLY)\n')
        self.write(self.root / "pvzPerf/SexyAppFramework/CMakeLists.txt",
                   'add_custom_command(TARGET game POST_BUILD "-DSOURCE=${CMAKE_SOURCE_DIR}/effects" '
                   '"-DDESTINATION=runtime/effects" -P "CMake/StageNumericEffects.cmake")\n')

    def test_retired_vfx_and_identical_external_dds_removed_but_images_audio_licenses_kept(self):
        self.external_effect_mirror()
        self.entries += [("effects/Fixture.hlsl.vfx", effect()),
                         ("effects/lut.dds", b"DDS actual texture bytes"),
                         ("effects/NOTICE.txt", b"License and attribution must survive"),
                         ("images/lookup.dds", b"Unrelated image DDS must survive")]
        self.make_archive()
        (self.stage / "effects/lut.dds").unlink()
        result = self.build()
        reasons = [row["reason"] for row in result["removed"]]
        self.assertEqual(reasons.count("retired_scale_binary"), 1)
        self.assertEqual(reasons.count("external_scale_dds"), 1)
        self.assertEqual(len(result["external_scale_dds"]), 1)
        with zipfile.ZipFile(self.output) as archive:
            for name, data in self.entries:
                if name in ("effects/NOTICE.txt", "images/lookup.dds",
                            "images/Actual.PNG", "sounds/Actual.ogg"):
                    self.assertEqual(archive.read(name), data)
            self.assertNotIn("effects/lut.dds", archive.namelist())
            self.assertNotIn("effects/Fixture.hlsl.vfx", archive.namelist())

    def test_external_dds_mismatch_or_missing_copy_never_causes_removal(self):
        self.entries.append(("effects/lut.dds", b"DDS actual texture bytes"))
        self.make_archive()
        self.external_effect_mirror(b"Different external DDS")
        result = self.build()
        self.assertFalse(result["external_scale_dds"])
        self.assertIn("effects/lut.dds", [r["name"] for r in result["retained"]])

    def test_production_binary_reader_blocks_retirement_but_offline_test_does_not(self):
        call = 'ScaleEffectBinary::Load("effects/Fixture.hlsl.vfx", metadata, error);\n'
        self.write(self.root / "pvzPerf/Lawn/Fixture.cpp", call)
        self.assert_rejected("Production scale binary reader")
        self.write(self.root / "pvzPerf/Lawn/Fixture.cpp", "")
        self.write(self.root / "pvzPerf/SexyAppFramework/graphics/vulkan/tests/Fixture.cpp", call)
        result = self.build()
        self.assertEqual(result["source_reference_audit"]["scale_binary_readers"], [])
        self.assertIn("SexyAppFramework/graphics/vulkan/tests/Fixture.cpp",
                      result["source_reference_audit"]["offline_scale_binary_readers"])

    def test_live_pak_dds_reference_and_missing_copy_contract_block_removal(self):
        self.entries.append(("effects/lut.dds", b"DDS actual texture bytes"))
        self.make_archive()
        self.external_effect_mirror()
        self.write(self.root / "pvzPerf/Lawn/Fixture.cpp", 'p_fopen("effects/lut.dds", "rb");\n')
        self.assert_rejected("production reference")
        self.write(self.root / "pvzPerf/Lawn/Fixture.cpp", "")
        self.write(self.root / "CMake/StageNumericEffects.cmake", "# DDS staging was removed\n")
        self.assert_rejected("staging contract")

    def test_stager_omits_vfx_and_mirrored_dds_without_touching_reference_package(self):
        self.build()
        with zipfile.ZipFile(self.output) as archive:
            entries = [(i.filename, archive.read(i)) for i in archive.infolist()]
        self.make_archive(entries + [("effects/Fixture.hlsl.vfx", effect())])
        self.external_effect_mirror()
        before = pak.fingerprint(self.source)
        target = self.base / "external-effects-stage"
        result = stager.stage_runtime(self.source, target, root=self.root)
        self.assertEqual(pak.fingerprint(self.source), before)
        self.assertEqual(len(result["omitted"]), 2)
        self.assertEqual(len(result["files"]), 273)
        self.assertFalse((target / "effects").exists())

    def test_disabled_startup_hlsl_compiler_blocks_new_production_pack(self):
        self.write(self.root / "pvzPerf/SexyAppFramework/graphics/vulkan/CMakeLists.txt", "")
        self.assert_rejected("Missing startup HLSL compiler definition")

    def test_changing_shader_extension_cannot_hide_a_shader_payload(self):
        for header in pak.SHADER_HEADERS:
            with self.subTest(header=header):
                self.make_archive(self.entries + [("effects/hidden.bin", header + b"shader payload")])
                self.assert_rejected("Shader payload under a non-shader filename")

    def test_staged_asset_collision_never_replaces_retained_entry(self):
        self.entries.append(("effects/lut.dds", b"different original texture"))
        self.make_archive()
        self.assert_rejected("collision differs")

    def test_numeric_references_never_escape_archive(self):
        for name in ("../outside", "/absolute", "C:/outside", "a/./b", "a//b", "a./b", "a//"):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    pak.key(name)
        with self.assertRaisesRegex(ValueError, "Unsafe"):
            pak.effect_references("effects/Fixture.hlsl.vfx", effect("../../user-data"))

    def test_zip64_offset_relocation(self):
        record = bytearray(46)
        record[:4] = b"PK\x01\x02"
        struct.pack_into("<I", record, 42, 42)
        relocated = pak.relocate_central(bytes(record), 1 << 32)
        self.assertEqual(struct.unpack_from("<I", relocated, 42)[0], 0xFFFFFFFF)
        self.assertEqual(struct.unpack_from("<HHQ", relocated, 46), (1, 8, 1 << 32))
        moved = pak.relocate_central(relocated, 123)
        self.assertEqual(struct.unpack_from("<Q", moved, 50)[0], 123)
        record = bytearray(46)
        record[:4] = b"PK\x01\x02"
        struct.pack_into("<3I", record, 20, 0xFFFFFFFF, 0xFFFFFFFF, 0)
        struct.pack_into("<H", record, 30, 20)
        record += struct.pack("<HH2Q", 1, 16, 123, 123)
        relocated = pak.relocate_central(bytes(record), 1 << 32)
        self.assertEqual(struct.unpack_from("<HH3Q", relocated, 46), (1, 24, 123, 123, 1 << 32))
        self.assertEqual(struct.unpack_from("<H", relocated, 30)[0], 28)

    def test_zip64_local_descriptors_and_central_offset_metadata(self):
        self.entries.append(("images/Late.png", b"image after removed author records"))
        self.make_archive(descriptor=True, force_zip64=True)
        with zipfile.ZipFile(self.source) as archive, self.source.open("rb") as stream:
            records = pak.central_records(archive, stream)
            offset = archive.getinfo("images/Late.png").header_offset
            records[-1] = pak.relocate_central(pak.relocate_central(records[-1], 1 << 32), offset)
            buffer = io.BytesIO(self.source.read_bytes()[:archive.start_dir])
            buffer.seek(0, os.SEEK_END)
            pak.write_end(buffer, records, archive.comment)
        self.source.write_bytes(buffer.getvalue())
        result = self.build()
        self.assertTrue(all(row["metadata_preserved"] for row in result["retained"]))
        with zipfile.ZipFile(self.source) as source, zipfile.ZipFile(self.output) as output:
            self.assertNotEqual(source.getinfo("images/Late.png").extra,
                                output.getinfo("images/Late.png").extra)
            self.assertEqual(output.read("images/Late.png"), b"image after removed author records")

    def test_production_guard_unknown_branches_and_disabled_else(self):
        text = "#ifdef SEXY_FONT_TEXT_COMPAT_TEST_ONLY\nbad();\n#else\ngood();\n#endif\n"
        self.assertNotIn("bad", pak.production_text(text))
        self.assertIn("good", pak.production_text(text))
        text = "#if UNKNOWN\nfirst();\n#else\nsecond();\n#endif\n"
        self.assertIn("first", pak.production_text(text))
        self.assertIn("second", pak.production_text(text))

    def test_font_path_resolution_accepts_already_numeric_path(self):
        self.assertEqual(pak.font_path("data\\Font00.TXT"), "compiled/fonts/font00.compiled")
        self.assertEqual(pak.font_path("compiled/fonts/Font00.compiled"),
                         "compiled/fonts/font00.compiled")
        self.assertEqual(pak.font_path("data/Font.with.dots"), "compiled/fonts/font.with.dots.compiled")
        with self.assertRaises(ValueError):
            pak.font_path("../outside.txt")
        with self.assertRaises(ValueError):
            pak.font_path("data/Font00.txt/")

    def test_numeric_body_damage_rejected_even_with_valid_crc(self):
        variants = {
            "compiled/reanim/Asset000.compiled": (reanim(), {
                32: 257, 36: 10, 40: 0, 44: 31, 48: 2, 52: 0x7FC00000, 84: 3, 88: 2, 92: 1}),
            "compiled/particles/Asset000.compiled": (particle(), {
                32: 3, 56: 5, 60: 100001, len(particle()) - 24: 0,
                len(particle()) - 20: 2, len(particle()) - 16: 1, len(particle()) - 4: 3}),
            "compiled/fonts/Font00.compiled": (font(), {
                32: 1, 32 + 8 + len("data/glyph"): 4097,
                32 + 16 + len("data/glyph"): 2}),
        }
        for name, (original, changes) in variants.items():
            for offset, value in changes.items():
                with self.subTest(name=name, offset=offset):
                    data = bytearray(original)
                    struct.pack_into("<I", data, offset, value)
                    with self.assertRaises(ValueError):
                        pak.validate_numeric(name, reseal(name, data), REGISTRIES)
            for data in (original[:-1], original + b"author identifiers"):
                with self.subTest(name=name, size=len(data)):
                    with self.assertRaises(ValueError):
                        pak.validate_numeric(name, reseal(name, data), REGISTRIES)

    def test_font_maps_layers_glyphs_and_kerning_are_bounded_and_unique(self):
        name = "compiled/fonts/Font00.compiled"
        base = font()
        glyph = struct.pack("<I8iI", 65, *([0] * 8), 0)
        valid = bytearray(base[:-4] + struct.pack("<I", 1) + glyph)
        pak.validate_numeric(name, reseal(name, valid), REGISTRIES)
        duplicate_glyphs = base[:-4] + struct.pack("<I", 2) + glyph * 2
        invalid_glyph = base[:-4] + struct.pack("<I", 1) + struct.pack("<I8iI", 128, *([0] * 8), 0)
        duplicate_kerns = base[:-4] + struct.pack("<I", 1)
        duplicate_kerns += struct.pack("<I8iI", 65, *([0] * 8), 2) + struct.pack("<IiIi", 66, 0, 66, 0)
        duplicate_layers = bytearray(base[:32] + base[32:] * 2)
        struct.pack_into("<I", duplicate_layers, 24, 2)
        duplicate_maps = bytearray(base[:32] + struct.pack("<4I", 65, 66, 65, 67) + base[32:])
        struct.pack_into("<I", duplicate_maps, 20, 2)
        invalid_tag = bytearray(base[:50] + struct.pack("<2I", 1, 1) + base[54:])
        struct.pack_into("<I", invalid_tag, 28, 1)
        negative_rect = bytearray(valid)
        struct.pack_into("<i", negative_rect, len(negative_rect) - 36, -1)
        for data in (duplicate_glyphs, invalid_glyph, duplicate_kerns, duplicate_layers,
                     duplicate_maps, invalid_tag, negative_rect):
            with self.subTest(data=bytes(data[-40:])):
                with self.assertRaises(ValueError):
                    pak.validate_numeric(name, reseal(name, data), REGISTRIES)

    def test_reanim_ui_captions_and_numeric_attacher_segments(self):
        name = "compiled/reanim/Asset000.compiled"
        data = bytearray(reanim())
        struct.pack_into("<I", data, 48, 1)
        caption = b"Actual UI caption\xff"
        data = data[:92] + struct.pack("<I", len(caption)) + caption + data[92:]
        self.assertEqual(pak.validate_numeric(name, reseal(name, data), REGISTRIES), set())
        data[96] = 0
        with self.assertRaisesRegex(ValueError, "NUL"):
            pak.validate_numeric(name, reseal(name, data), REGISTRIES)
        data = bytearray(reanim()[:-4])
        struct.pack_into("<I", data, 36, 8)
        data += struct.pack("<3I4If3I", 1, 0, 1, 0, 1, 0xFFFFFFFF, 0, 12.0, 0, 0, 1)
        pak.validate_numeric(name, reseal(name, data), REGISTRIES)
        for offset, value in ((96, 1), (104, 1), (108, 0), (112, 146),
                              (116, 10), (120, 0x7F800000), (124, 3), (128, 1), (132, 0)):
            with self.subTest(offset=offset):
                damaged = bytearray(data)
                struct.pack_into("<I", damaged, offset, value)
                with self.assertRaises(ValueError):
                    pak.validate_numeric(name, reseal(name, damaged), REGISTRIES)

    def test_particle_nonempty_tracks_fields_and_invalid_nodes(self):
        name = "compiled/particles/Asset000.compiled"
        original = particle()
        node = struct.pack("<3f2I", 0.0, 1.0, 2.0, 0, 0)
        data = bytearray(original[:60] + struct.pack("<I", 1) + node + original[64:])
        struct.pack_into("<I", data, 28, len(original) - 56 + len(node))
        pak.validate_numeric(name, reseal(name, data), REGISTRIES)
        for offset, value in ((64, 0x7FC00000), (76, 14), (80, 14)):
            with self.subTest(offset=offset):
                damaged = bytearray(data)
                struct.pack_into("<I", damaged, offset, value)
                with self.assertRaises(ValueError):
                    pak.validate_numeric(name, reseal(name, damaged), REGISTRIES)
        # The first field has two empty tracks, after the 22 system tracks.
        at = 60 + 22 * 4
        data = bytearray(original[:at] + struct.pack("<4I", 1, 1, 0, 0) + original[at + 4:])
        struct.pack_into("<I", data, 28, len(original) - 56 + 12)
        pak.validate_numeric(name, reseal(name, data), REGISTRIES)
        struct.pack_into("<I", data, at + 4, 12)
        with self.assertRaisesRegex(ValueError, "field type"):
            pak.validate_numeric(name, reseal(name, data), REGISTRIES)

    def test_resource_identity_type_bodies_paths_and_font_reference_cycles(self):
        image_body = struct.pack("<iiII3i2I", 0, 1, 0, 0, 0, 0, 0, 0, 0)
        variants = [
            {20: dict(symbol=0)}, {20: dict(symbol=2)}, {21: dict(symbol=1)},
            {20: dict(group=0)}, {879: dict(group=1)}, {21: dict(type_=0)},
            {0: dict(reference=880, flags=4)}, {0: dict(reference=21, flags=4, path="")},
            {0: dict(reference=1, flags=4, path=""), 1: dict(reference=0, flags=4, path="")},
            {0: dict(flags=6, reference=1)}, {0: dict(body=struct.pack("<3I", 0, 16, 0))},
            {0: dict(body=struct.pack("<4I", 12, 0, 1, 0))},
            {21: dict(body=struct.pack("<di", float("nan"), 0))},
            {20: dict(body=image_body)}, {20: dict(tail=b"author bytes")},
            {21: dict(body=b"")}, {20: dict(path="../outside")},
            {20: dict(path="images/Actual/")}, {0: dict(path="data/Font00.txt/")},
            {20: dict(alpha="C:/outside")}, {20: dict(alpha_grid="a/../b")},
            {21: dict(path="bad\0path")}, {0: dict(path="../Font00.txt")},
            {0: dict(font_image="images/Actual")}, {21: dict(alpha="images/Actual")},
        ]
        for changes in variants:
            with self.subTest(changes=changes):
                with self.assertRaises(ValueError):
                    pak.validate_numeric(pak.RESOURCE_PATH, resources(changes), REGISTRIES)

    def test_system_font_flag_rejected_for_active_and_program_fonts(self):
        for flags in (2, 3, 8194):
            with self.subTest(flags=flags):
                with self.assertRaisesRegex(ValueError, "font resource flags"):
                    pak.validate_numeric(pak.RESOURCE_PATH,
                                         resources({0: dict(flags=flags)}), REGISTRIES)

    def test_typed_image_hole_packages_without_record_or_symbol_binding(self):
        registries = registries_with_holes(20)
        self.set_resource_schema(registries)
        result = self.build()
        self.assertEqual(result["source_reference_audit"]["registries"], registries)
        with zipfile.ZipFile(self.output) as archive:
            data = archive.read(pak.RESOURCE_PATH)
        self.assertEqual(struct.unpack_from("<3I", data, 12), (3, 880, 879))
        self.assertEqual(registries["image_slots"], [pak.RESOURCE_SLOT_NONE, pak.RESOURCE_SLOT_NONE, 879])
        reader = pak.Reader(data[36:])
        slots = [pak.Reader(reader.take(reader.word())).word() for _ in range(879)]
        reader.finish()
        self.assertEqual(slots, [slot for slot in range(880) if slot != 20])

    def test_multiple_typed_holes_and_reserved_symbols_are_not_compacted(self):
        registries = registries_with_holes(20, 22, 23)
        registries["image_slots"] += [pak.RESOURCE_SLOT_NONE] * 3
        registries["image_max"] = 5
        self.set_resource_schema(registries)
        result = self.build()
        self.assertEqual(result["source_reference_audit"]["registries"], registries)
        with zipfile.ZipFile(self.output) as archive:
            data = archive.read(pak.RESOURCE_PATH)
        self.assertEqual(struct.unpack_from("<3I", data, 12), (6, 880, 877))

    def test_schema_with_no_image_bindings_accepts_only_explicit_empty_symbols(self):
        registries = registries_with_holes(20)
        registries["slots"] = 879
        registries["image_slots"][2] = pak.RESOURCE_SLOT_NONE
        self.set_resource_schema(registries)
        self.build()

    def test_missing_active_records_cannot_be_treated_as_implicit_holes(self):
        for registries in (REGISTRIES, registries_with_holes(22)):
            valid_slots = [slot for slot in range(registries["slots"])
                           if slot >= 879 or registries["resource_kinds"][slot] != pak.RESOURCE_KIND_NONE]
            self.make_sources(registries)
            for missing in (0, 20, 21, 23, 879):
                for advertised in (None, len(valid_slots)):
                    with self.subTest(holes=registries["resource_kinds"].count(3),
                                      missing=missing, advertised=advertised):
                        data = resources(registries=registries,
                                         record_slots=[slot for slot in valid_slots if slot != missing],
                                         record_count=advertised)
                        self.write(self.stage / pak.RESOURCE_PATH, data)
                        self.assert_rejected()

    def test_reserved_slot_record_rejected_even_when_count_and_crc_match(self):
        registries = registries_with_holes(20)
        self.set_resource_schema(registries)
        record_slots = [slot for slot in range(880) if slot != 22]
        for type_ in range(4):
            for flags in (0, 1, 8192):
                with self.subTest(type_=type_, flags=flags):
                    data = resources({20: dict(type_=type_, flags=flags)}, registries=registries,
                                     record_slots=record_slots)
                    self.write(self.stage / pak.RESOURCE_PATH, data)
                    self.assert_rejected("descriptor identity")

    def test_duplicate_record_cannot_replace_missing_active_record_in_sparse_schema(self):
        registries = registries_with_holes(22)
        self.make_sources(registries)
        for missing in (0, 20, 21, 23, 879):
            with self.subTest(missing=missing):
                record_slots = [slot for slot in range(880) if slot not in (22, missing)] + [24]
                self.write(self.stage / pak.RESOURCE_PATH,
                           resources(registries=registries, record_slots=record_slots))
                self.assert_rejected("descriptor identity")

    def test_sparse_schema_rejects_stale_and_incorrect_numeric_headers(self):
        registries = registries_with_holes(22)
        self.make_sources(registries)
        self.write(self.stage / pak.RESOURCE_PATH, resources())
        self.assert_rejected("header")
        original = resources(registries=registries)
        for offset in (0, 4, 8, 12, 16, 20, 24):
            with self.subTest(offset=offset):
                data = bytearray(original)
                data[offset] ^= 1
                self.write(self.stage / pak.RESOURCE_PATH, reseal(pak.RESOURCE_PATH, data))
                self.assert_rejected("header")

    def test_generated_hole_and_symbol_registries_reject_invalid_bindings(self):
        for symbol, slot in ((0, 879), (1, 20), (1, 21), (1, 0),
                             (1, 879), (1, 880), (2, pak.RESOURCE_SLOT_NONE)):
            with self.subTest(symbol=symbol, slot=slot):
                registries = registries_with_holes(20)
                registries["image_slots"][symbol] = slot
                self.make_sources(registries)
                self.assert_rejected("binding registries")
        for change in ("kind", "kind_count", "symbol_count", "slot_count"):
            with self.subTest(change=change):
                registries = registries_with_holes(20)
                if change == "kind":
                    registries["resource_kinds"][22] = 4
                elif change == "kind_count":
                    registries["resource_kinds"].pop()
                elif change == "symbol_count":
                    registries["image_slots"].pop()
                else:
                    registries["slots"] = 878
                self.make_sources(registries)
                self.assert_rejected("binding registries")

    def test_sparse_schema_rejects_reserved_and_wrong_descriptor_symbols(self):
        registries = registries_with_holes(20)
        self.set_resource_schema(registries)
        for symbol in (0, 1, 3, pak.RESOURCE_SLOT_NONE):
            with self.subTest(symbol=symbol):
                self.write(self.stage / pak.RESOURCE_PATH,
                           resources({879: dict(symbol=symbol)}, registries=registries))
                self.assert_rejected("descriptor identity")

    def test_font_reference_to_reserved_slot_rejected_even_if_inactive(self):
        registries = registries_with_holes(2)
        for flags in (4, 5, 8196):
            with self.subTest(flags=flags):
                data = resources({0: dict(reference=2, flags=flags, path="")}, registries=registries)
                with self.assertRaisesRegex(ValueError, "descriptor identity"):
                    pak.validate_numeric(pak.RESOURCE_PATH, data, registries)

    def test_sparse_schema_never_skips_missing_active_asset_dependencies(self):
        registries = registries_with_holes(22)
        self.set_resource_schema(registries)
        for slot, path in ((20, "images/Missing"), (21, "sounds/Missing"),
                           (0, "data/MissingFont.txt")):
            with self.subTest(slot=slot):
                self.write(self.stage / pak.RESOURCE_PATH,
                           resources({slot: dict(path=path)}, registries=registries))
                self.assert_rejected("font paths" if slot == 0 else "Missing referenced")

    def test_definition_references_to_reserved_image_symbol_rejected(self):
        registries = registries_with_holes(20)
        for name, original, offset in (("compiled/reanim/Asset000.compiled", reanim(), 84),
                                       ("compiled/particles/Asset000.compiled", particle(), 32)):
            for symbol in (0, 1, 2):
                with self.subTest(name=name, symbol=symbol):
                    data = bytearray(original)
                    struct.pack_into("<I", data, offset, symbol)
                    if symbol == 1:
                        with self.assertRaisesRegex(ValueError, "resource ID|emitter identity"):
                            pak.validate_numeric(name, reseal(name, data), registries)
                    else:
                        pak.validate_numeric(name, reseal(name, data), registries)

    def test_missing_image_audio_alpha_and_glyph_dependencies_fail_packaging(self):
        for slot, fields in ((20, dict(path="images/Missing")),
                             (21, dict(path="sounds/Missing")),
                             (20, dict(alpha="images/Missing")),
                             (20, dict(alpha_grid="images/Missing"))):
            with self.subTest(slot=slot, fields=fields):
                self.write(self.stage / pak.RESOURCE_PATH, resources({slot: fields}))
                self.assert_rejected("Missing referenced")
        self.write(self.stage / pak.RESOURCE_PATH, resources())
        self.write(self.stage / "compiled/fonts/Font00.compiled", font("data/MissingGlyph"))
        self.assert_rejected("Missing referenced")

    def test_alpha_only_image_and_sound_probe_extensions(self):
        self.assertEqual(pak.physical_references("image", "images/Actual",
                         {"images/_actual.png"}), {"images/_actual.png"})
        self.assertEqual(pak.physical_references("image", "images/Actual",
                         {"images/actual.jpg", "images/actual_.png"}),
                         {"images/actual.jpg", "images/actual_.png"})
        self.assertEqual(pak.physical_references("sound", "sounds/Actual",
                         {"sounds/actual.au"}), {"sounds/actual.au"})
        for kind, path, files in (("image", "images/Actual.bmp", {"images/actual.bmp"}),
                                  ("sound", "sounds/Actual.ogg", {"sounds/actual.ogg"})):
            with self.assertRaises(ValueError):
                pak.physical_references(kind, path, files)

    def test_unknown_compiled_or_unlisted_font_dsl_is_not_silently_retained(self):
        for name, data in (("compiled/fonts/old.compiled", b"old symbolic compiled font"),
                           ("data/Unlisted.txt", b"Define A (1); CreateLayer A;"),
                           ("data/UnlistedEOF.txt", b"CreateLayer A")):
            with self.subTest(name=name):
                self.make_archive(self.entries + [(name, data)])
                self.assert_rejected()

    def test_original_font_basename_aliases_cannot_drop_distinct_author_files(self):
        entries = []
        for name, data in self.entries:
            if name == "data/Font01.txt":
                name = "data/nested/Font00.txt"
            entries.append((name, data))
        self.make_archive(entries)
        self.write(self.stage / pak.RESOURCE_PATH, resources({1: dict(path="data/nested/Font00.txt")}))
        self.assert_rejected("basename collision")

    def test_effect_dependencies_cannot_reintroduce_author_files_or_prose(self):
        for path in ("README.md", "../properties/resources.xml",
                     "../data/Font00.txt", "../particles/Asset000.xml", "../reanim/Asset000.reanim",
                     "lut.dds/"):
            with self.subTest(path=path):
                with self.assertRaisesRegex(ValueError, "DDS"):
                    pak.effect_references("effects/Fixture.hlsl.vfx", effect(path))

    def test_effect_numeric_metadata_and_resource_graph_are_strict(self):
        param = (123, 0, 0, 0.5, 0.0, 1.0, 0.1, 0.5)
        self.assertEqual(pak.effect_references("effects/Fixture.hlsl.vfx",
                         effect(params=(param,), size_expr=((1, 0), (0, 2), (7, 0)))),
                         {"effects/lut.dds"})
        integer_param = (123, 0, 1, 0.5, 0.0, 1.0, 0.1, 1.0)
        pak.effect_references("effects/Fixture.hlsl.vfx", effect(params=(integer_param,)))
        cases = [
            dict(output=3), dict(output=0), dict(style=2), dict(format_=0),
            dict(pass_inputs=(3,)), dict(pass_inputs=(1,)), dict(pass_outputs=()),
            dict(pass_outputs=(0,)), dict(pass_outputs=(2,)), dict(pass_outputs=(1, 1)),
            dict(samplers=((2, 0),)), dict(samplers=((1, 4),)),
            dict(size_expr=((9, 0),)), dict(size_expr=((5, 0),)),
            dict(size_expr=((0, -1),)), dict(size_expr=((1, 1),)),
            dict(size_expr=((0, 1), (0, 1))),
            dict(params=(param, param)),
            dict(params=((123, 0, 1, 0.5, 0.0, 1.0, 0.1, 0.5),)),
            dict(params=((123, 0, 1, 1e20, -1e30, 1e30, 1.0, 1.0),)),
        ]
        for at, value in ((1, 1), (2, 2), (3, float("nan")), (6, 0), (7, 2.0)):
            changed = list(param)
            changed[at] = value
            cases.append(dict(params=(tuple(changed),)))
        for args in cases:
            with self.subTest(args=args):
                with self.assertRaises(ValueError):
                    pak.effect_references("effects/Fixture.hlsl.vfx", effect(**args))

    def test_staged_numeric_change_between_validation_and_fingerprint_rejected(self):
        original_validate = pak.validate_numeric
        target = self.stage / pak.RESOURCE_PATH

        def mutate(name, data, *args, **kwargs):
            result = original_validate(name, data, *args, **kwargs)
            if name == pak.RESOURCE_PATH:
                target.write_bytes(b"unvalidated replacement")
            return result

        with mock.patch.object(pak, "validate_numeric", side_effect=mutate):
            self.assert_rejected("changed after validation")

    def test_windows_unsafe_names_and_zip_symlinks_rejected(self):
        for name in ("data/CON.txt", "images/a?.png", "data/AUX", "data/LPT9.log"):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    pak.key(name)
        with zipfile.ZipFile(self.source, "a") as archive:
            info = zipfile.ZipInfo("images/link.png")
            info.create_system = 3
            info.external_attr = 0o120777 << 16
            archive.writestr(info, b"outside")
        self.assert_rejected("Symlink")

    def test_local_zip_crc_and_size_mismatch_rejected(self):
        original = self.source.read_bytes()
        for offset in (14, 18, 22):
            with self.subTest(offset=offset):
                data = bytearray(original)
                data[offset] ^= 1
                self.source.write_bytes(data)
                self.assert_rejected("Local/central ZIP")

    def test_manifest_precedes_archive_and_both_files_are_fsynced(self):
        real_link = os.link
        order = []
        manifest = self.output.with_suffix(".manifest.json")

        def observe(source, destination):
            destination = Path(destination)
            order.append(destination)
            if destination == manifest:
                self.assertFalse(self.output.exists())
            else:
                self.assertTrue(manifest.is_file())
                self.assertEqual(json.loads(manifest.read_text())["output"]["sha256"],
                                 pak.fingerprint(Path(source))["sha256"])
            return real_link(source, destination)

        with mock.patch.object(pak.os, "link", side_effect=observe), \
                mock.patch.object(pak.os, "fsync", wraps=os.fsync) as sync:
            self.build()
        self.assertEqual(order, [manifest, self.output])
        self.assertEqual(sync.call_count, 2)

    def test_publication_interruption_after_link_rolls_back_owned_files(self):
        real_link = os.link
        for destination in (self.output.with_suffix(".manifest.json"), self.output):
            with self.subTest(destination=destination):
                def interrupt(source, target):
                    real_link(source, target)
                    if Path(target) == destination:
                        raise KeyboardInterrupt("interrupted after successful link")

                with mock.patch.object(pak.os, "link", side_effect=interrupt):
                    with self.assertRaises(KeyboardInterrupt):
                        self.build()
                self.assertFalse(self.output.exists())
                self.assertFalse(self.output.with_suffix(".manifest.json").exists())
                self.assertEqual(list(self.base.glob(".numeric-*")), [])

    def test_archive_publication_race_preserves_user_file_and_removes_own_manifest(self):
        real_link = os.link

        def raced_link(source, destination):
            if Path(destination) == self.output:
                self.output.write_bytes(b"concurrent user archive")
            return real_link(source, destination)

        with mock.patch.object(pak.os, "link", side_effect=raced_link):
            with self.assertRaises(FileExistsError):
                self.build()
        self.assertEqual(self.output.read_bytes(), b"concurrent user archive")
        self.assertFalse(self.output.with_suffix(".manifest.json").exists())
        self.assertEqual(list(self.base.glob(".numeric-*")), [])

    def test_rollback_never_removes_replaced_manifest(self):
        real_link = os.link
        manifest = self.output.with_suffix(".manifest.json")

        def replace_manifest(source, destination):
            if Path(destination) == self.output:
                manifest.unlink()
                manifest.write_bytes(b"replacement user manifest")
                raise OSError("simulated output publication failure")
            return real_link(source, destination)

        with mock.patch.object(pak.os, "link", side_effect=replace_manifest):
            with self.assertRaises(OSError):
                self.build()
        self.assertEqual(manifest.read_bytes(), b"replacement user manifest")
        self.assertFalse(self.output.exists())
        self.assertEqual(list(self.base.glob(".numeric-*")), [])

    def test_fsync_and_post_publication_temp_cleanup_failures_leave_no_outputs(self):
        with mock.patch.object(pak.os, "fsync", side_effect=OSError("simulated fsync failure")):
            self.assert_rejected("fsync")
        real_unlink = Path.unlink
        failed = False

        def fail_once(path, *args, **kwargs):
            nonlocal failed
            if path.name.startswith(".numeric-") and self.output.exists() and not failed:
                failed = True
                raise OSError("simulated temporary cleanup failure")
            return real_unlink(path, *args, **kwargs)

        with mock.patch.object(Path, "unlink", new=fail_once):
            self.assert_rejected("cleanup")

    def test_staged_change_and_source_change_before_publish_rejected(self):
        real_end = pak.write_end
        for target in (self.stage / "effects/lut.dds", self.root / "pvzPerf/Lawn/Fixture.cpp", self.source):
            with self.subTest(target=target):
                original = target.read_bytes()

                def mutate(*args):
                    real_end(*args)
                    target.write_bytes(original + b"\nchanged concurrently")

                with mock.patch.object(pak, "write_end", side_effect=mutate):
                    self.assert_rejected("changed")
                target.write_bytes(original)

    def test_manifest_publication_race_preserves_user_manifest(self):
        real_link = os.link
        manifest = self.output.with_suffix(".manifest.json")

        def raced_link(source, destination):
            if Path(destination) == manifest:
                manifest.write_bytes(b"concurrent user data")
            return real_link(source, destination)

        with mock.patch.object(pak.os, "link", side_effect=raced_link):
            with self.assertRaises(FileExistsError):
                self.build()
        self.assertFalse(self.output.exists())
        self.assertEqual(manifest.read_bytes(), b"concurrent user data")
        self.assertEqual(list(self.base.glob(".numeric-*")), [])


class ProgramAtlasBindingTests(unittest.TestCase):
    def test_program_binding_parser_preserves_empty_and_rejects_duplicates(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            path = root / "pvzPerf/Sexy.TodLib/generated/DefinitionImageBindings.h"
            path.parent.mkdir(parents=True)
            path.write_text('{"compiled/reanim/a.compiled", "compiled/atlases/shared.atlas.png"},\n'
                            '{"compiled/reanim/b.compiled", ""},\n', encoding="utf-8")
            self.assertEqual(pak.read_definition_image_bindings(root),
                             {"compiled/reanim/a.compiled": "compiled/atlases/shared.atlas.png",
                              "compiled/reanim/b.compiled": ""})
            path.write_text('{"compiled/reanim/a.compiled", ""},\n' * 2, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate"):
                pak.read_definition_image_bindings(root)
            path.write_text('{"compiled/reanim/a.compiled", "../outside.png"},\n', encoding="utf-8")
            with self.assertRaises(ValueError):
                pak.read_definition_image_bindings(root)

    def test_packager_resolves_inline_atlas_from_program_binding_only(self):
        from PIL import Image
        sys.path.insert(0, str(TOOL.parent))
        import definition_atlas
        fixture = NumericRuntimePak64Tests()
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        fixture.build()
        source = fixture.base / "pixels.png"
        Image.new("RGBA", (3, 2), (10, 20, 30, 40)).save(source)
        name, image = "compiled/particles/asset000.compiled", "compiled/atlases/shared.atlas.png"
        descriptor = definition_atlas.ImageDescriptor(1, "x/pixels", "", "", 0, 1, 1, 0xFFFFFF)
        data, png, _ = definition_atlas.compile_definition(
            name, particle(), {1: descriptor}, {"x/pixels.png": source}, atlas_path=image)
        self.assertNotIn(image.encode(), data)
        fixture.write(fixture.stage / name, data)
        fixture.write(fixture.stage / image, png)
        header = fixture.root / "pvzPerf/Sexy.TodLib/generated/DefinitionImageBindings.h"
        fixture.write(header, ('{"' + name + '", "' + image + '"},\n').encode())
        candidate = fixture.base / "inline-candidate.pak64"
        report = pak.build(fixture.output, fixture.stage, candidate, fixture.root,
                           replace_definitions=True)
        self.assertIn(image, report["numeric_file_references"][name])
        with zipfile.ZipFile(candidate) as archive:
            self.assertEqual(archive.read(name), data)
            self.assertEqual(archive.read(image), png)


if __name__ == "__main__":
    unittest.main()
