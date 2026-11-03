"""Package a Windows release with explicitly supplied generated test saves and effects."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import stat
import subprocess
import zipfile


SAVE_ENTRIES = frozenset({
    "data/users.dat", "data/user1.dat64", "data/game1_58.dat64",
    "retail/users.dat", "retail/user1.dat", "retail/game1_13.dat",
})
EFFECT_NOTICES = ("SOURCES.txt", "Magpie-LICENSE.txt", "Anime4K-LICENSE.txt",
                  "SSimDownscaler-LGPL-3.0.txt")


def read_test_saves(path):
    """Read only the generator's exact entries, without extracting ZIP paths."""
    result = {}
    seen = set()
    with zipfile.ZipFile(path) as archive:
        for info in archive.infolist():
            name = info.filename
            if name in seen:
                raise ValueError(f"Duplicate test-save ZIP entry: {name}")
            seen.add(name)
            if stat.S_ISLNK(info.external_attr >> 16):
                raise ValueError(f"Symlink in test-save ZIP: {name}")
            if name in {"data/", "retail/"} and info.is_dir():
                continue
            if name not in SAVE_ENTRIES | {"README.txt"} or info.is_dir():
                raise ValueError(f"Unexpected test-save ZIP entry: {name}")
            result["TEST-SAVES.txt" if name == "README.txt" else name] = archive.read(info)
    missing = SAVE_ENTRIES - result.keys()
    if missing:
        raise ValueError(f"Missing generated test saves: {sorted(missing)}")
    if any(not result[name] for name in SAVE_ENTRIES):
        raise ValueError("Generated test saves must not be empty")
    return result


def effect_files(directory):
    if not directory.is_dir():
        raise FileNotFoundError(directory)
    for path in sorted(directory.rglob("*")):
        relative = path.relative_to(directory)
        if any(part.startswith(".") or part.lower() in {
                "build", "debug", "cache", "__pycache__", "intermediate", "intermediates"
        } for part in relative.parts):
            continue
        name = path.name.lower()
        document = (path.name in EFFECT_NOTICES
                    or name.startswith(("readme", "sources", "license", "copying", "notice"))
                    or "license" in name)
        if path.is_file() and (path.suffix.lower() in {".hlsl", ".hlsli", ".dds"}
                               or (document and path.suffix.lower() in {"", ".md", ".txt", ".rst"})):
            yield "effects/" + relative.as_posix(), path


