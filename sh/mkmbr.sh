#!/bin/sh

cd $(dirname $(realpath $(dirname ${0})))

IMAGE=build/vexsys.mbr.img

dd if=/dev/zero of=${IMAGE} bs=1M count=16 2>/dev/null
printf "label: dos\nstart=2048,size=8192,type=83\ntype=83\n" | sfdisk --force ${IMAGE} >/dev/null

# write recognizable patterns to LBA 0 of each partition
printf "PARTITION_ONE_DATA" | dd of=${IMAGE} bs=512 seek=2048 conv=notrunc 2>/dev/null
printf "PARTITION_TWO_DATA" | dd of=${IMAGE} bs=512 seek=10240 conv=notrunc 2>/dev/null

printf "Created %s\n" ${IMAGE}
