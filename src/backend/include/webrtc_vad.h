#ifndef VOICE_TRANSCRIPTION_WEBRTC_VAD_H
#define VOICE_TRANSCRIPTION_WEBRTC_VAD_H

#include <vector>

// Forward declaration to avoid circular includes
namespace voice_transcription {
    class AudioChunk;  // Forward declaration
}

namespace voice_transcription {

/**
 * Voice Activity Detection handler using WebRTC VAD implementation
 * Detects whether audio chunks contain speech
 */
class VADHandler {
public:
    /**
     * Constructs a VAD handler
     * 
     * @param sample_rate Sample rate of audio in Hz (typically 16000)
     * @param frame_duration_ms Frame duration in milliseconds (10, 20, or 30)
     * @param aggressiveness VAD aggressiveness (0-3, higher = fewer false positives)
     */
    VADHandler(int sample_rate, int frame_duration_ms, int aggressiveness);
    
    /**
     * Destructor - cleans up VAD resources
     */
    ~VADHandler();

    /**
     * Process an audio chunk and determine if it contains speech
     * 
     * @param chunk Audio chunk to analyze
     * @return true if speech is detected, false otherwise
     */
    bool is_speech(const AudioChunk& chunk);
    
    /**
     * Adjust VAD aggressiveness level
     * 
     * @param aggressiveness New aggressiveness level (0-3)
     */
    void set_aggressiveness(int aggressiveness);
    
    /**
     * Get current aggressiveness level
     * 
     * @return Current aggressiveness level (0-3)
     */
    int get_aggressiveness() const { return aggressiveness_; }

private:
    void* vad_handle_;           // WebRTC VAD handle
    int sample_rate_;            // Audio sample rate
    int frame_duration_ms_;      // Frame duration in ms
    int aggressiveness_;         // VAD aggressiveness level
    std::vector<int16_t> temp_buffer_; // Buffer for float to int16 conversion
};

} // namespace voice_transcription

#endif // VOICE_TRANSCRIPTION_WEBRTC_VAD_H