"""Variant source roots, corpus completeness, packaging and independent counts."""
from __future__ import annotations
import os as _devtools_os
from pathlib import Path as _DevToolsPath
DEVTOOLS_ROOT = _DevToolsPath(_devtools_os.environ.get("PVZ_DEVTOOLS_SOURCE_DIR", str(_DevToolsPath(__file__).resolve().parents[3] / "PvzDevTools")))
DEVTOOLS_REFERENCE = DEVTOOLS_ROOT / "tests/reference/python"

import importlib.util
from pathlib import Path
import sys
import unittest
import zipfile

import test_numeric_runtime_pak64 as fixtures

compiler, pak, particle = fixtures.compiler, fixtures.pak, fixtures.particle

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import verify_compact_image_symbols as compact


EXPANSION = (
    '#define PARTICLE_IMAGE_VARIANT(effect, base, component, image) \\\n'
    '\t{ ParticleEffect::effect, "particles\\\\" #component ".xml" },\n'
    '#include "ParticleImageVariants.inc"\n'
    '#undef PARTICLE_IMAGE_VARIANT\n')


class ParticleVariantSourceTests(unittest.TestCase):
    def setUp(self):
        self.fixture = fixtures.NumericRuntimePak64Tests()
        self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.root = self.fixture.root

    def table(self, count=2):
        entries = [
            f"PARTICLE_IMAGE_VARIANT(PARTICLE_VARIANT_{i}, Fixture0, Variant{i}, IMAGE_VARIANT_{i})"
            for i in range(count)
        ]
        # Restricted table requires real effect-style tokens; synthetic base
        # names are adjusted to the same form without changing base paths.
        path = self.root / "pvzPerf/Sexy.TodLib/TodParticle.cpp"
        text = path.read_text().replace("ParticleEffect::Fixture", "ParticleEffect::PARTICLE_FIXTURE_")
        self.fixture.write(path, text + EXPANSION)
        entries = [entry.replace(", Fixture0,", ", PARTICLE_FIXTURE_0,") for entry in entries]
        self.fixture.write(self.root / pak.PARTICLE_VARIANTS, "// Author metadata\n" + "\n".join(entries))
        for i in range(count):
            self.fixture.write(self.fixture.stage / f"compiled/particles/variant{i}.compiled", particle())

    def audit(self):
        numeric = {name for name in pak.stage_inventory(self.fixture.stage) if pak.numeric_kind(name)}
        return pak.source_audit(self.root, {}, numeric)

    def test_sources_without_variant_table_keep_original_expected_count(self):
        self.assertEqual(pak.expected_counts(self.root), pak.COUNTS)
        self.assertEqual(len(self.audit()["numeric_definition_tables"]["particles"]), 106)

    def test_restricted_table_expands_roots_and_fingerprint_without_code_image_roots(self):
        self.table()
        source = self.root / "pvzPerf/Sexy.TodLib/TodParticle.cpp"
        # The real Windows checkout uses CRLF, including macro continuations.
        source.write_bytes(source.read_bytes().replace(b"\n", b"\r\n"))
        audit = self.audit()
        paths = audit["numeric_definition_tables"]["particles"]
        self.assertEqual(len(paths), 108)
        self.assertIn("compiled/particles/variant0.compiled", paths)
        self.assertIn("Sexy.TodLib/ParticleImageVariants.inc", {r["name"] for r in audit["files"]})
        self.assertEqual(compiler.code_image_roots(self.root, {1: "IMAGE_VARIANT_0"}), set())
        table = self.root / pak.PARTICLE_VARIANTS
        self.fixture.write(table, table.read_text() + "\n// fingerprint changes with metadata\n")
        self.assertNotEqual(audit["fingerprint"], self.audit()["fingerprint"])

    def test_56_new_records_package_from_legacy_archive_and_repack_numeric_base(self):
        self.table(56)
        report = self.fixture.build()
        self.assertEqual(report["numeric_counts"], {**pak.COUNTS, "particles": 162})
        with zipfile.ZipFile(self.fixture.output) as archive:
            for i in range(56):
                self.assertEqual(archive.read(f"compiled/particles/variant{i}.compiled"), particle())
        # An earlier numeric production package still has only the original 106.
        baseline = fixtures.NumericRuntimePak64Tests()
        baseline.setUp()
        self.addCleanup(baseline.doCleanups)
        baseline.build()
        output = self.fixture.base / "expanded-numeric.pak64"
        repacked = pak.build(baseline.output, self.fixture.stage, output, self.root)
        self.assertEqual(repacked["numeric_counts"]["particles"], 162)

    def test_original_base_is_required_even_when_total_count_is_correct(self):
        self.table()
        (self.fixture.stage / "compiled/particles/asset001.compiled").unlink()
        self.fixture.write(self.fixture.stage / "compiled/particles/unregistered.compiled", particle())
        with self.assertRaisesRegex(ValueError, "lacks numeric equivalent"):
            self.audit()

    def test_duplicate_or_missing_base_table_literal_still_fails(self):
        self.table()
        path = self.root / "pvzPerf/Sexy.TodLib/TodParticle.cpp"
        self.fixture.write(path, path.read_text().replace("Asset001.xml", "Asset000.xml"))
        with self.assertRaisesRegex(ValueError, "original 106-entry|identities or paths are duplicated"):
            self.audit()

    def test_explicit_v3_catalog_checks_variant_bindings(self):
        self.table()
        path = self.root / "pvzPerf/Sexy.TodLib/TodParticle.cpp"
        literals = "".join(
            f'{{ ParticleEffect::PARTICLE_VARIANT_{i}, "particles\\\\Variant{i}.xml" }},\n'
            for i in range(2))
        source = path.read_text().replace(EXPANSION, literals)
        self.fixture.write(path, source)
        self.assertEqual(len(self.audit()["numeric_definition_tables"]["particles"]), 108)
        source = source.replace('"particles\\\\Variant0.xml"', '"particles\\\\SWAP.xml"')
        source = source.replace('"particles\\\\Variant1.xml"', '"particles\\\\Variant0.xml"')
        source = source.replace('"particles\\\\SWAP.xml"', '"particles\\\\Variant1.xml"')
        self.fixture.write(path, source)
        with self.assertRaisesRegex(ValueError, "incorrect runtime binding"):
            self.audit()

    def test_variant_record_is_required_and_unknown_record_is_rejected(self):
        self.table()
        path = self.fixture.stage / "compiled/particles/variant1.compiled"
        path.unlink()
        with self.assertRaisesRegex(ValueError, "variant lacks numeric equivalent"):
            self.audit()
        self.fixture.write(path, particle())
        self.fixture.write(self.fixture.stage / "compiled/particles/unknown.compiled", particle())
        with self.assertRaisesRegex(ValueError, "exactly match"):
            self.audit()

    def test_restricted_table_rejects_arbitrary_cpp_unknown_bases_and_duplicates(self):
        self.table()
        path = self.root / pak.PARTICLE_VARIANTS
        original = path.read_text()
        cases = [
            (original + "\n#include \"extra.inc\"\n", "Malformed"),
            (original + "\nint arbitrary = 1;\n", "Malformed"),
            (original + "\n" + original, "Duplicate"),
            (original.replace("Variant1", "variant0"), "Duplicate"),
            (original.replace("PARTICLE_FIXTURE_0", "PARTICLE_MISSING"), "unknown base"),
            (original.replace("PARTICLE_VARIANT_0", "PARTICLE_FIXTURE_0"), "reused effect"),
            (original.replace("Variant0", "Asset000"), "aliases an original"),
            ("// no entries\n", "Empty"),
        ]
        for text, message in cases:
            with self.subTest(message=message):
                self.fixture.write(path, text)
                with self.assertRaisesRegex(ValueError, message):
                    self.audit()

    def test_missing_table_or_changed_runtime_macro_cannot_hide_variant_roots(self):
        self.table()
        path = self.root / "pvzPerf/Sexy.TodLib/TodParticle.cpp"
        original = path.read_text()
        self.fixture.write(path, original.replace('#component ".xml"', '"Wrong.xml"'))
        with self.assertRaisesRegex(ValueError, "Unsupported.*expansion"):
            self.audit()
        self.fixture.write(path, original)
        (self.root / pak.PARTICLE_VARIANTS).unlink()
        with self.assertRaisesRegex(ValueError, "include disagree"):
            self.audit()

    def test_compact_record_count_comes_from_table_and_still_requires_base_106(self):
        directory = self.fixture.base / "compact-particles"
        directory.mkdir()
        for i in range(106):
            self.fixture.write(directory / f"Asset{i}.xml.v4.compiled", b"fixture")
        self.assertEqual(len(compact.record_paths(directory, "particles", root=self.root)), 106)
        self.table(56)
        with self.assertRaisesRegex(ValueError, "expected 162 records"):
            compact.record_paths(directory, "particles", root=self.root)
        for i in range(56):
            self.fixture.write(directory / f"Variant{i}.xml.v4.compiled", b"fixture")
        self.assertEqual(len(compact.record_paths(directory, "particles", root=self.root)), 162)
        (directory / "Variant0.xml.v4.compiled").rename(directory / "Unexpected.xml.v4.compiled")
        with self.assertRaisesRegex(ValueError, "missing declared image-variant"):
            compact.record_paths(directory, "particles", root=self.root)
        (directory / "Unexpected.xml.v4.compiled").rename(directory / "Variant0.xml.v4.compiled")
        (directory / "Asset0.xml.v4.compiled").unlink()
        with self.assertRaisesRegex(ValueError, "expected 162 records"):
            compact.record_paths(directory, "particles", root=self.root)

    def test_definition_bindings_use_normalized_paths_and_do_not_reorder_emitters(self):
        self.table()
        pipeline_path = DEVTOOLS_REFERENCE / "resourcePipeline/src/build_resources.py"
        spec = importlib.util.spec_from_file_location("variant_pipeline", pipeline_path)
        pipeline = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(pipeline)
        target = self.root / "pvzPerf/Sexy.TodLib/generated/DefinitionImageBindings.h"
        pipeline.emit_definition_image_bindings(target, {
            "compiled/particles/variant1.compiled": {
                "atlas_path": "compiled/atlases/shared.atlas.png", "atlas_width": 64, "atlas_height": 32},
            "compiled/particles/variant0.compiled": {
                "atlas_path": "compiled/atlases/shared.atlas.png", "atlas_width": 64, "atlas_height": 32},
        })
        bindings = pak.read_definition_image_bindings(self.root)
        self.assertEqual(list(bindings), ["compiled/particles/variant0.compiled",
                                          "compiled/particles/variant1.compiled"])
        self.assertEqual(set(bindings.values()), {"compiled/atlases/shared.atlas.png"})


if __name__ == "__main__":
    unittest.main()
