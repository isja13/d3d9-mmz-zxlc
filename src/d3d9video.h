#ifndef D3D9VIDEO_H
#define D3D9VIDEO_H

#include "d3d9_common.h"
#include "../RetroArch/RetroArch/libretro-common/include/gfx/math/matrix_4x4.h"
#include "../Cg/cg.h"
#ifdef __cplusplus
#include <string>
#include <vector>
#endif

namespace ZeroMod {

    struct zm_cg_vertex {
        float x, y, z, w;   // POSITION
        float r, g, b, a;   // COLOR
        float u, v;         // TEXCOORD0
    };

   // static_assert(sizeof(zm_slang_vertex) == 24, "zm_slang_vertex size mismatch");

    struct video_viewport_t {
        int x;
        int y;
        int width;
        int height;
        int full_width;
        int full_height;
    };

    struct cg_pass_rt {
        CGprogram vprog = nullptr;
        CGprogram pprog = nullptr;

        // cached params (optional but recommended)
        CGparameter p_mvp = nullptr;
        CGparameter p_in_video = nullptr;
        CGparameter p_in_tex = nullptr;
        CGparameter p_out = nullptr;
        CGparameter p_framecount = nullptr; // try name variants later

        // primary sampler (decal/ORIG/PREV)
        CGparameter p_decal = nullptr;

        // output RT for this pass (null for final pass rendering to backbuffer)
        IDirect3DTexture9* rt_tex = nullptr;
        IDirect3DSurface9* rt_surf = nullptr;
        UINT rt_w = 0, rt_h = 0;

        // parsed config
        bool filter_linear = false;
        enum { SCALE_SOURCE, SCALE_VIEWPORT } scale_type = SCALE_SOURCE;
        float scale = 1.0f;
#ifdef __cplusplus
        struct cg_param_rt {
            std::string key_lower;  // for matching cgp overrides (lowercased)
            std::string cg_name;    // exact cg param name (as in shader)
            float value = 0.0f;
            CGparameter h = nullptr;
        };
        std::vector<cg_param_rt> params;
#endif
    };

    struct cg_chain_rt {
        bool active = false;
        int num_passes = 0;
        cg_pass_rt* passes = nullptr;

        // hold onto preset_dir if you need LUTs later
        char* preset_dir = nullptr;
    };

    struct d3d9_video_struct {

        uint32_t magic;

        bool keep_aspect;
        bool should_resize;
        bool quitting;
        bool needs_restore;
        bool overlays_enabled;
        bool resolution_hd_enable;
        bool widescreen_mode;

        void* libra_rt;   // opaque runtime owned by librashader module

        UINT quad_w = 0;
        UINT quad_h = 0;

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

        CGprogram cg_vprog = nullptr;
        CGprogram cg_pprog = nullptr;
        bool      cg_active = false;
        // NEW: minimal state for "path-based shader pipeline"
        bool shader_reload_pending;   // set when shader_path changes; slang layer will consume
        bool shader_is_path;          // true if last set_shader input was a filesystem path

        UINT last_texW = 0;
        UINT last_texH = 0;
        bool last_zx;

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

        cg_chain_rt cg;

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
