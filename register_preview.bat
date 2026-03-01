@echo off
:: Register Comic Preview Handler only
:: Run as Administrator

net session >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: This script must be run as Administrator.
    echo Right-click and select "Run as administrator".
    pause
    exit /b 1
)

set DLL_PATH=%~dp0x64\Release\ComicTooltipExt.dll
set CLSID={B5E84A2F-3D71-4C8A-9F20-1A2B3C4D5E6F}
set PREVIEW_GUID={8895b1c6-b41f-4c1c-a562-0d564250836f}
set PREVHOST_APPID={6d2b5079-2f0b-48dd-ab7f-97cec514d30b}

echo === Registering Comic Preview Handler ===

:: Register CLSID
reg add "HKCR\CLSID\%CLSID%\InprocServer32" /ve /d "%DLL_PATH%" /f
reg add "HKCR\CLSID\%CLSID%\InprocServer32" /v "ThreadingModel" /d "Apartment" /f

:: AppID for prevhost.exe surrogate
reg add "HKCR\CLSID\%CLSID%" /v "AppID" /d "%PREVHOST_APPID%" /f

:: Allow file access from low-integrity process
reg add "HKCR\CLSID\%CLSID%" /v "DisableLowILProcessIsolation" /t REG_DWORD /d 1 /f

:: Whitelist in Windows preview handlers
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PreviewHandlers" /v "%CLSID%" /d "Comic Preview Handler" /f

:: Register for .cbz and .cbr (direct extension)
reg add "HKCR\.cbz\shellex\%PREVIEW_GUID%" /ve /d "%CLSID%" /f
reg add "HKCR\.cbr\shellex\%PREVIEW_GUID%" /ve /d "%CLSID%" /f

:: Register under common ProgIDs (if they exist)
reg add "HKCR\cYo.ComicRack\shellex\%PREVIEW_GUID%" /ve /d "%CLSID%" /f 2>nul

echo.
echo === Done ===
echo Preview handler registered for .cbz and .cbr files.
echo Open Explorer, press Alt+P for the Preview Pane, and click a comic file.
pause
