#!/bin/sh
# ============================================================================
# BZFlag Linux diagnostics - ALL-IN-ONE
# ============================================================================
# One script, zero arguments: finds the game, probes the machine, launches
# the game in a captured windowed run, collects crash logs, writes ONE
# results file to send back.
#
# Output (in the folder you run this from):
#   RESULTS-diag-linux.txt   <- send this file back (everything is in it)
#
# Portable: installs nothing, no root needed (worst case some sections say
# "permission denied" - that is fine, send the file anyway).

DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$DIR/RESULTS-diag-linux.txt"
LOGTAG="bzflag"

: > "$OUT"

say() { printf '%s\n' "$*" ; printf '%s\n' "$*" >> "$OUT"; }
run_capture() { # run_capture <label> <cmd...> - console + file; label must
                # not go through sh -c: echo it, only the command executes
    say "-- $1"
    shift
    "$@" >> "$OUT" 2>&1
}

say "================ BZFlag Linux diagnostics $(date) ================"
say "user=$(whoami)  cwd=$DIR"

# ----------------------------------------------------------------------------
say ""
say "SECTION 1: OS + hardware"
# ----------------------------------------------------------------------------
if [ -r /etc/os-release ]; then
    grep -E "^(PRETTY_NAME|NAME|VERSION_ID)=" /etc/os-release >> "$OUT" 2>&1
else
    say "(no /etc/os-release)"
fi
say "kernel: $(uname -a)"
say "userland arch: $(uname -m)"
if command -v lsb_release >/dev/null 2>&1; then
    say "desktop session: XDG_SESSION_TYPE=$XDG_SESSION_TYPE WAYLAND_DISPLAY=$WAYLAND_DISPLAY DISPLAY=$DISPLAY"
else
    say "desktop session: XDG_SESSION_TYPE=$XDG_SESSION_TYPE WAYLAND_DISPLAY=$WAYLAND_DISPLAY DISPLAY=$DISPLAY"
fi
grep -m1 MemTotal /proc/meminfo >> "$OUT" 2>&1

# ----------------------------------------------------------------------------
say ""
say "SECTION 2: GPU + OpenGL"
# ----------------------------------------------------------------------------
run_capture "PCI GPU hardware" lspci
if command -v glxinfo >/dev/null 2>&1; then
    glxinfo 2>/dev/null | grep -E "OpenGL (vendor|renderer|version)|direct rendering|core profile" >> "$OUT"
    echo "full glxinfo follows:" >> "$OUT"
    glxinfo >> "$OUT" 2>&1
else
    say "glxinfo NOT installed - install it for the exact GL version:"
    say "  Debian/Ubuntu: sudo apt install mesa-utils | Fedora: sudo dnf install glx-utils | Arch: pacman -S mesa-utils"
fi

# ----------------------------------------------------------------------------
say ""
say "SECTION 3: locate the game"
# ----------------------------------------------------------------------------
bzflag_bin=""
for cand in "$DIR/bzflag" /usr/local/bin/bzflag /usr/games/bzflag /usr/bin/bzflag \
            /opt/bzflag/bin/bzflag; do
    [ -x "$cand" ] && bzflag_bin="$cand" && break
done
bzfs_bin=""
for cand in "$DIR/bzfs" /usr/local/bin/bzfs /usr/games/bzfs /usr/bin/bzfs; do
    [ -x "$cand" ] && bzfs_bin="$cand" && break
done

if [ -n "$bzflag_bin" ]; then
    say "client binary: $bzflag_bin"
    say "-- client version --"
    "$bzflag_bin" -version >> "$OUT" 2>&1
else
    say "client binary NOT FOUND (looked in script dir + standard prefixes) - is the game installed?"
fi
[ -n "$bzfs_bin" ] && say "server binary: $bzfs_bin"

pkgver="$(dpkg -s bzflag 2>/dev/null | grep -E '^(Version|Status)' || rpm -q bzflag 2>/dev/null)"
[ -n "$pkgver" ] && say "package: $pkgver"

# ----------------------------------------------------------------------------
say ""
say "SECTION 4: shared-library dependency check"
# ----------------------------------------------------------------------------
if [ -n "$bzflag_bin" ] && command -v ldd >/dev/null 2>&1; then
    echo "-- ldd $bzflag_bin --" >> "$OUT"
    ldd "$bzflag_bin" >> "$OUT" 2>&1
    MISSING=$(ldd "$bzflag_bin" 2>/dev/null | grep -c "not found")
    if [ "$MISSING" = "0" ]; then
        say "[OK] all shared libraries resolve (load-time fine; failure would be runtime)"
    else
        say "[MISSING] $MISSING libraries unresolved - lines above marked 'not found' = install blockers"
    fi
else
    say "(ldd unavailable or binary not found)"
fi

# ----------------------------------------------------------------------------
say ""
say "SECTION 5: crash + system log entries (last 30 days)"
# ----------------------------------------------------------------------------
if command -v journalctl >/dev/null 2>&1; then
    journalctl -k --since "30 days ago" 2>/dev/null | grep -iE "bzflag|segfault|out of memory|killed process" >> "$OUT" || say "(no kernel entries)"
    journalctl --since "30 days ago" 2>/dev/null | grep -i "$LOGTAG" | grep -v grep >> "$OUT" || say "(no journal entries)"
else
    say "(journalctl unavailable)"
fi
if command -v coredumpctl >/dev/null 2>&1; then
    say "-- coredumps --"
    coredumpctl list 2>/dev/null | grep -i bzflag >> "$OUT" || say "(no bzflag coredumps)"
else
    say "(coredumpctl unavailable)"
fi

# ----------------------------------------------------------------------------
say ""
say "SECTION 6: BZFlag user config"
# ----------------------------------------------------------------------------
if [ -d "$HOME/.bzf/2.4" ]; then
    ls -la "$HOME/.bzf/2.4" >> "$OUT" 2>&1
    say "IMPORTANT: settings in ~/.bzf/2.4/config.cfg override defaults silently."
    grep -E "^set (quality|useQuality|shadows|meshVBO|bgVBO|worldShader|aniso|multisample|texture|resolution|fullscreen|saveEnergy)" "$HOME/.bzf/2.4/config.cfg" 2>/dev/null >> "$OUT" || say "(no render-relevant lines or no config)"
else
    say "(no $HOME/.bzf/2.4)"
fi

# ----------------------------------------------------------------------------
say ""
say "SECTION 7: live run (captured)"
# ----------------------------------------------------------------------------
if [ -n "$bzflag_bin" ]; then
    say "Launching the game windowed with debug logging - press Enter to start."
    say "The game opens on top; play a few seconds in the scene where the problem happens, then quit the game (Escape)."
    read _dummy
    echo "==== LIVE RUN $(date) ====" >> "$OUT"
    "$bzflag_bin" -window 1280x800 -debug >> "$OUT" 2>&1
    EC=$?
    say "[game exit code $EC]"
    if [ "$EC" = "139" ]; then say ">>> SIGSEGV crash - the section 5 entries name the faulting library"
    elif [ "$EC" = "134" ]; then say ">>> SIGABRT - check the log above for assertion text"
    fi
else
    say "(skipped - no binary found)"
fi

# ----------------------------------------------------------------------------
say ""
say "================ END of diagnostics ================"
say "SEND BACK THIS FILE: $OUT"
printf "\nDiagnostics complete. File: %s\n" "$OUT"