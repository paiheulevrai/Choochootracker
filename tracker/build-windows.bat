@echo off
setlocal
cd /d "%~dp0"
echo Building Mobile Groove for Windows...

set "MSYS2_ROOT=C:\msys64"
if not exist "%MSYS2_ROOT%\usr\bin\bash.exe" (
    echo Error: MSYS2 was not found at %MSYS2_ROOT%.
    exit /b 1
)

if not exist "..\.tmp" mkdir "..\.tmp"
set "MOBILEGROOVE_TMP=%CD%\..\.tmp"
set "CHERE_INVOKING=1"

"%MSYS2_ROOT%\usr\bin\bash.exe" -lc "export PATH=/ucrt64/bin:/usr/bin; export TMP=$MOBILEGROOVE_TMP; export TEMP=$MOBILEGROOVE_TMP; make windows"
if %ERRORLEVEL% NEQ 0 (
    echo Error: Build failed.
    exit /b 1
)

echo Build complete: build\windows\chipnomad.exe
endlocal
