#include "webrtc_vad.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <memory>

// Real implementation of WebRTC VAD (Voice Activity Detection)
// Since we can't use the actual WebRTC VAD library directly here,
// this implements a simplified but effective energy-based VAD

namespace {
    // Constants for voice detection
    constexpr float ENERGY_THRESHOLD_FACTOR = 1.5f;
    constexpr float SPEECH_PROB_THRESHOLD = 0.6f;
    constexpr int FRAME_HISTORY_SIZE = 15;
    constexpr float SPECTRAL_FLATNESS_THRESHOLD = 5.0f;
    
    // VAD implementation state
    struct VADState {
        float background_energy;
        float current_energy;
        float speech_probability;
        std::vector<float> energy_history;
        int aggressive_mode;
        
        VADState() : background_energy(0.0f), 
                    current_energy(0.0f), 
                    speech_probability(0.0f), 
                    aggressive_mode(2) {
            energy_history.resize(FRAME_HISTORY_SIZE, 0.0f);
        }
    };
    
    // Compute energy of a frame
    float compute_frame_energy(const int16_t* audio_frame, size_t frame_length) {
        float energy = 0.0f;
        for (size_t i = 0; i < frame_length; i++) {
            float sample = static_cast<float>(audio_frame[i]) / 32768.0f;
            energy += sample * sample;
        }
        return energy / frame_length;
    }
    
    // Compute spectral flatness (simplified)
    float compute_spectral_flatness(const int16_t* audio_frame, size_t frame_length) {
        if (frame_length < 8) return 0.0f;
        
        // Divide the frame into 8 bands and compute energy in each band
        std::vector<float> band_energy(8, 0.0f);
        size_t band_size = frame_length / 8;
        
        for (size_t band = 0; band < 8; band++) {
            size_t start = band * band_size;
            size_t end = (band + 1) * band_size;
            
            for (size_t i = start; i < end && i < frame_length; i++) {
                float sample = static_cast<float>(audio_frame[i]) / 32768.0f;
                band_energy[band] += sample * sample;
            }
            band_energy[band] /= band_size;
        }
        
        // Compute geometric mean
        float log_sum = 0.0f;
        for (float energy : band_energy) {
            log_sum += std::log(energy + 1e-10f);
        }
        float geometric_mean = std::exp(log_sum / 8.0f);
        
        // Compute arithmetic mean
        float arithmetic_mean = 0.0f;
        for (float energy : band_energy) {
            arithmetic_mean += energy;
        }
        arithmetic_mean /= 8.0f;
        
        // Spectral flatness measure
        if (arithmetic_mean < 1e-10f) return 0.0f;
        return geometric_mean / arithmetic_mean;
    }
    
    // Update background energy estimate
    void update_background_energy(VADState* state, float frame_energy, bool is_speech) {
        // If not speech, update background energy with slow adaptation
        if (!is_speech) {
            if (state->background_energy == 0.0f) {
                state->background_energy = frame_energy;
            } else {
                state->background_energy = 0.95f * state->background_energy + 0.05f * frame_energy;
            }
        }
    }
    
    // Determine if frame contains speech
    bool detect_speech(VADState* state, float frame_energy, float spectral_flatness) {
        // Update energy history
        state->energy_history.push_back(frame_energy);
        if (state->energy_history.size() > FRAME_HISTORY_SIZE) {
            state->energy_history.erase(state->energy_history.begin());
        }
        
        // Update current energy (smoothed)
        state->current_energy = 0.0f;
        for (float energy : state->energy_history) {
            state->current_energy += energy;
        }
        state->current_energy /= state->energy_history.size();
        
        // Initialize background energy if needed
        if (state->background_energy == 0.0f) {
            state->background_energy = state->current_energy;
        }
        
        // Compute threshold based on aggressiveness
        float threshold_factor = ENERGY_THRESHOLD_FACTOR;
        switch (state->aggressive_mode) {
            case 0: threshold_factor = 1.2f; break;  // Least aggressive
            case 1: threshold_factor = 1.5f; break;
            case 2: threshold_factor = 2.0f; break;
            case 3: threshold_factor = 2.5f; break;  // Most aggressive
        }
        
        // Compute speech probability based on energy
        float energy_ratio = state->current_energy / (state->background_energy + 1e-10f);
        float energy_speech_prob = std::min(1.0f, std::max(0.0f, 
            (energy_ratio - 1.0f) / (threshold_factor - 1.0f)));
            
        // Include spectral flatness in decision (speech has lower spectral flatness)
        float flatness_factor = std::max(0.0f, 
            1.0f - (spectral_flatness / SPECTRAL_FLATNESS_THRESHOLD));
            
        // Combine factors
        float speech_prob = 0.7f * energy_speech_prob + 0.3f * flatness_factor;
        
        // Smooth the probability over time
        state->speech_probability = 0.7f * state->speech_probability + 0.3f * speech_prob;
        
        // Apply threshold based on aggressiveness
        float threshold = SPEECH_PROB_THRESHOLD;
        switch (state->aggressive_mode) {
            case 0: threshold = 0.5f; break;  // Least aggressive
            case 1: threshold = 0.6f; break;
            case 2: threshold = 0.7f; break;
            case 3: threshold = 0.8f; break;  // Most aggressive
        }
        
        bool is_speech = state->speech_probability > threshold;
        
        // Update background energy
        update_background_energy(state, frame_energy, is_speech);
        
        return is_speech;
    }
}

// Implementation of WebRTC VAD API

extern "C" {

void* WebRtcVad_Create() {
    return new VADState();
}

int WebRtcVad_Init(void* handle) {
    if (!handle) return -1;
    VADState* state = static_cast<VADState*>(handle);
    state->background_energy = 0.0f;
    state->current_energy = 0.0f;
    state->speech_probability = 0.0f;
    std::fill(state->energy_history.begin(), state->energy_history.end(), 0.0f);
    return 0;
}

void WebRtcVad_Free(void* handle) {
    if (handle) {
        delete static_cast<VADState*>(handle);
    }
}

int WebRtcVad_set_mode(void* handle, int mode) {
    if (!handle || mode < 0 || mode > 3) return -1;
    static_cast<VADState*>(handle)->aggressive_mode = mode;
    return 0;
}

int WebRtcVad_Process(void* handle, int sample_rate_hz, const int16_t* audio_frame, size_t frame_length) {
    if (!handle || !audio_frame || frame_length == 0) return -1;
    
    VADState* state = static_cast<VADState*>(handle);
    
    // Compute frame energy
    float frame_energy = compute_frame_energy(audio_frame, frame_length);
    
    // Compute spectral flatness
    float spectral_flatness = compute_spectral_flatness(audio_frame, frame_length);
    
    // Detect speech
    bool is_speech = detect_speech(state, frame_energy, spectral_flatness);
    
    return is_speech ? 1 : 0;
}

} // extern "C"