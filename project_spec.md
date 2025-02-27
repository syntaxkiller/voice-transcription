# Voice Transcription Application Specification

## Target Platform
- **Windows**

## Hardware Requirements
- **Minimum:** Any modern x86_64 CPU with SSE2 support
- **Recommended:** AMD Ryzen 5 5600G or equivalent with dedicated GPU (e.g., AMD Radeon RX 6700 XT)

## Overview
This specification details a robust, real-time, English-only voice transcription application for Windows, designed for:
- **Performance:** Low-latency transcription with optimized resource usage
- **Privacy:** Complete offline operation with no cloud dependencies
- **Reliability:** Fault tolerance and graceful error handling

### Key Features
- User-friendly GUI with clear status indicators
- Transcription toggling via a user-defined keyboard shortcut
- Audio streaming with voice activity detection (VAD)
- Speech transcription using the offline, open-source Vosk engine
- Text output via simulated keypresses with optional clipboard support
- Basic dictation commands for punctuation and formatting
- Robust error handling with graceful degradation

### Implementation Uses
- **Backend:** C++ (audio processing, Vosk integration, Windows API)
- **Frontend:** Python/PyQt (GUI)
- **Bridge:** pybind11 (connecting backend and frontend)

---

## Implementation Approach

Development focuses on direct integration with real libraries and APIs for immediate functionality. The application uses established libraries like PortAudio, Vosk, and WebRTC VAD. Comprehensive testing and robust error handling with graceful degradation are key aspects.

### 1. GUI Initialization & Audio Device Selection

#### Device Enumeration
- **Startup Scan:** Scan for available audio input devices using PortAudio upon startup.
- **Function:** `display_audio_devices() -> List[AudioDevice]`  
  _(A Python function that calls the C++ backend)_
- **Returns:** A list of `AudioDevice` objects (defined in C++ and exposed to Python) containing:
  - `id (int)`: System-specific device ID
  - `raw_name (str)`: Raw device name from PortAudio
  - `label (str)`: User-friendly label (e.g., "Built-in Microphone", "USB Headset")
  - `is_default (bool)`: Indicates if the device is the system default input device
  - `supported_sample_rates (List[int])`: Supported sample rates (filtered to 16000 Hz for Vosk)

#### Default Device Selection & Monitoring
- **Default Selection:** Automatically select and highlight the system's default input device if compatible.
- **Device Monitoring:** Monitor device changes using Windows `WM_DEVICECHANGE` messages via:
  - C++ functions: `create_hidden_window`, `message_loop`, `destroy_hidden_window`, and `WindowProc` (exposed to Python via pybind11)
- **On Device Unplug:**
  - Display an alert in the GUI (e.g., "Device Disconnected: [Device Name]")
  - Attempt automatic switching to the system default input device (if available and compatible)
  - If no compatible default is found, prompt the user to select a device manually
- **Error Handling:** Implement multiple fallback strategies for device disconnection with clear user guidance

#### Sample Rate Compatibility
- **Vosk Model Requirement:** Vosk models require a fixed 16000 Hz sample rate.
- **Compatibility Check:** During device enumeration, filter supported sample rates to include only 16000 Hz using the C++ function `ControlledAudioStream::check_device_compatibility` (exposed to Python).
- **User Selection:** Only display devices that support 16000 Hz; if the default device is incompatible, show a clear error message and prompt for manual selection.

---

### 2. Keyboard Shortcut Capture

#### Capture Mechanism
- **Activation:** A "Capture Shortcut" button in the GUI opens a modal dialog.
- **Instructions:** The dialog prompts the user to press the desired key combination (e.g., "Ctrl+Shift+T").
- **Cancellation:** Option to cancel via the Escape key or a "Cancel" button.
- **Countdown Timer:** A 3-second visual countdown is displayed, allowing cancellation during the countdown.

#### Validation & Conflict Resolution
- **Validation:** Requires at least one modifier key (Ctrl, Alt, Shift, or Win) and one non-modifier key.  
  Implemented by the `ShortcutValidator` class with the method `validate_shortcut(modifiers: List[str], key: str) -> bool`.
- **Error Handling:** Displays specific error messages if the shortcut is invalid (e.g., "At least one modifier key required").

