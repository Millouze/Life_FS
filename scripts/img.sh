#!/bin/sh

# Fix the paths if necessary
SHARED="../share/"

make
make -C mkfs
make img -C mkfs
cp scripts/benchmark.sh ${SHARED}
cp mkfs/test.img ouichefs.ko ${SHARED}
cp usercode/unit_test.c ${SHARED}
cp usercode/benchmark.c ${SHARED}
cp usercode/utils.c ${SHARED}
cp usercode/utils.h ${SHARED}
