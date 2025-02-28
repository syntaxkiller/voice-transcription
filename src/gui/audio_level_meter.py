#!/usr/bin/env python3
from PyQt5.QtWidgets import QWidget
from PyQt5.QtGui import QPainter, QColor, QLinearGradient, QPen
from PyQt5.QtCore import Qt, QTimer, QRect, QSize, pyqtProperty, pyqtSignal
import numpy as np
import time

class AudioLevelMeter(QWidget):
    """Widget that displays audio input levels visually"""
    
    levelChanged = pyqtSignal(float)  # Signal for level changes
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self._level = 0.0  # Current level (0.0-1.0)
        self._peak_level = 0.0  # Peak level
        self._peak_hold_time = 1000  # Peak hold time in milliseconds
        self._peak_decay_rate = 0.001  # How fast the peak level decays per ms
        self._last_peak_time = 0  # Last time the peak was updated
        
        # Level history for moving average
        self._level_history = np.zeros(10)
        self._history_index = 0
        
        # Timer for animation and peak decay
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._update_peak)
        self._timer.start(16)  # ~60 fps
        
        # Set minimum size for the widget
        self.setMinimumSize(20, 100)
        
    def _update_peak(self):
        """Update peak level with decay"""
        current_time = time.time() * 1000
        elapsed = current_time - self._last_peak_time
        
        # Decay peak after hold time
        if self._peak_level > self._level and elapsed > self._peak_hold_time:
            decay_amount = self._peak_decay_rate * elapsed
            self._peak_level = max(self._level, self._peak_level - decay_amount)
            self.update()  # Redraw with new peak level
            
        self._last_peak_time = current_time
        
    def set_level(self, level):
        """Set the current audio level (0.0-1.0)"""
        # Normalize level to 0.0-1.0
        level = max(0.0, min(1.0, level))
        
        # Add to moving average
        self._level_history[self._history_index] = level
        self._history_index = (self._history_index + 1) % len(self._level_history)
        
        # Calculate smoothed level
        smoothed_level = np.mean(self._level_history)
        
        # Update current level
        if smoothed_level != self._level:
            self._level = smoothed_level
            
            # Update peak level if needed
            if smoothed_level > self._peak_level:
                self._peak_level = smoothed_level
                self._last_peak_time = time.time() * 1000
                
            # Emit signal
            self.levelChanged.emit(self._level)
            
            # Redraw
            self.update()
            
    def get_level(self):
        """Get current audio level (0.0-1.0)"""
        return self._level
        
    def sizeHint(self):
        """Suggested size for the widget"""
        return QSize(20, 150)
        
    def paintEvent(self, event):
        """Paint the level meter"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        # Get widget dimensions
        w = self.width()
        h = self.height()
        
        # Determine if vertical or horizontal based on dimensions
        vertical = h > w
        
        # Calculate bar dimensions
        if vertical:
            bar_height = int(h * self._level)
            peak_height = int(h * self._peak_level)
            meter_rect = QRect(0, h - bar_height, w, bar_height)
            peak_rect = QRect(0, h - peak_height, w, 2)
        else:
            bar_width = int(w * self._level)
            peak_width = int(w * self._peak_level)
            meter_rect = QRect(0, 0, bar_width, h)
            peak_rect = QRect(peak_width - 2, 0, 2, h)
        
        # Create gradient for the meter
        gradient = QLinearGradient()
        if vertical:
            gradient.setStart(0, h)
            gradient.setFinalStop(0, 0)
        else:
            gradient.setStart(0, 0)
            gradient.setFinalStop(w, 0)
            
        # Color stops for gradient
        gradient.setColorAt(0.0, QColor(0, 255, 0))  # Green at the bottom/left
        gradient.setColorAt(0.6, QColor(255, 255, 0))  # Yellow in the middle
        gradient.setColorAt(0.8, QColor(255, 128, 0))  # Orange
        gradient.setColorAt(1.0, QColor(255, 0, 0))    # Red at the top/right
        
        # Draw background
        painter.setPen(Qt.NoPen)
        painter.setBrush(QColor(20, 20, 20))
        painter.drawRect(0, 0, w, h)
        
        # Draw level meter
        painter.setBrush(gradient)
        painter.drawRect(meter_rect)
        
        # Draw peak indicator
        painter.setBrush(QColor(255, 255, 255))
        painter.drawRect(peak_rect)
        
        # Draw level markings
        painter.setPen(QPen(QColor(80, 80, 80), 1))
        
        # Draw level markers every 10%
        for i in range(1, 10):
            level = i / 10.0
            if vertical:
                y = h - int(h * level)
                painter.drawLine(0, y, w, y)
            else:
                x = int(w * level)
                painter.drawLine(x, 0, x, h)
        
    # Define level as a Qt property
    level = pyqtProperty(float, get_level, set_level)
    
    def calculate_level_from_audio_chunk(self, audio_chunk):
        """Calculate audio level from an audio chunk"""
        if not audio_chunk or audio_chunk.size() == 0:
            return 0.0
            
        # Convert audio_chunk to numpy array (if needed)
        # Note: This assumes audio_chunk.data() returns a buffer of float samples
        data = np.frombuffer(audio_chunk.data(), dtype=np.float32)
        
        # Calculate RMS (root mean square) amplitude
        rms = np.sqrt(np.mean(np.square(data)))
        
        # Convert to dB scale (optional)
        # Note: -60 dB is considered silence, 0 dB is maximum level
        if rms > 0:
            db = 20 * np.log10(rms)
            # Normalize to 0.0-1.0 range (-60dB to 0dB)
            level = (db + 60) / 60
            level = max(0.0, min(1.0, level))
        else:
            level = 0.0
            
        return level

class AudioLevelMonitor:
    """Helper class to monitor audio levels from the audio stream"""
    
    def __init__(self, audio_stream, level_meter):
        self.audio_stream = audio_stream
        self.level_meter = level_meter
        self.is_monitoring = False
        self.level_timer = QTimer()
        self.level_timer.timeout.connect(self._update_level)
        
    def start_monitoring(self):
        """Start monitoring audio levels"""
        if not self.is_monitoring and self.audio_stream and self.audio_stream.is_active():
            self.is_monitoring = True
            self.level_timer.start(50)  # Update every 50ms
            
    def stop_monitoring(self):
        """Stop monitoring audio levels"""
        self.is_monitoring = False
        self.level_timer.stop()
        self.level_meter.set_level(0.0)
        
    def _update_level(self):
        """Update the level meter with the current audio level"""
        if not self.is_monitoring or not self.audio_stream or not self.audio_stream.is_active():
            self.stop_monitoring()
            return
            
        # Get next audio chunk
        chunk = self.audio_stream.get_next_chunk(10)  # Small timeout
        
        if chunk:
            # Calculate level from chunk
            level = self.level_meter.calculate_level_from_audio_chunk(chunk)
            
            # Update level meter
            self.level_meter.set_level(level)