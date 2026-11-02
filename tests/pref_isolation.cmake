# usage: cmake -DTNFS=<exe> -DWORK=<scratch dir> -P pref_isolation.cmake
# Runs the game with --mods and no game data (it stops at once, before platform_init) with TNFS_PREF_DIR set to an empty folder: that folder must stay empty,
# and the log must be <mods>/user/tnfs.log.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/mods" "${WORK}/pref" "${WORK}/cwd")
execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
                        ${TNFS} --mods ${WORK}/mods
                WORKING_DIRECTORY "${WORK}/cwd" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 30)
file(GLOB_RECURSE leaked "${WORK}/pref/*")
if(leaked)
    message(FATAL_ERROR "with --mods the game wrote into the pref dir: ${leaked}")
endif()
if(NOT EXISTS "${WORK}/mods/user/tnfs.log")
    message(FATAL_ERROR "no ${WORK}/mods/user/tnfs.log (rc=${rc}, stderr: ${err})")
endif()
file(READ "${WORK}/mods/user/tnfs.log" log)
if(NOT log MATCHES "no game data")
    message(FATAL_ERROR "the log lacks the no-game-data line: ${log}")
endif()
