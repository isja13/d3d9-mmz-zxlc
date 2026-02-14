#include "cg_runtime.h"
#include "cg_load.h"

#include <windows.h>
#include <mutex>
#include <cstdio>

#include "../Cg/cg.h"
#include "../Cg/cgd3d9.h"

static std::once_flag g_rt_once;
static bool   g_rt_inited = false;
static CGprofile g_vp = CG_PROFILE_UNKNOWN;
static CGprofile g_fp = CG_PROFILE_UNKNOWN;

static char g_rt_err[1024] = { 0 };

static void set_err(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vsnprintf(g_rt_err, sizeof(g_rt_err), fmt, va);
    va_end(va);
}

static void clear_err()
{
    g_rt_err[0] = 0;
}

static void capture_cg_error(const char* tag)
{
    CGerror e = cgGetError();
    if (e == CG_NO_ERROR) return;

    const char* es = cgGetErrorString(e);
    const char* listing = cgGetLastListing(nullptr); // may be null

    if (listing && listing[0]) {
        set_err("%s: CgError=%d (%s)\nCgListing:\n%s",
            tag, (int)e, es ? es : "?", listing);
    }
    else {
        set_err("%s: CgError=%d (%s)", tag, (int)e, es ? es : "?");
    }
}

bool cg_runtime_init_once()
{
    std::call_once(g_rt_once, [&] {
        clear_err();

        if (!cg_load_is_ready()) {
            set_err("cg_runtime_init_once: cg_load not ready (%s)", cg_load_last_error());
            g_rt_inited = false;
            return;
        }

        g_vp = cgD3D9GetLatestVertexProfile();
        g_fp = cgD3D9GetLatestPixelProfile();

        if (g_vp == CG_PROFILE_UNKNOWN || g_fp == CG_PROFILE_UNKNOWN) {
            capture_cg_error("cgD3D9GetLatest*Profile");
            if (!g_rt_err[0])
                set_err("cgD3D9GetLatest*Profile returned UNKNOWN (vp=%d fp=%d)", (int)g_vp, (int)g_fp);
            g_rt_inited = false;
            return;
        }

        // Optional: request latest profile options. Safe to omit.
     //   cgD3D9SetOptimalOptions(g_vp);
       // cgD3D9SetOptimalOptions(g_fp);
        capture_cg_error("cgD3D9SetOptimalOptions");
        if (g_rt_err[0]) {
            g_rt_inited = false;
            return;
        }

        g_rt_inited = true;
        set_err("OK");
        });

    return g_rt_inited;
}
const char* cg_runtime_last_error()
{
    return g_rt_err[0] ? g_rt_err : "no error";
}

void cg_runtime_on_reset_pre()
{
    // Later: release RT chain / programs if you choose.
}

void cg_runtime_on_reset_post(IDirect3DDevice9* dev)
{
    (void)dev;
    // Later: re-load programs or rebuild chain if needed.
}

static void cg_release_rt(ZeroMod::cg_pass_rt& p)
{
    if (p.rt_surf) { p.rt_surf->Release(); p.rt_surf = nullptr; }
    if (p.rt_tex) { p.rt_tex->Release();  p.rt_tex = nullptr; }
    p.rt_w = p.rt_h = 0;
}

static bool cg_ensure_rt(IDirect3DDevice9* dev, ZeroMod::cg_pass_rt& p, UINT w, UINT h)
{
    if (!dev || !w || !h) return false;
    if (p.rt_tex && p.rt_surf && p.rt_w == w && p.rt_h == h) return true;

    cg_release_rt(p);

    IDirect3DTexture9* t = nullptr;
    HRESULT hr = dev->CreateTexture(
        w, h, 1,
        D3DUSAGE_RENDERTARGET,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        &t, nullptr);
    if (FAILED(hr) || !t) return false;

    IDirect3DSurface9* s = nullptr;
    hr = t->GetSurfaceLevel(0, &s);
    if (FAILED(hr) || !s) { t->Release(); return false; }

    p.rt_tex = t;
    p.rt_surf = s;
    p.rt_w = w;
    p.rt_h = h;
    return true;
}

namespace ZeroMod {

