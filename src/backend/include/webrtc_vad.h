#ifndef WEBRTC_VAD_H
#define WEBRTC_VAD_H

#include <cstdint>
#include <cstddef>

// WebRTC VAD API declarations
// This is a simplified version of the WebRTC VAD API for our mock implementation

#ifdef __cplusplus
extern "C" {
#endif

// Create a new VAD instance
void* WebRtcVad_Create();

// Initialize a VAD instance
int WebRtcVad_Init(void* handle);

// Free a VAD instance
void WebRtcVad_Free(void* handle);

// Set the aggressiveness mode
// 0: Least aggressive, more false positives
// 3: Most aggressive, more false negatives
int WebRtcVad_set_mode(void* handle, int mode);

// Process a frame to determine if it contains speech
// Returns: 1 - speech detected, 0 - no speech, -1 - error
int WebRtcVad_Process(void* handle, int fs, const int16_t* audio_frame, size_t frame_length);

#ifdef __cplusplus
}
#endif

#endif // WEBRTC_VAD_H