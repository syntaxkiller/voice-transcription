//  This snippet will fix the error in vosk_transcription_engine.cpp
//  The error is related to type casting with vosk_recognizer_accept_waveform function

// Original line with error:
// vosk_recognizer_accept_waveform(recognizer_, pcm.data(), pcm.size() * sizeof(int16_t))

// Corrected implementation:
// First, convert the data to a char array
char* data_ptr = reinterpret_cast<char*>(pcm.data());
int data_length = pcm.size() * sizeof(int16_t);
 
// Then call the function with the correct types
if (vosk_recognizer_accept_waveform(recognizer_, data_ptr, data_length)) {
    // End of utterance, get final result
    json_result = vosk_recognizer_result(recognizer_);
    is_final = true;
} else {
    // Utterance continues, get partial result
    json_result = vosk_recognizer_partial_result(recognizer_);
    is_final = false;
}