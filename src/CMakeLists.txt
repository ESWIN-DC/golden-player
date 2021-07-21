
set(ARGUS_UTILS_DIR ${THIRDPARTY_DIR}/tegra_multimedia_api/argus/samples/utils)
set(CUDA_PATH /usr/local/cuda)

set(TEGRA_ARMABI aarch64-linux-gnu)

include_directories(
    # ${PROJECT_INCLUDE_DIR}
    ../include
    "${THIRDPARTY_DIR}/spdlog/include"
    "${THIRDPARTY_DIR}/json/include"
    "${THIRDPARTY_DIR}/asio/asio/include"
    "${THIRDPARTY_DIR}/tegra_multimedia_api/include"
    "${THIRDPARTY_DIR}/tegra_multimedia_api/argus/include"
    "${THIRDPARTY_DIR}/tegra_multimedia_api/include/libjpeg-8b"
    "${ALGO_CUDA_DIR}"
    "${ALGO_TRT_DIR}"
    "${ARGUS_UTILS_DIR}"
    "${CMAKE_SYSROOT}/${CUDA_PATH}/include"
    "${CMAKE_SYSROOT}/usr/include/${TEGRA_ARMABI}"
    "${CMAKE_SYSROOT}/usr/include/libdrm"
    "${CMAKE_SYSROOT}/usr/include/opencv4")

file(GLOB tmmSources ${CLASS_DIR}/*.cpp)

add_custom_command(
    OUTPUT ${ALGO_CUDA_DIR}/NvAnalysis.o
    OUTPUT ${ALGO_CUDA_DIR}/NvCudaProc.o
    COMMAND make -C ${ALGO_CUDA_DIR}
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    COMMENT "Build CUDA ..."
)

add_library(golden-player STATIC
    ${tmmSources}
    ${ALGO_CUDA_DIR}/NvAnalysis.o
    ${ALGO_CUDA_DIR}/NvCudaProc.o
    ${ARGUS_UTILS_DIR}/Thread.cpp
    ${ARGUS_UTILS_DIR}/NativeBuffer.cpp
    ${ARGUS_UTILS_DIR}/nvmmapi/NvNativeBuffer.cpp
    gp_beader.cpp
    gp_threadpool.cpp
    gp_configuration.cpp
    video_decode_context.cpp
    video_encode_context.cpp
    gp_nvvideo_decoder.cpp
    gp_video_decoder_group.cpp
    gp_nvvideo_encoder.cpp
    camera_recorder.cpp
    gp_camera_v4l2.cpp
    gp_display_egl.cpp
    gp_filesink.cpp
    gp_filesrc.cpp
    gp_socket_server.cpp
    gp_socket_client.cpp
    gp_pipeline.cpp)

install(TARGETS golden-player DESTINATION lib)
