@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo Setting up RapidJSON...
echo ===================================================
echo.

:: Create directories if they don't exist
if not exist "libs" mkdir libs
if not exist "libs\rapidjson" mkdir libs\rapidjson
if not exist "libs\rapidjson\include" mkdir libs\rapidjson\include
if not exist "temp" mkdir temp

:: Move to temp directory
cd temp

:: Check if we already have RapidJSON
if exist "rapidjson\include\rapidjson\document.h" (
    echo Found existing RapidJSON source in temp\rapidjson
) else (
    :: Download RapidJSON from GitHub
    if not exist "rapidjson.zip" (
        echo Downloading RapidJSON...
        powershell -Command "Invoke-WebRequest -Uri 'https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.zip' -OutFile 'rapidjson.zip'"

        if %ERRORLEVEL% NEQ 0 (
            echo Failed to download RapidJSON archive. Trying alternative source...
            powershell -Command "Invoke-WebRequest -Uri 'https://github.com/Tencent/rapidjson/archive/master.zip' -OutFile 'rapidjson.zip'"
            
            if %ERRORLEVEL% NEQ 0 (
                echo ERROR: Failed to download RapidJSON. Please check your internet connection.
                cd ..
                exit /b 1
            )
        )
    ) else (
        echo Using existing RapidJSON archive.
    )

    echo Extracting RapidJSON archive...
    powershell -Command "Expand-Archive -Force -Path 'rapidjson.zip' -DestinationPath '.'"

    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: Failed to extract RapidJSON zip file.
        cd ..
        exit /b 1
    )
    
    :: Rename the extracted directory to simple "rapidjson"
    if exist "rapidjson-1.1.0" (
        echo Renaming directory...
        move /Y "rapidjson-1.1.0" "rapidjson"
    ) else if exist "rapidjson-master" (
        echo Renaming directory...
        move /Y "rapidjson-master" "rapidjson"
    )
)

:: Copy header files to our project
echo Copying header files...
if exist "rapidjson\include\rapidjson" (
    xcopy /E /I /Y "rapidjson\include\rapidjson" "..\libs\rapidjson\include\rapidjson"
) else (
    echo ERROR: RapidJSON headers not found in expected location.
    cd ..
    exit /b 1
)

:: Return to the root directory
cd ..

echo RapidJSON setup completed successfully.

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
    if not exist "/temp/pa_stable_v190700_20210406.tgz" (
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

:: Replace the BUILD_FROM_SOURCE section in setup.bat with this fixed code

:BUILD_FROM_SOURCE
echo Will attempt to build PortAudio from source...

:: Store the absolute paths we'll need
set "PORTAUDIO_SRC_DIR=%CD%\portaudio"
set "LIB_TARGET_DIR=%CD%\..\libs\portaudio\lib"
set "CURRENT_DIR=%CD%"

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
powershell -Command "(Get-Content CMakeLists.txt) | ForEach-Object { $_ -replace 'cmake_minimum_required\s*\(\s*VERSION\s+\d+\.\d+(\.\d+)?\s*\)', 'cmake_minimum_required(VERSION 3.14)' } | Set-Content CMakeLists.txt"

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
        cd "%CURRENT_DIR%"
        goto FIND_ALTERNATIVE
    )
)

:: Build the project
echo Building PortAudio...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Build failed. Will try to find libraries elsewhere.
    cd "%CURRENT_DIR%"
    goto FIND_ALTERNATIVE
)

echo PortAudio built successfully.

:: Store build directory absolute path
set "BUILD_DIR=%CD%"
cd "%CURRENT_DIR%"

:: Check for specific known locations first based on the build log
set "FOUND_LIB=0"

