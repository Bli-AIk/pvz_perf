"""Archive all 50 first/replay adventure routes, without claiming full solves."""
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
assert 'FAIL ' not in log and 'Adventure failed' not in log
assert 'PASS actual fifty adventure levels:' in log
assert 'PASS static production particles:' in log, 'Require the complete runtime suite'
base_waves = [4,6,8,10,8,10,20,10,20,20,10,20,10,20,10,10,20,10,20,20,
              10,20,20,30,20,20,30,20,30,30,10,20,10,20,20,10,20,10,20,20,
              10,20,20,30,20,20,30,20,30,30]
cases = []
for match in re.finditer(r'^Adventure primary level=(\d+) replay=(\d+) Hz=(\d+) background=(\d+) waves=(\d+) rows=(\d+) conveyor=(\d+) award=(\d+)$', log, re.M):
    level, replay, hz, background, waves, rows, conveyor, award = map(int, match.groups())
    assert background == (1 if level == 35 else 5 if level == 50 else (level-1)//10)
    expected_waves = base_waves[level-1]
    if replay and level not in (10, 20, 30):
        expected_waves = 20 if expected_waves < 10 else expected_waves+10
    assert waves == (8 if level == 15 else expected_waves)
    assert conveyor == (level in (5, 10, 20, 25, 30, 40, 45, 50))
    assert rows == (1 if not replay and level == 1 else 3 if not replay and level in (2, 3)
                    else 6 if background in (2, 3) else 5)
    expected_award = 15 if level % 10 == 9 else 18 if replay else {
        4:9, 14:10, 24:11, 34:14, 44:13, 50:21}.get(level, 7)
    assert award == expected_award
    cases.append(dict(level=level, replay=bool(replay), schedule=hz, background=background,
                      waves=waves, rows=rows, conveyor=bool(conveyor), award=award))
assert len(cases) == 400
assert {(c['level'], c['replay'], c['schedule']) for c in cases} == {
    (level, replay, hz) for level in range(1, 51) for replay in (False, True) for hz in (100, 30, 240, 0)}
tutorials = [dict(level=int(level), schedule=int(hz), planted=int(count)) for level, hz, count in
             re.findall(r'^Adventure tutorial level=(\d+) Hz=(\d+) planted=(\d+) completed=1$', log, re.M)]
assert len(tutorials) == 8
assert {(c['level'], c['schedule'], c['planted']) for c in tutorials} == {
    (level, hz, 2 if level == 1 else 3) for level in (1, 2) for hz in (100, 30, 240, 0)}
paths = ['Tests/native/ImageRuntimeTests.cpp', 'pvzPerf/Lawn/Board.cpp',
         'pvzPerf/Lawn/Challenge.cpp', 'pvzPerf/Lawn/Zombie.cpp', 'pvzPerf/Lawn/Coin.cpp',
         'pvzPerf/Lawn/SeedPacket.cpp', 'pvzPerf/LawnApp.cpp']
report = dict(
    scope='Fifty adventure levels, first/replay: listed init/clock/rules, teaching actions, first wave, pause and real death-driven rewards',
    limits='Legal banks supplied directly; teaching funds increased; conveyor delay shortened; intermediate combat skipped; Vase stages constructed; no full campaign/award-screen claim',
    cases=cases, initial_teaching=tutorials,
    sources={path: hashlib.sha256((root/path).read_bytes()).hexdigest() for path in paths},
    executable_sha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
    pak_sha256=hashlib.sha256(args.pak.read_bytes()).hexdigest(),
    git_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    working_tree_modified=True,
)
shutil.copyfile(args.log, destination/'runtime.txt')
(destination/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('Archived 400 adventure routes and eight real initial teaching chains')
