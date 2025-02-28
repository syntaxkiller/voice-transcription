#include "audio_stream.h"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <thread>
#include <condition_variable>

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

// Use direct construction for buffer (more efficient)
AudioCallbackContext::AudioCallbackContext() 
    : buffer(MAX_BUFFER_SIZE, 0.0f) {
    // The in-class initializers handle other members
}

// Write data to the circular buffer
void AudioCallbackContext::write_data(const float* data, size_t length) {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    
    // Check for buffer overflow
    // Calculate available data first
    size_t data_available = (buffer_pos >= read_pos) ? 
        (buffer_pos - read_pos) : 
        (MAX_BUFFER_SIZE - read_pos + buffer_pos);
        
    // Then calculate available space
    size_t available_space = MAX_BUFFER_SIZE - data_available;
    if (length > available_space) {
        buffer_overflow = true;
        
        // Overwrite old data by advancing read_pos
        read_pos = (read_pos + (length - available_space)) % MAX_BUFFER_SIZE;
    }
    
    // Write data to the circular buffer
    for (size_t i = 0; i < length; i++) {
        buffer[(buffer_pos + i) % MAX_BUFFER_SIZE] = data[i];
    }
    
    buffer_pos = (buffer_pos + length) % MAX_BUFFER_SIZE;
    
    // Notify waiting threads that data is ready
    data_ready_cv.notify_one();
}

// Read data from the circular buffer
size_t AudioCallbackContext::read_data(float* output, size_t length) {
    std::unique_lock<std::mutex> lock(buffer_mutex);
    
    // Calculate available data
    size_t data_available = (buffer_pos >= read_pos) ? 
        (buffer_pos - read_pos) : 
        (MAX_BUFFER_SIZE - read_pos + buffer_pos);
        
    if (data_available < length) {
        // Not enough data yet
        return 0;
    }
    
    // Read data from the circular buffer
    for (size_t i = 0; i < length; i++) {
        output[i] = buffer[(read_pos + i) % MAX_BUFFER_SIZE];
    }
    
    read_pos = (read_pos + length) % MAX_BUFFER_SIZE;
    
    // Reset overflow flag
    bool had_overflow = buffer_overflow;
    buffer_overflow = false;
    
    return length;
}

// Wait for data with timeout
bool AudioCallbackContext::wait_for_data(size_t min_samples, int timeout_ms) {
    std::unique_lock<std::mutex> lock(buffer_mutex);
    
    // Check if we already have enough data
    size_t data_available = (buffer_pos >= read_pos) ? 
        (buffer_pos - read_pos) : 
        (MAX_BUFFER_SIZE - read_pos + buffer_pos);
        
    if (data_available >= min_samples) {
        return true;
    }
    
    // Wait for data with timeout
    return data_ready_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
        [this, min_samples]() {
            size_t data_available = (buffer_pos >= read_pos) ? 
                (buffer_pos - read_pos) : 
                (MAX_BUFFER_SIZE - read_pos + buffer_pos);
            return data_available >= min_samples;
        });
}

