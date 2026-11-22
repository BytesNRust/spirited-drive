#!/bin/sh
# tools/sky_glow --heading: a node's heading is the world azimuth 90 - 360 * turns on every node of two tracks (the calibration the glow's azimuth stands on).  Exit 77 without the CD's data.
bin=$1
original=$2
[ -d "$original" ] || { echo "skipped: no $original"; exit 77; }
status=0
for t in TR1 AL1; do
  line=$("$bin" --heading "$original" $t | grep 'sign -1 offset 90.0')
  n=$(echo "$line" | sed 's/.*: \([0-9]*\) of \([0-9]*\) nodes.*/\1/'); m=$(echo "$line" | sed 's/.*: \([0-9]*\) of \([0-9]*\) nodes.*/\2/')
  [ -n "$line" ] && [ "$n" = "$m" ] && [ "$m" -gt 50 ] || { echo "FAIL: $t: $line"; status=1; }
done
[ $status -eq 0 ] && echo "sky_glow_heading: ok"
exit $status
