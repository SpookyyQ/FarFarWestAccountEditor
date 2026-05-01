@echo off
setlocal

set ROOT=%~dp0
set BUILD_DIR=%ROOT%build
set RELEASE_ROOT=%ROOT%release
set APP_DIR=%RELEASE_ROOT%\FarFarWest Unlock all tool
set ZIP_PATH=%RELEASE_ROOT%\FarFarWest Unlock all tool.zip

call "%ROOT%build.bat"
if errorlevel 1 exit /b 1

if exist "%APP_DIR%" rmdir /S /Q "%APP_DIR%"
if exist "%ZIP_PATH%" del /F /Q "%ZIP_PATH%"
if not exist "%RELEASE_ROOT%" mkdir "%RELEASE_ROOT%"
mkdir "%APP_DIR%"

copy /Y "%BUILD_DIR%\FarFarWest Unlock all tool.exe" "%APP_DIR%\" >nul
copy /Y "%BUILD_DIR%\WebView2Loader.dll" "%APP_DIR%\" >nul
copy /Y "%BUILD_DIR%\libc++.whl" "%APP_DIR%\" >nul
copy /Y "%BUILD_DIR%\libunwind.whl" "%APP_DIR%\" >nul
copy /Y "%BUILD_DIR%\libwinpthread-1.dll" "%APP_DIR%\" >nul
copy /Y "%ROOT%README_RELEASE.txt" "%APP_DIR%\README.txt" >nul
if exist "%APP_DIR%\ui" rmdir /S /Q "%APP_DIR%\ui"
xcopy /E /I /Y "%BUILD_DIR%\ui" "%APP_DIR%\ui\" >nul

powershell -NoProfile -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; if (Test-Path '%ZIP_PATH%') { Remove-Item -LiteralPath '%ZIP_PATH%' -Force }; [System.IO.Compression.ZipFile]::CreateFromDirectory('%APP_DIR%', '%ZIP_PATH%')"
if errorlevel 1 exit /b 1

echo Packaged %APP_DIR%
echo Created %ZIP_PATH%
