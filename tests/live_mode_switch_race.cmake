# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P live_mode_switch_race.cmake
# A change of mode with no restart (port_follow.h): a rally on Rusty Springs in Stock, the mode made Restomod as Game Mode's Yes makes it (TNFS_DRIVE_AGAIN, development builds),
# then the same rally again: the second draws Restomod's dust (spray v4), as a game started in Restomod does. Before, the first race's switch stayed for the whole game.
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref")
file(WRITE "${WORK}/user/port.cfg" "profile=stock\n")
# Both races run their clock four times faster (TNFS_CLOCK_RATE), so no key goes in at a time: TNFS_TRACE_PLAYER=510 logs mark:trace=510, 1020 ... 3060, and the one key sent in a
# running race, the Esc, waits for the first race's tick 510.
# the first race seen (its dust's switch logged), given up at its tick 510 from the pause menu (Esc, Up to Quit, Enter, Y), Enter twice on its results once they have faded in (headless the first
# wakes the pointer onto Done), then the second race. Each key waits for the game to read it: mark:race_keys (an earlier Esc is ignored), mark:race_menu (the menu's keys),
# mark:yes_no (its question: keys sent before are dropped). The second race runs to its tick 3060 (what the old fixed waits gave it: a crash or a sanitizer's report in it
# still fails the test): mark:trace=3060, which the first race, given up soon after its tick 510, never reaches. The script ends there (status 3); a line
# that never comes ends the run at that wait (status 3 too) and the checks below say what is missing.
set(script "0:WAIT:mark:spray_v4=0:90,0:WAIT:mark:race_keys:90,0:WAIT:mark:trace=510:90,0:ESC,0:WAIT:mark:race_menu:60,300:UP,300:ENTER,0:WAIT:mark:yes_no:60,300:Y")
string(APPEND script ",0:WAIT:mark:results=circuit:120,300:ENTER,300:ENTER,0:WAIT:mark:spray_v4=1:90,0:WAIT:mark:trace=3060:200,0:WAIT:mark:never:1")
execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ASAN_OPTIONS=detect_leaks=0 TNFS_BANDS=0 TNFS_VERBOSE=1
                        TNFS_INPUT_SETTLE=300 TNFS_DRIVE_AGAIN=restomod TNFS_CLOCK_RATE=4 TNFS_TRACE_PLAYER=510 "TNFS_INPUT_SCRIPT=${script}"
                        ${TNFS} --data ${REPO}/original --mods ${WORK} --user ${WORK}/user --drive TR1r --type 0
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err TIMEOUT 400)
if(NOT rc EQUAL 3)
    message(FATAL_ERROR "expected status 3 (the script's end), got ${rc}: ${err}")
endif()
file(READ "${WORK}/user/spirited-drive.log" log)
string(FIND "${log}" "bad TNFS_INPUT_SCRIPT item" bad)
if(NOT bad EQUAL -1)
    message(FATAL_ERROR "the input script has an item the game does not take: ${err}")
endif()
# a mark's own line only: the script's "waiting for mark:..." lines name the same text and must not satisfy these
string(FIND "${log}" "\nmark:spray_v4=0\n" stock)
string(FIND "${log}" "\nmark:mode=restomod\n" changed)
string(FIND "${log}" "\nmark:spray_v4=1\n" restomod)
if(stock EQUAL -1 OR changed EQUAL -1 OR NOT stock LESS changed)
    message(FATAL_ERROR "the first race was not Stock's, or the mode was not changed after it: ${err}")
endif()
if(restomod EQUAL -1 OR NOT changed LESS restomod)
    message(FATAL_ERROR "the second race, in Restomod, did not draw Restomod's dust (spray v4): ${err}")
endif()
string(FIND "${log}" "\nmark:trace=510\n" given_up)
if(given_up EQUAL -1 OR NOT given_up LESS changed)
    message(FATAL_ERROR "the first race did not run to its tick 510 before the Esc: ${err}")
endif()
# the first 3060 in the log: after the second race's start, so the first race was given up before it
string(FIND "${log}" "\nmark:trace=3060\n" ran)
if(ran EQUAL -1 OR NOT restomod LESS ran)
    message(FATAL_ERROR "the second race did not run to its tick 3060: ${err}")
endif()
file(REMOVE_RECURSE "${WORK}")
