# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P rain_night_headlights.cmake
# The bumper view at night in rain with the headlights on, headless, four times: the frame must be the same each time and the run must report no
# sanitizer error. The road's tunnel flags (road_tunnels) were read after they were freed in this setup (the rain's lookup freed the headlight pool's array):
# an ASan build stops on it, any other build showed a different mirror from run to run.
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref")
# the night rain setup of an owner's settings, in Restomod's file (the lights, the rain, the street lamps), 21:9 at render scale 2: fewer keys did not show it
set(keys "format=21:9" "render_scale=2" "light_time=night" "weather=clear"
    "scale=fill"
    "fov_adjust=10"
    "view_ultra=1"
    "perspective=1"
    "texture_filter=1"
    "fog=1"
    "f3_ultra=1"
    "laps=0"
    "light_effects=1"
    "car_soft_shadow=1"
    "body_shine=4"
    "records_unified=1"
    "car_colours=1"
    "fog_sample=1"
    "sky_fill=1"
    "sky_fill_below=1"
    "radar_all_views=1"
    "tach_dim=1"
    "gauge_wheel=1"
    "police_glow=1"
    "replay_no_ffb=1"
    "last_camera=1"
    "camera_kind=1"
    "lamp_flares=1"
    "rally_gravel_shoulder=1"
    "close_houses=1"
    "brake_over_gas=1"
    "rain_fx=1"
    "rain_grip=1"
    "gauge_light=3"
    "finish_gate_light=1"
    "reverse_rows=1"
    "city_lift=3"
    "vegas_lift=2"
)
string(REPLACE ";" "\n" cfg "${keys}")
file(WRITE "${WORK}/user/port.cfg" "profile=restomod\npsx_dir=\n")
# and its extras: the bumper view with its mirror (bumper_mirror) is what draws the road behind
set(extras
    "start=central"
    "units=kmh"
    "dash_box=lever"
    "mirror=0"
    "rally=0"
    "warrior=1"
    "vegas=1"
    "explode=0"
    "nitro=0"
    "turbo=0"
    "replay_cameras=1"
    "warrior_rival=1"
    "no_movies=1"
    "no_assists=1"
    "texture_filter=1"
    "fog=1"
    "bumper_mirror=1"
    "bumper_radar=1"
    "variant_time=1"
    "stats_overlay=0"
    "car_markers=0"
    "lunar_springs=1"
    "movies_wide=1"
    "follow_camera=1"
    "extra_liveries=1"
    "reverse=1"
    "lunar_gravity=1"
    "rally_gravel=1"
    "update_check=1"
    "car_paint=004405000000"
    "difficulty=1"
)
string(REPLACE ";" "\n" extras_text "${extras}")
file(WRITE "${WORK}/user/restomod.cfg" "${cfg}\n${extras_text}\n")
set(first "")
foreach(run 1 2 3 4)
    execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ASAN_OPTIONS=detect_leaks=0
                            TNFS_LIGHT_TIME=night TNFS_WEATHER=rain TNFS_VIEW=1 TNFS_AUTOPILOT=1 TNFS_FRAMES=1 TNFS_BANDS=0
                            ${TNFS} --data ${REPO}/original --user ${WORK}/user --drive CY2 --node 240 --screenshot ${WORK}/end${run}.ppm
                    WORKING_DIRECTORY "${REPO}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 100)
    if(err MATCHES "AddressSanitizer|runtime error")
        message(FATAL_ERROR "run ${run}: sanitizer error: ${err}")
    endif()
    if(NOT EXISTS "${WORK}/end${run}.ppm")
        message(FATAL_ERROR "run ${run} (rc=${rc}) wrote no frame: ${err}")
    endif()
    file(MD5 "${WORK}/end${run}.ppm" sum)
    if(first STREQUAL "")
        set(first "${sum}")
    elseif(NOT sum STREQUAL first)
        message(FATAL_ERROR "run ${run}: the frame differs from run 1 (${sum} against ${first}): a state that changes from run to run")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK}")
