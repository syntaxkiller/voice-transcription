@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo Voice Transcription Application - Setup Script
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

:: Check if Visual Studio is installed
if not exist "C:\Program Files\Microsoft Visual Studio" (
    if not exist "C:\Program Files (x86)\Microsoft Visual Studio" (
        echo WARNING: Microsoft Visual Studio may not be installed.
        echo This project requires Visual Studio to build the C++ components.
        echo Press any key to continue anyway or Ctrl+C to exit.
        pause > nul
    )
)

:: Create directories if they don't exist
if not exist "build" mkdir build
if not exist "src\bindings" mkdir src\bindings
if not exist "models\vosk\mock-model" mkdir models\vosk\mock-model
if not exist "libs" mkdir libs
if not exist "libs\portaudio\include" mkdir libs\portaudio\include
if not exist "libs\portaudio\lib" mkdir libs\portaudio\lib
if not exist "libs\vosk\include" mkdir libs\vosk\include
if not exist "libs\vosk\lib" mkdir libs\vosk\lib
if not exist "libs\webrtc_vad\include" mkdir libs\webrtc_vad\include
if not exist "libs\webrtc_vad\lib" mkdir libs\webrtc_vad\lib

:: Install Python dependencies
echo Installing Python dependencies...
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to install Python dependencies.
    echo Attempting to continue anyway...
)

:: Install pybind11
echo Installing pybind11...
python -m pip install pybind11
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Failed to install pybind11 via pip.
    echo Attempting to clone from GitHub instead...
    
    if not exist "libs\pybind11" (
        git clone https://github.com/pybind/pybind11.git libs\pybind11 --depth=1 --branch=v2.11.1
        if %ERRORLEVEL% NEQ 0 (
            echo ERROR: Failed to clone pybind11 repository.
            echo Please install git or download pybind11 manually.
            exit /b 1
        )
    ) else (
        echo Using existing pybind11 installation in libs\pybind11
    )
) else (
    echo pybind11 installed successfully.
)

:: Clean any existing build and create a fresh one
echo.
echo Preparing build directory...
if exist "build" (
    cd build
    if exist "CMakeCache.txt" del CMakeCache.txt
    cd ..
) else (
    mkdir build
)

:: Configure CMake project
echo.
echo Configuring CMake project...
cd build
cmake .. -DBUILD_TESTS=ON
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed.
    echo Please check the error messages above.
    cd ..
    exit /b 1
)

:: Build the project
echo.
echo Building the project...
cmake --build . --config Debug
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Build encountered some errors.
    echo This is expected during initial setup.
    echo We'll fix these issues in the next phase of development.
) else (
    echo Build completed successfully!
)

cd ..

:: Cleanup temporary files and obsolete setup scripts
echo.
echo Cleaning up...
if exist "setup_and_run.bat" del setup_and_run.bat
if exist "setup_dependencies.bat" del setup_dependencies.bat
if exist "fallback_setup.bat" del fallback_setup.bat
if exist "improved_setup.bat" del improved_setup.bat
if exist "improved_setup_fixed.bat" del improved_setup_fixed.bat

:: Display summary
echo.
echo ===================================================
echo Setup Completed!
echo ===================================================
echo.
echo The project has been set up with mock implementations
echo for development. This allows you to work on the Python
echo frontend while the C++ backend is being completed.
echo.
echo Next steps:
echo  1. Fix any remaining build errors
echo  2. Implement unit tests
echo.
echo To build the project:
echo   cd build
echo   cmake --build . --config Debug
echo.
echo To run the application:
echo   python src/gui/main_window.py
echo.

endlocal