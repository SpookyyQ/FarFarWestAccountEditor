@echo off
setlocal

set ROOT=%~dp0
set SRC=%ROOT%src\main.cpp
set OUTDIR=%ROOT%build
set EXE=%OUTDIR%\FarFarWestSaveStudio.exe
set CLANG=C:\Program Files\Windhawk\Compiler\bin\clang++.exe

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

"%CLANG%" --target=x86_64-w64-windows-gnu -std=c++20 -O2 -municode -mwindows ^
  "%SRC%" -o "%EXE%" -lcomctl32 -lshell32 -lshlwapi -ldwmapi -lbcrypt -lole32 -luuid -lmsimg32

if errorlevel 1 exit /b 1

echo Built %EXE%
