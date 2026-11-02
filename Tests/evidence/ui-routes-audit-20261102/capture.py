"""Archive actual application UI, birth, opening and lifecycle routes."""
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
assert 'FAIL ' not in log and 'Almanac failed' not in log and 'Award failed' not in log
assert 'PASS actual Almanac:' in log and 'PASS actual Adventure award routes:' in log
assert 'PASS static production particles:' in log, 'Require the complete runtime suite'
pages = []
for kind, level, width, hz, identifier in re.findall(
        r'^Almanac (plant|zombie) level=(\d+) width=(\d+) Hz=(\d+) (?:seed|type)=(\d+) visible=1$', log, re.M):
    pages.append(dict(kind=kind, in_level=bool(int(level)), width=int(width), schedule=int(hz), identifier=int(identifier)))
assert len(pages) == 1200
assert {(c['kind'], c['in_level'], c['width'], c['schedule'], c['identifier']) for c in pages} == {
    (kind, in_level, width, hz, identifier)
    for kind, count in [('plant',49),('zombie',26)] for identifier in range(count)
    for in_level in (False,True) for width in (800,1400) for hz in (100,30,240,0)}
almanac = [dict(in_level=bool(int(level)), width=int(width), schedule=int(hz))
           for level, width, hz in re.findall(
               r'^Almanac primary level=(\d+) width=(\d+) Hz=(\d+) plants=49 zombies=26 closed=1$', log, re.M)]
assert len(almanac) == 16
assert {(c['in_level'],c['width'],c['schedule']) for c in almanac} == {
    (in_level,width,hz) for in_level in (False,True) for width in (800,1400) for hz in (100,30,240,0)}
awards = []
for values in re.findall(
        r'^Award primary level=(\d+) replay=(\d+) Hz=(\d+) next=(\d+) scene=(\d+) almanac=(\d+) store=(-?\d+) clicks=(\d+)$', log, re.M):
    level, replay, hz, next_level, scene, opened_almanac, store, clicks = map(int, values)
    assert next_level == (1 if level == 50 else level+1)
    assert scene == (6 if level == 50 else 2)
    assert opened_almanac == (not replay and level == 14)
    assert store == ({24:301,34:601,41:3100}.get(level,-1) if not replay else -1)
    assert store == -1 or clicks > 0
    awards.append(dict(level=level,replay=bool(replay),schedule=hz,next_level=next_level,
                       scene=scene,almanac=opened_almanac,store=store,store_clicks=clicks))
assert len(awards) == 400
assert {(c['level'],c['replay'],c['schedule']) for c in awards} == {
    (level,replay,hz) for level in range(1,51) for replay in (False,True) for hz in (100,30,240,0)}
births = [dict(group=int(group),identifier=int(identifier),scene=int(scene),birth_ns=int(born),animations=int(count))
          for group,identifier,scene,born,count in re.findall(
              r'^Actor birth group=(\d+) type=(\d+) scene=(\d+) born=(\d+) animations=(\d+) ok=1$',log,re.M)]
assert len(births) == 620 and {(c['group'],c['identifier'],c['scene'],c['birth_ns']) for c in births} == {
    (group,identifier,scene,born) for group in range(4) for identifier in range(53 if group==0 else 34)
    for scene in (2,3) for born in (0,31_234_567_890)}
losses = []
loss_entries = [(0,level,True) for level in (1,11,21,31,41,50)]+[
    (9,50,True),(66,50,True),(58,50,True),
    (20,50,False),(7,50,False),(36,50,False),(45,50,False),(55,50,False)]
for mode,level,width,hz,retry,cutscene,elapsed in re.findall(
        r'^Loss primary mode=(\d+) level=(\d+) width=(\d+) Hz=(\d+) retry=(\d+) cutscene=(\d+) movieNs=(\d+) ok=1$',log,re.M):
    mode,level,width,hz,retry,cutscene,elapsed=map(int,(mode,level,width,hz,retry,cutscene,elapsed))
    assert (mode,level,bool(cutscene)) in loss_entries
    assert 11_000_000_000 <= elapsed < 11_040_000_000 if cutscene else elapsed == 0
    losses.append(dict(mode=mode,level=level,width=width,schedule=hz,retry=bool(retry),cutscene=bool(cutscene),movie_ns=elapsed))