def package(args):
    root = Path(__file__).resolve().parents[1]
    release = args.release.resolve()
    api = {"vulkan": "Vulkan 1.3", "d3d12": "DirectX 12",
           "opengl": "OpenGL 3.3", "d3d9": "Direct3D 9Ex / SM3.0"}[args.backend]
    shader_extension = {"vulkan": ".spv", "d3d12": ".dxil",
                        "opengl": None, "d3d9": ".cso"}[args.backend]
    modern = args.backend in {"vulkan", "d3d12"}
    if (args.platform == "win10") != modern:
        raise ValueError("Use win7 with d3d9/opengl or win10 with d3d12/vulkan")
    recommendation = "Recommended Windows build.\n\n" if args.backend == "vulkan" else ""
    files = {}

    def include(path, name):
        if not path.is_file():
            raise FileNotFoundError(path)
        files[name] = path

    for name in ("pvz_perf.exe", "main.pak64", "bass.dll"):
        include(release / name, name)
    if modern:
        for name in ("dxcompiler.dll", "dxil.dll", "start-lp14.cmd"):
            include(release / name, name)
    include(release / "data/pvz_dat64_converter.exe", "data/pvz_dat64_converter.exe")
    scale = root / "scale.json"
    if not modern:
        scale = root / f"pvzPerf/SexyAppFramework/graphics/{args.backend}/default-scale.json"
        if args.backend == "d3d9" and not scale.is_file():
            scale = root / "pvzPerf/SexyAppFramework/graphics/opengl/default-scale.json"
        config = json.loads(scale.read_text(encoding="utf-8"))
        if (config["HDR"]["enable"] is not False
                or config["window"].get("effects") != []
                or config["fullScreen"].get("effects") != []
                or config["window"].get("multiplier") != 1
                or config["fullScreen"].get("output", {"width": 0, "height": 0})
                != {"width": 0, "height": 0}):
            raise ValueError("Win7 defaults must disable HDR, effects and custom scaling")
    include(scale, "scale.json")
    include(root / "LICENSE", "LICENSE")
    include(root / "pvzPerf/SexyAppFramework/PopCap Framework License.txt",
            "licenses/PopCap-Framework.txt")
    folders = [("shaders", {shader_extension})] if shader_extension else []
    for folder, extensions in folders:
        for path in sorted((release / folder).rglob("*")):
            if path.is_file() and path.suffix.lower() in extensions:
                # HDR regression tests share the build output directory; their
                # probe is not a runtime shader or a release dependency.
                if path.stem in {"hdr_probe", "hdr_linear_probe", "game_hdr_ps"} or path.stem.startswith("game_reanim"):
                    # Removed GPU animation pipelines can remain in incremental build trees.
                    continue
                include(path, path.relative_to(release).as_posix())
    if shader_extension and not any(n.startswith("shaders/") for n in files):
        raise ValueError(f"No compiled {shader_extension} shaders found")
    for name, path in effect_files(args.effects_directory):
        include(path, name)
    for name in ("README.md", *EFFECT_NOTICES,
                 "Anime4K/Anime4K_Upscale_Denoise_UL.hlsl", "SSimDownscaler.hlsl", "StubDefs.hlsli"):
        if "effects/" + name not in files:
            raise ValueError("Missing staged effect or notice: " + name)
    for name in EFFECT_NOTICES:
        include(args.effects_directory / name, "licenses/" + name)
    for path in sorted(args.notices.iterdir()):
        if path.is_file():
            destination = "licenses/" + path.name
            if destination in files and files[destination].read_bytes() != path.read_bytes():
                raise ValueError(f"Conflicting notice: {path.name}")
            include(path, "licenses/" + path.name)
    forbidden = [name for name in files
                 if Path(name).suffix.lower() in {".dat", ".dat64", ".pdb", ".log"}
                 or Path(name).name.lower() in {"users.dat", "settings.json"}
                 or re.fullmatch(r"(?:msvcp\d+.*|msvcr\d+.*|vcruntime\d+.*|concrt\d+.*|ucrtbase|api-ms-win-crt-.*)\.dll",
                                 Path(name).name.lower())]
    if forbidden:
        raise ValueError(f"Personal or debug files in package: {forbidden}")
    saves = read_test_saves(args.test_saves)
    source_commit = args.source_commit or subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    readme = (
        f"pvz_perf {args.version} - Windows x64 / {args.platform} / {api}\n\n"
        f"Source revision: {source_commit}\n\n"
        f"{recommendation}"
        "BREAKING CHANGES: saves and main.pak64 are not interchangeable with v1.x/v2.x.\n"
        "The game loads only PVZDAT64 v4 level checkpoints (schema 0x10745).\n"
        "Convert original DAT / DAT64 v1/v2 checkpoints with the standalone converter first.\n"
        "users.dat summaries and PVZUSR64 v13 player details retain their existing formats.\n"
        "This main.pak64 uses generated reanim/particle definitions and current offline atlases. Update EXE and PAK64 together.\n"
        "Do not mix old and new executables, definition files or resource packages.\n\n"
        "Extract the entire archive into a fresh writable directory, then run pvz_perf.exe.\n"
        "data/ contains the generated PerfTest completed profile and canonical endless baseline.\n"
        "retail/ contains equivalent original-format saves; see TEST-SAVES.txt when present.\n"
        "No personal settings or progressed personal saves are included.\n"
        "Re-extract the supplied baseline before each benchmark comparison.\n"
        "The DAT/DAT64 converter is in data/pvz_dat64_converter.exe.\n"
        "Supported level formats: original 1073 DAT, DAT64 v1, v2 and v4; all v3 intermediates are unsupported.\n"
        "All DAT64 v3 intermediate inputs and targets are unsupported.\n"
        "The game uses the Microsoft Visual C++ x64 runtime (/MD); the converter is /MT.\n"
        "CRT DLLs are not bundled. Install a compatible Microsoft Visual C++ x64 runtime on the host.\n"
        + ("External scaling effects are configured in scale.json; see effects/README.md.\n"
           if modern else
           "Magpie effects and licenses are included for reference. This backend cannot run them.\n"
           "scale.json disables HDR, external effects and custom scaling.\n")
        + (f"Requires {api} and the game's bindless feature set.\n\n" if modern else
           f"Requires a vendor graphics driver supporting {api}; Microsoft Basic/GDI OpenGL is insufficient.\n"
           "Win7/old-GPU support requires the stated driver capabilities; real hardware coverage is limited.\n"
           "Double-click pvz_perf.exe. No LP14 affinity launcher is included for older CPUs.\n\n")
        +
        "This product includes portions of the PopCap Games Framework, "
        "(c) 2005-2009 PopCap Games, Inc. All rights reserved. "
        "(http://popcapframework.sourceforge.net/).\n"
        "Third-party components and game resources retain their respective rights "
        "and licenses; see licenses/.\n"
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name, path in sorted(files.items()):
            archive.write(path, name)
        for name, data in sorted(saves.items()):
            archive.writestr(name, data)
        archive.writestr("README.txt", readme)
    with zipfile.ZipFile(args.output) as archive:
        failed = archive.testzip()
        if failed:
            raise ValueError(f"Archive CRC failure: {failed}")
    print(json.dumps({"archive": str(args.output.resolve()),
                      "bytes": args.output.stat().st_size,
                      "sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
                      "files": len(files) + len(saves) + 1, "personal_saves": False,
                      "generated_test_saves": True}, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release", type=Path, required=True)
    parser.add_argument("--notices", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", default="v3.1")
    parser.add_argument("--test-saves", type=Path, required=True,
                        help="Generated package-test-saves.py ZIP; both data/ and retail/ are required")
    parser.add_argument("--effects-directory", type=Path, required=True,
                        help="Staged Magpie effects, README, SOURCES and licenses")
    parser.add_argument("--source-commit", help="Source revision; defaults to git HEAD at packaging time")
    parser.add_argument("--cpu-baseline", default="unspecified",
                        help="Build's CPU baseline, recorded without inferring build flags")
    parser.add_argument("--backend", choices=("d3d12", "vulkan", "opengl", "d3d9"), default="d3d12")
    parser.add_argument("--platform", choices=("win10", "win7"), default="win10")
    package(parser.parse_args())
