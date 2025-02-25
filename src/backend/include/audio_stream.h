#pragma once

#include <portaudio.h>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <queue>
#include <functional>
#include <optional>
#include <stdexcept>
#include <atomic>
#include <condition_variable>

namespace voice_transcription {

// Audio device information structure
struct AudioDevice {
    int id;
    std::string raw_name;
    std::string label;
    bool is_default;
    std::vector<int> supported_sample_rates;
};

// Audio chunk structure
class AudioChunk {
public:
    AudioChunk(size_t size);
    AudioChunk(const float* data, size_t size);
    ~AudioChunk() = default;

    // No copy operations - we want to move these chunks around
    AudioChunk(const AudioChunk&) = delete;
    AudioChunk& operator=(const AudioChunk&) = delete;

    // Move operations
    AudioChunk(AudioChunk&&) noexcept;
    AudioChunk& operator=(AudioChunk&&) noexcept;

    const float* data() const { return data_.get(); }
    float* data() { return data_.get(); }
    size_t size() const { return size_; }

private:
    std::unique_ptr<float[]> data_;
    size_t size_;
};

// Exception for audio stream errors
class AudioStreamException : public std::runtime_error {
public:
    explicit AudioStreamException(const std::string& message)
        : std::runtime_error(message) {}
};

// Audio callback context
struct AudioCallbackContext {
    std::queue<std::unique_ptr<AudioChunk>> audio_queue;
    std::mutex queue_mutex;
	std::atomic<bool> stop_requested{false};
    std::condition_variable queue_cv;
    int frames_per_buffer;
};

// Controlled audio stream class
class ControlledAudioStream {
public:
    ControlledAudioStream(int device_id, int sample_rate, int frames_per_buffer);
    ~ControlledAudioStream();

    // No copy operations
    ControlledAudioStream(const ControlledAudioStream&) = delete;
    ControlledAudioStream& operator=(const ControlledAudioStream&) = delete;
    
    // Move operations
    ControlledAudioStream(ControlledAudioStream&&) noexcept;
    ControlledAudioStream& operator=(ControlledAudioStream&&) noexcept;

    // Stream control methods
    bool start();
    void stop();
    void pause();
    void resume();
    bool is_active() const;

    // Getter methods
    int get_device_id() const { return device_id_; }
    int get_sample_rate() const { return sample_rate_; }
    int get_frames_per_buffer() const { return frames_per_buffer_; }
    std::string get_last_error() const { return last_error_; }

    // Audio data retrieval
    std::optional<std::unique_ptr<AudioChunk>> get_next_chunk();
    
    // Static methods for device management
    static std::vector<AudioDevice> enumerate_devices();
    static bool check_device_compatibility(int device_id, int required_sample_rate);
	
	std::atomic<bool> is_transcribing_{false};

private:
    // Private method to initialize PortAudio if needed
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
    AudioCallbackContext callback_context_;
    std::string last_error_;
    bool is_paused_;
    
    // Static initialization flag
    static bool portaudio_initialized_;
};

} // namespace voice_transcription