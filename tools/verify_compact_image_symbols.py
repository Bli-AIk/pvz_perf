#!/usr/bin/env python3
"""Verify isolated image-symbol compaction with structural, whole-byte checks.

Run --capture-inputs before the generator/compiler, then run without that flag.
Only the supplied candidate directory is written. No compiler or game is run.
"""

from __future__ import annotations
import os as _devtools_os
from pathlib import Path as _DevToolsPath
DEVTOOLS_ROOT = _DevToolsPath(_devtools_os.environ.get("PVZ_DEVTOOLS_SOURCE_DIR", str(_DevToolsPath(__file__).resolve().parents[2] / "PvzDevTools")))
DEVTOOLS_REFERENCE = DEVTOOLS_ROOT / "tests/reference/python"

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import zlib
import build_numeric_runtime_pak64 as pak


REMOVED_ID = 165
REMOVED_NAME = "IMAGE_PARTNER_LOGO"
EXPECTED_RECORDS = {"reanim": 146, "particles": 106}
SUFFIXES = {"reanim": ".reanim.v4.compiled", "particles": ".xml.v4.compiled"}
PARTICLE_FIXTURES = {
    name + ".xml.v4.compiled" for name in (
        "ArtChallengePlant", "CircleEvenSpacing", "KernelSplat",
        "PerfTest", "SeedPacketPickup", "SimpleParticle",
    )
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fingerprint(path: Path) -> dict:
    data = path.read_bytes()
    return {"bytes": len(data), "sha256": sha256(data)}


class Reader:
    def __init__(self, data: bytes, label: str, base: int = 0):
        self.data, self.label, self.base, self.at = data, label, base, 0

    def take(self, size: int) -> bytes:
        require(0 <= size <= len(self.data) - self.at,
                f"{self.label}: truncated field at {self.base + self.at}, size={size}")
        result = self.data[self.at:self.at + size]
        self.at += size
        return result

    def u32(self) -> int:
        return struct.unpack("<I", self.take(4))[0]

    def count(self, stride: int, limit: int = 1 << 20) -> int:
        count = self.u32()
        require(count <= limit and count * stride <= len(self.data) - self.at,
                f"{self.label}: invalid count={count} at {self.base + self.at - 4}")
        return count

    def offset(self) -> int:
        return self.base + self.at

    def end(self) -> None:
        require(self.at == len(self.data),
                f"{self.label}: trailing bytes at {self.offset()}")


def registry(data: bytes, label: str) -> list[str]:
    reader = Reader(data, label)
    require(reader.u32() == 0x4953594D, f"{label}: invalid registry magic")
    names = []
    for expected in range(1, reader.count(8) + 1):
        require(reader.u32() == expected, f"{label}: nonsequential registry ID")
        raw = reader.take(reader.count(1))
        name = raw.decode("utf-8")
        require(name and "\0" not in name, f"{label}: empty/NUL symbol")
        names.append(name)
    reader.end()
    require(len(set(names)) == len(names), f"{label}: duplicate symbols")
    return names


def registry_bytes(names: list[str]) -> bytes:
    result = bytearray(struct.pack("<II", 0x4953594D, len(names)))
    for ident, name in enumerate(names, 1):
        raw = name.encode("utf-8")
        result.extend(struct.pack("<II", ident, len(raw)))
        result.extend(raw)
    return bytes(result)


def header_ids(data: bytes, names: list[str], crc: int, label: str) -> dict[str, str]:
    text = data.decode("utf-8")
    match = re.search(
        r"enum\s+class\s+ImageSymbolId\s*:\s*std::uint32_t\s*\{([^}]*)\};",
        text, re.S)
    require(match is not None, f"{label}: missing ImageSymbolId enum")
    entries = []
    for line in match[1].splitlines():
        if not line.strip():
            continue
        entry = re.fullmatch(r"\s*([A-Za-z_][A-Za-z_0-9]*)\s*=\s*(\d+)\s*,\s*", line)
        require(entry is not None, f"{label}: unexpected enum line {line!r}")
        entries.append((entry[1], int(entry[2])))
    require([value for _, value in entries] == list(range(len(names) + 1)),
            f"{label}: gaps, aliases, wrong count or enum order")
    require(entries[0] == ("None", 0) and
            len({ident for ident, _ in entries}) == len(entries),
            f"{label}: invalid None or duplicate enum identifier")
    for constant, expected in (
        ("IMAGE_SYMBOL_ID_COUNT", len(names) + 1),
        ("IMAGE_SYMBOL_ID_MAX", len(names)),
        ("IMAGE_SYMBOL_ID_REGISTRY_CRC", crc),
    ):
        values = re.findall(
            r"\b" + constant + r"\s*=\s*(0x[0-9A-Fa-f]+|\d+)[uU]?\s*;", text)
        require(len(values) == 1 and int(values[0], 0) == expected,
                f"{label}: incorrect {constant}")
    return {name: entries[index][0] for index, name in enumerate(names, 1)}


def envelope(data: bytes, cookie: int, label: str) -> Reader:
    reader = Reader(data, label)
    require(reader.u32() == cookie, f"{label}: wrong envelope cookie")
    size, crc = reader.u32(), reader.u32()
    payload = reader.take(size)
    reader.end()
    require(zlib.crc32(payload) == crc, f"{label}: invalid outer CRC")
    return Reader(payload, label, base=12)


def parse_record(data: bytes, kind: str, label: str, image_max: int) -> dict:
    require(kind in ("reanim", "particles"),
            f"{label}: image-symbol compaction only accepts legacy reanim/particle records; "
            f"{kind} has no image-symbol fields")
    images = []
    stats = Counter()
    reader = envelope(data, 0xDEADFED7 if kind == "reanim" else 0xDEADFED8, label)

    def image_field(source: Reader) -> None:
        offset, value = source.offset(), source.u32()
        require(value <= image_max, f"{label}: out-of-range image ID {value} at {offset}")
        images.append((offset, value))

    if kind == "reanim":
        require(reader.u32() == 0x35544449, f"{label}: expected IDT5")
        reader.u32()  # Track table hash.
        hash_offset, image_hash = reader.offset(), reader.u32()
        reader.take(8)  # Font table hash and raw FPS bits.
        count = reader.count(16, 8192)
        require(count > 0, f"{label}: empty reanim")
        tracks = []
        for _ in range(count):
            track_id = reader.u32()
            frames = reader.u32()
            stride, mode = reader.u32(), reader.u32()
            require(0 < frames <= 1 << 20 and stride == 32 and mode in (0, 1),
                    f"{label}: invalid track framing")
            reader.take(frames * stride)
            tracks.append((track_id, frames, mode))
            stats["transform_bytes"] += frames * stride
        stats["tracks"] = count
        stats["frames"] = sum(frames for _, frames, _ in tracks)
        for _, frames, mode in tracks:
            for _ in range(frames):
                image_field(reader)
                reader.u32()  # Font ID remains an exact byte comparison.
                if mode:
                    text = reader.take(reader.count(1, 0x7FFFFFFE))
                    require(b"\0" not in text, f"{label}: NUL in UI caption")
                    text.decode("utf-8")
                    stats["ui_caption_bytes"] += len(text)
                    stats["ui_caption_slots"] += 1
        previous_track = -1
        for _ in range(reader.count(8, count)):
            track_index, segments = reader.u32(), reader.u32()
            require(previous_track < track_index < count, f"{label}: invalid segment track")
            previous_track = track_index
            _, frames, mode = tracks[track_index]
            require(mode == 0 and segments <= frames, f"{label}: invalid attacher framing")
            previous_end = 0
            for _ in range(segments):
                start, end, _, _, _, loop, run_start, run_end = struct.unpack(
                    "<8I", reader.take(32))
                require(previous_end <= start < end <= frames and loop <= 2 and
                        run_start <= start < end <= run_end <= frames,
                        f"{label}: invalid attacher bounds")
                previous_end = end
            stats["attacher_segments"] += segments
        reader.end()
    else:
        require(reader.u32() == 0x35504449, f"{label}: expected IDP5")
        hash_offset, image_hash = reader.offset(), reader.u32()
        reader.u32()  # Emitter name table hash.
        count, body_size = reader.u32(), reader.u32()
        require(count <= 1000, f"{label}: invalid emitter count")
        body_offset = reader.offset()
        body = Reader(reader.take(body_size), label, body_offset)

        def track() -> None:
            nodes = body.count(20, 100000)
            body.take(nodes * 20)
            stats["float_tracks"] += 1
            stats["raw_track_node_bytes"] += nodes * 20

        for _ in range(count):
            image_field(body)
            body.take(24)  # Six exact raw emitter scalars.
            for _ in range(22):
                track()
            for _ in range(2):
                fields = body.count(12, 4)
                for _ in range(fields):
                    require(0 < body.u32() < 12, f"{label}: invalid field type")
                    track()
                    track()
                stats["fields"] += fields
            for _ in range(16):
                track()
        body.end()
        require(reader.u32() == 0x33544449 and reader.u32() == count,
                f"{label}: invalid duration section")
        for _ in range(count):
            target = reader.u32()
            require(target == 0xFFFFFFFF or target < count, f"{label}: invalid duration target")
        require(reader.u32() == 0x35544449 and reader.u32() == count,
                f"{label}: invalid emitter name section")
        reader.take(count * 4)
        reader.end()
        stats["emitters"] = count
        stats["emitter_name_ids"] = count
    stats["image_fields"] = len(images)
    return {"images": images, "hash_offset": hash_offset,
            "image_hash": image_hash, "stats": dict(stats)}


def mapped_image(value: int) -> int:
    require(value != REMOVED_ID, f"removed old image symbol {REMOVED_ID} is still referenced")
    return value - (value > REMOVED_ID)


def compare_record(old: bytes, new: bytes, kind: str, label: str,
                   old_crc: int, new_crc: int, old_names: list[str],
                   new_names: list[str]) -> dict:
    before = parse_record(old, kind, label + " old", len(old_names))
    after = parse_record(new, kind, label + " candidate", len(new_names))
    require(before["image_hash"] == old_crc and after["image_hash"] == new_crc,
            f"{label}: registry hash mismatch")
    require(before["hash_offset"] == after["hash_offset"] and
            len(before["images"]) == len(after["images"]),
            f"{label}: image field layout changed")
    expected = bytearray(old)
    shifted = 0
    used = set()
    # Only schema-derived image locations and the two declared CRC fields change.
    for (offset, value), (new_offset, actual) in zip(before["images"], after["images"]):
        mapped = mapped_image(value)
        require(new_offset == offset and actual == mapped,
                f"{label}: image mapping mismatch at {offset}: {value} -> {actual}")
        if value:
            require(old_names[value - 1] == new_names[actual - 1],
                    f"{label}: image spelling identity changed at {offset}")
            used.add(value)
        struct.pack_into("<I", expected, offset, mapped)
        shifted += value != mapped
    struct.pack_into("<I", expected, before["hash_offset"], new_crc)
    struct.pack_into("<I", expected, 8, zlib.crc32(expected[12:]))
    if bytes(expected) != new:
        first = next((i for i, (a, b) in enumerate(zip(expected, new)) if a != b),
                     min(len(expected), len(new)))
        raise ValueError(f"{label}: non-image byte mismatch at {first}; "
                         f"expected bytes={len(expected)}, actual bytes={len(new)}")
    return {"bytes": len(old), "old_sha256": sha256(old), "candidate_sha256": sha256(new),
            "shifted_image_fields": shifted, "used_old_image_ids": sorted(used),
            "bytes_outside_changed_fields": len(old) - 8 - 4 * shifted, **before["stats"]}


def input_paths(root: Path, compiler_root: Path) -> list[Path]:
    deployed = root / "out/x64-win32-dx12/Release"
    files = [
        root / "pvzPerf/Sexy.TodLib/generated/image_symbol_ids.bin",
        root / "pvzPerf/Sexy.TodLib/generated/ImageSymbolId.h",
        DEVTOOLS_ROOT / "saveConverter/src/FrozenResourceIds.h",
        DEVTOOLS_ROOT / "saveConverter/src/ResourceIdRemap.h",
        root / "tools/gen_image_symbol_registry.py",
        root / "tools/reanim_type_map.txt",
        root / "assets/definitions/toolchain/reanim_track_families.txt",
        root / "assets/definitions/toolchain/font_symbol_ids.bin",
        deployed / "main.pak64",
        compiler_root / "out/x64-win32-dx12/tools/resourceCompiler/build/reanimc.exe",
    ]
    files.extend(base / pak.PARTICLE_VARIANTS for base in (root, compiler_root)
                 if (base / pak.PARTICLE_VARIANTS).exists())
    for directory, pattern in (
        (deployed / "compiled", "*"),
        (compiler_root / "assets/definitions/reanim", "*.dsl"),
        (compiler_root / "assets/definitions/particles", "*.dsl"),
        (compiler_root / "assets/definitions/particle-reference", "*"),
    ):
        require(directory.is_dir(), f"missing protected input directory: {directory}")
        files.extend(p for p in directory.rglob(pattern) if p.is_file())
    require(all(p.is_file() for p in files), "missing required protected input")
    return sorted(set(files), key=lambda p: str(p).casefold())


def record_paths(directory: Path, kind: str, extra: int = 0, *,
                 root: Path | None = None) -> dict[str, Path]:
    require(directory.is_dir(), f"missing {kind} directory: {directory}")
    result = {}
    for path in directory.rglob("*.v4.compiled"):
        require(path.name.endswith(SUFFIXES[kind]), f"unexpected production record: {path}")
        name = path.relative_to(directory).as_posix()
        require(name.casefold() not in {n.casefold() for n in result},
                f"case-aliased record: {path}")
        result[name] = path
    expected = (pak.expected_counts(root)[kind] if root is not None
                else EXPECTED_RECORDS[kind]) + extra
    require(len(result) == expected,
            f"{kind}: expected {expected} records, found {len(result)}")
    if root is not None and kind == "particles":
        variants = {entry["component"].casefold() + SUFFIXES[kind]
                    for entry in pak.particle_image_variants(root)}
        require(variants <= {name.casefold() for name in result},
                "particles: missing declared image-variant records")
    return result


def write_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def input_category(path: Path, root: Path, compiler_root: Path) -> str:
    if path == root / "tools/gen_image_symbol_registry.py":
        return "main_agent_generator_tool_code"
    if path.is_relative_to(root / "out/modern-game/Release"):
        return "production_compiled_assets_and_pak"
    if path.is_relative_to(root / "pvzPerf/Sexy.TodLib/generated") or path in {
            DEVTOOLS_ROOT / "saveConverter/src/FrozenResourceIds.h",
            DEVTOOLS_ROOT / "saveConverter/src/ResourceIdRemap.h"}:
        return "production_sidecar_headers_and_save_mapping"
    if path.is_relative_to(compiler_root / "assets/definitions/particle-reference"):
        return "native_particle_gold"
    if path.is_relative_to(compiler_root / "out/x64-win32-dx12/tools/resourceCompiler/build"):
        return "existing_dsl_compiler"
    if path.suffix == ".dsl":
        return "author_dsl"
    return "compiler_input_tables"


def generator_guard_evidence(root: Path, candidate: Path, change: dict) -> dict:
    source = root / "tools/gen_image_symbol_registry.py"
    current = source.read_bytes()
    require(change["after"] == {"bytes": len(current), "sha256": sha256(current)},
            "generator changed during authorized-difference verification")
    guard = (
        b"    if removed & requested:\n"
        b"        raise ValueError('Removed image symbols are still referenced by the supplied corpus')\n"
    )
    require(current.count(guard) == 1, "authorized generator guard does not match exactly")
    without_guard = current.replace(guard, b"", 1)
    require(change["before"] == {"bytes": len(without_guard), "sha256": sha256(without_guard)},
            "generator has changes beyond the explicitly authorized two-line guard")
    replay = candidate / "generator-current-no-dump"
    outputs = {}
    for name in ("image_symbol_ids.bin", "ImageSymbolId.h"):
        data = (replay / name).read_bytes()
        require(data == (candidate / name).read_bytes(),
                f"current generator no-dump replay differs from original candidate: {name}")
        outputs[name] = {"bytes": len(data), "sha256": sha256(data)}
    preserved = candidate / "verification-before-authorized-recheck.failed.json"
    require(json.loads(preserved.read_text(encoding="utf-8"))["status"] == "failed",
            "original strict failure evidence was not preserved")
    return {
        "path": str(source), "before": change["before"], "after": change["after"],
        "authorization": "user explicitly identified this main-agent concurrent tool change",
        "exact_added_guard": guard.decode("ascii").splitlines(),
        "only_this_guard_added_proven_by_original_sha256": True,
        "no_dump_replay_outputs_byte_identical": outputs,
        "preserved_strict_failure": str(preserved),
        "original_input_baseline_not_refreshed": True,
    }


def capture(root: Path, compiler_root: Path, candidate: Path) -> dict:
    manifest = candidate / "input-fingerprints.json"
    require(not manifest.exists(), "refusing to overwrite input baseline")
    inventory = {str(p.resolve()): fingerprint(p) for p in input_paths(root, compiler_root)}
    records = {kind: sorted(record_paths(root / "out/modern-game/Release/compiled" / kind, kind, root=root))
               for kind in EXPECTED_RECORDS}
    for name in ("image_symbol_ids.bin", "ImageSymbolId.h"):
        source = root / "pvzPerf/Sexy.TodLib/generated" / name
        require(source.read_bytes() == (candidate / "oracle" / name).read_bytes() ==
                (candidate / name).read_bytes(), f"initial candidate/oracle copy differs: {name}")
    result = {"root": str(root), "compiler_root": str(compiler_root),
              "files": inventory, "production_records": records,
              "resource_id2": "converter-only frozen 879 historical IDs, slot kinds and save remap are protected"}
    write_json(manifest, result)
    return {"status": "captured", "protected_input_files": len(inventory),
            "records": {k: len(v) for k, v in records.items()}}


def expect_rejected(name: str, operation, tests: list[str]) -> None:
    try:
        operation()
    except ValueError:
        tests.append(name)
        return
    raise ValueError(f"negative self-test unexpectedly accepted: {name}")


def self_tests(old: bytes, new: bytes, kind: str, old_crc: int, new_crc: int,
               old_names: list[str], new_names: list[str]) -> list[str]:
    tests = []

    def check(a: bytes, b: bytes) -> None:
        compare_record(a, b, kind, "negative self-test", old_crc, new_crc,
                       old_names, new_names)

    def mutate(data: bytes, at: int, value: int | None = None) -> bytes:
        result = bytearray(data)
        if value is None:
            result[at] ^= 1
        else:
            struct.pack_into("<I", result, at, value)
        struct.pack_into("<I", result, 8, zlib.crc32(result[12:]))
        return bytes(result)

    parsed = parse_record(old, kind, "negative fixture", len(old_names))
    require(parsed["images"], "negative fixture has no image fields")
    offset = parsed["images"][0][0]
    expect_rejected(f"{kind}: old165 reference",
                    lambda: check(mutate(old, offset, REMOVED_ID), new), tests)
    expect_rejected(f"{kind}: out-of-range image",
                    lambda: check(old, mutate(new, offset, len(new_names) + 1)), tests)
    # These positions are raw FPS bits and the first raw emitter scalar.
    other_offset = 28 if kind == "reanim" else offset + 4
    expect_rejected(f"{kind}: non-image scalar byte",
                    lambda: check(old, mutate(new, other_offset)), tests)
    expect_rejected(f"{kind}: trailing bytes", lambda: check(old, new + b"\0"), tests)
    expect_rejected(f"{kind}: truncated record", lambda: check(old, new[:-1]), tests)
    broken_crc = bytearray(new)
    broken_crc[8] ^= 1
    expect_rejected(f"{kind}: broken outer CRC", lambda: check(old, bytes(broken_crc)), tests)
    expect_rejected(f"{kind}: wrong registry hash",
                    lambda: check(old, mutate(new, parsed["hash_offset"], new_crc ^ 1)), tests)
    expect_rejected(f"{kind}: non-image final byte",
                    lambda: check(old, mutate(new, len(new) - 1)), tests)
    # An unrelated scalar with the same integer bits must NOT be renumbered.
    check(mutate(old, other_offset, REMOVED_ID),
          mutate(new, other_offset, REMOVED_ID))
    tests.append(f"{kind}: non-image raw165 preserved (positive)")
    return tests


def verify(root: Path, compiler_root: Path, candidate: Path, report: dict,
           acknowledge_generator_guard: bool = False) -> None:
    baseline = json.loads((candidate / "input-fingerprints.json").read_text(encoding="utf-8"))
    require(baseline["root"] == str(root) and baseline["compiler_root"] == str(compiler_root),
            "input baseline belongs to a different workspace")
    old_data = (candidate / "oracle/image_symbol_ids.bin").read_bytes()
    new_data = (candidate / "image_symbol_ids.bin").read_bytes()
    old_names = registry(old_data, "old registry")
    new_names = registry(new_data, "candidate registry")
    require(len(old_names) == 2288 and old_names[REMOVED_ID - 1] == REMOVED_NAME,
            "old registry does not have expected ID165/count")
    require(len(new_names) == 2287 and new_names ==
            old_names[:REMOVED_ID - 1] + old_names[REMOVED_ID:],
            "candidate names are not exactly old names minus ID165")
    require(new_data == registry_bytes(new_names), "candidate sidecar byte layout changed")
    old_crc, new_crc = zlib.crc32(old_data), zlib.crc32(new_data)
    require(old_crc != new_crc, "registry CRC did not change")
    old_header = (candidate / "oracle/ImageSymbolId.h").read_bytes()
    new_header = (candidate / "ImageSymbolId.h").read_bytes()
    require(not re.search(r"reserved|partner", new_header.decode("utf-8"), re.I),
            "candidate header contains Reserved/Partner")
    old_ids = header_ids(old_header, old_names, old_crc, "old header")
    new_ids = header_ids(new_header, new_names, new_crc, "candidate header")
    require(new_ids == {name: old_ids[name] for name in new_names},
            "an existing enum identifier no longer corresponds to its original name")
    for name, data in (("image_symbol_ids.bin", old_data), ("ImageSymbolId.h", old_header)):
        source = str((root / "pvzPerf/Sexy.TodLib/generated" / name).resolve())
        require(baseline["files"][source] == {"bytes": len(data), "sha256": sha256(data)},
                f"oracle differs from pre-compile production fingerprint: {name}")
    report["registry"] = {
        "removed_old_id": REMOVED_ID, "removed_name": REMOVED_NAME,
        "old_symbol_count": len(old_names), "candidate_symbol_count": len(new_names),
        "header_count": len(new_names) + 1, "header_max": len(new_names),
        "old_crc32": f"{old_crc:08X}", "candidate_crc32": f"{new_crc:08X}",
        "first_164_unchanged": True, "later_ids_shifted_down_one": True,
        "all_remaining_enum_identifiers_preserved": True,
        "no_reserved_or_partner_in_candidate_header": True,
    }
    report["records"] = {}
    report["self_tests"] = []
    approved = {}
    for kind in EXPECTED_RECORDS:
        production = record_paths(root / "out/modern-game/Release/compiled" / kind, kind, root=root)
        fixtures = PARTICLE_FIXTURES if kind == "particles" else set()
        outputs = record_paths(candidate / kind, kind, extra=len(fixtures), root=root)
        require(sorted(production) == baseline["production_records"][kind] and
                set(outputs) == set(production) | fixtures,
                f"{kind}: full corpus has missing/unexpected records")
        totals, rows = Counter(), []
        for index, (name, path) in enumerate(sorted(production.items())):
            old, new = path.read_bytes(), outputs[name].read_bytes()
            require(baseline["files"][str(path.resolve())] ==
                    {"bytes": len(old), "sha256": sha256(old)},
                    f"deployed record changed after input capture: {path}")
            row = compare_record(old, new, kind, name, old_crc, new_crc, old_names, new_names)
            rows.append({"name": name, **row})
            totals.update({k: v for k, v in row.items() if isinstance(v, int)})
            if index == 0:
                report["self_tests"].extend(
                    self_tests(old, new, kind, old_crc, new_crc, old_names, new_names))
        auxiliary = sorted(p.relative_to(candidate / kind).as_posix()
                           for p in (candidate / kind).rglob("*")
                           if p.is_file() and p not in outputs.values())
        report["records"][kind] = {
            "count": len(rows), "production_record_set_exact": True,
            "only_image_fields_registry_hash_and_outer_crc_changed": True,
            "totals": dict(totals), "files": rows,
            "auxiliary_outputs_isolated_not_for_deployment": auxiliary,
        }
        approved[kind] = {name: outputs[name] for name in production}
        if fixtures:
            oracle = record_paths(candidate / "oracle/particles", kind, extra=len(fixtures), root=root)
            require(set(oracle) == set(outputs), "particle oracle/full corpus record sets differ")
            oracle_totals, fixture_rows = Counter(), []
            for name in sorted(outputs):
                row = compare_record(oracle[name].read_bytes(), outputs[name].read_bytes(),
                                     kind, "full corpus oracle " + name, old_crc, new_crc,
                                     old_names, new_names)
                oracle_totals.update({k: v for k, v in row.items() if isinstance(v, int)})
                if name in fixtures:
                    fixture_rows.append({"name": name, **row})
            require((candidate / "particles/ParticleEmitterName.h").read_bytes() ==
                    (candidate / "oracle/particles/ParticleEmitterName.h").read_bytes(),
                    "generated emitter-name header changed with image compaction")
            report["full_particle_corpus_oracle"] = {
                "count": len(outputs), "totals": dict(oracle_totals),
                "only_image_fields_registry_hash_and_outer_crc_changed": True,
                "emitter_name_header_byte_identical": True,
                "extra_fixture_count": len(fixtures), "extra_fixtures": fixture_rows,
                "fixtures_not_for_deployment": True,
            }
    report["record_verification_status"] = "passed"
    report["resource_id2"] = baseline["resource_id2"]
    approved_root = candidate / "production-candidate/compiled"
    approved_rows = []
    approved_paths = set()
    for kind, paths in approved.items():
        for name, source in sorted(paths.items()):
            target = approved_root / kind / name
            require(target.resolve().is_relative_to(candidate),
                    f"candidate output path escapes candidate directory: {target}")
            target.parent.mkdir(parents=True, exist_ok=True)
            data = source.read_bytes()
            if target.exists():
                require(target.read_bytes() == data, f"existing candidate subset differs: {target}")
            else:
                with target.open("xb") as stream:
                    stream.write(data)
            approved_rows.append({"name": f"compiled/{kind}/{name}",
                                  "bytes": len(data), "sha256": sha256(data)})
            approved_paths.add(target.resolve())
        subset = record_paths(approved_root / kind, kind, root=root)
        require(set(subset) == set(paths), f"{kind}: isolated production subset record set differs")
    require({p.resolve() for p in approved_root.rglob("*") if p.is_file()} == approved_paths,
            "isolated production subset contains auxiliary files")
    approved_manifest = {"status": "verified-candidate-only-not-deployed",
                         "files": approved_rows, "count": len(approved_rows)}
    write_json(candidate / "production-candidate/manifest.json", approved_manifest)
    report["production_candidate"] = {
        "path": str(approved_root.parent), "records": len(approved_rows),
        "counts": {kind: pak.expected_counts(root)[kind] for kind in EXPECTED_RECORDS},
        "no_auxiliary_records_or_headers": True,
        "record_set_exact": True, "not_deployed": True,
    }
    report["scope"] = [
        "Whole-record byte equality after patching only structurally located image fields, "
        "image registry hash, and outer CRC in memory.",
        "No raw float parsing/re-encoding and no blind word substitution.",
        "Every original ID165 image reference is rejected; candidate ID165 denotes old ID166.",
        "Auxiliary v2 records and compiler-generated headers are isolated evidence only.",
        "The raw particle corpus contains six known compiler-only fixtures; the separately "
        "materialized production-candidate exactly matches the deployed base and image-variant records.",
        "No game build, launch, performance measurement, packaging, or deployment.",
        "This report does not assert the main agent's runtime/schema/header work is complete.",
    ]
    paths = input_paths(root, compiler_root)
    current = {str(p.resolve()): fingerprint(p) for p in paths}
    before = baseline["files"]
    changed = [
        {"path": path, "before": before.get(path), "after": current.get(path)}
        for path in sorted(set(before) | set(current))
        if before.get(path) != current.get(path)
    ]
    report["protected_inputs"] = {
        "files": len(current), "baseline_files": len(before),
        "all_fingerprints_unchanged": not changed,
        "unchanged_files": sum(before.get(path) == stamp for path, stamp in current.items()),
        "changes": changed,
    }
    groups = {}
    for path in sorted(set(before) | set(current)):
        category = input_category(Path(path), root, compiler_root)
        group = groups.setdefault(category, {"files": 0, "unchanged": 0, "changed": []})
        group["files"] += 1
        if before.get(path) == current.get(path):
            group["unchanged"] += 1
        else:
            group["changed"].append(path)
    report["protected_inputs"]["groups"] = groups
    report["protected_inputs"]["not_in_original_baseline"] = (
        "runtime/schema sources and tools/compile_numeric_resources.py are main-agent-owned "
        "concurrent consumer/tool code; no unchanged-fingerprint claim is made for them"
    )
    authorized = []
    generator_path = str((root / "tools/gen_image_symbol_registry.py").resolve())
    for change in changed:
        if acknowledge_generator_guard and change["path"] == generator_path:
            authorized.append(generator_guard_evidence(root, candidate, change))
    accepted_paths = {change["path"] for change in authorized}
    unexpected = [row for row in changed if row["path"] not in accepted_paths]
    report["protected_inputs"]["explicitly_authorized_tool_changes"] = authorized
    report["protected_inputs"]["unexpected_changes"] = unexpected
    report["input_fingerprint_status"] = (
        "failed" if unexpected else
        "passed_with_explicitly_authorized_generator_guard" if authorized else "passed"
    )
    require(not unexpected, "protected inputs or their inventories changed: " +
            ", ".join(row["path"] for row in unexpected))
    report["scope"].append(
        "The optional authorized-generator acknowledgement accepts only the exact added "
        "removed/requested guard, proven against the original SHA256 and a byte-identical "
        "no-dump replay; author/production data and all 252 whole-record checks remain strict."
    )
    report["status"] = "passed"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--compiler-root", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--capture-inputs", action="store_true")
    parser.add_argument("--acknowledge-authorized-generator-guard", action="store_true")
    args = parser.parse_args()
    root, compiler_root, candidate = (p.resolve() for p in
                                     (args.root, args.compiler_root, args.candidate))
    require(root.is_dir() and compiler_root.is_dir() and candidate.is_dir(),
            "all directories must already exist")
    require(candidate.is_relative_to(root / "log") and
            not candidate.is_relative_to(root / "out"), "output must remain under root/log")
    report = {"status": "failed", "candidate": str(candidate)}
    try:
        if args.capture_inputs:
            print(json.dumps(capture(root, compiler_root, candidate)))
            return 0
        verify(root, compiler_root, candidate, report,
               acknowledge_generator_guard=args.acknowledge_authorized_generator_guard)
    except (OSError, ValueError, KeyError, IndexError, struct.error) as error:
        report["error"] = str(error)
        if not args.capture_inputs:
            write_json(candidate / "verification.json", report)
        print(f"compact-image verification failed: {error}", file=sys.stderr)
        return 1
    write_json(candidate / "verification.json", report)
    print(json.dumps({"status": report["status"], "registry": report["registry"],
                      "records": {k: v["count"] for k, v in report["records"].items()},
                      "input_fingerprint_status": report["input_fingerprint_status"],
                      "protected_input_files": report["protected_inputs"]["files"],
                      "negative_self_tests": sum("(positive)" not in t
                                                 for t in report["self_tests"]),
                      "positive_self_tests": sum("(positive)" in t
                                                 for t in report["self_tests"])}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
