@echo off
rem ===========================================================================
rem  MS2K Interface - standalone app installer (per-user, no admin required).
rem  Installs the MS2000 editor to %LOCALAPPDATA%\Programs and adds Start Menu
rem  + Desktop shortcuts. Finds the exe next to this script (release bundle) or
rem  in ..\build (a local developer build).
rem ===========================================================================
setlocal EnableDelayedExpansion
title MS2K Interface - Installer

set "EXE=MS2K_Interface.exe"
set "SRC="
for %%P in (
  "%~dp0%EXE%"
  "%~dp0..\build\MS2K_Interface_artefacts\%EXE%"
) do if not defined SRC if exist "%%~fP" set "SRC=%%~fP"

if not defined SRC (
  echo.
  echo   Could not find %EXE%.
  echo   Build it first from the repo root:
  echo       cmake --build build --target MS2K_Interface
  echo   ...or place this installer in the same folder as the built exe.
  echo.
  pause
  exit /b 1
)

set "DEST=%LOCALAPPDATA%\Programs\MS2K Interface"
echo.
echo   Installing the MS2000 editor (standalone app)
echo     from : "%SRC%"
echo     to   : "%DEST%"
echo.
if not exist "%DEST%" mkdir "%DEST%"
copy /y "%SRC%" "%DEST%\%EXE%" >nul
if errorlevel 1 ( echo   ERROR: copy failed. & pause & exit /b 1 )

set "LNK_SM=%APPDATA%\Microsoft\Windows\Start Menu\Programs\MS2K Interface.lnk"
set "LNK_DT=%USERPROFILE%\Desktop\MS2K Interface.lnk"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$s=New-Object -ComObject WScript.Shell; foreach($p in @('%LNK_SM%','%LNK_DT%')){$k=$s.CreateShortcut($p); $k.TargetPath='%DEST%\%EXE%'; $k.WorkingDirectory='%DEST%'; $k.Description='Korg MS2000 Editor'; $k.Save()}" 2>nul

echo   Installed. Open "MS2K Interface" from the Start Menu or Desktop.
echo   (To uninstall: delete "%DEST%" and the two shortcuts.)
echo.
pause
exit /b 0
