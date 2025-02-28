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

## 3. Transcription Activation & Audio Streaming

#### Audio Streaming & Processing
- **Streaming Initialization:** Captures audio from the selected device using PortAudio at 16000 Hz.
  - **Function:** `stream_audio(device_id: int) -> ControlledAudioStream`  
    _(Creates a C++ object from Python)_
- **C++ Methods (exposed to Python) in `ControlledAudioStream`:**
  - `start() -> bool` - Starts audio capture with robust error handling
  - `stop()` - Stops capture with proper resource cleanup
  - `pause()` - Temporarily pauses audio capture
  - `resume()` - Resumes paused capture
  - `is_active() -> bool` - Checks if capture is active
  - `get_device_id() -> int` - Gets current device ID
  - `get_sample_rate() -> int` - Gets current sample rate
  - `get_frames_per_buffer() -> int` - Gets frame buffer size
  - `get_last_error() -> str` - Gets last error message
  - `get_next_chunk() -> Optional[AudioChunk]` - Gets next audio chunk with timeout handling and buffer management
- **Error Handling:** Implements layered fallback strategies with specific error codes/messages.
- #### Buffer Management
- **Circular Buffer Implementation:**
  - Efficient ring buffer design with separate read/write pointers
  - Fixed-size allocation to prevent memory growth
  - Thread-safe operations using mutexes and condition variables
  - Automatic overflow handling to maintain streaming quality
- **Thread Synchronization:**
  - Condition variables for efficient wait operations
  - Mutex protection for shared buffer access
  - Non-blocking reads with timeout capabilities
- **Memory Protection:** 
  - Limits buffer size to prevent memory growth during long sessions
  - Implements proper overflow handling to maintain audio quality
  - Provides proper resource cleanup on stream termination
- **Latency Optimization:**
  - Buffer priming during startup
  - Efficient ring buffer reduces copy operations
  - Direct notification of waiting threads when data is available

#### Voice Activity Detection (VAD)
- **Implementation:** Uses advanced spectral analysis with energy thresholding.
  - **C++ Class:** `VADHandler` with:
    - `is_speech(chunk: AudioChunk) -> bool` - Determines if chunk contains speech using spectral analysis
    - `set_aggressiveness(level: int) -> void` - Sets VAD sensitivity (0-3)
    - `get_aggressiveness() -> int` - Gets current VAD sensitivity
- **Audio Chunking:** Processes audio in 20ms chunks (320 samples at 16000 Hz) using the `AudioChunker` class.
- **Hangover Timer:** Continues processing briefly after silence (default 300ms) using the `SilenceDetector` class.
- **Spectral Analysis:** Computes spectral flatness to distinguish speech from noise.
- **Adaptable Thresholding:** Automatically adjusts detection threshold based on background noise levels.

#### Speech Recognition
- **Engine:** Uses the Vosk API for speech recognition.
- **Model Options:** Configurable Vosk model path.
- **Background Loading:** Model is loaded asynchronously to keep the UI responsive
  - Implemented via `std::async` in C++ with progress reporting
  - Progress monitoring through atomic variables for thread safety
  - UI feedback during loading process via Python callbacks
- **Integration:** Through the `VoskTranscriber` class.
  - **Constructor:** `VoskTranscriber(const std::string& model_path, float sample_rate)`
  - **Methods:**
    - `transcribe(std::unique_ptr<AudioChunk> chunk) -> TranscriptionResult` - Process audio chunk
    - `transcribe_with_vad(std::unique_ptr<AudioChunk> chunk, bool is_speech) -> TranscriptionResult` - Process with VAD information
    - `reset() -> void` - Reset the recognizer state
    - `is_model_loaded() -> bool` - Check if model is ready for use
    - `is_loading() -> bool` - Check if model is still loading
    - `get_loading_progress() -> float` - Get loading progress (0.0-1.0)
    - `get_last_error() -> std::string` - Get error information
- **Memory Management:** Uses RAII for proper Vosk object management.
- **GUI Status:** Displays loading progress in status bar during initialization.
- **Model Loading:** Handled in a background thread via `std::async` with non-blocking progress reporting.
- **JSON Processing:** Uses RapidJSON for parsing Vosk output with proper error handling.

#### Audio Level Visualization
- **GUI Component:** Real-time audio level meter showing input strength.
  - **Python Class:** `AudioLevelMeter` that inherits from `QWidget`.
  - **Visualization:** Gradient coloring from green (low) to red (high) with peak indicators.
