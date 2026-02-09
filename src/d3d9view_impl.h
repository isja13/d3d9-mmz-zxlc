#ifndef D3D9VIEW_IMPL_H
#define D3D9VIEW_IMPL_H

#include "d3d9devicechild_impl.h"

// Macro for private members related to ID3D9View
#define ID3D9VIEW_PRIV \
    IDirect3DResource9* resource = nullptr;

// Macro for initializing ID3D9View members
#define ID3D9VIEW_INIT(n) \
    resource(n)

// Macro for implementing common methods for ID3D9View-derived classes
#define ID3D9VIEW_IMPL(d, b) \
    IDirect3DResource9*& d::get_resource() { \
        return impl->resource; \
    } \
    IDirect3DResource9* d::get_resource() const { \
        return impl->resource; \
    } \
    void STDMETHODCALLTYPE d::GetResource( \
        IDirect3DResource9** ppResource \
    ) { \
        *ppResource = impl->resource; \
        if (impl->resource) { \
            impl->resource->AddRef(); \
        } \
    } \
    ID3D9DEVICECHILD_IMPL(d, b)

// Macro for declaring common methods for ID3D9View-derived classes
#define ID3D9VIEW_DECL(base) \
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override; \
    virtual ULONG STDMETHODCALLTYPE AddRef() override; \
    virtual ULONG STDMETHODCALLTYPE Release() override; \
    virtual void STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) override; \
    virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags) override; \
    virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) override; \
    virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override; \
    virtual DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override; \
    virtual DWORD STDMETHODCALLTYPE GetPriority() override; \
    virtual void STDMETHODCALLTYPE PreLoad() override; \
    virtual D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override;

#endif // D3D9VIEW_IMPL_H
