#include "d3d9video.h"
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
#include "cg_preset.h"

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

        d3d9->magic = 0x39564433;
        d3d9->dev = device;
        d3d9->final_viewport.Width = 4;
        d3d9->final_viewport.Height = 4;
        d3d9->shader_path = nullptr;
        d3d9->shader_preset = false;            
        d3d9->shader_reload_pending = false;    
        d3d9->shader_is_path = false;           

        zm_cg_vertex vertices[4] = {
            { -1,  1, 0, 1,   1,1,1,1,   0,0 }, // TL
            { -1, -1, 0, 1,   1,1,1,1,   0,1 }, // BL
            {  1,  1, 0, 1,   1,1,1,1,   1,0 }, // TR
            {  1, -1, 0, 1,   1,1,1,1,   1,1 }, // BR
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

        D3DVERTEXELEMENT9 decl[] = {
            // stream, offset, type, method, usage, usageIndex
            { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
            { 0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
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

        //ZeroMod::librashader_d3d9_runtime_destroy(d3d9);

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
            OutputDebugStringA("[ZeroMod] d3d9_gfx_frame: CGP reload pending\n");

            bool ok = ZeroMod::cg_d3d9_load_cgp_preset(d3d9, d3d9->shader_path);
            if (!ok)
                OutputDebugStringA("[ZeroMod] d3d9_gfx_frame: CGP load failed\n");
        }
        d3d9->shader_reload_pending = false;
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

    bool d3d9_gfx_set_shader(d3d9_video_struct* d3d9, const char* shader_source)
    {
        if (!d3d9 || d3d9->magic != 0x39564433) {
            OutputDebugStringA("[ZeroMod] d3d9_gfx_set_shader: BAD MAGIC / null\n");
            return false;
        }

        // Clear shader (restore stock / no CGP)
        if (!shader_source || !*shader_source) {
            OutputDebugStringA("[ZeroMod] d3d9_gfx_set_shader: clearing shader\n");
            zm_set_shader_path(d3d9, nullptr);

            // Optional: if you have active Cg programs, free them here.
            // ZeroMod::cg_d3d9_unload(d3ZeroMod::...;
            return true;
        }

        // CGP-only: always treat as a filesystem path to a preset
        zm_set_shader_path(d3d9, shader_source);

        // optional status flag if you want to keep it
        d3d9->shader_preset = true;

        OutputDebugStringA("[ZeroMod] d3d9_gfx_set_shader: queued CGP preset reload\n");
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
