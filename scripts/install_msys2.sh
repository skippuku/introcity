#!/bin/bash

if [ $(dirname $0) = . ]; then
    # was probably run from scripts dir, try to go back into repo root
    cd ..
fi

make config || exit 1
make release || exit 1

PREFIX=/usr/local
set -x

mkdir -p $PREFIX/bin
cp -f build/release/intro $PREFIX/bin/intro

mkdir -p /etc/introcity
cp -f intro.cfg /etc/introcity/intro.cfg
