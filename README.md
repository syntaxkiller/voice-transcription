# Voice Transcription Application

A real-time voice-to-text transcription tool that works entirely offline for Windows, focusing on privacy, low-latency, and reliability.

![Status: Active Development](https://img.shields.io/badge/Status-Active%20Development-blue)

## Key Features

- **🔒 Complete Privacy:** Works entirely offline with no cloud dependencies
- **⌨️ Keyboard Shortcut Toggle:** Start/stop transcription with a customizable hotkey (default: Ctrl+Shift+T)
- **🗣️ Smart Speech Detection:** Automatically detects when you're speaking using Voice Activity Detection
- **🔤 Dictation Commands:** Use commands like "period", "new line", "question mark" for punctuation and formatting
- **✍️ Flexible Output:** Choose between simulated keypresses or clipboard output
- **🪟 User-Friendly Interface:** Clean GUI with device selection and status indicators
- **⚡ Background Model Loading:** Non-blocking model initialization with progress feedback
- **💾 Robust Buffer Management:** Stable audio streaming with memory efficiency
- **🛡️ Robust Error Handling:** Graceful recovery from issues like device disconnections

## Screenshots

*(Coming soon: Screenshots of application in action)*

## Quick Start Guide

### Installation

1. **Download the latest release**
   - Download the latest release from the [Releases](https://github.com/yourusername/voice-transcription/releases) page
   - Or clone this repository to build from source

2. **Run the setup script**
   ```
   setup.bat
   ```
   This script will:
   - Install required Python dependencies
   - Set up PortAudio for audio capture
   - Configure the build environment
   - Build the C++ backend components

3. **Launch the application**
   ```
   python src/gui/main_window.py
   ```

### Basic Usage

1. **Select your microphone** from the device dropdown
2. **Set your preferred hotkey** using the "Change Shortcut" button (default is Ctrl+Shift+T)
3. **Press your hotkey** or click "Start Transcription" to begin transcribing
   - The first time you start, the speech model will load in the background (progress is shown in the status bar)
4. **Speak naturally** and watch as your speech is converted to text in your active application
5. **Press your hotkey again** to stop transcription

### Dictation Commands

Use these spoken commands to add punctuation and formatting:

| Say this... | To get this... |
|-------------|----------------|
| "period" | . |
| "comma" | , |
| "question mark" | ? |
| "exclamation point" | ! |
| "new line" | (Enter key) |
| "new paragraph" | (Double Enter) |
| "space" | (Space) |
| "control enter" | (Ctrl+Enter) |
| "all caps" | (Caps Lock on) |
| "caps lock" | (Caps Lock off) |

## Troubleshooting

### Common Issues

#### Application won't start
- Check that you've run `setup.bat` successfully
- Verify Python 3.8+ is installed and in your PATH
- Make sure all dependencies are installed (`pip install -r requirements.txt`)

#### No microphones showing
- Check that your microphone is properly connected
- Verify that Windows recognizes your microphone
- Some microphones may not support the required 16000 Hz sample rate

#### Transcription not working
- Ensure your microphone is selected in the dropdown
- Check that your microphone is not being used by another application
- Verify that you're in a reasonably quiet environment
- Try adjusting your microphone volume in Windows settings

#### Long model loading time
- The Vosk speech recognition model is loaded in the background
- Loading progress is displayed in the status bar
- First-time loading can take 10-30 seconds depending on your system
- The application remains responsive during loading

#### Text not appearing in applications
- Make sure the target application has focus when transcribing
- Try using clipboard output instead (in Options)
- Some applications may block simulated keypresses for security reasons

### Error Messages

| Error | Solution |
|-------|----------|
| "Audio device disconnected" | Reconnect your microphone or select a different one |
| "Failed to load model" | Check that the Vosk model files are in the correct location |
| "Failed to register hotkey" | Try a different keyboard shortcut |
| "Output error" | Check if the target application is still active |
| "Loading model..." | This is normal - wait for the model to load completely |

## System Requirements

- **Operating System:** Windows 10/11
- **Processor:** Any modern x86_64 CPU
- **Memory:** Minimum 4GB RAM (8GB recommended)
- **Disk Space:** 500MB for application + models
- **Audio:** Working microphone that supports 16000 Hz sampling
- **Prerequisites:** 
  - Python 3.8 or later
  - Visual Studio 2019/2022 with C++ workload (for building from source)
  - CMake 3.14 or later (for building from source)

## Building from Source

### Complete Build

For a full build from source:

1. **Install prerequisites:**
   - Python 3.8+
   - Visual Studio 2019/2022 with C++ workload
   - CMake 3.14+
   - Git

2. **Clone the repository:**
   ```
   git clone https://github.com/yourusername/voice-transcription.git
   cd voice-transcription
   ```

3. **Run the setup script:**
   ```
   setup.bat
   ```

4. **Build the project:**
   ```
   cd build
   cmake --build . --config Release
   ```

5. **Run the application:**
   ```
   python src/gui/main_window.py
   ```

### Development Build

For active development:

1. **Configure with CMake in Debug mode:**
   ```
   cd build
   cmake .. -DCMAKE_BUILD_TYPE=Debug
   ```

2. **Build the project:**
   ```
   cmake --build . --config Debug
   ```

3. **Run with the Python debugger:**
   ```
   python -m pdb src/gui/main_window.py
   ```

## Architecture Overview

The application uses a hybrid architecture:

- **C++ Backend:** Handles performance-critical operations:
  - Audio capture and streaming with PortAudio
  - Voice activity detection with WebRTC VAD
  - Speech recognition with Vosk (loaded in background)
  - Keyboard simulation for text output
  
- **Python Frontend:** Provides the user interface:
  - PyQt5 GUI components
  - Configuration management
  - User interaction handling
  - Progress reporting and status display

The C++ and Python components communicate via pybind11 bindings, allowing seamless integration while maintaining performance.

## Configuration

The application can be configured by editing `src/config/settings.json`:

```json
{
  "audio": {
    "default_device_id": -1,
    "vad_aggressiveness": 2,
    "hangover_timeout_ms": 300,
    "sample_rate": 16000,
    "frames_per_buffer": 320,
    "noise_threshold": 0.05
  },
  "transcription": {
    "engine": "vosk",
    "model_path": "models/vosk/vosk-model-en-us-0.22",
    "keypress_delay_ms": 20,
    "keypress_rate_limit_cps": 100
  },
  "shortcut": {
    "modifiers": ["Ctrl", "Shift"],
    "key": "T"
  },
  "ui": {
    "pause_on_active_window_change": false,
    "confirmation_feedback": true
  },
  "output": {
    "method": "simulated_keypresses",
    "clipboard_output": false
  },
  "dictation_commands": {
    "supported_commands": [
      { "phrase": "period", "action": "." },
      { "phrase": "comma", "action": "," },
      { "phrase": "question mark", "action": "?" },
      { "phrase": "exclamation point", "action": "!" },
      { "phrase": "new line", "action": "{ENTER}" },
      { "phrase": "new paragraph", "action": "{ENTER}{ENTER}" },
      { "phrase": "space", "action": " " },
      { "phrase": "control enter", "action": "{CTRL+ENTER}" },
      { "phrase": "all caps", "action": "{CAPSLOCK}" },
      { "phrase": "caps lock", "action": "{CAPSLOCK}" }
    ]
  }
}
```

## Project Status

### Current Development Stage

The application has stable core functionality with all major architectural components in place. Features like audio capture, voice activity detection, transcription, and text output are fully functional.

**Working Features:**
- Audio device enumeration and selection
- Keyboard shortcut capture and registration
- Voice activity detection 
- Background speech recognition model loading with progress feedback
- Robust audio streaming with proper buffer management
- Text output via simulated keypresses or clipboard
- Dictation commands for punctuation and formatting

**Under Development:**
- Enhancing the user interface with audio level visualization
- Optimizing performance and reducing latency
- Implementing comprehensive testing framework
- Creating detailed user documentation

### Recent Progress

- Implemented background model loading with progress reporting
- Enhanced audio streaming with robust buffer management
- Improved error handling with recovery mechanisms for audio issues
- Added RapidJSON for better parsing of transcription results

## Contributing

Contributions are welcome! Here's how you can help:

1. **Report issues** by creating an issue in the GitHub repository
2. **Suggest features** that would make the application more useful
3. **Submit pull requests** with bug fixes or new features

Please see [CONTRIBUTING.md](CONTRIBUTING.md) for more details on the contribution process.

## Frequently Asked Questions

**Q: Does this application send any data to the cloud?**  
A: No, everything works completely offline. Your speech data never leaves your computer.

**Q: What languages are supported?**  
A: Currently only English is supported, as we're using the English Vosk model.

**Q: Can I add custom dictation commands?**  
A: Yes, you can add custom commands by editing the `dictation_commands` section in the `settings.json` file.

**Q: Why does the application need a specific sample rate (16000 Hz)?**  
A: The Vosk speech recognition model is trained on audio at 16000 Hz. Using a different sample rate would reduce accuracy.

**Q: Why does the application take a moment to start transcribing the first time?**  
A: The speech recognition model is loaded in the background to keep the UI responsive. This can take 10-30 seconds depending on your system. Progress is displayed in the status bar.

**Q: Can I use this for real-time captioning or subtitles?**  
A: While the application can output text in real-time, it isn't specifically designed for captioning or subtitles. However, you could use the clipboard output option to paste text into a captioning tool.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [Vosk](https://alphacephei.com/vosk/) for the offline speech recognition engine
- [PortAudio](http://www.portaudio.com/) for the audio I/O library
- [WebRTC VAD](https://github.com/wiseman/py-webrtcvad) for voice activity detection
- [PyQt](https://riverbankcomputing.com/software/pyqt/) for the GUI framework
- [pybind11](https://github.com/pybind/pybind11) for C++/Python binding
- [RapidJSON](https://rapidjson.org/) for JSON parsing