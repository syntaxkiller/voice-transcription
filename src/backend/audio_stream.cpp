#include "audio_stream.h"
#include <cstring>
#include <iostream>
#include <sstream>

namespace voice_transcription {

// Static initialization
bool ControlledAudioStream::portaudio_initialized_ = false;
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
            &callback_context_
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
      is_paused_(false) {
    
    // Initialize PortAudio if needed
    ensure_portaudio_initialized();
    
    // Set up callback context
    callback_context_.frames_per_buffer = frames_per_buffer;
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
    // Stop the stream if it's already active
    if (stream_) {
        stop();
    }
    
    // Clear any previous error
    last_error_.clear();
    
    // Input parameters
    PaStreamParameters inputParams;
    std::memset(&inputParams, 0, sizeof(inputParams));
    inputParams.device = device_id_;
    inputParams.channelCount = 1;  // Mono
    inputParams.sampleFormat = paFloat32;  // 32-bit float format
    inputParams.suggestedLatency = Pa_GetDeviceInfo(device_id_)->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;
    
    // Open the stream
    PaError err = Pa_OpenStream(
        &stream_,
        &inputParams,  // Input parameters
        nullptr,       // No output parameters (we're only capturing)
        sample_rate_,
        frames_per_buffer_,
        paClipOff,     // Don't clip input
        audio_callback,
        &callback_context_
    );
    
    if (err != paNoError) {
        last_error_ = Pa_GetErrorText(err);
        return false;
    }
    
    // Start the stream
    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        last_error_ = Pa_GetErrorText(err);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        return false;
    }
    
    return true;
}

void ControlledAudioStream::stop() {
    if (!stream_) {
        return;
    }
    
    // Stop and close the stream
    if (Pa_IsStreamActive(stream_)) {
        Pa_StopStream(stream_);
    }
    
    Pa_CloseStream(stream_);
    stream_ = nullptr;
    
    // Clear the audio queue
    std::lock_guard<std::mutex> lock(callback_context_.queue_mutex);
    while (!callback_context_.audio_queue.empty()) {
        callback_context_.audio_queue.pop();
    }
}

void ControlledAudioStream::pause() {
    if (!stream_ || is_paused_) {
        return;
    }
    
    if (Pa_IsStreamActive(stream_)) {
        Pa_StopStream(stream_);
        is_paused_ = true;
    }
}

void ControlledAudioStream::resume() {
    if (!stream_ || !is_paused_) {
        return;
    }
    
    PaError err = Pa_StartStream(stream_);
    if (err == paNoError) {
        is_paused_ = false;
    } else {
        last_error_ = Pa_GetErrorText(err);
    }
}

bool ControlledAudioStream::is_active() const {
    if (!stream_) {
        return false;
    }
    
    if (is_paused_) {
        return false;
    }
    
    return Pa_IsStreamActive(stream_) == 1;
}

std::optional<std::unique_ptr<AudioChunk>> ControlledAudioStream::get_next_chunk(int timeout_ms) {
    if (!stream_ || !is_active()) {
        return std::nullopt;
    }
    
    // Use a condition variable to wait for data with timeout
    std::unique_lock<std::mutex> lock(callback_context_.queue_mutex);
    
    // Wait for data or timeout
    if (timeout_ms > 0) {
        bool has_data = callback_context_.queue_cv.wait_for(
            lock, 
            std::chrono::milliseconds(timeout_ms),
            [this]() { 
                return !callback_context_.audio_queue.empty() || 
                       callback_context_.stop_requested;
            }
        );
        
        if (!has_data || callback_context_.stop_requested) {
            return std::nullopt;
        }
    } else if (callback_context_.audio_queue.empty()) {
        return std::nullopt;
    }
    
    // Get the next chunk from the queue
    auto chunk = std::move(callback_context_.audio_queue.front());
    callback_context_.audio_queue.pop();
    return std::move(chunk);
}

// Update stop method
void ControlledAudioStream::stop() {
    if (!stream_) {
        return;
    }
    
    // Signal threads to stop
    callback_context_.stop_requested = true;
    callback_context_.queue_cv.notify_all();
    
    // Stop and close the stream
    if (Pa_IsStreamActive(stream_)) {
        Pa_StopStream(stream_);
    }
    
    Pa_CloseStream(stream_);
    stream_ = nullptr;
    
    // Clear the audio queue
    std::lock_guard<std::mutex> lock(callback_context_.queue_mutex);
    while (!callback_context_.audio_queue.empty()) {
        callback_context_.audio_queue.pop();
    }
    
    // Reset stop flag
    callback_context_.stop_requested = false;
}

