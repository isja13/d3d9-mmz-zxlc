#ifndef D3D9VIDEO_H
#define D3D9VIDEO_H

#include "d3d9_common.h"
#include "../RetroArch/RetroArch/libretro-common/include/gfx/math/matrix_4x4.h"

namespace ZeroMod {

    struct video_viewport_t {
        int x;
        int y;
        int width;
        int height;
        int full_width;
        int full_height;
    };

    struct d3d9_video_struct {
        bool keep_aspect;
        bool should_resize;
        bool quitting;
        bool needs_restore;
        bool overlays_enabled;
        bool resolution_hd_enable;
        bool widescreen_mode;

        unsigned cur_mon_id;
        unsigned dev_rotation;

        overlay_t* menu;
        const d3d9_renderchain_driver_t* renderchain_driver;
        void* renderchain_data;

        RECT font_rect;
        RECT font_rect_shifted;
        math_matrix_4x4 mvp;
        math_matrix_4x4 mvp_rotate;
        math_matrix_4x4 mvp_transposed;

        struct video_viewport_t vp;
        struct video_shader shader;
        video_info_t video_info;
        LPDIRECT3DDEVICE9 dev;
        D3DVIEWPORT9 final_viewport;
        IDirect3DSurface9* renderTargetView;

        char* shader_path;

        struct {
            int size;
            int offset;
            void* buffer;
            void* decl;
        } menu_display;

        size_t overlays_size;
        overlay_t* overlays;

        LPDIRECT3DVERTEXBUFFER9 frame_vbo;
        LPDIRECT3DVERTEXDECLARATION9 vertex_decl;
        IDirect3DPixelShader9* pixel_shader;

        bool shader_preset;
    };

    d3d9_video_struct* d3d9_gfx_init(IDirect3DDevice9* device, D3DFORMAT format);
    void d3d9_gfx_free(d3d9_video_struct* d3d9);
    bool d3d9_gfx_frame(d3d9_video_struct* d3d9, IDirect3DTexture9* texture, UINT64 frame_count);
    void d3d9_update_viewport(d3d9_video_struct* d3d9, IDirect3DSurface9* renderTargetView, video_viewport_t* viewport);
    bool d3d9_gfx_set_shader(d3d9_video_struct* d3d9, const char* shader_source);

    // Additional function declarations
    void d3d9_hlsl_set_param_1f(void* fprg, IDirect3DDevice9* dev, const char* param, float* value);
    void d3d9_hlsl_set_param_2f(void* fprg, IDirect3DDevice9* dev, const char* param, float* value);
    void d3d9_vertex_buffer_free(IDirect3DVertexBuffer9* buffer, IDirect3DVertexDeclaration9* decl);
    void d3d9_texture_free(IDirect3DTexture9* texture);
    bool d3d9_vertex_declaration_new(IDirect3DDevice9* dev, const D3DVERTEXELEMENT9* decl, void** vertex_decl);

} // namespace ZeroMod

typedef ZeroMod::d3d9_video_struct ZeroMod_d3d9_video_t;  // Ensure the typedef is defined

#endif // D3D9VIDEO_H
