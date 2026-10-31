"""Build first, then serially refresh current production microbenchmarks on LP14."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import statistics
import subprocess
import sys
import tempfile
import time


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--emitter-fixture', type=Path, required=True)
    parser.add_argument('--retail-probe', type=Path, required=True)
    parser.add_argument('--multiplier', choices=['11x', '25x'], required=True)
    parser.add_argument('--finalize-existing', action='store_true',
                        help='Audit an existing completed run after a separately recorded fixture repair')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    os.chdir(root)
    output = args.output.resolve()
    if args.finalize_existing:
        manifest = json.loads((output/'manifest.json').read_text())
        table = json.loads((output/'table-models/manifest.json').read_text())
        if len(table['jobs']) != 14 or any(j['build_exit'] or len(j['runs']) != 2 or
                any(r.get('exit', 1) or r.get('mismatch_marker') for r in j['runs']) for j in table['jobs']):
            raise RuntimeError('Table repair is incomplete')
        production = json.loads((output/'production/manifest.json').read_text())
        if not production.get('validation', '').startswith('PASS'):
            raise RuntimeError('Missing production vector validation')
        for name in ['reanim_frame_cache_benchmark', 'reanim_identity_benchmark',
                     'particle_life_side_cache_benchmark', 'trig']:
            for run in [1,2]:
                path = output/name/('run-'+str(run))/'samples.csv'
                if not path.is_file() or path.stat().st_size < 100:
                    raise RuntimeError('Missing measured process '+str(path))
        frozen = json.loads((output/'frozen-draw-summary.json').read_text())
        if len(frozen['results']) != 2 or any(r['samples'] != 640 for r in frozen['results']):
            raise RuntimeError('Incomplete fixed DrawScreen processes')
        for path in (root/'pvzPerf/Sexy.TodLib').glob('*.cpp'):
            target = output/'source-snapshot'/path.name
            target.parent.mkdir(exist_ok=True)
            shutil.copy2(path, target)
        manifest.update(complete=True,
                        fixture_repair='Explicit three-clock dispatcher ABI; table group rerun and audited',
                        game_executable_sha256=sha(root/'out/x64-win32-vulkan/Release/pvz_perf.exe'),
                        pak_sha256=sha(root/'out/x64-win32-vulkan/Release/main.pak64'))
        (output/'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
        (output/'sha256.json').write_text(json.dumps({str(p.relative_to(output)):sha(p) for p in output.rglob('*')
            if p.is_file() and p.name!='sha256.json'}, indent=2))
        print('COMPLETE audited fixture repair '+str(output), flush=True)
        return
    output.mkdir(parents=True, exist_ok=False)
    work = Path(tempfile.mkdtemp(prefix='pvz-production-refresh-'))
    tools = root/'Tests/performance'
    builds = {}
    manifest = dict(version='3.2.4', affinity='LP14 / 0x4000',
                    frequency=args.multiplier+' user-controlled; not independently sampled',
                    work=str(work), serial=True, runs=[])

    def persist():
        (output/'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')

    def invoke(command, stdout, cwd=root, env=None, timeout=1200):
        start = time.perf_counter()
        result = subprocess.run([str(x) for x in command], cwd=cwd, env=env,
                                capture_output=True, timeout=timeout)
        stdout.parent.mkdir(parents=True, exist_ok=True)
        stdout.write_bytes(result.stdout)
        stdout.with_suffix('.stderr.txt').write_bytes(result.stderr)
        manifest['runs'].append(dict(command=[str(x) for x in command],
                                    seconds=time.perf_counter()-start, exit=result.returncode))
        persist()
        if result.returncode:
            raise RuntimeError('Failed; see '+str(stdout))
        print('PASS '+stdout.parent.name+'/'+stdout.name, flush=True)

    # All compiles/links finish before timed executions begin.
    for name, backend in [('production_leaf_benchmark', 'vulkan'),
                          ('production_structure_benchmark', 'vulkan'),
                          ('reanim_frame_cache_benchmark', 'vulkan'),
                          ('reanim_identity_benchmark', 'vulkan'),
                          ('particle_life_side_cache_benchmark', 'vulkan'),
                          ('production_frozen_draw', 'dx12')]:
        dest = work/('build-'+name)
        invoke([sys.executable, tools/'build-production-benchmark.py',
                '--build', root/('out/x64-win32-'+backend), '--source', tools/(name+'.cpp'),
                '--output', dest], output/'builds'/name/'build.txt')
        shutil.copy2(dest/'build-manifest.json', output/'builds'/name/'build-manifest.json')
        builds[name] = dest/'benchmark.exe'

    # Real FastMath translation unit, linked separately from the benchmark.
    script = work/'build-trig.cmd'
    script.write_text('@echo off\ncall "C:/UserProgram/Visual Studuio/2026/Community/Common7/Tools/VsDevCmd.bat" -arch=x64 -no_logo\n'
        + 'cl /nologo /O2 /EHsc /std:c++latest /MD /I"'+str(root/'pvzPerf/SexyAppFramework')+'" '
        + '"'+str(tools/'trig_power_benchmark.cpp')+'" "'+str(root/'pvzPerf/SexyAppFramework/misc/FastMath.cpp')
        +'" /Fe"'+str(work/'trig.exe')+'"\n', encoding='utf-8')
    invoke([os.environ['COMSPEC'], '/d', '/c', script], output/'builds/trig/build.txt', cwd=work)
    launcher = work/'pin.exe'
    script = work/'build-pin.cmd'
    script.write_text('@echo off\ncall "C:/UserProgram/Visual Studuio/2026/Community/Common7/Tools/VsDevCmd.bat" -arch=x64 -no_logo\n'
        +'cl /nologo /O2 /EHsc /std:c++20 "'+str(tools/'run-pinned-benchmark.cpp')+'" /Fe"'+str(launcher)+'"\n', encoding='utf-8')
    invoke([os.environ['COMSPEC'], '/d', '/c', script], output/'builds/launcher/build.txt', cwd=work)

    invoke([sys.executable, tools/'run-production-small-benchmarks.py',
            '--retail', 'C:/UserProgram/Plants_Vs_Zombies_V1.2.0.1073_EN/PlantsVsZombies.exe',
            '--retail-probe', args.retail_probe, '--current-probe', builds['production_leaf_benchmark'],
            '--structure-probe', builds['production_structure_benchmark'],
            '--pak', root/'out/x64-win32-vulkan/Release/main.pak64', '--output', output/'production'],
           output/'production-console.txt')
    for name in ['reanim_frame_cache_benchmark', 'reanim_identity_benchmark', 'particle_life_side_cache_benchmark', 'trig']:
        for run in [1, 2]:
            directory = output/name/('run-'+str(run))
            directory.mkdir(parents=True)
            if name == 'trig':
                command = [launcher, work/'trig.exe']
            elif name in ['particle_life_side_cache_benchmark', 'reanim_identity_benchmark']:
                command = [builds[name]]
            else:
                command = [builds[name], directory] + (['reverse'] if run == 2 else [])
            invoke(command, directory/'samples.csv')
    # Frame crossing and repeated consumers are current behavior; keep separate scope.
    for mode in ['compare_progression', 'repeat_queries']:
        directory = output/'reanim_frame_cache_benchmark'/mode
        directory.mkdir()
        invoke([builds['reanim_frame_cache_benchmark'], directory, mode], directory/'samples.csv')
    pixels = []
    frozen = []
    for run in [1, 2]:
        directory = output/'frozen-draw'/('run-'+str(run))
        directory.mkdir(parents=True)
        env = dict(os.environ, PVZ_APPDATA_DIR=str(directory/'profile'),
                   PVZ_SETTINGS_FILE=str(directory/'settings.json'))
        invoke([builds['production_frozen_draw'], root/'out/x64-win32-dx12/Release', directory],
               directory/'stdout.txt', env=env)
        data = list(csv.DictReader((directory/'frozen-draw-perf.csv').open()))
        values = sorted(float(r['draw_cpu_us']) for r in data)
        pixels.append(sha(directory/'frozen-draw-pixels.bin'))
        frozen.append(dict(run=run, samples=len(data), median_cpu_us=statistics.median(values),
                           p90_nearest_rank_us=values[(len(values)*9+9)//10-1],
                           median_thread_cycles=statistics.median(int(r['thread_cycles']) for r in data)))
    if pixels[0] != pixels[1]:
        raise RuntimeError('Frozen-draw pixels differ between processes')
    (output/'frozen-draw-summary.json').write_text(json.dumps(dict(
        backend='D3D12', scene='800x600; 32 plants; 40 zombies', warmup=128,
        scope='CPU DrawScreen; flush/fence/readback excluded', pixel_sha256=pixels[0], results=frozen), indent=2))

    # Only retained current tables and identity contracts. Retired models stay archived.
    invoke([sys.executable, tools/'refresh-small-benchmarks.py', '--archive', args.archive,
            '--emitter-fixture', args.emitter_fixture, '--output', output/'table-models',
            '--only', 'current_|bench_particle_emitter_names',
            '--vs-root', 'C:/UserProgram/Visual Studuio/2026/Community'], output/'table-console.txt')
    table = json.loads((output/'table-models/manifest.json').read_text())
    if len(table['jobs']) != 14 or any(j['build_exit'] or len(j['runs']) != 2 or
            any(r.get('exit', 1) or r.get('mismatch_marker') for r in j['runs']) for j in table['jobs']):
        raise RuntimeError('Incomplete table/identity verification')
    for path in (root/'pvzPerf/Sexy.TodLib').glob('*.cpp'):
        target = output/'source-snapshot'/path.name
        target.parent.mkdir(exist_ok=True)
        shutil.copy2(path, target)
    manifest.update(complete=True, game_executable_sha256=sha(root/'out/x64-win32-vulkan/Release/pvz_perf.exe'),
                    pak_sha256=sha(root/'out/x64-win32-vulkan/Release/main.pak64'))
    persist()
    (output/'sha256.json').write_text(json.dumps({str(p.relative_to(output)):sha(p) for p in output.rglob('*')
        if p.is_file() and p.name!='sha256.json'}, indent=2))
    print('COMPLETE '+str(output), flush=True)


if __name__ == '__main__':
    main()
