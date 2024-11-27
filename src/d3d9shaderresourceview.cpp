#include "d3d9shaderresourceview.h"
#include "log.h"
#include "globals.h"

//#define LOGGER default_logger
#define LOG_MFUN(_, ...) LOG_MFUN_DEF(MyID3D9ShaderResourceView, ## __VA_ARGS__)

std::unordered_map<IDirect3DResource9*, MyID3D9ShaderResourceView*> cached_srvs_map;

class MyID3D9ShaderResourceView::Impl {
public:
    IDirect3DResource9* inner;
    D3DSURFACE_DESC desc;
    IDirect3DResource9* resource;

    Impl(IDirect3DResource9* inner, const D3DSURFACE_DESC* pDesc, IDirect3DResource9* resource)
        : inner(inner), resource(resource) {
        if (pDesc) {
            desc = *pDesc;
        }
    }
};

MyID3D9ShaderResourceView::MyID3D9ShaderResourceView(
    IDirect3DResource9** inner,
    const D3DSURFACE_DESC* pDesc,
    IDirect3DResource9* resource)
    : impl(nullptr) {
    LOG_MFUN(_, LOG_ARG(inner));

    if (!inner || !*inner || !pDesc || !resource) {
        return;
    }

    impl = new Impl(*inner, pDesc, resource);

    cached_srvs_map.emplace(*inner, this);
    *inner = this;
}

MyID3D9ShaderResourceView::~MyID3D9ShaderResourceView() {
    LOG_MFUN();

    if (impl) {
        cached_srvs_map.erase(impl->inner);
        delete impl;
        impl = nullptr;
    }
}

HRESULT STDMETHODCALLTYPE MyID3D9ShaderResourceView::GetDevice(IDirect3DDevice9** ppDevice) {
    LOG_MFUN();
    return impl->inner->GetDevice(ppDevice);
}

HRESULT STDMETHODCALLTYPE MyID3D9ShaderResourceView::GetDesc(D3DSURFACE_DESC* pDesc) {
    LOG_MFUN();
    if (!pDesc) {
        return E_POINTER;
    }

    *pDesc = impl->desc;
    return S_OK;
}

D3DSURFACE_DESC& MyID3D9ShaderResourceView::get_desc() {
    return impl->desc;
}

const D3DSURFACE_DESC& MyID3D9ShaderResourceView::get_desc() const {
    return impl->desc;
}

IDirect3DResource9* MyID3D9ShaderResourceView::get_resource() const {
    return impl->resource;
}

IDirect3DResource9*& MyID3D9ShaderResourceView::get_inner() {
    return impl->inner;
}
