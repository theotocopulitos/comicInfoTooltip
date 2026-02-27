@echo off
echo Building Comic Tooltip Shell Extension...

REM Check if Visual Studio environment is set
if not defined VCINSTALLDIR (
    echo Visual Studio environment not detected. Please run this from a Developer Command Prompt.
    echo.
    echo To set up the environment:
    echo 1. Open "Developer Command Prompt for VS 2022" from Start Menu
    echo 2. Navigate to this directory
    echo 3. Run build.bat again
    pause
    exit /b 1
)

REM Clean previous build
echo Cleaning previous build...
if exist "x64" rmdir /s /q "x64"
if exist "Debug" rmdir /s /q "Debug"
if exist "Release" rmdir /s /q "Release"

REM Build Release configuration
echo Building Release configuration...
msbuild ComicTooltipExt.vcxproj /p:Configuration=Release /p:Platform=x64 /p:OutDir=Release\ /verbosity:minimal

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Build successful!
    echo Output: Release\ComicTooltipExt.dll
    echo.
    echo To install the shell extension, run: install.bat
) else (
    echo.
    echo Build failed!
    echo Please check the error messages above.
)

pause
