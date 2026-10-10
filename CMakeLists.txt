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
    src/formats/chunk.c
    src/formats/ffn.c
    src/formats/fsh.c
    src/formats/mesh.c
    src/formats/tri.c
    src/game/frontend/config_dat.c
    src/game/frontend/race_settings.c
    src/game/gfx/flat.c
    src/game/gfx/flat_edge.c
    src/game/gfx/flat_scan.c
    src/game/gfx/object3d.c
    src/game/gfx/object_mappers.c
    src/game/gfx/palette_match.c
    src/game/gfx/quad.c
    src/game/gfx/quad_tri.c
    src/game/race/car_draw.c
    src/game/race/car_drive.c
    src/game/race/car_engine.c
    src/game/race/car_gearbox.c
    src/game/race/car_grip.c
    src/game/race/car_physics.c
    src/game/race/ai_lanes.c
    src/game/race/ai_police.c
    src/game/race/ai_manager.c
    src/game/race/ai_manager_tick.c
    src/game/race/car_progress.c
    src/game/race/car_road.c
    src/game/race/car_scenery.c
    src/game/race/car_setup.c
    src/game/race/car_load.c
    src/game/race/car_body.c
    src/game/race/car_crash.c
    src/game/race/car_crash_tick.c
    src/game/race/crash_trail.c
    src/game/race/rigid_boxes.c
    src/game/race/car_collide.c
    src/game/race/race_contacts.c
    src/game/race/ai_model.c
    src/game/race/ai_parked.c
    src/game/race/ai_spun.c
    src/game/race/ai_setup.c
    src/game/race/ai_profiles.c
    src/game/race/ai_zones.c
    src/game/race/car_held.c
    src/game/race/rigid_body.c
    src/game/race/car_control.c
    src/game/race/ai_drive.c
    src/game/race/race_sim.c
    src/game/sys/rigid_math.c
    src/game/race/car_tyres.c
    src/game/race/car_vertical.c
    src/game/race/car_walls.c
    src/game/race/horizon.c
    src/game/race/model_draw.c
    src/game/race/race_state.c
    src/game/race/sky_ring.c
    src/game/race/track_art.c
    src/game/race/track_draw.c
    src/game/race/track_geometry.c
    src/game/race/track_info.c
    src/game/race/track_objects.c
    src/game/race/track_render.c
    src/game/race/track_tables.c
    src/game/race/view.c
    src/game/sys/fixed_math.c
    src/game/sys/palette.c
    src/formats/qfs.c
    src/formats/qfs_btree.c
    src/formats/qfs_huffman.c
    src/formats/qfs_refpack.c
)
target_include_directories(tnfs_core PUBLIC src)
target_compile_options(tnfs_core PRIVATE ${TNFS_WARNINGS})
# The recovered game code keeps the original's 32-bit wraparound arithmetic.
if(NOT MSVC)
    target_compile_options(tnfs_core PRIVATE -fwrapv)
endif()

add_executable(tnfs
    src/main.c
    src/game/frontend/config.c
    src/game/frontend/control_central.c
    src/game/frontend/dialog.c
    src/game/frontend/frontend.c
    src/game/frontend/main_menu.c
    src/game/frontend/screens.c
    src/game/frontend/text_field.c
    src/game/sys/input.c
    src/game/sys/pointer.c
    src/game/sys/resource.c
    src/game/sys/text.c
    src/game/sys/timer.c
    src/game/sys/video.c
    src/viewer/asset_viewer.c
    src/viewer/model_view.c
    src/viewer/object_sink.c
    src/viewer/ported_view.c
    src/viewer/raster.c
    src/viewer/track_viewer.c
    src/viewer/viewer_main.c
    src/platform/sdl3/platform_sdl3.c
)
target_link_libraries(tnfs PRIVATE tnfs_core)
if(UNIX AND NOT APPLE)
    target_link_libraries(tnfs PRIVATE m)
endif()
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
    foreach(name qfs fsh ffn chunk mesh palette race_settings config_dat tri sky_ring quad object_mappers flat
                 palette_match)
        add_executable(test_${name} tests/test_${name}.c)
        target_link_libraries(test_${name} PRIVATE tnfs_core)
        target_compile_options(test_${name} PRIVATE ${TNFS_WARNINGS})
        add_test(NAME ${name} COMMAND test_${name})
    endforeach()
endif()

