"""Serial table/model refresh; retired runtime paths are excluded from the runnable set."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def declaration(text, name, last=False):
    matches = list(re.finditer(r"^.*\b" + name + r"\s*\[.*?=\s*\{", text, re.M))
    if not matches:
        raise ValueError("Missing declaration " + name)
    match = matches[-1] if last else matches[0]
    start = match.start()
    opening = text.index("{", match.start())
    # Tokenize strings/comments so braces inside them cannot terminate a declaration.
    tokens = re.finditer(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[opening:], re.S)
    depth = 0
    for token in tokens:
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                end = text.index(";", opening + token.end()) + 1
                return start, end, text[start:end]
    raise ValueError("Unclosed declaration " + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--vs-root", type=Path, required=True)
    parser.add_argument("--msvc-version", default="14.51.36231")
    parser.add_argument("--only", help="Run only jobs whose source stem matches this regular expression")
    parser.add_argument("--emitter-fixture", type=Path)
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    work = Path(tempfile.mkdtemp(prefix="pvz-small-bench-build-"))
    archived = work / "archive"
    for source in args.archive.rglob("*"):
        if source.suffix in (".h", ".cpp"):
            target = archived / source.relative_to(args.archive)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            if target.suffix == ".h":
                original = target.read_text(encoding="utf-8-sig")
                dependencies = project / "Tests/reference/microbenchmarks/dependencies"
                adjusted = original.replace('"C:/Users/Administrator/CLionProjects/codex/PlantsVsZombiesModern/', '"' + dependencies.as_posix() + '/')
                if adjusted != original:
                    target.write_text(adjusted, encoding="utf-8")
    env_file = work / "environment.cmd"
    env_file.write_text('@echo off\ncall "' + str(args.vs_root / "Common7/Tools/VsDevCmd.bat") + '" -arch=x64 -no_logo >nul\nset\n')
    setup = subprocess.run([os.environ["COMSPEC"], "/d", "/c", str(env_file)], capture_output=True, text=True, check=True)
    env = os.environ.copy()
    for line in setup.stdout.splitlines():
        if "=" in line and not line.startswith("="):
            key, value = line.split("=", 1)
            env[key.upper()] = value
    toolset = args.vs_root / "VC/Tools/MSVC" / args.msvc_version
    env["PATH"] = str(toolset / "bin/Hostx64/x64") + ";" + env["PATH"]
    env["INCLUDE"] = str(toolset / "include") + ";" + env.get("INCLUDE", "")
    env["LIB"] = str(toolset / "lib/x64") + ";" + env.get("LIB", "")
    compiler = toolset / "bin/Hostx64/x64/cl.exe"
    launcher = work / "run-pinned-benchmark.exe"
    subprocess.run([str(compiler), "/nologo", "/O2", "/EHsc", "/std:c++20", "/Fo"+str(work / "launcher.obj"), "/Fe"+str(launcher), str(project / "Tests/performance/run-pinned-benchmark.cpp")], env=env, cwd=work, check=True)
    selection = list(sorted((archived / "r4").glob("bench*.cpp")))
    selection += [archived / "ifelse" / f"bench{n}_{name}.cpp" for n, name in [(1,"seedpacket"),(2,"potoffset"),(4,"coindraw"),(5,"plantshadow"),(7,"plantheight"),(8,"fire")]]
    selection += [p for p in sorted((archived / "ui").glob("bench*.cpp")) if p.name[5:7] in ("02","07","08","09","11","13")]
    selection += [archived / p for p in ("r3/t16_cursor.cpp", "r3/b_zombie_events.cpp", "r2plant/bench_plant.cpp", "cellbucket/bench_cellbucket.cpp", "todlib/bench_attachment.cpp")]
    selection += [project / "Tests/performance" / (n + ".cpp") for n in ("bench_curve_dispatch", "bench_particle_roster", "bench_reanim_trackwalk", "bench_zombie_rowscan", "bench_sincos")]
    # Frozen records remain in evidence; these no longer describe a current
    # field/path or a shipped optimization candidate.
    retired = {"bench03_shootctr", "bench10_prodraw", "bench12_proimmediate", "bench13_dynamics",
               "bench16_falltime", "bench11_ZombatarItems_DrawCategoryHighlight", "bench_plant"}
    selection = [source for source in selection if source.stem not in retired]
    current = [("01_dmgflags","kDamageRangeFlags","Lawn/Plant.cpp"),("02_fireproj","kFireProjectiles","Lawn/Plant.cpp"),("04_flowerpot","kFlowerPotAdjust","Lawn/Plant.cpp"),("05_packetdraw","kSeedPacketDrawMetrics","Lawn/SeedPacket.cpp"),("06_zimgoff","kZombieImageOffsetFix","Lawn/Zombie.cpp"),("07_walkhead","kWalkHeadPos","Lawn/Zombie.cpp"),("08_seedupdate","kSeedTypeUpdates","Lawn/Plant.cpp"),("11_proshadow","kShadowParams","Lawn/Projectile.cpp"),("14_seedcost","kSeedCostOverrides","Lawn/Plant.cpp"),("15_storecost","kStoreItemCosts","Lawn/UI/StoreScreen.cpp")]
    jobs = [(p, "historical_fixture_reproduction", {}) for p in selection]
    for fixture, table, relative in current:
        source = project / "pvzPerf" / relative
        if not source.exists():
            source = next((project / "pvzPerf").rglob(Path(relative).name))
        text = source.read_text(encoding="utf-8-sig")
        _, _, extracted = declaration(text, table)
        old = archived / "r4" / ("bench" + fixture + ".cpp")
        original = old.read_text(encoding="utf-8-sig")
        lo, hi, _ = declaration(original, table)
        target = old.with_name("current_" + old.name)
        adapted = original[:lo] + extracted + original[hi:]
        metadata = {"production_source":str(source.relative_to(project)), "production_sha256":digest(source), "table":table, "declaration_sha256":hashlib.sha256(extracted.encode()).hexdigest()}
        if table == "kSeedTypeUpdates" and "std::int64_t" in extracted:
            # Keep the production table verbatim; adapt only common test stubs
            # and the invocation ABI to the explicit owner/visual/animation clocks.
            split = adapted.index('// ---------------- FormA')
            prefix = re.sub(r'void (Update\w+)\(\)',
                            r'void \1(std::int64_t = 0, std::int64_t = 0, std::int64_t = 0)', adapted[:split])
            adapted = prefix + adapted[split:]
            for helper in ("kProductionUpdate", "kSpikeweedUpdate"):
                pattern = r'^\s*static constexpr auto '+helper+r'\s*=.*?;\s*};'
                helper_line = re.search(pattern, text, re.M)
                if not helper_line:
                    raise ValueError("Missing current dispatcher helper " + helper)
                adapted = re.sub(pattern, lambda _: helper_line.group(), adapted, count=1, flags=re.M)
            adapted = adapted.replace('void (*aUpdate)(Plant*)',
                                      'void (*aUpdate)(std::int64_t, std::int64_t, std::int64_t, Plant*)')
            adapted = adapted.replace('aUpdate(this_);', 'aUpdate(0, 0, 0, this_);')
            metadata['harness_adapter'] = 'Explicit 3-clock ABI; common Mix stubs ignore clocks. Not full Plant::Update.'
        target.write_text(adapted, encoding="utf-8")
        jobs.append((target, "current_table_in_historical_harness", metadata))
    for fixture, table, relative, last in [("ifelse/bench7_plantheight.cpp", "kPlantHeightOffsets", "Lawn/Plant.cpp", True), ("ifelse/bench5_plantshadow.cpp", "kPlantShadow", "Lawn/Plant.cpp", True), ("ui/bench13_ZombatarColors_SetColorMode.cpp", "kZombatarPaletteRange", "Lawn/Widget/ZombatarColors.cpp", False)]:
        source = project / "pvzPerf" / relative
        _, _, extracted = declaration(source.read_text(encoding="utf-8-sig"), table)
        old = archived / fixture
        original = old.read_text(encoding="utf-8-sig")
        lo, hi, _ = declaration(original, table, last=last)
        target = old.with_name("current_" + old.name)
        target.write_text(original[:lo] + extracted + original[hi:], encoding="utf-8")
        jobs.append((target, "current_table_in_historical_harness", {"production_source":str(source.relative_to(project)), "production_sha256":digest(source), "table":table, "declaration_sha256":hashlib.sha256(extracted.encode()).hexdigest(), "candidate":"C(static)" if last else "B"}))
    if args.emitter_fixture:
        shutil.copy2(args.emitter_fixture, work / "particle_emitter_bench_fixture.h")
        jobs.append((project / "Tests/performance/bench_particle_emitter_names.cpp", "current_name_contract_historical_corpus", {"fixture_sha256":digest(args.emitter_fixture)}))
    if args.only:
        jobs = [job for job in jobs if re.search(args.only, job[0].stem)]
    manifest = {"created_local":time.strftime("%Y-%m-%dT%H:%M:%S"), "work":str(work), "compiler":str(compiler), "flags":["/O2","/EHsc","/std:c++20","/GL-"], "affinity":"0x4000", "frequency":"User-confirmed 11x; not measured by runner", "scope":"Archived fixtures remain historical models. Extracted tables are current production declarations in the unchanged historical harness; not complete current functions.", "jobs":[]}
    for index, (source, scope, extra) in enumerate(jobs):
        label = f"{index:02d}_{source.stem}"
        target_dir = output / label
        target_dir.mkdir()
        shutil.copy2(source, target_dir / source.name)
        executable = work / (label + ".exe")
        command = [str(compiler), "/nologo", "/O2", "/EHsc", "/std:c++20", "/GL-", "/utf-8", "/I"+str(project / "pvzPerf"), "/Fo"+str(work / (label+".obj")), "/Fe"+str(executable), str(source)]
        if scope == "current_name_contract_historical_corpus":
            command[command.index("/std:c++20")] = "/std:c++23preview"
            command += ["/Ob2", "/DNDEBUG", "/W4", "/MD", "/fp:precise", "/I"+str(work), "/link", "/INCREMENTAL:NO", "/OPT:REF", "/OPT:NOICF"]
        result = {"name":label,"scope":scope,"source_sha256":digest(source),"command":command,**extra}
        compiled = subprocess.run(command, env=env, cwd=work, capture_output=True, timeout=120)
        (target_dir / "build.txt").write_bytes(compiled.stdout + compiled.stderr)
        result["build_exit"] = compiled.returncode
        result["runs"] = []
        if compiled.returncode == 0:
            result["exe_sha256"] = digest(executable)
            for repeat in range(2):
                started = time.perf_counter()
                try:
                    invocation = [str(launcher), str(executable)]
                    if scope == "current_name_contract_historical_corpus":
                        report = target_dir / f"reports-{repeat+1}"
                        report.mkdir()
                        invocation.append(str(report))
                    run = subprocess.run(invocation, env=env, cwd=work, capture_output=True, timeout=610)
                    content = run.stdout + run.stderr
                    (target_dir / f"run-{repeat+1}.txt").write_bytes(content)
                    result["runs"].append({"exit":run.returncode,"seconds":time.perf_counter()-started,"mismatch_marker":bool(re.search(rb"\b(?:MISMATCH|DIFF|FAILED)\b", content))})
                    if run.returncode:
                        break
                except subprocess.TimeoutExpired:
                    result["runs"].append({"timeout":True})
                    break
        manifest["jobs"].append(result)
        (output / "manifest.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding="utf-8")
        print(f'{index+1}/{len(jobs)} {label} build={compiled.returncode} runs={result["runs"]}', flush=True)


if __name__ == "__main__":
    main()
