#!/bin/sh
# BZFlag environment report - macOS port of run-envreport.cmd
# Appends machine/OS/GPU report + app-bundle sanity into RESULTS.txt.

DIR="$(cd "$(dirname "$0")" && pwd)"
RES="$DIR/RESULTS.txt"

echo "===== ENV REPORT $(date) user=$(whoami) =====" >> "$RES"

echo "[macOS version + hardware]" >> "$RES"
sw_vers >> "$RES" 2>&1
sysctl -n hw.model >> "$RES" 2>&1
sysctl -n machdep.cpu.brand_string >> "$RES" 2>&1
echo "arch=$(uname -m)" >> "$RES"
echo "RAM: $(sysctl -n hw.memsize) bytes" >> "$RES"

echo "[GPU and driver]" >> "$RES"
system_profiler SPDisplaysDataType 2>/dev/null | grep -E "Chipset|Chipset Model|Vendor|Metal|Total Number of Cores|VRAM" >> "$RES" 2>&1

echo "[BZFlag.app probe]" >> "$RES"
APP="/Applications/BZFlag.app"
[ -d "$APP" ] || APP="$DIR/BZFlag.app"
if [ -d "$APP" ]; then
    echo "app found: $APP" >> "$RES"
    BIN="$APP/Contents/MacOS/bzflag"
    if [ -x "$BIN" ]; then
        echo "binary: $BIN ($(ls -la "$BIN" | awk '{print $5}') bytes)" >> "$RES"
        echo "-- load commands of the binary (dyld paths) --" >> "$RES"
        otool -L "$BIN" >> "$RES" 2>&1
        echo "-- any runner paths (homebrew/runner build dirs) left in load commands are install blockers --" >> "$RES"
        otool -L "$BIN" 2>/dev/null | grep -E "/opt/homebrew|/usr/local|/Users/" | tee -a "$RES" 2>/dev/null
    else
        echo "NO EXECUTABLE at $BIN - broken bundle" >> "$RES"
    fi
    RESDIR="$APP/Contents/Resources"
    echo "-- Resources data/fonts present? --" >> "$RES"
    ls "$RESDIR/bzflag/data/fonts" 2>/dev/null | head -3 >> "$RES" || echo "NO FONTS at $RESDIR/bzflag/data/fonts" >> "$RES"
    echo "-- Frameworks (bundled dylibs) present? --" >> "$RES"
    ls "$APP/Contents/Frameworks" 2>/dev/null >> "$RES" || echo "NO Frameworks dir" >> "$RES"
else
    echo "BZFlag.app NOT FOUND (looked in /Applications and script dir)" >> "$RES"
fi

echo "[Existing BZFlag user data]" >> "$RES"
ls -la "$HOME/.bzf/2.4" >> "$RES" 2>&1 || echo "$HOME/.bzf/2.4 does not exist" >> "$RES"

echo "===== END ENV REPORT =====" >> "$RES"
echo "Wrote the report to RESULTS.txt. Send that file back."
echo "Note: if the game was blocked by macOS Local Network privacy (LAN joins fail with 'No route to host'), collect:" >> "$RES" 2>/dev/null
echo "tccutil reset LocalNetwork" >> "$RES" 2>/dev/null || true