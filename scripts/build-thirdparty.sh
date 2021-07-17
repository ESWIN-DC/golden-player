#!/bin/bash

SCRIPT_HOME="$(dirname "$( readlink -m $( type -p ${0} ))")"

BUILD_DIR="${SCRIPT_HOME}/../third_party/spdlog/build"
[ -d "$BUILD_DIR" ] || mkdir "$BUILD_DIR"
cd "$BUILD_DIR"
cmake -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_HOME}/arm64-cross.cmake" ..
cmake --build .
