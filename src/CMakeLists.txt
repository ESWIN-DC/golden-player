
set(ARGUS_UTILS_DIR ${THIRDPARTY_DIR}/tegra_multimedia_api-R32.2.3/argus/samples/utils)
set(CUDA_PATH /usr/local/cuda)

set(TEGRA_ARMABI aarch64-linux-gnu)

include_directories(
    # ${PROJECT_INCLUDE_DIR}
    ../include
	"${THIRDPARTY_DIR}/spdlog/include" 
	"${THIRDPARTY_DIR}/json/include" 
	"${THIRDPARTY_DIR}/tegra_multimedia_api-R32.2.3/include"
    "${THIRDPARTY_DIR}/tegra_multimedia_api-R32.2.3/argus/include"
	"${THIRDPARTY_DIR}/tegra_multimedia_api-R32.2.3/include/libjpeg-8b" 
	"${ALGO_CUDA_DIR}" 
	"${ALGO_TRT_DIR}"
    "${ARGUS_UTILS_DIR}"
	"${CMAKE_SYSROOT}/${CUDA_PATH}/include" 
	"${CMAKE_SYSROOT}/usr/include/$(TEGRA_ARMABI)" 
	"${CMAKE_SYSROOT}/usr/include/libdrm" 
	"${CMAKE_SYSROOT}/usr/include/opencv4")

file(GLOB tmmSources ${CLASS_DIR}/*.cpp)

add_library(ccccc STATIC
	${ALGO_CUDA_DIR}/NvCudaProc.cpp
	${ALGO_CUDA_DIR}/NvAnalysis.cu
)

add_library(golden-player STATIC
	${tmmSources}
	${ARGUS_UTILS_DIR}/Thread.cpp
	${ARGUS_UTILS_DIR}/NativeBuffer.cpp
	${ARGUS_UTILS_DIR}/nvmmapi/NvNativeBuffer.cpp
    video_decode_context.cpp
	video_encode_context.cpp
	video_decoder.cpp
	video_encoder.cpp
	camera_recorder.cpp
	camera_v4l2.cpp
	display.cpp
	display_egl.cpp
	gp_file.cpp
	pipeline.cpp)

install(TARGETS golden-player DESTINATION lib)
