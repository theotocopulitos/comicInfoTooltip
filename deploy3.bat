@echo off
echo === Stopping processes ===
taskkill /f /im explorer.exe 2>nul
taskkill /f /im dllhost.exe 2>nul
taskkill /f /im Everything.exe 2>nul
taskkill /f /im python.exe 2>nul

echo === Waiting 4 seconds ===
ping -n 5 127.0.0.1 > nul

echo === Attempting copy to new name first ===
copy /y "C:\Users\Arturo\Documents\Windsurf\git\ComicsShellExtension\CascadeProjects\windsurf-project\x64\Release\ComicTooltipExt.dll" "C:\Windows\System32\ComicTooltipExt_new.dll"
if errorlevel 1 (
    echo COPY TO NEW NAME ALSO FAILED - check permissions
    pause
    exit /b 1
)
echo Copy to new name OK

echo === Deleting old DLL ===
del /f "C:\Windows\System32\ComicTooltipExt.dll"
if errorlevel 1 (
    echo DELETE FAILED - file still locked
    pause
    exit /b 1
)

echo === Renaming new to final name ===
rename "C:\Windows\System32\ComicTooltipExt_new.dll" "ComicTooltipExt.dll"

echo === Result ===
dir "C:\Windows\System32\ComicTooltipExt.dll"

echo === Starting Explorer ===
start explorer.exe
echo Done!
pause
