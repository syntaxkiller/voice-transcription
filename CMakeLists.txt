cmake_minimum_required(VERSION 3.14)
project(voice_transcription VERSION 0.1.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find Python
find_package(Python 3.8 COMPONENTS Interpreter Development REQUIRED)

# Find pybind11
find_package(pybind11 CONFIG REQUIRED)

# Find PortAudio
find_package(portaudio CONFIG REQUIRED)

# Set include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/include
    ${PORTAUDIO_INCLUDE_DIRS}
)

# Backend source files
set(BACKEND_SOURCES
    src/backend/audio_stream.cpp
    src/backend/vosk_transcription_engine.cpp
    src/backend/keyboard_sim.cpp
    src/backend/window_manager.cpp
    src/backend/webrtc_vad.cpp
    src/backend/vosk_api.cpp
)

# Create pybind11 module
pybind11_add_module(voice_transcription_backend
    src/bindings/pybind_wrapper.cpp
    ${BACKEND_SOURCES}
)

# Link libraries
target_link_libraries(voice_transcription_backend PRIVATE
    ${PORTAUDIO_LIBRARIES}
    # Add Vosk library here
    # Add WebRTC VAD library here
)

# Conditionally link Windows-specific libraries
if(WIN32)
    target_link_libraries(voice_transcription_backend PRIVATE
        user32
        kernel32
    )
endif()

# Add definitions for Windows
if(WIN32)
    target_compile_definitions(voice_transcription_backend PRIVATE 
        _WIN32_WINNT=0x0601  # Target Windows 7 or later
        NOMINMAX             # Avoid min/max macro conflicts
        UNICODE              # Use Unicode Windows API
        _UNICODE
    )
endif()

# Copy the compiled module to the Python package directory
add_custom_command(TARGET voice_transcription_backend POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:voice_transcription_backend>"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Installation
install(TARGETS voice_transcription_backend
    LIBRARY DESTINATION src
    RUNTIME DESTINATION src
)