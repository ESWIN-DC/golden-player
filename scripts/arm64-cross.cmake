set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm64)

set(CMAKE_SYSROOT /sources/public/jetson/mnt)
set(CMAKE_STAGING_PREFIX /usr/)

set(tools /sources/public/jetson/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu)
set(CMAKE_C_COMPILER ${tools}/bin/aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER ${tools}/bin/aarch64-linux-gnu-g++)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_CUDA_COMPILER "/usr/local/cuda/bin/nvcc")
set(
    CMAKE_CUDA_FLAGS
    ${CMAKE_NVCC_FLAGS}
    "-ccbin ${CMAKE_CXX_COMPILER}"
)