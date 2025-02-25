#pragma once

#include <string>
#include <functional>
#include <windows.h>

namespace voice_transcription {

// Callback function type for device change notifications
using DeviceChangeCallback = std::function<void()>;

// Window manager class
class WindowManager {
public:
    WindowManager();
    ~WindowManager();

    // No copy operations
    WindowManager(const WindowManager&) = delete;
    WindowManager& operator=(const WindowManager&) = delete;
    
    // Window creation and message loop
    bool create_hidden_window();
    void destroy_hidden_window();
    void message_loop();
    
    // Foreground window tracking
    static std::string get_foreground_window_title();
    static HWND get_foreground_window_handle();
    
    // Set callback for device change notifications
    void set_device_change_callback(DeviceChangeCallback callback);
    
    // Helper method for window procedure
    LRESULT window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

private:
    // Window handle and procedure
    HWND hidden_window_;
    static LRESULT CALLBACK window_proc_static(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    
    // Device change notification callback
    DeviceChangeCallback device_change_callback_;
    
    // Window class name
    static constexpr const char* WINDOW_CLASS_NAME = "VoiceTranscriptionHiddenWindow";
    
    // Flag to indicate if the message loop should continue
    bool run_message_loop_;
};

// Shortcut capture class
class ShortcutCapture {
public:
    ShortcutCapture();
    ~ShortcutCapture();
    
    // Start and stop capture
    void start_capture(int timeout_seconds = 3);
    void stop_capture();
    
    // Set capture callback
    using CaptureCallback = std::function<void(WORD modifiers, WORD key)>;
    void set_capture_callback(CaptureCallback callback);
    
    // Low-level keyboard hook procedure
    static LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM wparam, LPARAM lparam);

private:
    // Keyboard hook handle
    HHOOK keyboard_hook_;
    
    // Capture callback
    static CaptureCallback capture_callback_;
    
    // Timer for capture timeout
    UINT_PTR timer_id_;
    
    // Static instance for hook callbacks
    static ShortcutCapture* instance_;
};

} // namespace voice_transcription