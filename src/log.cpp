#include "log.h"
#include "globals.h"
#include "overlay.h"
#include "conf.h"
#include <d3d9types.h>
#include "d3d9buffer.h"
#include "d3d9texture1d.h"
#include "d3d9texture2d.h"
#include "d3d9rendertargetview.h"
#include "d3d9shaderresourceview.h"
#include "d3d9depthstencilview.h"
#include "d3d9inputlayout.h"
//#include "d3d9_definitions.h"
#include "MyID3DIndexBuffer9.h"
#include <map>
#include <string>


#define SO_B_LEN 1024 // Define the stream output buffer length appropriately

#ifndef LOG
#define LOG(msg) // Define LOG if it's not defined (you might have a different implementation)
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreorder"
#include "../HLSLcc/include/hlslcc.h"
#pragma GCC diagnostic pop

class Logger::Impl {
public:
    Impl(LPCTSTR file_name, Config* config, Overlay* overlay);
    ~Impl();

    bool log_begin(Logger* outer);
    void log_end(Logger* outer);

    void set_overlay(Overlay* overlay);
    void set_config(Config* config);
    bool get_started() const;
    void next_frame();

private:
    bool file_init();
    void file_shutdown();
    void update_config();

    bool hotkey_active(const std::vector<BYTE>& vks) const;

    LPCTSTR file_name;
    Config* config;
    Overlay* overlay;
    bool started;
    UINT64 start_count;
    UINT64 frame_count;
    HANDLE file;
    std::ostringstream oss;
    cs_wrapper oss_cs;

    bool log_enabled;
    bool log_toggle_hotkey_active;
    bool log_frame_hotkey_active;
    bool log_frame_active;
};

Logger::Logger(LPCTSTR file_name, Config* config, Overlay* overlay)
    : impl(new Impl(file_name, config, overlay)), oss(std::cout) {}

Logger::~Logger() {
    delete impl;
}

// Define the ShaderLogger constructor
ShaderLogger::ShaderLogger(const void* a) : a(a) {
    // Assuming the shader source is in ASCII or UTF-8 format, otherwise adapt as necessary
    source = std::string(static_cast<const char*>(a));
}



void Logger::log_item(PIXEL_SHADER_ALPHA_DISCARD item) const {
    switch (item) {
    case PIXEL_SHADER_ALPHA_DISCARD::UNKNOWN:
        LOG("UNKNOWN");
        break;
    case PIXEL_SHADER_ALPHA_DISCARD::NONE:
        LOG("NONE");
        break;
    case PIXEL_SHADER_ALPHA_DISCARD::EQUAL:
        LOG("EQUAL");
        break;
    case PIXEL_SHADER_ALPHA_DISCARD::LESS:
        LOG("LESS");
        break;
    case PIXEL_SHADER_ALPHA_DISCARD::LESS_OR_EQUAL:
        LOG("LESS_OR_EQUAL");
        break;
    default:
        LOG("Unknown value");
    }
}

void Logger::log_item(const ShaderLogger& a) const {
    oss << "Shader Source: " << a.source << "\n";
}

void Logger::set_overlay(Overlay* overlay) {
    impl->set_overlay(overlay);
}

void Logger::set_config(Config* config) {
    impl->set_config(config);
}

bool Logger::get_started() const {
    return impl->get_started();
}

void Logger::next_frame() {
    impl->next_frame();
}

bool Logger::log_begin() {
    return impl->log_begin(this);
}

void Logger::log_end() {
    impl->log_end(this);
}

void Logger::log_assign() const {
    log_item(" = ");
}

void Logger::log_sep() const {
    log_item(", ");
}

void Logger::log_fun_name(LPCSTR n) const {
    log_item(n);
}

void Logger::log_fun_begin() const {
    log_item('(');
}

void Logger::log_fun_args() const {
    log_fun_end();
}

void Logger::log_fun_args_next() const {
    log_fun_args();
}

void Logger::log_fun_sep() const {
    log_sep();
}

void Logger::log_fun_end() const {
    log_item(')');
}

void Logger::log_struct_begin() const {
    log_item('{');
}

void Logger::log_struct_member_access() const {
    log_item('.');
}

void Logger::log_struct_sep() const {
    log_sep();
}

void Logger::log_struct_end() const {
    log_item('}');
}

void Logger::log_array_begin() const {
    log_item('[');
}

void Logger::log_array_sep() const {
    log_sep();
}

void Logger::log_array_end() const {
    log_item(']');
}

void Logger::log_string_begin() const {
    log_item('"');
}

void Logger::log_string_end() const {
    log_item('"');
}

void Logger::log_char_begin() const {
    log_item('\'');
}

