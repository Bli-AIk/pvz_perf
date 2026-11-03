"""Exercise the actual birth-time zombie particle selectors without the game."""
from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
ZOMBIE = ROOT / "pvzPerf/Lawn/Zombie.cpp"
HEAD = {
    name: name for name in (
        "POLEVAULTER", "FOOTBALL", "DANCER", "BACKUP_DANCER", "SNORKEL",
        "BOBSLED", "DOLPHIN_RIDER", "DIGGER", "YETI", "LADDER", "IMP")
}
ARM = {
    "POLEVAULTER": "HAND", "NEWSPAPER": "NEWSPAPER", "FOOTBALL": "FOOTBALL",
    "DANCER": "DANCER", "BACKUP_DANCER": "BACKUP_DANCER", "BOBSLED": "BOBSLED",
    "DOLPHIN_RIDER": "HAND", "JACK_IN_THE_BOX": "JACKBOX", "BALLOON": "HAND",
    "DIGGER": "DIGGER", "POGO": "HAND", "YETI": "YETI", "LADDER": "HAND",
    "IMP": "IMP",
}
COMPONENT = {
    "POLEVAULTER": "Polevaulter", "FOOTBALL": "Football", "DANCER": "Dancer",
    "BACKUP_DANCER": "BackupDancer", "SNORKEL": "Snorkel", "BOBSLED": "Bobsled",
    "DOLPHIN_RIDER": "DolphinRider", "DIGGER": "Digger", "YETI": "Yeti",
    "LADDER": "Ladder", "IMP": "Imp", "HAND": "Hand", "NEWSPAPER": "Newspaper",
    "JACKBOX": "Jackbox",
}
ROOT_COMPONENT = {
    "ZOMBIE_HEAD": "ZombieHead", "MOWERED_ZOMBIE_HEAD": "MoweredZombieHead",
    "ZOMBIE_HEAD_POOL": "ZombieHeadPool", "ZOMBIE_ARM": "ZombieArm",
    "MOWERED_ZOMBIE_ARM": "MoweredZombieArm",
}


def selector(source, part):
    start = source.index(f"struct Zombie{part}ParticleVariant")
    end = source.index("} // namespace" if part == "Head" else "}  // namespace", start)
    return source[start:end]


def compile_probe(source):
    enums = (ROOT / "pvzPerf/ConstEnums.h").read_text(encoding="utf-8")
    types = re.search(r"enum ZombieType\s*\{.*?\n\};", enums, re.S).group()
    effects = re.search(r"enum ParticleEffect\s*\{.*?\n\};", enums, re.S).group()
    names = re.findall(r"^\s*(ZOMBIE_\w+)\s*[,=]", types, re.M)
    names = [name for name in names if name not in ("ZOMBIE_INVALID", "ZOMBIE_CACHED_POLEVAULTER_WITH_POLE")]
    # The commented console-only entries are not enum values.
    names = [name for name in names if name not in ("ZOMBIE_TARGET", "ZOMBIE_TRASH_CAN", "ZOMBIE_GRAVESTONE")]
    checks = []
    for part, mapping, bases in (
        ("Head", HEAD, ("ZOMBIE_HEAD", "MOWERED_ZOMBIE_HEAD", "ZOMBIE_HEAD_POOL")),
        ("Arm", ARM, ("ZOMBIE_ARM", "MOWERED_ZOMBIE_ARM")),
    ):
        for name in names:
            suffix = mapping.get(name.removeprefix("ZOMBIE_"))
            for base in bases:
                component = ROOT_COMPONENT[base]
                expected = (f"static_cast<std::uint32_t>(ParticleVariants::{component}::"
                            f"{component + COMPONENT[suffix] if suffix else 'Default'})")
                checks.append(
                    f"static_assert(GetZombie{part}ParticleVariant(PARTICLE_{base}, {name})"
                    f" == {expected});")
        for base in bases:
            for invalid in ("ZOMBIE_INVALID", "NUM_ZOMBIE_TYPES", "NUM_CACHED_ZOMBIE_TYPES"):
                checks.append(
                    f"static_assert(GetZombie{part}ParticleVariant(PARTICLE_{base}, {invalid})"
                    f" == 0);")
        for name in names:
            checks.append(
                f"static_assert(GetZombie{part}ParticleVariant(PARTICLE_ZOMBIE_NEWSPAPER_HEAD,"
                f" {name}) == 0);")
    return (
        '#include <array>\n#include "Sexy.TodLib/generated/ParticleVariantId.h"\n' + types + "\n" + effects + "\n"
        + selector(source, "Head") + "\n" + selector(source, "Arm") + "\n"
        + "\n".join(checks) + "\n"
    )


