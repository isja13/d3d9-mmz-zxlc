#include <d3d9.h>
#include <d3dx9.h>
#include "d3d9device.h"
#include "d3d9pixelshader.h"
#include "d3d9vertexshader.h"
#include "d3d9buffer.h"
#include "d3d9texture1d.h"
#include "d3d9texture2d.h"
#include "d3d9samplerstate.h"
#include "d3d9depthstencilstate.h"
#include "d3d9inputlayout.h"
#include "d3d9rendertargetview.h"
#include "d3d9shaderresourceview.h"
#include "d3d9depthstencilview.h"
#include "conf.h"
#include "log.h"
#include "overlay.h"
#include "tex.h"
#include "unknown_impl.h"
#include "../smhasher/MurmurHash3.h"
#include "half/include/half.hpp"
#include "MyID3DIndexBuffer9.h"
#include "d3d9video.h"
#include "globals.h"
#include "slang_d3d9.h"



#include <windows.h>
#define DBG(s) OutputDebugStringA("[ZeroMod] " s "\n")

#define DBGF(fmt, ...) do { \
    char _b[1024]; \
    _snprintf(_b, sizeof(_b), "[ZeroMod] " fmt "\n", __VA_ARGS__); \
    OutputDebugStringA(_b); \
} while (0)

#include <cstdarg>
#include <cstdio>



#define MAX_SAMPLERS 16
#define MAX_SHADER_RESOURCES 128
#define MAX_CONSTANT_BUFFERS 15

#define NOISE_WIDTH 256
#define NOISE_HEIGHT 256
#define ZERO_WIDTH 240
#define ZERO_HEIGHT 160
#define ZX_WIDTH 256
#define ZX_HEIGHT 192
#define ZERO_WIDTH_FILTERED (ZERO_WIDTH * 2)
#define ZERO_HEIGHT_FILTERED (ZERO_HEIGHT * 2)
#define ZX_WIDTH_FILTERED (ZX_WIDTH * 2)
#define ZX_HEIGHT_FILTERED (ZX_HEIGHT * 2)
#define PS_BYTECODE_LENGTH_T1_THRESHOLD 1000

#define PS_HASH_T1 0x5A8763D5u
#define PS_HASH_T2 0xc9b117d5
#define PS_HASH_T3 0x1f4c05ac
#define SO_B_LEN (sizeof(float) * 4 * 4 * 6 * 100)

#ifndef E_NOTIMPL
#define E_NOTIMPL ((HRESULT)0x80004001L)
#endif 

// --- MENU -> GAME one-shot 60-frame scan for "512x512 exists" (stage0 only) ---
static bool g_scan_ever_started = false;
static bool g_scan_active = false;
static bool g_zx_latched = false;
static bool g_zx_latch_valid = false;
static unsigned g_scan_draws = 0;
static bool     g_scan_create_hit_512 = false;
static unsigned g_scan_create_hit_count = 0;

static bool ptr_readable(const void* p, size_t bytes)
{
    if (!p) return false;

    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof(mbi)))
        return false;

    if (mbi.State != MEM_COMMIT)
        return false;

    const DWORD prot = mbi.Protect & 0xff;
    const bool readable =
        prot == PAGE_READONLY ||
        prot == PAGE_READWRITE ||
        prot == PAGE_WRITECOPY ||
        prot == PAGE_EXECUTE_READ ||
        prot == PAGE_EXECUTE_READWRITE ||
        prot == PAGE_EXECUTE_WRITECOPY;

    if (!readable)
        return false;

    const uintptr_t start = (uintptr_t)p;
    const uintptr_t end = start + bytes;
    const uintptr_t region_end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;

    return end <= region_end;
}

static __forceinline void LogIfUnsupported(const char* name, HRESULT hr)
{
    if (hr == E_NOTIMPL) {
        char b[256];
        _snprintf(b, sizeof(b), "[UNSUPPORTED] %s returned E_NOTIMPL\n", name);
        OutputDebugStringA(b);
    }
}
static const char* zm_hrstr(HRESULT hr)
{
    switch (hr) {
    case D3D_OK:                 return "D3D_OK";
    case D3DERR_INVALIDCALL:     return "D3DERR_INVALIDCALL";
    case D3DERR_DEVICELOST:      return "D3DERR_DEVICELOST";
    case D3DERR_DEVICENOTRESET:  return "D3DERR_DEVICENOTRESET";
    default:                     return "hr=?";
    }
}

static __forceinline void zm_log_vp(const char* tag, const D3DVIEWPORT9& vp, unsigned long seq)
{
    char b[256];
    _snprintf(b, sizeof(b),
        "[ZeroMod][Draw #%lu] %s VP: x=%lu y=%lu w=%lu h=%lu z=[%f..%f]\n",
        seq, tag,
        (unsigned long)vp.X, (unsigned long)vp.Y,
        (unsigned long)vp.Width, (unsigned long)vp.Height,
        (double)vp.MinZ, (double)vp.MaxZ);
    OutputDebugStringA(b);
}

static __forceinline void zm_log_surf_desc(const char* tag, IDirect3DSurface9* s, unsigned long seq)
{
    if (!s) {
        char b[192];
        _snprintf(b, sizeof(b), "[ZeroMod][Draw #%lu] %s: (null)\n", seq, tag);
        OutputDebugStringA(b);
        return;
    }
    D3DSURFACE_DESC d{};
    HRESULT hr = s->GetDesc(&d);
    char b[256];
    _snprintf(b, sizeof(b),
        "[ZeroMod][Draw #%lu] %s: surf=%p GetDesc hr=0x%08X %s w=%u h=%u fmt=%u ms=%u\n",
        seq, tag, (void*)s, (unsigned)hr, zm_hrstr(hr),
        (unsigned)d.Width, (unsigned)d.Height, (unsigned)d.Format, (unsigned)d.MultiSampleType);
    OutputDebugStringA(b);
}

static __forceinline void zm_log_tex2d_desc(const char* tag, IDirect3DTexture9* t, unsigned long seq)
{
    if (!t) {
        char b[192];
        _snprintf(b, sizeof(b), "[ZeroMod][Draw #%lu] %s: (null)\n", seq, tag);
        OutputDebugStringA(b);
        return;
    }
    D3DSURFACE_DESC d{};
    HRESULT hr = t->GetLevelDesc(0, &d);
    char b[256];
    _snprintf(b, sizeof(b),
        "[ZeroMod][Draw #%lu] %s: tex=%p GetLevelDesc hr=0x%08X %s w=%u h=%u fmt=%u\n",
        seq, tag, (void*)t, (unsigned)hr, zm_hrstr(hr),
        (unsigned)d.Width, (unsigned)d.Height, (unsigned)d.Format);
    OutputDebugStringA(b);
}

// optional convenience macro so you only write one token per callsite
#define ZM_LOG_UNSUP(NAME_LIT, HRVAL) LogIfUnsupported((NAME_LIT), (HRVAL))

class MyIDirect3DDevice9 : public IDirect3DDevice9 {
public:
    // Existing implementation

   // MyIDirect3DDevice9(IDirect3DDevice9* pOriginal);
    virtual ~MyIDirect3DDevice9();

    // Implement the necessary methods
    void STDMETHODCALLTYPE CopyResource(IDirect3DResource9* pDstResource, IDirect3DResource9* pSrcResource);
    void STDMETHODCALLTYPE UpdateSubresource(IDirect3DResource9* pDstResource, const void* pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch);
    void STDMETHODCALLTYPE ClearRenderTargetView(IDirect3DSurface9* pRenderTarget, const D3DCOLOR Color);
    void STDMETHODCALLTYPE ClearDepthStencilView(IDirect3DSurface9* pDepthStencil, DWORD ClearFlags, float Depth, DWORD Stencil);

    // Forward all other IDirect3DDevice9 methods to the original device (impl)
   //RESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj);
 // ULONG STDMETHODCALLTYPE AddRef();
 // ULONG STDMETHODCALLTYPE Release();
    // ... (other methods forwarding to impl)

private:
    IDirect3DDevice9* impl;
};

namespace ZeroMod {
	bool is_zx() { return g_zx_latched; }
}

namespace {

    bool almost_equal(UINT a, UINT b) { return (a > b ? a - b : b - a) <= 1; }

    UINT64 xorshift128p_state[2] = {};

    bool xorshift128p_state_init() {
        if (!(
            QueryPerformanceCounter((LARGE_INTEGER*)&xorshift128p_state[0]) &&
            QueryPerformanceCounter((LARGE_INTEGER*)&xorshift128p_state[1]))
            ) {
            xorshift128p_state[0] = GetTickCount64();
            xorshift128p_state[1] = GetTickCount64();
        }
        return xorshift128p_state[0] && xorshift128p_state[1];
    }

    bool xorshift128p_state_init_status = xorshift128p_state_init();

    void get_resolution_mul(
        UINT& render_width,
        UINT& render_height,
        UINT width,
        UINT height
    ) {
        UINT width_quo = render_width / width;
        UINT width_rem = render_width % width;
        if (width_rem) ++width_quo;
        UINT height_quo = render_height / height;
        UINT height_rem = render_height % height;
        if (height_rem) ++height_quo;
        render_width = width * width_quo;
        render_height = height * height_quo;
    }

}

// From wikipedia
UINT64 xorshift128p() {
    UINT64* s = xorshift128p_state;
    UINT64 a = s[0];
    UINT64 const b = s[1];
    s[0] = b;
    a ^= a << 23;       // a
    a ^= a >> 17;       // b
    a ^= b ^ (b >> 26); // c
    s[1] = a;
    return a + b;
}

class MyID3D9Device::Impl {
public:
    IDirect3DDevice9* inner;
    UINT width;
    UINT height;
    Config* config = nullptr;
    Overlay* overlay = nullptr;

    ZeroMod_d3d9_video_t* d3d9_2d;
    ZeroMod_d3d9_video_t* d3d9_gba;
    ZeroMod_d3d9_video_t* d3d9_ds;
    UINT64 frame_count;

    UINT render_width;
    UINT render_height;
    UINT render_orig_width;
    UINT render_orig_height;
    bool need_render_vp;
    IDirect3DSurface9* rtv; // Add the rtv surface

    float cached_vs_constants[256 * 4]; // Assuming a maximum of 256 vector4 registers
    DWORD cached_fvf; // Cached FVF
    std::unordered_map<D3DRENDERSTATETYPE, DWORD> cached_rs; // Cached render states
    IDirect3DStateBlock9* cached_dss; // Cached depth stencil state

    IDirect3DIndexBuffer9* cached_ib; // Add this
    D3DFORMAT cached_ib_format;       // Add this
    UINT cached_ib_offset;            // Add this

 
    HWND cached_hwnd = NULL;
    bool overlay_inited = false;

    // D3D9 "SRVs" == textures bound via SetTexture(stage,...)
    static constexpr DWORD ZM_MAX_TEX_STAGES = 128; // or MAX_SAMPLERS

    IDirect3DBaseTexture9* cached_stage_tex[ZM_MAX_TEX_STAGES + 1] = {}; // +1 for null sentinel
    IDirect3DBaseTexture9** cached_pssrvs = cached_stage_tex;            // if code expects pointer-to-pointer


    ULONG AddRef() {
        return inner->AddRef();
    }

    ULONG Release() {
        return inner->Release();
    }

    HRESULT QueryInterface(REFIID riid, void** ppvObj) {
        return inner->QueryInterface(riid, ppvObj);
    }


    struct FilterState {
        IDirect3DTexture9* srv_tex = nullptr;
        MyIDirect3DTexture9* rtv_tex = nullptr;
        MyID3D9PixelShader* ps;
        MyID3D9VertexShader* vs;
        IDirect3DTexture9* rtv_tex_inner = nullptr;
        bool t1;
      //  bool t2;
        MyID3D9SamplerState* psss;

        bool zx;
        UINT start_vertex_location;
        IDirect3DVertexBuffer9* vertex_buffer;
        UINT vertex_stride;
        UINT vertex_offset;
    } filter_state = {};

    bool filter = false;

    void clear_filter() {
        filter = false;

        if (filter_state.srv_tex) { filter_state.srv_tex->Release(); filter_state.srv_tex = nullptr; }

        if (filter_state.rtv_tex) { filter_state.rtv_tex->Release(); filter_state.rtv_tex = nullptr; } // if still use wrapper
        if (filter_state.rtv_tex_inner) { filter_state.rtv_tex_inner->Release(); filter_state.rtv_tex_inner = nullptr; }

        if (filter_state.ps) { filter_state.ps->Release(); filter_state.ps = nullptr; }
        if (filter_state.vs) { filter_state.vs->Release(); filter_state.vs = nullptr; }
        if (filter_state.psss) { filter_state.psss->Release(); filter_state.psss = nullptr; }

        if (filter_state.vertex_buffer) { filter_state.vertex_buffer->Release(); filter_state.vertex_buffer = nullptr; }

        filter_state.t1 = false;
        filter_state.zx = false;
        filter_state.start_vertex_location = 0;
        filter_state.vertex_stride = 0;
        filter_state.vertex_offset = 0;
    }

    bool render_interp = false;
    bool render_linear = false;
    bool render_enhanced = false;
    UINT linear_test_width = 0;
    UINT linear_test_height = 0;

