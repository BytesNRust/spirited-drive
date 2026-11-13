# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P boot_skip_chain.cmake
# One press per screen (the owner's report, 2026-11-13): a key or a click that stops EA's movie must not also close the title that follows it
# (title.c's fresh press). Each run presses once during the movie, then waits; the title must stay most of its own 500 ticks (before: about 60).
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
foreach(press "ENTER" "CLICK:320:240")
    file(REMOVE_RECURSE "${WORK}")
    file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref")
    set(script "1000:MOUSE:5:5,0:WAIT:mark:movie=:60,300:${press},0:WAIT:mark:screen=game_mode:40,0:WAIT:mark:never:1")
    execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ASAN_OPTIONS=detect_leaks=0
                            TNFS_INPUT_SETTLE=300 "TNFS_INPUT_SCRIPT=${script}"
                            ${TNFS} --data ${REPO}/original --mods ${WORK} --user ${WORK}/user
                    RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err TIMEOUT 150)
    if(NOT rc EQUAL 3)
        message(FATAL_ERROR "${press}: expected status 3 (the script's end), got ${rc}: ${err}")
    endif()
    file(READ "${WORK}/user/spirited-drive.log" log)
    if(NOT log MATCHES "mark:movie_end skipped=1")
        message(FATAL_ERROR "${press}: the press did not stop EA's movie")
    endif()
    if(NOT log MATCHES "mark:hold screen=title ticks=([0-9]+)")
        message(FATAL_ERROR "${press}: no title screen")
    endif()
    if(CMAKE_MATCH_1 LESS 400)
        message(FATAL_ERROR "${press}: the press that stopped the movie also closed the title (${CMAKE_MATCH_1} ticks)")
    endif()
endforeach()
file(REMOVE_RECURSE "${WORK}")
