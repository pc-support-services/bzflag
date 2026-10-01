BZFlag Windows diagnostics pack
===============================

Purpose
-------
Turns the "double-click bzflag.exe, it flashes and vanishes" report into real
diagnostics on ANY Windows machine. Ships as a standalone folder: unzips
nothing, installs nothing, admin rights not needed.

WHAT'S INSIDE
-------------
1. DIAGNOSTIC LAUNCHERS
   run-diag.cmd              - launches bzflag.exe -window -debug, captures
                               all console output to diag-log.txt
   run-diag-fullscreen.cmd   - launches bzflag.exe with NO arguments (the
                               exact plain double-click repro), also captured
   run-envreport.cmd         - machine report: Windows version/build, CPU,
                               RAM, GPU + driver, and whether the OS is ARM64
                               (x64 programs run under emulation on ARM64 -
                               today's leading suspect for silent crashes)
   BOTH launchers print the exit code: -1073741819 (0xC0000005) = access
   violation (crash); 0 = clean exit; 3221225786 (0xC0000135) = a DLL was
   missing.

2. SYSTEM EXERCISERS (run order depends on what failed above)
   diag-glinfo.cmd           - OpenGL capability probe. Tries wglinfo.exe
                               (bundled), falls back to a bundled VBS/PowerShell
                               GL renderer query. Shows GPU vendor + driver
                               version + GL version as Windows sees them.
   diag-dependency-check.cmd - verifies every DLL bzflag.exe imports is present
                               in the folder (the "it should at least start"
                               check) and prints a per-DLL verdict.
   diag-cpu-arch.cmd         - one-line answer: is this Windows ARM64?

3. EVENT LOG + CRASH RETRIEVER
   collect-eventlog.cmd      - pulls Application Error (Event ID 1000) and
                               Windows Error Reporting (Event ID 1001) entries
                               for bzflag.exe from the last 30 days into
                               eventlog-bzflag.txt. This is the single most
                               valuable artifact: the "faulting module" line
                               names exactly what died (bzflag.exe itself,
                               SDL2.dll, glew32.dll, or the GPU driver - each
                               points at a different fix).
   collect-werreport.cmd     - if WER has a stored report for bzflag.exe
                               (C:\ProgramData\Microsoft\Windows\WER\), copies
                               the whole report folder into wer-reports\ -
                               includes the crash offset and the loaded-module
                               list.

4. README-FOR-REPORTER.txt - what to run, in order, and where to send results.
   RESULTS.txt               - gets appended by every tool above; this file
                               (plus collect-eventlog output) is what gets
                               sent back.

TO RUN (order matters)
----------------------
1. Double-click run-diag.cmd. A console opens, press a key. If a BZFlag
   window appears it works - press Escape to quit. If it flashes and dies,
   the console stays open and the log gets written.
2. Then run-diag-fullscreen.cmd (plain double-click repro).
3. Then run-envreport.cmd.
4. Then collect-eventlog.cmd.
5. Send back: diag-log.txt, RESULTS.txt, eventlog-bzflag.txt
   (and wer-reports\ if it got created).

NOTES
-----
- If the machine is ARM64 Windows: current release packages are x64 and run
  under Prism emulation. That is a known silent-failure corner - we need an
  ARM64 build for a clean verdict.
- Everything writes into the folder you ran it from; nothing is installed.
- Safe to run multiple times; RESULTS.txt and diag-log.txt append.
- If the machine is dual-boot with Linux: this folder's files persist on the
  Windows filesystem either way; run collect-eventlog.cmd BEFORE rebooting
  back to Linux, or read the Event Viewer file from Linux later (it's at
  C:\Windows\System32\winevt\Logs\Application.evtx and parses from Linux
  too).