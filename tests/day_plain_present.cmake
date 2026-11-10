# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P day_plain_present.cmake
# By day, with the tint neutral and the dash's ratio neutral, the present skips the PS1 light's split (drive_light_needs_split): the world, half and plain
# palettes are the same. The frame must be the one the split makes (TNFS_FORCE_SPLIT=1), in the day, in the rain, at dusk and at night, and in a day
# tunnel (TR6, where the car's light ratio is not 1 and the split must still run: its dash art is the half palette).
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref")
file(WRITE "${WORK}/user/port.cfg" "profile=restomod\npsx_dir=\nrender_scale=2\nformat=16:9\ngauge_light=3\n")

# frame(<name> <track> <time> <weather> <start node> <forced 0|1>) -> md5 in ${name}_md5
function(frame name track time weather node forced)
    set(split "")
    if(forced)
        set(split TNFS_FORCE_SPLIT=1)
    endif()
    file(REMOVE "${WORK}/${name}.ppm")
    execute_process(COMMAND ${CMAKE_COMMAND} -E env ${split} TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ASAN_OPTIONS=detect_leaks=0
                            TNFS_VIEW=0 TNFS_FRAMES=1 TNFS_PSX_TIME=${time} TNFS_WEATHER=${weather} TNFS_START_NODE=${node}
                            ${TNFS} --data ${REPO}/original --user ${WORK}/user --drive ${track} --node 70 --screenshot ${WORK}/${name}.ppm
                    WORKING_DIRECTORY "${REPO}" RESULT_VARIABLE rc ERROR_VARIABLE err OUTPUT_QUIET TIMEOUT 100)
    if(NOT EXISTS "${WORK}/${name}.ppm")
        message(FATAL_ERROR "${name}: no frame (rc=${rc}): ${err}")
    endif()
    if(err MATCHES "AddressSanitizer|runtime error")
        message(FATAL_ERROR "${name}: sanitizer error: ${err}")
    endif()
    file(MD5 "${WORK}/${name}.ppm" md5)
    set(${name}_md5 "${md5}" PARENT_SCOPE)
endfunction()

# name track time weather node
set(scenes
    "cy1_day CY1 day clear 70"
    "tr1_rain TR1 day rain 70"
    "tr6_tunnel_a TR6 day clear 220"
    "tr6_tunnel_b TR6 day clear 260"
    "tr6_tunnel_c TR6 day clear 300"
    "cy1_dusk CY1 dusk clear 70"
    "cy1_night CY1 night clear 70")
foreach(scene IN LISTS scenes)
    separate_arguments(f UNIX_COMMAND "${scene}")
    list(GET f 0 name)
    list(GET f 1 track)
    list(GET f 2 time)
    list(GET f 3 weather)
    list(GET f 4 node)
    frame(${name}_plain ${track} ${time} ${weather} ${node} 0)
    frame(${name}_split ${track} ${time} ${weather} ${node} 1)
    if(NOT "${${name}_plain_md5}" STREQUAL "${${name}_split_md5}")
        message(FATAL_ERROR "${name}: the frame differs from the split present's (${${name}_plain_md5} vs ${${name}_split_md5})")
    endif()
endforeach()
