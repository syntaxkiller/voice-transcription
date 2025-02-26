# Voice Transcription Application

A real-time voice-to-text transcription tool that works entirely offline for Windows, focusing on privacy, low-latency, and reliability.

---

## Overview

This application enables real-time speech transcription with a simple keyboard shortcut toggle. It captures audio from your microphone, processes it through an offline speech recognition engine (Vosk), and outputs the text either through simulated keypresses or the clipboard.

---

## Key Features

- **Privacy-focused:** Works completely offline with no cloud dependencies  
- **Keyboard shortcut toggle:** Quickly start/stop transcription using a customizable hotkey  
- **Voice Activity Detection:** Efficiently processes speech while ignoring silence  
- **Dictation commands:** Support for punctuation and formatting commands (e.g., "period", "new line")  
- **Multiple output methods:** Choose between simulated keypresses or clipboard  
- **User-friendly interface:** Clean GUI with status indicators and device selection  

---

## Dependencies

### Required Software

- **Python 3.8+** – For running the application GUI  
- **Visual Studio 2019/2022** – With C++ workload for building the C++ backend  
- **CMake 3.14+** – For building the C++ components  
- **Git** – For fetching dependencies  

### Libraries

#### Python Dependencies

- **PyQt5** – GUI framework  
- **pybind11** – C++/Python interoperability  
- **pytest** – Testing framework  
- **colorlog** – Enhanced logging  

#### C++ Dependencies

- **PortAudio** – Audio capture  
- **Vosk** – Speech recognition  
- **WebRTC VAD** – Voice activity detection  
- **pybind11** – C++/Python binding  
- **GoogleTest** – C++ testing framework  

---

## Setup Instructions

### Quick Setup

1. **Clone this repository:**

   ~~~bash
   git clone https://github.com/yourusername/voice-transcription.git
   cd voice-transcription
   ~~~

2. **Run the setup script (this will install necessary dependencies and build the project):**

   ~~~bash
   setup.bat
   ~~~

3. **Launch the application:**

   ~~~bash
   python src/gui/main_window.py
   ~~~

### Manual Setup

If the automatic setup script doesn't work, follow these steps:

1. **Install Python dependencies:**

   ~~~bash
   pip install -r requirements.txt
   ~~~

2. **Create necessary directories:**

   ~~~bash
   mkdir -p build
   mkdir -p libs/portaudio/include
   mkdir -p libs/vosk/include
   mkdir -p libs/webrtc_vad/include
   mkdir -p models/vosk/mock-model
   ~~~

3. **Configure and build with CMake:**

   ~~~bash
   cd build
   cmake ..
   cmake --build . --config Debug
   ~~~

4. **Ensure the built module is copied to the source directory:**

   ~~~bash
   copy build\bin\Debug\voice_transcription_backend.pyd src\
   ~~~

5. **Launch the application:**

   ~~~bash
   python src/gui/main_window.py
   ~~~

---

## Project Structure
/voice-transcription/ 
├── src/ # Source code
│ ├── gui/ # Python GUI components
│ ├── backend/ # C++ backend
│ │ ├── include/ # Header files
│ │ └── ... # Implementation files
│ ├── bindings/ # pybind11 bindings
│ ├── utils/ # Utility modules
│ └── config/ # Configuration files
├── models/ # Speech recognition models
│ └── vosk/ # Vosk speech models
├── libs/ # External libraries
├── tests/ # Unit tests
├── build/ # Build output
├── CMakeLists.txt # C++ build configuration
├── requirements.txt # Python dependencies
└── setup.bat # Setup script
---

## Using the Application

1. Select an audio input device from the dropdown menu.  
2. Set your preferred hotkey with the **"Change Shortcut"** button.  
3. Start transcription by clicking the button or using your shortcut.  
4. Speak into your microphone and watch as your speech is converted to text.  
5. Use dictation commands like "period", "new line", etc. for punctuation and formatting.  

---

## Dictation Commands

- **"period"** → `.`  
- **"comma"** → `,`  
- **"question mark"** → `?`  
- **"exclamation point"** → `!`  
- **"new line"** → (simulates Enter key)  
- **"new paragraph"** → (simulates double Enter)  
- **"space"** → (inserts a space)  
- **"control enter"** → (simulates Ctrl+Enter)  
- **"all caps"** → (toggles Caps Lock on)  
- **"caps lock"** → (toggles Caps Lock off)  

---

## Development Notes

### Current State

- The basic architecture is in place with a working Python GUI and C++ backend framework.  
- Mock implementations are used in some places to facilitate development.  
- Core functionality (audio capture, VAD, transcription, output) is implemented.  

### Next Steps

- Ensure all dependencies are properly installed.  
- Complete the Vosk model integration with proper error handling.  
- Implement comprehensive testing (unit tests and integration tests).  
- Optimize performance, especially for latency-critical paths.  
- Enhance the user interface with better feedback.  

---

## Building for Development

### Debug Build

~~~bash
cd build
cmake ..
cmake --build . --config Debug
~~~

### Release Build

~~~bash
cd build
cmake ..
cmake --build . --config Release
~~~

---

## Known Issues

- Initial setup may require adjusting paths depending on your system configuration.  
- Vosk model download is not yet automated in the setup script.  
- Currently, the app is Windows-only due to platform-specific APIs for keypress simulation and hotkey capturing.  
- Voice Activity Detection may need tuning for different environments and microphones.  

---

## License

This project is licensed under the MIT License – see the [LICENSE](./LICENSE) file for details.