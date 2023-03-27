cmake_minimum_required(VERSION 3.10)

project(gba)

set (CMAKE_CXX_STANDARD 20)

set( SDL_STATIC ON CACHE BOOL "" FORCE )
set( SDL_SHARED OFF CACHE BOOL "" FORCE )

add_subdirectory(SDL)

add_executable(gba gba.cpp "opcodes.cpp" "opcodes.h" "memory.cpp" "memory.hpp" "timer.hpp" "timer.cpp")

target_link_libraries( ${CMAKE_PROJECT_NAME} PUBLIC SDL2main SDL2-static )