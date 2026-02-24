#ifndef D3D9RENDERTARGETVIEW_H
#define D3D9RENDERTARGETVIEW_H

#include "main.h"
#include "unknown.h"
#include "d3d9view.h"
#include "d3d9texture2d.h"
#include "d3d9viewbase.h"

class MyID3D9RenderTargetView : public MyID3D9View, public IDirect3DSurface9 {
    class Impl;
    Impl* impl;

public:
    MyID3D9RenderTargetView(
        IDirect3DSurface9** inner,
        const D3DSURFACE_DESC* pDesc,
        IDirect3DResource9* resource
    );

    virtual ~MyID3D9RenderTargetView();

    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) override;
    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC* pDesc) override;

    D3DSURFACE_DESC& get_desc();
    const D3DSURFACE_DESC& get_desc() const;

    IDirect3DResource9* get_resource() const override;
    IDirect3DSurface9*& get_inner(); // Adding the method declaration

    void replace_inner(IDirect3DSurface9* new_inner);
};

extern std::unordered_map<IDirect3DSurface9*, MyID3D9RenderTargetView*> cached_rtvs_map;

#endif // D3D9RENDERTARGETVIEW_H
