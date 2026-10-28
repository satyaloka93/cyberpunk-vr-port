@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Switch-CyberpunkVRPort.ps1" -Action Install -Edition Fork %*
set "RC=%ERRORLEVEL%"
echo.
if not "%RC%"=="0" echo Installer stopped with code %RC%. Read the explanation above.
pause
exit /b %RC%