    static void cg_bind_and_draw_pass(
        IDirect3DDevice9* dev,
        cg_pass_rt& pass,
        IDirect3DTexture9* in_tex,
        UINT in_video_w, UINT in_video_h,
        UINT in_tex_w, UINT in_tex_h,
        UINT out_w, UINT out_h,
        UINT64 frame_count)
    {
        D3DSURFACE_DESC td{};
        if (in_tex && SUCCEEDED(in_tex->GetLevelDesc(0, &td))) {
            in_tex_w = td.Width;
            in_tex_h = td.Height;
        }
        // Bind programs
        cgD3D9BindProgram(pass.vprog);
        CGerror e = cgGetError();
        if (e != CG_NO_ERROR) {
            char b[256];
            _snprintf(b, sizeof(b), "[ZeroMod][CG] bind VS failed: %d (%s)\n", (int)e, cgGetErrorString(e));
            OutputDebugStringA(b);
            return; // or set a flag and bail
        }

        cgD3D9BindProgram(pass.pprog);
        e = cgGetError();
        if (e != CG_NO_ERROR) {
            char b[256];
            _snprintf(b, sizeof(b), "[ZeroMod][CG] bind PS failed: %d (%s)\n", (int)e, cgGetErrorString(e));
            OutputDebugStringA(b);
            return;
        }

        // Params (set only if present)
        static const float kIdentity[16] = {
            1,0,0,0,
            0,1,0,0,
            0,0,1,0,
            0,0,0,1
        };

        if (pass.p_mvp)        cgSetMatrixParameterfc(pass.p_mvp, kIdentity);

        float in_video[2] = { (float)in_video_w, (float)in_video_h };
        float in_texsz[2] = { (float)in_tex_w, (float)in_tex_h };
        float out_sz[2] = { (float)out_w, (float)out_h };

        if (pass.p_in_video)   cgSetParameter2fv(pass.p_in_video, in_video);
        if (pass.p_in_tex)     cgSetParameter2fv(pass.p_in_tex, in_texsz);
        if (pass.p_out)        cgSetParameter2fv(pass.p_out, out_sz);
        if (pass.p_framecount) cgSetParameter1f(pass.p_framecount, (float)frame_count);

        // Sampler
        if (pass.p_decal) {
            cgD3D9SetTextureParameter(pass.p_decal, in_tex);
            cgSetSamplerState(pass.p_decal);
        }
        // upload #pragma parameters
        for (auto& pr : pass.params)
        {
            if (pr.h)
                cgSetParameter1f(pr.h, pr.value);
        }
        cgUpdateProgramParameters(pass.vprog);
        cgUpdateProgramParameters(pass.pprog);

        if (!pass.p_decal) {
            OutputDebugStringA("[ZeroMod][CG] WARN: pass has no p_decal sampler (likely black output)\n");
        }

        dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
    }

    static void cg_update_quad_pos_xy(IDirect3DVertexBuffer9* vb, float sx, float sy)
    {
        if (!vb) return;

        zm_cg_vertex* v = nullptr;
        HRESULT hr = vb->Lock(0, 0, (void**)&v, 0);
        if (FAILED(hr) || !v) return;

        // actual VB order is:
        // 0: TL, 1: BL, 2: TR, 3: BR  (triangle strip)
        v[0].x = -sx; v[0].y = +sy; // TL
        v[1].x = -sx; v[1].y = -sy; // BL
        v[2].x = +sx; v[2].y = +sy; // TR
        v[3].x = +sx; v[3].y = -sy; // BR

        vb->Unlock();
    }
    static void cg_add_quad_uv_offset(IDirect3DVertexBuffer9* vb, float du, float dv)
    {
        if (!vb) return;

        zm_cg_vertex* v = nullptr;
        HRESULT hr = vb->Lock(0, 0, (void**)&v, 0);
        if (FAILED(hr) || !v) return;

        for (int i = 0; i < 4; ++i) {
            v[i].u += du;
            v[i].v += dv;
        }

        vb->Unlock();
    }

