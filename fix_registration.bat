@echo off
:: Fix shell extension registration - must run as Administrator

set CLSID={A1B2C3D4-E5F6-7890-ABCD-EF1234567892}
set TIPGUID={00021500-0000-0000-C000-000000000046}

echo Approving shell extension...
reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved" /v "%CLSID%" /t REG_SZ /d "Comic Tooltip Extension" /f

echo Registering under cYo.ComicRack ProgID...
reg add "HKCR\cYo.ComicRack\shellex\%TIPGUID%" /ve /t REG_SZ /d "%CLSID%" /f

echo Registering directly under .cbz and .cbr (belt-and-suspenders)...
reg add "HKCR\.cbz\shellex\%TIPGUID%" /ve /t REG_SZ /d "%CLSID%" /f
reg add "HKCR\.cbr\shellex\%TIPGUID%" /ve /t REG_SZ /d "%CLSID%" /f

echo Notifying shell of changes...
ie4uinit.exe -show

echo Done. Please restart Explorer or log off and back on.
pause
