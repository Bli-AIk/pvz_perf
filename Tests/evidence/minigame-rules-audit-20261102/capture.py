"""Archive the actual twenty-mode rules checks and explicit fixture limits."""
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
assert 'FAIL ' not in log and 'Minigame failed' not in log
assert 'PASS actual twenty minigame rules:' in log
assert 'PASS static production particles:' in log, 'Require the complete runtime suite'
cases = []
for line in log.splitlines():
    match = re.fullmatch(r'Minigame primary mode=(\d+) Hz=(\d+) (.*)', line)
    if match:
        cases.append(dict(mode=int(match[1]), schedule=int(match[2]),
                          observations=dict(item.split('=', 1) for item in match[3].split())))
assert len(cases) == 80
assert {(case['mode'], case['schedule']) for case in cases} == {
    (mode, hz) for mode in range(1, 21) for hz in (100, 30, 240, 0)}
portal = []
for zombie, hz, arrival, next_x, row in re.findall(
        r'^Portal zombie type=(\d+) Hz=(\d+) arrivalX=([^ ]+) nextX=([^ ]+) row=(\d+)$', log, re.M):
    assert int(row) == 1 and 600 < float(arrival) < 760 and abs(float(next_x)-float(arrival)) < 15
    portal.append(dict(type=int(zombie), schedule=int(hz), arrival_x=float(arrival), next_x=float(next_x)))
assert len(portal) == 12
assert {(case['type'], case['schedule']) for case in portal} == {
    (zombie, hz) for zombie in (0, 16, 22) for hz in (100, 30, 240, 0)}
food = re.findall(r'^Aquarium pursuit Hz=(\d+) brain=0 fishHealth=(\d+) x=([^\r\n]+)$', log, re.M)
assert len(food) == 4 and all(int(health) > 150 for _, health, _ in food)
source_paths = ['Tests/native/ImageRuntimeTests.cpp', 'pvzPerf/Lawn/Challenge.cpp',
                'pvzPerf/Lawn/Zombie.cpp', 'pvzPerf/Lawn/Board.cpp',
                'pvzPerf/Lawn/System/SaveClockProjection.cpp']
report = dict(
    scope='Twenty achievement minigames: listed initial/rule/input/clock scenarios, not full campaign playthroughs',
    remaining_scope='Adventure, puzzle, survival, limbo and garden/tree modes are not certified by this evidence',
    cases=cases, portal_cases=portal,
    aquarium_pursuit=[dict(schedule=int(hz), recovered_health=int(health), x=float(x)) for hz, health, x in food],
    sources={path: hashlib.sha256((root/path).read_bytes()).hexdigest() for path in source_paths},
    executable_sha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
    pak_sha256=hashlib.sha256(args.pak.read_bytes()).hexdigest(),
    git_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    working_tree_modified=True,
)
shutil.copyfile(args.log, destination/'runtime.txt')
(destination/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print(f'Archived {len(cases)} minigame cases, {len(portal)} portal continuations and four aquarium pursuits')
