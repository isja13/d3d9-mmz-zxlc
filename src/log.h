#ifndef LOG_H
#define LOG_H

#include "main.h"
#include <d3d9.h>
#include <d3dx9.h>
#include <d3d9types.h>
#include "d3d9buffer.h"
#include "d3d9shaderresourceview.h"
#include "d3d9rendertargetview.h"
#include "d3d9depthstencilview.h"
#include "conf.h"
#include "overlay.h"
#include "d3d9samplerstate.h"
#include "d3d9texture1d.h"
#include "d3d9texture2d.h"
#include "d3d9device.h"
#include "macros.h"
#include "d3d9vertexshader.h"
#include "d3d9pixelshader.h"
#include "globals.h"

#include "half/include/half.hpp"

#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <type_traits>
#include <tuple>
#include <unordered_map>


#define LOG_ERROR "ERROR"

// Custom Definitions
#define D3DAPPENDELEMENT 0xFFFFFFFF

// Map Definitions
#ifndef FLAG_MAP
#define FLAG_MAP(T) std::unordered_map<T, const char*>
#endif

#ifndef ENUM_MAP
#define ENUM_MAP(T) std::unordered_map<T, const char*>
#endif

// Inline Logging Function
inline void log(const std::string& message) {
    std::cout << message << std::endl;
}

// Logger Class Declaration
class Config;

class Logger;
//extern Logger* default_logger;
//extern Logger dummy_logger; // Declare dummy_logger here

#if ENABLE_LOGGER

// Logger Macros
#define LOG_STARTED ((default_logger) && (default_logger)->get_started())
#define LOG_FUN(_, ...) do { if LOG_STARTED (default_logger)->log_fun(std::string(__FILE__) + ":" + __func__, ## __VA_ARGS__); } while (0)
#define LOG_MFUN_DEF(n, ...) do { if LOG_STARTED (default_logger)->log_fun(std::string(#n "::") + __func__, LOG_ARG(this), ## __VA_ARGS__); } while (0)

#else

#define LOG_STARTED false
#define LOG_FUN(_, ...) (void)0
#define LOG_MFUN_DEF(n, ...) (void)0

#endif

// Struct and Argument Logging Macros
#define LOG_STRUCT_MEMBER(member) #member, (STRUCT)->member
#define LOG_STRUCT_MEMBER_TYPE(member, type, ...) #member, type((STRUCT)->member, ## __VA_ARGS__)
#define LOG_ARG(n) #n, n
#define LOG_ARG_TYPE(n, t, ...) #n, t(n, ## __VA_ARGS__)

#define LOG_RET(ret) default_logger.log_item(ret)

template <typename T>
struct LogItem;

//template <typename T>
//void log_item(const T& item);

template<size_t N>
struct LogSkip {
    const bool skip;
    operator bool() const { return skip; }
    LogSkip<N - 1> operator*() const {
        static_assert(N);
        return { skip };
    }
};

template<size_t N>
struct LogIf : LogSkip<N> {
    template<class T, class = std::enable_if_t<std::is_constructible_v<T, bool>>>
    explicit LogIf(const T& cond) : LogSkip<N>{ !cond } {}
};

template<class T>
struct DefaultLogger {
    const T& a;
    explicit DefaultLogger(const T& a) : a(a) {}
};

template<int L, class T>
struct NumLenLoggerBase {
    T a;
    explicit NumLenLoggerBase(T a) : a(a) {}
};

template<int L, class T>
NumLenLoggerBase<L, T> NumLenLogger(T a) {
    return { a };
}

template<class T>
auto constptr_Logger(T* a) {
    return (const T*)a;
}

template<class T>
struct NumHexLogger {
    T a;
    explicit NumHexLogger(T a) : a(a) {}
};

template<class T>
struct NumBinLogger {
    T a;
    explicit NumBinLogger(T a) {}
};

struct HotkeyLogger {
    std::vector<BYTE>& a;
};

template<class TT, class T>
struct ArrayLoggerBase {
    std::conditional_t<
        std::is_function_v<TT>,
        std::add_pointer_t<TT>,
        TT
    > tt;
    T* a;
    size_t n;
};

template<class T>
T& deref_ftor(T* a) {
    return *a;
}

template<class T>
T* ref_ftor(T* a) {
    return a;
}

template<class T, class... Ts>
T ctor_ftor_base(Ts&&... as) {
    return T(std::forward<T>(as)...);
}

