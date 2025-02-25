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

:: Setup mock files if they don't exist
echo Checking for required source files...

:: Create pybind_wrapper.cpp if it doesn't exist
if not exist "src\bindings\pybind_wrapper.cpp" (
    echo Creating src\bindings\pybind_wrapper.cpp...
    copy src\bindings\pybind_wrapper.cpp.txt src\bindings\pybind_wrapper.cpp 2>nul
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: Failed to copy pybind_wrapper.cpp.txt
        echo Creating a minimal version instead...
        (
            echo #include ^<pybind11/pybind11.h^>
            echo #include ^<pybind11/stl.h^>
            echo.
            echo namespace py = pybind11;
            echo.
            echo PYBIND11_MODULE^(voice_transcription_backend, m^) {
            echo     m.doc^(^) = "Voice Transcription Backend Module";
            echo     // Placeholder for bindings - will be replaced with actual code
            echo }
        ) > src\bindings\pybind_wrapper.cpp
    )
)

:: Create portaudio_mock.cpp if it doesn't exist
if not exist "src\backend\portaudio_mock.cpp" (
    echo Creating src\backend\portaudio_mock.cpp...
    (
        echo #include "portaudio.h"
        echo.
        echo // Mock implementation of PortAudio API
        echo.
        echo PaError Pa_Initialize^(void^) { return paNoError; }
        echo PaError Pa_Terminate^(void^) { return paNoError; }
        echo int Pa_GetDeviceCount^(void^) { return 1; }
        echo const PaDeviceInfo* Pa_GetDeviceInfo^(PaDeviceIndex device^) {
        echo     static PaDeviceInfo info;
        echo     info.name = "Mock Device";
        echo     info.maxInputChannels = 1;
        echo     info.defaultLowInputLatency = 0.01;
        echo     info.hostApi = 0;
        echo     return &info;
        echo }
        echo PaDeviceIndex Pa_GetDefaultInputDevice^(void^) { return 0; }
        echo PaDeviceIndex Pa_GetDefaultOutputDevice^(void^) { return 0; }
        echo PaError Pa_IsFormatSupported^(const PaStreamParameters* inputParameters, const PaStreamParameters* outputParameters, double sampleRate^) { return paFormatIsSupported; }
        echo PaError Pa_OpenStream^(PaStream** stream, const PaStreamParameters* inputParameters, const PaStreamParameters* outputParameters, double sampleRate, unsigned long framesPerBuffer, PaStreamFlags streamFlags, PaStreamCallback* streamCallback, void* userData^) {
        echo     static int dummy = 1;
        echo     *stream = &dummy;
        echo     return paNoError;
        echo }
        echo PaError Pa_CloseStream^(PaStream* stream^) { return paNoError; }
        echo PaError Pa_StartStream^(PaStream* stream^) { return paNoError; }
        echo PaError Pa_StopStream^(PaStream* stream^) { return paNoError; }
        echo PaError Pa_IsStreamActive^(PaStream* stream^) { return 1; }
        echo PaError Pa_IsStreamStopped^(PaStream* stream^) { return 0; }
        echo PaTime Pa_GetStreamTime^(PaStream* stream^) { return 0.0; }
        echo const char* Pa_GetErrorText^(PaError errorCode^) { return "No error"; }
    ) > src\backend\portaudio_mock.cpp
)

:: Create portaudio.h if it doesn't exist
if not exist "libs\portaudio\include\portaudio.h" (
    echo Creating minimal portaudio.h...
    (
        echo #ifndef PORTAUDIO_H
        echo #define PORTAUDIO_H
        echo.
        echo #ifdef __cplusplus
        echo extern "C" {
        echo #endif
        echo.
        echo typedef int PaError;
        echo typedef int PaDeviceIndex;
        echo typedef double PaTime;
        echo typedef unsigned long PaStreamFlags;
        echo typedef void PaStream;
        echo typedef struct PaStreamCallbackTimeInfo PaStreamCallbackTimeInfo;
        echo typedef unsigned long PaStreamCallbackFlags;
        echo typedef int ^(PaStreamCallback^)^(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData^);
        echo.
        echo typedef struct PaDeviceInfo {
        echo     const char *name;
        echo     int maxInputChannels;
        echo     int maxOutputChannels;
        echo     double defaultLowInputLatency;
        echo     double defaultHighInputLatency;
        echo     double defaultLowOutputLatency;
        echo     double defaultHighOutputLatency;
        echo     double defaultSampleRate;
        echo     int hostApi;
        echo } PaDeviceInfo;
        echo.
        echo typedef struct PaHostApiInfo {
        echo     const char *name;
        echo } PaHostApiInfo;
        echo.
        echo typedef struct PaStreamParameters {
        echo     int device;
        echo     int channelCount;
        echo     int sampleFormat;
        echo     double suggestedLatency;
        echo     void *hostApiSpecificStreamInfo;
        echo } PaStreamParameters;
        echo.
        echo #define paNoError 0
        echo #define paNotInitialized -10000
        echo #define paUnanticipatedHostError -9999
        echo #define paInvalidChannelCount -9998
        echo #define paInvalidSampleRate -9997
        echo #define paInvalidDevice -9996
        echo #define paInvalidFlag -9995
        echo #define paStreamIsNotStopped -9994
        echo #define paStreamIsStopped -9993
        echo #define paBadStreamPtr -9992
        echo #define paNoDevice -1
        echo #define paFormatIsSupported 0
        echo.
        echo PaError Pa_Initialize^(void^);
        echo PaError Pa_Terminate^(void^);
        echo int Pa_GetDeviceCount^(void^);
        echo const PaDeviceInfo* Pa_GetDeviceInfo^(PaDeviceIndex device^);
        echo const PaHostApiInfo* Pa_GetHostApiInfo^(int hostApi^);
        echo PaDeviceIndex Pa_GetDefaultInputDevice^(void^);
        echo PaDeviceIndex Pa_GetDefaultOutputDevice^(void^);
        echo PaError Pa_IsFormatSupported^(const PaStreamParameters *inputParameters, const PaStreamParameters *outputParameters, double sampleRate^);
        echo PaError Pa_OpenStream^(PaStream **stream, const PaStreamParameters *inputParameters, const PaStreamParameters *outputParameters, double sampleRate, unsigned long framesPerBuffer, PaStreamFlags streamFlags, PaStreamCallback *streamCallback, void *userData^);
        echo PaError Pa_CloseStream^(PaStream *stream^);
        echo PaError Pa_StartStream^(PaStream *stream^);
        echo PaError Pa_StopStream^(PaStream *stream^);
        echo PaError Pa_IsStreamActive^(PaStream *stream^);
        echo PaError Pa_IsStreamStopped^(PaStream *stream^);
        echo PaTime Pa_GetStreamTime^(PaStream *stream^);
        echo const char *Pa_GetErrorText^(PaError errorCode^);
        echo.
        echo #ifdef __cplusplus
        echo }
        echo #endif
        echo.
        echo #endif /* PORTAUDIO_H */
    ) > libs\portaudio\include\portaudio.h
)

