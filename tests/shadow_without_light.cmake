# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P shadow_without_light.cmake
# The soft shadow (psx_shadow) is its own switch: it needs no light and no PS1 files (drive_light.c). With the light off (psx_light=0) and the
# shadow on, a car row drawn at the start read the light's node count through a null light (row_prepare's tunnel look) and the run died on a
# segmentation fault. The race must draw its frame.
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref")
file(WRITE "${WORK}/user/port.cfg" "profile=custom\npsx_dir=\npsx_light=0\npsx_shadow=1\nformat=16:9\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ASAN_OPTIONS=detect_leaks=0
                        TNFS_VIEW=0 TNFS_FRAMES=1
                        ${TNFS} --data ${REPO}/original --user ${WORK}/user --drive CY1 --node 70 --screenshot ${WORK}/end.ppm
                WORKING_DIRECTORY "${REPO}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 100)
if(NOT EXISTS "${WORK}/end.ppm")
    message(FATAL_ERROR "the shadow on with no light wrote no frame (rc=${rc}): ${err}")
endif()
if(err MATCHES "AddressSanitizer|runtime error")
    message(FATAL_ERROR "sanitizer error: ${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
