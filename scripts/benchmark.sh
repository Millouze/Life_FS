#!/bin/sh

mkdir -p /mnt/ouiche_fs
insmod /share/ouichefs.ko
mount /share/test.img /mnt/ouiche_fs/

  
