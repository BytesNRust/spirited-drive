#!/bin/zsh
# sweep_asan.sh WORKTREE OUT [PHASE...]: the playable game headless under build/dev (ASan + UBSan), each run with
# the SDL dummy drivers and timeout -k, at most MAXP (default 3) at a time and none started while the 5-minute load
# is 7 or more. Phases (all when none is given):
#   drive   --drive on every track, a give-up from the in-race menu (Esc, Up to Quit, Enter, Y) then Enter through
#           the results to after_race; the cars and race types rotate over the tracks so each car runs several times
#   variant the same give-up on every track's m and r variants (NAMEm, NAMEr), the cars rotating
#   frame   --drive --node --screenshot on every track (the headless frame path)
#   menus   the full game from the intro (TNFS_INPUT_SETTLE: each click once the screen is still): Options' toggles
#           and each sub-screen, Load and Save (save, view, delete; replays and tournaments), the credits, the
#           showcase's pages for every car, each ending at Game Mode's Exit; each run on its own copy of MODS
#   movie   --movie on every TGV
#   long    --drive under TNFS_AUTOPILOT=1 (a test harness only) to the race's end, the full field on the circuits
#           and a segment of each open road: races long enough to wrap the skid marks' ring (125; TR1, TR2, TR6, TR7
#           and AL1 wrap it); an open road's results then wait for a key (rc 124 "after its race over")
#   replay  --replay on each replay of the CD and the scratch config's own to its end, then Esc (which leaves the
#           last picture; one in the replay opens its menu and the next closes it), every 15 s from 60 s
# Each run leaves OUT/<name>.log; a line per run goes to OUT/summary: its rc (124: the timeout ran out), its seconds
# and the sanitizer's first report. MODS (a scratch --mods dir with a user/config.dat) keeps the real config out.
export PATH=/nix/var/nix/profiles/default/bin:$PATH
cd $1 || exit 1
out=${2:a}; shift 2
mkdir -p $out; : > $out/summary
phases=(${@:-drive variant frame replay menus movie})
maxp=${MAXP:-3}
bin=$PWD/build/dev/tnfs
data=$PWD/original
mods=(); [[ -n $MODS ]] && mods=(--mods $MODS)
export SDL_VIDEO_DRIVER=dummy SDL_AUDIO_DRIVER=dummy
export ASAN_OPTIONS=halt_on_error=1:detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1

TRACKS=(CY1 CY2 CY3 CL1 CL2 CL3 AL1 AL2 AL3 TR1 TR2 TR3 TR4 TR6 TR7)
CARS=(0 1 2 3 4 5 6 7 11)        # the eight and the Warrior
TYPES=(2 0 1 3)                  # the full field, time trial, head to head (traffic, cops), tournament

# Esc at `at` ms, then Quit and Yes, then Enter every 4 s until `until`
giveup() {
  local at=$1 until=$2 s="$1:ESC,$(( $1 + 6000 )):UP,$(( $1 + 10000 )):ENTER,$(( $1 + 16000 )):Y"
  for t in $(seq $(( at + 24000 )) 4000 $until); do s+=",$t:ENTER"; done
  print -r -- $s
}

escapes() {
  local s="$1:ESC"
  for t in $(seq $(( $1 + 15000 )) 15000 $2); do s+=",$t:ESC"; done
  print -r -- $s
}

