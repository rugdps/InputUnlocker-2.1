cmake_minimum_required(VERSION 3.22)
project(InputUnlocker)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_SOURCE_DIR src)

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
            ${CMAKE_SOURCE_DIR}
            ${DOBBY_INCLUDE_DIRECTORIES}
            $<TARGET_PROPERTY:dobby,INCLUDE_DIRECTORIES>
    )
endif()

# pass through defines for easier building
if(IU_NO_MISSING_MARKER)
    add_compile_options(/D IU_NO_MISSING_MARKER)
endif()
if(IU_MISSING_MARKER)
    add_compile_options(/D IU_MISSING_MARKER=${IU_MISSING_MARKER})
endif()
if(IU_NO_UTF8_VALIDATION)
    add_compile_options(/D IU_NO_UTF8_VALIDATION)
endif()
if(IU_INVALID_UTF8_MARKER)
    add_compile_options(/D IU_INVALID_UTF8_MARKER=${IU_INVALID_UTF8_MARKER})
endif()

add_library(InputUnlocker SHARED ${CMAKE_SOURCE_DIR}/main.cpp ${CMAKE_SOURCE_DIR}/utils.hpp ${CMAKE_SOURCE_DIR}/utils.cpp)

target_link_libraries(InputUnlocker dobby)
