#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# How many parts of tests said SKIPPED-PART in a build's last ctest run: the number alone, for a public log (the test log itself carries what the tests read from the CD).
# usage: ci_skipped_parts.sh BUILD_DIR
set -u
log=${1:?usage: ci_skipped_parts.sh BUILD_DIR}/Testing/Temporary/LastTest.log
[ -f "$log" ] || { echo "skipped parts: no test log"; exit 1; }
echo "skipped parts: $(grep -c SKIPPED-PART "$log")"
