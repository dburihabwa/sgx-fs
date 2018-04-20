#! /usr/bin/env bash

make clean
make SGX_MODE=HW DEBUG=1 SGX_PRERELEASE=1
mkdir -p /tmp/mnt/fuse
sudo umount -f /mnt/fuse
./app -d /tmp/mnt/fuse
