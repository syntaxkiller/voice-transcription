#!/usr/bin/env python3
import re
from pathlib import Path
import sys
import os

# Import logger
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from utils.logger import setup_logger

class CommandProcessor:
    """Process transcription text to detect and execute dictation commands"""
    def __init__(self, supported_commands=None):
        self.logger = setup_logger("command_processor")
        
        # Default commands if none provided
        if not supported_commands:
            supported_commands = [
                {"phrase": "period", "action": "."},
                {"phrase": "comma", "action": ","},
                {"phrase": "question mark", "action": "?"},
                {"phrase": "exclamation point", "action": "!"},
                {"phrase": "new line", "action": "{ENTER}"},
                {"phrase": "new paragraph", "action": "{ENTER}{ENTER}"},
                {"phrase": "space", "action": " "},
                {"phrase": "control enter", "action": "{CTRL+ENTER}"},
                {"phrase": "all caps", "action": "{CAPSLOCK}"},
                {"phrase": "caps lock", "action": "{CAPSLOCK}"}
            ]
            
        # Build command dictionary
        self.commands = {}
        for command in supported_commands:
            if "phrase" in command and "action" in command:
                self.commands[command["phrase"].lower()] = command["action"]
                
        # Compile regex patterns for faster matching
        self._compile_patterns()
        
    def _compile_patterns(self):
        """Compile regex patterns for command detection"""
        # Create pattern to match any command
        command_phrases = [re.escape(phrase) for phrase in self.commands.keys()]
        if command_phrases:
            self.command_pattern = re.compile(
                r'\b(' + '|'.join(command_phrases) + r')\b',
                re.IGNORECASE
            )
        else:
            self.command_pattern = None
            
    def process(self, text):
        """Process text to replace command phrases with their actions"""
        if not text or not self.command_pattern:
            return text
            
        # Process transcribed text to handle commands
        # We need to be careful with the order to avoid issues with overlapping commands
        # For example, "new paragraph" should be processed before "new line"
        
        # First, find all command matches
        matches = list(self.command_pattern.finditer(text.lower()))
        
        # Process matches from end to beginning to avoid index shifts
        processed_text = text
        for match in reversed(matches):
            start, end = match.span()
            command_phrase = match.group(1).lower()
            action = self.commands.get(command_phrase, "")
            
            # Replace command with action
            processed_text = processed_text[:start] + action + processed_text[end:]
            self.logger.debug(f"Replaced command '{command_phrase}' with '{action}'")
        
        # Handle common capitalization patterns
        # For example, capitalize first letter of sentences
        sentences = re.split(r'([.!?]\s+)', processed_text)
        result = ""
        for i in range(0, len(sentences), 2):
            sentence = sentences[i]
            if i > 0 or (sentence and sentence[0].isalpha()):
                sentence = sentence[0].upper() + sentence[1:] if sentence else sentence
            result += sentence
            if i + 1 < len(sentences):
                result += sentences[i + 1]
        
        return result
    
    def add_command(self, phrase, action):
        """Add a new command to the processor"""
        if not phrase or not action:
            return False
            
        self.commands[phrase.lower()] = action
        self._compile_patterns()
        return True
        
    def remove_command(self, phrase):
        """Remove a command from the processor"""
        if not phrase:
            return False
            
        phrase = phrase.lower()
        if phrase in self.commands:
            del self.commands[phrase]
            self._compile_patterns()
            return True
            
        return False
        
    def get_commands(self):
        """Get the list of supported commands"""
        return [{"phrase": phrase, "action": action} for phrase, action in self.commands.items()]