#ifndef MAIN_H
#define MAIN_H

#define ENABLE_SLANG_SHADER 1

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN

#include <sdkddkver.h>  // let MinGW/Windows headers decide defaults

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Win7 (matches MinGW's 0x601 in your log)
#endif

#ifndef WINVER
#define WINVER _WIN32_WINNT
#endif

#include <windows.h>
#include <tchar.h>
#include <d3d9.h>
#include <dinput.h>

//#ifdef UNICODE
//#define _T(x) L ## x
//#else
//#define _T(x) x
//#endif

#ifdef UNICODE
#define BASE_DLL_NAME L"\\dinput8.dll"
#else
#define BASE_DLL_NAME "\\dinput8.dll"
#endif

#define VK_VALUE_BEGIN 1
#define VK_VALUE_END ((BYTE)-1)

#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <deque>
#include <string>
#include <string_view>
#define _tstring std::basic_string<TCHAR>
#define _tstring_view std::basic_string_view<TCHAR>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <type_traits>
#include <limits>
#include <algorithm>
#include <codecvt>
#include <tuple>
#include <atomic>
#include <regex>

#include "custom_query_type.h"

#define CONCAT_BASE(a, b) a ## b
#define CONCAT(a, b) CONCAT_BASE(a, b)

#ifndef STRINGIFY
#define STRINGIFY_BASE(n) #n
#define STRINGIFY(n) STRINGIFY_BASE(n)
#endif

#ifndef LSTRINGIFY
#define LSTRINGIFY(x) L ## #x
#endif

struct _tstring_view_icmp {
    bool operator()(const _tstring_view& a, const _tstring_view& b) const;
};

#define MAP_ENUM(t) std::map<_tstring_view, t, _tstring_view_icmp>

#define MAP_ENUM_ITEM(n) { _T(#n), n }

#ifndef ENUM_MAP
#define ENUM_MAP(t) std::map<t, std::string>
#endif

#ifndef ENUM_MAP_ITEM
#define ENUM_MAP_ITEM(n) { n, #n }
#endif

#define ENUM_CLASS_MAP_ITEM(n) { ENUM_CLASS::n, #n }
#ifndef FLAG_MAP
#define FLAG_MAP(t) std::vector<std::pair<t, std::string>>
#endif

#define MOD_NAME "MMZZXLC FilterHack"
#define LOG_FILE_NAME _T("interp-mod.log")
#define INI_FILE_NAME _T("filter-hack.ini")

inline void zm_notimpl(const char* func) {
    char b[512];
    _snprintf(b, sizeof(b), "[ZeroMod] NOTIMPL HIT: %s\n", func);
    b[sizeof(b) - 1] = '\0';
    OutputDebugStringA(b);
}

#define ZM_NOTIMPL_RET() do { zm_notimpl(__FUNCTION__); return E_NOTIMPL; } while(0)
#define ZM_NOTIMPL_RET_NAMED(name) do { zm_notimpl(name); return E_NOTIMPL; } while(0)

inline void zm_trace_hr(const char* tag, HRESULT hr) {
    if (hr != D3D_OK) {
        char b[512];
        _snprintf(b, sizeof(b), "[ZeroMod] HR %s hr=0x%08lX\n", tag, (unsigned long)hr);
        b[sizeof(b) - 1] = '\0';
        OutputDebugStringA(b);
    }
}



class Overlay;
//class CustomQueryType;


class cs_wrapper {
    class Impl;
    Impl* impl;

public:
    cs_wrapper();
    ~cs_wrapper();
    void begin_cs();
    bool try_begin_cs();
    void end_cs();
};

#endif // MAIN_H
