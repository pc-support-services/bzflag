BZFlag Windows diagnostics - QUICK RUN SHEET
============================================

Run in this order. Each tool drops output into RESULTS.txt or its own file
in the same folder. Send back ALL of:

  diag-log.txt, RESULTS.txt, eventlog-bzflag.txt
  (plus wer-reports\ folder if collect-werreport.cmd created one)

1. run-diag.cmd
     Press a key. BZFlag window appears (works) or flash+die (broken).
     Record what happened + the exit code printed at the end.
2. run-diag-fullscreen.cmd
     Same as plain double-click. Record what happened + the exit code.
3. run-envreport.cmd
     Machine report. Look for "System Type" - ARM64 on that line means
     x64 runs under emulation, which is today's leading suspect.
4. diag-glinfo.cmd
     GPU + driver version probe.
5. diag-dependency-check.cmd
     Confirms every DLL bzflag.exe imports is present (all-[OK] = fine at
     load time; failure is at runtime, not "a file is missing").
6. diag-cpu-arch.cmd
     One-line ARM/x64 verdict. (If step 3 already answered it, skip.)
7. collect-eventlog.cmd
     Most valuable: pulls Application Error entries for bzflag.exe;
     the faulting-module line names what died.
8. collect-werreport.cmd
     Copies any stored WER crash reports for bzflag.exe into wer-reports\.

Then zip the whole folder and send it back.

Notes
-----
- Everything is portable; nothing is installed; no admin needed (event log
  read works from a normal user account; WER collect works from the same
  account that ran the crashing app, or admin).
- Safe to run multiple times.
- If the machine is ARM64: the current release is x64 and runs under
  Prism emulation. If the event log faulting module is ntdll.dll and the
  machine is ARM64, the fix is a native ARM64 build - nothing on your end.