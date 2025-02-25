#!/usr/bin/env python3
import sys
import os
from PyQt5.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QComboBox, QPushButton, 
    QLabel, QMessageBox, QSizePolicy
)
from PyQt5.QtCore import pyqtSignal, Qt

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

class DeviceSelector(QWidget):
    """Widget for selecting audio input devices"""
    device_selected = pyqtSignal(int)  # Emits device ID when selected
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.logger = setup_logger("device_selector")
        self.available_devices = []
        self._init_ui()
        self.refresh_devices()
        
    def _init_ui(self):
        """Initialize user interface"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        
        # Description label
        description = QLabel("Select an audio input device:")
        layout.addWidget(description)
        
        # Device combo box and refresh button
        device_layout = QHBoxLayout()
        
        self.device_combo = QComboBox()
        self.device_combo.setSizePolicy(
            QSizePolicy.Expanding, 
            QSizePolicy.Fixed
        )
        self.device_combo.currentIndexChanged.connect(self._on_device_selected)
        
        self.refresh_button = QPushButton("Refresh")
        self.refresh_button.clicked.connect(self.refresh_devices)
        
        device_layout.addWidget(self.device_combo)
        device_layout.addWidget(self.refresh_button)
        layout.addLayout(device_layout)
        
        # Device info label
        self.device_info = QLabel()
        self.device_info.setWordWrap(True)
        layout.addWidget(self.device_info)
        
    def refresh_devices(self):
        """Refresh the list of available audio devices"""
        try:
            # Get all devices
            self.available_devices = backend.ControlledAudioStream.enumerate_devices()
            
            # Filter for compatible devices (16000 Hz sample rate)
            compatible_devices = [
                device for device in self.available_devices 
                if backend.ControlledAudioStream.check_device_compatibility(
                    device.id, 16000  # Required sample rate
                )
            ]
            
            # Update combo box
            self.device_combo.clear()
            
            if not compatible_devices:
                self.device_combo.addItem("No compatible devices found", -1)
                self.device_info.setText(
                    "No compatible audio input devices found. Please connect a microphone that supports 16000 Hz sampling."
                )
                return
                
            default_index = 0
            
            for i, device in enumerate(compatible_devices):
                # Format: "Built-in Microphone [default]"
                device_name = device.label
                if device.is_default:
                    device_name += " [default]"
                    default_index = i
                    
                self.device_combo.addItem(device_name, device.id)
                
            # Select default device
            self.device_combo.setCurrentIndex(default_index)
            
            # Show device info
            self._update_device_info()
            
        except Exception as e:
            self.logger.error(f"Error refreshing devices: {str(e)}")
            QMessageBox.warning(
                self,
                "Device Error",
                f"Error refreshing audio devices: {str(e)}"
            )
            
    def _on_device_selected(self, index):
        """Handle device selection"""
        if index < 0:
            return
            
        device_id = self.device_combo.itemData(index)
        if device_id is not None and device_id >= 0:
            self.device_selected.emit(device_id)
            self._update_device_info()
            
    def _update_device_info(self):
        """Update the device information display"""
        index = self.device_combo.currentIndex()
        if index < 0:
            return
            
        device_id = self.device_combo.itemData(index)
        if device_id is None or device_id < 0:
            self.device_info.setText("No device selected")
            return
            
        # Find the device in our list
        device = next((d for d in self.available_devices if d.id == device_id), None)
        if not device:
            self.device_info.setText("Device information not available")
            return
            
        # Display device info
        info_text = f"""
        <b>Device:</b> {device.raw_name}
        <b>Sample Rates:</b> {', '.join(str(rate) for rate in device.supported_sample_rates)} Hz
        <b>Default Device:</b> {'Yes' if device.is_default else 'No'}
        """
        self.device_info.setText(info_text)
        
    def get_selected_device_id(self):
        """Get the currently selected device ID"""
        index = self.device_combo.currentIndex()
        if index < 0:
            return None
            
        return self.device_combo.itemData(index)