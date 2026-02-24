#include "slang_d3d9.h"
#include "d3d9video.h"
#include "log.h"

#include "../retroarch/retroarch/gfx/video_shader_parse.h"
#include "../retroarch/retroarch/gfx/drivers_shader/slang_process.h"
#include "../retroarch/retroarch/gfx/drivers_shader/slang_reflection.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <vector>
#include <stdint.h>

#define ZM_DUMP_PASS0_HLSL 0

#ifndef ZEROMOD_FLOAT4_T_DEFINED
#define ZEROMOD_FLOAT4_T_DEFINED

// ---- Per-draw verbose logging ----
// Set to 1 to enable detailed per-pass draw diagnostics (MP-PROOF, SLANG-DUMP, etc.)
// Set to 0 for normal operation to keep logs clean.
#define ZM_VERBOSE_DRAW 0

#if ZM_VERBOSE_DRAW
#define zm_draw_dbgf(...)   zm_draw_dbgf(__VA_ARGS__)
#define zm_draw_dbg4(l, v)  zm_dbg_float4((l), (v))
#define zm_draw_vp(l, v)    zm_dbg_vp((l), (v))
#else
#define zm_draw_dbgf(...)   ((void)0)
#define zm_draw_dbg4(l, v)  ((void)0)
#define zm_draw_vp(l, v)    ((void)0)
#endif

IDirect3DSurface9* last_slang_rtv = nullptr; 

typedef struct float4_t
{
    float x, y, z, w;
} float4_t;
#endif

namespace ZeroMod {

    struct d3d9_slang_runtime
    {
        char* built_for_path;
        bool  built;

        unsigned num_passes = 0;
        d3d9_slang_pass passes[GFX_MAX_SHADERS]; // fixed array

        // frame-live semantics data (must be updated per pass)
        IDirect3DTexture9* live_original_tex = nullptr;

        float4_t live_original_size = { 0,0,0,0 }; // (w,h,1/w,1/h)
        float4_t live_source_size = { 0,0,0,0 }; // same as original for pass0

        float4_t live_output_size = { 0,0,0,0 }; // (w,h,1/w,1/h) of current pass viewport
        float4_t live_final_viewport = { 0,0,0,0 }; // (w,h,1/w,1/h) of final viewport

        uint32_t live_frame_count = 0;
        int32_t  live_frame_dir = 1;
    };
    static void slang_pass_clear(d3d9_slang_pass& p)
    {
        if (p.vs_ct) { p.vs_ct->Release(); p.vs_ct = nullptr; }
        if (p.ps_ct) { p.ps_ct->Release(); p.ps_ct = nullptr; }

        if (p.vs) { p.vs->Release(); p.vs = nullptr; }
        if (p.ps) { p.ps->Release(); p.ps = nullptr; }

        if (p.rt_surf) { p.rt_surf->Release(); p.rt_surf = nullptr; }
        if (p.rt) { p.rt->Release();      p.rt = nullptr; }

        if (p.hlsl_vs) { free(p.hlsl_vs); p.hlsl_vs = nullptr; }
        if (p.hlsl_ps) { free(p.hlsl_ps); p.hlsl_ps = nullptr; }

        p.compiled = false;
        p.sem_valid = false;  //compiled pass program owned
        p.source_sampler_reg = -1;
        p.tl_inited = false;
    }
    static bool slang_all_passes_compiled(const d3d9_slang_runtime* rt)
    {
        if (!rt) return false;
        if (!rt->built) return false;
        if (rt->num_passes == 0) return false;

        for (unsigned i = 0; i < rt->num_passes && i < GFX_MAX_SHADERS; ++i)
        {
            const d3d9_slang_pass& p = rt->passes[i];
            if (!p.compiled || !p.vs || !p.ps)
                return false;
        }
        return true;
    }

    static bool slang_bind_and_draw_pass(
        IDirect3DDevice9* dev,
        d3d9_video_struct* d3d9,
        d3d9_slang_runtime* rt,
        d3d9_slang_pass& P,
        const video_shader_pass& cfg,
        IDirect3DTexture9* in_tex,
        const D3DVIEWPORT9& pass_vp);

    static void zm_dbgf(const char* fmt, ...)
    {
        char b[1024];
        va_list va;
        va_start(va, fmt);
        _vsnprintf(b, sizeof(b), fmt, va);
        va_end(va);
        b[sizeof(b) - 1] = '\0';
        OutputDebugStringA(b);
    }
    static bool slang_ensure_rt(
        IDirect3DDevice9* dev,
        d3d9_slang_pass& p,
        UINT w,
        UINT h,
        bool fp_fbo
    )
    {
        if (!dev || !w || !h)
            return false;

        // If existing RT matches size, keep it.
        if (p.rt && p.rt_surf)
        {
            D3DSURFACE_DESC d{};
            if (SUCCEEDED(p.rt->GetLevelDesc(0, &d)) &&
                d.Width == w && d.Height == h)
            {
                return true;
            }
        }

        // Release old
        if (p.rt_surf) { p.rt_surf->Release(); p.rt_surf = nullptr; }
        if (p.rt) { p.rt->Release();      p.rt = nullptr; }

        // Pick format
        // NOTE: D3D9 floating RT support depends on device caps; this mirrors CG-style behavior.
        const D3DFORMAT fmt = fp_fbo ? D3DFMT_A16B16G16R16F : D3DFMT_A8R8G8B8;

        HRESULT hr = dev->CreateTexture(
            w, h,
            1,
            D3DUSAGE_RENDERTARGET,
            fmt,
            D3DPOOL_DEFAULT,
            &p.rt,
            nullptr);

        if (FAILED(hr) || !p.rt)
            return false;

        hr = p.rt->GetSurfaceLevel(0, &p.rt_surf);
        if (FAILED(hr) || !p.rt_surf)
        {
            p.rt->Release();
            p.rt = nullptr;
            return false;
        }

        return true;
    }
    static bool ensure_zero_rt(IDirect3DDevice9* dev, zm_zero_stage_rt& Z, UINT w, UINT h)
    {
        if (!dev || !w || !h) return false;

        if (Z.tex && Z.surf && Z.w == w && Z.h == h)
            return true;

        if (Z.surf) { Z.surf->Release(); Z.surf = nullptr; }
        if (Z.tex) { Z.tex->Release();  Z.tex = nullptr; }
        Z.w = Z.h = 0;

        // Use the same format for normal pass RTs.
        const D3DFORMAT fmt = D3DFMT_A8R8G8B8;

        IDirect3DTexture9* t = nullptr;
        HRESULT hr = dev->CreateTexture(
            w, h, 1, D3DUSAGE_RENDERTARGET, fmt, D3DPOOL_DEFAULT, &t, nullptr);

        if (FAILED(hr) || !t) return false;

        IDirect3DSurface9* s = nullptr;
        hr = t->GetSurfaceLevel(0, &s);
        if (FAILED(hr) || !s) { t->Release(); return false; }

        Z.tex = t;
        Z.surf = s;
        Z.w = w;
        Z.h = h;
        return true;
    }

    static inline void zm_dbg_float4(const char* label, const float4_t& v)
    {
        zm_dbgf("    %-14s = {%.3f, %.3f, %.6f, %.6f}\n", label, v.x, v.y, v.z, v.w);
    }

    static inline void zm_dbg_vp(const char* label, const D3DVIEWPORT9& vp)
    {
        zm_dbgf("    %-14s = x=%u y=%u w=%u h=%u\n",
            label, (unsigned)vp.X, (unsigned)vp.Y, (unsigned)vp.Width, (unsigned)vp.Height);
    }
    static inline bool f4eq(const float4_t& a, const float4_t& b)
    {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    }

    static inline float4_t size4_from_tex(IDirect3DTexture9* t)
    {
        D3DSURFACE_DESC d{};
        if (!t || FAILED(t->GetLevelDesc(0, &d)) || !d.Width || !d.Height)
            return { 0,0,0,0 };

        float w = (float)d.Width;
        float h = (float)d.Height;
        return { w, h, 1.0f / w, 1.0f / h };
    }

    static inline float4_t size4_from_surf(IDirect3DSurface9* s)
    {
        D3DSURFACE_DESC d{};
        if (!s || FAILED(s->GetDesc(&d)) || !d.Width || !d.Height)
            return { 0,0,0,0 };

        float w = (float)d.Width;
        float h = (float)d.Height;
        return { w, h, 1.0f / w, 1.0f / h };
    }

