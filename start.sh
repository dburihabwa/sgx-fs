#! /usr/bin/env bash

make clean
make SGX_MODE=HW DEBUG=1 SGX_PRERELEASE=1
mkdir -p /tmp/mnt/fuse
sudo umount -f /mnt/fuse
./app-sgx-ramfs -d /tmp/mnt/fuse
