cmake_minimum_required(VERSION 3.16)

project(gba)

set (CMAKE_CXX_STANDARD 20)

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>" )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>" )

set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

add_subdirectory( SDL  EXCLUDE_FROM_ALL )

add_executable(gba WIN32 "gba.cpp" "opcodes.cpp" "opcodes.h" "memory.cpp" "memory.hpp" "timer.hpp" "timer.cpp" "ppu.cpp" "apu.cpp" "apu.hpp")

target_link_libraries( ${CMAKE_PROJECT_NAME} PUBLIC SDL3::SDL3 )

if(WIN32)
    target_link_libraries( ${CMAKE_PROJECT_NAME} PUBLIC comdlg32 ole32 )
endif()
