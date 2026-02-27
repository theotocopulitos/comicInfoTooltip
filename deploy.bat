@echo off
taskkill /f /im explorer.exe 2>nul
taskkill /f /im dllhost.exe 2>nul
ping -n 3 127.0.0.1 > nul
copy /y "C:\Users\Arturo\Documents\Windsurf\git\ComicsShellExtension\CascadeProjects\windsurf-project\x64\Release\ComicTooltipExt.dll" "C:\Windows\System32\ComicTooltipExt.dll"
if errorlevel 1 (
    echo COPY FAILED
    pause
) else (
    echo Copy OK
    dir "C:\Windows\System32\ComicTooltipExt.dll"
)
start explorer.exe
