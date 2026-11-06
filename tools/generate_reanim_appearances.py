"""Materialize the game's independent appearance choices as complete DSL variants.

This is authoring tooling, not the resource compiler. Neither the compiler nor
the game needs to inspect C++ image symbols. ResourceId appears only in the
separate offline save-conversion catalog.
"""
from __future__ import annotations
import os as _devtools_os
from pathlib import Path as _DevToolsPath
DEVTOOLS_ROOT = _DevToolsPath(_devtools_os.environ.get("PVZ_DEVTOOLS_SOURCE_DIR", str(_DevToolsPath(__file__).resolve().parents[2] / "PvzDevTools")))
DEVTOOLS_REFERENCE = DEVTOOLS_ROOT / "tests/reference/python"
import argparse
import itertools
import json
import math
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SPECS: dict[str, dict[str, list[str]]] = {}


def add(root: str, track: str, *images: str) -> None:
    values = SPECS.setdefault(root, {}).setdefault(track, [])
    for image in images:
        if image not in values:
            values.append(image)


def image_names(*suffixes: str) -> list[str]:
    return ["IMAGE_REANIM_" + name for name in suffixes]


def specs() -> None:
    add("Wallnut", "anim_face", *image_names("WALLNUT_CRACKED1", "WALLNUT_CRACKED2"))
    add("Tallnut", "anim_idle", *image_names("TALLNUT_CRACKED1", "TALLNUT_CRACKED2"))
    add("Garlic", "anim_face", *image_names("GARLIC_BODY2", "GARLIC_BODY3"))
    add("Pumpkin", "Pumpkin_front", *image_names("PUMPKIN_DAMAGE1", "PUMPKIN_DAMAGE3"))
    add("Pot", "Pot_top", *image_names("POT_TOP_DARK"))
    add("ZenGarden_fertilizer", "bag", "IMAGE_MONEYBAG")
    add("CrazyDave", "Dave_mouths", *image_names(
        "CRAZYDAVE_MOUTH1", "CRAZYDAVE_MOUTH5", "CRAZYDAVE_MOUTH4", "CRAZYDAVE_MOUTH6"))
    add("SunFlower", "anim_idle", *image_names(
        *(f"SUNFLOWER_HEAD_SING{i}" for i in range(1, 6)), "SUNFLOWER_HEAD_WINK"))
    add("SelectorScreen", "almanac_key_shadow",
        *image_names("SELECTORSCREEN_ALMANAC_SHADOW", "SELECTORSCREEN_KEY_SHADOW"))
    add("SelectorScreen", "woodsign2", *image_names("SELECTORSCREEN_WOODSIGN2_PRESS"))
    add("SelectorScreen", "woodsign3", *image_names("SELECTORSCREEN_WOODSIGN3_PRESS"))
    # Gross-out faces share option 1 across every eligible zombie family.
    for root in ("Zombie", "Zombie_polevaulter", "Zombie_paper", "Zombie_disco", "Zombie_backup"):
        add(root, "anim_head1", *image_names("ZOMBIE_HEAD_GROSSOUT"))
    add("Zombie", "anim_head1", "IMAGE_BLANK", *image_names(
        *(f"ZOMBIE_HEAD_SUNGLASSES{i}" for i in range(1, 5))))
    add("Zombie", "Zombie_mustache", *image_names("ZOMBIE_MUSTACHE2", "ZOMBIE_MUSTACHE3"))
    add("Zombie_paper", "anim_head1", *image_names("ZOMBIE_PAPER_MADHEAD"))
    add("Zombie_gargantuar", "anim_head1", *image_names(
        "ZOMBIE_GARGANTUAR_HEAD_REDEYE", "ZOMBIE_GARGANTUAR_HEAD2_REDEYE", "ZOMBIE_GARGANTUAR_HEAD2"))
    add("Zombie_gargantuar", "Zombie_gargantuar_telephonepole",
        *image_names("ZOMBIE_GARGANTUAR_ZOMBIE", "ZOMBIE_GARGANTUAR_DUCKXING"))
    add("Zombie_gargantuar", "Zombie_gargantua_body1",
        *image_names("ZOMBIE_GARGANTUAR_BODY1_2", "ZOMBIE_GARGANTUAR_BODY1_3"))
    add("Zombie_gargantuar", "Zombie_gargantuar_outerarm_lower",
        *image_names("ZOMBIE_GARGANTUAR_OUTERARM_LOWER2"))
    add("Zombie_gargantuar", "Zombie_gargantuar_outerleg_foot",
        *image_names("ZOMBIE_GARGANTUAR_FOOT2"))
    for root, track, image in (
        ("Zombie", "Zombie_outerarm_upper", "ZOMBIE_OUTERARM_UPPER2"),
        ("Zombie_polevaulter", "Zombie_polevaulter_outerarm_upper", "ZOMBIE_POLEVAULTER_OUTERARM_UPPER2"),
        ("Zombie_paper", "Zombie_paper_leftarm_upper", "ZOMBIE_PAPER_LEFTARM_UPPER2"),
        ("Zombie_football", "zombie_football_leftarm_upper", "ZOMBIE_FOOTBALL_LEFTARM_UPPER2"),
        ("Zombie_disco", "Zombie_disco_outerarm_upper", "ZOMBIE_DISCO_OUTERARM_UPPER2"),
        ("Zombie_backup", "Zombie_disco_outerarm_upper", "ZOMBIE_BACKUP_OUTERARM_UPPER2"),
        ("Zombie_snorkle", "Zombie_snorkle_outerarm_upper", "ZOMBIE_SNORKLE_OUTERARM_UPPER2"),
        ("Zombie_bobsled", "Zombie_dolphinrider_outerarm_upper", "ZOMBIE_BOBSLED_OUTERARM_UPPER2"),
        ("Zombie_dolphinrider", "Zombie_dolphinrider_outerarm_upper", "ZOMBIE_DOLPHINRIDER_OUTERARM_UPPER2"),
        ("Zombie_jackbox", "Zombie_jackbox_outerarm_lower", "ZOMBIE_JACKBOX_OUTERARM_LOWER2"),
        ("Zombie_balloon", "Zombie_outerarm_upper", "ZOMBIE_BALLOON_OUTERARM_UPPER2"),
        ("Zombie_digger", "Zombie_digger_outerarm_upper", "ZOMBIE_DIGGER_OUTERARM_UPPER2"),
        ("Zombie_pogo", "Zombie_outerarm_upper", "ZOMBIE_POGO_OUTERARM_UPPER2"),
        ("Zombie_yeti", "Zombie_yeti_outerarm_upper", "ZOMBIE_YETI_OUTERARM_UPPER2"),
        ("Zombie_ladder", "Zombie_ladder_outerarm_upper", "ZOMBIE_LADDER_OUTERARM_UPPER2"),
        ("Zombie_imp", "Zombie_imp_outerarm_upper", "ZOMBIE_IMP_ARM1_BONE"),
    ):
        add(root, track, *image_names(image))
    add("Zombie_pogo", "Zombie_pogo_stickhands", *image_names("ZOMBIE_POGO_STICKHANDS2"))
    add("Zombie_pogo", "Zombie_pogo_stick", *image_names("ZOMBIE_POGO_STICKDAMAGE2"))
    add("Zombie_pogo", "Zombie_pogo_stick2", *image_names("ZOMBIE_POGO_STICK2DAMAGE2"))
    add("Zombie_flagpole", "Zombie_flag", *image_names("ZOMBIE_FLAG3"))
    for root, track, first, second in (
        ("Zombie", "anim_cone", "ZOMBIE_CONE2", "ZOMBIE_CONE3"),
        ("Zombie", "anim_bucket", "ZOMBIE_BUCKET2", "ZOMBIE_BUCKET3"),
        ("Zombie", "anim_screendoor", "ZOMBIE_SCREENDOOR2", "ZOMBIE_SCREENDOOR3"),
        ("Zombie_paper", "Zombie_paper_paper", "ZOMBIE_PAPER_PAPER2", "ZOMBIE_PAPER_PAPER3"),
        ("Zombie_football", "zombie_football_helmet", "ZOMBIE_FOOTBALL_HELMET2", "ZOMBIE_FOOTBALL_HELMET3"),
        ("Zombie_digger", "Zombie_digger_hardhat", "ZOMBIE_DIGGER_HARDHAT2", "ZOMBIE_DIGGER_HARDHAT3"),
        ("Zombie_ladder", "Zombie_ladder_1", "ZOMBIE_LADDER_1_DAMAGE1", "ZOMBIE_LADDER_1_DAMAGE2"),
    ):
        add(root, track, *image_names(first, second))
    for part in (1, 2):
        add("Zombie_zamboni", f"Zombie_zamboni_{part}",
            *image_names(f"ZOMBIE_ZAMBONI_{part}_DAMAGE1", f"ZOMBIE_ZAMBONI_{part}_DAMAGE2"))
    add("Zombie_catapult", "Zombie_catapult_siding", *image_names("ZOMBIE_CATAPULT_SIDING_DAMAGE"))
    add("Zombie_catapult", "Zombie_catapult_pole", *image_names(
        "ZOMBIE_CATAPULT_POLE", "ZOMBIE_CATAPULT_POLE_WITHBALL",
        "ZOMBIE_CATAPULT_POLE_DAMAGE", "ZOMBIE_CATAPULT_POLE_DAMAGE_WITHBALL"))
    for track, stem in (("Boss_head", "HEAD"), ("Boss_jaw", "JAW"),
                        ("Boss_outerarm_hand", "OUTERARM_HAND"), ("Boss_outerarm_thumb2", "OUTERARM_THUMB")):
        add("Zombie_boss", track, *image_names(
            f"ZOMBIE_BOSS_{stem}_DAMAGE1", f"ZOMBIE_BOSS_{stem}_DAMAGE2"))
    add("Zombie_boss", "Boss_innerleg_foot", *image_names("ZOMBIE_BOSS_FOOT_DAMAGE1"))
    add("Zombie_boss", "Boss_outerleg_foot", *image_names("ZOMBIE_BOSS_FOOT_DAMAGE2"))
    add("Zombie_boss", "Boss_eyeglow_red", *image_names("ZOMBIE_BOSS_EYEGLOW_BLUE"))
    add("Zombie_boss", "Boss_mouthglow_red", *image_names("ZOMBIE_BOSS_MOUTHGLOW_BLUE"))
    for track in ("impblink", "imphead"):
        add("Zombie_charred_gargantuar", track, "IMAGE_BLANK")


