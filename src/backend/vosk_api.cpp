#include "vosk_api.h"
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <chrono>
#include <sstream>
#include <thread>
#include <filesystem>

// Mock implementation of the Vosk API for testing and development
// when the actual Vosk library is not available.

namespace {
    // Mock model and recognizer classes
    struct MockVoskModel {
        std::string model_path;
        bool loaded;
        std::string language;
    };

    struct MockVoskRecognizer {
        MockVoskModel* model;
        float sample_rate;
        bool words_enabled;
        int max_alternatives;
        std::vector<std::string> buffered_text;
        size_t buffer_position;
        std::mt19937 random_generator;
        int utterance_length; // Number of audio chunks before utterance is "complete"
        int current_utterance_length;
        bool has_partial;
    };

    // List of mock phrases to return
    const std::vector<std::string> mock_phrases = {
        "hello world",
        "voice transcription",
        "this is a test",
        "the quick brown fox jumps over the lazy dog",
        "speech recognition is working",
        "please speak clearly into the microphone",
        "i'm sorry i didn't catch that",
        "can you repeat that",
        "this is a mock implementation",
        "press the shortcut to start transcription",
        "period",
        "comma",
        "question mark",
        "exclamation point",
        "new line",
        "new paragraph",
        "all caps",
        "caps lock",
        "how are you today",
        "the weather is nice"
    };

    // Generate a random phrase
    std::string generate_random_phrase(std::mt19937& generator) {
        std::uniform_int_distribution<size_t> distribution(0, mock_phrases.size() - 1);
        return mock_phrases[distribution(generator)];
    }

    // Generate JSON response for a partial result
    std::string generate_partial_result(const std::string& text) {
        return "{\"partial\":\"" + text + "\"}";
    }

    // Generate JSON response for a final result
    std::string generate_final_result(const std::string& text) {
        float confidence = 0.8f + (std::rand() % 20) / 100.0f; // Random confidence between 0.8 and 0.99
        
        // Format JSON with a bit more complexity to mimic real Vosk output
        std::ostringstream json;
        json << "{";
        json << "\"text\":\"" << text << "\",";
        json << "\"result\":[";
        
        // Split text into words and add timestamps
        std::istringstream iss(text);
        std::string word;
        float start_time = 0.0f;
        bool first_word = true;
        
        while (iss >> word) {
            if (!first_word) {
                json << ",";
            }
            first_word = false;
            
            float word_duration = 0.1f + (word.length() * 0.05f); // Approx. duration based on word length
            json << "{";
            json << "\"word\":\"" << word << "\",";
            json << "\"start\":" << start_time << ",";
            json << "\"end\":" << (start_time + word_duration) << ",";
            json << "\"conf\":" << confidence;
            json << "}";
            
            start_time += word_duration + 0.05f; // Add a small gap between words
        }
        
        json << "],";
        json << "\"confidence\":" << confidence;
        json << "}";
        
        return json.str();
    }
}

// Vosk API implementation

extern "C" {

// Model functions
VoskModel *vosk_model_new(const char *model_path) {
    if (!model_path) {
        return nullptr;
    }
    
    // Check if path exists (simple check for mock)
    if (!std::filesystem::exists(model_path)) {
        // Add a small delay to simulate loading time
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return nullptr;
    }
    
    // Add a delay to simulate model loading
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    MockVoskModel* model = new MockVoskModel();
    model->model_path = model_path;
    model->loaded = true;
    model->language = "en-us"; // Default language
    
    return reinterpret_cast<VoskModel*>(model);
}

void vosk_model_free(VoskModel *model) {
    if (model) {
        delete reinterpret_cast<MockVoskModel*>(model);
    }
}

// Recognizer functions
VoskRecognizer *vosk_recognizer_new(VoskModel *model, float sample_rate) {
    if (!model) {
        return nullptr;
    }
    
    MockVoskModel* mock_model = reinterpret_cast<MockVoskModel*>(model);
    if (!mock_model->loaded) {
        return nullptr;
    }
    
    MockVoskRecognizer* recognizer = new MockVoskRecognizer();
    recognizer->model = mock_model;
    recognizer->sample_rate = sample_rate;
    recognizer->words_enabled = false;
    recognizer->max_alternatives = 0;
    recognizer->buffer_position = 0;
    
    // Initialize random generator
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    recognizer->random_generator = std::mt19937(seed);
    
    // Random utterance length between 5-15 chunks
    std::uniform_int_distribution<int> distribution(5, 15);
    recognizer->utterance_length = distribution(recognizer->random_generator);
    recognizer->current_utterance_length = 0;
    recognizer->has_partial = false;
    
    // Generate a random phrase to start with
    recognizer->buffered_text.push_back(generate_random_phrase(recognizer->random_generator));
    
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
        reinterpret_cast<MockVoskRecognizer*>(recognizer)->words_enabled = (words != 0);
    }
}