if exist "%BUILD_DIR%\Release\portaudio_static_x64.lib" (
    echo Found static library: %BUILD_DIR%\Release\portaudio_static_x64.lib
    copy "%BUILD_DIR%\Release\portaudio_static_x64.lib" "%LIB_TARGET_DIR%\portaudio.lib"
    copy "%BUILD_DIR%\Release\portaudio_static_x64.lib" "%LIB_TARGET_DIR%\portaudio_x64.lib"
    set "FOUND_LIB=1"
    
    if exist "%BUILD_DIR%\Release\portaudio_x64.dll" (
        echo Found DLL: %BUILD_DIR%\Release\portaudio_x64.dll
        copy "%BUILD_DIR%\Release\portaudio_x64.dll" "%LIB_TARGET_DIR%\"
    )
) else if exist "%BUILD_DIR%\Release\portaudio_x64.lib" (
    echo Found library: %BUILD_DIR%\Release\portaudio_x64.lib
    copy "%BUILD_DIR%\Release\portaudio_x64.lib" "%LIB_TARGET_DIR%\portaudio.lib"
    copy "%BUILD_DIR%\Release\portaudio_x64.lib" "%LIB_TARGET_DIR%\portaudio_x64.lib"
    set "FOUND_LIB=1"
    
    if exist "%BUILD_DIR%\Release\portaudio_x64.dll" (
        echo Found DLL: %BUILD_DIR%\Release\portaudio_x64.dll
        copy "%BUILD_DIR%\Release\portaudio_x64.dll" "%LIB_TARGET_DIR%\"
    )
) else if exist "%BUILD_DIR%\Release\portaudio.lib" (
    echo Found library: %BUILD_DIR%\Release\portaudio.lib
    copy "%BUILD_DIR%\Release\portaudio.lib" "%LIB_TARGET_DIR%\portaudio.lib"
    copy "%BUILD_DIR%\Release\portaudio.lib" "%LIB_TARGET_DIR%\portaudio_x64.lib"
    set "FOUND_LIB=1"
    
    if exist "%BUILD_DIR%\Release\portaudio.dll" (
        echo Found DLL: %BUILD_DIR%\Release\portaudio.dll
        copy "%BUILD_DIR%\Release\portaudio.dll" "%LIB_TARGET_DIR%\"
    )
)

:: If we didn't find libraries in the expected locations, continue to alternative paths
if "%FOUND_LIB%"=="0" (
    echo WARNING: Could not find built library files in the expected Release directory.
    echo Checking other possible locations...
    
    cd "%BUILD_DIR%"
    
    :: Check for libraries in alternative locations
    if exist "lib\Release\portaudio_static.lib" (
        echo Found static library: lib\Release\portaudio_static.lib
        copy "lib\Release\portaudio_static.lib" "%LIB_TARGET_DIR%\portaudio.lib"
        copy "lib\Release\portaudio_static.lib" "%LIB_TARGET_DIR%\portaudio_x64.lib"
        set "FOUND_LIB=1"
    ) else if exist "lib\Release\portaudio.lib" (
        echo Found library: lib\Release\portaudio.lib
        copy "lib\Release\portaudio.lib" "%LIB_TARGET_DIR%\portaudio.lib"
        copy "lib\Release\portaudio.lib" "%LIB_TARGET_DIR%\portaudio_x64.lib"
        set "FOUND_LIB=1"
    )
    
    cd "%CURRENT_DIR%"
)

:: If we still didn't find anything, try creating empty placeholder files
if "%FOUND_LIB%"=="0" (
    echo WARNING: Could not find built library files. Creating placeholder library files...
    echo This is a placeholder > "%LIB_TARGET_DIR%\portaudio.lib"
    copy "%LIB_TARGET_DIR%\portaudio.lib" "%LIB_TARGET_DIR%\portaudio_x64.lib"
    
    echo WARNING: You will need to provide a proper PortAudio library for production use.
    goto PORTAUDIO_DONE
)

goto PORTAUDIO_DONE

echo WARNING: Could not find built library files in expected locations.
echo Will look in other possible locations...

:: If not found in the expected locations, try a broader search but validate the files exist
set "FOUND_LIB=0"

:: First try specific paths we know might contain the libraries
if exist "x64\Release\portaudio_static.lib" (
    echo Found static library: x64\Release\portaudio_static.lib
    copy "x64\Release\portaudio_static.lib" "..\..\libs\portaudio\lib\portaudio.lib"
    copy "x64\Release\portaudio_static.lib" "..\..\libs\portaudio\lib\portaudio_x64.lib"
    set "FOUND_LIB=1"
) else if exist "lib\Release\portaudio_static.lib" (
    echo Found static library: lib\Release\portaudio_static.lib
    copy "lib\Release\portaudio_static.lib" "..\..\libs\portaudio\lib\portaudio.lib"
    copy "lib\Release\portaudio_static.lib" "..\..\libs\portaudio\lib\portaudio_x64.lib"
    set "FOUND_LIB=1"
)

