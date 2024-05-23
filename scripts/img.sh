#!/bin/sh

# Fix the paths if necessary
SHARED="../share/"

make
make -C mkfs
make img -C mkfs
mv mkfs/test.img ouichefs.ko ${SHARED}