#### Low-Level API Integration
- **Hotkey Registration:** Uses Windows `RegisterHotKey` API for global hotkey registration.
  - **C++ Functions (exposed via pybind11):**
    - `register_global_hotkey(modifiers: int, key: int) -> bool`
    - `unregister_global_hotkey(modifiers: int, key: int) -> bool`
    - `translate_modifiers(modifiers: List[str]) -> int`
    - `translate_key(key: str) -> int`
- **Capture Method:** Uses `pynput` within the modal dialog via the `ShortcutCaptureDialog` class to monitor key events.

#### Function & Persistence
- **Shortcut Capture Function:** `capture_shortcut(timeout: int = 3) -> Shortcut`  
  _(A Python function using the C++ backend)_
  - **Returns:** A `Shortcut` object with:
    - `modifiers (List[str])`: Modifier keys (e.g., `["Ctrl", "Shift"]`)
    - `key (str)`: The non-modifier key (e.g., `"T"`)
    - `is_valid (bool)`: Validity flag
    - `display_string (str)`: Human-readable string (e.g., `"Ctrl+Shift+T"`)
- **Error Handling:** Raises `ShortcutCaptureError` if capture is cancelled or times out.
- **Persistence:** The captured shortcut is displayed in the GUI and stored in `settings.json` using `ConfigManager.save_shortcut(shortcut: Shortcut) -> bool`.

---

### 3. Transcription Activation & Audio Streaming

#### Shortcut-Activated Transcription
- **Activation:** A global listener waits for the user-defined shortcut.
- **Trigger:** When the shortcut is pressed, a Python callback toggles transcription on/off using the `ShortcutListener` class with its `on_hotkey_triggered` callback.

#### Visual Feedback
- **Indicators:** When transcription is active:
  - A blinking microphone icon (500ms interval)
  - A "Transcribing…" label in the GUI  
  Implemented by the `StatusIndicator` class with `set_transcribing(is_transcribing: bool)`.

#### Window Change Detection
- **Monitoring:** The active window is monitored via the C++ function `get_foreground_window_title` (exposed to Python).
- **Notification:** A notification is displayed if the active window changes during transcription, managed by the `WindowMonitor` class (checks every 500ms).

#### Audio Streaming & Processing
- **Streaming Initialization:** Captures audio from the selected device using PortAudio at 16000 Hz.
  - **Function:** `stream_audio(device_id: int) -> ControlledAudioStream`  
    _(Creates a C++ object from Python)_
- **C++ Methods (exposed to Python) in `ControlledAudioStream`:**
  - `start() -> bool`
  - `stop()`
  - `pause()`
  - `resume()`
  - `is_active() -> bool`
  - `get_next_chunk() -> Optional[AudioChunk]`
  - `get_device_id() -> int`
  - `get_sample_rate() -> int`
  - `get_frames_per_buffer() -> int`
  - `get_last_error() -> str`
  - `get_buffer_size() -> int`
  - `get_buffer_used() -> int`
- **Error Handling:** Implements layered fallback strategies for handling audio stream issues, with specific error codes/messages.

#### Voice Activity Detection (VAD)
- **Implementation:** Uses WebRTC VAD with configurable aggressiveness (default is 2).
  - **C++ Class:** `VADHandler` with:
    - `process_frame(frame: AudioFrame) -> bool`
    - `set_aggressiveness(level: int) -> void`
    - `get_aggressiveness() -> int`
- **Audio Chunking:** Processes audio in 20ms chunks (320 samples at 16000 Hz) using the `AudioChunker` class.
- **Hangover Timer:** Continues processing briefly after silence (default 300ms) using the `SilenceDetector` class.

#### Speech Recognition
- **Engine:** Uses the Vosk API for speech recognition.
- **Model Options:** Configurable Vosk model path.
- **Integration:** Through the `VoskTranscriber` class.
  - **Constructor:** `VoskTranscriber(const std::string& model_path, float sample_rate)`
  - **Methods:**
    - `transcribe(std::unique_ptr<AudioChunk> chunk) -> TranscriptionResult`
    - `get_last_error() -> std::string`
    - `is_model_loaded() -> bool`
    - `reset() -> void`
    - `get_model_info() -> ModelInfo`
- **Memory Management:** Uses RAII for proper Vosk object management.
- **GUI Status:** Displays "Offline – Vosk" until the model loads.
- **Model Loading:** Handled in a background thread via the `ModelLoader` class with progress reporting.
- **JSON Processing:** Uses RapidJSON for parsing Vosk output with proper error handling.

