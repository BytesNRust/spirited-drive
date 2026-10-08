cmake_minimum_required(VERSION 3.24)
project(tnfs C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(tnfs
    src/main.c
    src/platform/sdl3/platform_sdl3.c
)
target_include_directories(tnfs PRIVATE src)

if(MSVC)
    target_compile_options(tnfs PRIVATE /W4)
else()
    target_compile_options(tnfs PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_compile_options(tnfs PRIVATE --use-port=sdl3)
    target_link_options(tnfs PRIVATE --use-port=sdl3 -sASYNCIFY -sALLOW_MEMORY_GROWTH)
else()
    find_package(SDL3 REQUIRED CONFIG)
    target_link_libraries(tnfs PRIVATE SDL3::SDL3)
endif()