class ZombieParticleVariantTests(unittest.TestCase):
    def test_table_mappings_match_legacy_images(self):
        source = ZOMBIE.read_text(encoding="utf-8")
        for part, expected in (("Head", HEAD), ("Arm", ARM)):
            rows = re.findall(
                rf"ZOMBIE_{part.upper()}_PARTICLE_VARIANT\((\w+), (\w+)\);",
                selector(source, part))
            self.assertEqual(dict(rows), {key: COMPONENT[value] for key, value in expected.items()})
            self.assertEqual(len(rows), len(expected))
            self.assertIn("std::array<", selector(source, part))
            self.assertIn("aVariants{};", selector(source, part))
            runtime = selector(source, part).split(f"GetZombie{part}ParticleVariant", 1)[1]
            self.assertNotIn("for (", runtime)
            self.assertIn("[aTypeIndex]", runtime)

    def test_birth_sites_preserve_effect_controls(self):
        source = ZOMBIE.read_text(encoding="utf-8")
        self.assertNotIn("->OverrideImage(", source)
        self.assertNotIn("kDropHeadParticleImage", source)
        self.assertNotIn("kLostArmParticleImage", source)
        head = source.split("void Zombie::DropHead(", 1)[1].split("\n}\n", 1)[0]
        self.assertLess(head.index("PHASE_ZOMBIE_MOWERED"), head.index("else if (mInPool)"))
        self.assertLess(head.index("PARTICLE_ZOMBIE_NEWSPAPER_HEAD"), head.index("GetZombieHeadParticleVariant"))
        self.assertEqual(head.count("RENDER_GROUP_HIDDEN);"), 7)
        self.assertIn("aRenderOrder = mRenderOrder - 1;", head)
        mustache = head.split("if (mBoard->mMustacheMode", 1)[1].split("if (mBoard->mFutureMode", 1)[0]
        self.assertNotIn("Rand", mustache)
        for style in ("2", "3"):
            self.assertIn(f"aMustacheOption == ReanimOptions::Zombie::Zombie_mustache::ZOMBIE_MUSTACHE{style}", mustache)
            self.assertIn(f"ParticleVariants::ZombieMustache::ZombieMustache{style}", mustache)
        self.assertIn("auto aMustacheVariant = ParticleVariants::ZombieMustache::Default;", mustache)
        self.assertIn("aSunglassParticle->OverrideFrame(ParticleEmitterRanges::All, aFrame);", head)
        arm = source.split("void Zombie::SetupReanimForLostArm(", 1)[1].split("\n}\n", 1)[0]
        self.assertIn("!mInPool && !TestBit(theDamageFlags, DamageFlags::DAMAGE_DOESNT_LEAVE_BODY)", arm)
        self.assertIn("aBodyReanim->SetVariantOption(", arm)
        self.assertLess(arm.index("GetZombieArmParticleVariant"), arm.index("AddTodParticle"))
        spit = source.split("void Zombie::BossHeadSpitEffect(", 1)[1].split("\n}\n", 1)[0]
        self.assertEqual(spit.count("PARTICLE_ZOMBIE_BOSS_FIREBALL"), 2)
        self.assertEqual(spit.count("ParticleVariants::ZombieBossFireball::ZombieBossIceballBurst"), 1)

    @unittest.skipUnless(os.environ.get("PVZ_TEST_CXX"), "Set PVZ_TEST_CXX to a C++20 compiler")
    def test_compiled_selectors_all_zombies_bases_and_invalid_values(self):
        compiler = os.environ["PVZ_TEST_CXX"]
        with tempfile.TemporaryDirectory(prefix="pvz-zombie-particle-") as temporary:
            directory = Path(temporary)
            probe = directory / "probe.cpp"
            probe.write_text(compile_probe(ZOMBIE.read_text(encoding="utf-8")), encoding="utf-8")
            include = ROOT / "pvzPerf"
            if Path(compiler).name.lower() == "cl.exe":
                args = [compiler, "/nologo", "/std:c++20", "/c", str(probe),
                        f"/I{include}", f"/Fo{directory / 'probe.obj'}"]
            else:
                args = [compiler, "-std=c++20", "-c", str(probe),
                        f"-I{include}", "-o", str(directory / "probe.o")]
            result = subprocess.run(args, capture_output=True, text=True, errors="replace", timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
