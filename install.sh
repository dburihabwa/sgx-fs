#! /usr/bin/env bash

install_dependencies() {
    # Install SGX driver
    apt-get install --yes --quiet autoconf automake build-essential g++ git libcurl4-openssl-dev libfuse-dev libprotobuf-dev libssl-dev libtool "linux-headers-$(uname -r)" ocaml pkg-config protobuf-compiler python wget &&\
    if [[ ! -d /opt/linux-sgx-driver ]]; then
	    git clone https://github.com/intel/linux-sgx-driver /opt/linux-sgx-driver
    fi
    cd /opt/linux-sgx-driver
    git clean -df
    make
    mkdir -p "/lib/modules/$(uname -r)/kernel/drivers/intel/sgx"
    cp isgx.ko "/lib/modules/$(uname -r)/kernel/drivers/intel/sgx"
    sh -c "cat /etc/modules | grep -Fxq isgx || echo isgx >> /etc/modules"
    /sbin/depmod
    /sbin/modprobe isgx


    # Install SGX SDK and PSW
    apt-get update && apt-get dist-upgrade --yes --quiet && apt-get install libssl-dev libcurl4-openssl-dev libprotobuf-dev --yes --quiet
    if [[ ! -d /opt/linux-sgx ]]; then
         git clone https://github.com/intel/linux-sgx /opt/linux-sgx
    fi
    cd /opt/linux-sgx
    git clean -df
    git checkout sgx_2.1.2
    ./download_prebuilt.sh
    make sdk_install_pkg
    make psw_install_pkg
    cd linux/installer/bin
    echo "yes" | ./sgx_linux_x64_sdk_2.1.102.43402.bin
    ./sgx_linux_x64_psw_2.1.102.43402.bin
    source /opt/linux-sgx/linux/installer/bin/sgxsdk/environment
    echo -e "\n##### Intel SGX SDK\nsource /opt/linux-sgx/linux/installer/bin/sgxsdk/environment\n" >> ~/.bashrc
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    if [[ "${EUID}" -ne 0 ]]; then
      echo "${0} must be run as root"
      exit
    fi
    install_dependencies
fi
