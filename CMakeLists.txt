cmake_minimum_required(VERSION 3.22)
project(InputUnlocker)

set(CMAKE_CXX_STANDARD 11)

if(NOT TARGET dobby)
    set(DOBBY_DIR Dobby)
    macro(SET_OPTION option value)
        set(${option} ${value} CACHE INTERNAL "" FORCE)
    endmacro()
    SET_OPTION(DOBBY_DEBUG OFF)
    SET_OPTION(DOBBY_GENERATE_SHARED OFF)
    add_subdirectory(${DOBBY_DIR} dobby)
    get_property(DOBBY_INCLUDE_DIRECTORIES
            TARGET dobby
            PROPERTY INCLUDE_DIRECTORIES)
    include_directories(
            .
            ${DOBBY_INCLUDE_DIRECTORIES}
            $<TARGET_PROPERTY:dobby,INCLUDE_DIRECTORIES>
    )
endif()

add_library(InputUnlocker SHARED main.cpp)

target_link_libraries(InputUnlocker dobby)
