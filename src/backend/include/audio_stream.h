#ifndef AUDIO_STREAM_H
#define AUDIO_STREAM_H

#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <optional>
#include <functional>
#include <portaudio.h>

namespace voice_transcription {

// Exception class for audio stream errors
class AudioStreamException : public std::runtime_error {
public:
    explicit AudioStreamException(const std::string& message)
        : std::runtime_error(message) {}
};

// Audio device information
struct AudioDevice {
    int id;
    std::string raw_name;
    std::string label;
    bool is_default;
    std::vector<int> supported_sample_rates;
};

// Audio chunk class for handling blocks of audio data
class AudioChunk {
public:
    // Constructor for an empty chunk of given size
    explicit AudioChunk(size_t size);
    
    // Constructor from raw audio data
    AudioChunk(const float* data, size_t size);
    
    // Move constructor
    AudioChunk(AudioChunk&& other) noexcept;
    
    // Move assignment operator
    AudioChunk& operator=(AudioChunk&& other) noexcept;
    
    // Delete copy constructor and assignment
    AudioChunk(const AudioChunk&) = delete;
    AudioChunk& operator=(const AudioChunk&) = delete;
    
    // Access methods
    size_t size() const { return size_; }
    float* data() { return data_.get(); }
    const float* data() const { return data_.get(); }
    
private:
    std::unique_ptr<float[]> data_;
    size_t size_;
};

// Audio callback context structure
struct AudioCallbackContext {
    int frames_per_buffer = 0;
    std::vector<float> buffer;
    size_t buffer_pos = 0;
    std::mutex buffer_mutex;
    bool is_paused = false;
    
    // Define move constructor and assignment
    AudioCallbackContext() = default;
    
    // Delete copy constructor and assignment
    AudioCallbackContext(const AudioCallbackContext&) = delete;
    AudioCallbackContext& operator=(const AudioCallbackContext&) = delete;
    
    // Define move operations
    AudioCallbackContext(AudioCallbackContext&&) = default;
    AudioCallbackContext& operator=(AudioCallbackContext&&) = default;
};

// PortAudio stream wrapper with controlled buffering
class ControlledAudioStream {
public:
    // Constructor
    ControlledAudioStream(int device_id, int sample_rate, int frames_per_buffer);
    
    // Destructor
    ~ControlledAudioStream();
    
    // Move operations
    ControlledAudioStream(ControlledAudioStream&& other) noexcept;
    ControlledAudioStream& operator=(ControlledAudioStream&& other) noexcept;
    
    // Delete copy operations
    ControlledAudioStream(const ControlledAudioStream&) = delete;
    ControlledAudioStream& operator=(const ControlledAudioStream&) = delete;
    
    // Stream control
    bool start();
    void stop();
    void pause();
    void resume();
    bool is_active() const;
    
    // Buffer access
    std::optional<AudioChunk> get_next_chunk(int timeout_ms = 0);
    
    // Device information
    int get_device_id() const { return device_id_; }
    int get_sample_rate() const { return sample_rate_; }
    int get_frames_per_buffer() const { return frames_per_buffer_; }
    std::string get_last_error() const { return last_error_; }
    
    // Static methods
    static std::vector<AudioDevice> enumerate_devices();
    static bool check_device_compatibility(int device_id, int sample_rate);
    
private:
    // PortAudio initialize/terminate
    static void ensure_portaudio_initialized();
    
    // Audio callback function
    static int audio_callback(const void* input_buffer, void* output_buffer,
                             unsigned long frames_per_buffer,
                             const PaStreamCallbackTimeInfo* time_info,
                             PaStreamCallbackFlags status_flags,
                             void* user_data);
    
    // Member variables
    int device_id_;
    int sample_rate_;
    int frames_per_buffer_;
    PaStream* stream_;
    std::unique_ptr<AudioCallbackContext> callback_context_;
    std::string last_error_;
    bool is_paused_;
    
    // Static PortAudio initialization flag
    static bool portaudio_initialized_;
};

} // namespace voice_transcription

#endif // AUDIO_STREAM_H