#### Vosk API Requirements
- Utilizes `VoskModel` and `VoskRecognizer` objects.
- Requires 16-bit PCM, mono, 16000 Hz audio input.
- Supports partial and final results.
- Ensures proper release and verification (e.g., checksums) of model data.

#### Audio Processing Function
- **Function:** `process_audio(audio_stream: ControlledAudioStream) -> TranscriptionResult`  
  _Runs in a separate thread._
  - **Returns:** A `TranscriptionResult` object with:
    - `text (str)`: Transcribed text
    - `is_final (bool)`: Final or partial result indicator
    - `confidence (float)`: Confidence score (0.0–1.0)
    - `alternatives (List[str])`: Alternative transcriptions
    - `word_timestamps (List[WordTiming])`: Timing information per word
- **Error Handling:** Implements layered error strategies with specific error code/message.

#### Threading and Queues
- **Concurrency:** Audio capture, VAD processing, transcription, and text output run in separate threads.
  - Coordinated by the `ThreadManager` class.
- **Audio Queue:** Managed by an internal buffer in `ControlledAudioStream` (with mutex protection) using a `RingBuffer<AudioSample>` class.
  - **Buffer Size:** Configurable (default 10 seconds of audio, i.e., 160,000 samples at 16000 Hz)
- **Transcription Queue:** Managed via Python threading and signals with the `TranscriptionQueue` class:
  - Methods include `enqueue(result: TranscriptionResult)`, `get_next()`, `clear()`, and `is_empty()`.
- **Thread Management:** Uses Python's `concurrent.futures.ThreadPoolExecutor` for managing threads with proper shutdown.
- **PyQt Integration:** Uses signals and slots (with custom `QObject`-derived classes) for thread-safe GUI updates.
- **Shutdown:** Utilizes an atomic flag (`std::atomic<bool>`) to signal thread termination and proper join operations.

---

### 4. Text Output

#### Simulated Keypress Output
- **Method:** Simulate keypresses via Windows `SendInputW` API.
  - **C++ Function:** `simulate_keypresses(text: std::wstring) -> bool` (exposed to Python)
- **Details:**
  - Uses `INPUT` structures with proper Unicode handling.
  - Converts text to key codes, handling modifiers and special characters.
- **Error Handling:** Implements recovery strategies for keypress simulation failures.
- **Rate Limiting:** Limits output to 100 characters per second using the `OutputRateLimiter` class.
- **Batching:** Groups keypresses into batches (every 20–50ms) for performance via the `KeypressBatcher` class.

#### Clipboard Output
- **Alternative Method:** Option to copy text to the clipboard instead of simulating keypresses.
  - Controlled by the `clipboard_output` setting (default is false).
- **Managed by:** the `ClipboardManager` class with methods:
  - `copy_to_clipboard(text: str) -> bool`
  - `get_clipboard_text() -> str`
  - `clear_clipboard() -> void`

#### Focus Management
- **User Guidance:** Advises users to keep the target application focused.
- **Warning:** Displays a warning if focus changes during transcription.
- **Special Key Handling:** Supports keys like `{ENTER}` and `{TAB}` by mapping to virtual key codes using the `SpecialKeyHandler` class.

---

### 5. Dictation Commands

#### Command Set
- **Supported Commands:**
  - `"period"` → Inserts `.`
  - `"comma"` → Inserts `,`
  - `"question mark"` → Inserts `?`
  - `"exclamation point"` → Inserts `!`
  - `"new line"` → Inserts `{ENTER}` (simulated Enter key)
  - `"new paragraph"` → Inserts `{ENTER}{ENTER}`
  - `"space"` → Inserts a single space `" "`
  - `"control enter"` → Simulates `{CTRL+ENTER}`
  - `"all caps"` → Toggles Caps Lock on via `{CAPSLOCK}`
  - `"caps lock"` → Toggles Caps Lock off via `{CAPSLOCK}`

#### Command Processor
- **Module:** `command_processor.py`
- **Function:** Processes raw transcription text to identify and execute commands.
- **Implemented by:** the `CommandProcessor` class with methods:
  - `process_text(text: str) -> ProcessedText`
  - `add_command(phrase: str, action: str) -> bool`
  - `remove_command(phrase: str) -> bool`
  - `get_all_commands() -> List[Command]`
