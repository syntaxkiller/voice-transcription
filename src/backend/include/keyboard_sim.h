#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <windows.h>

namespace voice_transcription {

// Exception for keyboard simulation errors
class KeypressSimulationException : public std::runtime_error {
public:
    explicit KeypressSimulationException(const std::string& message)
        : std::runtime_error(message) {}
};

// Keyboard shortcut structure
struct Shortcut {
    std::vector<std::string> modifiers; // e.g., ["Ctrl", "Shift"]
    std::string key;                   // e.g., "T"
    bool is_valid;
    
    // Convert modifiers and key to Windows virtual key codes
    WORD get_key_code() const;
    WORD get_modifiers_code() const;
};

// Keyboard simulator class
class KeyboardSimulator {
public:
    KeyboardSimulator();
    
    // Main simulation method
    bool simulate_keypresses(const std::wstring& text, int delay_ms = 20);
    
    // Special key handling
    bool simulate_special_key(const std::wstring& key_command);
    
    // Methods for handling keyboard shortcuts
    static bool register_global_hotkey(const Shortcut& shortcut);
    static bool unregister_global_hotkey(const Shortcut& shortcut);

private:
    // Helper methods
    bool send_unicode_character(wchar_t character);
    bool send_special_key(WORD virtual_key, bool is_extended = false);
    bool send_key_with_modifiers(WORD virtual_key, WORD modifiers);
    
    // Maps for special keys and modifiers
    std::unordered_map<std::wstring, WORD> special_key_map_;
    std::unordered_map<std::string, WORD> modifier_map_;
    
    // Hotkey IDs
    static inline int next_hotkey_id_ = 1;
    static std::unordered_map<int, std::pair<WORD, WORD>> registered_hotkeys_;
};

// Clipboard operations (alternative to keypresses)
class ClipboardManager {
public:
    static bool set_clipboard_text(const std::wstring& text);
    static std::wstring get_clipboard_text();
};

} // namespace voice_transcription