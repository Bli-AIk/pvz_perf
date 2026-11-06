#!/usr/bin/env python3
"""Run the real standalone compiler, verify direct numeric names, and build a candidate pak.

No C++ build, game launch, header installation, or runtime package replacement.
The old package is a read-only byte oracle; compiler outputs must already use
<stem>.compiled, without an intermediate rename.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
from types import SimpleNamespace
import zipfile

import build_numeric_runtime_pak64 as pak
import stage_runtime_package as stager


def verify_output(source: Path, compiled: Path, kind: str) -> dict:
    actual = stager.compiler_records(compiled, kind)
    expected = {}
    with zipfile.ZipFile(source) as archive:
        for name, info in pak.archive_index(archive).items():
            if info.is_dir():
                continue
            mapped = pak.migrated_numeric_path(name)
            if mapped and pak.numeric_kind(mapped) == kind:
                filename = Path(mapped).name
                pak.require(filename not in expected, f"Ambiguous package reference: {name}")
                expected[filename] = (info.filename, archive.read(info))
    pak.require(len(expected) == pak.COUNTS[kind],
                f"Reference package has an incomplete {kind} corpus")
    extra = set(actual) - set(expected)
    pak.require(set(expected) <= set(actual) and
                (not extra or kind == "particles" and extra == stager.PARTICLE_COMPILER_FIXTURES),
                f"Compiler {kind} record set differs from the production corpus and known fixtures")
    rows = []
    for name, (reference, data) in sorted(expected.items()):
        generated = actual[name].read_bytes()
        pak.require(generated == data, f"Compiler payload changed: {kind}/{name}")
        rows.append({"name": actual[name].name, "reference": reference,
                     "bytes": len(data), "sha256": pak.digest(data)})
    return {"production_records": len(rows), "compiler_only_fixtures": sorted(extra),
            "all_production_payloads_byte_identical": True, "files": rows}


def run_compiler(compiler: Path, arguments: list[str], log: Path) -> None:
    with log.open("xb") as stream:
        result = subprocess.run([str(compiler), *arguments], stdout=stream,
                                stderr=subprocess.STDOUT, check=False)
    pak.require(result.returncode == 0, f"Compiler exited {result.returncode}; see {log}")


def probe_compiler(compiler: Path, log: Path) -> None:
    with log.open("xb") as stream:
        result = subprocess.run([str(compiler), "--help"], stdout=stream,
                                stderr=subprocess.STDOUT, check=False)
    text = log.read_text(encoding="utf-8", errors="replace")
    pak.require(result.returncode in (0, 2) and
                "<Module>.compiled" in text and "<name>.compiled" in text,
                f"Compiler executable predates direct numeric output names; rebuild it serially: {compiler}")


def verify_particle_gold(directory: Path, registry: Path) -> dict:
    spec = importlib.util.spec_from_file_location(
        "particle_corpus_verifier", Path(__file__).with_name("verify-particle-emitter-corpus.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.verify(SimpleNamespace(gold_v2=directory, compiled=directory,
                                        image_registry=registry,
                                        output=directory / "verification.json"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=pak.ROOT)
    parser.add_argument("--compiler-root", type=Path, help="repository containing tools and assets")
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--source-pak", type=Path)
    parser.add_argument("--output", type=Path, required=True, help="fresh isolated output directory")
    args = parser.parse_args()
    root = args.root.resolve()
    compiler_root = (args.compiler_root or root).resolve()
    compiler = (args.compiler or compiler_root /
                "out/x64-win32-dx12/tools/resourceCompiler/build/reanimc.exe").resolve()
    source = (args.source_pak or root / "out/x64-win32-dx12/Release/main.pak64").resolve()
    output = args.output.absolute()
    owns_output = False
    report = {"status": "failed", "compiler_invoked": False}
    try:
        pak.require(compiler.is_file() and source.is_file(), "Missing compiler or reference package")
        pak.require(not os.path.lexists(output) and output.parent.is_dir(),
                    "Choose a fresh output directory with an existing parent")
        pak.require(not output.resolve().is_relative_to(root / "out") and
                    not output.resolve().is_relative_to(compiler_root / "assets"),
                    "Compiler verification output must remain outside build/author trees")
        output.mkdir()
        owns_output = True
        image = root / "pvzPerf/Sexy.TodLib/generated/image_symbol_ids.bin"
        font = root / "assets/definitions/toolchain/font_symbol_ids.bin"
        source_stamp = pak.fingerprint(source)
        report["compiler"] = {"path": str(compiler), **pak.fingerprint(compiler)}
        report["source"] = {"path": str(source), **source_stamp}
        common = ["--image-registry", str(image)]
        report["compiler_invoked"] = True
        probe_compiler(compiler, output / "compiler.usage.log")
        run_compiler(compiler, [
            "compile-v2", "--input", str(compiler_root / "assets/definitions/reanim/root.dsl"),
            "--output", str(output / "reanim"),
            "--type-map", str(root / "tools/reanim_type_map.txt"),
            "--family-file", str(root / "assets/definitions/toolchain/reanim_track_families.txt"),
            "--font-registry", str(font), *common,
        ], output / "reanim.compile.log")
        run_compiler(compiler, [
            "compile-particle-v2",
            "--input", str(compiler_root / "assets/definitions/particles/all.root.dsl"),
            "--output", str(output / "particles"),
            "--gold64", str(compiler_root / "assets/definitions/particle-reference"), *common,
        ], output / "particles.compile.log")
        report["records"] = {
            kind: verify_output(source, output / kind, kind) for kind in ("reanim", "particles")
        }
        report["particle_gold"] = verify_particle_gold(output / "particles", image)
        for generated, deployed in (
            (output / "reanim/ReanimTrackId.h", root / "pvzPerf/Sexy.TodLib/ReanimTrackId.h"),
            (output / "reanim/ReanimTrackMphfGlobal.h", root / "pvzPerf/Sexy.TodLib/ReanimTrackMphfGlobal.h"),
            (output / "particles/ParticleEmitterName.h",
             root / "pvzPerf/Sexy.TodLib/generated/ParticleEmitterName.h"),
        ):
            pak.require(generated.read_bytes() == deployed.read_bytes(),
                        f"Generated header differs; requires main-thread integration: {generated}")
        stager.stage_runtime(source, output / "runtime-stage",
                             {kind: output / kind for kind in ("reanim", "particles")}, root=root)
        package = output / "runtime.pak64"
        manifest = pak.build(source, output / "runtime-stage", package, root,
                             replace_definitions=True)
        report["candidate"] = manifest["output"]
        pak.require(pak.fingerprint(source) == source_stamp, "Runtime reference package changed")
        pak.require(pak.fingerprint(compiler) == {k: report["compiler"][k] for k in ("bytes", "sha256")},
                    "Compiler executable changed during verification")
        report["status"] = "passed"
    except (OSError, ValueError, AssertionError, struct.error) as error:
        report["error"] = str(error)
        if owns_output:
            target = output / "verification.failed.json"
            if not target.exists():
                target.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"Numeric compiler verification failed: {error}", file=sys.stderr)
        return 1
    (output / "verification.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Verified real compiler: 146 animation / 106 production particle records; {package}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
