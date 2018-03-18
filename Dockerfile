FROM ubuntu:16.04

RUN apt-get update &&\
    apt-get dist-upgrade --yes --quiet &&\
    apt-get install --yes --quiet build-essential curl git ninja-build pkg-config python3-pip udev wget
WORKDIR /opt
RUN pip3 install --upgrade pip &&\
    pip3 install meson &&\
    wget https://github.com/libfuse/libfuse/archive/fuse-3.0.2.tar.gz --quiet &&\
    tar xaf fuse-3.0.2.tar.gz &&\
    cd libfuse-fuse-3.0.2 &&\
    mkdir build &&\
    meson .. &&\
    cd .. &&\
    ninja &&\
    ninja install
COPY . /opt/sfusegx
WORKDIR /opt/sfusegx
RUN ldconfig && make
ENV LD_LIBRARY_PATH "/usr/lib:/usr/local/lib:/usr/local/lib/x86_64-linux-gnu/"