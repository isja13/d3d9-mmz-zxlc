#include "d3d9inputlayout.h"
#include "d3d9devicechild_impl.h"
#include "log.h"
#include "globals.h"

//#define LOGGER default_logger
#define LOG_MFUN(...) LOG_MFUN_DEF(MyID3D9InputLayout, __VA_ARGS__)

// Ensure the Impl class is correctly defined here
class MyID3D9InputLayout::Impl {
public:
    IDirect3DVertexDeclaration9* inner;
    CustomVertexElement* descs;
    UINT descs_num;

    Impl(IDirect3DVertexDeclaration9** inner, const D3DVERTEXELEMENT9* pInputElementDescs, UINT NumElements)
        : inner(*inner),
        descs(new CustomVertexElement[NumElements]),
        descs_num(NumElements) {
        for (UINT i = 0; i < NumElements; ++i) {
            descs[i].SemanticName = "Unknown"; // Assign proper value
            descs[i].SemanticIndex = pInputElementDescs[i].UsageIndex;
            descs[i].InputSlot = 0; // No equivalent in D3D9, set to 0
            descs[i].AlignedByteOffset = pInputElementDescs[i].Offset;
            descs[i].Format = static_cast<D3DDECLTYPE>(pInputElementDescs[i].Type); // Cast BYTE to D3DDECLTYPE
        }
    }

    ~Impl() {
        delete[] descs;
    }
};

MyID3D9InputLayout::MyID3D9InputLayout(IDirect3DVertexDeclaration9** inner, const D3DVERTEXELEMENT9* pInputElementDescs, UINT NumElements)
    : impl(new Impl(inner, pInputElementDescs, NumElements)) {}

MyID3D9InputLayout::~MyID3D9InputLayout() {
    delete impl;
}

HRESULT STDMETHODCALLTYPE MyID3D9InputLayout::GetDevice(IDirect3DDevice9** ppDevice) {
    LOG_MFUN("GetDevice", LOG_ARG(ppDevice));
    return impl->inner->GetDevice(ppDevice);
}

HRESULT STDMETHODCALLTYPE MyID3D9InputLayout::GetDeclaration(D3DVERTEXELEMENT9* pElement, UINT* pNumElements) {
    LOG_MFUN("GetDeclaration", LOG_ARG(pElement), LOG_ARG(pNumElements));
    return impl->inner->GetDeclaration(pElement, pNumElements);
}

ULONG STDMETHODCALLTYPE MyID3D9InputLayout::AddRef() {
    LOG_MFUN("AddRef");
    return impl->inner->AddRef();
}

ULONG STDMETHODCALLTYPE MyID3D9InputLayout::Release() {
    LOG_MFUN("Release");
    return impl->inner->Release();
}

HRESULT STDMETHODCALLTYPE MyID3D9InputLayout::QueryInterface(REFIID riid, void** ppvObject) {
    LOG_MFUN("QueryInterface", LOG_ARG(riid), LOG_ARG(ppvObject));
    return impl->inner->QueryInterface(riid, ppvObject);
}

HRESULT STDMETHODCALLTYPE MyID3D9InputLayout::GetPrivateData(const GUID& guid, void* pData, DWORD* pDataSize) {
    LOG_MFUN("GetPrivateData", LOG_ARG(guid), LOG_ARG(pData), LOG_ARG(pDataSize));
    ZM_NOTIMPL_RET();
}

HRESULT STDMETHODCALLTYPE MyID3D9InputLayout::SetPrivateData(const GUID& guid, const void* pData, DWORD DataSize, DWORD Flags) {
    LOG_MFUN("SetPrivateData", LOG_ARG(guid), LOG_ARG(pData), LOG_ARG(DataSize), LOG_ARG(Flags));
    ZM_NOTIMPL_RET();
}

HRESULT STDMETHODCALLTYPE MyID3D9InputLayout::FreePrivateData(const GUID& guid) {
    LOG_MFUN("FreePrivateData", LOG_ARG(guid));
    ZM_NOTIMPL_RET();
}

IDirect3DVertexDeclaration9*& MyID3D9InputLayout::get_inner() {
    return impl->inner;
}

UINT& MyID3D9InputLayout::get_descs_num() {
    return impl->descs_num;
}

UINT MyID3D9InputLayout::get_descs_num() const {
    return impl->descs_num;
}

CustomVertexElement*& MyID3D9InputLayout::get_descs() {
    return impl->descs;
}

CustomVertexElement* MyID3D9InputLayout::get_descs() const {
    return impl->descs;
}

std::unordered_map<IDirect3DVertexDeclaration9*, MyID3D9InputLayout*> cached_ils_map;
