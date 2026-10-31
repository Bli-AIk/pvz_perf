#!/system/bin/sh
# Detached battery-side telemetry. No game memory, affinity or frequency changes.
package=$1
directory=$2
mkdir -p "$directory" || exit 1
exec >"$directory/samples.txt" 2>"$directory/errors.txt"
echo $$ >"$directory/sampler.pid"
dumpsys package "$package" >"$directory/package.txt"
dumpsys battery >"$directory/battery-start.txt"
dumpsys batterystats "$package" >"$directory/batterystats-start.txt"
for policy in /sys/devices/system/cpu/cpufreq/policy*; do
    echo "$policy $(cat "$policy/related_cpus") max=$(cat "$policy/cpuinfo_max_freq")"
done >"$directory/cpu-topology.txt"
read start rest </proc/uptime
echo "$start" >"$directory/start-uptime.txt"
seen=0
missing=0
count=0
while [ "$count" -lt 3600 ] && [ ! -f "$directory/stop" ]; do
    read uptime rest </proc/uptime
    echo "BEGIN $uptime"
    pid=$(pidof "$package")
    echo "PID $pid"
    for field in current_now voltage_now power_now charge_counter temp status capacity; do
        echo "BAT $field $(cat "/sys/class/power_supply/battery/$field" 2>/dev/null)"
    done
    for policy in /sys/devices/system/cpu/cpufreq/policy*; do
        echo "FREQ ${policy##*/} $(cat "$policy/scaling_cur_freq" 2>/dev/null)"
    done
    echo END
    if [ -n "$pid" ]; then seen=1; missing=0; else missing=$((missing+1)); fi
    if [ "$seen" -eq 1 ] && [ "$missing" -ge 3 ]; then
        echo process-exited >"$directory/stop-reason.txt"
        break
    fi
    count=$((count+1))
    sleep 1
done
dumpsys batterystats "$package" >"$directory/batterystats-end.txt"
dumpsys battery >"$directory/battery-end.txt"
echo "$count" >"$directory/done"
