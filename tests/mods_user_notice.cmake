# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P mods_user_notice.cmake
# --mods without --user runs in the player's own folder: stderr names it and says --user changes it. With --user the line is not there.
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref" "${WORK}/piece")
file(WRITE "${WORK}/user/port.cfg" "profile=stock\n")
set(common ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ASAN_OPTIONS=detect_leaks=0 TNFS_FRAMES=1 TNFS_BANDS=0
          ${TNFS} --data ${REPO}/original --mods ${WORK}/piece --track NOSUCHPC --screenshot ${WORK}/shot.ppm)
execute_process(COMMAND ${common} RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err TIMEOUT 120)
string(FIND "${err}" "user folder: ${WORK}/pref/ (--user DIR uses another)" at)
if(at EQUAL -1)
    message(FATAL_ERROR "no --user: stderr does not name the user folder: ${err}")
endif()
execute_process(COMMAND ${common} --user ${WORK}/user RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err TIMEOUT 120)
string(FIND "${err}" "user folder:" at)
if(NOT at EQUAL -1)
    message(FATAL_ERROR "with --user the notice must not appear: ${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