- **Features:**
  - Peak level detection with configurable hold time
  - Gradual peak decay for better visualization
  - Color-coded levels with intuitive threshold indicators
  - Both vertical and horizontal orientation support
- **Audio Processing:**
  - RMS (Root Mean Square) amplitude calculation
  - Logarithmic (dB) scale conversion for better dynamic range
  - Moving average for smooth visual transitions
- **Integration:**
  - Direct connection to the audio stream for real-time updates
  - `AudioLevelMonitor` class to bridge backend and UI
  - Automatic updates synchronized with transcription state

#### Noise Filtering
- **Implementation:** Background noise estimation and filtering.
  - **C++ Class:** `NoiseFilter` with:
    - `filter(chunk: AudioChunk) -> void` - Apply noise filtering to audio chunk
    - `calibrate(chunk: AudioChunk) -> void` - Calibrate with a silence sample
    - `auto_calibrate(chunk: AudioChunk, is_speech: bool) -> void` - Automatically update noise floor
- **Features:**
  - Adaptive noise floor estimation during silence periods
  - Soft noise gate for natural-sounding results
  - Spectral subtraction for improved speech clarity
  - Automatic calibration based on ambient conditions
- **Integration:**
  - Optional enabling/disabling via configuration
  - Seamless integration with the transcription pipeline
  - Minimal CPU overhead with optimized implementation

---

## 4. Text Output

### Simulated Keypress Output
- **Method:** Simulate keypresses via Windows `SendInputW` API.
  - **C++ Function:** `simulate_keypresses(text: std::wstring, delay_ms: int = 20) -> bool` (exposed to Python)
- **Details:**
  - Uses `INPUT` structures with proper Unicode handling.
  - Converts text to key codes, handling modifiers and special characters.
- **Error Handling:** Implements recovery strategies for keypress simulation failures.
- **Rate Limiting:** Limits output to 100 characters per second using the `OutputRateLimiter` class.
- **Batching:** Groups keypresses into batches (every 20–50ms) for performance via the `KeypressBatcher` class.

### Clipboard Output
- **Alternative Method:** Option to copy text to the clipboard instead of simulating keypresses.
  - Controlled by the `clipboard_output` setting (default is false).
- **Managed by:** the `ClipboardManager` class with methods:
  - `set_clipboard_text(text: std::wstring) -> bool`
  - `get_clipboard_text() -> std::wstring`

### Focus Management
- **User Guidance:** Advises users to keep the target application focused.
- **Warning:** Displays a warning if focus changes during transcription.
- **Special Key Handling:** Supports keys like `{ENTER}` and `{TAB}` by mapping to virtual key codes using the `SpecialKeyHandler` class.

---

## 5. Dictation Commands

#### Command Set
- **Core Commands:**
  - Punctuation: `"period"` (`.`), `"comma"` (`,`), `"question mark"` (`?`), etc.
  - Formatting: `"new line"` (`{ENTER}`), `"new paragraph"` (`{ENTER}{ENTER}`), etc.
  - Special keys: `"control enter"` (`{CTRL+ENTER}`), `"tab key"` (`{TAB}`), etc.
  - Capitalization: `"all caps"` (enables caps mode), `"no caps"` (disables caps mode)
  - Symbols: `"open parenthesis"` (`(`), `"quote"` (`"`), `"hyphen"` (`-`), etc.

- **Command Aliases:** Multiple ways to trigger the same action
  - Example: `"period"`, `"full stop"`, and `"dot"` all produce `.`
  - Example: `"exclamation point"` and `"exclamation mark"` both produce `!`
  
#### Command Processor
- **Module:** `enhanced_command_processor.py`
- **Implementation:** The `EnhancedCommandProcessor` class with methods:
  - `process(text: str) -> str` - Basic command processing
  - `process_with_context(text: str, context: Dict) -> str` - Context-aware processing
  - `add_command(phrase: str, action: str, aliases: List[str]) -> bool` - Add command with aliases
  - `remove_command(phrase: str) -> bool` - Remove a command
  - `get_commands() -> List[Dict]` - Get all commands with aliases
  - `set_capitalization_mode(mode: str) -> bool` - Set capitalization mode
  - `set_smart_punctuation(enabled: bool) -> bool` - Enable/disable smart punctuation

#### Context-Aware Features
- **Application Detection:**
  - Identifies the current foreground application
  - Applies application-specific formatting rules
  - Customizes command behavior based on context
  
- **Smart Capitalization:**
  - Automatically capitalizes first letter of sentences
  - Handles proper formatting after punctuation
  - Supports ALL CAPS mode for emphasis
  
