@echo off
title BZFlag diagnostics - environment report
echo === BZFlag environment report ===
setlocal enabledelayedexpansion
set RES=%~dp0RESULTS.txt

echo ===== ENV REPORT %date% %time% user=%username% ===== >> "%RES%"

echo [Windows version] >> "%RES%"
ver >> "%RES%"

echo [System model / OEM string] >> "%RES%"
systeminfo | findstr /C:"OS Name" /C:"OS Version" /C:"System Manufacturer" /C:"System Model" /C:"System Type" >> "%RES%" 2>&1

echo [RAM] >> "%RES%"
wmic computersystem get TotalPhysicalMemory >> "%RES%" 2>&1

echo [GPU and driver] >> "%RES%"
wmic path win32_VideoController get Name,DriverVersion,AdapterRAM >> "%RES%" 2>&1

echo [Processor] >> "%RES%"
wmic cpu get Name,Manufacturer >> "%RES%" 2>&1

echo [OS architecture] >> "%RES%"
echo PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITECTURE% >> "%RES%"
echo PROCESSOR_ARCHITEW6432=%PROCESSOR_ARCHITEW6432% >> "%RES%"
reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" /v PROCESSOR_ARCHITECTURE >> "%RES%" 2>&1

echo [Existing BZFlag user data] >> "%RES%"
if exist "%LOCALAPPDATA%\BZFlag\2.4" (
  dir "%LOCALAPPDATA%\BZFlag\2.4" >> "%RES%" 2>&1
) else (
  echo %LOCALAPPDATA%\BZFlag\2.4 does not exist >> "%RES%"
)

echo ===== END ENV REPORT ===== >> "%RES%"
echo. 
echo Wrote to RESULTS.txt. Key line: "System Type" contains ARM64 if this is
echo an ARM machine - x64 runs under emulation there.
pause