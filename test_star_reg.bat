@echo off
:: Temporarily register under * (all files) to test if IQueryInfo fires at all
reg add "HKCR\*\shellex\{00021500-0000-0000-C000-000000000046}" /ve /t REG_SZ /d "{A1B2C3D4-E5F6-7890-ABCD-EF1234567892}" /f
echo Registered under * - now hover over ANY file in Explorer and check C:\ComicTooltipExt_debug.log
echo Press any key to remove the * registration...
pause
reg delete "HKCR\*\shellex\{00021500-0000-0000-C000-000000000046}" /f
echo Removed.
