cmake_minimum_required(VERSION 3.24)
project(tnfs VERSION 0.9.1 LANGUAGES C)

# The version players see: the git tag when the tree sits on one (v0.9.0 -> 0.9.0), else
# <project version>-dev+<short hash>. TNFS_VERSION_OVERRIDE (e.g. from CI) wins over both.
set(TNFS_VERSION "${PROJECT_VERSION}-dev")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags --exact-match --match "v[0-9]*"
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE TNFS_GIT_TAG
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET RESULT_VARIABLE TNFS_GIT_TAG_RC)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE TNFS_GIT_HASH
                    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(TNFS_GIT_TAG_RC EQUAL 0 AND TNFS_GIT_TAG)
        string(REGEX REPLACE "^v" "" TNFS_VERSION "${TNFS_GIT_TAG}")
    elseif(TNFS_GIT_HASH)
        set(TNFS_VERSION "${PROJECT_VERSION}-dev+${TNFS_GIT_HASH}")
    endif()
endif()
if(TNFS_VERSION_OVERRIDE)
    set(TNFS_VERSION "${TNFS_VERSION_OVERRIDE}")
endif()
# The version goes into a C string and the update check's SemVer order: MAJOR.MINOR.PATCH[-pre][+build], plain.
if(NOT TNFS_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+(-[0-9A-Za-z.]+)?(\\+[0-9A-Za-z.]+)?$")
    message(FATAL_ERROR "TNFS_VERSION '${TNFS_VERSION}' is not MAJOR.MINOR.PATCH[-pre][+build]")
endif()
string(REGEX REPLACE "[-+].*$" "" TNFS_VERSION_CORE "${TNFS_VERSION}")
if(NOT TNFS_VERSION_CORE VERSION_EQUAL PROJECT_VERSION)
    if(TNFS_RELEASE)
        message(FATAL_ERROR "version ${TNFS_VERSION} (tag) and project(VERSION ${PROJECT_VERSION}) disagree: bump project()")
    else()
        message(WARNING "version ${TNFS_VERSION} (tag) and project(VERSION ${PROJECT_VERSION}) disagree")
    endif()
endif()
message(STATUS "tnfs version: ${TNFS_VERSION}")
configure_file(${CMAKE_SOURCE_DIR}/game/tnfs_version.h.in ${CMAKE_BINARY_DIR}/generated/tnfs_version.h @ONLY)

# Release builds (the packaged app) never look in the source tree for data.
option(TNFS_RELEASE "Build for distribution: no source-tree data fallbacks" OFF)
option(TNFS_PROBES "Build the exe-comparison probes (tools/probe_*.c)" OFF)

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
    core/formats/psx_bigf.c
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
    core/update/update_check.c
    core/iso/iso9660.c
    core/formats/tri_exchange.c
    core/formats/track_gen.c
    core/formats/tri_reverse.c
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
    game/frontend/best_times_pages.c
    game/frontend/cheat_code.c
    game/frontend/config_dat.c
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
    game/frontend/showcase_variants.c
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
    game/frontend/records_import.c
    game/frontend/load_save2.c
    game/frontend/net_game.c
    game/frontend/net_host.c
    game/frontend/central.c
    game/frontend/central_banner.c
    game/frontend/boot_screen.c
    game/frontend/boot_screen_glue.c
    game/frontend/screen_hold.c
    game/frontend/boot_relief.c
    game/frontend/boot_relief_glue.c
    game/frontend/boot_relief_ops.c
    game/frontend/boot_quant.c
    game/frontend/credits_pages.c
    game/frontend/credits_pages_glue.c
    game/frontend/credits_relief.c
    game/frontend/version_footer.c
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
    game/frontend/tgv_wide.c
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
    game/gfx/raster_record.c
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
    game/audio/thunder.c
    game/audio/wet_roll.c
    game/audio/splash.c
    game/audio/rain_bus.c
    game/audio/hush.c
    game/race/rain_roll.c
    game/race/rain_cars.c
    game/race/wrong_way.c
    game/race/snow_roll.c
    game/race/weather_snow.c
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
    game/race/session_rules.c
    game/race/race_view_pick.c
    game/race/replay_panel.c
    game/race/dash_controls.c
    game/race/car_body_draw.c
    game/race/car_lights.c
    game/race/lamp_flare.c
    game/viewer/shade_fit.c
    game/race/car_render.c
    game/race/rigid_boxes.c
    game/race/car_collide.c
    game/race/race_contacts.c
    game/race/race_finish.c
    game/race/race_camera.c
    game/race/race_forces.c
    game/race/tach_dim.c
    game/race/wheel_front.c
    game/race/race_results.c
    game/race/race_view.c
    game/race/race_chase.c
    game/race/follow_yaw.c
    game/race/race_follow.c
    game/race/race_splits.c
    game/race/ai_model.c
    game/race/ai_parked.c
    game/race/ai_spun.c
    game/race/ai_setup.c
    game/race/ai_zones.c
    game/race/car_held.c
    game/race/rigid_body.c
    game/race/car_control.c
    game/race/analog_pad.c
    game/race/ai_drive.c
    game/race/race_sim.c
    game/sys/rigid_math.c
    game/race/car_tyres.c
    game/race/car_vertical.c
    game/race/car_walls.c
    game/race/car_model.c
    game/race/car_livery.c
    game/race/car_variant.c
    game/race/car_paint.c
    game/race/car_repaint.c
    game/race/car_mask.c
    game/race/track_mask.c
    game/race/dash_mask.c
    game/race/dash_nose.c
    game/race/dash_color.c
    game/sys/data_path.c
    game/sys/art_cache.c
    game/sys/sha256.c
    game/sys/data_check.c
    game/sys/file_url.c
    game/sys/data_manifest.c
    game/exe/exe_image.c
    game/exe/exe_tables.c
    game/exe/exe_tables_menu.c
    game/exe/exe_tables_config.c
    game/exe/exe_tables_ai.c
    game/exe/exe_tables_split.c
    game/exe/exe_tables_view.c
    game/exe/exe_tables_track.c
    game/sys/data_copy.c
    game/sys/data_root.c
    game/race/lamp_emissive.c
    game/race/car_shine.c
    game/race/car_envmap.c
    game/race/track_node_light.c
    game/race/light_synth.c
    game/race/track_light_file.c
    game/race/track_light_find.c
    game/race/terrain_light.c
    game/race/headlight_pool.c
    game/race/street_lamp.c
    game/race/tunnel_lamps.c
    game/race/window_lights.c
    game/race/reflector.c
    game/race/spray_tint.c
    game/race/wiper.c
    game/race/finish_gate_light.c
    game/race/tunnel_lamp_textures.c
    game/race/police_lights.c
    game/race/psx_lgs.c
    game/race/psx_lgt.c
    game/race/light_tint.c
    game/race/psx_sky.c
    game/race/psx_sky_bitmap.c
    game/race/car_wheels.c
    game/race/race_grid.c
    game/race/race_models.c
    game/race/single_rival.c
    game/race/race_sound.c
    game/race/race_sound_engine.c
    game/race/race_sound_tyres.c
    game/race/race_sound_others.c
    game/race/race_sound_music.c
    game/race/race_palette.c
    game/race/car_colours.c
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
    game/race/race_menu.c
    game/race/race_net.c
    game/race/race_net_receive.c
    game/race/race_net_setup.c
    game/race/horizon.c
    game/race/sky_stretch.c
    game/race/model_draw.c
    game/race/race_state.c
    game/race/sky_ring.c
    game/race/weather.c
    game/race/wet_zones.c
    game/viewer/wet_zones_load.c
    game/race/rain_spray.c
    game/race/spray_pace.c
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
    game/net/net_udp.c
    game/net/net_udp_service.c
    game/net/net_runtime.c
    game/race/race_net_services.c
    game/race/race_net_players.c
    game/race/race_net_games.c
    game/race/replay_controls.c
    game/race/track_far.c
    game/race/track_fence.c
    game/race/track_wall.c
    game/race/track_near.c
    game/race/track_object_draw.c
    game/race/track_draw.c
    game/race/track_geometry.c
    game/race/track_fam.c
    game/race/lunar_springs.c
    game/race/difficulty.c
    game/race/track_gravity.c
    game/race/rally_gravel.c
    game/race/dust_trail.c
    game/race/fam_mirror.c
    game/race/track_info.c
    game/race/track_object_walk.c
    game/race/track_objects.c
    game/race/track_render.c
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
    game/sys/key_latch.c
    game/sys/page_keys.c
    game/sys/menu_key.c
    game/sys/palette.c
    game/sys/text_soft.c
    game/sys/port_settings.c
    game/sys/port_settings_store.c
    game/sys/port_follow.c
    game/sys/port_settings_scene.c
    game/sys/port_settings_effects.c
    game/sys/port_old_files.c
    game/sys/port_profile.c
    game/sys/free_laps.c
    game/sys/reverse_track.c
    game/sys/port_extras.c
    game/sys/port_stats.c
    game/sys/port_icon.c
    game/sys/port_icon_ours.c
    game/sys/port_controls.c
    game/sys/race_records.c
    game/sys/control_map.c
    game/sys/pointer_state.c
    game/sys/shape_bank.c
    game/viewer/screen_layout.c
    game/viewer/dash_box.c
    game/viewer/drive_livery.c
    game/viewer/drive_lamps.c
    game/viewer/reflect_pass.c
    game/viewer/street_halo.c
    game/viewer/spray_v4_switch.c
    game/viewer/halo_track.c
    game/viewer/car_lamp_halo.c
    game/viewer/weather_sky.c
    game/viewer/weather_hooks.c
    game/viewer/weather_rain.c
    game/viewer/drive_light.c
    game/viewer/drive_light_sky.c
    game/viewer/drive_light_night.c
    game/viewer/class_overlay.c
    game/viewer/class_cache.c
    game/sys/backdrop.c
    game/frontend/variant_rule.c
    game/sys/resource.c
)
target_include_directories(tnfs_game PUBLIC game . ${CMAKE_BINARY_DIR}/generated)
if(NOT TNFS_RELEASE)
    target_compile_definitions(tnfs_game PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")   # car_mask.c: the source tree's data/car_masks
endif()
target_link_libraries(tnfs_game PUBLIC tnfs_core)
target_sources(tnfs_game PRIVATE game/exe/exe_tables_store.c)   # the exe's tables live here, filled at start-up (game/exe/exe_tables.h)
target_compile_options(tnfs_game PRIVATE ${TNFS_WARNINGS})
# The recovered game code keeps the original's 32-bit wraparound arithmetic.
if(NOT MSVC)
    target_compile_options(tnfs_game PRIVATE -fwrapv)
endif()

set(TNFS_SDL3_PLATFORM
    game/platform/sdl3/platform_sdl3.c
    game/platform/sdl3/platform_audio_sdl3.c
    game/platform/sdl3/platform_input_sdl3.c
    game/platform/sdl3/platform_mods_sdl3.c
    game/platform/sdl3/platform_user_sdl3.c
    game/platform/sdl3/platform_present_sdl3.c
    game/platform/sdl3/platform_pool_sdl3.c)   # platform_sdl3.c split by responsibility; the tests that link the platform list them all
add_executable(tnfs
    game/main.c
    game/frontend/config.c
    game/frontend/ea_lib.c
    game/frontend/frontend.c
    game/frontend/menu_glue.c
    game/frontend/menu_glue_dialogs.c
    game/frontend/menu_glue_extras.c
    game/frontend/menu_glue_screens.c
    game/frontend/link_game.c
    game/frontend/link_messages.c
    game/frontend/link_session.c
    game/frontend/link_central.c
    game/frontend/link_race.c
    game/frontend/link_rules.c
    game/frontend/central_glue.c
    game/frontend/central_glue_games.c
    game/frontend/central_glue_results.c
    game/frontend/central_glue_race.c
    game/frontend/central_glue_options.c
    game/frontend/central_glue_plus.c
    game/frontend/central_pages.c
    game/frontend/central_options_text.c
    game/frontend/menu_audio.c
    game/frontend/screens.c
    game/sys/input.c
    game/sys/bands.c
    game/sys/band_index.c
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
    game/viewer/light_edit.c
    game/viewer/race_stats.c
    game/viewer/race_stats_draw.c
    game/viewer/wrong_way_draw.c
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
    game/viewer/cone_cells.c
    game/viewer/tunnel_cells.c
    game/viewer/tunnel_cells_draw.c
    game/viewer/modern_look.c
    game/viewer/weather_draw.c
    game/viewer/wiper_draw.c
    game/viewer/wiper_scene.c
    game/viewer/spray_v4_draw.c
    game/viewer/spray_v4_art.c
    game/race/spray_v4.c
    game/viewer/horizon_smooth.c
    game/viewer/sky_art_draw.c
    game/viewer/sky_smooth.c
    game/viewer/drive_bumper.c
    game/viewer/model_view.c
    game/viewer/object_sink.c
    game/viewer/shine_draw.c
    game/viewer/shine_class.c
    game/viewer/body_shine.c
    game/viewer/soft_shadow.c
    game/viewer/ported_view.c
    game/viewer/view_lamps.c
    game/viewer/view_pick.c
    game/viewer/view_screen.c
    game/viewer/view_spray.c
    game/viewer/view_rain.c
    game/viewer/view_cars.c
    game/viewer/view_tags.c
    game/viewer/view_sky.c
    game/viewer/view_frame.c
    game/viewer/view_smoke.c
    game/viewer/view_models.c
    game/viewer/view_light_tables.c
    game/viewer/view_light.c
    game/viewer/view_tables.c
    game/viewer/view_weather.c
    game/viewer/follow_arm.c
    game/viewer/follow_behind.c
    game/viewer/ported_viewport.c
    game/viewer/raster.c
    game/viewer/track_load.c
    game/viewer/scenery_dump.c
    game/viewer/track_fly.c
    game/viewer/drive_wide.c
    game/viewer/drive_wide_dump.c
    game/viewer/drive_wide_wipers.c
    game/viewer/drive_draw.c
    game/viewer/drive_bumper_draw.c
    game/viewer/drive_run.c
    game/viewer/drive_run_shot.c
    game/viewer/track_viewer.c
    game/viewer/track_viewer_cars.c
    game/viewer/viewer_main.c
    ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c
    game/platform/sdl3/first_run_sdl3.c
    game/platform/sdl3/platform_joy_sdl3.c
    game/platform/sdl3/update_sdl3.c
)
if(NOT TNFS_RELEASE)
    target_compile_definitions(tnfs PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")   # main.c: a development build run from the repository takes ./original when there is no --data
endif()
target_link_libraries(tnfs PRIVATE tnfs_game)
if(WIN32)   # the program's icon (Explorer, the taskbar): data/icon/spirited.ico as a resource
    set(TNFS_ICON_ICO "${CMAKE_SOURCE_DIR}/data/icon/spirited.ico")
    configure_file(game/sys/spirited.rc.in "${CMAKE_BINARY_DIR}/generated/spirited.rc" @ONLY)
    target_sources(tnfs PRIVATE "${CMAKE_BINARY_DIR}/generated/spirited.rc")
endif()
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

# The drafts of the hand-made car masks (tools/car_mask_draft.c): reads the CD's CARFAMS, writes PNGs.
if(NOT EMSCRIPTEN)
    add_executable(car_mask_draft tools/car_mask_draft.c game/viewer/drive_livery.c)
    target_link_libraries(car_mask_draft PRIVATE tnfs_game)
    target_compile_options(car_mask_draft PRIVATE ${TNFS_WARNINGS})
endif()

# The textures a track's reflective mask is painted over (tools/track_mask_draft.c): reads the CD's ETRACKFM, writes PNGs.
if(NOT EMSCRIPTEN)
    add_executable(track_mask_draft tools/track_mask_draft.c)
    target_link_libraries(track_mask_draft PRIVATE tnfs_game)
    target_compile_options(track_mask_draft PRIVATE ${TNFS_WARNINGS})
endif()

# The first yellow layer of the track masks from the built-in lamp table (tools/track_lamp_marks.c): reads the CD's ETRACKFM, writes the slot masks' PNGs.
if(NOT EMSCRIPTEN)
    add_executable(track_lamp_marks tools/track_lamp_marks.c)
    target_link_libraries(track_lamp_marks PRIVATE tnfs_game)
    target_compile_options(track_lamp_marks PRIVATE ${TNFS_WARNINGS})
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
    if(NOT WIN32)
        add_test(NAME try_online_local COMMAND ${CMAKE_COMMAND} -DREPO=${CMAKE_SOURCE_DIR} -DWORK=${CMAKE_BINARY_DIR}/try_online_local -P ${CMAKE_SOURCE_DIR}/tests/try_online_local.cmake)
    endif()
    foreach(name qfs qfs_encode doc fsh ffn ico chunk mesh pbs pdn bnk tri tri_write tri_reverse tsv png fileset shpi_exchange table_exchange tri_exchange wav bnk_exchange mesh_exchange resource_exchange validate modpath slots track_gen psx_bigf update_check)
        add_executable(test_${name} tests/test_${name}.c)
        target_link_libraries(test_${name} PRIVATE tnfs_core)
        target_compile_options(test_${name} PRIVATE ${TNFS_WARNINGS})
        add_test(NAME ${name} COMMAND test_${name})
    endforeach()
foreach(name port_old_files band_textures single_rival palette race_settings config_dat sky_ring quad object_mappers flat palette_match music_reader net_udp net_udp_service net_runtime replay_image tgv_frame win_movie port_settings port_settings_values port_settings_session screen_layout backdrop graphics_plus page_keys controls_plus port_controls control_map port_extras extras_apply race_records records_import records_glue_load records_route track_select_plus extras_menu extras_background extras_layout extras_focus options_extras track_near race_pause_resume_esc race_replay_esc race_replay_resume track_palette_repair race_music_stop mirror_and_rally graphics_polish fam_mirror config_bgnd object3d_mirror object3d_both_sides object3d_close_houses race_keys_free text_soft graphics_own graphics_own_modes graphics_own_light f3_ultra title_art item_art page_arrows port_log speed_units best_times best_times_pages start_keys race_forces results_marks standings_cheats lunar_springs lunar_gravity intro_path intro_skip intro_order race_horn_voice replay_launch pad_and_keys joy_pad menu_song dash_box mirror_rows tall_target free_laps free_laps_guards second_livery car_variant car_paint car_repaint car_mask track_mask car_stem car_paint_cfm car_colours car_paint_dash showcase_variants track_node_light tunnel_lamp_textures remap_mode psx_lgs psx_sky psx_sky_bitmap weather track_select_screen car_shine car_envmap object3d_normals sky_stretch car_distance difficulty light_synth lamp_emissive drive_lamps headlight_pool street_lamp tunnel_lamps window_lights reflector reflect_pass street_halo halo_track weather_hooks spray_tint wiper finish_gate_light police_lights analog_pad rally_gravel rally_gravel_volume dash_mask dash_nose dash_color lamp_flare shade_fit thunder wet_roll splash rain_bus hush rain_roll rain_cars wrong_way snow_roll weather_snow snow_fx snow_remap key_latch track_wall chase_slope data_path data_check terrain_light port_profile live_mode_switch title_press game_mode_profile exe_image exe_tables_menu exe_tables_config exe_tables_load file_url tach_dim wheel_front rain_spray spray_pace track_light_file session_rules race_net_wire port_icon_ours)
        add_executable(test_${name} tests/test_${name}.c)
        target_link_libraries(test_${name} PRIVATE tnfs_game)
        target_compile_options(test_${name} PRIVATE ${TNFS_WARNINGS})
        add_test(NAME ${name} COMMAND test_${name})
    endforeach()
    # the ISO reader on synthetic images (tests/iso_build.c writes them)
    add_executable(test_iso9660 tests/test_iso9660.c tests/iso_build.c)
    target_link_libraries(test_iso9660 PRIVATE tnfs_core)
    target_compile_options(test_iso9660 PRIVATE ${TNFS_WARNINGS})
    add_test(NAME iso9660 COMMAND test_iso9660)
    # the CD check of the clean configuration background runs when original/ is there
    target_compile_definitions(test_config_bgnd PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_car_paint_cfm PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_rain_roll PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_snow_roll PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_drive_lamps PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_car_colours PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_car_paint_dash PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_title_art PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_item_art PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_page_arrows PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_port_icon_ours PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_sources(test_data_check PRIVATE tests/iso_build.c)   # the synthetic images of the ISO checks
    target_compile_definitions(test_data_check PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_exe_image PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_exe_tables_menu PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_exe_tables_config PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_compile_definitions(test_exe_tables_load PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    target_include_directories(test_exe_tables_load PRIVATE tests)
    target_include_directories(test_exe_tables_menu PRIVATE tests)
    target_include_directories(test_exe_tables_config PRIVATE tests)
    # the exe-table extractors (game/exe/exe_tables_<family>.c): each against the table the port still holds
    foreach(name exe_tables_ai exe_tables_split exe_tables_view exe_tables_track)
        add_executable(test_${name} tests/test_${name}.c)
        target_link_libraries(test_${name} PRIVATE tnfs_game)
        target_compile_options(test_${name} PRIVATE ${TNFS_WARNINGS})
        target_compile_definitions(test_${name} PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
        add_test(NAME ${name} COMMAND test_${name})
    endforeach()
    # port: the mirror kept on the dash (drive_mirror.c, the viewer's: built into the test with its source)
    add_executable(test_mirror_on_dash tests/test_mirror_on_dash.c game/viewer/drive_mirror.c game/viewer/follow_arm.c game/viewer/follow_behind.c game/viewer/modern_road.c
                   game/sys/bands.c game/sys/band_index.c ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
    target_include_directories(test_mirror_on_dash PRIVATE game)
    target_link_libraries(test_mirror_on_dash PRIVATE tnfs_game SDL3::SDL3)
    target_compile_options(test_mirror_on_dash PRIVATE ${TNFS_WARNINGS})
    add_test(NAME mirror_on_dash COMMAND test_mirror_on_dash)
    # the screen picture's pool across races (ea_lib's pictures, built into the test with its source)
    add_executable(test_screen_pool tests/test_screen_pool.c game/frontend/ea_lib.c)
    target_link_libraries(test_screen_pool PRIVATE tnfs_game)
    target_compile_options(test_screen_pool PRIVATE ${TNFS_WARNINGS})
    add_test(NAME screen_pool COMMAND test_screen_pool)
    # port (render scale): the scaled viewport, built into the test with its source
    add_executable(test_render_scale tests/test_render_scale.c game/viewer/ported_viewport.c)
    target_include_directories(test_render_scale PRIVATE game)
    target_link_libraries(test_render_scale PRIVATE tnfs_game)
    target_compile_options(test_render_scale PRIVATE ${TNFS_WARNINGS})
    add_test(NAME render_scale COMMAND test_render_scale)
    # port: the tunnel lamps' module, built into the test with its source
    add_executable(test_view_lamps tests/test_view_lamps.c game/viewer/view_lamps.c)
    target_include_directories(test_view_lamps PRIVATE game)
    target_link_libraries(test_view_lamps PRIVATE tnfs_game)
    target_compile_options(test_view_lamps PRIVATE ${TNFS_WARNINGS})
    add_test(NAME view_lamps COMMAND test_view_lamps)
    add_executable(test_view_pick tests/test_view_pick.c game/viewer/view_pick.c)
    target_include_directories(test_view_pick PRIVATE game)
    target_link_libraries(test_view_pick PRIVATE tnfs_game)
    target_compile_options(test_view_pick PRIVATE ${TNFS_WARNINGS})
    add_test(NAME view_pick COMMAND test_view_pick)
    add_executable(test_class_overlay tests/test_class_overlay.c)
    target_include_directories(test_class_overlay PRIVATE game)
    target_link_libraries(test_class_overlay PRIVATE tnfs_game)
    target_compile_options(test_class_overlay PRIVATE ${TNFS_WARNINGS})
    add_test(NAME class_overlay COMMAND test_class_overlay)
    add_executable(test_class_cache tests/test_class_cache.c)
    target_include_directories(test_class_cache PRIVATE game)
    target_link_libraries(test_class_cache PRIVATE tnfs_game)
    target_compile_options(test_class_cache PRIVATE ${TNFS_WARNINGS})
    add_test(NAME class_cache COMMAND test_class_cache)
    add_executable(test_modern_road tests/test_modern_road.c game/viewer/modern_road.c)
    target_include_directories(test_modern_road PRIVATE game)
    target_link_libraries(test_modern_road PRIVATE tnfs_game)
    target_compile_options(test_modern_road PRIVATE ${TNFS_WARNINGS})
    add_test(NAME modern_road COMMAND test_modern_road)
    add_executable(test_cone_cells tests/test_cone_cells.c game/viewer/cone_cells.c game/viewer/modern_road.c)
    target_include_directories(test_cone_cells PRIVATE game)
    target_link_libraries(test_cone_cells PRIVATE tnfs_game)
    target_compile_options(test_cone_cells PRIVATE ${TNFS_WARNINGS})
    add_test(NAME cone_cells COMMAND test_cone_cells)
    add_executable(test_tunnel_cells tests/test_tunnel_cells.c game/viewer/tunnel_cells.c)
    target_include_directories(test_tunnel_cells PRIVATE game)
    target_link_libraries(test_tunnel_cells PRIVATE tnfs_game)
    target_compile_options(test_tunnel_cells PRIVATE ${TNFS_WARNINGS})
    add_test(NAME tunnel_cells COMMAND test_tunnel_cells)
    add_executable(test_weather_draw tests/test_weather_draw.c game/viewer/weather_draw.c)
    target_include_directories(test_weather_draw PRIVATE game)
    target_link_libraries(test_weather_draw PRIVATE tnfs_game)
    target_compile_options(test_weather_draw PRIVATE ${TNFS_WARNINGS})
    add_test(NAME weather_draw COMMAND test_weather_draw)
    add_executable(test_wiper_draw tests/test_wiper_draw.c game/viewer/wiper_draw.c)
    target_include_directories(test_wiper_draw PRIVATE game)
    target_link_libraries(test_wiper_draw PRIVATE tnfs_game)
    target_compile_options(test_wiper_draw PRIVATE ${TNFS_WARNINGS})
    add_test(NAME wiper_draw COMMAND test_wiper_draw)
    add_executable(test_wiper_scene tests/test_wiper_scene.c game/viewer/wiper_scene.c)
    target_include_directories(test_wiper_scene PRIVATE game)
    target_link_libraries(test_wiper_scene PRIVATE tnfs_game)
    target_compile_options(test_wiper_scene PRIVATE ${TNFS_WARNINGS})
    add_test(NAME wiper_scene COMMAND test_wiper_scene)
    add_executable(test_spray_v4 tests/test_spray_v4.c game/race/spray_v4.c game/viewer/spray_v4_art.c)
    target_include_directories(test_spray_v4 PRIVATE game)
    target_link_libraries(test_spray_v4 PRIVATE tnfs_game)
    target_compile_options(test_spray_v4 PRIVATE ${TNFS_WARNINGS})
    add_test(NAME spray_v4 COMMAND test_spray_v4)
    add_executable(test_wet_zones tests/test_wet_zones.c)
    target_include_directories(test_wet_zones PRIVATE game)
    target_link_libraries(test_wet_zones PRIVATE tnfs_game)
    target_compile_options(test_wet_zones PRIVATE ${TNFS_WARNINGS})
    add_test(NAME wet_zones COMMAND test_wet_zones)
    add_test(NAME export_keeps_edits COMMAND sh ${CMAKE_SOURCE_DIR}/tests/export_keeps_edits.sh)
    # port (emissive lamps): no tint, light table or fog table moves an emissive palette entry
    add_executable(test_palette_emissive tests/test_palette_emissive.c game/viewer/modern_road.c)
    target_include_directories(test_palette_emissive PRIVATE game)
    target_link_libraries(test_palette_emissive PRIVATE tnfs_game)
    target_compile_options(test_palette_emissive PRIVATE ${TNFS_WARNINGS})
    add_test(NAME palette_emissive COMMAND test_palette_emissive)
    # port: the render bands' exactness (a clipped draw equals the unclipped one inside the clip): the viewer's rasterizers built in
    add_executable(test_band_exact tests/test_band_exact.c game/viewer/modern_road.c game/viewer/sky_smooth.c game/viewer/horizon_smooth.c game/sys/band_index.c)
    target_include_directories(test_band_exact PRIVATE game)
    target_link_libraries(test_band_exact PRIVATE tnfs_game)
    target_compile_options(test_band_exact PRIVATE ${TNFS_WARNINGS})
    add_test(NAME band_exact COMMAND test_band_exact)
    add_executable(test_modern_style tests/test_modern_style.c game/viewer/modern_road.c)
    target_include_directories(test_modern_style PRIVATE game)
    target_link_libraries(test_modern_style PRIVATE tnfs_game)
    target_compile_options(test_modern_style PRIVATE ${TNFS_WARNINGS})
    add_test(NAME modern_style COMMAND test_modern_style)
    add_executable(test_modern_look tests/test_modern_look.c game/viewer/modern_look.c game/viewer/modern_road.c game/race/weather.c)
    target_include_directories(test_modern_look PRIVATE game)
    target_link_libraries(test_modern_look PRIVATE tnfs_game)
    target_compile_options(test_modern_look PRIVATE ${TNFS_WARNINGS})
    add_test(NAME modern_look COMMAND test_modern_look)
    add_executable(test_lamp_entries tests/test_lamp_entries.c game/viewer/modern_road.c game/viewer/horizon_smooth.c game/sys/band_index.c)
    target_include_directories(test_lamp_entries PRIVATE game)
    target_link_libraries(test_lamp_entries PRIVATE tnfs_game)
    target_compile_options(test_lamp_entries PRIVATE ${TNFS_WARNINGS})
    add_test(NAME lamp_entries COMMAND test_lamp_entries)
    add_executable(test_horizon_smooth tests/test_horizon_smooth.c game/viewer/horizon_smooth.c game/sys/band_index.c)
    target_include_directories(test_horizon_smooth PRIVATE game)
    target_link_libraries(test_horizon_smooth PRIVATE tnfs_game)
    target_compile_options(test_horizon_smooth PRIVATE ${TNFS_WARNINGS})
    add_test(NAME horizon_smooth COMMAND test_horizon_smooth)
    add_executable(test_sky_art_draw tests/test_sky_art_draw.c game/viewer/sky_art_draw.c game/viewer/horizon_smooth.c game/sys/band_index.c)
    target_include_directories(test_sky_art_draw PRIVATE game)
    target_link_libraries(test_sky_art_draw PRIVATE tnfs_game)
    target_compile_options(test_sky_art_draw PRIVATE ${TNFS_WARNINGS})
    add_test(NAME sky_art_draw COMMAND test_sky_art_draw)
    add_executable(test_sky_smooth tests/test_sky_smooth.c game/viewer/sky_smooth.c game/viewer/horizon_smooth.c game/sys/band_index.c)
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
    add_executable(test_shine_draw tests/test_shine_draw.c game/viewer/shine_draw.c)
    target_include_directories(test_shine_draw PRIVATE game)
    target_link_libraries(test_shine_draw PRIVATE tnfs_game)
    target_compile_options(test_shine_draw PRIVATE ${TNFS_WARNINGS})
    add_test(NAME shine_draw COMMAND test_shine_draw)
    add_executable(test_shine_class tests/test_shine_class.c game/viewer/shine_class.c game/viewer/shine_draw.c)
    target_include_directories(test_shine_class PRIVATE game)
    target_link_libraries(test_shine_class PRIVATE tnfs_game)
    target_compile_options(test_shine_class PRIVATE ${TNFS_WARNINGS})
    add_test(NAME shine_class COMMAND test_shine_class)
    add_executable(test_sink_shine tests/test_sink_shine.c game/viewer/object_sink.c game/viewer/modern_road.c game/viewer/shine_draw.c game/viewer/shine_class.c)
    target_include_directories(test_sink_shine PRIVATE game)
    target_link_libraries(test_sink_shine PRIVATE tnfs_game)
    target_compile_options(test_sink_shine PRIVATE ${TNFS_WARNINGS})
    add_test(NAME sink_shine COMMAND test_sink_shine)
    add_executable(test_body_shine tests/test_body_shine.c game/viewer/body_shine.c game/viewer/object_sink.c game/viewer/modern_road.c game/viewer/shine_draw.c game/viewer/shine_class.c)
    target_include_directories(test_body_shine PRIVATE game)
    target_link_libraries(test_body_shine PRIVATE tnfs_game)
    target_compile_options(test_body_shine PRIVATE ${TNFS_WARNINGS})
    add_test(NAME body_shine COMMAND test_body_shine)
    add_executable(test_modern_objects tests/test_modern_objects.c game/viewer/object_sink.c game/viewer/modern_road.c game/viewer/shine_draw.c game/viewer/shine_class.c)
    target_include_directories(test_modern_objects PRIVATE game)
    target_link_libraries(test_modern_objects PRIVATE tnfs_game)
    target_compile_options(test_modern_objects PRIVATE ${TNFS_WARNINGS})
    add_test(NAME modern_objects COMMAND test_modern_objects)
    add_executable(test_close_boxes tests/test_close_boxes.c)
    target_include_directories(test_close_boxes PRIVATE game)
    target_link_libraries(test_close_boxes PRIVATE tnfs_game)
    target_compile_options(test_close_boxes PRIVATE ${TNFS_WARNINGS})
    add_test(NAME close_boxes COMMAND test_close_boxes)
    add_executable(test_near_scenery tests/test_near_scenery.c)
    target_include_directories(test_near_scenery PRIVATE game)
    target_link_libraries(test_near_scenery PRIVATE tnfs_game)
    target_compile_options(test_near_scenery PRIVATE ${TNFS_WARNINGS})
    add_test(NAME near_scenery COMMAND test_near_scenery)
    add_executable(test_object_sink tests/test_object_sink.c game/viewer/object_sink.c game/viewer/modern_road.c game/viewer/shine_draw.c game/viewer/shine_class.c)
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
    add_executable(test_boot_screen tests/test_boot_screen.c)
    target_include_directories(test_boot_screen PRIVATE game .)
    target_link_libraries(test_boot_screen PRIVATE tnfs_game)
    target_compile_options(test_boot_screen PRIVATE ${TNFS_WARNINGS})
    add_test(NAME boot_screen COMMAND test_boot_screen)

    foreach(t boot_relief_ops boot_relief boot_quant art_cache screen_hold)
        add_executable(test_${t} tests/test_${t}.c)
        target_include_directories(test_${t} PRIVATE game .)
        target_link_libraries(test_${t} PRIVATE tnfs_game)
        target_compile_options(test_${t} PRIVATE ${TNFS_WARNINGS})
        add_test(NAME ${t} COMMAND test_${t})
    endforeach()
    add_executable(test_credits tests/test_credits.c)
    target_include_directories(test_credits PRIVATE game .)
    target_link_libraries(test_credits PRIVATE tnfs_game)
    target_compile_options(test_credits PRIVATE ${TNFS_WARNINGS})
    add_test(NAME credits COMMAND test_credits)
    add_executable(test_credits_pages tests/test_credits_pages.c)
    target_include_directories(test_credits_pages PRIVATE game .)
    target_link_libraries(test_credits_pages PRIVATE tnfs_game)
    target_compile_options(test_credits_pages PRIVATE ${TNFS_WARNINGS})
    add_test(NAME credits_pages COMMAND test_credits_pages)
    add_executable(test_credits_relief tests/test_credits_relief.c)
    target_include_directories(test_credits_relief PRIVATE game .)
    target_link_libraries(test_credits_relief PRIVATE tnfs_game)
    target_compile_options(test_credits_relief PRIVATE ${TNFS_WARNINGS})
    add_test(NAME credits_relief COMMAND test_credits_relief)
    add_executable(test_version_footer tests/test_version_footer.c)
    target_include_directories(test_version_footer PRIVATE game .)
    target_link_libraries(test_version_footer PRIVATE tnfs_game)
    target_compile_options(test_version_footer PRIVATE ${TNFS_WARNINGS})
    add_test(NAME version_footer COMMAND test_version_footer)
    add_executable(test_port_stats tests/test_port_stats.c)
    target_include_directories(test_port_stats PRIVATE game .)
    target_link_libraries(test_port_stats PRIVATE tnfs_game)
    target_compile_options(test_port_stats PRIVATE ${TNFS_WARNINGS})
    add_test(NAME port_stats COMMAND test_port_stats)
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
    add_executable(test_text_entry_pad tests/test_text_entry_pad.c)
    target_include_directories(test_text_entry_pad PRIVATE game .)
    target_link_libraries(test_text_entry_pad PRIVATE tnfs_game)
    target_compile_options(test_text_entry_pad PRIVATE ${TNFS_WARNINGS})
    add_test(NAME text_entry_pad COMMAND test_text_entry_pad)
    add_executable(test_joy_menu_key tests/test_joy_menu_key.c)
    target_include_directories(test_joy_menu_key PRIVATE game .)
    target_link_libraries(test_joy_menu_key PRIVATE tnfs_game)
    target_compile_options(test_joy_menu_key PRIVATE ${TNFS_WARNINGS})
    add_test(NAME joy_menu_key COMMAND test_joy_menu_key)
    add_executable(test_row_cars_behind tests/test_row_cars_behind.c)
    target_include_directories(test_row_cars_behind PRIVATE game .)
    target_link_libraries(test_row_cars_behind PRIVATE tnfs_game)
    target_compile_options(test_row_cars_behind PRIVATE ${TNFS_WARNINGS})
    add_test(NAME row_cars_behind COMMAND test_row_cars_behind)
    add_executable(test_track_cache_ring tests/test_track_cache_ring.c)
    target_include_directories(test_track_cache_ring PRIVATE game .)
    target_link_libraries(test_track_cache_ring PRIVATE tnfs_game)
    target_compile_options(test_track_cache_ring PRIVATE ${TNFS_WARNINGS})
    add_test(NAME track_cache_ring COMMAND test_track_cache_ring)
    add_executable(test_track_cache_window tests/test_track_cache_window.c)
    target_include_directories(test_track_cache_window PRIVATE game .)
    target_link_libraries(test_track_cache_window PRIVATE tnfs_game)
    target_compile_options(test_track_cache_window PRIVATE ${TNFS_WARNINGS})
    add_test(NAME track_cache_window COMMAND test_track_cache_window)
    add_executable(test_follow_behind tests/test_follow_behind.c game/viewer/follow_behind.c game/viewer/follow_arm.c)
    target_include_directories(test_follow_behind PRIVATE game .)
    target_link_libraries(test_follow_behind PRIVATE tnfs_game)
    target_compile_options(test_follow_behind PRIVATE ${TNFS_WARNINGS})
    add_test(NAME follow_behind COMMAND test_follow_behind)
    add_executable(test_follow_camera tests/test_follow_camera.c)
    target_include_directories(test_follow_camera PRIVATE game .)
    target_link_libraries(test_follow_camera PRIVATE tnfs_game)
    target_compile_options(test_follow_camera PRIVATE ${TNFS_WARNINGS})
    add_test(NAME follow_camera COMMAND test_follow_camera)
    add_executable(test_follow_yaw tests/test_follow_yaw.c)
    target_include_directories(test_follow_yaw PRIVATE game .)
    target_link_libraries(test_follow_yaw PRIVATE tnfs_game)
    target_compile_options(test_follow_yaw PRIVATE ${TNFS_WARNINGS})
    add_test(NAME follow_yaw COMMAND test_follow_yaw)
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
    add_executable(test_car_shadow tests/test_car_shadow.c game/viewer/follow_arm.c)
    target_include_directories(test_car_shadow PRIVATE game .)
    target_link_libraries(test_car_shadow PRIVATE tnfs_game)
    target_compile_options(test_car_shadow PRIVATE ${TNFS_WARNINGS})
    add_test(NAME car_shadow COMMAND test_car_shadow)
    add_executable(test_soft_shadow tests/test_soft_shadow.c game/viewer/soft_shadow.c)
    target_include_directories(test_soft_shadow PRIVATE game .)
    target_link_libraries(test_soft_shadow PRIVATE tnfs_game)
    target_compile_options(test_soft_shadow PRIVATE ${TNFS_WARNINGS})
    add_test(NAME soft_shadow COMMAND test_soft_shadow)
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
    # port: the primitives' recording and its replay by bands
    add_executable(test_raster_record tests/test_raster_record.c game/viewer/modern_road.c game/sys/bands.c game/sys/band_index.c
                   ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
    target_include_directories(test_raster_record PRIVATE game)
    target_link_libraries(test_raster_record PRIVATE tnfs_game SDL3::SDL3)
    target_compile_options(test_raster_record PRIVATE ${TNFS_WARNINGS})
    add_test(NAME raster_record COMMAND test_raster_record)
    add_executable(test_bands tests/test_bands.c game/sys/bands.c game/sys/band_index.c ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
    target_link_libraries(test_bands PRIVATE tnfs_game SDL3::SDL3)
    target_compile_options(test_bands PRIVATE ${TNFS_WARNINGS})
    add_test(NAME bands COMMAND test_bands)
    add_executable(test_tgv_wide tests/test_tgv_wide.c game/frontend/tgv_wide.c)
    target_include_directories(test_tgv_wide PRIVATE game)
    target_link_libraries(test_tgv_wide PRIVATE tnfs_game)
    target_compile_options(test_tgv_wide PRIVATE ${TNFS_WARNINGS})
    add_test(NAME tgv_wide COMMAND test_tgv_wide)
    add_executable(test_video_wide tests/test_video_wide.c game/sys/video.c ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
    target_link_libraries(test_video_wide PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_video_wide PRIVATE m)
    endif()
    target_compile_options(test_video_wide PRIVATE ${TNFS_WARNINGS})
    add_test(NAME video_wide COMMAND test_video_wide)
    add_executable(test_platform_mods tests/test_platform_mods.c ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
    target_link_libraries(test_platform_mods PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_platform_mods PRIVATE m)
    endif()
    target_compile_options(test_platform_mods PRIVATE ${TNFS_WARNINGS})
    add_test(NAME platform_mods COMMAND test_platform_mods)
    add_executable(test_platform_cfg_cache tests/test_platform_cfg_cache.c ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
    target_link_libraries(test_platform_cfg_cache PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_platform_cfg_cache PRIVATE m)
    endif()
    target_compile_options(test_platform_cfg_cache PRIVATE ${TNFS_WARNINGS})
    add_test(NAME platform_cfg_cache COMMAND test_platform_cfg_cache)
    add_executable(test_platform_keys tests/test_platform_keys.c ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
    target_link_libraries(test_platform_keys PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_platform_keys PRIVATE m)
    endif()
    target_compile_options(test_platform_keys PRIVATE ${TNFS_WARNINGS})
    add_test(NAME platform_keys COMMAND test_platform_keys)
    # port: the names TNFS_INPUT_SCRIPT's key table understands, so a page walk can be scripted by keyboard
    add_executable(test_script_keys tests/test_script_keys.c ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
    target_link_libraries(test_script_keys PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_script_keys PRIVATE m)
    endif()
    target_compile_options(test_script_keys PRIVATE ${TNFS_WARNINGS})
    add_test(NAME script_keys COMMAND test_script_keys)
    # port: the page keys on the keyboard path: a scripted PGDN is the PageDown (0x5100) input_get_key reports,
    # and only while the page keys are on. The input layer is in the executable, not in tnfs_game, so its source
    # is built into the test.
    add_executable(test_input_page_keys tests/test_input_page_keys.c game/sys/input.c ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
    target_link_libraries(test_input_page_keys PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_input_page_keys PRIVATE m)
    endif()
    target_compile_options(test_input_page_keys PRIVATE ${TNFS_WARNINGS})
    add_test(NAME input_page_keys COMMAND test_input_page_keys)
    add_executable(test_platform_text_input tests/test_platform_text_input.c ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
    target_link_libraries(test_platform_text_input PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_platform_text_input PRIVATE m)
    endif()
    target_compile_options(test_platform_text_input PRIVATE ${TNFS_WARNINGS})
    add_test(NAME platform_text_input COMMAND test_platform_text_input)
    add_executable(test_platform_cursor tests/test_platform_cursor.c ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
    target_link_libraries(test_platform_cursor PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_platform_cursor PRIVATE m)
    endif()
    target_compile_options(test_platform_cursor PRIVATE ${TNFS_WARNINGS})
    add_test(NAME platform_cursor COMMAND test_platform_cursor)
    add_executable(test_platform_display tests/test_platform_display.c ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
    target_link_libraries(test_platform_display PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_platform_display PRIVATE m)
    endif()
    target_compile_options(test_platform_display PRIVATE ${TNFS_WARNINGS})
    add_test(NAME platform_display COMMAND test_platform_display)
    # the test includes platform_sdl3.c and platform_present_sdl3.c (to reach the renderer and the pacing); the rest of the platform is linked
    add_executable(test_platform_present tests/test_platform_present.c game/platform/sdl3/platform_audio_sdl3.c game/platform/sdl3/platform_input_sdl3.c
                   game/platform/sdl3/platform_mods_sdl3.c game/platform/sdl3/platform_user_sdl3.c game/platform/sdl3/platform_pool_sdl3.c
                   game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
    target_link_libraries(test_platform_present PRIVATE tnfs_game SDL3::SDL3)
    if(UNIX AND NOT APPLE)
        target_link_libraries(test_platform_present PRIVATE m)
    endif()
    target_compile_options(test_platform_present PRIVATE ${TNFS_WARNINGS})
    add_test(NAME platform_present COMMAND test_platform_present)
    add_executable(test_reference_mod tests/test_reference_mod.c ${TNFS_SDL3_PLATFORM} game/platform/sdl3/pad_keys.c game/platform/sdl3/input_script.c)
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
        add_test(NAME tri_reverse_cd COMMAND test_tri_reverse ${TNFS_TRI_FILES})
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
        add_executable(test_lunar_jump tests/test_lunar_jump.c)
        target_link_libraries(test_lunar_jump PRIVATE tnfs_game)
        target_compile_options(test_lunar_jump PRIVATE ${TNFS_WARNINGS})
        add_test(NAME lunar_jump COMMAND test_lunar_jump "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_follow_behind_rows tests/test_follow_behind_rows.c game/viewer/follow_behind.c game/viewer/follow_arm.c)
        target_include_directories(test_follow_behind_rows PRIVATE game .)
        target_link_libraries(test_follow_behind_rows PRIVATE tnfs_game)
        target_compile_options(test_follow_behind_rows PRIVATE ${TNFS_WARNINGS})
        add_test(NAME follow_behind_rows COMMAND test_follow_behind_rows "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_race tests/test_race.c)
        target_link_libraries(test_race PRIVATE tnfs_game)
        target_compile_options(test_race PRIVATE ${TNFS_WARNINGS})
        add_test(NAME race COMMAND test_race "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_reverse_h2h_grid tests/test_reverse_h2h_grid.c)
        target_link_libraries(test_reverse_h2h_grid PRIVATE tnfs_game)
        target_compile_options(test_reverse_h2h_grid PRIVATE ${TNFS_WARNINGS})
        add_test(NAME reverse_h2h_grid COMMAND test_reverse_h2h_grid "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_replay_cheats_sim tests/test_replay_cheats_sim.c)
        target_link_libraries(test_replay_cheats_sim PRIVATE tnfs_game)
        target_compile_options(test_replay_cheats_sim PRIVATE ${TNFS_WARNINGS})
        add_test(NAME replay_cheats_sim COMMAND test_replay_cheats_sim "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_replay_nitro_sim tests/test_replay_nitro_sim.c)
        target_link_libraries(test_replay_nitro_sim PRIVATE tnfs_game)
        target_compile_options(test_replay_nitro_sim PRIVATE ${TNFS_WARNINGS})
        add_test(NAME replay_nitro_sim COMMAND test_replay_nitro_sim "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_race_sound_trace tests/test_race_sound_trace.c)
        target_link_libraries(test_race_sound_trace PRIVATE tnfs_game)
        target_compile_options(test_race_sound_trace PRIVATE ${TNFS_WARNINGS})
        add_test(NAME race_sound_trace COMMAND test_race_sound_trace "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_keyboard_race_sim tests/test_keyboard_race_sim.c)
        target_link_libraries(test_keyboard_race_sim PRIVATE tnfs_game)
        target_compile_options(test_keyboard_race_sim PRIVATE ${TNFS_WARNINGS})
        add_test(NAME keyboard_race_sim COMMAND test_keyboard_race_sim "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_chase_steady tests/test_chase_steady.c)
        target_link_libraries(test_chase_steady PRIVATE tnfs_game)
        target_compile_options(test_chase_steady PRIVATE ${TNFS_WARNINGS})
        add_test(NAME chase_steady COMMAND test_chase_steady "${CMAKE_SOURCE_DIR}/original/SIMDATA")
        add_executable(test_handbrake_lamps tests/test_handbrake_lamps.c)
        target_link_libraries(test_handbrake_lamps PRIVATE tnfs_game)
        target_compile_options(test_handbrake_lamps PRIVATE ${TNFS_WARNINGS})
        add_test(NAME handbrake_lamps COMMAND test_handbrake_lamps "${CMAKE_SOURCE_DIR}/original/SIMDATA")
    endif()

    find_program(TNFS_PYTHON3 python3)   # the PSD mask tool's lamp colours (skips itself when Pillow is missing)
    if(TNFS_PYTHON3)
        add_test(NAME car_mask_psd COMMAND ${TNFS_PYTHON3} "${CMAKE_SOURCE_DIR}/tests/test_car_mask_psd.py")
        add_test(NAME track_mask_psd COMMAND ${TNFS_PYTHON3} "${CMAKE_SOURCE_DIR}/tests/test_track_mask_psd.py")
        add_test(NAME dash_mask_psd COMMAND ${TNFS_PYTHON3} "${CMAKE_SOURCE_DIR}/tests/test_dash_mask_psd.py")
        add_test(NAME dash_psd_update COMMAND ${TNFS_PYTHON3} "${CMAKE_SOURCE_DIR}/tests/test_dash_psd_update.py")
        add_test(NAME spirited_logo COMMAND ${TNFS_PYTHON3} "${CMAKE_SOURCE_DIR}/tests/test_spirited_logo.py")
        add_test(NAME spirited_icon COMMAND ${TNFS_PYTHON3} "${CMAKE_SOURCE_DIR}/tests/test_spirited_icon.py")
        add_test(NAME profile_tables COMMAND ${TNFS_PYTHON3} -I "${CMAKE_SOURCE_DIR}/tests/profile_tables.py" "${CMAKE_SOURCE_DIR}")
        add_test(NAME profile_tables_sabotage COMMAND ${TNFS_PYTHON3} -I "${CMAKE_SOURCE_DIR}/tests/profile_tables.py" "${CMAKE_SOURCE_DIR}" --sabotage)
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



    add_executable(psx_sky_dump tools/psx_sky_dump.c)
    target_link_libraries(psx_sky_dump PRIVATE tnfs_game)
    target_compile_options(psx_sky_dump PRIVATE ${TNFS_WARNINGS})
    add_executable(psx_ratio_dump tools/psx_ratio_dump.c)
    target_link_libraries(psx_ratio_dump PRIVATE tnfs_game)
    target_compile_options(psx_ratio_dump PRIVATE ${TNFS_WARNINGS})
    add_executable(psx_light_mae tools/psx_light_mae.c)
    target_link_libraries(psx_light_mae PRIVATE tnfs_game)
    target_compile_options(psx_light_mae PRIVATE ${TNFS_WARNINGS})
    add_executable(psx_light_synth_lgt tools/psx_light_synth_lgt.c)
    target_link_libraries(psx_light_synth_lgt PRIVATE tnfs_game)
    target_compile_options(psx_light_synth_lgt PRIVATE ${TNFS_WARNINGS})
    add_executable(psx_light_fit tools/psx_light_fit.c)
    target_link_libraries(psx_light_fit PRIVATE tnfs_game)
    target_compile_options(psx_light_fit PRIVATE ${TNFS_WARNINGS})

    # the exe-comparison probes (probe_*.c, quad_draw, flat_draw, palette_match_run): each runs a ported function on the cases it is sent, to compare
    # with the exe's own; built only when asked for (-DTNFS_PROBES=ON), from the private workshop's tools/ (-DTNFS_WORKSHOP=DIR or the TNFS_WORKSHOP variable)
    if(TNFS_PROBES)
        set(TNFS_WORKSHOP "" CACHE PATH "The private workshop checkout the probes' sources are in (its tools/); empty: the TNFS_WORKSHOP environment variable")
        set(TNFS_PROBE_WORKSHOP "${TNFS_WORKSHOP}")
        if(TNFS_PROBE_WORKSHOP STREQUAL "")
            set(TNFS_PROBE_WORKSHOP "$ENV{TNFS_WORKSHOP}")
        endif()
        set(TNFS_PROBE_TOOLS "${TNFS_PROBE_WORKSHOP}/tools")
        if(TNFS_PROBE_WORKSHOP STREQUAL "" OR NOT EXISTS "${TNFS_PROBE_TOOLS}/probe_frontend.c")
            message(FATAL_ERROR "TNFS_PROBES=ON needs the workshop checkout with the probes' sources: pass -DTNFS_WORKSHOP=DIR "
                                "or set the TNFS_WORKSHOP environment variable to a directory whose tools/ holds probe_frontend.c "
                                "(now: '${TNFS_PROBE_WORKSHOP}')")
        endif()
        add_executable(probe_3d ${TNFS_PROBE_TOOLS}/probe_3d.c)
        target_link_libraries(probe_3d PRIVATE tnfs_game)
        target_compile_options(probe_3d PRIVATE ${TNFS_WARNINGS})
        add_executable(probe_object3d ${TNFS_PROBE_TOOLS}/probe_object3d.c)
        target_link_libraries(probe_object3d PRIVATE tnfs_game)
        target_compile_options(probe_object3d PRIVATE ${TNFS_WARNINGS})
        add_executable(probe_math ${TNFS_PROBE_TOOLS}/probe_math.c)
        target_link_libraries(probe_math PRIVATE tnfs_game)
        add_executable(probe_frontend ${TNFS_PROBE_TOOLS}/probe_frontend.c)
        target_link_libraries(probe_frontend PRIVATE tnfs_game)
        target_compile_options(probe_frontend PRIVATE ${TNFS_WARNINGS})
        add_executable(probe_input ${TNFS_PROBE_TOOLS}/probe_input.c ${TNFS_PROBE_TOOLS}/probe_race_menu.c ${TNFS_PROBE_TOOLS}/probe_race_net.c ${TNFS_PROBE_TOOLS}/probe_race_session.c
            ${TNFS_PROBE_TOOLS}/probe_transport.c ${TNFS_PROBE_TOOLS}/probe_packet_open.c ${TNFS_PROBE_TOOLS}/probe_net_request.c
            ${TNFS_PROBE_TOOLS}/probe_net_service.c ${TNFS_PROBE_TOOLS}/probe_net_serve.c ${TNFS_PROBE_TOOLS}/probe_net_link.c
            ${TNFS_PROBE_TOOLS}/probe_net_pktdata.c)
        target_link_libraries(probe_input PRIVATE tnfs_game)
        target_compile_options(probe_input PRIVATE ${TNFS_WARNINGS})
        add_executable(probe_needle ${TNFS_PROBE_TOOLS}/probe_needle.c)
        target_link_libraries(probe_needle PRIVATE tnfs_game)
        target_compile_options(probe_needle PRIVATE ${TNFS_WARNINGS})
        add_executable(probe_movie ${TNFS_PROBE_TOOLS}/probe_movie.c)
        target_link_libraries(probe_movie PRIVATE tnfs_game)
        target_compile_options(probe_movie PRIVATE ${TNFS_WARNINGS})
        add_executable(probe_palette ${TNFS_PROBE_TOOLS}/probe_palette.c)
        add_executable(probe_config ${TNFS_PROBE_TOOLS}/probe_config.c)
        target_link_libraries(probe_config PRIVATE tnfs_game)
        target_compile_options(probe_config PRIVATE ${TNFS_WARNINGS})
        target_link_libraries(probe_palette PRIVATE tnfs_game)
        target_compile_options(probe_palette PRIVATE ${TNFS_WARNINGS})
        add_executable(probe_dash ${TNFS_PROBE_TOOLS}/probe_dash.c)
        target_link_libraries(probe_dash PRIVATE tnfs_game)
        target_compile_options(probe_dash PRIVATE ${TNFS_WARNINGS})
        add_executable(probe_car ${TNFS_PROBE_TOOLS}/probe_car.c)
        target_link_libraries(probe_car PRIVATE tnfs_game)
        target_compile_options(probe_car PRIVATE ${TNFS_WARNINGS})
        add_executable(quad_draw ${TNFS_PROBE_TOOLS}/quad_draw.c)   # the port's side of the rasteriser's and the palette's comparisons
        target_link_libraries(quad_draw PRIVATE tnfs_game)
        target_compile_options(quad_draw PRIVATE ${TNFS_WARNINGS})
        add_executable(flat_draw ${TNFS_PROBE_TOOLS}/flat_draw.c)
        target_link_libraries(flat_draw PRIVATE tnfs_game)
        target_compile_options(flat_draw PRIVATE ${TNFS_WARNINGS})
        add_executable(palette_match_run ${TNFS_PROBE_TOOLS}/palette_match_run.c)
        target_link_libraries(palette_match_run PRIVATE tnfs_game)
        target_compile_options(palette_match_run PRIVATE ${TNFS_WARNINGS})
    endif()

    add_executable(qfs_unpack tools/qfs_unpack.c)
    target_link_libraries(qfs_unpack PRIVATE tnfs_core)
    target_compile_options(qfs_unpack PRIVATE ${TNFS_WARNINGS})
endif()

# port: no test writes to the player's own folder (the log, the settings, a replay): each one gets a scratch pref dir (platform_user_dir: TNFS_PREF_DIR),
# set for every test at the end of this file (a test registered after this point was left out of the loop that used to be here)
if(NOT EMSCRIPTEN)
    # --user keeps every user file in its folder and touches nothing in the pref dir, the log included, even a run that stops for want of game data; --mods holds none of them (the old <mods>/user/ moves once)
    add_test(NAME pref_isolation COMMAND ${CMAKE_COMMAND} -DTNFS=$<TARGET_FILE:tnfs> -DWORK=${CMAKE_BINARY_DIR}/pref_isolation -P ${CMAKE_SOURCE_DIR}/tests/pref_isolation.cmake)
    set_tests_properties(pref_isolation PROPERTIES TIMEOUT 60)
    # a development build run from the repository with no --data takes ./original (tools/golden_frames.sh runs so)
    if(NOT TNFS_RELEASE AND EXISTS "${CMAKE_SOURCE_DIR}/original/NFS_WIN.EXE")
        add_test(NAME dev_data_default COMMAND ${CMAKE_COMMAND} -DTNFS=$<TARGET_FILE:tnfs> -DREPO=${CMAKE_SOURCE_DIR} -DWORK=${CMAKE_BINARY_DIR}/dev_data_default -P ${CMAKE_SOURCE_DIR}/tests/dev_data_default.cmake)
        set_tests_properties(dev_data_default PROPERTIES TIMEOUT 120)
        # the anonymous counters of a "release" version run with --mods: the flag in the --user folder, nothing in the pref dir, nothing when Off
        add_test(NAME stats_mods_isolation COMMAND ${CMAKE_COMMAND} -DTNFS=$<TARGET_FILE:tnfs> -DREPO=${CMAKE_SOURCE_DIR} -DWORK=${CMAKE_BINARY_DIR}/stats_mods_isolation -P ${CMAKE_SOURCE_DIR}/tests/stats_mods_isolation.cmake)
        set_tests_properties(stats_mods_isolation PROPERTIES TIMEOUT 120)
        # the bumper view at night in rain with the headlights: the same frame each run, no sanitizer error (the tunnel flags read after being freed)
        add_test(NAME rain_night_headlights COMMAND ${CMAKE_COMMAND} -DTNFS=$<TARGET_FILE:tnfs> -DREPO=${CMAKE_SOURCE_DIR} -DWORK=${CMAKE_BINARY_DIR}/rain_night_headlights -P ${CMAKE_SOURCE_DIR}/tests/rain_night_headlights.cmake)
        set_tests_properties(rain_night_headlights PROPERTIES TIMEOUT 400)
        # the top row after a change of view as the start lights go out: the redraw's labels reach the strip (no green left under the numbers)
        add_test(NAME hud_lights_view_change COMMAND ${CMAKE_COMMAND} -DTNFS=$<TARGET_FILE:tnfs> -DREPO=${CMAKE_SOURCE_DIR} -DWORK=${CMAKE_BINARY_DIR}/hud_lights_view_change -P ${CMAKE_SOURCE_DIR}/tests/hud_lights_view_change.cmake)
        set_tests_properties(hud_lights_view_change PROPERTIES TIMEOUT 400)
        # the soft shadow with the light off: a car row drawn with no light loaded (car_soft_shadow is its own switch)
        add_test(NAME shadow_without_light COMMAND ${CMAKE_COMMAND} -DTNFS=$<TARGET_FILE:tnfs> -DREPO=${CMAKE_SOURCE_DIR} -DWORK=${CMAKE_BINARY_DIR}/shadow_without_light -P ${CMAKE_SOURCE_DIR}/tests/shadow_without_light.cmake)
        set_tests_properties(shadow_without_light PROPERTIES TIMEOUT 120)
        # the day's frame through the plain present (drive_light_needs_split) is the one the split present makes, in a day tunnel too
        add_test(NAME day_plain_present COMMAND ${CMAKE_COMMAND} -DTNFS=$<TARGET_FILE:tnfs> -DREPO=${CMAKE_SOURCE_DIR} -DWORK=${CMAKE_BINARY_DIR}/day_plain_present -P ${CMAKE_SOURCE_DIR}/tests/day_plain_present.cmake)
        set_tests_properties(day_plain_present PROPERTIES TIMEOUT 600)
        # the light editor: saving the light file mid-race must not overrun the lines it draws
        add_test(NAME light_edit_reload COMMAND ${CMAKE_SOURCE_DIR}/tests/light_edit_reload.sh $<TARGET_FILE:tnfs> ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/light_edit_reload)
        set_tests_properties(light_edit_reload PROPERTIES TIMEOUT 120)
        add_test(NAME light_edit_places COMMAND ${CMAKE_SOURCE_DIR}/tests/light_edit_places.sh $<TARGET_FILE:tnfs> ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/light_edit_places)
        set_tests_properties(light_edit_places PROPERTIES TIMEOUT 240)
        # a replay plays with the conditions it was recorded with (a CD's demo: day and clear), not the settings'
        add_test(NAME replay_conditions COMMAND ${CMAKE_SOURCE_DIR}/tests/replay_conditions.sh $<TARGET_FILE:tnfs> ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/replay_conditions)
        set_tests_properties(replay_conditions PROPERTIES TIMEOUT 300)
        # two races in one process (TNFS_DRIVE_THEN): the second track registers its own reflector masks, not the first's
        add_test(NAME two_tracks_reflectors COMMAND ${CMAKE_SOURCE_DIR}/tests/two_tracks_reflectors.sh $<TARGET_FILE:tnfs> ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/two_tracks_reflectors)
        set_tests_properties(two_tracks_reflectors PROPERTIES TIMEOUT 300)
        add_test(NAME trail_frame_skip COMMAND ${CMAKE_SOURCE_DIR}/tests/trail_frame_skip.sh $<TARGET_FILE:tnfs> ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/trail_frame_skip)
        set_tests_properties(trail_frame_skip PROPERTIES TIMEOUT 600)
        add_test(NAME wipers COMMAND ${CMAKE_SOURCE_DIR}/tests/wipers.sh $<TARGET_FILE:tnfs> ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/wipers)
        set_tests_properties(wipers PROPERTIES TIMEOUT 400)
        # rain_lamps must change the frame of a masked car in rain and storm, and not in clear; an old port.cfg moves at the real startup
        add_test(NAME rain_lamps_masked COMMAND ${CMAKE_COMMAND} -DTNFS=$<TARGET_FILE:tnfs> -DREPO=${CMAKE_SOURCE_DIR} -DWORK=${CMAKE_BINARY_DIR}/rain_lamps_masked -P ${CMAKE_SOURCE_DIR}/tests/rain_lamps_masked.cmake)
        set_tests_properties(rain_lamps_masked PROPERTIES TIMEOUT 400)
        add_test(NAME old_files_startup COMMAND ${CMAKE_COMMAND} -DTNFS=$<TARGET_FILE:tnfs> -DREPO=${CMAKE_SOURCE_DIR} -DWORK=${CMAKE_BINARY_DIR}/old_files_startup -P ${CMAKE_SOURCE_DIR}/tests/old_files_startup.cmake)
        set_tests_properties(old_files_startup PROPERTIES TIMEOUT 200)
        add_test(NAME input_script_wait COMMAND ${CMAKE_COMMAND} -DTNFS=$<TARGET_FILE:tnfs> -DREPO=${CMAKE_SOURCE_DIR} -DWORK=${CMAKE_BINARY_DIR}/input_script_wait -P ${CMAKE_SOURCE_DIR}/tests/input_script_wait.cmake)
        set_tests_properties(input_script_wait PROPERTIES TIMEOUT 150)
        add_test(NAME live_mode_switch_race COMMAND ${CMAKE_COMMAND} -DTNFS=$<TARGET_FILE:tnfs> -DREPO=${CMAKE_SOURCE_DIR} -DWORK=${CMAKE_BINARY_DIR}/live_mode_switch_race -P ${CMAKE_SOURCE_DIR}/tests/live_mode_switch_race.cmake)
        set_tests_properties(live_mode_switch_race PROPERTIES TIMEOUT 450)
        add_test(NAME boot_skip_chain COMMAND ${CMAKE_COMMAND} -DTNFS=$<TARGET_FILE:tnfs> -DREPO=${CMAKE_SOURCE_DIR} -DWORK=${CMAKE_BINARY_DIR}/boot_skip_chain -P ${CMAKE_SOURCE_DIR}/tests/boot_skip_chain.cmake)
        set_tests_properties(boot_skip_chain PROPERTIES TIMEOUT 360)
    endif()
endif()

# port: every test, whenever it was registered above, gets a scratch pref dir, so a test that starts tnfs with no --user (or forgets one) writes in the build, never in the player's own
# folder; pref_guard fails when a test has none (tests/pref_guard.cmake)
if(NOT EMSCRIPTEN)
    add_test(NAME pref_guard COMMAND ${CMAKE_COMMAND} -DCTEST=${CMAKE_CTEST_COMMAND} -DBUILD=${CMAKE_BINARY_DIR} -P ${CMAKE_SOURCE_DIR}/tests/pref_guard.cmake)
    set_tests_properties(pref_guard PROPERTIES TIMEOUT 180)
    get_property(tnfs_all_tests DIRECTORY PROPERTY TESTS)
    foreach(t IN LISTS tnfs_all_tests)
        set_property(TEST ${t} APPEND PROPERTY ENVIRONMENT "TNFS_PREF_DIR=${CMAKE_BINARY_DIR}/test_pref")
    endforeach()
endif()

# the tests that read the exe's tables install them from original/NFS_WIN.EXE first (tests/exe_tables_install.h; a no-op when the tables are in the source)
foreach(name config_dat screen_layout records_route records_import config_name mirror_on_dash modern_road follow_behind_rows race_sound_trace)
    if(TARGET test_${name})
        target_compile_definitions(test_${name} PRIVATE TNFS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    endif()
endforeach()
# the tests that run the real game take the tables from the exe at its start-up: a hang (a poisoned or missing table) ends them in two minutes
# (five in a sanitizer build, where a loaded machine runs them at half the speed: day_plain_present took 111 s of 120 under ASan)
set(TNFS_GAME_TEST_TIMEOUT 120)
if(CMAKE_C_FLAGS MATCHES "-fsanitize")
    set(TNFS_GAME_TEST_TIMEOUT 300)
endif()
foreach(name mods_overlay track_new reference_mod two_tracks_reflectors trail_frame_skip rain_lamps_masked old_files_startup dev_data_default
             rain_night_headlights hud_lights_view_change shadow_without_light day_plain_present light_edit_reload light_edit_places wipers)
    if(TEST ${name})
        set_tests_properties(${name} PROPERTIES TIMEOUT ${TNFS_GAME_TEST_TIMEOUT})
    endif()
endforeach()