int vosk_recognizer_accept_waveform(VoskRecognizer *recognizer, const char *data, int length) {
    if (!recognizer || !data || length <= 0) {
        return -1;
    }
    
    MockVoskRecognizer* mock_recognizer = reinterpret_cast<MockVoskRecognizer*>(recognizer);
    
    // Increment utterance counter
    mock_recognizer->current_utterance_length++;
    
    // Set partial flag to indicate we have some audio processed
    mock_recognizer->has_partial = true;
    
    // Check if we've reached the end of an utterance
    if (mock_recognizer->current_utterance_length >= mock_recognizer->utterance_length) {
        // Reset utterance counter
        mock_recognizer->current_utterance_length = 0;
        
        // Reset utterance length to a new random value
        std::uniform_int_distribution<int> distribution(5, 15);
        mock_recognizer->utterance_length = distribution(mock_recognizer->random_generator);
        
        // Add a new random phrase to the buffer
        mock_recognizer->buffered_text.push_back(generate_random_phrase(mock_recognizer->random_generator));
        
        // Return 1 to indicate end of utterance
        return 1;
    }
    
    // Return 0 to indicate utterance continues
    return 0;
}

const char *vosk_recognizer_result(VoskRecognizer *recognizer) {
    if (!recognizer) {
        return "{}";
    }
    
    MockVoskRecognizer* mock_recognizer = reinterpret_cast<MockVoskRecognizer*>(recognizer);
    
    if (mock_recognizer->buffer_position >= mock_recognizer->buffered_text.size()) {
        return "{}";
    }
    
    // Get the current phrase
    std::string text = mock_recognizer->buffered_text[mock_recognizer->buffer_position];
    
    // Move to the next phrase
    mock_recognizer->buffer_position++;
    
    // Generate a final result JSON
    static std::string result = generate_final_result(text);
    
    return result.c_str();
}

const char *vosk_recognizer_partial_result(VoskRecognizer *recognizer) {
    if (!recognizer) {
        return "{\"partial\":\"\"}";
    }
    
    MockVoskRecognizer* mock_recognizer = reinterpret_cast<MockVoskRecognizer*>(recognizer);
    
    if (!mock_recognizer->has_partial || 
        mock_recognizer->buffer_position >= mock_recognizer->buffered_text.size()) {
        return "{\"partial\":\"\"}";
    }
    
    // Get the current phrase
    std::string text = mock_recognizer->buffered_text[mock_recognizer->buffer_position];
    
    // For partial results, we'll show only part of the text based on utterance progress
    float progress = static_cast<float>(mock_recognizer->current_utterance_length) / 
                    mock_recognizer->utterance_length;
    
    size_t partial_length = static_cast<size_t>(text.length() * progress);
    std::string partial_text = text.substr(0, partial_length);
    
    // Generate a partial result JSON
    static std::string result = generate_partial_result(partial_text);
    
    return result.c_str();
}

const char *vosk_recognizer_final_result(VoskRecognizer *recognizer) {
    return vosk_recognizer_result(recognizer);
}

void vosk_recognizer_reset(VoskRecognizer *recognizer) {
    if (!recognizer) {
        return;
    }
    
    MockVoskRecognizer* mock_recognizer = reinterpret_cast<MockVoskRecognizer*>(recognizer);
    
    // Reset buffer position and utterance counters
    mock_recognizer->buffer_position = 0;
    mock_recognizer->current_utterance_length = 0;
    mock_recognizer->has_partial = false;
    
    // Generate a new random utterance length
    std::uniform_int_distribution<int> distribution(5, 15);
    mock_recognizer->utterance_length = distribution(mock_recognizer->random_generator);
    
    // Clear buffered text and add a new random phrase
    mock_recognizer->buffered_text.clear();
    mock_recognizer->buffered_text.push_back(generate_random_phrase(mock_recognizer->random_generator));
}

} // extern "C"