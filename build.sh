#!/bin/bash -ex

pushd $(dirname "$0")

if [ ! -d build ]; then
    mkdir build
fi

pushd build
cmake -DCMAKE_BUILD_TYPE=Release ../

pushd hello_world
make
popd
popd
popd