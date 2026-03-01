@echo off
:: Register both Comic Tooltip and Preview Handlers
:: Run as Administrator

net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: This script must be run as Administrator.
    echo Right-click and select "Run as administrator".
    pause
    exit /b 1
)

set DLL_PATH=%~dp0x64\Release\ComicTooltipExt.dll
set TOOLTIP_CLSID={A1B2C3D4-E5F6-7890-ABCD-EF1234567892}
set TOOLTIP_GUID={00021500-0000-0000-C000-000000000046}
set PREVIEW_CLSID={B5E84A2F-3D71-4C8A-9F20-1A2B3C4D5E6F}
set PREVIEW_GUID={8895b1c6-b41f-4c1c-a562-0d564250836f}
set PREVHOST_APPID={6d2b5079-2f0b-48dd-ab7f-97cec514d30b}

echo === Registering Comic Tooltip Handler ===
reg add "HKCR\CLSID\%TOOLTIP_CLSID%\InprocServer32" /ve /d "%DLL_PATH%" /f
reg add "HKCR\CLSID\%TOOLTIP_CLSID%\InprocServer32" /v "ThreadingModel" /d "Apartment" /f
reg add "HKCR\.cbz\shellex\%TOOLTIP_GUID%" /ve /d "%TOOLTIP_CLSID%" /f
reg add "HKCR\.cbr\shellex\%TOOLTIP_GUID%" /ve /d "%TOOLTIP_CLSID%" /f

echo.
echo === Registering Comic Preview Handler ===
reg add "HKCR\CLSID\%PREVIEW_CLSID%\InprocServer32" /ve /d "%DLL_PATH%" /f
reg add "HKCR\CLSID\%PREVIEW_CLSID%\InprocServer32" /v "ThreadingModel" /d "Apartment" /f
reg add "HKCR\CLSID\%PREVIEW_CLSID%" /v "AppID" /d "%PREVHOST_APPID%" /f
reg add "HKCR\CLSID\%PREVIEW_CLSID%" /v "DisableLowILProcessIsolation" /t REG_DWORD /d 1 /f
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PreviewHandlers" /v "%PREVIEW_CLSID%" /d "Comic Preview Handler" /f
reg add "HKCR\.cbz\shellex\%PREVIEW_GUID%" /ve /d "%PREVIEW_CLSID%" /f
reg add "HKCR\.cbr\shellex\%PREVIEW_GUID%" /ve /d "%PREVIEW_CLSID%" /f
reg add "HKCR\cYo.ComicRack\shellex\%PREVIEW_GUID%" /ve /d "%PREVIEW_CLSID%" /f 2>nul

echo.
echo === Done ===
echo Both handlers registered for .cbz and .cbr files.
echo - Tooltip: hover over a comic file to see metadata.
echo - Preview: press Alt+P in Explorer and click a comic file.
pause
