include ../../Rules.mk

APP := gplayer

ARGUS_UTILS_DIR := $(TOP_DIR)/argus/samples/utils

CPPFLAGS += \
	-I"$(ARGUS_UTILS_DIR)" \
	-I"$(TOP_DIR)/samples/golden_player/include"

LDFLAGS += \
	-L"../src" \
	-lgplayer

LDFLAGS += \
	-lnveglstream_camconsumer -lnvargus_socketclient	

SRCS := \
	gplayer.cpp \

OBJS := $(SRCS:.cpp=.o)

all: $(APP)

%.o: %.cpp
	@echo "Compiling: $<"
	$(CPP) $(CPPFLAGS) -c $<

$(APP): $(OBJS)
	@echo "Linking: $@"
	$(CPP) -o $@ $(OBJS) $(CPPFLAGS) $(LDFLAGS)

clean:
	$(AT)rm -rf $(APP) $(OBJS)
