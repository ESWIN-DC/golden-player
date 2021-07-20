# Golden Player

A C++ multimedia framework for NVIDIA® Jetson modules.

## Prerequisites

To pull the source code. You can clone the source code by following commands.

```bash
git clone https://github.com/ESWIN-DC/golden-player
cd golden-player
git submodule update --init --recursive
```

Or, If you have a github account.

```bash
git clone git@github.com:ESWIN-DC/golden-player.git
cd golden-player
git submodule update --init --recursive
```

## How to build

### 1. Set environment variables

Golden player supports cross compilation and native compilation. You must update the environment variables. For to fit your requirements, please modify the script text in the "scripts/env.sh".

```bash
source ./scripts/env.sh
```

The upward script needs a target "TARGET_ROOTFS", which is the jetson system image. For more information, please refer to the jetson online documentation: https://docs.nvidia.com/jetson/l4t-multimedia/cross_platform_support.html.

### 2. Build thirdparties

```bash
./scripts/build-thirdparty.sh
```

### 3. Build

Golden Player supports both make and cmake, but we will deprecate make in the future, and move to cmake. The following is the usage of cmake.

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug  -DCMAKE_TOOLCHAIN_FILE="../scripts/arm64-cross.cmake" ..
make -j 8
```

The above is to build the debug version, you can change the cmake option to build the release version.

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release  -DCMAKE_TOOLCHAIN_FILE="../scripts/arm64-cross.cmake" ..
make -j 8
```

## Deploy

The default installation directory is "/usr".

```bash
cd build
make install
```

You can change the default installation directory by running the following commands.

```bash
cd build
cmake --install . --prefix "your prefix path"
```
