# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P live_mode_switch_race.cmake
# A change of mode with no restart (port_follow.h): a rally on Rusty Springs in Stock, the mode made Restomod as Game Mode's Yes makes it (TNFS_DRIVE_AGAIN, development builds),
# then the same rally again: the second draws Restomod's dust (spray v4), as a game started in Restomod does. Before, the first race's switch stayed for the whole game.
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref")
file(WRITE "${WORK}/user/port.cfg" "profile=stock\n")
# the first race seen (its dust's switch logged), given up from the pause menu (Esc, Up to Quit, Enter, Y) and Enter through the results to the second; a line that never comes ends
# the run (status 3) once the second has run a while. The log then says, in order: Stock's dust, the change of mode, Restomod's dust.
set(script "0:WAIT:mark:spray_v4=0:90,8000:ESC,2000:UP,2000:ENTER,2000:Y")   # Esc once the start's fly-over is over (an Esc in it only cuts it short)
foreach(i RANGE 5)
    string(APPEND script ",3000:ENTER")
endforeach()
string(APPEND script ",0:WAIT:mark:never:40")
execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ASAN_OPTIONS=detect_leaks=0 TNFS_BANDS=0 TNFS_VERBOSE=1
                        TNFS_INPUT_SETTLE=300 TNFS_DRIVE_AGAIN=restomod "TNFS_INPUT_SCRIPT=${script}"
                        ${TNFS} --data ${REPO}/original --mods ${WORK} --user ${WORK}/user --drive TR1r --type 0
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err TIMEOUT 400)
if(NOT rc EQUAL 3)
    message(FATAL_ERROR "expected status 3 (the script's end), got ${rc}: ${err}")
endif()
file(READ "${WORK}/user/spirited-drive.log" log)
string(FIND "${log}" "mark:spray_v4=0" stock)
string(FIND "${log}" "mark:mode=restomod" changed)
string(FIND "${log}" "mark:spray_v4=1" restomod)
if(stock EQUAL -1 OR changed EQUAL -1 OR NOT stock LESS changed)
    message(FATAL_ERROR "the first race was not Stock's, or the mode was not changed after it: ${err}")
endif()
if(restomod EQUAL -1 OR NOT changed LESS restomod)
    message(FATAL_ERROR "the second race, in Restomod, did not draw Restomod's dust (spray v4): ${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
