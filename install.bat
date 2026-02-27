@echo off
echo Installing Comic Tooltip Shell Extension...

REM Register the DLL
regsvr32 /s ComicTooltipExt.dll

if %ERRORLEVEL% EQU 0 (
    echo Installation successful!
    echo.
    echo The shell extension has been registered for .cbr and .cbz files.
    echo You may need to restart Windows Explorer to see the changes.
    echo.
    echo To uninstall, run: uninstall.bat
) else (
    echo Installation failed!
    echo Please run this as Administrator.
)

pause
