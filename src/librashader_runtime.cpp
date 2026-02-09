// librashader_runtime.cpp
#include <windows.h>
#include <string>
#include <mutex>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define LIBRA_RUNTIME_D3D9
#include "../libraShader/librashader.h"
#include "../libraShader/librashader_ld.h"
#include "librashader_runtime.h" 

// One-time init guard
static std::once_flag g_libra_once;
static libra_instance_t g_libra{};
static bool g_libra_loaded = false;
static const float* librashader_default_mvp()
{
    // From librashader docs: final pass quad [0,1] uses this projection by default.
    // Layout exactly as published.
    static const float kMvp[16] = {
        2.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
       -1.0f,-1.0f, 0.0f, 1.0f,
    };
    return kMvp;
}
static std::wstring get_module_dir_w(HMODULE mod)
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(mod, path, MAX_PATH);

    wchar_t* last = wcsrchr(path, L'\\');
    if (last) *last = 0;

    return std::wstring(path);
}

static void dbg_print(const char* msg)
{
    OutputDebugStringA(msg);
    printf("%s", msg);
}

void librashader_init_once(HMODULE proxy_module)
{
    std::call_once(g_libra_once, [&] {
        // 1) Preload librashader.dll by absolute path (avoids DLL search path issues)
        std::wstring dir = get_module_dir_w(proxy_module);
        std::wstring dllPath = dir + L"\\librashader.dll";

        HMODULE h = LoadLibraryW(dllPath.c_str());
        if (!h)
        {
            DWORD err = GetLastError();
            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod] LoadLibraryW(librashader.dll) FAILED. err=%lu\n", (unsigned long)err);
            dbg_print(b);
            g_libra_loaded = false;
            return;
        }

        dbg_print("[ZeroMod] Preloaded librashader.dll OK\n");

        // 2) Now populate the API table from the loader header
        g_libra = librashader_load_instance();

        // 3) Validate load + ABI
        size_t abi = g_libra.instance_abi_version ? g_libra.instance_abi_version() : 0;
        size_t api = g_libra.instance_api_version ? g_libra.instance_api_version() : 0;

        g_libra_loaded = (g_libra.instance_loaded != 0) && (abi == LIBRASHADER_CURRENT_ABI);

        unsigned long long abi_ull = (unsigned long long)abi;
        unsigned long long api_ull = (unsigned long long)api;
        int expected = (int)LIBRASHADER_CURRENT_ABI;

        char b[256];
        _snprintf(b, sizeof(b),
            "[ZeroMod] librashader_load_instance: loaded=%d abi=%llu expected=%d api=%llu\n",
            (int)g_libra_loaded, abi_ull, expected, api_ull);
        dbg_print(b);
        });
}

bool librashader_is_loaded()
{
    return g_libra_loaded;
}

const libra_instance_t* librashader_api()
{
    return &g_libra;
}

namespace ZeroMod {

    struct LibraD3D9Runtime
    {
        libra_d3d9_filter_chain_t chain = nullptr;
    };

    static void dbgA(const char* s)
    {
        OutputDebugStringA(s);
    }

    static void dump_libra_error(const libra_instance_t* api, libra_error_t err, const char* tag)
    {
        if (!api || !err) return;

        char b[256];
        int eno = api->error_errno ? (int)api->error_errno(err) : -1;
        _snprintf(b, sizeof(b), "[ZeroMod] %s: librashader error errno=%d\n", tag, eno);
        dbgA(b);

        if (api->error_write && api->error_free_string)
        {
            char* msg = nullptr;
            if (api->error_write(err, &msg) == 0 && msg)
            {
                dbgA("[ZeroMod] librashader error msg:\n");
                dbgA(msg);
                dbgA("\n");
                api->error_free_string(&msg);
            }
        }
        else if (api->error_print)
        {
            api->error_print(err);
        }

        if (api->error_free)
        {
            // error_free takes a pointer-to-error handle
            api->error_free(&err);
        }
    }

    //static const float* identity_mvp()
  //  {
      //  static const float I[16] = {
         //   1,0,0,0,
        //    0,1,0,0,
        //    0,0,1,0,
         //   0,0,0,1
      //  };
      //  return I;
   // }

    void librashader_d3d9_runtime_destroy(d3d9_video_struct* d3d9)
    {
        if (!d3d9) return;

        LibraD3D9Runtime* rt = (LibraD3D9Runtime*)d3d9->libra_rt;
        if (!rt) return;

        const libra_instance_t* api = librashader_api();
        if (librashader_is_loaded() && api && api->d3d9_filter_chain_free)
        {
            libra_error_t err = api->d3d9_filter_chain_free(&rt->chain);
            if (err) dump_libra_error(api, err, "d3d9_filter_chain_free");
        }

        rt->chain = nullptr;
        free(rt);
        d3d9->libra_rt = nullptr;

        dbgA("[ZeroMod] librashader_d3d9_runtime_destroy: done\n");
    }

