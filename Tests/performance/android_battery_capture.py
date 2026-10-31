"""Enter on PC starts phone recording over USB; then unplug, play and export."""
import argparse
import csv
import json
from pathlib import Path
import re
import shlex
import subprocess
import time


def decode_samples(text):
    rows = []
    row = None
    for line in text.splitlines():
        if line.startswith('BEGIN '):
            row = dict(uptime=float(line[6:]), battery={}, frequencies_khz={})
        elif row is not None:
            if line == 'END':
                rows.append(row)
                row = None
            elif line.startswith('{'):
                try:
                    row['game'] = json.loads(line)
                except ValueError:
                    row['invalid_counter'] = line
            elif re.match(r'^\d+ \(', line):
                fields = line.rpartition(')')[2].split()
                row['cpu_ticks'] = int(fields[11])+int(fields[12])
            elif line.startswith('BAT '):
                _, key, *value = line.split(maxsplit=2)
                row['battery'][key] = value[0] if value else ''
            elif line.startswith('FREQ '):
                parts = line.split()
                if len(parts) == 3:
                    row['frequencies_khz'][parts[1]] = int(parts[2])
            elif line.startswith('GPU_BUSY '):
                parts = line.split()
                if len(parts) == 3:
                    busy, total = map(int, parts[1:])
                    row['gpu_busy_pct'] = 100*busy/total if total else None
    return rows


def summarize(rows, start, ticks):
    valid = []
    for previous, row in zip(rows, rows[1:]):
        dt = row['uptime']-previous['uptime']
        a, b = previous.get('game', {}), row.get('game', {})
        if dt <= 0 or previous['uptime'] < start or not a or not b:
            continue
        if not all(r['battery'].get('status') == 'Discharging' for r in (previous, row)):
            continue
        if a.get('scene') != 3 or b.get('scene') != 3 or a.get('paused') or b.get('paused'):
            continue
        if 'cpu_ticks' not in previous or 'cpu_ticks' not in row:
            continue
        valid.append(dict(uptime=row['uptime'], seconds=dt,
                          fps=((b['draw']-a['draw']) & 0xffffffff)/dt,
                          cpu_percent=(row['cpu_ticks']-previous['cpu_ticks'])/ticks/dt*100,
                          gpu_busy_pct=row.get('gpu_busy_pct')))
    seconds = sum(r['seconds'] for r in valid)
    valid_times = {r['uptime'] for r in valid}
    electrical = []
    for previous, row in zip(rows, rows[1:]):
        if row['uptime'] not in valid_times:
            continue
        try:
            watts = abs(float(row['battery']['current_now'])*float(row['battery']['voltage_now']))/1e12
            electrical.append((watts,row['uptime']-previous['uptime']))
        except (KeyError, ValueError):
            pass
    electrical_seconds = sum(dt for watts,dt in electrical)
    power = dict(available=bool(electrical_seconds),
                 source='Whole-device battery current x voltage, using Linux sysfs microampere/microvolt units; not process power.')
    if electrical_seconds:
        energy = sum(watts*dt for watts,dt in electrical)
        power.update(mean_w=energy/electrical_seconds, integrated_wh=energy/3600,
                     valid_seconds=electrical_seconds)
    return dict(valid_seconds=seconds, valid_intervals=len(valid), battery_side_power=power,
                fps_mean=sum(r['fps']*r['seconds'] for r in valid)/seconds if seconds else None,
                cpu_percent=sum(r['cpu_percent']*r['seconds'] for r in valid)/seconds if seconds else None,
                note='Battle counters with unpaused endpoints and Discharging battery status. '
                     'BatteryStats mAh is a UID estimate, not a per-process hardware energy meter. '
                     'Raw battery electrical telemetry is retained without assuming vendor units or cell topology.'), valid


