#include "d3d9depthstencilview.h"
#include "log.h"
#include "globals.h"

#define LOG_MFUN(_, ...) LOG_MFUN_DEF(MyID3D9DepthStencilView, ## __VA_ARGS__)

class MyIDirect3DSurface9::Impl {
public:
    IDirect3DSurface9* inner;

    explicit Impl(IDirect3DSurface9* in) : inner(in) {}
};

MyIDirect3DSurface9::MyIDirect3DSurface9(IDirect3DSurface9** inner)
    : impl(nullptr)
{
    if (!inner || !*inner)
        return;

    impl = new Impl(*inner);
    *inner = this;
}

MyIDirect3DSurface9::~MyIDirect3DSurface9()
{
    if (impl) {
        if (impl->inner)
            impl->inner->Release();
        delete impl;
        impl = nullptr;
    }
}

HRESULT MyIDirect3DSurface9::GetDevice(IDirect3DDevice9** ppDevice)
{
    return impl && impl->inner ? impl->inner->GetDevice(ppDevice) : D3DERR_INVALIDCALL;
}

IDirect3DSurface9*& MyIDirect3DSurface9::get_inner()
{
    return impl->inner;
}

void MyIDirect3DSurface9::replace_inner(IDirect3DSurface9* new_inner)
{
    if (!impl || !new_inner)
        return;

    if (impl->inner == new_inner)
        return;

    new_inner->AddRef();
    if (impl->inner)
        impl->inner->Release();
    impl->inner = new_inner;
}

IDirect3DResource9* MyIDirect3DSurface9::get_resource()
{
    return nullptr;
}

// ---- DSV ----

MyID3D9DepthStencilView::MyID3D9DepthStencilView(IDirect3DSurface9** inner)
    : MyIDirect3DSurface9(inner)
{
    LOG_MFUN(_, LOG_ARG(inner));
}

MyID3D9DepthStencilView::~MyID3D9DepthStencilView()
{
    LOG_MFUN();
}

HRESULT MyID3D9DepthStencilView::GetDevice(IDirect3DDevice9** ppDevice)
{
    // Use the base inner
    return get_inner() ? get_inner()->GetDevice(ppDevice) : D3DERR_INVALIDCALL;
}