template<class T, auto TF, class TT, class... Ts>
struct ctor_ftor {
    const std::tuple<Ts&&...> as;
    T operator()(TT* a) const {
        return std::apply(
            ctor_ftor_base<T, TT, Ts...>,
            std::tuple_cat(std::make_tuple(TF(a)), as)
        );
    }
};

template<class T, class TT, class... Ts>
using ctor_defer_ftor = ctor_ftor<T, deref_ftor<TT>, TT, Ts...>;

template<class T>
ArrayLoggerBase<decltype(deref_ftor<T>), T> ArrayLoggerDeref(T* a, size_t n) {
    return { deref_ftor<T>, a, n };
}

template<class T>
ArrayLoggerBase<decltype(ref_ftor<T>), T> ArrayLoggerRef(T* a, size_t n) {
    return { ref_ftor<T>, a, n };
}

template<class T, class TT, class... Ts>
ArrayLoggerBase<ctor_defer_ftor<T, TT>, TT> ArrayLoggerDeref(TT* a, size_t n, Ts&&... as) {
    return { {std::forward_as_tuple(as...)}, a, n };
}

template<template<class> class T, class TT, class... Ts>
auto ArrayLoggerDeref(TT* a, size_t n, Ts&&... as) {
    return ArrayLoggerDeref<decltype(T(*a))>(a, n, std::forward<Ts>(as)...);
}

struct StringLogger {
    LPCSTR a;
    explicit StringLogger(LPCSTR a) : a(a) {}
};

struct RawStringLogger {
    LPCSTR a;
    LPCSTR p;
    RawStringLogger(LPCSTR a, LPCSTR p) : a(a), p(p) {}
};

struct ByteArrayLogger {
    const char* b;
    UINT n;
    ByteArrayLogger(const void* b, UINT n) : b(static_cast<const char*>(b)), n(n) {}
};

struct CharLogger {
    CHAR a;
    explicit CharLogger(CHAR a) : a(a) {}
};

struct ShaderLogger {
    const void* a;
    std::string source;
    DWORD lang = 0;
    explicit ShaderLogger(const void* a);
};

struct D3D9_CLEAR_Logger {
    DWORD a;
    explicit D3D9_CLEAR_Logger(DWORD a) : a(a) {}
};

struct D3D9_BIND_Logger {
    DWORD a;
    explicit D3D9_BIND_Logger(DWORD a) : a(a) {}
};

struct D3D9_CPU_ACCESS_Logger {
    DWORD a;
    explicit D3D9_CPU_ACCESS_Logger(DWORD a) : a(a) {}
};

struct D3D9_RESOURCE_MISC_Logger {
    DWORD a;
    explicit D3D9_RESOURCE_MISC_Logger(DWORD a) : a(a) {}
};

struct D3D9_SUBRESOURCE_DATA {
    const void* pSysMem;
    UINT SysMemPitch;
    UINT SysMemSlicePitch;
};

struct D3D9_SUBRESOURCE_DATA_Logger {
    const D3D9_SUBRESOURCE_DATA* pInitialData;
    UINT ByteWidth;
    explicit D3D9_SUBRESOURCE_DATA_Logger(const D3D9_SUBRESOURCE_DATA* p, UINT n) : pInitialData(p), ByteWidth(n) {}
};

struct ID3D9Resource_id_Logger {
    UINT64 id;
    explicit ID3D9Resource_id_Logger(UINT64 id) : id(id) {}
};

struct MyID3D9Resource_Logger {
    const IDirect3DResource9* r;
    explicit MyID3D9Resource_Logger(const IDirect3DResource9* r) : r(r) {}
};

struct MyVertexBuffer_Logger {
    const D3DVERTEXELEMENT9* input_layout;
    const IDirect3DVertexBuffer9* vertex_buffer;
    UINT stride;
    UINT offset;
    UINT VertexCount;
    UINT StartVertexLocation;
};

struct MyIndexedVertexBuffer_Logger {
    const D3DVERTEXELEMENT9* input_layout;
    const IDirect3DVertexBuffer9* vertex_buffer;
    const IDirect3DIndexBuffer9* index_buffer;
    UINT stride;
    UINT offset;
    UINT IndexCount;
    UINT StartIndexLocation;
    UINT BaseVertexLocation;
    DWORD index_format;
    UINT index_offset;
};


