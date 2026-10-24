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
    core/formats/ico.c
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
    game/frontend/best_times.c
    game/frontend/cheat_code.c
    game/frontend/config_dat.c
    game/frontend/config_records.c
    game/frontend/extras_apply.c
    game/frontend/extras_menu.c
    game/frontend/control_central_art.c
    game/frontend/disk_room.c
    game/frontend/file_list.c
    game/frontend/frame_bench.c
    game/frontend/frontend_loop.c
    game/frontend/game_file.c
    game/frontend/game_list.c
    game/frontend/game_mode.c
    game/frontend/intro.c
    game/frontend/intro_glue.c
    game/frontend/key_capture.c
    game/frontend/load_save.c
    game/frontend/options_menu.c
    game/frontend/check_options.c
    game/frontend/chat.c
    game/frontend/showcase.c
    game/frontend/showcase_pages.c
    game/frontend/showcase_figures.c
    game/frontend/credits.c
    game/frontend/controls_menu.c
    game/frontend/audio_menu.c
    game/frontend/graphics_menu.c
    game/frontend/graphics_plus.c
    game/frontend/graphics_own.c
    game/frontend/config_bgnd.c
    game/frontend/title_art.c
    game/frontend/item_art.c
    game/frontend/page_arrows.c
    game/sys/port_log.c
    game/frontend/controls_plus.c
    game/frontend/records_route.c
    game/frontend/records_glue.c
    game/frontend/load_save2.c
    game/frontend/net_game.c
    game/frontend/net_host.c
    game/frontend/central.c
    game/frontend/central_banner.c
    game/frontend/central_follow.c
    game/frontend/central_draw.c
    game/frontend/central_select.c
    game/frontend/central_keys.c
    game/frontend/key_name.c
    game/frontend/menu_race.c
    game/frontend/menu_sound.c
    game/frontend/message_banner.c
    game/frontend/name_box.c
    game/frontend/number_field.c
    game/frontend/palette_put.c
    game/frontend/race_settings.c
    game/frontend/random_pick.c
    game/frontend/record_check.c
    game/frontend/replay_box.c
    game/frontend/results_screen.c
    game/frontend/best_totals_screen.c
    game/frontend/track_results.c
    game/frontend/lap_times.c
    game/frontend/circuit_results.c
    game/frontend/save_box.c
    game/frontend/save_files.c
    game/frontend/screen_clear.c
    game/frontend/screen_show.c
    game/frontend/standings.c
    game/frontend/stat_text.c
    game/frontend/text_box.c
    game/frontend/text_entry.c
    game/frontend/text_fit.c
    game/frontend/tgv.c
    game/frontend/tgv_frame.c
    game/frontend/tgv_player.c
    game/frontend/title.c
    game/frontend/track_select.c
    game/frontend/tournament.c
    game/frontend/cop_stop.c
    game/frontend/win_movie.c
    game/frontend/after_race.c
    game/frontend/next_segment.c
    game/frontend/wide_field.c
    game/frontend/yes_no_box.c
    game/gfx/flat.c
    game/gfx/flat_edge.c
    game/gfx/flat_scan.c
    game/gfx/object3d.c
    game/gfx/blit.c
    game/gfx/clip.c
    game/gfx/object_mappers.c
    game/gfx/inverse_map.c
    game/gfx/palette_match.c
    game/gfx/quad.c
    game/gfx/quad_tri.c
    game/gfx/needle.c
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
    game/race/car_tag.c
    game/race/input_queue.c
    game/race/race_screen.c
    game/race/race_draw_setup.c
    game/race/car_draw_setup.c
    game/race/race_enter.c
    game/race/race_teardown.c
    game/audio/sound_mix.c
    game/audio/sound_channel.c
    game/audio/sound_driver.c
    game/audio/sound_queue.c
    game/audio/sound_bank.c
    game/audio/sound_adpcm.c
    game/audio/sound_stream.c
    game/audio/music_reader.c
    game/race/race_music.c
    game/race/mouse_steer.c
    game/race/input_read.c
    game/race/input_setup.c
    game/race/input_tick.c
    game/race/race_keys.c
    game/race/start_keys.c
    game/race/race_detail.c
    game/race/race_loop.c
    game/race/race_timer.c
    game/race/race_replay.c
    game/race/race_replay_seek.c
    game/race/race_replay_cuts.c
    game/race/race_replay_file.c
    game/race/race_replay_image.c
    game/race/race_view_pick.c
    game/race/replay_panel.c
    game/race/dash_controls.c
    game/race/car_body_draw.c
    game/race/car_lights.c
    game/race/car_render.c
    game/race/rigid_boxes.c
    game/race/car_collide.c
    game/race/race_contacts.c
    game/race/race_finish.c
    game/race/race_camera.c
    game/race/race_forces.c
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
    game/race/car_model.c
    game/race/car_wheels.c
    game/race/race_grid.c
    game/race/race_models.c
    game/race/single_rival.c
    game/race/race_sound.c
    game/race/race_palette.c
    game/race/dash_art.c
    game/race/dash_load.c
    game/race/hud_colours.c
    game/race/hud_frame.c
    game/race/cockpit.c
    game/race/cockpit_colours.c
    game/race/cockpit_load.c
    game/race/cockpit_draw.c
    game/race/cockpit_frame.c
    game/race/race_panel.c
    game/race/mirror_sky.c
    game/race/mirror_scene.c
    game/race/car_shadow.c
    game/race/skid_marks.c
    game/race/row_cars.c
    game/race/tyre_smoke.c
    game/race/puffs.c
    game/race/track_step.c
    game/gfx/shape_flip.c
    game/gfx/thick_line.c
    game/gfx/sprite_scaled.c
    game/race/cockpit_teardown.c
    game/race/cockpit_redraw.c
    game/race/hud.c
    game/race/hud_keys.c
    game/race/hud_mask.c
    game/race/hud_map.c
    game/race/hud_needle.c
    game/race/hud_redraw.c
    game/race/race_view_tables.c
    game/race/race_menu.c
    game/race/race_menu_tables.c
    game/race/race_net.c
    game/race/horizon.c
    game/race/model_draw.c
    game/race/race_state.c
    game/race/sky_ring.c
    game/race/track_art.c
    game/race/track_palette_repair.c
    game/race/track_build.c
    game/race/track_cache.c
    game/race/race_schedule.c
    game/race/race_frame.c
    game/race/race_pointer.c
    game/race/race_tick.c
    game/race/frame_draw.c
    game/race/race_run.c
    game/race/race_session.c
    game/race/race_from_menu.c
    game/net/net_session.c
    game/net/net_queue.c
    game/net/net_packet.c
    game/net/net_packet_open.c
    game/net/net_request.c
    game/net/net_service.c
    game/net/net_server.c
    game/net/net_link.c
    game/net/net_serial.c
    game/net/net_udp.c
    game/net/net_udp_service.c
    game/net/net_runtime.c
    game/race/race_net_services.c
    game/race/race_net_players.c
    game/race/race_net_games.c
    game/race/replay_controls.c
    game/race/track_far.c
    game/race/track_fence.c
    game/race/track_near.c
    game/race/track_object_draw.c
    game/race/track_draw.c
    game/race/track_geometry.c
    game/race/track_fam.c
    game/race/lunar_springs.c
    game/race/fam_mirror.c
    game/race/track_info.c
    game/race/track_object_walk.c
    game/race/track_objects.c
    game/race/track_render.c
    game/race/track_tables.c
    game/race/view.c
    game/sys/fixed_math.c
    game/sys/inverse_load.c
    game/sys/key_codes.c
    game/sys/joystick.c
    game/sys/joy_options.c
    game/sys/joy_start.c
    game/sys/joy_pad.c
    game/sys/joy_calibrate.c
    game/sys/hot_key.c
    game/sys/page_keys.c
    game/sys/menu_key.c
    game/sys/palette.c
    game/sys/text_soft.c
    game/sys/port_settings.c
    game/sys/port_extras.c
    game/sys/port_icon.c
    game/sys/port_controls.c
    game/sys/port_records.c
    game/sys/control_map.c
    game/sys/pointer_state.c
    game/sys/shape_bank.c
    game/viewer/screen_layout.c
    game/viewer/dash_box.c
    game/sys/backdrop.c
    game/frontend/variant_rule.c
    game/sys/resource.c
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
    game/frontend/ea_lib.c
    game/frontend/frontend.c
    game/frontend/menu_glue.c
    game/frontend/link_game.c
    game/frontend/link_messages.c
    game/frontend/link_session.c
    game/frontend/link_central.c
    game/frontend/link_race.c
    game/frontend/central_glue.c
    game/frontend/menu_audio.c
    game/frontend/screens.c
    game/sys/input.c
    game/sys/bands.c
    game/sys/pointer.c
    game/sys/text.c
    game/sys/timer.c
    game/sys/video.c
    game/viewer/asset_viewer.c
    game/viewer/drive_palette.c
    game/viewer/drive_smoke.c
    game/viewer/drive_autopilot.c
    game/viewer/drive_input.c
    game/viewer/drive_interp.c
    game/viewer/drive_clock.c
    game/viewer/race_stats.c
    game/viewer/race_stats_draw.c
    game/viewer/draw_rands.c
    game/viewer/drive_slide.c
    game/viewer/layer_compose.c
    game/viewer/drive_joy.c
    game/viewer/drive_keys.c
    game/viewer/drive_replay.c
    game/viewer/drive_panel.c
    game/viewer/drive_pause.c
    game/viewer/drive_menu.c
    game/viewer/drive_hud.c
    game/viewer/drive_hud_mask.c
    game/viewer/drive_audio.c
    game/viewer/drive_mirror.c
    game/viewer/modern_road.c
    game/viewer/modern_look.c
    game/viewer/horizon_smooth.c
    game/viewer/sky_smooth.c
    game/viewer/drive_bumper.c
    game/viewer/model_view.c
    game/viewer/object_sink.c
    game/viewer/ported_view.c
    game/viewer/raster.c
    game/viewer/track_load.c
    game/viewer/track_fly.c
    game/viewer/drive_wide.c
    game/viewer/drive_draw.c
    game/viewer/drive_bumper_draw.c
    game/viewer/drive_run.c
    game/viewer/track_viewer.c
    game/viewer/viewer_main.c
    game/platform/sdl3/platform_sdl3.c
    game/platform/sdl3/platform_joy_sdl3.c
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
    foreach(name qfs qfs_encode doc fsh ffn ico chunk mesh pbs pdn bnk tri tri_write tsv png fileset shpi_exchange table_exchange tri_exchange wav bnk_exchange mesh_exchange resource_exchange validate modpath slots track_gen)
        add_executable(test_${name} tests/test_${name}.c)
        target_link_libraries(test_${name} PRIVATE tnfs_core)
        target_compile_options(test_${name} PRIVATE ${TNFS_WARNINGS})
        add_test(NAME ${name} COMMAND test_${name})
    endforeach()