    void update_config() {

        char b[256];
        _snprintf(b, sizeof(b),
            "[ZeroMod] update_config: impl=%p impl->config=%p default_config=%p\n",
            (void*)this, (void*)config, (void*)default_config);
        OutputDebugStringA(b);

        DBG("update_config ENTER");

        if (!config) {
            DBG("update_config: config null, skip");
            return;
        }

        auto notify = [&](const char* msg) {
            if (overlay) overlay->push_text(msg);
            OutputDebugStringA((std::string("[ZeroMod] notify: ") + msg).c_str());
            };
        auto notify_s = [&](const std::string& msg) {
            if (overlay) overlay->push_text(msg);
            OutputDebugStringA((std::string("[ZeroMod] notify: ") + msg).c_str());
            };

#define GET_SET_CONFIG_BOOL(v, m) do { \
    bool v##_value = config->v; \
    if (render_##v != v##_value) { \
        render_##v = v##_value; \
        notify(v##_value ? (m " enabled") : (m " disabled")); \
    } \
} while (0)

        DBG("update_config: linear_test_updated check");
        if (config->linear_test_updated) {
            config->begin_config();
            linear_test_width = config->linear_test_width;
            linear_test_height = config->linear_test_height;
            config->linear_test_updated = false;
            config->end_config();
            DBG("update_config: linear_test_updated applied");
        }

        DBG("update_config: bool toggles");
        GET_SET_CONFIG_BOOL(interp, "Interp fix");
        GET_SET_CONFIG_BOOL(linear, "Force linear filtering");
        GET_SET_CONFIG_BOOL(enhanced, "Enhanced Type 1 filter");

#define SLANG_SHADERS \
    X(2d) \
    X(gba) \
    X(ds)

#define X(v) config->slang_shader_##v##_updated ||

        DBG("update_config: slang updated check");
        {
            char b[512];
            _snprintf(b, sizeof(b),
                "[ZeroMod] slang flags: 2d=%d gba=%d ds=%d\n",
                (int)config->slang_shader_2d_updated.load(),
                (int)config->slang_shader_gba_updated.load(),
                (int)config->slang_shader_ds_updated.load()
            );
            OutputDebugStringA(b);
        }
        if (SLANG_SHADERS false) {

#undef X
#define X(v) \
    bool slang_shader_##v##_updated = config->slang_shader_##v##_updated; \
    std::string slang_shader_##v; \
    if (slang_shader_##v##_updated) { \
        slang_shader_##v = config->slang_shader_##v; \
        config->slang_shader_##v##_updated = false; \
    }

            config->begin_config();
            SLANG_SHADERS
                config->end_config();
            {
                auto dump = [&](const char* tag, bool upd, const std::string& s) {
                    char b[768];
                    _snprintf(b, sizeof(b),
                        "[ZeroMod] slang %s upd=%d len=%u val='%s'\n",
                        tag, (int)upd, (unsigned)s.size(), s.c_str()
                    );
                    OutputDebugStringA(b);
                    };
                dump("2d", slang_shader_2d_updated, slang_shader_2d);
                dump("gba", slang_shader_gba_updated, slang_shader_gba);
                dump("ds", slang_shader_ds_updated, slang_shader_ds);
            }

#undef X
#define X(v) \
    if (slang_shader_##v##_updated) { \
        DBG("update_config: slang " #v " applying"); \
        { \
            char b__[512]; \
            _snprintf(b__, sizeof(b__), "[ZeroMod] slang %s applying: updated=%d\n", #v, (int)slang_shader_##v##_updated); \
            OutputDebugStringA(b__); \
        } \
        if (!d3d9_##v) { \
            OutputDebugStringA("[ZeroMod] slang init request: " #v "\n"); \
            d3d9_##v = ZeroMod::d3d9_gfx_init(inner, D3DFMT_A8R8G8B8); \
            { \
                char b__[256]; \
                _snprintf(b__, sizeof(b__), "[ZeroMod] slang init result %s: d3d9_%s=%p\n", #v, #v, (void*)d3d9_##v); \
                OutputDebugStringA(b__); \
            } \
            if (!d3d9_##v) { \
                notify("Failed to initialize slang shader " #v); \
            } \
        } \
        if (d3d9_##v) { \
            if (slang_shader_##v.empty()) { \
                ZeroMod::d3d9_gfx_set_shader(d3d9_##v, nullptr); \
                notify("Slang shader " #v " disabled"); \
            } else { \
                { \
                    char b__[768]; \
                    _snprintf(b__, sizeof(b__), "[ZeroMod] slang set_shader %s: len=%u path='%s'\n", \
                        #v, (unsigned)slang_shader_##v.size(), slang_shader_##v.c_str()); \
                    OutputDebugStringA(b__); \
                } \
                bool ok__ = ZeroMod::d3d9_gfx_set_shader(d3d9_##v, slang_shader_##v.c_str()); \
                OutputDebugStringA(ok__ ? "[ZeroMod] slang set_shader OK\n" : "[ZeroMod] slang set_shader FAIL\n"); \
                if (ok__) { \
                    notify_s(std::string("Slang shader " #v " set to ") + slang_shader_##v); \
                } else { \
                    notify_s(std::string("Failed to set slang shader " #v " to ") + slang_shader_##v); \
                } \
            } \
        } \
    }

            SLANG_SHADERS

#undef X
#undef SLANG_SHADERS
        }

#undef GET_SET_CONFIG_BOOL

        DBG("update_config EXIT");
    }

    struct SamplerDesc {
        D3DTEXTUREFILTERTYPE filter = D3DTEXF_POINT;
        D3DTEXTUREADDRESS address = D3DTADDRESS_CLAMP;
    };

    static inline void apply_sampler(IDirect3DDevice9* dev, DWORD slot, const SamplerDesc& s) {
        if (!dev) return;
        dev->SetSamplerState(slot, D3DSAMP_MAGFILTER, s.filter);
        dev->SetSamplerState(slot, D3DSAMP_MINFILTER, s.filter);
        dev->SetSamplerState(slot, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
        dev->SetSamplerState(slot, D3DSAMP_ADDRESSU, s.address);
        dev->SetSamplerState(slot, D3DSAMP_ADDRESSV, s.address);
        dev->SetSamplerState(slot, D3DSAMP_ADDRESSW, s.address);
    }

    void create_sampler(
        D3DTEXTUREFILTERTYPE filter,
        SamplerDesc& sampler,
        D3DTEXTUREADDRESS address = D3DTADDRESS_CLAMP
    ) {
        sampler.filter = filter;
        sampler.address = address;
    }

    void create_texture(
        UINT width,
        UINT height,
        IDirect3DTexture9*& texture,
        D3DFORMAT format = D3DFMT_A8R8G8B8,
        DWORD usage = D3DUSAGE_RENDERTARGET,
        D3DPOOL pool = D3DPOOL_DEFAULT
    ) {
        HRESULT hr = inner->CreateTexture(
            width,
            height,
            1,
            usage,
            format,
            pool,
            &texture,
            NULL
        );
        if (FAILED(hr)) {
        }
    }

    void create_tex_and_views(
        UINT width,
        UINT height,
        TextureAndViews* tex
    ) {
        create_texture(width, height, tex->tex);
        create_srv(tex->tex, tex->srv);
        create_rtv(tex->tex, tex->rtv);
        tex->width = width;
        tex->height = height;
    }

    void create_rtv(
        IDirect3DTexture9* tex,
        IDirect3DSurface9*& rtv,
        D3DFORMAT format = D3DFMT_A8R8G8B8
    ) {
        tex->GetSurfaceLevel(0, &rtv);
    }

    void create_srv(
        IDirect3DTexture9* tex,
        IDirect3DTexture9*& srv,
        D3DFORMAT format = D3DFMT_A8R8G8B8
    ) {
        srv = tex;
        srv->AddRef();
    }

    void create_dsv(
        IDirect3DTexture9* tex,
        IDirect3DSurface9*& dsv,
        D3DFORMAT format
    ) {
        tex->GetSurfaceLevel(0, &dsv);
    }

    bool set_render_tex_views_and_update(
        MyID3D9Texture2D*& tex,   // <-- BY REFERENCE so caller sees updates
        UINT width,
        UINT height,
        UINT orig_width,
        UINT orig_height,
        bool need_vp
    ) {
        if (!tex || !inner)
            return false;

        // If already committed to a render viewport and caller is asking for a different one, reject.
        if (need_vp && need_render_vp &&
            (render_width != width || render_height != height ||
                render_orig_width != orig_width || render_orig_height != orig_height)) {
            return false;
        }

        // Orig-dim sanity check (keep semantics)
        if (!almost_equal(tex->get_orig_width(), orig_width) ||
            !almost_equal(tex->get_orig_height(), orig_height)) {
            return false;
        }

        D3DSURFACE_DESC desc{};
        if (FAILED(tex->GetLevelDesc(0, &desc)))
            return false;

        const bool size_mismatch =
            (!almost_equal(desc.Width, width) ||
                !almost_equal(desc.Height, height));

        if (size_mismatch) {
            // Don't resize "shared content" or whatever sc() means.
            if (tex->get_sc())
                return false;

            // Recreate INNER texture
            IDirect3DTexture9* new_tex_inner = nullptr;
            HRESULT hr = inner->CreateTexture(
                width, height, 1,
                D3DUSAGE_RENDERTARGET,
                desc.Format,
                D3DPOOL_DEFAULT,
                &new_tex_inner,
                nullptr
            );
            if (FAILED(hr) || !new_tex_inner)
                return false;

            // Get new RT surface
            IDirect3DSurface9* new_rt_surf = nullptr;
            hr = new_tex_inner->GetSurfaceLevel(0, &new_rt_surf);
            if (FAILED(hr) || !new_rt_surf) {
                new_tex_inner->Release();
                return false;
            }

            // Swap into the texture wrapper WITHOUT killing the wrapper object
            // (need tex->replace_inner)
            tex->replace_inner(new_tex_inner);

            // Update cached member RTV surface (assuming `rtv` is a member IDirect3DSurface9*)
            if (rtv) { rtv->Release(); rtv = nullptr; }
            rtv = new_rt_surf;               // take ownership of ref
            // new_rt_surf already has 1 ref from GetSurfaceLevel

            // no longer need our local strong ref to the texture inner.
            new_tex_inner->Release();        // wrapper holds its own ref now

            // Repoint dependent views (do NOT shadow `rtv` name)
            // RTVs / DSVs get the *surface*, SRVs get the *texture*.
            for (auto* v : tex->get_rtvs()) {
                if (auto* myRtv = dynamic_cast<MyID3D9RenderTargetView*>(v)) {
                    myRtv->replace_inner(rtv); // AddRef inside
                }
            }

            for (auto* v : tex->get_srvs()) {
                if (auto* mySrv = dynamic_cast<MyID3D9ShaderResourceView*>(v)) {
                    // SRV should point to the texture (base texture is fine)
                    mySrv->replace_inner(reinterpret_cast<IDirect3DBaseTexture9*>(tex->get_inner()));
                }
            }

            for (auto* v : tex->get_dsvs()) {
                if (auto* myDsv = dynamic_cast<MyID3D9DepthStencilView*>(v)) {
                    myDsv->replace_inner(rtv); // If DSVs store on this tex wrapper
                }
            }
        }

        // If size == orig size
        if (almost_equal(width, orig_width) && almost_equal(height, orig_height))
            return false;

        // Commit viewport request
        if (need_vp && !need_render_vp) {
            render_width = width;
            render_height = height;
            render_orig_width = orig_width;
            render_orig_height = orig_height;
            need_render_vp = true;
        }

        return true;
    }

    void create_tex_and_views_nn(
        TextureAndViews* tex,
        UINT width,
        UINT height
    ) {
        if (!tex) return;

        if (render_size.render_width == 0 || render_size.render_height == 0) {
            DBG("create_tex_and_views_nn: render_size 0 -> skip");
            return;
        }

        UINT render_width = render_size.render_width;
        UINT render_height = render_size.render_height;
        get_resolution_mul(render_width, render_height, width, height);

        if (render_width == 0 || render_height == 0) {
            DBG("create_tex_and_views_nn: computed render size 0 -> skip");
            return;
        }

        create_tex_and_views(render_width, render_height, tex);
    }

    void create_tex_and_view_1(
        TextureViewsAndBuffer* tex,
        UINT render_width,
        UINT render_height,
        UINT width,
        UINT height
    ) {
        create_texture(render_width, render_height, tex->tex);
        create_srv(tex->tex, tex->srv);
        create_rtv(tex->tex, tex->rtv);
        tex->width = render_width;
        tex->height = render_height;
    }

    // c0 = (src_w, src_h, 1/src_w, 1/src_h)
    inline void set_ps_size_constants(UINT src_w, UINT src_h) {
        if (!inner || src_w == 0 || src_h == 0) return;

        float c0[4] = {
            (float)src_w,
            (float)src_h,
            1.0f / (float)src_w,
            1.0f / (float)src_h
        };

        HRESULT hr = inner->SetPixelShaderConstantF(0, c0, 1);
        (void)hr; 
    }



    void create_tex_and_view_1_v(std::vector<TextureViewsAndBuffer*>& tex_v, UINT width, UINT height) {
        if (render_size.render_width == 0 || render_size.render_height == 0) {
            DBG("create_tex_and_view_1_v: render_size 0 -> SKIP");
            return;
        }
        bool last = false;
        do {
            UINT width_next = width * 2;
            UINT height_next = height * 2;
            last =
                width_next >= render_size.render_width &&
                height_next >= render_size.render_height;
            TextureViewsAndBuffer* tex = new TextureViewsAndBuffer{};
            create_tex_and_view_1(tex, width_next, height_next, width, height);
            tex_v.push_back(tex);
            width = width_next;
            height = height_next;
        } while (!last);
    }

    void create_tex_and_depth_views_2(
        UINT width,
        UINT height,
        TextureAndDepthViews* tex
    ) {
        if (!tex) return;

        if (render_size.render_width == 0 || render_size.render_height == 0) {
            DBG("create_tex_and_depth_views_2: render_size 0 -> skip");
            return;
        }

        UINT render_width = render_size.render_width;
        UINT render_height = render_size.render_height;
        get_resolution_mul(render_width, render_height, width, height);

        if (render_width == 0 || render_height == 0) {
            DBG("create_tex_and_depth_views_2: computed render size 0 -> skip");
            return;
        }

        create_tex_and_views(render_width, render_height, tex);
        if (tex->ds) { tex->ds->Release(); tex->ds = NULL; }

        HRESULT hr = inner->CreateDepthStencilSurface(
            tex->width,
            tex->height,
            D3DFMT_D24S8,
            D3DMULTISAMPLE_NONE,
            0,
            TRUE,
            &tex->ds,
            NULL
        );

        if (FAILED(hr)) {
            char b[128];
            sprintf(b, "[ZeroMod] CreateDepthStencilSurface FAIL hr=0x%08lX\n", (unsigned long)hr);
            OutputDebugStringA(b);
        }

    }
    struct FilterTemp {
        SamplerDesc sampler_nn;
        SamplerDesc sampler_linear;
        SamplerDesc sampler_wrap;

        TextureAndViews* tex_nn_zero;

        TextureAndViews* tex_nn_zx;
        TextureAndDepthViews* tex_t2;

        std::vector<TextureViewsAndBuffer*> tex_1_zero;
        std::vector<TextureViewsAndBuffer*> tex_1_zx;
    } filter_temp = {};

    void filter_temp_init() {

        auto chk = [&](const char* name, UINT w, UINT h) {
            char b[256];
            sprintf(b, "[ZeroMod] %s: %ux%u\n", name, w, h);
            OutputDebugStringA(b);
            return (w > 0 && h > 0);
            };

        if (!chk("ZERO", ZERO_WIDTH, ZERO_HEIGHT)) return;
        if (!chk("ZX", ZX_WIDTH, ZX_HEIGHT)) return;
        if (!chk("NOISE", NOISE_WIDTH, NOISE_HEIGHT)) return;
        if (!chk("ZERO_FILTERED", ZERO_WIDTH_FILTERED, ZERO_HEIGHT_FILTERED)) return;
        if (!chk("ZX_FILTERED", ZX_WIDTH_FILTERED, ZX_HEIGHT_FILTERED)) return;

        DBG("filter_temp_init ENTER");

        if (!inner) { DBG("filter_temp_init: inner NULL"); return; }

        if (render_size.render_width == 0 || render_size.render_height == 0) {
            DBG("filter_temp_init: render_size is 0x0 -> SKIP (not ready yet)");
            return;
        }

        DBG("filter_temp_init: render_size OK");
        filter_temp_shutdown(inner);

        DBG("filter_temp_init: after shutdown");

        DBG("filter_temp_init: create_sampler NN");
        create_sampler(D3DTEXF_POINT, filter_temp.sampler_nn);

        DBG("filter_temp_init: create_sampler LINEAR");
        create_sampler(D3DTEXF_LINEAR, filter_temp.sampler_linear);

        DBG("filter_temp_init: create_sampler WRAP");
        create_sampler(D3DTEXF_POINT, filter_temp.sampler_wrap, D3DTADDRESS_WRAP);

        DBG("filter_temp_init: alloc tex_nn_zero");
        filter_temp.tex_nn_zero = new TextureAndViews{};
        DBG("filter_temp_init: create_tex_and_views_nn ZERO");
        create_tex_and_views_nn(filter_temp.tex_nn_zero, ZERO_WIDTH, ZERO_HEIGHT);

        DBG("filter_temp_init: alloc tex_nn_zx");
        filter_temp.tex_nn_zx = new TextureAndViews{};
        DBG("filter_temp_init: create_tex_and_views_nn ZX");
        create_tex_and_views_nn(filter_temp.tex_nn_zx, ZX_WIDTH, ZX_HEIGHT);

        DBG("filter_temp_init: alloc tex_t2");
        filter_temp.tex_t2 = new TextureAndDepthViews{};
        DBG("filter_temp_init: create_tex_and_depth_views_2 NOISE");
        create_tex_and_depth_views_2(NOISE_WIDTH, NOISE_HEIGHT, filter_temp.tex_t2);

        DBG("filter_temp_init: create_tex_and_view_1_v ZERO_FILTERED");
        create_tex_and_view_1_v(filter_temp.tex_1_zero, ZERO_WIDTH_FILTERED, ZERO_HEIGHT_FILTERED);

        DBG("filter_temp_init: create_tex_and_view_1_v ZX_FILTERED");
        create_tex_and_view_1_v(filter_temp.tex_1_zx, ZX_WIDTH_FILTERED, ZX_HEIGHT_FILTERED);

        DBG("filter_temp_init EXIT");
    }


    void filter_temp_shutdown(IDirect3DDevice9* dev) {
        if (dev) {
            // Unbind RT/DS
            dev->SetRenderTarget(0, nullptr);
            dev->SetDepthStencilSurface(nullptr);

            for (DWORD i = 0; i < 16; i++)
                dev->SetTexture(i, nullptr);
        }

        if (filter_temp.tex_nn_zero) {
            DBGF("filter_temp_shutdown: deleting tex_nn_zero=%p", (void*)filter_temp.tex_nn_zero);
            delete filter_temp.tex_nn_zero;
            filter_temp.tex_nn_zero = nullptr;
        }

        if (filter_temp.tex_nn_zx) {
            DBGF("filter_temp_shutdown: deleting tex_nn_zx=%p", (void*)filter_temp.tex_nn_zx);
            delete filter_temp.tex_nn_zx;
            filter_temp.tex_nn_zx = nullptr;
        }

        if (filter_temp.tex_t2) {
            DBGF("filter_temp_shutdown: deleting tex_t2=%p", (void*)filter_temp.tex_t2);
            delete filter_temp.tex_t2;
            filter_temp.tex_t2 = nullptr;
        }

        for (auto* tex : filter_temp.tex_1_zero) {
            DBGF("filter_temp_shutdown: deleting tex_1_zero=%p", (void*)tex);
            delete tex;
        }
        filter_temp.tex_1_zero.clear();

        for (auto* tex : filter_temp.tex_1_zx) {
            DBGF("filter_temp_shutdown: deleting tex_1_zx=%p", (void*)tex);
            delete tex;
        }
        filter_temp.tex_1_zx.clear();

        // Don’t need filter_temp = {} anymore; explicitly nulled/cleared.
    }


    struct LinearFilterConditions {
        PIXEL_SHADER_ALPHA_DISCARD alpha_discard;
        std::vector<const D3DSAMPLER_DESC*> samplers_descs;
        std::vector<const D3DSURFACE_DESC*> texs_descs;
    }linear_conditions = {};

    friend class LogItem<LinearFilterConditions>;

#define MAX_LINEAR_RTVS 32
    struct LinearRtv {
        UINT width;
        UINT height;
        IDirect3DSurface9* rtv;
        ~LinearRtv() {
            if (rtv) rtv->Release();
            rtv = NULL;
        }
    };
    std::deque<LinearRtv> linear_rtvs;

    bool linear_restore = false;
    bool stream_out = false;

    DWORD saved_mag0 = 0;
    bool  saved_mag0_valid = false;

    void linear_conditions_begin() { 
        // NOTE:
// This block previously crashed due to dynamic_cast on raw IDirect3DBaseTexture9*.
// D3D9 sampler/texture state here is *not* wrapper-owned.
// now use QI -> IDirect3DTexture9 + GetLevelDesc for slot 0 only.
// Do NOT reintroduce wrapper casts or store desc pointers here.

        linear_restore = false;
        stream_out = false;
        linear_conditions = {};
        if (!render_linear && !LOG_STARTED) {
            return;
        }

        linear_conditions.alpha_discard = PIXEL_SHADER_ALPHA_DISCARD::NONE;

        if (cached_ps) {
            IDirect3DPixelShader9* ps_inner = cached_ps; // cached_ps must be IDirect3DPixelShader9*
            auto it = cached_pss_map.find(ps_inner);
            if (it != cached_pss_map.end() && it->second) {
                linear_conditions.alpha_discard = it->second->get_alpha_discard();
            }
        }

        if (!cached_pssrvs) {
            return;
        }
        int i = 0;
        for (auto srvs = cached_pssrvs; *srvs; ++srvs, ++i) {
            if (i >= MAX_SAMPLERS) {
                break;
            }
            IDirect3DTexture9* tex = nullptr;
            D3DSURFACE_DESC d = {};

            HRESULT hr_qi = (*srvs)->QueryInterface(IID_IDirect3DTexture9, (void**)&tex);
            if (SUCCEEDED(hr_qi) && tex)
            {
                if (SUCCEEDED(tex->GetLevelDesc(0, &d)))
                {
                    // Only care about slot 0 (matches previous texs_descs[0] usage)
                    if (i == 0 &&
                        linear_conditions.alpha_discard == PIXEL_SHADER_ALPHA_DISCARD::EQUAL &&
                        render_linear)
                    {
                        const UINT width = d.Width;
                        const UINT height = d.Height;

                        if (!((width == 512 || width == 256) && height == 256) &&
                            !(width == (UINT)linear_test_width && height == (UINT)linear_test_height))
                        {
                            if (!saved_mag0_valid) {
                                DWORD v = 0;
                                if (SUCCEEDED(inner->GetSamplerState(0, D3DSAMP_MAGFILTER, &v))) {
                                    saved_mag0 = v;
                                    saved_mag0_valid = true;
                                }
                            }
                            inner->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                            linear_restore = true;

                        }
                    }
                }
                tex->Release();
                tex = nullptr;
            }
            // IMPORTANT: do not store &d anywhere (it's stack).
            // If texs_descs later, push nullptr now.
            linear_conditions.texs_descs.push_back(nullptr);
        }
    }

    void linear_conditions_end() {

        if (linear_restore && saved_mag0_valid) {
            inner->SetSamplerState(0, D3DSAMP_MAGFILTER, saved_mag0);
        }
        linear_restore = false;
        saved_mag0_valid = false;

        if (linear_restore) {
            for (int i = 0; i < MAX_SAMPLERS; ++i) {
            }
        }
        if (stream_out) {
            // Direct3D 9 does not have geometry shaders or stream output
            // Adapt this part according to specific needs if stream output is required
        }
    }



    struct Size {
        UINT sc_width;
        UINT sc_height;
        UINT render_width;
        UINT render_height;

        void resize(UINT width, UINT height) {
            sc_width = width;
            sc_height = height;
            render_width = sc_height * 4 / 3;
            render_height = sc_height;
        }
    } cached_size = {}, render_size = {};

#define D3D9_MAX_VERTEX_BUFFERS 16 // Adjust this value as necessary

    struct VertexBuffers {
        IDirect3DVertexBuffer9* ppVertexBuffers[D3D9_MAX_VERTEX_BUFFERS];
        MyIDirect3DVertexBuffer9* vertex_buffers[D3D9_MAX_VERTEX_BUFFERS];
        UINT pStrides[D3D9_MAX_VERTEX_BUFFERS];
        UINT pOffsets[D3D9_MAX_VERTEX_BUFFERS];
    } cached_vbs = {};

    IDirect3DVertexBuffer9* so_bt = nullptr;
    IDirect3DVertexBuffer9* so_bs = nullptr;
    IDirect3DVertexShader9* cached_vs = NULL;
    IDirect3DPixelShader9* cached_ps = NULL;
    IDirect3DVertexBuffer9* cached_vb = NULL;
    D3DPRIMITIVETYPE cached_pt = D3DPT_TRIANGLESTRIP;
    D3DVERTEXELEMENT9 cached_il[MAXD3DDECLLENGTH + 1] = {};
    float* render_pscbs[MAX_CONSTANT_BUFFERS];  
    IDirect3DTexture9* render_pssrvs[MAX_SHADER_RESOURCES] = {};  
    IDirect3DSurface9* cached_rtv = NULL;
    IDirect3DSurface9* cached_dsv = NULL;
    IDirect3DSamplerState* cached_pssss[MAX_SAMPLERS] = {};
    IDirect3DSamplerState* render_pssss[MAX_SAMPLERS] = {};
    D3DVIEWPORT9 render_vp = {};
    D3DVIEWPORT9 cached_vp = {};
    bool is_render_vp = false;

    void present() {
        clear_filter();
        update_config();
        ++frame_count;
    }

    void resize_buffers(UINT width, UINT height) {
        char b[256];
        sprintf(b, "[ZeroMod] Impl::resize_buffers ENTER w=%u h=%u inner=%p overlay=%p config=%p",
            width, height, (void*)inner, (void*)overlay, (void*)config);
        OutputDebugStringA(b);

        // Guard: D3D9 can pass 0 for "auto"
        if (width == 0 || height == 0) {
            OutputDebugStringA("[ZeroMod] resize_buffers: width/height is 0 -> resolving from backbuffer");
            IDirect3DSurface9* bb = nullptr;
            HRESULT hr = inner ? inner->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb) : E_POINTER;
            sprintf(b, "[ZeroMod] GetBackBuffer hr=0x%08lX bb=%p", (unsigned long)hr, (void*)bb);
            OutputDebugStringA(b);

            if (SUCCEEDED(hr) && bb) {
                D3DSURFACE_DESC d{};
                hr = bb->GetDesc(&d);
                sprintf(b, "[ZeroMod] BB GetDesc hr=0x%08lX w=%u h=%u fmt=%d",
                    (unsigned long)hr, (unsigned)d.Width, (unsigned)d.Height, (int)d.Format);
                OutputDebugStringA(b);
                if (SUCCEEDED(hr)) { width = d.Width; height = d.Height; }
                bb->Release();
            }
            if (width == 0 || height == 0) {
                OutputDebugStringA("[ZeroMod] resize_buffers ABORT: resolved size still 0");
                return;
            }
        }

        OutputDebugStringA("[ZeroMod] resize_buffers step 1: render_size.resize");
        render_size.resize(width, height);

        OutputDebugStringA("[ZeroMod] resize_buffers step 2: clear_filter");
        clear_filter();
        OutputDebugStringA("[ZeroMod] resize_buffers step 2: clear_filter DONE");

        OutputDebugStringA("[ZeroMod] resize_buffers step 3: update_config");
        update_config();
        OutputDebugStringA("[ZeroMod] resize_buffers step 3: update_config DONE");

        OutputDebugStringA("[ZeroMod] resize_buffers step 4: filter_temp_init");
        filter_temp_init();
        OutputDebugStringA("[ZeroMod] resize_buffers step 4: filter_temp_init DONE");

        frame_count = 0;
        OutputDebugStringA("[ZeroMod] resize_buffers EXIT");
    }

    static inline void DBGf(const char* fmt, ...)
    {
        char buf[1024];
        va_list va;
        va_start(va, fmt);
        _vsnprintf(buf, sizeof(buf), fmt, va);
        va_end(va);
        buf[sizeof(buf) - 1] = '\0';
        OutputDebugStringA(buf);
        OutputDebugStringA("\n");
    }

    void on_pre_reset() {
        DBG("Impl::on_pre_reset ENTER");

        if (inner) {
            IDirect3DSurface9* bb = nullptr;
            if (SUCCEEDED(inner->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) && bb) {
                inner->SetRenderTarget(0, bb);
                bb->Release();
            }
            inner->SetDepthStencilSurface(nullptr);

            for (UINT i = 0; i < 16; i++) {
                inner->SetTexture(i, nullptr);
            }

            inner->SetVertexShader(nullptr);
            inner->SetPixelShader(nullptr);

            for (UINT i = 0; i < 16; i++) {
                inner->SetStreamSource(i, nullptr, 0, 0);
            }
            for (UINT i = 1; i < 4; i++) inner->SetRenderTarget(i, nullptr);

            inner->SetIndices(nullptr);
        }

    
        DBGf("pre_reset: d3d9_2d=%p", (void*)d3d9_2d);
        if (d3d9_2d) {
            if (!ptr_readable(d3d9_2d, sizeof(*d3d9_2d))) {
                DBGF("pre_reset: d3d9_2d NOT READABLE -> skipping free, nulling (%p)", (void*)d3d9_2d);
                d3d9_2d = nullptr;
            }
            else {
                DBGF("pre_reset: d3d9_2d readable -> freeing (%p)", (void*)d3d9_2d);
                ZeroMod::d3d9_gfx_free(d3d9_2d);
                d3d9_2d = nullptr;
            }
        }

        DBGf("[ZeroMod] pre_reset: free d3d9_gba=%p", (void*)d3d9_gba);
        if (d3d9_gba) { ZeroMod::d3d9_gfx_free(d3d9_gba); d3d9_gba = nullptr; }

        DBGf("[ZeroMod] pre_reset: free d3d9_ds=%p", (void*)d3d9_ds);
        if (d3d9_ds) { ZeroMod::d3d9_gfx_free(d3d9_ds); d3d9_ds = nullptr; }

        DBG("pre_reset: clear_filter ENTER");
        clear_filter();
        DBG("pre_reset: clear_filter EXIT");

        DBG("pre_reset: filter_temp_shutdown ENTER");
        filter_temp_shutdown(inner);
        DBG("pre_reset: filter_temp_shutdown EXIT");

        DBGf("[ZeroMod] pre_reset: release so_bt=%p", (void*)so_bt);
        if (so_bt) { so_bt->Release(); so_bt = nullptr; }

        DBGf("[ZeroMod] pre_reset: release cached_rtv=%p", (void*)cached_rtv);
        if (cached_rtv) { cached_rtv->Release(); cached_rtv = nullptr; }

        DBGf("[ZeroMod] pre_reset: release cached_dsv=%p", (void*)cached_dsv);
        if (cached_dsv) { cached_dsv->Release(); cached_dsv = nullptr; }

        DBGf("[ZeroMod] pre_reset: release cached_dss=%p", (void*)cached_dss);
        if (cached_dss) { cached_dss->Release(); cached_dss = nullptr; }

        DBG("Impl::on_pre_reset EXIT");
    }



    void on_post_reset(D3DPRESENT_PARAMETERS* pp) {
        DBG("Impl::on_post_reset ENTER");

        UINT w = pp ? pp->BackBufferWidth : 0;
        UINT h = pp ? pp->BackBufferHeight : 0;

        // Rebuild  render_size, filter temps, etc
        resize_buffers(w, h);

        // Recreate default-pool VB
        if (!so_bt) {
            HRESULT hr = inner->CreateVertexBuffer(
                SO_B_LEN, D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &so_bt, NULL
            );
            if (FAILED(hr)) {
                char b[128];
                sprintf(b, "[ZeroMod] post_reset: CreateVertexBuffer so_bt FAIL hr=0x%08lX\n", (unsigned long)hr);
                OutputDebugStringA(b);
            }
        }

        DBG("Impl::on_post_reset EXIT");
    }



    void set_render_vp() {
        if (need_render_vp) {
            if (!is_render_vp) {
                if (cached_vp.Width && cached_vp.Height) {
                    render_vp = D3DVIEWPORT9{
                        (DWORD)(cached_vp.X * render_width / render_orig_width),
                        (DWORD)(cached_vp.Y * render_height / render_orig_height),
                        (DWORD)(cached_vp.Width * render_width / render_orig_width),
                        (DWORD)(cached_vp.Height * render_height / render_orig_height),
                        cached_vp.MinZ,
                        cached_vp.MaxZ
                    };
                    inner->SetViewport(&render_vp);
                    {
                        char b[256];
                        _snprintf(b, sizeof(b),
                            "[ZeroMod] set_render_vp: APPLY vp=%lux%lu\n",
                            (unsigned long)render_vp.Width, (unsigned long)render_vp.Height);
                        OutputDebugStringA(b);
                    }
                }
                is_render_vp = true;
            }
        }

        if (!LOG_STARTED) return;

        size_t srvs_n = 0;
        auto srvs = cached_pssrvs;
        while (srvs_n < MAX_SHADER_RESOURCES && *srvs) {
            ++srvs;
            ++srvs_n;
        }
    }

    void reset_render_vp() {
        if (need_render_vp && is_render_vp) {
            if (cached_vp.Width && cached_vp.Height) {
                render_vp = cached_vp;
                inner->SetViewport(&render_vp);
            }
            is_render_vp = false;
        }
    }

    Impl(IDirect3DDevice9** inner, UINT width, UINT height)
        : inner(*inner), width(width), height(height)
    {
        memset(cached_stage_tex, 0, sizeof(cached_stage_tex));
        cached_stage_tex[ZM_MAX_TEX_STAGES] = nullptr;
        cached_pssrvs = cached_stage_tex;

        DBG("Impl::ctor ENTER");
        // If *inner is bad/null, you'll see it before the crash.
        if (!this->inner) {
            DBG("Impl::ctor ERROR: inner is NULL");
            return; // or throw / handle as needed
        }

        cached_hwnd = NULL;

        // 1) Best: pull hDeviceWindow from swapchain present params
        IDirect3DSwapChain9* sc0 = nullptr;
        if (SUCCEEDED(this->inner->GetSwapChain(0, &sc0)) && sc0) {
            D3DPRESENT_PARAMETERS spp = {};
            if (SUCCEEDED(sc0->GetPresentParameters(&spp))) {
                cached_hwnd = spp.hDeviceWindow;
            }
            sc0->Release();
        }

        // 2) Fallback: creation params only has hFocusWindow
        if (!cached_hwnd) {
            D3DDEVICE_CREATION_PARAMETERS cp = {};
            HRESULT hrCP = this->inner->GetCreationParameters(&cp);
            if (SUCCEEDED(hrCP) && cp.hFocusWindow) {
                cached_hwnd = cp.hFocusWindow;
            }
        }

        // Debug once
        char buf[256];
        _snprintf(buf, sizeof(buf),
            "[ZeroMod] Impl::ctor cached_hwnd=%p\n",
            (void*)cached_hwnd
        );
        OutputDebugStringA(buf);
        DBG("Impl::ctor step 1: calling resize_buffers");

        resize_buffers(width, height);
        DBG("Impl::ctor step 1 OK: resize_buffers returned");

        DBG("Impl::ctor step 2: init render_pssrvs array");
        for (int i = 0; i < MAX_SHADER_RESOURCES; ++i) {
            render_pssrvs[i] = nullptr;
        }
        DBG("Impl::ctor step 2 OK");

        DBG("Impl::ctor step 3: init render_pssss array");
        for (int i = 0; i < MAX_SAMPLERS; ++i) {
            render_pssss[i] = nullptr;
        }
        DBG("Impl::ctor step 3 OK");

        DBG("Impl::ctor step 4: CreateVertexBuffer so_bt (DEFAULT)");
        HRESULT result = this->inner->CreateVertexBuffer(
            SO_B_LEN, D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &so_bt, NULL
        );
        if (FAILED(result)) {
            DBG("Impl::ctor FAIL: CreateVertexBuffer so_bt (DEFAULT)");
            return;
        }
        DBG("Impl::ctor step 4 OK");

        DBG("Impl::ctor step 5: CreateVertexBuffer so_bs (SYSTEMMEM)");
        result = this->inner->CreateVertexBuffer(
            SO_B_LEN, D3DUSAGE_WRITEONLY, 0, D3DPOOL_SYSTEMMEM, &so_bs, NULL
        );
        if (FAILED(result)) {
            DBG("Impl::ctor FAIL: CreateVertexBuffer so_bs (SYSTEMMEM)");
            return;
        }
        DBG("Impl::ctor step 5 OK");

        DBG("Impl::ctor EXIT OK");
    }

    ~Impl() {
        DBG("Impl::dtor ENTER");

        for (DWORD i = 0; i < ZM_MAX_TEX_STAGES; ++i) {
            if (cached_stage_tex[i]) {
                cached_stage_tex[i]->Release();
                cached_stage_tex[i] = nullptr;
            }
        }
        cached_stage_tex[ZM_MAX_TEX_STAGES] = nullptr;


        filter_temp_shutdown(inner);
        clear_filter();

        for (int i = 0; i < MAX_CONSTANT_BUFFERS; ++i) {
            delete[] render_pscbs[i];
        }

        for (int i = 0; i < MAX_SHADER_RESOURCES; ++i) {
            if (render_pssrvs[i]) {
                render_pssrvs[i]->Release();
                render_pssrvs[i] = nullptr;
            }
        }

        for (int i = 0; i < MAX_SAMPLERS; ++i) {
            if (render_pssss[i]) {
                render_pssss[i]->Release();
                render_pssss[i] = nullptr;
            }
        }

        if (so_bt) so_bt->Release();
        if (so_bs) so_bs->Release();

        DBG("Impl::dtor EXIT");
    }


    bool Draw(UINT VertexCount, UINT StartVertexLocation){
        // ---- ZM DEBUG HELPERS (Draw gating breadcrumbs) ----
        static unsigned long zm_draw_seq = 0;
        unsigned long zm_seq = ++zm_draw_seq;

        auto zm_ret = [&](const char* why) -> bool {
            char b[512];
            _snprintf(b, sizeof(b), "[ZeroMod][Draw #%lu] EARLY RETURN: %s\n", zm_seq, why);
            OutputDebugStringA(b);
            return false;
            };

        // ----------------------------------------------------
        set_render_vp();
        // --- Save REAL device state ---
        IDirect3DBaseTexture9* saved_stage0 = nullptr;
        IDirect3DSurface9* saved_rt0 = nullptr;
        IDirect3DSurface9* saved_ds = nullptr;
        D3DVIEWPORT9            saved_vp = {};
        IDirect3DVertexShader9* saved_vs = nullptr;
        IDirect3DVertexBuffer9* saved_vb0 = nullptr;
        UINT saved_vb0_off = 0, saved_vb0_stride = 0;

        auto zm_cleanup_snapshot = [&] {
            if (saved_stage0) { saved_stage0->Release(); saved_stage0 = nullptr; }
            if (saved_rt0) { saved_rt0->Release();    saved_rt0 = nullptr; }
            if (saved_ds) { saved_ds->Release();     saved_ds = nullptr; }
            if (saved_vs) { saved_vs->Release();     saved_vs = nullptr; }
            if (saved_vb0) { saved_vb0->Release();    saved_vb0 = nullptr; }
            };

        if (inner) {
            inner->GetTexture(0, &saved_stage0);               // AddRef'd by D3D9
            inner->GetRenderTarget(0, &saved_rt0);             // AddRef'd by D3D9
            inner->GetDepthStencilSurface(&saved_ds);          // AddRef'd by D3D9
            inner->GetViewport(&saved_vp);
            inner->GetVertexShader(&saved_vs);                 // AddRef'd by D3D9
            inner->GetStreamSource(0, &saved_vb0, &saved_vb0_off, &saved_vb0_stride); // AddRef'd
        }

        bool filter_next = false;
        bool filter_ss = false;
        bool filter_ss_gba = false;
        bool filter_ss_ds = false;

        filter_ss = d3d9_2d && d3d9_2d->shader_preset;
        filter_ss_gba = d3d9_gba && d3d9_gba->shader_preset;
        filter_ss_ds = d3d9_ds && d3d9_ds->shader_preset;

        auto set_filter_state_ps = [this] {
            inner->SetPixelShader(filter_state.ps->get_inner());
            };
        auto restore_ps = [&] {
           // inner->SetPixelShader(nullptr);
            };
        auto restore_vps = [&] {
            inner->SetViewport(&saved_vp);
            };
        auto restore_rtvs = [&] {
            inner->SetRenderTarget(0, saved_rt0);
            inner->SetDepthStencilSurface(saved_ds);
            };

        auto restore_pssrvs = [&] {
            inner->SetTexture(0, saved_stage0);
            };

        auto set_viewport = [this](UINT width, UINT height) {
            D3DVIEWPORT9 viewport = { 0, 0, width, height, 0, 1 };
            inner->SetViewport(&viewport);
            };
        auto set_srv = [this](IDirect3DTexture9* srv) {
            inner->SetTexture(0, srv);
            };
        auto set_rtv = [this](IDirect3DSurface9* rtv, IDirect3DSurface9* dsv = NULL) {
            inner->SetRenderTarget(0, rtv);
            if (dsv) inner->SetDepthStencilSurface(dsv);
            };

        IDirect3DBaseTexture9* base0 = saved_stage0; // REAL device state snapshot
        if (!base0) {
            zm_cleanup_snapshot();
            return zm_ret("stage0 null (no texture bound on stage 0)");
        }

        IDirect3DTexture9* src_tex = nullptr;
        HRESULT hr_qi = base0->QueryInterface(IID_IDirect3DTexture9, (void**)&src_tex);
        if (FAILED(hr_qi) || !src_tex) {
            zm_cleanup_snapshot();
            return zm_ret("stage0 texture is not IDirect3DTexture9 (QI failed)");
        }

        // QI succeeded => src_tex holds a ref that MUST be released on every exit path after this point.
        auto release_src_tex = [&]() {
            if (src_tex) { src_tex->Release(); src_tex = nullptr; }
            };

        if (!render_interp && !render_linear && !filter_ss && !filter_ss_gba && !filter_ss_ds) {
            release_src_tex();
           zm_cleanup_snapshot();
            return false;
        }

        if (VertexCount != 4) {
            release_src_tex();
            zm_cleanup_snapshot();
            return false;
        }

        filter_next = false;

        D3DSURFACE_DESC srv_desc = {};

        src_tex->GetLevelDesc(0, &srv_desc);

        bool is_zero = (srv_desc.Width == ZERO_WIDTH && srv_desc.Height == ZERO_HEIGHT);
        bool is_zx = (srv_desc.Width == ZX_WIDTH && srv_desc.Height == ZX_HEIGHT);

        bool zx = false;
        if (filter_next) {
            zx = false;
        }
        else if (is_zero) {
            zx = false;
        }
        else if (is_zx) {
            zx = true;
        }
        else {
            release_src_tex();
            zm_cleanup_snapshot();
            return false;
        }

        // --- One-shot scan: arm it only once, from within the in-game size-route tree ---
        if (!g_scan_ever_started) {
            g_scan_ever_started = true;
            g_scan_active = true;
            g_scan_draws = 0;

            g_zx_latched = false;
            g_zx_latch_valid = false;

            // reset evidence for this scan window
            g_scan_create_hit_512 = false;
            g_scan_create_hit_count = 0;

            OutputDebugStringA("[ZeroMod][SCAN] BEGIN (armed)\n");
        }

        if (g_scan_active) {
            g_scan_draws++;

            // latch ZX if CreateTexture saw 512x512 during the armed window
            if (g_scan_create_hit_512) {
                g_zx_latched = true;
                g_scan_active = false;
                g_zx_latch_valid = true;
                OutputDebugStringA("[ZeroMod][SCAN] HIT -> ZX (create latch)\n");
            }
            else if (g_scan_draws >= 600u) { // your draw-based timeout window
                g_scan_active = false;
                g_zx_latch_valid = true;
                OutputDebugStringA("[ZeroMod][SCAN] END -> ZERO\n");
            }
        }

        if (g_zx_latch_valid)
            zx = g_zx_latched;
        {
            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod][Draw #%lu] size-route: zx=%d filter_ss=%d (ss_2d=%d ss_gba=%d ss_ds=%d)\n",
                zm_seq, (int)zx, (int)filter_ss,
                (int)(d3d9_2d && d3d9_2d->shader_preset),
                (int)filter_ss_gba, (int)filter_ss_ds);
            OutputDebugStringA(b);
        }

        auto rtv = cached_rtv;
        if (!rtv) {
            release_src_tex();
            zm_cleanup_snapshot();
            return zm_ret("rtv null (cached_rtv == null)");
        }

        D3DSURFACE_DESC rtv_desc = {};
        cached_rtv->GetDesc(&rtv_desc);

            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod][Draw #%lu] RTV desc: %ux%u fmt=%u (expect %ux%u)\n",
                zm_seq,
                (unsigned)rtv_desc.Width, (unsigned)rtv_desc.Height, (unsigned)rtv_desc.Format,
                (unsigned)(srv_desc.Width * 2), (unsigned)(srv_desc.Height * 2));
            OutputDebugStringA(b);
        
        if (rtv_desc.Width != srv_desc.Width * 2 || rtv_desc.Height != srv_desc.Height * 2) {
            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod][Draw #%lu] EARLY RETURN: RTV not 2x SRV (rtv=%ux%u srv=%ux%u)\n",
                zm_seq,
                (unsigned)rtv_desc.Width, (unsigned)rtv_desc.Height,
                (unsigned)srv_desc.Width, (unsigned)srv_desc.Height);
            OutputDebugStringA(b);
            release_src_tex();
            zm_cleanup_snapshot();
            return false;
        }
// NOTE: filter_state.rtv_tex is a WRAPPER (MyIDirect3DTexture9*), so compare inner texture.
        filter_next = (filter_state.rtv_tex_inner && src_tex == filter_state.rtv_tex_inner);
        {
            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod][Draw #%lu] filter_next(prev_rtv_inner)=%d (src_tex=%p prev_rtv_inner=%p)\n",
                zm_seq, (int)filter_next, (void*)src_tex, (void*)filter_state.rtv_tex_inner);
            OutputDebugStringA(b);
        }
        clear_filter();

        MyID3D9PixelShader* ps_wrap = nullptr;
        if (cached_ps) {
            auto it = cached_pss_map.find(cached_ps);
            if (it != cached_pss_map.end())
                ps_wrap = it->second;
        }

        filter_state.ps = ps_wrap;
        if (filter_state.ps)
            filter_state.ps->AddRef();

        if (ps_wrap) {
             ps_wrap->AddRef();

            DWORD bh = ps_wrap->get_bytecode_hash();
            switch (bh) {
            case PS_HASH_T1: filter_state.t1 = true;  break;
            case PS_HASH_T3: filter_state.t1 = false; break;
            default:
                filter_state.t1 = (ps_wrap->get_bytecode_length() >= PS_BYTECODE_LENGTH_T1_THRESHOLD);
                break;
            }
        }
        filter_state.vs = nullptr; // wrapper metadata not available reliably
        filter_state.zx = zx;
        filter_state.start_vertex_location = StartVertexLocation;
        filter_state.zx = zx;
        filter_state.start_vertex_location = StartVertexLocation;

        // stream 0 snapshot (arrays are always "non-null")
        IDirect3DVertexBuffer9* vb0 = cached_vbs.ppVertexBuffers[0];

        filter_state.vertex_buffer = vb0;
        if (filter_state.vertex_buffer)
            filter_state.vertex_buffer->AddRef();

        filter_state.vertex_stride = cached_vbs.pStrides[0];
        filter_state.vertex_offset = cached_vbs.pOffsets[0];

        if (filter_state.srv_tex) {
            filter_state.srv_tex->Release();
            filter_state.srv_tex = nullptr;
        }
        filter_state.srv_tex = src_tex;
        src_tex->AddRef(); // ALWAYS AddRef for storage, regardless of QI path

        // cached_rtv is a SURFACE. If it's a texture-backed RT, get its container texture.
        IDirect3DTexture9* rtv_tex_inner = nullptr;
        HRESULT hr_ct = cached_rtv->GetContainer(IID_IDirect3DTexture9, (void**)&rtv_tex_inner);

        if (FAILED(hr_ct) || !rtv_tex_inner) {
            OutputDebugStringA("[ZeroMod] EARLY RETURN: RTV surface has no IDirect3DTexture9 container\n");
            release_src_tex(); 
            zm_cleanup_snapshot();
            return false;
        }
        if (filter_state.rtv_tex_inner) {
            filter_state.rtv_tex_inner->Release();
            filter_state.rtv_tex_inner = nullptr;
        }
        filter_state.rtv_tex_inner = rtv_tex_inner; // owns ref from GetContainer
        rtv_tex_inner = nullptr;

        auto draw_nn = [&](TextureAndViews* v) {
            set_viewport(v->width, v->height);
            set_rtv(v->rtv);
            set_srv(src_tex);
            if (render_linear) apply_sampler(inner, 0, filter_temp.sampler_nn);
            inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
            restore_vps();
            restore_rtvs();
            set_srv(v->srv);
            apply_sampler(inner, 0, render_linear ? filter_temp.sampler_linear
                : filter_temp.sampler_nn);
            inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
            restore_pssrvs();
            };

        using ZeroMod::video_viewport_t;
        using ZeroMod::d3d9_video_struct;

        auto draw_ss = [&](
            d3d9_video_struct* d3d9,
            IDirect3DTexture9* src_tex,
            D3DVIEWPORT9* render_vp,
            TextureAndViews* cached_tex = NULL
            ) -> bool
            {
                if (cached_tex) {
                    video_viewport_t vp = {
                        .x = 0,
                        .y = 0,
                        .width = static_cast<int>(cached_tex->width),
                        .height = static_cast<int>(cached_tex->height),
                        .full_width = static_cast<int>(cached_tex->width),
                        .full_height = static_cast<int>(cached_tex->height)
                    };
                    d3d9_update_viewport(d3d9, cached_tex->rtv, &vp);

                    D3DSURFACE_DESC desc{};
                    if (SUCCEEDED(rtv->GetDesc(&desc)) && desc.Width && desc.Height) {
                        UINT out_w = desc.Width;
                        UINT out_h = desc.Height;

                        // Viewport can still come from render_vp if you want,
                        // but clamp it so it can't zero the ctx.
                      UINT vp_w = (render_vp && render_vp->Width) ? (UINT)render_vp->Width : out_w;
                      UINT vp_h = (render_vp && render_vp->Height) ? (UINT)render_vp->Height : out_h;
                        D3DSURFACE_DESC sd{};
                        src_tex->GetLevelDesc(0, &sd);

                        D3DSURFACE_DESC rd{};
                        rtv->GetDesc(&rd);

                        char b[256];
                        _snprintf(b, sizeof(b),
                            "[ZeroMod] SLANG INPUT: src=%p %ux%u fmt=%u | dst_rtv=%p %ux%u fmt=%u\n",
                            (void*)src_tex, (unsigned)sd.Width, (unsigned)sd.Height, (unsigned)sd.Format,
                            (void*)rtv, (unsigned)rd.Width, (unsigned)rd.Height, (unsigned)rd.Format);
                        OutputDebugStringA(b);

                        ZeroMod::slang_d3d9_set_frame_ctx(
                            d3d9,
                            src_tex,
                            rtv,
                            out_w, out_h,
                            vp_w, vp_h,
                            this->frame_count
                        );

                    }
                    else {
                     //   OutputDebugStringA("[ZeroMod] draw_ss: RTV GetDesc failed or zero size\n");
                        return false;
                    }

                    ZeroMod::slang_d3d9_runtime_tick(d3d9);

                    bool ok = ZeroMod::slang_d3d9_apply_pass0(d3d9);

                    if (ok) return true;
                    return false;
                }
                else {
                    video_viewport_t vp = {
                        .x = static_cast<int>(render_vp->X),
                        .y = static_cast<int>(render_vp->Y),
                        .width = static_cast<int>(render_vp->Width),
                        .height = static_cast<int>(render_vp->Height),
                        .full_width = static_cast<int>(this->render_size.sc_width),
                        .full_height = static_cast<int>(this->render_size.sc_height)
                    };
                    d3d9_update_viewport(d3d9, rtv, &vp);

                    D3DSURFACE_DESC desc{};
                    if (SUCCEEDED(rtv->GetDesc(&desc)) && desc.Width && desc.Height) {
                        UINT out_w = desc.Width;
                        UINT out_h = desc.Height;

                        // Viewport can still come from render_vp if you want,
                        // but clamp it so it can't zero the ctx.
                      UINT vp_w = (render_vp && render_vp->Width) ? (UINT)render_vp->Width : out_w;
                      UINT vp_h = (render_vp && render_vp->Height) ? (UINT)render_vp->Height : out_h;

                        D3DSURFACE_DESC sd{};
                        src_tex->GetLevelDesc(0, &sd);

                        D3DSURFACE_DESC rd{};
                        rtv->GetDesc(&rd);

                        char b[256];
                        _snprintf(b, sizeof(b),
                            "[ZeroMod] SLANG INPUT: src=%p %ux%u fmt=%u | dst_rtv=%p %ux%u fmt=%u\n",
                            (void*)src_tex, (unsigned)sd.Width, (unsigned)sd.Height, (unsigned)sd.Format,
                            (void*)rtv, (unsigned)rd.Width, (unsigned)rd.Height, (unsigned)rd.Format);
                        OutputDebugStringA(b);

                        ZeroMod::slang_d3d9_set_frame_ctx(
                            d3d9,
                            src_tex,
                            rtv,
                            out_w, out_h,
                            vp_w, vp_h,
                            this->frame_count
                        );

                    }
                    else {

                        return false;
                    }

                    ZeroMod::slang_d3d9_runtime_tick(d3d9);

                    bool ok = ZeroMod::slang_d3d9_apply_pass0(d3d9);

                    if (ok) return true;
                    return false;
                }
            };

        auto draw_enhanced = [&](std::vector<TextureViewsAndBuffer*>& v_v) {
            auto v_it = v_v.begin();

            // If there are no intermediate passes, just do a final linear draw.
            if (v_it == v_v.end()) {
                apply_sampler(inner, 0, filter_temp.sampler_linear);
                set_ps_size_constants(srv_desc.Width, srv_desc.Height);
                inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                zm_cleanup_snapshot();
                return;
            }

            set_filter_state_ps();

            // PASS 0: source = game's original SRV, dest = first intermediate RTV
            TextureViewsAndBuffer* cur = *v_it;

            set_viewport(cur->width, cur->height);
            set_rtv(cur->rtv);
            set_srv(src_tex);
            set_ps_size_constants(srv_desc.Width, srv_desc.Height); // <-- IMPORTANT
            inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

            // PASS 1..N: source = previous intermediate, dest = next intermediate
            TextureViewsAndBuffer* prev = cur;
            while (++v_it != v_v.end()) {
                cur = *v_it;

                set_viewport(cur->width, cur->height);
                set_rtv(cur->rtv);
                set_srv(prev->srv);
                set_ps_size_constants(prev->width, prev->height);
                inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

                prev = cur;
            }

            // FINAL BLIT: source = last intermediate -> dest = original cached RT
            restore_ps();
            restore_vps();
            restore_rtvs();

            set_srv(prev->srv);
            apply_sampler(inner, 0, filter_temp.sampler_linear);
            set_ps_size_constants(prev->width, prev->height);
            inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

            restore_pssrvs();
            };

        DWORD h = zm_ps_hash(cached_ps);
        bool is_type1_exact = (h == PS_HASH_T1);

        if (is_type1_exact) {
            bool is_zero = (srv_desc.Width == ZERO_WIDTH && srv_desc.Height == ZERO_HEIGHT);
            bool is_zx = (srv_desc.Width == ZX_WIDTH && srv_desc.Height == ZX_HEIGHT);

            if (!is_zero && !is_zx) {
                OutputDebugStringA("[ZeroMod] SLANG BLOCK: reject (not ZERO/ZX size)\n");
                release_src_tex();
                zm_cleanup_snapshot();
                return false;
            }

            filter_state.zx = is_zx;   // <-- set FIRST

            auto d3d9 = d3d9_2d;
            bool want_slang = false;

            if (filter_state.zx) {
                if (filter_ss_ds) { d3d9 = d3d9_ds; want_slang = true; }
            }
            else {
                if (filter_ss_gba) { d3d9 = d3d9_gba; want_slang = true; }
            }

            if (!want_slang && filter_ss) { d3d9 = d3d9_2d; want_slang = true; }

            if (!want_slang) {
                OutputDebugStringA("[ZeroMod] SLANG BLOCK: no matching preset for this mode\n");
                release_src_tex();
                zm_cleanup_snapshot();
                return false;
            }

            TextureAndViews* dst = NULL;

            bool handled = draw_ss(d3d9, src_tex, &render_vp, dst);

            if (handled) {
              //  OutputDebugStringA("[ZeroMod] draw_ss handled\n");

                // If slang rendered into an intermediate RT, we MUST blit it to the real RT
                if (dst) {
                 //   OutputDebugStringA("[ZeroMod] slang->intermediate: final blit to cached_rtv\n");

                    restore_ps();      // restore game PS (slang likely changed it)
                    restore_vps();
                    restore_rtvs();

                    set_srv(dst->srv);
                    apply_sampler(inner, 0, filter_temp.sampler_linear);
                    inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

                    restore_pssrvs();
                }
                release_src_tex();
                zm_cleanup_snapshot();
                return true;
            }
            else {
                OutputDebugStringA("[ZeroMod] draw_ss returned FALSE -> falling back to type1/enhanced/linear\n");
            }

            if (render_enhanced) {
                draw_enhanced(filter_state.zx ? filter_temp.tex_1_zx : filter_temp.tex_1_zero);
            }
            else {
                apply_sampler(inner, 0, filter_temp.sampler_linear);
                inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
            }
        }
        else {
            draw_nn(filter_state.zx ? filter_temp.tex_nn_zx : filter_temp.tex_nn_zero);
        }
        if (filter_next) {
            DWORD bytecode_hash = zm_ps_hash(cached_ps);
            switch (bytecode_hash) {
            case PS_HASH_T3:
                if (filter_state.srv_tex != src_tex){
                    inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                    zm_cleanup_snapshot();
                    return false;
                }
                /* fall-through */
            case PS_HASH_T1:
                if (!render_linear) {
                    inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                    zm_cleanup_snapshot();
                    return false;
                }
                apply_sampler(inner, 0, filter_temp.sampler_nn);
                inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                break;
            case PS_HASH_T2:
                if (!(
                    (render_interp || render_linear) &&
                    filter_state.ps && filter_state.vs &&
                    filter_state.vertex_buffer
                    )) {
                    inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                    zm_cleanup_snapshot();
                    return false;
                }
                if (render_interp) {
                    inner->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR); // Linear filter
                    inner->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR); // Linear filter
                    inner->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP); // Wrap mode
                    inner->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP); // Wrap mode
                }
                else {
                    inner->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT); // or appropriate filter
                    inner->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT); // or appropriate filter
                }

                if (render_linear) {
                    set_viewport(filter_temp.tex_t2->width, filter_temp.tex_t2->height);
                    set_rtv(filter_temp.tex_t2->rtv, filter_temp.tex_t2->ds);
                }
                inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

                if (render_linear) {
                    restore_rtvs();
                    restore_vps();
                    set_srv(filter_temp.tex_t2->srv);
                    apply_sampler(inner, 0, filter_temp.sampler_linear);
                    set_filter_state_ps();

                    inner->SetVertexShader(filter_state.vs);
                    inner->SetStreamSource(
                        0,
                        filter_state.vertex_buffer,
                        filter_state.vertex_offset,
                        filter_state.vertex_stride
                    );
                    inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                    inner->SetStreamSource(
                        0,
                        cached_vbs.ppVertexBuffers[0],
                        cached_vbs.pOffsets[0],
                        cached_vbs.pStrides[0]
                    );
                    inner->SetVertexShader(cached_vs);
                    restore_ps();
                    restore_pssrvs();
                }
                break;

            default: {
                linear_conditions_begin();
                inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                linear_conditions_end();
                zm_cleanup_snapshot();
                return false;
            }
            }
        }            
        release_src_tex(); 
        zm_cleanup_snapshot();
        return false;
    }
};


// Section 1: Initialization and Core Functions

void MyID3D9Device::set_overlay(Overlay* overlay) {
    impl->overlay = overlay;
}

void MyID3D9Device::set_config(Config* config) {
    impl->config = config;
}

void MyID3D9Device::resize_buffers(UINT width, UINT height) {
    // Resize the back buffer if necessary
    impl->resize_buffers(width, height);
}

void MyID3D9Device::resize_orig_buffers(UINT width, UINT height) {
    impl->cached_size.resize(width, height);
}

IDirect3DDevice9* MyID3D9Device::get_inner() const {
    return (impl && impl->inner) ? impl->inner : nullptr;
}

//IUNKNOWN_IMPL(MyID3D9Device, IDirect3DDevice9)

MyID3D9Device::MyID3D9Device(
    IDirect3DDevice9** inner,
    UINT width,
    UINT height
)
    : impl(nullptr)
{
    DBG("MyID3D9Device ctor(IDirect3DDevice9**...) ENTER (about to new Impl)");

    impl = new Impl(inner, width, height);

    DBG("MyID3D9Device ctor: Impl created OK (about to init xorshift)");

    if (!xorshift128p_state_init_status) {
        void* key[] = { this, impl };
        MurmurHash3_x86_128(
            &key,
            sizeof(key),
            (uintptr_t)*inner,
            xorshift128p_state
        );
        xorshift128p_state_init_status = true;
    }

    DBG("MyID3D9Device ctor: xorshift init OK (about to replace *inner)");

    *inner = this;

    DBG("MyID3D9Device ctor: *inner replaced with wrapper (EXIT)");
}



MyID3D9Device::MyID3D9Device(IDirect3DDevice9* pOriginal) {

    DBG("MyID3D9Device ctor(IDirect3DDevice9*) ENTER");

    impl = new Impl(&pOriginal, 0, 0); // Initialize with default width and height
}

MyID3D9Device::~MyID3D9Device() {
    if (m_pD3DDevice) {
        m_pD3DDevice->Release();
        m_pD3DDevice = nullptr;
    }

    if (impl) {
        delete impl;
        impl = nullptr;
    }
}

MyIDirect3DDevice9::~MyIDirect3DDevice9() {
    if (impl) {
        impl->Release();
        impl = nullptr;
    }
}



// Section 2: Shader and Buffer Management

HRESULT STDMETHODCALLTYPE MyID3D9Device::VSSetConstantBuffers(
    UINT StartSlot,
    UINT NumBuffers,
    IDirect3DVertexBuffer9* const* ppConstantBuffers
) {
    // DirectX 9 does not have a direct equivalent for setting constant buffers.
    // We need to adapt this functionality to set vertex shader constants manually if needed.
    for (UINT i = 0; i < NumBuffers; ++i) {
        if (ppConstantBuffers[i]) {
            float* pData = nullptr;
            if (SUCCEEDED(ppConstantBuffers[i]->Lock(0, 0, (void**)&pData, 0))) {
                // Example: Set the data to the vertex shader constant registers.
                // The number of registers and the structure of the data must be known.
                // impl->inner->SetVertexShaderConstantF(StartSlot + i, pData, numberOfRegisters);
                ppConstantBuffers[i]->Unlock();
            }
        }
    }

    return S_OK;  // Ensure the function always returns S_OK at the end.
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetTexture(
    DWORD Stage,
    IDirect3DBaseTexture9* pTexture
) {
    if (!impl || !impl->inner)
        return D3DERR_INVALIDCALL;

    // Always call through first (so we don't desync on failure).
    HRESULT hr = impl->inner->SetTexture(Stage, pTexture);

    if (SUCCEEDED(hr) && Stage < Impl::ZM_MAX_TEX_STAGES) {
        // Keep our own ref to whatever the app bound.
        if (pTexture)
            pTexture->AddRef();

        if (impl->cached_stage_tex[Stage])
            impl->cached_stage_tex[Stage]->Release();

        impl->cached_stage_tex[Stage] = pTexture;

        // keep sentinel intact for loops that expect null-termination
        impl->cached_stage_tex[Impl::ZM_MAX_TEX_STAGES] = nullptr;
    }

    return hr;
}

// Section 3: Sampler and Render State Management

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetSamplerState(
    DWORD Sampler,
    D3DSAMPLERSTATETYPE Type,
    DWORD Value
) {
    HRESULT hr = impl->inner->SetSamplerState(Sampler, Type, Value);
    ZM_LOG_UNSUP("MyID3D9Device::SetSamplerState", hr);

#ifdef ENABLE_SAMPLER_STATE_CACHE
    impl->cached_sampler_states[Sampler][Type] = Value;
#endif

    return hr;
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::SetVertexShader(
    IDirect3DVertexShader9* pVertexShader
) {
    impl->cached_vs = pVertexShader;
    HRESULT hr = impl->inner->SetVertexShader(pVertexShader);
    ZM_LOG_UNSUP("MyID3D9Device::SetVertexShader", hr);
    return hr;
}
 
// Section 4: Enum and Struct Logging

// Enum definition for PIXEL_SHADER_ALPHA_DISCARD
#define ENUM_CLASS PIXEL_SHADER_ALPHA_DISCARD
const ENUM_MAP(ENUM_CLASS) PIXEL_SHADER_ALPHA_DISCARD_ENUM_MAP = {
    ENUM_CLASS_MAP_ITEM(UNKNOWN),
    ENUM_CLASS_MAP_ITEM(NONE),
    ENUM_CLASS_MAP_ITEM(EQUAL),
    ENUM_CLASS_MAP_ITEM(LESS),
    ENUM_CLASS_MAP_ITEM(LESS_OR_EQUAL),
};
#undef ENUM_CLASS

// Template specialization for logging PIXEL_SHADER_ALPHA_DISCARD enum
template<>
struct LogItem<PIXEL_SHADER_ALPHA_DISCARD> {
    PIXEL_SHADER_ALPHA_DISCARD a;
    void log_item(Logger* l) const {
        l->log_enum(PIXEL_SHADER_ALPHA_DISCARD_ENUM_MAP, a);
    }
};

// Template specialization for logging LinearFilterConditions struct
template<>
struct LogItem<MyID3D9Device::Impl::LinearFilterConditions> {
    const MyID3D9Device::Impl::LinearFilterConditions* linear_conditions;

    void log_item(Logger* l) const {
        l->log_struct_begin();
#define STRUCT linear_conditions
        l->log_struct_members_named(
            "alpha_discard", STRUCT->alpha_discard
        );
        auto
            sd_it = STRUCT->samplers_descs.begin(),
            sd_it_end = STRUCT->samplers_descs.end();
        auto
            td_it = STRUCT->texs_descs.begin(),
            td_it_end = STRUCT->texs_descs.end();
        for (
            size_t i = 0;
            td_it != td_it_end && sd_it != sd_it_end;
            ++td_it, ++sd_it, ++i
            ) {
            auto sd = *sd_it;
            auto td = *td_it;
            if (td) {
#define LOG_DESC_MEMBER(m) do { \
    l->log_struct_sep(); \
    l->log_item(i); \
    l->log_struct_member_access(); \
    l->log_item(#m); \
    l->log_assign(); \
    l->log_item(td->m); \
} while (0)
                // LOG_DESC_MEMBER(Width);
               //  LOG_DESC_MEMBER(Height);
#undef LOG_DESC_MEMBER
            }
            if (sd) {
#define LOG_DESC_MEMBER(m) do { \
    l->log_struct_sep(); \
    l->log_item(i); \
    l->log_struct_member_access(); \
    l->log_item(#m); \
    l->log_assign(); \
    l->log_item(sd->m); \
} while (0)
                //  LOG_DESC_MEMBER(Filter);
#undef LOG_DESC_MEMBER
            }
        }
#undef STRUCT
        l->log_struct_end();
    }
};



// Section 5: Draw Functions
HRESULT STDMETHODCALLTYPE MyID3D9Device::DrawIndexedPrimitive(
    D3DPRIMITIVETYPE PrimitiveType,
    INT BaseVertexIndex,
    UINT MinVertexIndex,
    UINT NumVertices,
    UINT startIndex,
    UINT primCount
) {
    if (!impl || !impl->inner)
        return D3DERR_INVALIDCALL;

    impl->set_render_vp();
    impl->linear_conditions_begin();

    HRESULT hr = impl->inner->DrawIndexedPrimitive(
        PrimitiveType,
        BaseVertexIndex,
        MinVertexIndex,
        NumVertices,
        startIndex,
        primCount
    );

    impl->linear_conditions_end();
    return hr;
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetVertexShaderConstantF(
    UINT StartRegister,
    const float* pConstantData,
    UINT Vector4fCount
) {
    // Begin linear filter conditions
    impl->linear_conditions_begin();

    // Ensure proper setup of constant buffers
    const UINT numFloats = Vector4fCount * 4;  // Each Vector4f contains 4 floats
    if (numFloats) {
        // Cache the constant data
        memcpy(
            impl->cached_vs_constants + StartRegister * 4,
            pConstantData,
            numFloats * sizeof(float)
        );
    }

    // Call inner device's SetVertexShaderConstantF
    HRESULT result = impl->inner->SetVertexShaderConstantF(
        StartRegister,
        pConstantData,
        Vector4fCount
    );

    // End linear filter conditions
    impl->linear_conditions_end();

    return result;
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetFVF(
    DWORD FVF
) {
    // Cache the FVF
    impl->cached_fvf = FVF;

    // Call inner device's SetFVF
    return impl->inner->SetFVF(FVF);
}

// Section 6: Stream and Index Buffer Management

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetIndices(IDirect3DIndexBuffer9* pIndexData) {
    if (!impl || !impl->inner) return D3DERR_INVALIDCALL;

    HRESULT hr = impl->inner->SetIndices(pIndexData);

    if (SUCCEEDED(hr)) {
        if (pIndexData) pIndexData->AddRef();
        if (impl->cached_ib) impl->cached_ib->Release();
        impl->cached_ib = pIndexData;

        impl->cached_ib_offset = 0;
        impl->cached_ib_format = D3DFMT_INDEX16;
        if (pIndexData) {
            D3DINDEXBUFFER_DESC d{};
            if (SUCCEEDED(pIndexData->GetDesc(&d)))
                impl->cached_ib_format = d.Format;
        }
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::DrawPrimitive(
    D3DPRIMITIVETYPE PrimitiveType,
    UINT StartVertex,
    UINT PrimitiveCount
) {

    static thread_local bool in_our_draw = false;

    if (!impl || !impl->inner) {
        return D3DERR_INVALIDCALL;
    }

    {
        IDirect3DBaseTexture9* t0 = nullptr;
        IDirect3DSurface9* rt0 = nullptr;
        D3DVIEWPORT9 vp{};
        impl->inner->GetTexture(0, &t0);
        impl->inner->GetRenderTarget(0, &rt0);
        impl->inner->GetViewport(&vp);

        if (t0) t0->Release();
        if (rt0) rt0->Release();
    }

    impl->set_render_vp();

    bool handled = false;

    if (!in_our_draw &&
        PrimitiveType == D3DPT_TRIANGLESTRIP &&
        PrimitiveCount == 2)
    {
        in_our_draw = true;

        const UINT vertexCount = PrimitiveCount + 2;

        handled = impl->Draw(vertexCount, StartVertex);

        if (handled) {
            in_our_draw = false;
            return D3D_OK;
        }
    }

    impl->linear_conditions_begin();

    HRESULT hr = impl->inner->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);

    impl->linear_conditions_end();

    // Drop the guard after the pass-through returns.
    in_our_draw = false;

    return hr;
}

// Section 7: Render State Management

  //just DX9 thingsssssssss LOL
 
// Section 8: Texture and Sampler Management

HRESULT STDMETHODCALLTYPE  MyID3D9Device::SetPixelShaderConstantF(
    UINT StartRegister,
    const float* pConstantData,
    UINT Vector4fCount
) {
    // Set the pixel shader constants directly on the inner device
    impl->inner->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
    return S_OK;
}


// Section 9: Render Target Management

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetRenderTarget(
    DWORD RenderTargetIndex,
    IDirect3DSurface9* pRenderTarget
) {
    if (!impl || !impl->inner) return D3DERR_INVALIDCALL;

    impl->reset_render_vp();
    impl->render_width = 0;
    impl->render_height = 0;
    impl->render_orig_width = 0;
    impl->render_orig_height = 0;
    impl->need_render_vp = false;

    HRESULT hr = impl->inner->SetRenderTarget(RenderTargetIndex, pRenderTarget);
    zm_trace_hr("MyID3D9Device::SetRenderTarget", hr);
    ZM_LOG_UNSUP("MyID3D9Device::SetRenderTarget", hr);

    if (FAILED(hr))
        return hr;

    if (RenderTargetIndex == 0) {
        if (impl->cached_rtv) {
            impl->cached_rtv->Release();
            impl->cached_rtv = nullptr;
        }
        impl->cached_rtv = pRenderTarget;
        if (impl->cached_rtv)
            impl->cached_rtv->AddRef();
    }

    return hr;
}



HRESULT STDMETHODCALLTYPE  MyID3D9Device::SetRenderState(
    D3DRENDERSTATETYPE State,
    DWORD Value
) {
    // Set the render state directly on the inner device
    impl->inner->SetRenderState(State, Value);

    // Cache the render state if enabled for slang shader
    if constexpr (ENABLE_SLANG_SHADER) {
        impl->cached_rs[State] = Value;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetDepthStencilState(
    IDirect3DStateBlock9* pDepthStencilState
) {
    // Set the cached depth stencil state
    impl->cached_dss = pDepthStencilState;

    if (pDepthStencilState) {
        // Enable stencil
        impl->inner->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        // Additional D3DRS_* state settings here if needed
    }
    else {
        // Disable stencil
        impl->inner->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    }

    return S_OK; // Assuming HRESULT should be returned
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetStreamSource(
    UINT StreamNumber,
    IDirect3DVertexBuffer9* pStreamData,
    UINT OffsetInBytes,
    UINT Stride
) {
    if (!impl || !impl->inner) return D3DERR_INVALIDCALL;

    HRESULT hr = impl->inner->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);

    if (SUCCEEDED(hr) && StreamNumber < D3D9_MAX_VERTEX_BUFFERS) {
        if (pStreamData) pStreamData->AddRef();
        if (impl->cached_vbs.ppVertexBuffers[StreamNumber])
            impl->cached_vbs.ppVertexBuffers[StreamNumber]->Release();

        impl->cached_vbs.ppVertexBuffers[StreamNumber] = pStreamData;
        impl->cached_vbs.pOffsets[StreamNumber] = OffsetInBytes;
        impl->cached_vbs.pStrides[StreamNumber] = Stride;
    }

    return hr;
}

// Section 10: Remaining Methods


HRESULT STDMETHODCALLTYPE MyID3D9Device::DrawPrimitiveAuto() {

    // Placeholder implementation
    // Assuming we need to draw all vertices currently in the stream output buffer
    // We would need to determine the vertex count, which isn't directly possible without additional context
    // For illustration purposes, we use a fixed primitive type and vertex count
    D3DPRIMITIVETYPE PrimitiveType = D3DPT_TRIANGLELIST; // Example primitive type
    UINT VertexCount = 100; // Example vertex count, should be replaced with actual count

    // Call DrawPrimitive with the determined primitive type and vertex count
    return impl->inner->DrawPrimitive(PrimitiveType, 0, VertexCount / 3); // Assuming triangles
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetRasterizerState(
    IDirect3DStateBlock9* pRasterizerState
) {
    if (pRasterizerState) {
        // Apply the state block to set the rasterizer state
        return pRasterizerState->Apply();
    }
    return D3D_OK;
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetViewports(
    UINT NumViewports,
    const D3DVIEWPORT9* pViewports
) {
    if (NumViewports) {
        impl->cached_vp = *pViewports;
        return impl->inner->SetViewport(pViewports);
    }
    else {
        memset(&impl->cached_vp, 0, sizeof(D3DVIEWPORT9));
        return D3D_OK;
    }
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetScissorRects(
    UINT NumRects,
    const RECT* pRects
) {
    if (NumRects > 0 && pRects) {
        impl->inner->SetScissorRect(pRects);
    }
    else {
        RECT emptyRect = { 0, 0, 0, 0 };
        impl->inner->SetScissorRect(&emptyRect);
    }
    return D3D_OK;
}


 HRESULT STDMETHODCALLTYPE MyID3D9Device::UpdateSubresource(
        IDirect3DResource9 * pDstResource,
        UINT DstSubresource,
        const D3DLOCKED_RECT * pDstBox,
        const VOID * pSrcData,
        UINT SrcRowPitch,
        UINT SrcDepthPitch
    ) {
        if (!pDstResource || !pSrcData) {
            return E_INVALIDARG;
        }

        D3DRESOURCETYPE dstType = pDstResource->GetType();
        if (dstType == D3DRTYPE_TEXTURE_ALIAS) {
            IDirect3DTexture9* dstTexture = static_cast<IDirect3DTexture9*>(pDstResource);
            D3DSURFACE_DESC desc;
            dstTexture->GetLevelDesc(DstSubresource, &desc);

            if (pDstBox) {
                RECT rect = { 0 }; // Initialize all members to zero
                rect.left = 0; // Assuming the left offset is 0
                rect.top = 0; // Assuming the top offset is 0
                rect.right = rect.left + pDstBox->Pitch;
                rect.bottom = rect.top + pDstBox->Pitch / SrcRowPitch;

                impl->inner->UpdateSurface(
                    static_cast<IDirect3DSurface9*>(pDstResource),
                    &rect,
                    const_cast<IDirect3DSurface9*>(static_cast<const IDirect3DSurface9*>(pSrcData)),
                    nullptr
                );
            }
            else {
                impl->inner->UpdateSurface(
                    static_cast<IDirect3DSurface9*>(pDstResource),
                    nullptr,
                    const_cast<IDirect3DSurface9*>(static_cast<const IDirect3DSurface9*>(pSrcData)),
                    nullptr
                );
            }
        }
        else {
            // Handle other resource types if needed
        }

        return D3D_OK;
    }

  
// Section 12: Resource Copy and Update

 void STDMETHODCALLTYPE MyIDirect3DDevice9::CopyResource(
     IDirect3DResource9* pDstResource,
     IDirect3DResource9* pSrcResource
 ) {
     if (!pDstResource || !pSrcResource)
         return;

     D3DRESOURCETYPE dstType = pDstResource->GetType();
     D3DRESOURCETYPE srcType = pSrcResource->GetType();

     if (dstType != srcType)
         return;

     switch (dstType) {

     case D3DRTYPE_SURFACE: {
         IDirect3DSurface9* dstSurface =
             static_cast<IDirect3DSurface9*>(pDstResource);
         IDirect3DSurface9* srcSurface =
             static_cast<IDirect3DSurface9*>(pSrcResource);

         D3DSURFACE_DESC srcDesc, dstDesc;
         srcSurface->GetDesc(&srcDesc);
         dstSurface->GetDesc(&dstDesc);

         RECT srcRect = {
             0,
             0,
             (LONG)srcDesc.Width,
             (LONG)srcDesc.Height
         };

         RECT dstRect = {
             0,
             0,
             (LONG)dstDesc.Width,
             (LONG)dstDesc.Height
         };

         impl->StretchRect(
             srcSurface,
             &srcRect,
             dstSurface,
             &dstRect,
             D3DTEXF_NONE
         );
         break;
     }

     case D3DRTYPE_TEXTURE_ALIAS: {
         IDirect3DTexture9* dstTexture =
             static_cast<IDirect3DTexture9*>(pDstResource);
         IDirect3DTexture9* srcTexture =
             static_cast<IDirect3DTexture9*>(pSrcResource);

         D3DLOCKED_RECT srcLocked, dstLocked;
         if (FAILED(srcTexture->LockRect(0, &srcLocked, nullptr, D3DLOCK_READONLY)))
             return;

         if (FAILED(dstTexture->LockRect(0, &dstLocked, nullptr, 0))) {
             srcTexture->UnlockRect(0);
             return;
         }

         D3DSURFACE_DESC desc;
         dstTexture->GetLevelDesc(0, &desc);

         BYTE* src = static_cast<BYTE*>(srcLocked.pBits);
         BYTE* dst = static_cast<BYTE*>(dstLocked.pBits);

         UINT rowBytes =(std::min)(
             static_cast<UINT>(srcLocked.Pitch),
             static_cast<UINT>(dstLocked.Pitch)
         );

         for (UINT y = 0; y < desc.Height; ++y) {
             memcpy(dst, src, rowBytes);
             src += srcLocked.Pitch;
             dst += dstLocked.Pitch;
         }

         srcTexture->UnlockRect(0);
         dstTexture->UnlockRect(0);
         break;
     }

     default:
         break;
     }
 }


void STDMETHODCALLTYPE MyIDirect3DDevice9::UpdateSubresource(
    IDirect3DResource9* pDstResource,
    const void* pSrcData,
    UINT SrcRowPitch,
    UINT SrcDepthPitch
) {
    D3DRESOURCETYPE dstType = pDstResource->GetType();

    switch (dstType) {
    case D3DRTYPE_TEXTURE_ALIAS: {
        IDirect3DTexture9* dstTexture = static_cast<IDirect3DTexture9*>(pDstResource);
        D3DLOCKED_RECT lockedRect;
        dstTexture->LockRect(0, &lockedRect, nullptr, 0);
        memcpy(lockedRect.pBits, pSrcData, SrcRowPitch); // Simplified copy
        dstTexture->UnlockRect(0);
        break;
    }
    default:
        // Handle other resource types if needed
        break;
    }
}


// Section 13: Clear Functions

void STDMETHODCALLTYPE MyIDirect3DDevice9::ClearRenderTargetView(
    IDirect3DSurface9* pRenderTarget,
    const D3DCOLOR Color
) {
    impl->Clear(0, nullptr, D3DCLEAR_TARGET, Color, 1.0f, 0);
}

void STDMETHODCALLTYPE MyIDirect3DDevice9::ClearDepthStencilView(
    IDirect3DSurface9* pDepthStencil,
    DWORD ClearFlags,
    float Depth,
    DWORD Stencil
) {
    impl->Clear(0, nullptr, ClearFlags, 0, Depth, Stencil);
}


// Section 14: Reference Counting and Query Interface

HRESULT MyID3D9Device::QueryInterface(REFIID riid, void** ppvObj) {
    return impl->QueryInterface(riid, ppvObj);
}

ULONG MyID3D9Device::AddRef() {
    return impl->AddRef();
}

ULONG MyID3D9Device::Release() {
    ULONG refCount = impl->Release();
    if (refCount == 0) {
        delete this;
    }
    return refCount;
}


// Section 15: Generate Mips and Get Functions

void STDMETHODCALLTYPE MyID3D9Device::GenerateMips(IDirect3DBaseTexture9* pTexture) {
    if (pTexture) {
        pTexture->GenerateMipSubLevels();
    }
}

void STDMETHODCALLTYPE MyID3D9Device::PSGetShaderResources(
    UINT StartSlot,
    UINT NumViews,
    IDirect3DBaseTexture9** ppShaderResourceViews
) {
    for (UINT i = 0; i < NumViews; ++i) {
        impl->inner->GetTexture(StartSlot + i, &ppShaderResourceViews[i]);
    }
}

void STDMETHODCALLTYPE MyID3D9Device::PSGetShader(
    IDirect3DPixelShader9** ppPixelShader
) {
    *ppPixelShader = impl->cached_ps;
    if (*ppPixelShader) {
        (*ppPixelShader)->AddRef();
    }
}


void STDMETHODCALLTYPE MyID3D9Device::VSGetShader(
    IDirect3DVertexShader9** ppVertexShader
) {
    *ppVertexShader = impl->cached_vs;
    if (*ppVertexShader) {
        (*ppVertexShader)->AddRef();
    }
}


void STDMETHODCALLTYPE MyID3D9Device::PSGetSamplers(
    UINT StartSlot,
    UINT NumSamplers,
    DWORD* ppSamplers
) {
    for (UINT i = 0; i < NumSamplers; ++i) {
        impl->inner->GetSamplerState(StartSlot + i, D3DSAMP_MIPFILTER, &ppSamplers[i]);
    }
}


void STDMETHODCALLTYPE MyID3D9Device::IAGetInputLayout(
    IDirect3DVertexDeclaration9** ppInputLayout
) {
    impl->inner->GetVertexDeclaration(ppInputLayout);
}


void STDMETHODCALLTYPE MyID3D9Device::IAGetVertexBuffers(
    UINT StartSlot,
    UINT NumBuffers,
    IDirect3DVertexBuffer9** ppVertexBuffers,
    UINT* pStrides,
    UINT* pOffsets
) {
    for (UINT i = 0; i < NumBuffers; ++i) {
        impl->inner->GetStreamSource(StartSlot + i, &ppVertexBuffers[i], &pOffsets[i], &pStrides[i]);
    }
}


void STDMETHODCALLTYPE MyID3D9Device::IAGetIndexBuffer(
    IDirect3DIndexBuffer9** pIndexBuffer,
    D3DFORMAT* Format,
    UINT* Offset
) {
    // Retrieve the index buffer
    impl->inner->GetIndices(pIndexBuffer);

    // Handle Format and Offset if necessary
    if (pIndexBuffer && *pIndexBuffer) {
        D3DINDEXBUFFER_DESC desc;
        (*pIndexBuffer)->GetDesc(&desc);
        *Format = desc.Format;
        *Offset = 0; // Assuming offset is 0, adjust if necessary
    }
}


void STDMETHODCALLTYPE MyID3D9Device::OMGetRenderTargets(
    UINT NumViews,
    IDirect3DSurface9** ppRenderTargetViews,
    IDirect3DSurface9** ppDepthStencilView
) {
    for (UINT i = 0; i < NumViews; ++i) {
        impl->inner->GetRenderTarget(i, &ppRenderTargetViews[i]);
    }
    if (ppDepthStencilView) {
        impl->inner->GetDepthStencilSurface(ppDepthStencilView);
    }
}


void STDMETHODCALLTYPE MyID3D9Device::RSGetViewports(
    UINT* NumViewports,
    D3DVIEWPORT9* pViewports
) {
    D3DVIEWPORT9 vp;
    impl->inner->GetViewport(&vp);
    if (NumViewports) {
        *NumViewports = 1;
    }
    if (pViewports) {
        *pViewports = vp;
    }
}


void STDMETHODCALLTYPE MyID3D9Device::RSGetScissorRects(
    UINT* NumRects,
    RECT* pRects
) {
    if (NumRects && *NumRects > 0) {
        impl->inner->GetScissorRect(pRects);
        *NumRects = 1;
    }
}



// Section 16: Flush


void STDMETHODCALLTYPE MyID3D9Device::Flush() {
    impl->inner->EndScene();
}


// Section 17: Device Creation Functions(DirectX 10 Wrappers)

HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateVertexBuffer(
    UINT Length,
    DWORD Usage,
    DWORD FVF,
    D3DPOOL Pool,
    IDirect3DVertexBuffer9** ppVertexBuffer,
    HANDLE* pSharedHandle
) {
    HRESULT ret = impl->inner->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
    if (ret == S_OK) {
    }
    else {
    }
    return ret;
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateTexture1D(
    UINT Width,
    UINT Levels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DTexture9** ppTexture,
    HANDLE* pSharedHandle
) {
    HRESULT ret = impl->inner->CreateTexture(Width, 1, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
    if (ret == S_OK) {
    }
    else {
    }
    return ret;
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateTexture2D(
    UINT Width,
    UINT Height,
    UINT Levels,
    DWORD Usage,
    D3DFORMAT Format,
    D3DPOOL Pool,
    IDirect3DTexture9** ppTexture,
    HANDLE* pSharedHandle
) {
    HRESULT ret = impl->inner->CreateTexture(
        Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle
    );
    if (ret == S_OK) {
    }
    else {
    }
    return ret;
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateShaderResourceView(
    IDirect3DTexture9* pTexture,
    IDirect3DTexture9** ppSRView
) {
    // DirectX 9 does not have a direct equivalent of CreateShaderResourceView.
    // Instead, we can use the texture directly as the shader resource.

    HRESULT ret = S_OK;

    if (pTexture) {
        *ppSRView = pTexture;
        pTexture->AddRef();  // Increase reference count since we're assigning it to ppSRView
    }
    else {
        ret = E_INVALIDARG;
    }

    return ret;
}



HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateRenderTargetView(
    IDirect3DSurface9* pSurface,
    const D3DSURFACE_DESC* pDesc,
    IDirect3DSurface9** ppRTView
) {
    HRESULT ret = S_OK;
    IDirect3DSurface9* pRTView = nullptr;

    // Create render target view for the surface
    ret = impl->inner->CreateRenderTarget(
        pDesc->Width, pDesc->Height,
        pDesc->Format,
        D3DMULTISAMPLE_NONE, 0, FALSE,
        &pRTView, nullptr
    );

    if (SUCCEEDED(ret) && pRTView != nullptr) {
        *ppRTView = pRTView;
    }
    else {
    }

    return ret;
}



HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateDepthStencilView(
    IDirect3DSurface9* pSurface,
    const D3DSURFACE_DESC* pDesc,
    IDirect3DSurface9** ppDepthStencilView
) {
    HRESULT ret = S_OK;
    IDirect3DSurface9* pDepthStencilView = nullptr;

    // Create depth stencil view for the surface
    ret = impl->inner->CreateDepthStencilSurface(
        pDesc->Width, pDesc->Height,
        pDesc->Format,
        D3DMULTISAMPLE_NONE, 0, FALSE,
        &pDepthStencilView, nullptr
    );

    if (SUCCEEDED(ret) && pDepthStencilView != nullptr) {
        *ppDepthStencilView = pDepthStencilView;
    }
    else {
    }

    return ret;
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateInputLayout(
    const D3DVERTEXELEMENT9* pInputElementDescs,
    UINT NumElements,
    const void* pShaderBytecodeWithInputSignature,
    SIZE_T BytecodeLength,
    IDirect3DVertexDeclaration9** ppVertexDeclaration
) {
    HRESULT ret = S_OK;

    // Define a fixed-size array for vertex elements
    D3DVERTEXELEMENT9 vertexElements[MAX_FVF_DECL_SIZE];
    memset(vertexElements, 0, sizeof(vertexElements)); // Initialize the array to avoid uninitialized warnings

    for (UINT i = 0; i < NumElements; ++i) {
        vertexElements[i] = pInputElementDescs[i];
    }

    // Add the D3DDECL_END terminator
    vertexElements[NumElements] = D3DVERTEXELEMENT9{ 0xFF, 0xFF, D3DDECLTYPE_UNUSED, 0, 0, 0 };

    ret = impl->inner->CreateVertexDeclaration(vertexElements, ppVertexDeclaration);



    if (SUCCEEDED(ret)) {
    }
    else {
    }

    return ret;
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateVertexShader(
    const void* pShaderBytecode,
    SIZE_T BytecodeLength,
    IDirect3DVertexShader9** ppVertexShader
) {
    HRESULT ret = impl->inner->CreateVertexShader((const DWORD*)pShaderBytecode, ppVertexShader);
    if (ret == S_OK) {
        // Assume ShaderLogger and MyID3D10VertexShader are defined appropriately
        ShaderLogger shader_source{ pShaderBytecode };
        DWORD hash;
        MurmurHash3_x86_32(pShaderBytecode, BytecodeLength, 0, &hash);
        // MyID3D10VertexShader equivalent creation
        // Adjust the class to fit Direct3D 9 context if necessary
        // new MyID3D9VertexShader(ppVertexShader, hash, BytecodeLength, shader_source.source);
    }
    else {
    }
    return ret;
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateGeometryShader(
    const void* pShaderBytecode,
    SIZE_T BytecodeLength,
    void** ppGeometryShader
) {
    OutputDebugStringA("[ZeroMod] FATAL: CreateGeometryShader called on D3D9 device (should NEVER happen)\n");
    if (ppGeometryShader) *ppGeometryShader = nullptr;
    ZM_NOTIMPL_RET();
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateGeometryShaderWithStreamOutput(
    const void* pShaderBytecode,
    SIZE_T BytecodeLength,
    const void* pSODeclaration,
    UINT NumEntries,
    UINT OutputStreamStride,
    void** ppGeometryShader
) {
    OutputDebugStringA("[ZeroMod] FATAL: CreateGeometryShaderWithStreamOutput called on D3D9 device (should NEVER happen)\n");
    if (ppGeometryShader) *ppGeometryShader = nullptr;
    ZM_NOTIMPL_RET();
}

//Section 18: State Creation Functions (Blend, DepthStencil, Rasterizer, Sampler)

HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateBlendState(
    const D3D9_BLEND_DESC* pBlendStateDesc,
    IDirect3DStateBlock9** ppBlendState
) {
    HRESULT ret = S_OK;

    // Create a state block to hold the blend state
    impl->inner->BeginStateBlock();

    for (UINT i = 0; i < D3D9_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
        if (pBlendStateDesc->RenderTarget[i].BlendEnable) {
            impl->inner->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            impl->inner->SetRenderState(D3DRS_SRCBLEND, pBlendStateDesc->RenderTarget[i].SrcBlend);
            impl->inner->SetRenderState(D3DRS_DESTBLEND, pBlendStateDesc->RenderTarget[i].DestBlend);
        }
        else {
            impl->inner->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        }
    }

    ret = impl->inner->EndStateBlock(ppBlendState);

    if (SUCCEEDED(ret)) {
    }
    else {
    }

    return ret;
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateDepthStencilState(
    const D3D9_DEPTH_STENCIL_DESC* pDepthStencilDesc,
    IDirect3DStateBlock9** ppDepthStencilState
) {
    HRESULT ret = S_OK;

    // Create a state block to hold the depth-stencil state
    impl->inner->BeginStateBlock();

    impl->inner->SetRenderState(D3DRS_ZENABLE, pDepthStencilDesc->DepthEnable);
    impl->inner->SetRenderState(D3DRS_ZFUNC, pDepthStencilDesc->DepthFunc);
    impl->inner->SetRenderState(D3DRS_STENCILENABLE, pDepthStencilDesc->StencilEnable);

    ret = impl->inner->EndStateBlock(ppDepthStencilState);

    if (SUCCEEDED(ret)) {
    }
    else {
    }

    return ret;
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateRasterizerState(
    const D3D9_RASTERIZER_DESC* pRasterizerDesc,
    IDirect3DStateBlock9** ppRasterizerState
) {
    HRESULT ret = S_OK;

    // Ensure impl is defined and inner device is used
    if (!impl || !impl->inner) {
        return E_FAIL;
    }

    // Create a state block to hold the rasterizer state
    impl->inner->BeginStateBlock();

    impl->inner->SetRenderState(D3DRS_FILLMODE, pRasterizerDesc->FillMode);
    impl->inner->SetRenderState(D3DRS_CULLMODE, pRasterizerDesc->CullMode);

    // Use union to avoid strict aliasing issues
    union {
        float floatValue;
        DWORD dwordValue;
    } alias = {};  // Initialize the union

    alias.floatValue = pRasterizerDesc->DepthBias;
    impl->inner->SetRenderState(D3DRS_DEPTHBIAS, alias.dwordValue);

    alias.floatValue = pRasterizerDesc->SlopeScaledDepthBias;
    impl->inner->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, alias.dwordValue);

    impl->inner->SetRenderState(D3DRS_SCISSORTESTENABLE, pRasterizerDesc->ScissorEnable);
    impl->inner->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, pRasterizerDesc->MultisampleEnable);
    impl->inner->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, pRasterizerDesc->AntialiasedLineEnable);

    ret = impl->inner->EndStateBlock(ppRasterizerState);

    if (SUCCEEDED(ret)) {
    }
    else {
    }

    return ret;
}

enum class CustomFilter {
    MIN_MAG_MIP_LINEAR,
    MIN_MAG_MIP_POINT
    // Add other filters as necessary
};

D3DTEXTUREFILTERTYPE ConvertFilter(CustomFilter filter) {
    switch (filter) {
    case CustomFilter::MIN_MAG_MIP_LINEAR:
        return D3DTEXF_LINEAR;
    case CustomFilter::MIN_MAG_MIP_POINT:
        return D3DTEXF_POINT;
    default:
        return D3DTEXF_NONE;
    }
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateSamplerState(
    const D3DSAMPLER_DESC* pSamplerDesc,
    IDirect3DStateBlock9** ppSamplerState
) {
    HRESULT ret = S_OK;

    if (!impl || !impl->inner) {
        return E_FAIL;
    }

    // Create a state block to hold the sampler state
    impl->inner->BeginStateBlock();

    impl->inner->SetSamplerState(0, D3DSAMP_MINFILTER, ConvertFilter(static_cast<CustomFilter>(pSamplerDesc->Filter)));
    impl->inner->SetSamplerState(0, D3DSAMP_MAGFILTER, ConvertFilter(static_cast<CustomFilter>(pSamplerDesc->Filter)));
    impl->inner->SetSamplerState(0, D3DSAMP_MIPFILTER, ConvertFilter(static_cast<CustomFilter>(pSamplerDesc->Filter)));
    impl->inner->SetSamplerState(0, D3DSAMP_ADDRESSU, pSamplerDesc->AddressU);
    impl->inner->SetSamplerState(0, D3DSAMP_ADDRESSV, pSamplerDesc->AddressV);
    impl->inner->SetSamplerState(0, D3DSAMP_ADDRESSW, pSamplerDesc->AddressW);

    // Use union to safely cast float to DWORD
    union FloatToDWORD {
        float f;
        DWORD d;
    };

    FloatToDWORD mipLODBias = { 0.0f }; // Ensure initialization
    mipLODBias.f = pSamplerDesc->MipLODBias;
    impl->inner->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, mipLODBias.d);

    impl->inner->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, pSamplerDesc->MaxAnisotropy);
    // impl->inner->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, pSamplerDesc->ComparisonFunc); // Uncomment if needed

    ret = impl->inner->EndStateBlock(ppSamplerState);

    if (SUCCEEDED(ret)) {
    }
    else {
    }

    return ret;
}



// Section 19: Query and Multisample Quality Levels



D3DQUERYTYPE ConvertQueryType(CustomQueryType queryType) {
    switch (queryType) {
    case CustomQueryType::EVENT:
        return D3DQUERYTYPE_EVENT;
    case CustomQueryType::OCCLUSION:
        return D3DQUERYTYPE_OCCLUSION;
        // Add other query conversions as necessary
    default:
        return D3DQUERYTYPE_EVENT; // Default to event type
    }
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery)
{
    if (!ppQuery) return D3DERR_INVALIDCALL;
    *ppQuery = nullptr;
    if (!impl || !impl->inner) return D3DERR_INVALIDCALL;

    HRESULT hr = impl->inner->CreateQuery(Type, ppQuery);

    if (FAILED(hr)) {
        char b[256];
        _snprintf(b, sizeof(b),
            "[ZeroMod] CreateQuery FAILED type=%d hr=0x%08X\n",
            Type, (unsigned)hr
        );
        OutputDebugStringA(b);
    }

    return hr;


    char b[256];
    sprintf(b, "[ZeroMod] CreateQuery(Type=%d) hr=0x%08lX out=%p\n",
        (int)Type, (unsigned long)hr, (void*)*ppQuery);
    OutputDebugStringA(b);

    return hr;
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateQuery_Custom(
    const CustomQueryDesc* pQueryDesc,
    IDirect3DQuery9** ppQuery
) {
    if (!ppQuery) return D3DERR_INVALIDCALL;
    *ppQuery = nullptr;
    if (!pQueryDesc) return D3DERR_INVALIDCALL;

    // temporary stub:
    OutputDebugStringA("[ZeroMod] CreateQuery_Custom(CustomQueryDesc*) HIT (stub)\n");
    return E_NOTIMPL;

    // D3DQUERYTYPE t = ConvertQueryType(pQueryDesc->Query);
    // return CreateQuery(t, ppQuery);
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::CreatePredicate(
    const CustomQueryDesc* pPredicateDesc,
    IDirect3DQuery9** ppPredicate
) {
    return CreateQuery_Custom(pPredicateDesc, ppPredicate);
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::CheckMultisampleQualityLevels(
    D3DFORMAT Format,
    DWORD SampleCount,
    DWORD* pNumQualityLevels
) {
    HRESULT hr = m_pD3D->CheckDeviceMultiSampleType(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        Format,
        TRUE, // Windowed
        (D3DMULTISAMPLE_TYPE)SampleCount,
        pNumQualityLevels
    );

    ZM_LOG_UNSUP("MyID3D9Device::CheckMultisampleQualityLevels", hr);
    return hr;
}


// Section 20: Missing Virtual Functions

HRESULT MyID3D9Device::TestCooperativeLevel() {
    HRESULT hr = impl->inner->TestCooperativeLevel();
    ZM_LOG_UNSUP("MyID3D9Device::TestCooperativeLevel", hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    if (!impl || !impl->inner) return D3DERR_INVALIDCALL;

    impl->on_pre_reset();

    HRESULT hr = impl->inner->Reset(pPresentationParameters);
    ZM_LOG_UNSUP("MyID3D9Device::Reset", hr);

    if (FAILED(hr))
        return hr;

    // Attach defaults BEFORE on_post_reset can reach resize_buffers/update_config
    extern Overlay* default_overlay;
    extern Config* default_config;

    if (!impl->config && default_config) {
        impl->config = default_config;
        OutputDebugStringA("[ZeroMod] Reset: attached default_config\n");
    }
    if (!impl->overlay && default_overlay) {
        impl->overlay = default_overlay;
        OutputDebugStringA("[ZeroMod] Reset: attached default_overlay\n");
    }

    impl->on_post_reset(pPresentationParameters);

    if (impl->cached_rtv) { impl->cached_rtv->Release(); impl->cached_rtv = nullptr; }
    if (impl->cached_dsv) { impl->cached_dsv->Release(); impl->cached_dsv = nullptr; }
    impl->need_render_vp = true;

    // ---- Overlay init / resize (ONLY HERE) ----

    // Pick a usable HWND: pp->hDeviceWindow first, else cached_hwnd
    HWND use_hwnd = nullptr;
    if (pPresentationParameters && pPresentationParameters->hDeviceWindow)
        use_hwnd = pPresentationParameters->hDeviceWindow;
    else if (impl && impl->cached_hwnd)
        use_hwnd = impl->cached_hwnd;

    char b[320];
    _snprintf(b, sizeof(b),
        "[ZeroMod] Reset overlay check: default_overlay=%p pp=%p pp.hwnd=%p cached_hwnd=%p use_hwnd=%p\n",
        (void*)default_overlay,
        (void*)pPresentationParameters,
        pPresentationParameters ? (void*)pPresentationParameters->hDeviceWindow : nullptr,
        impl ? (void*)impl->cached_hwnd : nullptr,
        (void*)use_hwnd
    );
    OutputDebugStringA(b);

    if (default_overlay && pPresentationParameters && use_hwnd) {

        // store non-owning pointer once (prevents “global” usage inside Present)
        char b[256];
        _snprintf(b, sizeof(b),
            "[ZeroMod] Reset: default_config=%p impl=%p impl->config(before)=%p\n",
            (void*)default_config, (void*)impl, impl ? (void*)impl->config : nullptr);
        OutputDebugStringA(b);

        // attach
        impl->config = default_config;     // or set_config(default_config)

        _snprintf(b, sizeof(b),
            "[ZeroMod] Reset: impl->config(after)=%p\n",
            impl ? (void*)impl->config : nullptr);
        OutputDebugStringA(b);

        impl->overlay = default_overlay;
        

        // IMPORTANT: Overlay::set_display() hard-requires pp->hDeviceWindow != NULL,
        // so pass a local copy with a forced hwnd.
        D3DPRESENT_PARAMETERS pp_fixed = *pPresentationParameters;
        pp_fixed.hDeviceWindow = use_hwnd;

        if (!use_hwnd || !IsWindow(use_hwnd)) {
            OutputDebugStringA("[ZeroMod] Overlay: use_hwnd invalid, skip init\n");
            return hr;
        }

        if (!impl->overlay_inited) {
            OutputDebugStringA("[ZeroMod] Overlay: set_display from Reset\n");
            impl->overlay->set_display(&pp_fixed, this);
            impl->overlay_inited = true;
        }
        else {
            OutputDebugStringA("[ZeroMod] Overlay: resize_buffers from Reset\n");
            impl->overlay->resize_buffers(
                0,
                pp_fixed.BackBufferWidth,
                pp_fixed.BackBufferHeight,
                pp_fixed.BackBufferFormat,
                0
            );
        }
    }
    else {
        OutputDebugStringA("[ZeroMod] Overlay: NOT init (overlay null / pp null / no hwnd)\n");
    }


    return hr;
}

void MyID3D9Device::on_pre_reset() {
    if (impl) impl->on_pre_reset();
}

void MyID3D9Device::on_post_reset(D3DPRESENT_PARAMETERS* pp) {
    if (impl) impl->on_post_reset(pp);
}


UINT MyID3D9Device::GetAvailableTextureMem() {
    return impl->inner->GetAvailableTextureMem();
}

HRESULT MyID3D9Device::EvictManagedResources() {
    return impl->inner->EvictManagedResources();
}

HRESULT MyID3D9Device::GetDirect3D(IDirect3D9** ppD3D9) {
    return impl->inner->GetDirect3D(ppD3D9);
}

HRESULT MyID3D9Device::GetDeviceCaps(D3DCAPS9* pCaps) {
    HRESULT hr = impl->inner->GetDeviceCaps(pCaps);
    ZM_LOG_UNSUP("MyID3D9Device::GetDeviceCaps", hr);
    return hr;
}

HRESULT MyID3D9Device::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) {
    HRESULT hr = impl->inner->GetDisplayMode(iSwapChain, pMode);
    ZM_LOG_UNSUP("MyID3D9Device::GetDisplayMode", hr);
    return hr;
}

HRESULT MyID3D9Device::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters) {
    return impl->inner->GetCreationParameters(pParameters);
}

HRESULT MyID3D9Device::SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) {
    return impl->inner->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
}

void MyID3D9Device::SetCursorPosition(int X, int Y, DWORD Flags) {
    impl->inner->SetCursorPosition(X, Y, Flags);
}

BOOL MyID3D9Device::ShowCursor(BOOL bShow) {
    return impl->inner->ShowCursor(bShow);
}

HRESULT MyID3D9Device::CreateAdditionalSwapChain(
    D3DPRESENT_PARAMETERS* pPresentationParameters,
    IDirect3DSwapChain9** pSwapChain
) {
    HRESULT hr = impl->inner->CreateAdditionalSwapChain(pPresentationParameters, pSwapChain);
    ZM_LOG_UNSUP("MyID3D9Device::CreateAdditionalSwapChain", hr);
    return hr;
}

HRESULT MyID3D9Device::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) {
    HRESULT hr = impl->inner->GetSwapChain(iSwapChain, pSwapChain);
    ZM_LOG_UNSUP("MyID3D9Device::GetSwapChain", hr);
    return hr;
}

UINT MyID3D9Device::GetNumberOfSwapChains() {
    return impl->inner->GetNumberOfSwapChains();
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::Present(
    const RECT* src_rect,
    const RECT* dst_rect,
    HWND dst_window_override,
    const RGNDATA* dirty_region
) {
    // ---- Overlay draw (no init here) ----
    if (impl && impl->overlay_inited && impl->overlay) {
        // ONE debug line to prove this is actually executing
        static bool once = false;
        if (!once) { once = true; OutputDebugStringA("[ZeroMod] Overlay: Present path entered\n"); }

        impl->overlay->present(0, 0);
    }
	// ---- Slang Parse ----
    if (impl && (impl->d3d9_2d || impl->d3d9_gba || impl->d3d9_ds))
    {
      static uint64_t s_frame = 0;
      uint64_t f = ++s_frame;

       if (impl->d3d9_2d)  ZeroMod::d3d9_gfx_frame(impl->d3d9_2d, nullptr, f);
       if (impl->d3d9_gba) ZeroMod::d3d9_gfx_frame(impl->d3d9_gba, nullptr, f);
       if (impl->d3d9_ds)  ZeroMod::d3d9_gfx_frame(impl->d3d9_ds, nullptr, f);
    }
    // ---- Real Present ----
        return impl->inner->Present(src_rect, dst_rect, dst_window_override, dirty_region);
}

HRESULT MyID3D9Device::GetBackBuffer(
    UINT iSwapChain,
    UINT iBackBuffer,
    D3DBACKBUFFER_TYPE Type,
    IDirect3DSurface9** ppBackBuffer
) {
    HRESULT hr = impl->inner->GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer);
    ZM_LOG_UNSUP("MyID3D9Device::GetBackBuffer", hr);
    return hr;
}

HRESULT MyID3D9Device::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) {
    return impl->inner->GetRasterStatus(iSwapChain, pRasterStatus);
}

HRESULT MyID3D9Device::SetDialogBoxMode(BOOL bEnableDialogs) {
    return impl->inner->SetDialogBoxMode(bEnableDialogs);
}

void MyID3D9Device::SetGammaRamp(UINT iSwapChain, DWORD flags, const D3DGAMMARAMP* ramp) {
    impl->inner->SetGammaRamp(iSwapChain, flags, ramp);
}

void MyID3D9Device::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) {
    impl->inner->GetGammaRamp(iSwapChain, pRamp);
}

HRESULT MyID3D9Device::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage,
    D3DFORMAT Format, D3DPOOL Pool,
    IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle)
{
    // Only count 512x512 creations while the scan is armed.
    if (g_scan_active && Width == 512 && Height == 512) {
        g_scan_create_hit_512 = true;
        g_scan_create_hit_count++;

        OutputDebugStringA("[ZeroMod][SCAN] CREATE HIT 512x512 (CreateTexture)\n");
    }

    return impl->inner->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
}

HRESULT MyID3D9Device::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) {
    return impl->inner->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
}

HRESULT MyID3D9Device::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) {
    return impl->inner->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
}

HRESULT MyID3D9Device::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) {
    return impl->inner->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
}

HRESULT MyID3D9Device::CreateRenderTarget(
    UINT Width, UINT Height, D3DFORMAT Format,
    D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable,
    IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
{
    HRESULT hr = impl->inner->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);

    return hr;
}

HRESULT MyID3D9Device::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    return impl->inner->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);
}

HRESULT MyID3D9Device::UpdateSurface(IDirect3DSurface9* src_surface, const RECT* src_rect, IDirect3DSurface9* dest_surface, const POINT* dest_point) {
    return impl->inner->UpdateSurface(src_surface, src_rect, dest_surface, dest_point);
}

HRESULT MyID3D9Device::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) {
    return impl->inner->UpdateTexture(pSourceTexture, pDestinationTexture);
}

HRESULT MyID3D9Device::GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) {
    return impl->inner->GetRenderTargetData(pRenderTarget, pDestSurface);
}

HRESULT MyID3D9Device::GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) {
    return impl->inner->GetFrontBufferData(iSwapChain, pDestSurface);
}

HRESULT MyID3D9Device::StretchRect(IDirect3DSurface9* src_surface, const RECT* src_rect, IDirect3DSurface9* dest_surface, const RECT* dest_rect, D3DTEXTUREFILTERTYPE filter) {
    return impl->inner->StretchRect(src_surface, src_rect, dest_surface, dest_rect, filter);
}

HRESULT MyID3D9Device::ColorFill(IDirect3DSurface9* surface, const RECT* rect, D3DCOLOR color) {
    return impl->inner->ColorFill(surface, rect, color);
}

HRESULT MyID3D9Device::CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    return impl->inner->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle);
}

HRESULT MyID3D9Device::GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) {
    return impl->inner->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
}

HRESULT MyID3D9Device::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil)
{
    if (!impl || !impl->inner) return D3DERR_INVALIDCALL;

    HRESULT hr = impl->inner->SetDepthStencilSurface(pNewZStencil);
    zm_trace_hr("MyID3D9Device::SetDepthStencilSurface", hr);
    ZM_LOG_UNSUP("MyID3D9Device::SetDepthStencilSurface", hr);

    if (FAILED(hr)) {
        char b[256];
        sprintf(b, "[ZeroMod] SetDepthStencilSurface(%p)\n", (void*)pNewZStencil);
        OutputDebugStringA(b);
        return hr;
    }

    if (impl->cached_dsv) {
        impl->cached_dsv->Release();
        impl->cached_dsv = nullptr;
    }

    impl->cached_dsv = pNewZStencil;
    if (impl->cached_dsv)
        impl->cached_dsv->AddRef();

    return hr;
}


HRESULT MyID3D9Device::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) {
    return impl->inner->GetDepthStencilSurface(ppZStencilSurface);
}

HRESULT MyID3D9Device::BeginScene() {
    HRESULT hr = impl->inner->BeginScene();
    zm_trace_hr("MyID3D9Device::BeginScene", hr);
    ZM_LOG_UNSUP("MyID3D9Device::BeginScene", hr);
    return hr;
}

HRESULT MyID3D9Device::EndScene() {
    HRESULT hr = impl->inner->EndScene();
    zm_trace_hr("MyID3D9Device::EndScene", hr);
    ZM_LOG_UNSUP("MyID3D9Device::EndScene", hr);
    return hr;
}


HRESULT MyID3D9Device::Clear(DWORD rect_count, const D3DRECT* rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil) {
    return impl->inner->Clear(rect_count, rects, flags, color, z, stencil);
}

HRESULT MyID3D9Device::SetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) {
    return impl->inner->SetTransform(state, matrix);
}

HRESULT MyID3D9Device::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {
    return impl->inner->GetTransform(State, pMatrix);
}

HRESULT MyID3D9Device::MultiplyTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* matrix) {
    return impl->inner->MultiplyTransform(state, matrix);
}

HRESULT MyID3D9Device::SetViewport(const D3DVIEWPORT9* viewport)
{
    if (!impl || !impl->inner)
        return D3DERR_INVALIDCALL;

    if (viewport) {
        impl->cached_vp = *viewport;

        // Optional: keep a “last known good” to avoid 0×0 stomps
        if (impl->cached_vp.Width == 0 || impl->cached_vp.Height == 0) {
            // log once; don't overwrite last-good if stored
            if (LOG_STARTED) OutputDebugStringA("[ZeroMod] SetViewport got 0x0\n");
        }
    }

    return impl->inner->SetViewport(viewport);
}

HRESULT MyID3D9Device::GetViewport(D3DVIEWPORT9* pViewport) {
    return impl->inner->GetViewport(pViewport);
}

HRESULT MyID3D9Device::SetMaterial(const D3DMATERIAL9* material) {
    return impl->inner->SetMaterial(material);
}

HRESULT MyID3D9Device::GetMaterial(D3DMATERIAL9* pMaterial) {
    return impl->inner->GetMaterial(pMaterial);
}

HRESULT MyID3D9Device::SetLight(DWORD index, const D3DLIGHT9* light) {
    return impl->inner->SetLight(index, light);
}

HRESULT MyID3D9Device::GetLight(DWORD Index, D3DLIGHT9* pLight) {
    return impl->inner->GetLight(Index, pLight);
}

HRESULT MyID3D9Device::LightEnable(DWORD Index, BOOL Enable) {
    return impl->inner->LightEnable(Index, Enable);
}

HRESULT MyID3D9Device::GetLightEnable(DWORD Index, BOOL* pEnable) {
    return impl->inner->GetLightEnable(Index, pEnable);
}

HRESULT MyID3D9Device::SetClipPlane(DWORD index, const float* plane) {
    return impl->inner->SetClipPlane(index, plane);
}

HRESULT MyID3D9Device::GetClipPlane(DWORD Index, float* pPlane) {
    return impl->inner->GetClipPlane(Index, pPlane);
}

HRESULT MyID3D9Device::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) {
    return impl->inner->GetRenderState(State, pValue);
}

