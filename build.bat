@echo off
rem Build the game with CMake and Ninja (tools from install_tools.py).
rem Usage: build.bat [Debug^|Release]   (default: Release)
setlocal
cd /d "%~dp0"
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Release
cmake -S . -B "build\%CONFIG%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% || goto :failed
cmake --build "build\%CONFIG%" || goto :failed
echo.
echo Built build\%CONFIG%\OpenGLStarter.exe (sounds copied to build\%CONFIG%\assets)
exit /b 0
:failed
echo Build failed.
exit /b 1
