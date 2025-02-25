#include "webrtc_vad.h"
#include <cstdlib>
#include <ctime>
#include <thread>
#include <chrono>

// Mock implementation of WebRTC VAD functions for testing

void* WebRtcVad_Create() {
    // Seed random number generator for speech simulation
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
        seeded = true;
    }
    
    // Allocate a mock VAD handle (just a dummy pointer)
    return new int(1);
}

int WebRtcVad_Init(void* handle) {
    // Always succeed in mock implementation
    return 0;
}

int WebRtcVad_Free(void* handle) {
    if (handle) {
        delete static_cast<int*>(handle);
    }
    return 0;
}

int WebRtcVad_set_mode(void* handle, int mode) {
    // Check mode is valid (0-3)
    if (mode < 0 || mode > 3) {
        return -1;
    }
    
    // Always succeed for valid mode
    return 0;
}

int WebRtcVad_Process(
    void* handle, 
    int sample_rate_hz,
    const int16_t* audio_frame,
    size_t frame_length
) {
    // Validate sample rate (WebRTC VAD supports 8000, 16000, 32000, 48000 Hz)
    if (sample_rate_hz != 8000 && 
        sample_rate_hz != 16000 && 
        sample_rate_hz != 32000 && 
        sample_rate_hz != 48000) {
        return -1;
    }
    
    // Validate frame length
    if (frame_length == 0) {
        return -1;
    }
    
    // Simple heuristic for mock: analyze audio energy
    int64_t sum_abs = 0;
    for (size_t i = 0; i < frame_length; i++) {
        sum_abs += std::abs(audio_frame[i]);
    }
    
    // Calculate average amplitude
    double avg_amplitude = static_cast<double>(sum_abs) / frame_length;
    
    // Threshold for speech detection (arbitrary for mock)
    // Higher threshold makes speech detection less sensitive
    double threshold = 500.0; 
    
    // Simulate some randomness to make it realistic
    // Add some randomness to avoid constant detection patterns
    int random_factor = std::rand() % 500;
    
    // Return 1 for speech, 0 for non-speech
    return (avg_amplitude > threshold - random_factor) ? 1 : 0;
}