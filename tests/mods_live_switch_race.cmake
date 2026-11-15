# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P mods_live_switch_race.cmake
# dep59 n4 phase 2: the mods in effect follow a change of mode with no restart (mod_set.h). A rally in Stock with --mods given: Stock loads no mods, so --mods is ignored
# with a line in the log; the mode made Restomod as Game Mode's Yes makes it (TNFS_DRIVE_AGAIN, development builds), then the same rally again in a storm: --mods is in
# effect (the settings' write puts it in effect before the mode's mark is logged) and the storm's thunder is read from the mod's sounds/thunder.wav (the repo's own CC0
# sample, copied).
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref" "${WORK}/mod/sounds")
file(COPY_FILE "${REPO}/data/sounds/thunder.wav" "${WORK}/mod/sounds/thunder.wav")
file(WRITE "${WORK}/user/port.cfg" "profile=stock\n")
file(WRITE "${WORK}/user/restomod.cfg" "weather=storm\nrain_v2=1\n")
# the first race seen, given up from the pause menu and Enter through the results to the second, which runs until its first thunder is read; a line that never comes then
# ends the run (status 3)
set(script "0:WAIT:mark:spray_v4=0:90,8000:ESC,2000:UP,2000:ENTER,2000:Y")
foreach(i RANGE 5)
    string(APPEND script ",3000:ENTER")
endforeach()
string(APPEND script ",0:WAIT:thunder: :150,0:WAIT:mark:never:2")
execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ASAN_OPTIONS=detect_leaks=0 TNFS_BANDS=0 TNFS_VERBOSE=1
                        TNFS_INPUT_SETTLE=300 TNFS_DRIVE_AGAIN=restomod "TNFS_INPUT_SCRIPT=${script}"
                        ${TNFS} --data ${REPO}/original --mods ${WORK}/mod --user ${WORK}/user --drive TR1r --type 0
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err TIMEOUT 500)
if(NOT rc EQUAL 3)
    message(FATAL_ERROR "expected status 3 (the script's end), got ${rc}: ${err}")
endif()
file(READ "${WORK}/user/spirited-drive.log" log)
string(FIND "${log}" "mods: --mods ${WORK}/mod is ignored: stock loads no mods" ignored)
string(FIND "${log}" "mark:mode=restomod" changed)
string(FIND "${log}" "mods: in effect: mod\n" in_effect)
string(FIND "${log}" "thunder: ${WORK}/mod/sounds/thunder.wav: loaded" thunder)
if(ignored EQUAL -1 OR changed EQUAL -1 OR NOT ignored LESS changed)
    message(FATAL_ERROR "Stock did not log --mods as ignored before the change of mode")
endif()
if(in_effect EQUAL -1 OR NOT ignored LESS in_effect OR NOT in_effect LESS changed)
    message(FATAL_ERROR "the change to Restomod did not put --mods in effect")
endif()
if(thunder EQUAL -1 OR NOT in_effect LESS thunder)
    message(FATAL_ERROR "the storm's thunder in Restomod was not read from the mod")
endif()
file(REMOVE_RECURSE "${WORK}")