namespace {
    template<class = void, class...>
    struct DXGIBuffer_Logger;

    template<>
    struct DXGIBuffer_Logger<> {
        const char*& buffer;
    };

    template<class T, class... Ts>
    struct DXGIBuffer_Logger : DXGIBuffer_Logger<Ts...> {
        using DXGIBuffer_Logger<Ts...>::buffer;
    };

    struct DXGIBufferType {
        template<size_t N>
        struct Typeless;

        template<size_t N>
        struct Unused;

        template<size_t N>
        struct SInt;

        template<size_t N>
        struct UInt;

        template<size_t N>
        struct SNorm;

        template<size_t N>
        struct UNorm;

        struct Float;
        struct Half;
    };
}


// Define Float and UInt types for logging purposes
struct Float {
    float value;
    explicit Float(float v) : value(v) {}
};

template<int N>
struct UInt {
    unsigned int value;
    explicit UInt(unsigned int v) : value(v) {}
};

class Logger {
    class Impl;
    Impl* impl;
    std::ostream& oss;

    bool log_begin();
    void log_end();

    void log_assign() const;
    void log_sep() const;
    void log_fun_name(LPCSTR n) const;
    void log_fun_begin() const;
    template<class T>
    void log_fun_arg(LPCSTR n, T&& v) const {
        log_item(n);
        log_assign();
        log_item(std::forward<T>(v));
    }
    void log_fun_args() const;
    template <class T>
    void log_fun_args(T&& r) const {
        log_fun_end(std::forward<T>(r));
    }
    template<class T, class... Ts>
    void log_fun_args(LPCSTR n, T&& v, Ts&&... as) const {
        log_fun_arg(n, std::forward<T>(v));
        log_fun_args_next(std::forward<Ts>(as)...);
    }
    void log_fun_args_next() const;
    template<class T>
    void log_fun_args_next(T&& r) const {
        log_fun_args(std::forward<T>(r));
    }
    template<class... Ts>
    void log_fun_args_next(LPCSTR n, Ts&&... as) const {
        log_fun_sep();
        log_fun_args(n, std::forward<Ts>(as)...);
    }

    template<size_t N, class T, class... Ts, class = std::enable_if_t<N>>
    void log_fun_args(const LogSkip<N> skip, LPCSTR n, T&& v, Ts&&... as) const {
        if (skip)
            log_fun_args(*skip, std::forward<Ts>(as)...);
        else
            log_fun_args(n, std::forward<T>(v), *skip, std::forward<Ts>(as)...);
    }
    template<class... Ts>
    void log_fun_args(const LogSkip<0> skip, Ts&&... as) const {
        log_fun_args(std::forward<Ts>(as)...);
    }
    template<size_t N, class T, class... Ts, class = std::enable_if_t<N>>
    void log_fun_args_next(const LogSkip<N> skip, LPCSTR n, T&& v, Ts&&... as) const {
        if (skip)
            log_fun_args_next(*skip, std::forward<Ts>(as)...);
        else
            log_fun_args_next(n, std::forward<T>(v), *skip, std::forward<Ts>(as)...);
    }
    template<class... Ts>
    void log_fun_args_next(const LogSkip<0> skip, Ts&&... as) const {
        log_fun_args_next(std::forward<Ts>(as)...);
    }

    void log_fun_sep() const;
    void log_fun_end() const;
    template<class T>
    void log_fun_ret(T&& v) const {
        log_assign();
        log_item(std::forward<T>(v));
    }
    template<class... Ts>
    void log_fun_name_begin(LPCSTR n, Ts&&... as) const {
        log_fun_name(n);
        log_fun_begin();
        log_fun_args(std::forward<Ts>(as)...);
    }
    template<class T>
    void log_fun_end(T r) const {
        log_fun_end();
        log_fun_ret(r);
    }
    void log_struct_begin() const;
    void log_struct_member_access() const;
    void log_struct_sep() const;
    void log_struct_end() const;
    void log_array_begin() const;
    void log_array_sep() const;
    void log_array_end() const;
    void log_string_begin() const;
    void log_string_end() const;
    void log_char_begin() const;
    void log_char_end() const;
    void log_null() const;

    template<class T>
    std::enable_if_t<std::is_arithmetic_v<T>> log_item(T a) const {
        oss << +a;
    }