assert len(losses)==224 and {(c['mode'],c['level'],c['width'],c['schedule'],c['retry']) for c in losses} == {
    (mode,level,width,hz,retry) for mode,level,_ in loss_entries
    for width in (800,1400) for hz in (100,30,240,0) for retry in (False,True)}
openings = [dict((key,int(value)) for key,value in re.findall(r'(\w+)=(-?\d+)',line))
            for line in re.findall(r'^Opening primary (.+)$',log,re.M)]
opening_expected = {(0,level,replay,0,-1,hz)
                    for level in range(1,51) for replay in (0,1) for hz in (100,30,240,0)}
opening_expected |= {(mode,50,1,0,-1,hz) for mode in range(1,72) if mode != 35 for hz in (100,30,240,0)}
opening_expected |= {(0,level,0,stock,answer,hz) for level,stock in ((13,0),(16,0),(16,1))
                     for answer in (0,1,2) for hz in (100,30,240,0)}
opening_expected |= {(0,13,0,1,0,hz) for hz in (100,30,240,0)}
assert len(openings)==720 and {
    tuple(c[key] for key in ('mode','level','replay','stock','answer','Hz')) for c in openings} == opening_expected
for case in openings:
    assert case['ok']==1 and 0<case['elapsedNs']<60_000_000_000
    assert case['dave']==-1 or case['dialogue']>0
    if case['mode']==0 and case['level']==5 and not case['replay']:
        assert case['shovels']==3
    if case['answer']>=0:
        assert case['offers']==int(not(case['level']==13 and case['stock']==1))
boss = [dict((key,int(value)) for key,value in re.findall(r'(\w+)=(-?\d+)',line))
        for line in re.findall(r'^Lifecycle boss (.+)$',log,re.M)]
assert len(boss)==8 and {(c['mode'],c['Hz']) for c in boss} == {(mode,hz) for mode in (0,5) for hz in (100,30,240,0)}
assert all(c['stage']==3 and c['bungee']==c['rv']==c['dying']==c['awards']==c['ok']==1 for c in boss)
sales = [dict((key,int(value)) for key,value in re.findall(r'(\w+)=(-?\d+)',line))
         for line in re.findall(r'^Lifecycle sale (.+)$',log,re.M)]
assert len(sales)==8 and {(c['Hz'],c['accept']) for c in sales} == {(hz,accept) for hz in (100,30,240,0) for accept in (0,1)}
assert all(c['ok']==1 and c['coins']==1000+c['accept']*c['price'] and
           c['records']==c['live']==c['pots']==3-c['accept'] for c in sales)
assert 'PASS actual opening flows:' in log and 'PASS actual late lifecycle:' in log
paths = ['Tests/native/ImageRuntimeTests.cpp','tools/test-image-runtime.ps1',
         'Tests/native/OpeningFlowRuntimeChecks.h','Tests/native/OpeningFlowRuntimeChecks.cpp',
         'pvzPerf/LawnApp.cpp','pvzPerf/Lawn/Widget/Almanac.cpp',
         'pvzPerf/Lawn/Widget/AwardScreen.cpp','pvzPerf/Lawn/Widget/StoreScreen.cpp',
         'pvzPerf/Lawn/Widget/CreditScreen.cpp','pvzPerf/Lawn/ZenGarden.cpp',
         'pvzPerf/Lawn/Board.cpp','pvzPerf/Lawn/CutScene.cpp','pvzPerf/Sexy.TodLib/Reanimator.cpp']
report = dict(
    scope='All 75 selectable Almanac previews in 16 contexts; 400 awards, 620 births, 224 loss routes, 720 openings and 16 late lifecycle routes',
    limits='Controlled unlocked profiles and supplied caches; preview contribution is a production self-comparison, not a retail pixel oracle; level-complete boundary is constructed and achievements already shown',
    almanac_contexts=almanac,pages=pages,adventure_awards=awards,actor_births=births,loss_routes=losses,
    openings=openings,boss_damage_death=boss,garden_sales=sales,
    sources={path:hashlib.sha256((root/path).read_bytes()).hexdigest() for path in paths},
    executable_sha256=hashlib.sha256(args.executable.read_bytes()).hexdigest(),
    pak_sha256=hashlib.sha256(args.pak.read_bytes()).hexdigest(),
    git_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
    working_tree_modified=True,
)
shutil.copyfile(args.log,destination/'runtime.txt')
(destination/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Archived 1200 Almanac pages / 16 contexts, 400 awards, 620 births, 224 losses, 720 openings and 16 late lifecycles')
