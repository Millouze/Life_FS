#!/bin/sh

umount /mnt/ouiche_fs
rm -R /mnt/ouiche_fs
rmmod ouichefs.ko

mkdir -p /mnt/ouiche_fs
insmod /share/ouichefs.ko
mount /share/test.img /mnt/ouiche_fs/
mv /share/benchmark.c ~/
gcc -Wall ~/benchmark.c -o ~/benchmark
