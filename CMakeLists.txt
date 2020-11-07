cmake_minimum_required(VERSION 3.12)

project(Vkav VERSION 0.5.0 LANGUAGES CXX)

configure_file(include/Version.hpp.in Version.hpp)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(
  "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb;-Wall;-Wextra>"
  "$<$<CONFIG:ASAN>:-O0;-g3;-ggdb;-Wall;-Wextra;-fno-omit-frame-pointer;-fsanitize=address>"
  "$<$<CONFIG:RELEASE>:-O3;-march=native;-DNDEBUG>"
)

add_link_options(
  "$<$<CONFIG:ASAN>:-fno-omit-frame-pointer;-fsanitize=address>"
)

# Platform specific flags
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
		set(AUDIO_BACKEND "LIBSOUNDIO")
	endif()
endif()

if (NOT DEFINED AUDIO_BACKEND)
	set(AUDIO_BACKEND "LIBSOUNDIO")
endif()


add_library(audioModule
	src/CoreAudio.cpp
	src/PulseAudio.cpp
	src/PulseAudioAsync.cpp
	src/WASAPI.cpp
	src/libsoundio.cpp
)
target_include_directories(audioModule PRIVATE include)

if (${AUDIO_BACKEND} MATCHES COREAUDIO)
	target_compile_definitions(audioModule PRIVATE -DCOREAUDIO)
	find_library(coreAudio CoreAudio)
	if (NOT coreAudio)
		message(FATAL_ERROR "Unable to locate CoreAudio!")
	endif()
	target_link_libraries(audioModule PRIVATE ${coreAudio})
elseif (${AUDIO_BACKEND} MATCHES PULSEAUDIO)
	find_library(pulseAudio pulse)
	if (NOT pulseAudio)
		message(FATAL_ERROR "Unable to locate pulseaudio!")
	endif()
	find_library(pulseAudioSimple pulse-simple)
	if (pulseAudioSimple)
		target_compile_definitions(audioModule PRIVATE -DPULSEAUDIO)
		target_link_libraries(audioModule PRIVATE ${pulseAudioSimple} -lpthread)
	else()
		target_compile_definitions(audioModule PRIVATE -DPULSEAUDIO_ASYNC)
	endif()
	target_link_libraries(audioModule PRIVATE ${pulseAudio})
elseif (${AUDIO_BACKEND} MATCHES WASAPI)
	target_compile_definitions(audioModule PRIVATE -DWASAPI)
	find_library(WASAPI wasapi)
	if (NOT WASAPI)
		message(FATAL_ERROR "Unable to locate WASAPI!")
	endif()
	target_link_libraries(audioModule PRIVATE ${WASAPI})
elseif (${AUDIO_BACKEND} MATCHES LIBSOUNDIO)
	target_compile_definitions(audioModule PRIVATE -DLIBSOUNDIO)
	find_library(libsoundio soundio)
	find_path(libsoundioIncludeDir NAMES soundio/soundio.h)
	if (NOT libsoundio)
		message(FATAL_ERROR "Unable to locate libsoundio!")
	endif()
	target_include_directories(audioModule PRIVATE ${libsoundioIncludeDir})
	target_link_libraries(audioModule PRIVATE ${libsoundio})
endif()

add_library(graphicsModule
	src/Render.cpp
	src/Image.cpp
	src/X11.cpp
	src/Calculate.cpp
)
target_include_directories(graphicsModule
	PRIVATE
		include
		"${PROJECT_BINARY_DIR}"
)

if (DEFINED GLFW_PATH)
	set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
	add_subdirectory(${GLFW_PATH})
	target_include_directories(graphicsModule PRIVATE ${GLFW_PATH}/include)
	target_link_libraries(graphicsModule PRIVATE glfw ${GLFW_LIBRARIES})
else()
	find_library(GLFW glfw REQUIRED)
	target_include_directories(graphicsModule PRIVATE ${GLFW_INCLUDE_DIRS})
	target_link_libraries(graphicsModule PRIVATE ${GLFW})
endif()

set(VULKAN_SDK_LIB_PATH "/usr/lib/x86_64-linux-gnu")
set(VULKAN_SDK_LAYER_PATH "/usr/share/vulkan/explicit_layer.d")

