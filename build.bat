@echo off
setlocal

set ROOT=%~dp0
set SRC=%ROOT%src\main.cpp
set OUTDIR=%ROOT%build
set EXE=%OUTDIR%\FarFarWest Unlock all tool.exe
set UIDIR=%ROOT%assets\ui
set WV2INC=%ROOT%third_party_webview2\build\native\include
set WV2LIB=%ROOT%third_party_webview2\build\native\x64\WebView2Loader.dll.lib
set WV2DLL=%ROOT%third_party_webview2\build\native\x64\WebView2Loader.dll

rem --- Locate toolchain root ---
rem Priority: LLVM_MINGW env var -> common install paths -> Windhawk fallback

set TOOLCHAIN_ROOT=

if defined LLVM_MINGW (
    if exist "%LLVM_MINGW%\bin\clang++.exe" set TOOLCHAIN_ROOT=%LLVM_MINGW%
)

if not defined TOOLCHAIN_ROOT if exist "C:\llvm-mingw\bin\clang++.exe" set TOOLCHAIN_ROOT=C:\llvm-mingw
if not defined TOOLCHAIN_ROOT if exist "C:\tools\llvm-mingw\bin\clang++.exe" set TOOLCHAIN_ROOT=C:\tools\llvm-mingw
if not defined TOOLCHAIN_ROOT if exist "%LOCALAPPDATA%\llvm-mingw\bin\clang++.exe" set TOOLCHAIN_ROOT=%LOCALAPPDATA%\llvm-mingw
if not defined TOOLCHAIN_ROOT if exist "C:\Program Files\LLVM-MinGW\bin\clang++.exe" set TOOLCHAIN_ROOT=C:\Program Files\LLVM-MinGW
if not defined TOOLCHAIN_ROOT if exist "C:\Program Files\Windhawk\Compiler\bin\clang++.exe" set TOOLCHAIN_ROOT=C:\Program Files\Windhawk\Compiler

if not defined TOOLCHAIN_ROOT (
    echo.
    echo ERROR: No compatible toolchain found.
    echo.
    echo Download LLVM-MinGW from:
    echo   https://github.com/mstorsjo/llvm-mingw/releases
    echo.
    echo Extract the ZIP to a folder and either:
    echo   a) Set the LLVM_MINGW environment variable to that folder, or
    echo   b) Extract to one of these paths:
    echo        C:\llvm-mingw
    echo        C:\tools\llvm-mingw
    echo.
    exit /b 1
)

set CLANG=%TOOLCHAIN_ROOT%\bin\clang++.exe
set TARGET_BIN=%TOOLCHAIN_ROOT%\x86_64-w64-mingw32\bin
set LIBCXX=%TARGET_BIN%\libc++.dll
set LIBUNWIND=%TARGET_BIN%\libunwind.dll
set LIBWINPTHREAD=%TARGET_BIN%\libwinpthread-1.dll

echo Using toolchain: %TOOLCHAIN_ROOT%

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

powershell -ExecutionPolicy Bypass -File "%ROOT%make_icon_res.ps1"
if errorlevel 1 exit /b 1

"%CLANG%" --target=x86_64-w64-windows-gnu -std=c++20 -O2 -municode -mwindows ^
  -I"%WV2INC%" "%SRC%" "%WV2LIB%" "%OUTDIR%\app.res" -o "%EXE%" -lcomctl32 -lshell32 -lshlwapi -ldwmapi -lbcrypt -lole32 -loleaut32 -luuid -lmsimg32

if errorlevel 1 exit /b 1

copy /Y "%LIBCXX%" "%OUTDIR%\libc++.dll" >nul
copy /Y "%LIBUNWIND%" "%OUTDIR%\libunwind.dll" >nul
copy /Y "%LIBWINPTHREAD%" "%OUTDIR%\libwinpthread-1.dll" >nul
copy /Y "%WV2DLL%" "%OUTDIR%\WebView2Loader.dll" >nul
if exist "%OUTDIR%\ui" rmdir /S /Q "%OUTDIR%\ui"
xcopy /E /I /Y "%UIDIR%" "%OUTDIR%\ui\" >nul

echo Built %EXE%