- **Implementation:** Uses regular expressions for mapping phrases to actions, with compiled patterns for efficiency and context-aware recognition.
- **Execution:** Commands are executed via simulated keypresses through the `CommandExecutor` class.

---

### 6. Performance and Optimization

#### Multi-threading & Asynchronous Processing
- **Architecture:** Separate threads for audio capture, VAD, transcription, and output processing.
  - Audio capture runs at real-time priority.
  - Processing threads run at normal priority.
  - The GUI thread remains responsive and isolated.

#### Latency Targets
- **Target Latency:** Achieve 100–300ms latency from the end of speech to text output.
- **Monitoring:** Latency is logged for benchmarking by the `PerformanceMonitor` class.

#### Inter-Language Communication
- **Integration:** Uses `pybind11` to bridge C++ and Python.
  - Proper bindings with clear ownership transfer semantics.
- **Optimization:** Minimizes cross-language calls by passing pointers and batching operations.

#### Resource Management
- **Audio Chunks:** Processed in fixed-size 20ms chunks (320 samples at 16000 Hz) via the `AudioChunk` class.
- **Memory Pooling:** Uses a memory pool for audio chunks to reduce allocation overhead.
- **Buffer Management:** Uses PortAudio and internal buffers with mutexes for thread safety, with proper cleanup on shutdown.

---

### 7. User Experience & Error Handling

#### Error Management

**Categories:**
- **Audio Device Errors:** (e.g., device disconnection, initialization failure) — Error codes 1000–1999.
- **Transcription Errors:** (e.g., model loading failure, recognition errors) — Error codes 2000–2999.
- **Shortcut Capture Errors:** (e.g., invalid shortcut, registration failure) — Error codes 3000–3999.
- **Output Errors:** (e.g., SendInputW failure) — Error codes 4000–4999.
- **Setup Errors:** (e.g., dependency installation failure, build failures) — Error codes 5000–5999.
- **Configuration Errors:** (e.g., settings.json corruption) — Error codes 6000–6999.

- **Logging:** Errors are logged with timestamps and context via the `Logger` class.  
  **Format:** `[TIMESTAMP] [SEVERITY] [COMPONENT] [CODE] Message`
- **User Messaging:** Clear, non-technical error messages are displayed with recovery instructions using the `ErrorMessageFormatter` class.

#### Error Handling Philosophy
- **Layered Fallback Strategy:** Each operation attempts the most direct approach first, then falls back to increasingly specific alternatives if failures occur.
- **Graceful Degradation:** When a component fails, the application attempts to continue with limited functionality rather than failing completely.
- **Clear User Communication:** All errors are reported to users in plain language with actionable guidance.
- **Recovery Attempts:** For transient errors, the application makes multiple recovery attempts before reporting failure.
- **Structured Error Hierarchy:** A standardized error code system allows for consistent handling across components.

#### Automatic Recovery
- **Audio Device Disconnection:** Attempts automatic switching to the default device (max 3 attempts, 500ms apart).
- **Transient Errors:** Retries up to 3 times before alerting the user, managed by the `ErrorRecoveryManager` class.
- **Dependency Failures:** Setup process includes multiple fallback options for acquiring and installing dependencies.
- **Configuration Recovery:** Attempts to restore corrupted configuration from backups or defaults.

---

### 8. Development Environment

#### Build and Setup

**Setup Script:** A consolidated `setup.bat` handles dependencies with multiple fallback mechanisms:
- Attempts to download pre-built binaries first
- Falls back to building from source if needed
- Provides clear error messages with recovery options
- Uses absolute paths to avoid directory navigation issues
- Includes verification steps to confirm successful installation

**Dependency Management:** Clear separation between essential and optional dependencies with appropriate handling for each category:
- **Essential packages:** Required for core functionality (PyQt5, pybind11, etc.)
- **Optional packages:** Enhance functionality but not strictly required (pytest, etc.)

**Automation:** Creates necessary directories and files, with clear error messages for missing dependencies.

#### Testing Strategy
- **Unit Testing:** Separate unit tests for C++ (using Google Test) and Python (using pytest).
- **Integration Tests:** Tests for cross-language functionality using pybind11.
- **Continuous Integration:** Utilizes GitHub Actions for automated Windows builds, tests, and coverage reporting.
- **Test Coverage:** Aims for >80% coverage of core functionality, including error paths.

