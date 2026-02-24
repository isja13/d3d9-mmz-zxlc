#ifndef D3D9BUFFER_H
#define D3D9BUFFER_H

#include "main.h"
#include <d3d9.h>
#include <unordered_map>

class MyIDirect3DVertexBuffer9 : public IDirect3DVertexBuffer9 {
    IDirect3DVertexBuffer9* m_pVB;
    DWORD m_bytecodeHash;
    SIZE_T m_bytecodeLength;

public:
    MyIDirect3DVertexBuffer9(
        IDirect3DDevice9** device,
        const void* pVertices,
        UINT length,
        DWORD usage
    );

    virtual ~MyIDirect3DVertexBuffer9();

    HRESULT STDMETHODCALLTYPE Lock(
        UINT OffsetToLock,
        UINT SizeToLock,
        void** ppbData,
        DWORD Flags
    );

    HRESULT STDMETHODCALLTYPE Unlock();

    HRESULT STDMETHODCALLTYPE GetDesc(
        D3DVERTEXBUFFER_DESC* pDesc
    );

    bool get_cached_state() const;
    const char* get_cached() const;
};

extern std::unordered_map<IDirect3DVertexBuffer9*, MyIDirect3DVertexBuffer9*> cached_vb_map;

#endif // D3D9BUFFER_H