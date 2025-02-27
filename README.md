# Voice Transcription Application

A real-time voice-to-text transcription tool that works entirely offline for Windows, focusing on privacy, low-latency, and reliability.

## Overview

This application enables real-time speech transcription with a simple keyboard shortcut toggle. It captures audio from your microphone, processes it through an offline speech recognition engine (Vosk), and outputs the text either through simulated keypresses or the clipboard.

## Key Features

- **Privacy-focused:** Works completely offline with no cloud dependencies
- **Keyboard shortcut toggle:** Quickly start/stop transcription using a customizable hotkey
- **Voice Activity Detection:** Efficiently processes speech while ignoring silence
- **Dictation commands:** Support for punctuation and formatting commands (e.g., "period", "new line")
- **Multiple output methods:** Choose between simulated keypresses or clipboard
- **User-friendly interface:** Clean GUI with status indicators and device selection
- **Robust error handling:** Graceful fallbacks and clear error messages

## Dependencies

### Required Software

- Python 3.8+ – For running the application GUI
- Visual Studio 2019/2022 – With C++ workload for building the C++ backend
- CMake 3.14+ – For building the C++ components
- Git – For fetching dependencies

### Libraries

#### Python Dependencies

- PyQt5 – GUI framework
- pybind11 – C++/Python interoperability
- pytest – Testing framework
- colorlog – Enhanced logging

#### C++ Dependencies

- PortAudio – Audio capture
- Vosk – Speech recognition
- WebRTC VAD – Voice activity detection
- RapidJSON – JSON parsing for transcription results
- pybind11 – C++/Python binding
- GoogleTest – C++ testing framework

## Setup Instructions

### Quick Setup

1. **Clone this repository:**

       git clone https://github.com/yourusername/voice-transcription.git
       cd voice-transcription

2. **Run the setup script** (this will install necessary dependencies and build the project):

       setup.bat

   The setup script includes robust error handling with fallback mechanisms that will:
   
   - Install all required Python dependencies
   - Set up PortAudio with multiple fallback methods
   - Build the C++ backend components
   - Properly configure the environment for development

3. **Launch the application:**

       python src/gui/main_window.py

### Manual Setup

If you need to perform a manual setup:

1. **Install Python dependencies:**

       pip install -r requirements.txt

2. **Set up C++ dependencies:**

   - Download and install PortAudio
   - Download and include RapidJSON headers
   - Set up Vosk and its model files

3. **Configure and build with CMake:**

       mkdir build
       cd build
       cmake ..
       cmake --build . --config Release

4. **Ensure the built module is copied to the source directory:**

       copy build\bin\Release\voice_transcription_backend.pyd src\

5. **Launch the application:**

       python src/gui/main_window.py

## Project Structure

Copy/voice-transcription/  
├── src/  (Source code)  
│   ├── gui/  (Python GUI components)  
│   ├── backend/  (C++ backend)  
│   │   ├── include/  (Header files)  
│   │   └── ...  (Implementation files)  
│   ├── bindings/  (pybind11 bindings)  
│   ├── utils/  (Utility modules)  
│   └── config/  (Configuration files)  
├── models/  (Speech recognition models)  
│   └── vosk/  (Vosk speech models)  
├── libs/  (External libraries)  
├── tests/  (Unit tests)  
├── build/  (Build output)  
├── CMakeLists.txt  (C++ build configuration)  
├── requirements.txt  (Python dependencies)  
└── setup.bat  (Setup script with robust error handling)

## Using the Application

- Select an audio input device from the dropdown menu.
- Set your preferred hotkey with the "Change Shortcut" button.
- Start transcription by clicking the button or using your shortcut.
- Speak into your microphone and watch as your speech is converted to text.
- Use dictation commands like "period", "new line", etc. for punctuation and formatting.

## Dictation Commands

- "period" → .
- "comma" → ,
- "question mark" → ?
- "exclamation point" → !
- "new line" → (simulates Enter key)
- "new paragraph" → (simulates double Enter)
- "space" → (inserts a space)
- "control enter" → (simulates Ctrl+Enter)
- "all caps" → (toggles Caps Lock on)
- "caps lock" → (toggles Caps Lock off)

## Development Notes

### Current State

- The basic architecture is in place with a working Python GUI and C++ backend
- A functional Vosk transcription engine has been implemented
- Core functionality (audio capture, VAD, transcription, output) is implemented
- JSON parsing with RapidJSON for transcription results is implemented
- Robust setup process with improved error handling for dependencies
- Working PortAudio integration with multiple fallback methods for setup

### Next Steps

- Comprehensive testing of the Vosk transcription implementation
- Enhancing the user interface for better feedback on transcription quality
- Implementing configurable transcription parameters
- Optimizing performance for lower latency

## Building for Development

### Debug Build

       cd build
       cmake ..
       cmake --build . --config Debug

### Release Build

       cd build
       cmake ..
       cmake --build . --config Release

## Known Issues

- The application is Windows-only due to platform-specific APIs for keypress simulation and hotkey capturing.
- Voice Activity Detection may need tuning for different environments and microphones.
- Large speech models can consume significant memory; consider using smaller models on lower-end hardware.

## License

This project is licensed under the MIT License – see the LICENSE file for details.