# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P mods_stock_ignored.cmake
# Stock loads no mods, so --mods is ignored there; a --track of a piece's stem then fails (the piece is not loaded) and the run must say why on stderr, not only in the log.
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref" "${WORK}/piece")
file(WRITE "${WORK}/user/port.cfg" "profile=stock\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ASAN_OPTIONS=detect_leaks=0 TNFS_FRAMES=1 TNFS_BANDS=0
                        ${TNFS} --data ${REPO}/original --mods ${WORK}/piece --user ${WORK}/user --track NOSUCHPC --screenshot ${WORK}/shot.ppm
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err TIMEOUT 120)
if(rc EQUAL 0)
    message(FATAL_ERROR "a track that is not there was drawn")
endif()
string(FIND "${err}" "is ignored: stock loads no mods (Restomod only)" at)
if(at EQUAL -1)
    message(FATAL_ERROR "stderr does not say why the piece was ignored: ${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
