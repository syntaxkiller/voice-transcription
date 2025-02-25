#include "audio_stream.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <random>

namespace voice_transcription {

// Static initialization
bool ControlledAudioStream::portaudio_initialized_ = false;

// AudioChunk implementation
AudioChunk::AudioChunk(size_t size) : size_(size) {
    data_ = std::make_unique<float[]>(size);
    std::memset(data_.get(), 0, size * sizeof(float));
}

AudioChunk::AudioChunk(const float* data, size_t size) : size_(size) {
    data_ = std::make_unique<float[]>(size);
    std::memcpy(data_.get(), data, size * sizeof(float));
}

AudioChunk::AudioChunk(AudioChunk&& other) noexcept
    : data_(std::move(other.data_)), size_(other.size_) {
    other.size_ = 0;
}

AudioChunk& AudioChunk::operator=(AudioChunk&& other) noexcept {
    if (this != &other) {
        data_ = std::move(other.data_);
        size_ = other.size_;
        other.size_ = 0;
    }
    return *this;
}

// ControlledAudioStream implementation
ControlledAudioStream::ControlledAudioStream(int device_id, int sample_rate, int frames_per_buffer)
    : device_id_(device_id), 
      sample_rate_(sample_rate),
      frames_per_buffer_(frames_per_buffer),
      stream_(nullptr),
      callback_context_(std::make_unique<AudioCallbackContext>()),
      is_paused_(false) {
    
    // Initialize PortAudio if needed
    ensure_portaudio_initialized();
    
    // Set up callback context
    callback_context_->frames_per_buffer = frames_per_buffer;
}

ControlledAudioStream::~ControlledAudioStream() {
    // Stop the stream if it's active
    if (stream_ && is_active()) {
        stop();
    }
}

ControlledAudioStream::ControlledAudioStream(ControlledAudioStream&& other) noexcept
    : device_id_(other.device_id_),
      sample_rate_(other.sample_rate_),
      frames_per_buffer_(other.frames_per_buffer_),
      stream_(other.stream_),
      callback_context_(std::move(other.callback_context_)),
      last_error_(std::move(other.last_error_)),
      is_paused_(other.is_paused_) {
    
    other.stream_ = nullptr;
}

ControlledAudioStream& ControlledAudioStream::operator=(ControlledAudioStream&& other) noexcept {
    if (this != &other) {
        // Stop the current stream if active
        if (stream_ && is_active()) {
            stop();
        }
        
        device_id_ = other.device_id_;
        sample_rate_ = other.sample_rate_;
        frames_per_buffer_ = other.frames_per_buffer_;
        stream_ = other.stream_;
        callback_context_ = std::move(other.callback_context_);
        last_error_ = std::move(other.last_error_);
        is_paused_ = other.is_paused_;
        
        other.stream_ = nullptr;
    }
    return *this;
}

bool ControlledAudioStream::start() {
    try {
        // Stop the stream if it's already active
        if (stream_) {
            stop();
        }
        
        // Clear any previous error
        last_error_.clear();
        
        // Input parameters
        PaStreamParameters inputParams;
        std::memset(&inputParams, 0, sizeof(inputParams));
        
        // Validate device ID
        if (device_id_ < 0 || device_id_ >= Pa_GetDeviceCount()) {
            last_error_ = "Invalid device ID";
            return false;
        }
        
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(device_id_);
        if (!deviceInfo) {
            last_error_ = "Failed to get device info";
            return false;
        }
        
        if (deviceInfo->maxInputChannels <= 0) {
            last_error_ = "Selected device doesn't support input";
            return false;
        }
        
        inputParams.device = device_id_;
        inputParams.channelCount = 1;  // Mono
        inputParams.sampleFormat = paFloat32;  // 32-bit float format
        inputParams.suggestedLatency = deviceInfo->defaultLowInputLatency;
        inputParams.hostApiSpecificStreamInfo = nullptr;
        
        // Validate sample rate
        PaError sampleRateError = Pa_IsFormatSupported(&inputParams, nullptr, sample_rate_);
        if (sampleRateError != paFormatIsSupported) {
            last_error_ = std::string("Sample rate not supported: ") + Pa_GetErrorText(sampleRateError);
            return false;
        }
        
        // Open the stream
        PaError err = Pa_OpenStream(
            &stream_,
            &inputParams,  // Input parameters
            nullptr,       // No output parameters (we're only capturing)
            sample_rate_,
            frames_per_buffer_,
            paClipOff,     // Don't clip input
            audio_callback,
            callback_context_.get()
        );
        
        if (err != paNoError) {
            last_error_ = std::string("Failed to open audio stream: ") + Pa_GetErrorText(err);
            return false;
        }
        
        // Start the stream
        err = Pa_StartStream(stream_);
        if (err != paNoError) {
            last_error_ = std::string("Failed to start audio stream: ") + Pa_GetErrorText(err);
            Pa_CloseStream(stream_);
            stream_ = nullptr;
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        last_error_ = std::string("Exception in start(): ") + e.what();
        return false;
    }
    catch (...) {
        last_error_ = "Unknown exception in start()";
        return false;
    }
}