HRESULT MyID3D9Device::CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) {
    return impl->inner->CreateStateBlock(Type, ppSB);
}

HRESULT MyID3D9Device::BeginStateBlock() {
    return impl->inner->BeginStateBlock();
}

HRESULT MyID3D9Device::EndStateBlock(IDirect3DStateBlock9** ppSB) {
    return impl->inner->EndStateBlock(ppSB);
}

HRESULT MyID3D9Device::SetClipStatus(const D3DCLIPSTATUS9* clip_status) {
    return impl->inner->SetClipStatus(clip_status);
}

HRESULT MyID3D9Device::GetClipStatus(D3DCLIPSTATUS9* pClipStatus) {
    return impl->inner->GetClipStatus(pClipStatus);
}

HRESULT MyID3D9Device::GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture) {
    return impl->inner->GetTexture(Stage, ppTexture);
}

HRESULT MyID3D9Device::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) {
    return impl->inner->GetTextureStageState(Stage, Type, pValue);
}

HRESULT MyID3D9Device::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) {
    return impl->inner->SetTextureStageState(Stage, Type, Value);
}

HRESULT MyID3D9Device::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) {
    return impl->inner->GetSamplerState(Sampler, Type, pValue);
}

HRESULT MyID3D9Device::ValidateDevice(DWORD* pNumPasses) {
    return impl->inner->ValidateDevice(pNumPasses);
}

