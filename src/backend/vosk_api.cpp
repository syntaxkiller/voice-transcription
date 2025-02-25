#include "vosk_api.h"
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <unordered_map>

// Mock implementation of Vosk API for testing

// Structure to hold internal model data
struct MockVoskModel {
    std::string model_path;
    bool is_valid;
};

// Structure to hold internal recognizer data
struct MockVoskRecognizer {
    MockVoskModel* model;
    float sample_rate;
    bool words;
    int max_alternatives;
    std::vector<std::string> partial_results;
    std::string current_buffer;
    int buffer_frames;
    std::mutex mutex;
};

// List of possible mock transcriptions
const std::vector<std::string> MOCK_PHRASES = {
    "voice transcription test",
    "testing microphone input",
    "this is a test of the transcription system",
    "hello world how are you today",
    "the quick brown fox jumps over the lazy dog",
    "voice recognition software in development",
    "period comma question mark new line",
    "all caps start of sentence end of paragraph",
    "testing testing one two three",
    "microphone check one two one two"
};

// Initialize random seed
static bool rand_initialized = false;
static std::mutex rand_mutex;

// Model functions
VoskModel *vosk_model_new(const char *model_path) {
    // Initialize random if not already done
    std::lock_guard<std::mutex> lock(rand_mutex);
    if (!rand_initialized) {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        rand_initialized = true;
    }
    
    // Create a mock model
    MockVoskModel* model = new MockVoskModel();
    model->model_path = model_path ? model_path : "";
    
    // Simulate model loading failure for invalid paths
    model->is_valid = !model->model_path.empty();
    
    return reinterpret_cast<VoskModel*>(model);
}

void vosk_model_free(VoskModel *model) {
    if (model) {
        delete reinterpret_cast<MockVoskModel*>(model);
    }
}

// Recognizer functions
VoskRecognizer *vosk_recognizer_new(VoskModel *model, float sample_rate) {
    MockVoskModel* mock_model = reinterpret_cast<MockVoskModel*>(model);
    if (!mock_model || !mock_model->is_valid) {
        return nullptr;
    }
    
    // Create a mock recognizer
    MockVoskRecognizer* recognizer = new MockVoskRecognizer();
    recognizer->model = mock_model;
    recognizer->sample_rate = sample_rate;
    recognizer->words = false;
    recognizer->max_alternatives = 1;
    recognizer->buffer_frames = 0;
    
    // Add some initial partial results
    recognizer->partial_results.push_back(""); // Start with empty
    
    return reinterpret_cast<VoskRecognizer*>(recognizer);
}

void vosk_recognizer_free(VoskRecognizer *recognizer) {
    if (recognizer) {
        delete reinterpret_cast<MockVoskRecognizer*>(recognizer);
    }
}

void vosk_recognizer_set_max_alternatives(VoskRecognizer *recognizer, int max_alternatives) {
    if (recognizer) {
        reinterpret_cast<MockVoskRecognizer*>(recognizer)->max_alternatives = max_alternatives;
    }
}

void vosk_recognizer_set_words(VoskRecognizer *recognizer, int words) {
    if (recognizer) {
        reinterpret_cast<MockVoskRecognizer*>(recognizer)->words = (words != 0);
    }
}

int vosk_recognizer_accept_waveform(VoskRecognizer *recognizer, const char *data, int length) {
    MockVoskRecognizer* mock_rec = reinterpret_cast<MockVoskRecognizer*>(recognizer);
    if (!mock_rec) {
        return -1;
    }
    
    std::lock_guard<std::mutex> lock(mock_rec->mutex);
    
    // Accumulate audio data
    mock_rec->buffer_frames += length / 2; // Assuming int16 data (2 bytes per sample)
    
    // Decide if we've reached end of utterance
    // In mock: End utterance after ~2 seconds of audio (32000 samples at 16kHz)
    if (mock_rec->buffer_frames >= 32000) {
        // Select a random phrase as the result
        int idx = std::rand() % MOCK_PHRASES.size();
        mock_rec->current_buffer = MOCK_PHRASES[idx];
        mock_rec->buffer_frames = 0;
        return 1; // Indicate end of utterance
    }
    
    // Still accumulating
    // Generate incremental partial results
    if (mock_rec->buffer_frames > 5000) { // After ~0.3 seconds
        int idx = std::rand() % MOCK_PHRASES.size();
        std::string phrase = MOCK_PHRASES[idx];
        
        // Take portion of the phrase based on buffer size
        float progress = std::min(1.0f, mock_rec->buffer_frames / 32000.0f);
        int chars = static_cast<int>(phrase.length() * progress);
        mock_rec->current_buffer = phrase.substr(0, chars);
    }
    
    return 0; // Not end of utterance yet
}

const char *vosk_recognizer_result(VoskRecognizer *recognizer) {
    MockVoskRecognizer* mock_rec = reinterpret_cast<MockVoskRecognizer*>(recognizer);
    if (!mock_rec) {
        return "{}";
    }
    
    std::lock_guard<std::mutex> lock(mock_rec->mutex);
    
    // Create a JSON result
    std::stringstream ss;
    ss << "{\"text\":\"" << mock_rec->current_buffer << "\",\"confidence\":0.9}";
    
    // Store in recognizer (persists until next call)
    mock_rec->current_buffer = ss.str();
    
    return mock_rec->current_buffer.c_str();
}

const char *vosk_recognizer_partial_result(VoskRecognizer *recognizer) {
    MockVoskRecognizer* mock_rec = reinterpret_cast<MockVoskRecognizer*>(recognizer);
    if (!mock_rec) {
        return "{\"partial\":\"\"}";
    }
    
    std::lock_guard<std::mutex> lock(mock_rec->mutex);
    
    // Create a JSON result
    std::stringstream ss;
    ss << "{\"partial\":\"" << mock_rec->current_buffer << "\"}";
    
    // Store in recognizer (persists until next call)
    std::string json = ss.str();
    
    return json.c_str();
}

void vosk_recognizer_reset(VoskRecognizer *recognizer) {
    MockVoskRecognizer* mock_rec = reinterpret_cast<MockVoskRecognizer*>(recognizer);
    if (!mock_rec) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mock_rec->mutex);
    mock_rec->current_buffer.clear();
    mock_rec->buffer_frames = 0;
}