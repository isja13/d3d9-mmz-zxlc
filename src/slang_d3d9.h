/*#pragma once
#include <d3d9.h>
#include "d3d9video.h"
#include <d3dx9shader.h>
#include "../retroarch/retroarch/gfx/drivers_shader/slang_process.h"

// Forward declare so this header doesn't drag in D3D9 headers everywhere.
//namespace ZeroMod { struct d3d9_video_struct; }

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

	bool slang_d3d9_apply_pass0(d3d9_video_struct* d3d9);

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

	// Allocate runtime object (no device calls yet)
	bool slang_d3d9_runtime_create(d3d9_video_struct* d3d9);

	// Free runtime object + any future COM resources
	void slang_d3d9_runtime_destroy(d3d9_video_struct* d3d9);

	// Called when parse-only has succeeded (d3d9->shader_preset == true)
	// This should "build" internal pass list from d3d9->shader.
	// For now: logs and marks runtime as ready.
	bool slang_d3d9_runtime_build_from_parsed(d3d9_video_struct* d3d9);

	// Per-frame tick hook (safe to call from Present)
	// For now: will only re-build if needed and emit once-per-change logs.
	void slang_d3d9_runtime_tick(d3d9_video_struct* d3d9);

} // namespace ZeroMod */
   /*   static void dump_cbuffer(const char* tag, const cbuffer_sem_t& cb)
       {
           // binding + uniform_count exist in your build (you already use cb.uniforms/cb.uniform_count elsewhere)
           zm_dbgf("[ZeroMod] CBUF %s: bind=%u uniforms=%d\n",
               tag,
               (unsigned)cb.binding,
               (int)cb.uniform_count);

           for (int i = 0; i < cb.uniform_count; i++)
           {
               const uniform_sem_t& u = cb.uniforms[i];

               // In your build: u.id/u.offset/u.size exist (stage_mask does NOT)
               if (!u.id[0])
                   continue;

               zm_dbgf("[ZeroMod]   [%02d] id='%s' off=%u size=%u\n",
                   i,
                   u.id,
                   (unsigned)u.offset,
                   (unsigned)u.size);
           }
       }

       static void dump_pass_semantics(const pass_semantics_t& sem)
       {
           zm_dbgf("[ZeroMod] ========= PASS SEMANTICS DUMP BEGIN =========\n");

           // --- textures ---
           for (int i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
           {
               const texture_sem_t& t = sem.textures[i];

               // minimal: show everything, don't try to be smart
               zm_dbgf("[ZeroMod] TEXSEM[%02d] bind=%u stage_mask=0x%X\n",
                   i,
                   (unsigned)t.binding,
                   (unsigned)t.stage_mask);
           }

           // --- only the cbuffers we know exist in your usage ---
           dump_cbuffer("UBO", sem.cbuffers[SLANG_CBUFFER_UBO]);
           dump_cbuffer("PUSH/PC", sem.cbuffers[SLANG_CBUFFER_PC]);

           zm_dbgf("[ZeroMod] ========= PASS SEMANTICS DUMP END =========\n");
       }

       static void dump_ct_regs(ID3DXConstantTable* ct, const char* tag)
       {
           if (!ct) { zm_dbgf("[ZeroMod] CT %s = null\n", tag); return; }

           D3DXCONSTANTTABLE_DESC td{};
           ct->GetDesc(&td);

           zm_dbgf("[ZeroMod] --- CT %s constants=%u ---\n", tag, (unsigned)td.Constants);

           for (UINT i = 0; i < td.Constants; i++)
           {
               D3DXHANDLE h = ct->GetConstant(NULL, i);
               if (!h) continue;

               D3DXCONSTANT_DESC cd{};
               UINT count = 1;
               if (FAILED(ct->GetConstantDesc(h, &cd, &count)) || !count)
                   continue;

               zm_dbgf("[ZeroMod]  %s: name='%s' regset=%u reg=%u count=%u bytes=%u class=%u type=%u\n",
                   tag,
                   cd.Name ? cd.Name : "(null)",
                   (unsigned)cd.RegisterSet,
                   (unsigned)cd.RegisterIndex,
                   (unsigned)cd.RegisterCount,
                   (unsigned)cd.Bytes,
                   (unsigned)cd.Class,
                   (unsigned)cd.Type);
           }
       }
           static void apply_sampler_slot(IDirect3DDevice9* dev, int s, unsigned pass_filter)
    {
        dev->SetSamplerState(s, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(s, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        dev->SetSamplerState(s, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);

      //  DWORD f = (pass_filter == RARCH_FILTER_LINEAR) ? D3DTEXF_LINEAR : D3DTEXF_POINT;
      //  dev->SetSamplerState(s, D3DSAMP_MINFILTER, D3DTEXF_POINT);
       // dev->SetSamplerState(s, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        dev->SetSamplerState(s, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    }
        static void log_rt_vp_scissor(IDirect3DDevice9* dev)
    {
        IDirect3DSurface9* rt0 = nullptr;
        HRESULT hr = dev->GetRenderTarget(0, &rt0);
        D3DSURFACE_DESC rd{};
        if (SUCCEEDED(hr) && rt0)
            rt0->GetDesc(&rd);

        D3DVIEWPORT9 vp{};
        dev->GetViewport(&vp);

        RECT sc{};
        BOOL sc_en = FALSE;
        dev->GetRenderState(D3DRS_SCISSORTESTENABLE, (DWORD*)&sc_en);
        dev->GetScissorRect(&sc);

        zm_dbgf("[ZeroMod] RT0=%p %ux%u fmt=%u | VP x=%u y=%u w=%u h=%u | SCEN=%d sc=[%ld,%ld..%ld,%ld]\n",
            rt0, rd.Width, rd.Height, (unsigned)rd.Format,
            (unsigned)vp.X, (unsigned)vp.Y, (unsigned)vp.Width, (unsigned)vp.Height,
            (int)sc_en, sc.left, sc.top, sc.right, sc.bottom);

        if (rt0) rt0->Release();
    }

       */


