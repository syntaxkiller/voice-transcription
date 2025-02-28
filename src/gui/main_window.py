#!/usr/bin/env python3
import sys
import os
import json
import threading
import time
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from .audio_level_meter import AudioLevelMeter, AudioLevelMonitor
from utils.enhanced_command_processor import EnhancedCommandProcessor
from utils.error_recovery_system import ErrorRecoveryManager, ErrorCategory

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QPushButton, QLabel, QComboBox, QCheckBox, QGroupBox,
    QSystemTrayIcon, QMenu, QAction, QMessageBox, QStatusBar
)
from PyQt5.QtCore import Qt, QTimer, pyqtSignal, pyqtSlot, QObject
from PyQt5.QtGui import QIcon, QFont

# Import custom modules
from .device_selector import DeviceSelector
from .shortcut_capture import ShortcutCaptureDialog
from .status_indicator import StatusIndicator

# Import utils
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from utils.logger import setup_logger
from utils.command_processor import CommandProcessor

# Import C++ backend
try:
    import voice_transcription_backend as backend
except ImportError:
    # Show a meaningful error message if the backend module is not found
    print("Error: Could not import voice_transcription_backend module")
    print("Make sure the C++ backend is compiled and installed correctly")
    sys.exit(1)

# Configuration path
CONFIG_PATH = Path(__file__).parents[2] / "config" / "settings.json"

class SignalEmitter(QObject):
    """Helper class for emitting signals from non-Qt threads"""
    shortcut_detected_signal = pyqtSignal()

