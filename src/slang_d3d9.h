#pragma once
#include <d3d9.h>
#include "d3d9video.h"
#include <d3dx9shader.h>
#include "../retroarch/retroarch/gfx/drivers_shader/slang_process.h"

extern IDirect3DSurface9* last_slang_rtv;

namespace ZeroMod {

	bool is_zx();

	void slang_d3d9_set_frame_ctx(
        d3d9_video_struct* d3d9,
        IDirect3DTexture9* src_tex,
        IDirect3DSurface9* dst_rtv,
        UINT dst_w,
        UINT dst_h,
        UINT vp_x,
        UINT vp_y,
        UINT64 frame_count);

	struct slang_d3d9_runtime;

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
	);

	struct d3d9_slang_pass
	{
		IDirect3DVertexShader9* vs = nullptr;
		IDirect3DPixelShader9* ps = nullptr;

		ID3DXConstantTable* vs_ct = nullptr;
		ID3DXConstantTable* ps_ct = nullptr;

		IDirect3DTexture9* rt = nullptr;
		IDirect3DSurface9* rt_surf = nullptr;

		char* hlsl_vs = nullptr;
		char* hlsl_ps = nullptr;

		bool compiled = false;
		D3DVIEWPORT9 vp = {};

		pass_semantics_t sem = {};
		bool sem_valid = false;
        int  source_sampler_reg;   // -1 = unknown
        bool tl_inited;
	};

	// Allocate runtime object (no device calls)
	bool slang_d3d9_runtime_create(d3d9_video_struct* d3d9);

	// Free runtime object + any future COM resources
	void slang_d3d9_runtime_destroy(d3d9_video_struct* d3d9);

	// Called when parse-only has succeeded (d3d9->shader_preset == true)
	// This should "build" internal pass list from d3d9->shader.
	// Logs and marks runtime as ready.
	bool slang_d3d9_runtime_build_from_parsed(d3d9_video_struct* d3d9);

	// Per-frame tick hook (Call from Present)
	// Will only re-build and emit once-per-change logs.
	void slang_d3d9_runtime_tick(d3d9_video_struct* d3d9);

} // namespace ZeroMod