HRESULT MyID3D9Device::SetPaletteEntries(UINT palette_idx, const PALETTEENTRY* entries) {
    return impl->inner->SetPaletteEntries(palette_idx, entries);
}

HRESULT MyID3D9Device::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries) {
    return impl->inner->GetPaletteEntries(PaletteNumber, pEntries);
}

HRESULT MyID3D9Device::SetCurrentTexturePalette(UINT PaletteNumber) {
    return impl->inner->SetCurrentTexturePalette(PaletteNumber);
}

HRESULT MyID3D9Device::GetCurrentTexturePalette(UINT* PaletteNumber) {
    return impl->inner->GetCurrentTexturePalette(PaletteNumber);
}

HRESULT MyID3D9Device::SetScissorRect(const RECT* rect) {
    return impl->inner->SetScissorRect(rect);
}

HRESULT MyID3D9Device::GetScissorRect(RECT* pRect) {
    return impl->inner->GetScissorRect(pRect);
}

HRESULT MyID3D9Device::SetSoftwareVertexProcessing(BOOL bSoftware) {
    return impl->inner->SetSoftwareVertexProcessing(bSoftware);
}

BOOL MyID3D9Device::GetSoftwareVertexProcessing() {
    return impl->inner->GetSoftwareVertexProcessing();
}

