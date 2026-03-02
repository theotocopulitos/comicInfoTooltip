@echo off
:: Deploy the built DLL to System32
:: Right-click - Run as Administrator

net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 goto NOADMIN

set SRC=%~dp0x64\Release\ComicTooltipExt.dll
set DST=C:\Windows\System32\ComicTooltipExt.dll

if not exist "%SRC%" goto NODLL

echo === Stopping processes ===
taskkill /f /im prevhost.exe >nul 2>&1
taskkill /f /im dllhost.exe >nul 2>&1
taskkill /f /im explorer.exe >nul 2>&1

echo === Waiting 2 seconds ===
timeout /t 2 /nobreak >nul

echo === Copying DLL ===
copy /y "%SRC%" "%DST%"
if errorlevel 1 goto COPYFAIL

echo === Result ===
dir "%DST%"

echo === Starting Explorer ===
start explorer.exe

echo Done!
pause
goto :EOF

:NOADMIN
echo ERROR: This script must be run as Administrator.
echo Right-click and select "Run as administrator".
pause
goto :EOF

:NODLL
echo ERROR: DLL not found at %SRC%
echo Build the project first, Release x64.
pause
goto :EOF

:COPYFAIL
echo FAILED: Could not copy DLL. File may still be locked.
start explorer.exe
pause
goto :EOF
