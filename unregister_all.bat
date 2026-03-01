@echo off
:: Unregister both Comic Tooltip and Preview Handlers
:: Run as Administrator

net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: This script must be run as Administrator.
    echo Right-click and select "Run as administrator".
    pause
    exit /b 1
)

set TOOLTIP_CLSID={A1B2C3D4-E5F6-7890-ABCD-EF1234567892}
set TOOLTIP_GUID={00021500-0000-0000-C000-000000000046}
set PREVIEW_CLSID={B5E84A2F-3D71-4C8A-9F20-1A2B3C4D5E6F}
set PREVIEW_GUID={8895b1c6-b41f-4c1c-a562-0d564250836f}

echo === Unregistering Comic Tooltip Handler ===
reg delete "HKCR\.cbz\shellex\%TOOLTIP_GUID%" /f 2>nul
reg delete "HKCR\.cbr\shellex\%TOOLTIP_GUID%" /f 2>nul
reg delete "HKCR\CLSID\%TOOLTIP_CLSID%" /f 2>nul

echo === Unregistering Comic Preview Handler ===
reg delete "HKCR\.cbz\shellex\%PREVIEW_GUID%" /f 2>nul
reg delete "HKCR\.cbr\shellex\%PREVIEW_GUID%" /f 2>nul
reg delete "HKCR\cYo.ComicRack\shellex\%PREVIEW_GUID%" /f 2>nul
reg delete "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PreviewHandlers" /v "%PREVIEW_CLSID%" /f 2>nul
reg delete "HKCR\CLSID\%PREVIEW_CLSID%" /f 2>nul

echo.
echo === Done ===
echo All handlers unregistered. Restart Explorer for changes to take effect.
pause
