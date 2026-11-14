@echo off
setlocal
echo FutureBattle - Wide Test Field: 240m x 180m, 250 vs 250
echo 50 leaders + 450 Mass followers   Q/J/K: select/delegate/manual
echo P: start/pause AI   E/Tab: TPS/RTS   F5: restart
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Setup\Play-Prototype.ps1" -LargeField
if errorlevel 1 (
  echo Launch failed. See the error above.
  pause
  exit /b 1
)
