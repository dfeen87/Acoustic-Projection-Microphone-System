@echo off
REM APM REST API Server Launcher for Windows
REM Starts the FastAPI backend with global node access

setlocal

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."

cd /d "%PROJECT_ROOT%"

REM Check if Python is available
where python >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: Python is not installed or not in PATH
    exit /b 1
)

REM Check if uvicorn is available
python -c "import uvicorn" >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Installing backend dependencies...
    pip install -r backend\requirements.txt
)

REM Set defaults
if not defined APM_API_HOST set APM_API_HOST=0.0.0.0
if not defined APM_API_PORT set APM_API_PORT=8080

REM Parse command line arguments
set "RELOAD="
:parse_args
if "%~1"=="" goto :run
if "%~1"=="--reload" set "RELOAD=--reload"
if "%~1"=="--host" set "APM_API_HOST=%~2" & shift
if "%~1"=="--port" set "APM_API_PORT=%~2" & shift
shift
goto :parse_args

:run
echo Starting APM REST API Server...
echo Host: %APM_API_HOST%
echo Port: %APM_API_PORT%
echo.

python backend\main.py --host %APM_API_HOST% --port %APM_API_PORT% %RELOAD%
