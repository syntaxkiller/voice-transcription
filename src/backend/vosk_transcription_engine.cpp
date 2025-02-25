#include "vosk_transcription_engine.h"
#include <cstring>
#include <cmath>
#include <chrono>
#include <stdexcept>
#include <algorithm>
#include <regex>

// JSON parsing helpers - simplistic implementation for this example
// In a real implementation, use a proper JSON library like nlohmann/json or rapidjson
namespace {
    std::string extract_json_string(const std::string& json, const std::string& key) {
        std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch match;
        if (std::regex_search(json, match, pattern) && match.size() > 1) {
            return match.str(1);
        }
        return "";
    }

    double extract_json_number(const std::string& json, const std::string& key) {
        std::regex pattern("\"" + key + "\"\\s*:\\s*([0-9.]+)");
        std::smatch match;
        if (std::regex_search(json, match, pattern) && match.size() > 1) {
            try {
                return std::stod(match.str(1));
            } catch (...) {
                return 0.0;
            }
        }
        return 0.0;
    }

    bool extract_json_bool(const std::string& json, const std::string& key) {
        std::regex pattern("\"" + key + "\"\\s*:\\s*(true|false)");
        std::smatch match;
        if (std::regex_search(json, match, pattern) && match.size() > 1) {
            return match.str(1) == "true";
        }
        return false;
    }
}

