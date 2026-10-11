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
    core/formats/bnk.c
    core/formats/bnk_exchange.c
    core/formats/chunk.c
    core/formats/doc.c
    core/formats/ffn.c
    core/formats/fileset.c
    core/formats/fixed_record.c
    core/formats/fsh.c
    core/formats/mesh.c
    core/formats/mesh_exchange.c
    core/formats/modpath.c
    core/formats/pbs.c
    core/formats/pbs_fields.c
    core/formats/limits.c
    core/formats/pdn.c
    core/formats/png.c
    core/formats/record_exchange.c
    core/formats/resource_exchange.c
    core/formats/shpi_exchange.c
    core/formats/slots.c
    core/formats/table_exchange.c
    core/formats/tri.c
    core/formats/tri_exchange.c
    core/formats/track_gen.c
    core/formats/tri_write.c
    core/formats/tsv.c
    core/formats/validate.c
    core/formats/validate_misc.c
    core/formats/validate_track.c
    core/formats/wav.c
    core/formats/qfs.c
    core/formats/qfs_btree.c
    core/formats/qfs_encode.c
    core/formats/qfs_huffman.c
    core/formats/qfs_inplace.c
    core/formats/qfs_refpack.c
)
target_include_directories(tnfs_core PUBLIC core)
if(UNIX)
    target_link_libraries(tnfs_core PUBLIC m)   # track_gen
endif()
target_compile_options(tnfs_core PRIVATE ${TNFS_WARNINGS})

# The recovered game logic: platform-independent, links no SDL.
add_library(tnfs_game STATIC
    game/frontend/config_dat.c
    game/frontend/race_settings.c
    game/gfx/flat.c
    game/gfx/flat_edge.c
    game/gfx/flat_scan.c
    game/gfx/object3d.c
    game/gfx/object_mappers.c
    game/gfx/palette_match.c
    game/gfx/quad.c
    game/gfx/quad_tri.c
    game/race/car_draw.c
    game/race/car_drive.c
    game/race/car_engine.c
    game/race/car_gearbox.c
    game/race/car_grip.c
    game/race/car_physics.c
    game/race/ai_lanes.c
    game/race/ai_police.c
    game/race/ai_manager.c
    game/race/ai_manager_tick.c
    game/race/car_progress.c
    game/race/car_road.c
    game/race/car_scenery.c
    game/race/car_setup.c
    game/race/car_load.c
    game/race/car_body.c
    game/race/car_crash.c
    game/race/car_crash_tick.c
    game/race/crash_trail.c
    game/race/rigid_boxes.c
    game/race/car_collide.c
    game/race/race_contacts.c
    game/race/race_finish.c
    game/race/race_camera.c
    game/race/race_results.c
    game/race/race_view.c
    game/race/race_chase.c
    game/race/race_splits.c
    game/race/race_split_tables.c
    game/race/ai_model.c
    game/race/ai_parked.c
    game/race/ai_spun.c
    game/race/ai_setup.c
    game/race/ai_profiles.c
    game/race/ai_zones.c
    game/race/car_held.c
    game/race/rigid_body.c
    game/race/car_control.c
    game/race/ai_drive.c
    game/race/race_sim.c
    game/sys/rigid_math.c
    game/race/car_tyres.c
    game/race/car_vertical.c
    game/race/car_walls.c
    game/race/horizon.c
    game/race/model_draw.c
    game/race/race_state.c
    game/race/sky_ring.c
    game/race/track_art.c
    game/race/track_draw.c
    game/race/track_geometry.c
    game/race/track_info.c
    game/race/track_objects.c
    game/race/track_render.c
    game/race/track_tables.c
    game/race/view.c
    game/sys/fixed_math.c
    game/sys/palette.c
)
target_include_directories(tnfs_game PUBLIC game .)
target_link_libraries(tnfs_game PUBLIC tnfs_core)
target_compile_options(tnfs_game PRIVATE ${TNFS_WARNINGS})
# The recovered game code keeps the original's 32-bit wraparound arithmetic.
if(NOT MSVC)
    target_compile_options(tnfs_game PRIVATE -fwrapv)
