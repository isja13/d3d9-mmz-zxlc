#ifndef MYID3DINDEXBUFFER9_H
#define MYID3DINDEXBUFFER9_H

#include <d3d9.h>
#include "log.h"

// Custom index buffer class
class MyIDirect3DIndexBuffer9 : public IDirect3DIndexBuffer9 {
    IDirect3DIndexBuffer9* m_pIB;

public:
    MyIDirect3DIndexBuffer9(
        IDirect3DDevice9** device,
        const void* pIndices,
        UINT length,
        DWORD usage
    );

    virtual ~MyIDirect3DIndexBuffer9();

    HRESULT STDMETHODCALLTYPE Lock(
        UINT OffsetToLock,
        UINT SizeToLock,
        void** ppbData,
        DWORD Flags
    );

    HRESULT STDMETHODCALLTYPE Unlock();

    HRESULT STDMETHODCALLTYPE GetDesc(
        D3DINDEXBUFFER_DESC* pDesc
    );

    // Any additional methods or member variables
};

extern std::unordered_map<IDirect3DIndexBuffer9*, MyIDirect3DIndexBuffer9*> cached_ib_map;

#endif // MYID3DINDEXBUFFER9_H