void Logger::log_char_end() const {
    log_item('\'');
}

void Logger::log_null() const {
    log_item("NULL");
}

template<>
void Logger::log_item(bool a) const {
    oss << std::boolalpha << a;
    oss.flags(std::ios::fmtflags{});
}

template<>
void Logger::log_item(CHAR a) const {
    oss << a;
}

template<>
void Logger::log_item(WCHAR a) const {
    WCHAR s[2] = { a, 0 };
    log_item(s);
}

template<>
void Logger::log_item(LPCSTR a) const {
    oss << a;
}



void Logger::log_item(const char* msg) const {
    std::cout << "Log: " << msg << std::endl;
}

void Logger::log_item(char msg) const {
    std::cout << "Log: " << msg << std::endl;
}

void Logger::log_item(const wchar_t* msg) const {
    std::wcout << L"Log: " << msg << std::endl;
}

template<>
void Logger::log_item(unsigned long msg) const {
    std::cout << "Log: " << msg << std::endl;
}

void Logger::log_item(StringLogger a) const {
    std::cout << "Log: " << a.a << std::endl;
}

void Logger::log_item(RawStringLogger a) const {
    std::cout << "Log: " << a.a << " " << a.p << std::endl;
}

void Logger::log_item(ByteArrayLogger a) const {
    std::cout << "Log: ByteArrayLogger with size " << a.n << std::endl;
}

void Logger::log_item(CharLogger a) const {
    std::cout << "Log: " << a.a << std::endl;
}

void Logger::log_item(D3D9_CLEAR_Logger a) const {
    std::cout << "Log: D3D9_CLEAR_Logger " << a.a << std::endl;
}

void Logger::log_item(D3D9_BIND_Logger a) const {
    std::cout << "Log: D3D9_BIND_Logger " << a.a << std::endl;
}

void Logger::log_item(D3D9_CPU_ACCESS_Logger a) const {
    std::cout << "Log: D3D9_CPU_ACCESS_Logger " << a.a << std::endl;
}

void Logger::log_item(D3D9_RESOURCE_MISC_Logger a) const {
    std::cout << "Log: D3D9_RESOURCE_MISC_Logger " << a.a << std::endl;
}

void Logger::log_item(D3D9_SUBRESOURCE_DATA_Logger a) const {
    std::cout << "Log: D3D9_SUBRESOURCE_DATA_Logger with ByteWidth " << a.ByteWidth << std::endl;
}


std::ostream& operator<<(std::ostream& os, const HotkeyLogger& logger) {
    os << "HotkeyLogger [";
    for (size_t i = 0; i < logger.a.size(); ++i) {
        os << std::hex << static_cast<int>(logger.a[i]);
        if (i != logger.a.size() - 1) {
            os << ", ";
        }
    }
    os << "]";
    return os;
}

void Logger::log_item(const GUID* guid) const {
    std::cout << "Log: GUID " << std::hex << std::setw(8) << std::setfill('0') << guid->Data1 << "-"
        << std::setw(4) << guid->Data2 << "-"
        << std::setw(4) << guid->Data3 << "-"
        << std::setw(2) << static_cast<int>(guid->Data4[0]) << std::setw(2) << static_cast<int>(guid->Data4[1]) << "-"
        << std::setw(2) << static_cast<int>(guid->Data4[2]) << std::setw(2) << static_cast<int>(guid->Data4[3])
        << std::setw(2) << static_cast<int>(guid->Data4[4]) << std::setw(2) << static_cast<int>(guid->Data4[5])
        << std::setw(2) << static_cast<int>(guid->Data4[6]) << std::setw(2) << static_cast<int>(guid->Data4[7]) << std::endl;
}

void Logger::log_item(const D3DSAMPLER_DESC* sampler_desc) const {
    std::cout << "Log: D3DSAMPLER_DESC" << std::endl;
}

void Logger::log_item(D3DLOCKED_BOX a) const {
    std::cout << "Log: D3DLOCKED_BOX" << std::endl;
}

void Logger::log_item(D3DLOCKED_RECT a) const {
    std::cout << "Log: D3DLOCKED_RECT" << std::endl;
}

void Logger::log_item(D3DDECLUSAGE a) {
    std::cout << "Log: D3DDECLUSAGE " << static_cast<int>(a) << std::endl;
}

void Logger::log_item(D3DPRIMITIVETYPE a) const {
    std::cout << "Log: D3DPRIMITIVETYPE " << static_cast<int>(a) << std::endl;
}

void Logger::log_item(const MyID3D9ShaderResourceView* a) const {
    std::cout << "Log: MyID3D9ShaderResourceView" << std::endl;
}

