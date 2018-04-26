#! /usr/bin/env bash

make clean -s
make SGX_MODE=HW DEBUG=1 SGX_PRERELEASE=1 -s
mkdir -p /tmp/mnt/fuse
sudo umount -f /tmp/mnt/fuse
#nohup ./app-sgx-ramfs -f /tmp/mnt/fuse > fs.log 2>&1 &
./app-sgx-ramfs /tmp/mnt/fuse