---

### 9. Languages and Technologies

#### Backend (C++)
- **Language Standard:** C++17
- **Libraries/Dependencies:**
  - PortAudio 19.7.0 or later (audio capture)
  - Windows APIs (SendInputW, RegisterHotKey, GetForegroundWindow, WM_DEVICECHANGE)
  - WebRTC VAD (voice activity detection)
  - Vosk API 0.3.45 or later (speech recognition)
  - RapidJSON (for parsing Vosk results)

#### GUI (Python)
- **Python Version:** 3.8 or later
- **Framework:** PyQt 5.15 or later

#### Inter-Language Bridge
- **Tool:** pybind11 2.9.0 or later (for C++ to Python integration)

#### Testing
- **C++ Testing:** Google Test framework
- **Python Testing:** pytest with pytest-qt for GUI tests

---

### 10. File Structure
/voice-transcription/
├── src/
│   ├── gui/
│   │   ├── main_window.py          # Main application window (PyQt)
│   │   ├── device_selector.py      # Audio input device selection
│   │   ├── shortcut_capture.py     # Keyboard shortcut capture
│   │   ├── status_indicator.py     # Visual feedback (transcribing, errors, etc.)
│   │   └── ...                   # Other GUI modules
│   ├── backend/
│   │   ├── include/
│   │   │   ├── audio_stream.h              # Audio streaming, VAD
│   │   │   ├── vosk_transcription_engine.h # Vosk integration
│   │   │   ├── keyboard_sim.h              # Keypress simulation
│   │   │   ├── window_manager.h            # Windows API interactions
│   │   │   ├── webrtc_vad.h                # Voice activity detection
│   │   │   └── vosk_api.h                  # Vosk API header
│   │   ├── audio_stream.cpp
│   │   ├── vosk_transcription_engine.cpp
│   │   ├── keyboard_sim.cpp
│   │   └── window_manager.cpp
│   ├── bindings/
│   │   └── pybind_wrapper.cpp      # pybind11 bindings
│   ├── config/
│   │   └── settings.json           # User preferences
│   ├── utils/
│   │   ├── logger.py               # Logging
│   │   ├── error_handler.py        # Error handling
│   │   └── command_processor.py    # Dictation command processing
│   └── assets/
│       ├── icons/                # GUI icons
│       └── fonts/                # Custom fonts
├── tests/
│   ├── test_audio.py             # Unit tests for audio/VAD
│   ├── test_gui.py               # GUI tests
│   ├── test_integration.py       # End-to-end tests
│   ├── test_commands.py          # Command tests
│   └── test_keyboard_sim.py      # Keypress simulation tests
├── libs/                         # External libraries
│   ├── portaudio/                # PortAudio library
│   ├── vosk/                     # Vosk library
│   ├── webrtc_vad/               # WebRTC VAD library
│   ├── pybind11/                 # pybind11 library
│   ├── rapidjson/                # RapidJSON library for JSON parsing
│   └── googletest/               # Google Test framework
├── models/
│   └── vosk/
│       └── vosk-model-small-en-us-0.15/   # Vosk model (or other chosen model)
├── docs/
│   ├── user_guide.md             # User documentation
│   ├── installation_guide.md     # Installation instructions
│   └── troubleshooting.md        # Troubleshooting
├── logs/
│   └── app.log                   # Application logs
├── build/                        # Build output directory
├── setup.bat                   # Consolidated setup script with robust error handling
├── requirements.txt            # Python dependencies
├── CMakeLists.txt              # C++ build configuration
├── .gitignore                  # Git ignore file
├── README.md                   # Project overview
├── progress.md                 # Project progress tracking
├── project_spec.md             # This specification document
├── LICENSE
└── CHANGELOG.md

### 11. Build Process

#### Prerequisites
- **Visual Studio:** 2019 or 2022 with C++ workload  
- **Python:** 3.8 or later with required packages (see `requirements.txt`)  
- **CMake:** 3.14 or later

#### Build Steps
1. **Run the setup script:**  
   Run `setup.bat`  
   This script handles:
   - Python dependency installation with robust error handling
   - PortAudio setup through multiple fallback methods:
     - Downloading pre-built binaries
     - Building from source with CMake
     - Searching for existing installations
     - Creating placeholders with warnings if all else fails
   - RapidJSON setup for JSON parsing
   - Creation of necessary directories and files
   - Configuration of the build environment

