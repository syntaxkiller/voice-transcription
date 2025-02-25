@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo Voice Transcription - Fallback Setup Script
echo ===================================================
echo This script will download pre-compiled binaries for required dependencies.
echo.

:: Create necessary directories
if not exist "libs" mkdir libs
if not exist "libs\portaudio" mkdir libs\portaudio
if not exist "libs\portaudio\include" mkdir libs\portaudio\include
if not exist "libs\portaudio\lib" mkdir libs\portaudio\lib

if not exist "libs\vosk" mkdir libs\vosk
if not exist "libs\vosk\include" mkdir libs\vosk\include
if not exist "libs\vosk\lib" mkdir libs\vosk\lib

if not exist "libs\webrtc_vad" mkdir libs\webrtc_vad
if not exist "libs\webrtc_vad\include" mkdir libs\webrtc_vad\include
if not exist "libs\webrtc_vad\lib" mkdir libs\webrtc_vad\lib

if not exist "libs\pybind11" mkdir libs\pybind11

if not exist "models\vosk\mock-model" mkdir models\vosk\mock-model

:: Create a temporary directory for downloads
if not exist "temp" mkdir temp
cd temp

echo.
echo Step 1: Setting up PortAudio
echo ---------------------------------------------------
powershell -Command "Invoke-WebRequest -Uri 'https://github.com/PortAudio/portaudio/releases/download/pa_stable_v190700_20210406/portaudio-binaries-msvc64-2022-01-27.zip' -OutFile 'portaudio.zip'"
powershell -Command "Expand-Archive -Path 'portaudio.zip' -DestinationPath 'portaudio-extract' -Force"

echo Looking for PortAudio files...
for /r "portaudio-extract" %%i in (*.dll) do (
    echo Found DLL: %%i
    copy "%%i" "..\libs\portaudio\lib\"
)

for /r "portaudio-extract" %%i in (*.lib) do (
    echo Found LIB: %%i
    copy "%%i" "..\libs\portaudio\lib\"
)

for /r "portaudio-extract" %%i in (portaudio.h) do (
    echo Found header: %%i
    copy "%%i" "..\libs\portaudio\include\"
)

echo.
echo Step 2: Setting up Vosk
echo ---------------------------------------------------
powershell -Command "Invoke-WebRequest -Uri 'https://github.com/alphacep/vosk-api/releases/download/v0.3.45/vosk-win64-0.3.45.zip' -OutFile 'vosk.zip'"
powershell -Command "Expand-Archive -Path 'vosk.zip' -DestinationPath 'vosk-extract' -Force"

echo Looking for Vosk files...
for /r "vosk-extract" %%i in (*.dll) do (
    echo Found DLL: %%i
    copy "%%i" "..\libs\vosk\lib\"
)

for /r "vosk-extract" %%i in (*.lib) do (
    echo Found LIB: %%i
    copy "%%i" "..\libs\vosk\lib\"
)

for /r "vosk-extract" %%i in (*.h) do (
    echo Found header: %%i
    copy "%%i" "..\libs\vosk\include\"
)

echo.
echo Step 3: Setting up mock WebRTC VAD
echo ---------------------------------------------------
echo Creating mock WebRTC VAD header...
echo #pragma once > ..\libs\webrtc_vad\include\webrtc_vad.h
echo #include ^<cstdint^> >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo #include ^<cstddef^> >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo. >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo #ifdef __cplusplus >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo extern "C" { >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo #endif >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo. >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo void* WebRtcVad_Create^(^); >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo int WebRtcVad_Init^(void* handle^); >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo int WebRtcVad_Free^(void* handle^); >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo int WebRtcVad_set_mode^(void* handle, int mode^); >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo int WebRtcVad_Process^(void* handle, int sample_rate_hz, const int16_t* audio_frame, size_t frame_length^); >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo. >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo #ifdef __cplusplus >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo } >> ..\libs\webrtc_vad\include\webrtc_vad.h
echo #endif >> ..\libs\webrtc_vad\include\webrtc_vad.h

echo.
echo Step 4: Setting up PyBind11
echo ---------------------------------------------------
echo Cloning PyBind11 repository...
cd ..
if not exist "libs\pybind11\CMakeLists.txt" (
    git clone https://github.com/pybind/pybind11.git libs\pybind11 --depth=1 --branch=v2.11.1
) else (
    echo PyBind11 is already set up.
)

echo.
echo Step 5: Creating a mock model directory
echo ---------------------------------------------------
echo > models\vosk\mock-model\README.txt
echo This is a mock model directory used for testing. >> models\vosk\mock-model\README.txt
echo The actual model should be downloaded separately. >> models\vosk\mock-model\README.txt

echo.
echo Clean up temporary files
echo ---------------------------------------------------
rmdir /S /Q temp

echo.
echo ===================================================
echo Setup completed!
echo ===================================================
echo.
echo The following libraries have been set up:
echo - PortAudio
echo - Vosk
echo - Mock WebRTC VAD
echo - PyBind11
echo.
echo You can now build the project with CMake:
echo   mkdir build
echo   cd build
echo   cmake ..
echo   cmake --build . --config Release
echo.

endlocal