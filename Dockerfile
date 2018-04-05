FROM ubuntu:16.04

RUN apt-get update &&\
    apt-get dist-upgrade --yes --quiet &&\
    apt-get install --yes --quiet autoconf automake build-essential g++ git libcurl4-openssl-dev libfuse-dev libprotobuf-dev libssl-dev libtool ocaml pkg-config protobuf-compiler python wget &&\
    git clone https://github.com/intel/linux-sgx /opt/linux-sgx &&\
    cd /opt/linux-sgx &&\
    git checkout sgx_2.1.2 &&\
    ./download_prebuilt.sh &&\
    make sdk_install_pkg &&\
    make psw_install_pkg &&\
    cd linux/installer/bin &&\
    echo "yes" | ./sgx_linux_x64_sdk_2.1.102.43402.bin &&\
    ./sgx_linux_x64_psw_2.1.102.43402.bin &&\
    source /opt/linux-sgx/linux/installer/bin/sgxsdk/environment  &&\
    echo -e "\n##### Intel SGX SDK\nsource /opt/linux-sgx/linux/installer/bin/sgxsdk/environment\n" >> ~/.bashrc
COPY . /opt/sfusegx/
WORKDIR /opt/sfusegx
RUN make SGX_MODE=HW SGX_PRERELEASE=1
