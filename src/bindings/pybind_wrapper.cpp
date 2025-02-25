#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>

#include "audio_stream.h"
#include "vosk_transcription_engine.h"
#include "keyboard_sim.h"
#include "window_manager.h"

namespace py = pybind11;
using namespace voice_transcription;

PYBIND11_MODULE(voice_transcription_backend, m) {
    m.doc() = "Voice Transcription Backend Module";
    
    // Exception binding
    py::register_exception<AudioStreamException>(m, "AudioStreamError");
    py::register_exception<KeypressSimulationException>(m, "KeypressSimulationError");
    
    // AudioDevice class
    py::class_<AudioDevice>(m, "AudioDevice")
        .def(py::init<>())
        .def_readwrite("id", &AudioDevice::id)
        .def_readwrite("raw_name", &AudioDevice::raw_name)
        .def_readwrite("label", &AudioDevice::label)
        .def_readwrite("is_default", &AudioDevice::is_default)
        .def_readwrite("supported_sample_rates", &AudioDevice::supported_sample_rates);
    
    // AudioChunk class
    py::class_<AudioChunk>(m, "AudioChunk")
        .def(py::init<size_t>())
        .def("size", &AudioChunk::size)
        .def("data", [](const AudioChunk& chunk) {
            return py::array_t<float>(
                {chunk.size()},
                {sizeof(float)},
                chunk.data(),
                py::capsule(chunk.data(), [](void*) {})
            );
        });
    
    // ControlledAudioStream class
    py::class_<ControlledAudioStream>(m, "ControlledAudioStream")
        .def(py::init<int, int, int>())
        .def("start", &ControlledAudioStream::start)
        .def("stop", &ControlledAudioStream::stop)
        .def("pause", &ControlledAudioStream::pause)
        .def("resume", &ControlledAudioStream::resume)
        .def("is_active", &ControlledAudioStream::is_active)
        .def("get_device_id", &ControlledAudioStream::get_device_id)
        .def("get_sample_rate", &ControlledAudioStream::get_sample_rate)
        .def("get_frames_per_buffer", &ControlledAudioStream::get_frames_per_buffer)
        .def("get_last_error", &ControlledAudioStream::get_last_error)
        .def("get_next_chunk", &ControlledAudioStream::get_next_chunk)
        .def_static("enumerate_devices", &ControlledAudioStream::enumerate_devices)
        .def_static("check_device_compatibility", &ControlledAudioStream::check_device_compatibility);
    
    // TranscriptionResult class
    py::class_<TranscriptionResult>(m, "TranscriptionResult")
        .def(py::init<>())
        .def_readwrite("raw_text", &TranscriptionResult::raw_text)
        .def_readwrite("processed_text", &TranscriptionResult::processed_text)
        .def_readwrite("is_final", &TranscriptionResult::is_final)
        .def_readwrite("confidence", &TranscriptionResult::confidence)
        .def_readwrite("timestamp_ms", &TranscriptionResult::timestamp_ms);
    
    // VADHandler class
    py::class_<VADHandler>(m, "VADHandler")
        .def(py::init<int, int, int>())
        .def("is_speech", &VADHandler::is_speech)
        .def("set_aggressiveness", &VADHandler::set_aggressiveness)
        .def("get_aggressiveness", &VADHandler::get_aggressiveness);
    
    // VoskTranscriber class
    py::class_<VoskTranscriber>(m, "VoskTranscriber")
        .def(py::init<const std::string&, float>())
        .def("transcribe", &VoskTranscriber::transcribe)
        .def("transcribe_with_vad", &VoskTranscriber::transcribe_with_vad)
        .def("reset", &VoskTranscriber::reset)
        .def("is_model_loaded", &VoskTranscriber::is_model_loaded)
        .def("get_last_error", &VoskTranscriber::get_last_error);
    
    // Shortcut class
    py::class_<Shortcut>(m, "Shortcut")
        .def(py::init<>())
        .def_readwrite("modifiers", &Shortcut::modifiers)
        .def_readwrite("key", &Shortcut::key)
        .def_readwrite("is_valid", &Shortcut::is_valid);
    
    // KeyboardSimulator class
    py::class_<KeyboardSimulator>(m, "KeyboardSimulator")
        .def(py::init<>())
        .def("simulate_keypresses", &KeyboardSimulator::simulate_keypresses)
        .def("simulate_special_key", &KeyboardSimulator::simulate_special_key)
        .def_static("register_global_hotkey", &KeyboardSimulator::register_global_hotkey)
        .def_static("unregister_global_hotkey", &KeyboardSimulator::unregister_global_hotkey);
    
    // ClipboardManager - static methods
    m.def("set_clipboard_text", &ClipboardManager::set_clipboard_text);
    m.def("get_clipboard_text", &ClipboardManager::get_clipboard_text);
    
    // WindowManager class
    py::class_<WindowManager>(m, "WindowManager")
        .def(py::init<>())
        .def("create_hidden_window", &WindowManager::create_hidden_window)
        .def("destroy_hidden_window", &WindowManager::destroy_hidden_window)
        .def("message_loop", &WindowManager::message_loop)
        .def("set_device_change_callback", &WindowManager::set_device_change_callback)
        .def_static("get_foreground_window_title", &WindowManager::get_foreground_window_title);
    
    // ShortcutCapture class
    py::class_<ShortcutCapture>(m, "ShortcutCapture")
        .def(py::init<>())
        .def("start_capture", &ShortcutCapture::start_capture)
        .def("stop_capture", &ShortcutCapture::stop_capture)
        .def("set_capture_callback", &ShortcutCapture::set_capture_callback);
}