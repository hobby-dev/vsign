#!/bin/sh

set -eu

CXX=${CXX:-clang++}

mkdir -p build
${CXX} $* src/main.cpp src/portable-memory-mapping/MemoryMapped.cpp -O3 -std=c++14 -mavx2 -maes -pthread -fstack-protector -fstack-protector-all -Wall -Wpedantic -Wextra -Werror -Weffc++ -Wswitch-default -Wstack-protector -Wpadded -Wno-unused-function -Wdisabled-optimization -o build/vsign

