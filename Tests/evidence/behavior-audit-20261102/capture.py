"""Cross-check the bounded native behavior audit across every current enum slot."""
import hashlib
import json
from pathlib import Path
import re

destination = Path(__file__).resolve().parent
root = destination.parents[2]
evidence = destination.parent
names = [
    'plant-behavior-audit-20261101', 'zombie-behavior-audit-20261102',
    'minigame-rules-audit-20261102', 'puzzle-rules-audit-20261102',
    'survival-rules-audit-20261102', 'adventure-rules-audit-20261102',
    'hidden-rules-audit-20261102', 'garden-intro-rules-audit-20261102',
    'ui-routes-audit-20261102',
]
reports = {name: json.loads((evidence/name/'report.json').read_text(encoding='utf-8')) for name in names}
sources = {}
artifacts = set()
for name, report in reports.items():
    for path, digest in report.get('sources', report.get('source_sha256', {})).items():
        actual = hashlib.sha256((root/path).read_bytes()).hexdigest()
        assert digest == actual, f'{name} has a stale source hash: {path}'
        assert path not in sources or sources[path] == digest
        sources[path] = digest
    artifacts.add((report.get('executable_sha256', report.get('runtime_executable_sha256')), report['pak_sha256']))
assert len(artifacts) == 1, 'All reports must refer to the same actual native executable and PAK'

source = (root/'pvzPerf/ConstEnums.h').read_text(encoding='utf-8-sig')
mode_names = re.findall(r'^\s*(GAMEMODE_[A-Z0-9_]+)\s*,', source.split('enum GameMode',1)[1].split('NUM_GAME_MODES',1)[0], re.M)
assert len(mode_names) == 73 and mode_names[-1] == 'GAMEMODE_CUSTOM_LEVEL'
plants = reports[names[0]]['cases']
zombies = reports[names[1]]['cases']
assert {(c['seed'],c['schedule']) for c in plants} == {(i,hz) for i in range(53) for hz in (100,30,240,0)}
assert {(c['type'],c['schedule']) for c in zombies} == {(i,hz) for i in range(34) for hz in (100,30,240,0)}

routes = {}
for family in names[2:5]+[names[6]]:
    for case in reports[family]['cases']:
        routes.setdefault(int(case['mode']),set()).add(int(case['schedule']))
assert len(reports[names[5]]['cases']) == 400
assert {(c['level'],c['replay'],c['schedule']) for c in reports[names[5]]['cases']} == {
    (level,replay,hz) for level in range(1,51) for replay in (False,True) for hz in (100,30,240,0)}
routes[0] = {c['schedule'] for c in reports[names[5]]['cases']}
for mode, section in ((34,'garden'),(35,'intro'),(71,'tree')):
    routes[mode] = {c['schedule'] for c in reports[names[7]][section]}
assert set(routes) == set(range(72)), 'A supported enum slot is missing or a reserved slot was counted'
assert all(schedules == {100,30,240,0} for schedules in routes.values())
ui = reports[names[8]]
assert len(ui['openings']) == 720 and len(ui['boss_damage_death']) == len(ui['garden_sales']) == 8

report = dict(
    scope='Every real plant, zombie and supported GameMode (including Intro and gardens) has a bounded principal-workflow check; additional application opening, UI and lifecycle routes are checked separately',
    seed_types=53, zombie_types=34,
    supported_modes=[dict(id=mode,name=mode_names[mode],schedules=sorted(schedules)) for mode,schedules in sorted(routes.items())],
    reserved_modes=[dict(id=72,name=mode_names[72],reason='No playable retail level; retained enum slot')],
    runtime_executable_sha256=next(iter(artifacts))[0],pak_sha256=next(iter(artifacts))[1],
    reports={name:hashlib.sha256((evidence/name/'report.json').read_bytes()).hexdigest() for name in names},
    sources=sources,
    limits=[
        'This is not exhaustive verification of every interaction, random outcome, save interruption, terrain or state combination.',
        'High-health targets, supplied profiles and constructed completion boundaries are described in each report; they are not full player playthroughs.',
        'The reported Blover coin-location incident remains unreproduced in the six-row native drop test and is not labelled fixed.',
        'These Windows production-object checks do not constitute Android or other-platform device acceptance or a performance result.',
    ],
)
(destination/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('Verified one artifact pair, current source hashes, 53 plants, 34 zombies and all 72 supported enum slots')
