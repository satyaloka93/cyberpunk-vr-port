@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Switch-CyberpunkVRPort.ps1" %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" echo Switcher stopped with code %RC%. See the explanation above.
pause
exit /b %RC%
