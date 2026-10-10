#!/bin/sh
# BZFlag crash collector for macOS (port of collect-eventlog.cmd).
# macOS has no event log; the equivalents are:
#   - CrashReporter .ips/.panic files in ~/Library/Logs/DiagnosticReports
#   - the system log (log show) entries mentioning bzflag
# Collects both into eventlog-bzflag.txt next to this script.

DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$DIR/eventlog-bzflag.txt"

echo "=== BZFlag crash collector (macOS) ==="

{
    echo "===== BZFLAG CRASH REPORT last 30 days $(date) ====="

    echo "-- CrashReporter reports for bzflag --"
    found=0
    for CRDIR in "$HOME/Library/Logs/DiagnosticReports" "/Library/Logs/DiagnosticReports"; do
        if [ -d "$CRDIR" ]; then
            for f in "$CRDIR"/bzflag*.ips "$CRDIR"/bzflag*.crash; do
                if [ -f "$f" ]; then
                    found=1
                    echo "=== FILE: $f ==="
                    # .ips files: first line JSON header, rest a JSON body with
                    # exception + termination + faulting module
                    head -40 "$f"
                    echo
                fi
            done
        fi
    done
    [ "$found" = "0" ] && echo "(no crash reports for bzflag found)"

    echo "-- system log entries mentioning bzflag (last 7 days, last 100) --"
    log show --last 7d --predicate 'process CONTAINS "bzflag" OR eventMessage CONTAINS "bzflag"' 2>/dev/null | tail -100 || echo "(log show unavailable or permission denied)"
    echo "===== END ====="
} > "$OUT" 2>&1

echo "Wrote $OUT"
echo "If the file reports 'permission denied' for the system log, run this"
echo "once from Terminal.app and press the Allow button:"
echo "  sudo log show --last 7d --predicate 'process CONTAINS \"bzflag\"' > $OUT"