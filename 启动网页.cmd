@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0open_web.ps1" %*
if errorlevel 1 (
  echo Web launch failed.
  pause
  exit /b 1
)
