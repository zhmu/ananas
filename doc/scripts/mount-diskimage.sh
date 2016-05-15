#!/bin/sh
sudo mount -t vfat -o loop,offset=1048576,uid=1000 /tmp/disk.img /mnt
