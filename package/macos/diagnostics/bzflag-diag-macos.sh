#!/bin/sh
# ============================================================================
# BZFlag macOS diagnostics - ALL-IN-ONE
# ============================================================================
# One script, zero arguments: sanity-checks the app bundle, probes the
# machine, launches the game in a captured run, collects crash reports,
# writes ONE results file to send back.
#
# Output (in the folder you run this from):
#   RESULTS-diag-macos.txt   <- send this file back
#
# Portable: installs nothing. The system-log section may print "permission
# denied" - run the script once from Terminal.app and press Allow if so.

DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$DIR/RESULTS-diag-macos.txt"

: > "$OUT"

say() { printf '%s\n' "$*" ; printf '%s\n' "$*" >> "$OUT"; }

say "================ BZFlag macOS diagnostics $(date) ================"
say "user=$(whoami)  cwd=$DIR"

# ----------------------------------------------------------------------------
say ""
say "SECTION 1: OS + hardware"
# ----------------------------------------------------------------------------
run_capture() { say "-- $1"; }
sw_vers >> "$OUT" 2>&1
say "model: $(sysctl -n hw.model 2>/dev/null)"
say "cpu: $(sysctl -n machdep.cpu.brand_string 2>/dev/null)"
say "arch: $(uname -m)"
say "ram: $(sysctl -n hw.memsize 2>/dev/null) bytes"
say "-- GPU/Metal --"
system_profiler SPDisplaysDataType 2>/dev/null | grep -E "Chipset|Vendor|Metal|Total Number of Cores|VRAM" >> "$OUT" 2>&1

# ----------------------------------------------------------------------------
say ""
say "SECTION 2: locate the app + BUNDLE SANITY (the usual install blockers)"
# ----------------------------------------------------------------------------
APP=""
for cand in "$DIR/BZFlag.app" /Applications/BZFlag.app "$HOME/Applications/BZFlag.app"; do
    [ -d "$cand" ] && APP="$cand" && break
done

if [ -z "$APP" ]; then
    say "BZFlag.app NOT FOUND (looked next to this script, /Applications, ~/Applications)"
else
    say "app found: $APP"
    BIN="$APP/Contents/MacOS/bzflag"
    if [ -x "$BIN" ]; then
        say "binary: $BIN ($(stat -f %z "$BIN") bytes)"
        say "-- load commands (otool -L) --"
        otool -L "$BIN" >> "$OUT" 2>&1
        RUNNERS=$(otool -L "$BIN" 2>/dev/null | grep -cE "/opt/homebrew|/usr/local/|/Users/")
        if [ "$RUNNERS" != "0" ]; then
            say "[BLOCKER] $RUNNERS load commands point at build-machine paths (/opt/homebrew, /usr/local, /Users) - the app cannot launch elsewhere"
        else
            say "[OK] all load commands in-bundle"
        fi
        say "-- Frameworks bundled --"
        ls "$APP/Contents/Frameworks" >> "$OUT" 2>&1 || say "NO Frameworks dir"
        say "libSDL3 present:" $(ls "$APP/Contents/Frameworks" 2>/dev/null | grep -c "libSDL3")
        say "-- Resources data/fonts --"
        ls "$APP/Contents/Resources/bzflag/data/fonts" 2>/dev/null | head -3 >> "$OUT" || say "NO fonts at Resources/bzflag/data/fonts (the 'No fonts found' class)"
    else
        say "NO EXECUTABLE at $BIN - broken bundle"
    fi
fi

# ----------------------------------------------------------------------------
say ""
say "SECTION 3: BZFlag user config"
# ----------------------------------------------------------------------------
if [ -d "$HOME/.bzf/2.4" ]; then
    ls -la "$HOME/.bzf/2.4" >> "$OUT" 2>&1
    grep -E "^set (quality|shadows|meshVBO|bgVBO|worldShader|aniso|texture|resolution|fullscreen|saveEnergy)" "$HOME/.bzf/2.4/config.cfg" 2>/dev/null >> "$OUT" || say "(no render-relevant config lines)"
else
    say "(no $HOME/.bzf/2.4)"
fi

# ----------------------------------------------------------------------------
say ""
say "SECTION 4: crash reports (last 30 days)"
# ----------------------------------------------------------------------------
found=0
for CRDIR in "$HOME/Library/Logs/DiagnosticReports" "/Library/Logs/DiagnosticReports"; do
    for f in "$CRDIR"/bzflag*.ips "$CRDIR"/bzflag*.crash; do
        if [ -f "$f" ]; then
            found=1
            say "-- $(basename "$f") ($(stat -f %Sm -t '%Y-%m-%d' "$f")) --"
            head -40 "$f" >> "$OUT" 2>&1
        fi
    done
done
[ "$found" = "0" ] && say "(no bzflag crash reports found)"

say ""
say "-- system log mentions (last 7 days; needs permission - see below if empty) --"
log show --last 7d --predicate 'process CONTAINS "bzflag" OR eventMessage CONTAINS "bzflag"' 2>/dev/null | tail -100 >> "$OUT" || say "(log show unavailable)"
if ! grep -q "bzflag" "$OUT" < /dev/null; then
    say "(if the log section is empty: run 'sudo log show --last 7d | grep -i bzflag > log-bzflag.txt' in Terminal and send that too)"
fi

# ----------------------------------------------------------------------------
say ""
say "SECTION 5: live run (captured)"
# ----------------------------------------------------------------------------
if [ -n "$BIN" ] && [ -x "$BIN" ]; then
    say "Launching the game windowed with debug logging - press Enter to start."
    say "Play in the scene where the problem happens, then quit the game to end the capture."
    read _dummy
    echo "==== LIVE RUN $(date) ====" >> "$OUT"
    "$BIN" -window -debug >> "$OUT" 2>&1
    EC=$?
    say "[game exit code $EC]"
    [ "$EC" != "0" ] && say ">>> nonzero exit: check the dyld lines above; collect 'otool -L' output if a Library-not-loaded error appears"
else
    say "(skipped - no binary found)"
fi

# ----------------------------------------------------------------------------
say ""
say "SECTION 6: LAN-join privacy note (macOS Local Network)"
# ----------------------------------------------------------------------------
say "If LAN server joins fail with 'No route to host' while internet servers work:"
say "  = macOS Local Network privacy denial (system, not the game)."
say "  Fix: sudo tccutil reset LocalNetwork, relaunch, press Allow."

say ""
say "================ END of diagnostics ================"
say "SEND BACK THIS FILE: $OUT"
printf "\nDiagnostics complete. File: %s\n" "$OUT"