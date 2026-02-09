#ifndef D3D9TEXTURE2D_H
#define D3D9TEXTURE2D_H

#include <d3d9.h>
#include <unordered_set>
#include <vector>
#include "d3d9types_wrapper.h"
#include "d3d9shaderresourceview.h"

// Define D3D_SRV_DIMENSION_TEXTURE2D for Direct3D 9
#define D3D_SRV_DIMENSION_TEXTURE2D D3DRTYPE_TEXTURE_ALIAS

// Forward declaration of MyID3D9ShaderResourceView
class MyID3D9ShaderResourceView;

// Forward declaration of Impl class
class MyID3D9Texture2DImpl;

class MyID3D9Texture2D : public IDirect3DTexture9 {
public:
    MyID3D9Texture2D(IDirect3DTexture9** inner, const D3DSURFACE_DESC* pDesc, UINT64 id);
    virtual ~MyID3D9Texture2D();

    IDirect3DTexture9*& get_inner();

    // Back-compat: old call sites expect get_srvs()
    std::unordered_set<MyID3D9ShaderResourceView*>& get_srvs() { return srvs; }
    const std::unordered_set<MyID3D9ShaderResourceView*>& get_srvs() const { return srvs; }

    const D3DSURFACE_DESC& get_desc() const;

    UINT get_orig_width() const;
    UINT get_orig_height() const;
    bool get_sc() const;

    const std::vector<IDirect3DSurface9*>& get_rtvs() const;
    const std::vector<IDirect3DTexture9*>& get_srv_inners() const;
    const std::vector<IDirect3DSurface9*>& get_dsvs() const;

    // IDirect3DTexture9 Methods
    HRESULT STDMETHODCALLTYPE LockRect(UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD Flags) override;
    HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level) override;
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level, D3DSURFACE_DESC* pDesc) override;
    HRESULT STDMETHODCALLTYPE AddDirtyRect(const RECT* pDirtyRect) override;

    // IDirect3DResource9 Methods
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) override;
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags) override;
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void* pData, DWORD* pSizeOfData) override;
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override;
    DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override;
    DWORD STDMETHODCALLTYPE GetPriority() override;
    void STDMETHODCALLTYPE PreLoad() override;
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override;
    DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) override;
    DWORD STDMETHODCALLTYPE GetLOD() override;
    DWORD STDMETHODCALLTYPE GetLevelCount() override;
    HRESULT STDMETHODCALLTYPE SetAutoGenFilterType(D3DTEXTUREFILTERTYPE FilterType) override;
    D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE GetAutoGenFilterType() override;
    void STDMETHODCALLTYPE GenerateMipSubLevels() override;
    void replace_inner(IDirect3DTexture9* new_inner);
    void replace_inner(IDirect3DTexture9* new_inner, const D3DSURFACE_DESC& new_desc);

private:
    MyID3D9Texture2DImpl* impl; // Pointer to implementation
    std::unordered_set<MyID3D9ShaderResourceView*> srvs;
};

#endif // D3D9TEXTURE2D_H
