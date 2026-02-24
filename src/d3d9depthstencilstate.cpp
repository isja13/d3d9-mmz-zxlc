#include "d3d9depthstencilstate.h"
#include "d3d9samplerstate.h"
#include "d3d9devicechild_impl.h"
#include "log.h"
#include "globals.h"

#define LOG_MFUN(_, ...) LOG_MFUN_DEF(MyID3D9DepthStencilState, ## __VA_ARGS__)

class MyID3D9DepthStencilState::Impl {
    friend class MyID3D9DepthStencilState;

    IDirect3DDevice9* inner;
    D3D9_DEPTH_STENCIL_DESC desc;

    Impl(
        IDirect3DDevice9* inner,
        const D3D9_DEPTH_STENCIL_DESC* pDesc
    ) :
        inner(inner),
        desc(*pDesc)
    {}

    ~Impl() {}
};

D3D9_DEPTH_STENCIL_DESC& MyID3D9DepthStencilState::get_desc() {
    return impl->desc;
}

const D3D9_DEPTH_STENCIL_DESC& MyID3D9DepthStencilState::get_desc() const {
    return impl->desc;
}

MyID3D9DepthStencilState::MyID3D9DepthStencilState(
    IDirect3DDevice9* inner,
    const D3D9_DEPTH_STENCIL_DESC* pDesc
) :
    impl(new Impl(inner, pDesc))
{
     LOG_MFUN(_, LOG_ARG(inner));
    cached_dsss_map.emplace(inner, this);
}

MyID3D9DepthStencilState::~MyID3D9DepthStencilState() {
     LOG_MFUN();
    cached_dsss_map.erase(impl->inner);
    delete impl;
}

void STDMETHODCALLTYPE MyID3D9DepthStencilState::GetDesc(
    D3D9_DEPTH_STENCIL_DESC* pDesc
) {
     LOG_MFUN();
    if (pDesc) *pDesc = impl->desc;
}

std::unordered_map<IDirect3DDevice9*, MyID3D9DepthStencilState*> cached_dsss_map;
