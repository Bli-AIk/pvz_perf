"""Capture the 13 hidden-mode rule/action scenarios, separately from playthroughs."""
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
assert 'FAIL ' not in log and 'Hidden failed' not in log
assert 'PASS actual thirteen hidden modes:' in log
assert 'PASS static production particles:' in log, 'Require the complete runtime suite'
backgrounds = [3,0,0,0,4,1,4,0,0,0,0,3,0]
waves = [20,40,40,40,30,20,20,40,40,30,0,30,40]
cases = []
for match in re.finditer(r'^Hidden primary mode=(\d+) Hz=(\d+) background=(\d+) waves=(\d+) checked=1$', log, re.M):
    mode, hz, background, count = map(int, match.groups())
    assert background == backgrounds[mode-21] and count == waves[mode-21]
    cases.append(dict(mode=mode, schedule=hz, background=background, waves=count))
assert len(cases) == 52
assert {(c['mode'], c['schedule']) for c in cases} == {
    (mode, hz) for mode in range(21, 34) for hz in (100, 30, 240, 0)}
paths = ['Tests/native/ImageRuntimeTests.cpp', 'pvzPerf/Lawn/Board.cpp',
         'pvzPerf/Lawn/Challenge.cpp', 'pvzPerf/Lawn/Plant.cpp',
         'pvzPerf/Lawn/Zombie.cpp', 'pvzPerf/Lawn/Projectile.cpp']
report = dict(
    scope='Thirteen hidden modes: the README lists the actual per-mode actions and boundaries checked at four schedules',
    limits='Selected actions; shortened waits and constructed boundaries; no claim that hidden prototypes are fully playable or every wave was solved',
    cases=cases,
    sources={path: hashlib.sha256((root/path).read_bytes()).hexdigest() for path in paths},
    executable_sha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
    pak_sha256=hashlib.sha256(args.pak.read_bytes()).hexdigest(),
    git_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    working_tree_modified=True,
)
shutil.copyfile(args.log, destination/'runtime.txt')
(destination/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('Archived 52 hidden-mode rule/action/pause cases')
