#!/bin/bash

if [ "$EUID" -ne 0 ]; then
    echo "need root privileges. try sudo."
    exit
fi

if [ $(dirname $0) = . ]; then
    # was probably run from scripts dir, try to go back into repo root
    cd ..
fi

if [ ! -f intro ]; then
    sudo -u ${SUDO_USER} -- make config || exit 1
fi

PREFIX=/usr/local
set -x

mkdir -p $PREFIX/bin
cp -f intro $PREFIX/bin/intro
chmod 755 $PREFIX/bin/intro # read/execute

mkdir -p /etc/introcity
cp -f intro.cfg /etc/introcity/intro.cfg
chmod 744 /etc/introcity/intro.cfg # read
