#!/bin/sh
# BZFlag diagnostic launcher - macOS port of run-diag.cmd
# Launches the BZFlag.app binary in a terminal-visible run, captures the
# output into diag-log.txt next to this script.
#
# The .app's binary is at BZFlag.app/Contents/MacOS/bzflag; this script
# auto-finds it in /Applications, or run the script from inside the app
# bundle's Resources for a self-contained diag folder.

DIR="$(cd "$(dirname "$0")" && pwd)"
LOG="$DIR/diag-log.txt"
RES="$DIR/RESULTS.txt"

echo "=== BZFlag diagnostic run (windowed) ==="
echo "Everything BZFlag prints lands in diag-log.txt. Close the game to end the capture."
echo "Press Enter now to start the game."
read _dummy

: >> "$LOG"

bzflag_bin=""
for cand in "$DIR/BZFlag.app/Contents/MacOS/bzflag" \
            /Applications/BZFlag.app/Contents/MacOS/bzflag \
            ~/Applications/BZFlag.app/Contents/MacOS/bzflag; do
    if [ -x "$cand" ]; then
        bzflag_bin="$cand"
        break
    fi
done

echo "[0] binary probe:" >> "$LOG"
if [ -z "$bzflag_bin" ]; then
    echo "ERROR: BZFlag.app not found (looked next to this script and in /Applications)." | tee -a "$LOG"
    echo "[0] app NOT FOUND" >> "$RES" 2>/dev/null || echo "[0] app NOT FOUND" >> "$LOG"
    exit 1
fi
echo "found: $bzflag_bin" >> "$LOG"
"$bzflag_bin" -version >> "$LOG" 2>&1
echo "[version exit code $?]" >> "$LOG"

# data-path sanity (fonts are the usual silent failure on a clean install)
APP_RES="$(dirname "$bzflag_bin")/../Resources"
echo "[0a] bundle Resources present?" >> "$LOG"
ls "$APP_RES/bzflag/data/fonts" 2>/dev/null | head -3 >> "$LOG" || echo "NO FONTS DIR at $APP_RES/bzflag/data/fonts - the 'No fonts found' class" >> "$LOG"

echo "[1] launching game (windowed, debug)" >> "$LOG"
"$bzflag_bin" -window -debug >> "$LOG" 2>&1
EC=$?
echo "[2] game exit code $EC" >> "$LOG"
echo "[game exit code $EC]" >> "$RES" 2>/dev/null || echo "[game exit code $EC]" >> "$LOG"
if [ "$EC" != "0" ]; then
    echo ">>> if this was a dyld 'Library not loaded' error, collect also:" >> "$LOG"
    echo "    otool -L '$bzflag_bin'" >> "$LOG"
fi
echo
echo "Game exited. Results in diag-log.txt and RESULTS.txt in this folder."