# Integration tests over the user's own CD data (original/ is gitignored).
if(NOT EMSCRIPTEN)
    file(GLOB_RECURSE TNFS_ASSET_FILES
        "${CMAKE_SOURCE_DIR}/original/*.QFS" "${CMAKE_SOURCE_DIR}/original/*.qfs"
        "${CMAKE_SOURCE_DIR}/original/*.FSH" "${CMAKE_SOURCE_DIR}/original/*.fsh"
        "${CMAKE_SOURCE_DIR}/original/*.FFN" "${CMAKE_SOURCE_DIR}/original/*.ffn")
    if(TNFS_ASSET_FILES)
        add_executable(test_assets tests/test_assets.c)
        target_link_libraries(test_assets PRIVATE tnfs_core)
        target_compile_options(test_assets PRIVATE ${TNFS_WARNINGS})
        add_test(NAME assets COMMAND test_assets ${TNFS_ASSET_FILES})
    endif()

    file(GLOB TNFS_CONFIG_DAT "${CMAKE_SOURCE_DIR}/original/GAMEDATA/CONFIG/CONFIG.DAT"
        "${CMAKE_SOURCE_DIR}/original/gamedata/config/config.dat")
    if(TNFS_CONFIG_DAT)
        list(GET TNFS_CONFIG_DAT 0 TNFS_CONFIG_DAT_FILE)
        add_test(NAME config_dat_cd COMMAND test_config_dat ${TNFS_CONFIG_DAT_FILE})
    endif()

    file(GLOB TNFS_TRI_FILES "${CMAKE_SOURCE_DIR}/original/SIMDATA/MISC/*.TRI"
        "${CMAKE_SOURCE_DIR}/original/SIMDATA/MISC/*.tri")
    if(TNFS_TRI_FILES)
        add_test(NAME tri_cd COMMAND test_tri ${TNFS_TRI_FILES})
        add_executable(test_track tests/test_track.c)
        target_link_libraries(test_track PRIVATE tnfs_core)
        target_compile_options(test_track PRIVATE ${TNFS_WARNINGS})
        add_test(NAME track COMMAND test_track "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_drive tests/test_drive.c)
        target_link_libraries(test_drive PRIVATE tnfs_core)
        target_compile_options(test_drive PRIVATE ${TNFS_WARNINGS})
        add_test(NAME drive COMMAND test_drive "${CMAKE_SOURCE_DIR}/original/SIMDATA")
    endif()

    file(GLOB_RECURSE TNFS_MODEL_FILES
        "${CMAKE_SOURCE_DIR}/original/SIMDATA/*.FAM" "${CMAKE_SOURCE_DIR}/original/SIMDATA/*.fam"
        "${CMAKE_SOURCE_DIR}/original/SIMDATA/*.CFM" "${CMAKE_SOURCE_DIR}/original/SIMDATA/*.cfm")
    if(TNFS_MODEL_FILES)
        add_executable(test_models tests/test_models.c)
        target_link_libraries(test_models PRIVATE tnfs_core)
        target_compile_options(test_models PRIVATE ${TNFS_WARNINGS})
        add_test(NAME models COMMAND test_models ${TNFS_MODEL_FILES})
    endif()

    add_executable(probe_3d tools/probe_3d.c)
    target_link_libraries(probe_3d PRIVATE tnfs_core)
    target_compile_options(probe_3d PRIVATE ${TNFS_WARNINGS})

    add_executable(probe_object3d tools/probe_object3d.c)
    target_link_libraries(probe_object3d PRIVATE tnfs_core)
    target_compile_options(probe_object3d PRIVATE ${TNFS_WARNINGS})

    add_executable(probe_math tools/probe_math.c)
    target_link_libraries(probe_math PRIVATE tnfs_core)
    add_executable(probe_car tools/probe_car.c)
    target_link_libraries(probe_car PRIVATE tnfs_core)
    target_compile_options(probe_car PRIVATE ${TNFS_WARNINGS})

    add_executable(qfs_unpack tools/qfs_unpack.c)
    target_link_libraries(qfs_unpack PRIVATE tnfs_core)
    target_compile_options(qfs_unpack PRIVATE ${TNFS_WARNINGS})

    add_executable(quad_draw tools/quad_draw.c)
    target_link_libraries(quad_draw PRIVATE tnfs_core)
    target_compile_options(quad_draw PRIVATE ${TNFS_WARNINGS})

    add_executable(flat_draw tools/flat_draw.c)
    target_link_libraries(flat_draw PRIVATE tnfs_core)
    target_compile_options(flat_draw PRIVATE ${TNFS_WARNINGS})

    add_executable(palette_match_run tools/palette_match_run.c)
    target_link_libraries(palette_match_run PRIVATE tnfs_core)
    target_compile_options(palette_match_run PRIVATE ${TNFS_WARNINGS})
endif()
