#ifndef D3D9VERTEXSHADER_H
#define D3D9VERTEXSHADER_H

#include <vector>
#include <d3d9.h>
#include <string>
#include "main.h"
#include "d3d9devicechild.h"
#include "macros.h"

class MyID3D9VertexShader : public IDirect3DVertexShader9 {
    class Impl;
    Impl* impl;

public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    const std::vector<_D3DVERTEXELEMENT9>& get_decl_entries() const;

    MyID3D9VertexShader(
        IDirect3DDevice9** device,
        const DWORD* bytecode,
        SIZE_T bytecodeLength,
        const std::string& source,
        std::vector<_D3DVERTEXELEMENT9>&& declEntries
    );

    virtual ~MyID3D9VertexShader();
};

#endif // D3D9VERTEXSHADER_H
