@echo off
title BZFlag diagnostics - event log extractor
echo === BZFlag event log extractor ===
setlocal enabledelayedexpansion
set RES=%~dp0RESULTS.txt
set OUT=%~dp0eventlog-bzflag.txt

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
 "$days=(Get-Date).AddDays(-30);" ^
 "$applog=Get-WinEvent -FilterHashtable @{LogName='Application';Id=1000,1001;StartTime=$days} -ErrorAction SilentlyContinue |" ^
 "Where-Object { $_.Message -match 'bzflag' };" ^
 "if (-not $applog) { 'No Application Error / WER entries for bzflag.exe in the last 30 days.' | Out-File -FilePath '%OUT%' -Encoding utf8 }" ^
 "else { $applog | ForEach-Object { ''; '==== ' + $_.TimeCreated + ' (Event ID ' + $_.Id + ') ===='; $_.Message } | Out-File -FilePath '%OUT%' -Encoding utf8 }"

echo ===== EVENTLOG EXTRACT %date% %time% ===== >> "%RES%"
if exist "%OUT%" (
  findstr /C:"====" /C:"Exception code" /C:"faulting" /C:"Faulting" "%OUT%" >> "%RES%" 2>&1
  echo Full listing in eventlog-bzflag.txt in this folder.
) else (
  echo eventlog-bzflag.txt was NOT created - check PowerShell errors above.
)
echo.
echo This is the most valuable artifact: the faulting-module line names what
echo crashed (bzflag.exe itself, SDL2.dll, glew32.dll, or the GPU driver).
pause