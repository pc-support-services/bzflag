@echo off
REM ============================================================================
REM BZFlag Windows diagnostics - ALL-IN-ONE
REM ============================================================================
REM One script: env report, DLL dependency check, GL probe, crash/Event Log
REM collection, and a captured game run. Everything lands in ONE results file
REM to send back.
REM
REM Output (in the folder you run this from):
REM   RESULTS-diag-windows.txt   <- send this file back

setlocal enabledelayedexpansion
set DIR=%~dp0
set OUT=%DIR%RESULTS-diag-windows.txt

type nul > "%OUT%"
call :log "================ BZFlag Windows diagnostics %date% %time% ================"
call :log "user=%username%  cwd=%DIR%"

echo.
echo Section 1/6: OS + hardware (running...)
call :sect "SECTION 1: OS + hardware"
systeminfo | findstr /C:"OS Name" /C:"OS Version" /C:"System Manufacturer" /C:"System Model" /C:"System Type" >> "%OUT%" 2>&1
call :log "PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITECTURE%"
call :log "PROCESSOR_ARCHITEW6432=%PROCESSOR_ARCHITEW6432%"
echo %PROCESSOR_ARCHITECTURE% | findstr /I ARM64 >nul && call :log "[NOTE] ARM64 Windows - x64 builds run under Prism emulation"
echo %PROCESSOR_ARCHITECTURE% | findstr /I x86 >nul && call :log "[BLOCKER] 32-bit Windows - x64 builds will NOT run"
wmic computersystem get TotalPhysicalMemory >> "%OUT%" 2>&1

echo Section 2/6: GPU (running...)
call :sect "SECTION 2: GPU + driver"
wmic path win32_VideoController get Name,DriverVersion,DriverDate,AdapterRAM,VideoModeDescription >> "%OUT%" 2>&1
where wglinfo64 >nul 2>&1 && (wglinfo64 >> "%OUT%" 2>&1)
where wglinfo >nul 2>&1 && (wglinfo >> "%OUT%" 2>&1)
if not exist wglinfo.exe if not exist "%DIR%wglinfo64.exe" (call :log "wglinfo not on PATH: GL version unknown; GPU + driver above is usually enough")

echo Section 3/6: locating the game (running...)
call :sect "SECTION 3: locate the game"
set BZEXE=
for %%C in ("%DIR%bzflag.exe" "%DIR%bzflag\bin\bzflag.exe" "%DIR%bin\bzflag.exe") do (
  if exist %%C set BZEXE=%%~C
)
if defined BZEXE goto haveexe
echo ERROR: bzflag.exe not found next to this script. >> "%OUT%"
echo [BLOCKER] bzflag.exe not found - unpack the game zip next to this script. >> "%OUT%"
echo Game not found - sections 4 through 6 need it. Press Enter to finish.
pause
goto end
:haveexe
call :log "client binary: %BZEXE%"
"%BZEXE%" -version >> "%OUT%" 2>&1
call :log "[version exit code %errorlevel%]"

echo Section 4/6: DLL dependency check (running...)
call :sect "SECTION 4: DLL dependency check (PE import walk)"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
 "$exe='%BZEXE%';" ^
 "$bytes=[IO.File]::ReadAllBytes($exe);" ^
 "$peOff=[BitConverter]::ToInt32($bytes,0x3C);" ^
 "$numSec=[BitConverter]::ToUInt16($bytes,$peOff+6);" ^
 "$optSize=[BitConverter]::ToUInt16($bytes,$peOff+20);" ^
 "$optOff=$peOff+24;" ^
 "$dataDirOff=$optOff+224;" ^
 "$importRVA=[BitConverter]::ToUInt32($bytes,$dataDirOff+8);" ^
 "$sectOff=$optOff+$optSize;" ^
 "function RVA2Off([uint32]$rva){for($i=0;$i -lt $numSec;$i++){$s=$sectOff+$i*40;$va=[BitConverter]::ToUInt32($bytes,$s+12);$vs=[BitConverter]::ToUInt32($bytes,$s+8);$ra=[BitConverter]::ToUInt32($bytes,$s+20);if($rva -ge $va -and $rva -lt ($va+$vs)){return $ra+($rva-$va)}}return 0}" ^
 "$impOff=RVA2Off $importRVA;" ^
 "$names=@();" ^
 "for($i=$impOff;;$i+=20){$nameRVA=[BitConverter]::ToUInt32($bytes,$i+12);if($nameRVA -eq 0){break};$nOff=RVA2Off $nameRVA;$n='';$j=$nOff;while($bytes[$j] -ne 0){$n+=[char]$bytes[$j];$j++};$names+=$n}" ^
 "foreach($n in $names){if(Test-Path (Join-Path (Split-Path $exe) $n)){'[OK] '+$n}else{'[MISSING] '+$n}}" ^
 "'Total imports: '+$names.Count" >> "%OUT%" 2>&1

echo Section 5/6: crash logs (running...)
call :sect "SECTION 5: Event Log + WER (last 30 days)"
powershell -NoProfile -Command ^
 "$d=(Get-Date).AddDays(-30);" ^
 "Get-WinEvent -FilterHashtable @{LogName='Application';Id=1000,1001;StartTime=$d} -ErrorAction SilentlyContinue |" ^
 "Where-Object {$_.Message -match 'bzflag'} |" ^
 "Select-Object -First 12 | Format-List TimeCreated,Id,Message | Out-String" >> "%OUT%" 2>&1
if exist "%ProgramData%\Microsoft\Windows\WER\ReportArchive" (
  for /d %%D in ("%ProgramData%\Microsoft\Windows\WER\ReportArchive\AppCrash_bzflag*") do (
    echo [WER report] %%D >> "%OUT%"
  )
)

echo Section 6/6: user data (running...)
call :sect "SECTION 6: BZFlag user data"
if exist "%LOCALAPPDATA%\BZFlag\2.4" (
  dir "%LOCALAPPDATA%\BZFlag\2.4" >> "%OUT%" 2>&1
) else (
  echo %LOCALAPPDATA%\BZFlag\2.4 does not exist >> "%OUT%"
)

echo.
echo Section 7: live run
call :sect "SECTION 7: live run (captured)"
call :log "Launching the game windowed with debug logging - press Enter, then play in the problem scene and quit."
pause
echo ==== LIVE RUN %date% %time% ==== >> "%OUT%"
"%BZEXE%" -window -debug >> "%OUT%" 2>&1
set EC=%errorlevel%
call :log "[game exit code %EC%]"
if "%EC%"=="-1073741819" call :log ">>> access violation (0xC0000005): faulting module is in the Event Log section above"

:end
call :log "================ END of diagnostics ================"
call :log "SEND BACK THIS FILE: %OUT%"
echo.
echo Done. Send back: %OUT%
pause
exit /b 0

:log
echo %~1
echo %~1>> "%OUT%"
exit /b 0

:sect
echo.>> "%OUT%"
echo %~1 >> "%OUT%"
echo %~1
exit /b 0