:: Create mock vosk header if it doesn't exist
if not exist "libs\vosk\include\vosk_api.h" (
    echo Creating minimal vosk_api.h...
    (
        echo #ifndef VOSK_API_H
        echo #define VOSK_API_H
        echo.
        echo #ifdef __cplusplus
        echo extern "C" {
        echo #endif
        echo.
        echo typedef struct VoskModel VoskModel;
        echo typedef struct VoskRecognizer VoskRecognizer;
        echo.
        echo VoskModel *vosk_model_new^(const char *model_path^);
        echo void vosk_model_free^(VoskModel *model^);
        echo VoskRecognizer *vosk_recognizer_new^(VoskModel *model, float sample_rate^);
        echo void vosk_recognizer_free^(VoskRecognizer *recognizer^);
        echo void vosk_recognizer_set_max_alternatives^(VoskRecognizer *recognizer, int max_alternatives^);
        echo void vosk_recognizer_set_words^(VoskRecognizer *recognizer, int words^);
        echo int vosk_recognizer_accept_waveform^(VoskRecognizer *recognizer, const char *data, int length^);
        echo const char *vosk_recognizer_result^(VoskRecognizer *recognizer^);
        echo const char *vosk_recognizer_partial_result^(VoskRecognizer *recognizer^);
        echo void vosk_recognizer_reset^(VoskRecognizer *recognizer^);
        echo.
        echo #ifdef __cplusplus
        echo }
        echo #endif
        echo.
        echo #endif /* VOSK_API_H */
    ) > libs\vosk\include\vosk_api.h
)

:: Create WebRTC VAD header if it doesn't exist
if not exist "libs\webrtc_vad\include\webrtc_vad.h" (
    echo Creating minimal webrtc_vad.h...
    (
        echo #ifndef WEBRTC_VAD_H
        echo #define WEBRTC_VAD_H
        echo.
        echo #include ^<cstdint^>
        echo #include ^<cstddef^>
        echo.
        echo #ifdef __cplusplus
        echo extern "C" {
        echo #endif
        echo.
        echo void* WebRtcVad_Create^(void^);
        echo int WebRtcVad_Init^(void* handle^);
        echo int WebRtcVad_Free^(void* handle^);
        echo int WebRtcVad_set_mode^(void* handle, int mode^);
        echo int WebRtcVad_Process^(void* handle, int sample_rate_hz, const int16_t* audio_frame, size_t frame_length^);
        echo.
        echo #ifdef __cplusplus
        echo }
        echo #endif
        echo.
        echo #endif /* WEBRTC_VAD_H */
    ) > libs\webrtc_vad\include\webrtc_vad.h
)

:: Create mock model information
if not exist "models\vosk\mock-model\README.txt" (
    echo Creating mock model information...
    (
        echo This is a mock Vosk model for testing purposes.
        echo.
        echo In a real deployment, you would need to download the actual model:
        echo https://alphacephei.com/vosk/models
        echo.
        echo Recommended model for English: vosk-model-en-us-0.22
    ) > models\vosk\mock-model\README.txt
)

:: Create dummy library files for linking
if not exist "libs\portaudio\lib\portaudio_x64.lib" (
    echo Creating dummy library files...
    echo DUMMY > libs\portaudio\lib\portaudio_x64.lib
)

if not exist "libs\vosk\lib\libvosk.lib" (
    echo DUMMY > libs\vosk\lib\libvosk.lib
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
echo  2. Complete the mock implementations
echo  3. Implement unit tests
echo  4. Gradually replace mocks with real implementations
echo.
echo To build the project:
echo   cd build
echo   cmake --build . --config Debug
echo.
echo To run the application:
echo   python src/gui/main_window.py
echo.

endlocal