# The fuzz harnesses (docs/fuzzing.md): one per reader of a file a mod can carry. Every build replays the cases kept in tests/fuzz/<reader>
# as the ctest fuzz_<reader>; -DTNFS_FUZZ=ON (clang) also builds the libFuzzer programs fuzz_<reader> and the target `fuzz` that runs them.
option(TNFS_FUZZ "Build the libFuzzer harnesses and the fuzz target (clang)" OFF)
set(TNFS_FUZZ_SECONDS 600 CACHE STRING "Seconds the fuzz target gives each reader")
set(TNFS_FUZZ_SEEDS "" CACHE PATH "A folder of the player's own files to seed from (never committed): fuzz/run.sh picks each reader's by extension")

# the readers in tnfs_core, then those in tnfs_game
set(TNFS_FUZZ_CORE qfs fsh chunk tri bnk wav png ico ffn records psx_bigf doc validate zip mod_ini mod_check edition_ini)
set(TNFS_FUZZ_GAME track_light terrain_rules settings_cfg config_dat psx_light masks sound_wav title_art replay_trailer car_model track_art sound_bank music_stream tgv shape_bank)

if(NOT EMSCRIPTEN AND NOT WIN32)
    if(TNFS_FUZZ)
        # coverage for the libraries the harnesses reach; ASan and UBSan come from CMAKE_C_FLAGS (docs/fuzzing.md), and a UBSan report stops
        foreach(lib tnfs_core tnfs_game)
            target_compile_options(${lib} PRIVATE -fsanitize=fuzzer-no-link -fno-sanitize-recover=undefined)
        endforeach()
    endif()
    foreach(name IN LISTS TNFS_FUZZ_CORE TNFS_FUZZ_GAME)
        if(name IN_LIST TNFS_FUZZ_CORE)
            set(lib tnfs_core)
            set(extra)
        else()
            set(lib tnfs_game)
            set(extra fuzz/platform_stub.c)
        endif()
        add_executable(fuzz_replay_${name} fuzz/${name}.c fuzz/replay.c ${extra})
        target_link_libraries(fuzz_replay_${name} PRIVATE ${lib})
        target_include_directories(fuzz_replay_${name} PRIVATE ${CMAKE_SOURCE_DIR})
        # a reader with no kept cases yet replays nothing
        set(cases)
        if(EXISTS ${CMAKE_SOURCE_DIR}/tests/fuzz/${name})
            set(cases ${CMAKE_SOURCE_DIR}/tests/fuzz/${name})
        endif()
        add_test(NAME fuzz_${name} COMMAND fuzz_replay_${name} ${cases})
        set_tests_properties(fuzz_${name} PROPERTIES TIMEOUT 120)
        if(TNFS_FUZZ)
            add_executable(fuzz_${name} fuzz/${name}.c ${extra})
            target_link_libraries(fuzz_${name} PRIVATE ${lib})
            target_include_directories(fuzz_${name} PRIVATE ${CMAKE_SOURCE_DIR})
            target_compile_options(fuzz_${name} PRIVATE -fsanitize=fuzzer -fno-sanitize-recover=undefined)
            target_link_options(fuzz_${name} PRIVATE -fsanitize=fuzzer)
            list(APPEND tnfs_fuzz_programs fuzz_${name})
        endif()
    endforeach()
    if(TNFS_FUZZ)
        add_custom_target(fuzz
            COMMAND ${CMAKE_COMMAND} -E env FUZZ_SEEDS=${TNFS_FUZZ_SEEDS}
                    bash ${CMAKE_SOURCE_DIR}/fuzz/run.sh ${CMAKE_BINARY_DIR} ${TNFS_FUZZ_SECONDS} ${TNFS_FUZZ_CORE} ${TNFS_FUZZ_GAME}
            DEPENDS ${tnfs_fuzz_programs}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            USES_TERMINAL)
    endif()
endif()
