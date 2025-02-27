#include "vosk_transcription_engine.h"
#include <chrono>
#include <cstring>
#include <vector>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

namespace voice_transcription {

// VADHandler implementation
VADHandler::VADHandler(int sample_rate, int frame_duration_ms, int aggressiveness)
    : sample_rate_(sample_rate), 
      frame_duration_ms_(frame_duration_ms),
      aggressiveness_(aggressiveness),
      vad_handle_(nullptr) {
    
    // Initialize WebRTC VAD
    vad_handle_ = WebRtcVad_Create();
    if (vad_handle_) {
        WebRtcVad_Init(vad_handle_);
        WebRtcVad_set_mode(vad_handle_, aggressiveness_);
    }
    
    // Allocate temporary buffer
    size_t frame_size = (sample_rate_ * frame_duration_ms_ / 1000);
    temp_buffer_.resize(frame_size);
}

VADHandler::~VADHandler() {
    if (vad_handle_) {
        WebRtcVad_Free(vad_handle_);
        vad_handle_ = nullptr;
    }
}

bool VADHandler::is_speech(const AudioChunk& chunk) {
    if (!vad_handle_ || chunk.size() == 0) {
        return false;
    }
    
    // Convert float samples to int16
    for (size_t i = 0; i < chunk.size() && i < temp_buffer_.size(); i++) {
        temp_buffer_[i] = static_cast<int16_t>(chunk.data()[i] * 32767.0f);
    }
    
    // Process with WebRTC VAD
    int result = WebRtcVad_Process(
        vad_handle_,
        sample_rate_,
        temp_buffer_.data(),
        temp_buffer_.size()
    );
    
    return result > 0;
}

void VADHandler::set_aggressiveness(int aggressiveness) {
    if (aggressiveness < 0 || aggressiveness > 3) {
        return;
    }
    
    aggressiveness_ = aggressiveness;
    if (vad_handle_) {
        WebRtcVad_set_mode(vad_handle_, aggressiveness_);
    }
}

// VoskTranscriber implementation
VoskTranscriber::VoskTranscriber(const std::string& model_path, float sample_rate)
    : model_(nullptr),
      recognizer_(nullptr),
      sample_rate_(sample_rate),
      has_speech_started_(false) {
    
    try {
        // Load model
        model_ = vosk_model_new(model_path.c_str());
        
        if (model_) {
            // Create recognizer
            recognizer_ = vosk_recognizer_new(model_, sample_rate_);
            
            if (recognizer_) {
                // Set recognizer options
                vosk_recognizer_set_max_alternatives(recognizer_, 1);
                vosk_recognizer_set_words(recognizer_, 1);
            } else {
                last_error_ = "Failed to create recognizer";
            }
        } else {
            last_error_ = "Failed to load model from path: " + model_path;
        }
    } catch (const std::exception& e) {
        last_error_ = "Exception during initialization: " + std::string(e.what());
        if (model_) {
            vosk_model_free(model_);
            model_ = nullptr;
        }
    } catch (...) {
        last_error_ = "Unknown exception during initialization";
        if (model_) {
            vosk_model_free(model_);
            model_ = nullptr;
        }
    }
}

VoskTranscriber::~VoskTranscriber() {
    if (recognizer_) {
        vosk_recognizer_free(recognizer_);
        recognizer_ = nullptr;
    }
    
    if (model_) {
        vosk_model_free(model_);
        model_ = nullptr;
    }
}

VoskTranscriber::VoskTranscriber(VoskTranscriber&& other) noexcept
    : model_(other.model_),
      recognizer_(other.recognizer_),
      sample_rate_(other.sample_rate_),
      last_error_(std::move(other.last_error_)),
      has_speech_started_(other.has_speech_started_) {
    
    other.model_ = nullptr;
    other.recognizer_ = nullptr;
}

VoskTranscriber& VoskTranscriber::operator=(VoskTranscriber&& other) noexcept {
    if (this != &other) {
        // Clean up current resources
        if (recognizer_) {
            vosk_recognizer_free(recognizer_);
        }
        
        if (model_) {
            vosk_model_free(model_);
        }
        
        // Move resources
        model_ = other.model_;
        recognizer_ = other.recognizer_;
        sample_rate_ = other.sample_rate_;
        last_error_ = std::move(other.last_error_);
        has_speech_started_ = other.has_speech_started_;
        
        // Clear the moved object
        other.model_ = nullptr;
        other.recognizer_ = nullptr;
    }
    
    return *this;
}

TranscriptionResult VoskTranscriber::transcribe(std::unique_ptr<AudioChunk> chunk) {
    TranscriptionResult result;
    result.is_final = false;
    result.confidence = 0.0;
    result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    if (!recognizer_ || !chunk || chunk->size() == 0) {
        result.raw_text = "";
        result.processed_text = "";
        return result;
    }
    
    try {
        // Lock to prevent concurrent access to recognizer
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
        result = parse_result(json_result);
        result.is_final = is_final;
        
    } catch (const std::exception& e) {
        last_error_ = "Exception during transcription: " + std::string(e.what());
        result.raw_text = "";
        result.processed_text = "";
        result.is_final = false;
        result.confidence = 0.0;
    } catch (...) {
        last_error_ = "Unknown exception during transcription";
        result.raw_text = "";
        result.processed_text = "";
        result.is_final = false;
        result.confidence = 0.0;
    }
    
    return result;
}

TranscriptionResult VoskTranscriber::transcribe_with_vad(std::unique_ptr<AudioChunk> chunk, bool is_speech) {
    // Process based on VAD result
    if (is_speech) {
        has_speech_started_ = true;
    }
    
    if (has_speech_started_) {
        return transcribe(std::move(chunk));
    }
    
    // If no speech detected and hasn't started yet, return empty result
    TranscriptionResult empty_result;
    empty_result.raw_text = "";
    empty_result.processed_text = "";
    empty_result.is_final = false;
    empty_result.confidence = 0.0;
    empty_result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    return empty_result;
}

void VoskTranscriber::reset() {
    std::lock_guard<std::mutex> lock(recognizer_mutex_);
    
    if (recognizer_) {
        vosk_recognizer_reset(recognizer_);
    }
    
    has_speech_started_ = false;
}

TranscriptionResult VoskTranscriber::parse_result(const std::string& json_result) {
    TranscriptionResult result;
    result.raw_text = "";
    result.processed_text = "";
    result.is_final = false;
    result.confidence = 0.0;
    result.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
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

std::string VoskTranscriber::extract_text_from_json(const std::string& json) {
    try {
        // Parse the JSON using RapidJSON
        rapidjson::Document doc;
        doc.Parse(json.c_str());
        
        // Check for "text" field (for final results)
        if (doc.HasMember("text") && doc["text"].IsString()) {
            return doc["text"].GetString();
        } 
        // Check for "partial" field (for partial results)
        else if (doc.HasMember("partial") && doc["partial"].IsString()) {
            return doc["partial"].GetString();
        }
    } catch (...) {
        // Handle any exceptions
    }
    
    return "";
}

} // namespace voice_transcription