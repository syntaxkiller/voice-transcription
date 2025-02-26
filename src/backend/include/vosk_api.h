#ifndef VOSK_API_H
#define VOSK_API_H

#ifdef __cplusplus
extern "C" {
#endif

// Opaque model and recognizer structures
typedef struct VoskModel VoskModel;
typedef struct VoskRecognizer VoskRecognizer;

// Model functions
VoskModel *vosk_model_new(const char *model_path);
void vosk_model_free(VoskModel *model);

// Recognizer functions
VoskRecognizer *vosk_recognizer_new(VoskModel *model, float sample_rate);
void vosk_recognizer_free(VoskRecognizer *recognizer);
void vosk_recognizer_set_max_alternatives(VoskRecognizer *recognizer, int max_alternatives);
void vosk_recognizer_set_words(VoskRecognizer *recognizer, int words);
int vosk_recognizer_accept_waveform(VoskRecognizer *recognizer, const char *data, int length);
const char *vosk_recognizer_result(VoskRecognizer *recognizer);
const char *vosk_recognizer_partial_result(VoskRecognizer *recognizer);
const char *vosk_recognizer_final_result(VoskRecognizer *recognizer);
void vosk_recognizer_reset(VoskRecognizer *recognizer);

#ifdef __cplusplus
}
#endif

#endif // VOSK_API_H