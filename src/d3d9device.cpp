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
#define PS_HASH_T1 0xa54c4b2
#define PS_HASH_T2 0xc9b117d5
#define PS_HASH_T3 0x1f4c05ac
#define SO_B_LEN (sizeof(float) * 4 * 4 * 6 * 100)

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
    Overlay* overlay;
    Config* config;

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
        MyIDirect3DTexture9* srv;
        MyIDirect3DTexture9* rtv_tex;
        MyID3D9PixelShader* ps;
        MyID3D9VertexShader* vs;
        bool t1;
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
        if (filter_state.srv) filter_state.srv->Release();
        if (filter_state.rtv_tex) filter_state.rtv_tex->Release();
        if (filter_state.ps) filter_state.ps->Release();
        if (filter_state.vs) filter_state.vs->Release();
        if (filter_state.psss) filter_state.psss->Release();
        if (filter_state.vertex_buffer) filter_state.vertex_buffer->Release();
        filter_state = {};
    }

    bool render_interp = false;
    bool render_linear = false;
    bool render_enhanced = false;
    UINT linear_test_width = 0;
    UINT linear_test_height = 0;

    void update_config() {
        if (!config) return;

        if (config->linear_test_updated) {
            config->begin_config();
            linear_test_width = config->linear_test_width;
            linear_test_height = config->linear_test_height;
            config->linear_test_updated = false;
            config->end_config();
        }

#define GET_SET_CONFIG_BOOL(v, m) do { \
    bool v ## _value = config->v; \
    if (render_ ## v != v ## _value) { \
        render_ ## v = v ## _value; \
        if (v ## _value) overlay->push_text(m " enabled"); else overlay->push_text(m " disabled"); \
    } \
} while (0)

        GET_SET_CONFIG_BOOL(interp, "Interp fix");
        GET_SET_CONFIG_BOOL(linear, "Force linear filtering");
        GET_SET_CONFIG_BOOL(enhanced, "Enhanced Type 1 filter");

#define SLANG_SHADERS \
    X(2d) \
    X(gba) \
    X(ds)

#define X(v) \
    config->slang_shader_ ## v ## _updated ||

        if (SLANG_SHADERS false) {

#undef X

#define X(v) \
    bool slang_shader_ ## v ## _updated = \
        config->slang_shader_ ## v ## _updated; \
    std::string slang_shader_ ## v; \
    if (slang_shader_ ## v ## _updated) { \
        slang_shader_ ## v = config->slang_shader_ ## v; \
        config->slang_shader_ ## v ## _updated = false; \
    }

            config->begin_config();
            SLANG_SHADERS
                config->end_config();

#undef X

#define X(v) \
    if (slang_shader_ ## v ## _updated) { \
        if (!d3d9_ ## v) { \
            if (!(d3d9_ ## v = ZeroMod::d3d9_gfx_init(inner, D3DFMT_A8R8G8B8))) { \
                overlay->push_text("Failed to initialize slang shader " #v); \
            } \
        } \
        if (d3d9_ ## v) { \
            if (!slang_shader_ ## v.size()) { \
                ZeroMod::d3d9_gfx_set_shader(d3d9_ ## v, nullptr); \
                overlay->push_text("Slang shader " #v " disabled"); \
            } else if (ZeroMod::d3d9_gfx_set_shader(d3d9_ ## v, slang_shader_ ## v.c_str())) { \
                overlay->push_text("Slang shader " #v " set to " + slang_shader_ ## v); \
            } else { \
                overlay->push_text("Failed to set slang shader " #v " to " + slang_shader_ ## v); \
            } \
        } \
    }

            SLANG_SHADERS

#undef X

#undef SLANG_SHADERS

        }
    }

    void create_sampler(
        D3DTEXTUREFILTERTYPE filter,
        IDirect3DSamplerState*& sampler,
        D3DTEXTUREADDRESS address = D3DTADDRESS_CLAMP
    ) {
        // Set sampler states directly
        inner->SetSamplerState(0, D3DSAMP_MAGFILTER, filter);
        inner->SetSamplerState(0, D3DSAMP_MINFILTER, filter);
        inner->SetSamplerState(0, D3DSAMP_ADDRESSU, address);
        inner->SetSamplerState(0, D3DSAMP_ADDRESSV, address);
        inner->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
        inner->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 16);
        inner->SetSamplerState(0, D3DSAMP_ADDRESSW, address);
        inner->SetSamplerState(0, D3DSAMP_BORDERCOLOR, 0x00000000);
        inner->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, 0);
        inner->SetSamplerState(0, D3DSAMP_MIPMAPLODBIAS, 0);
        inner->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);

        sampler = nullptr;  // Adjust according to your actual implementation needs.
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
            // Handle error (e.g., log error)
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
    MyID3D9Texture2D* tex,
    UINT width,
    UINT height,
    UINT orig_width,
    UINT orig_height,
    bool need_vp
) {
    if (need_vp && need_render_vp &&
        (render_width != width || render_height != height || render_orig_width != orig_width || render_orig_height != orig_height)) {
        return false;
    }

    if (!almost_equal(tex->get_orig_width(), orig_width) || !almost_equal(tex->get_orig_height(), orig_height)) {
        return false;
    }

    D3DSURFACE_DESC desc;
    tex->GetLevelDesc(0, &desc);

    if (!almost_equal(desc.Width, width) || !almost_equal(desc.Height, height)) {
        if (tex->get_sc()) {
            return false;
        }

        tex->Release();
        inner->CreateTexture(width, height, 1, D3DUSAGE_RENDERTARGET, desc.Format, D3DPOOL_DEFAULT, (IDirect3DTexture9**)&tex, NULL);
        tex->GetSurfaceLevel(0, &rtv);

        // Update associated RTVs, SRVs, and DSVs if necessary
        for (auto rtv : tex->get_rtvs()) {
            auto myRtv = dynamic_cast<MyID3D9RenderTargetView*>(rtv);
            if (myRtv) {
                myRtv->Release();
                tex->GetSurfaceLevel(0, reinterpret_cast<IDirect3DSurface9**>(&myRtv->get_inner()));
            }
        }
        for (auto srv : tex->get_srvs()) {
            auto mySrv = dynamic_cast<MyID3D9ShaderResourceView*>(srv);
            if (mySrv) {
                mySrv->Release();
                mySrv->get_inner() = tex->get_inner();
                mySrv->get_inner()->AddRef();
            }
        }
        for (auto dsv : tex->get_dsvs()) {
            auto myDsv = dynamic_cast<MyID3D9DepthStencilView*>(dsv);
            if (myDsv) {
                myDsv->Release();
                tex->GetSurfaceLevel(0, reinterpret_cast<IDirect3DSurface9**>(&myDsv->get_inner()));
            }
        }
    }

    if (almost_equal(width, orig_width) && almost_equal(height, orig_height)) {
        return false;
    }

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
        UINT render_width = render_size.render_width;
        UINT render_height = render_size.render_height;
        get_resolution_mul(render_width, render_height, width, height);
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
        float ps_cb_data[4] = {
            (float)width,
            (float)height,
            (float)(1.0 / width),
            (float)(1.0 / height)
        };

        // Initialize D3DVERTEXBUFFER_DESC to zero
     //   D3DVERTEXBUFFER_DESC desc = {};
    //    desc.Size = sizeof(ps_cb_data);
    //    desc.Usage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;
    //    desc.FVF = D3DFVF_XYZRHW;
    //    desc.Pool = D3DPOOL_DEFAULT;

        D3DLOCKED_RECT rect;
        tex->tex->LockRect(0, &rect, NULL, D3DLOCK_DISCARD);
        memcpy(rect.pBits, ps_cb_data, sizeof(ps_cb_data));
        tex->tex->UnlockRect(0);
    }


    void create_tex_and_view_1_v(
        std::vector<TextureViewsAndBuffer*>& tex_v,
        UINT width,
        UINT height
    ) {
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
        UINT render_width = render_size.render_width;
        UINT render_height = render_size.render_height;
        get_resolution_mul(render_width, render_height, width, height);
        create_tex_and_views(render_width, render_height, tex);
        create_texture(tex->width, tex->height, tex->tex_ds, D3DFMT_D24S8, D3DUSAGE_DEPTHSTENCIL, D3DPOOL_DEFAULT);
        create_dsv(tex->tex_ds, tex->dsv, D3DFMT_D24S8);
    }

    struct FilterTemp {
        IDirect3DSamplerState* sampler_nn;
        IDirect3DSamplerState* sampler_linear;
        IDirect3DSamplerState* sampler_wrap;
        TextureAndViews* tex_nn_zero;

        TextureAndViews* tex_nn_zx;
        TextureAndDepthViews* tex_t2;

        std::vector<TextureViewsAndBuffer*> tex_1_zero;
        std::vector<TextureViewsAndBuffer*> tex_1_zx;
    } filter_temp = {};

    void filter_temp_init() {
        filter_temp_shutdown();
        create_sampler(D3DTEXF_POINT, filter_temp.sampler_nn);
        create_sampler(D3DTEXF_LINEAR, filter_temp.sampler_linear);
        create_sampler(D3DTEXF_POINT, filter_temp.sampler_wrap, D3DTADDRESS_WRAP);
        filter_temp.tex_nn_zero = new TextureAndViews{};
        create_tex_and_views_nn(filter_temp.tex_nn_zero, ZERO_WIDTH, ZERO_HEIGHT);
        filter_temp.tex_nn_zx = new TextureAndViews{};
        create_tex_and_views_nn(filter_temp.tex_nn_zx, ZX_WIDTH, ZX_HEIGHT);
        filter_temp.tex_t2 = new TextureAndDepthViews{};
        create_tex_and_depth_views_2(NOISE_WIDTH, NOISE_HEIGHT, filter_temp.tex_t2);
        create_tex_and_view_1_v(filter_temp.tex_1_zero, ZERO_WIDTH_FILTERED, ZERO_HEIGHT_FILTERED);
        create_tex_and_view_1_v(filter_temp.tex_1_zx, ZX_WIDTH_FILTERED, ZX_HEIGHT_FILTERED);
    }

    void filter_temp_shutdown() {
        if (filter_temp.sampler_nn) filter_temp.sampler_nn->Release();
        if (filter_temp.sampler_linear) filter_temp.sampler_linear->Release();
        if (filter_temp.sampler_wrap) filter_temp.sampler_wrap->Release();
        if (filter_temp.tex_nn_zero) delete filter_temp.tex_nn_zero;
        if (filter_temp.tex_nn_zx) delete filter_temp.tex_nn_zx;
        if (filter_temp.tex_t2) delete filter_temp.tex_t2;
        for (auto tex : filter_temp.tex_1_zero) delete tex;
        for (auto tex : filter_temp.tex_1_zx) delete tex;
        filter_temp = {};
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

    void linear_conditions_begin() {
        linear_restore = false;
        stream_out = false;
        linear_conditions = {};
        if (!render_linear && !LOG_STARTED) return;

        if (cached_ps) {
            auto myCachedPs = dynamic_cast<MyID3D9PixelShader*>(cached_ps);
            if (myCachedPs) {
                linear_conditions.alpha_discard = myCachedPs->get_alpha_discard();
            }
        }

        IDirect3DSamplerState* sss[MAX_SAMPLERS] = {};
        auto sss_inner = sss;
        auto pssss = cached_pssss;
        for (auto srvs = cached_pssrvs; *srvs; ++srvs, ++pssss, ++sss_inner) {
            auto srv = dynamic_cast<MyID3D9Texture2D*>(*srvs);
            linear_conditions.texs_descs.push_back(
                srv && srv->get_desc().Type == D3D_SRV_DIMENSION_TEXTURE2D ?
                &srv->get_desc() :
                nullptr
            );
            auto ss = dynamic_cast<MyID3D9SamplerState*>(*pssss);
            linear_conditions.samplers_descs.push_back(
                ss ? &ss->get_desc() : nullptr
            );
            *sss_inner = ss ? ss->get_inner() : nullptr;
        }

        if (linear_conditions.alpha_discard == PIXEL_SHADER_ALPHA_DISCARD::EQUAL) {
            if (render_linear && !linear_conditions.texs_descs.empty()) {
                UINT width = linear_conditions.texs_descs[0]->Width;
                UINT height = linear_conditions.texs_descs[0]->Height;
                if (!((width == 512 || width == 256) && height == 256) &&
                    !(width == linear_test_width && height == linear_test_height)) {
                    inner->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
                    linear_restore = true;
                }
            }
        }
    }

    void linear_conditions_end() {
        if (linear_restore) {
            for (int i = 0; i < MAX_SAMPLERS; ++i) {
                if (render_pssss[i]) {
                    auto mySamplerState = dynamic_cast<MyID3D9SamplerState*>(render_pssss[i]);
                    if (mySamplerState) {
                        D3DSAMPLER_DESC desc = {}; // Ensure desc is initialized
                        mySamplerState->GetDesc(&desc);

                        // Use union to safely cast float to DWORD
                        union FloatToDWORD {
                            float f;
                            DWORD d;
                        };

                        FloatToDWORD mipLODBias = {}; // Ensure mipLODBias is initialized
                        FloatToDWORD borderColor = {}; // Ensure borderColor is initialized

                        mipLODBias.f = desc.MipLODBias;
                        borderColor.f = *(reinterpret_cast<float*>(&desc.BorderColor));

                        inner->SetSamplerState(i, D3DSAMP_MAGFILTER, desc.MagFilter);
                        inner->SetSamplerState(i, D3DSAMP_MINFILTER, desc.MinFilter);
                        inner->SetSamplerState(i, D3DSAMP_MIPFILTER, desc.MipFilter);
                        inner->SetSamplerState(i, D3DSAMP_ADDRESSU, desc.AddressU);
                        inner->SetSamplerState(i, D3DSAMP_ADDRESSV, desc.AddressV);
                        inner->SetSamplerState(i, D3DSAMP_ADDRESSW, desc.AddressW);
                        inner->SetSamplerState(i, D3DSAMP_MIPMAPLODBIAS, mipLODBias.d);
                        inner->SetSamplerState(i, D3DSAMP_MAXANISOTROPY, desc.MaxAnisotropy);
                        inner->SetSamplerState(i, D3DSAMP_BORDERCOLOR, borderColor.d); // Note: Direct3D 9 expects a single DWORD for border color
                    }
                }
            }
        }
        if (stream_out) {
            // Direct3D 9 does not have geometry shaders or stream output
            // Adapt this part according to your specific needs if stream output is required
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
    MyID3D9PixelShader* cached_ps = NULL;
    IDirect3DVertexBuffer9* cached_vb = NULL;
    D3DPRIMITIVETYPE cached_pt = D3DPT_TRIANGLESTRIP;
    D3DVERTEXELEMENT9 cached_il[MAXD3DDECLLENGTH + 1] = {};
   // IDirect3DSamplerState* render_pssss[MAX_SAMPLERS];
    float* render_pscbs[MAX_CONSTANT_BUFFERS];  // Update this line
    IDirect3DTexture9* render_pssrvs[MAX_SHADER_RESOURCES] = {};  // Update this line
    IDirect3DSurface9* cached_rtv = NULL;
    IDirect3DSurface9* cached_dsv = NULL;
    IDirect3DSamplerState* cached_pssss[MAX_SAMPLERS] = {};
    IDirect3DSamplerState* render_pssss[MAX_SAMPLERS] = {}; // Define render_pssss
    IDirect3DTexture9* cached_pssrvs[MAX_SHADER_RESOURCES] = {};
  //  IDirect3DSamplerState* render_pssss[MAX_SAMPLERS] = {};  // Update this line
    D3DVIEWPORT9 render_vp = {};
    D3DVIEWPORT9 cached_vp = {};
   // bool need_render_vp = false;
    bool is_render_vp = false;

    void present() {
        clear_filter();
        update_config();
        ++frame_count;
    }

    void resize_buffers(UINT width, UINT height) {
        render_size.resize(width, height);
        clear_filter();
        update_config();
        filter_temp_init();
        frame_count = 0;
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

        // Proper logging call with LOG_STARTED check
       // if (LOGGER) {
      //      LOGGER->log_fun("MyIDirect3DDevice9::set_render_vp");
       //     LOG_ARG("cached_rtv: ", cached_rtv);
      //      LOG_ARG("cached_dsv: ", cached_dsv);
            // LOG_ARG_TYPE(cached_pssrvs, srvs_n); // Uncomment and fix this if needed
       //     LogIf<5>{is_render_vp};
      //      LOG_ARG("render_vp: ", render_vp);
        //    LOG_ARG("render_width: ", render_width);
     //       LOG_ARG("render_height: ", render_height);
      //      LOG_ARG("render_orig_width: ", render_orig_width);
      //      LOG_ARG("render_orig_height: ", render_orig_height);
      //  }
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

    Impl(IDirect3DDevice9** inner, UINT width, UINT height) : inner(*inner), width(width), height(height) {
        resize_buffers(width, height);

        HRESULT result;

        // Initialize render_pscbs


        // Initialize render_pssrvs to nullptr
        for (int i = 0; i < MAX_SHADER_RESOURCES; ++i) {
            render_pssrvs[i] = nullptr;
        }

        // Initialize render_pssss to nullptr
        for (int i = 0; i < MAX_SAMPLERS; ++i) {
            render_pssss[i] = nullptr;
        }

        // Create vertex buffer
        result = this->inner->CreateVertexBuffer(SO_B_LEN, D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &so_bt, NULL);
        if (FAILED(result)) {
            // Handle error
        }

        // Create system memory vertex buffer
        result = this->inner->CreateVertexBuffer(SO_B_LEN, D3DUSAGE_WRITEONLY, 0, D3DPOOL_SYSTEMMEM, &so_bs, NULL);
        if (FAILED(result)) {
            // Handle error
        }

        // Additional buffer creation or initialization, if needed
    }


    ~Impl() {
        filter_temp_shutdown();
        clear_filter();

        // Clean up render_pscbs
        for (int i = 0; i < MAX_CONSTANT_BUFFERS; ++i) {
            delete[] render_pscbs[i];
        }

        // Clean up render_pssrvs
        for (int i = 0; i < MAX_SHADER_RESOURCES; ++i) {
            if (render_pssrvs[i]) {
                render_pssrvs[i]->Release();
                render_pssrvs[i] = nullptr;
            }
        }

        // Clean up render_pssss
        for (int i = 0; i < MAX_SAMPLERS; ++i) {
            if (render_pssss[i]) {
                render_pssss[i]->Release();
                render_pssss[i] = nullptr;
            }
        }

        if (so_bt) so_bt->Release();
        if (so_bs) so_bs->Release();
    }

    void Draw(UINT VertexCount, UINT StartVertexLocation) {
        set_render_vp();
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
        auto restore_ps = [this] {
            inner->SetPixelShader(cached_ps->get_inner());
            };
        auto restore_vps = [this] {
            inner->SetViewport(&render_vp);
            };
        auto restore_pscbs = [this] {
            inner->SetVertexShaderConstantF(0, render_pscbs[0], MAX_CONSTANT_BUFFERS);
            };
        auto restore_rtvs = [this] {
            inner->SetRenderTarget(0, cached_rtv);
            inner->SetDepthStencilSurface(cached_dsv);
            };
        auto restore_pssrvs = [this] {
            inner->SetTexture(0, render_pssrvs[0]);
            };
        auto restore_pssss = [this] {
            for (DWORD i = 0; i < MAX_SAMPLERS; ++i) {
                if (render_pssss[i]) {
                    D3DSAMPLER_DESC desc;
                    render_pssss[i]->GetDesc(&desc);
                    inner->SetSamplerState(i, D3DSAMP_MAGFILTER, desc.MagFilter);
                    inner->SetSamplerState(i, D3DSAMP_MINFILTER, desc.MinFilter);
                    inner->SetSamplerState(i, D3DSAMP_MIPFILTER, desc.MipFilter);
                }
            }
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
        auto set_psss = [this](IDirect3DSamplerState* psss) {
            for (DWORD i = 0; i < MAX_SAMPLERS; ++i) {
                if (psss) {
                    D3DSAMPLER_DESC desc;
                    psss->GetDesc(&desc);
                    inner->SetSamplerState(i, D3DSAMP_MAGFILTER, desc.MagFilter);
                    inner->SetSamplerState(i, D3DSAMP_MINFILTER, desc.MinFilter);
                    inner->SetSamplerState(i, D3DSAMP_MIPFILTER, desc.MipFilter);
                }
            }
            };

        MyIDirect3DTexture9* srv = dynamic_cast<MyIDirect3DTexture9*>(*cached_pssrvs);

        if (!render_interp && !render_linear && !filter_ss && !filter_ss_gba && !filter_ss_ds) return;
        if (VertexCount != 4) return;

        auto psss = *cached_pssss;
        if (!psss) return;
        filter_next = filter && psss == filter_state.psss;

        if (!srv) return;

        D3DSURFACE_DESC srv_desc;
        srv->GetLevelDesc(0, &srv_desc);
        filter_next &= srv == filter_state.rtv_tex;

        bool zx = false;
        if (filter_next) {
            zx = false;
        }
        else if (srv_desc.Width == ZERO_WIDTH && srv_desc.Height == ZERO_HEIGHT) {
            filter_ss |= filter_ss_gba;
        }
        else if (srv_desc.Width == ZX_WIDTH && srv_desc.Height == ZX_HEIGHT) {
            zx = true;
            filter_ss |= filter_ss_ds;
        }
        else {
            return;
        }

        auto rtv = cached_rtv;
        if (!rtv) return;

        D3DSURFACE_DESC rtv_desc;
        dynamic_cast<MyIDirect3DTexture9*>(rtv)->GetLevelDesc(0, &rtv_desc);
        if (rtv_desc.Width != srv_desc.Width * 2 || rtv_desc.Height != srv_desc.Height * 2) return;

        clear_filter();
        if (cached_ps) {
            filter_state.ps = dynamic_cast<MyID3D9PixelShader*>(cached_ps);
            if (filter_state.ps) filter_state.ps->AddRef();
            switch (filter_state.ps->get_bytecode_hash()) {
            case PS_HASH_T1: filter_state.t1 = true; break;
            case PS_HASH_T3: filter_state.t1 = false; break;
            default:
                filter_state.t1 = filter_state.ps->get_bytecode_length() >= PS_BYTECODE_LENGTH_T1_THRESHOLD;
                break;
            }
        }
        filter_state.vs = dynamic_cast<MyID3D9VertexShader*>(cached_vs);
        if (filter_state.vs) filter_state.vs->AddRef();
        filter_state.psss = dynamic_cast<MyID3D9SamplerState*>(psss);
        if (filter_state.psss) filter_state.psss->AddRef();
        filter_state.zx = zx;
        filter_state.start_vertex_location = StartVertexLocation;
        filter_state.vertex_buffer = cached_vbs.ppVertexBuffers[0];
        if (filter_state.vertex_buffer) filter_state.vertex_buffer->AddRef();
        filter_state.vertex_stride = cached_vbs.pStrides[0];
        filter_state.vertex_offset = cached_vbs.pOffsets[0];
        filter_state.srv = srv;
        srv->AddRef();
        filter_state.rtv_tex = dynamic_cast<MyIDirect3DTexture9*>(rtv);
        filter_state.rtv_tex->AddRef();

        auto draw_nn = [&](TextureAndViews* v) {
            set_viewport(v->width, v->height);
            set_rtv(v->rtv);
            set_srv(filter_state.srv->get_inner());
            if (render_linear) set_psss(filter_temp.sampler_nn);
            inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
            restore_vps();
            restore_rtvs();
            set_srv(v->srv);
            set_psss(filter_temp.sampler_linear);
            inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
            restore_pssrvs();
            restore_pssss();
            };

        using ZeroMod::video_viewport_t;
        using ZeroMod::d3d9_video_struct;

        auto draw_ss = [&](
            d3d9_video_struct* d3d9,
            MyIDirect3DTexture9* srv,
            D3DVIEWPORT9* render_vp,
            TextureAndViews* cached_tex = NULL
            ) {
                if (cached_tex) {
                    video_viewport_t vp = {
                        .x = 0,
                        .y = 0,
                        .width = static_cast<int>(cached_tex->width),
                        .height = static_cast<int>(cached_tex->height),
                        .full_width = static_cast<int>(cached_tex->width),
                        .full_height = static_cast<int>(cached_tex->height)
                    };
                    d3d9_update_viewport(
                        d3d9,
                        cached_tex->rtv,
                        &vp
                    );
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
                    d3d9_update_viewport(
                        d3d9,
                        rtv,
                        &vp
                    );
                }
            };

        auto draw_enhanced = [&](std::vector<TextureViewsAndBuffer*>& v_v) {
            auto v_it = v_v.begin();
            if (v_it == v_v.end()) {
                set_psss(filter_temp.sampler_linear);
                inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                restore_pssss();
                return;
            }
            set_filter_state_ps();
            auto set_it_viewport = [&] {
                set_viewport((*v_it)->width, (*v_it)->height);
                };
            set_it_viewport();
            auto set_it_rtv = [&] {
                set_rtv((*v_it)->rtv);
                };
            set_it_rtv();
            auto set_it_pscbs = [&] {
                inner->SetVertexShaderConstantF(0, (const float*)(&(*v_it)->ps_cb), 1);
                };
            set_it_pscbs();
            inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

            auto& v_it_prev = *v_it;  // Explicit reference
            auto set_it_prev_srv = [&] {
                set_srv(v_it_prev->srv);
                };
            while (++v_it != v_v.end()) {
                set_it_rtv();
                set_it_prev_srv();
                set_it_viewport();
                set_it_pscbs();
                inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                v_it_prev = *v_it;  // Update the reference
            }
            restore_ps();
            restore_vps();
            restore_rtvs();
            restore_pscbs();
            set_it_prev_srv();
            set_psss(filter_temp.sampler_linear);
            inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
            restore_pssrvs();
            restore_pssss();
            };

        if (filter_state.t1) {
            auto d3d9 = d3d9_2d;
            if (filter_state.zx) {
                if (filter_ss_ds) {
                    d3d9 = d3d9_ds;
                    filter_ss = true;
                }
            }
            else {
                if (filter_ss_gba) {
                    d3d9 = d3d9_gba;
                    filter_ss = true;
                }
            }
            if (filter_ss) {
                draw_ss(d3d9, filter_state.srv, &render_vp, render_interp ? filter_state.zx ? filter_temp.tex_nn_zx : filter_temp.tex_nn_zero : NULL);
            }
            else if (render_enhanced) {
                draw_enhanced(filter_state.zx ? filter_temp.tex_1_zx : filter_temp.tex_1_zero);
            }
            else {
                set_psss(filter_temp.sampler_linear);
                inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                restore_pssss();
            }
        }
        else {
            draw_nn(filter_state.zx ? filter_temp.tex_nn_zx : filter_temp.tex_nn_zero);
        }

        if (filter_next) {
            DWORD bytecode_hash = cached_ps ? cached_ps->get_bytecode_hash() : 0;
            switch (bytecode_hash) {
            case PS_HASH_T3:
                if (filter_state.srv != srv) {
                  //  MyVertexBuffer_Logger vertices = {
                      //  .input_layout = cached_il,
                     //   .vertex_buffer = cached_vbs.ppVertexBuffers[0],
                     //   .stride = cached_vbs.pStrides[0],
                    //    .offset = cached_vbs.pOffsets[0],
                    //    .VertexCount = VertexCount,
                    //    .StartVertexLocation = StartVertexLocation
                  //  };
                    inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                    return;
                }
                /* fall-through */
            case PS_HASH_T1:
                if (!render_linear) {
                  //  MyVertexBuffer_Logger vertices = {
                      //  .input_layout = cached_il,
                     //   .vertex_buffer = cached_vbs.ppVertexBuffers[0],
                     //   .stride = cached_vbs.pStrides[0],
                    //    .offset = cached_vbs.pOffsets[0],
                     //   .VertexCount = VertexCount,
                     //   .StartVertexLocation = StartVertexLocation
                 //   };
                    inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                    return;
                }
                set_psss(filter_temp.sampler_nn);
                inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                restore_pssss();
                break;
            case PS_HASH_T2:
                if (!(
                    (render_interp || render_linear) &&
                    filter_state.ps && filter_state.vs &&
                    filter_state.vertex_buffer
                    )) {
                   // MyVertexBuffer_Logger vertices = {
                      //  .input_layout = cached_il,
                     //   .vertex_buffer = cached_vbs.ppVertexBuffers[0],
                     //   .stride = cached_vbs.pStrides[0],
                      //  .offset = cached_vbs.pOffsets[0],
                     //   .VertexCount = VertexCount,
                      //  .StartVertexLocation = StartVertexLocation
                 //   };
                    inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                    return;
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
                    set_rtv(filter_temp.tex_t2->rtv, filter_temp.tex_t2->dsv);
                }
                inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

                if (render_linear) {
                    restore_rtvs();
                    restore_vps();
                    set_srv(filter_temp.tex_t2->srv);
                    set_psss(filter_temp.sampler_linear);
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
                restore_pssss();
                break;

            default: {
                linear_conditions_begin();
            //    MyVertexBuffer_Logger vertices = {
                 //   .input_layout = cached_il,
                 //   .vertex_buffer = cached_vbs.vertex_buffers[0],
                //    .stride = cached_vbs.pStrides[0],
                //    .offset = cached_vbs.pOffsets[0],
                //    .VertexCount = VertexCount,
                //    .StartVertexLocation = StartVertexLocation
            //    };

                inner->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
                linear_conditions_end();
                return;
            }
            }
        }
    }

};


// Section 1: Initialization and Core Functions

void MyID3D9Device::set_overlay(Overlay* overlay) {
    impl->overlay = overlay;
}

void MyID3D9Device::set_config(Config* config) {
    impl->config = config;
}

void MyID3D9Device::present() {
    // Present the back buffer to the screen
    impl->inner->Present(nullptr, nullptr, nullptr, nullptr);
}

void MyID3D9Device::resize_buffers(UINT width, UINT height) {
    // Resize the back buffer if necessary
    impl->resize_buffers(width, height);
}

void MyID3D9Device::resize_orig_buffers(UINT width, UINT height) {
    impl->cached_size.resize(width, height);
}

//IUNKNOWN_IMPL(MyID3D9Device, IDirect3DDevice9)

MyID3D9Device::MyID3D9Device(
    IDirect3DDevice9** inner,
    UINT width,
    UINT height
) :
    impl(new Impl(inner, width, height))
{
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
  //  LOG_MFUN();
    *inner = this;
}


MyID3D9Device::MyID3D9Device(IDirect3DDevice9* pOriginal) {
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
    // Uncomment the LOG_MFUN line if logging is needed.
    // LOG_MFUN(_, LOG_ARG(StartSlot), LOG_ARG(NumBuffers), LOG_ARG_TYPE(ppConstantBuffers, ArrayLoggerDeref, NumBuffers));

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
    // Uncomment the LOG_MFUN line if logging is needed.
    // LOG_MFUN(_, LOG_ARG(Stage), LOG_ARG(pTexture));

    return impl->inner->SetTexture(Stage, pTexture);
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::SetPixelShader(
    IDirect3DPixelShader9* pPixelShader
) {
    // Uncomment the LOG_MFUN line if logging is needed.
    // LOG_MFUN(_, LOG_ARG(pPixelShader));

    impl->cached_ps = static_cast<MyID3D9PixelShader*>(pPixelShader);
    return impl->inner->SetPixelShader(pPixelShader);
}


// Section 3: Sampler and Render State Management

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetSamplerState(
    DWORD Sampler,
    D3DSAMPLERSTATETYPE Type,
    DWORD Value
) {
    // LOG_MFUN(_,
  //  LOG_ARG(Sampler);
   //     LOG_ARG(Type);
   //     LOG_ARG(Value);
    // );

    HRESULT hr = impl->inner->SetSamplerState(Sampler, Type, Value);

    // Cache the sampler state if necessary for later use
#ifdef ENABLE_SAMPLER_STATE_CACHE
    impl->cached_sampler_states[Sampler][Type] = Value;
#endif

    return hr;
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetVertexShader(
    IDirect3DVertexShader9* pVertexShader
) {
    // LOG_MFUN(_,
   // LOG_ARG(pVertexShader);
    // );

    impl->cached_vs = pVertexShader;
    HRESULT hr = impl->inner->SetVertexShader(pVertexShader);
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
    impl->set_render_vp();  // Set render viewport
    impl->linear_conditions_begin();  // Begin linear filter conditions

    // Prepare vertex buffer logger
   // MyIndexedVertexBuffer_Logger vertices = {
   //     .input_layout = impl->cached_il,
   //     .vertex_buffer = impl->cached_vbs.vertex_buffers[0],
    //    .index_buffer = impl->cached_ib,
  //      .stride = impl->cached_vbs.pStrides[0],
   //     .offset = impl->cached_vbs.pOffsets[0],
   //     .IndexCount = NumVertices,
  //      .StartIndexLocation = startIndex,
 //  .BaseVertexLocation = static_cast<UINT>(BaseVertexIndex),
 //       .index_format = impl->cached_ib_format,
  //      .index_offset = impl->cached_ib_offset
 //   };


    // Log function call with arguments
    /*
    LOG_MFUN(_;
        LogIf<1>{impl->linear_restore};
        LOG_ARG(vertices);
        LOG_ARG(NumVertices);
        LOG_ARG(startIndex);
        LOG_ARG(BaseVertexIndex);
        LOG_ARG(impl->linear_conditions)
    );
    */

    // Call inner device's DrawIndexedPrimitive
    impl->inner->DrawIndexedPrimitive(
        PrimitiveType,
        BaseVertexIndex,
        MinVertexIndex,
        NumVertices,
        startIndex,
        primCount
    );

    impl->linear_conditions_end();  // End linear filter conditions
    return S_OK;
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
    // Log function call with arguments
    // LOG_MFUN(_;;
    //    LOG_ARG(FVF)
    // );

    // Cache the FVF
    impl->cached_fvf = FVF;

    // Call inner device's SetFVF
    return impl->inner->SetFVF(FVF);
}

// Section 6: Stream and Index Buffer Management

HRESULT STDMETHODCALLTYPE  MyID3D9Device::SetStreamSource(
    UINT StreamNumber,
    IDirect3DVertexBuffer9* pStreamData,
    UINT OffsetInBytes,
    UINT Stride
) {
    // Log function call with arguments
   // LOG_MFUN(_,
   // LOG_ARG(StreamNumber);
     //   LOG_ARG_TYPE(pStreamData, MyID3D10Buffer_Logger), // Assuming MyID3D10Buffer_Logger is defined
 //   LOG_ARG(OffsetInBytes);
       // LOG_ARG(Stride)
  //  );

    // Set the stream source in the inner device
    impl->inner->SetStreamSource(
        StreamNumber,
        pStreamData,
        OffsetInBytes,
        Stride
    );
    return S_OK;
}

HRESULT STDMETHODCALLTYPE  MyID3D9Device::SetIndices(
    IDirect3DIndexBuffer9* pIndexData
) {
    // Log function call with arguments
  //  LOG_MFUN(_,
     //   LOG_ARG_TYPE(pIndexData, MyID3D10Buffer_Logger) // Assuming MyID3D10Buffer_Logger is defined
  //  );

    // Set the indices in the inner device
    impl->inner->SetIndices(pIndexData); return S_OK;
}


HRESULT STDMETHODCALLTYPE  MyID3D9Device::DrawPrimitive(
    D3DPRIMITIVETYPE PrimitiveType,
    UINT StartVertex,
    UINT PrimitiveCount
) {
    // Set render viewport and begin linear conditions
    impl->set_render_vp();
    impl->linear_conditions_begin();

    // Log function call with arguments
   // LOG_MFUN(_,
  //  LOG_ARG(PrimitiveType);
  //  LOG_ARG(StartVertex);
  //  LOG_ARG(PrimitiveCount);
    //   LOG_ARG(impl->linear_conditions)
   // );

    // Call inner device's DrawPrimitive
    impl->inner->DrawPrimitive(
        PrimitiveType,
        StartVertex,
        PrimitiveCount
    );

    // End linear conditions
    impl->linear_conditions_end();
    return S_OK;
}

 
// Section 7: Render State Management

  


 
// Section 8: Texture and Sampler Management

// STDMETHODCALLTYPE  MyID3D9Device::SetTexture(
  //  DWORD Stage,
    //IDirect3DBaseTexture9* pTexture
//) {
   // LOG_MFUN(_,
   // LOG_ARG(Stage);
       // LOG_ARG(pTexture,
  //  );

    // Set the texture directly on the inner device
  //  impl->inner->SetTexture(Stage, pTexture);
//}


HRESULT STDMETHODCALLTYPE  MyID3D9Device::SetPixelShaderConstantF(
    UINT StartRegister,
    const float* pConstantData,
    UINT Vector4fCount
) {
    //LOG_MFUN(_,
  //  LOG_ARG(StartRegister);
  //  LOG_ARG(Vector4fCount);
 //   LOG_ARG_TYPE(pConstantData, ArrayLoggerRef, Vector4fCount);
  //  );

    // Set the pixel shader constants directly on the inner device
    impl->inner->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
    return S_OK;
}


// Section 9: Render Target Management

HRESULT STDMETHODCALLTYPE  MyID3D9Device::SetRenderTarget(
    DWORD RenderTargetIndex,
    IDirect3DSurface9* pRenderTarget
) {
    // LOG_MFUN(_,
  //   LOG_ARG(RenderTargetIndex);
  //   LOG_ARG(pRenderTarget);
     //);

     // Resetting viewport and render dimensions
    impl->reset_render_vp();
    impl->render_width = 0;
    impl->render_height = 0;
    impl->render_orig_width = 0;
    impl->render_orig_height = 0;
    impl->need_render_vp = false;

    // Set the cached render target
    impl->cached_rtv = pRenderTarget;

    // Set the render target directly on the inner device
    impl->inner->SetRenderTarget(RenderTargetIndex, pRenderTarget);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE  MyID3D9Device::SetRenderState(
    D3DRENDERSTATETYPE State,
    DWORD Value
) {
  //  LOG_MFUN(_,
 //   LOG_ARG(State);
  //  LOG_ARG(Value);
  //  );

    // Set the render state directly on the inner device
    impl->inner->SetRenderState(State, Value);

    // Cache the render state if enabled for slang shader
    if constexpr (ENABLE_SLANG_SHADER) {
        impl->cached_rs[State] = Value;
        // Additional blending state caching (commented out)
        // impl->cached_bs.pBlendState = pBlendState;
        // impl->cached_bs.BlendFactor[0] = BlendFactor[0];
        // impl->cached_bs.BlendFactor[1] = BlendFactor[1];
        // impl->cached_bs.BlendFactor[2] = BlendFactor[2];
        // impl->cached_bs.BlendFactor[3] = BlendFactor[3];
        // impl->cached_bs.SampleMask = SampleMask;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::SetDepthStencilState(
    IDirect3DStateBlock9* pDepthStencilState
) {
    // LOG_MFUN(
    //    LOG_ARG(pDepthStencilState)
    // );

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

//HRESULT STDMETHODCALLTYPE MyID3D9Device::SetStreamSource(
  //  UINT StreamNumber,
 //   IDirect3DVertexBuffer9* pStreamData,
//    UINT OffsetInBytes,
  //  UINT Stride
//) {
    //LOG_MFUN(_,
 //   LOG_ARG(StreamNumber);
 //   LOG_ARG(pStreamData);
 //   LOG_ARG(OffsetInBytes);
  //  LOG_ARG(Stride);
  //  );

    // Set the stream source directly on the inner device
  //  impl->inner->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
//}


// Section 10: Remaining Methods


HRESULT STDMETHODCALLTYPE MyID3D9Device::DrawPrimitiveAuto() {
    // LOG_MFUN();

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
    // LOG_MFUN();

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
   // LOG_MFUN(
      //  LOG_ARG(NumViewports),
      //  LOG_ARG_TYPE(pViewports, ArrayLoggerRef, NumViewports)
    //);
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
 //   LOG_MFUN();
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
        // LOG_MFUN(
        //     LOG_ARG_TYPE(pDstResource, MyID3D10Resource_Logger),
        //     LOG_ARG(DstSubresource),
        //     LOG_ARG(pDstBox)
        // );

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
    D3DRESOURCETYPE dstType = pDstResource->GetType();
    D3DRESOURCETYPE srcType = pSrcResource->GetType();

    if (dstType != srcType) return;

    switch (dstType) {
    case D3DRTYPE_SURFACE: {
        IDirect3DSurface9* dstSurface = static_cast<IDirect3DSurface9*>(pDstResource);
        IDirect3DSurface9* srcSurface = static_cast<IDirect3DSurface9*>(pSrcResource);
        impl->StretchRect(srcSurface, nullptr, dstSurface, nullptr, D3DTEXF_NONE);
        break;
    }
    case D3DRTYPE_TEXTURE_ALIAS: {
        IDirect3DTexture9* dstTexture = static_cast<IDirect3DTexture9*>(pDstResource);
        IDirect3DTexture9* srcTexture = static_cast<IDirect3DTexture9*>(pSrcResource);
        // Implement texture copy if needed
        // Lock, memcpy, Unlock approach might be needed here
        D3DLOCKED_RECT srcLockedRect, dstLockedRect;
        srcTexture->LockRect(0, &srcLockedRect, nullptr, 0);
        dstTexture->LockRect(0, &dstLockedRect, nullptr, 0);
        memcpy(dstLockedRect.pBits, srcLockedRect.pBits, dstLockedRect.Pitch * dstTexture->GetLevelCount());
        srcTexture->UnlockRect(0);
        dstTexture->UnlockRect(0);
        break;
    }
    default:
        // Handle other resource types if needed
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
   // LOG_MFUN(
  //  LOG_ARG(pRenderTarget);
  // /   //  LOG_ARG(Color)
   // );
    impl->Clear(0, nullptr, D3DCLEAR_TARGET, Color, 1.0f, 0);
}

void STDMETHODCALLTYPE MyIDirect3DDevice9::ClearDepthStencilView(
    IDirect3DSurface9* pDepthStencil,
    DWORD ClearFlags,
    float Depth,
    DWORD Stencil
) {
   // LOG_MFUN(
   // LOG_ARG(pDepthStencil);
    //LOG_ARG(ClearFlags);
  //  LOG_ARG(Depth);
      //  LOG_ARG(Stencil)
   // );
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
   // LOG_MFUN(
       // LOG_ARG(pTexture)
  //  );
    if (pTexture) {
        pTexture->GenerateMipSubLevels();
    }
}

void STDMETHODCALLTYPE MyID3D9Device::PSGetShaderResources(
    UINT StartSlot,
    UINT NumViews,
    IDirect3DBaseTexture9** ppShaderResourceViews
) {
   // LOG_MFUN(
       // LOG_ARG(StartSlot),
      //  LOG_ARG(NumViews)
   // );
    for (UINT i = 0; i < NumViews; ++i) {
        impl->inner->GetTexture(StartSlot + i, &ppShaderResourceViews[i]);
    }
}

void STDMETHODCALLTYPE MyID3D9Device::PSGetShader(
    IDirect3DPixelShader9** ppPixelShader
) {
   // LOG_MFUN(
   //     LOG_ARG(ppPixelShader)
  //  );
    *ppPixelShader = impl->cached_ps;
    if (*ppPixelShader) {
        (*ppPixelShader)->AddRef();
    }
}


void STDMETHODCALLTYPE MyID3D9Device::VSGetShader(
    IDirect3DVertexShader9** ppVertexShader
) {
   // LOG_MFUN(
       // LOG_ARG(ppVertexShader)
   // );
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
  //  LOG_MFUN(
    //    LOG_ARG(StartSlot),
     //   LOG_ARG(NumSamplers)
  //  );
    for (UINT i = 0; i < NumSamplers; ++i) {
        impl->inner->GetSamplerState(StartSlot + i, D3DSAMP_MIPFILTER, &ppSamplers[i]);
    }
}


void STDMETHODCALLTYPE MyID3D9Device::IAGetInputLayout(
    IDirect3DVertexDeclaration9** ppInputLayout
) {
   // LOG_MFUN(
      //  LOG_ARG(ppInputLayout)
   // );
    impl->inner->GetVertexDeclaration(ppInputLayout);
}


void STDMETHODCALLTYPE MyID3D9Device::IAGetVertexBuffers(
    UINT StartSlot,
    UINT NumBuffers,
    IDirect3DVertexBuffer9** ppVertexBuffers,
    UINT* pStrides,
    UINT* pOffsets
) {
   // LOG_MFUN(
    //    LOG_ARG(StartSlot),
    //    LOG_ARG(NumBuffers)
   // );
    for (UINT i = 0; i < NumBuffers; ++i) {
        impl->inner->GetStreamSource(StartSlot + i, &ppVertexBuffers[i], &pOffsets[i], &pStrides[i]);
    }
}


void STDMETHODCALLTYPE MyID3D9Device::IAGetIndexBuffer(
    IDirect3DIndexBuffer9** pIndexBuffer,
    D3DFORMAT* Format,
    UINT* Offset
) {
   // LOG_MFUN(
     //   LOG_ARG(pIndexBuffer),
     //   LOG_ARG(Format),
     //   LOG_ARG(Offset)
   // );

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
  //  LOG_MFUN(
      //  LOG_ARG(NumViews),
     //   LOG_ARG(ppRenderTargetViews),
     //   LOG_ARG(ppDepthStencilView)
  //  );
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
    //LOG_MFUN(
     //   LOG_ARG(NumViewports),
     //   LOG_ARG(pViewports)
   // );
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
  //  LOG_MFUN(
   //     LOG_ARG(NumRects),
   //     LOG_ARG(pRects)
  //  );
    if (NumRects && *NumRects > 0) {
        impl->inner->GetScissorRect(pRects);
        *NumRects = 1;
    }
}



// Section 16: Flush


void STDMETHODCALLTYPE MyID3D9Device::Flush() {
  //  LOG_MFUN();
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
        // You can add additional code here if needed, e.g., wrapping the buffer
        // new MyID3D9VertexBuffer(ppVertexBuffer, ...);
      //  LOG_MFUN(
         //   LOG_ARG(Length),
          //  LOG_ARG(Usage),
          //  LOG_ARG(FVF),
         //   LOG_ARG(Pool),
         //   LOG_ARG(*ppVertexBuffer),
         //   ret
       // );
    }
    else {
      //  LOG_MFUN(
          //  LOG_ARG(Length),
          //  LOG_ARG(Usage),
         //   LOG_ARG(FVF),
          //  LOG_ARG(Pool),
         //   ret
       // );
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
        // You can add additional code here if needed, e.g., wrapping the texture
        // new MyID3D9Texture1D(ppTexture, ...);
      //  LOG_MFUN(
      //      LOG_ARG(Width),
       //     LOG_ARG(Levels),
      //      LOG_ARG(Usage),
       //     LOG_ARG(Format),
      //      LOG_ARG(Pool),
      //      LOG_ARG(*ppTexture),
      //      ret
      //  );
    }
    else {
      //  LOG_MFUN(
      //      LOG_ARG(Width),
       //     LOG_ARG(Levels),
       //     LOG_ARG(Usage),
       //     LOG_ARG(Format),
       //     LOG_ARG(Pool),
        //    ret
      //  );
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
        // You can add additional code here if needed, e.g., wrapping the texture
        // new MyID3D9Texture2D(ppTexture, ...);
      //  LOG_MFUN(
       //     LOG_ARG(Width),
       //     LOG_ARG(Height),
       //     LOG_ARG(Levels),
       //     LOG_ARG(Usage),
       //     LOG_ARG(Format),
       //     LOG_ARG(Pool),
       //   LOG_ARG(*ppTexture),
         // ret
      //);
    }
    else {
      //LOG_MFUN(
      //    LOG_ARG(Width),
         //LOG_ARG(Height),
        //  LOG_ARG(Levels),
        //  LOG_ARG(Usage),
        //  LOG_ARG(Format),
       //   LOG_ARG(Pool),
      //    ret
      //);
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
        // LOG_MFUN(
        //     LOG_ARG(pTexture),
        //     LOG_ARG(*ppSRView),
        //     ret
        // );
    }
    else {
        ret = E_INVALIDARG;
        // LOG_MFUN(
        //     LOG_ARG(pTexture),
        //     ret
        // );
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
     //   LOG_MFUN(
     //       LOG_ARG(pSurface),
      //      LOG_ARG(pDesc),
      //      LOG_ARG(pRTView),
     //       ret
      //  );
    }
    else {
       // LOG_MFUN(
     //       LOG_ARG(pSurface),
      //      LOG_ARG(pDesc),
     //       ret
     //   );
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
    //    LOG_MFUN(
      //      LOG_ARG(pSurface),
     //       LOG_ARG(pDesc),
     //       LOG_ARG(pDepthStencilView),
    //        ret
    //    );
    }
    else {
     //   LOG_MFUN(
      //      LOG_ARG(pSurface),
     //       LOG_ARG(pDesc),
     //       ret
     //   );
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
     //   LOG_MFUN(
      //      LOG_ARG_TYPE(pInputElementDescs, ArrayLoggerRef, NumElements),
       //     LOG_ARG_TYPE(ppVertexDeclaration, ArrayLoggerDeref, NumElements),
      //      ret
      //  );
    }
    else {
      //  LOG_MFUN(
       //     LOG_ARG_TYPE(pInputElementDescs, ArrayLoggerRef, NumElements),
       //     ret
      //  );
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
        // LOG_MFUN(_, LOG_ARG(shader_source), LOG_ARG_TYPE(hash, NumHexLogger), LOG_ARG(BytecodeLength), LOG_ARG(*ppVertexShader), ret);
    }
    else {
        // LOG_MFUN(_, LOG_ARG(BytecodeLength), ret);
    }
    return ret;
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateGeometryShader(
    const void* pShaderBytecode,
    SIZE_T BytecodeLength,
    void** ppGeometryShader
) {
    // LOG_MFUN(_, LOG_ARG(BytecodeLength), E_NOTIMPL);
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateGeometryShaderWithStreamOutput(
    const void* pShaderBytecode,
    SIZE_T BytecodeLength,
    const void* pSODeclaration,
    UINT NumEntries,
    UINT OutputStreamStride,
    void** ppGeometryShader
) {
    // LOG_MFUN(_, LOG_ARG(BytecodeLength), E_NOTIMPL);
    return E_NOTIMPL;
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::CreatePixelShader(
    const void* pShaderBytecode,
    SIZE_T BytecodeLength,
    IDirect3DPixelShader9** ppPixelShader
) {
    HRESULT ret = impl->inner->CreatePixelShader((const DWORD*)pShaderBytecode, ppPixelShader);
    if (ret == S_OK) {
        ShaderLogger shader_source{ pShaderBytecode };
        DWORD hash;
        MurmurHash3_x86_32(pShaderBytecode, BytecodeLength, 0, &hash);
        new MyID3D9PixelShader(*ppPixelShader, hash, BytecodeLength, shader_source.source);
       // LOG_MFUN(_, LOG_ARG(shader_source), LOG_ARG_TYPE(hash, NumHexLogger), LOG_ARG(BytecodeLength), LOG_ARG(*ppPixelShader), ret);
    }
    else {
      //  LOG_MFUN(_, LOG_ARG(BytecodeLength), ret);
    }
    return ret;
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
        // LOG_MFUN(_, LOG_ARG(pBlendStateDesc), LOG_ARG(*ppBlendState));
    }
    else {
        // LOG_MFUN(_, LOG_ARG(pBlendStateDesc), ret);
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
      //  LOG_MFUN(
       //     LOG_ARG(pDepthStencilDesc),
      //      LOG_ARG(*ppDepthStencilState)
     //   );
    }
    else {
     //   LOG_MFUN(
     //       LOG_ARG(pDepthStencilDesc),
     //       ret
     //   );
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
        // LOG_MFUN(
        //     LOG_ARG(pRasterizerDesc),
        //     LOG_ARG(*ppRasterizerState)
        // );
    }
    else {
        // LOG_MFUN(
        //     LOG_ARG(pRasterizerDesc),
        //     ret
        // );
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
        // LOG_MFUN(LOG_ARG(pSamplerDesc), LOG_ARG(*ppSamplerState), ret);
    }
    else {
        // LOG_MFUN(LOG_ARG(pSamplerDesc), ret);
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


HRESULT STDMETHODCALLTYPE MyID3D9Device::CreateQuery(
    const CustomQueryDesc* pQueryDesc,
    IDirect3DQuery9** ppQuery
) {
    HRESULT ret = impl->inner->CreateQuery(
        ConvertQueryType(pQueryDesc->Query),
        ppQuery
    );

  //  LOG_MFUN(
  //      LOG_ARG(pQueryDesc),
   //     LOG_ARG(*ppQuery),
  //      ret
  //  );

    return ret;
}

HRESULT STDMETHODCALLTYPE MyID3D9Device::CreatePredicate(
    const CustomQueryDesc* pPredicateDesc,
    IDirect3DQuery9** ppPredicate
) {
    return CreateQuery(pPredicateDesc, ppPredicate);
}


HRESULT STDMETHODCALLTYPE MyID3D9Device::CheckMultisampleQualityLevels(
    D3DFORMAT Format,
    DWORD SampleCount,
    DWORD* pNumQualityLevels
) {
 //   LOG_MFUN();
    // DirectX 9 can check multisample support through IDirect3D9
    return m_pD3D->CheckDeviceMultiSampleType(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        Format,
        TRUE, // Windowed
        (D3DMULTISAMPLE_TYPE)SampleCount,
        pNumQualityLevels
    );
}

// Section 20: Missing Virtual Functions

HRESULT MyID3D9Device::TestCooperativeLevel() {
    return impl->inner->TestCooperativeLevel();
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
    return impl->inner->GetDeviceCaps(pCaps);
}

HRESULT MyID3D9Device::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) {
    return impl->inner->GetDisplayMode(iSwapChain, pMode);
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

HRESULT MyID3D9Device::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain) {
    return impl->inner->CreateAdditionalSwapChain(pPresentationParameters, pSwapChain);
}

HRESULT MyID3D9Device::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) {
    return impl->inner->GetSwapChain(iSwapChain, pSwapChain);
}

UINT MyID3D9Device::GetNumberOfSwapChains() {
    return impl->inner->GetNumberOfSwapChains();
}

HRESULT MyID3D9Device::Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    return impl->inner->Reset(pPresentationParameters);
}

HRESULT MyID3D9Device::Present(const RECT* src_rect, const RECT* dst_rect, HWND dst_window_override, const RGNDATA* dirty_region) {
    return impl->inner->Present(src_rect, dst_rect, dst_window_override, dirty_region);
}

HRESULT MyID3D9Device::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer) {
    return impl->inner->GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer);
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

HRESULT MyID3D9Device::CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) {
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

HRESULT MyID3D9Device::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) {
    return impl->inner->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
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

HRESULT MyID3D9Device::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) {
    return impl->inner->SetDepthStencilSurface(pNewZStencil);
}

HRESULT MyID3D9Device::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) {
    return impl->inner->GetDepthStencilSurface(ppZStencilSurface);
}

HRESULT MyID3D9Device::BeginScene() {
    return impl->inner->BeginScene();
}

HRESULT MyID3D9Device::EndScene() {
    return impl->inner->EndScene();
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

HRESULT MyID3D9Device::SetViewport(const D3DVIEWPORT9* viewport) {
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

HRESULT MyID3D9Device::CreatePixelShader(const DWORD* byte_code, IDirect3DPixelShader9** shader) {
    return impl->inner->CreatePixelShader(byte_code, shader);
}

HRESULT MyID3D9Device::GetPixelShader(IDirect3DPixelShader9** ppShader) {
    return impl->inner->GetPixelShader(ppShader);
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

HRESULT MyID3D9Device::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) {
    return impl->inner->CreateQuery(Type, ppQuery);
}

