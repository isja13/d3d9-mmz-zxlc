#include <d3d9.h>
#include "d3d9buffer.h"
#include "log.h"
#include "globals.h"

//#define LOGGER default_logger
#define LOG_MFUN(_, ...) LOG_MFUN_DEF(MyIDirect3DVertexBuffer9, ## __VA_ARGS__)

std::unordered_map<IDirect3DVertexBuffer9*, MyIDirect3DVertexBuffer9*> cached_vb_map;

MyIDirect3DVertexBuffer9::MyIDirect3DVertexBuffer9(IDirect3DDevice9** device, const void* pVertices, UINT length, DWORD usage)
    : m_pVB(nullptr), m_bytecodeHash(0), m_bytecodeLength(length) {
    HRESULT hr = (*device)->CreateVertexBuffer(length, usage, 0, D3DPOOL_DEFAULT, &m_pVB, NULL);
    if (SUCCEEDED(hr) && m_pVB) {
        void* pVoid;
        hr = m_pVB->Lock(0, length, &pVoid, 0);
        if (SUCCEEDED(hr)) {
            memcpy(pVoid, pVertices, length);
            m_pVB->Unlock();
        }
    }
}

MyIDirect3DVertexBuffer9::~MyIDirect3DVertexBuffer9() {
    if (m_pVB) {
        m_pVB->Release();
        m_pVB = nullptr;
    }
}

HRESULT STDMETHODCALLTYPE MyIDirect3DVertexBuffer9::Lock(UINT OffsetToLock, UINT SizeToLock, void** ppbData, DWORD Flags) {
    return m_pVB ? m_pVB->Lock(OffsetToLock, SizeToLock, ppbData, Flags) : D3DERR_INVALIDCALL;
}

HRESULT STDMETHODCALLTYPE MyIDirect3DVertexBuffer9::Unlock() {
    return m_pVB ? m_pVB->Unlock() : D3DERR_INVALIDCALL;
}

HRESULT STDMETHODCALLTYPE MyIDirect3DVertexBuffer9::GetDesc(D3DVERTEXBUFFER_DESC* pDesc) {
    if (!pDesc)
        return D3DERR_INVALIDCALL;
    return m_pVB ? m_pVB->GetDesc(pDesc) : D3DERR_INVALIDCALL;
}

bool MyIDirect3DVertexBuffer9::get_cached_state() const {
    return m_pVB != nullptr;
}

const char* MyIDirect3DVertexBuffer9::get_cached() const {
    if (m_pVB) {
        void* data = nullptr;
        m_pVB->Lock(0, 0, &data, 0);
        m_pVB->Unlock();
        return static_cast<const char*>(data);
    }
    return nullptr;
}
