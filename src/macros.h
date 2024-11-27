#ifndef MACROS_H
#define MACROS_H

// ID3D9RESOURCE_PRIV Macro Definition
#ifndef ID3D9RESOURCE_PRIV
#define ID3D9RESOURCE_PRIV \
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) override; \
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags) override; \
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) override; \
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override; \
    DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override; \
    DWORD STDMETHODCALLTYPE GetPriority() override; \
    void STDMETHODCALLTYPE PreLoad() override; \
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override;
#endif

// ID3D9RESOURCE_IMPL Macro Definition
#ifndef ID3D9RESOURCE_IMPL
#define ID3D9RESOURCE_IMPL(T) \
    HRESULT STDMETHODCALLTYPE T::GetDevice(IDirect3DDevice9** ppDevice) { return inner->GetDevice(ppDevice); } \
    HRESULT STDMETHODCALLTYPE T::SetPrivateData(REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags) { return inner->SetPrivateData(refguid, pData, SizeOfData, Flags); } \
    HRESULT STDMETHODCALLTYPE T::GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) { return inner->GetPrivateData(refguid, pData, pSizeOfData); } \
    HRESULT STDMETHODCALLTYPE T::FreePrivateData(REFGUID refguid) { return inner->FreePrivateData(refguid); } \
    DWORD STDMETHODCALLTYPE T::SetPriority(DWORD PriorityNew) { return inner->SetPriority(PriorityNew); } \
    DWORD STDMETHODCALLTYPE T::GetPriority() { return inner->GetPriority(); } \
    void STDMETHODCALLTYPE T::PreLoad() { inner->PreLoad(); } \
    D3DRESOURCETYPE STDMETHODCALLTYPE T::GetType() { return inner->GetType(); }
#endif

// ID3D9DEVICECHILD_DECL Macro Definition
#ifndef ID3D9DEVICECHILD_DECL
#define ID3D9DEVICECHILD_DECL(b) \
    virtual HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) override; \
    IUNKNOWN_DECL(b)
#endif

#endif // MACROS_H
