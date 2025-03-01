# Voice Transcription Application

## 🚧Application unfinished: under active development🚧

A real-time voice-to-text transcription tool that works entirely offline for Windows, focusing on privacy, low-latency, and reliability.

## Key Features

- **🔒 Complete Privacy:** Works entirely offline with no cloud dependencies
- **⌨️ Keyboard Shortcut Toggle:** Start/stop transcription with a customizable hotkey (default: Ctrl+Shift+T)
- **🗣️ Smart Speech Detection:** Advanced voice activity detection with spectral analysis
- **🎙️ Audio Level Visualization:** Real-time feedback on audio input levels
- **🧹 Noise Filtering:** Background noise reduction for improved transcription accuracy
- **🔤 Enhanced Dictation Commands:** Context-aware command processing with smart punctuation
- **✍️ Flexible Output:** Choose between simulated keypresses or clipboard output
- **🪟 User-Friendly Interface:** Clean GUI with device selection and status indicators
- **⚡ Background Model Loading:** Non-blocking model initialization with progress feedback
- **💾 Optimized Audio Pipeline:** Low-latency circular buffer with efficient thread synchronization
- **🛡️ Robust Error Recovery:** Automatic recovery from device disconnections and other issues

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

### Enhanced Features

#### Noise Filtering
- Enable noise filtering in the options panel to improve transcription quality
- The filter automatically adapts to your environment's background noise levels
- Particularly useful in noisy environments

#### Audio Visualization
- Monitor your audio input levels in real-time with the audio level meter
- Green-to-red gradient shows input strength with peak level indicators
- Helps ensure your microphone is working correctly

#### Context-Aware Commands
- Commands are now context-sensitive based on the active application
- Special formatting is applied for code editors vs. word processors
- Smart punctuation and capitalization improves readability

#### Advanced Dictation Commands

Use these spoken commands to add punctuation and formatting:

| Say this... | To get this... |
|-------------|----------------|
| "period" (or "full stop", "dot") | . |
| "comma" | , |
| "question mark" | ? |
| "exclamation point" (or "exclamation mark") | ! |
| "new line" | (Enter key) |
| "new paragraph" | (Double Enter) |
| "space" | (Space) |
| "control enter" | (Ctrl+Enter) |
| "all caps" | (Enables ALL CAPS mode) |
| "no caps" | (Disables ALL CAPS mode) |
| "open parenthesis" | ( |
| "close parenthesis" | ) |
| "quote" | " |
| "single quote" | ' |
| "hyphen" (or "dash") | - |
| "underscore" | _ |

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
- Check the audio level meter to confirm your voice is being detected

#### Long model loading time
- The Vosk speech recognition model is loaded in the background
- Loading progress is displayed in the status bar
- First-time loading can take 10-30 seconds depending on your system
- The application remains responsive during loading

#### Text not appearing in applications
- Make sure the target application has focus when transcribing
- Try using clipboard output instead (in Options)
- Some applications may block simulated keypresses for security reasons

### Error Recovery

The application now includes an automatic error recovery system for:
- Audio device disconnections (automatically switches to an available device)
- Stream start failures (attempts different buffer sizes)
- Output errors (switches between keypress and clipboard methods)
- Transcription engine errors (resets and retries)

If you encounter persistent errors:
1. Check the logs in the `logs` directory
2. Restart the application
3. Try changing audio devices or output methods
4. If problems persist, please report the issue with your log files

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

## Architecture Overview

The application uses a hybrid architecture:

- **C++ Backend:** Handles performance-critical operations:
  - Audio capture and streaming with optimized circular buffer
  - Advanced voice activity detection with spectral analysis
  - Noise filtering for improved transcription accuracy
  - Speech recognition with Vosk (loaded in background)
  - Keyboard simulation for text output
  
- **Python Frontend:** Provides the user interface:
  - PyQt5 GUI components including audio level visualization
  - Enhanced command processor with context awareness
  - Comprehensive error recovery system
  - Configuration management
  - User interaction handling

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
    "noise_threshold": 0.05,
    "use_noise_filtering": true
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
  "error_handling": {
    "auto_recovery": true,
    "max_recovery_attempts": 3
  },
  "dictation_commands": {
    "capitalization_mode": "auto",
    "smart_punctuation": true,
    "supported_commands": [
      { "phrase": "period", "action": ".", "aliases": ["full stop", "dot"] },
      { "phrase": "comma", "action": "," },
      { "phrase": "question mark", "action": "?" },
      { "phrase": "exclamation point", "action": "!", "aliases": ["exclamation mark"] },
      { "phrase": "new line", "action": "{ENTER}" },
      { "phrase": "new paragraph", "action": "{ENTER}{ENTER}" },
      { "phrase": "space", "action": " " },
      { "phrase": "control enter", "action": "{CTRL+ENTER}" },
      { "phrase": "all caps", "action": "{ALLCAPS_ON}" },
      { "phrase": "no caps", "action": "{ALLCAPS_OFF}" },
      { "phrase": "open parenthesis", "action": "(", "aliases": ["left parenthesis"] },
      { "phrase": "close parenthesis", "action": ")", "aliases": ["right parenthesis"] },
      { "phrase": "quote", "action": "\"", "aliases": ["double quote"] },
      { "phrase": "single quote", "action": "'" }
    ]
  }
}
```

## Project Status

### Current Development Stage

The application is now in beta status with significant improvements to stability, performance, and user experience.

**Working Features:**
- Audio device enumeration and selection
- Advanced voice activity detection with spectral analysis
- Optimized audio buffer management with circular buffer
- Real-time audio level visualization
- Background noise filtering
- Context-aware dictation commands with smart punctuation
- Keyboard shortcut capture and registration
- Background speech recognition model loading with progress feedback
- Robust error recovery system with automatic fallbacks
- Text output via simulated keypresses or clipboard

**Under Development:**
- Support for additional languages
- Custom language model training
- Specialized vocabulary for different domains (medical, legal, etc.)
- Automatic updates mechanism

### Recent Improvements

- Implemented optimized circular buffer for audio streams with improved latency
- Added real-time audio level visualization for better user feedback
- Developed advanced voice activity detection using spectral analysis
- Implemented background noise filtering for better transcription accuracy
- Created a context-aware command processor with smart punctuation
- Built comprehensive error recovery system with automatic fallbacks

## Contributing

Contributions are welcome! Here's how you can help:

1. **Report issues** by creating an issue in the GitHub repository
2. **Suggest features** that would make the application more useful
3. **Submit pull requests** with bug fixes or new features

Please see [CONTRIBUTING.md](CONTRIBUTING.md) for more details on the contribution process.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [Vosk](https://alphacephei.com/vosk/) for the offline speech recognition engine
- [PortAudio](http://www.portaudio.com/) for the audio I/O library
- [PyQt](https://riverbankcomputing.com/software/pyqt/) for the GUI framework
- [pybind11](https://github.com/pybind/pybind11) for C++/Python binding
- [NumPy](https://numpy.org/) for audio processing calculations
- [RapidJSON](https://rapidjson.org/) for JSON parsing
