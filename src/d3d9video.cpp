#include "d3d9video.h"
//#include "video_driver.h"
#include "retroarch.h"
#include <d3dx9.h>
#include "log.h"
#include <vector>
#include <string>
#include <d3d9.h>
#include <d3d9types.h>
#include <cstring>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include "slang_d3d9_preset_load.h"
#include "slang_d3d9.h"


namespace ZeroMod {

    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;

    d3d9_video_struct* d3d9_gfx_init(IDirect3DDevice9* device, D3DFORMAT format)
    {
        if (!device) return NULL;

        d3d9_video_struct* d3d9 = (d3d9_video_struct*)calloc(1, sizeof(d3d9_video_struct));
        if (!d3d9) return NULL;

        memset(&d3d9->slang_frame, 0, sizeof(d3d9->slang_frame));
        d3d9->slang_frame.frame_direction = 1.0f;
        d3d9->slang_frame_valid = false;
        d3d9->magic = 0x39564433;
        d3d9->dev = device;
        d3d9->final_viewport.Width = 4;
        d3d9->final_viewport.Height = 4;
        d3d9->slang_rt = nullptr;
        d3d9->shader_path = nullptr;
        d3d9->shader_preset = false;            
        d3d9->shader_reload_pending = false;    
        d3d9->shader_is_path = false;           

        zm_slang_vertex vertices[] = {
                        { -1.0f,  1.0f, 0.0f, 1.0f,  u0, v0 }, // TL
                        { -1.0f, -1.0f, 0.0f, 1.0f,  u0, v1 }, // BL
                        {  1.0f,  1.0f, 0.0f, 1.0f,  u1, v0 }, // TR
                        {  1.0f, -1.0f, 0.0f, 1.0f,  u1, v1 }, // BR
        };

        // Initialize vertex buffer
        if (FAILED(d3d9->dev->CreateVertexBuffer(sizeof(vertices), 0, 0, D3DPOOL_MANAGED, &d3d9->frame_vbo, NULL))) {
            d3d9_gfx_free(d3d9);
            return NULL;
        }

        void* pVertices = nullptr; // Initialize pVertices
        if (FAILED(d3d9->frame_vbo->Lock(0, sizeof(vertices), (void**)&pVertices, 0))) {
            d3d9_gfx_free(d3d9);
            return NULL;
        }
        memcpy(pVertices, vertices, sizeof(vertices));
        d3d9->frame_vbo->Unlock();

        D3DVERTEXELEMENT9 decl[] =
        {
            // stream, offset, type, method, usage, usageIndex
            { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 }, // TEXCOORD0 = float4 position
            { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 }, // TEXCOORD1 = float2 uv
            D3DDECL_END()
        };

        if (!d3d9_vertex_declaration_new(d3d9->dev, decl, (void**)&d3d9->vertex_decl)) {
            d3d9_gfx_free(d3d9);
            return NULL;
        }

        // Additional initialization steps
        d3d9->pixel_shader = nullptr;

        return d3d9;
    }

    void d3d9_gfx_free(d3d9_video_struct* d3d9)
    {
        if (!d3d9) return;

        if (d3d9->magic != 0x39564433) {
            OutputDebugStringA("[ZeroMod] d3d9_gfx_free: BAD MAGIC -> skipping\n");
            return;
        }

        ZeroMod::slang_d3d9_runtime_destroy(d3d9);

        d3d9->magic = 0;

        IDirect3DVertexBuffer9* vb = d3d9->frame_vbo;
        IDirect3DVertexDeclaration9* vd = d3d9->vertex_decl;
        IDirect3DPixelShader9* ps = d3d9->pixel_shader;

        d3d9->frame_vbo = nullptr;
        d3d9->vertex_decl = nullptr;
        d3d9->pixel_shader = nullptr;

        if (vb) vb->Release();
        if (vd) vd->Release();
        if (ps) ps->Release();

        if (d3d9->shader_path) {
            free(d3d9->shader_path);
            d3d9->shader_path = nullptr;
        }
        d3d9->shader_preset = false;
        d3d9->shader_reload_pending = false;
        d3d9->shader_is_path = false;

        d3d9->slang_frame_valid = false;
        memset(&d3d9->slang_frame, 0, sizeof(d3d9->slang_frame));

        free(d3d9);
    }

