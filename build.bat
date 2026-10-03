@echo off
setlocal
set "WENA_ROOT=%~dp0"

rem Everything else a build needs (Git for Windows' sh, MinGW-w64, MSYS2,
rem Docker Desktop, the Android NDK) is installed by scripts\toolchain.py when
rem that build runs, with Chocolatey or winget; it needs Python 3.
set "WENA_INSTALLER="
where winget >nul 2>nul && set "WENA_INSTALLER=winget"
where choco >nul 2>nul && set "WENA_INSTALLER=choco"
where py >nul 2>nul
if errorlevel 1 if "%WENA_INSTALLER%"=="choco" (
  echo Installing Python 3: choco install python
  choco install --yes --no-progress python
)
where py >nul 2>nul
if errorlevel 1 if "%WENA_INSTALLER%"=="winget" (
  echo Installing Python 3: winget install Python.Python.3.13
  winget install --exact --id Python.Python.3.13 --silent --accept-source-agreements --accept-package-agreements
)
rem A new install is on PATH only in new terminals; look where it goes.
where py >nul 2>nul
if errorlevel 1 if exist "%SystemRoot%\py.exe" set "PATH=%SystemRoot%;%PATH%"
where py >nul 2>nul
if errorlevel 1 if exist "%LOCALAPPDATA%\Programs\Python\Launcher\py.exe" set "PATH=%LOCALAPPDATA%\Programs\Python\Launcher;%PATH%"
where py >nul 2>nul
if errorlevel 1 (
  echo Python 3 is missing. Install Chocolatey from https://chocolatey.org/install
  echo or winget, or open a new terminal after installing Python, and run build.bat again.
  exit /b 1
)

if "%~1"=="" (
  py -3 "%WENA_ROOT%scripts\wena.py" menu
) else (
  rem Named commands never prompt or pause after a long-running process.
  py -3 "%WENA_ROOT%scripts\wena.py" %*
)
exit /b %ERRORLEVEL%