    bool cg_d3d9_frame(
        d3d9_video_struct* d3d9,
        IDirect3DTexture9* src_tex,
        IDirect3DSurface9* dst_rtv,
        UINT vp_x, UINT vp_y,
        UINT vp_w, UINT vp_h,
        UINT64 frame_count,
        bool scissor_on,
        RECT scissor,
        bool vp_is_3_2
    )
    {
        static uint64_t s_once = 0;
        if (s_once++ == 0)
            OutputDebugStringA("[ZeroMod][CG] cg_d3d9_frame entered (chain)\n");

        if (!d3d9 || d3d9->magic != 0x39564433) return false;
        if (!d3d9->dev || !src_tex || !dst_rtv) return false;
        if (!cg_load_is_ready()) return false;

        // Must have a loaded chain
        if (!d3d9->cg.active || d3d9->cg.num_passes <= 0 || !d3d9->cg.passes)
            return false;

        IDirect3DDevice9* dev = d3d9->dev;

        HRESULT hr_tc = dev->TestCooperativeLevel();
        if (hr_tc != D3D_OK) return false;

        if (!vp_w || !vp_h) {
            D3DSURFACE_DESC d{};
            if (FAILED(dst_rtv->GetDesc(&d)) || !d.Width || !d.Height) return false;
            vp_x = 0; vp_y = 0; vp_w = (UINT)d.Width; vp_h = (UINT)d.Height;
        }
        UINT ox = vp_x, oy = vp_y, ow = vp_w, oh = vp_h;

        if (scissor_on) {
            LONG sw = scissor.right - scissor.left;
            LONG sh = scissor.bottom - scissor.top;

            if (sw > 0 && sh > 0) {
                // If scissor is a proper subset (common letterbox), prefer it.
                if ((UINT)sw <= ow && (UINT)sh <= oh) {
                    ox = (UINT)scissor.left;
                    oy = (UINT)scissor.top;
                    ow = (UINT)sw;
                    oh = (UINT)sh;
                }
            }
        }
        // viewport + scissor killer (keep yours)
        {
            D3DVIEWPORT9 d3dvp{};
            d3dvp.X = (DWORD)ox;
            d3dvp.Y = (DWORD)oy;
            d3dvp.Width = (DWORD)ow;
            d3dvp.Height = (DWORD)oh;
            d3dvp.MinZ = 0.0f;
            d3dvp.MaxZ = 1.0f;
            dev->SetViewport(&d3dvp);

            // If you have a rect, use scissor to hard-clip writes to the game image region.
            dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
            RECT sr{ (LONG)ox, (LONG)oy, (LONG)(ox + ow), (LONG)(oy + oh) };
            dev->SetScissorRect(&sr);
        }

        dev->SetDepthStencilSurface(nullptr);

        // baseline states (keep)
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

        if (!d3d9->frame_vbo || !d3d9->vertex_decl) return false;
        dev->SetVertexDeclaration(d3d9->vertex_decl);
        dev->SetStreamSource(0, d3d9->frame_vbo, 0, sizeof(zm_cg_vertex));

        // Source dims
        D3DSURFACE_DESC src_desc{};
        src_tex->GetLevelDesc(0, &src_desc);
        const UINT orig_w = src_desc.Width;
        const UINT orig_h = src_desc.Height;

        IDirect3DTexture9* in_tex = src_tex;
        UINT in_w = orig_w, in_h = orig_h;

        const int N = d3d9->cg.num_passes;
        // default = no overscan
        float pass0_sx = 1.0f;
        float pass0_sy = 1.0f;
        bool do_pass0_overscan = false;

        if (vp_is_3_2 && orig_w == 256 && orig_h == 192) {
            do_pass0_overscan = true;
            pass0_sx = 1.070f;  // match your logged VPOS
            pass0_sy = 1.200f;  // already matches
        }

        for (int i = 0; i < N; i++)
        {
            cg_pass_rt& p = d3d9->cg.passes[i];

            // Determine desired out size from preset
            UINT base_w = (p.scale_type == cg_pass_rt::SCALE_VIEWPORT) ? ow : orig_w;
            UINT base_h = (p.scale_type == cg_pass_rt::SCALE_VIEWPORT) ? oh : orig_h;
            float sc = (p.scale > 0.0f) ? p.scale : 1.0f;
            UINT want_w = (UINT)(base_w * sc + 0.5f);
            UINT want_h = (UINT)(base_h * sc + 0.5f);
            if (!want_w) want_w = 1;
            if (!want_h) want_h = 1;

            UINT out_w = want_w;
            UINT out_h = want_h;

            IDirect3DSurface9* out_surf = nullptr;

            if (i == N - 1)
            {
                out_surf = dst_rtv;
                out_w = ow;
                out_h = oh;
                char b[256];
                _snprintf(b, sizeof(b),
                    "[ZeroMod][CG] LAST PASS: src=%ux%u in=%ux%u out=%ux%u rect=%u,%u %ux%u sc_on=%u\n",
                    (unsigned)orig_w, (unsigned)orig_h,
                    (unsigned)in_w, (unsigned)in_h,
                    (unsigned)out_w, (unsigned)out_h,
                    (unsigned)ox, (unsigned)oy, (unsigned)ow, (unsigned)oh,
                    (unsigned)(scissor_on ? 1 : 0));
                OutputDebugStringA(b);
            }
            else
            {
                if (!cg_ensure_rt(dev, p, out_w, out_h))
                    return false;
                out_surf = p.rt_surf;
            }

            dev->SetRenderTarget(0, out_surf);

            // Viewport MUST match the render target region you intend to fill.
            D3DVIEWPORT9 pass_vp{};
            if (i == N - 1) {
                pass_vp.X = (DWORD)ox;
                pass_vp.Y = (DWORD)oy;
                pass_vp.Width = (DWORD)ow;
                pass_vp.Height = (DWORD)oh;
                out_w = ow;
                out_h = oh;
            }
            else {
                pass_vp.X = 0;
                pass_vp.Y = 0;
                pass_vp.Width = (DWORD)out_w;
                pass_vp.Height = (DWORD)out_h;
            }
            pass_vp.MinZ = 0.0f;
            pass_vp.MaxZ = 1.0f;
            dev->SetViewport(&pass_vp);
            if (i == N - 1) {
                // last pass: respect your intended output region
                dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
                RECT sr{ (LONG)ox, (LONG)oy, (LONG)(ox + ow), (LONG)(oy + oh) };
                dev->SetScissorRect(&sr);
            }
            else {
                // intermediate passes: full RT
                dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
                // (or enable + set to 0..out_w/out_h if you prefer)
            }
            // Input sampler state
            dev->SetTexture(0, in_tex);
            dev->SetSamplerState(0, D3DSAMP_MINFILTER, p.filter_linear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
            dev->SetSamplerState(0, D3DSAMP_MAGFILTER, p.filter_linear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
            dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

            // ---- PASS0 OVERSCAN + HALF-TEXEL UV FIX (Zero only) ----
            if (i == 0 && do_pass0_overscan)
            {
                cg_update_quad_pos_xy(d3d9->frame_vbo, pass0_sx, pass0_sy);

                const float y_off_px = +0.10f;              // your empirical value (source pixels)
                const float dv = y_off_px / (float)in_h;    // in_h == 192

                cg_add_quad_uv_offset(d3d9->frame_vbo, 0.0f, dv);
            }

            cg_bind_and_draw_pass(dev, p, in_tex,
                in_w, in_h,   // IN.video_size
                in_w, in_h,   // IN.texture_size
                out_w, out_h, // IN.output_size (matches bound RT)
                frame_count);

            // restore after pass0 so nothing else is affected
            if (i == 0 && do_pass0_overscan)
            {
                const float y_off_px = +0.10f;
                const float dv = y_off_px / (float)in_h;

                cg_add_quad_uv_offset(d3d9->frame_vbo, 0.0f, -dv); // undo
                cg_update_quad_pos_xy(d3d9->frame_vbo, 1.0f, 1.0f);
            }

            if (i != N - 1) {
                in_tex = p.rt_tex;
                in_w = out_w;
                in_h = out_h;
            }
        }

        // restore final viewport to destination viewport for safety
        {
            D3DVIEWPORT9 d3dvp{};
            d3dvp.X = (DWORD)vp_x;
            d3dvp.Y = (DWORD)vp_y;
            d3dvp.Width = (DWORD)vp_w;
            d3dvp.Height = (DWORD)vp_h;
            d3dvp.MinZ = 0.0f;
            d3dvp.MaxZ = 1.0f;
            dev->SetViewport(&d3dvp);
        }

        // Cg error check
        CGerror e = cgGetError();
        if (e != CG_NO_ERROR) {
            const char* es = cgGetErrorString(e);
            char b[256];
            _snprintf(b, sizeof(b), "[ZeroMod][CG] frame: CgError=%d (%s)\n", (int)e, es ? es : "?");
            OutputDebugStringA(b);
            return false;
        }

        return true;
    }

} // namespace ZeroMod
