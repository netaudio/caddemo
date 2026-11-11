@echo off
setlocal
rem ScanPro CAD UI - 使用指定的 CMake 与 Qt 6.7.3 构建
set "CMAKE=D:\cmake-4.3.4-windows-x86_64\bin\cmake.exe"
set "QT_ROOT=C:\Qt\6.7.3\msvc2019_64"

if not exist "%CMAKE%" (
    echo [ERROR] CMake not found: %CMAKE%
    exit /b 1
)
if not exist "%QT_ROOT%\lib\cmake\Qt6\Qt6Config.cmake" (
    echo [ERROR] Qt 6.7.3 msvc2019_64 not found: %QT_ROOT%
    exit /b 1
)

"%CMAKE%" -S "%~dp0." -B "%~dp0build" -G "Visual Studio 16 2019" -A x64 -DQT_ROOT="%QT_ROOT%"
if errorlevel 1 exit /b 1

"%CMAKE%" --build "%~dp0build" --config Release --parallel
if errorlevel 1 exit /b 1

echo.
echo [OK] %~dp0build\Release\caddemo.exe
echo      Qt runtime has been deployed by windeployqt in POST_BUILD step.
