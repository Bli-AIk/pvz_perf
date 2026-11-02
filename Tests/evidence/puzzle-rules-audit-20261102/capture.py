"""Archive real puzzle operations; later-round setup is labelled separately."""
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
assert 'FAIL ' not in log and 'Puzzle failed' not in log
assert 'PASS actual twenty puzzle rules:' in log
assert 'PASS static production particles:' in log, 'Require the complete runtime suite'

def rows(pattern, fields):
    return [dict(zip(fields, values)) for values in re.findall(pattern, log, re.M)]

cases = rows(r'^Puzzle primary mode=(\d+) Hz=(\d+) stage=(\d+) award=(\d+) result=(\d+)$',
             ('mode', 'schedule', 'stage', 'award', 'result'))
assert len(cases) == 80
assert {(int(c['mode']), int(c['schedule'])) for c in cases} == {
    (mode, hz) for mode in range(36, 56) for hz in (100, 30, 240, 0)}
defenders = rows(r'^Puzzle defender mode=(\d+) Hz=(\d+) kind=(shoot|mine) observed=1$',
                 ('mode', 'schedule', 'kind'))
bites = rows(r'^Puzzle bite mode=(\d+) Hz=(\d+) type=(\d+) health=1 score=0$',
             ('mode', 'schedule', 'zombie_type'))
brains = rows(r'^Puzzle brains mode=(\d+) Hz=(\d+) complete=5 nextStage=(\d+)$',
              ('mode', 'schedule', 'stage'))
pots = rows(r'^Puzzle pots mode=(\d+) Hz=(\d+) opened=(\d+) nextStage=(\d+)$',
            ('mode', 'schedule', 'opened', 'stage'))
for group, start in ((defenders, 36), (bites, 36), (brains, 36), (pots, 46)):
    assert len(group) == 40
    assert {(int(c['mode']), int(c['schedule'])) for c in group} == {
        (mode, hz) for mode in range(start, start+10) for hz in (100, 30, 240, 0)}
endless = rows(r'^Puzzle endless mode=(\d+) stage=(\d+) reward=(\d+) giants=(\d+) puffs=(\d+) sunflowers=(\d+)$',
               ('mode', 'stage', 'reward', 'giants', 'puffs', 'sunflowers'))
assert len(endless) == 18
assert {(int(c['mode']), int(c['stage'])) for c in endless} == {
    (mode, stage) for mode in (45, 55) for stage in (0, 1, 2, 8, 9, 10, 19, 79, 80)}
for case in endless:
    mode, stage = int(case['mode']), int(case['stage'])
    assert int(case['reward']) == ((stage+1) % (3 if mode == 45 else 10) == 0)
    if mode == 55:
        assert int(case['giants']) == 1+min(stage//10, 8)

paths = ['Tests/native/ImageRuntimeTests.cpp', 'pvzPerf/Lawn/Challenge.cpp',
         'pvzPerf/Lawn/Plant.cpp', 'pvzPerf/Lawn/Zombie.cpp', 'pvzPerf/Lawn/Board.cpp']
report = dict(
    scope='Twenty puzzles: actual listed inputs, attacks, bites, payloads, terminal states and first endless repopulation',
    limits='Controlled combat targets; vases opened non-zombies first; later stages constructed explicitly, not played through',
    cases=cases, defenders=defenders, brain_bites=bites, complete_brains=brains,
    opened_pots=pots, constructed_endless_boundaries=endless,
    sources={path: hashlib.sha256((root/path).read_bytes()).hexdigest() for path in paths},
    executable_sha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
    pak_sha256=hashlib.sha256(args.pak.read_bytes()).hexdigest(),
    git_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    working_tree_modified=True,
)
shutil.copyfile(args.log, destination/'runtime.txt')
(destination/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('Archived 80 puzzles, 40 defender attacks, 40 brain sequences, 40 pot sequences and 18 constructed endless boundaries')
