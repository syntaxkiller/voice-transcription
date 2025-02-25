#include "window_manager.h"
#include <dbt.h>  // For device change notifications
#include <iostream>

namespace voice_transcription {

// Static initialization for ShortcutCapture
ShortcutCapture* ShortcutCapture::instance_ = nullptr;
ShortcutCapture::CaptureCallback ShortcutCapture::capture_callback_ = nullptr;

// WindowManager implementation
WindowManager::WindowManager()
    : hidden_window_(NULL),
      run_message_loop_(false) {
}

WindowManager::~WindowManager() {
    if (hidden_window_) {
        destroy_hidden_window();
    }
}

bool WindowManager::create_hidden_window() {
    // Register window class
    WNDCLASSA wc = { 0 };
    wc.lpfnWndProc = window_proc_static;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassA(&wc)) {
        // If registration fails, check if the class is already registered
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    // Create hidden window
    hidden_window_ = CreateWindowA(
        WINDOW_CLASS_NAME,          // Class name
        "Voice Transcription",      // Window title
        WS_OVERLAPPED,              // Style
        CW_USEDEFAULT, CW_USEDEFAULT, // Position
        0, 0,                        // Size
        NULL,                        // Parent window
        NULL,                        // Menu
        GetModuleHandle(NULL),       // Instance
        this                         // Additional data
    );

    if (!hidden_window_) {
        return false;
    }

    // Register for device change notifications
    DEV_BROADCAST_DEVICEINTERFACE filter = { 0 };
    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;

    HDEVNOTIFY device_notify = RegisterDeviceNotificationA(
        hidden_window_,
        &filter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );

    return true;
}

void WindowManager::destroy_hidden_window() {
    if (hidden_window_) {
        DestroyWindow(hidden_window_);
        hidden_window_ = NULL;
    }

    // Unregister window class
    UnregisterClassA(WINDOW_CLASS_NAME, GetModuleHandle(NULL));
}

void WindowManager::message_loop() {
    if (!hidden_window_) {
        return;
    }

    run_message_loop_ = true;
    MSG msg;

    while (run_message_loop_ && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

std::string WindowManager::get_foreground_window_title() {
    char title[256] = { 0 };
    HWND foreground_window = GetForegroundWindow();

    if (!foreground_window) {
        return "";
    }

    GetWindowTextA(foreground_window, title, sizeof(title));
    return std::string(title);
}

HWND WindowManager::get_foreground_window_handle() {
    return GetForegroundWindow();
}

void WindowManager::set_device_change_callback(DeviceChangeCallback callback) {
    device_change_callback_ = callback;
}

LRESULT WindowManager::window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_DESTROY:
            run_message_loop_ = false;
            PostQuitMessage(0);
            return 0;

        case WM_DEVICECHANGE:
            if (wparam == DBT_DEVICEARRIVAL || wparam == DBT_DEVICEREMOVECOMPLETE) {
                if (device_change_callback_) {
                    device_change_callback_();
                }
            }
            return TRUE;

        case WM_HOTKEY:
            // Process registered hotkeys
            // Each hotkey has a unique ID that we assigned when registering
            // We would normally dispatch this to a callback or signal
            if (device_change_callback_) {
                // Use the same callback for hotkey notifications
                // In a real implementation, we would have separate callbacks
                device_change_callback_();
            }
            return 0;

        default:
            return DefWindowProc(hwnd, message, wparam, lparam);
    }
}

LRESULT CALLBACK WindowManager::window_proc_static(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    // Get the window manager instance
    WindowManager* manager = nullptr;

    if (message == WM_CREATE) {
        // Store the instance pointer
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
        manager = reinterpret_cast<WindowManager*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(manager));
    } else {
        // Retrieve the instance pointer
        manager = reinterpret_cast<WindowManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (manager) {
        return manager->window_proc(hwnd, message, wparam, lparam);
    } else {
        return DefWindowProc(hwnd, message, wparam, lparam);
    }
}

// ShortcutCapture implementation
ShortcutCapture::ShortcutCapture()
    : keyboard_hook_(NULL),
      timer_id_(0) {
    // Store the instance for the hook callback
    instance_ = this;
}

ShortcutCapture::~ShortcutCapture() {
    // Remove the hook
    stop_capture();

    // Clear the static instance
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

void ShortcutCapture::start_capture(int timeout_seconds) {
    // Set the keyboard hook
    keyboard_hook_ = SetWindowsHookEx(
        WH_KEYBOARD_LL,
        keyboard_hook_proc,
        GetModuleHandle(NULL),
        0  // System-wide hook
    );

    // Set a timer for capture timeout
    if (timeout_seconds > 0) {
        timer_id_ = SetTimer(NULL, 0, timeout_seconds * 1000, NULL);
    }
}

void ShortcutCapture::stop_capture() {
    // Remove the keyboard hook
    if (keyboard_hook_) {
        UnhookWindowsHookEx(keyboard_hook_);
        keyboard_hook_ = NULL;
    }

    // Kill the timer
    if (timer_id_) {
        KillTimer(NULL, timer_id_);
        timer_id_ = 0;
    }
}

void ShortcutCapture::set_capture_callback(CaptureCallback callback) {
    capture_callback_ = callback;
}

LRESULT CALLBACK ShortcutCapture::keyboard_hook_proc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION) {
        // Process the keyboard message
        KBDLLHOOKSTRUCT* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);

        // Only process key down events
        if (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) {
            // Get key code and modifiers
            WORD key_code = static_cast<WORD>(kb->vkCode);
            WORD modifiers = 0;

            // Check for modifier keys
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
                modifiers |= MOD_CONTROL;
            }
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                modifiers |= MOD_SHIFT;
            }
            if (GetAsyncKeyState(VK_MENU) & 0x8000) {  // Alt key
                modifiers |= MOD_ALT;
            }
            if (GetAsyncKeyState(VK_LWIN) & 0x8000 || GetAsyncKeyState(VK_RWIN) & 0x8000) {
                modifiers |= MOD_WIN;
            }

            // Skip if the key is a modifier key itself
            if (key_code != VK_CONTROL && key_code != VK_SHIFT && 
                key_code != VK_MENU && key_code != VK_LWIN && key_code != VK_RWIN) {
                // Fire the callback if set
                if (instance_ && capture_callback_) {
                    capture_callback_(modifiers, key_code);

                    // Stop the capture
                    instance_->stop_capture();

                    // Consume the key event
                    return 1;
                }
            }
        }
    }

    // Call the next hook in the chain
    return CallNextHookEx(NULL, code, wparam, lparam);
}

} // namespace voice_transcription