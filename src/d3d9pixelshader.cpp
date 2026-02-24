#include "d3d9pixelshader.h"
#include "log.h"
#include "globals.h"

#define ENABLE_LOGGER 1

#define LOG_MFUN(_, ...) LOG_MFUN_DEF(MyID3D9PixelShader, ## __VA_ARGS__)

class MyID3D9PixelShader::Impl {
public:
    IDirect3DPixelShader9* inner;
    DWORD bytecode_hash;
    SIZE_T bytecode_length;
    std::string source;
    PIXEL_SHADER_ALPHA_DISCARD alpha_discard;
    std::vector<UINT> texcoord_sampler_map;
    std::vector<std::tuple<std::string, std::string>> uniform_list;

    Impl(
        IDirect3DPixelShader9* inner,
        DWORD bytecode_hash,
        SIZE_T bytecode_length,
        const std::string& source
    ) :
        inner(inner),
        bytecode_hash(bytecode_hash),
        bytecode_length(bytecode_length),
        source(source),
        alpha_discard(
            source.find("discard") == std::string::npos ||
            source.find("fAlphaRef") == std::string::npos ?
            PIXEL_SHADER_ALPHA_DISCARD::NONE :
            source.find("==CBROPTestPS.fAlphaRef") != std::string::npos ?
            PIXEL_SHADER_ALPHA_DISCARD::EQUAL :
            source.find("<CBROPTestPS.fAlphaRef") != std::string::npos ?
            PIXEL_SHADER_ALPHA_DISCARD::LESS :
            source.find("CBROPTestPS.fAlphaRef>=") != std::string::npos ?
            PIXEL_SHADER_ALPHA_DISCARD::LESS_OR_EQUAL :
            PIXEL_SHADER_ALPHA_DISCARD::NONE
        )
    {
#if ENABLE_LOGGER
        std::unordered_map<std::string, UINT> tex_is;
        {
            std::string source_next = source;
            std::regex uniform_regex(R"(uniform\s+(\w+)\s+(\w+);)");
            std::smatch uniform_sm;
            UINT i = 0;
            while (std::regex_search(
                source_next,
                uniform_sm,
                uniform_regex
            )) {
                uniform_list.emplace_back(uniform_sm[1], uniform_sm[2]);

                std::regex sampler_regex(R"(\w*sampler\w+)");
                std::smatch sampler_sm;
                std::string type = uniform_sm[1];
                if (std::regex_search(
                    type,
                    sampler_sm,
                    sampler_regex
                )) {
                    tex_is.emplace(
                        uniform_sm[2],
                        sampler_sm[0] == "sampler2D" ? i : -1
                    );
                    ++i;
                }

                source_next = uniform_sm.suffix();
            }
        }
        for (UINT i = 0;; ++i) {
            std::string texcoord_var_name = "vs_TEXCOORD" + std::to_string(i);
            {
                std::regex texcoord_regex(R"(in\s+vec4\s+)" + texcoord_var_name + ";");
                std::smatch texcoord_sm;
                if (!std::regex_search(
                    source,
                    texcoord_sm,
                    texcoord_regex
                )) break;
            }
            {
                std::regex texture_regex(R"(texture\((\w+),\s*)" + texcoord_var_name);
                std::smatch texture_sm;
                if (std::regex_search(
                    source,
                    texture_sm,
                    texture_regex
                )) {
                    auto it = tex_is.find(texture_sm[1]);
                    texcoord_sampler_map.push_back(it != tex_is.end() ? it->second : -1);
                }
                else {
                    texcoord_sampler_map.push_back(-1);
                }
            }
        }
#endif
    }

    ~Impl() {}
};

DWORD MyID3D9PixelShader::get_bytecode_hash() const { return impl->bytecode_hash; }
SIZE_T MyID3D9PixelShader::get_bytecode_length() const { return impl->bytecode_length; }
const std::string& MyID3D9PixelShader::get_source() const { return impl->source; }
PIXEL_SHADER_ALPHA_DISCARD MyID3D9PixelShader::get_alpha_discard() const { return impl->alpha_discard; }
const std::vector<UINT>& MyID3D9PixelShader::get_texcoord_sampler_map() const { return impl->texcoord_sampler_map; }
const std::vector<std::tuple<std::string, std::string>>& MyID3D9PixelShader::get_uniform_list() const { return impl->uniform_list; }
IDirect3DPixelShader9* MyID3D9PixelShader::get_inner() const { return impl->inner; }

MyID3D9PixelShader::MyID3D9PixelShader(
    IDirect3DPixelShader9* inner,
    DWORD bytecode_hash,
    SIZE_T bytecode_length,
    const std::string& source
)
    : impl(new Impl(inner, bytecode_hash, bytecode_length, source))
{
    // IMPORTANT: wrapper must keep inner alive regardless of what the game does
    if (impl->inner)
        impl->inner->AddRef();

    // Map both identities -> wrapper metadata
    cached_pss_map[impl->inner] = this;
    cached_pss_map[(IDirect3DPixelShader9*)this] = this;
}

MyID3D9PixelShader::~MyID3D9PixelShader()
{
    OutputDebugStringA("[ZeroMod] MyID3D9PixelShader dtor\n");
    // impl->inner should already be released in Release() when rc hits 0
    delete impl;
    impl = nullptr;
}

HRESULT STDMETHODCALLTYPE MyID3D9PixelShader::QueryInterface(REFIID riid, void** ppvObj)
{
    if (!ppvObj) return E_POINTER;
    *ppvObj = nullptr;

    if (riid == IID_IUnknown || riid == IID_IDirect3DPixelShader9)
    {
        *ppvObj = static_cast<IDirect3DPixelShader9*>(this);
        AddRef();
        return S_OK;
    }

    // Forward anything else to inner
    return impl->inner->QueryInterface(riid, ppvObj);
}

ULONG STDMETHODCALLTYPE MyID3D9PixelShader::AddRef()
{
    LONG rc = InterlockedIncrement((LONG*)&refcount_);
    return (ULONG)rc;
}

ULONG STDMETHODCALLTYPE MyID3D9PixelShader::Release()
{
    LONG rc = InterlockedDecrement((LONG*)&refcount_);
    if (rc == 0)
    {
        OutputDebugStringA("[ZeroMod][PS WRAP] RC==0 -> deleting wrapper\n");
        cached_pss_map.erase(impl->inner);
        cached_pss_map.erase((IDirect3DPixelShader9*)this);

        if (impl->inner) {
            impl->inner->Release();
            impl->inner = nullptr;
        }
        delete this;
    }
    return (ULONG)rc;
}

HRESULT STDMETHODCALLTYPE MyID3D9PixelShader::GetDevice(IDirect3DDevice9** ppDevice) {
    return impl->inner->GetDevice(ppDevice);
}

HRESULT STDMETHODCALLTYPE MyID3D9PixelShader::GetFunction(void* pData, UINT* pSizeOfData) {
    return impl->inner->GetFunction(pData, pSizeOfData);
}

std::unordered_map<IDirect3DPixelShader9*, MyID3D9PixelShader*> cached_pss_map;
