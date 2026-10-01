@echo off
title BZFlag diagnostic launcher (windowed)
echo === BZFlag diagnostic run (windowed) ===
echo A console window stays open and captures everything into diag-log.txt.
echo Press a key now to start the game.
setlocal
set LOG=%~dp0diag-log.txt
set RES=%~dp0RESULTS.txt

echo ==== WINDOWED RUN %date% %time% user=%username% ====>> "%LOG%"
echo [0] exe self-version check: >> "%LOG%"
"%~dp0bzflag.exe" -version >> "%LOG%" 2>&1
echo [exit code %errorlevel%] >> "%LOG%"
echo [1] launching GAME -window -debug (windowed, skips first-run fullscreen default) >> "%LOG%"
echo ===== run-diag (windowed) %date% %time% user=%username% ===== >> "%RES%"
echo [version check exit code %errorlevel%] >> "%RES%"
pause
"%~dp0bzflag.exe" -window -debug >> "%LOG%" 2>&1
set EC=%errorlevel%
echo [2] game exit code %EC% >> "%LOG%"
echo [game exit code %EC%] >> "%RES%"
if "%EC%"=="-1073741819" echo >> "%RES%": access violation (0xC0000005). Check collect-eventlog.cmd output for the faulting module. >> "%RES%"
echo.
echo Game exited. Now also try run-diag-fullscreen.cmd (the plain double-click repro).
echo Results are in diag-log.txt and RESULTS.txt in this folder.
pause