namespace voice_transcription {

// VADHandler implementation
VADHandler::VADHandler(int sample_rate, int frame_duration_ms, int aggressiveness)
    : sample_rate_(sample_rate),
      frame_duration_ms_(frame_duration_ms),
      aggressiveness_(aggressiveness) {
    
    // Initialize WebRTC VAD
    vad_handle_ = WebRtcVad_Create();
    if (!vad_handle_) {
        throw std::runtime_error("Failed to create WebRTC VAD instance");
    }
    
    // Configure VAD
    if (WebRtcVad_Init(vad_handle_) != 0) {
        WebRtcVad_Free(vad_handle_);
        throw std::runtime_error("Failed to initialize WebRTC VAD");
    }
    
    // Set aggressiveness
    set_aggressiveness(aggressiveness);
    
    // Initialize temporary buffer for float to int16 conversion
    int samples_per_frame = (sample_rate * frame_duration_ms) / 1000;
    temp_buffer_.resize(samples_per_frame);
}

VADHandler::~VADHandler() {
    if (vad_handle_) {
        WebRtcVad_Free(vad_handle_);
        vad_handle_ = nullptr;
    }
}

bool VADHandler::is_speech(const AudioChunk& chunk) {
    // Calculate expected samples based on the frame duration
    int expected_samples = (sample_rate_ * frame_duration_ms_) / 1000;
    
    // WebRTC VAD requires specific frame sizes
    if (chunk.size() != expected_samples) {
        return false;  // Invalid frame size
    }
    
    // Convert float audio ([-1.0, 1.0]) to int16 ([-32768, 32767])
    for (size_t i = 0; i < chunk.size(); ++i) {
        float sample = chunk.data()[i];
        // Clamp to [-1.0, 1.0] range
        sample = std::max(-1.0f, std::min(1.0f, sample));
        // Scale to int16 range
        temp_buffer_[i] = static_cast<int16_t>(sample * 32767.0f);
    }
    
    // Process with WebRTC VAD
    int is_speech = WebRtcVad_Process(
        vad_handle_,
        sample_rate_,
        temp_buffer_.data(),
        temp_buffer_.size()
    );
    
    return is_speech == 1;
}

void VADHandler::set_aggressiveness(int aggressiveness) {
    if (aggressiveness < 0 || aggressiveness > 3) {
        aggressiveness = 2;  // Default to moderate aggressiveness
    }
    
    if (WebRtcVad_set_mode(vad_handle_, aggressiveness) != 0) {
        throw std::runtime_error("Failed to set WebRTC VAD aggressiveness");
    }
    
    aggressiveness_ = aggressiveness;
}

// VoskTranscriber implementation
VoskTranscriber::VoskTranscriber(const std::string& model_path, float sample_rate)
    : model_(nullptr),
      recognizer_(nullptr),
      sample_rate_(sample_rate),
      has_speech_started_(false) {
    
    try {
        // Load Vosk model
        model_ = vosk_model_new(model_path.c_str());
        if (!model_) {
            last_error_ = "Failed to load Vosk model from " + model_path;
            return;
        }
        
        // Create recognizer
        recognizer_ = vosk_recognizer_new(model_, sample_rate);
        if (!recognizer_) {
            last_error_ = "Failed to create Vosk recognizer";
            vosk_model_free(model_);
            model_ = nullptr;
            return;
        }
        
        // Configure recognizer
        vosk_recognizer_set_max_alternatives(recognizer_, 1);
        vosk_recognizer_set_words(recognizer_, 1);  // Include word timestamps
        
    } catch (const std::exception& e) {
        last_error_ = "Exception during Vosk initialization: " + std::string(e.what());
        
        // Clean up
        if (recognizer_) {
            vosk_recognizer_free(recognizer_);
            recognizer_ = nullptr;
        }
        
        if (model_) {
            vosk_model_free(model_);
            model_ = nullptr;
        }
    }
}

VoskTranscriber::~VoskTranscriber() {
    // Free resources
    if (recognizer_) {
        vosk_recognizer_free(recognizer_);
        recognizer_ = nullptr;
    }
    
    if (model_) {
        vosk_model_free(model_);
        model_ = nullptr;
    }
}

VoskTranscriber::VoskTranscriber(VoskTranscriber&& other) noexcept
    : model_(other.model_),
      recognizer_(other.recognizer_),
      sample_rate_(other.sample_rate_),
      last_error_(std::move(other.last_error_)),
      has_speech_started_(other.has_speech_started_) {
    
    other.model_ = nullptr;
    other.recognizer_ = nullptr;
}

VoskTranscriber& VoskTranscriber::operator=(VoskTranscriber&& other) noexcept {
    if (this != &other) {
        // Free current resources
        if (recognizer_) {
            vosk_recognizer_free(recognizer_);
        }
        
        if (model_) {
            vosk_model_free(model_);
        }
        
        // Move resources from other
        model_ = other.model_;
        recognizer_ = other.recognizer_;
        sample_rate_ = other.sample_rate_;
        last_error_ = std::move(other.last_error_);
        has_speech_started_ = other.has_speech_started_;
        
        // Clear other
        other.model_ = nullptr;
        other.recognizer_ = nullptr;
    }
    
    return *this;
}

TranscriptionResult VoskTranscriber::transcribe(std::unique_ptr<AudioChunk> chunk) {
    TranscriptionResult result;
    result.is_final = false;
    result.confidence = 0.0;
    result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    if (!is_model_loaded() || !chunk) {
        result.raw_text = "";
        result.processed_text = "";
        return result;
    }
    
    try {
        std::lock_guard<std::mutex> lock(recognizer_mutex_);
        
        // Convert float audio to int16
        std::vector<int16_t> pcm(chunk->size());
        for (size_t i = 0; i < chunk->size(); ++i) {
            float sample = chunk->data()[i];
            // Clamp to [-1.0, 1.0] range
            sample = std::max(-1.0f, std::min(1.0f, sample));
            // Scale to int16 range
            pcm[i] = static_cast<int16_t>(sample * 32767.0f);
        }
        
        // Process audio chunk
        bool is_final = false;
        const char* json_result = nullptr;
        
        // Check for end of utterance
        if (vosk_recognizer_accept_waveform(recognizer_, pcm.data(), pcm.size() * sizeof(int16_t))) {
            // End of utterance, get final result
            json_result = vosk_recognizer_result(recognizer_);
            is_final = true;
        } else {
            // Utterance continues, get partial result
            json_result = vosk_recognizer_partial_result(recognizer_);
            is_final = false;
        }
        
        // Process the result
        if (json_result) {
            result = parse_result(json_result);
            result.is_final = is_final;
        }
        
    } catch (const std::exception& e) {
        last_error_ = "Error during transcription: " + std::string(e.what());
    }
    
    return result;
}

TranscriptionResult VoskTranscriber::transcribe_with_vad(std::unique_ptr<AudioChunk> chunk, bool is_speech) {
    if (!is_model_loaded() || !chunk) {
        TranscriptionResult result;
        result.is_final = false;
        result.confidence = 0.0;
        result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        return result;
    }
    
    // Track speech detection for utterance boundaries
    if (is_speech) {
        has_speech_started_ = true;
    }
    
    // Only process if we've detected speech in this session
    if (has_speech_started_) {
        auto result = transcribe(std::move(chunk));
        
        // If this is the end of an utterance and we're no longer detecting speech,
        // reset the speech detection flag for the next utterance
        if (result.is_final && !is_speech) {
            has_speech_started_ = false;
        }
        
        return result;
    } else {
        // Return empty result if no speech has been detected yet
        TranscriptionResult result;
        result.is_final = false;
        result.confidence = 0.0;
        result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        return result;
    }
}

void VoskTranscriber::reset() {
    std::lock_guard<std::mutex> lock(recognizer_mutex_);
    
    if (recognizer_) {
        vosk_recognizer_reset(recognizer_);
    }
    
    has_speech_started_ = false;
}

TranscriptionResult VoskTranscriber::parse_result(const std::string& json_result) {
    TranscriptionResult result;
    result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    // Extract text
    result.raw_text = extract_text_from_json(json_result);
    result.processed_text = result.raw_text;  // Initially the same, will be processed by CommandProcessor
    
    // Extract confidence (if available)
    result.confidence = extract_json_number(json_result, "confidence");
    if (result.confidence == 0.0) {
        // If not found, default to reasonably high confidence
        result.confidence = 0.9;
    }
    
    return result;
}

std::string VoskTranscriber::extract_text_from_json(const std::string& json) {
    // First try to get "text" field (final result)
    std::string text = extract_json_string(json, "text");
    if (!text.empty()) {
        return text;
    }
    
    // Then try "partial" field (partial result)
    text = extract_json_string(json, "partial");
    if (!text.empty()) {
        return text;
    }
    
    // If all else fails, return empty string
    return "";
}

} // namespace voice_transcription