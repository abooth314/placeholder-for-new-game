@echo off
setlocal
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\playtest.ps1" %*
set "playtest_exit=%errorlevel%"
if not "%playtest_exit%"=="0" pause
exit /b %playtest_exit%