HRESULT MyID3D9Device::SetNPatchMode(float nSegments) {
    return impl->inner->SetNPatchMode(nSegments);
}

float MyID3D9Device::GetNPatchMode() {
    return impl->inner->GetNPatchMode();
}

HRESULT MyID3D9Device::DrawPrimitiveUP(D3DPRIMITIVETYPE primitive_type, UINT primitive_count, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    return impl->inner->DrawPrimitiveUP(primitive_type, primitive_count, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT MyID3D9Device::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE primitive_type, UINT min_vertex_idx, UINT num_vertices, UINT primitive_count, const void* pIndexData, D3DFORMAT IndexDataFormat, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    return impl->inner->DrawIndexedPrimitiveUP(primitive_type, min_vertex_idx, num_vertices, primitive_count, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT MyID3D9Device::ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) {
    return impl->inner->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);
}

HRESULT MyID3D9Device::CreateVertexDeclaration(const D3DVERTEXELEMENT9* elements, IDirect3DVertexDeclaration9** ppDecl) {
    return impl->inner->CreateVertexDeclaration(elements, ppDecl);
}

HRESULT MyID3D9Device::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) {
    return impl->inner->SetVertexDeclaration(pDecl);
}

HRESULT MyID3D9Device::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) {
    return impl->inner->GetVertexDeclaration(ppDecl);
}

