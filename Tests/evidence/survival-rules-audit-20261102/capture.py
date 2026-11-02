"""Capture actual survival repick lifetimes, with explicit combat shortcuts."""
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
assert 'FAIL ' not in log and 'Survival failed' not in log
assert 'PASS actual fifteen survival rules:' in log
assert 'PASS static production particles:' in log, 'Require the complete runtime suite'
cases = []
for match in re.finditer(r'^Survival primary mode=(\d+) Hz=(\d+) background=(\d+) waves=(\d+) retained=1 repick=1 final=(\d+)$', log, re.M):
    mode, hz, background, waves, final = map(int, match.groups())
    assert background == (mode-56) % 5
    assert waves == (10 if mode >= 66 else 20)
    assert final == (mode >= 61)
    cases.append(dict(mode=mode, schedule=hz, background=background, waves=waves,
                      retained_actors=True, repick=True, finite=bool(final)))
assert len(cases) == 60
assert {(c['mode'], c['schedule']) for c in cases} == {
    (mode, hz) for mode in range(56, 71) for hz in (100, 30, 240, 0)}
paths = ['Tests/native/ImageRuntimeTests.cpp', 'pvzPerf/Lawn/Board.cpp',
         'pvzPerf/Lawn/Challenge.cpp', 'pvzPerf/Lawn/Zombie.cpp',
         'pvzPerf/Lawn/CutScene.cpp', 'pvzPerf/Lawn/Widget/SeedChooserScreen.cpp',
         'pvzPerf/LawnApp.cpp']
report = dict(
    scope='Fifteen survival modes: first/final wave, five-second end wait, real CheckForGameEnd and chooser, cooldown/actor retention, resume and pause',
    limits='Intermediate combat skipped; finite stage four and endless stage five explicitly constructed; no full five-stage playthrough claim',
    cases=cases,
    sources={path: hashlib.sha256((root/path).read_bytes()).hexdigest() for path in paths},
    executable_sha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
    pak_sha256=hashlib.sha256(args.pak.read_bytes()).hexdigest(),
    git_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    working_tree_modified=True,
)
shutil.copyfile(args.log, destination/'runtime.txt')
(destination/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('Archived 60 survival rules/repick/cooldown/resume cases')
