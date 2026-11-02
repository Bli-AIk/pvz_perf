"""Archive successful, real-game zombie workflows without overstating coverage."""
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
assert 'PASS actual Boss primary actions:' in log
assert 'PASS actual Zombie primary catalog:' in log
assert 'PASS static production particles:' in log, 'Require the full runtime suite to reach its final pass'
cases = []
for line in log.splitlines():
    match = re.fullmatch(r'Zombie primary type=(\d+) Hz=(\d+) ok=1 (.*)', line)
    if match:
        zombie, hz = map(int, match.groups()[:2])
        fields = dict(item.split('=', 1) for item in match.group(3).split())
        cases.append(dict(type=zombie, schedule=hz, group='primary action', observations=fields))
    match = re.fullmatch(r'Catapult end Hz=(\d+) launches=20 shots=20 ammoNs=0 x=([^ ]+) scene=3', line)
    if match:
        cases.append(dict(type=22, schedule=int(match[1]), group='twenty launches and walking resume',
                          observations=dict(x=float(match[2]))))
    match = re.fullmatch(r'Zombie Boss Hz=(\d+) stage=5 spawn=1 ball=1 rv=1 bungee=1 stomp=1 phase=79', line)
    if match:
        cases.append(dict(type=25, schedule=int(match[1]), group='entry and five attacks'))
assert len(cases) == 136
assert {(case['type'], case['schedule']) for case in cases} == {
    (zombie, hz) for zombie in range(34) for hz in (100, 30, 240, 0)}
status_cases = re.findall(r'^Zombie butter type=(\d+) Hz=(\d+) maxResumeJump=([^ ]+) finalX=([^\r\n]+)$', log, re.M)
assert len(status_cases) == 20
assert {(int(zombie), int(hz)) for zombie, hz, _, _ in status_cases} == {
    (zombie, hz) for zombie in (0, 10, 11, 14, 22) for hz in (100, 30, 240, 0)}
mount_cases = re.findall(r'^Dolphin mount Hz=(\d+) delta=([^ ]+) nextDelta=([^ ]+) ok=1$', log, re.M)
assert len(mount_cases) == 4 and {int(hz) for hz, _, _ in mount_cases} == {100, 30, 240, 0}
source_paths = ['Tests/native/ImageRuntimeTests.cpp', 'pvzPerf/Lawn/Zombie.cpp',
                'pvzPerf/Lawn/Zombie.h', 'pvzPerf/Lawn/Motion.cpp']
report = dict(
    scope='Primary behavior workflows for every ZombieType; not exhaustive interactions, states, saves or modes',
    schedules={'100': '100 Hz', '30': '30 Hz', '240': '240 Hz', '0': 'irregular 1.23-32.33 ms'},
    cases=cases,
    butter_cases=[dict(type=int(zombie), schedule=int(hz), maximum_resume_jump=float(jump), final_x=float(x))
                  for zombie, hz, jump, x in status_cases],
    dolphin_mount_cases=[dict(schedule=int(hz), mount_delta=float(delta), next_delta=float(next_delta))
                         for hz, delta, next_delta in mount_cases],
    sources={path: hashlib.sha256((root/path).read_bytes()).hexdigest() for path in source_paths},
    executable_sha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
    pak_sha256=hashlib.sha256(args.pak.read_bytes()).hexdigest(),
    git_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    working_tree_modified=True,
)
shutil.copyfile(args.log, destination/'runtime.txt')
(destination/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print(f'Archived {len(cases)} primary workflows and {len(status_cases)} butter hold/resume cases')
