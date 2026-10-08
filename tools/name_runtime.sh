#!/usr/bin/env bash
# Names Watcom runtime functions in the Ghidra project and refreshes the map.
# Run inside the RE shell: nix develop .#re -c tools/name_runtime.sh
set -euo pipefail
cd "$(dirname "$0")/.."

: "${WATCOM11_LIBS:?run inside nix develop .#re}"
out=analysis/nfs_win
headless="$(dirname "$(readlink -f "$(which ghidra)")")/support/analyzeHeadless"

python3 -I tools/watcom_sigs.py original/NFS_WIN.EXE \
    "$WATCOM11_LIBS"/nt/clib3r.lib "$WATCOM11_LIBS"/nt/clib3s.lib \
    "$WATCOM11_LIBS"/math387r.lib "$WATCOM11_LIBS"/math387s.lib "$WATCOM11_LIBS"/math3r.lib \
    "$WATCOM11_LIBS"/nt/emu387.lib "$WATCOM11_LIBS"/noemu387.lib "$WATCOM11_LIBS"/nt/plib3r.lib \
    -o "$out/watcom_names.tsv"

"$headless" ghidra/project tnfs -process NFS_WIN.EXE -noanalysis -scriptPath ghidra/scripts \
    -postScript ApplyNames.java "$PWD/$out/watcom_names.tsv" \
    -postScript ExportMap.java "$PWD/$out" > "$out/../ghidra_names.log" 2>&1
grep -E 'ApplyNames|ExportMap' "$out/../ghidra_names.log"
