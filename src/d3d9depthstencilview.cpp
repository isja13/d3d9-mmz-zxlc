#include "d3d9depthstencilview.h"
#include "log.h"
#include "globals.h"

//#define LOGGER default_logger
#define LOG_MFUN(_, ...) LOG_MFUN_DEF(MyID3D9DepthStencilView, ## __VA_ARGS__)

class MyIDirect3DSurface9::Impl {
public:
    IDirect3DSurface9* inner;

    Impl(IDirect3DSurface9* inner) : inner(inner) {}

    ~Impl() {
        inner = nullptr;
    }
};

MyIDirect3DSurface9::MyIDirect3DSurface9(IDirect3DSurface9** inner)
    : impl(new Impl(*inner)) {
    *inner = this; // Assign `this` to `inner`
}

MyIDirect3DSurface9::~MyIDirect3DSurface9() {
    delete impl;
}

HRESULT MyIDirect3DSurface9::GetDevice(IDirect3DDevice9** ppDevice) {
    return impl->inner->GetDevice(ppDevice);
}

IDirect3DSurface9*& MyIDirect3DSurface9::get_inner() {
    return impl->inner;
}

IDirect3DResource9* MyIDirect3DSurface9::get_resource() {
    // Implement this method as needed
    return nullptr;
}

class MyID3D9DepthStencilView::Impl {
public:
    MyIDirect3DSurface9* inner;

    Impl(MyIDirect3DSurface9* inner) : inner(inner) {}

    ~Impl() {
        inner = nullptr;
    }
};

MyID3D9DepthStencilView::MyID3D9DepthStencilView(MyIDirect3DSurface9** inner)
    : MyIDirect3DSurface9(reinterpret_cast<IDirect3DSurface9**>(inner)), impl(new Impl(static_cast<MyIDirect3DSurface9*>(*inner))) {
    *inner = this; // Properly cast `this` to `MyIDirect3DSurface9*`
}

MyID3D9DepthStencilView::~MyID3D9DepthStencilView() {
    delete impl;
}

HRESULT MyID3D9DepthStencilView::GetDevice(IDirect3DDevice9** ppDevice) {
    return impl->inner->GetDevice(ppDevice);
}

MyIDirect3DSurface9*& MyID3D9DepthStencilView::get_inner() {
    return impl->inner;
}
