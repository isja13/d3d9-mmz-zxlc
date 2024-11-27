#include "ini.h"
#include "overlay.h"
#include "main.h"
#include "conf.h"
#include "globals.h"

#ifndef ENABLE_LOGGER
#define ENABLE_LOGGER 1
#endif

#define ENABLE_SLANG_SHADER 1


// Define STRINGIFY and LSTRINGIFY
//#define STRINGIFY(x) #x
//#define LSTRINGIFY(x) L ## #x

namespace {

    typedef MAP_ENUM(::BYTE) VkMapEnum; // Fully qualify BYTE
    const VkMapEnum VK_MAP_ENUM = {
        MAP_ENUM_ITEM(VK_LBUTTON),
        MAP_ENUM_ITEM(VK_RBUTTON),
        MAP_ENUM_ITEM(VK_CANCEL),
        MAP_ENUM_ITEM(VK_MBUTTON),
        MAP_ENUM_ITEM(VK_XBUTTON1),
        MAP_ENUM_ITEM(VK_XBUTTON2),
        MAP_ENUM_ITEM(VK_BACK),
        MAP_ENUM_ITEM(VK_TAB),
        MAP_ENUM_ITEM(VK_CLEAR),
        MAP_ENUM_ITEM(VK_RETURN),
        MAP_ENUM_ITEM(VK_SHIFT),
        MAP_ENUM_ITEM(VK_CONTROL),
        MAP_ENUM_ITEM(VK_MENU),
        MAP_ENUM_ITEM(VK_PAUSE),
        MAP_ENUM_ITEM(VK_CAPITAL),
        MAP_ENUM_ITEM(VK_KANA),
        MAP_ENUM_ITEM(VK_HANGEUL),
        MAP_ENUM_ITEM(VK_HANGUL),
        MAP_ENUM_ITEM(VK_JUNJA),
        MAP_ENUM_ITEM(VK_FINAL),
        MAP_ENUM_ITEM(VK_HANJA),
        MAP_ENUM_ITEM(VK_KANJI),
        MAP_ENUM_ITEM(VK_ESCAPE),
        MAP_ENUM_ITEM(VK_CONVERT),
        MAP_ENUM_ITEM(VK_NONCONVERT),
        MAP_ENUM_ITEM(VK_ACCEPT),
        MAP_ENUM_ITEM(VK_MODECHANGE),
        MAP_ENUM_ITEM(VK_SPACE),
        MAP_ENUM_ITEM(VK_PRIOR),
        MAP_ENUM_ITEM(VK_NEXT),
        MAP_ENUM_ITEM(VK_END),
        MAP_ENUM_ITEM(VK_HOME),
        MAP_ENUM_ITEM(VK_LEFT),
        MAP_ENUM_ITEM(VK_UP),
        MAP_ENUM_ITEM(VK_RIGHT),
        MAP_ENUM_ITEM(VK_DOWN),
        MAP_ENUM_ITEM(VK_SELECT),
        MAP_ENUM_ITEM(VK_PRINT),
        MAP_ENUM_ITEM(VK_EXECUTE),
        MAP_ENUM_ITEM(VK_SNAPSHOT),
        MAP_ENUM_ITEM(VK_INSERT),
        MAP_ENUM_ITEM(VK_DELETE),
        MAP_ENUM_ITEM(VK_HELP),
        MAP_ENUM_ITEM(VK_LWIN),
        MAP_ENUM_ITEM(VK_RWIN),
        MAP_ENUM_ITEM(VK_APPS),
        MAP_ENUM_ITEM(VK_SLEEP),
        MAP_ENUM_ITEM(VK_NUMPAD0),
        MAP_ENUM_ITEM(VK_NUMPAD1),
        MAP_ENUM_ITEM(VK_NUMPAD2),
        MAP_ENUM_ITEM(VK_NUMPAD3),
        MAP_ENUM_ITEM(VK_NUMPAD4),
        MAP_ENUM_ITEM(VK_NUMPAD5),
        MAP_ENUM_ITEM(VK_NUMPAD6),
        MAP_ENUM_ITEM(VK_NUMPAD7),
        MAP_ENUM_ITEM(VK_NUMPAD8),
        MAP_ENUM_ITEM(VK_NUMPAD9),
        MAP_ENUM_ITEM(VK_MULTIPLY),
        MAP_ENUM_ITEM(VK_ADD),
        MAP_ENUM_ITEM(VK_SEPARATOR),
        MAP_ENUM_ITEM(VK_SUBTRACT),
        MAP_ENUM_ITEM(VK_DECIMAL),
        MAP_ENUM_ITEM(VK_DIVIDE),
        MAP_ENUM_ITEM(VK_F1),
        MAP_ENUM_ITEM(VK_F2),
        MAP_ENUM_ITEM(VK_F3),
        MAP_ENUM_ITEM(VK_F4),
        MAP_ENUM_ITEM(VK_F5),
        MAP_ENUM_ITEM(VK_F6),
        MAP_ENUM_ITEM(VK_F7),
        MAP_ENUM_ITEM(VK_F8),
        MAP_ENUM_ITEM(VK_F9),
        MAP_ENUM_ITEM(VK_F10),
        MAP_ENUM_ITEM(VK_F11),
        MAP_ENUM_ITEM(VK_F12),
        MAP_ENUM_ITEM(VK_F13),
        MAP_ENUM_ITEM(VK_F14),
        MAP_ENUM_ITEM(VK_F15),
        MAP_ENUM_ITEM(VK_F16),
        MAP_ENUM_ITEM(VK_F17),
        MAP_ENUM_ITEM(VK_F18),
        MAP_ENUM_ITEM(VK_F19),
        MAP_ENUM_ITEM(VK_F20),
        MAP_ENUM_ITEM(VK_F21),
        MAP_ENUM_ITEM(VK_F22),
        MAP_ENUM_ITEM(VK_F23),
        MAP_ENUM_ITEM(VK_F24),
        MAP_ENUM_ITEM(VK_NUMLOCK),
        MAP_ENUM_ITEM(VK_SCROLL),
        MAP_ENUM_ITEM(VK_OEM_NEC_EQUAL),
        MAP_ENUM_ITEM(VK_OEM_FJ_JISHO),
        MAP_ENUM_ITEM(VK_OEM_FJ_MASSHOU),
        MAP_ENUM_ITEM(VK_OEM_FJ_TOUROKU),
        MAP_ENUM_ITEM(VK_OEM_FJ_LOYA),
        MAP_ENUM_ITEM(VK_OEM_FJ_ROYA),
        MAP_ENUM_ITEM(VK_LSHIFT),
        MAP_ENUM_ITEM(VK_RSHIFT),
        MAP_ENUM_ITEM(VK_LCONTROL),
        MAP_ENUM_ITEM(VK_RCONTROL),
        MAP_ENUM_ITEM(VK_LMENU),
        MAP_ENUM_ITEM(VK_RMENU),
        MAP_ENUM_ITEM(VK_BROWSER_BACK),
        MAP_ENUM_ITEM(VK_BROWSER_FORWARD),
        MAP_ENUM_ITEM(VK_BROWSER_REFRESH),
        MAP_ENUM_ITEM(VK_BROWSER_STOP),
        MAP_ENUM_ITEM(VK_BROWSER_SEARCH),
        MAP_ENUM_ITEM(VK_BROWSER_FAVORITES),
        MAP_ENUM_ITEM(VK_BROWSER_HOME),
        MAP_ENUM_ITEM(VK_VOLUME_MUTE),
        MAP_ENUM_ITEM(VK_VOLUME_DOWN),
        MAP_ENUM_ITEM(VK_VOLUME_UP),
        MAP_ENUM_ITEM(VK_MEDIA_NEXT_TRACK),
        MAP_ENUM_ITEM(VK_MEDIA_PREV_TRACK),
        MAP_ENUM_ITEM(VK_MEDIA_STOP),
        MAP_ENUM_ITEM(VK_MEDIA_PLAY_PAUSE),
        MAP_ENUM_ITEM(VK_LAUNCH_MAIL),
        MAP_ENUM_ITEM(VK_LAUNCH_MEDIA_SELECT),
        MAP_ENUM_ITEM(VK_LAUNCH_APP1),
        MAP_ENUM_ITEM(VK_LAUNCH_APP2),
        MAP_ENUM_ITEM(VK_OEM_1),
        MAP_ENUM_ITEM(VK_OEM_PLUS),
        MAP_ENUM_ITEM(VK_OEM_COMMA),
        MAP_ENUM_ITEM(VK_OEM_MINUS),
        MAP_ENUM_ITEM(VK_OEM_PERIOD),
        MAP_ENUM_ITEM(VK_OEM_2),
        MAP_ENUM_ITEM(VK_OEM_3),
        MAP_ENUM_ITEM(VK_OEM_4),
        MAP_ENUM_ITEM(VK_OEM_5),
        MAP_ENUM_ITEM(VK_OEM_6),
        MAP_ENUM_ITEM(VK_OEM_7),
        MAP_ENUM_ITEM(VK_OEM_8),
        MAP_ENUM_ITEM(VK_OEM_AX),
        MAP_ENUM_ITEM(VK_OEM_102),
        MAP_ENUM_ITEM(VK_ICO_HELP),
        MAP_ENUM_ITEM(VK_ICO_00),
        MAP_ENUM_ITEM(VK_PROCESSKEY),
        MAP_ENUM_ITEM(VK_ICO_CLEAR),
        MAP_ENUM_ITEM(VK_PACKET),
        MAP_ENUM_ITEM(VK_OEM_RESET),
        MAP_ENUM_ITEM(VK_OEM_JUMP),
        MAP_ENUM_ITEM(VK_OEM_PA1),
        MAP_ENUM_ITEM(VK_OEM_PA2),
        MAP_ENUM_ITEM(VK_OEM_PA3),
        MAP_ENUM_ITEM(VK_OEM_WSCTRL),
        MAP_ENUM_ITEM(VK_OEM_CUSEL),
        MAP_ENUM_ITEM(VK_OEM_ATTN),
        MAP_ENUM_ITEM(VK_OEM_FINISH),
        MAP_ENUM_ITEM(VK_OEM_COPY),
        MAP_ENUM_ITEM(VK_OEM_AUTO),
        MAP_ENUM_ITEM(VK_OEM_ENLW),
        MAP_ENUM_ITEM(VK_OEM_BACKTAB),
        MAP_ENUM_ITEM(VK_ATTN),
        MAP_ENUM_ITEM(VK_CRSEL),
        MAP_ENUM_ITEM(VK_EXSEL),
        MAP_ENUM_ITEM(VK_EREOF),
        MAP_ENUM_ITEM(VK_PLAY),
        MAP_ENUM_ITEM(VK_ZOOM),
        MAP_ENUM_ITEM(VK_NONAME),
        MAP_ENUM_ITEM(VK_PA1),
        MAP_ENUM_ITEM(VK_OEM_CLEAR),
    };

