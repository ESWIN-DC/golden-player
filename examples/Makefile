include ../Rules.mk

APP := gplayer gplayer-h264 gplayer-mjpeg gplayer-filesrc-h264

ARGUS_UTILS_DIR := $(TOP_DIR)/third_party/tegra_multimedia_api-R32.2.3/argus/samples/utils

CPPFLAGS += \
	-I"$(ARGUS_UTILS_DIR)" \
	-I"$(TOP_DIR)/include"

LDFLAGS += \
	-L"$(TOP_DIR)/src" \
	-L"$(TOP_DIR)/third_party/spdlog/build" \
	-lgolden-player \
	-lspdlog

LDFLAGS += \
	-lnveglstream_camconsumer -lnvargus_socketclient	

SRCS := \
	gplayer.cpp \
	gplayer-h264.cpp \
	gplayer-mjpeg.cpp \
	gplayer-filesrc-h264.cpp \

OBJS := $(SRCS:.cpp=.o)

all: $(APP)

%.o: %.cpp
	@echo "Compiling: $<"
	$(CPP) $(CPPFLAGS) -c $<

gplayer: gplayer.o ../src/libgolden-player.a
	@echo "Linking: $@"
	$(CPP) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

gplayer-h264: gplayer-h264.o ../src/libgolden-player.a
	@echo "Linking: $@"
	$(CPP) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

gplayer-mjpeg:  gplayer-mjpeg.o ../src/libgolden-player.a
	@echo "Linking: $@"	
	$(CPP) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

gplayer-filesrc-h264:  gplayer-filesrc-h264.o ../src/libgolden-player.a
	@echo "Linking: $@"	
	$(CPP) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

install: $(APP)
	@echo "Installing: $(APP)"
	[ -d "$(INSTALL_DIR)/bin" ] || mkdir -p "$(INSTALL_DIR)/bin"
	cp $(APP) $(INSTALL_DIR)/bin

clean:
	$(AT)rm -rf $(APP) $(OBJS)
