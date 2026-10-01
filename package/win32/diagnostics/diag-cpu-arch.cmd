@echo off
title BZFlag diagnostics - CPU / architecture check
echo === BZFlag architecture check ===
setlocal enabledelayedexpansion
set RES=%~dp0RESULTS.txt
echo ===== ARCH CHECK %date% %time% ===== >> "%RES%"
echo PROCESSOR_ARCHITECTURE=%PROCESSOR_ARCHITECTURE% >> "%RES%"
echo PROCESSOR_ARCHITEW6432=%PROCESSOR_ARCHITEW6432% >> "%RES%"
echo %PROCESSOR_ARCHITECTURE% | findstr /I ARM64 >nul && echo This Windows is ARM64 - x64 builds run under Prism emulation. >> "%RES%"
echo %PROCESSOR_ARCHITECTURE% | findstr /I ARM64 >nul && echo This Windows is ARM64 - x64 builds run under Prism emulation.
echo %PROCESSOR_ARCHITECTURE% | findstr /I AMD64 >nul && echo This Windows is genuine x64 (no emulation involved).
echo %PROCESSOR_ARCHITECTURE% | findstr /I x86 >nul && echo This Windows is 32-bit x86 - x64 builds will NOT run at all.
pause