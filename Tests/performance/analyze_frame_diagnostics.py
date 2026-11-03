"""Align one-second frame diagnostics with foreground whole-fight samples."""
import argparse
import csv
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

from compare_endless_runs import load, stats


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--previous', type=Path, required=True)
    args = parser.parse_args()
    current = load(args.capture)
    metadata = json.loads((args.capture / 'measurement.json').read_text(encoding='utf-8-sig'))
    origin = metadata['sample_clock_origin_ns']
    intervals = [(origin + (r['elapsed_s'] - r['interval_s']) * 1e9,
                  origin + r['elapsed_s'] * 1e9) for r in current]
    with (args.capture / 'frame-phases.csv').open(encoding='utf-8-sig', newline='') as stream:
        phases = [{k: int(v) for k, v in row.items()} for row in csv.DictReader(stream)]
    selected = []
    for row in phases:
        if row['scene'] != 3 or row['paused']:
            continue
        end = row['qpc_ns']
        begin = end - row['interval_ns']
        overlap = sum(max(0, min(end, hi) - max(begin, lo)) for lo, hi in intervals)
        # Discard boundary rows instead of allocating aggregate costs to a
        # partly paused/background second as though they were uniform.
        if overlap >= row['interval_ns'] * .999:
            selected.append(row)
    def total(key):
        return sum(r[key] for r in selected)
    stages = ['update', 'draw', 'build_draw', 'submit', 'present', 'acquire', 'fence_poll', 'fence_wait']
    summary = {
        'whole_fight': stats(current),
        'phase_selection': 'scene=3, paused=0, >=99.9% overlap with valid foreground game intervals',
        'phase_rows': len(selected), 'phase_seconds': total('interval_ns') / 1e9,
        'cpu_wall_ms_per_call': {s: total(s + '_ns') / total(s + '_calls') / 1e6
                                 if total(s + '_calls') else None for s in stages},
        'cpu_calls': {s: total(s + '_calls') for s in stages},
        'gpu_scene_ms': total('gpu_scene_ns') / total('gpu_samples') / 1e6,
        'gpu_output_ms': total('gpu_output_ns') / total('gpu_samples') / 1e6,
        'gpu_samples': total('gpu_samples'),
        'counters': {key: total(key) for key in ['submitted', 'fence_skipped', 'acquire_skipped']},
        'graphics_states': [dict(zip(['hdr', 'scene_width', 'scene_height', 'output_width', 'output_height', 'scale', 'present_mode'], state))
                            for state in sorted({tuple(r[key] for key in ['hdr', 'scene_width', 'scene_height', 'output_width', 'output_height', 'scale', 'present_mode'])
                                                 for r in selected})],
        'phase_note': 'CPU stages are inclusive wall times, not mutually exclusive CPU accounting. GPU intervals can include device bubbles. Results arrive on slot reuse.',
    }
    runs = [(args.baseline, 'v3.2.4', '#287abd'),
            (args.previous, '3.3 beta 上轮（未记录实际HDR）', '#da7041'),
            (args.capture, '当前 SDR 插桩版', '#20945a')]
    summary['comparison'] = {label: stats(load(path)) for path, label, _ in runs}
    plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'DejaVu Sans']
    plt.rcParams['axes.unicode_minus'] = False
    fig, axes = plt.subplots(4, 1, figsize=(14, 13), sharex=True)
    for path, label, color in runs:
        rows = load(path)
        x = [(r['board_counter'] - 2291960000000) / 1e9 for r in rows]
        axes[0].plot(x, [r['draw_fps'] for r in rows], label=label, color=color, lw=1.1, alpha=.85)
        axes[1].plot(x, [r['cpu_percent'] for r in rows], label=label, color=color, lw=1, alpha=.85)
    x = [(r['board_ns'] - 2291960000000) / 1e9 for r in selected]
    for key, label, color in [('update', '逻辑更新', '#386ba1'), ('build_draw', 'CPU 绘制入口', '#b84c7c'),
                              ('submit', '主机提交', '#a18b2e'), ('present', '主机呈现调用', '#e17a33')]:
        axes[2].plot(x, [r[key + '_ns'] / max(1, r[key + '_calls']) / 1e6 for r in selected],
                     label=label, lw=1, color=color)
    for key, label, color in [('gpu_scene_ns', 'GPU 场景', '#7b58ad'), ('gpu_output_ns', 'GPU 最终输出', '#299b90')]:
        axes[3].plot(x, [r[key] / max(1, r['gpu_samples']) / 1e6 for r in selected],
                     label=label, lw=1, color=color)
    for ax, ylabel in zip(axes, ['绘制 FPS', '进程 CPU（单核=100%）', '主机阶段平均墙钟耗时（ms/次）', 'GPU平均时间区间（ms/帧）']):
        ax.set_ylabel(ylabel)
        ax.grid(alpha=.2)
        ax.legend(loc='upper right', fontsize=9)
    axes[0].set_ylim(0, 265)
    axes[1].set_ylim(0, 110)
    axes[-1].set_xlabel('关卡时间距共同起点（秒）')
    fig.suptitle('泳池无尽：SDR 复测与分相定位 · LP14 / 用户设定11x / 240FPS上限', fontsize=16)
    fig.text(.075, .025, '按关卡时间对齐，原始一秒样本无平滑。当前实测HDR=0、960×600→960×600、无超分辨率链。\n跨轮战斗与代码并非完全相同；本轮有插桩，回升不能全部归因于HDR。主机阶段包含关系详见记录。', fontsize=10)
    fig.tight_layout(rect=(0, .065, 1, .965))
    fig.savefig(args.capture / 'sdr-comparison.png', dpi=150)
    fig.savefig(args.capture / 'sdr-comparison.svg')
    (args.capture / 'phase-summary.json').write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding='utf-8')
    with (args.capture / 'selected-frame-phases.csv').open('w', newline='', encoding='utf-8') as stream:
        writer = csv.DictWriter(stream, fieldnames=phases[0].keys())
        writer.writeheader(); writer.writerows(selected)
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