class TranscriptionController(QObject):
    """Controller class for handling transcription operations"""
    transcription_signal = pyqtSignal(object)  # TranscriptionResult
    audio_error_signal = pyqtSignal(dict)
    transcription_error_signal = pyqtSignal(dict)
    output_error_signal = pyqtSignal(dict)
    focus_changed_signal = pyqtSignal(str)
    device_list_changed_signal = pyqtSignal()
    
    def __init__(self, config):
        super().__init__()
        self.config = config
        self.logger = setup_logger("transcription_controller")
        self.use_noise_filtering = True
        self.is_transcribing = False
        self.audio_stream = None
        self.vad_handler = None
        self.transcriber = None
        self.keyboard_simulator = None
        self.command_processor = EnhancedCommandProcessor(
            self.config.get("dictation_commands", {}).get("supported_commands", []),
            config_path=CONFIG_PATH
        )
        self.thread_pool = ThreadPoolExecutor(max_workers=3)
        self.stop_event = threading.Event()
        self.window_manager = None
        self.error_recovery = ErrorRecoveryManager(self)
        
    def initialize(self):
        """Initialize transcription components"""
        try:
            # Initialize window manager for device change notifications
            self.window_manager = backend.WindowManager()
            self.window_manager.set_device_change_callback(self._on_device_change)
            self.window_manager.create_hidden_window()
            
            # Start window manager message loop in a separate thread
            self.thread_pool.submit(self.window_manager.message_loop)
            
            # Initialize keyboard simulator
            self.keyboard_simulator = backend.KeyboardSimulator()
            
            # Initialize VAD handler
            sample_rate = self.config["audio"]["sample_rate"]
            vad_aggressiveness = self.config["audio"]["vad_aggressiveness"]
            frame_duration_ms = 20  # 20ms frames for WebRTC VAD
            self.vad_handler = backend.VADHandler(sample_rate, frame_duration_ms, vad_aggressiveness)
            
            # Initialize Vosk transcriber
            model_path = str(Path(__file__).parents[2] / self.config["transcription"]["model_path"])
            self.logger.info(f"Loading Vosk model from: {model_path}")
            self.transcriber = backend.VoskTranscriber(model_path, sample_rate)
            
            # Start model loading progress monitoring
            self._start_model_loading_progress_monitoring()
            
            self.logger.info("Transcription controller initialized successfully")
            return True
            
        except Exception as e:
            self.logger.error(f"Failed to initialize transcription controller: {str(e)}")
            self.transcription_error_signal.emit({"code": "INIT_ERROR", "message": str(e)})
            return False
    
    def toggle_noise_filtering(self, enabled):
        """Toggle noise filtering on/off"""
        self.use_noise_filtering = enabled
        if self.transcriber:
            self.transcriber.enable_noise_filtering(enabled)
        return True

    def _start_model_loading_progress_monitoring(self):
        """Start a thread to monitor model loading progress"""
        def monitor_progress():
            try:
                last_progress = 0.0
                while self.transcriber.is_loading():
                    # Get current progress
                    progress = self.transcriber.get_loading_progress()
                    
                    # Only emit signal if progress has changed significantly
                    if abs(progress - last_progress) >= 0.05:  # 5% change
                        last_progress = progress
                        # Emit status update signal
                        self.transcription_error_signal.emit({
                            "code": "MODEL_LOADING", 
                            "message": f"Loading Vosk model... {int(progress * 100)}%"
                        })
                    
                    time.sleep(0.2)  # Check progress every 200ms
                
                # Final check after loading completes
                if self.transcriber.is_model_loaded():
                    self.transcription_error_signal.emit({
                        "code": "MODEL_LOADED", 
                        "message": "Vosk model loaded successfully"
                    })
                else:
                    error_msg = f"Failed to load Vosk model: {self.transcriber.get_last_error()}"
                    self.logger.error(error_msg)
                    self.transcription_error_signal.emit({
                        "code": "MODEL_LOAD_ERROR", 
                        "message": error_msg
                    })
            except Exception as e:
                self.logger.error(f"Error in model loading progress monitoring: {str(e)}")
        
        # Start the monitoring thread
        self.thread_pool.submit(monitor_progress)

    def _on_transcription_error(self, error):
        """Called when a transcription error occurs"""
        self.logger.error(f"Transcription error: {error['code']} - {error['message']}")
        
        # Handle model loading progress updates
        if error['code'] == "MODEL_LOADING":
            # Update status bar with loading progress
            self.status_bar.showMessage(error['message'])
            return
        elif error['code'] == "MODEL_LOADED":
            # Update status bar with success message
            self.status_bar.showMessage(error['message'], 3000)  # Show for 3 seconds
            return
        
        # Handle other errors with dialog
        QMessageBox.warning(
            self, 
            "Transcription Error", 
            f"Transcription error: {error['message']}"
        )

    def cleanup(self):
        """Clean up resources"""
        self.stop_transcription()
        if self.window_manager:
            self.window_manager.destroy_hidden_window()
        self.thread_pool.shutdown(wait=False)
        
    def start_transcription(self, device_id):
        """Start the transcription process"""
        if self.is_transcribing:
            return
            
        self.stop_event.clear()
        self.is_transcribing = True
        
        try:
            # Initialize audio stream
            sample_rate = self.config["audio"]["sample_rate"]
            frames_per_buffer = self.config["audio"]["frames_per_buffer"]
            self.audio_stream = backend.ControlledAudioStream(device_id, sample_rate, frames_per_buffer)
            
            if not self.audio_stream.start():
                error_msg = f"Failed to start audio stream: {self.audio_stream.get_last_error()}"
                self.logger.error(error_msg)
                
                # Use error recovery system
                error_details = self.error_recovery.handle_error(
                    ErrorCategory.AUDIO_DEVICE,
                    "STREAM_START_ERROR",
                    error_msg,
                    context={"device_id": device_id}
                )
                
                # Only emit error signal if recovery failed
                if not error_details.get("recovery_success", False):
                    self.audio_error_signal.emit(error_details)
                    self.is_transcribing = False
                    return False
                
            # Start transcription thread
            self.thread_pool.submit(self._transcription_thread)
            self.logger.info(f"Started transcription with device ID: {device_id}")
            return True
            
        except Exception as e:
            self.logger.error(f"Failed to start transcription: {str(e)}")
            
            # Use error recovery system
            error_details = self.error_recovery.handle_error(
                ErrorCategory.GENERAL,
                "START_ERROR",
                f"Failed to start transcription: {str(e)}",
                exception=e
            )
            
            self.audio_error_signal.emit(error_details)
            self.is_transcribing = False
            return False
    
    def stop_transcription(self):
        """Stop the transcription process"""
        if not self.is_transcribing:
            return
            
        self.stop_event.set()
        self.is_transcribing = False
        
        if self.audio_stream:
            self.audio_stream.stop()
            self.audio_stream = None
            
        self.logger.info("Stopped transcription")
    
    def toggle_transcription(self, device_id):
        """Toggle transcription on/off"""
        if self.is_transcribing:
            self.stop_transcription()
            return False
        else:
            return self.start_transcription(device_id)

    def _transcription_thread(self):
        """Thread function for audio processing and transcription"""
        self.logger.info("Transcription thread started")
        hangover_timeout_ms = self.config["audio"]["hangover_timeout_ms"]
        hangover_counter = 0
        silence_time = 0
        last_speech_time = 0
        speech_detected = False
        
        try:
            while not self.stop_event.is_set() and self.audio_stream and self.audio_stream.is_active():
                # Check for window focus change
                if self.config["ui"]["pause_on_active_window_change"]:
                    current_window = backend.WindowManager.get_foreground_window_title()
                    self.focus_changed_signal.emit(current_window)
                
                # Get next audio chunk
                chunk = self.audio_stream.get_next_chunk()
                if not chunk:
                    time.sleep(0.01)  # Small sleep to prevent CPU hogging
                    continue
                
                # Check for speech using VAD
                is_speech = self.vad_handler.is_speech(chunk)
                current_time = time.time() * 1000  # Current time in milliseconds
                
                if is_speech:
                    speech_detected = True
                    hangover_counter = hangover_timeout_ms
                    last_speech_time = current_time
                elif speech_detected:
                    # Handle hangover period
                    hangover_counter -= 20  # 20ms per frame
                    silence_time = current_time - last_speech_time
                    
                    if hangover_counter <= 0 and silence_time > hangover_timeout_ms:
                        # End of speech detected
                        speech_detected = False
                
                # Process with transcriber - using noise filtering if enabled
                if speech_detected or hangover_counter > 0:
                    if self.use_noise_filtering:
                        result = self.transcriber.transcribe_with_noise_filtering(chunk, is_speech)
                    else:
                        result = self.transcriber.transcribe_with_vad(chunk, is_speech)

                    # Process commands in the transcription
                    if result.raw_text:
                        # Get foreground application for context
                        current_window = backend.WindowManager.get_foreground_window_title()
                        
                        context = {"application_name": current_window}
                        result.processed_text = self.command_processor.process_with_context(
                            result.raw_text, context
                        )
                        
                        # Emit result for GUI updates
                        self.transcription_signal.emit(result)
                        
                        # Output text if it's a final result
                        if result.is_final and result.processed_text:
                            self._output_text(result.processed_text)
        except Exception as e:
            self.logger.error(f"Error in transcription thread: {str(e)}")
            
            # Use error recovery system
            error_details = self.error_recovery.handle_error(
                ErrorCategory.TRANSCRIPTION,
                "TRANSCRIPTION_ERROR",
                f"Error during transcription: {str(e)}",
                exception=e
            )
            
            self.transcription_error_signal.emit(error_details)
        finally:
            self.is_transcribing = False
            self.logger.info("Transcription thread stopped")
    
    def _output_text(self, text):
        """Output text via keypresses or clipboard"""
        try:
            output_method = self.config["output"]["method"]
            if output_method == "simulated_keypresses":
                delay_ms = self.config["transcription"]["keypress_delay_ms"]
                success = self.keyboard_simulator.simulate_keypresses(text, delay_ms)
                
                # Check for failure and try recovery
                if not success:
                    error_msg = "Failed to simulate keypresses"
                    self.logger.error(error_msg)
                    
                    # Use error recovery system
                    error_details = self.error_recovery.handle_error(
                        ErrorCategory.OUTPUT,
                        "OUTPUT_ERROR",
                        error_msg,
                        context={"text": text[:20] + "..." if len(text) > 20 else text}
                    )
                    
                    if error_details.get("recovery_success", False):
                        # Retry with new method from recovery
                        self._output_text(text)
                    else:
                        self.output_error_signal.emit(error_details)
                        
            elif output_method == "clipboard":
                success = backend.set_clipboard_text(text)
                
                # Similar error handling for clipboard method
                if not success:
                    error_msg = "Failed to set clipboard text"
                    self.logger.error(error_msg)
                    
                    error_details = self.error_recovery.handle_error(
                        ErrorCategory.OUTPUT,
                        "OUTPUT_ERROR",
                        error_msg,
                        context={"text": text[:20] + "..." if len(text) > 20 else text}
                    )
                    
                    if error_details.get("recovery_success", False):
                        self._output_text(text)
                    else:
                        self.output_error_signal.emit(error_details)
                        
            else:
                self.logger.warning(f"Unknown output method: {output_method}")
                
        except Exception as e:
            self.logger.error(f"Error outputting text: {str(e)}")
            
            # Use error recovery system
            error_details = self.error_recovery.handle_error(
                ErrorCategory.OUTPUT,
                "OUTPUT_ERROR",
                f"Error outputting text: {str(e)}",
                exception=e,
                context={"text": text[:20] + "..." if len(text) > 20 else text}
            )
            
            self.output_error_signal.emit(error_details)
    
    def _on_device_change(self):
        """Callback for device change notifications"""
        self.logger.info("Audio device change detected")
        self.audio_error_signal.emit({"code": "DEVICE_CHANGE", "message": "Audio device change detected"})
        self.device_list_changed_signal.emit()
        
