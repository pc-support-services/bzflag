@echo off
title BZFlag diagnostics - WER report collector
echo === BZFlag Windows Error Reporting collector ===
setlocal enabledelayedexpansion
set RES=%~dp0RESULTS.txt
set DEST=%~dp0wer-reports

echo ===== WER COLLECT %date% %time% ===== >> "%RES%"
if not exist "%ProgramData%\Microsoft\Windows\WER" (
  echo No WER store found at %ProgramData%\Microsoft\Windows\WER >> "%RES%"
  echo Nothing to collect.
  pause
  exit /b 1
)
if exist "%DEST%" rmdir /s /q "%DEST%"
mkdir "%DEST%"

set FOUND=0
for /d %%D in ("%ProgramData%\Microsoft\Windows\WER\ReportArchive\*bzflag*") do (
  xcopy "%%D" "%DEST%\%%~nxD\" /E /I /Q /Y >nul 2>&1
  set FOUND=1
  echo Copied %%~nxD >> "%RES%"
)
for /d %%D in ("%ProgramData%\Microsoft\Windows\WER\ReportQueue\*bzflag*") do (
  xcopy "%%D" "%DEST%\%%~nxD\" /E /I /Q /Y >nul 2>&1
  set FOUND=1
  echo Copied %%~nxD >> "%RES%"
)
for /d %%D in ("C:\Users\%username%\AppData\Local\Microsoft\Windows\WER\ReportArchive\*bzflag*") do (
  xcopy "%%D" "%DEST%\%%~nxD\" /E /I /Q /Y >nul 2>&1
  set FOUND=1
  echo Copied user-side %%~nxD >> "%RES%"
)
for /d %%D in ("C:\Users\%username%\AppData\Local\Microsoft\Windows\WER\ReportQueue\*bzflag*") do (
  xcopy "%%D" "%DEST%\%%~nxD\" /E /I /Q /Y >nul 2>&1
  set FOUND=1
  echo Copied user-side %%~nxD >> "%RES%"
)

if "%FOUND%"=="0" (
  echo No WER reports matching *bzflag* found in ReportArchive/ReportQueue. >> "%RES%"
  echo No WER reports found. If the crash was recent, run collect-eventlog.cmd instead.
) else (
  echo Copied WER report folders below: >> "%RES%"
  dir /b "%DEST%" >> "%RES%" 2>&1
)
pause