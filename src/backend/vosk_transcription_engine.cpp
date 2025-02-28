#include "vosk_transcription_engine.h"
#include <chrono>
#include <atomic>
#include <future>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

namespace voice_transcription {

// Add these new members to the VoskTranscriber class in vosk_transcription_engine.h
/*
private:
    // Background loading members
    std::atomic<bool> is_loading_;
    std::atomic<float> loading_progress_;
    std::future<bool> loading_future_;
    std::string model_path_;
    
    // Background loading methods
    bool load_model_background();
    TranscriptionResult create_empty_result() const;
*/

// Improved constructor with background loading
VoskTranscriber::VoskTranscriber(const std::string& model_path, float sample_rate)
    : model_(nullptr),
      recognizer_(nullptr),
      sample_rate_(sample_rate),
      has_speech_started_(false),
      is_loading_(true),
      loading_progress_(0.0f),
      model_path_(model_path) {
    
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
        // Proceed with normal transcription as in the original code
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

} // namespace voice_transcription