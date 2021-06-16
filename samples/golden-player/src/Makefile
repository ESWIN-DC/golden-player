
include ../../Rules.mk

CPPFLAGS += -I"../include"

APP := libgplayer.a

SRCS := \
	csvparser.cpp \
	video_decoder.cpp \
	$(wildcard $(CLASS_DIR)/*.cpp)

OBJS := $(SRCS:.cpp=.o)

all: $(APP)

$(CLASS_DIR)/%.o: $(CLASS_DIR)/%.cpp
	$(AT)$(MAKE) -C $(CLASS_DIR)

%.o: %.cpp
	@echo "Compiling: $<"
	$(CPP) $(CPPFLAGS) -c $<

$(APP): $(OBJS)
	@echo "Linking: $@"
	$(AR) rcs $@ $(OBJS) 

clean:
	$(AT)rm -rf $(APP) $(OBJS)
