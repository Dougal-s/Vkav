src = $(wildcard *.cpp)
hdr = $(wildcard *.hpp)
obj = $(src:.cpp=.o)

VULKAN_SDK_PATH = 
VULKAN_SDK_INCLUDE = $(VULKAN_SDK_PATH)/include
VULKAN_SDK_LIB_PATH = $(VULKAN_SDK_PATH)/lib
VULKAN_SDK_LAYER_PATH = $(VULKAN_SDK_PATH)/etc/explicit_layer.d

CFLAGS = -std=c++17 -I$(VULKAN_SDK_INCLUDE)
LDFLAGS = -lglfw -lvulkan -lpulse -lpulse-simple -lpthread -lstdc++fs

ifeq ($(OS),Windows_NT)
    AUDIO_BACKEND ?= WASAPI
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        AUDIO_BACKEND ?= PULSEAUDIO
    endif
    ifeq ($(UNAME_S),Darwin)
        AUDIO_BACKEND ?= COREAUDIO
    endif
endif

CFLAGS += -D $(AUDIO_BACKEND)

ifdef DISABLE_PNG
	CFLAGS += -D DISABLE_PNG
else
	LDFLAGS += -lpng
endif

ifdef DISABLE_JPEG
	CFLAGS += -D DISABLE_JPEG
else
	LDFLAGS += -ljpeg
endif

ifeq ($(BUILD),debug)
	CFLAGS += -O0 -Wall -ggdb
else
	CFLAGS += -DNDEBUG -O3 -march=native
	STRIP = strip --strip-all Vkav
endif

all: compile run

format:
	clang-format -style=file -i $(src) $(hdr)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $^

compile: $(obj)
	$(CXX) $(CFLAGS) -o Vkav $(obj) $(LDFLAGS)
	$(STRIP)

run:
	LD_LIBRARY_PATH=$(VULKAN_SDK_LIB_PATH) VK_LAYER_PATH=$(VULKAN_SDK_LAYER_PATH) ./Vkav -v

clean:
	rm -f Vkav $(obj)