2. **Configure with CMake:**  
   In the command line, run:  
   cd build  
   cmake ..

3. **Build the project:**  
   Execute:  
   cmake --build . --config Release

4. **Run the application:**  
   Execute:  
   python src/gui/main_window.py

#### Debugging
- **C++ Debugging:** Use Visual Studio for C++ debugging by opening the generated solution file in the build directory.
- **Python Debugging:** Use the Python debugger for Python code, or attach Visual Studio to the Python process.
- **Logs:** Examine log files in the `logs` directory for detailed diagnostics.

---

### 12. Settings File (`settings.json`)

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
    "model_path": "models/vosk/vosk-model-small-en-us-0.15",
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
      { "phrase": "comma",  "action": "," },
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

---

### 13. Implementation Notes

#### Development Philosophy
- **Robust Error Handling:** Implement thorough error recovery systems with multi-layered fallback strategies.
- **Graceful Degradation:** Allow partial functionality when components fail rather than complete failure.
- **Comprehensive Testing:** Ensure all components are thoroughly tested, including error paths.
- **Clear User Communication:** Provide actionable, clear error messages to users.

#### Implementation Patterns
- **Layered Fallback Strategy:** Each critical operation includes multiple fallback approaches.
- **Essential vs. Optional Components:** Clear separation between components that are essential for operation and those that provide additional features.
- **Verification Steps:** Operations that could silently fail include verification to confirm success.
- **Absolute Path References:** Use absolute paths to avoid directory navigation issues in scripts.

#### Future Enhancements
- Add support for automatic punctuation.
- Provide a plugin system for application-specific commands.
- Add domain-specific language models for improved accuracy in specific fields.
- Implement background training for personalized voice models.
- Add support for speaker diarization (identifying different speakers).
- Implement automatic punctuation and capitalization.

### 14. Implementation Status and Roadmap

#### Current Status

The project has established its basic architecture with the following components:
- Basic project structure with C++ backend and Python frontend
- PortAudio integration for audio device management (partial implementation)
- WebRTC VAD interface for voice activity detection
- Keyboard simulation module for text output
- Window management for device/focus tracking
- Python GUI shell using PyQt5

#### Current Issues

Build process needs fixes:
- CMake configuration has target ordering issues
- Target-specific commands appear before target definition
- Build fails due to these ordering problems

### Implementation Roadmap

#### Phase 1: Foundation (Current Focus)
1. **Fix Build System**
   - Correct CMake configuration issues
   - Implement proper target ordering
   - Ensure clean builds on Windows

2. **Basic C++/Python Integration**
   - Implement minimal pybind11 bindings for core functions
   - Test data transfer between languages
   - Create verification tests for cross-language calls

3. **Core Audio Pipeline**
   - Complete PortAudio device enumeration
   - Implement audio streaming
   - Integrate WebRTC VAD
   - Connect basic Vosk functionality
   - Implement text output methods

#### Phase 2: MVP Integration
1. **End-to-End Flow**
   - Connect pipeline components
   - Create thread-safe communication
   - Implement state management
   - Build transcription toggle functionality

2. **Basic GUI Integration**
   - Connect backend to Python GUI
   - Implement device selection UI
   - Add status indicators
   - Create simple settings interface

#### Phase 3: Polish and Refinement
1. **Robustness Improvements**
   - Enhance error handling strategies
   - Implement recovery mechanisms
   - Add graceful degradation
   - Create user-friendly error messages

2. **User Experience Enhancements**
   - Improve visual feedback
   - Add audio level visualization
   - Enhance dictation commands
   - Optimize performance

### Verification Strategy

Each implementation phase will include:
- Unit tests for individual components
- Integration tests for connected components
- End-to-end tests for complete workflows
- Performance benchmarks where applicable

### Build Process Current State

The build process currently faces issues with CMake configuration:
- Target definition ordering problems
- Improper placement of target-specific commands
- Required corrections:
  1. Define the `voice_transcription_backend` target before any references to it
  2. Move all target-specific commands after the target definition
  3. Ensure proper dependency resolution order

These build issues must be resolved before proceeding with further implementation tasks.