    BYTE ini_parse_vk(_tstring_view s) {
        if (s.size() == 1) {
            TCHAR c = s[0];
            if (c >= _T('0') && c <= _T('9')) return c;
            c = _totupper(c);
            if (c >= _T('A') && c <= _T('Z')) return c;
        }

        LPCTSTR const hex_prefix = _T("0X");
        const size_t hex_prefix_size = _tcslen(hex_prefix);
        if (s.size() > hex_prefix_size && _tcsnicmp(hex_prefix, s.data(), hex_prefix_size) == 0) {
            _tstring str = _tstring(s);
            LPTSTR endptr;
            unsigned long c = _tcstoul(str.c_str() + hex_prefix_size, &endptr, 16);
            if (endptr == str.c_str() + str.size() && c < VK_VALUE_END) return c;
            return 0;
        }

        auto it = VK_MAP_ENUM.find(s);
        if (it != VK_MAP_ENUM.end()) return it->second;

        return 0;
    }

    std::vector<BYTE> ini_parse_vk_comb(_tstring_view s) {
        std::vector<BYTE> ret;
        LPCTSTR str = _T("+-");
        _tstring_view::size_type pos = 0;
        while (1) {
            _tstring_view::size_type pos_end = s.find_first_of(str, pos);
            BYTE vk = ini_parse_vk(s.substr(pos, pos_end - pos));
            if (!vk) return {};
            ret.push_back(vk);
            if (pos_end == _tstring_view::npos) break;
            pos = s.find_first_not_of(str, pos_end);
        }
        return ret;
    }

}

