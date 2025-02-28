#include "vosk_transcription_engine.h"
#include "webrtc_vad.h"  // Add this explicit include
#include <chrono>
#include <algorithm>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
namespace voice_transcription {

// Simple noise filter implementation
class NoiseFilter {
public:
    NoiseFilter(float threshold = 0.05f, int window_size = 10)
        : noise_threshold_(threshold), window_size_(window_size) {
        noise_floor_ = 0.0f;
        calibrated_ = false;
    }
    
    // Process an audio chunk to remove background noise
    void filter(AudioChunk& chunk) {
        if (!chunk.data() || chunk.size() == 0) {
            return;
        }
        
        // Calculate current frame energy
        float frame_energy = calculate_energy(chunk);
        
        // Update noise floor estimate during silence periods
        update_noise_floor(frame_energy);
        
        // Apply noise gate if energy is below threshold
        if (frame_energy < noise_floor_ * 1.5f) {
            // Apply soft noise gate (reduce amplitude rather than silence completely)
            float reduction_factor = std::min(1.0f, frame_energy / (noise_floor_ * 1.5f));
            reduction_factor = std::pow(reduction_factor, 2.0f); // Squared for more aggressive reduction
            
            // Apply reduction
            for (size_t i = 0; i < chunk.size(); i++) {
                chunk.data()[i] *= reduction_factor;
            }
        }
        
        // Apply spectral subtraction (simplified)
        if (calibrated_) {
            // Subtract estimated noise floor from each sample
            for (size_t i = 0; i < chunk.size(); i++) {
                float sample = chunk.data()[i];
                float sign = sample >= 0 ? 1.0f : -1.0f;
                float abs_sample = std::abs(sample);
                
                // Subtract noise floor (with flooring to avoid negative values)
                float filtered = sign * std::max(0.0f, abs_sample - noise_floor_ * 0.5f);
                
                // Apply soft-decision filter
                float gain = abs_sample < noise_floor_ ? 0.1f : 1.0f;
                chunk.data()[i] = filtered * gain;
            }
        }
    }
    
    // Calibrate the noise filter with background noise
    void calibrate(const AudioChunk& chunk) {
        if (!chunk.data() || chunk.size() == 0) {
            return;
        }
        
        // Calculate energy of the calibration frame
        float frame_energy = calculate_energy(chunk);
        
        // Initialize noise floor with this energy
        noise_floor_ = frame_energy;
        noise_energy_history_.clear();
        calibrated_ = true;
    }
    
    // Auto-calibrate the noise filter with running energy estimates
    void auto_calibrate(const AudioChunk& chunk, bool is_speech) {
        if (!chunk.data() || chunk.size() == 0) {
            return;
        }
        
        // If this is silence (not speech), use it to calibrate
        if (!is_speech) {
            float frame_energy = calculate_energy(chunk);
            
            // Add to history
            noise_energy_history_.push_back(frame_energy);
            if (noise_energy_history_.size() > window_size_) {
                noise_energy_history_.pop_front();
            }
            
            // Update noise floor with average of recent silence frames
            if (noise_energy_history_.size() >= 3) {
                float avg_energy = 0.0f;
                for (float e : noise_energy_history_) {
                    avg_energy += e;
                }
                avg_energy /= noise_energy_history_.size();
                
                // Smooth transition for noise floor updates
                if (!calibrated_) {
                    noise_floor_ = avg_energy;
                    calibrated_ = true;
                } else {
                    noise_floor_ = 0.9f * noise_floor_ + 0.1f * avg_energy;
                }
            }
        }
    }
    
    // Check if the filter is calibrated
    bool is_calibrated() const {
        return calibrated_;
    }
    
    // Get the current noise floor
    float get_noise_floor() const {
        return noise_floor_;
    }
    
    // Set the noise threshold directly
    void set_noise_threshold(float threshold) {
        noise_threshold_ = threshold;
    }
    
private:
    // Calculate energy of a frame
    float calculate_energy(const AudioChunk& chunk) const {
        float energy = 0.0f;
        for (size_t i = 0; i < chunk.size(); i++) {
            energy += chunk.data()[i] * chunk.data()[i];
        }
        return energy / chunk.size();
    }
    
    // Update noise floor estimate
    void update_noise_floor(float frame_energy) {
        // If energy is very low, it's likely silence - use it to update noise floor
        if (!calibrated_ || frame_energy < noise_floor_ * 1.2f) {
            if (!calibrated_) {
                noise_floor_ = frame_energy;
                calibrated_ = true;
            } else {
                // Slowly adapt noise floor (90% old, 10% new)
                noise_floor_ = 0.95f * noise_floor_ + 0.05f * frame_energy;
            }
        }
    }
    