    void log_item(bool a) const;
    void log_item(CHAR a) const;
    void log_item(WCHAR a) const;
    void log_item(LPCSTR a) const;
    void log_item(LPCWSTR a) const;
    void log_item(const std::string& a) const;

   // void log_items_base() const;
  //  template<class T, class... Ts>
  //  void log_items_base(T a, Ts... as) const {
   //     log_item(a);
   //     log_items_base(as...);
  //  }

    template<class T>
    void log_item(T* a) const {
        log_item((const T*)a);
    }

    template<class T>
    void log_item(const T* a) const {
        if (a) log_item(NumHexLogger(a));
        else log_null();
    }

    template<int L, class T>
    void log_item(NumLenLoggerBase<L, T> a) const {
        oss << std::setfill('0') << std::setw(L) << +a.a;
        oss.flags(std::ios::fmtflags{});
    }

    template<class T>
    void log_item(NumHexLogger<T> a) const {
        oss << std::hex << std::showbase << +a.a;
        oss.flags(std::ios::fmtflags{});
    }

    template<class T>
    void log_item(NumBinLogger<T> a) const {
        auto b = (LPBYTE)&a.a;
        UINT n = sizeof(a.a);
        UINT bits = std::numeric_limits<BYTE>::digits;
        BYTE m = (BYTE)1 << (bits - 1);
        oss << "0b";
        for (UINT i = n; i > 0;) {
            --i;
            BYTE c = b[i];
            for (UINT j = 0; j < bits; ++j) {
                oss << (c & m ? "1" : "0");
                c <<= 1;
            }
        }
    }

    template<class TT, class T>
    void log_item(ArrayLoggerBase<TT, T> a) const {
        if (!a.a) { log_null(); return; }
        log_array_begin();
        bool first = true;
        for (T* t = a.a; t != a.a + a.n; ++t) {
            if (first) {
                first = false;
            }
            else {
                log_array_sep();
            }
            log_item(a.tt(t));
        }
        log_array_end();
    }

    template<class T>
    void log_enum(const ENUM_MAP(T)& map, T a, bool hex = false) const {
        typename ENUM_MAP(T)::const_iterator it = map.find(a);
        if (it != map.end()) {
            log_item(it->second);
        }
        else {
            if constexpr (std::is_enum_v<T>) {
                auto v = std::underlying_type_t<T>(a);
                if (hex) {
                    log_item(NumHexLogger(v));
                }
                else {
                    log_item(+v);
                }
            }
            else {
                if (hex) {
                    log_item(NumHexLogger(a));
                }
                else {
                    log_item(+a);
                }
            }
        }
    }

   // void log_flag_sep() const;
   // template<typename T>
   // void log_flag(const FLAG_MAP(T)& map, T a) const {
    //    T t = 0;
   //     bool first = true;
    //    for (const auto& v : map) {
    //        if (a & v.first) {
    //            if (first) {
    //                first = false;
 //               }
//                else {
 //                   log_flag_sep();
 //               }
 //               log_item(v.second);
   //             t |= v.first;
  //          }
  //      }
  //      t = a & ~t;
   //     if (first) {
  //          log_item(NumHexLogger(t));
 //       }
   //     else {
    //        if (t) {
 //               log_flag_sep();
 //               log_item(NumHexLogger(t));
 //           }
 //       }
 //   }

    void log_item(StringLogger a) const;
    void log_item(RawStringLogger a) const;
    void log_item(ByteArrayLogger a) const;
    void log_item(CharLogger a) const;
    void log_item(const ShaderLogger& a) const;
    void log_item(D3D9_CLEAR_Logger a) const;
    void log_item(D3D9_BIND_Logger a) const;
    void log_item(D3D9_CPU_ACCESS_Logger a) const;
    void log_item(D3D9_RESOURCE_MISC_Logger a) const;
    void log_item(D3D9_SUBRESOURCE_DATA_Logger a) const;
  //  void log_item(HotkeyLogger a) const;
    void log_item(const GUID* guid) const;
    void log_item(const D3DSAMPLER_DESC* sampler_desc) const;

    void log_item(PIXEL_SHADER_ALPHA_DISCARD item) const; // Declaration

