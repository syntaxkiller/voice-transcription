#include "keyboard_sim.h"
#include <unordered_map>
#include <thread>
#include <chrono>
#include <sstream>
#include <regex>

namespace voice_transcription {

// Static initialization
std::unordered_map<int, std::pair<WORD, WORD>> KeyboardSimulator::registered_hotkeys_;

// Mapping from modifier strings to Windows virtual key codes
static const std::unordered_map<std::string, WORD> MODIFIER_MAP = {
    { "Ctrl", MOD_CONTROL },
    { "Alt", MOD_ALT },
    { "Shift", MOD_SHIFT },
    { "Win", MOD_WIN }
};

// Mapping from key strings to Windows virtual key codes
static const std::unordered_map<std::string, WORD> KEY_MAP = {
    // Alphabet
    { "A", 0x41 }, { "B", 0x42 }, { "C", 0x43 }, { "D", 0x44 }, { "E", 0x45 },
    { "F", 0x46 }, { "G", 0x47 }, { "H", 0x48 }, { "I", 0x49 }, { "J", 0x4A },
    { "K", 0x4B }, { "L", 0x4C }, { "M", 0x4D }, { "N", 0x4E }, { "O", 0x4F },
    { "P", 0x50 }, { "Q", 0x51 }, { "R", 0x52 }, { "S", 0x53 }, { "T", 0x54 },
    { "U", 0x55 }, { "V", 0x56 }, { "W", 0x57 }, { "X", 0x58 }, { "Y", 0x59 },
    { "Z", 0x5A },
    
    // Numbers
    { "0", 0x30 }, { "1", 0x31 }, { "2", 0x32 }, { "3", 0x33 }, { "4", 0x34 },
    { "5", 0x35 }, { "6", 0x36 }, { "7", 0x37 }, { "8", 0x38 }, { "9", 0x39 },
    
    // Function keys
    { "F1", VK_F1 }, { "F2", VK_F2 }, { "F3", VK_F3 }, { "F4", VK_F4 },
    { "F5", VK_F5 }, { "F6", VK_F6 }, { "F7", VK_F7 }, { "F8", VK_F8 },
    { "F9", VK_F9 }, { "F10", VK_F10 }, { "F11", VK_F11 }, { "F12", VK_F12 },
    
    // Special keys
    { "Tab", VK_TAB }, { "Enter", VK_RETURN }, { "Space", VK_SPACE },
    { "Backspace", VK_BACK }, { "Delete", VK_DELETE }, { "Escape", VK_ESCAPE },
    { "Home", VK_HOME }, { "End", VK_END }, { "PageUp", VK_PRIOR }, { "PageDown", VK_NEXT },
    { "Left", VK_LEFT }, { "Right", VK_RIGHT }, { "Up", VK_UP }, { "Down", VK_DOWN },
    { "Insert", VK_INSERT }, { "CapsLock", VK_CAPITAL }
};

// Shortcut implementation
WORD Shortcut::get_key_code() const {
    auto it = KEY_MAP.find(key);
    if (it != KEY_MAP.end()) {
        return it->second;
    }
    return 0;
}

WORD Shortcut::get_modifiers_code() const {
    WORD mod_code = 0;
    for (const auto& mod : modifiers) {
        auto it = MODIFIER_MAP.find(mod);
        if (it != MODIFIER_MAP.end()) {
            mod_code |= it->second;
        }
    }
    return mod_code;
}

// KeyboardSimulator implementation
KeyboardSimulator::KeyboardSimulator() {
    // Initialize special key map
    special_key_map_ = {
        { L"{ENTER}", VK_RETURN },
        { L"{TAB}", VK_TAB },
        { L"{SPACE}", VK_SPACE },
        { L"{BACKSPACE}", VK_BACK },
        { L"{DELETE}", VK_DELETE },
        { L"{ESCAPE}", VK_ESCAPE },
        { L"{HOME}", VK_HOME },
        { L"{END}", VK_END },
        { L"{PAGEUP}", VK_PRIOR },
        { L"{PAGEDOWN}", VK_NEXT },
        { L"{LEFT}", VK_LEFT },
        { L"{RIGHT}", VK_RIGHT },
        { L"{UP}", VK_UP },
        { L"{DOWN}", VK_DOWN },
        { L"{INSERT}", VK_INSERT },
        { L"{CAPSLOCK}", VK_CAPITAL }
    };
    
    // Initialize modifier map
    modifier_map_ = {
        { "Ctrl", MOD_CONTROL },
        { "Alt", MOD_ALT },
        { "Shift", MOD_SHIFT },
        { "Win", MOD_WIN }
    };
}

bool KeyboardSimulator::simulate_keypresses(const std::wstring& text, int delay_ms) {
    if (text.empty()) {
        return true;
    }
    
    // Process special key sequences like {ENTER}, {CTRL+ENTER}, etc.
    std::wregex special_key_regex(L"\\{([^}]+)\\}");
    std::wregex modifier_key_regex(L"([A-Za-z]+)\\+([A-Za-z]+)");
    
    size_t pos = 0;
    while (pos < text.length()) {
        // Check for special key sequence
        std::wsmatch match;
        if (std::regex_search(text.begin() + pos, text.end(), match, special_key_regex)) {
            // Send text before the special key
            if (match.position() > 0) {
                std::wstring prefix = text.substr(pos, match.position());
                for (wchar_t c : prefix) {
                    if (!send_unicode_character(c)) {
                        return false;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
                }
            }
            
            // Process the special key
            std::wstring key_command = match.str(1);
            bool success = simulate_special_key(key_command);
            if (!success) {
                return false;
            }
            
            // Skip past the special key
            pos += match.position() + match.length();
        } else {
            // No special key, just send the character
            if (!send_unicode_character(text[pos])) {
                return false;
            }
            pos++;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }
    
    return true;
}

bool KeyboardSimulator::simulate_special_key(const std::wstring& key_command) {
    // Check for modifier+key combination (e.g., CTRL+ENTER)
    std::wregex modifier_key_regex(L"([A-Za-z]+)\\+([A-Za-z]+)");
    std::wsmatch match;
    
    if (std::regex_search(key_command, match, modifier_key_regex)) {
        // Extract modifier and key
        std::wstring modifier_str = match.str(1);
        std::wstring key_str = match.str(2);
        
        // Convert to ASCII for lookup
        std::string modifier_ascii(modifier_str.begin(), modifier_str.end());
        std::string key_ascii(key_str.begin(), key_str.end());
        
        // Look up modifier code
        WORD modifier_code = 0;
        for (const auto& [mod_str, mod_code] : modifier_map_) {
            if (_stricmp(modifier_ascii.c_str(), mod_str.c_str()) == 0) {
                modifier_code = mod_code;
                break;
            }
        }
        
        // Look up key code
        WORD key_code = 0;
        for (const auto& [k_str, k_code] : KEY_MAP) {
            if (_stricmp(key_ascii.c_str(), k_str.c_str()) == 0) {
                key_code = k_code;
                break;
            }
        }
        
        if (modifier_code != 0 && key_code != 0) {
            return send_key_with_modifiers(key_code, modifier_code);
        }
    } else {
        // Check for simple special key (e.g., ENTER)
        auto it = special_key_map_.find(L"{" + key_command + L"}");
        if (it != special_key_map_.end()) {
            return send_special_key(it->second);
        }
    }
    
    // Unknown special key
    return false;
}

bool KeyboardSimulator::send_unicode_character(wchar_t character) {
    // Prepare input event for keyboard
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = 0; // Virtual key code not used for Unicode
    input.ki.wScan = character; // Unicode character
    input.ki.dwFlags = KEYEVENTF_UNICODE; // Unicode flag
    
    // Send key down
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        return false;
    }
    
    // Send key up
    input.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        return false;
    }
    
    return true;
}

bool KeyboardSimulator::send_special_key(WORD virtual_key, bool is_extended) {
    // Prepare input event for keyboard
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = virtual_key;
    input.ki.wScan = 0;
    input.ki.dwFlags = is_extended ? KEYEVENTF_EXTENDEDKEY : 0;
    
    // Send key down
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        return false;
    }
    
