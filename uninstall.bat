@echo off
echo Uninstalling Comic Tooltip Shell Extension...

REM Unregister the DLL
regsvr32 /s /u ComicTooltipExt.dll

if %ERRORLEVEL% EQU 0 (
    echo Uninstallation successful!
    echo.
    echo The shell extension has been unregistered.
    echo You may need to restart Windows Explorer to see the changes.
) else (
    echo Uninstallation failed!
    echo Please run this as Administrator.
)

pause
