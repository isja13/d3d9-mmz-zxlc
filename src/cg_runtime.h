#pragma once

#include <d3d9.h>
#include "d3d9video.h"

bool        cg_runtime_init_once();  // caches latest D3D9 CG profiles
const char* cg_runtime_last_error();

// Optional reset hooks (stubs for now; safe to call)
void cg_runtime_on_reset_pre();
void cg_runtime_on_reset_post(IDirect3DDevice9* dev);
namespace ZeroMod {
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
    );
}