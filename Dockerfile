FROM ubuntu:16.04

RUN apt-get update &&\
    apt-get dist-upgrade --yes --quiet &&\
    apt-get install --yes --quiet clang libfuse-dev pkg-config
COPY . /opt/sfusegx/
WORKDIR /opt/sfusegx
RUN ldconfig && make