#include "portaudio.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <random>
#include <iostream>

// Mock implementation of PortAudio API

// Static storage for device info
static const int MAX_DEVICES = 5;
static PaDeviceInfo device_info[MAX_DEVICES];
static int num_devices = 0;
static bool pa_initialized = false;
static std::mt19937 rng; // Random number generator

// Helper function to setup mock devices
static void setup_mock_devices() {
    if (num_devices > 0) {
        return; // Already initialized
    }
    
    // Seed random number generator
    rng.seed(static_cast<unsigned int>(std::time(nullptr)));
    
    // Create a few mock devices
    num_devices = 3;
    
    // Device 0 - Default microphone
    device_info[0].name = "Built-in Microphone (Mock)";
    device_info[0].maxInputChannels = 1;
    device_info[0].maxOutputChannels = 0;
    device_info[0].defaultLowInputLatency = 0.01;
    device_info[0].defaultHighInputLatency = 0.1;
    device_info[0].defaultLowOutputLatency = 0.0;
    device_info[0].defaultHighOutputLatency = 0.0;
    device_info[0].defaultSampleRate = 16000;
    device_info[0].hostApi = 0;
    
    // Device 1 - USB headset
    device_info[1].name = "USB Headset (Mock)";
    device_info[1].maxInputChannels = 1;
    device_info[1].maxOutputChannels = 2;
    device_info[1].defaultLowInputLatency = 0.02;
    device_info[1].defaultHighInputLatency = 0.1;
    device_info[1].defaultLowOutputLatency = 0.02;
    device_info[1].defaultHighOutputLatency = 0.1;
    device_info[1].defaultSampleRate = 48000;
    device_info[1].hostApi = 0;
    
    // Device 2 - Bluetooth headset
    device_info[2].name = "Bluetooth Headset (Mock)";
    device_info[2].maxInputChannels = 1;
    device_info[2].maxOutputChannels = 2;
    device_info[2].defaultLowInputLatency = 0.05;
    device_info[2].defaultHighInputLatency = 0.2;
    device_info[2].defaultLowOutputLatency = 0.05;
    device_info[2].defaultHighOutputLatency = 0.2;
    device_info[2].defaultSampleRate = 16000;
    device_info[2].hostApi = 0;
}

// Mock PortAudio API implementations

PaError Pa_Initialize(void) {
    setup_mock_devices();
    pa_initialized = true;
    return paNoError;
}

PaError Pa_Terminate(void) {
    pa_initialized = false;
    return paNoError;
}

int Pa_GetDeviceCount(void) {
    if (!pa_initialized) {
        return paNotInitialized;
    }
    return num_devices;
}

const PaDeviceInfo* Pa_GetDeviceInfo(PaDeviceIndex device) {
    if (!pa_initialized) {
        return nullptr;
    }
    
    if (device < 0 || device >= num_devices) {
        return nullptr;
    }
    
    return &device_info[device];
}

PaDeviceIndex Pa_GetDefaultInputDevice(void) {
    if (!pa_initialized) {
        return paNoDevice;
    }
    
    // Return first device with input channels as default
    for (int i = 0; i < num_devices; i++) {
        if (device_info[i].maxInputChannels > 0) {
            return i;
        }
    }
    
    return paNoDevice;
}

PaDeviceIndex Pa_GetDefaultOutputDevice(void) {
    if (!pa_initialized) {
        return paNoDevice;
    }
    
    // Return first device with output channels as default
    for (int i = 0; i < num_devices; i++) {
        if (device_info[i].maxOutputChannels > 0) {
            return i;
        }
    }
    
    return paNoDevice;
}

PaError Pa_IsFormatSupported(const PaStreamParameters* inputParameters,
                             const PaStreamParameters* outputParameters,
                             double sampleRate) {
    if (!pa_initialized) {
        return paNotInitialized;
    }
    
    // Check input parameters
    if (inputParameters) {
        if (inputParameters->device < 0 || inputParameters->device >= num_devices) {
            return paInvalidDevice;
        }
        
        if (inputParameters->channelCount <= 0 ||
            inputParameters->channelCount > device_info[inputParameters->device].maxInputChannels) {
            return paInvalidChannelCount;
        }
    }
    
    // Check output parameters
    if (outputParameters) {
        if (outputParameters->device < 0 || outputParameters->device >= num_devices) {
            return paInvalidDevice;
        }
        
        if (outputParameters->channelCount <= 0 ||
            outputParameters->channelCount > device_info[outputParameters->device].maxOutputChannels) {
            return paInvalidChannelCount;
        }
    }
    
    // Check sample rate (allow 8000, 16000, 32000, 44100, 48000)
    if (sampleRate != 8000 && sampleRate != 16000 && sampleRate != 32000 &&
        sampleRate != 44100 && sampleRate != 48000) {
        return paInvalidSampleRate;
    }
    
    // For device 0, only 16000 Hz is allowed (to match Vosk requirement)
    if ((inputParameters && inputParameters->device == 0 && sampleRate != 16000) ||
        (outputParameters && outputParameters->device == 0 && sampleRate != 16000)) {
        return paInvalidSampleRate;
    }
    
    return paFormatIsSupported;
}

// Mock structure to store stream data
struct MockPaStream {
    int device_id;
    int channel_count;
    float sample_rate;
    unsigned long frames_per_buffer;
    void* user_data;
    PaStreamCallback* callback;
    bool is_active;
    std::vector<float> dummy_buffer;
    std::chrono::time_point<std::chrono::steady_clock> start_time;
};

