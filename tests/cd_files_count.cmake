# The CD's folders under original/ are whole: resource_exchange_cd round-trips whatever files it finds there, so a partial copy would pass with fewer. The counts are the CD's (1235 files in
# all); more is let through (a stray file the system left there is round-tripped like the rest).
# usage: cmake -DORIGINAL=<original/ dir> -P tests/cd_files_count.cmake
set(short "")
foreach(folder SIMDATA:474 GAMEDATA:6 FRONTEND:755)
    string(REPLACE ":" ";" folder "${folder}")
    list(GET folder 0 name)
    list(GET folder 1 want)
    file(GLOB_RECURSE files "${ORIGINAL}/${name}/*")
    list(LENGTH files found)
    message(STATUS "${name}: ${found} files (the CD has ${want})")
    if(found LESS want)
        string(APPEND short " ${name} has ${found} of ${want}.")
    endif()
endforeach()
if(short)
    message(FATAL_ERROR "original/ is not the whole CD:${short}")
endif()
