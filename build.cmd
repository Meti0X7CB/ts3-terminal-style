@echo off
rem Builds terminalstyle.dll (build\styles) and the gallery test, then renders gallery.png
setlocal
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set VS=%%i
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set QT=C:\Qt\5.15.2\msvc2019_64
cd /d "%~dp0"
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QT% || exit /b 1
cmake --build build || exit /b 1
set PATH=%QT%\bin;%PATH%
set QT_QPA_PLATFORM_PLUGIN_PATH=%QT%\plugins\platforms
build\gallery.exe "%~dp0build" "%~dp0build\gallery.png"