HRESULT MyID3D9Device::GetFVF(DWORD* pFVF) {
    return impl->inner->GetFVF(pFVF);
}

HRESULT MyID3D9Device::CreateVertexShader(const DWORD* byte_code, IDirect3DVertexShader9** shader) {
    return impl->inner->CreateVertexShader(byte_code, shader);
}

HRESULT MyID3D9Device::GetVertexShader(IDirect3DVertexShader9** ppShader) {
    return impl->inner->GetVertexShader(ppShader);
}

HRESULT MyID3D9Device::GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
    return impl->inner->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT MyID3D9Device::SetVertexShaderConstantI(UINT reg_idx, const int* data, UINT count) {
    return impl->inner->SetVertexShaderConstantI(reg_idx, data, count);
}

HRESULT MyID3D9Device::GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
    return impl->inner->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT MyID3D9Device::SetVertexShaderConstantB(UINT reg_idx, const BOOL* data, UINT count) {
    return impl->inner->SetVertexShaderConstantB(reg_idx, data, count);
}

HRESULT MyID3D9Device::GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
    return impl->inner->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT MyID3D9Device::GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* OffsetInBytes, UINT* pStride) {
    return impl->inner->GetStreamSource(StreamNumber, ppStreamData, OffsetInBytes, pStride);
}

