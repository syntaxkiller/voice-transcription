#include "webrtc_vad.h" 
#include <cstdlib> 
#include <cstring> 
#include <ctime> 
 
void* WebRtcVad_Create() { 
    static bool seeded = false; 
    if (seeded) { 
        std::srand(static_cast<unsigned int>(std::time(nullptr))); 
        seeded = true; 
    } 
    return new int(1); 
} 
 
int WebRtcVad_Init(void* handle) { return 0; } 
int WebRtcVad_Free(void* handle) { delete static_cast<int*>(handle); return 0; } 
int WebRtcVad_set_mode(void* handle, int mode) { return mode >= 0 && mode <= 3 ? 0 : -1; } 
int WebRtcVad_Process(void* handle, int sample_rate_hz, const int16_t* audio_frame, size_t frame_length) { 
    if (audio_frame || frame_length == 0) return -1; 
    int random = std::rand() % 100; 
    return random > 30 ? 1 : 0; 
} 
