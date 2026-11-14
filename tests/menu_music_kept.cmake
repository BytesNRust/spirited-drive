# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P menu_music_kept.cmake
# The menus' music from Game Mode through the name box to Control Central. The front end's sound bank is freed on the way, and the voices stopped
# with it must be its own: the remembered effect handles are 0 when no effect played, and handle 0 can be the music's stream (the first handle the
# driver hands out when the intro movies are off and nothing sounded before the music), so stopping them by handle cut the music (the owner's
# "mark:music lost" as Central opened). The music must still be playing a few seconds into Central.
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
# the title, Game Mode's Single, the name box left
set(step 60)
set(script "0:WAIT:mark:screen=title:${step},4000:ESC,0:WAIT:mark:screen=game_mode:${step},300:CLICK:310:217,0:WAIT:mark:screen=name_box:${step}")
set(script "${script},500:TEXT:R,500:ENTER,0:WAIT:mark:screen=central:${step},0:WAIT:mark:never:4")
# the title's song is picked by the clock, and one of its 12 picks plays nothing (0045638c's skipped track): a walk with no song is walked again
foreach(attempt 1 2 3)
    file(REMOVE_RECURSE "${WORK}")
    file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref" "${WORK}/mods")
    # the intro movies off (Restomod's file), so the music is the first sound and takes handle 0
    file(WRITE "${WORK}/user/port.cfg" "profile=restomod\n")
    file(WRITE "${WORK}/user/restomod.cfg" "no_movies=1\n")
    execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ASAN_OPTIONS=detect_leaks=0
                            TNFS_INPUT_SETTLE=1500 "TNFS_INPUT_SCRIPT=${script}"
                            ${TNFS} --data ${REPO}/original --mods ${WORK}/mods --user ${WORK}/user
                    WORKING_DIRECTORY "${REPO}" RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err TIMEOUT 280)
    if(err MATCHES "AddressSanitizer|runtime error")
        message(FATAL_ERROR "sanitizer error: ${err}")
    endif()
    string(FIND "${err}" "WAIT mark:never timed out" reached)
    if(NOT rc EQUAL 3 OR reached EQUAL -1)
        message(FATAL_ERROR "the walk did not reach Central and wait there (status ${rc}): ${err}")
    endif()
    file(READ "${WORK}/user/spirited-drive.log" log)
    string(FIND "${log}" "mark:music play=" played)
    if(NOT played EQUAL -1)
        break()
    endif()
    message(STATUS "walk ${attempt}: the title picked no song")
endforeach()
if(played EQUAL -1)
    message(FATAL_ERROR "no music played on the way to Central in three walks")
endif()
string(REGEX MATCH "mark:music lost[^\n]*" lost "${log}")
if(NOT lost STREQUAL "")
    message(FATAL_ERROR "the music stopped with no call that stops it on the way to Central: ${lost}")
endif()
file(REMOVE_RECURSE "${WORK}")
