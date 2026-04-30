@echo off
setlocal

set ROOT=%~dp0
set SRC=%ROOT%src\main.cpp
set OUTDIR=%ROOT%build
set EXE=%OUTDIR%\FarFarWest Unlock all tool.exe
set CLANG=C:\Program Files\Windhawk\Compiler\bin\clang++.exe
set TARGET_BIN=C:\Program Files\Windhawk\Compiler\x86_64-w64-mingw32\bin
set LIBCXX=%TARGET_BIN%\libc++.dll
set LIBUNWIND=%TARGET_BIN%\libunwind.dll
set LIBWINPTHREAD=C:\Program Files\Windhawk\Compiler\x86_64-w64-mingw32\bin\libwinpthread-1.dll

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

"%CLANG%" --target=x86_64-w64-windows-gnu -std=c++20 -O2 -municode -mwindows ^
  "%SRC%" -o "%EXE%" -lcomctl32 -lshell32 -lshlwapi -ldwmapi -lbcrypt -lole32 -luuid -lmsimg32

if errorlevel 1 exit /b 1

copy /Y "%LIBCXX%" "%OUTDIR%\libc++.dll" >nul
copy /Y "%LIBUNWIND%" "%OUTDIR%\libunwind.dll" >nul
copy /Y "%LIBWINPTHREAD%" "%OUTDIR%\libwinpthread-1.dll" >nul
copy /Y "%LIBCXX%" "%OUTDIR%\libc++.whl" >nul
copy /Y "%LIBUNWIND%" "%OUTDIR%\libunwind.whl" >nul

echo Built %EXE%
