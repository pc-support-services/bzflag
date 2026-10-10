#!/bin/sh
# BZFlag diagnostic launcher (windowed) - Linux port of run-diag.cmd
# Captures everything into diag-log.txt in the folder this script lives in.
#
# Usage: ./run-diag-linux.sh   (put this folder next to the bzflag binary
#        or anywhere - it auto-finds the binary; see find_bzflag below)

DIR="$(cd "$(dirname "$0")" && pwd)"
LOG="$DIR/diag-log.txt"
RES="$DIR/RESULTS.txt"

echo "=== BZFlag diagnostic run (windowed) ==="
echo "A console stays open and captures everything into diag-log.txt."
echo "Press Enter now to start the game."
read _dummy

: >> "$LOG"
echo "==== WINDOWED RUN $(date) user=$(whoami) ====" >> "$LOG"

bzflag_bin=""
for cand in "$DIR/bzflag" "$DIR/../games/bzflag" "$DIR/usr/games/bzflag" \
            /usr/local/bin/bzflag /usr/games/bzflag /usr/bin/bzflag; do
    if [ -x "$cand" ]; then
        bzflag_bin="$cand"
        break
    fi
done

echo "[0] binary probe:" >> "$LOG"
if [ -z "$bzflag_bin" ]; then
    echo "ERROR: bzflag binary not found (looked in $DIR and standard install prefixes)" | tee -a "$LOG"
    echo "[0] binary NOT FOUND - install the package or put this folder next to the binary" >> "$RES" 2>/dev/null || echo "[0] binary NOT FOUND" >> "$LOG"
    echo "Press Enter to close."
    read _dummy
    exit 1
fi
echo "found: $bzflag_bin" >> "$LOG"
"$bzflag_bin" -version >> "$LOG" 2>&1
echo "[version exit code $?]" >> "$LOG"

echo "[1] launching GAME -window -windowed-size 1280x800 -debug (windowed, isolated debug log)" >> "$LOG"
"$bzflag_bin" -window -windowed-size 1280x800 -debug >> "$LOG" 2>&1
EC=$?
echo "[2] game exit code $EC" >> "$LOG"
echo "[game exit code $EC]" >> "$RES" 2>/dev/null || echo "[game exit code $EC]" >> "$LOG"
if [ "$EC" = "139" ] || [ "$EC" = "134" ]; then
    echo "game died with SIGSEGV (139)/SIGABRT (134) = native crash. Check the kernel log: dmesg | grep -i bzflag, and journalctl -k -g bzflag" >> "$RES" 2>/dev/null || true
    echo ">>> native crash (segfault). Collect: dmesg | grep -i bzflag > crash-dmesg.txt" >> "$LOG"
fi
echo
echo "Game exited. Results are in diag-log.txt and RESULTS.txt in this folder."
echo "Press Enter to close."
read _dummy