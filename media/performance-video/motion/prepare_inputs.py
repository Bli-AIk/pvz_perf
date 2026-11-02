"""Package original 1073 reanim XML/PNG and measured samples for an offline webpage.

No animation rasterization, resampling, smoothing, or new performance measurement.
All source hashes and selection rules are retained in the generated bundle.
"""
from pathlib import Path
import argparse
import base64
import csv
import hashlib
import json
import math
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'out/media/performance-v3.2.4-motion'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(original):
    evidence = {}

    def record(path):
        try:
            name = str(path.relative_to(ROOT)).replace('\\', '/')
        except ValueError:
            name = str(path).replace('\\', '/')
        evidence[name] = {'sha256': sha(path), 'bytes': path.stat().st_size}

    def load_json(path):
        record(path)
        return json.loads(path.read_text(encoding='utf-8-sig'))

    def load_csv(path):
        record(path)
        return list(csv.DictReader(path.read_text(encoding='utf-8-sig').splitlines()))

    reanim_dir = original / 'reanim'
    images = {p.stem.upper(): p for p in reanim_dir.glob('*.png')}
    xml, symbols = {}, set()
    for name in ['PeaShooter', 'Zombie', 'SunFlower']:
        path = reanim_dir / (name + '.reanim')
        record(path)
        xml[name] = path.read_text(encoding='utf-8-sig')
        tree = ET.fromstring('<reanim>' + xml[name] + '</reanim>')
        symbols.update(n.text for n in tree.iter('i') if n.text)
    pngs = {}
    for symbol in sorted(symbols):
        name = symbol.removeprefix('IMAGE_REANIM_')
        if name not in images:
            raise ValueError('Missing original image: ' + symbol)
        path = images[name]
        record(path)
        pngs[symbol] = 'data:image/png;base64,' + base64.b64encode(path.read_bytes()).decode('ascii')

    desktop = {}
    for key, folder, label in [
        ('project', 'windows-vulkan-lp14-240-20261101', 'pvz_perf · 240Hz 上限'),
        ('original', 'original-1073-11x-full-20261029', '原版 1073'),
    ]:
        path = ROOT / 'Tests/evidence' / folder / 'samples.csv'
        all_rows = load_csv(path)
        valid = [r for r in all_rows if r['segment'] == 'game' and r['valid'] == '1'
                 and r['scene'] == '3' and r['foreground'] == '1']
        origin = float(valid[0]['elapsed_s']) - float(valid[0]['interval_s'])
        points = [[float(r['elapsed_s'])-origin, float(r['draw_fps']), float(r['cpu_percent']),
                   float(r['interval_s']), int(r['wave'])] for r in valid]
        desktop[key] = {'label': label, 'columns': ['seconds', 'fps', 'cpuPercent', 'intervalSeconds', 'wave'],
                        'points': points, 'duration': points[-1][0],
                        'selection': 'segment=game, valid=1, scene=3, foreground=1',
                        'originElapsedSeconds': origin, 'clock': '采样时钟，保留排除区间的时间缺口',
                        'cpuFrequency': None, 'cpuFrequencyNote': '11x 是用户设置；本轮未采 CPU 频率'}
    folder = ROOT / 'Tests/evidence/ultimate-vs-perf-lp14-20261101'
    rows = load_csv(folder / 'ultimate-samples.csv')
    end = load_json(folder / 'ultimate-measurement.json')['duration_s'] - 3
    valid = [r for r in rows if r['phase'] == 'capture' and r['foreground'] == '1'
             and float(r['elapsed_s'])-float(r['duration_s']) >= 3 and float(r['elapsed_s']) <= end]
    points = [[float(r['elapsed_s'])-3, float(r['rtss_fps']), float(r['cpu_percent']),
               float(r['duration_s']), None] for r in valid]
    desktop['ultimate'] = {'label': '精华版 · 100Hz 逻辑 / 解绘制帧率', 'points': points,
                           'duration': points[-1][0], 'columns': desktop['project']['columns'],
                           'selection': f'foreground capture, full interval inside [3,{end}]',
                           'clock': 'CPU 采样时刻；FPS 为当时最近的 RTSS 呈现窗口，未重采样',
                           'cpuFrequency': None, 'cpuFrequencyNote': '未采 CPU 频率'}
    phone = {}
    for key, folder, label in [
        ('project', 'pvz-perf-v3.2.4-phone-battery-20261101-030609', 'pvz_perf 3.2.4'),
        ('ultimate', 'ultimate-phone-battery-20261101-024830', '精华版 2.4.3'),
    ]:
        source = ROOT.parent / 'GpuBenchmarks' / folder
        meta = load_json(source / 'capture.json')
        rows = load_json(source / 'samples.json')
        summary = load_json(source / 'summary.json')
        valid = []
        for a, b in zip(rows, rows[1:]):
            dt = b['uptime'] - a['uptime']
            if not 0 < dt < 5 or a['uptime'] < meta['start_uptime']+5:
                continue
            if any(r['battery'].get('status') != 'Discharging' for r in (a, b)):
                continue
            watts = abs(float(b['battery']['current_now'])*float(b['battery']['voltage_now']))/1e12
            freqs = [b['frequencies_khz'].get('policy'+str(i)) for i in [0, 2, 5, 7]]
            if any(v is None for v in freqs):
                raise ValueError('Incomplete CPU frequency sample')
            valid.append([a['uptime'], b['uptime'], watts, *[v/1e6 for v in freqs]])
        duration = sum(b-a for a, b, *_ in valid)
        mean = sum((b-a)*w for a, b, w, *_ in valid)/duration
        assert math.isclose(duration, summary['discharge_seconds'], abs_tol=1e-5)
        assert math.isclose(mean, summary['mean_device_w'], abs_tol=1e-8)
        origin = valid[0][0]
        phone[key] = {'label': label, 'points': [[a-origin, b-origin, *v] for a,b,*v in valid],
                      'columns': ['startSeconds', 'endSeconds', 'wholeDeviceWatts',
                                  'policy0GHz', 'policy2GHz', 'policy5GHz', 'policy7GHz'],
                      'duration': valid[-1][1]-origin, 'selectedSeconds': duration,
                      'summary': summary, 'fps': None, 'cpuPercent': None,
                      'selection': '原采集脚本口径：开始后5秒，双端Discharging，0<dt<5秒',
                      'note': '瞬时 CPU policy 频率；不是进程跑核记录。未采 FPS / CPU 占用。整机功耗包含屏幕、风扇和系统。'}
    derived = load_json(ROOT / 'Tests/evidence/v324-microbenchmarks-20261101/derived-summary.json')
    selection = next(row for row in derived['table_models'] if row['table'] == 'kFireProjectiles')
    selection_table = {
        'columns': ['子弹类型选择', '原 switch 模型', '二维数据表模型'],
        'rows': [[f'独立进程 {i+1}（ns/次）', f'{old:.2f}', f'{new:.2f}']
                 for i,(old,new) in enumerate(selection['processes'])],
        'note': 'kFireProjectiles · MSVC /O2 · LP14 · 用户11x · min-of-5。\n17 种合法射手、两种武器共用同一输入与校验桩；耗时降低约 84%，只衡量子弹类型选择，不是完整 Fire 或原版 EXE。',
    }
    root_folder = ROOT / 'Tests/evidence/root-motion-optimization-20261029/after'
    root_rows = load_csv(root_folder / 'run1/accuracy.csv')
    root_repeat = load_csv(root_folder / 'run2/accuracy.csv')
    assert root_rows == root_repeat, 'Root-motion accuracy repeats differ'
    record(ROOT / 'Tests/performance/zombie_root_motion_benchmark.cpp')
    record(root_folder.parent / 'README.md')
    normal = [row for row in root_rows if row['definition'] == 'normal']
    assert [int(row['fps']) for row in normal] == [100, 120, 200, 240, 1000]
    assert all(int(row['duration_ns']) == 60_000_000_000 for row in normal)
    assert len({row['current_x'] for row in normal}) == 1
    root_motion = {
        'durationSeconds': 60, 'startX': 900, 'animationRate': 12,
        'definition': 'normal',
        'scope': '原速度累加算术的多步长重建实验，非原版EXE多帧率实测；固定普通僵尸_ground轨道。',
        'samples': [{'hz': int(row['fps']), 'legacyX': float(row['legacy_x']),
                     'phaseX': float(row['current_x']),
                     'referenceX': float(row['reference_x'])} for row in normal],
    }
    return {'selectionBenchmark': selection_table, 'rootMotion': root_motion,
            'reanim': {'xml': xml, 'images': pngs, 'source': 'Original 1073 unpacked .reanim and PNG'},
            'traces': {'desktop': desktop, 'phone': phone}, 'sources': evidence}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--original-pak', type=Path,
                        default=Path('C:/UserProgram/Plants_Vs_Zombies_V1.2.0.1073_EN/pak'))
    args = parser.parse_args()
    result = build(args.original_pak)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'web-inputs.json').write_text(json.dumps(result, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    (OUT / 'web-input-provenance.json').write_text(json.dumps({
        'sources': result['sources'],
        'desktop': {k: {n:v for n,v in d.items() if n!='points'} for k,d in result['traces']['desktop'].items()},
        'phone': {k: {n:v for n,v in d.items() if n!='points'} for k,d in result['traces']['phone'].items()},
    }, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'originalDefinitions': len(result['reanim']['xml']), 'pngs': len(result['reanim']['images']),
                      'desktopSamples': {k:len(v['points']) for k,v in result['traces']['desktop'].items()},
                      'phoneIntervals': {k:len(v['points']) for k,v in result['traces']['phone'].items()},
                      'bundleBytes': (OUT/'web-inputs.json').stat().st_size}))
