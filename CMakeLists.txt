#
# CMAKE file for Gnilk Utility Library
#
cmake_minimum_required(VERSION 2.8)


project(macmix)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

include_directories("src/")
if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
    set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)
endif()

#
# Source
#

list(APPEND src src/filexm.cpp src/filexm.h)
list(APPEND src src/mixer.cpp src/mixer.h)
list(APPEND src src/mutex.h)
list(APPEND src src/ringbuffer.cpp src/ringbuffer.cpp)

list(APPEND src src/main.cpp)



#
# Exec
#
add_executable(macmix ${src})
target_include_directories(macmix PRIVATE .)

set_property(TARGET macmix PROPERTY CXX_STANDARD 11)

set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/local/lib)
find_library(IOKIT_FRAMEWORK IOKit)
find_library(AUDIO_TOOLBOX AudioToolbox)
find_library(CORE_AUDIO CoreAudio)
find_library(AUDIO_UNIT AudioUnit)
find_library(CORE_SERVICES CoreServices)
find_library(CORE_FRAMEWORK CoreFoundation)

message(STATUS ${CMAKE_OSX_ARCHITECTURES})

target_link_libraries(macmix ${CORE_FRAMEWORK} ${IOKIT_FRAMEWORK} ${CORE_AUDIO} ${AUDIO_TOOLBOX} ${AUDIO_UNIT} ${CORE_SERVICES})

