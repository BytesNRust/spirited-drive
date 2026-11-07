# usage: cmake -DREPO=<repository> -DWORK=<scratch dir> -P try_online_local.cmake
# tools/try_online_local.sh with DRY_RUN=1: the two commands it would run -- a host on UDP 15000 and a guest on 15001 that looks for it, each with its own user folder,
# the data and mods folders, under timeout -k for two hours, and no dummy drivers.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND ${CMAKE_COMMAND} -E env DRY_RUN=1 TNFS_HOME=${WORK} SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy bash ${REPO}/tools/try_online_local.sh ./tnfs
                RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "the dry run failed (${rc}): ${err}")
endif()
foreach(part
    "host:  env -u SDL_VIDEO_DRIVER -u SDL_AUDIO_DRIVER timeout -k 5 7200 ./tnfs --data ./original --mods ./mods --user ./user --net-port 15000\n"
    "guest: env -u SDL_VIDEO_DRIVER -u SDL_AUDIO_DRIVER timeout -k 5 7200 ./tnfs --data ./original --mods ./mods --user ./user-guest --net-port 15001 --net-host 127.0.0.1:15000\n")
    string(FIND "${out}" "${part}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "missing: ${part}\ngot: ${out}")
    endif()
endforeach()
if(EXISTS "${WORK}/user" OR EXISTS "${WORK}/mods")
    message(FATAL_ERROR "a dry run wrote folders")
endif()
file(REMOVE_RECURSE "${WORK}")