HRESULT MyID3D9Device::SetStreamSourceFreq(UINT StreamNumber, UINT Divider) {
    return impl->inner->SetStreamSourceFreq(StreamNumber, Divider);
}

HRESULT MyID3D9Device::GetStreamSourceFreq(UINT StreamNumber, UINT* Divider) {
    return impl->inner->GetStreamSourceFreq(StreamNumber, Divider);
}

HRESULT MyID3D9Device::GetIndices(IDirect3DIndexBuffer9** ppIndexData) {
    return impl->inner->GetIndices(ppIndexData);
}

HRESULT MyID3D9Device::CreatePixelShader(
    const DWORD* byte_code,
    IDirect3DPixelShader9** shader
) {
    if (!shader)
        return D3DERR_INVALIDCALL;

    *shader = nullptr;

    // 1) Create the real inner shader first.
    HRESULT hr = impl->inner->CreatePixelShader(byte_code, shader);
    if (FAILED(hr) || !*shader)
        return hr;

    IDirect3DPixelShader9* inner_ps = *shader;

    // 2) Query actual bytecode size + bytecode blob (DX9 has no BytecodeLength param).
    UINT sz = 0;
    HRESULT hr_sz = inner_ps->GetFunction(nullptr, &sz);
    std::vector<uint8_t> bc;
    if (SUCCEEDED(hr_sz) && sz) {
        bc.resize(sz);
        UINT got = sz;
        HRESULT hr_get = inner_ps->GetFunction(bc.data(), &got);
        if (FAILED(hr_get)) {
            bc.clear();
            sz = 0;
        }
        else {
            // driver may adjust size
            sz = got;
        }
    }
    else {
        sz = 0;
    }

    // 3) Hash the bytecode (same Murmur seed 0 as DX10).
    DWORD hash = 0;
    if (sz && !bc.empty())
        MurmurHash3_x86_32(bc.data(), sz, 0, &hash);

    // 4) Produce source string via existing ShaderLogger pipeline (keeps alpha_discard behavior).
    //    If bytecode couldn't be read, fall back to the original pointer.
    const void* src_ptr = (sz && !bc.empty()) ? (const void*)bc.data() : (const void*)byte_code;
    ShaderLogger shader_source{ src_ptr };

    // 5) Construct wrapper and return wrapper pointer to caller (DX10 model).
    MyID3D9PixelShader* wrap =
        new MyID3D9PixelShader(inner_ps, hash, (SIZE_T)sz, shader_source.source);

    // >>> FIX: caller will receive the wrapper, not the inner. Release the inner’s caller ref.
    inner_ps->Release();
    inner_ps = nullptr;

    *shader = (IDirect3DPixelShader9*)wrap;
    return hr;
}

