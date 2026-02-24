#include "d3d9rendertargetview.h"
#include "log.h"
#include "globals.h"

#define LOG_MFUN(_, ...) LOG_MFUN_DEF(MyID3D9RenderTargetView, ## __VA_ARGS__)

std::unordered_map<IDirect3DSurface9*, MyID3D9RenderTargetView*> cached_rtvs_map;

class MyID3D9RenderTargetView::Impl {
public:
    IDirect3DSurface9* inner;
    D3DSURFACE_DESC desc;
    IDirect3DResource9* resource;

    Impl(IDirect3DSurface9* inner, const D3DSURFACE_DESC* pDesc, IDirect3DResource9* resource)
        : inner(inner), resource(resource) {
        if (pDesc) {
            desc = *pDesc;
        }
    }
};

MyID3D9RenderTargetView::MyID3D9RenderTargetView(
    IDirect3DSurface9** inner,
    const D3DSURFACE_DESC* pDesc,
    IDirect3DResource9* resource)
    : impl(nullptr) {
    LOG_MFUN(_, LOG_ARG(inner));

    if (!inner || !*inner || !pDesc || !resource) {
        return;
    }

    impl = new Impl(*inner, pDesc, resource);

    cached_rtvs_map.emplace(*inner, this);
    *inner = this;
}

MyID3D9RenderTargetView::~MyID3D9RenderTargetView() {
    LOG_MFUN();

    if (impl) {
        cached_rtvs_map.erase(impl->inner);

        if (impl->desc.Type == D3DRTYPE_SURFACE) {
            auto tex = static_cast<MyID3D9Texture2D*>(impl->resource);
            if (tex) {
                auto resourceView = dynamic_cast<MyID3D9ShaderResourceView*>(this);
                if (resourceView) {
                    tex->get_srvs().erase(resourceView);
                }
            }
        }

        delete impl;
        impl = nullptr;
    }
}
void MyID3D9RenderTargetView::replace_inner(IDirect3DSurface9* new_inner)
{
    if (!impl)
        return;

    if (new_inner == impl->inner)
        return;

    if (new_inner)
        new_inner->AddRef(); // caller expects AddRef inside

    IDirect3DSurface9* old_inner = impl->inner;

    // Keep the cache map consistent with the *current* inner pointer
    if (old_inner)
        cached_rtvs_map.erase(old_inner);

    impl->inner = new_inner;

    if (new_inner)
        cached_rtvs_map.emplace(new_inner, this);

    if (old_inner)
        old_inner->Release();
}

HRESULT STDMETHODCALLTYPE MyID3D9RenderTargetView::GetDevice(IDirect3DDevice9** ppDevice) {
    LOG_MFUN();
    return impl->inner->GetDevice(ppDevice);
}

HRESULT STDMETHODCALLTYPE MyID3D9RenderTargetView::GetDesc(D3DSURFACE_DESC* pDesc) {
    LOG_MFUN();
    if (!pDesc) {
        return E_POINTER;
    }

    *pDesc = impl->desc;
    return S_OK;
}

D3DSURFACE_DESC& MyID3D9RenderTargetView::get_desc() {
    return impl->desc;
}

const D3DSURFACE_DESC& MyID3D9RenderTargetView::get_desc() const {
    return impl->desc;
}

IDirect3DResource9* MyID3D9RenderTargetView::get_resource() const {
    return impl->resource;
}

IDirect3DSurface9*& MyID3D9RenderTargetView::get_inner() {
    return impl->inner;
}
