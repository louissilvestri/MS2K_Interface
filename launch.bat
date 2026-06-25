@echo off
rem Launch the MS2K_Interface editor (statically linked - no DLLs needed).
setlocal
set "EXE=%~dp0build\MS2K_Interface_artefacts\MS2K_Interface.exe"

if not exist "%EXE%" (
    echo MS2K_Interface.exe not found at:
    echo   %EXE%
    echo.
    echo Build it first:  cmake --build build --target MS2K_Interface
    echo.
    pause
    exit /b 1
)

start "" "%EXE%"
endlocal
