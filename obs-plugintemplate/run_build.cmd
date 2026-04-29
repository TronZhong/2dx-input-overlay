@echo off
setlocal

cd /d %~dp0
if errorlevel 1 goto :fail

where cmake >nul 2>nul
if errorlevel 1 goto :fail_no_cmake

set "PRESET=windows-x64"
set "BUILD_DIR=build_x64"
set "FALLBACK_PRESET=windows-vs2022-x64"
set "FALLBACK_BUILD_DIR=build_vs2022_x64"
set "CONFIG=Release"
set "RELEASE_DIR=%CD%\release"

echo [INFO] Configure preset %PRESET%
cmake --preset %PRESET%
if errorlevel 1 (
	echo [WARN] Preset %PRESET% failed. Trying fallback preset %FALLBACK_PRESET%.
	set "PRESET=%FALLBACK_PRESET%"
	set "BUILD_DIR=%FALLBACK_BUILD_DIR%"
	cmake --preset %PRESET%
	if errorlevel 1 goto :fail
)

echo [INFO] Build preset %PRESET% with config %CONFIG%
cmake --build --preset %PRESET% --config %CONFIG%
if errorlevel 1 goto :fail

if exist "%RELEASE_DIR%" (
	echo [INFO] Clean release dir: %RELEASE_DIR%
	rmdir /s /q "%RELEASE_DIR%"
)

echo [INFO] Install files into: %RELEASE_DIR%
cmake --install "%BUILD_DIR%" --prefix "%RELEASE_DIR%" --config %CONFIG%
if errorlevel 1 goto :fail

echo [OK] OBS plugin build completed via preset %PRESET%.
echo [OK] Staged OBS layout: %RELEASE_DIR%
goto :end

:fail_no_cmake
echo [ERROR] CMake was not found in PATH. Please install CMake and reopen terminal.
exit /b 1

:fail
echo [ERROR] OBS plugin build failed.
exit /b 1

:end
exit /b 0
