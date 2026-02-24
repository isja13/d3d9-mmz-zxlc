#include "d3d9texture2d.h"
#include "log.h"
#include "macros.h"
#include "unknown_impl.h"
#include "globals.h"

#define LOG_MFUN(_, ...) LOG_MFUN_DEF(MyID3D9Texture2D, ## __VA_ARGS__)

// Class Implementation
class MyID3D9Texture2DImpl {
public:
    IDirect3DTexture9* inner;
    D3DSURFACE_DESC desc;
    UINT orig_width;
    UINT orig_height;
    bool sc;
    std::vector<IDirect3DSurface9*> rtvs;
    std::vector<IDirect3DTexture9*> srvs;
    std::vector<IDirect3DSurface9*> dsvs;

    MyID3D9Texture2DImpl(IDirect3DTexture9* inner, const D3DSURFACE_DESC* pDesc, UINT64 id)
        : inner(inner), desc(*pDesc), orig_width(pDesc->Width), orig_height(pDesc->Height), sc(false) {}
};

IDirect3DTexture9*& MyID3D9Texture2D::get_inner() {
    return impl->inner;
}

MyID3D9Texture2D::MyID3D9Texture2D(IDirect3DTexture9** inner, const D3DSURFACE_DESC* pDesc, UINT64 id)
    : impl(new MyID3D9Texture2DImpl(*inner, pDesc, id)) {
    LOG_MFUN(_, LOG_ARG(*inner), LOG_ARG_TYPE(id, NumHexLogger<UINT64>));
    *inner = this;
}

MyID3D9Texture2D::~MyID3D9Texture2D() {
    LOG_MFUN();
    delete impl;
}

const D3DSURFACE_DESC& MyID3D9Texture2D::get_desc() const {
    return impl->desc;
}

UINT MyID3D9Texture2D::get_orig_width() const {
    return impl->orig_width;
}

UINT MyID3D9Texture2D::get_orig_height() const {
    return impl->orig_height;
}

bool MyID3D9Texture2D::get_sc() const {
    return impl->sc;
}

const std::vector<IDirect3DSurface9*>& MyID3D9Texture2D::get_rtvs() const {
    return impl->rtvs;
}

const std::vector<IDirect3DTexture9*>& MyID3D9Texture2D::get_srv_inners() const {
    return impl->srvs;
}

const std::vector<IDirect3DSurface9*>& MyID3D9Texture2D::get_dsvs() const {
    return impl->dsvs;
}

HRESULT STDMETHODCALLTYPE MyID3D9Texture2D::LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags) {
    LOG_MFUN();
    return impl->inner->LockRect(Level, pLockedRect, pRect, Flags);
}

HRESULT STDMETHODCALLTYPE MyID3D9Texture2D::UnlockRect(UINT Level) {
    LOG_MFUN();
    return impl->inner->UnlockRect(Level);
}

HRESULT STDMETHODCALLTYPE MyID3D9Texture2D::GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) {
    LOG_MFUN();
    if (!pDesc)
        return D3DERR_INVALIDCALL;

    *pDesc = impl->desc;       // <- current size lives here
    return S_OK;
}

HRESULT STDMETHODCALLTYPE MyID3D9Texture2D::AddDirtyRect(const RECT* pDirtyRect) {
    LOG_MFUN();
    return impl->inner->AddDirtyRect(pDirtyRect);
}

// IDirect3DResource9 Methods
HRESULT STDMETHODCALLTYPE MyID3D9Texture2D::GetDevice(IDirect3DDevice9** ppDevice) {
    return impl->inner->GetDevice(ppDevice);
}

HRESULT STDMETHODCALLTYPE MyID3D9Texture2D::SetPrivateData(REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags) {
    return impl->inner->SetPrivateData(refguid, pData, SizeOfData, Flags);
}

HRESULT STDMETHODCALLTYPE MyID3D9Texture2D::GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) {
    return impl->inner->GetPrivateData(refguid, pData, pSizeOfData);
}

HRESULT STDMETHODCALLTYPE MyID3D9Texture2D::FreePrivateData(REFGUID refguid) {
    return impl->inner->FreePrivateData(refguid);
}

DWORD STDMETHODCALLTYPE MyID3D9Texture2D::SetPriority(DWORD PriorityNew) {
    return impl->inner->SetPriority(PriorityNew);
}

DWORD STDMETHODCALLTYPE MyID3D9Texture2D::GetPriority() {
    return impl->inner->GetPriority();
}

void STDMETHODCALLTYPE MyID3D9Texture2D::PreLoad() {
    impl->inner->PreLoad();
}

void MyID3D9Texture2D::replace_inner(IDirect3DTexture9* new_inner, const D3DSURFACE_DESC& new_desc)
{
    if (!new_inner)
        return;

    new_inner->AddRef();

    if (impl->inner)
        impl->inner->Release();

    impl->inner = new_inner;

    impl->desc = new_desc;
}

void MyID3D9Texture2D::replace_inner(IDirect3DTexture9* new_inner)
{
    if (!new_inner)
        return;

    D3DSURFACE_DESC d = {};
    if (SUCCEEDED(new_inner->GetLevelDesc(0, &d)))
        replace_inner(new_inner, d);
    else {
        // fallback: still swap, but keep existing desc
        new_inner->AddRef();
        if (impl->inner) impl->inner->Release();
        impl->inner = new_inner;
    }
}

D3DRESOURCETYPE STDMETHODCALLTYPE MyID3D9Texture2D::GetType() {
    return impl->inner->GetType();
}

DWORD STDMETHODCALLTYPE MyID3D9Texture2D::SetLOD(DWORD LODNew) {
    return impl->inner->SetLOD(LODNew);
}

DWORD STDMETHODCALLTYPE MyID3D9Texture2D::GetLOD() {
    return impl->inner->GetLOD();
}

DWORD STDMETHODCALLTYPE MyID3D9Texture2D::GetLevelCount() {
    return impl->inner->GetLevelCount();
}

HRESULT STDMETHODCALLTYPE MyID3D9Texture2D::SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) {
    return impl->inner->SetAutoGenFilterType(FilterType);
}

D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE MyID3D9Texture2D::GetAutoGenFilterType() {
    return impl->inner->GetAutoGenFilterType();
}

void STDMETHODCALLTYPE MyID3D9Texture2D::GenerateMipSubLevels() {
    impl->inner->GenerateMipSubLevels();
}