void Logger::log_item(const MyID3D9RenderTargetView* a) const {
    std::cout << "Log: MyID3D9RenderTargetView" << std::endl;
}

void Logger::log_item(const MyID3D9DepthStencilView* a) const {
    std::cout << "Log: MyID3D9DepthStencilView" << std::endl;
}

void Logger::log_item(MyID3D9Resource_Logger a) const {
    std::cout << "Log: MyID3D9Resource_Logger" << std::endl;
}

void Logger::log_item(const D3DVERTEXELEMENT9& item) {
    std::cout << "Log: D3DVERTEXELEMENT9" << std::endl;
}

void Logger::log_item(const D3DVERTEXBUFFER_DESC& item) {
    std::cout << "Log: D3DVERTEXBUFFER_DESC" << std::endl;
}

void Logger::log_item(DWORD a) {
    std::cout << "Log: DWORD " << a << std::endl;
}

void Logger::log_item(PIXEL_SHADER_ALPHA_DISCARD item) {
    std::cout << "Log: PIXEL_SHADER_ALPHA_DISCARD" << std::endl;
}

template<>
void Logger::log_item(LPCWSTR a) const {
    int len = WideCharToMultiByte(CP_UTF8, 0, a, -1, NULL, 0, NULL, NULL);
    std::vector<CHAR> b(len);
    WideCharToMultiByte(CP_UTF8, 0, a, -1, b.data(), len, NULL, NULL);
    log_item(b.data());
}

void Logger::log_item(const std::string& a) const {
    oss << a;
}

Logger::Impl::Impl(LPCTSTR file_name, Config* config, Overlay* overlay)
    : file_name(file_name), config(config), overlay(overlay),
    started(false), start_count(0), frame_count(0),
    file(INVALID_HANDLE_VALUE), log_enabled(false),
    log_toggle_hotkey_active(false), log_frame_hotkey_active(false),
    log_frame_active(false) {
    update_config();
}

Logger::Impl::~Impl() {
    file_shutdown();
}

bool Logger::Impl::log_begin(Logger* outer) {
    if (!started) return false;
    oss_cs.begin_cs();
    if (oss.tellp() || (file == INVALID_HANDLE_VALUE && !file_init())) {
        oss_cs.end_cs();
        return false;
    }
    outer->log_item("("); // This is fine, it's a char
    outer->log_item(")("); // This is fine, it's a char
    outer->log_item(static_cast<DWORD>(GetCurrentThreadId())); // Ensure it's a DWORD
    outer->log_item(")"); // This is fine, it's a char
    return true;
}

void Logger::Impl::log_end(Logger* outer) {
    DWORD written = 0;
    std::ostringstream::pos_type len = oss.tellp();
    if (len != -1) {
        WriteFile(file, oss.str().c_str(), len, &written, NULL);
    }
    oss.clear();
    oss.seekp(0);
    oss_cs.end_cs();
}

bool Logger::Impl::file_init() {
    if (file != INVALID_HANDLE_VALUE) return true;
    file = CreateFile(file_name, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    return true;
}

void Logger::Impl::file_shutdown() {
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
        file = INVALID_HANDLE_VALUE;
    }
}

void Logger::Impl::update_config() {
    if (log_frame_active) {
        log_frame_active = false;
    }

    if (!config) return;
    config->begin_config();

    if (log_enabled != config->logging_enabled) {
        if ((log_enabled = config->logging_enabled)) {
            if (!get_started()) {
            }
        }
        else {
            if (get_started()) {
            }
        }
    }

    if (config->hwnd.load() != GetForegroundWindow()) goto end;
    if (hotkey_active(config->log_toggle_hotkey)) {
        if (!log_toggle_hotkey_active) {
            log_toggle_hotkey_active = true;
            if (!get_started()) {
            }
            else {
            }
        }
    }
    else {
        log_toggle_hotkey_active = false;
    }
    if (hotkey_active(config->log_frame_hotkey)) {
        if (!log_frame_hotkey_active) {
            log_frame_hotkey_active = true;
        }
    }
    else {
        log_frame_hotkey_active = false;
    }

end:
    config->end_config();
}

bool Logger::Impl::hotkey_active(const std::vector<BYTE>& vks) const {
    if (!vks.size()) return false;
    for (BYTE vk : vks) {
        if (!GetAsyncKeyState(vk)) return false;
    }
    return true;
}

void Logger::Impl::set_config(Config* config) {
    this->config = config;
    update_config();
}

void Logger::Impl::set_overlay(Overlay* overlay) {
    this->overlay = overlay;
}

bool Logger::Impl::get_started() const {
    return started;
}

void Logger::Impl::next_frame() {
    frame_count++;
}