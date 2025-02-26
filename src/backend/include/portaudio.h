#ifndef PORTAUDIO_H
#define PORTAUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int PaError;
typedef int PaDeviceIndex;
typedef double PaTime;
typedef unsigned long PaStreamFlags;
typedef void PaStream;
typedef struct PaStreamCallbackTimeInfo PaStreamCallbackTimeInfo;
typedef unsigned long PaStreamCallbackFlags;
typedef int (PaStreamCallback)(const void *input, void *output, unsigned long frameCount, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData);

typedef struct PaDeviceInfo {
    const char *name;
    int maxInputChannels;
    int maxOutputChannels;
    double defaultLowInputLatency;
    double defaultHighInputLatency;
    double defaultLowOutputLatency;
    double defaultHighOutputLatency;
    double defaultSampleRate;
    int hostApi;
} PaDeviceInfo;

typedef struct PaHostApiInfo {
    const char *name;
} PaHostApiInfo;

typedef struct PaStreamParameters {
    int device;
    int channelCount;
    int sampleFormat;
    double suggestedLatency;
    void *hostApiSpecificStreamInfo;
} PaStreamParameters;

// Constants for PortAudio
#define paNoError 0
#define paNotInitialized -10000
#define paUnanticipatedHostError -9999
#define paInvalidChannelCount -9998
#define paInvalidSampleRate -9997
#define paInvalidDevice -9996
#define paInvalidFlag -9995
#define paStreamIsNotStopped -9994
#define paStreamIsStopped -9993
#define paBadStreamPtr -9992
#define paNoDevice -1
#define paFormatIsSupported 0

// Stream callback return codes
#define paContinue 0
#define paComplete 1
#define paAbort 2

// Sample formats
#define paFloat32 1
#define paInt32 2
#define paInt24 4
#define paInt16 8
#define paInt8 16
#define paUInt8 32
#define paCustomFormat 65536

// Stream flags
#define paClipOff 1
#define paDitherOff 2
#define paNeverDropInput 4
#define paPrimeOutputBuffersUsingStreamCallback 8
#define paPlatformSpecificFlags 0xFFFF0000

// Function declarations
PaError Pa_Initialize(void);
PaError Pa_Terminate(void);
int Pa_GetDeviceCount(void);
const PaDeviceInfo* Pa_GetDeviceInfo(PaDeviceIndex device);
const PaHostApiInfo* Pa_GetHostApiInfo(int hostApi);
PaDeviceIndex Pa_GetDefaultInputDevice(void);
PaDeviceIndex Pa_GetDefaultOutputDevice(void);
PaError Pa_IsFormatSupported(const PaStreamParameters *inputParameters, const PaStreamParameters *outputParameters, double sampleRate);
PaError Pa_OpenStream(PaStream **stream, const PaStreamParameters *inputParameters, const PaStreamParameters *outputParameters, double sampleRate, unsigned long framesPerBuffer, PaStreamFlags streamFlags, PaStreamCallback *streamCallback, void *userData);
PaError Pa_CloseStream(PaStream *stream);
PaError Pa_StartStream(PaStream *stream);
PaError Pa_StopStream(PaStream *stream);
PaError Pa_IsStreamActive(PaStream *stream);
PaError Pa_IsStreamStopped(PaStream *stream);
PaTime Pa_GetStreamTime(PaStream *stream);
long Pa_GetStreamReadAvailable(PaStream *stream);
long Pa_GetStreamWriteAvailable(PaStream *stream); 
const char *Pa_GetErrorText(PaError errorCode);

#ifdef __cplusplus
}
#endif

#endif /* PORTAUDIO_H */