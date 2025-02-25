#!/usr/bin/env python3
import os
import logging
import logging.handlers
from pathlib import Path
import colorlog

# Define log levels
LOG_LEVELS = {
    "DEBUG": logging.DEBUG,
    "INFO": logging.INFO,
    "WARNING": logging.WARNING,
    "ERROR": logging.ERROR,
    "CRITICAL": logging.CRITICAL
}

# Get log level from environment or use INFO as default
LOG_LEVEL = os.environ.get("VOICE_TRANSCRIPTION_LOG_LEVEL", "INFO").upper()
if LOG_LEVEL not in LOG_LEVELS:
    LOG_LEVEL = "INFO"

# Log directory
LOG_DIR = Path(__file__).parents[2] / "logs"
LOG_DIR.mkdir(exist_ok=True)
LOG_FILE = LOG_DIR / "app.log"

# Configure color formatter for console output
COLOR_FORMAT = "%(log_color)s%(asctime)s - %(name)s - %(levelname)s - %(message)s"
color_formatter = colorlog.ColorFormatter(
    COLOR_FORMAT,
    datefmt="%Y-%m-%d %H:%M:%S",
    log_colors={
        "DEBUG": "cyan",
        "INFO": "green",
        "WARNING": "yellow",
        "ERROR": "red",
        "CRITICAL": "red,bg_white",
    }
)

# Configure file formatter
file_formatter = logging.Formatter(
    "%(asctime)s - %(name)s - %(levelname)s - %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)

# Create console handler
console_handler = logging.StreamHandler()
console_handler.setFormatter(color_formatter)
console_handler.setLevel(LOG_LEVELS[LOG_LEVEL])

# Create file handler
file_handler = logging.handlers.RotatingFileHandler(
    LOG_FILE,
    maxBytes=10 * 1024 * 1024,  # 10 MB
    backupCount=5,
    encoding="utf-8"
)
file_handler.setFormatter(file_formatter)
file_handler.setLevel(LOG_LEVELS[LOG_LEVEL])

def setup_logger(name):
    """Set up a logger with the given name"""
    logger = logging.getLogger(name)
    logger.setLevel(LOG_LEVELS[LOG_LEVEL])
    
    # Add handlers if they haven't been added already
    if not logger.handlers:
        logger.addHandler(console_handler)
        logger.addHandler(file_handler)
        
    # Prevent propagation to root logger
    logger.propagate = False
    
    return logger