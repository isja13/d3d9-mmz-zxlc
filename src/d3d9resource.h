#ifndef D3D9RESOURCE_H
#define D3D9RESOURCE_H


#include "macros.h"
#include "d3d9devicechild.h"
#include <d3d9.h>

/*#define IUNKNOWN_PRIV(Class) \
    ULONG STDMETHODCALLTYPE AddRef() override { return impl->inner->AddRef(); } \
    ULONG STDMETHODCALLTYPE Release() override { return impl->inner->Release(); } \
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override { return impl->inner->QueryInterface(riid, ppvObj); }

#define ID3D9RESOURCE_PRIV \
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) override { return impl->inner->GetDevice(ppDevice); } \
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags) override { return impl->inner->SetPrivateData(refguid, pData, SizeOfData, Flags); } \
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) override { return impl->inner->GetPrivateData(refguid, pData, pSizeOfData); } \
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override { return impl->inner->FreePrivateData(refguid); } \
    DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override { return impl->inner->SetPriority(PriorityNew); } \
    DWORD STDMETHODCALLTYPE GetPriority() override { return impl->inner->GetPriority(); } \
    void STDMETHODCALLTYPE PreLoad() override { impl->inner->PreLoad(); } \
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { return impl->inner->GetType(); }
    */
#endif // D3D9RESOURCE_H
