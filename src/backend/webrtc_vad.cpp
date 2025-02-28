#include "webrtc_vad.h"
#include "audio_stream.h"  // Include here, not in the header
#include <cmath>
#include <algorithm>
#include <memory>

// External WebRTC VAD API
extern "C" {
    void* WebRtcVad_Create();
    int WebRtcVad_Init(void* handle);
    void WebRtcVad_Free(void* handle);
    int WebRtcVad_set_mode(void* handle, int mode);
    int WebRtcVad_Process(void* handle, int fs, const int16_t* audio_frame, size_t frame_length);
}

namespace voice_transcription {

// Constructor
VADHandler::VADHandler(int sample_rate, int frame_duration_ms, int aggressiveness) 
    : sample_rate_(sample_rate),
      frame_duration_ms_(frame_duration_ms),
      aggressiveness_(aggressiveness) {
    
    // Initialize WebRTC VAD
    vad_handle_ = WebRtcVad_Create();
    if (vad_handle_) {
        WebRtcVad_Init(vad_handle_);
        WebRtcVad_set_mode(vad_handle_, aggressiveness_);
    }
    
    // Calculate frame size and initialize temp buffer
    int frame_size = (sample_rate_ * frame_duration_ms_) / 1000;
    temp_buffer_.resize(frame_size);
}

// Destructor
VADHandler::~VADHandler() {
    if (vad_handle_) {
        WebRtcVad_Free(vad_handle_);
        vad_handle_ = nullptr;
    }
}

// Process an audio chunk and determine if it contains speech
bool VADHandler::is_speech(const AudioChunk& chunk) {
    if (!vad_handle_ || chunk.size() == 0) {
        return false;
    }
    
    // Convert float to int16_t for WebRTC VAD
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
    
    // 1 = speech detected, 0 = no speech, -1 = error
    return result > 0;
}

// Adjust VAD parameters
void VADHandler::set_aggressiveness(int aggressiveness) {
    if (aggressiveness >= 0 && aggressiveness <= 3) {
        aggressiveness_ = aggressiveness;
        if (vad_handle_) {
            WebRtcVad_set_mode(vad_handle_, aggressiveness_);
        }
    }
}

} // namespace voice_transcription