@echo off
title BZFlag diagnostics - OpenGL capability probe
echo === BZFlag OpenGL probe ===
setlocal enabledelayedexpansion
set RES=%~dp0RESULTS.txt
echo ===== GL PROBE %date% %time% ===== >> "%RES%"

where wglinfo >nul 2>&1 && (wglinfo >> "%RES%" 2>&1 & goto done)
where wglinfo64 >nul 2>&1 && (wglinfo64 >> "%RES%" 2>&1 & goto done)

echo wglinfo not on PATH - falling back to driver-registry query (shows GPU + driver, not GL version) >> "%RES%"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
 "Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,DriverDate,VideoModeDescription,AdapterRAM | Format-List | Out-String" >> "%RES%" 2>&1
echo.
echo NOTE: this fallback shows the GPU + driver version. If you also need the
echo exact OpenGL version reported by the driver, the GPU vendor's tool
echo (e.g. glview from RealTech) can print that; the driver version above is
echo usually enough for the crash triage.
:done
echo Results appended to RESULTS.txt.
pause