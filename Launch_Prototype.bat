@echo off
setlocal
cd /d "%~dp0"
if not exist "dist\curved_world.exe" powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build.ps1"
if errorlevel 1 exit /b 1
start "" "%~dp0dist\curved_world.exe"
