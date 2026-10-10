#!/bin/sh
# BZFlag OpenGL capability probe for Linux (port of diag-glinfo.cmd)

DIR="$(cd "$(dirname "$0")" && pwd)"
RES="$DIR/RESULTS.txt"
echo "=== BZFlag OpenGL probe ==="
echo "===== GL PROBE $(date) =====" >> "$RES"

if command -v glxinfo >/dev/null 2>&1; then
    glxinfo >> "$RES" 2>&1
    echo "glxinfo output appended to RESULTS.txt."
    # the lines that matter, echoed to the console too
    glxinfo 2>/dev/null | grep -E "OpenGL (vendor|renderer|version)|direct rendering" | head -6
else
    echo "glxinfo not installed - install it with:" | tee -a "$RES"
    echo "  Debian/Ubuntu: sudo apt install mesa-utils" | tee -a "$RES"
    echo "  Fedora:        sudo dnf install glx-utils" | tee -a "$RES"
    echo "  Arch:          sudo pacman -S mesa-utils" | tee -a "$RES"
    echo
    echo "Falling back to PCI GPU probe (shows hardware, not GL version):" | tee -a "$RES"
    lspci 2>/dev/null | grep -iE "vga|3d|display" | tee -a "$RES" || echo "(lspci unavailable)" | tee -a "$RES"
fi