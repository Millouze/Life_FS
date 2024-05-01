#!/bin/sh

mkdir -p /mnt/ouiche_fs
insmod /share/pnl/ouichefs.ko
mount /share/pnl/test.img /mnt/ouiche_fs/

# ./benchmark noms de fichiers 