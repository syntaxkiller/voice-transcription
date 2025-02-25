@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo Voice Transcription Application - Setup and Run
echo ===================================================
echo.

:: Check for Python installation
python --version > nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Python not found. Please install Python 3.8 or later.
    echo Visit https://www.python.org/downloads/ to download and install Python.
    exit /b 1
)

:: Check for CMake installation
cmake --version > nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake not found. Please install CMake.
    echo Visit https://cmake.org/download/ to download and install CMake.
    exit /b 1
)

:: Create directories if they don't exist
if not exist "build" mkdir build
if not exist "models\vosk\mock-model" mkdir "models\vosk\mock-model"

:: Install Python dependencies
echo Installing Python dependencies...
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to install Python dependencies.
    exit /b 1
)

:: Configure and build C++ backend
echo Building C++ backend...
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed.
    cd ..
    exit /b 1
)

cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed.
    cd ..
    exit /b 1
)
cd ..

:: Copy backend to src directory if it's not there already
if exist "build\bin\Release\voice_transcription_backend.pyd" (
    echo Copying backend module to src directory...
    copy "build\bin\Release\voice_transcription_backend.pyd" "src\" > nul
) else if exist "build\lib\Release\voice_transcription_backend.pyd" (
    echo Copying backend module to src directory...
    copy "build\lib\Release\voice_transcription_backend.pyd" "src\" > nul
) else (
    echo WARNING: Could not find the compiled backend module.
    echo Please check the build output for errors.
)

:: Run unit tests
echo.
echo Running unit tests...
python test_application.py
set TEST_RESULT=%ERRORLEVEL%
if %TEST_RESULT% NEQ 0 (
    echo WARNING: Some tests failed.
) else (
    echo All tests passed successfully.
)

:: Ask user if they want to run the application
echo.
set /p RUN_APP=Would you like to run the application now? (Y/N): 
if /i "%RUN_APP%"=="Y" (
    echo Starting Voice Transcription Application...
    python src\gui\main_window.py
)

echo.
echo Setup and testing complete.
if %TEST_RESULT% NEQ 0 (
    echo WARNING: Some tests failed, but you can still try running the application.
    echo Check test output for details.
)

endlocal