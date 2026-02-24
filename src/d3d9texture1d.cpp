#include "d3d9texture1d.h"
#include "log.h"
#include "globals.h"

#define LOG_MFUN(_, ...) LOG_MFUN_DEF(MyIDirect3DTexture9, ## __VA_ARGS__)

class MyIDirect3DTexture9::Impl {
    friend class MyIDirect3DTexture9;

    IDirect3DTexture9* inner;
    _D3DSURFACE_DESC desc;
    void* mapped_data = nullptr;

    Impl(
        IDirect3DTexture9** inner,
        const _D3DSURFACE_DESC* pDesc,
        UINT64 id
    ) :
        inner(*inner),
        desc(*pDesc)
    {
        D3DSURFACE_DESC temp_desc;
        HRESULT hr = (*inner)->GetLevelDesc(0, &temp_desc);
        if (SUCCEEDED(hr)) {
            // Convert D3DSURFACE_DESC to _D3DSURFACE_DESC
            desc.Format = temp_desc.Format;
            desc.Type = temp_desc.Type;
            desc.Usage = temp_desc.Usage;
            desc.Pool = temp_desc.Pool;
            desc.MultiSampleType = temp_desc.MultiSampleType;
            desc.MultiSampleQuality = temp_desc.MultiSampleQuality;
            desc.Width = temp_desc.Width;
            desc.Height = temp_desc.Height;
        }
        else {
            LOG_MFUN(_;
                LOG_ARG(*inner);
                LOG_ARG_TYPE(hr, NumHexLogger<UINT64>)
            );
        }
    }

    ~Impl() {
        if (inner) inner->Release();
    }

    HRESULT LockRect(
        UINT Level,
        D3DLOCKED_RECT* pLockedRect,
        const RECT* pRect,
        DWORD Flags
    ) {
        return inner->LockRect(Level, pLockedRect, pRect, Flags);
    }

    HRESULT UnlockRect(UINT Level) {
        return inner->UnlockRect(Level);
    }

    HRESULT GetLevelDesc(UINT Level, _D3DSURFACE_DESC* pDesc) {
        if (!pDesc) return E_POINTER;

        D3DSURFACE_DESC temp_desc;
        HRESULT hr = inner->GetLevelDesc(Level, &temp_desc);
        if (SUCCEEDED(hr)) {
            // Convert D3DSURFACE_DESC to _D3DSURFACE_DESC
            pDesc->Format = temp_desc.Format;
            pDesc->Type = temp_desc.Type;
            pDesc->Usage = temp_desc.Usage;
            pDesc->Pool = temp_desc.Pool;
            pDesc->MultiSampleType = temp_desc.MultiSampleType;
            pDesc->MultiSampleQuality = temp_desc.MultiSampleQuality;
            pDesc->Width = temp_desc.Width;
            pDesc->Height = temp_desc.Height;
        }
        return hr;
    }

    DWORD GetLevelCount() const {
        return inner->GetLevelCount();
    }
};

MyIDirect3DTexture9::MyIDirect3DTexture9(
    IDirect3DTexture9** inner,
    const _D3DSURFACE_DESC* pDesc,
    UINT64 id
) :
    impl(new Impl(inner, pDesc, id))
{
    LOG_MFUN(_;
    LOG_ARG(*inner);
        LOG_ARG_TYPE(id, NumHexLogger<UINT64>)
   ) ;
    cached_t9_map.emplace(*inner, this);
    *inner = this;
}

MyIDirect3DTexture9::~MyIDirect3DTexture9() {
    LOG_MFUN();
    cached_t9_map.erase(impl->inner);
    delete impl;
}

_D3DSURFACE_DESC& MyIDirect3DTexture9::get_desc() {
    return impl->desc;
}

const _D3DSURFACE_DESC& MyIDirect3DTexture9::get_desc() const {
    return impl->desc;
}

IDirect3DTexture9* MyIDirect3DTexture9::get_inner() const {
    return impl->inner;
}

HRESULT STDMETHODCALLTYPE MyIDirect3DTexture9::LockRect(
    UINT Level,
    D3DLOCKED_RECT* pLockedRect,
    const RECT* pRect,
    DWORD Flags
) {
    return impl->LockRect(Level, pLockedRect, pRect, Flags);
}

HRESULT STDMETHODCALLTYPE MyIDirect3DTexture9::UnlockRect(UINT Level) {
    return impl->UnlockRect(Level);
}

HRESULT STDMETHODCALLTYPE MyIDirect3DTexture9::GetLevelDesc(
    UINT Level,
    _D3DSURFACE_DESC* pDesc
) {
    return impl->GetLevelDesc(Level, pDesc);
}

std::unordered_map<IDirect3DTexture9*, MyIDirect3DTexture9*> cached_t9_map;

HRESULT STDMETHODCALLTYPE MyIDirect3DTexture9::QueryInterface(REFIID riid, void** ppvObj) {
    return impl->inner->QueryInterface(riid, ppvObj);
}

ULONG STDMETHODCALLTYPE MyIDirect3DTexture9::AddRef() {
    return impl->inner->AddRef();
}

ULONG STDMETHODCALLTYPE MyIDirect3DTexture9::Release() {
    return impl->inner->Release();
}

HRESULT STDMETHODCALLTYPE MyIDirect3DTexture9::GetDevice(IDirect3DDevice9** ppDevice) {
    return impl->inner->GetDevice(ppDevice);
}

HRESULT STDMETHODCALLTYPE MyIDirect3DTexture9::SetPrivateData(REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags) {
    return impl->inner->SetPrivateData(refguid, pData, SizeOfData, Flags);
}

HRESULT STDMETHODCALLTYPE MyIDirect3DTexture9::GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) {
    return impl->inner->GetPrivateData(refguid, pData, pSizeOfData);
}

HRESULT STDMETHODCALLTYPE MyIDirect3DTexture9::FreePrivateData(REFGUID refguid) {
    return impl->inner->FreePrivateData(refguid);
}

DWORD STDMETHODCALLTYPE MyIDirect3DTexture9::SetPriority(DWORD PriorityNew) {
    return impl->inner->SetPriority(PriorityNew);
}

DWORD STDMETHODCALLTYPE MyIDirect3DTexture9::GetPriority() {
    return impl->inner->GetPriority();
}

void STDMETHODCALLTYPE MyIDirect3DTexture9::PreLoad() {
    impl->inner->PreLoad();
}

D3DRESOURCETYPE STDMETHODCALLTYPE MyIDirect3DTexture9::GetType() {
    return impl->inner->GetType();
}

DWORD STDMETHODCALLTYPE MyIDirect3DTexture9::SetLOD(DWORD LODNew) {
    return impl->inner->SetLOD(LODNew);
}

DWORD STDMETHODCALLTYPE MyIDirect3DTexture9::GetLOD() {
    return impl->inner->GetLOD();
}

DWORD STDMETHODCALLTYPE MyIDirect3DTexture9::GetLevelCount() {
    return impl->GetLevelCount();
}

HRESULT STDMETHODCALLTYPE MyIDirect3DTexture9::SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) {
    return impl->inner->SetAutoGenFilterType(FilterType);
}

D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE MyIDirect3DTexture9::GetAutoGenFilterType() {
    return impl->inner->GetAutoGenFilterType();
}

void STDMETHODCALLTYPE MyIDirect3DTexture9::GenerateMipSubLevels() {
    impl->inner->GenerateMipSubLevels();
}