std::vector<AudioDevice> ControlledAudioStream::enumerate_devices() {
    // Initialize PortAudio if needed
    ensure_portaudio_initialized();
    
    std::vector<AudioDevice> devices;
    
    // Get the default input device
    int default_device = Pa_GetDefaultInputDevice();
    if (default_device == paNoDevice) {
        default_device = -1;  // No default device
    }
    
    // Enumerate all devices
    int device_count = Pa_GetDeviceCount();
    for (int i = 0; i < device_count; i++) {
        const PaDeviceInfo* device_info = Pa_GetDeviceInfo(i);
        
        // Skip output-only devices
        if (device_info->maxInputChannels <= 0) {
            continue;
        }
        
        AudioDevice device;
        device.id = i;
        device.raw_name = device_info->name;
        
        // Create a friendly label
        std::string host_api_name = Pa_GetHostApiInfo(device_info->hostApi)->name;
        device.label = std::string(device_info->name) + " (" + host_api_name + ")";
        
        // Check if this is the default device
        device.is_default = (i == default_device);
        
        // Check supported sample rates
        std::vector<int> standard_rates = {8000, 11025, 16000, 22050, 44100, 48000};
        for (int rate : standard_rates) {
            PaStreamParameters inputParams;
            std::memset(&inputParams, 0, sizeof(inputParams));
            inputParams.device = i;
            inputParams.channelCount = 1;  // Mono
            inputParams.sampleFormat = paFloat32;
            inputParams.suggestedLatency = device_info->defaultLowInputLatency;
            inputParams.hostApiSpecificStreamInfo = nullptr;
            
            if (Pa_IsFormatSupported(&inputParams, nullptr, rate) == paFormatIsSupported) {
                device.supported_sample_rates.push_back(rate);
            }
        }
        
        devices.push_back(device);
    }
    
    return devices;
}

bool ControlledAudioStream::check_device_compatibility(int device_id, int required_sample_rate) {
    // Initialize PortAudio if needed
    ensure_portaudio_initialized();
    
    // Check if device exists
    if (device_id < 0 || device_id >= Pa_GetDeviceCount()) {
        return false;
    }
    
    // Check if device supports input
    const PaDeviceInfo* device_info = Pa_GetDeviceInfo(device_id);
    if (!device_info || device_info->maxInputChannels <= 0) {
        return false;
    }
    
    // Check if device supports the required sample rate
    PaStreamParameters inputParams;
    std::memset(&inputParams, 0, sizeof(inputParams));
    inputParams.device = device_id;
    inputParams.channelCount = 1;  // Mono
    inputParams.sampleFormat = paFloat32;
    inputParams.suggestedLatency = device_info->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;
    
    return Pa_IsFormatSupported(&inputParams, nullptr, required_sample_rate) == paFormatIsSupported;
}

void ControlledAudioStream::ensure_portaudio_initialized() {
    if (!portaudio_initialized_) {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            throw AudioStreamException(std::string("Failed to initialize PortAudio: ") + Pa_GetErrorText(err));
        }
        portaudio_initialized_ = true;
    }
}

int ControlledAudioStream::audio_callback(
    const void* input_buffer,
    void* output_buffer,
    unsigned long frames_per_buffer,
    const PaStreamCallbackTimeInfo* time_info,
    PaStreamCallbackFlags status_flags,
    void* user_data) {
    
    // Cast user data to our context
    auto* context = static_cast<AudioCallbackContext*>(user_data);
    
    // Check if stop was requested
    if (context->stop_requested) {
        return paComplete;
    }
    
    // Create a new audio chunk with the input data
    const float* input = static_cast<const float*>(input_buffer);
    auto chunk = std::make_unique<AudioChunk>(input, frames_per_buffer);
    
    // Add the chunk to the queue
    {
        std::lock_guard<std::mutex> lock(context->queue_mutex);
        context->audio_queue.push(std::move(chunk));
        
        // If the queue is getting too large, remove old chunks
        const size_t MAX_QUEUE_SIZE = 100;  // Arbitrary limit to prevent memory issues
        while (context->audio_queue.size() > MAX_QUEUE_SIZE) {
            context->audio_queue.pop();
        }
    }
    
    // Notify waiting threads
    context->queue_cv.notify_one();
    
    return paContinue;
}

} // namespace voice_transcription