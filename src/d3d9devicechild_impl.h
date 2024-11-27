#ifndef D3D9DEVICECHILD_IMPL_H
#define D3D9DEVICECHILD_IMPL_H

#include "unknown_impl.h"
#include "macros.h"

#define ID3D9DEVICECHILD_IMPL(d, b) \
    HRESULT STDMETHODCALLTYPE d::GetDevice(IDirect3DDevice9** ppDevice) { \
        LOG_MFUN(); \
        return impl->inner->GetDevice(ppDevice); \
    } \
    IUNKNOWN_IMPL(d, b)

#endif // D3D9DEVICECHILD_IMPL_H

