@echo off
title BZFlag diagnostics - DLL dependency check
echo === BZFlag DLL dependency check ===
setlocal enabledelayedexpansion
set RES=%~dp0RESULTS.txt
echo ===== DEPENDENCY CHECK %date% %time% ===== >> "%RES%"

if not exist "%~dp0bzflag.exe" (
  echo ERROR: bzflag.exe not found next to this script. Put this folder next to the game or use the packaged zip. >> "%RES%"
  pause
  exit /b 1
)

echo Checking imports of %~dp0bzflag.exe via PowerShell (no external tools needed) >> "%RES%"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
 "$exe='%~dp0bzflag.exe';" ^
 "$bytes=[IO.File]::ReadAllBytes($exe);" ^
 "$peOff=[BitConverter]::ToInt32($bytes,0x3C);" ^
 "$numSec=[BitConverter]::ToUInt16($bytes,$peOff+6);" ^
 "$optSize=[BitConverter]::ToUInt16($bytes,$peOff+20);" ^
 "$optOff=$peOff+24;" ^
 "$dataDirOff=$optOff+(224);" ^
 "$importRVA=[BitConverter]::ToUInt32($bytes,$dataDirOff+8);" ^
 "$sectOff=$optOff+$optSize;" ^
 "function RVA2Off([uint32]$rva){" ^
 " for($i=0;$i -lt $numSec;$i++){$s=$sectOff+$i*40;$va=[BitConverter]::ToUInt32($bytes,$s+12);$vs=[BitConverter]::ToUInt32($bytes,$s+8);$ra=[BitConverter]::ToUInt32($bytes,$s+20);if($rva -ge $va -and $rva -lt ($va+$vs)){return $ra+($rva-$va)}} return 0 }" ^
 "$impOff=RVA2Off $importRVA;" ^
 "$names=@();" ^
 "for($i=$impOff;;$i+=20){" ^
 " $nameRVA=[BitConverter]::ToUInt32($bytes,$i+12);if($nameRVA -eq 0){break};" ^
 " $nOff=RVA2Off $nameRVA;$n='';$j=$nOff;" ^
 " while($bytes[$j] -ne 0){$n+=[char]$bytes[$j];$j++};" ^
 " $names+=$n}" ^
 "foreach($n in $names){" ^
 " if(Test-Path (Join-Path (Split-Path $exe) $n)){ '[OK] '+$n } else { '[MISSING] '+$n } }" ^
 "'Done. Total imports: '+$names.Count" >> "%RES%" 2>&1

echo Full DLL verdicts in RESULTS.txt. Any [MISSING] line is a start-blocker;
echo all [OK] means the failure is at runtime, not load time.
pause