@echo off
setlocal

call "D:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1

set "PATH=D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"

if not exist "%~dp0build\build.ninja" (
    cmake -S "%~dp0." -B "%~dp0build" -G Ninja
    if errorlevel 1 exit /b 1
)

cmake --build "%~dp0build"
exit /b %ERRORLEVEL%
