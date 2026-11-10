# included by the tests that run the game from a cmake -P script, before they touch WORK: the scratch folder is this run's own. WORK was a fixed folder of
# the build (build/<test>), which a second ctest on the same build (another agent, or the same one twice) emptied and rewrote under the first
# (pref_isolation, stats_mods_isolation, old_files_startup and the others failed together, and passed alone).
string(RANDOM LENGTH 8 ALPHABET "0123456789abcdef" _run)
set(WORK "${WORK}.${_run}")
