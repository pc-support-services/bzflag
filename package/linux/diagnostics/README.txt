BZFlag Linux diagnostics pack
=============================

Purpose
-------
Turns a "BZFlag won't start / crashes / black screen" report into real
diagnostics on any Linux machine. Portable folder: installs nothing.

WHAT'S INSIDE
-------------
run-diag-linux.sh          - launches bzflag -window -debug, captures all
                             console output to diag-log.txt, prints the
                             exit code (139 = SIGSEGV crash)
run-envreport-linux.sh     - distro, kernel, session type (X11/Wayland),
                             CPU, RAM, GPU + driver + GL version, existing
                             BZFlag user data, installed package version
diag-glinfo-linux.sh       - OpenGL capability probe (glxinfo; tells you
                             which package to install when it's missing)
diag-dependency-linux.sh   - ldd-based shared-library check with per-lib
                             verdicts ("not found" = install blocker)
collect-eventlog-linux.sh  - journalctl crash/kernel entries for bzflag
                             (last 30 days) + coredumpctl list

TO RUN (order matters)
----------------------
1. ./run-diag-linux.sh     (chmod +x first if needed)
2. ./run-envreport-linux.sh
3. ./diag-glinfo-linux.sh
4. ./diag-dependency-linux.sh
5. ./collect-eventlog-linux.sh
6. Send back: diag-log.txt, RESULTS.txt, eventlog-bzflag.txt

NOTES
-----
- Make the scripts executable first: chmod +x *.sh
- On Wayland sessions bzflag runs under XWayland; if the window never
  appears, try the X11 session or note your session type from step 2.
- Everything writes into the folder you ran from; safe to re-run.