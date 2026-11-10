# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P dev_data_default.cmake
# A development build run from the repository with --mods and no --data must find ./original (as tools/golden_frames.sh runs it) and write its frame.
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/mods" "${WORK}/pref")
execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
                        ${TNFS} --mods ${WORK}/mods --drive AL1 --entry 20 --screenshot ${WORK}/frame.ppm
                WORKING_DIRECTORY "${REPO}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 100)
if(NOT EXISTS "${WORK}/frame.ppm")
    message(FATAL_ERROR "no frame (rc=${rc}): ${out} ${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
