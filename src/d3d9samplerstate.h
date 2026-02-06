#ifndef D3D9SAMPLERSTATE_H
#define D3D9SAMPLERSTATE_H

#include <d3d9.h>
#include <d3d9types.h>
#include <d3d9caps.h>
#include <unordered_map>
#include "main.h"
#include "d3d9devicechild.h"
#include "d3d9devicechild_impl.h"
#include "unknown_impl.h"
#include "macros.h"

// Define IID_IDirect3DSamplerState
DEFINE_GUID(IID_IDirect3DSamplerState,
    0x12345678, 0x1234, 0x1234, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34, 0x12, 0x34);

struct D3DSAMPLER_DESC {
    D3DTEXTUREFILTERTYPE Filter;
    D3DTEXTUREADDRESS AddressU;
    D3DTEXTUREADDRESS AddressV;
    D3DTEXTUREADDRESS AddressW;
    float MipLODBias;
    DWORD MaxAnisotropy;
    D3DTEXTUREFILTERTYPE MagFilter;
    D3DTEXTUREFILTERTYPE MinFilter;
    D3DTEXTUREFILTERTYPE MipFilter;
    float BorderColor[4]; // RGBA
    float MinLOD;
    float MaxLOD;
};

struct IDirect3DSamplerState : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) = 0;
    virtual void STDMETHODCALLTYPE GetDesc(D3DSAMPLER_DESC* pDesc) = 0;
};

class MyID3D9SamplerState : public IDirect3DSamplerState {
    class Impl;
    Impl* impl;

public:
    virtual HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) override;
    virtual HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) override;
    virtual HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, const void* pData, DWORD SizeOfData, DWORD Flags) override;
    virtual HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override;
    virtual void STDMETHODCALLTYPE GetDesc(D3DSAMPLER_DESC* pDesc) override;
    virtual ULONG STDMETHODCALLTYPE AddRef() override;
    virtual ULONG STDMETHODCALLTYPE Release() override;
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;

    IDirect3DSamplerState* get_inner();
    const IDirect3DSamplerState* get_inner() const;

    MyID3D9SamplerState(
        IDirect3DSamplerState** inner,
        const D3DSAMPLER_DESC* pDesc,
        IDirect3DSamplerState* linear = nullptr
    );

    virtual ~MyID3D9SamplerState();
    D3DSAMPLER_DESC& get_desc();
    const D3DSAMPLER_DESC& get_desc() const;
};

extern std::unordered_map<IDirect3DSamplerState*, MyID3D9SamplerState*> cached_sss_map;

#endif // D3D9SAMPLERSTATE_H