foreach(name band_textures single_rival palette race_settings config_dat sky_ring quad object_mappers flat palette_match music_reader net_udp net_udp_service net_runtime replay_image tgv_frame win_movie port_settings screen_layout backdrop graphics_plus page_keys controls_plus port_controls control_map port_extras extras_apply port_records records_route track_select_plus extras_menu options_extras track_near race_pause_resume_esc race_replay_esc race_replay_resume track_palette_repair race_music_stop mirror_and_rally graphics_polish fam_mirror config_bgnd object3d_mirror race_keys_free text_soft graphics_own f3_ultra title_art item_art page_arrows port_log speed_units best_times start_keys race_forces results_marks standings_cheats lunar_springs intro_path intro_skip race_horn_voice replay_launch pad_and_keys menu_song dash_box mirror_rows)
        add_executable(test_${name} tests/test_${name}.c)
        target_link_libraries(test_${name} PRIVATE tnfs_game)
        target_compile_options(test_${name} PRIVATE ${TNFS_WARNINGS})
        add_test(NAME ${name} COMMAND test_${name})
    endforeach()
    # the CD check of the clean configuration background runs when original/ is there
    target_compile_definitions(test_config_bgnd PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_title_art PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_item_art PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_page_arrows PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_port_records PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    # port: the mirror kept on the dash (drive_mirror.c, the viewer's: built into the test with its source)
    add_executable(test_mirror_on_dash tests/test_mirror_on_dash.c game/viewer/drive_mirror.c game/viewer/modern_road.c)
    target_include_directories(test_mirror_on_dash PRIVATE game)
    target_link_libraries(test_mirror_on_dash PRIVATE tnfs_game)
    target_compile_options(test_mirror_on_dash PRIVATE ${TNFS_WARNINGS})
    add_test(NAME mirror_on_dash COMMAND test_mirror_on_dash)
    add_executable(test_modern_road tests/test_modern_road.c game/viewer/modern_road.c)
    target_include_directories(test_modern_road PRIVATE game)
    target_link_libraries(test_modern_road PRIVATE tnfs_game)
    target_compile_options(test_modern_road PRIVATE ${TNFS_WARNINGS})
    add_test(NAME modern_road COMMAND test_modern_road)
    # port: the render bands' exactness (a clipped draw equals the unclipped one inside the clip): the viewer's rasterizers built in
    add_executable(test_band_exact tests/test_band_exact.c game/viewer/modern_road.c game/viewer/sky_smooth.c game/viewer/horizon_smooth.c)
    target_include_directories(test_band_exact PRIVATE game)
    target_link_libraries(test_band_exact PRIVATE tnfs_game)
    target_compile_options(test_band_exact PRIVATE ${TNFS_WARNINGS})
    add_test(NAME band_exact COMMAND test_band_exact)
    add_executable(test_modern_style tests/test_modern_style.c game/viewer/modern_road.c)
    target_include_directories(test_modern_style PRIVATE game)
    target_link_libraries(test_modern_style PRIVATE tnfs_game)
    target_compile_options(test_modern_style PRIVATE ${TNFS_WARNINGS})
    add_test(NAME modern_style COMMAND test_modern_style)
    add_executable(test_modern_look tests/test_modern_look.c game/viewer/modern_look.c game/viewer/modern_road.c)
    target_include_directories(test_modern_look PRIVATE game)
    target_link_libraries(test_modern_look PRIVATE tnfs_game)
    target_compile_options(test_modern_look PRIVATE ${TNFS_WARNINGS})
    add_test(NAME modern_look COMMAND test_modern_look)
    add_executable(test_horizon_smooth tests/test_horizon_smooth.c game/viewer/horizon_smooth.c)
    target_include_directories(test_horizon_smooth PRIVATE game)
    target_link_libraries(test_horizon_smooth PRIVATE tnfs_game)
    target_compile_options(test_horizon_smooth PRIVATE ${TNFS_WARNINGS})
    add_test(NAME horizon_smooth COMMAND test_horizon_smooth)
    add_executable(test_sky_smooth tests/test_sky_smooth.c game/viewer/sky_smooth.c game/viewer/horizon_smooth.c)
    target_include_directories(test_sky_smooth PRIVATE game)
    target_link_libraries(test_sky_smooth PRIVATE tnfs_game)
    target_compile_options(test_sky_smooth PRIVATE ${TNFS_WARNINGS})
    add_test(NAME sky_smooth COMMAND test_sky_smooth)
    add_executable(test_drive_bumper tests/test_drive_bumper.c game/viewer/drive_bumper.c game/gfx/blit.c)
    target_include_directories(test_drive_bumper PRIVATE game)
    target_link_libraries(test_drive_bumper PRIVATE tnfs_game)
    target_compile_options(test_drive_bumper PRIVATE ${TNFS_WARNINGS})
    add_test(NAME drive_bumper COMMAND test_drive_bumper)

    add_executable(test_car_render_sides tests/test_car_render_sides.c)
    target_include_directories(test_car_render_sides PRIVATE game)
    target_link_libraries(test_car_render_sides PRIVATE tnfs_game)
    target_compile_options(test_car_render_sides PRIVATE ${TNFS_WARNINGS})
    add_test(NAME car_render_sides COMMAND test_car_render_sides)
    add_executable(test_modern_objects tests/test_modern_objects.c game/viewer/object_sink.c game/viewer/modern_road.c)
    target_include_directories(test_modern_objects PRIVATE game)
    target_link_libraries(test_modern_objects PRIVATE tnfs_game)
    target_compile_options(test_modern_objects PRIVATE ${TNFS_WARNINGS})
    add_test(NAME modern_objects COMMAND test_modern_objects)
    add_executable(test_near_scenery tests/test_near_scenery.c)
    target_include_directories(test_near_scenery PRIVATE game)
    target_link_libraries(test_near_scenery PRIVATE tnfs_game)
    target_compile_options(test_near_scenery PRIVATE ${TNFS_WARNINGS})
    add_test(NAME near_scenery COMMAND test_near_scenery)
    add_executable(test_object_sink tests/test_object_sink.c game/viewer/object_sink.c game/viewer/modern_road.c)
    target_include_directories(test_object_sink PRIVATE game)
    target_link_libraries(test_object_sink PRIVATE tnfs_game)
    target_compile_options(test_object_sink PRIVATE ${TNFS_WARNINGS})
    add_test(NAME object_sink COMMAND test_object_sink)
    add_executable(test_layer_compose tests/test_layer_compose.c game/viewer/layer_compose.c)
    target_include_directories(test_layer_compose PRIVATE game)
    target_link_libraries(test_layer_compose PRIVATE tnfs_game)
    target_compile_options(test_layer_compose PRIVATE ${TNFS_WARNINGS})
    add_test(NAME layer_compose COMMAND test_layer_compose)

    add_executable(test_central_opponent tests/test_central_opponent.c)
    target_include_directories(test_central_opponent PRIVATE game .)
    target_link_libraries(test_central_opponent PRIVATE tnfs_game)
    target_compile_options(test_central_opponent PRIVATE ${TNFS_WARNINGS})
    add_test(NAME central_opponent COMMAND test_central_opponent)
    add_executable(test_central_warrior_keep tests/test_central_warrior_keep.c)
    target_include_directories(test_central_warrior_keep PRIVATE game .)
    target_link_libraries(test_central_warrior_keep PRIVATE tnfs_game)
    target_compile_options(test_central_warrior_keep PRIVATE ${TNFS_WARNINGS})
    add_test(NAME central_warrior_keep COMMAND test_central_warrior_keep)
    add_executable(test_central_banner tests/test_central_banner.c)
    target_include_directories(test_central_banner PRIVATE game .)
    target_link_libraries(test_central_banner PRIVATE tnfs_game)
    target_compile_options(test_central_banner PRIVATE ${TNFS_WARNINGS})
    add_test(NAME central_banner COMMAND test_central_banner)
    add_executable(test_cheat_switches tests/test_cheat_switches.c)
    target_include_directories(test_cheat_switches PRIVATE game .)
    target_link_libraries(test_cheat_switches PRIVATE tnfs_game)
    target_compile_options(test_cheat_switches PRIVATE ${TNFS_WARNINGS})
    add_test(NAME cheat_switches COMMAND test_cheat_switches)
    add_executable(test_config_name tests/test_config_name.c game/frontend/config.c)
    target_include_directories(test_config_name PRIVATE game .)
    target_link_libraries(test_config_name PRIVATE tnfs_game)
    target_compile_options(test_config_name PRIVATE ${TNFS_WARNINGS})
    add_test(NAME config_name COMMAND test_config_name)
    add_executable(test_chase_cycle tests/test_chase_cycle.c)
    target_include_directories(test_chase_cycle PRIVATE game .)
    target_link_libraries(test_chase_cycle PRIVATE tnfs_game)
    target_compile_options(test_chase_cycle PRIVATE ${TNFS_WARNINGS})
    add_test(NAME chase_cycle COMMAND test_chase_cycle)
    add_executable(test_race_detail tests/test_race_detail.c)
    target_include_directories(test_race_detail PRIVATE game .)
    target_link_libraries(test_race_detail PRIVATE tnfs_game)
    target_compile_options(test_race_detail PRIVATE ${TNFS_WARNINGS})
    add_test(NAME race_detail COMMAND test_race_detail)
    add_executable(test_drive_slide tests/test_drive_slide.c game/viewer/drive_slide.c)
    target_include_directories(test_drive_slide PRIVATE game . core)
    target_link_libraries(test_drive_slide PRIVATE tnfs_game)
    target_compile_options(test_drive_slide PRIVATE ${TNFS_WARNINGS})
    add_test(NAME drive_slide COMMAND test_drive_slide)
    add_executable(test_race_stats tests/test_race_stats.c game/viewer/race_stats.c)
    target_include_directories(test_race_stats PRIVATE game .)
    target_link_libraries(test_race_stats PRIVATE tnfs_game)
    target_compile_options(test_race_stats PRIVATE ${TNFS_WARNINGS})
    add_test(NAME race_stats COMMAND test_race_stats)
    add_executable(test_drive_clock tests/test_drive_clock.c game/viewer/drive_clock.c)
    target_include_directories(test_drive_clock PRIVATE game .)
    target_link_libraries(test_drive_clock PRIVATE tnfs_game)
    target_compile_options(test_drive_clock PRIVATE ${TNFS_WARNINGS})
    add_test(NAME drive_clock COMMAND test_drive_clock)
    add_executable(test_draw_rands tests/test_draw_rands.c game/viewer/draw_rands.c)
    target_include_directories(test_draw_rands PRIVATE game .)
    target_link_libraries(test_draw_rands PRIVATE tnfs_game)
    target_compile_options(test_draw_rands PRIVATE ${TNFS_WARNINGS})
    add_test(NAME draw_rands COMMAND test_draw_rands)
    add_executable(test_cockpit_lights tests/test_cockpit_lights.c)
    target_include_directories(test_cockpit_lights PRIVATE game .)
    target_link_libraries(test_cockpit_lights PRIVATE tnfs_game)
    target_compile_options(test_cockpit_lights PRIVATE ${TNFS_WARNINGS})
    add_test(NAME cockpit_lights COMMAND test_cockpit_lights)
    add_executable(test_drive_interp tests/test_drive_interp.c game/viewer/drive_interp.c)
    target_include_directories(test_drive_interp PRIVATE game)
    target_link_libraries(test_drive_interp PRIVATE tnfs_game)
    target_compile_options(test_drive_interp PRIVATE ${TNFS_WARNINGS})
    add_test(NAME drive_interp COMMAND test_drive_interp)
    add_executable(test_video_clip tests/test_video_clip.c game/sys/video.c)
    target_include_directories(test_video_clip PRIVATE game)
    target_link_libraries(test_video_clip PRIVATE tnfs_game)
    target_compile_options(test_video_clip PRIVATE ${TNFS_WARNINGS})
    add_test(NAME video_clip COMMAND test_video_clip)
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

    file(GLOB TNFS_REPLAY_FILES "${CMAKE_SOURCE_DIR}/original/GAMEDATA/REPLAY/*.RPL")
    if(TNFS_REPLAY_FILES)
        add_test(NAME replay_image_cd COMMAND test_replay_image ${TNFS_REPLAY_FILES})
        add_test(NAME replay_launch_cd COMMAND test_replay_launch ${TNFS_REPLAY_FILES})
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
    # port: the job pool and the row splitter of the render bands
    add_executable(test_bands tests/test_bands.c game/sys/bands.c game/platform/sdl3/platform_sdl3.c)
    target_link_libraries(test_bands PRIVATE tnfs_game SDL3::SDL3)
    if(NOT MSVC)
        target_link_libraries(test_bands PRIVATE m)
    endif()
    target_compile_options(test_bands PRIVATE ${TNFS_WARNINGS})
    add_test(NAME bands COMMAND test_bands)
    add_executable(test_platform_mods tests/test_platform_mods.c game/platform/sdl3/platform_sdl3.c)
    target_link_libraries(test_platform_mods PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_platform_mods PRIVATE m)
    endif()
    target_compile_options(test_platform_mods PRIVATE ${TNFS_WARNINGS})
    add_test(NAME platform_mods COMMAND test_platform_mods)
    add_executable(test_platform_keys tests/test_platform_keys.c game/platform/sdl3/platform_sdl3.c)
    target_link_libraries(test_platform_keys PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_platform_keys PRIVATE m)
    endif()
    target_compile_options(test_platform_keys PRIVATE ${TNFS_WARNINGS})
    add_test(NAME platform_keys COMMAND test_platform_keys)
    add_executable(test_platform_text_input tests/test_platform_text_input.c game/platform/sdl3/platform_sdl3.c)
    target_link_libraries(test_platform_text_input PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_platform_text_input PRIVATE m)
    endif()
    target_compile_options(test_platform_text_input PRIVATE ${TNFS_WARNINGS})
    add_test(NAME platform_text_input COMMAND test_platform_text_input)
    add_executable(test_platform_cursor tests/test_platform_cursor.c game/platform/sdl3/platform_sdl3.c)
    target_link_libraries(test_platform_cursor PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_platform_cursor PRIVATE m)
    endif()
    target_compile_options(test_platform_cursor PRIVATE ${TNFS_WARNINGS})
    add_test(NAME platform_cursor COMMAND test_platform_cursor)
    add_executable(test_platform_display tests/test_platform_display.c game/platform/sdl3/platform_sdl3.c)
    target_link_libraries(test_platform_display PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_platform_display PRIVATE m)
    endif()
    target_compile_options(test_platform_display PRIVATE ${TNFS_WARNINGS})
    add_test(NAME platform_display COMMAND test_platform_display)
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
        add_executable(test_replay_cheats_sim tests/test_replay_cheats_sim.c)
        target_link_libraries(test_replay_cheats_sim PRIVATE tnfs_game)
        target_compile_options(test_replay_cheats_sim PRIVATE ${TNFS_WARNINGS})
        add_test(NAME replay_cheats_sim COMMAND test_replay_cheats_sim "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_replay_nitro_sim tests/test_replay_nitro_sim.c)
        target_link_libraries(test_replay_nitro_sim PRIVATE tnfs_game)
        target_compile_options(test_replay_nitro_sim PRIVATE ${TNFS_WARNINGS})
        add_test(NAME replay_nitro_sim COMMAND test_replay_nitro_sim "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_keyboard_race_sim tests/test_keyboard_race_sim.c)
        target_link_libraries(test_keyboard_race_sim PRIVATE tnfs_game)
        target_compile_options(test_keyboard_race_sim PRIVATE ${TNFS_WARNINGS})
        add_test(NAME keyboard_race_sim COMMAND test_keyboard_race_sim "${CMAKE_SOURCE_DIR}/original/SIMDATA")
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
    add_executable(probe_frontend tools/probe_frontend.c)
    target_link_libraries(probe_frontend PRIVATE tnfs_game)
    target_compile_options(probe_frontend PRIVATE ${TNFS_WARNINGS})
    add_executable(probe_input tools/probe_input.c tools/probe_race_menu.c tools/probe_race_net.c tools/probe_race_session.c
        tools/probe_transport.c tools/probe_packet_open.c tools/probe_net_request.c
        tools/probe_net_service.c tools/probe_net_serve.c tools/probe_net_link.c
        tools/probe_net_pktdata.c tools/probe_net_serial.c)
    target_link_libraries(probe_input PRIVATE tnfs_game)
    target_compile_options(probe_input PRIVATE ${TNFS_WARNINGS})
    add_executable(probe_needle tools/probe_needle.c)
    target_link_libraries(probe_needle PRIVATE tnfs_game)
    target_compile_options(probe_needle PRIVATE ${TNFS_WARNINGS})
    add_executable(probe_movie tools/probe_movie.c)
    target_link_libraries(probe_movie PRIVATE tnfs_game)
    target_compile_options(probe_movie PRIVATE ${TNFS_WARNINGS})
    add_executable(probe_palette tools/probe_palette.c)
    add_executable(probe_config tools/probe_config.c)
    target_link_libraries(probe_config PRIVATE tnfs_game)
    target_compile_options(probe_config PRIVATE ${TNFS_WARNINGS})
    target_link_libraries(probe_palette PRIVATE tnfs_game)
    target_compile_options(probe_palette PRIVATE ${TNFS_WARNINGS})
    add_executable(probe_dash tools/probe_dash.c)
    target_link_libraries(probe_dash PRIVATE tnfs_game)
    target_compile_options(probe_dash PRIVATE ${TNFS_WARNINGS})
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
