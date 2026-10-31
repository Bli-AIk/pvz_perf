"""Observe an existing game through RTSS published windows and process CPU time."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import csv
import json
from pathlib import Path
import struct
import time

k = c.WinDLL('kernel32', use_last_error=True)
u = c.WinDLL('user32', use_last_error=True)
k.OpenFileMappingW.argtypes = [w.DWORD, w.BOOL, w.LPCWSTR]
k.OpenFileMappingW.restype = w.HANDLE
k.MapViewOfFile.argtypes = [w.HANDLE, w.DWORD, w.DWORD, w.DWORD, c.c_size_t]
k.MapViewOfFile.restype = c.c_void_p
k.UnmapViewOfFile.argtypes = [c.c_void_p]
k.CloseHandle.argtypes = [w.HANDLE]
k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
k.OpenProcess.restype = w.HANDLE
k.GetProcessTimes.argtypes = [w.HANDLE] + [c.POINTER(w.FILETIME)] * 4
k.GetProcessAffinityMask.argtypes = [w.HANDLE, c.POINTER(c.c_size_t), c.POINTER(c.c_size_t)]
k.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
u.GetForegroundWindow.restype = w.HWND
u.GetWindowThreadProcessId.argtypes = [w.HWND, c.POINTER(w.DWORD)]


class Rtss:
    def __init__(self):
        self.handle = k.OpenFileMappingW(4, False, 'RTSSSharedMemoryV2')
        if not self.handle:
            raise c.WinError(c.get_last_error())
        self.view = k.MapViewOfFile(self.handle, 4, 0, 0, 0)
        if not self.view:
            k.CloseHandle(self.handle)
            raise c.WinError(c.get_last_error())

    def read(self, pid):
        header = struct.unpack('<5I', c.string_at(self.view, 20))
        signature, version, stride, offset, count = header
        if signature != 0x52545353 or version < 0x20000:
            raise RuntimeError('RTSS mapping is not initialized')
        if stride < 284 or count > 4096:
            raise RuntimeError('Unexpected RTSS application layout')
        for i in range(count):
            address = self.view + offset + i * stride
            first = c.string_at(address, 284)
            if struct.unpack_from('<I', first)[0] != pid:
                continue
            # Reject a concurrent publication rather than mix two windows.
            if first != c.string_at(address, 284):
                return None
            flags, start, end, frames, frame_us = struct.unpack_from('<5I', first, 264)
            duration = (end - start) & 0xffffffff
            if not start or not duration or duration > 10000:
                return None
            return dict(start_ms=start, end_ms=end, frames=frames,
                        duration_ms=duration, fps=frames * 1000 / duration,
                        last_frame_us=frame_us, api_flags=flags,
                        executable=first[4:264].split(b'\0')[0].decode('mbcs'))
        return None

    def close(self):
        k.UnmapViewOfFile(self.view)
        k.CloseHandle(self.handle)


def cpu_seconds(handle):
    values = [w.FILETIME() for _ in range(4)]
    if not k.GetProcessTimes(handle, *(c.byref(v) for v in values)):
        raise c.WinError(c.get_last_error())
    return sum((v.dwHighDateTime << 32) | v.dwLowDateTime for v in values[2:]) / 1e7


def foreground(pid):
    result = w.DWORD()
    u.GetWindowThreadProcessId(u.GetForegroundWindow(), c.byref(result))
    return result.value == pid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pid', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--require-affinity', type=lambda value: int(value, 0),
                        help='Refuse to arm until this mask is set externally; never changes affinity')
    args = parser.parse_args()
    rtss = Rtss()
    process = k.OpenProcess(0x100000 | 0x1000, False, args.pid)
    if not process:
        rtss.close()
        raise c.WinError(c.get_last_error())
    try:
        deadline = time.perf_counter() + 90
        before = None
        print('Waiting for advancing RTSS statistics (up to 90 seconds)...', flush=True)
        while True:
            if k.WaitForSingleObject(process, 0) != 258:
                raise RuntimeError('Game exited before RTSS verification')
            current = rtss.read(args.pid)
            if before and current and before['end_ms'] != current['end_ms']:
                break
            if current:
                before = current
            if time.perf_counter() >= deadline:
                raise RuntimeError('RTSS statistics did not advance within 90 seconds; no capture started')
            time.sleep(0.2)
        print(json.dumps(current, indent=2), flush=True)
        if args.verify:
            return
        args.output.mkdir(parents=True, exist_ok=False)
        affinity, system = c.c_size_t(), c.c_size_t()
        if not k.GetProcessAffinityMask(process, c.byref(affinity), c.byref(system)):
            raise c.WinError(c.get_last_error())
        metadata = dict(pid=args.pid, affinity_mask=hex(affinity.value),
                        source='RTSS shared memory presentation windows',
                        logic_hz=100, render_limit='uncapped', preroll_seconds=3,
                        note='No scene/wave introspection. Foreground is not a battle-state test.',
                        rtss=current)
        (args.output / 'conditions.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
        print('Prepare Continue/Paused. Press Enter to start; resume at t=3 seconds.', flush=True)
        print('Sampling ends when you Quit. Saves and affinity are not modified.', flush=True)
        while True:
            input('Press Enter to arm: ')
            if not k.GetProcessAffinityMask(process, c.byref(affinity), c.byref(system)):
                raise c.WinError(c.get_last_error())
            if args.require_affinity is None or affinity.value == args.require_affinity:
                break
            print(f'Not started: affinity is {hex(affinity.value)}, expected {hex(args.require_affinity)}. '
                  'Set affinity externally, then press Enter again.', flush=True)
        metadata['affinity_mask'] = hex(affinity.value)
        metadata['affinity_recorded_at'] = 'Enter/start'
        (args.output / 'conditions.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
        print(f'Capture affinity: {hex(affinity.value)}', flush=True)
        start = previous = time.perf_counter()
        previous_cpu = cpu_seconds(process)
        last_key = (current['start_ms'], current['end_ms'])
        latest_seen = start
        windows = []
        rows = []
        with (args.output / 'samples.csv').open('w', newline='') as sf, (args.output / 'present-windows.csv').open('w', newline='') as pf:
            sw = csv.DictWriter(sf, fieldnames=['elapsed_s', 'duration_s', 'cpu_s', 'cpu_percent', 'foreground', 'phase', 'rtss_fps', 'rtss_age_s'])
            pw = csv.DictWriter(pf, fieldnames=['observed_elapsed_s', 'foreground', 'phase', 'start_ms', 'end_ms', 'frames', 'duration_ms', 'fps'])
            sw.writeheader()
            pw.writeheader()
            while k.WaitForSingleObject(process, 0) == 258:
                now = time.perf_counter()
                elapsed = now - start
                fg = foreground(args.pid)
                phase = 'preroll' if elapsed < 3 else 'capture'
                value = rtss.read(args.pid)
                if value:
                    key = (value['start_ms'], value['end_ms'])
                    if key != last_key:
                        last_key = key
                        latest_seen = now
                        item = dict(observed_elapsed_s=elapsed, foreground=int(fg), phase=phase,
                                    **{name: value[name] for name in ['start_ms', 'end_ms', 'frames', 'duration_ms', 'fps']})
                        pw.writerow(item)
                        windows.append(item)
                if now - previous >= 1:
                    cpu = cpu_seconds(process)
                    duration = now - previous
                    row = dict(elapsed_s=elapsed, duration_s=duration, cpu_s=cpu-previous_cpu,
                               cpu_percent=100*(cpu-previous_cpu)/duration,
                               foreground=int(fg), phase=phase,
                               rtss_fps=value['fps'] if value and now-latest_seen < 2.5 else '',
                               rtss_age_s=now-latest_seen)
                    sw.writerow(row)
                    rows.append(row)
                    sf.flush()
                    pf.flush()
                    print(f"{elapsed:7.1f}s  FPS={row['rtss_fps']}  CPU={row['cpu_percent']:.2f}%", flush=True)
                    previous, previous_cpu = now, cpu
                time.sleep(0.1)
        # Preserve raw windows: gaps/overlaps must be checked before comparing runs.
        result = dict(stop_reason='game-exited', duration_s=time.perf_counter()-start,
                      cpu_samples=len(rows), presentation_windows=len(windows),
                      note='Raw data retained. Exclude preroll, background, menus and overlapping RTSS windows before reporting.')
        (args.output / 'measurement.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
        print(json.dumps(result, indent=2), flush=True)
    finally:
        k.CloseHandle(process)
        rtss.close()


if __name__ == '__main__':
    main()
