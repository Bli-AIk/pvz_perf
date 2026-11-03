"""Create a completed test profile and include the canonical endless checkpoint."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import zipfile


def completed_profile():
    # PlayerInfo::SyncDetails v13 field order; unused mode/purchase slots stay zero.
    records = [0] * 100
    for mode in range(1, 71):
        if mode != 35:
            records[mode] = 1
    # Survival records store completed flags, unlike ordinary win-count records.
    for mode in range(61, 66):
        records[mode] = 10
    for mode in range(66, 71):
        records[mode] = 5
    records[71] = 1000
    purchases = [0] * 80
    for item in list(range(9)) + [13, 16, 17, 18, 19, 20, 22, 23, 25, 27]:
        purchases[item] = 1
    purchases[9] = 2
    purchases[21] = 4
    for item in (14, 15):
        purchases[item] = 1020
    for item in (26, 28):
        purchases[item] = 1010
    purchases[24] = 3
    scalars = [0] * 18
    for index in (6, 7, 12, 15, 16):
        scalars[index] = 1
    # Two adventure completions, all trophies/achievements, ten seed slots.
    values = [13, 1, 99999, 2] + records + purchases + [0, 0] + scalars + [0]
    return (b"PVZUSR64" + struct.pack(f"<{len(values)}i", *values)
            + bytes([1, 1]) * 20 + b"\0" + struct.pack("<i", 0)
            + bytes([1]) * 20 + b"\1")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", default="current")
    parser.add_argument("--converter", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--perfect-output", type=Path, help="Optional completed-profile-only ZIP")
    parser.add_argument("--benchmark-output", type=Path, help="Optional independently named full benchmark ZIP")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest = json.loads((root / "Tests/manifest.json").read_text(encoding="utf-8"))
    source_files = {}
    for format_name in ("original", "project"):
        entry = manifest[format_name]
        path = root / "Tests" / entry["file"]
        if hashlib.sha256(path.read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError(f"Canonical checkpoint changed: {path}")
        source_files[format_name] = path
    profile = completed_profile()
    name = b"PerfTest"
    summary = struct.pack("<IHH", 14, 1, len(name)) + name + struct.pack("<II", 1, 1)
    converter = args.converter.resolve()
    with tempfile.TemporaryDirectory(prefix="pvz-completed-profile-") as temporary:
        directory = Path(temporary)
        dat64 = directory / "user1.dat64"
        dat = directory / "user1.dat"
        roundtrip = directory / "roundtrip.dat64"
        dat64.write_bytes(profile)
        for source, target in ((dat64, dat), (dat, roundtrip)):
            subprocess.run([str(converter), str(source), str(target)], check=True)
        if roundtrip.read_bytes() != profile:
            raise ValueError("Completed profile DAT64/DAT roundtrip differs")
        checkpoint = directory / "game1_58.dat64"
        subprocess.run([str(converter), "--target", "dat64-v4",
                        str(source_files["original"]), str(checkpoint)], check=True)
        checkpoint_bytes = checkpoint.read_bytes()
        if (checkpoint_bytes[:8] != b"PVZDAT64"
                or struct.unpack_from("<I", checkpoint_bytes, 8)[0] != 4
                or struct.unpack_from("<II", checkpoint_bytes, 36) != (0x10745, 0)):
            raise ValueError("Modern checkpoint must use DAT64 v4")
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(args.output, "x", zipfile.ZIP_DEFLATED) as archive:
            archive.writestr("data/users.dat", summary)
            archive.writestr("data/user1.dat64", profile)
            archive.write(checkpoint, "data/game1_58.dat64")
            archive.writestr("retail/users.dat", summary)
            archive.write(dat, "retail/user1.dat")
            archive.write(source_files["original"], "retail/game1_13.dat")
            archive.writestr("README.txt",
                f"pvz_perf {args.version} test saves\n\n"
                "Exit the game. Extract data/ beside pvz_perf.exe in a fresh test directory.\n"
                "Profile: PerfTest / ID1. This replaces the destination's profile summary.\n"
                "Completed profile: two adventure completions, all challenge trophies,\n"
                "all five normal survivals at five flags and five hard survivals at ten flags,\n"
                "20 achievements, ten seed slots, permanent store upgrades and coins.\n"
                "Wisdom tree: 1000. Zen garden and custom Zombatars are empty.\n"
                "No personal user profile is included; this profile is generated for testing.\n\n"
                "data/game1_58.dat64 is the Tests endless benchmark converted to DAT64 v4.\n"
                "The converter verifies that all saved semantics are preserved.\n"
                "Do not advance it and reuse the progressed checkpoint as a new baseline.\n"
                "Re-extract the benchmark before each comparison.\n"
                "retail/ contains equivalent profile/checkpoint files for original 1073.\n"
                "Project mode 58 and retail mode 13 refer to the same endless mode.\n")
        with zipfile.ZipFile(args.output) as archive:
            if archive.testzip():
                raise ValueError("Save archive CRC validation failed")
    if args.perfect_output:
        args.perfect_output.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(args.output) as source, zipfile.ZipFile(
                args.perfect_output, "x", zipfile.ZIP_DEFLATED) as archive:
            for name in ("data/users.dat", "data/user1.dat64"):
                archive.writestr(name, source.read(name))
            archive.writestr("README.txt", f"pvz_perf {args.version} completed test profile\n\n"
                "Exit the game, back up your own data, then extract data/ beside pvz_perf.exe.\n"
                "Generated PerfTest / ID1 profile; replaces destination profile summary.\n"
                "All permanent unlocks, two adventure completions, trophies, achievements, ten seed slots.\n"
                "Zen garden and custom Zombatars are empty. No level checkpoint is included.\n"
                "Use the separate endless benchmark ZIP for a canonical level checkpoint.\n")
    if args.benchmark_output:
        import shutil
        args.benchmark_output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("rb") as source, args.benchmark_output.open("xb") as target:
            shutil.copyfileobj(source, target)
    print(json.dumps({"archive": str(args.output.resolve()),
                      "completed_profile": True, "endless_benchmark": True,
                      "profile_roundtrip": "identical"}, indent=2))


if __name__ == "__main__":
    main()