    float noise_threshold_;
    float noise_floor_;
    bool calibrated_;
    int window_size_;
    std::deque<float> noise_energy_history_;
};

// Improved constructor with background loading
VoskTranscriber::VoskTranscriber(const std::string& model_path, float sample_rate)
    : model_(nullptr),
      recognizer_(nullptr),
      sample_rate_(sample_rate),
      has_speech_started_(false),
      is_loading_(true),
      loading_progress_(0.0f),
      model_path_(model_path),
      use_noise_filtering_(false) {
    
    // Start loading the model in a background thread
    loading_future_ = std::async(std::launch::async, 
                                 &VoskTranscriber::load_model_background, 
                                 this);
}

// Background model loading method
bool VoskTranscriber::load_model_background() {
    try {
        // Report initial progress
        loading_progress_ = 0.1f;
        
        // Load model (this is the time-consuming operation)
        loading_progress_ = 0.2f;
        model_ = vosk_model_new(model_path_.c_str());
        
        if (!model_) {
            last_error_ = "Failed to load model from path: " + model_path_;
            is_loading_ = false;
            loading_progress_ = 0.0f;
            return false;
        }
        
        // Report progress
        loading_progress_ = 0.7f;
        
        // Create recognizer
        recognizer_ = vosk_recognizer_new(model_, sample_rate_);
        
        if (!recognizer_) {
            last_error_ = "Failed to create recognizer";
            if (model_) {
                vosk_model_free(model_);
                model_ = nullptr;
            }
            is_loading_ = false;
            loading_progress_ = 0.0f;
            return false;
        }
        
        // Set recognizer options
        loading_progress_ = 0.9f;
        vosk_recognizer_set_max_alternatives(recognizer_, 1);
        vosk_recognizer_set_words(recognizer_, 1);
        
        // Complete loading
        loading_progress_ = 1.0f;
        is_loading_ = false;
        return true;
    } 
    catch (const std::exception& e) {
        last_error_ = "Exception during model loading: " + std::string(e.what());
        if (model_) {
            vosk_model_free(model_);
            model_ = nullptr;
        }
        is_loading_ = false;
        loading_progress_ = 0.0f;
        return false;
    }
    catch (...) {
        last_error_ = "Unknown exception during model loading";
        if (model_) {
            vosk_model_free(model_);
            model_ = nullptr;
        }
        is_loading_ = false;
        loading_progress_ = 0.0f;
        return false;
    }
}

// Helper method to create empty transcription results while model is loading
TranscriptionResult VoskTranscriber::create_empty_result() const {
    TranscriptionResult result;
    result.raw_text = "";
    result.processed_text = "";
    result.is_final = false;
    result.confidence = 0.0;
    result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return result;
}

// Enhanced destructor with proper future handling
VoskTranscriber::~VoskTranscriber() {
    // Wait for background loading to complete before destroying
    if (is_loading_.load() && loading_future_.valid()) {
        try {
            // Set a reasonable timeout (5 seconds)
            auto status = loading_future_.wait_for(std::chrono::seconds(5));
            if (status == std::future_status::timeout) {
                // Loading is taking too long, we'll need to force quit
                // This could lead to resource leaks, but we have to prevent hanging
                return;
            }
            
            // Get the result to prevent any exceptions from being thrown later
            loading_future_.get();
        } catch (...) {
            // Ignore any exceptions during cleanup
        }
    }
    
    // Clean up resources
    if (recognizer_) {
        vosk_recognizer_free(recognizer_);
        recognizer_ = nullptr;
    }
    
    if (model_) {
        vosk_model_free(model_);
        model_ = nullptr;
    }
}

// Move constructor and assignment operators
VoskTranscriber::VoskTranscriber(VoskTranscriber&& other) noexcept
    : model_(other.model_),
      recognizer_(other.recognizer_),
      sample_rate_(other.sample_rate_),
      has_speech_started_(other.has_speech_started_),
      noise_filter_(std::move(other.noise_filter_)),
      use_noise_filtering_(other.use_noise_filtering_),
      is_loading_(other.is_loading_.load()),
      loading_progress_(other.loading_progress_.load()),
      loading_future_(std::move(other.loading_future_)),
      model_path_(std::move(other.model_path_)),
      last_error_(std::move(other.last_error_)) {
    
    other.model_ = nullptr;
    other.recognizer_ = nullptr;
}

VoskTranscriber& VoskTranscriber::operator=(VoskTranscriber&& other) noexcept {
    if (this != &other) {
        // Clean up existing resources
        if (recognizer_) {
            vosk_recognizer_free(recognizer_);
        }
        if (model_) {
            vosk_model_free(model_);
        }
        
        // Move resources from other
        model_ = other.model_;
        recognizer_ = other.recognizer_;
        sample_rate_ = other.sample_rate_;
        has_speech_started_ = other.has_speech_started_;
        noise_filter_ = std::move(other.noise_filter_);
        use_noise_filtering_ = other.use_noise_filtering_;
        is_loading_ = other.is_loading_.load();
        loading_progress_ = other.loading_progress_.load();
        loading_future_ = std::move(other.loading_future_);
        model_path_ = std::move(other.model_path_);
        last_error_ = std::move(other.last_error_);
        
        // Clear other's resources
        other.model_ = nullptr;
        other.recognizer_ = nullptr;
    }
    return *this;
}

// Noise filtering methods
void VoskTranscriber::enable_noise_filtering(bool enable) {
    use_noise_filtering_ = enable;
}

bool VoskTranscriber::is_noise_filtering_enabled() const {
    return use_noise_filtering_;
}

void VoskTranscriber::calibrate_noise_filter(const AudioChunk& silence_chunk) {
    if (!noise_filter_) {
        noise_filter_ = std::make_unique<NoiseFilter>(0.05f, 10);
    }
    noise_filter_->calibrate(silence_chunk);
}

TranscriptionResult VoskTranscriber::transcribe_with_noise_filtering(
    std::unique_ptr<AudioChunk> chunk, bool is_speech) {
    
    // Initialize noise filter if needed
    if (!noise_filter_ && use_noise_filtering_) {
        noise_filter_ = std::make_unique<NoiseFilter>(0.05f, 10);
    }
    
    // Make a copy of the chunk for noise filtering
    auto filtered_chunk = std::make_unique<AudioChunk>(chunk->size());
    std::memcpy(filtered_chunk->data(), chunk->data(), chunk->size() * sizeof(float));
    
    // Apply noise filtering if enabled
    if (noise_filter_ && use_noise_filtering_) {
        if (!is_speech) {
            // Use silence to auto-calibrate the filter
            noise_filter_->auto_calibrate(*filtered_chunk, false);
        }
        
        // Apply the filter to the chunk
        noise_filter_->filter(*filtered_chunk);
    }
    
    // Use the filtered chunk for transcription
    return transcribe_with_vad(std::move(filtered_chunk), is_speech);
}

// New method to check loading state and get progress
float VoskTranscriber::get_loading_progress() const {
    return loading_progress_.load();
}

bool VoskTranscriber::is_loading() const {
    return is_loading_.load();
}

// Modified transcribe method that works with background loading
TranscriptionResult VoskTranscriber::transcribe(std::unique_ptr<AudioChunk> chunk) {
    // Check if we're still loading
    if (is_loading_.load()) {
        // Check if loading is complete now
        if (loading_future_.valid() && 
            loading_future_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            
            // Loading completed, get the result
            bool success = loading_future_.get();
            if (!success) {
                // Loading failed, return empty result with error
                TranscriptionResult result = create_empty_result();
                result.raw_text = "Model loading failed: " + last_error_;
                result.processed_text = result.raw_text;
                return result;
            }
        } else {
            // Still loading, return empty result with progress
            TranscriptionResult result = create_empty_result();
            float progress = loading_progress_.load();
            result.raw_text = "Loading model... " + std::to_string(int(progress * 100)) + "%";
            result.processed_text = result.raw_text;
            return result;
        }
    }
    
    // Regular transcription - only run if model is loaded
    if (!recognizer_ || !chunk || chunk->size() == 0) {
        TranscriptionResult result = create_empty_result();
        result.raw_text = "";
        result.processed_text = "";
        return result;
    }
    
    try {
        // Proceed with normal transcription
        std::lock_guard<std::mutex> lock(recognizer_mutex_);
        
        // Convert float samples to int16 for Vosk
        std::vector<int16_t> pcm(chunk->size());
        for (size_t i = 0; i < chunk->size(); i++) {
            pcm[i] = static_cast<int16_t>(chunk->data()[i] * 32767.0f);
        }
        
        // Process audio data
        std::string json_result;
        bool is_final;
        
        // Process waveform through Vosk
        const char* data_ptr = reinterpret_cast<const char*>(pcm.data());
        int data_length = static_cast<int>(pcm.size() * sizeof(int16_t));
        
        if (vosk_recognizer_accept_waveform(recognizer_, data_ptr, data_length)) {
            // End of utterance, get final result
            json_result = vosk_recognizer_result(recognizer_);
            is_final = true;
        } else {
            // Utterance continues, get partial result
            json_result = vosk_recognizer_partial_result(recognizer_);
            is_final = false;
        }
        
        // Parse result
        TranscriptionResult result = parse_result(json_result);
        result.is_final = is_final;
        return result;
    } 
    catch (const std::exception& e) {
        last_error_ = "Exception during transcription: " + std::string(e.what());
        TranscriptionResult result = create_empty_result();
        result.raw_text = "Error: " + last_error_;
        result.processed_text = result.raw_text;
        return result;
    } 
    catch (...) {
        last_error_ = "Unknown exception during transcription";
        TranscriptionResult result = create_empty_result();
        result.raw_text = "Error: " + last_error_;
        result.processed_text = result.raw_text;
        return result;
    }
}

// Process a chunk with VAD checking
TranscriptionResult VoskTranscriber::transcribe_with_vad(std::unique_ptr<AudioChunk> chunk, bool is_speech) {
    if (is_speech) {
        if (!has_speech_started_) {
            // Speech just started, reset the recognizer to start a new utterance
            if (recognizer_) {
                vosk_recognizer_reset(recognizer_);
            }
            has_speech_started_ = true;
        }
        
        // Process the chunk with speech
        return transcribe(std::move(chunk));
    } else {
        if (has_speech_started_) {
            // Speech just ended, get final result
            has_speech_started_ = false;
            
            // Create a dummy result since there's no actual audio to process
            TranscriptionResult result = create_empty_result();
            
            if (recognizer_) {
                // Get final result from recognizer
                std::string json_result = vosk_recognizer_final_result(recognizer_);
                result = parse_result(json_result);
                result.is_final = true;
            }
            
            return result;
        }
        
        // No speech and no active utterance, return empty result
        return create_empty_result();
    }
}

// Reset the recognizer
void VoskTranscriber::reset() {
    if (recognizer_) {
        std::lock_guard<std::mutex> lock(recognizer_mutex_);
        vosk_recognizer_reset(recognizer_);
    }
    has_speech_started_ = false;
}

// Modified is_model_loaded to work with background loading
bool VoskTranscriber::is_model_loaded() const {
    // If we're still loading, check if it's done now
    if (is_loading_.load() && loading_future_.valid()) {
        // Check if loading is complete without waiting
        auto status = loading_future_.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::ready) {
            // It's done, but we don't change any state here - that happens in transcribe()
            // We just report the current state
            return model_ != nullptr && recognizer_ != nullptr;
        } else {
            // Still loading
            return false;
        }
    }
    
