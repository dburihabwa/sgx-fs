#! /usr/bin/env bash

# Install SGX driver
apt-get install --yes --quiet autoconf automake build-essential g++ git libcurl4-openssl-dev libfuse-dev libprotobuf-dev libssl-dev libtool linux-headers-$(uname -r) ocaml pkg-config protobuf-compiler python wget &&\
git clone https://github.com/intel/linux-sgx-driver /opt/linux-sgx-driver
cd /opt/linux-sgx-driver
make 
mkdir -p "/lib/modules/"`uname -r`"/kernel/drivers/intel/sgx"    
cp isgx.ko "/lib/modules/"`uname -r`"/kernel/drivers/intel/sgx"    
sh -c "cat /etc/modules | grep -Fxq isgx || echo isgx >> /etc/modules"    
/sbin/depmod
/sbin/modprobe isgx


# Install SGX SDK
apt-get update &&\
apt-get dist-upgrade --yes --quiet &&\

git clone https://github.com/intel/linux-sgx /opt/linux-sgx &&\
cd /opt/linux-sgx &&\
git checkout sgx_2.1.2 &&\
./download_prebuilt.sh &&\
make sdk_install_pkg &&\
cd linux/installer/bin &&\
echo "yes" | ./sgx_linux_x64_sdk_2.1.102.43402.bin
source /opt/linux-sgx/linux/installer/bin/sgxsdk/environment
make SGX_MODE=HW SGX_PRERELEASE=1

echo -e "\n##### Intel SGX SDK\nsource /opt/linux-sgx/linux/installer/bin/sgxsdk/environment\n" >> ~/.bashrc 

