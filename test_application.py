#!/usr/bin/env python3
import os
import sys
import time
import unittest
from pathlib import Path

# Add project path to system path for imports
project_root = Path(__file__).resolve().parent
src_path = project_root / "src"

if str(src_path) not in sys.path:
    sys.path.insert(0, str(src_path))

# Try to import backend
try:
    import voice_transcription_backend as backend
except ImportError:
    print("Error: voice_transcription_backend module not found.")
    print("Make sure the C++ backend is compiled and installed correctly.")
    print("Run setup_and_run.bat to build the backend before running tests.")
    sys.exit(1)

# Import other modules
try:
    from utils.logger import setup_logger
    from utils.command_processor import CommandProcessor
except ImportError as e:
    print(f"Error importing Python modules: {e}")
    sys.exit(1)

# Setup logger
logger = setup_logger("test_application")

class AudioDeviceTests(unittest.TestCase):
    """Tests for audio device enumeration and selection"""
    
    def test_device_enumeration(self):
        """Test that audio devices can be enumerated"""
        devices = backend.ControlledAudioStream.enumerate_devices()
        
        # Should return a list (might be empty if no devices)
        self.assertIsInstance(devices, list)
        
        if devices:
            # Check device properties
            device = devices[0]
            self.assertIsInstance(device.id, int)
            self.assertIsInstance(device.raw_name, str)
            self.assertIsInstance(device.label, str)
            self.assertIsInstance(device.is_default, bool)
            self.assertIsInstance(device.supported_sample_rates, list)
            
            logger.info(f"Found device: {device.label} (ID: {device.id})")
            
    def test_device_compatibility(self):
        """Test device compatibility check"""
        devices = backend.ControlledAudioStream.enumerate_devices()
        
        if not devices:
            logger.warning("No audio devices found, skipping compatibility test")
            return
            
        # Check compatibility with required sample rate
        device_id = devices[0].id
        is_compatible = backend.ControlledAudioStream.check_device_compatibility(
            device_id, 16000  # Required sample rate
        )
        
        # Result should be a boolean
        self.assertIsInstance(is_compatible, bool)
        logger.info(f"Device {device_id} compatibility with 16000Hz: {is_compatible}")


class VADTests(unittest.TestCase):
    """Tests for Voice Activity Detection"""
    
    def test_vad_initialization(self):
        """Test VAD initialization"""
        vad = backend.VADHandler(16000, 20, 2)
        
        # Check initial aggressiveness
        self.assertEqual(vad.get_aggressiveness(), 2)
        
    def test_vad_speech_detection(self):
        """Test speech detection with silence and voice samples"""
        vad = backend.VADHandler(16000, 20, 2)
        
        # Create a silent audio chunk (all zeros)
        silent_chunk = backend.AudioChunk(320)  # 20ms at 16kHz
        
        # Create a speech-like audio chunk (sine wave)
        speech_chunk = backend.AudioChunk(320)
        speech_data = speech_chunk.data()
        
        # Fill with a simple sine wave to simulate speech
        import math
        for i in range(320):
            speech_data[i] = 0.75 * math.sin(2 * math.pi * 440 * i / 16000)
        
        # Test detection
        silence_result = vad.is_speech(silent_chunk)
        speech_result = vad.is_speech(speech_chunk)
        
        # Results should be booleans
        self.assertIsInstance(silence_result, bool)
        self.assertIsInstance(speech_result, bool)
        
        # In our mock implementation, silence should generally be detected as not speech
        logger.info(f"Silence detected as speech: {silence_result}")
        logger.info(f"Speech detected as speech: {speech_result}")


