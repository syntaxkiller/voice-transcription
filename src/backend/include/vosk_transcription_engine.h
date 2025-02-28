#ifndef VOSK_TRANSCRIPTION_ENGINE_H
#define VOSK_TRANSCRIPTION_ENGINE_H

#include "audio_stream.h"
#include "webrtc_vad.h"
#include <vosk_api.h>
#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <future>
#include <deque>

namespace voice_transcription {

// Transcription result structure
struct TranscriptionResult {
    std::string raw_text;         // Raw text from the transcription engine
    std::string processed_text;   // Text after command processing
    bool is_final;                // Whether this is a final result
    double confidence;            // Confidence score (0.0 to 1.0)
    int64_t timestamp_ms;         // Timestamp of when the transcription was generated
};

class NoiseFilter;

void VoskTranscriber::enable_noise_filtering(bool enable) {
    use_noise_filtering_ = enable;
}

bool VoskTranscriber::is_noise_filtering_enabled() const {
    return use_noise_filtering_;
}

void VoskTranscriber::calibrate_noise_filter(const AudioChunk& silence_chunk) {
    if (!noise_filter_) {
        noise_filter_ = std::make_unique<NoiseFilter>(0.05f, 10);
    }
    noise_filter_->calibrate(silence_chunk);
}

TranscriptionResult VoskTranscriber::transcribe_with_noise_filtering(
    std::unique_ptr<AudioChunk> chunk, bool is_speech) {
    
    // Initialize noise filter if needed
    if (!noise_filter_ && use_noise_filtering_) {
        noise_filter_ = std::make_unique<NoiseFilter>(0.05f, 10);
    }
    
    // Make a copy of the chunk for noise filtering
    auto filtered_chunk = std::make_unique<AudioChunk>(chunk->size());
    std::memcpy(filtered_chunk->data(), chunk->data(), chunk->size() * sizeof(float));
    
    // Apply noise filtering if enabled
    if (noise_filter_ && use_noise_filtering_) {
        if (!is_speech) {
            // Use silence to auto-calibrate the filter
            noise_filter_->auto_calibrate(*filtered_chunk, false);
        }
        
        // Apply the filter to the chunk
        noise_filter_->filter(*filtered_chunk);
    }
    
    // Use the filtered chunk for transcription
    return transcribe_with_vad(std::move(filtered_chunk), is_speech);
}

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

    void enable_noise_filtering(bool enable);
    bool is_noise_filtering_enabled() const;
    void calibrate_noise_filter(const AudioChunk& silence_chunk);
    TranscriptionResult transcribe_with_noise_filtering(
        std::unique_ptr<AudioChunk> chunk, bool is_speech);

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
    bool is_model_loaded() const;
    std::string get_last_error() const { return last_error_; }
    
    // Background loading status
    bool is_loading() const;
    float get_loading_progress() const;

    void VoskTranscriber::enable_noise_filtering(bool enable) {
    use_noise_filtering_ = enable;
}

private:
    // Noise filtering
    std::unique_ptr<NoiseFilter> noise_filter_;
    bool use_noise_filtering_ = false;
    
    // Parse json result to TranscriptionResult
    TranscriptionResult parse_result(const std::string& json_result);
    
    // Extract text from JSON
    std::string extract_text_from_json(const std::string& json);
    
    // Create empty result
    TranscriptionResult create_empty_result() const;
    
    // Background loading method
    bool load_model_background();

    // Member variables
    VoskModel* model_;
    VoskRecognizer* recognizer_;
    float sample_rate_;
    std::string last_error_;
    std::mutex recognizer_mutex_;
    bool has_speech_started_;
    
    // Background loading members
    std::atomic<bool> is_loading_;
    std::atomic<float> loading_progress_;
    std::future<bool> loading_future_;
    std::string model_path_;
};

} // namespace voice_transcription

#endif // VOSK_TRANSCRIPTION_ENGINE_H