    // Send key up
    input.ki.dwFlags |= KEYEVENTF_KEYUP;
    if (SendInput(1, &input, sizeof(INPUT)) != 1) {
        return false;
    }
    
    return true;
}

bool KeyboardSimulator::send_key_with_modifiers(WORD virtual_key, WORD modifiers) {
    // Map Windows modifiers to virtual key codes
    struct ModifierKeyPair {
        DWORD mod_flag;
        WORD vk_code;
    };
    
    static const ModifierKeyPair MOD_KEYS[] = {
        { MOD_CONTROL, VK_CONTROL },
        { MOD_SHIFT, VK_SHIFT },
        { MOD_ALT, VK_MENU },
        { MOD_WIN, VK_LWIN }
    };
    
    // Prepare input events for multiple keys
    std::vector<INPUT> inputs;
    inputs.reserve(8); // 4 modifiers down + key down + key up + 4 modifiers up
    
    // Prepare modifiers (key down)
    for (const auto& mod_pair : MOD_KEYS) {
        if (modifiers & mod_pair.mod_flag) {
            INPUT input = { 0 };
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = mod_pair.vk_code;
            input.ki.dwFlags = 0;
            inputs.push_back(input);
        }
    }
    
    // Prepare main key (key down)
    {
        INPUT input = { 0 };
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = virtual_key;
        input.ki.dwFlags = 0;
        inputs.push_back(input);
    }
    
    // Prepare main key (key up)
    {
        INPUT input = { 0 };
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = virtual_key;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(input);
    }
    
    // Prepare modifiers (key up) - release in reverse order
    for (auto it = std::rbegin(MOD_KEYS); it != std::rend(MOD_KEYS); ++it) {
        if (modifiers & it->mod_flag) {
            INPUT input = { 0 };
            input.type = INPUT_KEYBOARD;
            input.ki.wVk = it->vk_code;
            input.ki.dwFlags = KEYEVENTF_KEYUP;
            inputs.push_back(input);
        }
    }
    
    // Send all inputs at once
    UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    return sent == inputs.size();
}

