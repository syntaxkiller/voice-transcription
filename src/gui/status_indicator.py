#!/usr/bin/env python3
from PyQt5.QtWidgets import QWidget, QVBoxLayout, QLabel
from PyQt5.QtGui import QPainter, QColor, QBrush, QPen
from PyQt5.QtCore import Qt, QTimer, QSize, pyqtProperty

class StatusIndicator(QWidget):
    """Widget that indicates transcription status visually"""
    def __init__(self, parent=None):
        super().__init__(parent)
        self._init_ui()
        self._state = "idle"  # idle, active, error
        self._color = QColor(100, 100, 100)  # Default gray
        self._pulsate = False
        self._pulse_opacity = 1.0
        self._pulse_increasing = False
        
        # Pulsate timer
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._update_pulse)
        self._timer.setInterval(100)
        
    def _init_ui(self):
        """Initialize user interface"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        self.setFixedSize(50, 50)
        
    def set_state(self, state):
        """Set the current state (idle, active, error)"""
        self._state = state
        
        if state == "idle":
            self._color = QColor(100, 100, 100)  # Gray
            self._pulsate = False
            self._timer.stop()
        elif state == "active":
            self._color = QColor(0, 150, 0)  # Green
            self._pulsate = True
            self._pulse_opacity = 1.0
            self._pulse_increasing = False
            self._timer.start()
        elif state == "error":
            self._color = QColor(200, 0, 0)  # Red
            self._pulsate = True
            self._pulse_opacity = 1.0
            self._pulse_increasing = False
            self._timer.start()
        
        self.update()
        
    def _update_pulse(self):
        """Update the pulse effect"""
        if not self._pulsate:
            return
            
        if self._pulse_increasing:
            self._pulse_opacity += 0.1
            if self._pulse_opacity >= 1.0:
                self._pulse_opacity = 1.0
                self._pulse_increasing = False
        else:
            self._pulse_opacity -= 0.1
            if self._pulse_opacity <= 0.5:
                self._pulse_opacity = 0.5
                self._pulse_increasing = True
                
        self.update()
        
    def paintEvent(self, event):
        """Paint the status indicator"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        # Draw background (white circle)
        painter.setPen(Qt.NoPen)
        painter.setBrush(QBrush(QColor(255, 255, 255)))
        painter.drawEllipse(5, 5, 40, 40)
        
        # Draw state indicator
        color = QColor(self._color)
        if self._pulsate:
            color.setAlphaF(self._pulse_opacity)
            
        painter.setBrush(QBrush(color))
        painter.setPen(QPen(Qt.black, 1))
        
        if self._state == "idle":
            # Draw circle
            painter.drawEllipse(10, 10, 30, 30)
        elif self._state == "active":
            # Draw microphone icon
            painter.drawRoundedRect(15, 10, 20, 25, 5, 5)
            painter.drawEllipse(25, 35, 10, 10)
        elif self._state == "error":
            # Draw X
            painter.setPen(QPen(color, 4))
            painter.drawLine(15, 15, 35, 35)
            painter.drawLine(35, 15, 15, 35)
            
    def sizeHint(self):
        """Suggested size for the widget"""
        return QSize(50, 50)