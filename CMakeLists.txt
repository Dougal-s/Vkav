cmake_minimum_required(VERSION 3.10)

project(Vkav VERSION 0.1)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(
  "-Wall" "-Wextra"
  "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb>"
  "$<$<CONFIG:RELEASE>:-O3;-march=native;-DNDEBUG>"
)

if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
	add_definitions(-DLINUX)
	if (NOT DEFINED AUDIO_BACKEND)
		set(AUDIO_BACKEND "PULSEAUDIO")
	endif()
elseif (${CMAKE_SYSTEM_NAME} MATCHES Windows)
	add_definitions(-DWINDOWS)
	if (NOT DEFINED AUDIO_BACKEND)
		set(AUDIO_BACKEND "WASAPI")
	endif()
elseif (${CMAKE_SYSTEM_NAME} MATCHES Darwin)
	add_definitions(-DMACOS)
	if (NOT DEFINED AUDIO_BACKEND)
		set(AUDIO_BACKEND "COREAUDIO")
	endif()
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")

add_library(audioModule src/CoreAudio.cpp src/PulseAudio.cpp src/PulseAudioAsync.cpp src/WASAPI.cpp)
if (${AUDIO_BACKEND} MATCHES COREAUDIO)
	add_definitions(-DCOREAUDIO)
	find_library(coreAudio CoreAudio)
	if (NOT coreAudio)
		message(FATAL_ERROR "Unable to locate CoreAudio!")
	endif()
	target_link_libraries(audioModule ${coreAudio})
elseif (${AUDIO_BACKEND} MATCHES PULSEAUDIO)
	add_definitions(-DPULSEAUDIO)
	find_library(pulseAudio pulse)
	if (NOT pulseAudio)
		message(FATAL_ERROR "Unable to locate pulseaudio!")
	endif()
	find_library(pulseAudioSimple pulse-simple)
	if (NOT pulseAudioSimple)
		message(FATAL_ERROR "Unable to locate pulseaudio-simple!")
	endif()
	target_link_libraries(audioModule ${pulseAudio} ${pulseAudioSimple})
elseif (${AUDIO_BACKEND} MATCHES WASAPI)
	add_definitions(-DWASAPI)
	find_library(WASAPI wasapi)
	if (NOT WASAPI)
		message(FATAL_ERROR "Unable to locate WASAPI!")
	endif()
	target_link_libraries(audioModule ${WASAPI})
endif()
target_link_libraries(audioModule -lpthread)
target_include_directories(audioModule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(graphicsModule src/Render.cpp src/Image.cpp)
find_library(GLFW glfw)
target_link_libraries(graphicsModule ${GLFW})
find_package(Vulkan REQUIRED)
target_link_libraries(graphicsModule Vulkan::Vulkan)
find_package(PNG)
if (NOT ${PNG_FOUND})
	add_definitions(-DDISABLE_PNG)
else()
	target_link_libraries(graphicsModule PNG::PNG)
endif()
find_package(JPEG)
if (NOT ${JPEG_FOUND})
	add_definitions(-DDISABLE_JPEG)
else()
	target_link_libraries(graphicsModule JPEG::JPEG)
endif()
target_link_libraries(graphicsModule -lstdc++fs)
target_include_directories(graphicsModule PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(Vkav src/Vkav.cpp src/Proccess.cpp src/Settings.cpp src/Render.cpp src/Image.cpp src/Data.cpp)
target_include_directories(Vkav PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(Vkav audioModule)
target_link_libraries(Vkav graphicsModule)
target_link_libraries(Vkav -lstdc++fs)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES Windows)
	add_custom_command(TARGET Vkav POST_BUILD COMMAND strip --strip-all Vkav)
endif()

# Installer

if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
	install(TARGETS Vkav # executable
			CONFIGURATIONS Release
			RUNTIME DESTINATION bin)
	install(FILES src/config # config file
			CONFIGURATIONS Release
			TYPE SYSCONF
			DESTINATION /etc/Vkav)
	install(DIRECTORY src/shaders # shaders
			CONFIGURATIONS Release
			TYPE SYSCONF
			DESTINATION /etc/Vkav)
	install(FILES Vkav.desktop # .desktop file
			CONFIGURATIONS Release
			DESTINATION /usr/share/applications)
	install(FILES Vkav.png # icon
			CONFIGURATIONS Release
			DESTINATION /usr/share/icons/hicolor/48x48/apps)
	install(FILES Vkav.svg # icon
			CONFIGURATIONS Release
			DESTINATION /usr/share/icons/hicolor/scalable/apps)
elseif (${CMAKE_SYSTEM_NAME} MATCHES Windows)
elseif (${CMAKE_SYSTEM_NAME} MATCHES Darwin)
endif()

# For formatting source code
file(GLOB src
    "*.h"
    "*.cpp"
)

add_custom_target(format clang-format -style=file -i ${src})

set(VULKAN_SDK_LIB_PATH "/usr/lib/x86_64-linux-gnu")
set(VULKAN_SDK_LAYER_PATH "/usr/share/vulkan/explicit_layer.d")
add_custom_target(run LD_LIBRARY_PATH=${VULKAN_SDK_LIB_PATH} VK_LAYER_PATH=${VULKAN_SDK_LAYER_PATH} ./Vkav)
