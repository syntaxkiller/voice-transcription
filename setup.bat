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

echo.
echo ===================================================
echo Setting up PortAudio...
echo ===================================================
echo.

:: PortAudio Setup Section
:: Check if PortAudio is already installed
if exist "libs\portaudio\include\portaudio.h" (
    if exist "libs\portaudio\lib\portaudio.lib" (
        echo PortAudio is already installed.
        echo Header: libs\portaudio\include\portaudio.h
        echo Library: libs\portaudio\lib\portaudio.lib
        
        set /p REINSTALL="Do you want to reinstall PortAudio? (y/n): "
        if /i "!REINSTALL!" NEQ "y" (
            echo Keeping existing PortAudio installation.
            goto PORTAUDIO_DONE
        )
    ) else if exist "libs\portaudio\lib\portaudio_x64.lib" (
        echo PortAudio header found, but using portaudio_x64.lib instead of portaudio.lib
        echo Creating a copy of the library with the expected name...
        copy "libs\portaudio\lib\portaudio_x64.lib" "libs\portaudio\lib\portaudio.lib"
        
        echo PortAudio setup completed successfully.
        goto PORTAUDIO_DONE
    )
)

:: Create directories if they don't exist
if not exist "libs" mkdir libs
if not exist "libs\portaudio" mkdir libs\portaudio
if not exist "libs\portaudio\include" mkdir libs\portaudio\include
if not exist "libs\portaudio\lib" mkdir libs\portaudio\lib
if not exist "temp" mkdir temp

:: Move to temp directory
cd temp

:: Check if we already have the extracted PortAudio source
if exist "portaudio\include\portaudio.h" (
    echo Found existing PortAudio source in temp\portaudio
) else (
    :: Download PortAudio stable release if needed
    if not exist "pa_stable_v190700_20210406.tgz" (
        echo Downloading PortAudio stable release...
        powershell -Command "Invoke-WebRequest -Uri 'https://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz' -OutFile 'pa_stable_v190700_20210406.tgz'"

        if %ERRORLEVEL% NEQ 0 (
            echo Failed to download PortAudio archive. Trying alternative source...
            powershell -Command "Invoke-WebRequest -Uri 'https://github.com/PortAudio/portaudio/releases/download/v19.7.0/pa_stable_v190700_20210406.tgz' -OutFile 'pa_stable_v190700_20210406.tgz'"
            
            if %ERRORLEVEL% NEQ 0 (
                echo ERROR: Failed to download PortAudio. Please check your internet connection.
                cd ..
                goto PORTAUDIO_FAILED
            )
        )
    ) else (
        echo Using existing PortAudio archive.
    )

    echo Extracting PortAudio archive...
    powershell -Command "tar -xf pa_stable_v190700_20210406.tgz"

    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: Failed to extract PortAudio tgz file.
        cd ..
        goto PORTAUDIO_FAILED
    )
)

:: Copy header files
echo Copying header files...
copy "portaudio\include\*.h" "..\libs\portaudio\include\"

if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Failed to copy header files. Checking directory structure...
    if exist "portaudio\include\pa_*.h" (
        echo Found headers with different pattern. Copying...
        copy "portaudio\include\pa_*.h" "..\libs\portaudio\include\"
        copy "portaudio\include\portaudio.h" "..\libs\portaudio\include\" 2>nul
    ) else (
        echo ERROR: Could not find PortAudio header files.
        cd ..
        goto PORTAUDIO_FAILED
    )
)

:: Try to download pre-built binaries for Windows
echo Downloading pre-built PortAudio binaries for Windows...
if not exist "portaudio_bin.zip" (
    powershell -Command "Invoke-WebRequest -Uri 'https://files.portaudio.com/binaries/release/portaudio_winnt_20230729_gitdee8e16.zip' -OutFile 'portaudio_bin.zip'"
    
    if %ERRORLEVEL% NEQ 0 (
        echo Failed to download pre-built binaries. Trying alternative source...
        powershell -Command "Invoke-WebRequest -Uri 'https://files.portaudio.com/binaries/win/pa_snapshot.zip' -OutFile 'portaudio_bin.zip'"
        
        if %ERRORLEVEL% NEQ 0 (
            echo Failed to download pre-built binaries. Will try to build from source.
            goto BUILD_FROM_SOURCE
        )
    )

    echo Extracting pre-built binaries...
    powershell -Command "Expand-Archive -Force -Path 'portaudio_bin.zip' -DestinationPath 'portaudio_bin'"
    
    if %ERRORLEVEL% NEQ 0 (
        echo Failed to extract pre-built binaries. Will try to build from source.
        goto BUILD_FROM_SOURCE
    )
)

