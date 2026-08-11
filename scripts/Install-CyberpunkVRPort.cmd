@echo off
REM Double-clickable launcher for the CyberpunkVR Port installer.
REM
REM Exists because PowerShell refuses to run .ps1 files under the default ExecutionPolicy, so a
REM plain script would fail on most users' machines with "running scripts is disabled on this
REM system" before it did anything. -ExecutionPolicy Bypass applies to this one invocation only
REM and changes nothing machine-wide.
REM
REM Prefers PowerShell 7 (pwsh) when installed, otherwise Windows PowerShell 5.1, which is
REM present on every supported Windows.
REM
REM Optional: drag a folder onto this file, or pass one, to use it for downloads.
REM   Install-CyberpunkVRPort.cmd "D:\my downloads"

setlocal
cd /d "%~dp0"

set "PSEXE=powershell.exe"
where pwsh.exe >nul 2>&1 && set "PSEXE=pwsh.exe"

set "DLARG="
if not "%~1"=="" set "DLARG=-DownloadDir "%~1""

echo.
echo   CyberpunkVR Port installer
echo   Using: %PSEXE%
echo.

%PSEXE% -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-CyberpunkVRPort.ps1" %DLARG%
set "RC=%ERRORLEVEL%"

echo.
if not "%RC%"=="0" (
    echo   Install did not complete ^(exit %RC%^). Read the message above -- if it lists
    echo   files to download, fetch them, drop them in the downloads folder and run this again.
) else (
    echo   Done. Launch Cyberpunk 2077 normally from Steam.
)
echo.
pause
endlocal
