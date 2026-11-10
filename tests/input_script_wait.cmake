# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P input_script_wait.cmake
# The scripted input's WAIT (platform_sdl3.c): a line the game never logs stops the run at that step with status 3 and a message that names the step, in about the seconds given.
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref")
execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ASAN_OPTIONS=detect_leaks=0 TNFS_BANDS=0
                        TNFS_INPUT_SETTLE=300 "TNFS_INPUT_SCRIPT=300:ESC,0:WAIT:mark:never:3,300:ENTER"
                        ${TNFS} --data ${REPO}/original --user ${WORK}/user
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err TIMEOUT 120)
if(NOT rc EQUAL 3)
    message(FATAL_ERROR "expected status 3, got ${rc}: ${err}")
endif()
string(FIND "${err}" "input script: step 2: WAIT mark:never timed out after 3 s" at)
if(at EQUAL -1)
    message(FATAL_ERROR "the message does not name the step: ${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
