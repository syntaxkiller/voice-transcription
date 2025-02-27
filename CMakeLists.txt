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

# Find or fetch pybind11
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/pybind11/CMakeLists.txt")
    message(STATUS "Using local pybind11")
    add_subdirectory(libs/pybind11)
else()
    # Try to find installed pybind11
    find_package(pybind11 CONFIG QUIET)
    
    if(NOT pybind11_FOUND)
        message(STATUS "pybind11 not found, downloading...")
        include(FetchContent)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v2.11.1
        )
        FetchContent_MakeAvailable(pybind11)
    else()
        message(STATUS "Found installed pybind11")
    endif()
endif()

# Find PortAudio
set(PORTAUDIO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/portaudio")
set(PORTAUDIO_INCLUDE_DIR "${PORTAUDIO_DIR}/include")

# Check if the header exists
if(NOT EXISTS "${PORTAUDIO_INCLUDE_DIR}/portaudio.h")
    message(FATAL_ERROR "PortAudio header not found. Run the setup_portaudio.bat script first.")
endif()

message(STATUS "Found PortAudio header: ${PORTAUDIO_INCLUDE_DIR}/portaudio.h")

# Check for library files - try multiple possible names
find_library(PORTAUDIO_LIBRARY 
    NAMES 
        portaudio
        portaudio_x64 
        portaudio_x86
        portaudio_static_x64
        portaudio_static_x86
    PATHS
        "${PORTAUDIO_DIR}/lib"
    NO_DEFAULT_PATH
)

if(NOT PORTAUDIO_LIBRARY)
    message(FATAL_ERROR "PortAudio library not found. Run the setup_portaudio.bat script and check for errors.")
endif()

message(STATUS "Found PortAudio library: ${PORTAUDIO_LIBRARY}")

# Add portaudio DLL to output directory for Windows
if(WIN32)
    # Find the DLL
    find_file(PORTAUDIO_DLL
        NAMES 
            portaudio.dll
            portaudio_x64.dll
            portaudio_x86.dll
        PATHS
            "${PORTAUDIO_DIR}/lib"
        NO_DEFAULT_PATH
    )
    
    if(PORTAUDIO_DLL)
        message(STATUS "Found PortAudio DLL: ${PORTAUDIO_DLL}")
        # Add command to copy the DLL to output directory
        add_custom_command(TARGET voice_transcription_backend POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "${PORTAUDIO_DLL}"
                "$<TARGET_FILE_DIR:voice_transcription_backend>"
        )
    else()
        message(WARNING "PortAudio DLL not found. Dynamic linking may fail at runtime.")
    endif()
endif()

# Check if the header exists
if(EXISTS "${PORTAUDIO_INCLUDE_DIR}/portaudio.h")
    set(PORTAUDIO_HEADER_FOUND TRUE)
    message(STATUS "Found PortAudio header: ${PORTAUDIO_INCLUDE_DIR}/portaudio.h")
else()
    message(FATAL_ERROR "PortAudio header not found. Run setup.bat first.")
endif()

# Check for library if not using mock
if(NOT USING_PORTAUDIO_MOCK)
    # Check multiple possible library names
    find_library(PORTAUDIO_LIBRARY 
        NAMES 
            portaudio_x64 
            portaudio_x86
            portaudio_static_x64
            portaudio_static_x86
            portaudio
        PATHS
            "${PORTAUDIO_DIR}/lib"
        NO_DEFAULT_PATH
    )
    
    if(PORTAUDIO_LIBRARY)
        set(PORTAUDIO_FOUND TRUE)
        message(STATUS "Found PortAudio library: ${PORTAUDIO_LIBRARY}")
    else()
        set(USING_PORTAUDIO_MOCK TRUE)
        message(STATUS "PortAudio library not found, will use mock implementation")
    endif()
endif()

# Vosk - allow mock or real implementation
set(VOSK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/vosk")
set(VOSK_INCLUDE_DIR "${VOSK_DIR}/include")

# Check if the header exists
if(EXISTS "${VOSK_INCLUDE_DIR}/vosk_api.h")
    set(VOSK_HEADER_FOUND TRUE)
    message(STATUS "Found Vosk header: ${VOSK_INCLUDE_DIR}/vosk_api.h")
else()
    message(FATAL_ERROR "Vosk header not found. Run setup.bat first.")
endif()

# Look for Vosk library
find_library(VOSK_LIBRARY
    NAMES
        vosk
        libvosk
    PATHS
        "${VOSK_DIR}/lib"
    NO_DEFAULT_PATH
)

if(VOSK_LIBRARY)
    set(VOSK_FOUND TRUE)
    add_definitions(-DUSE_REAL_VOSK)
    message(STATUS "Found Vosk library: ${VOSK_LIBRARY}")
else()
    set(VOSK_FOUND FALSE)
    message(STATUS "Using Vosk mock implementation")
endif()

# WebRTC VAD - Using our mock implementation
set(WEBRTC_VAD_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/webrtc_vad/include")
if(EXISTS "${WEBRTC_VAD_INCLUDE_DIR}/webrtc_vad.h")
    set(WEBRTC_VAD_FOUND TRUE)
    message(STATUS "Using WebRTC VAD mock implementation")
else()
    message(FATAL_ERROR "WebRTC VAD header not found. Run setup.bat first.")
endif()

# Set include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/include
    ${PORTAUDIO_INCLUDE_DIR}
    ${VOSK_INCLUDE_DIR}
    ${WEBRTC_VAD_INCLUDE_DIR}
)

# Make sure necessary source files exist
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/pybind_wrapper.cpp")
    message(FATAL_ERROR "Missing source file: src/bindings/pybind_wrapper.cpp. Run setup.bat first.")
endif()

# Backend source files
set(BACKEND_SOURCES
    src/backend/audio_stream.cpp
    src/backend/vosk_transcription_engine.cpp
    src/backend/keyboard_sim.cpp
    src/backend/window_manager.cpp
    src/backend/webrtc_vad.cpp
)

# Create pybind11 module
pybind11_add_module(voice_transcription_backend
    src/bindings/pybind_wrapper.cpp
    ${BACKEND_SOURCES}
)

# Link libraries
if(NOT USING_PORTAUDIO_MOCK AND PORTAUDIO_LIBRARY)
    target_link_libraries(voice_transcription_backend PRIVATE ${PORTAUDIO_LIBRARY})
endif()

if(VOSK_FOUND)
    target_link_libraries(voice_transcription_backend PRIVATE ${VOSK_LIBRARY})
endif()

# Conditionally link Windows-specific libraries
if(WIN32)
    target_link_libraries(voice_transcription_backend PRIVATE
        user32
        kernel32
    )
endif()

# Add definitions for Windows and mocks
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

# Option to enable testing
option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    # Set up GoogleTest
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/googletest/CMakeLists.txt")
        # Add GoogleTest directly
        add_subdirectory(libs/googletest)
    else()
        # Download and build Google Test if not found
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.13.0
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
    
    # Enable testing
    enable_testing()
    
    # Create test directory
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
        file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    endif()
    
    # Create audio stream test file if it doesn't exist
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/audio_stream_test.cpp")
        file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/tests/audio_stream_test.cpp" 
            "#include <gtest/gtest.h>\n"
            "#include \"audio_stream.h\"\n\n"
            "using namespace voice_transcription;\n\n"
            "// Test that the audio device enumeration works\n"
            "TEST(AudioStreamTest, DeviceEnumeration) {\n"
            "    auto devices = ControlledAudioStream::enumerate_devices();\n"
            "    // Even if no devices are present, this should return an empty list, not crash\n"
            "    ASSERT_NO_THROW(devices = ControlledAudioStream::enumerate_devices());\n"
            "}\n\n"
            "// More tests will be added here\n"
            "int main(int argc, char **argv) {\n"
            "    ::testing::InitGoogleTest(&argc, argv);\n"
            "    return RUN_ALL_TESTS();\n"
            "}\n"
        )
    endif()
    
    # Create test executable with the appropriate sources
    add_executable(audio_tests 
        tests/audio_stream_test.cpp
        src/backend/audio_stream.cpp 
        src/backend/webrtc_vad.cpp
    )
    
    # Add PortAudio mock if using it
    if(USING_PORTAUDIO_MOCK)
        target_sources(audio_tests PRIVATE src/backend/portaudio_mock.cpp)
    endif()
    
    target_include_directories(audio_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/backend/include
        ${PORTAUDIO_INCLUDE_DIR}
        ${WEBRTC_VAD_INCLUDE_DIR}
    )
    
    # Link libraries
    target_link_libraries(audio_tests PRIVATE
        gtest
        gtest_main
    )
    
    if(NOT USING_PORTAUDIO_MOCK AND PORTAUDIO_LIBRARY)
        target_link_libraries(audio_tests PRIVATE ${PORTAUDIO_LIBRARY})
    endif()
    
    if(WIN32)
        target_link_libraries(audio_tests PRIVATE
            user32
            kernel32
        )
    endif()
    
    # Add compile definitions
    if(USING_PORTAUDIO_MOCK)
        target_compile_definitions(audio_tests PRIVATE USE_PORTAUDIO_MOCK)
    endif()
    
    # Add Windows-specific definitions
    if(WIN32)
        target_compile_definitions(audio_tests PRIVATE 
            _WIN32_WINNT=0x0601
            NOMINMAX
            UNICODE
            _UNICODE
        )
    endif()
    
    # Add tests to CTest
    add_test(NAME audio_tests COMMAND audio_tests)
endif()

# Installation
install(TARGETS voice_transcription_backend
    LIBRARY DESTINATION src
    RUNTIME DESTINATION src
)