
cmake_minimum_required(VERSION 3.18)
project(golden-player
        VERSION 0.1.0
        LANGUAGES CXX)

include_directories(${CMAKE_BINARY_DIR})
configure_file("${CMAKE_SOURCE_DIR}/version.h.in" "${CMAKE_BINARY_DIR}/version.h" @ONLY)

# submodule warning
message(" ")
message("Note:  this project uses git submodules in the source tree.")
message("       if you haven't already, run the following command from")
message("       the project's root directory:")
message(" ")
message("           git submodule update --init") 
message("\n")

# setup build flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall")    # -std=c++17
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE")
set(BUILD_DEPS "YES" CACHE BOOL "If YES, will install dependencies into sandbox.  Automatically reset to NO after dependencies are installed.")
set(BUILD_INTERACTIVE "YES" CACHE BOOL "If NO, will download/install the default DNN models without prompting the user, and skip installation of PyTorch.")
set(BUILD_EXPERIMENTAL "NO" CACHE BOOL "If YES, will enable support for experimental DNNs, examples, and plugins")


# copy configuration tools to build dir


# if this is the first time running cmake, perform pre-build dependency install script (or if the user manually triggers re-building the dependencies)
if( ${BUILD_DEPS} )
    message("-- Launching pre-build dependency installer script...")
    message("-- Build interactive:  ${BUILD_INTERACTIVE}")

    # execute_process(COMMAND sh ../CMakePreBuild.sh ${BUILD_INTERACTIVE}
    #             WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    #             RESULT_VARIABLE PREBUILD_SCRIPT_RESULT)

    set(BUILD_DEPS "NO" CACHE BOOL "If YES, will install dependencies into sandbox.  Automatically reset to NO after dependencies are installed." FORCE)
    message("-- Finished installing dependencies")
endif()


# setup CUDA
find_package(CUDAToolkit)
set(CMAKE_CUDA_ARCHITECTURES 53 62 72)


# setup project output paths
set(PROJECT_OUTPUT_DIR  ${PROJECT_BINARY_DIR}/${CMAKE_SYSTEM_PROCESSOR})
set(PROJECT_INCLUDE_DIR ${PROJECT_OUTPUT_DIR}/include)

file(MAKE_DIRECTORY ${PROJECT_INCLUDE_DIR})
file(MAKE_DIRECTORY ${PROJECT_OUTPUT_DIR}/bin)

message("-- system arch:  ${CMAKE_SYSTEM_PROCESSOR}")
message("-- output path:  ${PROJECT_OUTPUT_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_OUTPUT_DIR}/lib)


# Use absolute path for better access from everywhere
set(THIRDPARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
set(CLASS_DIR ${THIRDPARTY_DIR}/tegra_multimedia_api/samples/common/classes)
set(ALGO_CUDA_DIR ${THIRDPARTY_DIR}/tegra_multimedia_api/samples/common/algorithm/cuda)
set(ALGO_TRT_DIR ${THIRDPARTY_DIR}/tegra_multimedia_api/samples/common/algorithm/trt)


# transfer all headers to the include directory
file(MAKE_DIRECTORY ${PROJECT_INCLUDE_DIR}/golden-player)

file(GLOB gplayerIncludes include/*.h)

foreach(include ${gplayerIncludes})
    message("-- Copying ${include} to ${PROJECT_INCLUDE_DIR}/golden-player")
    configure_file(${include} ${PROJECT_INCLUDE_DIR}/golden-player COPYONLY)
endforeach()


# build subdirectories
add_subdirectory(src)
add_subdirectory(examples)


# install includes
foreach(include ${gplayerIncludes})
    install(FILES "${include}" DESTINATION include/golden-player)
endforeach()

# install symlink to networks and images
install(CODE "execute_process( COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/data/networks ${CMAKE_INSTALL_PREFIX}/bin/networks )" )
install(CODE "execute_process( COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/data/images ${CMAKE_INSTALL_PREFIX}/bin/images )" )

# install the shared library
install(TARGETS golden-player DESTINATION lib EXPORT gplayerConfig)

# install the cmake project, for importing
install(EXPORT gplayerConfig DESTINATION share/golden-player/cmake)

# run ldconfig after installing
if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL ${CMAKE_HOST_SYSTEM_PROCESSOR})
    install(CODE "execute_process( COMMAND ldconfig )")
endif()
