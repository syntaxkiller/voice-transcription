#ifndef VOSK_TRANSCRIPTION_ENGINE_H
#define VOSK_TRANSCRIPTION_ENGINE_H

#include "audio_stream.h"
#include <vosk_api.h>
#include <string>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <future>
#include <deque>

namespace voice_transcription {

// Forward declarations
class NoiseFilter;
class VADHandler;  // Forward declare, don't redefine

// Transcription result structure
struct TranscriptionResult {
    std::string raw_text;         // Raw text from the transcription engine
    std::string processed_text;   // Text after command processing
    bool is_final;                // Whether this is a final result
    double confidence;            // Confidence score (0.0 to 1.0)
    int64_t timestamp_ms;         // Timestamp of when the transcription was generated
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

    // Noise filtering methods
    void enable_noise_filtering(bool enable);
    bool is_noise_filtering_enabled() const;
    void calibrate_noise_filter(const AudioChunk& silence_chunk);
    TranscriptionResult transcribe_with_noise_filtering(
        std::unique_ptr<AudioChunk> chunk, bool is_speech);

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