#!/usr/bin/env python3
import sys
import os
import traceback
from enum import Enum, auto

# Import logger
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from utils.logger import setup_logger

class ErrorCategory(Enum):
    """Enumeration of error categories"""
    AUDIO_DEVICE = auto()
    TRANSCRIPTION = auto()
    SHORTCUT = auto()
    OUTPUT = auto()
    CONFIG = auto()
    GENERAL = auto()

class ErrorHandler:
    """Centralized error handling and reporting"""
    def __init__(self):
        self.logger = setup_logger("error_handler")
        
    def handle_error(self, category, code, message, exception=None, context=None):
        """Handle and log an error"""
        # Format error details
        error_details = {
            "category": category.name,
            "code": code,
            "message": message,
            "exception": str(exception) if exception else None,
            "context": context
        }
        
        # Log the error
        self.logger.error(f"Error: {category.name} - {code}: {message}")
        
        if exception:
            self.logger.error(f"Exception: {exception}")
            self.logger.error(f"Traceback: {traceback.format_exc()}")
            
        if context:
            self.logger.error(f"Context: {context}")
            
        # Return formatted error details for UI display
        return error_details
    
    def get_user_message(self, error_details):
        """Get a user-friendly error message"""
        category = error_details.get("category", "GENERAL")
        code = error_details.get("code", "UNKNOWN")
        message = error_details.get("message", "An unknown error occurred")
        
        # Generate user-friendly message based on category and code
        if category == "AUDIO_DEVICE":
            if code == "DEVICE_CHANGE":
                return "Audio device changed or disconnected. Please check your microphone connection."
            elif code == "STREAM_START_ERROR":
                return "Failed to start audio stream. Please try a different microphone."
            elif code == "DEVICE_NOT_FOUND":
                return "Audio device not found. Please connect a microphone."
            elif code == "INCOMPATIBLE_SAMPLE_RATE":
                return "This microphone doesn't support the required sample rate (16000 Hz)."
        elif category == "TRANSCRIPTION":
            if code == "MODEL_LOAD_ERROR":
                return "Failed to load speech recognition model. Please reinstall the application."
            elif code == "TRANSCRIPTION_ERROR":
                return "Error during speech recognition. Please try again."
        elif category == "SHORTCUT":
            if code == "REGISTRATION_ERROR":
                return "Failed to register keyboard shortcut. Please try a different key combination."
            elif code == "INVALID_SHORTCUT":
                return "Invalid shortcut. Must include at least one modifier key (Ctrl, Alt, Shift, Win)."
        elif category == "OUTPUT":
            if code == "OUTPUT_ERROR":
                return "Error sending text output. Please check target application."
        elif category == "CONFIG":
            if code == "CONFIG_LOAD_ERROR":
                return "Failed to load configuration. Default settings will be used."
            elif code == "CONFIG_SAVE_ERROR":
                return "Failed to save configuration. Your settings may not persist."
                
        # Default message
        return message
    
    def get_recovery_steps(self, error_details):
        """Get suggested recovery steps for an error"""
        category = error_details.get("category", "GENERAL")
        code = error_details.get("code", "UNKNOWN")
        
        # Generate recovery steps based on category and code
        if category == "AUDIO_DEVICE":
            if code == "DEVICE_CHANGE":
                return [
                    "Reconnect your microphone",
                    "Select a different microphone from the dropdown",
                    "Click the Refresh button to rescan for devices"
                ]
            elif code == "STREAM_START_ERROR":
                return [
                    "Select a different microphone",
                    "Restart the application",
                    "Check if another application is using the microphone"
                ]
        elif category == "TRANSCRIPTION":
            if code == "MODEL_LOAD_ERROR":
                return [
                    "Restart the application",
                    "Reinstall the application",
                    "Check that the model files are in the correct location"
                ]
        elif category == "SHORTCUT":
            if code == "REGISTRATION_ERROR":
                return [
                    "Try a different key combination",
                    "Check if another application is using the same shortcut",
                    "Restart the application"
                ]
        elif category == "OUTPUT":
            if code == "OUTPUT_ERROR":
                return [
                    "Check if the target application is still active",
                    "Try using clipboard output instead (in Options)",
                    "Restart the target application"
                ]
                
        # Default recovery steps
        return [
            "Restart the application",
            "Check the log file for more details",
            "Contact support if the problem persists"
        ]
        
# Create global instance
error_handler = ErrorHandler()