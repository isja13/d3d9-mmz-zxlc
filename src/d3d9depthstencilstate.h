#ifndef D3D9DEPTHSTENCILSTATE_H
#define D3D9DEPTHSTENCILSTATE_H

#include <d3d9.h>
#include <d3d9types.h>
#include <d3d9caps.h>
#include <unordered_map>
#include "main.h"
#include "d3d9devicechild.h"
#include "d3d9devicechild_impl.h"
#include "unknown_impl.h"
#include "macros.h"

struct D3D9_DEPTH_STENCILOP_DESC {
    D3DSTENCILOP StencilFailOp;
    D3DSTENCILOP StencilDepthFailOp;
    D3DSTENCILOP StencilPassOp;
    D3DCMPFUNC StencilFunc;
};

struct D3D9_DEPTH_STENCIL_DESC {
    BOOL DepthEnable;
    D3DZBUFFERTYPE DepthWriteMask;
    D3DCMPFUNC DepthFunc;
    BOOL StencilEnable;
    UINT8 StencilReadMask;
    UINT8 StencilWriteMask;
    D3D9_DEPTH_STENCILOP_DESC FrontFace;
    D3D9_DEPTH_STENCILOP_DESC BackFace;
};

class MyID3D9DepthStencilState : public IUnknown {
    class Impl;
    Impl* impl;

public:
    // Constructor
    MyID3D9DepthStencilState(
        IDirect3DDevice9* inner,
        const D3D9_DEPTH_STENCIL_DESC* pDesc
    );

    // Destructor
    virtual ~MyID3D9DepthStencilState();

    // Methods to access state information
    D3D9_DEPTH_STENCIL_DESC& get_desc();
    const D3D9_DEPTH_STENCIL_DESC& get_desc() const;

    // DirectX 9 equivalent method signatures
    virtual void STDMETHODCALLTYPE GetDesc(
        D3D9_DEPTH_STENCIL_DESC* pDesc
    );
};

extern std::unordered_map<IDirect3DDevice9*, MyID3D9DepthStencilState*> cached_dsss_map;

#endif // D3D9DEPTHSTENCILSTATE_H
