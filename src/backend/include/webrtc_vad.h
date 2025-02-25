#pragma once

#include <cstdint>
#include <cstddef>

// Mock header for WebRTC VAD functions
// This provides the same interface as the real WebRTC VAD

#ifdef __cplusplus
extern "C" {
#endif

// Create a VAD instance
void* WebRtcVad_Create();

// Initialize a VAD instance
int WebRtcVad_Init(void* handle);

// Free the VAD instance
int WebRtcVad_Free(void* handle);

// Set the VAD operating mode (0-3)
// 0: Least aggressive, more false positives (speech detected when there isn't)
// 3: Most aggressive, more false negatives (speech not detected when there is)
int WebRtcVad_set_mode(void* handle, int mode);

// Process an audio frame
// Returns: 1 - speech detected, 0 - no speech, -1 - error
int WebRtcVad_Process(
    void* handle,
    int sample_rate_hz,
    const int16_t* audio_frame,
    size_t frame_length
);

#ifdef __cplusplus
}
#endif