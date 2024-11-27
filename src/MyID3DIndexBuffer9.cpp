#include "MyID3DIndexBuffer9.h"
#include "log.h"
#include <d3d9.h>
#include <d3d9types.h>
#include "globals.h"

//#define LOGGER default_logger
#define LOG_MFUN(_, ...) LOG_MFUN_DEF(MyIDirect3DIndexBuffer9, ## __VA_ARGS__)

MyIDirect3DIndexBuffer9::MyIDirect3DIndexBuffer9(
    IDirect3DDevice9** device,
    const void* pIndices,
    UINT length,
    DWORD usage
) : m_pIB(nullptr)
{
    HRESULT hr = (*device)->CreateIndexBuffer(length, usage, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &m_pIB, NULL);
    if (SUCCEEDED(hr) && m_pIB)
    {
        void* pVoid;
        hr = m_pIB->Lock(0, length, &pVoid, 0);
        if (SUCCEEDED(hr))
        {
            memcpy(pVoid, pIndices, length);
            m_pIB->Unlock();
        }
    }
    // Additional error handling or logging if needed
}

MyIDirect3DIndexBuffer9::~MyIDirect3DIndexBuffer9()
{
    if (m_pIB)
    {
        m_pIB->Release();
        m_pIB = nullptr;
    }
}

HRESULT STDMETHODCALLTYPE MyIDirect3DIndexBuffer9::Lock(
    UINT OffsetToLock,
    UINT SizeToLock,
    void** ppbData,
    DWORD Flags
)
{
    HRESULT hr = m_pIB ? m_pIB->Lock(OffsetToLock, SizeToLock, ppbData, Flags) : D3DERR_INVALIDCALL;
    if (SUCCEEDED(hr))
    {
        // Additional logging or error handling if needed
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE MyIDirect3DIndexBuffer9::Unlock()
{
    HRESULT hr = m_pIB ? m_pIB->Unlock() : D3DERR_INVALIDCALL;
    if (SUCCEEDED(hr))
    {
        // Additional logging or error handling if needed
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE MyIDirect3DIndexBuffer9::GetDesc(
    D3DINDEXBUFFER_DESC* pDesc
)
{
    if (!pDesc)
        return D3DERR_INVALIDCALL;

    D3DINDEXBUFFER_DESC desc;
    if (m_pIB)
    {
        m_pIB->GetDesc(&desc);
        *pDesc = desc;
        return D3D_OK;
    }
    return D3DERR_INVALIDCALL;
}

std::unordered_map<IDirect3DIndexBuffer9*, MyIDirect3DIndexBuffer9*> cached_ib_map;