    void log_item(D3DLOCKED_BOX a) const;
    void log_item(D3DLOCKED_RECT a) const;
    void log_item(D3DDECLUSAGE a);
    void log_item(D3DPRIMITIVETYPE a) const;
    void log_item(const MyID3D9ShaderResourceView* a) const;
    void log_item(const MyID3D9RenderTargetView* a) const;
    void log_item(const MyID3D9DepthStencilView* a) const;
    void log_item(MyID3D9Resource_Logger a) const;
    void log_item(const D3DVERTEXELEMENT9& item);
    void log_item(const D3DVERTEXBUFFER_DESC& item);
    void log_item(DWORD a);

    void log_item(PIXEL_SHADER_ALPHA_DISCARD item);


  //  void log_struct_members() const;
  //  template<class T>
  //  void log_struct_members(T&& a) const {
  //      log_item(std::forward<T>(a));
  //  }
    template<class T, class TT, class... Ts>
    void log_struct_members(T&& a, TT&& aa, Ts&&... as) const {
        log_struct_members(std::forward<T>(a));
        log_struct_sep();
        log_struct_members(std::forward<TT>(aa), std::forward<Ts>(as)...);
    }
    template<class... Ts>
    void log_struct(Ts&&... as) const {
        log_struct_begin();
        log_struct_members(std::forward<Ts>(as)...);
        log_struct_end();
    }
  //  void log_struct_members_named() const;
  //  template<class T>
    template<class T>
    void log_struct_members_named(LPCSTR n, const T& a) const {
        log_item(n);
        log_assign();
        log_item(a);
    }
    template<class T, class TT, class... Ts>
    void log_struct_members_named(LPCSTR n, const T& a, LPCSTR nn, const TT& aa, const Ts&... as) const {
        log_struct_members_named(n, a);
        log_struct_sep();
        log_struct_members_named(nn, aa, as...);
    }

    template<class... Ts>
    void log_struct_named(Ts&&... as) const {
        log_struct_begin();
        log_struct_members_named(std::forward<Ts>(as)...);
        log_struct_end();
    }

public:
    Logger(LPCTSTR file_name, Config* config = nullptr, Overlay* overlay = nullptr);
    ~Logger();

    void set_overlay(Overlay* overlay);
    void set_config(Config* config);

    bool get_started() const;
    void next_frame();

   // template<class... Ts>
   // void log_fun(std::string&& n, Ts&&... as);

    public:
        void log_error(const char* msg) {
            log_item("LOG_ERROR");
            log_item(msg);
        }

        void log_error(const char* msg, const char* detail) {
            log_item("LOG_ERROR");
            log_item(msg);
            log_item(detail);
        }


private:
    template<class T> friend struct LogItem;
   // template<class T> friend void log_item(const T& item);

  //  template<class T>
   // void log_item(LogItem<T>&& a);

    template<class, class = void>
    struct LogItem_Ptr : std::false_type {};
    template<class T>
    struct LogItem_Ptr <
        T,
        decltype(LogItem<T>{std::declval<const T*>()}, (void)0)
    > : std::true_type {};
    template<class... Ts>
    static constexpr bool LogItem_Ptr_v = LogItem_Ptr<Ts...>::value;

    template<class T>
    using bare_t = std::remove_cv_t<std::remove_reference_t<T>>;

    template<class, class = void>
    struct LogItem_RRef : std::false_type {};
    template<class T>
    struct LogItem_RRef <
        T,
        decltype(LogItem<bare_t<T>>{std::declval<T&&>()}, (void)0)
    > : std::true_type {};
    template<class... Ts>
    static constexpr bool LogItem_RRef_v = LogItem_RRef<Ts...>::value;

    template<class, class = void>
    struct LogItem_Ref : std::false_type {};
    template<class T>
    struct LogItem_Ref <
        T,
        decltype(LogItem<bare_t<T>>{std::declval<T&>()}, (void)0)
    > : std::true_type {};
    template<class... Ts>
    static constexpr bool LogItem_Ref_v = !LogItem_RRef_v<Ts...> && LogItem_Ref<Ts...>::value;

   // template<class T>
    //auto log_item(T&& a) -> std::enable_if_t<LogItem_RRef_v<T>>;

  //  template<class T>
  //  auto log_item(T&& a) -> std::enable_if_t<LogItem_Ref_v<T>>;

    template<class, class = void>
    struct LogItem_impl : std::false_type {};
    template<class T>
    struct LogItem_impl < T, std::void_t<decltype(LogItem<bare_t<T>>{std::declval<T>()}) >> : std::true_type {};
    template<class... Ts>
    static constexpr bool LogItem_impl_v = LogItem_impl<Ts...>::value;