if (DEFINED VULKAN_SDK_PATH)
	if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
		target_include_directories(graphicsModule PRIVATE ${VULKAN_SDK_PATH}/x86_64/include)
		target_link_libraries(graphicsModule PRIVATE -L${VULKAN_SDK_PATH}/x86_64/lib)
		target_link_libraries(graphicsModule PRIVATE vulkan)
		set(VULKAN_SDK_LIB_PATH ${VULKAN_SDK_PATH}/x86_64/lib)
		set(VULKAN_SDK_LAYER_PATH ${VULKAN_SDK_PATH}/x86_64/etc/vulkan/explicit_layer.d)
	elseif (${CMAKE_SYSTEM_NAME} MATCHES Windows)
	elseif (${CMAKE_SYSTEM_NAME} MATCHES Darwin)
		target_include_directories(graphicsModule PRIVATE ${VULKAN_SDK_PATH}/macOS/include)
		target_link_libraries(graphicsModule PRIVATE -L${VULKAN_SDK_PATH}/macOS/lib)
		target_link_libraries(graphicsModule PRIVATE vulkan)
		set(VULKAN_ICD_FILENAMES ${VULKAN_SDK_PATH}/macOS/etc/vulkan/icd.d/MoltenVK_icd.json)
		set(VULKAN_SDK_LAYER_PATH ${VULKAN_SDK_PATH}/macOS/etc/vulkan/explicit_layer.d)
	endif()
else()
	find_package(Vulkan REQUIRED)
	target_link_libraries(graphicsModule PRIVATE Vulkan::Vulkan)
endif()

option(INCLUDE_X11 "Include X11 for extra functionality on X11 systems" ON)
if (INCLUDE_X11)
	find_package(X11)
	if (${X11_FOUND})
		target_compile_definitions(graphicsModule PRIVATE -DNATIVE_WINDOW_HINTS_SUPPORTED -DX11)
		target_include_directories(graphicsModule PRIVATE ${X11_INCLUDE_DIR})
		target_link_libraries(graphicsModule PRIVATE ${X11_LIBRARIES})
	endif()
endif()

# Image libraries
find_package(PNG)
if (${PNG_FOUND})
	target_link_libraries(graphicsModule PRIVATE PNG::PNG)
else()
	target_compile_definitions(graphicsModule PRIVATE -DDISABLE_PNG)
endif()
find_package(JPEG)
if (${JPEG_FOUND})
	target_link_libraries(graphicsModule PRIVATE JPEG::JPEG)
else()
	target_compile_definitions(graphicsModule PRIVATE -DDISABLE_JPEG)
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION MATCHES "8..*")
	target_link_libraries(graphicsModule PUBLIC -lstdc++fs)
endif()

add_executable(vkav
	src/Vkav.cpp
	src/Process.cpp
	src/Settings.cpp
	src/Data.cpp
	src/Calculate.cpp
)
target_include_directories(vkav
	PRIVATE
		include
		"${PROJECT_BINARY_DIR}"
)
target_link_libraries(vkav audioModule graphicsModule)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION MATCHES "8..*")
	target_link_libraries(vkav -lstdc++fs)
endif()

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES Windows AND ${CMAKE_BUILD_TYPE} MATCHES release)
	add_custom_command(TARGET vkav POST_BUILD COMMAND strip --strip-all vkav)
endif()

# Installer

if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
	install(TARGETS vkav
			CONFIGURATIONS Release
			RUNTIME DESTINATION bin)
	install(FILES src/config
			CONFIGURATIONS Release
			DESTINATION /etc/vkav)
	install(DIRECTORY src/modules
			CONFIGURATIONS Release
			DESTINATION /etc/vkav)
	install(FILES vkav.desktop
			CONFIGURATIONS Release
			DESTINATION /usr/share/applications)
	install(FILES vkav.png
			CONFIGURATIONS Release
			DESTINATION /usr/share/icons/hicolor/48x48/apps)
	install(FILES vkav.svg # icon
			CONFIGURATIONS Release
			DESTINATION /usr/share/icons/hicolor/scalable/apps)
elseif (${CMAKE_SYSTEM_NAME} MATCHES Windows)
	message("CMAKE installation unsupported on this platform. An app bundle must be created instead.")
elseif (${CMAKE_SYSTEM_NAME} MATCHES Darwin)
endif()

# For formatting source code
file(GLOB src
	"include/*.h"
	"src/*.cpp"
)

add_custom_target(format clang-format -style=file -i ${src})

add_custom_target(run VK_ICD_FILENAMES=${VULKAN_ICD_FILENAMES} LD_LIBRARY_PATH=${VULKAN_SDK_LIB_PATH} VK_LAYER_PATH=${VULKAN_SDK_LAYER_PATH} ./vkav)

# Unit Tests
option(BUILD_TESTS "Build unit tests" OFF)
if (BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
