#!/bin/sh
# A car kit in the current layout in the Studio, the happy path a user takes: Open (the car file is built as it opens), Preview, the mask changed from outside
# (in the kit's flat art-generated/car_masks/), Preview shows the edit, Build (build/piece and the zip carry the edit), Install into a player folder. The kit is a car you
# drive, laid out by `tnfs-studio car kit`, its card given a stem of its own (a mod's stem is no original car's). Needs the game's files (original/).
# usage: editor_e2e_kit.sh EDITOR TNFS ORIGINAL_DIR PORT_DATA STUDIO
. "$(dirname "$0")/editor_headless_common.sh"
tnfs=$2
orig=$3
data=$4
studio_cli=$5
[ -r "$orig/SIMDATA/CARFAMS/P911.CFM" ] && [ -d "$data/car_masks/P911" ] || { echo "skip: no game files in $orig"; exit 0; }
runs=0
studio() { runs=$((runs + 1)); "$editor" "$@" --user "$work/player$runs" >"$work/log" 2>&1; }

# 1. a kit of a car you drive: the game's 911 exported as a kit, copied, with a card of its own (an original car's stem is not allowed for a package)
studio --export-car P911 --into "$work/cars" --cd "$orig" --port-data "$data" --frames 1 || fail "Export original car: $(cat "$work/log")"
cp -R "$work/cars/p911" "$work/kit" || fail "no copy of the export"
python3 -c 'import shutil,sys; shutil.rmtree(sys.argv[1], ignore_errors=True)' "$work/kit/build"   # the export was opened once: its preview build is not part of the kit
[ -f "$work/kit/mod.ini" ] && [ -d "$work/kit/model/near/bank" ] || { fail "no kit was made"; exit "$failures"; }
sed 's/^stem=.*/stem=FLOW911/; s/^id=.*/id=flow911/' "$work/kit/mod.ini" > "$work/mod.ini.new" && mv "$work/mod.ini.new" "$work/kit/mod.ini"
kit="$work/kit"
python3 -c 'import sys,zlib,struct
w,h=309,166
def ch(t,d): return struct.pack(">I",len(d))+t+d+struct.pack(">I",zlib.crc32(t+d))
raw=b"".join(b"\0"+b"\x80"*(w*3) for _ in range(h))
open(sys.argv[1],"wb").write(b"\x89PNG\r\n\x1a\n"+ch(b"IHDR",struct.pack(">IIBBBBB",w,h,8,2,0,0,0))+ch(b"IDAT",zlib.compress(raw))+ch(b"IEND",b""))' "$kit/card.png" || fail "no card.png: the game wants Control Central's picture (309 x 166) in a piece"
mask="$kit/art-generated/car_masks/frnt_mask.png"
[ -f "$mask" ] || fail "the kit has no flat art-generated/car_masks/frnt_mask.png"
[ ! -d "$kit/build" ] || fail "a fresh kit has a build/"

# 2. Open: the car file is built as the kit opens (a headless run waits for it), the Preview shows the car
preview() { studio "$kit" --view day --preview-tex frnt --frames 14 --shot "$1"; }
preview "$work/preview-before.png" || fail "Open and Preview of the kit: $(cat "$work/log")"
[ -f "$kit/build/piece/SIMDATA/CARFAMS/FLOW911.CFM" ] || fail "Open did not build the car file: $(cat "$work/log")"
[ -s "$work/preview-before.png" ] || fail "no PNG of the Preview"

# 3. The kit's flat mask file changed from outside Studio, and the mask view reads it without writing it
cp "$mask" "$work/mask-before.png"
python3 -I "$(dirname "$0")/mask_poke.py" "$mask" 40 40 8 8 90 || fail "the outside change of the mask"
cp "$mask" "$work/mask-poked.png"
studio "$kit" --mask frnt --frames 8 || fail "mask view: $(cat "$work/log")"
cmp -s "$mask" "$work/mask-poked.png" || fail "the mask view wrote the mask file"
cmp -s "$mask" "$work/mask-before.png" && fail "the outside change was not made"

# 4. Preview shows the edit
preview "$work/preview-after.png" || fail "Preview of the edited kit: $(cat "$work/log")"
cmp -s "$work/preview-before.png" "$work/preview-after.png" && fail "the Preview does not show the edit"

# 4a. Check on a kit (the Check button): the 911 kit has no finding; a mask with no texture is a warning that the report lists
studio "$kit" --steps check --frames 4 || fail "Check on the kit: $(cat "$work/log")"
grep -q "^check: checked: 0 errors, 0 warnings" "$work/log" || fail "Check found something in a good kit: $(cat "$work/log")"
cp "$mask" "$kit/art-generated/car_masks/zzzz_mask.png"
studio "$kit" --steps check --frames 4 || fail "Check on a kit with a mask of no texture: $(cat "$work/log")"
grep -q "^check warning: zzzz_mask.png has no texture zzzz" "$work/log" && grep -q "^check: checked: 0 errors, 1 warning" "$work/log" || fail "Check does not list the mask of no texture: $(cat "$work/log")"
studio "$kit" --build --frames 2 || fail "Build of a kit with a mask of no texture: $(cat "$work/log")"
grep -q "built the car: .*; the check found 0 errors, 1 warning" "$work/log" || fail "Build does not say what its check found: $(cat "$work/log")"
python3 -c 'import os,sys; os.remove(sys.argv[1])' "$kit/art-generated/car_masks/zzzz_mask.png"

