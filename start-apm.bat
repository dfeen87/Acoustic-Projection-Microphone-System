@echo off
REM start-apm.bat - Windows launcher script
REM Production-ready APM system startup

setlocal enabledelayedexpansion

echo.
echo ========================================
echo APM System Launcher (Windows)
echo ========================================
echo.

REM Check Node.js
where node >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Node.js not found!
    echo Please install Node.js from https://nodejs.org/
    pause
    exit /b 1
)

REM Check CMake
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake not found!
    echo Please install CMake from https://cmake.org/
    pause
    exit /b 1
)

REM Check if dependencies are installed
if not exist "launcher\node_modules" (
    echo [INFO] Installing dependencies...
    cd launcher
    call npm install
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] Failed to install dependencies
        cd ..
        pause
        exit /b 1
    )
    cd ..
)

REM Check if backend is built
set BACKEND_FOUND=0
if exist "apm_backend.exe" set BACKEND_FOUND=1
if exist "build\apm_backend.exe" set BACKEND_FOUND=1
if exist "build\Release\apm_backend.exe" set BACKEND_FOUND=1
if exist "build\Debug\apm_backend.exe" set BACKEND_FOUND=1

if !BACKEND_FOUND! equ 0 (
    echo [INFO] Backend not found, building...
    
    if not exist "build" mkdir build
    
    echo [INFO] Configuring CMake...
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] CMake configuration failed
        pause
        exit /b 1
    )
    
    echo [INFO] Building backend...
    cmake --build build --config Release
    if %ERRORLEVEL% neq 0 (
        echo [ERROR] Build failed
        pause
        exit /b 1
    )
)

REM Start the system
echo.
echo [INFO] Starting APM System...
echo.
cd launcher
call npm start

REM If npm start exits, return to original directory
cd ..

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] APM System exited with error code %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)

endlocal
