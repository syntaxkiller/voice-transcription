@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo Voice Transcription - Setting up dependencies
echo ===================================================
echo.

:: Create libraries directory structure
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

:: Download and extract PortAudio
if not exist "libs\portaudio\lib\portaudio_x64.dll" (
    echo Downloading PortAudio...
    powershell -Command "Invoke-WebRequest -Uri 'http://files.portaudio.com/archives/pa_stable_v190700_20210406.tgz' -OutFile 'portaudio.tgz'"
    
    echo Extracting PortAudio...
    powershell -Command "tar -xf portaudio.tgz"
    
    echo Building PortAudio...
    cd portaudio
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release
    
    echo Copying PortAudio files...
    copy "Release\portaudio_x64.dll" "..\..\libs\portaudio\lib\"
    copy "Release\portaudio_x64.lib" "..\..\libs\portaudio\lib\"
    cd ..
    xcopy "include\*.*" "..\libs\portaudio\include\" /Y
    
    cd ..
    rmdir /S /Q portaudio
    del portaudio.tgz
)

:: Download and extract Vosk
if not exist "libs\vosk\lib\vosk.dll" (
    echo Downloading Vosk...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/alphacep/vosk-api/releases/download/v0.3.45/vosk-win64-0.3.45.zip' -OutFile 'vosk-win64.zip'"
    
    echo Extracting Vosk...
    powershell -Command "Expand-Archive -Path 'vosk-win64.zip' -DestinationPath 'vosk-temp' -Force"
    
    echo Copying Vosk files...
    copy "vosk-temp\*.dll" "libs\vosk\lib\"
    copy "vosk-temp\*.lib" "libs\vosk\lib\"
    copy "vosk-temp\*.h" "libs\vosk\include\"
    
    rmdir /S /Q vosk-temp
    del vosk-win64.zip
)

:: WebRTC VAD (using a pre-built binary for simplicity)
if not exist "libs\webrtc_vad\lib\webrtc_vad.dll" (
    echo Creating WebRTC VAD mock implementations...
    
    :: Create header file
    echo #pragma once > libs\webrtc_vad\include\webrtc_vad.h
    echo. >> libs\webrtc_vad\include\webrtc_vad.h
    echo #include ^<cstdint^> >> libs\webrtc_vad\include\webrtc_vad.h
    echo #include ^<cstddef^> >> libs\webrtc_vad\include\webrtc_vad.h
    echo. >> libs\webrtc_vad\include\webrtc_vad.h
    echo #ifdef __cplusplus >> libs\webrtc_vad\include\webrtc_vad.h
    echo extern "C" { >> libs\webrtc_vad\include\webrtc_vad.h
    echo #endif >> libs\webrtc_vad\include\webrtc_vad.h
    echo. >> libs\webrtc_vad\include\webrtc_vad.h
    echo void* WebRtcVad_Create^(^); >> libs\webrtc_vad\include\webrtc_vad.h
    echo int WebRtcVad_Init^(void* handle^); >> libs\webrtc_vad\include\webrtc_vad.h
    echo int WebRtcVad_Free^(void* handle^); >> libs\webrtc_vad\include\webrtc_vad.h
    echo int WebRtcVad_set_mode^(void* handle, int mode^); >> libs\webrtc_vad\include\webrtc_vad.h
    echo int WebRtcVad_Process^(void* handle, int sample_rate_hz, const int16_t* audio_frame, size_t frame_length^); >> libs\webrtc_vad\include\webrtc_vad.h
    echo. >> libs\webrtc_vad\include\webrtc_vad.h
    echo #ifdef __cplusplus >> libs\webrtc_vad\include\webrtc_vad.h
    echo } >> libs\webrtc_vad\include\webrtc_vad.h
    echo #endif >> libs\webrtc_vad\include\webrtc_vad.h
    
    :: Create simple implementation file for testing
    echo #include "webrtc_vad.h" > src\backend\webrtc_vad.cpp
    echo #include ^<cstdlib^> >> src\backend\webrtc_vad.cpp
    echo #include ^<cstring^> >> src\backend\webrtc_vad.cpp
    echo #include ^<ctime^> >> src\backend\webrtc_vad.cpp
    echo. >> src\backend\webrtc_vad.cpp
    echo void* WebRtcVad_Create^(^) { >> src\backend\webrtc_vad.cpp
    echo     static bool seeded = false; >> src\backend\webrtc_vad.cpp
    echo     if ^(!seeded^) { >> src\backend\webrtc_vad.cpp
    echo         std::srand^(static_cast^<unsigned int^>^(std::time^(nullptr^)^)^); >> src\backend\webrtc_vad.cpp
    echo         seeded = true; >> src\backend\webrtc_vad.cpp
    echo     } >> src\backend\webrtc_vad.cpp
    echo     return new int^(1^); >> src\backend\webrtc_vad.cpp
    echo } >> src\backend\webrtc_vad.cpp
    echo. >> src\backend\webrtc_vad.cpp
    echo int WebRtcVad_Init^(void* handle^) { return 0; } >> src\backend\webrtc_vad.cpp
    echo int WebRtcVad_Free^(void* handle^) { delete static_cast^<int*^>^(handle^); return 0; } >> src\backend\webrtc_vad.cpp
    echo int WebRtcVad_set_mode^(void* handle, int mode^) { return mode ^>= 0 ^&^& mode ^<= 3 ? 0 : -1; } >> src\backend\webrtc_vad.cpp
    echo int WebRtcVad_Process^(void* handle, int sample_rate_hz, const int16_t* audio_frame, size_t frame_length^) { >> src\backend\webrtc_vad.cpp
    echo     if ^(^!audio_frame ^|^| frame_length == 0^) return -1; >> src\backend\webrtc_vad.cpp
    echo     int random = std::rand^(^) %% 100; >> src\backend\webrtc_vad.cpp
    echo     return random ^> 30 ? 1 : 0; >> src\backend\webrtc_vad.cpp
    echo } >> src\backend\webrtc_vad.cpp
)

:: Setup Google Test
if not exist "libs\googletest" (
    echo Downloading Google Test...
    git clone https://github.com/google/googletest.git libs\googletest --depth=1 --branch=v1.13.0
)

echo ===================================================
echo Dependencies setup complete!
echo ===================================================
echo.
echo The following libraries are now available:
echo  - PortAudio
echo  - Vosk
echo  - WebRTC VAD (mock implementation)
echo  - Google Test
echo.
echo You can now build the project with CMake.
echo.

endlocal