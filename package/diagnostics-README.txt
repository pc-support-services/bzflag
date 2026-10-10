BZFlag diagnostics packs - README
=================================

Portable, self-contained diagnostic folders for all three platforms.
Each pack = ONE all-in-one script + a README. Run it, play through the
problem scene once, quit the game, send back the single results file.

THE ONE SCRIPT PER PLATFORM
---------------------------
Windows:  bzflag-diag-windows.cmd   (double-click)
macOS:    bzflag-diag-macos.sh      (chmod +x, then run from Terminal)
Linux:    bzflag-diag-linux.sh      (chmod +x, then run from a terminal)

Each writes ONE results file next to itself:
  RESULTS-diag-windows.txt / RESULTS-diag-macos.txt / RESULTS-diag-linux.txt
Send that file back. It contains, in order:
  1. OS + hardware + GPU/driver (Windows: + CPU arch ARM64 verdict)
  2. GL capability probe (Windows: wglinfo or registry GPU query)
  3. Game binary location + version (Windows: + DLL import walk verdict)
  4. (macOS) bundle sanity: otool -L runner-path check, Frameworks, fonts
  5. crash logs: Event Log/WER, DiagnosticReports .ips, journalctl/coredumpctl
  6. user config render-relevant lines
  7. captured live game run (windowed + debug) with the exit code

GRANULAR SCRIPTS (kept for follow-up questions)
-----------------------------------------------
Windows (from the original pack):
  run-diag.cmd, run-diag-fullscreen.cmd, run-envreport.cmd,
  diag-glinfo.cmd, diag-dependency-check.cmd, diag-cpu-arch.cmd,
  collect-eventlog.cmd, collect-werreport.cmd
Linux (package/linux/diagnostics/):
  run-diag-linux.sh, run-envreport-linux.sh, diag-glinfo-linux.sh,
  diag-dependency-linux.sh, collect-eventlog-linux.sh
macOS (package/macos/diagnostics/):
  run-diag-macos.sh, run-envreport-macos.sh, collect-eventlog-macos.sh

DISTRIBUTION
------------
Zip the diagnostics/<platform>/ folder and attach to the issue/report.
Everything is portable; nothing installs; no admin needed (except sudo
tccutil and log show on macOS when permissions require it).

KNOWN PLATFORM NOTES
--------------------
- Windows ARM64: x64 builds run under Prism emulation; the arch verdict
  is in section 1. Event Log faulting module ntdll.dll on ARM64 = emulate.
- macOS Local Network: LAN joins failing with "No route to host" while
  internet works = privacy denial; fix is `sudo tccutil reset LocalNetwork`
  then Allow the re-prompt (section 6 of the macOS script explains it).
- macOS dmg mis-assembly: leftover /opt/homebrew or /Users/ paths in the
  app binary's load commands = the app cannot launch on any other machine
  (section 2 flags it as [BLOCKER]).
- Linux Wayland: bzflag runs under XWayland; window-geometry warnings from
  the compositor in section 5 are normal.