# The game's --help and -h print the usage on standard output and end with status 0 (a package's check and a script take any other for a failure); an
# option it does not know still ends with a failure. No game data, no window, nothing written.
# usage: cmake -DTNFS=<tnfs executable> -P tests/exe_help.cmake
foreach(flag --help -h)
    execute_process(COMMAND "${TNFS}" ${flag} RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "${flag}: status ${status}, not 0: ${err}")
    endif()
    if(NOT out MATCHES "usage: .*--drive NAME.*--help")
        message(FATAL_ERROR "${flag}: no usage on standard output: '${out}'")
    endif()
endforeach()
execute_process(COMMAND "${TNFS}" --no-such-option RESULT_VARIABLE status OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 60)
if(status STREQUAL "0")
    message(FATAL_ERROR "--no-such-option: status 0, an unknown option must fail")
endif()
