"""Archive a completed native runtime log and its bounded plant coverage."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('log', type=Path)
parser.add_argument('executable', type=Path)
parser.add_argument('pak', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
destination = Path(__file__).resolve().parent
log = args.log.read_text(encoding='utf-8-sig')
assert 'FAIL ' not in log
assert 'PASS complete Plant primary-behavior catalog: all 53 SeedType values, four schedules each' in log
assert 'PASS static production particles:' in log, 'Require the full runtime suite to reach its final pass'
cases = []
for line in log.splitlines():
    match = re.fullmatch(r'Plant attack seed=(\d+) Hz=(\d+) shots=(\d+) types=(\d+) damage=(-?\d+) coins=(\d+)', line)
    if match:
        seed, hz, shots, types, damage, coins = map(int, match.groups())
        cases.append(dict(seed=seed, schedule=hz, group='attack/production', shots=shots,
                          projectile_type_mask=types, damage=damage, coins=coins))
    match = re.fullmatch(r'Plant special end seed=(\d+) Hz=(\d+) state=(\d+) dead=(\d+) targetDead=(\d+) damage=(-?\d+) targetRow=(\d+)', line)
    if match:
        seed, hz, state, dead, target_dead, damage, row = map(int, match.groups())
        cases.append(dict(seed=seed, schedule=hz, group='special/support', state=state,
                          plant_dead=bool(dead), target_dead=bool(target_dead), damage=damage, target_row=row))
assert len(cases) == 212
assert {(c['seed'], c['schedule']) for c in cases} == {(seed, hz) for seed in range(53) for hz in (100, 30, 240, 0)}
source = (root / 'pvzPerf/ConstEnums.h').read_text(encoding='utf-8-sig')
enum_body = source.split('enum SeedType', 1)[1].split('NUM_SEED_TYPES', 1)[0]
names = re.findall(r'^\s*(SEED_[A-Z0-9_]+)\s*(?:=\s*\d+)?\s*,', enum_body, re.M)
assert len(names) == 53
for case in cases:
    case['name'] = names[case['seed']]

def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

sources = ['Tests/native/ImageRuntimeTests.cpp', 'tools/test-image-runtime.ps1',
           'pvzPerf/Lawn/Plant.cpp', 'pvzPerf/Lawn/Zombie.cpp', 'pvzPerf/Lawn/Board.cpp',
           'pvzPerf/Lawn/Projectile.cpp', 'pvzPerf/Sexy.TodLib/EffectSystem.cpp',
           'pvzPerf/Sexy.TodLib/Reanimator.cpp', 'pvzPerf/SexyAppFramework/misc/TimeCounter.h']
report = dict(
    status='53 plant principal workflows verified; cross-mode coverage is indexed separately in behavior-audit-20261102',
    git_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    source_sha256={name: sha256(root / name) for name in sources},
    runtime_executable_sha256=sha256(args.executable), pak_sha256=sha256(args.pak),
    scenario_count=len(cases), plant_count=len(names),
    schedules={'100': '10,000,000 ns', '30': '33,333,333 ns', '240': '4,166,666 ns',
               '0': 'repeating 1234567/18765433/4000003/32333333/7891011/5432109/11111111 ns'},
    limitations=[
        'One principal scenario per SeedType, not every interaction/state/terrain/save combination.',
        'Stationary, high-health real zombies isolate the plant action; this is not a zombie-motion verdict.',
        'Umbrella uses the actual DoSpecial input; Gold Magnet starts its actual attracting animation, bypassing random activation delay.',
        'Sprout checks a real garden-owned plant and stable animation; growth/feeding is a separate requirement.',
        'The reported Blover coin-location incident is still unresolved; six-row native drop tests did not reproduce it.',
        'No frame-time, mobile or complete-mode acceptance claim is made.'
    ], cases=cases)
shutil.copyfile(args.log, destination / 'runtime.txt')
(destination / 'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(f'Archived {len(cases)} cases for {len(names)} plants')
