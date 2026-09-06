@echo off
setlocal
set "WENA_ROOT=%~dp0"

if "%~1"=="" (
  py -3 "%WENA_ROOT%scripts\wena.py" menu
) else (
  rem Named commands never prompt or pause after a long-running process.
  py -3 "%WENA_ROOT%scripts\wena.py" %*
)
exit /b %ERRORLEVEL%
