FROM ubuntu:16.04

RUN apt-get update &&\
    apt-get dist-upgrade --yes --quiet &&\
    apt-get install --yes --quiet autoconf automake build-essential g++ git libcurl4-openssl-dev kmod libfuse-dev libprotobuf-dev libssl-dev libtool ocaml pkg-config protobuf-compiler python wget &&\
    git clone https://github.com/intel/linux-sgx /opt/linux-sgx &&\
    cd /opt/linux-sgx &&\
    git checkout sgx_2.1.2 &&\
    ./download_prebuilt.sh &&\
    make sdk_install_pkg &&\
    make psw_install_pkg
RUN cd /opt/linux-sgx/linux/installer/bin &&\
    echo "yes" | ./sgx_linux_x64_sdk_2.1.102.43402.bin &&\
    /bin/bash -c "source /opt/linux-sgx/linux/installer/bin/sgxsdk/environment" &&\
    /opt/linux-sgx/linux/installer/bin/sgx_linux_x64_psw_2.1.102.43402.bin &&\
    echo -e "\n##### Intel SGX SDK\nsource /opt/linux-sgx/linux/installer/bin/sgxsdk/environment\n" >> ~/.bashrc
COPY . /opt/sfusegx/
WORKDIR /opt/sfusegx
RUN /bin/bash -c "source /opt/linux-sgx/linux/installer/bin/sgxsdk/environment" make SGX_MODE=HW DEBUG=1 SGX_PRERELEASE=1 &&\
    mkdir -p /tmp/mnt/ramfs /tmp/mnt/sgx-ramfs