    static D3DXHANDLE zm_find_ct_handle(ID3DXConstantTable* ct, const char* id)
    {
        if (!ct || !id || !id[0]) return nullptr;

        // MVP special-case (shader emits global_MVP)
        if (_stricmp(id, "MVP") == 0)
            return ct->GetConstantByName(nullptr, "global_MVP");

        auto try_name = [&](const char* name) -> D3DXHANDLE {
            return name ? ct->GetConstantByName(nullptr, name) : nullptr;
            };

        auto try_fmt1 = [&](const char* fmt, const char* a) -> D3DXHANDLE {
            char buf[128];
            _snprintf(buf, sizeof(buf), fmt, a);
            return try_name(buf);
            };

        auto try_fmt2 = [&](const char* fmt, const char* a, const char* b) -> D3DXHANDLE {
            char buf[128];
            _snprintf(buf, sizeof(buf), fmt, a, b);
            return try_name(buf);
            };

        auto try_all_for_id = [&](const char* s) -> D3DXHANDLE
            {
                // 1) raw
                if (D3DXHANDLE h = try_name(s)) return h;

                // 2) params_foo / params.foo
                if (D3DXHANDLE h = try_fmt1("params_%s", s)) return h;
                if (D3DXHANDLE h = try_fmt1("params.%s", s)) return h;

                // 3) global_foo / global.foo
                if (D3DXHANDLE h = try_fmt1("global_%s", s)) return h;
                if (D3DXHANDLE h = try_fmt1("global.%s", s)) return h;

                // 4) NEW: registers_foo (SPIRV-Cross common)
                if (D3DXHANDLE h = try_fmt1("registers_%s", s)) return h;

                // 5) NEW: Push.registers_foo (cbuffer-scoped exposure)
                if (D3DXHANDLE h = try_fmt1("Push.registers_%s", s)) return h;

                // 6) NEW: sometimes just Push.foo
                if (D3DXHANDLE h = try_fmt1("Push.%s", s)) return h;

                // 7) NEW: occasionally global/params can wrap registers_
                if (D3DXHANDLE h = try_fmt2("global_%s%s", "registers_", s)) return h; // "global_registers_Foo"
                if (D3DXHANDLE h = try_fmt2("params_%s%s", "registers_", s)) return h;

                return nullptr;
            };

        // First pass: exact id
        if (D3DXHANDLE h = try_all_for_id(id))
            return h;

        // Second pass: case-normalized variant (covers brighten_scanlines vs BRIGHTEN_SCANLINES)
        {
            char up[128];
            char lo[128];

            _snprintf(up, sizeof(up), "%s", id);
            _snprintf(lo, sizeof(lo), "%s", id);

            for (char* p = up; *p; ++p) *p = (char)toupper((unsigned char)*p);
            for (char* p = lo; *p; ++p) *p = (char)tolower((unsigned char)*p);

            if (_stricmp(up, id) != 0) {
                if (D3DXHANDLE h = try_all_for_id(up)) return h;
            }
            if (_stricmp(lo, id) != 0) {
                if (D3DXHANDLE h = try_all_for_id(lo)) return h;
            }
        }

        return nullptr;
    }