class Ini::Impl {
    friend class Ini;

    LPCTSTR file_name = NULL;
    struct atomic_ConfigPtr {
        std::atomic<Config*> config;
        atomic_ConfigPtr(Config* config = NULL) : config(config) {}
        atomic_ConfigPtr& operator=(Config* config) {
            this->config.store(config);
            return *this;
        }
        Config* operator->() const { return *this; }
        operator Config* () const { return config.load(); }
    } config = NULL;
    struct atomic_OverlayPtr {
        std::atomic<Overlay*> overlay;
        atomic_OverlayPtr(Overlay* overlay = NULL) : overlay(overlay) {}

        template<class... As>
        void operator()(const As&... as) const {
            if (overlay.load()) {
                overlay.load()->push_text(as...); // Corrected line
            }
        }

        atomic_OverlayPtr& operator=(Overlay* overlay) {
            this->overlay.store(overlay);
            return *this;
        }

        Overlay* operator->() const { return overlay.load(); } // Corrected line

        operator Overlay* () const { return overlay.load(); }
    } overlay = { NULL };

    HANDLE file = INVALID_HANDLE_VALUE;
    UINT64 ini_time = 0;
    bool thread_running = true;

    static DWORD WINAPI ini_ThreadProc(LPVOID lpParameter) {
        return ((Impl*)lpParameter)->ini_proc();
    }

