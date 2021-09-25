
set(ARGUS_UTILS_DIR ${THIRDPARTY_DIR}/tegra_multimedia_api/argus/samples/utils)
set(CUDA_PATH /usr/local/cuda)

set(TEGRA_ARMABI aarch64-linux-gnu)

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE) 
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

include_directories(
    ../include
	"${THIRDPARTY_DIR}/tegra_multimedia_api/include"
    "${THIRDPARTY_DIR}/tegra_multimedia_api/argus/include"
    "${THIRDPARTY_DIR}/tegra_multimedia_api/include/libjpeg-8b"
	"${THIRDPARTY_DIR}/asio/asio/include"
)

set(CMAKE_INSTALL_RPATH
	"${CMAKE_SYSROOT}/lib/${TEGRA_ARMABI}"
	"${CMAKE_SYSROOT}/usr/lib/${TEGRA_ARMABI}"
	"${CMAKE_SYSROOT}/usr/lib/${TEGRA_ARMABI}/tegra"
	"${CMAKE_SYSROOT}/${CUDA_PATH}/lib64"
)

link_directories(
    "${CMAKE_SYSROOT}/${CUDA_PATH}/lib64"
	"${CMAKE_SYSROOT}/usr/lib/${TEGRA_ARMABI}"
	"${CMAKE_SYSROOT}/usr/lib/${TEGRA_ARMABI}/tegra"
	"${CMAKE_SYSROOT}/usr/lib/${TEGRA_ARMABI}/tegra-egl")

link_directories(
	"${THIRDPARTY_DIR}/spdlog/build"
)

add_executable(gplayer 
    gplayer.cpp)

target_link_libraries(gplayer
    golden-player
    pthread v4l2 EGL GLESv2 X11
	nvbuf_utils nvjpeg nvosd drm
	cuda cudart
	nvinfer nvparsers
    spdlog
	nveglstream_camconsumer
	nvargus_socketclient)

add_executable(gplayer-h264 
    gplayer-h264.cpp)

target_link_libraries(gplayer-h264
    golden-player
    pthread v4l2 EGL GLESv2 X11
	nvbuf_utils nvjpeg nvosd drm
	cuda cudart
	nvinfer nvparsers
    spdlog
	nveglstream_camconsumer
	nvargus_socketclient)

add_executable(gplayer-mjpeg
    gplayer-mjpeg.cpp)

target_link_libraries(gplayer-mjpeg
    golden-player
    pthread v4l2 EGL GLESv2 X11
	nvbuf_utils nvjpeg nvosd drm
	cuda cudart
	nvinfer nvparsers
    spdlog
	nveglstream_camconsumer
	nvargus_socketclient)

add_executable(gplayer-filesrc-h264
    gplayer-filesrc-h264.cpp)

target_link_libraries(gplayer-filesrc-h264
    golden-player
    pthread v4l2 EGL GLESv2 X11
	nvbuf_utils nvjpeg nvosd drm
	cuda cudart
	nvinfer nvparsers
    spdlog
	nveglstream_camconsumer
	nvargus_socketclient)

add_executable(gplayer-multiple-display
    gplayer-multiple-display.cpp)

target_link_libraries(gplayer-multiple-display
    golden-player
    pthread v4l2 EGL GLESv2 X11
	nvbuf_utils nvjpeg nvosd drm
	cuda cudart
	nvinfer nvparsers
    spdlog
	nveglstream_camconsumer
	nvargus_socketclient)

add_executable(gplayer-multiple-filesrc
    gplayer-multiple-filesrc.cpp)

target_link_libraries(gplayer-multiple-filesrc
    golden-player
    pthread v4l2 EGL GLESv2 X11
	nvbuf_utils nvjpeg nvosd drm
	cuda cudart
	nvinfer nvparsers
    spdlog
	nveglstream_camconsumer
	nvargus_socketclient)

add_executable(gplayer-socket-server
    gplayer-socket-server.cpp)

target_link_libraries(gplayer-socket-server
    golden-player
    pthread v4l2 EGL GLESv2 X11
	nvbuf_utils nvjpeg nvosd drm
	cuda cudart
	nvinfer nvparsers
    spdlog
	nveglstream_camconsumer
	nvargus_socketclient)	

install(TARGETS gplayer DESTINATION bin)
