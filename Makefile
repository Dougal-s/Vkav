src = $(wildcard *.cpp)
obj = $(src:.cpp=.o)

VULKAN_SDK_PATH =

CFLAGS = -std=c++17 -I$(VULKAN_SDK_PATH)/include
LDFLAGS = -lglfw -lvulkan -lpulse -lpulse-simple -lpthread -lstdc++fs

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

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $^

compile: $(obj)
	$(CXX) $(CFLAGS) -o Vkav $(obj) $(LDFLAGS)
	$(STRIP)

run:
	LD_LIBRARY_PATH=$(VULKAN_SDK_PATH)/lib VK_LAYER_PATH=$(VULKAN_SDK_PATH)/etc/explicit_layer.d ./Vkav -v

clean:
	rm -f Vkav $(obj)
