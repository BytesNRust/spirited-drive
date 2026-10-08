cmake_minimum_required(VERSION 3.24)
project(tnfs C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    set(TNFS_WARNINGS /W4)
else()
    set(TNFS_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# Platform-independent game code: file formats today, recovered game logic later.
add_library(tnfs_core STATIC
    src/formats/qfs.c
    src/formats/qfs_btree.c
    src/formats/qfs_refpack.c
)
target_include_directories(tnfs_core PUBLIC src)
target_compile_options(tnfs_core PRIVATE ${TNFS_WARNINGS})

add_executable(tnfs
    src/main.c
    src/platform/sdl3/platform_sdl3.c
)
target_link_libraries(tnfs PRIVATE tnfs_core)
target_compile_options(tnfs PRIVATE ${TNFS_WARNINGS})

if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    target_compile_options(tnfs PRIVATE --use-port=sdl3)
    target_link_options(tnfs PRIVATE --use-port=sdl3 -sASYNCIFY -sALLOW_MEMORY_GROWTH)
else()
    find_package(SDL3 REQUIRED CONFIG)
    target_link_libraries(tnfs PRIVATE SDL3::SDL3)
endif()

if(NOT EMSCRIPTEN)
    enable_testing()
    foreach(name qfs)
        add_executable(test_${name} tests/test_${name}.c)
        target_link_libraries(test_${name} PRIVATE tnfs_core)
        target_compile_options(test_${name} PRIVATE ${TNFS_WARNINGS})
        add_test(NAME ${name} COMMAND test_${name})
    endforeach()
endif()

# Integration tests over the user's own CD data (original/ is gitignored).
if(NOT EMSCRIPTEN)
    file(GLOB_RECURSE TNFS_QFS_FILES "${CMAKE_SOURCE_DIR}/original/*.QFS" "${CMAKE_SOURCE_DIR}/original/*.qfs")
    if(TNFS_QFS_FILES)
        add_executable(test_assets_qfs tests/test_assets_qfs.c)
        target_link_libraries(test_assets_qfs PRIVATE tnfs_core)
        target_compile_options(test_assets_qfs PRIVATE ${TNFS_WARNINGS})
        add_test(NAME assets_qfs COMMAND test_assets_qfs ${TNFS_QFS_FILES})
    endif()
endif()
