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
set CLANG=C:\Program Files\Windhawk\Compiler\bin\clang++.exe
set TARGET_BIN=C:\Program Files\Windhawk\Compiler\x86_64-w64-mingw32\bin
set LIBCXX=%TARGET_BIN%\libc++.dll
set LIBUNWIND=%TARGET_BIN%\libunwind.dll
set LIBWINPTHREAD=C:\Program Files\Windhawk\Compiler\x86_64-w64-mingw32\bin\libwinpthread-1.dll

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

"%CLANG%" --target=x86_64-w64-windows-gnu -std=c++20 -O2 -municode -mwindows ^
  -I"%WV2INC%" "%SRC%" "%WV2LIB%" -o "%EXE%" -lcomctl32 -lshell32 -lshlwapi -ldwmapi -lbcrypt -lole32 -loleaut32 -luuid -lmsimg32

if errorlevel 1 exit /b 1

copy /Y "%LIBCXX%" "%OUTDIR%\libc++.dll" >nul
copy /Y "%LIBUNWIND%" "%OUTDIR%\libunwind.dll" >nul
copy /Y "%LIBWINPTHREAD%" "%OUTDIR%\libwinpthread-1.dll" >nul
copy /Y "%WV2DLL%" "%OUTDIR%\WebView2Loader.dll" >nul
if exist "%OUTDIR%\ui" rmdir /S /Q "%OUTDIR%\ui"
xcopy /E /I /Y "%UIDIR%" "%OUTDIR%\ui\" >nul

echo Built %EXE%
