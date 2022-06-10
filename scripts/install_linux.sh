#!/bin/bash

if [ "$EUID" -ne 0 ]; then
    echo "need root privileges. try sudo."
    exit 1
fi

if [ $(dirname $0) = . ]; then
    # was probably run from scripts dir, try to go back into repo root
    cd ..
fi

# git doesn't like to be run as super user, plus doing so would create protected files

sudo -u ${SUDO_USER} -- make config || exit 1
sudo -u ${SUDO_USER} -- make release || exit 1

PREFIX=/usr/local
set -x

mkdir -p $PREFIX/bin
cp -f build/release/intro $PREFIX/bin/intro
chmod 755 $PREFIX/bin/intro # read/execute

mkdir -p /etc/introcity
cp -f intro.cfg /etc/introcity/intro.cfg
chmod 744 /etc/introcity/intro.cfg # read