    // Standard check
    return model_ != nullptr && recognizer_ != nullptr;
}

// Improved parse_result method with better error handling
TranscriptionResult VoskTranscriber::parse_result(const std::string& json_result) {
    TranscriptionResult result = create_empty_result();
    
    try {
        // Parse the JSON using RapidJSON
        rapidjson::Document doc;
        rapidjson::ParseResult parseResult = doc.Parse(json_result.c_str());
        
        // Check if parsing failed
        if (parseResult.IsError()) {
            last_error_ = "JSON parse error: " + std::string(rapidjson::GetParseError_En(parseResult.Code()));
            return result;
        }
        
        // Check for "text" field (for final results)
        if (doc.HasMember("text") && doc["text"].IsString()) {
            result.raw_text = doc["text"].GetString();
            result.processed_text = result.raw_text;
            result.is_final = true;
            
            // Check for "result" field with word details
            if (doc.HasMember("result") && doc["result"].IsArray()) {
                const rapidjson::Value& words = doc["result"];
                double totalConf = 0.0;
                int wordCount = 0;
                
                // Iterate through words to calculate average confidence
                for (rapidjson::SizeType i = 0; i < words.Size(); i++) {
                    if (words[i].HasMember("conf") && words[i]["conf"].IsNumber()) {
                        totalConf += words[i]["conf"].GetDouble();
                        wordCount++;
                    }
                }
                
                // Calculate average confidence
                if (wordCount > 0) {
                    result.confidence = totalConf / wordCount;
                } else {
                    result.confidence = 1.0; // Default if no words with confidence
                }
            } else {
                result.confidence = 1.0; // Default if no detailed results
            }
        } 
        // Check for "partial" field (for partial results)
        else if (doc.HasMember("partial") && doc["partial"].IsString()) {
            result.raw_text = doc["partial"].GetString();
            result.processed_text = result.raw_text;
            result.is_final = false;
            result.confidence = 0.5; // Default confidence for partial results
        }
        
    } catch (const std::exception& e) {
        last_error_ = "Exception during result parsing: " + std::string(e.what());
    } catch (...) {
        last_error_ = "Unknown exception during result parsing";
    }
    
    return result;
}

// Extract text from JSON result
std::string VoskTranscriber::extract_text_from_json(const std::string& json) {
    try {
        rapidjson::Document doc;
        doc.Parse(json.c_str());
        
        if (doc.HasMember("text") && doc["text"].IsString()) {
            return doc["text"].GetString();
        } else if (doc.HasMember("partial") && doc["partial"].IsString()) {
            return doc["partial"].GetString();
        }
    } catch (...) {
        // Ignore parsing errors and return empty string
    }
    
    return "";
}

} // namespace voice_transcription