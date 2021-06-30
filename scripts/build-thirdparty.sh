#!/bin/bash

BUILD_DIR="third_party/spdlog/build"

[ -d "$BUILD_DIR" ] || mkdir "$BUILD_DIR"
cd "$BUILD_DIR"
cmake -DCMAKE_TOOLCHAIN_FILE="/sources/public/jetson/golden-player/scripts/arm64-cross.cmake" ..
cmake --build .
