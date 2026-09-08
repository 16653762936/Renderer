@echo off
setlocal
call "%~dp0build.bat"
if errorlevel 1 exit /b 1
"%~dp0build\renderer.exe"
exit /b %ERRORLEVEL%
