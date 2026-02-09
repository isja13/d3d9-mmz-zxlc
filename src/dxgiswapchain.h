#ifndef DXGISWAPCHAIN_H
#define DXGISWAPCHAIN_H

#include <windows.h>   // <-- required for DXGI headers to behave consistently
#include <unknwn.h>    // <-- IUnknown/REFIID etc. (belt + suspenders)

#include <vector>
#include <unordered_set>


#include <d3d9.h>
#include <d3dx9.h>
#include <d3d9types.h>
#include <dxgi.h>
#include <dxgitype.h>
#include "overlay.h"
#include "conf.h"
#include "backbuffer.h"
#include "globals.h"


// Forward declarations
class MyID3D9Device;

class MyIDXGISwapChain : public IUnknown {
public:
    MyIDXGISwapChain(D3DPRESENT_PARAMETERS* pSwapChainDesc, IDirect3DSwapChain9* inner, IDirect3DDevice9* device);

    HRESULT SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget);
    HRESULT GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget);
    HRESULT ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, D3DFORMAT NewFormat, UINT SwapChainFlags);
    HRESULT ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters);
    HRESULT GetContainingOutput(IDXGIOutput** ppOutput);
    HRESULT GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats);
    HRESULT GetLastPresentCount(UINT* pLastPresentCount);

    HRESULT GetBackBuffer(UINT Buffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppSurface);
    HRESULT GetBuffer(UINT Buffer, REFIID riid, void** ppSurface);

    virtual ~MyIDXGISwapChain();

    void set_overlay(Overlay* overlay);
    void set_config(Config* config);

    std::vector<BackBuffer*>& get_bbs();

    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);

    IDirect3DSwapChain9* get_inner() { return impl->inner; }

private:
    volatile LONG ref_count; // Add this line

    struct Impl {
        IDirect3DSwapChain9* inner;
        MyID3D9Device* device;
        Overlay* overlay;
      //Config* default_config;
        UINT cached_width;
        UINT cached_height;
        UINT cached_flags;
        UINT cached_buffer_count;
        D3DFORMAT cached_format;
        UINT display_width;
        UINT display_height;
        D3DPRESENT_PARAMETERS desc;
        std::vector<BackBuffer*> bbs;

        Impl(D3DPRESENT_PARAMETERS* pSwapChainDesc, IDirect3DSwapChain9* inner, IDirect3DDevice9* device);
        ~Impl();

        HRESULT my_resize_buffers(UINT BufferCount, UINT Width, UINT Height, UINT SwapChainFlags, D3DFORMAT NewFormat);

        void set_overlay(MyIDXGISwapChain* sc, Overlay* overlay);
    };

    Impl* impl;
};

#endif // DXGISWAPCHAIN_H
