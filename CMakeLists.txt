cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_ANDROID_NDK $ENV{NDK_ROOT}/platform)
set(CMAKE_TOOLCHAIN_FILE $ENV{NDK_ROOT}/build/cmake/android.toolchain.cmake)
set(CMAKE_SOURCE_DIR src)
set(APP_ABI armeabi-v7a)
set(ANDROID_STL gnustl_static)
set(ANDROID_PLATFORM android-17)
set(ANDROID_CPP_FEATURES rtti exceptions)

project(InputUnlockerHack)

SET(DOBBY_DEBUG OFF CACHE INTERNAL "" FORCE)
SET(DOBBY_GENERATE_SHARED OFF CACHE INTERNAL "" FORCE)
SET(Plugin.SymbolResolver ON CACHE INTERNAL "" FORCE)
add_subdirectory(Dobby dobby)

get_property(DOBBY_INCLUDE_DIRECTORIES
        TARGET dobby
        PROPERTY INCLUDE_DIRECTORIES)
include_directories(
        ${DOBBY_INCLUDE_DIRECTORIES}
        $<TARGET_PROPERTY:dobby,INCLUDE_DIRECTORIES>
)

# pass through defines for easier building
if(IU_NO_MISSING_MARKER)
    add_definitions(-DIU_NO_MISSING_MARKER)
endif()
if(IU_MISSING_MARKER)
    add_definitions(-DIU_MISSING_MARKER='${IU_MISSING_MARKER}')
endif()
if(IU_NO_UTF8_VALIDATION)
    add_definitions(-DIU_NO_UTF8_VALIDATION)
endif()
if(IU_INVALID_UTF8_MARKER)
    add_definitions(-DIU_INVALID_UTF8_MARKER='${IU_INVALID_UTF8_MARKER}')
endif()
if(NOT IU_NO_NETWORKING)
    add_definitions(-DIU_NW_VERSION=1)
endif()

add_library(InputUnlockerHack SHARED ${CMAKE_SOURCE_DIR}/library.cpp ${CMAKE_SOURCE_DIR}/utils.hpp ${CMAKE_SOURCE_DIR}/utils.cpp)
target_link_libraries(InputUnlockerHack dobby)
add_custom_command(
        TARGET InputUnlockerHack
        POST_BUILD
        COMMAND ${ANDROID_TOOLCHAIN_PREFIX}strip $<TARGET_FILE:InputUnlockerHack>
)