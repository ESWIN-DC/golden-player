
include ../Rules.mk

APP := libgolden-player.a

CPPFLAGS += \
	-Wall -O0 -g\
	-I"$(TOP_DIR)/include"

SRCS := \
	gp_beader.cpp \
	gp_threadpool.cpp \
	gp_configuration.cpp \
	video_decode_context.cpp \
	video_encode_context.cpp \
	gp_nvvideo_decoder.cpp \
	gp_nvvideo_encoder.cpp \
	camera_recorder.cpp \
	gp_camera_v4l2.cpp \
	display.cpp \
	gp_display_egl.cpp \
	gp_filesink.cpp \
	gp_filesrc.cpp \
	gp_pipeline.cpp \
	$(wildcard $(CLASS_DIR)/*.cpp)

# argus_camera.cpp \

ARGUS_UTILS_DIR := $(TOP_DIR)/third_party/tegra_multimedia_api-R32.2.3/argus/samples/utils

SRCS += \
	$(ARGUS_UTILS_DIR)/Thread.cpp \
	$(ARGUS_UTILS_DIR)/NativeBuffer.cpp \
	$(ARGUS_UTILS_DIR)/nvmmapi/NvNativeBuffer.cpp

CPPFLAGS += \
	-I"$(ARGUS_UTILS_DIR)"

LDFLAGS += \
	-lnveglstream_camconsumer -lnvargus_socketclient

OBJS := $(SRCS:.cpp=.o)

all: $(APP)

OBJS += \
	$(ALGO_CUDA_DIR)/NvAnalysis.o \
	$(ALGO_CUDA_DIR)/NvCudaProc.o

$(CLASS_DIR)/%.o: $(CLASS_DIR)/%.cpp
	$(AT)$(MAKE) -C $(CLASS_DIR)

$(ALGO_CUDA_DIR)/%.o: $(ALGO_CUDA_DIR)/%.cpp
	$(AT)$(MAKE) -C $(ALGO_CUDA_DIR)

$(ALGO_CUDA_DIR)/%.o: $(ALGO_CUDA_DIR)/%.cu
	$(AT)$(MAKE) -C $(ALGO_CUDA_DIR)

%.o: %.cpp $(wildcard $(TOP_DIR)/include/*.h)
	@echo "Compiling: $<"
	$(CPP) $(CPPFLAGS) -c $< -o $@

$(APP): $(OBJS)
	@echo "Linking: $@"
	$(AR) rcs $@ $(OBJS) 

install: $(APP)
	[ -d "$(INSTALL_DIR)/include" ] || mkdir -p "$(INSTALL_DIR)/include"
	[ -d "$(INSTALL_DIR)/lib" ] || mkdir -p "$(INSTALL_DIR)/lib"
	cp $(TOP_DIR)/include/* $(INSTALL_DIR)/include
	cp $(APP) $(INSTALL_DIR)/lib

clean:
	$(AT)rm -rf $(APP) $(OBJS) $(INSTALL_DIR)
