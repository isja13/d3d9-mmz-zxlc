#include "dxgiswapchain.h"
#include <d3d9.h>
#include <d3dx9.h>
#include "d3d9device.h"
#include "overlay.h"
#include "d3d9texture2d.h"
#include "d3d9rendertargetview.h"
#include "d3d9shaderresourceview.h"
#include "d3d9depthstencilview.h"
#include "conf.h"
#include "log.h"
#include "tex.h"
#include "unknown_impl.h"
#include "FormatConversion.h"
#include "d3d9types.h"
#include "present_parameters_storage.h"
#include "globals.h"
#include <cstdio>

#define LOG_MFUN(_, ...) LOG_MFUN_DEF(MyIDXGISwapChain, ## __VA_ARGS__)

DXGI_FORMAT ConvertD3DFormatToDXGIFormat(D3DFORMAT format) {
    switch (format) {
    case D3DFMT_R8G8B8: return DXGI_FORMAT_B8G8R8X8_UNORM;
    case D3DFMT_A8R8G8B8: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case D3DFMT_X8R8G8B8: return DXGI_FORMAT_B8G8R8X8_UNORM;
    case D3DFMT_R5G6B5: return DXGI_FORMAT_B5G6R5_UNORM;
    case D3DFMT_X1R5G5B5: return DXGI_FORMAT_B5G5R5A1_UNORM;
    case D3DFMT_A1R5G5B5: return DXGI_FORMAT_B5G5R5A1_UNORM;
    case D3DFMT_R3G3B2: return DXGI_FORMAT_R8G8_UNORM;
    case D3DFMT_A8: return DXGI_FORMAT_A8_UNORM;
    case D3DFMT_A8R3G3B2: return DXGI_FORMAT_B8G8R8A8_UNORM;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

D3DFORMAT ConvertDXGIFormatToD3DFormat(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM: return D3DFMT_A8R8G8B8;
    default: return D3DFMT_UNKNOWN;
    }
}

DXGI_FORMAT D3DToDXGIFormat(D3DFORMAT format) {
    switch (format) {
    case D3DFMT_A8R8G8B8: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3DFMT_X8R8G8B8: return DXGI_FORMAT_B8G8R8X8_UNORM;
    case D3DFMT_R5G6B5: return DXGI_FORMAT_B5G6R5_UNORM;
    case D3DFMT_A1R5G5B5: return DXGI_FORMAT_B5G5R5A1_UNORM;
    case D3DFMT_A8: return DXGI_FORMAT_A8_UNORM;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

D3DFORMAT DXGIToD3DFormat(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM: return D3DFMT_A8R8G8B8;
    case DXGI_FORMAT_B8G8R8X8_UNORM: return D3DFMT_X8R8G8B8;
    case DXGI_FORMAT_B5G6R5_UNORM: return D3DFMT_R5G6B5;
    case DXGI_FORMAT_B5G5R5A1_UNORM: return D3DFMT_A1R5G5B5;
    case DXGI_FORMAT_A8_UNORM: return D3DFMT_A8;
    default: return D3DFMT_UNKNOWN;
    }
}

#ifndef E_NOTIMPL
#define E_NOTIMPL ((HRESULT)0x80004001L)
#endif

static __forceinline void LogIfUnsupported(const char* name, HRESULT hr)
{
    if (hr == E_NOTIMPL) {
        char b[256];
        _snprintf(b, sizeof(b), "[UNSUPPORTED] %s returned E_NOTIMPL\n", name);
        OutputDebugStringA(b);
    }
}

#define ZM_LOG_UNSUP(NAME_LIT, HRVAL) LogIfUnsupported((NAME_LIT), (HRVAL))

MyIDXGISwapChain::Impl::Impl(D3DPRESENT_PARAMETERS* pSwapChainDesc, IDirect3DSwapChain9* inner, IDirect3DDevice9* device)
    : inner(inner),
    device(new MyID3D9Device(device)),
    overlay(nullptr),
    cached_width(pSwapChainDesc->BackBufferWidth),
    cached_height(pSwapChainDesc->BackBufferHeight),
    cached_flags(pSwapChainDesc->Flags),
    cached_buffer_count(pSwapChainDesc->BackBufferCount),
    cached_format(pSwapChainDesc->BackBufferFormat),
    display_width(0),
    display_height(0),
    desc(*pSwapChainDesc),
    bbs() 
{
    printf("[SC] Impl::Impl entered. inner=%p dev_in=%p dev_wrap=%p bb=%ux%u fmt=%d count=%u flags=0x%08X hwnd=%p\n",
        (void*)inner,
        (void*)device,
        (void*)this->device,
        (unsigned)pSwapChainDesc->BackBufferWidth,
        (unsigned)pSwapChainDesc->BackBufferHeight,
        (int)pSwapChainDesc->BackBufferFormat,
        (unsigned)pSwapChainDesc->BackBufferCount,
        (unsigned)pSwapChainDesc->Flags,
        (void*)pSwapChainDesc->hDeviceWindow);

    printf("[SC] AddRef inner\n");
    if (this->inner) this->inner->AddRef();
    printf("[SC] AddRef device wrapper\n");
    this->device->AddRef();
    printf("[SC] Impl::Impl done\n");
}

MyIDXGISwapChain::Impl::~Impl() {
    for (auto b : bbs) {
        b->get_sc() = nullptr;
        b->Release();
    }
    bbs.clear();

    if (overlay)
        overlay->set_display(nullptr, nullptr, nullptr);

    if (device)
        device->Release();

    if (inner) {
        inner->Release();
        inner = nullptr;
    }
}


HRESULT MyIDXGISwapChain::Impl::my_resize_buffers(UINT BufferCount, UINT Width, UINT Height, UINT SwapChainFlags, D3DFORMAT NewFormat) {
    HRESULT ret;

    cached_width = Width;
    cached_height = Height;
    cached_flags = SwapChainFlags;
    cached_buffer_count = BufferCount;
    cached_format = NewFormat;

    if (overlay) {
        D3DFORMAT d3dFormat = NewFormat;
        ret = overlay->resize_buffers(BufferCount, Width, Height, d3dFormat, SwapChainFlags);
    }
    else {
        D3DPRESENT_PARAMETERS modifiable_desc = desc;
        modifiable_desc.BackBufferWidth = Width;
        modifiable_desc.BackBufferHeight = Height;
        modifiable_desc.BackBufferFormat = NewFormat;
        modifiable_desc.BackBufferCount = BufferCount;
        modifiable_desc.MultiSampleType = D3DMULTISAMPLE_NONE;
        modifiable_desc.SwapEffect = D3DSWAPEFFECT_DISCARD;
        modifiable_desc.hDeviceWindow = this->desc.hDeviceWindow;
        modifiable_desc.Windowed = this->desc.Windowed;
        modifiable_desc.EnableAutoDepthStencil = FALSE;
        modifiable_desc.Flags = SwapChainFlags;
        ret = device->Reset(&modifiable_desc);

        // Store the presentation parameters
        PresentParametersStorage::SetPresentParameters(modifiable_desc);
    }

    if (ret != S_OK) {
        LOG_MFUN(_, "ResizeBuffers failed with error code: %ld\n", ret);
        return ret;
    }

    PresentParametersStorage::GetPresentParameters(&desc);
    cached_width = desc.BackBufferWidth;
    cached_height = desc.BackBufferHeight;
    cached_format = desc.BackBufferFormat;

    device->resize_buffers(Width, Height);

    return S_OK;
}

void MyIDXGISwapChain::Impl::set_overlay(MyIDXGISwapChain* sc, Overlay* overlay) {
    this->overlay = overlay;

    if (device && overlay)
        device->set_overlay(overlay);

    if (overlay) {
        if (display_width && display_height) {
            D3DPRESENT_PARAMETERS desc = this->desc;
            desc.BackBufferWidth = display_width;
            desc.BackBufferHeight = display_height;
            overlay->set_display(&desc, sc, device);
        }
        else {
            overlay->set_display(&desc, sc, device);
        }
    }
}

MyIDXGISwapChain::MyIDXGISwapChain(D3DPRESENT_PARAMETERS* pSwapChainDesc, IDirect3DSwapChain9* inner, IDirect3DDevice9* device)
    : ref_count(1) {
    printf("[SC] MyIDXGISwapChain ctor. inner=%p device=%p\n", inner, device);
    impl = new Impl(pSwapChainDesc, inner, device);
}

MyIDXGISwapChain::~MyIDXGISwapChain() {
    delete impl;
}

HRESULT MyIDXGISwapChain::SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget) {
    return S_OK;
}

HRESULT MyIDXGISwapChain::GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget) {
    *pFullscreen = FALSE;
    *ppTarget = nullptr;
    return S_OK;
}

