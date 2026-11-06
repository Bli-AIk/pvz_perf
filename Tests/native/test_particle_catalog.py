"""Retail particle identities and the frozen old appearance conversion."""
import os as _devtools_os
from pathlib import Path as _DevToolsPath
DEVTOOLS_ROOT = _DevToolsPath(_devtools_os.environ.get("PVZ_DEVTOOLS_SOURCE_DIR", str(_DevToolsPath(__file__).resolve().parents[3] / "PvzDevTools")))
DEVTOOLS_REFERENCE = DEVTOOLS_ROOT / "tests/reference/python"
from pathlib import Path
import hashlib
import json
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ParticleCatalogTests(unittest.TestCase):
    def test_enum_and_runtime_table_share_retail_ids(self):
        enums = (ROOT / "pvzPerf/ConstEnums.h").read_text(encoding="utf-8")
        body = re.search(r"enum ParticleEffect\s*\{(.*?)\n\};", enums, re.S)[1]
        self.assertRegex(body, r"PARTICLE_NONE\s*=\s*-1")
        names = re.findall(r"^\s*(PARTICLE_\w+),\s*$", body, re.M)
        self.assertEqual(len(names), 106)
        self.assertEqual(len(set(names)), 106)
        legacy = (DEVTOOLS_ROOT / "saveConverter/src/LegacyParticleIds.inc").read_text(encoding="utf-8")
        original = re.findall(r'LEGACY_PARTICLE_ID\("?(\w+)"?\)', legacy)[:106]
        self.assertEqual(names, original)
        self.assertNotIn("PARTICLE_NONE", names)
        self.assertRegex(body, r"NUM_PARTICLES\s*$")
        source = (ROOT / "pvzPerf/Sexy.TodLib/TodParticle.cpp").read_text(encoding="utf-8")
        bindings = re.findall(
            r'\{\s*ParticleEffect::(PARTICLE_\w+),\s*("(?:\\.|[^"])*")\s*\}', source)
        self.assertEqual([name for name, _ in bindings], names)
        # Reconstruct the frozen old catalog to detect incorrect file bindings.
        metadata = (ROOT / "pvzPerf/Sexy.TodLib/ParticleImageVariants.inc").read_text(encoding="utf-8")
        historical = {name: json.loads(path) for name, path in bindings}
        for effect, component in re.findall(
                r"^PARTICLE_IMAGE_VARIANT\((\w+),\s*\w+,\s*(\w+),", metadata, re.M):
            historical[effect] = f"particles\\{component}.xml"
        signature = "\n".join(name + "\0" + path for name, path in sorted(historical.items()))
        self.assertEqual(hashlib.sha256(signature.encode()).hexdigest(),
                         "a7fb870ba25f81e99827d604d0ef5aac49340a18b18a639e3e51e32a2d953df0")

    def test_variants_are_internal_and_conversion_preserves_all_old_ids(self):
        source = (ROOT / "pvzPerf/Sexy.TodLib/TodParticle.cpp").read_text(encoding="utf-8")
        bindings = {name: json.loads(path) for name, path in re.findall(
            r'\{\s*ParticleEffect::(PARTICLE_\w+),\s*("(?:\\.|[^"])*")\s*\}', source)}
        metadata = (ROOT / "pvzPerf/Sexy.TodLib/ParticleImageVariants.inc").read_text(encoding="utf-8")
        variants = re.findall(
            r"^PARTICLE_IMAGE_VARIANT\((PARTICLE_\w+),\s*(PARTICLE_\w+),\s*(\w+),\s*\w+\)",
            metadata, re.M)
        self.assertEqual(len(variants), 56)
        for effect, base, component in variants:
            self.assertIn(base, bindings)
            self.assertNotIn(effect, bindings)
        mapping_source = (ROOT / "pvzPerf/Sexy.TodLib/generated/ParticleAppearanceSaveCatalog.h").read_text(encoding="utf-8")
        mappings = re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+)\s*\}, // (\w+)", mapping_source)
        self.assertEqual(len(mappings), 162)
        self.assertEqual([int(old) for old, *_ in mappings], list(range(162)))
        header = (ROOT / "pvzPerf/Sexy.TodLib/generated/ParticleVariantId.h").read_text(encoding="utf-8")
        generated = {}
        for root, body in re.findall(r"enum class (\w+) : std::uint32_t \{([^}]+)\}", header):
            for component, index in re.findall(r"(\w+)\s*=\s*(\d+)", body):
                if component != "Default":
                    self.assertNotIn(component, generated)
                    generated[component] = int(index)
        base_order = list(bindings)
        for old, base, variant, name in mappings:
            if name in bindings:
                self.assertEqual((int(base), int(variant)), (base_order.index(name), 0))
            else:
                family = next(base for effect, base, _ in variants if effect == name)
                members = sorted((component, effect) for effect, base, component in variants if base == family)
                ordinal = next(i for i, (_, effect) in enumerate(members, 1) if effect == name)
                self.assertEqual((int(base), int(variant)), (base_order.index(family), ordinal))
                component = next(component for component, effect in members if effect == name)
                self.assertEqual(int(variant), generated[component],
                                 "Compiled appearance enum disagrees with save conversion")


if __name__ == "__main__":
    unittest.main()
