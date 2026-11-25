#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# check_tag_version.sh TAG [CMakeLists.txt]: a release tag vMAJOR.MINOR.PATCH[-pre] must name the version the source says, project(tnfs VERSION MAJOR.MINOR.PATCH)
# in CMakeLists.txt (the build refuses a tag that differs, but only after the long build: this says it first, with what to do). Exit 0 when they agree, 1 when they
# differ or the tag is not a version, 2 for bad arguments.
set -eu
[ $# -ge 1 ] && [ $# -le 2 ] || { echo "usage: $0 TAG [CMakeLists.txt]" >&2; exit 2; }
tag=$1
cmake=${2:-CMakeLists.txt}
case $tag in
    v[0-9]*.[0-9]*.[0-9]*) ;;
    *) echo "::error::tag $tag is not vMAJOR.MINOR.PATCH[-pre]" >&2; exit 1 ;;
esac
want=${tag#v}
core=${want%%[-+]*}
base=$(sed -n 's/^project(tnfs VERSION \([0-9][0-9.]*\).*/\1/p' "$cmake" | head -n 1)
[ -n "$base" ] || { echo "::error::no project(tnfs VERSION ...) line in $cmake" >&2; exit 1; }
if [ "$core" != "$base" ]; then
    echo "::error::the tag $tag names version $core but $cmake says project(tnfs VERSION $base). The version bump must be inside the tagged commit: bump it in main, delete the draft release and this tag (git push origin :refs/tags/$tag; git tag -d $tag), and tag the new commit (notes/runbook-release.md, 'Volver a etiquetar')." >&2
    exit 1
fi
echo "tag $tag matches project VERSION $base"
