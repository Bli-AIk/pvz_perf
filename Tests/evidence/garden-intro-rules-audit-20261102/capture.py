"""Archive actual Garden/Tree tools and the Intro application route."""
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
assert 'FAIL ' not in log and 'Garden failed' not in log and 'Intro failed' not in log
assert 'PASS actual garden/tree rules:' in log
assert 'PASS actual intro:' in log
assert 'PASS static production particles:' in log, 'Require the complete runtime suite'
garden = [dict(schedule=int(hz)) for hz in re.findall(
    r'^Garden primary Hz=(\d+) growth=3 tools=7 gardens=3 wheelbarrow=1 reload=1$', log, re.M)]
assert len(garden) == 4 and {c['schedule'] for c in garden} == {100, 30, 240, 0}
tree = [dict(height=int(height), schedule=int(hz), grown=int(grown))
        for height, hz, grown in re.findall(
            r'^Tree primary height=(\d+) Hz=(\d+) grown=(\d+) charged=1$', log, re.M)]
assert len(tree) == 20 and {(c['height'], c['schedule']) for c in tree} == {
    (height, hz) for height in (0, 1, 12, 50, 99) for hz in (100, 30, 240, 0)}
assert all(c['grown'] == c['height']+1 for c in tree)
intro = [dict(schedule=int(hz), elapsed_ns=int(elapsed)) for hz, elapsed in re.findall(
    r'^Intro primary Hz=(\d+) warmupNs=1000000000 adventure=1 audio=1 elapsedNs=(\d+)$', log, re.M)]
assert len(intro) == 4 and {c['schedule'] for c in intro} == {100, 30, 240, 0}
assert all(13_890_000_000 <= c['elapsed_ns'] < 13_930_000_000 for c in intro)
paths = ['Tests/native/ImageRuntimeTests.cpp', 'tools/test-image-runtime.ps1',
         'pvzPerf/Lawn/CutScene.cpp', 'pvzPerf/Lawn/Cutscene.h',
         'pvzPerf/Lawn/ZenGarden.cpp', 'pvzPerf/Lawn/Challenge.cpp',
         'pvzPerf/Lawn/Board.cpp', 'pvzPerf/LawnApp.cpp']
report = dict(
    scope='Four real Garden tool/growth/transfer/profile scenarios, twenty Tree feeding scenarios and four Intro application handoffs',
    limits='Controlled plant needs and purchased tools; no sale modal, all calendar boundaries or complete garden economy; silent audio request counts are not listening tests',
    garden=garden, tree=tree, intro=intro,
    sources={path: hashlib.sha256((root/path).read_bytes()).hexdigest() for path in paths},
    executable_sha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
    pak_sha256=hashlib.sha256(args.pak.read_bytes()).hexdigest(),
    git_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    working_tree_modified=True,
)
shutil.copyfile(args.log, destination/'runtime.txt')
(destination/'report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print('Archived 4 Garden, 20 Tree and 4 Intro scenarios')
