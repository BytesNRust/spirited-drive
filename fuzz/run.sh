#!/usr/bin/env bash
# usage: fuzz/run.sh <build dir configured with -DTNFS_FUZZ=ON> <seconds per reader> <reader>...
# Runs each reader's libFuzzer program for the given time (docs/fuzzing.md). The corpus lives in <build>/fuzz-corpus/<reader>, seeded once
# from tests/fuzz/<reader>, the repository's data/ and, with FUZZ_SEEDS set, the player's own files there (never committed); a crash, a
# sanitizer report or a hang stops the reader and leaves its input in <build>/fuzz-crashes. Exit 1 when any reader failed.
set -u
set -f   # the extensions below are patterns for find, never for the shell
build=$1 seconds=$2
shift 2
repo=$(cd "$(dirname "$0")/.." && pwd)
seeds=${FUZZ_SEEDS:-}

# the files a reader is seeded with, by extension; validate's first byte picks the file name (fuzz/validate.c's NAMES)
patterns() {
    case $1 in
        qfs) echo "*.qfs" ;;
        fsh) echo "*.fsh" ;;
        chunk|doc) echo "*.cfm *.fam" ;;
        tri) echo "*.tri" ;;
        bnk) echo "*.bnk" ;;
        wav) echo "*.wav" ;;
        png) echo "*.png" ;;
        ico) echo "*.ico" ;;
        ffn) echo "*.ffn" ;;
        records) echo "*.pbs *.pdn" ;;
        psx_bigf) echo "*.psh *.viv *.bin" ;;
        track_light|track_places|terrain_rules) echo "*.txt" ;;
        settings_cfg) echo "*.cfg" ;;
        config_dat) echo "config.dat" ;;
        psx_light) echo "*.lgs *.lgt *.inf" ;;
        masks) echo "*.png" ;;
        sound_wav) echo "*.wav" ;;
        title_art) echo "*.qfs" ;;
        replay_trailer) echo "*.rpl" ;;
        car_model) echo "*.cfm" ;;
        track_art) echo "*.fam" ;;
        sound_bank) echo "*.bnk" ;;
        music_stream) echo "*.asf *.as4" ;;
        tgv) echo "*.tgv" ;;
        shape_bank) echo "*.fsh" ;;
        *) echo "" ;;
    esac
}
validate_byte() {
    case $(echo "${1##*.}" | tr 'A-Z' 'a-z') in
        tri) echo 0 ;; fam) echo 1 ;; cfm) echo 2 ;; bnk) echo 3 ;; pbs) echo 4 ;; pdn) echo 5 ;; qfs) echo 6 ;; fsh) echo 7 ;; *) echo "" ;;
    esac
}

seed_from() {   # <reader> <corpus> <folder>
    local name=$1 corpus=$2 dir=$3 n=0
    [ -d "$dir" ] || return 0
    local pats
    if [ "$name" = validate ]; then pats="*.tri *.fam *.cfm *.bnk *.pbs *.pdn *.qfs *.fsh"; else pats=$(patterns "$name"); fi
    for p in $pats; do
        while IFS= read -r f; do
            [ "$n" -ge 300 ] && break
            local base
            base=$(basename "$f")
            if [ "$name" = validate ]; then
                local b
                b=$(validate_byte "$f")
                [ -n "$b" ] || continue
                { printf "\\$(printf '%03o' "$b")"; head -c 1048576 "$f"; } > "$corpus/seed_${n}_$base"
            else
                head -c 1048576 "$f" > "$corpus/seed_${n}_$base"   # a big file's start: its first chunks are what a reader takes first
            fi
            n=$((n + 1))
        done < <(find -L "$dir" -type f -iname "$p" 2>/dev/null | sort | head -300)
    done
}

failed=0
mkdir -p "$build/fuzz-crashes"
for name in "$@"; do
    prog=$build/fuzz_$name
    [ -x "$prog" ] || { echo "fuzz: no $prog (configure with -DTNFS_FUZZ=ON)"; failed=1; continue; }
    corpus=$build/fuzz-corpus/$name
    if [ ! -d "$corpus" ]; then
        mkdir -p "$corpus"
        seed_from "$name" "$corpus" "$repo/data"
        [ -n "$seeds" ] && seed_from "$name" "$corpus" "$seeds"
    fi
    echo "== $name: $seconds s, corpus $(find "$corpus" -type f | wc -l | tr -d ' ') files"
    if ! "$prog" -max_total_time="$seconds" -timeout=10 -rss_limit_mb=2048 -max_len=4194304 -print_final_stats=1 \
            -artifact_prefix="$build/fuzz-crashes/$name-" "$corpus" $( [ -d "$repo/tests/fuzz/$name" ] && echo "$repo/tests/fuzz/$name") > "$build/fuzz-crashes/$name.log" 2>&1; then
        echo "fuzz: $name FAILED, see $build/fuzz-crashes/$name.log"
        grep -m3 -E "ERROR: |runtime error|SUMMARY" "$build/fuzz-crashes/$name.log"
        failed=1
    else
        grep -E "stat::number_of_executed_units|cov: " "$build/fuzz-crashes/$name.log" | tail -2
    fi
done
exit $failed
