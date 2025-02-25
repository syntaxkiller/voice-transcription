#!/usr/bin/env python3
import sys
import os
import time
from PyQt5.QtWidgets import (
    QDialog, QVBoxLayout, QLabel, QPushButton, QHBoxLayout,
    QProgressBar, QMessageBox
)
from PyQt5.QtCore import Qt, QTimer, QEvent
from PyQt5.QtGui import QKeySequence, QKeyEvent

# Import utils
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from utils.logger import setup_logger

# Import C++ backend
try:
    import voice_transcription_backend as backend
except ImportError:
    print("Error: Could not import voice_transcription_backend module")
    print("Make sure the C++ backend is compiled and installed correctly")
    sys.exit(1)

# Map Qt key codes to string representation
KEY_MAP = {
    Qt.Key_A: "A", Qt.Key_B: "B", Qt.Key_C: "C", Qt.Key_D: "D",
    Qt.Key_E: "E", Qt.Key_F: "F", Qt.Key_G: "G", Qt.Key_H: "H",
    Qt.Key_I: "I", Qt.Key_J: "J", Qt.Key_K: "K", Qt.Key_L: "L",
    Qt.Key_M: "M", Qt.Key_N: "N", Qt.Key_O: "O", Qt.Key_P: "P",
    Qt.Key_Q: "Q", Qt.Key_R: "R", Qt.Key_S: "S", Qt.Key_T: "T",
    Qt.Key_U: "U", Qt.Key_V: "V", Qt.Key_W: "W", Qt.Key_X: "X",
    Qt.Key_Y: "Y", Qt.Key_Z: "Z",
    Qt.Key_0: "0", Qt.Key_1: "1", Qt.Key_2: "2", Qt.Key_3: "3",
    Qt.Key_4: "4", Qt.Key_5: "5", Qt.Key_6: "6", Qt.Key_7: "7",
    Qt.Key_8: "8", Qt.Key_9: "9",
    Qt.Key_F1: "F1", Qt.Key_F2: "F2", Qt.Key_F3: "F3", Qt.Key_F4: "F4",
    Qt.Key_F5: "F5", Qt.Key_F6: "F6", Qt.Key_F7: "F7", Qt.Key_F8: "F8",
    Qt.Key_F9: "F9", Qt.Key_F10: "F10", Qt.Key_F11: "F11", Qt.Key_F12: "F12",
    Qt.Key_Space: "Space",
    Qt.Key_Tab: "Tab",
    Qt.Key_Return: "Enter",
    Qt.Key_Backspace: "Backspace",
    Qt.Key_Delete: "Delete",
    Qt.Key_Insert: "Insert",
    Qt.Key_Home: "Home",
    Qt.Key_End: "End",
    Qt.Key_PageUp: "PageUp",
    Qt.Key_PageDown: "PageDown",
    Qt.Key_Left: "Left",
    Qt.Key_Right: "Right",
    Qt.Key_Up: "Up",
    Qt.Key_Down: "Down",
    Qt.Key_Escape: "Escape"
}

# Map Qt modifiers to string representation
MODIFIER_MAP = {
    Qt.ControlModifier: "Ctrl",
    Qt.AltModifier: "Alt",
    Qt.ShiftModifier: "Shift",
    Qt.MetaModifier: "Win"
}

class ShortcutCaptureDialog(QDialog):
    """Dialog for capturing keyboard shortcuts"""
    def __init__(self, parent=None, timeout=3):
        super().__init__(parent)
        self.logger = setup_logger("shortcut_capture")
        self.timeout = timeout
        self.remaining_time = timeout
        self.shortcut = backend.Shortcut()
        self.shortcut.is_valid = False
        self._init_ui()
        
    def _init_ui(self):
        """Initialize user interface"""
        self.setWindowTitle("Capture Shortcut")
        self.setFixedSize(400, 200)
        self.setModal(True)
        
        layout = QVBoxLayout(self)
        
        # Instructions
        instruction_label = QLabel(
            "Press the key combination you want to use as a shortcut.\n"
            "Must include at least one modifier key (Ctrl, Alt, Shift, Win)."
        )
        instruction_label.setAlignment(Qt.AlignCenter)
        layout.addWidget(instruction_label)
        
        # Countdown progress bar
        self.progress_bar = QProgressBar()
        self.progress_bar.setRange(0, self.timeout * 1000)  # milliseconds
        self.progress_bar.setValue(self.timeout * 1000)
        layout.addWidget(self.progress_bar)
        
        # Current key combination
        self.key_label = QLabel("Press a key combination...")
        self.key_label.setAlignment(Qt.AlignCenter)
        self.key_label.setStyleSheet("font-size: 16px; font-weight: bold;")
        layout.addWidget(self.key_label)
        
        # Buttons
        button_layout = QHBoxLayout()
        self.cancel_button = QPushButton("Cancel")
        self.cancel_button.clicked.connect(self.reject)
        button_layout.addWidget(self.cancel_button)
        layout.addLayout(button_layout)
        
        # Start timer
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._update_countdown)
        self.timer.start(100)  # 100ms updates
        
        # Set focus to capture keys
        self.setFocus()
        
    def _update_countdown(self):
        """Update the countdown timer"""
        self.remaining_time -= 0.1
        self.progress_bar.setValue(int(self.remaining_time * 1000))
        
        if self.remaining_time <= 0:
            if not self.shortcut.is_valid:
                QMessageBox.warning(
                    self,
                    "Timeout",
                    "No valid shortcut was captured. Please try again."
                )
                self.reject()
            else:
                self.accept()
                
    def keyPressEvent(self, event):
        """Handle key press events"""
        # Ignore auto-repeat events
        if event.isAutoRepeat():
            return
            
        # Cancel on Escape
        if event.key() == Qt.Key_Escape and not event.modifiers():
            self.reject()
            return
            
        # Capture key combination
        key = event.key()
        modifiers = event.modifiers()
        
        # Convert to string representation
        modifiers_list = []
        for mod, mod_str in MODIFIER_MAP.items():
            if modifiers & mod:
                modifiers_list.append(mod_str)
                
        # Get main key
        key_str = KEY_MAP.get(key, "")
        
        # Skip if key is a modifier key itself
        if key in (Qt.Key_Control, Qt.Key_Alt, Qt.Key_Shift, Qt.Key_Meta):
            return
            
        # Update display
        if modifiers_list and key_str:
            shortcut_text = " + ".join(modifiers_list + [key_str])
            self.key_label.setText(shortcut_text)
            
            # Save the shortcut
            self.shortcut.modifiers = modifiers_list
            self.shortcut.key = key_str
            self.shortcut.is_valid = len(modifiers_list) > 0
            
            # Accept if valid
            if self.shortcut.is_valid:
                # Wait a moment for visualization
                QTimer.singleShot(500, self.accept)
                
    def get_shortcut(self):
        """Get the captured shortcut"""
        return self.shortcut