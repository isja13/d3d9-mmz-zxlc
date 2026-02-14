#ifndef D3D9INPUTLAYOUT_H
#define D3D9INPUTLAYOUT_H

#include <d3d9.h>
#include "d3d9devicechild.h" // Include macros for device child implementation
#include "log.h"
#include "unknown_impl.h"

struct CustomVertexElement {
    const char* SemanticName;
    UINT SemanticIndex;
    UINT InputSlot;
    UINT AlignedByteOffset;
    D3DDECLTYPE Format;
};

class MyID3D9InputLayout : public IDirect3DVertexDeclaration9 {
    class Impl;
    Impl* impl;

public:
    MyID3D9InputLayout(IDirect3DVertexDeclaration9** inner, const D3DVERTEXELEMENT9* pInputElementDescs, UINT NumElements);
    ~MyID3D9InputLayout();

    // IDirect3DVertexDeclaration9 methods
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice);
    HRESULT STDMETHODCALLTYPE GetDeclaration(D3DVERTEXELEMENT9* pElement, UINT* pNumElements);

    // Custom methods for managing private data
    HRESULT STDMETHODCALLTYPE GetPrivateData(const GUID& guid, void* pData, DWORD* pDataSize);
    HRESULT STDMETHODCALLTYPE SetPrivateData(const GUID& guid, const void* pData, DWORD DataSize, DWORD Flags);
    HRESULT STDMETHODCALLTYPE FreePrivateData(const GUID& guid);

    // Additional methods specific to MyID3D9InputLayout
    IDirect3DVertexDeclaration9*& get_inner();
    UINT& get_descs_num();
    UINT get_descs_num() const;
    CustomVertexElement*& get_descs();
    CustomVertexElement* get_descs() const;

    // COM interface methods
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
};

#endif // D3D9INPUTLAYOUT_H