class MainWindow(QMainWindow):
    """Main application window"""
    def __init__(self):
        super().__init__()
        self.logger = setup_logger("main_window")
        self.config = self._load_config()
        self.signal_emitter = SignalEmitter()
        self.controller = TranscriptionController(self.config)
        self.audio_monitor = None  # Will be created when transcription starts

        # Initialize UI
        self._init_ui()
        
        # Connect signals
        self._connect_signals()
        
        # Initialize controller
        if not self.controller.initialize():
            QMessageBox.critical(
                self, 
                "Initialization Error", 
                "Failed to initialize transcription controller. Check logs for details."
            )
            
        # Register global hotkey
        self._register_global_hotkey()
        
    def _init_ui(self):
        """Initialize user interface"""
        self.setWindowTitle("Voice Transcription")
        self.setGeometry(100, 100, 600, 400)
        
        # Central widget
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)
        
        # Device selection group
        device_group = QGroupBox("Audio Input Device")
        device_layout = QVBoxLayout(device_group)
        
        self.device_selector = DeviceSelector()
        device_layout.addWidget(self.device_selector)
        
        # Shortcut group
        shortcut_group = QGroupBox("Transcription Shortcut")
        shortcut_layout = QHBoxLayout(shortcut_group)
        
        self.shortcut_label = QLabel("Current Shortcut: ")
        self.shortcut_value = QLabel(self._format_shortcut())
        self.shortcut_button = QPushButton("Change Shortcut")
        
        shortcut_layout.addWidget(self.shortcut_label)
        shortcut_layout.addWidget(self.shortcut_value)
        shortcut_layout.addWidget(self.shortcut_button)
        
        # Status group with audio level meter
        status_group = QGroupBox("Transcription Status")
        status_layout = QHBoxLayout(status_group)

        # Left side: status indicator and text
        left_panel = QVBoxLayout()
        left_panel.addWidget(self.status_indicator)
        left_panel.addWidget(self.status_text)
        left_panel.addWidget(self.toggle_button)

        # Right side: audio level meter
        right_panel = QVBoxLayout()
        self.level_meter = AudioLevelMeter()
        self.level_meter.setFixedWidth(20)
        level_label = QLabel("Audio Level")
        level_label.setAlignment(Qt.AlignCenter)
        right_panel.addWidget(self.level_meter)
        right_panel.addWidget(level_label)

        # Add both panels to status layout
        status_layout.addLayout(left_panel, 2)  # 2/3 of the width
        status_layout.addLayout(right_panel, 1)  # 1/3 of the width

        # Add status group to main layout
        main_layout.addWidget(status_group)
        
        self.status_indicator = StatusIndicator()
        self.status_text = QLabel("Idle")
        self.status_text.setAlignment(Qt.AlignCenter)
        self.toggle_button = QPushButton("Start Transcription")
        
        status_layout.addWidget(self.status_indicator)
        status_layout.addWidget(self.status_text)
        status_layout.addWidget(self.toggle_button)
        
        # Options group
        options_group = QGroupBox("Options")
        options_layout = QVBoxLayout(options_group)
        
        self.pause_on_window_change = QCheckBox("Pause on active window change")
        self.pause_on_window_change.setChecked(self.config["ui"]["pause_on_active_window_change"])
        
        self.use_clipboard = QCheckBox("Use clipboard instead of keypresses")
        self.use_clipboard.setChecked(self.config["output"]["clipboard_output"])
        
        options_layout.addWidget(self.pause_on_window_change)
        options_layout.addWidget(self.use_clipboard)

        self.auto_recovery = QCheckBox("Enable automatic error recovery")
        self.auto_recovery.setChecked(self.config.get("error_handling", {}).get("auto_recovery", True))
        self.auto_recovery.stateChanged.connect(self._update_auto_recovery_option)

        options_layout.addWidget(self.auto_recovery)

        # Add all groups to main layout
        main_layout.addWidget(device_group)
        main_layout.addWidget(shortcut_group)
        main_layout.addWidget(status_group)
        main_layout.addWidget(options_group)
        
        # Status bar
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("Ready")
        
        # System tray
        self.tray_icon = QSystemTrayIcon(self)
        self.tray_icon.setIcon(QIcon(str(Path(__file__).parent.parent.parent / "assets" / "icons" / "app_icon.png")))
        
        tray_menu = QMenu()
        show_action = QAction("Show", self)
        quit_action = QAction("Quit", self)
        toggle_action = QAction("Start Transcription", self)
        
        show_action.triggered.connect(self.show)
        quit_action.triggered.connect(self.close)
        toggle_action.triggered.connect(self._toggle_transcription)
        
        tray_menu.addAction(show_action)
        tray_menu.addAction(toggle_action)
        tray_menu.addSeparator()
        tray_menu.addAction(quit_action)
        
        self.tray_icon.setContextMenu(tray_menu)
        self.tray_icon.show()

    def _update_auto_recovery_option(self, state):
        """Update auto recovery option"""
        enabled = bool(state)
        if "error_handling" not in self.config:
            self.config["error_handling"] = {}
        self.config["error_handling"]["auto_recovery"] = enabled
        self._save_config()

    def _connect_signals(self):
        """Connect signals and slots"""
        # Button signals
        self.toggle_button.clicked.connect(self._toggle_transcription)
        self.shortcut_button.clicked.connect(self._change_shortcut)
        
        # Device selector signals
        self.device_selector.device_selected.connect(self._on_device_selected)
        
        # Checkbox signals
        self.pause_on_window_change.stateChanged.connect(self._update_window_change_option)
        self.use_clipboard.stateChanged.connect(self._update_clipboard_option)
        
        # Controller signals
        self.controller.transcription_signal.connect(self._on_transcription)
        self.controller.audio_error_signal.connect(self._on_audio_error)
        self.controller.transcription_error_signal.connect(self._on_transcription_error)
        self.controller.output_error_signal.connect(self._on_output_error)
        self.controller.focus_changed_signal.connect(self._on_focus_changed)
        self.controller.device_list_changed_signal.connect(self._on_device_list_changed)
        
        # Global shortcut signal
        self.signal_emitter.shortcut_detected_signal.connect(self._on_shortcut_detected)
        
    def _load_config(self):
        """Load configuration from file"""
        try:
            with open(CONFIG_PATH, 'r') as f:
                return json.load(f)
        except Exception as e:
            self.logger.error(f"Failed to load config: {str(e)}")
            QMessageBox.warning(
                self, 
                "Configuration Error", 
                f"Failed to load configuration: {str(e)}\nUsing default settings."
            )
            # Return default config
            return {
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
                    "pause_on_active_window_change": False,
                    "confirmation_feedback": True
                },
                "output": {
                    "method": "simulated_keypresses",
                    "clipboard_output": False
                },
                "dictation_commands": {
                    "supported_commands": []
                }
            }
            
    def _save_config(self):
        """Save configuration to file"""
        try:
            with open(CONFIG_PATH, 'w') as f:
                json.dump(self.config, f, indent=2)
            self.logger.info("Configuration saved")
        except Exception as e:
            self.logger.error(f"Failed to save config: {str(e)}")
            QMessageBox.warning(
                self, 
                "Configuration Error", 
                f"Failed to save configuration: {str(e)}"
            )
            
    def _format_shortcut(self):
        """Format shortcut for display"""
        shortcut = self.config["shortcut"]
        modifiers = " + ".join(shortcut["modifiers"])
        return f"{modifiers} + {shortcut['key']}"
        
    def _register_global_hotkey(self):
        """Register global hotkey for transcription toggle"""
        try:
            shortcut = backend.Shortcut()
            shortcut.modifiers = self.config["shortcut"]["modifiers"]
            shortcut.key = self.config["shortcut"]["key"]
            shortcut.is_valid = True
            
            if not backend.KeyboardSimulator.register_global_hotkey(shortcut):
                self.logger.error("Failed to register global hotkey")
                QMessageBox.warning(
                    self, 
                    "Shortcut Error", 
                    "Failed to register global hotkey. Please try a different shortcut."
                )
                return False
                
            # Set up a thread to monitor for the shortcut
            self._start_hotkey_listener()
            return True
            
        except Exception as e:
            self.logger.error(f"Error registering global hotkey: {str(e)}")
            return False
            
    def _start_hotkey_listener(self):
        """Start a thread to listen for global hotkey presses"""
        def hotkey_thread():
            try:
                # Create a message-only window for hotkey messages
                window_manager = backend.WindowManager()
                window_manager.create_hidden_window()
                
                # Set up a callback for hotkey messages
                def on_hotkey():
                    # Signal the main thread to toggle transcription
                    self.signal_emitter.shortcut_detected_signal.emit()
                
                # Register for hotkey messages in the window's message loop
                # This is handled by the C++ WindowManager class in its window_proc method
                
                # Start the message loop
                window_manager.message_loop()
                
            except Exception as e:
                self.logger.error(f"Error in hotkey listener thread: {str(e)}")
        
        # Start the thread
        thread = threading.Thread(target=hotkey_thread, daemon=True)
        thread.start()
        
    def _on_shortcut_detected(self):
        """Called when the global shortcut is detected"""
        self._toggle_transcription()
        
    def _change_shortcut(self):
        """Open dialog to capture a new shortcut"""
        dialog = ShortcutCaptureDialog(self)
        if dialog.exec_():
            shortcut = dialog.get_shortcut()
            
            # Unregister old shortcut
            old_shortcut = backend.Shortcut()
            old_shortcut.modifiers = self.config["shortcut"]["modifiers"]
            old_shortcut.key = self.config["shortcut"]["key"]
            old_shortcut.is_valid = True
            backend.KeyboardSimulator.unregister_global_hotkey(old_shortcut)
            
            # Update config with new shortcut
            self.config["shortcut"]["modifiers"] = shortcut.modifiers
            self.config["shortcut"]["key"] = shortcut.key
            self._save_config()
            
            # Update display
            self.shortcut_value.setText(self._format_shortcut())
            
            # Register new shortcut
            self._register_global_hotkey()
            
    def _toggle_transcription(self):
        """Toggle transcription on/off"""
        device_id = self.device_selector.get_selected_device_id()
        
        if not device_id:
            QMessageBox.warning(
                self, 
                "Device Error", 
                "No compatible audio input device selected."
            )
            return
            
        if self.controller.is_transcribing:
            self.controller.stop_transcription()
            self.toggle_button.setText("Start Transcription")
            self.status_text.setText("Idle")
            self.status_indicator.set_state("idle")
            self.status_bar.showMessage("Transcription stopped")
            
            # Stop audio level monitoring
            if self.audio_monitor:
                self.audio_monitor.stop_monitoring()
        else:
            if self.controller.start_transcription(device_id):
                self.toggle_button.setText("Stop Transcription")
                self.status_text.setText("Transcribing...")
                self.status_indicator.set_state("active")
                self.status_bar.showMessage("Transcription started")
                
                # Start audio level monitoring
                if not self.audio_monitor:
                    self.audio_monitor = AudioLevelMonitor(
                        self.controller.audio_stream, 
                        self.level_meter
                    )
                self.audio_monitor.start_monitoring()
            else:
                QMessageBox.warning(
                    self, 
                    "Transcription Error", 
                    "Failed to start transcription. See logs for details."
                )
                
    def _on_device_selected(self, device_id):
        """Called when a device is selected in the device selector"""
        self.config["audio"]["default_device_id"] = device_id
        self._save_config()
        
    def _update_window_change_option(self, state):
        """Update the pause on window change option"""
        self.config["ui"]["pause_on_active_window_change"] = bool(state)
        self._save_config()
        
    def _update_clipboard_option(self, state):
        """Update the clipboard output option"""
        use_clipboard = bool(state)
        self.config["output"]["clipboard_output"] = use_clipboard
        self.config["output"]["method"] = "clipboard" if use_clipboard else "simulated_keypresses"
        self._save_config()
        
    def _on_transcription(self, result):
        """Called when a transcription result is received"""
        if result.is_final:
            self.status_text.setText(f"Last: {result.processed_text}")
        else:
            self.status_text.setText(f"Partial: {result.raw_text}")
            
    def _on_audio_error(self, error):
        """Called when an audio error occurs"""
        self.logger.error(f"Audio error: {error['code']} - {error['message']}")
        
        # Get user-friendly message
        user_message = self.controller.error_recovery.get_user_message(error)
        
        if error["code"] == "DEVICE_CHANGE":
            self.device_selector.refresh_devices()
            
            # If device was disconnected while transcribing, try to switch to default
            if self.controller.is_transcribing:
                self.controller.stop_transcription()
                QMessageBox.warning(
                    self, 
                    "Device Disconnected", 
                    user_message
                )
                self.toggle_button.setText("Start Transcription")
                self.status_text.setText("Idle")
                self.status_indicator.set_state("idle")
        else:
            # If recovery was attempted and succeeded, show a less severe message
            if error.get("recovery_attempted", False) and error.get("recovery_success", True):
                self.status_bar.showMessage(user_message, 5000)  # Show for 5 seconds
            else:
                # Show a dialog for errors that couldn't be recovered
                QMessageBox.warning(
                    self, 
                    "Audio Error", 
                    user_message
                )

    def _on_transcription_error(self, error):
        """Called when a transcription error occurs"""
        self.logger.error(f"Transcription error: {error['code']} - {error['message']}")
        
        # Handle model loading progress updates
        if error['code'] == "MODEL_LOADING":
            # Update status bar with loading progress
            self.status_bar.showMessage(error['message'])
            return
        elif error['code'] == "MODEL_LOADED":
            # Update status bar with success message
            self.status_bar.showMessage(error['message'], 3000)  # Show for 3 seconds
            return
        
        # Get user-friendly message
        user_message = self.controller.error_recovery.get_user_message(error)
        
        # Only show dialog for errors that couldn't be recovered
        if not error.get("recovery_success", False):
            QMessageBox.warning(
                self, 
                "Transcription Error", 
                user_message
            )
        
    def _on_output_error(self, error):
        """Called when an output error occurs"""
        self.logger.error(f"Output error: {error['code']} - {error['message']}")
        QMessageBox.warning(
            self, 
            "Output Error", 
            f"Output error: {error['message']}"
        )
        
    def _on_focus_changed(self, window_title):
        """Called when the foreground window changes"""
        if self.config["ui"]["pause_on_active_window_change"] and self.controller.is_transcribing:
            self.status_bar.showMessage(f"Active window changed to: {window_title}")
            
    def _on_device_list_changed(self):
        """Called when the list of available audio devices changes"""
        self.device_selector.refresh_devices()
        
    def closeEvent(self, event):
        """Handle window close event"""
        if self.controller.is_transcribing:
            reply = QMessageBox.question(
                self, 
                "Exit Confirmation", 
                "Transcription is still active. Are you sure you want to quit?",
                QMessageBox.Yes | QMessageBox.No,
                QMessageBox.No
            )
            
            if reply == QMessageBox.No:
                event.ignore()
                return
                
        # Clean up resources
        if hasattr(self, 'controller'):
            self.controller.cleanup()
            
        # Save config
        self._save_config()
        
        # Hide tray icon
        if hasattr(self, 'tray_icon'):
            self.tray_icon.hide()
            
        event.accept()

def main():
    """Main entry point for the application"""
    app = QApplication(sys.argv)
    app.setApplicationName("Voice Transcription")
    app.setQuitOnLastWindowClosed(False)
    
    # Set application style
    app.setStyle("Fusion")
    
    # Create and show the main window
    window = MainWindow()
    window.show()
    
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()