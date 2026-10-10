#!/bin/sh
# BZFlag dependency check for Linux (port of diag-dependency-check.cmd)
# Verifies every shared library the bzflag binary needs is resolvable.

DIR="$(cd "$(dirname "$0")" && pwd)"
RES="$DIR/RESULTS.txt"

echo "=== BZFlag shared-library dependency check ==="
echo "===== DEPENDENCY CHECK $(date) =====" >> "$RES"

bzflag_bin=""
for cand in "$DIR/bzflag" /usr/local/bin/bzflag /usr/games/bzflag /usr/bin/bzflag; do
    [ -x "$cand" ] && bzflag_bin="$cand" && break
done

if [ -z "$bzflag_bin" ]; then
    echo "ERROR: bzflag binary not found next to this script or in standard prefixes" >> "$RES"
    echo "not found"; exit 1
fi

echo "ldd of $bzflag_bin" >> "$RES"
ldd "$bzflag_bin" >> "$RES" 2>&1

echo
echo "--- verdicts ---" >> "$RES"
MISSING=0
liblist=$(ldd "$bzflag_bin" 2>/dev/null)
echo "$liblist" | grep "not found" >> "$RES" && MISSING=1

if [ "$MISSING" = "0" ]; then
    echo "[OK] all shared libraries resolve" >> "$RES"
    echo "All shared libraries resolve - load-time is fine; a failure is at runtime."
else
    echo "[MISSING] see 'not found' lines above - install the packages providing them."
fi

DEPS=""
for pkg in libSDL2 libGLEW libGL curl; do
    DEPS="$DEPS $pkg"
done
echo "(hint: typical providers - libsdl2-2.0-0 libglew3 libgl1 libcurl4)" >> "$RES"
echo "Full verdicts in RESULTS.txt."