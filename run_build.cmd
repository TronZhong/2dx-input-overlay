@echo off
setlocal

REM Canonical build entry for this repository.
REM Always build in the single primary directory: build/

cmake -S . -B build
if errorlevel 1 goto :fail

cmake --build build --config Release
if errorlevel 1 goto :fail

echo [OK] Build completed: build\Release\single_hid_monitor.exe
goto :end

:fail
echo [ERROR] Build failed.
exit /b 1

:end
exit /b 0
