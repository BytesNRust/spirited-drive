#!/bin/sh
# The acceptance check of the migration of the original cars (dep100 item 6): three cars (the 911 you drive, the Wagon and the police car, a traffic car with no PBS)
# are exported by hand from Studio, the way a person does it, and each is compared file by file with its migrated kit. They may differ ONLY in art-source/masks (the PSDs
# of the owner, which a Studio export does not have); any other difference rejects the migration. build/ is left out: a Studio run makes it on opening, it is disposable.
# With REPORT the result is added to REPORT's file (MIGRATION-REPORT.md).
# With MIGRATED_DIR "-" the migration is made first, in a scratch folder, by EXPORTER (car_export_kits; no art folder: no PSDs, so nothing may differ at all).
# usage: migration_acceptance.sh EDITOR ORIGINAL_DIR DATA_DIR MIGRATED_DIR [REPORT [EXPORTER]]
editor=$1; orig=$2; data=$3; migrated=$4; report=$5; exporter=$6
[ -r "$orig/SIMDATA/CARFAMS/P911.CFM" ] && [ -d "$data/car_masks/P911" ] || { echo "skip: no game files in $orig"; exit 77; }
work=$(mktemp -d "${TMPDIR:-/tmp}/migration-acceptance-XXXXXX") || exit 1
trap 'python3 -c "import shutil,sys; shutil.rmtree(sys.argv[1], ignore_errors=True)" "$work"' EXIT
if [ "$migrated" = "-" ]; then
    migrated=$work/migrated
    "$exporter" "$orig" "$data" "$migrated" >"$work/migrate.log" 2>&1 || { echo "FAIL: the migration: $(tail -5 "$work/migrate.log")"; exit 1; }
fi
failures=0
lines=""
for car in P911 Wagon COPMUST; do
    kit=$(printf '%s' "$car" | tr 'A-Z' 'a-z')
    SDL_AUDIODRIVER=dummy "$editor" --export-car "$car" --into "$work" --cd "$orig" --port-data "$data" --user "$work/user" --frames 1 >"$work/$car.log" 2>&1 \
        || { echo "FAIL: Studio could not export $car: $(cat "$work/$car.log")"; failures=1; continue; }
    [ -d "$migrated/$kit" ] || { echo "FAIL: $migrated has no $kit"; failures=1; continue; }
    diff -rq -x build "$work/$kit" "$migrated/$kit" >"$work/$car.diff" 2>&1
    # the one difference allowed: the PSDs' folder is only in the migrated kit
    other=$(grep -v "^Only in $migrated/$kit/art-source: masks\$" "$work/$car.diff")
    if [ -n "$other" ]; then
        echo "FAIL: $car differs from its migrated kit beyond art-source/masks:"; echo "$other"; failures=1
        lines="$lines- $car: REJECTED, differs beyond art-source/masks\n"
    else
        n=$(find "$work/$kit" -type f ! -path '*/build/*' | wc -l | tr -d ' ')
        echo "ok: $car: $n files, equal to its migrated kit (art-source/masks aside)"
        lines="$lines- $car: $n files, byte for byte the migrated kit's; the only difference is art-source/masks (the owner's PSDs)\n"
    fi
done
if [ -n "$report" ] && [ "$failures" -eq 0 ]; then
    { printf '\n## Acceptance: a Studio export against the migrated kit\n\n'; printf "%b" "$lines"; } >> "$report"
fi
[ "$failures" -eq 0 ] && echo "migration_acceptance: ok"
exit "$failures"
