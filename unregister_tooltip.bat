@echo off
:: Unregister Comic Tooltip Handler only
:: Run as Administrator

net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: This script must be run as Administrator.
    echo Right-click and select "Run as administrator".
    pause
    exit /b 1
)

set CLSID={A1B2C3D4-E5F6-7890-ABCD-EF1234567892}
set TOOLTIP_GUID={00021500-0000-0000-C000-000000000046}

echo === Unregistering Comic Tooltip Handler ===

reg delete "HKCR\.cbz\shellex\%TOOLTIP_GUID%" /f 2>nul
reg delete "HKCR\.cbr\shellex\%TOOLTIP_GUID%" /f 2>nul
reg delete "HKCR\CLSID\%CLSID%" /f 2>nul

echo.
echo === Done ===
echo Tooltip handler unregistered. Restart Explorer for changes to take effect.
pause