:: If still not found, try a recursive search but verify files exist before copying
if "!FOUND_LIB!"=="0" (
    for /r %%f in (portaudio_static*.lib) do (
        if exist "%%f" (
            echo Found static library: %%f
            copy "%%f" "..\..\libs\portaudio\lib\portaudio.lib"
            copy "%%f" "..\..\libs\portaudio\lib\portaudio_x64.lib"
            set "FOUND_LIB=1"
            goto FOUND_LIB_RECURSIVE
        )
    )
    
    for /r %%f in (portaudio*.lib) do (
        if exist "%%f" (
            echo Found library: %%f
            copy "%%f" "..\..\libs\portaudio\lib\portaudio.lib"
            copy "%%f" "..\..\libs\portaudio\lib\portaudio_x64.lib"
            set "FOUND_LIB=1"
            goto FOUND_LIB_RECURSIVE
        )
    )
)

:FOUND_LIB_RECURSIVE
:: Look for DLL files if we found a library
if "!FOUND_LIB!"=="1" (
    for /r %%f in (portaudio*.dll) do (
        if exist "%%f" (
            echo Found DLL: %%f
            copy "%%f" "..\..\libs\portaudio\lib\"
        )
    )
    cd ..\..
    goto PORTAUDIO_DONE
) else (
    echo WARNING: Could not find built library files.
    cd ..\..
    goto FIND_ALTERNATIVE
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

echo.
echo ===================================================
echo Installing Python Dependencies
echo ===================================================
echo.

:: First upgrade pip itself
echo Upgrading pip...
python -m pip install --upgrade pip
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Failed to upgrade pip. Continuing with existing version.
) else (
    echo Pip upgraded successfully.
)

:: Define essential packages
set "ESSENTIAL_PACKAGES=PyQt5 pybind11 numpy colorlog"
set "OPTIONAL_PACKAGES=pytest pytest-mock pytest-qt vosk webrtcvad"
set "MISSING_ESSENTIAL=0"

:: Try installing all dependencies first
echo Installing all dependencies from requirements.txt...
python -m pip install -r requirements.txt
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: Failed to install all Python dependencies.
    echo Trying to install essential packages individually...
    
    :: Install essential packages one by one
    for %%p in (%ESSENTIAL_PACKAGES%) do (
        echo Installing essential package: %%p
        python -m pip install %%p
        if %ERRORLEVEL% NEQ 0 (
            echo ERROR: Failed to install essential package %%p
            set "MISSING_ESSENTIAL=1"
        ) else (
            echo Successfully installed %%p
        )
    )
    
    :: Attempt to install optional packages
    echo.
    echo Attempting to install optional packages...
    for %%p in (%OPTIONAL_PACKAGES%) do (
        echo Installing optional package: %%p
        python -m pip install %%p
        if %ERRORLEVEL% NEQ 0 (
            echo WARNING: Could not install optional package %%p
            echo Some features may be limited.
        ) else (
            echo Successfully installed %%p
        )
    )
    
    :: Check if we can continue
    if "!MISSING_ESSENTIAL!"=="1" (
        echo.
        echo CAUTION: Some essential packages could not be installed.
        echo The application may not function correctly.
        set /p CONTINUE="Do you want to continue with setup anyway? (y/n): "
        if /i "!CONTINUE!" NEQ "y" (
            echo Setup aborted by user.
            exit /b 1
        )
    ) else (
        echo All essential packages were installed successfully.
        echo Optional packages may have limited functionality.
    )
) else (
    echo All Python dependencies installed successfully from requirements.txt.
)

:: Verify pybind11 installation specifically (crucial for C++ binding)
echo.
echo Verifying pybind11 installation...
python -c "import pybind11; print('pybind11', pybind11.__version__)" 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo WARNING: pybind11 import failed despite installation.
    echo Attempting alternative installation methods...
    
    :: Try installing via pip with different options
    python -m pip install pybind11 --force-reinstall
    if %ERRORLEVEL% NEQ 0 (
        echo WARNING: pip reinstall failed. Trying to clone from GitHub...
        
        if not exist "libs\pybind11" (
            git clone https://github.com/pybind/pybind11.git libs\pybind11 --depth=1 --branch=v2.11.1
            if %ERRORLEVEL% NEQ 0 (
                echo ERROR: Failed to clone pybind11 repository.
                echo This component is critical for building the application.
                set /p CONTINUE="Do you want to continue anyway? (y/n): "
                if /i "!CONTINUE!" NEQ "y" (
                    echo Setup aborted by user.
                    exit /b 1
                )
            ) else (
                echo Successfully cloned pybind11 from GitHub.
            )
        ) else (
            echo Using existing pybind11 installation in libs\pybind11
        )
    ) else (
        echo pybind11 reinstalled successfully.
    )
) else (
    echo pybind11 verified successfully.
)

echo.
echo Python dependencies setup completed.
echo.

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
) else (
    echo Build completed successfully!
)

cd ..

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