  //  template<
   //     class T,
    //    class = std::enable_if_t<std::is_class_v<T> && !LogItem_impl_v<T>>
  //  >
   // auto log_item(const T& a) -> decltype(log_item(&a));
};


template<class... Ts>
struct LogItem<DXGIBuffer_Logger<Ts...>> : DXGIBufferType {
    DXGIBuffer_Logger<Ts...>& t;
    LogItem(DXGIBuffer_Logger<Ts...>& t) : t(t) {}

    static constexpr bool comp_size(size_t n) {
        return n && n <= 32 && !(n % 8);
    }

    template<size_t N, class... As>
    static void log_comp(Logger* l, DXGIBuffer_Logger<Typeless<N>, As...>& t) {
        static_assert(comp_size(N));
        UINT32 v = 0;
        for (size_t i = 0; i < N; i += 8)
            v |= (UINT32) * (UINT8*)t.buffer++ << i;
        l->log_item(NumHexLogger(v));
    }

    template<size_t N, class... As>
    static void log_comp(Logger* l, DXGIBuffer_Logger<SInt<N>, As...>& t) {
        static_assert(comp_size(N));
        UINT32 v = 0;
        for (size_t i = 0; i < N; i += 8)
            v |= (UINT32) * (UINT8*)t.buffer++ << i;
        v <<= (32 - N);
        auto s = (INT32)v;
        s >>= (32 - N);
        l->log_item(s);
    }

    template<size_t N, class... As>
    static void log_comp(Logger* l, DXGIBuffer_Logger<UInt<N>, As...>& t) {
        static_assert(comp_size(N));
        UINT32 v = 0;
        for (size_t i = 0; i < N; i += 8)
            v |= (UINT32) * (UINT8*)t.buffer++ << i;
        l->log_item(v);
    }

    template<size_t N, class... As>
    static void log_comp(Logger* l, DXGIBuffer_Logger<SNorm<N>, As...>& t) {
        static_assert(comp_size(N));
        UINT32 v = 0;
        for (size_t i = 0; i < N; i += 8)
            v |= (UINT32) * (UINT8*)t.buffer++ << i;
        v <<= (32 - N);
        auto s = (INT32)v;
        s >>= (32 - N);
        auto n = std::max((double)s / (((UINT32)1 << (N - 1)) - 1), -1.);
        l->log_item(n);
    }

    template<size_t N, class... As>
    static void log_comp(Logger* l, DXGIBuffer_Logger<UNorm<N>, As...>& t) {
        static_assert(comp_size(N));
        UINT32 v = 0;
        for (size_t i = 0; i < N; i += 8)
            v |= (UINT32) * (UINT8*)t.buffer++ << i;
        auto n = (double)v / (((UINT64)1 << N) - 1);
        l->log_item(n);
    }

    template<class... As>
    static void log_comp(Logger* l, DXGIBuffer_Logger<Float, As...>& t) {
        auto f = *(float*)t.buffer;
        t.buffer += sizeof(f);
        l->log_item(f);
    }

    template<class... As>
    static void log_comp(Logger* l, DXGIBuffer_Logger<Half, As...>& t) {
        auto f = *(half_float::half*)t.buffer;
        t.buffer += sizeof(f);
        l->log_item((float)f);
    }

    template<class A, class... As>
    void log_comps(Logger* l, DXGIBuffer_Logger<A, As...>& t) {
        log_comp(l, t);
        log_comps_next(l, *(DXGIBuffer_Logger<As...>*) & t);
    }
    template<class A, class... As>
    void log_comps_next(Logger* l, DXGIBuffer_Logger<A, As...>& t) {
        l->log_array_sep();
        log_comps(l, t);
    }
    template<size_t N, class... As>
    void log_comps_next(Logger* l, DXGIBuffer_Logger<Unused<N>, As...>& t) {
        static_assert(comp_size(N));
        t.buffer += N / 8;
        log_comps_next(l, *(DXGIBuffer_Logger<As...>*) & t);
    }

    void log_comps(Logger* l, DXGIBuffer_Logger<>& t) {}
    void log_comps_next(Logger* l, DXGIBuffer_Logger<>& t) {
        log_comps(l, t);
    }

    void log_item(Logger* l) {
        l->log_array_begin();
        log_comps(l, t);
        l->log_array_end();
    }
};

#endif // LOG_H
