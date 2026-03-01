@echo off
:: Unregister Comic Preview Handler only
:: Run as Administrator

net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: This script must be run as Administrator.
    echo Right-click and select "Run as administrator".
    pause
    exit /b 1
)

set CLSID={B5E84A2F-3D71-4C8A-9F20-1A2B3C4D5E6F}
set PREVIEW_GUID={8895b1c6-b41f-4c1c-a562-0d564250836f}

echo === Unregistering Comic Preview Handler ===

reg delete "HKCR\.cbz\shellex\%PREVIEW_GUID%" /f 2>nul
reg delete "HKCR\.cbr\shellex\%PREVIEW_GUID%" /f 2>nul
reg delete "HKCR\cYo.ComicRack\shellex\%PREVIEW_GUID%" /f 2>nul
reg delete "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PreviewHandlers" /v "%CLSID%" /f 2>nul
reg delete "HKCR\CLSID\%CLSID%" /f 2>nul

echo.
echo === Done ===
echo Preview handler unregistered. Restart Explorer for changes to take effect.
pause
