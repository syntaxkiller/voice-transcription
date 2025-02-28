#!/usr/bin/env python3
import re
from pathlib import Path
import sys
import os
import json

# Import logger
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from utils.logger import setup_logger

class EnhancedCommandProcessor:
    """Enhanced processor for transcription text to detect and execute dictation commands
    with improved context awareness and natural language capabilities"""
    
    def __init__(self, supported_commands=None, config_path=None):
        self.logger = setup_logger("enhanced_command_processor")
        
        # Default commands if none provided
        if not supported_commands:
            if config_path:
                # Try to load from config file
                try:
                    with open(config_path, 'r') as f:
                        config = json.load(f)
                        supported_commands = config.get("dictation_commands", {}).get("supported_commands", [])
                except Exception as e:
                    self.logger.error(f"Failed to load commands from config: {str(e)}")
                    supported_commands = self._get_default_commands()
            else:
                supported_commands = self._get_default_commands()
            
        # Build command dictionary
        self.commands = {}
        for command in supported_commands:
            if "phrase" in command and "action" in command:
                self.commands[command["phrase"].lower()] = command["action"]
                
                # Add aliases if available
                if "aliases" in command:
                    for alias in command["aliases"]:
                        self.commands[alias.lower()] = command["action"]
                
        # Compile regex patterns for faster matching
        self._compile_patterns()
        
        # State for context-aware processing
        self.capitalization_mode = "auto"  # auto, all_caps, or none
        self.in_sentence = False  # Whether we're in the middle of a sentence
        self.smart_punctuation = True  # Enable smart punctuation
        self.command_context = {}  # Context for complex commands
        
    def _get_default_commands(self):
        """Get the default set of commands"""
        return [
            {"phrase": "period", "action": ".", "aliases": ["full stop", "dot"]},
            {"phrase": "comma", "action": ","},
            {"phrase": "question mark", "action": "?"},
            {"phrase": "exclamation point", "action": "!", "aliases": ["exclamation mark"]},
            {"phrase": "colon", "action": ":"},
            {"phrase": "semicolon", "action": ";"},
            {"phrase": "new line", "action": "{ENTER}"},
            {"phrase": "new paragraph", "action": "{ENTER}{ENTER}"},
            {"phrase": "tab key", "action": "{TAB}", "aliases": ["tab"]},
            {"phrase": "space", "action": " "},
            {"phrase": "control enter", "action": "{CTRL+ENTER}"},
            {"phrase": "all caps", "action": "{ALLCAPS_ON}"},
            {"phrase": "no caps", "action": "{ALLCAPS_OFF}"},
            {"phrase": "caps on", "action": "{CAPSLOCK}"},
            {"phrase": "caps off", "action": "{CAPSLOCK}"},
            {"phrase": "open parenthesis", "action": "(", "aliases": ["left parenthesis"]},
            {"phrase": "close parenthesis", "action": ")", "aliases": ["right parenthesis"]},
            {"phrase": "open bracket", "action": "[", "aliases": ["left bracket"]},
            {"phrase": "close bracket", "action": "]", "aliases": ["right bracket"]},
            {"phrase": "open brace", "action": "{", "aliases": ["left brace"]},
            {"phrase": "close brace", "action": "}", "aliases": ["right brace"]},
            {"phrase": "quote", "action": "\"", "aliases": ["double quote"]},
            {"phrase": "single quote", "action": "'"},
            {"phrase": "backslash", "action": "\\"},
            {"phrase": "forward slash", "action": "/", "aliases": ["slash"]},
            {"phrase": "delete", "action": "{DELETE}"},
            {"phrase": "backspace", "action": "{BACKSPACE}"},
            {"phrase": "underscore", "action": "_"},
            {"phrase": "hyphen", "action": "-", "aliases": ["dash"]},
            {"phrase": "plus", "action": "+", "aliases": ["plus sign"]},
            {"phrase": "equals", "action": "=", "aliases": ["equal sign"]},
            {"phrase": "at sign", "action": "@", "aliases": ["at"]},
            {"phrase": "hash", "action": "#", "aliases": ["pound", "number sign"]}
        ]
        
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
            
        # Pattern for end of sentence detection
        self.sentence_end_pattern = re.compile(r'[.!?]\s*$')
        
        # Pattern for capitalization after end of sentence
        self.capitalize_pattern = re.compile(r'([.!?]\s+)([a-z])')
            
    def process(self, text):
        """
        Process text to replace command phrases with their actions
        and apply smart formatting
        """
        if not text or not self.command_pattern:
            return text
            
        # Process transcribed text to handle commands
        # First, find all command matches
        matches = list(self.command_pattern.finditer(text.lower()))
        
        # Process matches from end to beginning to avoid index shifts
        processed_text = text
        for match in reversed(matches):
            start, end = match.span()
            command_phrase = match.group(1).lower()
            action = self.commands.get(command_phrase, "")
            
            # Handle special capitalization commands
            if action == "{ALLCAPS_ON}":
                self.capitalization_mode = "all_caps"
                processed_text = processed_text[:start] + processed_text[end:]
                continue
            elif action == "{ALLCAPS_OFF}":
                self.capitalization_mode = "auto"
                processed_text = processed_text[:start] + processed_text[end:]
                continue
            
            # Replace command with action
            processed_text = processed_text[:start] + action + processed_text[end:]
            self.logger.debug(f"Replaced command '{command_phrase}' with '{action}'")
            
            # Update sentence state for period, question mark, exclamation
            if action in ['.', '!', '?']:
                self.in_sentence = False
            elif action not in [' ', '{ENTER}', '{ENTER}{ENTER}', '{TAB}'] and not self.in_sentence:
                self.in_sentence = True
        
        # Apply smart capitalization if enabled
        if self.capitalization_mode == "auto":
            # Capitalize first letter of sentences
            sentences = re.split(r'([.!?]\s+)', processed_text)
            result = ""
            for i in range(0, len(sentences), 2):
                sentence = sentences[i]
                if i == 0 and sentence and sentence[0].isalpha():
                    # Capitalize first letter of the first sentence
                    sentence = sentence[0].upper() + sentence[1:]
                elif i > 0 and sentence and sentence[0].isalpha():
                    # Capitalize first letter of subsequent sentences
                    sentence = sentence[0].upper() + sentence[1:]
                result += sentence
                if i + 1 < len(sentences):
                    result += sentences[i + 1]
        elif self.capitalization_mode == "all_caps":
            # Convert everything to uppercase
            result = processed_text.upper()
        else:
            result = processed_text
        
        # Apply smart spacing (ensure proper spacing around punctuation)
        result = self._apply_smart_spacing(result)
        
        return result
    
    def _apply_smart_spacing(self, text):
        """Apply smart spacing rules to ensure proper formatting"""
        # Remove spaces before punctuation
        text = re.sub(r'\s+([.,;:!?)])', r'\1', text)
        
        # Ensure space after punctuation (but not at end of string)
        text = re.sub(r'([.,;:!?])([^\s)])', r'\1 \2', text)
        
        # Remove spaces after opening brackets/parentheses
        text = re.sub(r'([\({\["])\s+', r'\1', text)
        
        # Remove spaces before closing brackets/parentheses
        text = re.sub(r'\s+([\)}\]"])', r'\1', text)
        
        # Ensure only single spaces between words
        text = re.sub(r'\s{2,}', ' ', text)
        
        return text
    
    def process_with_context(self, text, context=None):
        """Process text with context about the target application"""
        # Add application-specific processing here
        # For example, add code formatting for programming applications
        # or special symbols for math applications
        
        if not context:
            # Default processing
            return self.process(text)
        
        app_name = context.get("application_name", "").lower()
        
        # Process based on application context
        if "code" in app_name or "visual studio" in app_name or "vscode" in app_name:
            # Apply code-friendly processing
            return self._process_for_code_editor(text)
        elif "word" in app_name or "document" in app_name or "writer" in app_name:
            # Apply word processor friendly formatting
            return self._process_for_word_processor(text)
        else:
            # Default processing
            return self.process(text)
    
    def _process_for_code_editor(self, text):
        """Apply code-editor specific processing"""
        # Process with standard rules first
        processed = self.process(text)
        
        # Add code-specific command handling
        # Replace "new line" with just newline (no auto-indent)
        processed = processed.replace("{ENTER}", "\n")
        
        # Preserve indentation after line breaks
        processed = re.sub(r'\n(\s+)', r'\n\1', processed)
        
        return processed
    
    def _process_for_word_processor(self, text):
        """Apply word processor friendly formatting"""
        # Process with standard rules first
        processed = self.process(text)
        
        # Apply typographic fixes (smart quotes, em dashes, etc.)
        processed = processed.replace('--', '—')  # em dash
        processed = re.sub(r'"([^"]*)"', r'"\1"', processed)  # smart quotes
        
        return processed
    
    def add_command(self, phrase, action, aliases=None):
        """Add a new command to the processor"""
        if not phrase or not action:
            return False
            
        self.commands[phrase.lower()] = action
        
        # Add aliases if provided
        if aliases:
            for alias in aliases:
                self.commands[alias.lower()] = action
                
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
        # Group commands by action to properly represent aliases
        command_groups = {}
        for phrase, action in self.commands.items():
            if action not in command_groups:
                command_groups[action] = {"action": action, "phrase": phrase, "aliases": []}
            elif command_groups[action]["phrase"] != phrase:
                command_groups[action]["aliases"].append(phrase)
                
        return list(command_groups.values())
    
    def set_capitalization_mode(self, mode):
        """Set the capitalization mode"""
        if mode in ["auto", "all_caps", "none"]:
            self.capitalization_mode = mode
            return True
        return False
        
    def set_smart_punctuation(self, enabled):
        """Enable or disable smart punctuation"""
        self.smart_punctuation = enabled
        return True