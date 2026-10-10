BZFlag macOS diagnostics pack
=============================

Purpose
-------
Turns a "BZFlag.app won't start / crashes / fonts missing / LAN join fails"
report into real diagnostics on any Mac. Portable folder: installs nothing.

WHAT'S INSIDE
-------------
run-diag-macos.sh          - launches the app binary (windowed, debug),
                             captures console output to diag-log.txt, prints
                             the exit code, and checks the bundle Resources
                             (fonts = the classic silent failure)
run-envreport-macos.sh     - macOS version, hardware, GPU/Metal info plus a
                             BUNDLE SANITY CHECK: otool -L of the app binary
                             (leftover /opt/homebrew runner paths in load
                             commands = install blocker), Frameworks dir
                             contents (libSDL3.dylib must be there), fonts
                             present
collect-eventlog-macos.sh  - crash reports (.ips) for bzflag from
                             DiagnosticReports + system log entries
                             (last 7 days)

TO RUN (order matters)
----------------------
1. ./run-diag-macos.sh     (chmod +x first if needed)
2. ./run-envreport-macos.sh
3. ./collect-eventlog-macos.sh
4. Send back: diag-log.txt, RESULTS.txt, eventlog-bzflag.txt

NOTES
-----
- Make the scripts executable first: chmod +x *.sh
- The envreport's otool -L section is the single most valuable artifact:
  a load command pointing at /opt/homebrew or /Users/ means the dmg was
  mis-assembled and the app cannot launch on any other machine.
- LAN join failing with "No route to host" while internet works = macOS
  Local Network privacy denial; fix on the user side:
  sudo tccutil reset LocalNetwork  (then Allow the re-prompt)
- Everything writes into the folder you ran from; safe to re-run.