// Clear buffer
void AudioCallbackContext::clear() {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    read_pos = buffer_pos;
    buffer_overflow = false;
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

// Improved start method with priming of the audio buffer
bool ControlledAudioStream::start() {
    try {
        // Stop any existing stream
        if (stream_) {
            stop();
        }
        
        // Clear any previous error
        last_error_.clear();
        
        // Reset audio context
        callback_context_ = std::make_unique<AudioCallbackContext>();
        callback_context_->frames_per_buffer = frames_per_buffer_;
        
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
        
        // Open the stream with reduced latency settings
        PaError err = Pa_OpenStream(
            &stream_,
            &inputParams,  // Input parameters
            nullptr,       // No output parameters (we're only capturing)
            sample_rate_,
            frames_per_buffer_,
            paClipOff | paPrimeOutputBuffersUsingStreamCallback,  // Don't clip input, prime buffers
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
        
        // Let the stream run for a short time to prime the buffer
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        return true;
    }
    catch (const std::exception& e) {
        last_error_ = std::string("Exception in start(): ") + e.what();
        return false;
    }
}

// Improved stop method with better resource cleanup
void ControlledAudioStream::stop() {
    try {
        if (stream_) {
            // Stop the stream if it's active
            if (Pa_IsStreamActive(stream_) == 1) {
                PaError err = Pa_StopStream(stream_);
                if (err != paNoError) {
                    last_error_ = std::string("Failed to stop stream: ") + Pa_GetErrorText(err);
                }
            }
            
            // Close the stream
            PaError err = Pa_CloseStream(stream_);
            if (err != paNoError) {
                last_error_ = std::string("Failed to close stream: ") + Pa_GetErrorText(err);
            }
            
            stream_ = nullptr;
        }
        
        // Reset buffer state
        if (callback_context_) {
            callback_context_->clear();
        }
        
        is_paused_ = false;
    }
    catch (const std::exception& e) {
        last_error_ = std::string("Exception in stop(): ") + e.what();
        stream_ = nullptr;
    }
}
void ControlledAudioStream::pause() {
    is_paused_ = true;
}

void ControlledAudioStream::resume() {
    is_paused_ = false;
}

bool ControlledAudioStream::is_active() const {
    return stream_ && Pa_IsStreamActive(stream_) == 1;
}

// Enhanced method to get the next audio chunk with better latency
std::optional<AudioChunk> ControlledAudioStream::get_next_chunk(int timeout_ms) {
    if (!is_active() || is_paused_) {
        return std::nullopt;
    }
    
    try {
        // Wait for enough data with timeout
        if (!callback_context_->wait_for_data(frames_per_buffer_, timeout_ms)) {
            return std::nullopt;
        }
        
        // Create a new chunk to hold the data
        auto chunk = std::make_optional<AudioChunk>(frames_per_buffer_);
        
        // Read data from the circular buffer
        size_t bytes_read = callback_context_->read_data(chunk->data(), frames_per_buffer_);
        
        if (bytes_read != frames_per_buffer_) {
            return std::nullopt;
        }
        
        return chunk;
    }
    catch (const std::exception& e) {
        last_error_ = std::string("Exception in get_next_chunk(): ") + e.what();
        return std::nullopt;
    }
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

std::vector<AudioDevice> ControlledAudioStream::enumerate_devices() {
    // Make sure PortAudio is initialized
    ensure_portaudio_initialized();
    
    std::vector<AudioDevice> devices;
    int numDevices = Pa_GetDeviceCount();
    
    if (numDevices < 0) {
        // Error occurred
        return devices;
    }
    
    // Default input device
    int defaultInputDevice = Pa_GetDefaultInputDevice();
    
    // Iterate through all devices
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        
        if (deviceInfo && deviceInfo->maxInputChannels > 0) {
            // This device has input channels, add it to the list
            AudioDevice device;
            device.id = i;
            device.raw_name = deviceInfo->name;
            
            // Create a more user-friendly label
            const PaHostApiInfo* hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
            std::string hostName = hostInfo ? hostInfo->name : "Unknown";
            
            device.label = deviceInfo->name;
            
            // Check if it's the default device
            device.is_default = (i == defaultInputDevice);
            
            // Add some common sample rates to check
            const int sampleRates[] = { 8000, 16000, 22050, 32000, 44100, 48000, 96000 };
            
            for (int rate : sampleRates) {
                // Check if the device supports this sample rate
                PaStreamParameters params;
                params.device = i;
                params.channelCount = 1;
                params.sampleFormat = paFloat32;
                params.suggestedLatency = deviceInfo->defaultLowInputLatency;
                params.hostApiSpecificStreamInfo = nullptr;
                
                PaError err = Pa_IsFormatSupported(&params, nullptr, rate);
                if (err == paFormatIsSupported) {
                    device.supported_sample_rates.push_back(rate);
                }
            }
            
            devices.push_back(device);
        }
    }
    
    return devices;
}

bool ControlledAudioStream::check_device_compatibility(int device_id, int sample_rate) {
    // Make sure PortAudio is initialized
    ensure_portaudio_initialized();
    
    if (device_id < 0 || device_id >= Pa_GetDeviceCount()) {
        return false;
    }
    
    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(device_id);
    if (!deviceInfo || deviceInfo->maxInputChannels <= 0) {
        return false;
    }
    
    // Check if the device supports the required sample rate
    PaStreamParameters params;
    params.device = device_id;
    params.channelCount = 1;
    params.sampleFormat = paFloat32;
    params.suggestedLatency = deviceInfo->defaultLowInputLatency;
    params.hostApiSpecificStreamInfo = nullptr;
    
    PaError err = Pa_IsFormatSupported(&params, nullptr, sample_rate);
    return (err == paFormatIsSupported);
}

// Improved audio callback function for PortAudio
int ControlledAudioStream::audio_callback(
    const void* input_buffer,
    void* output_buffer,
    unsigned long frames_per_buffer,
    const PaStreamCallbackTimeInfo* time_info,
    PaStreamCallbackFlags status_flags,
    void* user_data
) {
    // Cast the user data to our context type
    AudioCallbackContext* context = static_cast<AudioCallbackContext*>(user_data);
    
    if (!context || context->is_paused) {
        return paContinue;
    }
    
    // Get the input buffer
    const float* in = static_cast<const float*>(input_buffer);
    
    if (in) {
        // Write data to the circular buffer
        context->write_data(in, frames_per_buffer);
    }
    
    return paContinue;
}

} // namespace voice_transcription