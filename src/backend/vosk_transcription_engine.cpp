#include "vosk_transcription_engine.h"
#include <chrono>
#include <cstring>
#include <vector>

namespace voice_transcription {

// VADHandler implementation
VADHandler::VADHandler(int sample_rate, int frame_duration_ms, int aggressiveness)
    : sample_rate_(sample_rate), 
      frame_duration_ms_(frame_duration_ms),
      aggressiveness_(aggressiveness),
      vad_handle_(nullptr) {
    
    // Initialize WebRTC VAD
    vad_handle_ = WebRtcVad_Create();
    if (vad_handle_) {
        WebRtcVad_Init(vad_handle_);
        WebRtcVad_set_mode(vad_handle_, aggressiveness_);
    }
    
    // Allocate temporary buffer
    size_t frame_size = (sample_rate_ * frame_duration_ms_ / 1000);
    temp_buffer_.resize(frame_size);
}

VADHandler::~VADHandler() {
    if (vad_handle_) {
        WebRtcVad_Free(vad_handle_);
        vad_handle_ = nullptr;
    }
}

bool VADHandler::is_speech(const AudioChunk& chunk) {
    if (!vad_handle_ || chunk.size() == 0) {
        return false;
    }
    
    // Convert float samples to int16
    for (size_t i = 0; i < chunk.size() && i < temp_buffer_.size(); i++) {
        temp_buffer_[i] = static_cast<int16_t>(chunk.data()[i] * 32767.0f);
    }
    
    // Process with WebRTC VAD
    int result = WebRtcVad_Process(
        vad_handle_,
        sample_rate_,
        temp_buffer_.data(),
        temp_buffer_.size()
    );
    
    return result > 0;
}

void VADHandler::set_aggressiveness(int aggressiveness) {
    if (aggressiveness < 0 || aggressiveness > 3) {
        return;
    }
    
    aggressiveness_ = aggressiveness;
    if (vad_handle_) {
        WebRtcVad_set_mode(vad_handle_, aggressiveness_);
    }
}

// VoskTranscriber implementation
VoskTranscriber::VoskTranscriber(const std::string& model_path, float sample_rate)
    : model_(nullptr),
      recognizer_(nullptr),
      sample_rate_(sample_rate),
      has_speech_started_(false) {
    
    // Load model
    model_ = vosk_model_new(model_path.c_str());
    
    if (model_) {
        // Create recognizer
        recognizer_ = vosk_recognizer_new(model_, sample_rate_);
        
        if (recognizer_) {
            // Set recognizer options
            vosk_recognizer_set_max_alternatives(recognizer_, 1);
            vosk_recognizer_set_words(recognizer_, 1);
        } else {
            last_error_ = "Failed to create recognizer";
        }
    } else {
        last_error_ = "Failed to load model from path: " + model_path;
    }
}

VoskTranscriber::~VoskTranscriber() {
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
        // Clean up current resources
        if (recognizer_) {
            vosk_recognizer_free(recognizer_);
        }
        
        if (model_) {
            vosk_model_free(model_);
        }
        
        // Move resources
        model_ = other.model_;
        recognizer_ = other.recognizer_;
        sample_rate_ = other.sample_rate_;
        last_error_ = std::move(other.last_error_);
        has_speech_started_ = other.has_speech_started_;
        
        // Clear the moved object
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
    
    if (!recognizer_ || !chunk) {
        result.raw_text = "";
        result.processed_text = "";
        return result;
    }
    
    // Lock to prevent concurrent access to recognizer
    std::lock_guard<std::mutex> lock(recognizer_mutex_);
    
    // Convert float samples to int16 for Vosk
    std::vector<int16_t> pcm(chunk->size());
    for (size_t i = 0; i < chunk->size(); i++) {
        pcm[i] = static_cast<int16_t>(chunk->data()[i] * 32767.0f);
    }
    
    // Process audio data
    std::string json_result;
    bool is_final;
    
    // First, convert the data to a char array
    char* data_ptr = reinterpret_cast<char*>(pcm.data());
    int data_length = static_cast<int>(pcm.size() * sizeof(int16_t));
    
    // Then call the function with the correct types
    if (vosk_recognizer_accept_waveform(recognizer_, data_ptr, data_length)) {
        // End of utterance, get final result
        json_result = vosk_recognizer_result(recognizer_);
        is_final = true;
    } else {
        // Utterance continues, get partial result
        json_result = vosk_recognizer_partial_result(recognizer_);
        is_final = false;
    }
    
    // Parse result
    result = parse_result(json_result);
    result.is_final = is_final;
    
    return result;
}

TranscriptionResult VoskTranscriber::transcribe_with_vad(std::unique_ptr<AudioChunk> chunk, bool is_speech) {
    // Process based on VAD result
    if (is_speech) {
        has_speech_started_ = true;
    }
    
    if (has_speech_started_) {
        return transcribe(std::move(chunk));
    }
    
    // If no speech detected and hasn't started yet, return empty result
    TranscriptionResult empty_result;
    empty_result.raw_text = "";
    empty_result.processed_text = "";
    empty_result.is_final = false;
    empty_result.confidence = 0.0;
    empty_result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    return empty_result;
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
    
    // Mock implementation - just extract text
    result.raw_text = extract_text_from_json(json_result);
    result.processed_text = result.raw_text;
    result.is_final = true;
    result.confidence = 0.9;
    result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    return result;
}

std::string VoskTranscriber::extract_text_from_json(const std::string& json) {
    // Mock implementation - just return a fixed text
    return "This is a mock transcription result";
}

} // namespace voice_transcription