- **Smart Punctuation:**
  - Ensures proper spacing around punctuation marks
  - Fixes common punctuation mistakes
  - Applies typographic improvements (em dashes, smart quotes) in word processors
  
- **Domain-Specific Processing:**
  - Code-friendly formatting for programming editors 
  - Document-optimized formatting for word processors
  - Support for specialized terminology in different domains

## 6. Error Recovery System

#### Error Management Framework
- **Categories:**
  - **Audio Device Errors:** (device disconnection, initialization failure) — Error codes 1000–1999
  - **Transcription Errors:** (model loading failure, recognition errors) — Error codes 2000–2999
  - **Shortcut Capture Errors:** (invalid shortcut, registration failure) — Error codes 3000–3999
  - **Output Errors:** (keypresses failure, clipboard errors) — Error codes 4000–4999
  - **Setup Errors:** (dependency installation issues, build failures) — Error codes 5000–5999
  - **Configuration Errors:** (settings.json corruption) — Error codes 6000–6999

- **Severity Levels:**
  - **INFO:** Informational messages, not errors (model loading progress)
  - **WARNING:** Non-critical issues that don't interrupt operation
  - **ERROR:** Significant errors affecting functionality that may be recoverable
  - **CRITICAL:** Severe errors preventing application operation

- **Logging & Reporting:**
  - Detailed logs with timestamps, severity, category, and context
  - User-friendly messages with recovery steps
  - Progress reporting for background operations
  
#### Recovery Strategies
- **Audio Device Recovery:**
  - Automatic switching to default device when disconnected
  - Retry with different buffer sizes on stream failures
  - Graceful handling of device enumeration changes
  
- **Transcription Recovery:**
  - Alternative model loading paths for model failures
  - Transcriber reset and reinitialize on errors
  - Graceful degradation with partial functionality
  
- **Output Recovery:**
  - Automatic switching between keypress and clipboard methods
  - Retry with rate limiting for failed output
  - Graceful fallback to simpler output methods
  
- **Recovery Implementation:**
  - Strategy pattern with specialized recovery handlers
  - Multiple recovery attempts with increasing delays
  - Configurable maximum attempts and timeouts
  - Background recovery for non-critical errors

#### User Experience Integration
- **Error Notifications:**
  - Severity-appropriate notification methods (status bar vs. dialog)
  - Clear explanations of what happened and why
  - Recovery feedback on automatic fixes
  
- **Recovery Steps:**
  - Actionable guidance for manual recovery
  - Contextual help based on error types
  - Links to detailed documentation for complex issues
  
- **Configurability:**
  - User-adjustable automatic recovery options
  - Maximum recovery attempts configuration
  - Notification verbosity settings

## 7. Performance and Optimization

#### Multi-threading & Asynchronous Processing
- **Architecture:** Separate threads for audio capture, VAD, transcription, and output processing.
  - Audio capture runs at real-time priority.
  - Processing threads run at normal priority.
  - Background loading operations use `std::async` for non-blocking execution.
  - The GUI thread remains responsive during intensive operations.

#### Latency Targets
- **Target Latency:** Achieve 100–300ms latency from the end of speech to text output.
- **Monitoring:** Latency is logged for benchmarking by the `PerformanceMonitor` class.

#### Inter-Language Communication
- **Integration:** Uses `pybind11` to bridge C++ and Python.
  - Proper bindings with clear ownership transfer semantics.
  - Background operations expose progress tracking to Python.
- **Optimization:** Minimizes cross-language calls by passing pointers and batching operations.

#### Resource Management
- **Audio Chunks:** Processed in fixed-size 20ms chunks (320 samples at 16000 Hz) via the `AudioChunk` class.
- **Memory Protection:** 
  - Uses size-limited buffers to prevent memory growth during long sessions.
  - Implements buffer overflow handling to maintain streaming quality.
  - Provides proper resource cleanup on stream termination.
- **Memory Pooling:** Uses a memory pool for audio chunks to reduce allocation overhead.
- **Buffer Management:** Uses PortAudio and internal buffers with mutexes for thread safety, with proper cleanup on shutdown.



## 8. User Experience & Error Handling

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
- **Progress Reporting:** Background operations like model loading provide real-time progress updates in the UI.

#### Error Handling Philosophy
- **Layered Fallback Strategy:** Each operation attempts the most direct approach first, then falls back to increasingly specific alternatives if failures occur.
- **Graceful Degradation:** When a component fails, the application attempts to continue with limited functionality rather than failing completely.
- **Clear User Communication:** All errors are reported to users in plain language with actionable guidance.
- **Non-Blocking Operations:** Time-consuming operations run in background threads with progress reporting.
- **Recovery Attempts:** For transient errors, the application makes multiple recovery attempts before reporting failure.
- **Structured Error Hierarchy:** A standardized error code system allows for consistent handling across components.

