#include "d3d9vertexshader.h"
#include "d3d9devicechild_impl.h"
#include "log.h"
#include <vector>
#include "globals.h"

#define LOG_MFUN(_, ...) LOG_MFUN_DEF(MyID3D9VertexShader, ## __VA_ARGS__)

class MyID3D9VertexShader::Impl {
public:
    IDirect3DDevice9* device;
    IDirect3DVertexShader9* m_pShader;
    std::vector<_D3DVERTEXELEMENT9> decl_entries;
    std::string source;
    DWORD bytecode_hash;
    SIZE_T bytecode_length;

    Impl(
        IDirect3DDevice9** device,
        const DWORD* bytecode,
        SIZE_T bytecodeLength,
        const std::string& source,
        std::vector<_D3DVERTEXELEMENT9>&& declEntries,
        IDirect3DVertexShader9* shader
    ) : device(*device),
        m_pShader(shader),
        decl_entries(std::move(declEntries)),
        source(source),
        bytecode_length(bytecodeLength)
    {
        // Compute bytecode hash
    }

    ~Impl() {
        if (m_pShader) {
            m_pShader->Release();
            m_pShader = nullptr;
        }
    }
};

MyID3D9VertexShader::MyID3D9VertexShader(
    IDirect3DDevice9** device,
    const DWORD* bytecode,
    SIZE_T bytecodeLength,
    const std::string& source,
    std::vector<_D3DVERTEXELEMENT9>&& declEntries
) : impl(new Impl(
    device,
    bytecode,
    bytecodeLength,
    source,
    std::move(declEntries),
    nullptr
))
{
    HRESULT hr = (*device)->CreateVertexShader(bytecode, &impl->m_pShader);
    if (SUCCEEDED(hr)) {
        // Initialize other members
    }
    else {
        // Handle shader creation failure
    }
}

MyID3D9VertexShader::~MyID3D9VertexShader() {
    delete impl;
    impl = nullptr;
}

const std::vector<_D3DVERTEXELEMENT9>& MyID3D9VertexShader::get_decl_entries() const {
    return impl->decl_entries;
}

HRESULT STDMETHODCALLTYPE MyID3D9VertexShader::QueryInterface(REFIID riid, void** ppvObject) {
    return impl->m_pShader->QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE MyID3D9VertexShader::AddRef() {
    return impl->m_pShader->AddRef();
}

ULONG STDMETHODCALLTYPE MyID3D9VertexShader::Release() {
    return impl->m_pShader->Release();
}
