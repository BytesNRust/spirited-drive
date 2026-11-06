# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P stats_mods_isolation.cmake
# A "release" version (TNFS_STATS_DRYRUN=0.9.0: the dev build counts as 0.9.0 and only logs what it would send) run with --user: its anonymous first_start flag
# goes to <user>/stats.cfg, the pref dir stays empty, and the log says it would send once. A run from a "dev" version (no DRYRUN) writes no stats.cfg.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref")
execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref TNFS_STATS_DRYRUN=0.9.0 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
                        ${TNFS} --user ${WORK}/user
                WORKING_DIRECTORY "${REPO}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 12)
file(GLOB_RECURSE leaked "${WORK}/pref/*")
if(leaked)
    message(FATAL_ERROR "with --user the stats wrote into the pref dir: ${leaked}")
endif()
if(NOT EXISTS "${WORK}/user/stats.cfg")
    message(FATAL_ERROR "no ${WORK}/user/stats.cfg (rc=${rc}): ${err}")
endif()
file(READ "${WORK}/user/stats.cfg" flags)
if(NOT flags MATCHES "first_start=1")
    message(FATAL_ERROR "stats.cfg lacks first_start=1: ${flags}")
endif()
file(READ "${WORK}/user/tnfs.log" log)
if(NOT log MATCHES "dry run, would send first_start v=0.9.0")
    message(FATAL_ERROR "the log lacks the would-send line: ${log}")
endif()
# the Off switch: nothing, not even the flag file
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref")
file(WRITE "${WORK}/user/stock.cfg" "update_check=0\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref TNFS_STATS_DRYRUN=0.9.0 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
                        ${TNFS} --user ${WORK}/user
                WORKING_DIRECTORY "${REPO}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 12)
if(EXISTS "${WORK}/user/stats.cfg")
    message(FATAL_ERROR "the switch was Off but stats.cfg was written")
endif()
file(GLOB_RECURSE leaked "${WORK}/pref/*")
if(leaked)
    message(FATAL_ERROR "with --user the Off run wrote into the pref dir: ${leaked}")
endif()
# a mod is never written: with --user and --mods apart, the flag goes to the player's folder and the mods folder stays empty
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref" "${WORK}/mod")
execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref TNFS_STATS_DRYRUN=0.9.0 SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
                        ${TNFS} --user ${WORK}/user --mods ${WORK}/mod
                WORKING_DIRECTORY "${REPO}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 12)
file(GLOB_RECURSE in_mod "${WORK}/mod/*")
if(in_mod)
    message(FATAL_ERROR "the run wrote into the mods folder: ${in_mod}")
endif()
file(GLOB_RECURSE leaked "${WORK}/pref/*")
if(leaked)
    message(FATAL_ERROR "with --user and --mods the stats wrote into the pref dir: ${leaked}")
endif()
if(NOT EXISTS "${WORK}/user/stats.cfg")
    message(FATAL_ERROR "with --mods the flag is not in the player's folder (rc=${rc}): ${err}")
endif()
