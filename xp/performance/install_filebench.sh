#!/usr/bin/env bash

# Disabling ASLR if needed
if [[ "$(grep 0 /proc/sys/kernel/randomize_va_space --count)" -eq "0" ]]; then
    echo 0 | tee /proc/sys/kernel/randomize_va_space
fi

# Install filebench
apt-get update &&\
apt-get install autoconf automake build-essential byacc flex libtool make wget --yes --quiet
cd /opt
wget https://github.com/filebench/filebench/archive/1.5-alpha3.tar.gz --quiet &&\
tar xaf 1.5-alpha3.tar.gz &&\
cd filebench-1.5-alpha3 &&\
libtoolize &&\
aclocal &&\
autoheader &&\
automake --add-missing &&\
autoconf &&\
./configure &&\
make &&\
make install