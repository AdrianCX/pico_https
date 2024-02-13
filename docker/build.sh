#!/bin/bash -ex

cd /source/build_docker
cmake -DCMAKE_BUILD_TYPE=Release ../
make
cd hello_world
make

mkdir -p /source/build_docker_test
cd /source/build_docker_test
cmake -DCMAKE_BUILD_TYPE=Release ../test/
make
ctest --verbose