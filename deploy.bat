@echo off
:: Deploy the built DLL to System32 (for development)
:: Run as Administrator
:: Stops Explorer and prevhost.exe to release the DLL lock

net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: This script must be run as Administrator.
    echo Right-click and select "Run as administrator".
    pause
    exit /b 1
)

set SRC=%~dp0x64\Release\ComicTooltipExt.dll
set DST=C:\Windows\System32\ComicTooltipExt.dll

if not exist "%SRC%" (
    echo ERROR: DLL not found at %SRC%
    echo Build the project first (Release x64).
    pause
    exit /b 1
)

echo === Stopping processes ===
taskkill /f /im explorer.exe 2>nul
taskkill /f /im prevhost.exe 2>nul
taskkill /f /im dllhost.exe 2>nul

echo === Waiting 3 seconds ===
ping -n 4 127.0.0.1 > nul

echo === Copying DLL ===
copy /y "%SRC%" "%DST%_new"
if errorlevel 1 (
    echo FAILED: Could not copy DLL. File may still be locked.
    start explorer.exe
    pause
    exit /b 1
)
del /f "%DST%" 2>nul
rename "%DST%_new" "ComicTooltipExt.dll"

echo === Result ===
dir "%DST%"

echo === Starting Explorer ===
start explorer.exe

echo Done!
pause
