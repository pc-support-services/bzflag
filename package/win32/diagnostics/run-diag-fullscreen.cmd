@echo off
title BZFlag diagnostic launcher (fullscreen / plain double-click repro)
echo === BZFlag diagnostic run (default args, same as plain double-click) ===
echo Press a key now to start the game.
setlocal
set LOG=%~dp0diag-log.txt
set RES=%~dp0RESULTS.txt

echo ==== FULLSCREEN-STYLE RUN %date% %time% user=%username% ==== >> "%LOG%"
echo ===== run-diag-fullscreen (default args) %date% %time% user=%username% ===== >> "%RES%"
pause
"%~dp0bzflag.exe" >> "%LOG%" 2>&1
set EC=%errorlevel%
echo [game exit code %EC%] >> "%LOG%"
echo [game exit code %EC%] >> "%RES%"
if "%EC%"=="-1073741819" echo >> "%RES%": access violation (0xC0000005). Check collect-eventlog.cmd output for the faulting module. >> "%RES%"
echo.
echo Game exited. Results are in diag-log.txt and RESULTS.txt in this folder.
pause