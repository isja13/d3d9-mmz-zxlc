#ifndef D3D9PIXELSHADER_H
#define D3D9PIXELSHADER_H

#include <vector>
#include "main.h"
#include <d3d9.h>
#include <tuple>
#include <string>
#include <unordered_map>
#include <regex>

enum class PIXEL_SHADER_ALPHA_DISCARD {
    UNKNOWN,
    NONE,
    EQUAL,
    LESS,
    LESS_OR_EQUAL
};

class MyID3D9PixelShader : public IDirect3DPixelShader9 {
    class Impl;
    Impl* impl;

private:

    volatile LONG refcount_ = 1;

public:
    DWORD get_bytecode_hash() const;
    SIZE_T get_bytecode_length() const;
    const std::string& get_source() const;
    PIXEL_SHADER_ALPHA_DISCARD get_alpha_discard() const;
    const std::vector<UINT>& get_texcoord_sampler_map() const;
    const std::vector<std::tuple<std::string, std::string>>& get_uniform_list() const;
    IDirect3DPixelShader9* get_inner() const;

    MyID3D9PixelShader(
        IDirect3DPixelShader9* inner,
        DWORD bytecode_hash,
        SIZE_T bytecode_length,
        const std::string& source
    );

    virtual ~MyID3D9PixelShader();

    // Implement all pure virtual methods from IDirect3DPixelShader9
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** ppDevice) override;
    HRESULT STDMETHODCALLTYPE GetFunction(void* pData, UINT* pSizeOfData) override;
};

extern std::unordered_map<IDirect3DPixelShader9*, MyID3D9PixelShader*> cached_pss_map;

static inline DWORD zm_ps_hash(IDirect3DPixelShader9* ps)
{
    if (!ps) return 0;
    auto it = cached_pss_map.find(ps);
    if (it == cached_pss_map.end() || !it->second) return 0;
    return it->second->get_bytecode_hash();
}

static inline SIZE_T zm_ps_len(IDirect3DPixelShader9* ps)
{
    if (!ps) return 0;
    auto it = cached_pss_map.find(ps);
    if (it == cached_pss_map.end() || !it->second) return 0;
    return it->second->get_bytecode_length();
}

#endif
