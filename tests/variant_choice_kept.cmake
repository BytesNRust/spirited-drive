# usage: cmake -DTNFS=<exe> -DREPO=<repository> -DWORK=<scratch dir> -P variant_choice_kept.cmake
# dep65: Restomod's track switches are the player's choice and nothing on the way clears it. The walk turns Mirror, Reverse and
# Rally on in the track screen (on Autumn Valley, the morning picked), goes to Options and back, then steps Control Central's
# card onto Lost Vegas and back. The card logs its words as they change ("mark:card=<track> <variants>|<laps, time, weather>"):
# after Options every circuit still has the three, Lost Vegas says only what it runs (the reverse, its night) and the track after
# it has the three again. Before the fix Options cleared them (config.dat's race settings read back) and Lost Vegas dropped the
# mirror and the rally for every track after it.
include("${CMAKE_CURRENT_LIST_DIR}/unique_work.cmake")
set(step 60)
set(next_track "300:MOUSE:390:455,300:CLICK:390:455,2000:MOUSE:330:405")
set(card_right "1500:CLICK:543:317")
set(script "0:WAIT:mark:screen=title:${step},4000:ESC,0:WAIT:mark:screen=game_mode:${step},300:CLICK:310:170,0:WAIT:mark:screen=name_box:${step}")
set(script "${script},500:TEXT:R,500:ENTER,0:WAIT:mark:screen=central:${step}")
# the track screen: three tracks on (Coastal to Autumn Valley), the three switches on, Done
set(script "${script},300:CLICK:400:320,0:WAIT:mark:screen=track_select:${step},${next_track},${next_track},${next_track}")
set(script "${script},1200:CLICK:150:281,1200:CLICK:150:304,1200:CLICK:150:327,1500:CLICK:40:455,0:WAIT:MIRROR+REVERSE+RALLY:${step}")
# Options and back
set(script "${script},4000:CLICK:215:450,0:WAIT:mark:screen=options:${step},3000:ESC,4000:MOUSE:600:470")
# the card on to Lost Vegas (Autumn Valley, 5, 6, 7, 8), then back one
set(script "${script},${card_right},${card_right},${card_right},${card_right},0:WAIT:mark:card=8 :${step},1500:CLICK:348:317")
set(script "${script},0:WAIT:mark:card=7 :${step},0:WAIT:mark:never:3")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}/user" "${WORK}/pref" "${WORK}/mods")
file(WRITE "${WORK}/user/port.cfg" "profile=restomod\n")
file(WRITE "${WORK}/user/restomod.cfg" "no_movies=1\nlight_time=dawn\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E env TNFS_PREF_DIR=${WORK}/pref SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy ASAN_OPTIONS=detect_leaks=0
                        TNFS_INPUT_SETTLE=1500 "TNFS_INPUT_SCRIPT=${script}"
                        ${TNFS} --data ${REPO}/original --mods ${WORK}/mods --user ${WORK}/user
                WORKING_DIRECTORY "${REPO}" RESULT_VARIABLE rc OUTPUT_QUIET ERROR_VARIABLE err TIMEOUT 280)
if(err MATCHES "AddressSanitizer|runtime error")
    message(FATAL_ERROR "sanitizer error: ${err}")
endif()
file(READ "${WORK}/user/spirited-drive.log" log)
string(FIND "${err}" "WAIT mark:never timed out" reached)
if(NOT rc EQUAL 3 OR reached EQUAL -1)
    string(REGEX MATCHALL "(mark:card|mark:screen|input script)[^\n]*" seen "${log}")
    message(FATAL_ERROR "the walk did not end on Control Central (status ${rc}): ${err}\n${seen}")
endif()
# the card's lines from Options on
string(FIND "${log}" "mark:screen=options" at)
string(SUBSTRING "${log}" ${at} -1 after)
string(REGEX MATCHALL "mark:card=[^\n]*" cards "${after}")
list(FILTER cards EXCLUDE REGEX " (came|waiting|timed out)")   # the input script's own lines about its WAITs
list(LENGTH cards count)
if(count LESS 4)
    message(FATAL_ERROR "the card said too little after Options: ${cards}")
endif()
foreach(card IN LISTS cards)
    if(card MATCHES "^mark:card=8 ")
        if(NOT card STREQUAL "mark:card=8 REVERSE|8 LAPS+NIGHT")
            message(FATAL_ERROR "Lost Vegas should say only its reverse and its night: ${card}")
        endif()
    elseif(NOT card MATCHES "^mark:card=[3-7] MIRROR\\+REVERSE\\+RALLY\\|[0-9]+ LAPS\\+MORNING$")
        message(FATAL_ERROR "a circuit lost the player's switches or morning: ${card} (all: ${cards})")
    endif()
endforeach()
list(GET cards -1 last)
if(NOT last MATCHES "^mark:card=7 ")
    message(FATAL_ERROR "the walk did not come back from Lost Vegas: ${cards}")
endif()
file(REMOVE_RECURSE "${WORK}")
