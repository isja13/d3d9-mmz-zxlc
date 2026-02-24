#ifndef D3D9TEXTURE1D_H
#define D3D9TEXTURE1D_H

#include <d3d9.h>
#include <d3d9types.h>
#include <unordered_map>

class MyIDirect3DTexture9 : public IDirect3DTexture9 {
    class Impl;
    Impl* impl;

public:
    MyIDirect3DTexture9(
        IDirect3DTexture9** inner,
        const _D3DSURFACE_DESC* pDesc,
        UINT64 id
    );

    virtual ~MyIDirect3DTexture9();

    _D3DSURFACE_DESC& get_desc();
    const _D3DSURFACE_DESC& get_desc() const;

    IDirect3DTexture9* get_inner() const;

    // Implementing methods from IDirect3DTexture9
    HRESULT STDMETHODCALLTYPE LockRect(
        UINT Level,
        D3DLOCKED_RECT* pLockedRect,
        const RECT* pRect,
        DWORD Flags
    ) override;

    HRESULT STDMETHODCALLTYPE UnlockRect(
        UINT Level
    ) override;

    HRESULT STDMETHODCALLTYPE GetLevelDesc(
        UINT Level,
        _D3DSURFACE_DESC* pDesc
    );

    // Other IDirect3DResource9 methods
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG, AddRef)() override;
    STDMETHOD_(ULONG, Release)() override;
    STDMETHOD(GetDevice)(IDirect3DDevice9** ppDevice) override;
    STDMETHOD(SetPrivateData)(REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags) override;
    STDMETHOD(GetPrivateData)(REFGUID refguid, void* pData, DWORD* pSizeOfData) override;
    STDMETHOD(FreePrivateData)(REFGUID refguid) override;
    STDMETHOD_(DWORD, SetPriority)(DWORD PriorityNew) override;
    STDMETHOD_(DWORD, GetPriority)() override;
    STDMETHOD_(void, PreLoad)() override;
    STDMETHOD_(D3DRESOURCETYPE, GetType)() override;

    // Other IDirect3DBaseTexture9 methods
    STDMETHOD_(DWORD, SetLOD)(DWORD LODNew) override;
    STDMETHOD_(DWORD, GetLOD)() override;
    STDMETHOD_(DWORD, GetLevelCount)() override;
    STDMETHOD(SetAutoGenFilterType)(D3DTEXTUREFILTERTYPE FilterType) override;
    STDMETHOD_(D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)() override;
    STDMETHOD_(void, GenerateMipSubLevels)() override;
};

extern std::unordered_map<IDirect3DTexture9*, MyIDirect3DTexture9*> cached_t9_map;

#endif // D3D9TEXTURE1D_H
