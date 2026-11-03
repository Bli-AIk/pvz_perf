"""Run retail/project scene measurements from one canonical endless save."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import time
import winreg

from windows_game_probe import Game, checked, kernel, user, w

ROOT = Path(__file__).resolve().parents[2]
RETAIL = Path("C:/UserProgram/Plants_Vs_Zombies_V1.2.0.1073_EN/PlantsVsZombies.exe")
RETAIL_DATA = Path("C:/ProgramData/PopCap Games/PlantsVsZombies/userdata")
PROJECT = ROOT / "out/x64-win32-vulkan/Release/pvz_perf.exe"
CONVERTER = PROJECT.parent / "data/pvz_dat64_converter.exe"
PROFILE = 1000003


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical_saves():
    assets = ROOT / "Tests"
    manifest = json.loads((assets / "manifest.json").read_text(encoding="utf-8"))
    paths = {}
    for kind in ("original", "project"):
        entry = manifest[kind]
        path = assets / entry["file"]
        if path.stat().st_size != entry["bytes"] or sha(path) != entry["sha256"]:
            raise RuntimeError(f"Canonical benchmark asset does not match manifest: {path}")
        paths[kind] = path
    return paths


def prepare(directory):
    saves = canonical_saves()
    build = PROJECT.parent.parent
    layout = json.loads(subprocess.check_output(
        [str(build / "Tests/performance/build/pvz_game_perf_layout.exe")], text=True))
    directory.mkdir(parents=True)
    fixture = directory / "fixture"
    old, new = fixture / "original", fixture / "project"
    old.mkdir(parents=True)
    new.mkdir()
    summary = (RETAIL_DATA / "users.dat").read_bytes()
    version, count = struct.unpack_from("<IH", summary)
    cursor, selected = 6, None
    for _ in range(count):
        length = struct.unpack_from("<H", summary, cursor)[0]
        start = cursor
        cursor += 2 + length
        sequence, identity = struct.unpack_from("<II", summary, cursor)
        cursor += 8
        if identity == 3:
            selected = summary[start:cursor - 4] + struct.pack("<I", PROFILE)
    if version != 14 or selected is None or cursor != len(summary):
        raise RuntimeError("Canonical user summary does not match the validated profile")
    one_user = struct.pack("<IH", version, 1) + selected
    (fixture / "users.dat").write_bytes(one_user)
    shutil.copy2(RETAIL_DATA / "user3.dat", old / f"user{PROFILE}.dat")
    shutil.copy2(saves["original"], old / f"game{PROFILE}_13.dat")
    for source, destination in (
        (old / f"user{PROFILE}.dat", new / f"user{PROFILE}.dat64"),
        (saves["original"], new / f"game{PROFILE}_58.dat64"),
    ):
        command = [str(CONVERTER), str(source), str(destination)]
        if source == saves["original"]:
            command += ["--target", "dat64-v4"]
        result = subprocess.run(command,
                                capture_output=True, text=True, check=True)
        (fixture / (destination.name + ".conversion.txt")).write_text(
            result.stdout + result.stderr, encoding="utf-8")
    layout["executable_sha256"] = sha(PROJECT)
    (fixture / "game-layout.json").write_text(json.dumps(layout, indent=2))
    manifest = {
        "source_save": str(saves["original"]),
        "source_sha256": sha(saves["original"]),
        "project_conversion_source": str(saves["original"]),
        "project_conversion_source_sha256": sha(saves["original"]),
        "project_save_format": "DAT64 v4",
        "backend": "VULKAN",
        "source_profile": str(RETAIL_DATA / "user3.dat"),
        "profile_id": PROFILE, "profile_summary_version": version,
        "canonical_original": str(old / f"game{PROFILE}_13.dat"),
        "canonical_original_sha256": sha(old / f"game{PROFILE}_13.dat"),
        "converted_project": str(new / f"game{PROFILE}_58.dat64"),
        "converted_project_sha256": sha(new / f"game{PROFILE}_58.dat64"),
        "converter_sha256": sha(CONVERTER), "project_sha256": sha(PROJECT),
        "retail_sha256": sha(RETAIL),
        "conversion_validation": "converter fully parses source/target and compares canonical semantics",
    }
    (fixture / "manifest.json").write_text(json.dumps(manifest, indent=2))
    return fixture


def wait_for(game, process, condition, timeout, description):
    deadline = time.perf_counter() + timeout
    while time.perf_counter() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"Game exited before {description}: {process.returncode}")
        try:
            state = game.snapshot()
        except RuntimeError as error:
            if str(error) != "Application is not constructed":
                raise
            state = None
        if state is not None and condition(state):
            return state
        time.sleep(.1)
    raise RuntimeError(f"Timed out waiting for {description}")


def enter_scene(game, process, scene):
    wait_for(game, process, lambda s: bool(s["mHWnd"]), 60, "native window")
    print(f"Prepare {scene} manually. Leave the start/resume action pending.", flush=True)
    input("Press Enter here to arm observation; start/resume the game at t=3 seconds. ")
    if process.poll() is not None:
        raise RuntimeError("Game exited before observation was armed")
    return game.snapshot()


def run_case(directory, fixture, kind, scene, multiplier, seconds):
    case = directory / f"{kind}-{scene}-{multiplier}"
    case.mkdir()
    env = os.environ.copy()
    for key in tuple(env):
        if key.startswith(("PVZ_", "VK_")):
            env.pop(key)
    env["DISABLE_RTSS_LAYER"] = "1"
    source = fixture / kind
    data = RETAIL_DATA if kind == "original" else case / "data"
    data.mkdir(exist_ok=True)
    shutil.copy2(fixture / "users.dat", data / "users.dat")
    for path in source.iterdir():
        shutil.copy2(path, data / path.name)
    executable = RETAIL if kind == "original" else PROJECT
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\PopCap\PlantsVsZombies") as key:
        retail_settings = {
            name: winreg.QueryValueEx(key, name)[0]
            for name in ("MusicVolume", "SfxVolume", "Muted", "ScreenMode", "WaitForVSync")
        }
    (case / "retail-settings.json").write_text(json.dumps(retail_settings, indent=2))
    if kind == "project":
        settings = json.loads((PROJECT.parent / "settings.json").read_text(encoding="utf-8-sig"))
        settings.update(targetFps=100, screenMode="windowed", customScale=0, waitForVSync=0,
                        musicVolume=retail_settings["MusicVolume"],
                        # Retail stores mixer percent; current JSON stores slider percent.
                        sfxVolume=round(max(0, min(100, retail_settings["SfxVolume"] / .65))),
                        muted=retail_settings["Muted"])
        (case / "settings.json").write_text(json.dumps(settings))
        env.update(PVZ_APPDATA_DIR=str(data), PVZ_SETTINGS_FILE=str(case / "settings.json"),
                   PVZ_LOG_DIR=str(case / "diagnostics"))
    output = (case / "process-output.txt").open("w")
    process = subprocess.Popen([str(executable)], cwd=executable.parent,
                               env=env, stdout=output, stderr=subprocess.STDOUT)
    handle = checked(kernel.OpenProcess(0x200 | 0x1000, False, process.pid))
    try:
        checked(kernel.SetProcessAffinityMask(handle, 0x4000))
    finally:
        kernel.CloseHandle(handle)
    game = None
    try:
        # Module enumeration is performed after the loader has started the image.
        time.sleep(1)
        game = Game(process.pid, kind, fixture / "game-layout.json")
        initial = enter_scene(game, process, scene)
        (case / "scene-entry.json").write_text(json.dumps(initial, indent=2))
        print(f"READY {kind} {scene} {multiplier}: {initial}", flush=True)
        from windows_game_probe import measure
        result = measure(game, seconds, scene, multiplier, case / "measurement",
                         preroll=3, interactive=True)
        print(json.dumps(result), flush=True)
        if process.poll() is None:
            print("Observation saved. Close the game normally to proceed to the next case.", flush=True)
            while process.poll() is None:
                time.sleep(.1)
        return result
    finally:
        if game:
            if process.poll() is None:
                user.PostMessageW(game.snapshot()["mHWnd"], 0x10, 0, 0)
                try:
                    process.wait(15)
                except subprocess.TimeoutExpired:
                    process.terminate()
                    process.wait(10)
            game.close()
        elif process.poll() is None:
            process.terminate()
            process.wait(10)
        output.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fixture", type=Path)
    parser.add_argument("--multiplier", choices=("11x", "25x"))
    parser.add_argument("--seconds", type=int, help="optional duration cap; otherwise Enter or game exit stops")
    parser.add_argument("--cases", nargs="+", default=(
        "original-credits", "project-credits", "original-endless", "project-endless"))
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args()
    if not args.prepare_only and args.multiplier is None:
        parser.error("Measurement requires an explicit --multiplier after the CPU has been locked")
    directory = args.output.resolve()
    if args.fixture:
        directory.mkdir(parents=True)
        fixture = args.fixture.resolve()
    else:
        fixture = prepare(directory)
    saves = canonical_saves()
    fixture_manifest = json.loads((fixture / "manifest.json").read_text())
    if sha(fixture / "original" / f"game{PROFILE}_13.dat") != sha(saves["original"]):
        raise RuntimeError("The original fixture differs from Tests")
    if sha(fixture / "project" / f"game{PROFILE}_58.dat64") != fixture_manifest["converted_project_sha256"]:
        raise RuntimeError("The converted fixture differs from its manifest")
    if sha(CONVERTER) != fixture_manifest["converter_sha256"] or sha(PROJECT) != fixture_manifest["project_sha256"]:
        raise RuntimeError("The executable/converter changed; prepare a new fixture")
    if args.prepare_only:
        print(fixture)
        return
    summary_file = RETAIL_DATA / "users.dat"
    backup = summary_file.read_bytes()
    (directory / "users-before.dat").write_bytes(backup)
    results = []
    try:
        for item in args.cases:
            kind, scene = item.split("-")
            results.append(run_case(directory, fixture, kind, scene, args.multiplier,
                                    args.seconds))
            (directory / "results.json").write_text(json.dumps(results, indent=2))
    finally:
        summary_file.write_bytes(backup)
        # Only the reserved fixture profile was used; normal user details/saves are untouched.
        retired = directory / "retired-fixture-files"
        retired.mkdir(exist_ok=True)
        for path in RETAIL_DATA.iterdir():
            if path.name in (f"user{PROFILE}.dat", f"game{PROFILE}_13.dat"):
                shutil.move(str(path), retired / path.name)


if __name__ == "__main__":
    main()