def blocks(text: str, pattern: str):
    for match in re.finditer(pattern, text):
        depth = 1
        for token in re.finditer(r'"(?:\\.|[^"\\])*"|[{}]', text[match.end():]):
            depth += (token[0] == "{") - (token[0] == "}")
            if not depth:
                yield match, text[match.end():match.end() + token.start()]
                break
        else:
            raise ValueError("Unclosed DSL block")


def load_roots():
    result = {}
    for path in sorted((ROOT / "assets/definitions/reanim/units").glob("*.dsl")):
        text = path.read_text(encoding="utf-8-sig")
        for match, body in blocks(text, r"export\s+component\s+(\w+)\s*\{"):
            tracks = []
            for _, track in blocks(body, r"\bTrack\s*\{"):
                name = re.search(r'source-name:\s*"([^"]+)"', track)[1]
                images = list(dict.fromkeys(re.findall(r"\bimage:\s*(IMAGE_\w+)", track)))
                tracks.append((name, images))
            result[match[1]] = tracks
    return result


def cpp_name(value: str) -> str:
    return re.sub(r"\W", "_", value)


def write_changed(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text(encoding="utf-8") != text:
        path.write_text(text, encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="validate choices and print sizes without writing")
    args = parser.parse_args()
    specs()
    roots = load_roots()
    type_map = {}
    for line in (ROOT / "tools/reanim_type_map.txt").read_text().splitlines():
        parts = line.split("#")[0].split()
        if parts and parts[-1].isdigit():
            type_map[parts[0]] = int(parts[-1])
    resource_ids = {name: int(index) for name, index in re.findall(
        r"\b(IMAGE_\w+)_ID\s*=\s*(\d+)", (DEVTOOLS_ROOT / "saveConverter/src/FrozenResourceIds.h").read_text())}
    catalog, definitions, axes, choices = {}, [], [], []
    header = ["// Generated authoring choice constants; no Image* or SymbolId.", "#pragma once",
              "#include <cstdint>", "namespace ReanimOptions {"]
    for root, tracks in roots.items():
        spec = SPECS.get(root, {})
        unknown = set(spec) - {name for name, _ in tracks}
        if unknown:
            raise ValueError(f"{root}: unknown appearance tracks {unknown}")
        root_axes = []
        stride = 1
        axis_offset = len(axes)
        actions = []
        seen_tracks = set()
        header.append(f"namespace {cpp_name(root)} {{")
        for index, (track, images) in enumerate(tracks):
            first = track not in seen_tracks
            seen_tracks.add(track)
            if track not in spec or not first:
                continue
            if not images:
                raise ValueError(f"{root}:{track} has no source image to replace")
            options = [""] + spec[track]
            root_axes.append((index, track, options, stride))
            axes.append((index, len(options), stride, len(choices)))
            choices.extend((879 if not image else resource_ids[image], digit)
                           for digit, image in enumerate(options))
            header.append(f"namespace {cpp_name(track)} {{ enum : std::uint32_t {{")
            header.extend(f"    {('Default' if not image else image.removeprefix('IMAGE_REANIM_').removeprefix('IMAGE_'))} = {digit},"
                          for digit, image in enumerate(options))
            header.append("}; }")
            actions.extend({"track": track, "option": digit} for digit in range(len(options)))
            stride *= len(options)
        header.append("}")
        if root.casefold() in type_map:
            definitions.append((type_map[root.casefold()], len(tracks), stride, axis_offset, len(root_axes)))
        if not spec:
            continue
        print(root, "variants", stride, "axes", len(root_axes))
        if args.check:
            continue
        name_of = lambda value: root if value == 0 else f"{root}_V{value:08d}"
        lines = ["// Generated complete combinations. Edit generate_reanim_appearances.py and regenerate."]
        transitions = {}
        for variant in range(stride):
            images_by_track = [list(images) for _, images in tracks]
            destinations = []
            for index, track, options, step in root_axes:
                digit = variant // step % len(options)
                if digit:
                    images_by_track[index] = [options[digit]] * len(images_by_track[index])
                destinations.extend(name_of(variant + (option - digit) * step)
                                    for option in range(len(options)))
            transitions[name_of(variant)] = destinations
            if variant:
                images_text = ", ".join("[" + ", ".join(
                    "none" if image == "IMAGE_BLANK" else json.dumps(image) for image in images) + "]"
                    for images in images_by_track)
                lines.append(f"export component {name_of(variant)} {{ variant-of: {root}; images: [{images_text}]; }}")
        catalog[root] = {"actions": actions, "transitions": transitions}
        write_changed(ROOT / f"assets/definitions/reanim/appearances/{root}.dsl", "\n".join(lines) + "\n")
    if args.check:
        return
    write_changed(ROOT / "assets/definitions/reanim-variant-actions.json",
                  json.dumps(catalog, ensure_ascii=False, separators=(",", ":")) + "\n")
    write_changed(ROOT / "pvzPerf/Sexy.TodLib/generated/ReanimVariantOptions.h",
                  "\n".join(header + ["}"]) + "\n")
    save_header = ["// Generated offline save migration identities; not used by rendering.",
                   "#pragma once", "#include <cstdint>", "namespace ReanimAppearanceSaveCatalog {",
                   "struct Definition { std::uint32_t type, trackCount, variantCount, axisOffset, axisCount; };",
                   "struct Axis { std::uint32_t trackIndex, radix, stride, choiceOffset; };",
                   "struct Choice { std::uint32_t canonicalModernResourceId, digit; };"]
    for name, cls, rows in (("Definitions", "Definition", sorted(definitions)),
                            ("Axes", "Axis", axes), ("Choices", "Choice", choices)):
        save_header.append(f"inline constexpr {cls} {name}[] = {{")
        save_header.extend("    {" + ", ".join(map(str, row)) + "}," for row in rows)
        save_header.append("};")
    save_header.append("}")
    write_changed(ROOT / "pvzPerf/Sexy.TodLib/generated/ReanimAppearanceSaveCatalog.h", "\n".join(save_header) + "\n")


if __name__ == "__main__":
    main()
