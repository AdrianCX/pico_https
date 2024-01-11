#!/bin/bash -ex

cd /source/build_docker
cmake -DCMAKE_BUILD_TYPE=Release ../
cd hello_world
make