#include "d3d9video.h"
//#include "video_driver.h"
#include "retroarch.h"
#include <d3dx9.h>
#include "log.h"
#include <vector>
#include <string>
#include <d3d9.h>
#include <d3d9types.h>

namespace ZeroMod {

    d3d9_video_struct* d3d9_gfx_init(IDirect3DDevice9* device, D3DFORMAT format) {
        d3d9_video_struct* d3d9 = (d3d9_video_struct*)calloc(1, sizeof(d3d9_video_struct));
        if (!d3d9) return NULL;

        d3d9->dev = device;
        d3d9->final_viewport.Width = 4;
        d3d9->final_viewport.Height = 4;

        D3DXMatrixOrthoOffCenterLH((D3DXMATRIX*)&d3d9->mvp, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

        struct Vertex {
            float x, y, z, w;
            float u, v;
        };

        Vertex vertices[] = {
            { 0.0f, 0.0f, 0.5f, 1.0f, 0.0f, 1.0f },
            { 0.0f, 1.0f, 0.5f, 1.0f, 0.0f, 0.0f },
            { 1.0f, 0.0f, 0.5f, 1.0f, 1.0f, 1.0f },
            { 1.0f, 1.0f, 0.5f, 1.0f, 1.0f, 0.0f }
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

        // Initialize vertex declaration
        D3DVERTEXELEMENT9 decl[] = {
            { 0, offsetof(Vertex, x), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, offsetof(Vertex, u), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
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

    void d3d9_gfx_free(d3d9_video_struct* d3d9) {
        if (!d3d9) return;

        if (d3d9->frame_vbo) d3d9->frame_vbo->Release();
        if (d3d9->vertex_decl) d3d9->vertex_decl->Release();
        if (d3d9->pixel_shader) d3d9->pixel_shader->Release();
        free(d3d9);
    }

    bool d3d9_gfx_frame(d3d9_video_struct* d3d9, IDirect3DTexture9* texture, UINT64 frame_count) {
        // Implement the logic to render a frame
        return true;
    }

    void d3d9_update_viewport(d3d9_video_struct* d3d9, IDirect3DSurface9* renderTargetView, video_viewport_t* viewport) {
        d3d9->renderTargetView = renderTargetView;
        d3d9->vp = *viewport;

        d3d9->final_viewport.X = d3d9->vp.x;
        d3d9->final_viewport.Y = d3d9->vp.y;
        d3d9->final_viewport.Width = d3d9->vp.width;
        d3d9->final_viewport.Height = d3d9->vp.height;
        d3d9->final_viewport.MinZ = 0.0f;
        d3d9->final_viewport.MaxZ = 1.0f;

        d3d9->dev->SetViewport(&d3d9->final_viewport);

        if (d3d9->shader_preset &&
            (static_cast<int>(d3d9->final_viewport.Width) != d3d9->vp.width ||
                static_cast<int>(d3d9->final_viewport.Height) != d3d9->vp.height)) {
            // Handle shader resizing if needed
        }
    }


    bool d3d9_gfx_set_shader(d3d9_video_struct* d3d9, const char* shader_source) {
        if (!shader_source) {
            if (d3d9->pixel_shader) {
                d3d9->pixel_shader->Release();
                d3d9->pixel_shader = nullptr;
            }
            return true;
        }

        // Compile the shader source
        ID3DXBuffer* shader_code = nullptr;
        ID3DXBuffer* error_msg = nullptr;
        HRESULT hr = D3DXCompileShader(
            shader_source,
            strlen(shader_source),
            NULL, NULL,
            "main",
            "ps_3_0",
            D3DXSHADER_DEBUG,
            &shader_code,
            &error_msg,
            NULL
        );

        if (FAILED(hr)) {
            if (error_msg) {
                // LOG_ERROR((char*)error_msg->GetBufferPointer());
                error_msg->Release();
            }
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
