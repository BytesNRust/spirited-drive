# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P hud_lights_view_change.cmake
# The top row after a change of view just as the start lights go out. A change of view runs the full redraw (004218d0), which resets the HUD; the exe
# draws its top row on the frame, so the redraw's labels cover the lights there. The port keeps the top row in its own strip: a change in the ticks
# after the green lights left them in the strip under the numbers for the whole race (the owner's green boxes). The top rows after a change at that
# moment must be the ones after a change a little later, once the labels were in.
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref")
file(WRITE "${WORK}/user/port.cfg" "profile=restomod\nformat=21:9\n")
set(top_rows 40)
foreach(at 182 200)   # 182: the ticks just after the lights' last draw (race tick 0x16c); 200: after the labels
    execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ASAN_OPTIONS=detect_leaks=0
                            TNFS_FRAMES=100 TNFS_VIEW_AT=${at}:2
                            ${TNFS} --data ${REPO}/original --user ${WORK}/user --drive TR2 --node 240 --screenshot ${WORK}/at${at}.ppm
                    WORKING_DIRECTORY "${REPO}" RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 200)
    if(NOT EXISTS "${WORK}/at${at}.ppm")
        message(FATAL_ERROR "the run with the view changed at ${at} wrote no frame (rc=${rc}): ${err}")
    endif()
    if(err MATCHES "AddressSanitizer|runtime error")
        message(FATAL_ERROR "sanitizer error: ${err}")
    endif()
    file(READ "${WORK}/at${at}.ppm" head LIMIT 32)
    string(REGEX MATCH "^P6[ \n]+([0-9]+)[ \n]+([0-9]+)[ \n]+255\n" header "${head}")
    if(header STREQUAL "")
        message(FATAL_ERROR "at${at}.ppm: not a P6 picture")
    endif()
    string(LENGTH "${header}" offset)
    math(EXPR bytes "${CMAKE_MATCH_1} * 3 * ${top_rows}")
    file(READ "${WORK}/at${at}.ppm" strip_${at} OFFSET ${offset} LIMIT ${bytes} HEX)
endforeach()
if(NOT strip_182 STREQUAL strip_200)
    message(FATAL_ERROR "the top row after a change of view as the start lights went out is not the one after a later change: the lights stayed in the strip")
endif()
file(REMOVE_RECURSE "${WORK}")
