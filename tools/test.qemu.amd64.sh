#!/bin/sh

SCRIPT_DIR=$(dirname $(realpath ${0}))
ROOT_DIR=$(realpath ${SCRIPT_DIR}/..)
RUN_SCRIPT=${SCRIPT_DIR}/run.qemu.amd64.sh
MAX_RETRIES=500
WAIT_SECONDS=5
LOGFILE=${ROOT_DIR}/qemu.log

cd ${ROOT_DIR}

kill_qemu() {
    taskkill /F /IM qemu-system-x86_64.exe > /dev/null 2>&1 \
        || pkill -x qemu-system-x86_64 2>/dev/null
}

attempt=0

while true; do
    attempt=$((attempt + 1))

    if [ ${attempt} -gt ${MAX_RETRIES} ]; then
        echo "TEST: exceeded maximum retry count (${MAX_RETRIES}), giving up"
        exit 1
    fi

    echo "TEST: starting QEMU (attempt ${attempt}/${MAX_RETRIES})"

    sh ${RUN_SCRIPT} "$@" &
    SCRIPT_PID=$!

    sleep ${WAIT_SECONDS}

    kill_qemu
    wait ${SCRIPT_PID} 2>/dev/null

    if grep -q "panic" ${LOGFILE} 2>/dev/null; then
        echo "TEST: kernel panic detected, stopping"
        echo "--- log ---"
        cat ${LOGFILE}
        echo "-----------"
        exit 1
    fi

    if grep -qE "TICK 4[^0-9]|TICK 4$" ${LOGFILE} 2>/dev/null; then
        echo "TEST: found TICK 4, restarting..."
        continue
    fi

    echo "TEST: run completed successfully"
    exit 0
done
