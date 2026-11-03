"""Validate offline atlas metadata and the native static loading contract."""
from __future__ import annotations

import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import unittest
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import build_numeric_runtime_pak64 as pak
import compile_numeric_resources as resources
import legacy_numeric_resources as legacy_resources
import resource_build_config
import scene_atlas

SPEC = importlib.util.spec_from_file_location(
    "atlas_resource_pipeline", ROOT / "tools/resourcePipeline/src/build_resources.py")
pipeline = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(pipeline)
PACKAGE = Path(os.environ.get(
    "PVZ_RESOURCE_PACKAGE", ROOT / "out/x64-win32-vulkan/cache/resources/main.pak64"))
GENERATED = ROOT / "pvzPerf/Sexy.TodLib/generated"


def envelope(cookie, payload):
    return struct.pack("<3I", cookie, len(payload), zlib.crc32(payload)) + payload


def reseal(data):
    return envelope(struct.unpack_from("<I", data)[0], data[12:])


def legacy_reanim(registries, images):
    payload = struct.pack("<4IfI", 0x35544449, registries["track"], registries["image"],
                          registries["font"], 12.0, 1)
    payload += struct.pack("<4I", 0, len(images), 32, 0)
    payload += struct.pack(f"<{8 * len(images)}f", *([0.0] * (8 * len(images))))
    payload += b"".join(struct.pack("<2I", image, 1) for image in images)
    return envelope(0xDEADFED7, payload + struct.pack("<I", 0))


def legacy_particle(registries, images):
    body = b"".join(struct.pack("<7I40I", image, 1, 1, 1, 1, 1, 0, *([0] * 40))
                    for image in images)
    payload = struct.pack("<5I", 0x35504449, registries["image"], registries["emitter"],
                          len(images), len(body)) + body
    payload += struct.pack("<2I", 0x33544449, len(images))
    payload += struct.pack(f"<{len(images)}I", *([0xFFFFFFFF] * len(images)))
    payload += struct.pack("<2I", 0x35544449, len(images))
    payload += struct.pack(f"<{len(images)}I", *([0] * len(images)))
    return envelope(0xDEADFED8, payload)


class ProgramAtlasMetadataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with zipfile.ZipFile(PACKAGE) as archive:
            cls.available = set(archive.namelist())
            cls.numeric = {name for name in cls.available if pak.numeric_kind(name)}
        cls.report = json.loads(PACKAGE.with_name("manifest.json").read_text(encoding="utf-8"))
        cls.registries = pak.source_audit(ROOT, {}, cls.numeric)["registries"]
        cls.definitions = cls.report["definition_atlases"]
        cls.scenes = cls.report["scene_atlases"]
        cls.bindings = pak.read_definition_image_bindings(ROOT)
        cls.variant_bindings = pak.read_definition_image_binding_rows(ROOT)
        cls.config = resource_build_config.load(Path(os.environ.get(
            "PVZ_RESOURCE_BUILD_CONFIG", ROOT / cls.report["build_config"].replace("\\", "/"))))
        cls.member_reports, cls.member_bindings = {}, {}
        for name, definition in cls.definitions.items():
            for row in definition.get("variants", [definition]):
                member = (row.get("path") or
                          f"compiled/{pak.numeric_kind(name)}/{row['name'].casefold()}.compiled"
                          ) if "variants" in definition else name
                cls.member_reports[member] = row
                cls.member_bindings[member] = row["atlas_path"]
        for member, row in cls.scenes.items():
            cls.member_reports[member] = row
            cls.member_bindings[member] = row["atlas_path"]

    def test_complete_binding_corpus_and_empty_images(self):
        self.assertEqual(self.report["format"], "pvz-resource-build-static-v1")
        expected = {"reanim": 0, "particles": 0, "projectiles": 0, "fonts": 20, "resources": 1}
        self.assertEqual(self.report["numeric_counts"], expected)
        self.assertEqual({kind: sum(pak.numeric_kind(name) == kind for name in self.numeric)
                          for kind in expected}, expected)
        roots = {f"compiled/{directory}/{name.casefold()}.compiled"
                 for kind, directory in (("reanim", "reanim"), ("particle", "particles"))
                 for _, name in self.config.definition_orders[kind]}
        roots.add("compiled/projectiles/projectiles.compiled")
        roots.add("compiled/zombatar/zombatar.compiled")
        self.assertEqual(set(self.definitions), roots)
        self.assertEqual(set(self.bindings), roots)
        self.assertEqual(list(self.bindings), sorted(self.bindings))
        self.assertEqual(set(self.report["files"]), self.available)
        for name, definition in self.definitions.items():
            self.assertEqual(self.bindings[name], definition["atlas_path"])
            rows = definition.get("variants", [definition])
            self.assertEqual(self.variant_bindings[name], [row["atlas_path"] for row in rows])
            for row in rows:
                self.assertEqual(row["region_count"], len(row["images"]))
                if row["images"]:
                    self.assertIn(row["atlas_path"], self.available)
                    self.assertGreater(row["atlas_width"], 0)
                    self.assertGreater(row["atlas_height"], 0)
                else:
                    self.assertEqual((row["atlas_path"], row["atlas_width"],
                                      row["atlas_height"]), ("", 0, 0))

    def test_configured_groups_share_program_images_across_definition_kinds(self):
        configured = resource_build_config.resolve_atlases(self.config, set(self.member_reports))
        configured.setdefault("fixed_ui", [scene_atlas.record_path("FixedUI")])
        groups = self.report["atlas_groups"]
        grouped, mixed = set(), []
        for group, members in configured.items():
            with self.subTest(group=group):
                self.assertFalse(grouped & set(members))
                grouped.update(members)
                self.assertEqual(groups[group]["members"], members)
                path = f"compiled/atlases/{group}.atlas.png"
                self.assertEqual(groups[group]["path"], path)
                active = [name for name in members if self.member_reports[name]["images"]]
                self.assertEqual({self.member_bindings[name] for name in active},
                                 {path} if active else set())
                dimensions = {(self.member_reports[name]["atlas_width"],
                               self.member_reports[name]["atlas_height"]) for name in active}
                self.assertEqual(dimensions, {(groups[group]["width"], groups[group]["height"])}
                                 if active else set())
                sources = {image["source"] for name in members
                           for image in self.member_reports[name]["images"]}
                self.assertEqual(groups[group]["images"], len(sources))
                if {pak.numeric_kind(name) for name in members} == {"reanim", "particles"}:
                    mixed.append(group)
        standalone = set(self.member_reports) - grouped
        required = set(configured) | {name for name in standalone
                                      if self.member_reports[name]["images"]}
        self.assertTrue(required <= set(groups))
        self.assertTrue(set(groups) <= set(configured) | standalone)
        for name in standalone:
            if name in groups:
                self.assertEqual(groups[name]["members"], [name])
            else:
                self.assertFalse(self.member_reports[name]["images"])
            expected = name.removesuffix(".compiled") + ".atlas.png" if (
                self.member_reports[name]["images"]) else ""
            self.assertEqual(self.member_bindings[name], expected)
        self.assertEqual(mixed, [group for group, members in configured.items()
                                if {pak.numeric_kind(name) for name in members} ==
                                {"reanim", "particles"}])
        paths = {path for rows in self.variant_bindings.values() for path in rows if path}
        paths.update(row["atlas_path"] for row in self.scenes.values())
        self.assertEqual({name for name in self.available if name.endswith(".atlas.png")}, paths)
        self.assertEqual(len(paths), self.report["atlas_memory"]["definition_atlas_count"])
        self.assertEqual(set(self.scenes), {scene_atlas.record_path(module) for module in scene_atlas.MODULES})

    def test_native_source_hashes_and_direct_loader_bindings(self):
        summary = self.report["static_definitions"]
        self.assertEqual(summary["format"], "pvz-static-definitions-v1")
        self.assertEqual(summary["reanim"]["definitions"], len(self.config.definition_orders["reanim"]))
        self.assertEqual(summary["particle"]["definitions"], 106)
        for relative, metadata in summary["generated_files"].items():
            with self.subTest(source=relative):
                data = (GENERATED / relative).read_bytes()
                self.assertEqual(len(data), metadata["bytes"])
                self.assertEqual(hashlib.sha256(data).hexdigest(), metadata["sha256"])
        loading = (GENERATED / "GeneratedDefinitionLoading.cpp").read_text(encoding="utf-8")
        loading += (GENERATED / "ZombatarAtlasData.cpp").read_text(encoding="utf-8")
        actual = re.findall(
            r"DefinitionBindAtlasPath\(([^,]+),\s*(\d+),\s*(\d+),\s*(\"[^\"]*\")\)", loading)
        # Reanimation images are direct C++ binds; CPU particle definitions
        # carry their variant image tables in the runtime behavior record.
        from test_particle_image_variants import ParticleVariantPackageTests
        ParticleVariantPackageTests.setUpClass()
        self.assertIn("TodParticleLoadCompiledDefinition(Particles[i],ParticlePaths[i])", loading)
        for path, (_, rows) in ParticleVariantPackageTests.runtime.items():
            root = path.replace("compiled/particle-runtime/", "compiled/particles/")
            expected_rows = self.definitions[root]["variants"]
            self.assertEqual([(w,h,p) for w,h,p,_ in rows],
                             [(r["atlas_width"],r["atlas_height"],r["atlas_path"]) for r in expected_rows])
        expected = []
        for kind, directory, table in (("reanim", "reanim", "Reanimations"),
                                        ):
            for index, (_, name) in enumerate(self.config.definition_orders[kind]):
                definition = self.definitions[f"compiled/{directory}/{name.casefold()}.compiled"]
                for variant, row in enumerate(definition["variants"]):
                    if row["atlas_path"]:
                        expected.append((f"{table}[{index}].mTextures[{variant}]",
                                         str(row["atlas_width"]), str(row["atlas_height"]),
                                         json.dumps(row["atlas_path"])))
        projectile = self.definitions["compiled/projectiles/projectiles.compiled"]
        expected.append(("Projectiles.mTexture", str(projectile["atlas_width"]),
                         str(projectile["atlas_height"]), json.dumps(projectile["atlas_path"])))
        zombatar = self.definitions["compiled/zombatar/zombatar.compiled"]
        expected.append(("gZombatarAtlas.mTexture", str(zombatar["atlas_width"]),
                         str(zombatar["atlas_height"]), json.dumps(zombatar["atlas_path"])))
        self.assertEqual(summary["zombatar_images"], 209)
        self.assertCountEqual(actual, expected)
        self.assertNotRegex(loading, r"DefinitionImageBindings|DefinitionImageRects|\.compiled")
        self.assertEqual(set(summary["atlases"]), {json.loads(row[3]) for row in expected} |
                         {row["atlas_path"] for row in self.scenes.values()} |
                         {r["atlas_path"] for d in self.definitions.values() for r in d.get("variants", []) if r["atlas_path"]})
        for pattern in ("*.cpp", "*.h"):
            for path in (ROOT / "pvzPerf").rglob(pattern):
                self.assertNotRegex(path.read_text(encoding="utf-8-sig"),
                                    r'#\s*include\s*["<][^">\n]*DefinitionImageBindings\.h',
                                    str(path))

    def test_bindings_publication_is_deterministic_and_preserves_mtime(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "generated/DefinitionImageBindings.h"
            pipeline.emit_definition_image_bindings(path, self.definitions)
            original = path.read_bytes()
            os.utime(path, ns=(1_600_000_000_000_000_000,) * 2)
            before = path.stat().st_mtime_ns
            pipeline.emit_definition_image_bindings(path, dict(reversed(list(self.definitions.items()))))
            self.assertEqual(path.stat().st_mtime_ns, before)
            self.assertEqual(path.read_bytes(), original)
            self.assertEqual(list(path.parent.iterdir()), [path])
            self.assertEqual(original, (GENERATED / "DefinitionImageBindings.h").read_bytes())
        extents = pak.read_definition_atlas_extents(ROOT)
        expected = {}
        for definition in self.definitions.values():
            for row in definition.get("variants", [definition]):
                if row["atlas_path"]:
                    dimensions = (row["atlas_width"], row["atlas_height"])
                    self.assertEqual(expected.setdefault(row["atlas_path"], dimensions), dimensions)
        self.assertEqual(extents, expected)

    def test_load_plan_contract_has_no_image_registry(self):
        self.assertNotIn("image", self.registries)
        self.assertNotIn("image_max", self.registries)
        self.assertEqual(self.registries["image_count"], len(resources.read_names(ROOT)))
        with zipfile.ZipFile(PACKAGE) as archive:
            data = archive.read(pak.RESOURCE_PATH)
        header = struct.unpack_from("<8I", data)
        self.assertEqual(header[1], 4)
        self.assertEqual(header[5], self.registries["binding_crc"])
        self.assertEqual(header[6], self.registries["plan_crc"])
        self.assertTrue(all(len(fields) == 4 for _, _, fields, _, _ in pak.resource_records(data)))
        self.assertTrue(all(len(tail) == 4 for _, _, fields, _, tail in pak.resource_records(data)
                            if fields[0] == 0))


class LegacyStructuralValidationTests(unittest.TestCase):
    """Offline fixtures continue exercising bounded readers, absent from the game."""
    @classmethod
    def setUpClass(cls):
        pairs, image_crc = legacy_resources.read_symbols(
            ROOT / "pvzPerf/Sexy.TodLib/generated/image_symbol_ids.bin")
        cls.image_symbols = {symbol for symbol, _ in pairs}
        cls.registries = {"legacy_fixture": True, "track": 11, "track_max": 9,
                          "track_capacity": 256, "attacher_range": [8, 10],
                          "image": image_crc, "image_max": max(cls.image_symbols),
                          "image_slots": [pak.RESOURCE_SLOT_NONE] + [0] * max(cls.image_symbols),
                          "font": 33, "emitter": 44, "emitter_count": 3}

    def test_legacy_collector_reads_only_image_words(self):
        expected = {symbol for symbol, _ in legacy_resources.read_symbols(
            ROOT / "pvzPerf/Sexy.TodLib/generated/image_symbol_ids.bin")[0]}
        self.assertEqual(self.image_symbols, expected)
        symbol = min(self.image_symbols - {1})
        for kind, factory in (("reanim", legacy_reanim), ("particles", legacy_particle)):
            collected = set()
            pak.validate_numeric(f"compiled/{kind}/fixture.compiled",
                                 factory(self.registries, [symbol, 0, symbol]),
                                 self.registries, image_ids=collected)
            self.assertEqual(collected, {symbol})

    def test_legacy_structural_checks_remain_required_with_collector(self):
        for kind, factory in (("reanim", legacy_reanim), ("particles", legacy_particle)):
            name = f"compiled/{kind}/fixture.compiled"
            original = factory(self.registries, [0])
            invalid_image = bytearray(original)
            struct.pack_into("<I", invalid_image, 84 if kind == "reanim" else 32,
                             self.registries["image_max"] + 1)
            malformed = bytearray(original)
            cases = [invalid_image]
            if kind == "reanim":
                struct.pack_into("<f", malformed, 52, float("nan"))
                invalid_font = bytearray(original)
                struct.pack_into("<I", invalid_font, 88, 2)
                cases.append(invalid_font)
            else:
                struct.pack_into("<I", malformed, len(malformed) - 4,
                                 self.registries["emitter_count"])
                invalid_track = bytearray(original)
                struct.pack_into("<I", invalid_track, 60, 100001)
                cases.append(invalid_track)
            cases.append(malformed)
            for data in cases:
                for collector in (None, set()):
                    with self.subTest(kind=kind, collector=collector), self.assertRaises(ValueError):
                        pak.validate_numeric(name, reseal(data), self.registries, image_ids=collector)


if __name__ == "__main__":
    unittest.main()
