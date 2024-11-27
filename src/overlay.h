#ifndef OVERLAY_H
#define OVERLAY_H

#include <string>
#include <type_traits>
#include <d3d9.h>
#include <d3dx9.h>
#include "main.h"
#include "dxgiswapchain.h"
#include <locale>
#include <codecvt>

// Forward declarations
class MyIDXGISwapChain;
class MyID3D9Device;

class Overlay {
    class Impl;
    Impl* impl;

    void push_text_base(std::string&& s);
    template<class T, class... Ts>
    std::enable_if_t<std::is_convertible_v<T, std::string>> push_text_base(std::string&& s, T&& a, Ts&&... as) {
        s += std::string(std::forward<T>(a));
        push_text_base(std::move(s), std::forward<Ts>(as)...);
    }
    template<class T, class... Ts>
    std::enable_if_t<std::is_convertible_v<T, std::wstring>> push_text_base(std::string&& s, T&& a, Ts&&... as) {
        push_text_base(
            std::move(s),
            std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(std::wstring(std::forward<T>(a))),
            std::forward<Ts>(as)...
        );
    }

public:
    Overlay();
    ~Overlay();

    void set_display(
        D3DPRESENT_PARAMETERS* pSwapChainDesc,
        MyIDXGISwapChain* pSwapChain,
        MyID3D9Device* pDevice
    );

    HRESULT present(
        UINT SyncInterval,
        UINT Flags
    );

    HRESULT resize_buffers(
        UINT buffer_count,
        UINT width,
        UINT height,
        D3DFORMAT format,
        UINT flags
    );

    template<class... Ts>
    void push_text(Ts&&... as) {
        std::string s;
        push_text_base(std::move(s), std::forward<Ts>(as)...);
    }

    void set_log_message(const std::string& message);
};

struct OverlayPtr {
    Overlay* overlay;
    OverlayPtr(Overlay* overlay = NULL) : overlay(overlay) {}
    template<class... As>
    void operator()(const As&... as) const {
        if (overlay) overlay->push_text(as...);
    }
    OverlayPtr& operator=(Overlay* overlay) {
        this->overlay = overlay;
        return *this;
    }
    Overlay* operator->() const { return overlay; }
    operator Overlay* () const { return overlay; }
};

#endif // OVERLAY_H