wait_slot() {
  while true; do
    local load=$(sysctl -n vm.loadavg | awk '{print int($3)}')
    (( ${#jobstates} < maxp && load < 7 )) && return
    sleep 5
  done
}

# run NAME TIMEOUT SCRIPT ARGS...: one run in the background
run() {
  local name=$1 limit=$2 script=$3; shift 3
  wait_slot
  (
    local start=$SECONDS
    TNFS_INPUT_SETTLE=${RUN_SETTLE:-0} TNFS_INPUT_SCRIPT=$script timeout -k 5 $limit $bin --data $data \
      ${RUN_MODS:+--mods} ${RUN_MODS:-$mods} "$@" > $out/$name.log 2>&1
    local rc=$? found=$(grep -m1 -E "runtime error|ERROR: AddressSanitizer|SUMMARY: (Address|Undefined)" $out/$name.log)
    local over=""; (( rc == 124 )) && grep -q "race over" $out/$name.log && over=" (after its race over)"
    print -r -- "$name rc $rc$over $(( SECONDS - start ))s ${found:-clean}" >> $out/summary
  ) &
}

# menu NAME SCRIPT: the full game with SCRIPT after the way to Control Central, on a copy of MODS
TO_CENTRAL="0:CLICK:600:30,0:CLICK:310:145,0:CLICK:600:30"   # the intro, Single player, the name box (a click is Enter)
OPTIONS="3000:CLICK:215:450"
OUT_OF_CENTRAL="3000:CLICK:575:450,3000:CLICK:310:366"       # Cancel, then Game Mode's Exit
menu() {
  local copy=$out/mods/$1
  [[ -d $copy ]] && find $copy -delete; mkdir -p $copy; [[ -n $MODS ]] && cp -R $MODS/. $copy/
  RUN_SETTLE=2000 RUN_MODS=$copy run menu_$1 ${3:-300} "$TO_CENTRAL,$2,$OUT_OF_CENTRAL"
}
twice() { print -r -- "0:CLICK:$1,0:CLICK:$1"; }

zmodload zsh/parameter
for phase in $phases; do
  case $phase in
  drive)
    for (( i = 1; i <= ${#TRACKS}; i++ )); do
      for k in 0 3 6; do
        car=${CARS[$(( (i + k) % ${#CARS} + 1 ))]} type=${TYPES[$(( (i + k / 3) % ${#TYPES} + 1 ))]}
        run drive_${TRACKS[i]}_car${car}_type$type 240 "$(giveup 20000 160000)" --drive ${TRACKS[i]} --car $car --type $type
      done
    done ;;
  variant)
    for (( i = 1; i <= ${#TRACKS}; i++ )); do
      for v in m r; do
        shift_by=0; [[ $v == r ]] && shift_by=4
        car=${CARS[$(( (i + shift_by) % ${#CARS} + 1 ))]}
        run variant_${TRACKS[i]}${v}_car$car 240 "$(giveup 20000 160000)" --drive ${TRACKS[i]}$v --car $car
      done
    done ;;
  frame)
    for t in $TRACKS; do run frame_$t 240 "" --drive $t --node 600 --screenshot $out/frame_$t.ppm; done ;;
  menus)
    menu options "$OPTIONS,$(twice 80:115),$(twice 80:149),$(twice 80:183),3000:CLICK:40:450"
    menu audio "$OPTIONS,3000:CLICK:80:216,$(twice 80:112),0:CLICK:80:112,0:CLICK:330:155,0:CLICK:480:155,0:CLICK:400:198,0:CLICK:400:242,$(twice 80:285),$(twice 80:371),3000:CLICK:40:450,3000:CLICK:40:450"
    menu graphics "$OPTIONS,3000:CLICK:80:251,$(twice 80:115),$(twice 80:167),$(twice 80:219),$(twice 80:271),$(twice 80:323),$(twice 80:375),3000:CLICK:40:450,3000:CLICK:40:450"
    menu controls "$OPTIONS,3000:CLICK:80:285,$(twice 80:115),0:CLICK:80:155,0:CLICK:80:195,$(twice 80:235),3000:CLICK:575:450,3000:CLICK:40:450"
    # a left click turns the credits' page, Escape leaves them
    menu credits "$OPTIONS,3000:CLICK:80:356,10000:CLICK:600:30,20000:ESC,3000:CLICK:40:450"
    menu replay_save "$OPTIONS,3000:CLICK:80:321,3000:CLICK:80:240,3000:CLICK:600:30,3000:CLICK:40:450,3000:CLICK:40:450"
    # a name from the list, Yes to "Load replay ?", two minutes of it, then the replay's Done back to Load and Save
    for y in 148 170 190 212; do
      menu replay_view_$y "$OPTIONS,3000:CLICK:80:321,3000:CLICK:80:280,3000:CLICK:430:$y,3000:Y,120000:CLICK:30:465,3000:CLICK:40:450,3000:CLICK:40:450" 400
    done
    menu highlights_view "$OPTIONS,3000:CLICK:80:321,3000:CLICK:80:320,3000:CLICK:430:148,3000:Y,120000:CLICK:30:465,3000:CLICK:40:450,3000:CLICK:40:450" 400
    # the race's own replays are never in the delete list (0042ad94): one saved as REPLAY2S first, deleted, then
    # the list (left open, Cancel lit) closed
    menu replay_delete "$OPTIONS,3000:CLICK:80:321,3000:CLICK:80:240,3000:TEXT:S,3000:CLICK:600:30,3000:CLICK:80:360,3000:CLICK:430:148,3000:Y,3000:CLICK:543:215,3000:CLICK:40:450,3000:CLICK:40:450"
    menu tournament "$OPTIONS,3000:CLICK:80:321,3000:CLICK:80:120,3000:CLICK:600:30,3000:CLICK:80:160,3000:CLICK:430:148,3000:CLICK:80:200,3000:CLICK:430:148,3000:Y,3000:CLICK:40:450,3000:CLICK:40:450"
    pages="3000:CLICK:420:140,0:CLICK:585:217,0:CLICK:585:217,0:CLICK:45:217,3000:CLICK:40:450,3000:CLICK:215:230,3000:CLICK:40:450,3000:CLICK:530:310,3000:CLICK:40:450,3000:CLICK:210:370,3000:CLICK:40:450,3000:CLICK:375:260,3000:CLICK:600:30"
    for first in 0 3 6; do
      next=""; for (( k = 0; k < first; k++ )); do next+=",3000:CLICK:310:450"; done
      menu showcase_$first "3000:CLICK:180:200$next,$pages,3000:CLICK:310:450,$pages,3000:CLICK:310:450,$pages,3000:CLICK:40:450"
    done ;;
  long)
    k=0
    for t in TR1 TR2 TR4 TR6 TR7 CY1 CL1 AL1; do
      k=$(( k + 1 )); TNFS_AUTOPILOT=1 run long_$t 1800 "" --drive $t --car ${CARS[k]} --type 2
    done ;;
  movie)
    for f in $data/FRONTEND/MOVIE/*.TGV(N); do run movie_${f:t:r} 200 "" --movie $f; done ;;
  replay)
    for f in $data/GAMEDATA/REPLAY/*.RPL(N) ${MODS:+$MODS/user/Replay.rpl}; do
      [[ -f $f ]] && run replay_${f:t:r} 300 "$(escapes 60000 280000)" --replay $f
    done ;;
  esac
done
wait
find $out -name '*.ppm' -delete
sort $out/summary
