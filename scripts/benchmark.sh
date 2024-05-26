#!/bin/sh

umount /mnt/ouiche_fs
rm -R /mnt/ouiche_fs
rmmod ouichefs.ko

mkdir -p /mnt/ouiche_fs
insmod /share/ouichefs.ko
mount /share/test.img /mnt/ouiche_fs/
mv /share/benchmark.c ~/
mv /share/unit_test.c ~/
mv /share/utils.c ~/
mv /share/utils.h ~/
gcc -c ~/utils.c
gcc -Wall ~/benchmark.c ~/utils.c -o ~/benchmark
gcc -Wall ~/unit_test.c ~/utils.c -o ~/unit_test
