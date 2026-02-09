#pragma once
#include <windows.h>
#include <d3d9.h>
#include "d3d9video.h"

void librashader_init_once(HMODULE proxy_module);
bool librashader_is_loaded();
const struct libra_instance_t* librashader_api();

namespace ZeroMod {

    bool librashader_d3d9_load_preset(d3d9_video_struct* d3d9, const char* preset_path);
    bool librashader_d3d9_frame(
        d3d9_video_struct* d3d9,
        IDirect3DTexture9* src_tex,
        IDirect3DSurface9* dst_rtv,
        UINT out_w, UINT out_h,
        UINT vp_w, UINT vp_h,
        UINT64 frame_count);

    void librashader_d3d9_runtime_destroy(d3d9_video_struct* d3d9);

} // namespace ZeroMod