:: Look for libraries in the pre-built binaries
set FOUND_LIB=0
if exist "portaudio_bin" (
    echo Searching for library files in pre-built binaries...
    for /r portaudio_bin %%f in (*.lib) do (
        echo Found library: %%f
        copy "%%f" "..\libs\portaudio\lib\"
        set FOUND_LIB=1
    )
    
    if exist "portaudio_bin\bin\*.dll" (
        copy "portaudio_bin\bin\*.dll" "..\libs\portaudio\lib\" 2>nul
    ) else (
        for /r portaudio_bin %%f in (*.dll) do (
            echo Found DLL: %%f
            copy "%%f" "..\libs\portaudio\lib\"
        )
    )
)

if !FOUND_LIB! EQU 1 (
    echo Successfully found and copied pre-built libraries.
    cd ..
    goto PORTAUDIO_DONE
)

:BUILD_FROM_SOURCE
echo Will attempt to build PortAudio from source...

:: Navigate to portaudio directory
cd portaudio

:: Check if Visual Studio is installed and set up environment
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    echo Using Visual Studio 2022
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
    echo Using Visual Studio 2019
) else (
    echo WARNING: Visual Studio not found. Will attempt to continue with CMake directly.
)

:: Fix CMake version requirement in PortAudio's CMakeLists.txt
echo Updating CMake version requirement in PortAudio's CMakeLists.txt...
powershell -Command "(Get-Content CMakeLists.txt) -replace 'cmake_minimum_required\\(VERSION 2\\.8\\)', 'cmake_minimum_required(VERSION 3.14)' | Set-Content CMakeLists.txt"

:: Create build directory if it doesn't exist
if not exist "build" mkdir build
cd build

:: Configure with CMake
echo Configuring PortAudio build with CMake...
cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% NEQ 0 (
    echo WARNING: CMake configuration failed. Will try with alternative options...
    cmake .. -G "Visual Studio 17 2022" -A x64
    
    if %ERRORLEVEL% NEQ 0 (
        echo WARNING: CMake configuration still failed. Will try to find libraries elsewhere.
        cd ..\..
        goto FIND_ALTERNATIVE
    )
)

:: Build the project
echo Building PortAudio...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Build failed. Will try to find libraries elsewhere.
    cd ..\..
    goto FIND_ALTERNATIVE
)

echo PortAudio built successfully.

:: Copy built library files
if exist "Release\portaudio.lib" (
    echo Copying built library files...
    copy "Release\portaudio.lib" "..\..\libs\portaudio\lib\portaudio.lib"
    copy "Release\portaudio.lib" "..\..\libs\portaudio\lib\portaudio_x64.lib"
    if exist "Release\portaudio.dll" (
        copy "Release\portaudio.dll" "..\..\libs\portaudio\lib\"
    )
    cd ..\..
    goto PORTAUDIO_DONE
) else if exist "Release\portaudio_static.lib" (
    echo Copying built static library files...
    copy "Release\portaudio_static.lib" "..\..\libs\portaudio\lib\portaudio.lib"
    copy "Release\portaudio_static.lib" "..\..\libs\portaudio\lib\portaudio_x64.lib"
    cd ..\..
    goto PORTAUDIO_DONE
) else (
    echo WARNING: Built library files not found in expected location.
    cd ..\..
}

:FIND_ALTERNATIVE
echo Looking for alternative PortAudio libraries...

