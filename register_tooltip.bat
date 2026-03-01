@echo off
:: Register Comic Tooltip Handler only
:: Run as Administrator

net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: This script must be run as Administrator.
    echo Right-click and select "Run as administrator".
    pause
    exit /b 1
)

set DLL_PATH=%~dp0x64\Release\ComicTooltipExt.dll
set CLSID={A1B2C3D4-E5F6-7890-ABCD-EF1234567892}
set TOOLTIP_GUID={00021500-0000-0000-C000-000000000046}

echo === Registering Comic Tooltip Handler ===

:: Register CLSID
reg add "HKCR\CLSID\%CLSID%\InprocServer32" /ve /d "%DLL_PATH%" /f
reg add "HKCR\CLSID\%CLSID%\InprocServer32" /v "ThreadingModel" /d "Apartment" /f

:: Register for .cbz and .cbr
reg add "HKCR\.cbz\shellex\%TOOLTIP_GUID%" /ve /d "%CLSID%" /f
reg add "HKCR\.cbr\shellex\%TOOLTIP_GUID%" /ve /d "%CLSID%" /f

echo.
echo === Done ===
echo Tooltip handler registered for .cbz and .cbr files.
echo You may need to restart Explorer for changes to take effect.
pause