    bool d3d9_gfx_frame(d3d9_video_struct* d3d9, IDirect3DTexture9* texture, UINT64 frame_count)
    {

        (void)texture;
        (void)frame_count;

        if (!d3d9 || d3d9->magic != 0x39564433)
            return false;

        if (!d3d9->shader_reload_pending)
            return true;

        OutputDebugStringA("[ZeroMod] d3d9_gfx_frame: entered\n");

        if (d3d9->shader_is_path && d3d9->shader_path && *d3d9->shader_path)
        {
            OutputDebugStringA("[ZeroMod] d3d9_gfx_frame: calling parse_only\n");

            bool ok = ZeroMod::slang_d3d9_load_preset_parse_only(d3d9);
            if (!d3d9->shader_reload_pending && d3d9->shader_preset)
                ZeroMod::slang_d3d9_runtime_tick(d3d9);

            if (!ok)
                OutputDebugStringA("[ZeroMod] d3d9_gfx_frame: preset parse failed\n");
        }
        else
        {
            d3d9->shader_reload_pending = false;
        }

        return true;
    }

    void d3d9_update_viewport(d3d9_video_struct* d3d9, IDirect3DSurface9* renderTargetView, video_viewport_t* viewport)
    {
        (void)renderTargetView;

        if (!d3d9 || d3d9->magic != 0x39564433 || !d3d9->dev || !viewport)
            return;

        d3d9->vp = *viewport;

        d3d9->final_viewport.X = d3d9->vp.x;
        d3d9->final_viewport.Y = d3d9->vp.y;
        d3d9->final_viewport.Width = d3d9->vp.width;
        d3d9->final_viewport.Height = d3d9->vp.height;
        d3d9->final_viewport.MinZ = 0.0f;
        d3d9->final_viewport.MaxZ = 1.0f;

        d3d9->dev->SetViewport(&d3d9->final_viewport);
    }
    static void zm_free_cstr(char*& p)
    {
        if (p) {
            free(p);
            p = nullptr;
        }
    }

