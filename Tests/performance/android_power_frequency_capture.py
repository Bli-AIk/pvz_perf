"""Enter starts detached phone power/frequency telemetry; unplug, play, reconnect."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import time
from android_battery_capture import decode_samples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='adb')
    parser.add_argument('--device', default='FY241471004E')
    parser.add_argument('--package', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)

    def command(*parts, check=True):
        result = subprocess.run([args.adb, '-s', args.device, *parts], capture_output=True,
                                text=True, encoding='utf-8', errors='replace', timeout=30)
        if check and result.returncode:
            raise RuntimeError(result.stderr or result.stdout)
        return result.stdout.strip()

    remote = '/data/local/tmp/pvz-power-frequency-' + str(time.time_ns())
    command('shell', 'mkdir -p ' + shlex.quote(remote))
    command('push', str(Path(__file__).with_name('android_power_frequency_sampler.sh')), remote+'/sampler.sh')
    meta = dict(package=args.package, device=args.device, remote=remote,
                method='Whole-device battery current*voltage and instantaneous CPU policy frequencies',
                fps='Not measured; user assumes full frame rate',
                scene='User-controlled endless battle; no pause/menu detection',
                preserve_until_improved=True)
    (args.output/'capture.json').write_text(json.dumps(meta, indent=2), encoding='utf-8')
    print('Prepare endless Continue/Paused. Keep cable connected.', flush=True)
    input('Press Enter to START phone recording: ')
    if not command('shell', 'pidof ' + shlex.quote(args.package), check=False):
        raise RuntimeError('Game is not running. No recording started.')
    ready = command('shell', 'nohup sh '+shlex.quote(remote+'/sampler.sh')+' '+shlex.quote(args.package)+' '+
                    shlex.quote(remote)+' </dev/null >/dev/null 2>&1 & '
                    'n=0; while [ ! -f '+shlex.quote(remote+'/start-uptime.txt')+' ] && [ "$n" -lt 150 ]; '
                    'do sleep 0.1; n=$((n+1)); done; cat '+shlex.quote(remote+'/start-uptime.txt'))
    meta['start_uptime'] = float(ready.splitlines()[-1])
    (args.output/'capture.json').write_text(json.dumps(meta, indent=2), encoding='utf-8')
    print('STARTED. Unplug all charging cables NOW. Resume battle after the countdown.', flush=True)
    for remaining in range(5, 0, -1):
        print(remaining, flush=True)
        time.sleep(1)
    print('Resume now. After finishing, Quit, reconnect USB and press Enter to export.', flush=True)
    input('Press Enter AFTER reconnecting: ')
    command('wait-for-device')
    command('shell', 'touch '+shlex.quote(remote+'/stop'))
    for _ in range(15):
        if command('shell', 'test -f '+shlex.quote(remote+'/done')+' && echo done', check=False) == 'done':
            break
        time.sleep(1)
    else:
        raise RuntimeError('Recorder did not acknowledge stop; raw data remains on phone.')
    command('pull', remote, str(args.output/'device'))
    rows = decode_samples((args.output/'device/samples.txt').read_text(errors='replace'))
    (args.output/'samples.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
    # Only discharge intervals; trim transitions. Detailed scene selection is manual.
    valid = []
    for a, b in zip(rows, rows[1:]):
        dt = b['uptime']-a['uptime']
        if not 0 < dt < 5 or a['uptime'] < meta['start_uptime']+5:
            continue
        if any(r['battery'].get('status') != 'Discharging' for r in (a,b)):
            continue
        try:
            watts = abs(float(b['battery']['current_now'])*float(b['battery']['voltage_now']))/1e12
        except (KeyError, ValueError):
            continue
        valid.append((dt, watts))
    seconds = sum(dt for dt, _ in valid)
    result = dict(discharge_seconds=seconds, mean_device_w=sum(dt*p for dt,p in valid)/seconds if seconds else None,
                  note='Whole-device electrical telemetry, not process power. Menus/pauses require manual exclusion. '
                       'CPU frequency samples are policy frequencies, not game-thread residency.')
    (args.output/'summary.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result, indent=2), flush=True)
    print('Raw files preserved: '+str(args.output), flush=True)


if __name__ == '__main__':
    main()