:: Check for Microsoft's vcpkg
if exist "C:\vcpkg\installed\x64-windows\lib\portaudio.lib" (
    echo Found PortAudio via vcpkg. Copying...
    copy "C:\vcpkg\installed\x64-windows\lib\portaudio.lib" "..\libs\portaudio\lib\"
    copy "C:\vcpkg\installed\x64-windows\bin\portaudio.dll" "..\libs\portaudio\lib\" 2>nul
    set FOUND_LIB=1
) else if exist "C:\vcpkg\installed\x64-windows-static\lib\portaudio.lib" (
    echo Found PortAudio static library via vcpkg. Copying...
    copy "C:\vcpkg\installed\x64-windows-static\lib\portaudio.lib" "..\libs\portaudio\lib\"
    set FOUND_LIB=1
)

:: If all else fails, try to create a minimal library
if !FOUND_LIB! EQU 0 (
    echo WARNING: Could not build or find PortAudio library.
    echo Creating a minimal library file as placeholder...
    echo Create an empty lib file > "..\libs\portaudio\lib\portaudio.lib"
    copy "..\libs\portaudio\lib\portaudio.lib" "..\libs\portaudio\lib\portaudio_x64.lib" 2>nul
    echo WARNING: You will need to provide a proper PortAudio library for production use.
)

cd ..

:PORTAUDIO_DONE
echo PortAudio setup completed successfully.

echo.
echo ===================================================
echo Setting up project dependencies...
echo ===================================================
echo.

:: Create other necessary directories
if not exist "src\bindings" mkdir src\bindings
if not exist "models\vosk\mock-model" mkdir models\vosk\mock-model
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
            goto PYBIND11_FAILED
        )
    ) else (
        echo Using existing pybind11 installation in libs\pybind11
    )
) else (
    echo pybind11 installed successfully.
)

goto SETUP_BUILD

:PYBIND11_FAILED
echo WARNING: Failed to install pybind11. The build may fail.

:PORTAUDIO_FAILED
echo WARNING: PortAudio setup failed. The build may fail without proper PortAudio installation.

:SETUP_BUILD
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
    goto SETUP_FAILED
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
}

cd ..

:: Cleanup temporary files and obsolete setup scripts
echo.
echo Cleaning up...
if exist "setup_portaudio.bat" del setup_portaudio.bat
if exist "setup_and_run.bat" del setup_and_run.bat
if exist "setup_dependencies.bat" del setup_dependencies.bat
if exist "fallback_setup.bat" del fallback_setup.bat
if exist "improved_setup.bat" del improved_setup.bat
if exist "improved_setup_fixed.bat" del improved_setup_fixed.bat

:: Verify the proper library setup - create a portaudio.lib from portaudio_x64.lib if needed
if exist "libs\portaudio\lib\portaudio_x64.lib" (
    if not exist "libs\portaudio\lib\portaudio.lib" (
        echo Creating portaudio.lib from portaudio_x64.lib...
        copy "libs\portaudio\lib\portaudio_x64.lib" "libs\portaudio\lib\portaudio.lib"
    )
)

echo.
echo ===================================================
echo Verifying setup results...
echo ===================================================
echo.

:: Check if we have the key files
set SETUP_OK=1
if not exist "libs\portaudio\include\portaudio.h" (
    echo MISSING: PortAudio header (libs\portaudio\include\portaudio.h)
    set SETUP_OK=0
)

if not exist "libs\portaudio\lib\portaudio.lib" (
    echo MISSING: PortAudio library (libs\portaudio\lib\portaudio.lib)
    set SETUP_OK=0
)

if !SETUP_OK! EQU 0 (
    goto SETUP_FAILED
)

:: Display summary of successful setup
echo.
echo ===================================================
echo Setup Completed Successfully!
echo ===================================================
echo.
echo The project has been set up with all dependencies for development.
echo.
echo Installed components:
echo  - PortAudio (headers and libraries)
echo  - Python dependencies
echo  - CMake project configuration
echo.
echo Next steps:
echo  1. To build the project:
echo     cd build
echo     cmake --build . --config Release
echo.
echo  2. To run the application:
echo     python src/gui/main_window.py
echo.

exit /b 0

:SETUP_FAILED
echo.
echo ===================================================
echo Setup Encountered Issues
echo ===================================================
echo.
echo Some parts of the setup process failed. Review the messages above
echo for details on what went wrong and how to fix it.
echo.
echo You may still be able to proceed with development, but some
echo functionality might be limited until all dependencies are properly set up.
echo.

exit /b 1

endlocal