    DWORD ini_proc() {
        while (1) {
            if (!thread_running) {
                ini_file_shutdown();
                delete this;
                return 0;
            }

            ini_file_init();

            UINT64 time = 0;
            if (GetFileTime(file, NULL, NULL, (LPFILETIME)&time)) {
                if (!ini_time || ini_time != time) load_ini();
                ini_time = time;
            }
            Sleep(250);
        }
    }

    Impl(LPCTSTR file_name) : file_name(file_name) {
        CreateThread(NULL, 0, ini_ThreadProc, this, 0, NULL);
    }

    void ini_file_init() {
        if (file == INVALID_HANDLE_VALUE) {
            file = CreateFile(
                file_name,
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );
            if (file == INVALID_HANDLE_VALUE) {
                overlay("Failed to create ini file ", file_name);
            }
        }
    }

    void ini_file_shutdown() {
        if (file != INVALID_HANDLE_VALUE) {
            CloseHandle(file);
            file = INVALID_HANDLE_VALUE;
        }
    }

    void load_ini() {
        if (!config) return;
        config->begin_config();

        const DWORD n_size = MAX_PATH + 1;
        TCHAR returned_string[n_size] = {};

#define GET_INI_VALUE(n) do { \
    GetPrivateProfileString(_T(STRINGIFY(SECTION)), _T(#n), NULL, returned_string, n_size, (_tstring(_T(".\\")) + file_name).c_str()); \
} while (0)

#define OVERLAY_PUSH_INVALID_VALUE(v) overlay("Invalid [" LSTRINGIFY(SECTION) "]." #v " value")

#define GET_SET_CONFIG_BOOL_VALUE_KEY(v, k) do { \
    GET_INI_VALUE(v); \
    bool v = _tcsicmp(returned_string, _T("TRUE")) == 0; \
    if (!v && *returned_string && _tcsicmp(returned_string, _T("FALSE")) != 0) { \
        OVERLAY_PUSH_INVALID_VALUE(v); \
    } \
    config->k = v; \
} while (0)
#define GET_SET_CONFIG_BOOL_VALUE(v) GET_SET_CONFIG_BOOL_VALUE_KEY(v, v)

#define GET_LONG_VALUE(v) do { \
    LPTSTR endptr; \
    v = _tcstol(returned_string, & endptr, 0); \
    if (returned_string + _tcslen(returned_string) != endptr) v = -1; \
} while (0)

#define GET_SET_CONFIG_UINT_VALUE(v) do { \
    GET_INI_VALUE(v); \
    long v; \
    GET_LONG_VALUE(v); \
    if (v < 0) { \
        OVERLAY_PUSH_INVALID_VALUE(v); \
        v = 0; \
    } \
    if (config->v != (UINT)v) { \
        config->v = v; \
        config->UPDATED_VAR = true; \
    } \
} while (0)

#ifdef _UNICODE
#define GET_UTF8_VAL(v) do { \
    int val_len = WideCharToMultiByte(CP_UTF8, 0, returned_string, -1, NULL, 0, NULL, NULL); \
    std::vector<CHAR> val_chars(val_len); \
    WideCharToMultiByte(CP_UTF8, 0, returned_string, -1, val_chars.data(), val_len, NULL, NULL); \
    v = std::string(val_chars.begin(), val_chars.end()); \
} while (0)
#else
#define GET_UTF8_VAL(v) do { \
    v = returned_string; \
} while (0)
#endif

#define GET_SET_CONFIG_UTF8_VALUE_KEY(v, k) do { \
    GET_INI_VALUE(v); \
    std::string v; \
    GET_UTF8_VAL(v); \
    if (v != config->k) { \
        config->k = v; \
        config->k ## _updated = true; \
    } \
} while (0)
#define GET_SET_CONFIG_UTF8_VALUE(v) GET_SET_CONFIG_UTF8_VALUE_KEY(v, v)