    static bool zm_set_uniform_by_desc(
        IDirect3DDevice9* dev,
        ID3DXConstantTable* ct,
        D3DXHANDLE h,
        const void* data,
        UINT bytes)
    {
        if (!dev || !ct || !h || !data || bytes == 0)
            return false;

        D3DXCONSTANT_DESC d{};
        UINT n = 1;
        if (FAILED(ct->GetConstantDesc(h, &d, &n)))
            return false;

        // Matrices: prefer SetMatrixTranspose when shape matches.
        if (d.Class == D3DXPC_MATRIX_ROWS || d.Class == D3DXPC_MATRIX_COLUMNS)
        {
            if (d.Rows == 4 && d.Columns == 4 && bytes >= sizeof(float) * 16)
            {
                const D3DXMATRIX* m = (const D3DXMATRIX*)data;
                return SUCCEEDED(ct->SetMatrixTranspose(dev, h, m));
            }
            // Fall through for unexpected matrix shapes.
        }

        // Default: set based on constant type
        if ((bytes & 3) != 0)
            return false;

        UINT want_dwords = bytes / 4;

        // Clamp to constant table register allocation.
        // One constant register = 4 x 32-bit slots.
        UINT max_dwords = d.RegisterCount * 4;
        if (max_dwords == 0) max_dwords = want_dwords; // safety fallback
        UINT dwords = (want_dwords < max_dwords) ? want_dwords : max_dwords;

        // TEMP DEBUG: dump type/shape/register for key uniforms (NOW dwords is defined)
        if (d.Name && (
            strstr(d.Name, "OriginalSize") ||
            strstr(d.Name, "SourceSize") ||
            strstr(d.Name, "OutputSize") ||
            strstr(d.Name, "FinalViewport") ||
            strstr(d.Name, "FrameCount") ||
            strstr(d.Name, "FrameDirection") ||
            strstr(d.Name, "Size") ||
            strstr(d.Name, "params") ||
            strstr(d.Name, "registers")
            ))
        {
            zm_draw_dbgf("[CT-WRITE] name='%s' class=%u type=%u reg=%u regCount=%u rows=%u cols=%u bytes=%u want_dw=%u clamp_dw=%u\n",
                d.Name,
                (unsigned)d.Class, (unsigned)d.Type,
                (unsigned)d.RegisterIndex, (unsigned)d.RegisterCount,
                (unsigned)d.Rows, (unsigned)d.Columns,
                (unsigned)bytes,
                (unsigned)want_dwords,
                (unsigned)dwords);
        }

        // If HLSL declared int/bool, write as int array.
        if (d.Type == D3DXPT_INT || d.Type == D3DXPT_BOOL)
            return SUCCEEDED(ct->SetIntArray(dev, h, (const int*)data, dwords));

        // Float (and everything else as float slots)
        return SUCCEEDED(ct->SetFloatArray(dev, h, (const float*)data, dwords));
    }
    static IDirect3DTexture9* zm_find_alias_rt_tex(
        const d3d9_slang_runtime* rt,
        const video_shader* preset,   // d3d9->shader
        const char* alias)
    {
        if (!rt || !preset || !alias || !alias[0]) return nullptr;

        const unsigned N = rt->num_passes;
        for (unsigned j = 0; j < N; ++j)
        {
            const char* a = preset->pass[j].alias;
            if (a && a[0] && strcmp(a, alias) == 0)
                return rt->passes[j].rt; // RT texture for that earlier pass
        }
        return nullptr;
    }
    static void zm_bind_all_ps_samplers_by_ct(
        IDirect3DDevice9* dev,
        d3d9_video_struct* d3d9,
        d3d9_slang_runtime* rt,
        const d3d9_slang_pass& P,
        const video_shader_pass& cfg,
        IDirect3DTexture9* in_tex)
    {
        if (!dev || !d3d9 || !rt || !P.ps_ct || !in_tex) return;

        D3DXCONSTANTTABLE_DESC td{};
        if (FAILED(P.ps_ct->GetDesc(&td))) return;

        auto addr_from_wrap = [](gfx_wrap_type w) -> D3DTEXTUREADDRESS {
            switch (w) {
            case RARCH_WRAP_BORDER: return D3DTADDRESS_BORDER;
            case RARCH_WRAP_EDGE:   return D3DTADDRESS_CLAMP;
            case RARCH_WRAP_REPEAT: return D3DTADDRESS_WRAP;
            case RARCH_WRAP_MIRRORED_REPEAT: return D3DTADDRESS_MIRROR;
            default: return D3DTADDRESS_CLAMP;
            }
            };
        auto filt_from_filter = [](unsigned f) -> D3DTEXTUREFILTERTYPE {
            return (f == RARCH_FILTER_NEAREST) ? D3DTEXF_POINT : D3DTEXF_LINEAR;
            };

        const D3DTEXTUREADDRESS addr = addr_from_wrap(cfg.wrap);
        const D3DTEXTUREFILTERTYPE ff = filt_from_filter(cfg.filter);

        for (UINT i = 0; i < td.Constants; ++i)
        {
            D3DXHANDLE h = P.ps_ct->GetConstant(nullptr, i);
            if (!h) continue;

            D3DXCONSTANT_DESC cd{};
            UINT n = 1;
            if (FAILED(P.ps_ct->GetConstantDesc(h, &cd, &n))) continue;

            if (cd.Class != D3DXPC_OBJECT) continue;
            if (cd.Type != D3DXPT_SAMPLER2D) continue;
            if (!cd.Name || !cd.Name[0]) continue;

            const int stage = (int)cd.RegisterIndex;
            if (stage < 0) continue;

            IDirect3DTexture9* tex = nullptr;

            if (strcmp(cd.Name, "Source") == 0)
            {
                tex = in_tex;
            }
            else if (strcmp(cd.Name, "Original") == 0)
            {
                tex = rt->live_original_tex ? rt->live_original_tex : in_tex;
            }
            else
            {
                // Alias samplers: refpass, scalefx_pass0, etc.
                tex = zm_find_alias_rt_tex(rt, &d3d9->shader, cd.Name);

                // Optional fallback if alias missing (keeps things from going black)
                if (!tex)
                    tex = in_tex;
            }

            dev->SetTexture(stage, tex);

            dev->SetSamplerState(stage, D3DSAMP_ADDRESSU, addr);
            dev->SetSamplerState(stage, D3DSAMP_ADDRESSV, addr);
            dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, ff);
            dev->SetSamplerState(stage, D3DSAMP_MINFILTER, ff);
            dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            // >>> kill gamma state leakage
            dev->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
            if (addr == D3DTADDRESS_BORDER)
                dev->SetSamplerState(stage, D3DSAMP_BORDERCOLOR, 0x00000000);

            zm_draw_dbgf("[SAMPLER-BIND] pass='%s' name='%s' stage=%d tex=%p\n",
                (cfg.alias[0] ? cfg.alias : "(none)"), cd.Name, stage, (void*)tex);
        }
    }

    static int zm_find_first_sampler2d_register(ID3DXConstantTable* ct)
    {
        if (!ct) return -1;

        D3DXCONSTANTTABLE_DESC td{};
        if (FAILED(ct->GetDesc(&td)))
            return -1;

        for (UINT i = 0; i < td.Constants; ++i)
        {
            D3DXHANDLE h = ct->GetConstant(nullptr, i);
            if (!h) continue;

            D3DXCONSTANT_DESC cd{};
            UINT n = 1;
            if (FAILED(ct->GetConstantDesc(h, &cd, &n)))
                continue;

            // Samplers show up as OBJECT class, SAMPLER2D type in D3DX.
            if (cd.Class == D3DXPC_OBJECT && cd.Type == D3DXPT_SAMPLER2D)
                return (int)cd.RegisterIndex;
        }

        return -1;
    }

    static void zm_force_push_uniforms_ct(IDirect3DDevice9* dev,
        ID3DXConstantTable* ct,
        d3d9_slang_runtime* rt)
    {
        if (!dev || !ct || !rt) return;

        // sizes (float4)
        struct { const char* name; const float4_t* v; } vec4s[] = {
            { "SourceSize",        &rt->live_source_size },
            { "OriginalSize",      &rt->live_original_size },
            { "OutputSize",        &rt->live_output_size },
            { "FinalViewportSize", &rt->live_final_viewport },
        };

        for (auto& it : vec4s)
        {
            if (D3DXHANDLE h = zm_find_ct_handle(ct, it.name))
                ct->SetFloatArray(dev, h, (const float*)it.v, 4);
        }

        // FrameCount (uint) - use 0 
        if (D3DXHANDLE h = zm_find_ct_handle(ct, "FrameCount"))
            ct->SetInt(dev, h, 0);

        // shader params - hard-force defaults to prove parameter path
        if (D3DXHANDLE h = zm_find_ct_handle(ct, "brighten_scanlines"))
            ct->SetFloat(dev, h, 16.0f);

        if (D3DXHANDLE h = zm_find_ct_handle(ct, "brighten_lcd"))
            ct->SetFloat(dev, h, 4.0f);
    }

    static void apply_cbuffer_vs_by_name(IDirect3DDevice9* dev,
        ID3DXConstantTable* ct,
        const cbuffer_sem_t& cb)
    {
        if (!dev || !ct || !cb.uniforms || cb.uniform_count <= 0)
            return;

        for (int i = 0; i < cb.uniform_count; i++)
        {
            const uniform_sem_t& u = cb.uniforms[i];
            if (!u.data || u.size == 0 || u.id[0] == 0)
                continue;

            // MVP is handled explicitly by set_mvp_vs(); do not stomp it here.
            if (_stricmp(u.id, "MVP") == 0)
                continue;

            D3DXHANDLE h = zm_find_ct_handle(ct, u.id);
            if (!h)
                continue;

            // Type-aware set (matrix vs float array)
            (void)zm_set_uniform_by_desc(dev, ct, h, u.data, (UINT)u.size);
        }
    }

    static void apply_cbuffer_ps_by_name(IDirect3DDevice9* dev,
        ID3DXConstantTable* ct,
        const cbuffer_sem_t& cb)
    {
        if (!dev || !ct || !cb.uniforms || cb.uniform_count <= 0)
            return;

        for (int i = 0; i < cb.uniform_count; i++)
        {
            const uniform_sem_t& u = cb.uniforms[i];
            if (!u.data || u.size == 0 || u.id[0] == 0)
                continue;

            // PS shouldn't be setting MVP; ignore quietly.
            if (_stricmp(u.id, "MVP") == 0)
                continue;

            // ---- DEBUG: show which IDs slang is giving (PS) ----
            zm_draw_dbgf("[SEM-PS] id='%s' size=%u\n", u.id, (unsigned)u.size);

            D3DXHANDLE h = zm_find_ct_handle(ct, u.id);

            // Keep failure signal.
            if (!h)
            {
                zm_dbgf("[ZeroMod] PS uniform NOT FOUND: '%s' size=%u\n",
                    u.id, (unsigned)u.size);
                continue;
            }

            // Type-aware set (matrix vs float array)
            if (!zm_set_uniform_by_desc(dev, ct, h, u.data, (UINT)u.size))
            {
                zm_dbgf("[ZeroMod] PS uniform set FAILED: '%s'\n", u.id);
            }
        }
    }

    static void set_mvp_vs(IDirect3DDevice9* dev, ID3DXConstantTable* ct, const D3DXMATRIX* m)
    {
        if (!dev || !ct || !m) return;

        D3DXHANDLE h = ct->GetConstantByName(nullptr, "global_MVP");
        if (!h) h = ct->GetConstantByName(nullptr, "MVP");
        if (!h) h = ct->GetConstantByName(nullptr, "params_MVP");

        if (!h) {
            OutputDebugStringA("[ZeroMod] VS MVP handle NOT FOUND\n");
            return;
        }
        HRESULT hr = ct->SetMatrixTranspose(dev, h, m);
        if (FAILED(hr)) {
            char b[128];
            _snprintf(b, sizeof(b), "[ZeroMod] VS MVP set FAILED hr=0x%08lX\n", (unsigned long)hr);
            OutputDebugStringA(b);
        }
    }
 
    static void set_halfpixel_vs(IDirect3DDevice9* dev, ID3DXConstantTable* ct, UINT w, UINT h)
    {
        if (!dev || !ct || !w || !h) return;

        D3DXHANDLE hhp = ct->GetConstantByName(nullptr, "gl_HalfPixel");
        if (!hhp) return;

        float hp[4] = { 1.0f / (float)w, 1.0f / (float)h, 0.0f, 0.0f };
        ct->SetFloatArray(dev, hhp, hp, 4);
    }

    static void zm_free_cstr(char*& p)
    {
        if (p) { free(p); p = NULL; }
    }

    static void runtime_clear(d3d9_slang_runtime* rt)
    {
        if (!rt) return;
        zm_free_cstr(rt->built_for_path);
        for (unsigned i = 0; i < rt->num_passes; i++)
            slang_pass_clear(rt->passes[i]);
        rt->num_passes = 0;
        rt->built = false;
    }

    bool slang_d3d9_runtime_create(d3d9_video_struct* d3d9)
    {
        if (!d3d9 || d3d9->magic != 0x39564433)
            return false;

        if (d3d9->slang_rt)
            return true;

        d3d9_slang_runtime* rt = (d3d9_slang_runtime*)calloc(1, sizeof(*rt));
        if (!rt)
            return false;

        d3d9->slang_rt = rt;

        zm_dbgf("[ZeroMod] slang_runtime_create: d3d9=%p rt=%p\n", (void*)d3d9, (void*)rt);
        return true;
    }

    void slang_d3d9_runtime_destroy(d3d9_video_struct* d3d9)
    {
        if (!d3d9 || d3d9->magic != 0x39564433)
            return;

        d3d9_slang_runtime* rt = (d3d9_slang_runtime*)d3d9->slang_rt;
        if (!rt)
            return;

        runtime_clear(rt);
        free(rt);
        d3d9->slang_rt = NULL;

        zm_dbgf("[ZeroMod] slang_runtime_destroy\n");
    }

    static const char* wrap_to_str(enum gfx_wrap_type w)
    {
        switch (w) {
        case RARCH_WRAP_BORDER: return "BORDER";
        case RARCH_WRAP_EDGE: return "EDGE";
        case RARCH_WRAP_REPEAT: return "REPEAT";
        case RARCH_WRAP_MIRRORED_REPEAT: return "MIRROR";
        default: return "?";
        }
    }

    static const char* scale_to_str(enum gfx_scale_type t)
    {
        switch (t) {
        case RARCH_SCALE_INPUT: return "INPUT";
        case RARCH_SCALE_ABSOLUTE: return "ABS";
        case RARCH_SCALE_VIEWPORT: return "VIEWPORT";
        default: return "?";
        }
    }

    static const char* filter_to_str(unsigned f)
    {
        switch (f) {
        case RARCH_FILTER_LINEAR: return "LINEAR";
        case RARCH_FILTER_NEAREST: return "NEAREST";
        default: return "UNSPEC";
        }
    }

    static bool d3d9_compile_shader(
        IDirect3DDevice9* dev,
        const char* src,
        const char* entry,
        const char* profile,
        IDirect3DVertexShader9** out_vs,
        IDirect3DPixelShader9** out_ps,
        ID3DXConstantTable** out_ct)
    {
        if (!dev || !src || !entry || !profile)
            return false;

        if (out_vs) *out_vs = nullptr;
        if (out_ps) *out_ps = nullptr;
        if (out_ct) *out_ct = nullptr;

        ID3DXBuffer* code = nullptr;
        ID3DXBuffer* err = nullptr;
        ID3DXConstantTable* ct = nullptr;

        HRESULT hr = D3DXCompileShader(
            src,
            (UINT)strlen(src),
            nullptr, nullptr,
            entry,
            profile,
            D3DXSHADER_OPTIMIZATION_LEVEL3,
            &code,
            &err,
            out_ct ? &ct : nullptr);

        if (FAILED(hr) || !code)
        {
            char buf[256];
            _snprintf(buf, sizeof(buf),
                "[ZeroMod] D3DXCompileShader FAILED hr=0x%08lX entry='%s' profile='%s'\n",
                (unsigned long)hr,
                entry ? entry : "(null)",
                profile ? profile : "(null)");
            OutputDebugStringA(buf);

            if (err && err->GetBufferPointer())
            {
                OutputDebugStringA("[ZeroMod] ---- Compiler Error ----\n");
                OutputDebugStringA((char*)err->GetBufferPointer());
                OutputDebugStringA("\n[ZeroMod] -----------------------\n");
            }
            else
            {
                OutputDebugStringA("[ZeroMod] No compiler error blob.\n");
            }

            // Dump the exact HLSL that failed
            static int fail_id = 0;
            char path[260];
            _snprintf(path, sizeof(path),
                "C:\\temp\\zm_compile_fail_%d_%s_%s.hlsl",
                fail_id++,
                profile,
                entry);

            FILE* f = nullptr;
            fopen_s(&f, path, "wb");
            if (f)
            {
                fwrite(src, 1, strlen(src), f);
                fclose(f);
                OutputDebugStringA("[ZeroMod] Wrote failing HLSL to C:\\temp\\\n");
            }

            if (err) err->Release();
            if (code) code->Release();
            if (ct) ct->Release();
            return false;
        }

        const bool isVS = (strncmp(profile, "vs_", 3) == 0);
        const bool isPS = (strncmp(profile, "ps_", 3) == 0);

        if (out_vs && isVS)
        {
            hr = dev->CreateVertexShader((DWORD*)code->GetBufferPointer(), out_vs);
            if (FAILED(hr) || !*out_vs)
            {
                code->Release();
                if (err) err->Release();
                if (ct) ct->Release();
                return false;
            }
        }

        if (out_ps && isPS)
        {
            hr = dev->CreatePixelShader((DWORD*)code->GetBufferPointer(), out_ps);
            if (FAILED(hr) || !*out_ps)
            {
                code->Release();
                if (err) err->Release();
                if (ct) ct->Release();
                return false;
            }
        }

        code->Release();
        if (err) err->Release();

        // hand off CT to caller if requested, otherwise release it
        if (out_ct)
            *out_ct = ct;
        else if (ct)
            ct->Release();

        return true;
    }

    bool slang_d3d9_runtime_build_from_parsed(d3d9_video_struct* d3d9)
    {
        if (!d3d9 || d3d9->magic != 0x39564433)
            return false;

        if (!d3d9->shader_preset) {
            zm_dbgf("[ZeroMod] slang_runtime_build_from_parsed: no parsed preset (shader_preset=0)\n");
            return false;
        }

        if (!slang_d3d9_runtime_create(d3d9))
            return false;

        d3d9_slang_runtime* rt = (d3d9_slang_runtime*)d3d9->slang_rt;

        // Early-out if already built for same preset AND all passes compiled.
        if (rt->built && rt->built_for_path && d3d9->shader_path &&
            strcmp(rt->built_for_path, d3d9->shader_path) == 0)
        {
            if (slang_all_passes_compiled(rt))
                return true;
            // else fall through to rebuild (previous build incomplete)
        }

        // Clear previous build state
        runtime_clear(rt);

        // Capture current preset identity
        if (d3d9->shader_path)
            rt->built_for_path = _strdup(d3d9->shader_path);

        // Log parsed preset summary
        zm_dbgf("[ZeroMod] slang_runtime_build_from_parsed: path='%s' passes=%u luts=%u params=%u vars=%u history=%d\n",
            d3d9->shader_path ? d3d9->shader_path : "(null)",
            (unsigned)d3d9->shader.passes,
            (unsigned)d3d9->shader.luts,
            (unsigned)d3d9->shader.num_parameters,
            (unsigned)d3d9->shader.variables,
            (int)d3d9->shader.history_size);

        // Clamp pass count
        unsigned passes = d3d9->shader.passes;
        if (passes > GFX_MAX_SHADERS) passes = GFX_MAX_SHADERS;
        rt->num_passes = passes;

        // Log each pass
        for (unsigned i = 0; i < passes; i++)
        {
            const video_shader_pass& p = d3d9->shader.pass[i];

            zm_dbgf("[ZeroMod] pass[%u] alias='%s' srcpath='%s'\n",
                i, p.alias, p.source.path);

            zm_dbgf("[ZeroMod] pass[%u] fbo=(%s,%s sx=%.3f sy=%.3f abs=%ux%u fp=%d srgb=%d) wrap=%s filter=%s mip=%d feedback=%d mod=%u\n",
                i,
                scale_to_str(p.fbo.type_x),
                scale_to_str(p.fbo.type_y),
                p.fbo.scale_x, p.fbo.scale_y,
                p.fbo.abs_x, p.fbo.abs_y,
                (int)p.fbo.fp_fbo,
                (int)p.fbo.srgb_fbo,
                wrap_to_str(p.wrap),
                filter_to_str(p.filter),
                (int)p.mipmap,
                (int)p.feedback,
                (unsigned)p.frame_count_mod);
        }

        if (passes == 0) {
            zm_dbgf("[ZeroMod] slang_runtime_build_from_parsed: passes==0\n");
            return false;
        }

        // ---------------------------------------------------------------------
        // MULTI-PASS COMPILE LOOP
        // ---------------------------------------------------------------------
        for (unsigned i = 0; i < passes; ++i)
        {
            d3d9_slang_pass& P = rt->passes[i];
            slang_pass_clear(P);

            zm_dbgf("[ZeroMod] pass%u: begin compile block\n", i);

            // IMPORTANT:
            // Build-time semantics values are placeholders.
            // slang_process wants pointers; real values will be pushed per-pass at runtime via CT.
            {
                // actual content backing texture is always 256x192.
                // “240x160” is an implicit crop handled by overscan/VPOS, NOT by changing sizes.
                const float OW = 256.0f;
                const float OH = 192.0f;

                rt->live_original_tex = nullptr;                    // only need the pointer to exist
                rt->live_original_size = { OW, OH, 1.0f / OW, 1.0f / OH };

                // For build-time, keep SourceSize sane/nonzero.
                rt->live_source_size = rt->live_original_size;

                // Output/FinalViewport placeholders: nonzero and stable.
                // 256x192 is fine; 256x256 arbitrary.
                rt->live_output_size = rt->live_original_size;
                rt->live_final_viewport = rt->live_original_size;

                rt->live_frame_count = 0;
                rt->live_frame_dir = 1;
            }

            semantics_map_t semantics_map = {};
            semantics_map.textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL].image = &rt->live_original_tex;
            semantics_map.textures[SLANG_TEXTURE_SEMANTIC_ORIGINAL].size = &rt->live_original_size;
            semantics_map.textures[SLANG_TEXTURE_SEMANTIC_SOURCE].image = &rt->live_original_tex;
            semantics_map.textures[SLANG_TEXTURE_SEMANTIC_SOURCE].size = &rt->live_source_size;

            semantics_map.uniforms[SLANG_SEMANTIC_MVP] = &d3d9->mvp;
            semantics_map.uniforms[SLANG_SEMANTIC_OUTPUT] = &rt->live_output_size;
            semantics_map.uniforms[SLANG_SEMANTIC_FINAL_VIEWPORT] = &rt->live_final_viewport;
            semantics_map.uniforms[SLANG_SEMANTIC_FRAME_COUNT] = &rt->live_frame_count;
            semantics_map.uniforms[SLANG_SEMANTIC_FRAME_DIRECTION] = &rt->live_frame_dir;

            pass_semantics_t sem = {};
            bool ok = slang_process(&d3d9->shader, i, RARCH_SHADER_HLSL, 30, &semantics_map, &sem);
            if (!ok) {
                const char* v = d3d9->shader.pass[i].source.string.vertex;
                const char* p = d3d9->shader.pass[i].source.string.fragment;
                zm_dbgf("[ZeroMod] pass%u: slang_process FAILED (vs_ptr=%p ps_ptr=%p)\n", i, (void*)v, (void*)p);
                P.sem = {};
                P.sem_valid = false;
                return false;
            }

            P.sem = sem;
            P.sem_valid = true;
            zm_dbgf("[ZeroMod] pass%u: slang_process OK\n", i);

            // generated HLSL now lives in shader.pass[i].source.string.*
            const char* vs_src = d3d9->shader.pass[i].source.string.vertex;
            const char* ps_src = d3d9->shader.pass[i].source.string.fragment;

            if (!vs_src || !*vs_src) {
                zm_dbgf("[ZeroMod] pass%u: VS source is null/empty\n", i);
                return false;
            }
            if (!ps_src || !*ps_src) {
                zm_dbgf("[ZeroMod] pass%u: PS source is null/empty\n", i);
                return false;
            }

            zm_dbgf("[ZeroMod] pass%u: VS len=%u | PS len=%u\n",
                i, (unsigned)strlen(vs_src), (unsigned)strlen(ps_src));

#if defined(ZM_DUMP_PASS0_HLSL)
            if (i == 0) {
                static bool dumped0 = false;
                if (!dumped0) {
                    dumped0 = true;
                    FILE* f = nullptr;
                    fopen_s(&f, "C:\\temp\\pass0_vs_src.hlsl", "wb");
                    if (f) { fwrite(vs_src, 1, strlen(vs_src), f); fclose(f); }
                    fopen_s(&f, "C:\\temp\\pass0_ps_src.hlsl", "wb");
                    if (f) { fwrite(ps_src, 1, strlen(ps_src), f); fclose(f); }
                    OutputDebugStringA("[ZeroMod] dumped pass0 VS/PS to C:\\temp\\\n");
                }
            }
#endif

            // Compile VS
            if (!d3d9_compile_shader(d3d9->dev, vs_src, "main", "vs_3_0",
                &P.vs, nullptr, &P.vs_ct))
            {
                zm_dbgf("[ZeroMod] pass%u: VS compile FAILED\n", i);
                return false;
            }

            // Compile PS
            if (!d3d9_compile_shader(d3d9->dev, ps_src, "main", "ps_3_0",
                nullptr, &P.ps, &P.ps_ct))
            {
                zm_dbgf("[ZeroMod] pass%u: PS compile FAILED\n", i);
                return false;
            }

            zm_dbgf("[ZeroMod] pass%u: compile OK (vs=%p ps=%p)\n", i, P.vs, P.ps);

            // --- TL init: resolve PS sampler register for "Source" ---
            P.source_sampler_reg = -1;
            P.tl_inited = true;

            if (P.ps_ct)
            {
                int reg = -1;

                D3DXHANDLE hs = P.ps_ct->GetConstantByName(nullptr, "Source");
                if (hs)
                {
                    D3DXCONSTANT_DESC cd{};
                    UINT n = 1;
                    if (SUCCEEDED(P.ps_ct->GetConstantDesc(hs, &cd, &n)))
                        reg = (int)cd.RegisterIndex;
                }

                if (reg < 0)
                    reg = zm_find_first_sampler2d_register(P.ps_ct);

                P.source_sampler_reg = reg;
            }

            if (P.source_sampler_reg < 0)
                zm_dbgf("[ZeroMod] pass%u: WARNING: no sampler2D found in PS CT; falling back to s0\n", i);

            // per-pass checks
            if (P.vs_ct)
            {
                if (!P.vs_ct->GetConstantByName(nullptr, "gl_HalfPixel"))
                    zm_dbgf("[ZeroMod] pass%u: NOTE: VS missing gl_HalfPixel\n", i);
                if (!P.vs_ct->GetConstantByName(nullptr, "global_MVP"))
                    zm_dbgf("[ZeroMod] pass%u: NOTE: VS missing global_MVP\n", i);
            }

            // Keep copies for inspection/debug
            if (P.hlsl_vs) { free(P.hlsl_vs); P.hlsl_vs = nullptr; }
            if (P.hlsl_ps) { free(P.hlsl_ps); P.hlsl_ps = nullptr; }
            P.hlsl_vs = _strdup(vs_src);
            P.hlsl_ps = _strdup(ps_src);

            P.compiled = true;
        }

        zm_dbgf("[ZeroMod] slang_runtime_build_from_parsed: rt->built will be set TRUE now\n");
        rt->built = true;
        zm_dbgf("[ZeroMod] slang_runtime_build_from_parsed: BUILT (passes=%u)\n", rt->num_passes);

        return true;
    }

    void slang_d3d9_runtime_tick(d3d9_video_struct* d3d9)
    {
        if (!d3d9 || d3d9->magic != 0x39564433)
            return;

        // If parse-only hasn’t succeeded, nothing to do.
        if (!d3d9->shader_preset)
            return;

        // Ensure runtime exists and is built for current preset.
        slang_d3d9_runtime_build_from_parsed(d3d9);
    }
    static void slang_update_quad_pos_xy(IDirect3DVertexBuffer9* vb, float sx, float sy)
    {
        if (!vb) return;

        zm_slang_vertex* v = nullptr;
        if (FAILED(vb->Lock(0, 0, (void**)&v, 0)) || !v) return;

        // TL, BL, TR, BR strip order
        v[0].x = -sx; v[0].y = +sy;
        v[1].x = -sx; v[1].y = -sy;
        v[2].x = +sx; v[2].y = +sy;
        v[3].x = +sx; v[3].y = -sy;

        vb->Unlock();
    }

    static void slang_set_quad_uv_rect(IDirect3DVertexBuffer9* vb, float u0, float v0, float u1, float v1)
    {
        if (!vb) return;

        zm_slang_vertex* v = nullptr;
        if (FAILED(vb->Lock(0, 0, (void**)&v, 0)) || !v) return;

        // TL, BL, TR, BR strip order
        v[0].u = u0; v[0].v = v0; // TL
        v[1].u = u0; v[1].v = v1; // BL
        v[2].u = u1; v[2].v = v0; // TR
        v[3].u = u1; v[3].v = v1; // BR

        vb->Unlock();
    }

    static void slang_reset_quad_uv(IDirect3DVertexBuffer9* vb)
    {
        slang_set_quad_uv_rect(vb, 0.0f, 0.0f, 1.0f, 1.0f);
    }

    static bool draw_fixedfunc_textured_quad_rhw_xy(
        IDirect3DDevice9* dev,
        IDirect3DTexture9* tex,
        int dst_x, int dst_y,          // <-- top-left in RT/backbuffer
        UINT dst_w, UINT dst_h,        // size of quad
        float u0, float v0, float u1, float v1,
        bool point_filter)
    {
        if (!dev || !tex || !dst_w || !dst_h) return false;

        struct FFVtx { float x, y, z, rhw; float u, v; };
        const DWORD FVF = D3DFVF_XYZRHW | D3DFVF_TEX1;

        // D3D9 pixel center convention
        const float x0 = (float)dst_x - 0.5f;
        const float y0 = (float)dst_y - 0.5f;
        const float x1 = (float)(dst_x + (int)dst_w) - 0.5f;
        const float y1 = (float)(dst_y + (int)dst_h) - 0.5f;

        FFVtx v[4] = {
            { x0, y0, 0.0f, 1.0f,  u0, v0 }, // TL
            { x0, y1, 0.0f, 1.0f,  u0, v1 }, // BL
            { x1, y0, 0.0f, 1.0f,  u1, v0 }, // TR
            { x1, y1, 0.0f, 1.0f,  u1, v1 }, // BR
        };

        dev->SetVertexShader(nullptr);
        dev->SetPixelShader(nullptr);
        dev->SetVertexDeclaration(nullptr);
        dev->SetFVF(FVF);

        dev->SetTexture(0, tex);

        dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

        dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

        const D3DTEXTUREFILTERTYPE f = point_filter ? D3DTEXF_POINT : D3DTEXF_LINEAR;
        dev->SetSamplerState(0, D3DSAMP_MINFILTER, f);
        dev->SetSamplerState(0, D3DSAMP_MAGFILTER, f);

        dev->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

        return SUCCEEDED(dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(FFVtx)));
    }

    bool slang_d3d9_frame(
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
        if (!d3d9 || d3d9->magic != 0x39564433) return false;
        if (!d3d9->dev || !src_tex || !dst_rtv) return false;
        if (!d3d9->shader_preset) return false;

        // Ensure runtime exists + built for current preset
        slang_d3d9_runtime_tick(d3d9);

        auto* rt = (d3d9_slang_runtime*)d3d9->slang_rt;
        if (!rt || !rt->built || rt->num_passes == 0) return false;

        IDirect3DDevice9* dev = d3d9->dev;

        // Resolve default viewport
        if (!vp_w || !vp_h) {
            D3DSURFACE_DESC d{};
            if (FAILED(dst_rtv->GetDesc(&d)) || !d.Width || !d.Height) return false;
            vp_x = 0; vp_y = 0; vp_w = (UINT)d.Width; vp_h = (UINT)d.Height;
        }

        // Match CG: derive (ox,oy,ow,oh) preferring scissor if subset
        UINT ox = vp_x, oy = vp_y, ow = vp_w, oh = vp_h;
        if (scissor_on) {
            LONG sw = scissor.right - scissor.left;
            LONG sh = scissor.bottom - scissor.top;
            if (sw > 0 && sh > 0) {
                if ((UINT)sw <= ow && (UINT)sh <= oh) {
                    ox = (UINT)scissor.left;
                    oy = (UINT)scissor.top;
                    ow = (UINT)sw;
                    oh = (UINT)sh;
                }
            }
        }

        // --- Save state ---
        IDirect3DSurface9* rt_saved = nullptr;
        IDirect3DSurface9* ds_saved = nullptr;
        D3DVIEWPORT9       vp_saved = {};
        IDirect3DVertexShader9* vs_saved = nullptr;
        IDirect3DPixelShader9* ps_saved = nullptr;
        IDirect3DBaseTexture9* tex0_saved = nullptr;

        dev->GetRenderTarget(0, &rt_saved);
        dev->GetDepthStencilSurface(&ds_saved);
        dev->GetViewport(&vp_saved);

        dev->GetVertexShader(&vs_saved);
        dev->GetPixelShader(&ps_saved);
        dev->GetTexture(0, &tex0_saved);

        auto restore_state = [&]() {
            dev->SetTexture(0, tex0_saved);
            dev->SetVertexShader(vs_saved);
            dev->SetPixelShader(ps_saved);

            dev->SetRenderTarget(0, rt_saved);
            dev->SetDepthStencilSurface(ds_saved);
            dev->SetViewport(&vp_saved);

            if (tex0_saved) { tex0_saved->Release(); tex0_saved = nullptr; }
            if (vs_saved) { vs_saved->Release();   vs_saved = nullptr; }
            if (ps_saved) { ps_saved->Release();   ps_saved = nullptr; }
            if (rt_saved) { rt_saved->Release();   rt_saved = nullptr; }
            if (ds_saved) { ds_saved->Release();   ds_saved = nullptr; }
            };

        // Baseline states
        dev->SetDepthStencilSurface(nullptr);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

        // Bind quad stream (same as apply_pass0)
        dev->SetStreamSource(0, d3d9->frame_vbo, 0, sizeof(zm_slang_vertex));
        dev->SetVertexDeclaration(d3d9->vertex_decl);

        // Source dims
        D3DSURFACE_DESC srcd{};
        src_tex->GetLevelDesc(0, &srcd);
        const UINT orig_w = srcd.Width;
        const UINT orig_h = srcd.Height;

        // Zero-mode: treat 240x160 as the active rect inside 256x192 (center crop)
        bool do_active_crop = false;

        // UV crop to center 240x160 inside 256x192:
        // u in [8/256 .. 248/256], v in [16/192 .. 176/192]
        float crop_u0 = 0.0f, crop_v0 = 0.0f, crop_u1 = 1.0f, crop_v1 = 1.0f;

        if (vp_is_3_2 && orig_w == 256 && orig_h == 192)
        {
            do_active_crop = true;
            crop_u0 = 8.0f / 256.0f;
            crop_u1 = 248.0f / 256.0f;
            crop_v0 = 16.0f / 192.0f;
            crop_v1 = 176.0f / 192.0f;
        }
        // Integer-fit inside game-rect viewport (ow/oh) for active 240x160
        UINT k = 1;
        UINT int_w = 0, int_h = 0;

        if (do_active_crop) {
            const UINT kx = ow / 240;
            const UINT ky = oh / 160;
            k = (kx < ky) ? kx : ky;
            if (k < 1) k = 1;

            int_w = 240 * k;
            int_h = 160 * k;

            if (!ensure_zero_rt(dev, d3d9->zero_pre, 240, 160)) { restore_state(); return false; }
            if (!ensure_zero_rt(dev, d3d9->zero_out, int_w, int_h)) { restore_state(); return false; }
        }
        // ZM-INT: Force integer scaling for ZX path
        else {
            const UINT kx = ow / orig_w;
            const UINT ky = oh / orig_h;
            k = (kx < ky) ? kx : ky;
            if (k < 1) k = 1;

            int_w = orig_w * k;
            int_h = orig_h * k;

            if (int_w != ow || int_h != oh) {
                if (!ensure_zero_rt(dev, d3d9->zero_out, int_w, int_h)) { restore_state(); return false; }
            }
        }
        const UINT eff_vp_w = int_w;
        const UINT eff_vp_h = int_h;

        IDirect3DTexture9* in_tex = src_tex;
        UINT in_w = orig_w, in_h = orig_h;
        (void)in_w;
        (void)in_h;

        const unsigned N = rt->num_passes;

        if (do_active_crop)
        {
            // Render cropped active image into integer-scaled RT (int_w x int_h)
            dev->SetRenderTarget(0, d3d9->zero_pre.surf);
            D3DVIEWPORT9 vp0{};
            vp0.X = 0; vp0.Y = 0;
            vp0.Width = 240;
            vp0.Height = 160;
            vp0.MinZ = 0; vp0.MaxZ = 1;
            dev->SetViewport(&vp0);

            dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

            // Fullscreen quad, but UV cropped to active rect
            slang_update_quad_pos_xy(d3d9->frame_vbo, 1.0f, 1.0f);

            // Draw with POINT to preserve exact integer scaling
            if (!draw_fixedfunc_textured_quad_rhw_xy(
                dev, src_tex,
                0, 0,
                240, 160,
                crop_u0, crop_v0, crop_u1, crop_v1,
                true))
            {
                restore_state();
                return false;
            }
            // Ensure shader pass quad is always full-screen and full UVs
            slang_update_quad_pos_xy(d3d9->frame_vbo, 1.0f, 1.0f);
            slang_reset_quad_uv(d3d9->frame_vbo);

            // Now feed shader chain from the integer-scaled texture
            in_tex = d3d9->zero_pre.tex;
            D3DSURFACE_DESC t{};
            in_tex->GetLevelDesc(0, &t);
            zm_draw_dbgf("[CHECK] in_tex GetLevelDesc = %ux%u\n", (unsigned)t.Width, (unsigned)t.Height);
            in_w = 240;
            in_h = 160;

            // original lattice for the preset is now 240x160
            rt->live_original_tex = in_tex;
        }
        dev->SetStreamSource(0, d3d9->frame_vbo, 0, sizeof(zm_slang_vertex));
        dev->SetVertexDeclaration(d3d9->vertex_decl);

        for (unsigned i = 0; i < N; ++i)
        {
            d3d9_slang_pass& P = rt->passes[i];
            const video_shader_pass& cfg = d3d9->shader.pass[i];

            if (!P.compiled) { restore_state(); return false; }

            // --- Determine out size (match CG/RA preset semantics) ---
            UINT base_w = (cfg.fbo.type_x == RARCH_SCALE_VIEWPORT) ? eff_vp_w : (UINT)size4_from_tex(in_tex).x;
            UINT base_h = (cfg.fbo.type_y == RARCH_SCALE_VIEWPORT) ? eff_vp_h : (UINT)size4_from_tex(in_tex).y;

            UINT out_w = 1, out_h = 1;

            if (cfg.fbo.type_x == RARCH_SCALE_ABSOLUTE)
                out_w = cfg.fbo.abs_x ? cfg.fbo.abs_x : 1;
            else
                out_w = (UINT)(base_w * (cfg.fbo.scale_x > 0 ? cfg.fbo.scale_x : 1.0f) + 0.5f);

            if (cfg.fbo.type_y == RARCH_SCALE_ABSOLUTE)
                out_h = cfg.fbo.abs_y ? cfg.fbo.abs_y : 1;
            else
                out_h = (UINT)(base_h * (cfg.fbo.scale_y > 0 ? cfg.fbo.scale_y : 1.0f) + 0.5f);

            if (out_w < 1) out_w = 1;
            if (out_h < 1) out_h = 1;

            // Final pass always fills game-rect (ow/oh)
            if (i == N - 1) { out_w = eff_vp_w; out_h = eff_vp_h; }

            // --- Select render target surface ---
            IDirect3DSurface9* out_surf = nullptr;
            if (i == N - 1) {
                if (int_w != ow || int_h != oh)
                    out_surf = d3d9->zero_out.surf;
                else
                    out_surf = dst_rtv;
            }
            else {
                const bool want_fp = (cfg.fbo.fp_fbo != 0);
                if (!slang_ensure_rt(dev, P, out_w, out_h, want_fp)) {
                    restore_state();
                    return false;
                }
                out_surf = P.rt_surf;
            }

            dev->SetRenderTarget(0, out_surf);

            // --- Viewport + scissor ---
            D3DVIEWPORT9 pass_vp{};
            if (i == N - 1) {
                // FINAL PASS:
                if (int_w != ow || int_h != oh) {
                    pass_vp.X = 0;
                    pass_vp.Y = 0;
                    pass_vp.Width = (DWORD)eff_vp_w;
                    pass_vp.Height = (DWORD)eff_vp_h;
                    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
                }
                else {
                    pass_vp.X = (DWORD)ox;
                    pass_vp.Y = (DWORD)oy;
                    pass_vp.Width = (DWORD)ow;
                    pass_vp.Height = (DWORD)oh;
                    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
                    RECT sr{ (LONG)ox, (LONG)oy, (LONG)(ox + ow), (LONG)(oy + oh) };
                    dev->SetScissorRect(&sr);
                }
            }
            else {
                // INTERMEDIATE PASS
                pass_vp.X = 0;
                pass_vp.Y = 0;
                pass_vp.Width = (DWORD)out_w;
                pass_vp.Height = (DWORD)out_h;
                dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
            }
            pass_vp.MinZ = 0.0f;
            pass_vp.MaxZ = 1.0f;
            dev->SetViewport(&pass_vp);

            // --- Update live semantics for THIS pass ---
            {
                // The actual input texture is always 256x192.
                // Any “240x160” is an *implicit* crop handled by overscan/VPos, NOT by changing sizes.
                // Therefore OriginalSize should match the actual original content texture.
                rt->live_original_tex = do_active_crop ? d3d9->zero_pre.tex : src_tex;
                rt->live_original_size = size4_from_tex(rt->live_original_tex);

                float4_t orig_sz = size4_from_tex(src_tex); // container 256x192
                float4_t src_sz = size4_from_tex(in_tex);  // ACTUAL sampled tex grid

                if (do_active_crop) {
                    const float W = 240.0f, H = 160.0f;
                    const float4_t logical = { W, H, 1.0f / W, 1.0f / H };

                    // OriginalSize: what to conceptually consider the "game source lattice"
                    orig_sz = logical;

                    // SourceSize MUST match the texture we're actually sampling.
                    // Only override it if the input texture really is 240x160.
                    if ((UINT)src_sz.x == 240 && (UINT)src_sz.y == 160) {
                        src_sz = logical;
                    }
                    // else: leave src_sz as size4_from_tex(in_tex) (e.g. 2160x1440)
                }

                rt->live_original_size = orig_sz;
                rt->live_source_size = src_sz;

                // OutputSize = current pass viewport region
                {
                    float outWf = (float)pass_vp.Width;
                    float outHf = (float)pass_vp.Height;
                    rt->live_output_size = { outWf, outHf, 1.0f / outWf, 1.0f / outHf };
                }

                // FinalViewportSize should reflect the final displayed viewport (game rect).
                {
                    float fW = do_active_crop ? (float)eff_vp_w : (float)ow;
                    float fH = do_active_crop ? (float)eff_vp_h : (float)oh;
                    rt->live_final_viewport = { fW, fH, 1.0f / fW, 1.0f / fH };
                }

                rt->live_frame_count = (uint32_t)frame_count;
                rt->live_frame_dir = 1;
            }
            // ---------------------------------------------------------------------
            // DEBUG DUMP: single-pass focus
            // ---------------------------------------------------------------------
#if ZM_VERBOSE_DRAW
            {
                const bool want_dump =
                    (strstr(cfg.source.path, "xbrz-freescale-pass1") != nullptr);

                if (want_dump)
                {
                    // texture sizes
                    float4_t sz_src = size4_from_tex(src_tex); // should be 256x192
                    float4_t sz_in = size4_from_tex(in_tex);

                    zm_draw_dbgf("\n[ZeroMod][SLANG-DUMP] pass=%u/%u  vp_is_3_2=%d  do_active_crop=%d  crop_uv=[%.6f,%.6f..%.6f,%.6f]\n",
                        i, (unsigned)N, (int)vp_is_3_2, (int)do_active_crop,
                        crop_u0, crop_v0, crop_u1, crop_v1);

                    zm_draw_dbgf("  ORIG: orig_w=%u orig_h=%u\n", (unsigned)orig_w, (unsigned)orig_h);
                    zm_draw_dbg4("size(src_tex)", sz_src);
                    zm_draw_dbg4("size(in_tex)", sz_in);

                    zm_draw_dbgf("  GAME-RECT (ox/oy/ow/oh): ox=%u oy=%u ow=%u oh=%u\n",
                        (unsigned)ox, (unsigned)oy, (unsigned)ow, (unsigned)oh);

                    zm_draw_vp("pass_vp", pass_vp);

                    // live semantics that feed forced CT push
                    zm_draw_dbgf("  LIVE SEMANTICS:\n");
                    zm_draw_dbg4("OriginalSize", rt->live_original_size);
                    zm_draw_dbg4("SourceSize", rt->live_source_size);
                    zm_draw_dbg4("OutputSize", rt->live_output_size);
                    zm_draw_dbg4("FinalViewport", rt->live_final_viewport);
                    zm_draw_dbgf("    FrameCount     = %u\n", (unsigned)rt->live_frame_count);
                    zm_draw_dbgf("    FrameDir       = %d\n", (int)rt->live_frame_dir);

                    // pass config summary
                    zm_draw_dbgf("  PASS CFG:\n");
                    // arrays are never null; treat empty string as "unset"
                    const char* alias_s = (cfg.alias[0] != '\0') ? cfg.alias : "(none)";
                    const char* path_s = (cfg.source.path[0] != '\0') ? cfg.source.path : "(none)";

                    zm_draw_dbgf("    alias='%s' src='%s'\n", alias_s, path_s);
                    const char* vsrc = cfg.source.string.vertex;
                    const char* psrc = cfg.source.string.fragment;
                    zm_draw_dbgf("    vs_ptr=%p ps_ptr=%p vs_len=%u ps_len=%u\n",
                        (void*)vsrc, (void*)psrc,
                        (unsigned)(vsrc ? strlen(vsrc) : 0),
                        (unsigned)(psrc ? strlen(psrc) : 0));

                    zm_draw_dbgf("    fbo: type_x=%s type_y=%s  sx=%.4f sy=%.4f  abs=%ux%u  fp=%d srgb=%d\n",
                        scale_to_str(cfg.fbo.type_x),
                        scale_to_str(cfg.fbo.type_y),
                        cfg.fbo.scale_x, cfg.fbo.scale_y,
                        (unsigned)cfg.fbo.abs_x, (unsigned)cfg.fbo.abs_y,
                        (int)cfg.fbo.fp_fbo, (int)cfg.fbo.srgb_fbo);

                    zm_draw_dbgf("    wrap=%s filter=%s mip=%d feedback=%d frame_mod=%u\n",
                        wrap_to_str(cfg.wrap),
                        filter_to_str(cfg.filter),
                        (int)cfg.mipmap,
                        (int)cfg.feedback,
                        (unsigned)cfg.frame_count_mod);

                    // pass runtime binding info
                    zm_draw_dbgf("  PASS RT:\n");
                    zm_draw_dbgf("    compiled=%d  vs=%p ps=%p  vs_ct=%p ps_ct=%p  sampler_reg=%d\n",
                        (int)P.compiled, P.vs, P.ps, P.vs_ct, P.ps_ct, (int)P.source_sampler_reg);

                }
            }
#endif
#if ZM_VERBOSE_DRAW
            // --- PROOF BLOCK: What are we ACTUALLY drawing with? (pass i) ---
            {
                // 1) Current RT
                IDirect3DSurface9* curRT = nullptr;
                dev->GetRenderTarget(0, &curRT);
                D3DSURFACE_DESC rtd{};
                if (curRT) curRT->GetDesc(&rtd);

                // 2) Current viewport
                D3DVIEWPORT9 curVP{};
                dev->GetViewport(&curVP);

                // 3) Scissor enable + rect (since scissor can silently kill intermediates)
                DWORD sc_en = 0;
                RECT sc_rc{};
                dev->GetRenderState(D3DRS_SCISSORTESTENABLE, &sc_en);
                dev->GetScissorRect(&sc_rc);

                // 4) What texture is bound at stage 0 and at given stage
                IDirect3DBaseTexture9* t0 = nullptr;
                dev->GetTexture(0, &t0);

                int s = P.source_sampler_reg;
                if (s < 0) s = 0;
                IDirect3DBaseTexture9* ts = nullptr;
                dev->GetTexture(s, &ts);

                // input size (in_tex)
                D3DSURFACE_DESC ind{};
                in_tex->GetLevelDesc(0, &ind);

                zm_draw_dbgf("[MP-PROOF] pass=%u/%u  s=%d\n", i, (unsigned)N, s);
                zm_draw_dbgf("  in_tex=%p %ux%u\n", (void*)in_tex, (unsigned)ind.Width, (unsigned)ind.Height);
                zm_draw_dbgf("  RT=%p %ux%u fmt=%u\n", (void*)curRT, (unsigned)rtd.Width, (unsigned)rtd.Height, (unsigned)rtd.Format);
                zm_draw_dbgf("  VP=%u,%u %ux%u\n", (unsigned)curVP.X, (unsigned)curVP.Y, (unsigned)curVP.Width, (unsigned)curVP.Height);
                zm_draw_dbgf("  SCISSOR en=%u rc=(%ld,%ld)-(%ld,%ld)\n",
                    (unsigned)sc_en, sc_rc.left, sc_rc.top, sc_rc.right, sc_rc.bottom);
                zm_draw_dbgf("  TEX stage0=%p  stageS=%p\n", (void*)t0, (void*)ts);

                if (t0) t0->Release();
                if (ts) ts->Release();
                if (curRT) curRT->Release();
            }
#endif
            // Draw this pass (factored from apply_pass0)
            if (!slang_bind_and_draw_pass(dev, d3d9, rt, P, cfg, in_tex, pass_vp)) {
                restore_state();
                return false;
            }

            // Next input texture is the RT just rendered (intermediate only)
            if (i != N - 1) {
                zm_draw_dbgf("[MP] after pass%u: out=%ux%u  P.rt=%p  P.rt_surf=%p\n",
                    i, out_w, out_h, (void*)P.rt, (void*)P.rt_surf);

                D3DSURFACE_DESC dd{};
                if (P.rt) P.rt->GetLevelDesc(0, &dd);
                zm_draw_dbgf("[MP] P.rt desc = %ux%u\n", (unsigned)dd.Width, (unsigned)dd.Height);
                in_tex = P.rt;
                in_w = out_w;
                in_h = out_h;
            }
        }
        if (int_w != ow || int_h != oh)
        {
            const UINT dx = (ow > eff_vp_w) ? (ow - eff_vp_w) / 2 : 0;
            const UINT dy = (oh > eff_vp_h) ? (oh - eff_vp_h) / 2 : 0;

            dev->SetRenderTarget(0, dst_rtv);

            D3DVIEWPORT9 vpF{};
            vpF.X = (DWORD)(ox + dx);
            vpF.Y = (DWORD)(oy + dy);
            vpF.Width = (DWORD)eff_vp_w;
            vpF.Height = (DWORD)eff_vp_h;
            vpF.MinZ = 0.0f; vpF.MaxZ = 1.0f;
            dev->SetViewport(&vpF);

            dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
            RECT sr{ (LONG)ox, (LONG)oy, (LONG)(ox + ow), (LONG)(oy + oh) };
            dev->SetScissorRect(&sr);

            // reuse your quad (full UVs)
            slang_update_quad_pos_xy(d3d9->frame_vbo, 1.0f, 1.0f);
            slang_reset_quad_uv(d3d9->frame_vbo);

            // set RT/viewport/scissor
            dev->SetRenderTarget(0, dst_rtv);
            dev->SetViewport(&vpF);
            dev->SetScissorRect(&sr);

            // RIGHT BEFORE FINAL BLIT (the draw)
            dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

            // log sizes RIGHT BEFORE FINAL BLIT
            D3DSURFACE_DESC zd{};
            d3d9->zero_out.tex->GetLevelDesc(0, &zd);
            zm_draw_dbgf("[FINAL BLIT] zero_out=%ux%u dst=%ux%u\n",
                (unsigned)zd.Width, (unsigned)zd.Height,
                (unsigned)eff_vp_w, (unsigned)eff_vp_h);

            if (!draw_fixedfunc_textured_quad_rhw_xy(
                dev, d3d9->zero_out.tex,
                (int)(ox + dx), (int)(oy + dy),   // <--- APPLY the 200px offset here
                eff_vp_w, eff_vp_h,               // size on screen
                0.0f, 0.0f, 1.0f, 1.0f,           // full UVs
                true))
            {
                restore_state();
                return false;
            }
        }

        // Restore caller state
        restore_state();
        return true;
    }

    static bool slang_bind_and_draw_pass(
        IDirect3DDevice9* dev,
        d3d9_video_struct* d3d9,
        d3d9_slang_runtime* rt,
        d3d9_slang_pass& P,
        const video_shader_pass& cfg,
        IDirect3DTexture9* in_tex,
        const D3DVIEWPORT9& pass_vp
    )
    {
        if (!dev || !d3d9 || !rt || !P.compiled || !P.vs || !P.ps || !in_tex)
            return false;
        // Clear a small known range to prevent state leakage (D3D9 hazard)
        for (int st = 0; st < 4; ++st)
            dev->SetTexture(st, nullptr);

        for (int st = 0; st < 8; ++st)  // or however many used
            dev->SetSamplerState(st, D3DSAMP_SRGBTEXTURE, FALSE);

        // halfpixel must match THIS pass viewport
        set_halfpixel_vs(dev, P.vs_ct, pass_vp.Width, pass_vp.Height);

        // Identity MVP
        D3DXMATRIX I;
        D3DXMatrixIdentity(&I);
        set_mvp_vs(dev, P.vs_ct, &I);

        // Bind shaders
        if (FAILED(dev->SetVertexShader(P.vs))) return false;
        if (FAILED(dev->SetPixelShader(P.ps)))  return false;

        // Upload cbuffers from semantics
        if (P.sem_valid)
        {
            const cbuffer_sem_t& ubo = P.sem.cbuffers[SLANG_CBUFFER_UBO];
            const cbuffer_sem_t& pc = P.sem.cbuffers[SLANG_CBUFFER_PC];

            const uint32_t VS_MASK = SLANG_STAGE_VERTEX_MASK;
            const uint32_t PS_MASK = SLANG_STAGE_FRAGMENT_MASK;

            if (ubo.stage_mask & VS_MASK) apply_cbuffer_vs_by_name(dev, P.vs_ct, ubo);
            if (ubo.stage_mask & PS_MASK) apply_cbuffer_ps_by_name(dev, P.ps_ct, ubo);

            if (pc.stage_mask & VS_MASK)  apply_cbuffer_vs_by_name(dev, P.vs_ct, pc);
            if (pc.stage_mask & PS_MASK)  apply_cbuffer_ps_by_name(dev, P.ps_ct, pc);
        }

        // Only use forced push when there isn't a valid reflected cbuffer path.
        // Otherwise it can clobber the real UBO/PC data layout.
        if (!P.sem_valid) {
            zm_force_push_uniforms_ct(dev, P.vs_ct, rt);
            zm_force_push_uniforms_ct(dev, P.ps_ct, rt);
        }

        if (rt && rt->num_passes == 1) {
            zm_draw_dbgf("[ZeroMod][SLANG-DUMP][DRAW] out_vp=%ux%u  live Out=%.0fx%.0f  Final=%.0fx%.0f  Src=%.0fx%.0f Orig=%.0fx%.0f\n",
                (unsigned)pass_vp.Width, (unsigned)pass_vp.Height,
                rt->live_output_size.x, rt->live_output_size.y,
                rt->live_final_viewport.x, rt->live_final_viewport.y,
                rt->live_source_size.x, rt->live_source_size.y,
                rt->live_original_size.x, rt->live_original_size.y);
        }

        auto dump_sampler = [&](ID3DXConstantTable* ct, const char* name) -> int
            {
                if (!ct) { zm_draw_dbgf("    sampler '%s': ct=null\n", name); return -1; }
                D3DXHANDLE h = ct->GetConstantByName(nullptr, name);
                if (!h) { zm_draw_dbgf("    sampler '%s': NOT FOUND\n", name); return -1; }
                int idx = (int)ct->GetSamplerIndex(h);
                zm_draw_dbgf("    sampler '%s': stage=%d\n", name, idx);
                return idx;
            };

        // per-pass
        zm_draw_dbgf("[SAMPLERS] (alias='%s')\n", (cfg.alias[0] ? cfg.alias : "(none)"));
        dump_sampler(P.ps_ct, "Source");
        dump_sampler(P.ps_ct, "Original");
        zm_draw_dbgf("    P.source_sampler_reg=%d\n", (int)P.source_sampler_reg);

        // Bind ALL sampler2D slots declared in PS (Source/Original/alias samplers)
        zm_bind_all_ps_samplers_by_ct(dev, d3d9, rt, P, cfg, in_tex);

        // --- POST-BIND PROOF (after SetTexture calls) ---
        if (P.ps_ct) {
            int sSrc = -1, sOrg = -1;
            if (auto hS = P.ps_ct->GetConstantByName(nullptr, "Source"))   sSrc = (int)P.ps_ct->GetSamplerIndex(hS);
            if (auto hO = P.ps_ct->GetConstantByName(nullptr, "Original")) sOrg = (int)P.ps_ct->GetSamplerIndex(hO);

            IDirect3DBaseTexture9* tS = nullptr;
            IDirect3DBaseTexture9* tO = nullptr;
            if (sSrc >= 0) dev->GetTexture(sSrc, &tS);
            if (sOrg >= 0) dev->GetTexture(sOrg, &tO);

            zm_draw_dbgf("[POSTBIND] Source stage=%d tex=%p  Original stage=%d tex=%p  live_original=%p in_tex=%p\n",
                sSrc, (void*)tS, sOrg, (void*)tO, (void*)rt->live_original_tex, (void*)in_tex);

            if (tS) tS->Release();
            if (tO) tO->Release();
        }

        // Baseline render states
        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        dev->SetRenderState(D3DRS_SRGBWRITEENABLE, (cfg.fbo.srgb_fbo != 0) ? TRUE : FALSE);

        HRESULT hr_dp = dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
        zm_draw_dbgf("[DRAW] pass alias='%s' hr=0x%08X\n", cfg.alias, (unsigned)hr_dp);
        return SUCCEEDED(hr_dp);
    }

} // namespace ZeroMod