bool KeyboardSimulator::register_global_hotkey(const Shortcut& shortcut) {
    if (!shortcut.is_valid) {
        return false;
    }
    
    WORD key_code = shortcut.get_key_code();
    WORD modifiers = shortcut.get_modifiers_code();
    
    if (key_code == 0 || modifiers == 0) {
        return false;
    }
    
    int id = next_hotkey_id_++;
    if (RegisterHotKey(NULL, id, modifiers, key_code)) {
        registered_hotkeys_[id] = std::make_pair(modifiers, key_code);
        return true;
    }
    
    return false;
}

bool KeyboardSimulator::unregister_global_hotkey(const Shortcut& shortcut) {
    WORD key_code = shortcut.get_key_code();
    WORD modifiers = shortcut.get_modifiers_code();
    
    // Find the hotkey ID
    for (const auto& [id, key_pair] : registered_hotkeys_) {
        if (key_pair.first == modifiers && key_pair.second == key_code) {
            if (UnregisterHotKey(NULL, id)) {
                registered_hotkeys_.erase(id);
                return true;
            }
            return false;
        }
    }
    
    return false;
}

// ClipboardManager implementation
bool ClipboardManager::set_clipboard_text(const std::wstring& text) {
    // Open clipboard
    if (!OpenClipboard(NULL)) {
        return false;
    }
    
    // Empty the clipboard
    if (!EmptyClipboard()) {
        CloseClipboard();
        return false;
    }
    
    // Calculate buffer size
    size_t bytes_needed = (text.length() + 1) * sizeof(wchar_t);
    
    // Allocate global memory
    HGLOBAL h_mem = GlobalAlloc(GMEM_MOVEABLE, bytes_needed);
    if (!h_mem) {
        CloseClipboard();
        return false;
    }
    
    // Lock and copy the text to memory
    wchar_t* mem_ptr = static_cast<wchar_t*>(GlobalLock(h_mem));
    if (!mem_ptr) {
        GlobalFree(h_mem);
        CloseClipboard();
        return false;
    }
    
    wcscpy_s(mem_ptr, text.length() + 1, text.c_str());
    GlobalUnlock(h_mem);
    
    // Set clipboard data
    HANDLE result = SetClipboardData(CF_UNICODETEXT, h_mem);
    
    // Close clipboard
    CloseClipboard();
    
    return result != NULL;
}

std::wstring ClipboardManager::get_clipboard_text() {
    std::wstring text;
    
    // Open clipboard
    if (!OpenClipboard(NULL)) {
        return text;
    }
    
    // Get handle to clipboard data
    HANDLE h_data = GetClipboardData(CF_UNICODETEXT);
    if (!h_data) {
        CloseClipboard();
        return text;
    }
    
    // Lock and copy the text from memory
    wchar_t* text_ptr = static_cast<wchar_t*>(GlobalLock(h_data));
    if (text_ptr) {
        text = text_ptr;
        GlobalUnlock(h_data);
    }
    
    // Close clipboard
    CloseClipboard();
    
    return text;
}

} // namespace voice_transcription