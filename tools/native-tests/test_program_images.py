"""Ordinary named image pointers remain independent of the saved ResourceId ABI."""
import os as _devtools_os
from pathlib import Path as _DevToolsPath
DEVTOOLS_ROOT = _DevToolsPath(_devtools_os.environ.get("PVZ_DEVTOOLS_SOURCE_DIR", str(_DevToolsPath(__file__).resolve().parents[3] / "PvzDevTools")))
DEVTOOLS_REFERENCE = DEVTOOLS_ROOT / "tests/reference/python"
import hashlib
from pathlib import Path
import re
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import compile_numeric_resources as resources
import legacy_numeric_resources as legacy

ROOT = Path(__file__).resolve().parents[2]


class ProgramImagesTests(unittest.TestCase):
    def test_program_declarations_contain_exactly_live_named_pointers(self):
        names = resources.read_names(ROOT)
        bindings = (ROOT / "pvzPerf/Sexy.TodLib/generated/NumericResourceBindings.h").read_text("utf-8")
        image_count = re.search(r"RESOURCE_LOAD_IMAGE_COUNT\s*=\s*(\d+)", bindings)
        self.assertIsNotNone(image_count)
        self.assertEqual(len(names), int(image_count[1]))
        self.assertEqual(len(set(names)), len(names))
        self.assertFalse(any(name.startswith(("IMAGE_ZOMBATAR_", "IMAGE_QUICKPLAY_")) for name in names))
        roots = resources.code_image_roots(ROOT, names)
        self.assertNotIn("IMAGE_REANIM_ZOMBIE_CATAPULT_BASKETBALL", names)
        self.assertEqual(set(names), resources.production_image_roots(names, roots))
        self.assertFalse((ROOT / "pvzPerf/Sexy.TodLib/ProgramImageSymbols.h").exists())
        for name in ("COBCANNON_POPCORN", "DIRTBIG", "DIRTSMALL", "ICE_SPARKLES",
                     "ICETRAP_PARTICLES", "MELONPULT_PARTICLES", "ROCKSMALL",
                     "WATERPARTICLE", "WINTERMELON_PARTICLES"):
            self.assertNotIn("IMAGE_" + name, names)

    def test_saved_resource_ids_preserve_the_released_order(self):
        frozen = DEVTOOLS_ROOT / "saveConverter/src/FrozenResourceIds.h"
        pairs = re.findall(r"\b(\w+)_ID\s*=\s*(\d+)\s*,",
                           frozen.read_text(encoding="utf-8"))
        names = [name for name, _ in pairs]
        self.assertEqual(len(names), 879)
        self.assertEqual([int(value) for _, value in pairs], list(range(879)))
        self.assertEqual(
            hashlib.sha256("\n".join(names).encode("utf-8")).hexdigest(),
            "92e7703a146ea92879c2ca0dc9ed8cab771acf20a7f48f557f6db1346bf7fad1")

    def test_converter_history_does_not_depend_on_runtime_resource_ids(self):
        directory = DEVTOOLS_ROOT / "saveConverter/src"
        for path in list(directory.glob("*.h")) + list(directory.glob("*.cpp")):
            with self.subTest(path=path.name):
                source = path.read_text(encoding="utf-8")
                self.assertNotRegex(source, r'#include\s*[<"][^>"]*(?:Resources|ResourceId)\.h[>"]')
                self.assertNotIn("Sexy::ResourceId", source)
                self.assertNotIn("Sexy::IsImageResourceId", source)
        generator = (ROOT / "tools/generate_reanim_appearances.py").read_text(encoding="utf-8")
        self.assertIn('DEVTOOLS_ROOT / "saveConverter/src/FrozenResourceIds.h"', generator)
        self.assertNotIn("Sexy.TodLib/generated/ResourceId.h", generator)

    def test_historical_registry_remains_available_for_migration_audits(self):
        historical = legacy.read_symbols(
            ROOT / "pvzPerf/Sexy.TodLib/generated/image_symbol_ids.bin")
        self.assertEqual(len(historical[0]), 2287)
        self.assertEqual(historical[1], 0x3AC4FECD)
        self.assertEqual(historical[0][1042], (1043, "IMAGE_REANIM_TANGLEKELP_ARM 8"))
        self.assertEqual(historical[0][1153], (1154, "IMAGE_REANIM_ZENGARDEN-WATERINGCAN4"))

    def test_program_declarations_do_not_count_as_code_resource_uses(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            library = root / "pvzPerf/Sexy.TodLib"
            library.mkdir(parents=True)
            (root / "pvzPerf/Resources.h").write_text(
                "extern Image* IMAGE_DECLARED;\nextern Image* IMAGE_USED;\n")
            (library / "consumer.cpp").write_text(
                "// IMAGE_DECLARED is only a comment\n"
                "auto image = IMAGE_USED;\n")
            self.assertEqual(resources.code_image_roots(
                root, resources.read_names(root)), {"IMAGE_USED"})

    def test_declaration_order_has_no_numeric_identity(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "pvzPerf").mkdir()
            header = root / "pvzPerf/Resources.h"
            header.write_text("extern Image* IMAGE_TWO;\nextern Image* IMAGE_ONE;\n")
            self.assertEqual(resources.read_names(root), ["IMAGE_TWO", "IMAGE_ONE"])
            header.write_text("extern Image* IMAGE_ONE;\nextern Image* IMAGE_TWO;\n")
            self.assertEqual(set(resources.read_names(root)), {"IMAGE_ONE", "IMAGE_TWO"})

    def test_duplicate_declarations_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "pvzPerf").mkdir()
            (root / "pvzPerf/Resources.h").write_text(
                "extern Image* IMAGE_ONE;\nextern Image* IMAGE_ONE;\n")
            with self.assertRaisesRegex(ValueError, "unique"):
                resources.read_names(root)


if __name__ == "__main__":
    unittest.main()
