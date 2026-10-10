#!/bin/sh
# BZFlag environment report - Linux port of run-envreport.cmd
# Appends a full machine/OS/GPU report into RESULTS.txt next to this script.

DIR="$(cd "$(dirname "$0")" && pwd)"
RES="$DIR/RESULTS.txt"

echo "===== ENV REPORT $(date) user=$(whoami) =====" >> "$RES"

echo "[distro + kernel]" >> "$RES"
(cat /etc/os-release 2>/dev/null || cat /etc/lsb-release 2>/dev/null || echo "no os-release file") >> "$RES" 2>&1
uname -a >> "$RES" 2>&1

echo "[desktop session type]" >> "$RES"
echo "XDG_SESSION_TYPE=$XDG_SESSION_TYPE WAYLAND_DISPLAY=$WAYLAND_DISPLAY DISPLAY=$DISPLAY" >> "$RES"

echo "[CPU]" >> "$RES"
grep -m2 "model name\|model[[:space:]]*:" /proc/cpuinfo >> "$RES" 2>&1
grep -m1 "architecture\|isa" /proc/cpuinfo >> "$RES" 2>&1 || true
uname -m >> "$RES"

echo "[RAM]" >> "$RES"
grep -m1 MemTotal /proc/meminfo >> "$RES" 2>&1

echo "[GPU and driver]" >> "$RES"
lspci 2>/dev/null | grep -iE "vga|3d|display" >> "$RES" 2>&1 || echo "(lspci not available)" >> "$RES"
(command -v glxinfo >/dev/null 2>&1 && glxinfo 2>/dev/null | grep -E "OpenGL (renderer|version|core profile version|vendor)") >> "$RES" 2>&1 || echo "(glxinfo not available - install mesa-utils for the exact GL version)" >> "$RES"

echo "[Existing BZFlag user data]" >> "$RES"
if [ -d "$HOME/.bzf/2.4" ]; then
    ls -la "$HOME/.bzf/2.4" >> "$RES" 2>&1
else
    echo "$HOME/.bzf/2.4 does not exist" >> "$RES"
fi

echo "[installed package]" >> "$RES"
(dpkg -s bzflag 2>/dev/null | grep -E "^(Package|Version|Status)") >> "$RES" 2>&1 || (rpm -q bzflag 2>/dev/null) >> "$RES" 2>&1 || echo "(bzflag not found via dpkg or rpm)" >> "$RES"

echo "===== END ENV REPORT =====" >> "$RES"
echo "Wrote the report to RESULTS.txt. Send that file back."