/*#include "slang_d3d9.h"
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

#define ZM_DUMP_PASS0_HLSL 1
//#define ZM_USE_GAME_STREAM 1
#define ZM_USE_OWN_QUAD 1

#ifndef ZEROMOD_FLOAT4_T_DEFINED
#define ZEROMOD_FLOAT4_T_DEFINED

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

        d3d9_slang_pass pass0;

        IDirect3DTexture9* live_original_tex = nullptr;

        float4_t live_original_size = { 0,0,0,0 }; // (w,h,1/w,1/h)
        float4_t live_source_size = { 0,0,0,0 }; // usually same as original for pass0

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

    static void slang_update_sizes_pass0(d3d9_video_struct* d3d9, d3d9_slang_runtime* rt)
    {
        // --- OriginalSize: manual per content ---
        const bool zx = ZeroMod::is_zx(); // your latch
        float ow = zx ? 256.0f : 240.0f;  // DS : GBA
        float oh = zx ? 192.0f : 160.0f;

        float4_t orig = { ow, oh, 1.0f / ow, 1.0f / oh };

        // --- SourceSize: from actual bound source texture ---
        float4_t src = size4_from_tex(d3d9->slang_frame.src_tex);

        // --- OutputSize: MUST match the RT you're actually drawing into ---
        float4_t out = size4_from_surf(d3d9->slang_frame.dst_rtv);

        // --- FinalViewportSize: in this minimal pass0 pipeline, treat as OutputSize ---
        float4_t fvp = out;

        rt->live_final_viewport = fvp;

        // Cache + log only on change
        static float4_t last_orig{ 0,0,0,0 }, last_src{ 0,0,0,0 }, last_out{ 0,0,0,0 };
        static IDirect3DTexture9* last_src_tex = nullptr;
        static IDirect3DSurface9* last_dst = nullptr;

        const bool changed =
            !f4eq(orig, last_orig) || !f4eq(src, last_src) || !f4eq(out, last_out) ||
            d3d9->slang_frame.src_tex != last_src_tex ||
            d3d9->slang_frame.dst_rtv != last_dst;

        if (changed)
        {
            last_orig = orig;
            last_src = src;
            last_out = out;
            last_src_tex = d3d9->slang_frame.src_tex;
            last_dst = d3d9->slang_frame.dst_rtv;

            zm_dbgf("[ZeroMod] SIZES pass0: zx=%d | Original=%.0fx%.0f | Source=%.0fx%.0f | Output=%.0fx%.0f | src_tex=%p dst=%p\n",
                (int)zx,
                orig.x, orig.y,
                src.x, src.y,
                out.x, out.y,
                d3d9->slang_frame.src_tex,
                d3d9->slang_frame.dst_rtv);
        }

        // Publish into runtime live semantics
        rt->live_original_tex = d3d9->slang_frame.src_tex; // for pass0 Original==Source texture object
        rt->live_original_size = orig;
        rt->live_source_size = src;

        rt->live_output_size = out;
        rt->live_final_viewport = fvp;
    }
    static D3DXHANDLE zm_find_ct_handle(ID3DXConstantTable* ct, const char* id)
    {
        if (!ct || !id || !id[0]) return nullptr;

        // MVP special-case (your shader emits global_MVP)
        if (_stricmp(id, "MVP") == 0)
            return ct->GetConstantByName(nullptr, "global_MVP");

        // 1) raw
        if (D3DXHANDLE h = ct->GetConstantByName(nullptr, id))
            return h;

        // 2) params_foo
        {
            char buf[128];
            _snprintf(buf, sizeof(buf), "params_%s", id);
            if (D3DXHANDLE h = ct->GetConstantByName(nullptr, buf))
                return h;
        }

        // 3) params.foo   (IMPORTANT: some compilers expose struct members with dot syntax)
        {
            char buf[128];
            _snprintf(buf, sizeof(buf), "params.%s", id);
            if (D3DXHANDLE h = ct->GetConstantByName(nullptr, buf))
                return h;
        }

        // 4) global_foo
        {
            char buf[128];
            _snprintf(buf, sizeof(buf), "global_%s", id);
            if (D3DXHANDLE h = ct->GetConstantByName(nullptr, buf))
                return h;
        }

        // 5) global.foo
        {
            char buf[128];
            _snprintf(buf, sizeof(buf), "global.%s", id);
            if (D3DXHANDLE h = ct->GetConstantByName(nullptr, buf))
                return h;
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

        const UINT dwords = bytes / 4;

        // If HLSL declared int/bool, write as int array.
        // Note: D3DX uses 32-bit slots either way.
        if (d.Type == D3DXPT_INT || d.Type == D3DXPT_BOOL)
        {
            return SUCCEEDED(ct->SetIntArray(dev, h, (const int*)data, dwords));
        }

        // Float (and everything else we treat as float slots)
        return SUCCEEDED(ct->SetFloatArray(dev, h, (const float*)data, dwords));
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

        // FrameCount (uint) - use 0 for now (lcd3x doesn't depend on it)
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

            D3DXHANDLE h = zm_find_ct_handle(ct, u.id);

            // Keep your "failure signal" only here (as you had).
            if (!h)
            {
                zm_dbgf("[ZeroMod] PS uniform NOT FOUND: '%s' size=%u\n",
                    u.id, (unsigned)u.size);
                continue;
            }

            // Type-aware set (matrix vs float array)
            if (!zm_set_uniform_by_desc(dev, ct, h, u.data, (UINT)u.size))
            {
                // Preserve your existing failure logging behavior.
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

        float hp[4] = { 0.5f / (float)w, 0.5f / (float)h, 0.0f, 0.0f };
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
        slang_pass_clear(rt->pass0);
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
            if (err && err->GetBufferPointer())
                OutputDebugStringA((char*)err->GetBufferPointer());
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

        if (rt->built && rt->built_for_path && d3d9->shader_path &&
            strcmp(rt->built_for_path, d3d9->shader_path) == 0)
        {
            // IMPORTANT:
            // If we marked "built" earlier but never actually compiled pass0,
            // we MUST NOT early-out here.
            if (rt->pass0.compiled)
                return true;
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

        // Log each pass (this is “useful out of the bones” right now)
        unsigned passes = d3d9->shader.passes;
        if (passes > GFX_MAX_SHADERS) passes = GFX_MAX_SHADERS;

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

        if (passes > 0)
        {
            zm_dbgf("[ZeroMod] pass0: begin compile block (passes=%u)\n", passes);
            slang_pass_clear(rt->pass0);
            {
                const video_shader_pass& p0 = d3d9->shader.pass[0];
                zm_dbgf(
                    "[RAW PASS0 BEFORE SIZE] "
                    "fbo=(type_x=%d type_y=%d sx=%f sy=%f abs=%ux%u fp=%d srgb=%d)\n",
                    p0.fbo.type_x,
                    p0.fbo.type_y,
                    p0.fbo.scale_x,
                    p0.fbo.scale_y,
                    p0.fbo.abs_x,
                    p0.fbo.abs_y,
                    (int)p0.fbo.fp_fbo,
                    (int)p0.fbo.srgb_fbo
                );
            }
            slang_update_sizes_pass0(d3d9, rt);

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
            bool ok = slang_process(&d3d9->shader, 0, RARCH_SHADER_HLSL, 30, &semantics_map, &sem);
            if (!ok) {
                zm_dbgf("[ZeroMod] pass0: slang_process FAILED\n");
                rt->pass0.sem = {};
                rt->pass0.sem_valid = false;
                return false;
            }

            rt->pass0.sem = sem;
            rt->pass0.sem_valid = true;
            zm_dbgf("[ZeroMod] pass0: slang_process OK\n");
          //  dump_pass_semantics(rt->pass0.sem);

            // slang_process has already run successfully with RARCH_SHADER_HLSL.
            // At THIS point these point at the generated HLSL we want to compile.
            const char* vs_src = d3d9->shader.pass[0].source.string.vertex;
            const char* ps_src = d3d9->shader.pass[0].source.string.fragment;

            if (!vs_src || !*vs_src) {
                zm_dbgf("[ZeroMod] pass0: VS source is null/empty\n");
                return false;
            }
            if (!ps_src || !*ps_src) {
                zm_dbgf("[ZeroMod] pass0: PS source is null/empty\n");
                return false;
            }
            zm_dbgf("[ZeroMod] pass0: VS len=%u | PS len=%u\n",
                (unsigned)strlen(vs_src), (unsigned)strlen(ps_src));

#if defined(ZM_DUMP_PASS0_HLSL)
            static bool dumped = false;
            if (!dumped) {
                dumped = true;
                FILE* f = nullptr;
                fopen_s(&f, "C:\\temp\\pass0_vs_src.hlsl", "wb");
                if (f) { fwrite(vs_src, 1, strlen(vs_src), f); fclose(f); }
                fopen_s(&f, "C:\\temp\\pass0_ps_src.hlsl", "wb");
                if (f) { fwrite(ps_src, 1, strlen(ps_src), f); fclose(f); }
                OutputDebugStringA("[ZeroMod] dumped pass0 VS/PS to C:\\temp\\\n");
            }
#endif

            // Compile VS
            if (!d3d9_compile_shader(d3d9->dev, vs_src, "main", "vs_3_0",
                &rt->pass0.vs, nullptr, &rt->pass0.vs_ct))
            {
                zm_dbgf("[ZeroMod] pass0: VS compile FAILED\n");
                return false;
            }

            // Compile PS
            if (!d3d9_compile_shader(d3d9->dev, ps_src, "main", "ps_3_0",
                nullptr, &rt->pass0.ps, &rt->pass0.ps_ct))
            {
                zm_dbgf("[ZeroMod] pass0: PS compile FAILED\n");
                return false;
            }

            zm_dbgf("[ZeroMod] pass0: compile OK (vs=%p ps=%p)\n", rt->pass0.vs, rt->pass0.ps);

           // dump_ct_regs(rt->pass0.vs_ct, "VS");
           // dump_ct_regs(rt->pass0.ps_ct, "PS");
           
            // --- TL init: resolve PS sampler register for "Source" ---
            rt->pass0.source_sampler_reg = -1;
            rt->pass0.tl_inited = true;

            if (rt->pass0.ps_ct)
            {
                // Prefer name if it exists, but fall back to scanning.
                int reg = -1;

                D3DXHANDLE hs = rt->pass0.ps_ct->GetConstantByName(nullptr, "Source");
                if (hs)
                {
                    D3DXCONSTANT_DESC cd{};
                    UINT n = 1;
                    if (SUCCEEDED(rt->pass0.ps_ct->GetConstantDesc(hs, &cd, &n)))
                        reg = (int)cd.RegisterIndex;
                }

                if (reg < 0)
                    reg = zm_find_first_sampler2d_register(rt->pass0.ps_ct);

                rt->pass0.source_sampler_reg = reg;
            }

            if (rt->pass0.source_sampler_reg < 0)
                zm_dbgf("[ZeroMod] pass0: WARNING: no sampler2D found in PS CT; falling back to s0\n");

            if (rt->pass0.vs_ct)
            {
                if (!rt->pass0.vs_ct->GetConstantByName(nullptr, "gl_HalfPixel"))
                    zm_dbgf("[ZeroMod] pass0: NOTE: VS missing gl_HalfPixel\n");
                if (!rt->pass0.vs_ct->GetConstantByName(nullptr, "global_MVP"))
                    zm_dbgf("[ZeroMod] pass0: NOTE: VS missing global_MVP\n");
            }

            // Keep copies for inspection/debug later (optional but useful)
            if (rt->pass0.hlsl_vs) { free(rt->pass0.hlsl_vs); rt->pass0.hlsl_vs = nullptr; }
            if (rt->pass0.hlsl_ps) { free(rt->pass0.hlsl_ps); rt->pass0.hlsl_ps = nullptr; }
            rt->pass0.hlsl_vs = _strdup(vs_src);
            rt->pass0.hlsl_ps = _strdup(ps_src);

            rt->pass0.compiled = true;
        }
        zm_dbgf("[ZeroMod] slang_runtime_build_from_parsed: rt->built will be set TRUE now\n");

        rt->built = true;

        zm_dbgf("[ZeroMod] slang_runtime_build_from_parsed: BUILT\n");

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

    bool slang_d3d9_apply_pass0(d3d9_video_struct* d3d9)
    {
        if (!d3d9) return false;
        if (!d3d9->shader_preset) return false;

        auto* rt = (d3d9_slang_runtime*)d3d9->slang_rt;
        if (!rt || !rt->built || !rt->pass0.compiled) return false;

        if (!d3d9->slang_frame_valid) return false;
        if (!d3d9->slang_frame.src_tex) return false;
        if (!d3d9->slang_frame.dst_rtv) return false;

        IDirect3DDevice9* dev = d3d9->dev;
        if (!dev) return false;

        slang_update_sizes_pass0(d3d9, rt);

        zm_dbgf("[ZeroMod] pass0 grid: src_tex=%.0fx%.0f out_rt=%.0fx%.0f (scale=%.3fx%.3f)\n",
            rt->live_source_size.x, rt->live_source_size.y,
            rt->live_output_size.x, rt->live_output_size.y,
            d3d9->shader.pass[0].fbo.scale_x,
            d3d9->shader.pass[0].fbo.scale_y);

        IDirect3DSurface9* rt_saved = nullptr;
        IDirect3DSurface9* ds_saved = nullptr;
        D3DVIEWPORT9       vp_saved = {};

        dev->GetRenderTarget(0, &rt_saved);            // AddRef'd
        dev->GetDepthStencilSurface(&ds_saved);        // AddRef'd (may be null)
        dev->GetViewport(&vp_saved);
        IDirect3DVertexShader9* vs_saved = nullptr;
        IDirect3DPixelShader9* ps_saved = nullptr;
        IDirect3DBaseTexture9* tex0_saved = nullptr;

        dev->GetVertexShader(&vs_saved);  // AddRef'd
        dev->GetPixelShader(&ps_saved);   // AddRef'd
        dev->GetTexture(0, &tex0_saved);  // AddRef'd

        // Helper to restore everything on every exit path
        auto restore_state = [&]() {

            // Restore shader + texture first (so the caller draw resumes correctly)
            dev->SetTexture(0, tex0_saved);
            dev->SetVertexShader(vs_saved);
            dev->SetPixelShader(ps_saved);

            // Restore RT/DS + viewport
            dev->SetRenderTarget(0, rt_saved);
            dev->SetDepthStencilSurface(ds_saved);
            dev->SetViewport(&vp_saved);

            // Release in reverse-ish order, null them so multiple calls are safe
            if (tex0_saved) { tex0_saved->Release(); tex0_saved = nullptr; }
            if (vs_saved) { vs_saved->Release(); vs_saved = nullptr; }
            if (ps_saved) { ps_saved->Release(); ps_saved = nullptr; }

            if (rt_saved) { rt_saved->Release(); rt_saved = nullptr; }
            if (ds_saved) { ds_saved->Release(); ds_saved = nullptr; }
            };
        // ---------------------------
        dev->SetRenderTarget(0, d3d9->slang_frame.dst_rtv);
        // Optional: depth off for safety (lots of post passes assume none)
        dev->SetDepthStencilSurface(nullptr);
        // --- set RT + viewport ---
        // Set viewport to match dst RTV size
        D3DSURFACE_DESC rd{};
        if (FAILED(d3d9->slang_frame.dst_rtv->GetDesc(&rd)) || !rd.Width || !rd.Height) {
            restore_state();
            return false;
        }

        D3DVIEWPORT9 vp{};
        vp.X = 0;
        vp.Y = 0;
        vp.Width = rd.Width;
        vp.Height = rd.Height;
        vp.MinZ = 0.0f;
        vp.MaxZ = 1.0f;
        dev->SetViewport(&vp);

        // halfpixel must match THIS viewport
        set_halfpixel_vs(dev, rt->pass0.vs_ct, vp.Width, vp.Height);

        // Identity MVP (clip space in -> clip space out)
        D3DXMATRIX I;
        D3DXMatrixIdentity(&I);
        set_mvp_vs(dev, rt->pass0.vs_ct, &I);

        dev->SetStreamSource(0, d3d9->frame_vbo, 0, sizeof(zm_slang_vertex));
        dev->SetVertexDeclaration(d3d9->vertex_decl);

        if (!rt->pass0.vs || !rt->pass0.ps) { restore_state(); return false; }

        HRESULT hr_vs = dev->SetVertexShader(rt->pass0.vs);
        if (FAILED(hr_vs)) { restore_state(); return false; }

        HRESULT hr_ps = dev->SetPixelShader(rt->pass0.ps);
        if (FAILED(hr_ps)) { restore_state(); return false; }

        // --- upload cbuffers ---
        if (rt->pass0.sem_valid)
        {
            const cbuffer_sem_t& ubo = rt->pass0.sem.cbuffers[SLANG_CBUFFER_UBO];
            const cbuffer_sem_t& pc = rt->pass0.sem.cbuffers[SLANG_CBUFFER_PC];

            // Respect stage masks (if stage_mask is unreliable, this still fails harmlessly: missing handles are ignored)
            const uint32_t VS_MASK = SLANG_STAGE_VERTEX_MASK;
            const uint32_t PS_MASK = SLANG_STAGE_FRAGMENT_MASK;

            if (ubo.stage_mask & VS_MASK) apply_cbuffer_vs_by_name(dev, rt->pass0.vs_ct, ubo);
            if (ubo.stage_mask & PS_MASK) apply_cbuffer_ps_by_name(dev, rt->pass0.ps_ct, ubo);

            if (pc.stage_mask & VS_MASK)  apply_cbuffer_vs_by_name(dev, rt->pass0.vs_ct, pc);
            if (pc.stage_mask & PS_MASK)  apply_cbuffer_ps_by_name(dev, rt->pass0.ps_ct, pc);
        }
		// --- force size uniforms ---
        zm_force_push_uniforms_ct(dev, rt->pass0.vs_ct, rt);
        zm_force_push_uniforms_ct(dev, rt->pass0.ps_ct, rt);

        zm_dbgf("[ZeroMod] PUSH(forced): SourceSize=%.0fx%.0f Orig=%.0fx%.0f Out=%.0fx%.0f FC=%u bs=%.3f bl=%.3f\n",
            rt->live_source_size.x, rt->live_source_size.y,
            rt->live_original_size.x, rt->live_original_size.y,
            rt->live_output_size.x, rt->live_output_size.y,
            0u,
            16.0,
            4.0);

        auto addr_from_wrap = [](gfx_wrap_type w) -> D3DTEXTUREADDRESS {
            switch (w) {
            case RARCH_WRAP_BORDER: return D3DTADDRESS_BORDER;
            case RARCH_WRAP_EDGE:   return D3DTADDRESS_CLAMP; // edge == clamp in D3D9
            case RARCH_WRAP_REPEAT: return D3DTADDRESS_WRAP;
            case RARCH_WRAP_MIRRORED_REPEAT: return D3DTADDRESS_MIRROR;
            default: return D3DTADDRESS_CLAMP;
            }
            };
        auto filt_from_filter = [](unsigned f) -> D3DTEXTUREFILTERTYPE {
            return (f == RARCH_FILTER_NEAREST) ? D3DTEXF_POINT : D3DTEXF_LINEAR;
            };

            // Re-bind the intended slang RT
            dev->SetRenderTarget(0, d3d9->slang_frame.dst_rtv);
       
        // --- bind Source texture ---
        int s = rt->pass0.source_sampler_reg;
        if (s < 0) s = 0;

        dev->SetTexture(s, d3d9->slang_frame.src_tex);

        const video_shader_pass& p0 = d3d9->shader.pass[0];
        zm_dbgf(
            "[USED PASS0 IN SIZE] "
            "fbo=(type_x=%d type_y=%d sx=%f sy=%f abs=%ux%u)\n",
            p0.fbo.type_x,
            p0.fbo.type_y,
            p0.fbo.scale_x,
            p0.fbo.scale_y,
            p0.fbo.abs_x,
            p0.fbo.abs_y
        );
        D3DTEXTUREADDRESS addr = addr_from_wrap(p0.wrap);
        D3DTEXTUREFILTERTYPE ff = filt_from_filter(p0.filter);

        dev->SetSamplerState(s, D3DSAMP_ADDRESSU, addr);
        dev->SetSamplerState(s, D3DSAMP_ADDRESSV, addr);
        dev->SetSamplerState(s, D3DSAMP_MAGFILTER, ff);
        dev->SetSamplerState(s, D3DSAMP_MINFILTER, ff);
        dev->SetSamplerState(s, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        if (addr == D3DTADDRESS_BORDER)
            dev->SetSamplerState(s, D3DSAMP_BORDERCOLOR, 0x00000000); // or RA's expected border


        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

        HRESULT hr_dp = dev->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
        restore_state();
        return SUCCEEDED(hr_dp);
    }
} // namespace ZeroMod
*/