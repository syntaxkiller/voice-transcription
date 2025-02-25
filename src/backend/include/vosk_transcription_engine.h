#pragma once

#include "audio_stream.h"
#include "webrtc_vad.h"
#include <vosk_api.h>
#include <string>
#include <memory>
#include <vector>
#include <mutex>

namespace voice_transcription {

// Transcription result structure
struct TranscriptionResult {
    std::string raw_text;         // Raw text from the transcription engine
    std::string processed_text;   // Text after command processing
    bool is_final;                // Whether this is a final result
    double confidence;            // Confidence score (0.0 to 1.0)
    int64_t timestamp_ms;         // Timestamp of when the transcription was generated
};

// Voice activity detection (VAD) handler
class VADHandler {
public:
    VADHandler(int sample_rate, int frame_duration_ms, int aggressiveness);
    ~VADHandler();

    // Process an audio chunk and determine if it contains speech
    bool is_speech(const AudioChunk& chunk);
    
    // Adjust VAD parameters
    void set_aggressiveness(int aggressiveness);
    int get_aggressiveness() const { return aggressiveness_; }

private:
    void* vad_handle_;  // WebRTC VAD handle
    int sample_rate_;
    int frame_duration_ms_;
    int aggressiveness_;
    std::vector<int16_t> temp_buffer_; // For float to int16 conversion
};

// Vosk transcription engine
class VoskTranscriber {
public:
    VoskTranscriber(const std::string& model_path, float sample_rate);
    ~VoskTranscriber();

    // No copy operations
    VoskTranscriber(const VoskTranscriber&) = delete;
    VoskTranscriber& operator=(const VoskTranscriber&) = delete;
    
    // Move operations
    VoskTranscriber(VoskTranscriber&&) noexcept;
    VoskTranscriber& operator=(VoskTranscriber&&) noexcept;

    // Process an audio chunk and return transcription
    TranscriptionResult transcribe(std::unique_ptr<AudioChunk> chunk);
    
    // Process a chunk with VAD checking
    TranscriptionResult transcribe_with_vad(std::unique_ptr<AudioChunk> chunk, bool is_speech);
    
    // Reset the recognizer
    void reset();
    
    // Status and error reporting
    bool is_model_loaded() const { return model_ != nullptr && recognizer_ != nullptr; }
    std::string get_last_error() const { return last_error_; }

private:
    // Convert JSON result to TranscriptionResult
    TranscriptionResult parse_result(const std::string& json_result);
    
    // Extract text from JSON
    std::string extract_text_from_json(const std::string& json);

    // Member variables
    VoskModel* model_;
    VoskRecognizer* recognizer_;
    float sample_rate_;
    std::string last_error_;
    std::mutex recognizer_mutex_;
    bool has_speech_started_;
};

} // namespace voice_transcription