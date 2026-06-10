#!/bin/bash

# This script launches multiple QEMU instances because
# I can't be fucked to serially launch them and wait for
# a floating error to show its ugly head

cd $(dirname $(realpath $(dirname ${0})))

run_script=tools/run.qemu.amd64.sh
max_instances=4
wait_seconds=10

for i in $(seq 1 ${max_instances}); do
    echo "TEST: starting QEMU (instance ${i}/${max_instances})"
    sh ${run_script} ${i} "$@" &
done

sleep ${wait_seconds}

echo "TEST: stopping QEMU instances"

cmd.exe "/c taskkill /F /IM qemu-system-x86_64.exe" > /dev/null 2>&1 || true
pkill -9 qemu-system-x86_64 > /dev/null 2>&1 || true
killall -9 qemu-system-x86_64 > /dev/null 2>&1 || true

for i in $(seq 0 ${max_instances}); do
    logfile="qemu_${i}.log"
    if grep -q "kernel panic" ${logfile} 2>/dev/null; then
        echo "TEST: kernel panic detected in instance ${i}, stopping"
        echo "--- log ${logfile} ---"
        cat ${logfile}
        echo "---- end ${logfile} -------"
        exit 0
    fi
done

exit 1
