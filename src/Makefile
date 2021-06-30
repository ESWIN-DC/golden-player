
include ../Rules.mk

APP := libgplayer.a

CPPFLAGS += \
	-Wall -O1 -g\
	-I"$(TOP_DIR)/include"

SRCS := \
	video_decode_context.cpp \
	video_encode_context.cpp \
	video_decoder.cpp \
	video_encoder.cpp \
	camera_recorder.cpp \
	display.cpp \
	pipeline.cpp \
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

$(CLASS_DIR)/%.o: $(CLASS_DIR)/%.cpp
	$(AT)$(MAKE) -C $(CLASS_DIR)

%.o: %.cpp
	@echo "Compiling: $<"
	$(CPP) $(CPPFLAGS) -c $< -o $@

$(APP): $(OBJS)
	@echo "Linking: $@"
	$(AR) rcs $@ $(OBJS) 

clean:
	$(AT)rm -rf $(APP) $(OBJS)
