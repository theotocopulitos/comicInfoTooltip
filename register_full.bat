@echo off
:: Full registration for Comic Tooltip Extension - run as Administrator

set SRCDLL=C:\Users\Arturo\Documents\Windsurf\git\ComicsShellExtension\CascadeProjects\windsurf-project\x64\Release\ComicTooltipExt.dll
set DLL=C:\Windows\System32\ComicTooltipExt.dll
copy /y "%SRCDLL%" "%DLL%"
set CLSID={A1B2C3D4-E5F6-7890-ABCD-EF1234567892}

echo [1] Registering DLL via regsvr32...
regsvr32 /s "%DLL%"

echo [2] Approving shell extension...
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" /v "%CLSID%" /t REG_SZ /d "Comic Tooltip Extension" /f

echo [3] Registering IQueryInfo tooltip handler...
reg add "HKCR\cYo.ComicRack\shellex\{00021500-0000-0000-C000-000000000046}" /ve /t REG_SZ /d "%CLSID%" /f
reg add "HKCR\.cbz\shellex\{00021500-0000-0000-C000-000000000046}" /ve /t REG_SZ /d "%CLSID%" /f
reg add "HKCR\.cbr\shellex\{00021500-0000-0000-C000-000000000046}" /ve /t REG_SZ /d "%CLSID%" /f
:: SystemFileAssociations applies regardless of which app is the default opener
reg add "HKCR\SystemFileAssociations\.cbz\shellex\{00021500-0000-0000-C000-000000000046}" /ve /t REG_SZ /d "%CLSID%" /f
reg add "HKCR\SystemFileAssociations\.cbr\shellex\{00021500-0000-0000-C000-000000000046}" /ve /t REG_SZ /d "%CLSID%" /f
:: CDisplayEx is the current default app for .cbz on this machine
reg add "HKCR\Applications\CDisplayEx.exe\shellex\{00021500-0000-0000-C000-000000000046}" /ve /t REG_SZ /d "%CLSID%" /f

echo [4] Registering as Property Handler (IPropertyStore)...
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.cbz" /ve /t REG_SZ /d "%CLSID%" /f
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\PropertySystem\PropertyHandlers\.cbr" /ve /t REG_SZ /d "%CLSID%" /f

echo [5] Removing conflicting InfoTip prop: entries...
reg delete "HKCR\cYo.ComicRack" /v "InfoTip" /f 2>nul
reg delete "HKCR\.cbz" /v "InfoTip" /f 2>nul
reg delete "HKCR\.cbr" /v "InfoTip" /f 2>nul
reg delete "HKCR\SystemFileAssociations\.cbz" /v "InfoTip" /f 2>nul
reg delete "HKCR\SystemFileAssociations\.cbr" /v "InfoTip" /f 2>nul

echo [6] Notifying shell of changes...
ie4uinit.exe -show

echo Done. Restarting Explorer...
taskkill /f /im explorer.exe >nul 2>&1
timeout /t 2 /nobreak >nul
start explorer.exe

echo Registration complete.