endif()

add_executable(tnfs
    game/main.c
    game/frontend/config.c
    game/frontend/control_central.c
    game/frontend/dialog.c
    game/frontend/frontend.c
    game/frontend/main_menu.c
    game/frontend/screens.c
    game/frontend/text_field.c
    game/sys/input.c
    game/sys/pointer.c
    game/sys/resource.c
    game/sys/text.c
    game/sys/timer.c
    game/sys/video.c
    game/viewer/asset_viewer.c
    game/viewer/model_view.c
    game/viewer/object_sink.c
    game/viewer/ported_view.c
    game/viewer/raster.c
    game/viewer/track_viewer.c
    game/viewer/viewer_main.c
    game/platform/sdl3/platform_sdl3.c
)
target_link_libraries(tnfs PRIVATE tnfs_game)
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

# The studio command line: file formats only, no game code and no SDL.
if(NOT EMSCRIPTEN)
    add_executable(tnfs-studio studio/main.c studio/fsio.c studio/slot.c studio/track_new.c)
    target_include_directories(tnfs-studio PRIVATE .)
    target_link_libraries(tnfs-studio PRIVATE tnfs_core)
    target_compile_options(tnfs-studio PRIVATE ${TNFS_WARNINGS})
endif()

if(NOT EMSCRIPTEN)
    enable_testing()
    foreach(name qfs qfs_encode doc fsh ffn chunk mesh pbs pdn bnk tri tri_write tsv png fileset shpi_exchange table_exchange tri_exchange wav bnk_exchange mesh_exchange resource_exchange validate modpath slots track_gen)
        add_executable(test_${name} tests/test_${name}.c)
        target_link_libraries(test_${name} PRIVATE tnfs_core)
        target_compile_options(test_${name} PRIVATE ${TNFS_WARNINGS})
        add_test(NAME ${name} COMMAND test_${name})
    endforeach()
    foreach(name palette race_settings config_dat sky_ring quad object_mappers flat palette_match)
        add_executable(test_${name} tests/test_${name}.c)
        target_link_libraries(test_${name} PRIVATE tnfs_game)
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
        target_link_libraries(test_assets PRIVATE tnfs_game)
        target_compile_options(test_assets PRIVATE ${TNFS_WARNINGS})
        add_test(NAME assets COMMAND test_assets ${TNFS_ASSET_FILES})
    endif()

    file(GLOB TNFS_CONFIG_DAT "${CMAKE_SOURCE_DIR}/original/GAMEDATA/CONFIG/CONFIG.DAT"
        "${CMAKE_SOURCE_DIR}/original/gamedata/config/config.dat")
    if(TNFS_CONFIG_DAT)
        list(GET TNFS_CONFIG_DAT 0 TNFS_CONFIG_DAT_FILE)
        add_test(NAME config_dat_cd COMMAND test_config_dat ${TNFS_CONFIG_DAT_FILE})
    endif()

    file(GLOB_RECURSE TNFS_QFS_FILES "${CMAKE_SOURCE_DIR}/original/*.QFS" "${CMAKE_SOURCE_DIR}/original/*.PBS"
        "${CMAKE_SOURCE_DIR}/original/*.PDN")
    if(TNFS_QFS_FILES)
        add_test(NAME qfs_encode_cd COMMAND test_qfs_encode ${TNFS_QFS_FILES})
    endif()
    file(GLOB_RECURSE TNFS_DOC_FILES "${CMAKE_SOURCE_DIR}/original/*.FAM" "${CMAKE_SOURCE_DIR}/original/*.CFM"
        "${CMAKE_SOURCE_DIR}/original/*.FSH" "${CMAKE_SOURCE_DIR}/original/*.QFS")
    if(TNFS_DOC_FILES)
        add_test(NAME doc_cd COMMAND test_doc ${TNFS_DOC_FILES})
    endif()
    file(GLOB_RECURSE TNFS_SHPI_FILES "${CMAKE_SOURCE_DIR}/original/*.FSH" "${CMAKE_SOURCE_DIR}/original/*.QFS")
    if(TNFS_SHPI_FILES)
        add_test(NAME shpi_exchange_cd COMMAND test_shpi_exchange ${TNFS_SHPI_FILES})
    endif()
    file(GLOB_RECURSE TNFS_MESH_FILES "${CMAKE_SOURCE_DIR}/original/*.FAM" "${CMAKE_SOURCE_DIR}/original/*.CFM")
    if(TNFS_MESH_FILES)
        add_test(NAME mesh_exchange_cd COMMAND test_mesh_exchange ${TNFS_MESH_FILES})
    endif()
    file(GLOB_RECURSE TNFS_ALL_FILES "${CMAKE_SOURCE_DIR}/original/SIMDATA/*" "${CMAKE_SOURCE_DIR}/original/GAMEDATA/*"
        "${CMAKE_SOURCE_DIR}/original/FRONTEND/*")
    if(TNFS_ALL_FILES)
        add_test(NAME resource_exchange_cd COMMAND test_resource_exchange ${TNFS_ALL_FILES})
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_test(NAME studio_cli COMMAND sh ${CMAKE_SOURCE_DIR}/tests/studio_cli.sh $<TARGET_FILE:tnfs-studio> ${CMAKE_SOURCE_DIR}/original)
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_test(NAME mods_overlay COMMAND sh ${CMAKE_SOURCE_DIR}/tests/mods_overlay.sh $<TARGET_FILE:tnfs> $<TARGET_FILE:tnfs-studio> ${CMAKE_SOURCE_DIR}/original)
        add_test(NAME track_new COMMAND sh ${CMAKE_SOURCE_DIR}/tests/track_new.sh $<TARGET_FILE:tnfs> $<TARGET_FILE:tnfs-studio> ${CMAKE_SOURCE_DIR}/original)
    endif()
    add_executable(test_platform_mods tests/test_platform_mods.c game/platform/sdl3/platform_sdl3.c)
    target_link_libraries(test_platform_mods PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_platform_mods PRIVATE m)
    endif()
    target_compile_options(test_platform_mods PRIVATE ${TNFS_WARNINGS})
    add_test(NAME platform_mods COMMAND test_platform_mods)
    add_executable(test_reference_mod tests/test_reference_mod.c game/platform/sdl3/platform_sdl3.c)
    target_link_libraries(test_reference_mod PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_reference_mod PRIVATE m)
    endif()
    target_compile_options(test_reference_mod PRIVATE ${TNFS_WARNINGS})
    if(EXISTS "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_test(NAME reference_mod COMMAND sh ${CMAKE_SOURCE_DIR}/tests/reference_mod.sh $<TARGET_FILE:tnfs> $<TARGET_FILE:tnfs-studio>
                 ${CMAKE_SOURCE_DIR}/original $<TARGET_FILE:test_reference_mod>)
    endif()
    file(GLOB TNFS_PBS_FILES "${CMAKE_SOURCE_DIR}/original/SIMDATA/CARSPECS/*.PBS"
        "${CMAKE_SOURCE_DIR}/original/SIMDATA/CARSPECS/*.pbs")
    if(TNFS_PBS_FILES)
        add_test(NAME pbs_cd COMMAND test_pbs ${TNFS_PBS_FILES})
    endif()
    file(GLOB TNFS_PDN_FILES "${CMAKE_SOURCE_DIR}/original/SIMDATA/CARFAMS/*.PDN"
        "${CMAKE_SOURCE_DIR}/original/SIMDATA/CARFAMS/*.pdn")
    if(TNFS_PDN_FILES)
        add_test(NAME pdn_cd COMMAND test_pdn ${TNFS_PDN_FILES})
    endif()
    file(GLOB TNFS_BNK_FILES "${CMAKE_SOURCE_DIR}/original/SIMDATA/SOUNDBNK/*.BNK"
        "${CMAKE_SOURCE_DIR}/original/SIMDATA/SOUNDBNK/*.bnk")
    if(TNFS_BNK_FILES)
        add_test(NAME bnk_cd COMMAND test_bnk ${TNFS_BNK_FILES})
        add_test(NAME bnk_exchange_cd COMMAND test_bnk_exchange ${TNFS_BNK_FILES})
    endif()

    file(GLOB TNFS_TRI_FILES "${CMAKE_SOURCE_DIR}/original/SIMDATA/MISC/*.TRI"
        "${CMAKE_SOURCE_DIR}/original/SIMDATA/MISC/*.tri")
    if(TNFS_TRI_FILES)
        add_test(NAME tri_cd COMMAND test_tri ${TNFS_TRI_FILES})
        add_test(NAME tri_write_cd COMMAND test_tri_write ${TNFS_TRI_FILES})
        add_test(NAME tri_exchange_cd COMMAND test_tri_exchange ${TNFS_TRI_FILES})
        add_test(NAME track_gen_cd COMMAND test_track_gen ${TNFS_TRI_FILES})
        add_executable(test_track tests/test_track.c)
        target_link_libraries(test_track PRIVATE tnfs_game)
        target_compile_options(test_track PRIVATE ${TNFS_WARNINGS})
        add_test(NAME track COMMAND test_track "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_drive tests/test_drive.c)
        target_link_libraries(test_drive PRIVATE tnfs_game)
        target_compile_options(test_drive PRIVATE ${TNFS_WARNINGS})
        add_test(NAME drive COMMAND test_drive "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_race tests/test_race.c)
        target_link_libraries(test_race PRIVATE tnfs_game)
        target_compile_options(test_race PRIVATE ${TNFS_WARNINGS})
        add_test(NAME race COMMAND test_race "${CMAKE_SOURCE_DIR}/original/SIMDATA")
    endif()

    file(GLOB_RECURSE TNFS_MODEL_FILES
        "${CMAKE_SOURCE_DIR}/original/SIMDATA/*.FAM" "${CMAKE_SOURCE_DIR}/original/SIMDATA/*.fam"
        "${CMAKE_SOURCE_DIR}/original/SIMDATA/*.CFM" "${CMAKE_SOURCE_DIR}/original/SIMDATA/*.cfm")
    if(TNFS_MODEL_FILES)
        add_executable(test_models tests/test_models.c)
        target_link_libraries(test_models PRIVATE tnfs_game)
        target_compile_options(test_models PRIVATE ${TNFS_WARNINGS})
        add_test(NAME models COMMAND test_models ${TNFS_MODEL_FILES})
    endif()

    add_executable(probe_3d tools/probe_3d.c)
    target_link_libraries(probe_3d PRIVATE tnfs_game)
    target_compile_options(probe_3d PRIVATE ${TNFS_WARNINGS})

    add_executable(probe_object3d tools/probe_object3d.c)
    target_link_libraries(probe_object3d PRIVATE tnfs_game)
    target_compile_options(probe_object3d PRIVATE ${TNFS_WARNINGS})

    add_executable(probe_math tools/probe_math.c)
    target_link_libraries(probe_math PRIVATE tnfs_game)
    add_executable(probe_car tools/probe_car.c)
    target_link_libraries(probe_car PRIVATE tnfs_game)
    target_compile_options(probe_car PRIVATE ${TNFS_WARNINGS})

    add_executable(qfs_unpack tools/qfs_unpack.c)
    target_link_libraries(qfs_unpack PRIVATE tnfs_core)
    target_compile_options(qfs_unpack PRIVATE ${TNFS_WARNINGS})

    add_executable(quad_draw tools/quad_draw.c)
    target_link_libraries(quad_draw PRIVATE tnfs_game)
    target_compile_options(quad_draw PRIVATE ${TNFS_WARNINGS})

    add_executable(flat_draw tools/flat_draw.c)
    target_link_libraries(flat_draw PRIVATE tnfs_game)
    target_compile_options(flat_draw PRIVATE ${TNFS_WARNINGS})

    add_executable(palette_match_run tools/palette_match_run.c)
    target_link_libraries(palette_match_run PRIVATE tnfs_game)
    target_compile_options(palette_match_run PRIVATE ${TNFS_WARNINGS})
endif()
