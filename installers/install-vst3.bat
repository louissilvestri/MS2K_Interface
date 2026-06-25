@echo off
rem ===========================================================================
rem  MS2K Editor - VST3 plugin installer.
rem  Copies the "MS2K Editor.vst3" bundle into the system VST3 folder
rem  (%CommonProgramFiles%\VST3) that DAWs scan by default. This needs admin, so
rem  the script self-elevates. Finds the bundle next to this script (release
rem  bundle) or in ..\build (a local developer build).
rem ===========================================================================
setlocal EnableDelayedExpansion
title MS2K Editor VST3 - Installer

rem --- self-elevate: writing to Program Files needs administrator rights ---
net session >nul 2>&1
if errorlevel 1 (
  echo   Requesting administrator rights to install into the system VST3 folder...
  powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)

set "BUNDLE=MS2K Editor.vst3"
set "SRC="
for %%P in (
  "%~dp0%BUNDLE%"
  "%~dp0..\build\MS2K_Plugin_artefacts\VST3\%BUNDLE%"
) do if not defined SRC if exist "%%~fP" set "SRC=%%~fP"

if not defined SRC (
  echo.
  echo   Could not find "%BUNDLE%".
  echo   Build it first from the repo root:
  echo       cmake --build build --target MS2K_Plugin_VST3
  echo   ...or place this installer in the same folder as the built .vst3.
  echo.
  pause
  exit /b 1
)

set "DEST=%CommonProgramFiles%\VST3\%BUNDLE%"
echo.
echo   Installing the MS2K Editor VST3
echo     from : "%SRC%"
echo     to   : "%DEST%"
echo.
if exist "%DEST%" rmdir /s /q "%DEST%"
robocopy "%SRC%" "%DEST%" /e /njh /njs /ndl /nfl /nc /ns /np >nul
if %errorlevel% geq 8 ( echo   ERROR: copy failed. & pause & exit /b 1 )

echo   Installed to "%DEST%".
echo   In your DAW, re-scan VST plug-ins
echo   (Reaper: Options ^> Preferences ^> Plug-ins ^> VST ^> Re-scan).
echo   (To uninstall: delete that folder.)
echo.
pause
exit /b 0
