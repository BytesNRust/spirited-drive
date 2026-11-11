# usage: cmake -DTNFS=<exe> -DWORK=<scratch dir> -P pref_isolation.cmake
# The player's folder and where it is. Each run has no game data (it stops at once, before platform_init) and TNFS_PREF_DIR set to a scratch folder:
#  1. --user DIR: the log is DIR/spirited-drive.log and the pref folder stays empty.
#  2. only --mods DIR: the player's folder is the pref folder (the log is there), the mods directory gets nothing.
#  3. a --mods DIR that still holds user/ files (an older install): they move to the pref folder once, said in the log, and the old copy of a file the pref folder has is left alone.
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
function(run)
    execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ${TNFS} ${ARGN}
                    WORKING_DIRECTORY "${WORK}/cwd" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 30)
endfunction()
function(log_matches file text)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR "no ${file} (stderr: ${err})")
    endif()
    file(READ "${file}" log)
    if(NOT log MATCHES "${text}")
        message(FATAL_ERROR "${file} lacks '${text}': ${log}")
    endif()
endfunction()

# 1
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/mods" "${WORK}/pref" "${WORK}/cwd")
run(--user ${WORK}/portable --mods ${WORK}/mods)
file(GLOB_RECURSE leaked "${WORK}/pref/*")
if(leaked)
    message(FATAL_ERROR "with --user the game wrote into the pref dir: ${leaked}")
endif()
log_matches("${WORK}/portable/spirited-drive.log" "no game data")

# 2
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/mods" "${WORK}/pref" "${WORK}/cwd")
run(--mods ${WORK}/mods)
file(GLOB_RECURSE inmods "${WORK}/mods/*")
if(inmods)
    message(FATAL_ERROR "--mods got files of the player's: ${inmods}")
endif()
log_matches("${WORK}/pref/spirited-drive.log" "no game data")

# 3
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/mods/user/sub" "${WORK}/pref" "${WORK}/cwd")
file(WRITE "${WORK}/mods/user/controls.cfg" "old controls\n")
file(WRITE "${WORK}/mods/user/port.cfg" "old port\n")
file(WRITE "${WORK}/mods/user/sub/x.dat" "x")
file(WRITE "${WORK}/pref/port.cfg" "new port\n")
run(--mods ${WORK}/mods)
file(READ "${WORK}/pref/controls.cfg" moved)
if(NOT moved STREQUAL "old controls\n" OR NOT EXISTS "${WORK}/pref/sub/x.dat" OR EXISTS "${WORK}/mods/user/controls.cfg")
    message(FATAL_ERROR "the old user/ files did not move")
endif()
file(READ "${WORK}/pref/port.cfg" kept)
file(READ "${WORK}/mods/user/port.cfg" old)
if(NOT kept STREQUAL "new port\n" OR NOT old STREQUAL "old port\n")
    message(FATAL_ERROR "a file in both places: the pref one must win and the old one stay (${kept} / ${old})")
endif()
log_matches("${WORK}/pref/spirited-drive.log" "item\\(s\\) moved from")
file(REMOVE_RECURSE "${WORK}")