# 4b. Use as card photo and Add photo on a kit: card.png is the KIT's (not build/piece's), the showcase pictures are in the kit's photos/; the next Build carries both
cp "$kit/card.png" "$work/card-before.png"
studio "$kit" --cd "$orig" --paint 3 --steps card-photo,status --frames 14 || fail "Use as card photo on a kit: $(cat "$work/log")"
grep -q "status: card.png made" "$work/log" || fail "no word that card.png was made on the kit: $(cat "$work/log")"
cmp -s "$kit/card.png" "$work/card-before.png" && fail "Use as card photo did not write the kit's card.png"
python3 -c 'import sys,zlib,struct
w,h,g=int(sys.argv[2]),int(sys.argv[3]),int(sys.argv[4])
def ch(t,d): return struct.pack(">I",len(d))+t+d+struct.pack(">I",zlib.crc32(t+d))
raw=b"".join(b"\0"+bytes([g,g,g,255])*w for _ in range(h))
open(sys.argv[1],"wb").write(b"\x89PNG\r\n\x1a\n"+ch(b"IHDR",struct.pack(">IIBBBBB",w,h,8,6,0,0,0))+ch(b"IDAT",zlib.compress(raw))+ch(b"IEND",b""))' "$work/Front Shot.png" 450 254 90 || fail "no showcase picture"
python3 -c 'import sys,zlib,struct
w,h,g=int(sys.argv[2]),int(sys.argv[3]),int(sys.argv[4])
def ch(t,d): return struct.pack(">I",len(d))+t+d+struct.pack(">I",zlib.crc32(t+d))
raw=b"".join(b"\0"+bytes([g,g,g,255])*w for _ in range(h))
open(sys.argv[1],"wb").write(b"\x89PNG\r\n\x1a\n"+ch(b"IHDR",struct.pack(">IIBBBBB",w,h,8,6,0,0,0))+ch(b"IDAT",zlib.compress(raw))+ch(b"IEND",b""))' "$work/Rear Shot.png" 300 200 200 || fail "no second showcase picture"
studio "$kit" --add-photo "$work/Front Shot.png" --add-photo "$work/Rear Shot.png" --save --frames 4 || fail "Add photo on a kit: $(cat "$work/log")"
grep -q '^photos=front-shot.png,rear-shot.png$' "$kit/mod.ini" || fail "photos= is not the two safe names: $(grep photos "$kit/mod.ini")"
[ -s "$kit/photos/front-shot.png" ] && [ -s "$kit/photos/rear-shot.png" ] || fail "the pictures are not in the kit's photos/"
[ ! -e "$kit/build/piece/front-shot.png" ] || fail "a photo was written into the disposable build/piece"

# 5. Build: the package is made again from the sources, and carries the edit under the stem
studio "$kit" --build --frames 2 || fail "Build: $(cat "$work/log")"
[ -s "$kit/build/flow911.zip" ] || fail "Build made no build/flow911.zip: $(cat "$work/log")"
cmp -s "$mask" "$kit/build/piece/car_masks/FLOW911/frnt_mask.png" || fail "the built piece does not carry the edited mask"
cmp -s "$kit/card.png" "$kit/build/piece/card.png" || fail "the built piece does not carry the kit's new card.png"
cmp -s "$kit/photos/front-shot.png" "$kit/build/piece/front-shot.png" && cmp -s "$kit/photos/rear-shot.png" "$kit/build/piece/rear-shot.png" || fail "the built piece does not carry the showcase photos: $(ls "$kit/build/piece")"
unzip -l "$kit/build/flow911.zip" >"$work/zip.txt" 2>&1 || fail "cannot list the zip"
grep -q "card.png" "$work/zip.txt" && grep -q "front-shot.png" "$work/zip.txt" && grep -q "rear-shot.png" "$work/zip.txt" || fail "the zip lacks card.png or the showcase photos: $(cat "$work/zip.txt")"

# 6. Install: the game unpacks the package into the player folder
"$editor" "$kit" --install --game "$tnfs" --frames 2 --user "$work/installed" >"$work/log" 2>&1 || fail "Install: $(cat "$work/log")"
grep -q "installed flow911" "$work/log" || fail "Install does not say it installed the piece: $(cat "$work/log")"
grep -rlq "^id=flow911" "$work/installed" || fail "the piece is not in the player folder"

[ "$failures" -eq 0 ] && echo "editor_e2e_kit: ok"
exit "$failures"