#### Automatic Recovery
- **Audio Device Disconnection:** Attempts automatic switching to the default device (max 3 attempts, 500ms apart).
- **Audio Buffer Management:** Automatically handles buffer overflow conditions without disrupting streaming.
- **Model Loading Issues:** Provides clear feedback during model loading with fallback mechanisms.
- **Transient Errors:** Retries up to 3 times before alerting the user, managed by the `ErrorRecoveryManager` class.
- **Dependency Failures:** Setup process includes multiple fallback options for acquiring and installing dependencies.
- **Configuration Recovery:** Attempts to restore corrupted configuration from backups or defaults.

#### Performance Optimizations

- **Audio Buffer Management:**
  - Circular buffer implementation for zero-copy reading
  - Condition variables for efficient thread waiting
  - Proper overflow protection with oldest-first discard policy
  - Memory usage limits to prevent growth during long sessions
  
- **Thread Synchronization:**
  - Fine-grained locking for minimal contention
  - Wait-notify pattern for buffer operations
  - Non-blocking reads with timeouts for responsiveness
  - Background processing for CPU-intensive operations
  
- **Noise Filtering:**
  - Adaptive filtering based on detected noise levels
  - Low-overhead spectral analysis
  - Minimal processing during silence periods
  - In-place filtering to reduce memory allocation
  
- **Memory Management:**
  - Optimized buffer sizes based on empirical testing
  - Proper cleanup of resources during shutdown
  - Buffer reuse when possible to reduce allocations
  - Smart pointer usage for automatic resource management

---

### 9. Build Process

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
   ```
   cd build
   cmake ..
   ```

3. **Build the project:**  
   Execute:  
   ```
   cmake --build . --config Release
   ```

4. **Run the application:**  
   Execute:  
   ```
   python src/gui/main_window.py
   ```

#### Debugging
- **C++ Debugging:** Use Visual Studio for C++ debugging by opening the generated solution file in the build directory.
- **Python Debugging:** Use the Python debugger for Python code, or attach Visual Studio to the Python process.
- **Logs:** Examine log files in the `logs` directory for detailed diagnostics.

---

### 10. Settings File (`settings.json`)

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
    "confirmation_feedback": true,
    "level_meter": {
      "peak_hold_time_ms": 1000,
      "show_peak_indicator": true
    }
  },
  "output": {
    "method": "simulated_keypresses",
    "clipboard_output": false
  },
  "error_handling": {
    "auto_recovery": true,
    "max_recovery_attempts": 3,
    "recovery_delay_ms": 500
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
      { "phrase": "caps on", "action": "{CAPSLOCK}" },
      { "phrase": "caps off", "action": "{CAPSLOCK}" },
      { "phrase": "open parenthesis", "action": "(", "aliases": ["left parenthesis"] },
      { "phrase": "close parenthesis", "action": ")", "aliases": ["right parenthesis"] },
      { "phrase": "open bracket", "action": "[", "aliases": ["left bracket"] },
      { "phrase": "close bracket", "action": "]", "aliases": ["right bracket"] },
      { "phrase": "open brace", "action": "{", "aliases": ["left brace"] },
      { "phrase": "close brace", "action": "}", "aliases": ["right brace"] },
      { "phrase": "quote", "action": "\"", "aliases": ["double quote"] },
      { "phrase": "single quote", "action": "'" },
      { "phrase": "backslash", "action": "\\" },
      { "phrase": "forward slash", "action": "/", "aliases": ["slash"] },
      { "phrase": "delete", "action": "{DELETE}" },
      { "phrase": "backspace", "action": "{BACKSPACE}" },
      { "phrase": "underscore", "action": "_" },
      { "phrase": "hyphen", "action": "-", "aliases": ["dash"] },
      { "phrase": "plus", "action": "+", "aliases": ["plus sign"] },
      { "phrase": "equals", "action": "=", "aliases": ["equal sign"] },
      { "phrase": "at sign", "action": "@", "aliases": ["at"] },
      { "phrase": "hash", "action": "#", "aliases": ["pound", "number sign"] }
    ]
  }
}
```

---

### 11. File Structure
```
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
│       └── vosk-model-en-us-0.22/   # Vosk model
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
```

---

### 12. Implementation Notes

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