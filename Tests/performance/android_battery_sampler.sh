#!/system/bin/sh
# Runs as adb shell after USB disconnect. No changes to the game or its clocks.
package=$1
symbol=$2
directory=$3
limit=${4:-1800}
mkdir -p "$directory" || exit 1
exec >"$directory/samples.txt" 2>"$directory/sampler-error.txt"
echo $$ >"$directory/sampler.pid"
getconf CLK_TCK >"$directory/clock-ticks.txt"
dumpsys package "$package" >"$directory/package.txt"
cmd package list packages -U "$package" >"$directory/uid.txt"
for policy in /sys/devices/system/cpu/cpufreq/policy*; do
    echo "$policy cpus=$(cat "$policy/related_cpus") max=$(cat "$policy/cpuinfo_max_freq")"
done >"$directory/cpu-topology.txt"
valid=0
missing=0
battle=0
unplugged=0
count=0
read start rest </proc/uptime
echo "$start" >"$directory/start-uptime.txt"
while [ "$count" -lt "$limit" ] && [ ! -f "$directory/stop" ]; do
    read uptime rest </proc/uptime
    echo "BEGIN $uptime"
    pid=$(pidof "$package")
    counters=
    if [ -n "$pid" ]; then
        run-as "$package" cat "/proc/$pid/stat"
        counters=$(run-as "$package" files/pvz-counter-reader "$pid" "$symbol" 2>>"$directory/counter-error.txt")
        echo "$counters"
    fi
    for field in current_now voltage_now power_now energy_now charge_counter temp status capacity; do
        echo "BAT $field $(cat /sys/class/power_supply/battery/$field 2>/dev/null)"
    done
    for policy in /sys/devices/system/cpu/cpufreq/policy*; do
        echo "FREQ ${policy##*/} $(cat "$policy/scaling_cur_freq" 2>/dev/null)"
    done
    echo "GPU_BUSY $(cat /sys/class/kgsl/kgsl-3d0/gpubusy 2>/dev/null)"
    echo END
    if [ "$unplugged" -eq 0 ] && [ "$(cat /sys/class/power_supply/battery/status)" = Discharging ]; then
        dumpsys battery >"$directory/battery-unplugged.txt"
        dumpsys batterystats "$package" >"$directory/batterystats-unplugged.txt"
        unplugged=1
    fi
    case "$counters" in
        '{'*) valid=1; missing=0 ;;
        *) missing=$((missing+1)) ;;
    esac
    case "$counters" in
        *'"scene":3,"paused":false'*)
            if [ "$battle" -eq 0 ] && [ "$unplugged" -eq 1 ]; then
                dumpsys batterystats "$package" >"$directory/batterystats-battle-start.txt"
                battle=1
            fi ;;
        *'"scene":3,'*) ;;
        *)
            if [ "$battle" -eq 1 ]; then
                dumpsys batterystats "$package" >"$directory/batterystats-battle-end.txt"
                battle=2
            fi ;;
    esac
    if [ "$valid" -eq 1 ] && [ "$missing" -ge 3 ]; then
        if [ -z "$pid" ]; then
            echo process-exited >"$directory/stop-reason.txt"
        else
            echo counter-unavailable-after-valid-samples >"$directory/stop-reason.txt"
        fi
        break
    fi
    count=$((count+1))
    sleep 1
done
dumpsys batterystats "$package" >"$directory/batterystats-end.txt"
dumpsys battery >"$directory/battery-end.txt"
echo "$count" >"$directory/done"
