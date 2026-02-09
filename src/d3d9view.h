#ifndef D3D9VIEW_H
#define D3D9VIEW_H

#include "d3d9devicechild.h"
#include "macros.h"

#define ID3D9VIEW_DECL(b) \
    IDirect3DResource9*& get_resource(); \
    IDirect3DResource9* get_resource() const; \
    virtual void STDMETHODCALLTYPE GetResource(IDirect3DResource9** ppResource); \
    ID3D9DEVICECHILD_DECL(b)

#endif // D3D9VIEW_H