#define GET_SET_CONFIG_VK_VALUE(v) do { \
    GET_INI_VALUE(hotkey_ ## v); \
    config->log_ ## v ## _hotkey = ini_parse_vk_comb(returned_string); \
    if (*returned_string && !config->log_ ## v ## _hotkey.size()) \
        overlay("Invalid hotkey for log " #v); \
} while (0)

#define SECTION logging

#if ENABLE_LOGGER
        GET_SET_CONFIG_BOOL_VALUE_KEY(enabled, logging_enabled);
        GET_SET_CONFIG_VK_VALUE(toggle);
        GET_SET_CONFIG_VK_VALUE(frame);
#endif


#undef SECTION

#define SECTION graphics

        GET_SET_CONFIG_BOOL_VALUE(interp);
        GET_SET_CONFIG_BOOL_VALUE(linear);
        GET_SET_CONFIG_BOOL_VALUE(enhanced);

#define UPDATED_VAR linear_test_updated
        GET_SET_CONFIG_UINT_VALUE(linear_test_width);
        GET_SET_CONFIG_UINT_VALUE(linear_test_height);
#undef UPDATED_VAR

#ifdef ENABLE_SLANG_SHADER
        GET_SET_CONFIG_UTF8_VALUE_KEY(slang_shader, slang_shader_2d);
        GET_SET_CONFIG_UTF8_VALUE(slang_shader_gba);
        GET_SET_CONFIG_UTF8_VALUE(slang_shader_ds);

        do {
            GetPrivateProfileStringW(L"SECTION", L"slang_shader", 0, returned_string, n_size, (std::basic_string<TCHAR>(L".\\") + file_name).c_str());
        } while (0);
        std::string slang_shader;
        do {
            int val_len = WideCharToMultiByte(65001, 0, returned_string, -1, nullptr, 0, nullptr, nullptr);
            std::vector<CHAR> val_chars(val_len);
            WideCharToMultiByte(65001, 0, returned_string, -1, val_chars.data(), val_len, nullptr, nullptr);
            slang_shader.assign(val_chars.begin(), val_chars.end());
        } while (0);
        if (slang_shader != config->slang_shader_2d) {
            config->slang_shader_2d = slang_shader;
            config->slang_shader_2d_updated = true;
        }

        do {
            GetPrivateProfileStringW(L"SECTION", L"slang_shader_gba", 0, returned_string, n_size, (std::basic_string<TCHAR>(L".\\") + file_name).c_str());
        } while (0);
        std::string slang_shader_gba;
        do {
            int val_len = WideCharToMultiByte(65001, 0, returned_string, -1, nullptr, 0, nullptr, nullptr);
            std::vector<CHAR> val_chars(val_len);
            WideCharToMultiByte(65001, 0, returned_string, -1, val_chars.data(), val_len, nullptr, nullptr);
            slang_shader_gba.assign(val_chars.begin(), val_chars.end());
        } while (0);
        if (slang_shader_gba != config->slang_shader_gba) {
            config->slang_shader_gba = slang_shader_gba;
            config->slang_shader_gba_updated = true;
        }

        do {
            GetPrivateProfileStringW(L"SECTION", L"slang_shader_ds", 0, returned_string, n_size, (std::basic_string<TCHAR>(L".\\") + file_name).c_str());
        } while (0);
        std::string slang_shader_ds;
        do {
            int val_len = WideCharToMultiByte(65001, 0, returned_string, -1, nullptr, 0, nullptr, nullptr);
            std::vector<CHAR> val_chars(val_len);
            WideCharToMultiByte(65001, 0, returned_string, -1, val_chars.data(), val_len, nullptr, nullptr);
            slang_shader_ds.assign(val_chars.begin(), val_chars.end());
        } while (0);
        if (slang_shader_ds != config->slang_shader_ds) {
            config->slang_shader_ds = slang_shader_ds;
            config->slang_shader_ds_updated = true;
        }
#endif


#undef SECTION

        config->end_config();
    }

    void set_config(Config* config) {
        this->config = config;
        load_ini();
    }

    void set_overlay(Overlay* overlay) {
        this->overlay = { overlay };
    }
};

Ini::Ini(LPCTSTR file_name, Overlay* overlay, Config* config) : impl(new Impl{ file_name }) {
    impl->set_overlay(overlay);
    impl->set_config(config);
}

Ini::~Ini() {
    impl->thread_running = false;
}

void Ini::set_config(Config* config) {
    impl->set_config(config);
}

void Ini::set_overlay(Overlay* overlay) {
    impl->set_overlay(overlay);
}

//Ini* default_ini = nullptr;