    bool librashader_d3d9_load_preset(d3d9_video_struct* d3d9, const char* preset_path)
    {
        if (!d3d9 || !preset_path || !*preset_path)
            return false;

        if (!librashader_is_loaded())
        {
            dbgA("[ZeroMod] librashader_d3d9_load_preset: librashader not loaded\n");
            return false;
        }

        const libra_instance_t* api = librashader_api();
        if (!api)
            return false;

        if (!api->preset_ctx_create || !api->preset_ctx_set_runtime || !api->preset_create_with_options ||
            !api->preset_free || !api->d3d9_filter_chain_create)
        {
            dbgA("[ZeroMod] librashader_d3d9_load_preset: missing required API functions\n");
            return false;
        }

        // Blow away any existing chain first.
        librashader_d3d9_runtime_destroy(d3d9);

        LibraD3D9Runtime* rt = (LibraD3D9Runtime*)calloc(1, sizeof(LibraD3D9Runtime));
        if (!rt) return false;

        // Build a preset context and force runtime to D3D9 HLSL.
        libra_preset_ctx_t ctx = nullptr;
        libra_error_t err = api->preset_ctx_create(&ctx);
        if (err)
        {
            dump_libra_error(api, err, "preset_ctx_create");
            free(rt);
            return false;
        }

        err = api->preset_ctx_set_runtime(&ctx, LIBRA_PRESET_CTX_RUNTIME_D3D9_HLSL);
        if (err)
        {
            dump_libra_error(api, err, "preset_ctx_set_runtime(D3D9_HLSL)");
            if (api->preset_ctx_free) api->preset_ctx_free(&ctx);
            free(rt);
            return false;
        }

        libra_preset_opt_t popt{};
        popt.version = LIBRASHADER_CURRENT_VERSION;
        popt.original_aspect_uniforms = false; // can enable later if needed
        popt.frametime_uniforms = false;

        libra_shader_preset_t preset = nullptr;
        err = api->preset_create_with_options(preset_path, &ctx, &popt, &preset);
        // NOTE: ctx is invalidated by create_with_options per docs; don’t free it afterwards.

        if (err || !preset)
        {
            dump_libra_error(api, err, "preset_create_with_options");
            free(rt);
            return false;
        }

        filter_chain_d3d9_opt_t fcopt{};
        fcopt.version = LIBRASHADER_CURRENT_VERSION;
        fcopt.force_no_mipmaps = false;
        fcopt.disable_cache = false;

        err = api->d3d9_filter_chain_create(&preset, d3d9->dev, &fcopt, &rt->chain);

        // Preset can be freed once chain is created.
        {
            libra_error_t err2 = api->preset_free(&preset);
            if (err2) dump_libra_error(api, err2, "preset_free");
        }

        if (err || !rt->chain)
        {
            dump_libra_error(api, err, "d3d9_filter_chain_create");
            free(rt);
            return false;
        }

        d3d9->libra_rt = rt;

        dbgA("[ZeroMod] librashader_d3d9_load_preset: chain created OK\n");
        return true;
    }
    static bool zm_make_viewport_from_dst(
        IDirect3DSurface9* dst_rtv,
        UINT& out_x, UINT& out_y, UINT& out_w, UINT& out_h
    ) {
        if (!dst_rtv) return false;

        D3DSURFACE_DESC d{};
        if (FAILED(dst_rtv->GetDesc(&d))) return false;
        if (!d.Width || !d.Height) return false;

        out_x = 0;
        out_y = 0;
        out_w = (UINT)d.Width;
        out_h = (UINT)d.Height;
        return true;
    }
    bool librashader_d3d9_frame(
        d3d9_video_struct* d3d9,
        IDirect3DTexture9* src_tex,
        IDirect3DSurface9* dst_rtv,
        UINT vp_x, UINT vp_y,
        UINT vp_w, UINT vp_h,
        UINT64 frame_count)
    {
     //   (void)out_w; (void)out_h; // out_w/out_h not directly needed by API (dst surface implies it)
        if (!d3d9 || !src_tex || !dst_rtv)
            return false;

        if (!librashader_is_loaded())
            return false;
        HRESULT hr_tc = d3d9->dev->TestCooperativeLevel();
        {
            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod][LIBRA] TestCooperativeLevel hr=0x%08lX\n",
                (unsigned long)hr_tc);
            OutputDebugStringA(b);
        }
        if (hr_tc != D3D_OK) return false;

        const libra_instance_t* api = librashader_api();
        if (!api || !api->d3d9_filter_chain_frame)
            return false;

        LibraD3D9Runtime* rt = (LibraD3D9Runtime*)d3d9->libra_rt;
        if (!rt || !rt->chain)
            return false;

        UINT fx = vp_x;
        UINT fy = vp_y;
        UINT fw = vp_w;
        UINT fh = vp_h;

        // HARD SAFETY: if incoming VP is invalid, derive from destination RTV
        if (!fw || !fh) {
            if (!zm_make_viewport_from_dst(dst_rtv, fx, fy, fw, fh))
                return false;
        }

        // (Optional but recommended) sanity log
        {
            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod][VPFIX] librashader vp from rtv: x=%u y=%u w=%u h=%u\n",
                (unsigned)fx, (unsigned)fy, (unsigned)fw, (unsigned)fh);
            OutputDebugStringA(b);
        }
        // --- HARD FORCE device viewport to match destination (safe: Present() wraps with stateblock) ---
        // This fixes the classic "quarter screen" symptom when device viewport is still the game's 256x192.
        {
            D3DVIEWPORT9 d3dvp{};
            d3dvp.X = (DWORD)fx;
            d3dvp.Y = (DWORD)fy;
            d3dvp.Width  = (DWORD)fw;
            d3dvp.Height = (DWORD)fh;
            d3dvp.MinZ = 0.0f;
            d3dvp.MaxZ = 1.0f;

            HRESULT hr_vp = d3d9->dev->SetViewport(&d3dvp);

            // Disable scissor; stale scissor can also cause "quarter render".
            HRESULT hr_sc = d3d9->dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

            // Optional: set scissor rect to full vp anyway (harmless even if scissor disabled).
            RECT sr{ (LONG)fx, (LONG)fy, (LONG)(fx + fw), (LONG)(fy + fh) };
            HRESULT hr_sr = d3d9->dev->SetScissorRect(&sr);

            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod][VPFORCE] SetViewport hr=0x%08lX SetScissorEnable hr=0x%08lX SetScissorRect hr=0x%08lX (x=%u y=%u w=%u h=%u)\n",
                (unsigned long)hr_vp, (unsigned long)hr_sc, (unsigned long)hr_sr,
                (unsigned)fx, (unsigned)fy, (unsigned)fw, (unsigned)fh);
            OutputDebugStringA(b);
        }
        {
            D3DSURFACE_DESC dd{};
            HRESULT hr_dd = dst_rtv->GetDesc(&dd);

            IDirect3DSurface9* rt0 = nullptr;
            HRESULT hr_rt0 = d3d9->dev->GetRenderTarget(0, &rt0);

            D3DSURFACE_DESC rd{};
            HRESULT hr_rd = rt0 ? rt0->GetDesc(&rd) : E_FAIL;

            char b[512];
            _snprintf(b, sizeof(b),
                "[ZeroMod][LIBRA] dst_rtv=%p GetDesc hr=0x%08lX %ux%u fmt=%u | RT0=%p GetRT0 hr=0x%08lX desc hr=0x%08lX %ux%u fmt=%u\n",
                (void*)dst_rtv, (unsigned long)hr_dd,
                (unsigned)(hr_dd == D3D_OK ? dd.Width : 0), (unsigned)(hr_dd == D3D_OK ? dd.Height : 0), (unsigned)(hr_dd == D3D_OK ? dd.Format : 0),
                (void*)rt0, (unsigned long)hr_rt0, (unsigned long)hr_rd,
                (unsigned)(hr_rd == D3D_OK ? rd.Width : 0), (unsigned)(hr_rd == D3D_OK ? rd.Height : 0), (unsigned)(hr_rd == D3D_OK ? rd.Format : 0)
            );
            OutputDebugStringA(b);

            if (rt0) rt0->Release();
        }

        libra_viewport_t vp{};
        vp.x = (float)fx;
        vp.y = (float)fy;
        vp.width = (uint32_t)fw;
        vp.height = (uint32_t)fh;

        const libra_viewport_t* vp_ptr = &vp;
        frame_d3d9_opt_t fopt{};
        fopt.version = LIBRASHADER_CURRENT_VERSION;
        fopt.clear_history = false;
        fopt.frame_direction = 1;
        fopt.rotation = 0;
        fopt.total_subframes = 1;
        fopt.current_subframe = 1;
        fopt.aspect_ratio = 0.0f;
        fopt.frames_per_second = 1.0f;
        fopt.frametime_delta = 0;

        libra_error_t err = api->d3d9_filter_chain_frame(
            &rt->chain,
            (size_t)frame_count,
            src_tex,
            dst_rtv,
            vp_ptr,
            librashader_default_mvp(),
            &fopt
        );
        {
            IDirect3DSurface9* rt0 = nullptr;
            HRESULT hr_rt0 = d3d9->dev->GetRenderTarget(0, &rt0);

            D3DSURFACE_DESC rd{};
            HRESULT hr_rd = rt0 ? rt0->GetDesc(&rd) : E_FAIL;

            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod][LIBRA] AFTER: GetRT0 hr=0x%08lX rt0=%p desc hr=0x%08lX %ux%u fmt=%u\n",
                (unsigned long)hr_rt0, (void*)rt0, (unsigned long)hr_rd,
                (unsigned)(hr_rd == D3D_OK ? rd.Width : 0), (unsigned)(hr_rd == D3D_OK ? rd.Height : 0), (unsigned)(hr_rd == D3D_OK ? rd.Format : 0)
            );
            OutputDebugStringA(b);

            if (rt0) rt0->Release();
        }

        if (err) {
            dump_libra_error(api, err, "d3d9_filter_chain_frame");
            return false;
        }
        return true;
    }

} // namespace ZeroMod