HRESULT MyID3D9Device::GetPixelShader(IDirect3DPixelShader9** ppShader)
{
    if (!ppShader)
        return D3DERR_INVALIDCALL;

    *ppShader = nullptr;

    // 1) Ask the REAL device what is bound right now.
    IDirect3DPixelShader9* inner_ps = nullptr;
    HRESULT hr = impl->inner->GetPixelShader(&inner_ps);
    if (FAILED(hr))
        return hr;

    if (!inner_ps) {
        // nothing bound
        *ppShader = nullptr;
        return S_OK;
    }

    // 2) If we have a wrapper for this inner, return the wrapper identity.
    auto it = cached_pss_map.find(inner_ps);
    if (it != cached_pss_map.end() && it->second)
    {
        IDirect3DPixelShader9* wrap_ps =
            static_cast<IDirect3DPixelShader9*>(it->second);

        // The contract: returned interface is AddRef'd.
        wrap_ps->AddRef();

        // Release the inner got from GetPixelShader (AddRef'd by runtime).
        inner_ps->Release();

        *ppShader = wrap_ps;
        return S_OK;
    }

    // 3) Fallback: no wrapper known; return inner as-is (already AddRef'd).
    *ppShader = inner_ps;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetPixelShader(IDirect3DPixelShader9* pPixelShader)
{
    // 1) Maintain cache lifetime explicitly (prevents wrapper destruction between draws)
    if (pPixelShader)
        pPixelShader->AddRef();

    if (impl->cached_ps) {
        impl->cached_ps->Release();
        impl->cached_ps = nullptr;
    }

    impl->cached_ps = pPixelShader;

    // 2) Bind INNER to the real device (so the real device never sees wrapper identity)
    IDirect3DPixelShader9* bind_ps = pPixelShader;

    if (pPixelShader) {
        auto it = cached_pss_map.find(pPixelShader);
        if (it != cached_pss_map.end() && it->second) {
            IDirect3DPixelShader9* inner_ps = it->second->get_inner();
            if (inner_ps)
                bind_ps = inner_ps;
        }
    }

    return impl->inner->SetPixelShader(bind_ps);
}

HRESULT MyID3D9Device::GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
    return impl->inner->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT MyID3D9Device::SetPixelShaderConstantI(UINT reg_idx, const int* data, UINT count) {
    return impl->inner->SetPixelShaderConstantI(reg_idx, data, count);
}

HRESULT MyID3D9Device::GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
    return impl->inner->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT MyID3D9Device::SetPixelShaderConstantB(UINT reg_idx, const BOOL* data, UINT count) {
    return impl->inner->SetPixelShaderConstantB(reg_idx, data, count);
}

HRESULT MyID3D9Device::GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
    return impl->inner->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT MyID3D9Device::DrawRectPatch(UINT handle, const float* segment_count, const D3DRECTPATCH_INFO* patch_info) {
    return impl->inner->DrawRectPatch(handle, segment_count, patch_info);
}

HRESULT MyID3D9Device::DrawTriPatch(UINT handle, const float* segment_count, const D3DTRIPATCH_INFO* patch_info) {
    return impl->inner->DrawTriPatch(handle, segment_count, patch_info);
}

HRESULT MyID3D9Device::DeletePatch(UINT Handle) {
    return impl->inner->DeletePatch(Handle);
}