HRESULT MyIDXGISwapChain::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, D3DFORMAT NewFormat, UINT SwapChainFlags) {
    return impl->my_resize_buffers(BufferCount, Width, Height, SwapChainFlags, NewFormat);
}

HRESULT MyIDXGISwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters) {
    return S_OK;
}

HRESULT MyIDXGISwapChain::GetContainingOutput(IDXGIOutput** ppOutput) {
    if (!ppOutput) return E_POINTER;
    *ppOutput = nullptr;
    return S_OK;
}

HRESULT MyIDXGISwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats) {
    if (!pStats) return E_POINTER;
    memset(pStats, 0, sizeof(*pStats));
    return S_OK;
}

HRESULT MyIDXGISwapChain::GetLastPresentCount(UINT* pLastPresentCount) {
    if (!pLastPresentCount) return E_POINTER;
    *pLastPresentCount = 0;
    return S_OK;
}

HRESULT MyIDXGISwapChain::GetBackBuffer(
    UINT Buffer,
    D3DBACKBUFFER_TYPE Type,
    IDirect3DSurface9** ppSurface
) {
    HRESULT hr = impl->inner->GetBackBuffer(Buffer, Type, ppSurface);
    ZM_LOG_UNSUP("MyIDXGISwapChain::GetBackBuffer", hr);
    return hr;
}


HRESULT MyIDXGISwapChain::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) {
    if (riid == IID_IDirect3DSurface9) {
        return impl->inner->GetBackBuffer(Buffer, D3DBACKBUFFER_TYPE_MONO,
            reinterpret_cast<IDirect3DSurface9**>(ppSurface));
    }
    return E_NOINTERFACE;
}

ULONG MyIDXGISwapChain::AddRef() {
    return (ULONG)InterlockedIncrement(&ref_count);
}

ULONG MyIDXGISwapChain::Release() {
    ULONG count = (ULONG)InterlockedDecrement(&ref_count);
    if (count == 0) delete this;
    return count;
}

HRESULT MyIDXGISwapChain::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    char b[256];
    sprintf(b, "[SC] QI riid=%08lX-%04X-%04X-....\n",
        (unsigned long)riid.Data1, riid.Data2, riid.Data3);
    OutputDebugStringA(b);

    if (riid == IID_IUnknown) {
        *ppv = static_cast<IUnknown*>(this);
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}


void MyIDXGISwapChain::set_overlay(Overlay* overlay) {
    impl->set_overlay(this, overlay);
}

void MyIDXGISwapChain::set_config(Config* config) {
    // Set the global default_config
    ::default_config = config;
}

std::vector<BackBuffer*>& MyIDXGISwapChain::get_bbs() {
    return impl->bbs;
}
