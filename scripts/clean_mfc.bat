@echo off
setlocal
if /I "%~1"=="/?" goto usage
if /I "%~1"=="-h" goto usage
if /I "%~1"=="--help" goto usage
if not "%~1"=="" (
  echo Unsupported clean argument.
  exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0clean_intermediates.ps1"
exit /b %errorlevel%

:usage
echo Usage: %~nx0
echo Removes allowlisted compiler files; preserves delivery/runtime/unknown files.
exit /b 0
