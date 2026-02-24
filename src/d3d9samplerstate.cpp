#include "d3d9samplerstate.h"
#include "d3d9devicechild_impl.h"
#include "log.h"
#include "globals.h"

#define LOG_MFUN(_, ...) LOG_MFUN_DEF(MyID3D9SamplerState, ## __VA_ARGS__)

class MyID3D9SamplerState::Impl {
    friend class MyID3D9SamplerState;

    IDirect3DSamplerState* inner;
    D3DSAMPLER_DESC desc;
    IDirect3DSamplerState* linear;

    Impl(
        IDirect3DSamplerState** inner,
        const D3DSAMPLER_DESC* pDesc,
        IDirect3DSamplerState* linear
    ) :
        inner(*inner),
        desc(*pDesc),
        linear(linear)
    {}

    ~Impl() {
        if (linear) linear->Release();
    }
};

D3DSAMPLER_DESC& MyID3D9SamplerState::get_desc() {
    return impl->desc;
}

const D3DSAMPLER_DESC& MyID3D9SamplerState::get_desc() const {
    return impl->desc;
}

MyID3D9SamplerState::MyID3D9SamplerState(
    IDirect3DSamplerState** inner,
    const D3DSAMPLER_DESC* pDesc,
    IDirect3DSamplerState* linear
) :
    impl(new Impl(inner, pDesc, linear))
{
    LOG_MFUN(_, LOG_ARG(*inner));
    cached_sss_map.emplace(*inner, this);
    *inner = this;
}

MyID3D9SamplerState::~MyID3D9SamplerState() {
    LOG_MFUN();
    cached_sss_map.erase(impl->inner);
    delete impl;
}

IDirect3DSamplerState* MyID3D9SamplerState::get_inner() {
    return impl->inner;
}

const IDirect3DSamplerState* MyID3D9SamplerState::get_inner() const {
    return impl->inner;
}

void STDMETHODCALLTYPE MyID3D9SamplerState::GetDesc(
    D3DSAMPLER_DESC* pDesc
) {
    LOG_MFUN();
    if (pDesc) *pDesc = impl->desc;
}

HRESULT STDMETHODCALLTYPE MyID3D9SamplerState::GetDevice(IDirect3DDevice9** ppDevice)
{
    if (!ppDevice) return D3DERR_INVALIDCALL;
    *ppDevice = nullptr;
    return D3DERR_INVALIDCALL; // not E_NOTIMPL
}


HRESULT STDMETHODCALLTYPE MyID3D9SamplerState::GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) {
    ZM_NOTIMPL_RET();
}

HRESULT STDMETHODCALLTYPE MyID3D9SamplerState::SetPrivateData(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags) {
    ZM_NOTIMPL_RET();
}

HRESULT STDMETHODCALLTYPE MyID3D9SamplerState::FreePrivateData(REFGUID refguid) {
    ZM_NOTIMPL_RET();
}

ULONG STDMETHODCALLTYPE MyID3D9SamplerState::AddRef() {
    return impl->inner->AddRef();
}

ULONG STDMETHODCALLTYPE MyID3D9SamplerState::Release() {
    return impl->inner->Release();
}

HRESULT STDMETHODCALLTYPE MyID3D9SamplerState::QueryInterface(REFIID riid, void** ppvObject) {
    return impl->inner->QueryInterface(riid, ppvObject);
}

std::unordered_map<IDirect3DSamplerState*, MyID3D9SamplerState*> cached_sss_map;