def uid_charge_estimate(directory):
    uid = re.search(r'uid:(\d+)', (directory/'uid.txt').read_text())
    if not uid:
        return dict(available=False, reason='Package UID missing')
    number = int(uid[1])
    user, app = divmod(number, 100000)
    label = f'u{user}a{app-10000}' if app >= 10000 else f'u{user}s{app}'
    values = []
    for name in ('batterystats-battle-start.txt', 'batterystats-battle-end.txt'):
        path = directory/name
        if not path.exists():
            return dict(available=False, uid=number, reason='Complete battle estimate snapshots missing')
        value = re.search(r'^\s*UID '+re.escape(label)+r':\s*([\d.eE+\-]+)', path.read_text(), re.M)
        if not value:
            return dict(available=False, uid=number, reason='ROM did not expose a UID charge estimate')
        values.append(float(value[1]))
    delta = values[1]-values[0]
    return dict(available=delta>=0, uid=number, start_mah=values[0], end_mah=values[1],
                estimated_charge_mah=delta if delta>=0 else None,
                note='Modeled application UID charge over first battle window, including pauses and '
                     'the small same-UID counter-reader overhead. Not hardware-measured process energy.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='adb')
    parser.add_argument('--device', default='FY241471004E')
    parser.add_argument('--package', default='com.openpvz.perf')
    parser.add_argument('--symbol', required=True, help='gLawnApp address in the installed ELF, hexadecimal')
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output/'BASELINE-KEEP.json').write_text(json.dumps(dict(
        preserve_until_improved=True, device=args.device, package=args.package,
        purpose='Battery-powered benchmark. Preserve raw files before and after optimization.'), indent=2)+'\n')
    adb = [args.adb, '-s', args.device]

    def command(*parts, check=True):
        result = subprocess.run(adb+list(parts), capture_output=True, text=True, encoding='utf-8',
                                errors='replace', timeout=30)
        if check and result.returncode:
            raise RuntimeError(result.stderr or result.stdout)
        return result.stdout.strip()

    remote = '/data/local/tmp/pvz-battery-'+str(time.time_ns())
    command('shell', 'mkdir -p '+shlex.quote(remote))
    command('push', str(Path(__file__).with_name('android_battery_sampler.sh')), remote+'/sampler.sh')
    metadata = dict(device=args.device, package=args.package, symbol=args.symbol,
                    remote=remote, start_policy='PC Enter sends start command over connected USB',
                    preserve_until_improved=True, clock='Phone /proc/uptime; no PC clock conversion')
    (args.output/'capture.json').write_text(json.dumps(metadata, indent=2)+'\n')
    print('Prepare Continue/Paused. KEEP USB CONNECTED until recording starts.', flush=True)
    input('Press Enter to START recording on the phone: ')
    # One USB transaction starts and acknowledges the recorder. After the
    # STARTED message no further USB query is needed until export.
    ready = command('shell', 'nohup sh '+shlex.quote(remote+'/sampler.sh')+' '+shlex.quote(args.package)+
            ' '+shlex.quote(args.symbol)+' '+shlex.quote(remote)+' </dev/null >/dev/null 2>&1 & '
            'n=0; while [ ! -f '+shlex.quote(remote+'/start-uptime.txt')+' ] && [ "$n" -lt 100 ]; '
            'do sleep 0.1; n=$((n+1)); done; cat '+shlex.quote(remote+'/start-uptime.txt'))
    metadata['start_device_uptime'] = float(ready.splitlines()[-1])
    (args.output/'capture.json').write_text(json.dumps(metadata, indent=2)+'\n')
    print('Phone recording has STARTED. UNPLUG USB now.', flush=True)
    for remaining in (5, 4, 3, 2, 1):
        print(f'Unplug USB; resume battle in {remaining}...', flush=True)
        time.sleep(1)
    print('Resume now. Recording runs on the phone without this connection.', flush=True)
    print('After Quit, RECONNECT USB and press Enter here to export. No chat reply is needed.', flush=True)
    input('Press Enter after reconnecting: ')
    command('wait-for-device')
    command('shell', 'touch '+remote+'/stop')
    for _ in range(12):
        if command('shell', 'test -f '+remote+'/done && echo done', check=False) == 'done':
            break
        time.sleep(1)
    command('pull', remote, str(args.output/'device'))
    directory = args.output/'device'
    rows = decode_samples((directory/'samples.txt').read_text(errors='replace'))
    ticks = int((directory/'clock-ticks.txt').read_text().strip())
    summary, intervals = summarize(rows, metadata['start_device_uptime'], ticks)
    summary['uid_charge_estimate'] = uid_charge_estimate(directory)
    reason = directory/'stop-reason.txt'
    summary['stop_reason'] = reason.read_text().strip() if reason.exists() else 'observer-stopped'
    (args.output/'samples.json').write_text(json.dumps(rows, indent=2)+'\n')
    (args.output/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    with (args.output/'intervals.csv').open('w', newline='') as stream:
        writer = csv.DictWriter(stream, fieldnames=['uptime','seconds','fps','cpu_percent','gpu_busy_pct'])
        writer.writeheader()
        writer.writerows(intervals)
    print(json.dumps(summary, indent=2), flush=True)
    print('All raw files retained: '+str(args.output), flush=True)


if __name__ == '__main__':
    main()
