# Voice Transcription Application

A real-time voice-to-text transcription tool that works entirely offline for Windows, focusing on privacy, low-latency, and reliability.

## Project Status: Active Development

⚠️ **Note:** This project is currently in active development. The core architecture is in place, and the build system is now working correctly. We've successfully fixed the CMake configuration issues that were previously blocking progress.

**Current Focus:** 
- Implementing and testing the core audio processing pipeline
- Setting up basic C++/Python integration tests
- Developing the device enumeration and audio streaming functionality

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

## Troubleshooting Common Build Issues

~~1. **Target definition order issue:**~~
   - ~~Error message: `No TARGET 'voice_transcription_backend' has been created in this directory`~~
   - ~~Fix: The project maintainers are working on fixing this issue in the CMakeLists.txt file~~
   - **FIXED**: This issue has been resolved in the latest version of the CMakeLists.txt file.

2. **PortAudio linking problems:**
   - Error message: `Could not find PortAudio library`
   - Potential fix: Manually copy the correct version of PortAudio library (portaudio.lib or portaudio_x64.lib) to the libs/portaudio/lib directory

3. **Python binding issues:**
   - Error message: `ImportError: No module named 'voice_transcription_backend'`
   - Fix: Ensure the build process completed successfully and the .pyd file was copied to the src directory

4. **RapidJSON C++17 warnings:**
   - Warning: `std::iterator class template is deprecated in C++17`
   - Fix: These are non-critical warnings. If desired, you can add `_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING` to the target's compile definitions.

### Development Build

For developers working on the codebase:

1. **Manual fix for CMake issues:**
   - Open CMakeLists.txt and ensure the target definition (`pybind11_add_module`) appears before any references to the target
   - Move all `target_compile_definitions`, `target_link_libraries`, and `add_custom_command` calls after the target definition
   - Re-run CMake configuration and build

2. **Testing the C++/Python bridge:**
   - Create a simple test script in Python that imports the voice_transcription_backend module
   - Call a basic function to verify the binding works
   - Check for any import or runtime errors

## Development Roadmap

### Current Focus (MVP Components)
- Fix CMake build configuration issues
- Implement and test basic C++/Python integration
- Develop core audio pipeline (device enumeration, streaming, VAD, transcription)
- Create minimal end-to-end processing flow
- Implement basic shortcut functionality
- Connect backend to simple GUI

### Future Enhancements
- Enhance UI with better visual feedback
- Improve error handling and recovery mechanisms
- Add customizable dictation commands
- Optimize performance for lower latency
- Add support for different transcription models

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

## License

This project is licensed under the MIT License – see the LICENSE file for details.