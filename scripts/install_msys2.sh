#!/bin/bash

if [ $(dirname $0) = . ]; then
    # was probably run from scripts dir, try to go back into repo root
    cd ..
fi

if [ ! -f intro.cfg ]; then
    make config || exit 1
fi

if [ ! -f build/release/intro ]; then
    make release || exit 1
fi

PREFIX=/usr/local
set -x

mkdir -p $PREFIX/bin
cp -f build/release/intro $PREFIX/bin/intro

mkdir -p /etc/introcity
cp -f intro.cfg /etc/introcity/intro.cfg
