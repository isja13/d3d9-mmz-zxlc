#ifndef D3D9SHADERRESOURCEVIEW_H
#define D3D9SHADERRESOURCEVIEW_H

#include "main.h"
#include "unknown.h"
#include "d3d9view.h"
#include "d3d9texture2d.h"
#include "d3d9viewbase.h"

class MyID3D9ShaderResourceView : public MyID3D9View, public IDirect3DResource9 {
    class Impl;
    Impl* impl;

public:
    MyID3D9ShaderResourceView(
        IDirect3DResource9** inner,
        const D3DSURFACE_DESC* pDesc,
        IDirect3DResource9* resource
    );

    virtual ~MyID3D9ShaderResourceView();

    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) override;
    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC* pDesc);

    D3DSURFACE_DESC& get_desc();
    const D3DSURFACE_DESC& get_desc() const;

    IDirect3DResource9* get_resource() const override;
    IDirect3DResource9*& get_inner();

    void replace_inner(IDirect3DBaseTexture9* new_inner);
};

extern std::unordered_map<IDirect3DResource9*, MyID3D9ShaderResourceView*> cached_srvs_map;

#endif // D3D9SHADERRESOURCEVIEW_H
