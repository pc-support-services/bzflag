#!/bin/sh
# BZFlag crash/launch-failure collector for Linux (port of collect-eventlog.cmd)
# Pulls all kernel/systemd + journal entries mentioning bzflag from the last
# 30 days into eventlog-bzflag.txt next to this script.

DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$DIR/eventlog-bzflag.txt"

echo "=== BZFlag system-log collector (journalctl) ==="

if ! command -v journalctl >/dev/null 2>&1; then
    echo "journalctl not available on this system." | tee "$OUT"
    echo "Fallback: send the contents of /var/log/syslog or /var/log/messages"
    exit 1
fi

{
    echo "===== BZFLAG JOURNAL ENTRIES last 30 days $(date) ====="
    echo "-- oom / segfault hints from the kernel --"
    journalctl -k --since "30 days ago" 2>/dev/null | grep -iE "bzflag|segfault|out of memory|killed process" || echo "(none)"
    echo "-- systemd user/system units mentioning bzflag --"
    journalctl --since "30 days ago" 2>/dev/null | grep -iE "bzflag" | grep -ivE "grep" || echo "(none)"
    echo "-- coredumpctl (if systemd-coredump installed) --"
    (command -v coredumpctl >/dev/null 2>&1 && coredumpctl list bzflag 2>/dev/null | tail -20) || echo "(coredumpctl not available)"
    echo "===== END ====="
} > "$OUT" 2>&1

echo "Wrote $OUT"
echo "If a coredump exists, also run:"
echo "  coredumpctl info bzflag >> $OUT"
echo "(needs the matching debug packages for full backtraces)"