PaError Pa_OpenStream(PaStream** stream,
                      const PaStreamParameters* inputParameters,
                      const PaStreamParameters* outputParameters,
                      double sampleRate,
                      unsigned long framesPerBuffer,
                      PaStreamFlags streamFlags,
                      PaStreamCallback* streamCallback,
                      void* userData) {
    if (!pa_initialized) {
        return paNotInitialized;
    }
    
    if (!stream) {
        return paBadStreamPtr;
    }
    
    if (!inputParameters && !outputParameters) {
        return paInvalidChannelCount;
    }
    
    // Validate input parameters
    if (inputParameters) {
        if (inputParameters->device < 0 || inputParameters->device >= num_devices) {
            return paInvalidDevice;
        }
        
        if (inputParameters->channelCount <= 0 ||
            inputParameters->channelCount > device_info[inputParameters->device].maxInputChannels) {
            return paInvalidChannelCount;
        }
    }
    
    // Validate output parameters
    if (outputParameters) {
        if (outputParameters->device < 0 || outputParameters->device >= num_devices) {
            return paInvalidDevice;
        }
        
        if (outputParameters->channelCount <= 0 ||
            outputParameters->channelCount > device_info[outputParameters->device].maxOutputChannels) {
            return paInvalidChannelCount;
        }
    }
    
    // Create mock stream
    MockPaStream* mock_stream = new MockPaStream();
    mock_stream->device_id = inputParameters ? inputParameters->device : (outputParameters ? outputParameters->device : 0);
    mock_stream->channel_count = inputParameters ? inputParameters->channelCount : (outputParameters ? outputParameters->channelCount : 1);
    mock_stream->sample_rate = static_cast<float>(sampleRate);
    mock_stream->frames_per_buffer = framesPerBuffer;
    mock_stream->user_data = userData;
    mock_stream->callback = streamCallback;
    mock_stream->is_active = false;
    mock_stream->dummy_buffer.resize(framesPerBuffer * mock_stream->channel_count, 0.0f);
    
    *stream = static_cast<PaStream*>(mock_stream);
    return paNoError;
}

PaError Pa_CloseStream(PaStream* stream) {
    if (!pa_initialized) {
        return paNotInitialized;
    }
    
    if (!stream) {
        return paBadStreamPtr;
    }
    
    MockPaStream* mock_stream = static_cast<MockPaStream*>(stream);
    
    // Clean up mock stream
    delete mock_stream;
    
    return paNoError;
}

PaError Pa_StartStream(PaStream* stream) {
    if (!pa_initialized) {
        return paNotInitialized;
    }
    
    if (!stream) {
        return paBadStreamPtr;
    }
    
    MockPaStream* mock_stream = static_cast<MockPaStream*>(stream);
    
    if (mock_stream->is_active) {
        return paStreamIsNotStopped;
    }
    
    mock_stream->is_active = true;
    mock_stream->start_time = std::chrono::steady_clock::now();
    
    return paNoError;
}

PaError Pa_StopStream(PaStream* stream) {
    if (!pa_initialized) {
        return paNotInitialized;
    }
    
    if (!stream) {
        return paBadStreamPtr;
    }
    
    MockPaStream* mock_stream = static_cast<MockPaStream*>(stream);
    
    if (!mock_stream->is_active) {
        return paStreamIsStopped;
    }
    
    mock_stream->is_active = false;
    
    return paNoError;
}

PaError Pa_IsStreamActive(PaStream* stream) {
    if (!pa_initialized) {
        return paNotInitialized;
    }
    
    if (!stream) {
        return paBadStreamPtr;
    }
    
    MockPaStream* mock_stream = static_cast<MockPaStream*>(stream);
    
    return mock_stream->is_active ? 1 : 0;
}

PaError Pa_IsStreamStopped(PaStream* stream) {
    if (!pa_initialized) {
        return paNotInitialized;
    }
    
    if (!stream) {
        return paBadStreamPtr;
    }
    
    MockPaStream* mock_stream = static_cast<MockPaStream*>(stream);
    
    return !mock_stream->is_active ? 1 : 0;
}

PaError Pa_GetStreamReadAvailable(PaStream* stream) {
    if (!pa_initialized) {
        return paNotInitialized;
    }
    
    if (!stream) {
        return paBadStreamPtr;
    }
    
    // Simulate some available frames
    return 1024;
}

PaError Pa_GetStreamWriteAvailable(PaStream* stream) {
    if (!pa_initialized) {
        return paNotInitialized;
    }
    
    if (!stream) {
        return paBadStreamPtr;
    }
    
    // Simulate some available frames
    return 1024;
}

PaTime Pa_GetStreamTime(PaStream* stream) {
    if (!pa_initialized || !stream) {
        return 0.0;
    }
    
    MockPaStream* mock_stream = static_cast<MockPaStream*>(stream);
    
    // Return elapsed time in seconds
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - mock_stream->start_time);
    
    return elapsed.count() / 1000.0;
}

const char* Pa_GetErrorText(PaError errorCode) {
    switch (errorCode) {
        case paNoError: return "Success";
        case paNotInitialized: return "PortAudio not initialized";
        case paUnanticipatedHostError: return "Unanticipated host error";
        case paInvalidChannelCount: return "Invalid channel count";
        case paInvalidSampleRate: return "Invalid sample rate";
        case paInvalidDevice: return "Invalid device";
        case paInvalidFlag: return "Invalid flag";
        case paStreamIsNotStopped: return "Stream is not stopped";
        case paStreamIsStopped: return "Stream is stopped";
        case paBadStreamPtr: return "Bad stream pointer";
        default: return "Unknown error";
    }
}