#include <gtest/gtest.h>
#include "audio_stream.h"

using namespace voice_transcription;

// Test that the audio device enumeration works
TEST(AudioStreamTest, DeviceEnumeration) {
    auto devices = ControlledAudioStream::enumerate_devices();
    // Even if no devices are present, this should return an empty list, not crash
    ASSERT_NO_THROW(devices = ControlledAudioStream::enumerate_devices());
}

// Test audio chunk creation and data access
TEST(AudioStreamTest, AudioChunkBasics) {
    const size_t chunk_size = 1024;
    
    // Test default constructor
    AudioChunk chunk(chunk_size);
    EXPECT_EQ(chunk.size(), chunk_size);
    EXPECT_NE(chunk.data(), nullptr);
    
    // Test data initialization
    for (size_t i = 0; i < chunk_size; i++) {
        EXPECT_FLOAT_EQ(chunk.data()[i], 0.0f);
    }
    
    // Test data modification
    float* data = chunk.data();
    for (size_t i = 0; i < chunk_size; i++) {
        data[i] = static_cast<float>(i) / chunk_size;
    }
    
    for (size_t i = 0; i < chunk_size; i++) {
        EXPECT_FLOAT_EQ(chunk.data()[i], static_cast<float>(i) / chunk_size);
    }
}

// Test move semantics of AudioChunk
TEST(AudioStreamTest, AudioChunkMove) {
    const size_t chunk_size = 1024;
    
    // Create a source chunk with some data
    AudioChunk source(chunk_size);
    float* source_data = source.data();
    for (size_t i = 0; i < chunk_size; i++) {
        source_data[i] = static_cast<float>(i) / chunk_size;
    }
    
    // Move construct to a new chunk
    AudioChunk moved(std::move(source));
    
    // Source should now be empty (size 0)
    EXPECT_EQ(source.size(), 0);
    
    // Moved chunk should have the data
    EXPECT_EQ(moved.size(), chunk_size);
    for (size_t i = 0; i < chunk_size; i++) {
        EXPECT_FLOAT_EQ(moved.data()[i], static_cast<float>(i) / chunk_size);
    }
}