class TranscriptionTests(unittest.TestCase):
    """Tests for transcription engine"""
    
    def setUp(self):
        """Set up transcription engine"""
        # Create a mock model path
        self.model_path = str(project_root / "models" / "vosk" / "mock-model")
        os.makedirs(self.model_path, exist_ok=True)
        
        # Initialize transcriber
        self.transcriber = backend.VoskTranscriber(self.model_path, 16000)
        
    def test_model_loading(self):
        """Test model loading"""
        # Check if model loaded successfully
        self.assertTrue(self.transcriber.is_model_loaded())
        
    def test_transcription(self):
        """Test basic transcription"""
        # Create a speech-like audio chunk
        chunk = backend.AudioChunk(1600)  # 100ms at 16kHz
        
        # Fill with a simple sine wave to simulate speech
        import math
        for i in range(1600):
            chunk.data()[i] = 0.75 * math.sin(2 * math.pi * 440 * i / 16000)
        
        # Transcribe
        result = self.transcriber.transcribe(chunk)
        
        # Check result properties
        self.assertIsInstance(result.raw_text, str)
        self.assertIsInstance(result.processed_text, str)
        self.assertIsInstance(result.is_final, bool)
        self.assertIsInstance(result.confidence, float)
        self.assertIsInstance(result.timestamp_ms, int)
        
        logger.info(f"Transcription result: '{result.raw_text}' (confidence: {result.confidence})")


class KeyboardSimulatorTests(unittest.TestCase):
    """Tests for keyboard simulation"""
    
    def setUp(self):
        """Set up keyboard simulator"""
        self.keyboard = backend.KeyboardSimulator()
        
    def test_simple_keypresses(self):
        """Test simple text output via keypresses"""
        # Should not raise exceptions
        self.keyboard.simulate_keypresses("Hello, world!")
        
    def test_special_keys(self):
        """Test special key simulation"""
        # Should not raise exceptions
        self.keyboard.simulate_special_key("ENTER")
        self.keyboard.simulate_special_key("CTRL+A")


class CommandProcessorTests(unittest.TestCase):
    """Tests for command processing"""
    
    def setUp(self):
        """Set up command processor"""
        self.processor = CommandProcessor()
        
    def test_command_replacement(self):
        """Test command replacement in text"""
        # Test period command
        text = "end of sentence period"
        processed = self.processor.process(text)
        self.assertEqual(processed, "end of sentence.")
        
        # Test new line command
        text = "first line new line second line"
        processed = self.processor.process(text)
        self.assertEqual(processed, "first line{ENTER}second line")
        
        # Test question mark command
        text = "is this working question mark"
        processed = self.processor.process(text)
        self.assertEqual(processed, "is this working?")
        
    def test_multiple_commands(self):
        """Test multiple commands in one text"""
        text = "hello comma how are you question mark new paragraph this is great exclamation point"
        processed = self.processor.process(text)
        expected = "hello, how are you?{ENTER}{ENTER}This is great!"
        self.assertEqual(processed, expected)
        
    def test_custom_command(self):
        """Test adding a custom command"""
        # Add custom command
        self.processor.add_command("smiley face", " :) ")
        
        # Test it
        text = "hello smiley face goodbye"
        processed = self.processor.process(text)
        self.assertEqual(processed, "hello :) goodbye")


def run_tests():
    """Run all tests"""
    # Create test suite
    test_suite = unittest.TestSuite()
    
    # Add test cases
    test_suite.addTest(unittest.makeSuite(AudioDeviceTests))
    test_suite.addTest(unittest.makeSuite(VADTests))
    test_suite.addTest(unittest.makeSuite(TranscriptionTests))
    test_suite.addTest(unittest.makeSuite(KeyboardSimulatorTests))
    test_suite.addTest(unittest.makeSuite(CommandProcessorTests))
    
    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    return runner.run(test_suite)


if __name__ == "__main__":
    print("Starting Voice Transcription Application Tests")
    print("=" * 60)
    
    # Run tests
    result = run_tests()
    
    # Print summary
    print("\nTest Summary:")
    print(f"Ran {result.testsRun} tests")
    print(f"Failures: {len(result.failures)}")
    print(f"Errors: {len(result.errors)}")
    
    # Exit with appropriate status code
    sys.exit(len(result.failures) + len(result.errors))