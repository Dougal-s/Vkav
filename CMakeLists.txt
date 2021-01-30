cmake_minimum_required(VERSION 3.12)

project(Vkav VERSION 0.6.0 LANGUAGES CXX)

configure_file(include/Version.hpp.in Version.hpp)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_compile_options(
	"$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb;-Wall;-Wextra;-Wpedantic>"
	"$<$<CONFIG:ASAN>:-O0;-g3;-ggdb;-Wall;-Wextra;-fno-omit-frame-pointer;-fsanitize=address>"
	"$<$<CONFIG:RELEASE>:-O3;-DNDEBUG>"
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


add_library(audioModule)
target_include_directories(audioModule PRIVATE include)

if (${AUDIO_BACKEND} MATCHES COREAUDIO)
	target_sources(audioModule PRIVATE src/CoreAudio.cpp)
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
		target_sources(audioModule PRIVATE src/PulseAudio.cpp)
		target_link_libraries(audioModule PRIVATE ${pulseAudioSimple} -lpthread)
	else()
		target_sources(audioModule PRIVATE src/PulseAudioAsync.cpp)
	endif()
	target_link_libraries(audioModule PRIVATE ${pulseAudio})
elseif (${AUDIO_BACKEND} MATCHES WASAPI)
	target_sources(audioModule PRIVATE src/WASAPI.cpp)
	find_library(WASAPI wasapi)
	if (NOT WASAPI)
		message(FATAL_ERROR "Unable to locate WASAPI!")
	endif()
	target_link_libraries(audioModule PRIVATE ${WASAPI})
elseif (${AUDIO_BACKEND} MATCHES LIBSOUNDIO)
	target_sources(audioModule PRIVATE src/libsoundio.cpp)
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
	src/Calculate.cpp
	src/ModuleConfig.cpp
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

if (DEFINED VULKAN_SDK_PATH)
	if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
		target_include_directories(graphicsModule PRIVATE ${VULKAN_SDK_PATH}/x86_64/include)
		target_link_libraries(graphicsModule PRIVATE -L${VULKAN_SDK_PATH}/x86_64/lib)
		target_link_libraries(graphicsModule PRIVATE vulkan)

		set(VULKAN_SDK_LIB_PATH "${VULKAN_SDK_PATH}/x86_64/lib")
		set(VULKAN_SDK_LAYER_PATH "${VULKAN_SDK_PATH}/x86_64/etc/vulkan/explicit_layer.d")

		set(ENV_VARS LD_LIBRARY_PATH=${VULKAN_SDK_LIB_PATH} VK_LAYER_PATH=${VULKAN_SDK_LAYER_PATH})
	elseif (${CMAKE_SYSTEM_NAME} MATCHES Windows)
	elseif (${CMAKE_SYSTEM_NAME} MATCHES Darwin)
		target_include_directories(graphicsModule PRIVATE ${VULKAN_SDK_PATH}/macOS/include)
		target_link_libraries(graphicsModule PRIVATE -L${VULKAN_SDK_PATH}/macOS/lib)
		target_link_libraries(graphicsModule PRIVATE vulkan)

		set(VULKAN_ICD_FILENAMES "${VULKAN_SDK_PATH}/macOS/etc/vulkan/icd.d/MoltenVK_icd.json")
		set(VULKAN_SDK_DYLIB_PATH "${VULKAN_SDK_PATH}/macOS/lib:$DYLD_LIBRARY_PATH")
		set(VULKAN_SDK_LAYER_PATH "${VULKAN_SDK_PATH}/macOS/etc/vulkan/explicit_layer.d")

		set(ENV_VARS VK_ICD_FILENAMES=${VULKAN_ICD_FILENAMES} DYLD_LIBRARY_PATH=${VULKAN_SDK_LIB_PATH} VK_LAYER_PATH=${VULKAN_SDK_LAYER_PATH})
	endif()
else()
	find_package(Vulkan REQUIRED)
	target_link_libraries(graphicsModule PRIVATE Vulkan::Vulkan)
endif()

option(INCLUDE_X11 "Include X11 for extra functionality on X11 systems" ON)
if (INCLUDE_X11)
	find_package(X11)
	if (${X11_FOUND})
		target_compile_definitions(graphicsModule PRIVATE -DNATIVE_WINDOW_HINTS_SUPPORTED)
		target_sources(graphicsModule PRIVATE src/X11.cpp)
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

if (${CMAKE_BUILD_TYPE} MATCHES Release)
	add_custom_command(TARGET vkav POST_BUILD COMMAND ${CMAKE_STRIP} vkav)
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

# compile modules
if (NOT DEFINED GLSLC_PATH)
	if (DEFINED VULKAN_SDK_PATH)
		set(GLSLC_PATH ${VULKAN_SDK_PATH}/x86_64/bin/glslc)
	else()
		set(GLSLC_PATH glslc)
	endif()
endif()

add_custom_target(shaders)
function(add_module MODULE STAGES)
	string(REPLACE " " "_" MODULE_NAME "${MODULE}")
	add_custom_target(${MODULE_NAME})
	foreach(STAGE RANGE 1 ${STAGES})
		if (EXISTS "${CMAKE_SOURCE_DIR}/src/modules/${MODULE}/${STAGE}/shader.vert")
			set(VERT_COMMAND ${GLSLC_PATH} -O shader.vert -o vert.spv)
		else()
			set(VERT_COMMAND "")
		endif()

		add_custom_target(
			${MODULE_NAME}_STAGE_${STAGE}
			COMMAND ${GLSLC_PATH} -O shader.frag -o frag.spv
			COMMAND ${VERT_COMMAND}
			BYPRODUCTS frag.spv vert.spv
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/modules/${MODULE}/${STAGE}"
		)
		add_dependencies(${MODULE_NAME} ${MODULE_NAME}_STAGE_${STAGE})
	endforeach()
	add_dependencies(shaders ${MODULE_NAME})
endfunction()

add_module(background 1)
add_module(bars 1)
add_module(eclipse 2)
add_module(fragment 1)
add_module(logo 1)
add_module(mist 1)
add_module(octahedron 1)
add_module("octahedron 2" 3)
add_module(orbital 1)
add_module(radial 1)
add_module(rings 1)

# Formatting source code
file(GLOB src
	"include/*.h"
	"src/*.cpp"
)
add_custom_target(format clang-format -style=file -i ${src})

add_custom_target(run ${ENV_VARS} ./vkav)

# Unit Tests
option(BUILD_TESTS "Build unit tests" OFF)
if (BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