    // Heuristic: looks like filesystem path or filename with an extension.
    static bool zm_looks_like_path(const char* s)
    {
        if (!s || !*s) return false;

        // Contains directory separators
        if (strchr(s, '\\') || strchr(s, '/'))
            return true;

        // Or a plausible shader-ish extension
        const char* exts[] = {
            ".slang", ".slangp",
            ".hlsl", ".fx",
            ".cg",
            ".glsl", ".frag", ".vert",
            ".txt" // some people ship hlsl in txt
        };

        for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); i++) {
            if (strstr(s, exts[i]))
                return true;
        }

        return false;
    }

    // Heuristic: looks like literal HLSL code (NOT a path).
    static bool zm_looks_like_hlsl_source(const char* s)
    {
        if (!s || !*s) return false;

        // If it contains newlines and common shader tokens, it's almost certainly source.
        // (Paths typically won't contain these.)
        if (strchr(s, '\n') || strchr(s, '\r')) {
            if (strstr(s, "float") || strstr(s, "sampler") || strstr(s, "Texture") ||
                strstr(s, "cbuffer") || strstr(s, "SV_Target") || strstr(s, "struct") ||
                strstr(s, "technique") || strstr(s, "return"))
                return true;
        }

        // Even without newlines: some short shaders start with these keywords.
        if (strstr(s, "float4") || strstr(s, "SV_Target") || strstr(s, "cbuffer"))
            return true;

        return false;
    }

    // Centralized setter so you don't leak. keep flags consistent.
    // - Passing nullptr clears path and resets flags.
    // - Passing non-null sets path, marks "is path", and sets reload_pending.
    static void zm_set_shader_path(ZeroMod::d3d9_video_struct* d3d9, const char* path)
    {
        if (!d3d9) return;

        if (!path || !*path) {
            zm_free_cstr(d3d9->shader_path);
            d3d9->shader_is_path = false;
            d3d9->shader_reload_pending = false;
            return;
        }

        // Only mark reload if it actually changed
        bool changed = true;
        if (d3d9->shader_path && strcmp(d3d9->shader_path, path) == 0)
            changed = false;

        zm_free_cstr(d3d9->shader_path);
        d3d9->shader_path = strdup(path);

        d3d9->shader_is_path = true;
        d3d9->shader_reload_pending = changed;

        if (changed) {
            char b[768];
            _snprintf(b, sizeof(b),
                "[ZeroMod] zm_set_shader_path: set '%s' (reload_pending=1)\n",
                d3d9->shader_path ? d3d9->shader_path : "(null)");
            OutputDebugStringA(b);
        }
    }

    void slang_d3d9_set_frame_ctx(
        d3d9_video_struct* d3d9,
        IDirect3DTexture9* src_tex,
        IDirect3DSurface9* dst_rtv,
        UINT dst_w,
        UINT dst_h,
        UINT vp_x,
        UINT vp_y,
        UINT64 frame_count)
    {
        if (!d3d9) return;

        d3d9->slang_frame.src_tex = src_tex;
        d3d9->slang_frame.dst_rtv = dst_rtv;
        d3d9->slang_frame.dst_w = dst_w;
        d3d9->slang_frame.dst_h = dst_h;

        d3d9->slang_frame.vp_x = vp_x;
        d3d9->slang_frame.vp_y = vp_y;

        d3d9->slang_frame.frame_count = frame_count;
        d3d9->slang_frame.frame_direction = 1.0f;

        d3d9->slang_frame_valid =
            (src_tex && dst_rtv && dst_w && dst_h);
    }

    bool d3d9_gfx_set_shader(d3d9_video_struct* d3d9, const char* shader_source)
    {
        if (!d3d9 || d3d9->magic != 0x39564433) {
            OutputDebugStringA("[ZeroMod] d3d9_gfx_set_shader: BAD MAGIC / null\n");
            return false;
        }

        // Clear shader (restore stock)
        if (!shader_source || !*shader_source) {
            OutputDebugStringA("[ZeroMod] d3d9_gfx_set_shader: clearing shader\n");

            zm_set_shader_path(d3d9, nullptr);

            if (d3d9->pixel_shader) {
                d3d9->pixel_shader->Release();
                d3d9->pixel_shader = nullptr;
            }
            return true;
        }

        // Prefer treating input as a PATH if it looks like one.
        if (zm_looks_like_path(shader_source) && !zm_looks_like_hlsl_source(shader_source)) {
            zm_set_shader_path(d3d9, shader_source);

            // IMPORTANT: do NOT compile here.
            // Slang layer will consume shader_path and build passes later.
            return true;
        }

        // Otherwise treat as literal HLSL source and compile like before
        OutputDebugStringA("[ZeroMod] d3d9_gfx_set_shader: treating input as literal HLSL source\n");

        ID3DXBuffer* shader_code = nullptr;
        ID3DXBuffer* error_msg = nullptr;

        HRESULT hr = D3DXCompileShader(
            shader_source,
            (UINT)strlen(shader_source),
            NULL, NULL,
            "main",
            "ps_3_0",
            D3DXSHADER_DEBUG,
            &shader_code,
            &error_msg,
            NULL
        );

        if (FAILED(hr)) {
            char b[256];
            _snprintf(b, sizeof(b), "[ZeroMod] D3DXCompileShader FAILED hr=0x%08lX\n", (unsigned long)hr);
            OutputDebugStringA(b);

            if (error_msg && error_msg->GetBufferPointer()) {
                OutputDebugStringA("[ZeroMod] D3DXCompileShader error:\n");
                OutputDebugStringA((char*)error_msg->GetBufferPointer());
                OutputDebugStringA("\n");
            }

            if (error_msg) error_msg->Release();
            if (shader_code) shader_code->Release();
            return false;
        }

        IDirect3DPixelShader9* new_shader = nullptr;
        hr = d3d9->dev->CreatePixelShader((DWORD*)shader_code->GetBufferPointer(), &new_shader);
        shader_code->Release();

        if (FAILED(hr)) {
            if (new_shader) new_shader->Release();
            return false;
        }

        if (d3d9->pixel_shader) d3d9->pixel_shader->Release();
        d3d9->pixel_shader = new_shader;

        // Since this was literal code, clear preset/path state
        zm_set_shader_path(d3d9, nullptr);
        d3d9->shader_is_path = false;
        d3d9->shader_reload_pending = false;

        return true;
    }

    void d3d9_hlsl_set_param_1f(void* fprg, IDirect3DDevice9* dev, const char* param, float* value) {
        // Set shader parameter 1f
    }

    void d3d9_hlsl_set_param_2f(void* fprg, IDirect3DDevice9* dev, const char* param, float* value) {
        // Set shader parameter 2f
    }

    void d3d9_vertex_buffer_free(IDirect3DVertexBuffer9* buffer, IDirect3DVertexDeclaration9* decl) {
        if (buffer) buffer->Release();
        if (decl) decl->Release();
    }

    void d3d9_texture_free(IDirect3DTexture9* texture) {
        if (texture) texture->Release();
    }

    bool d3d9_vertex_declaration_new(IDirect3DDevice9* dev, const D3DVERTEXELEMENT9* decl, void** vertex_decl) {
        if (FAILED(dev->CreateVertexDeclaration(decl, (IDirect3DVertexDeclaration9**)vertex_decl))) {
            return false;
        }
        return true;
    }

} // namespace ZeroMod
