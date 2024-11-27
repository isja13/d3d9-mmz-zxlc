#ifndef D3D9DEPTHSTENCILVIEW_H
#define D3D9DEPTHSTENCILVIEW_H

#include "main.h"
#include <d3d9.h>
#include <unordered_set>

class MyIDirect3DSurface9 : public IDirect3DSurface9 {
    class Impl;
    Impl* impl;

public:
    MyIDirect3DSurface9(IDirect3DSurface9** inner);
    virtual ~MyIDirect3DSurface9();

    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) override;
    IDirect3DSurface9*& get_inner(); // Adding the method declaration
    IDirect3DResource9* get_resource(); // Adding the method as defined in the cpp
};

class MyID3D9DepthStencilView : public MyIDirect3DSurface9 {
    class Impl;
    Impl* impl;

public:
    MyID3D9DepthStencilView(MyIDirect3DSurface9** inner);
    virtual ~MyID3D9DepthStencilView();

    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) override;
    MyIDirect3DSurface9*& get_inner(); // Adding the method declaration
};

#endif // D3D9DEPTHSTENCILVIEW_H
