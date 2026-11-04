"""Compare retained whole-fight observations on the same Board clock."""
import argparse
import csv
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def load(path):
    with (path / 'samples.csv').open(encoding='utf-8-sig', newline='') as stream:
        return [{k: (v if k == 'segment' else float(v) if v else None)
                 for k, v in row.items()} for row in csv.DictReader(stream)
                if row['segment'] == 'game' and row['valid'] == '1']


def stats(rows):
    seconds = sum(r['interval_s'] for r in rows)
    frames = sum(r['draw_fps'] * r['interval_s'] for r in rows)
    cpu = sum(r['cpu_percent'] * r['interval_s'] / 100 for r in rows)
    return dict(seconds=seconds, fps=frames / seconds,
                cpu_percent=100 * cpu / seconds, cpu_ms_per_frame=1000 * cpu / frames)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('current', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--baseline-label', default='v3.2.4')
    parser.add_argument('--current-label', default='3.3 beta（原生触控，无侧键模拟）')
    parser.add_argument('--overlay-fps-cpu', action='store_true',
                        help='Also plot the current run FPS and CPU on one timeline')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'DejaVu Sans']
    plt.rcParams['axes.unicode_minus'] = False
    fig, axes = plt.subplots(3, 1, figsize=(14, 10), sharex=True)
    results = {}
    for path, label, color in [(args.baseline, args.baseline_label, '#1678c8'),
                               (args.current, args.current_label, '#db6134')]:
        rows = load(path)
        x = [(r['board_counter'] - 2291960000000) / 1e9 for r in rows]
        for ax, key in zip(axes, ['draw_fps', 'cpu_percent', 'cpu_ms_per_draw_frame']):
            ax.plot(x, [r[key] for r in rows], color=color, lw=1.15, alpha=.85, label=label)
        results[label] = {'overall': stats(rows), 'wave_groups': {}}
        for start, end in [(0, 9), (10, 19), (20, 20)]:
            group = [r for r in rows if start <= r['wave'] <= end]
            if group:
                results[label]['wave_groups'][f'{start}-{end}'] = stats(group)
        for ax in axes:
            for wave in [10, 20]:
                match = next((r for r in rows if r['wave'] >= wave), None)
                if match:
                    at = (match['board_counter'] - 2291960000000) / 1e9
                    ax.axvline(at, color=color, alpha=.35, ls='--', lw=.8)
        axes[0].annotate(f'{label}: {stats(rows)["fps"]:.2f} FPS',
                         xy=(.015, .95 if path == args.baseline else .84),
                         xycoords='axes fraction', color=color, fontsize=11)
    for ax, title in zip(axes, ['绘制 FPS', '进程 CPU 占用（单核 = 100%）', '进程 CPU 时间 / 绘制帧（ms）']):
        ax.set_ylabel(title)
        ax.grid(alpha=.2)
        ax.legend(loc='upper right', fontsize=9)
    axes[0].set_ylim(0, 265)
    axes[1].set_ylim(0, 110)
    axes[-1].set_xlabel('关卡时间距共同起点（秒）')
    fig.suptitle('泳池无尽整局对比 · Vulkan · LP14 · 用户设定 11x · 240FPS 上限', fontsize=16)
    fig.text(.07, .025, '原始一秒样本，无平滑。虚线：各轮第 10、20 波起点。相同起点不代表战斗对象与操作完全一致。\nCPU ms/帧含全部进程线程工作，并非逐帧延迟；这两轮未采 GPU 频率。', fontsize=10)
    fig.tight_layout(rect=(0, .065, 1, .96))
    fig.savefig(args.output / 'comparison.png', dpi=150)
    fig.savefig(args.output / 'comparison.svg')
    if args.overlay_fps_cpu:
        rows = load(args.current)
        x = [(r['board_counter'] - 2291960000000) / 1e9 for r in rows]
        overlay, fps = plt.subplots(figsize=(14, 5.5))
        cpu = fps.twinx()
        fps.plot(x, [r['draw_fps'] for r in rows], color='#1678c8', lw=1.25, label='绘制 FPS')
        cpu.plot(x, [r['cpu_percent'] for r in rows], color='#e47524', lw=1.15,
                 alpha=.85, label='进程 CPU 占用')
        fps.set(xlabel='关卡时间距共同起点（秒）', ylabel='绘制 FPS', ylim=(0, 265))
        cpu.set(ylabel='进程 CPU 占用（单核 = 100%）', ylim=(0, 110))
        fps.grid(alpha=.2)
        lines = fps.lines + cpu.lines
        fps.legend(lines, [line.get_label() for line in lines], loc='lower left')
        overlay.suptitle(f'{args.current_label} · Vulkan · LP14 · 用户设定 11x · 240FPS 上限')
        overlay.text(.07, .02, '仅有效前台战斗，一秒采样，无平滑；左右坐标轴量纲不同。CPU 含整个进程线程工作。', fontsize=10)
        overlay.tight_layout(rect=(0, .055, 1, .94))
        overlay.savefig(args.output / 'fps-cpu.png', dpi=150)
        overlay.savefig(args.output / 'fps-cpu.svg')
    (args.output / 'comparison.json').write_text(json.dumps(results, indent=2, ensure_ascii=False), encoding='utf-8')
    print(json.dumps(results, indent=2, ensure_ascii=False))


if __name__ == '__main__':
    main()
