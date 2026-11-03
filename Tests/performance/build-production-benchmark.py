"""Link a dedicated benchmark against the existing Release game objects; do not alter the game EXE."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import hashlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--extra-source", type=Path, action="append", default=[],
                        help="Additional test translation unit linked with the same game flags")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ninja", type=Path, default=Path("C:/UserProgram/DevTools/ninja/ninja.exe"))
    parser.add_argument("--vs-root", type=Path, default=Path("C:/UserProgram/Visual Studuio/2026/Community"))
    parser.add_argument("--bass-dll", type=Path,
                        help="Explicit x64 BASS runtime when the paired Release has not staged it")
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[2]
    build = args.build.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    bass = args.bass_dll.resolve() if args.bass_dll else build / "Release/bass.dll"
    if not bass.is_file():
        raise RuntimeError(f"Missing paired audio runtime: {bass}; stage it or pass --bass-dll before running")
    bass_bytes = bass.read_bytes()
    pe_offset = int.from_bytes(bass_bytes[0x3c:0x40], "little")
    if bass_bytes[pe_offset:pe_offset + 6] != b"PE\0\0\x64\x86":
        raise RuntimeError(f"Expected an x64 PE BASS runtime: {bass}")
    database = json.loads(subprocess.check_output([str(args.ninja), "-C", str(build), "-t", "compdb", "-x"]))
    mains = [entry["command"] for entry in database if re.search(r"/FoSexyAppFramework[\\/]CMakeFiles[\\/]LawnProject.dir[\\/]__[\\/]main.cpp.obj", entry["command"])]
    links = [entry["command"] for entry in database if "vs_link_exe" in entry["command"] and "pvz_perf.exe" in entry["command"]]
    if len(mains) != 1 or len(links) != 1:
        raise RuntimeError("Expected one existing game main compile and executable link command")
    source = args.source.resolve()
    obj = output / "benchmark.obj"
    executable = output / "benchmark.exe"
    # CMake may spell TEMP using ADMINI~1 while pathlib resolves Administrator.
    # Match the actual compiler input instead of assuming identical path spelling.
    compile_command, replacements = re.subn(
        r'(?i)(/c|-c)\s+("[^"\r\n]*[/\\]main\.cpp"|[^\s"]*[/\\]main\.cpp)(?=\s|$)',
        lambda match: match[1] + ' "' + str(source) + '"', mains[0])
    if replacements != 1:
        raise RuntimeError("Expected one main.cpp compiler input to replace")
    compile_command = re.sub(r"/Fo\S+", lambda _: '/Fo"' + str(obj) + '"', compile_command)
    compile_command = re.sub(r"/Fd\S+", lambda _: '/Fd"' + str(output / "compile.pdb") + '"', compile_command)
    compile_commands = [compile_command]
    extra_objects = []
    for index, extra in enumerate(args.extra_source):
        extra = extra.resolve()
        extra_obj = output / f"extra-{index}.obj"
        extra_command = compile_command.replace(str(source), str(extra))
        extra_command = extra_command.replace(str(obj), str(extra_obj))
        extra_command = extra_command.replace(str(output / "compile.pdb"),
                                               str(output / f"extra-{index}.pdb"))
        compile_commands.append(extra_command)
        extra_objects.append(extra_obj)
    link = links[0].split(" -- ", 1)[1].split(" && ", 1)[0]
    link = link.replace("SexyAppFramework\\CMakeFiles\\LawnProject.dir\\__\\main.cpp.obj", '"' + str(obj) + '"')
    link += ''.join(' "' + str(extra_obj) + '"' for extra_obj in extra_objects)
    for key, filename in [("out", "benchmark.exe"), ("pdb", "benchmark.pdb"), ("implib", "benchmark.lib")]:
        link = re.sub(r"/" + key + r":\S+", lambda _: '/' + key + ':"' + str(output / filename) + '"', link, flags=re.I)
    link = re.sub(r"/subsystem:windows", "/subsystem:console", link, flags=re.I)
    match = re.match(r'"[^"]+link.exe"|\S*link.exe', link, re.I)
    if not match:
        raise RuntimeError("Cannot find linker executable")
    response = output / "link.rsp"
    response.write_text(link[match.end():], encoding="utf-8")
    vc = args.vs_root / "VC/Tools/MSVC/14.51.36231"
    commands = ["@echo off", 'call "' + str(args.vs_root / "Common7/Tools/VsDevCmd.bat") + '" -arch=x64 -no_logo',
                'set "PATH=' + str(vc / "bin/Hostx64/x64") + ';%PATH%"',
                'set "INCLUDE=' + str(vc / "include") + ';%INCLUDE%"',
                'set "LIB=' + str(vc / "lib/x64") + ';%LIB%"',
                ]
    for command in compile_commands:
        commands.extend([command, "if errorlevel 1 exit /b 1"])
    commands.extend([match.group() + ' @"' + str(response) + '"', "exit /b %errorlevel%"])
    batch = output / "build.cmd"
    batch.write_text("\n".join(commands), encoding="utf-8")
    result = subprocess.run([os.environ["COMSPEC"], "/d", "/c", str(batch)], cwd=build, capture_output=True)
    (output / "build.txt").write_bytes(result.stdout + result.stderr)
    if result.returncode:
        print((result.stdout + result.stderr).decode("utf-8", errors="replace")[-5000:])
        raise RuntimeError("Production benchmark build failed")
    for dll in (build / "Release").glob("*.dll"):
        shutil.copy2(dll, output / dll.name)
    shutil.copy2(bass, output / "bass.dll")
    shaders = build / "Release/shaders"
    if shaders.is_dir():
        shutil.copytree(shaders, output / "shaders", dirs_exist_ok=True)
    metadata = {
        "source": str(source),
        "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
        "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "game_executable_sha256": hashlib.sha256((build / "Release/pvz_perf.exe").read_bytes()).hexdigest(),
        "compile_command": compile_command,
        "extra_sources": [{"source": str(extra.resolve()),
                           "sha256": hashlib.sha256(extra.read_bytes()).hexdigest()}
                          for extra in args.extra_source],
        "link_response_sha256": hashlib.sha256(response.read_bytes()).hexdigest(),
        "lto_compile": "/GL" in compile_command,
        "lto_link": "/LTCG" in link.upper(),
        "bass_sha256": hashlib.sha256(bass_bytes).hexdigest(),
    }
    (output / "build-manifest.json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8")
    print(executable, flush=True)


if __name__ == "__main__":
    main()
