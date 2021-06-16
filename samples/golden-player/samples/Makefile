include ../../Rules.mk

APP := gplayer

CPPFLAGS += -I"../include"
LDFLAGS += \
	-L"../src" \
	-lgplayer

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
