#include "dinput8_dll.h"
#include "dxgiswapchain.h"
#include "directinput8a.h"
#include "overlay.h"
#include "conf.h"
#include "log.h"
#include "d3d9device.h"
#include "../minhook/include/MinHook.h"
#include <windows.h>
#include <d3d9.h>
#include <d3d9types.h>
#include <cstdio>
#include <tchar.h>
#include <string.h>
#include "globals.h"
#include "librashader_runtime.h"

#include <windows.h>
#define DBG(s) OutputDebugStringA("[ZeroMod] " s "\n")

extern HMODULE g_hThisModule;

// --- Forwarded exports from system dinput8.dll ---
DirectInput8Create_t   pDirectInput8Create = nullptr;
DllCanUnloadNow_t      pDllCanUnloadNow = nullptr;
DllGetClassObject_t    pDllGetClassObject = nullptr;
DllRegisterServer_t    pDllRegisterServer = nullptr;
DllUnregisterServer_t  pDllUnregisterServer = nullptr;
GetdfDIJoystick_t      pGetdfDIJoystick = nullptr;

// (these exist in your project; leaving them as-is)
IDirect3D9* pD3D9 = nullptr;
IDirect3DDevice9* pD3D9Device = nullptr;

static bool MinHook_Initialized = false;


static bool g_d3d9_caps_hooks_installed = false;



namespace {

    // Helpers
    static inline void zm_log_hr_any(const char* tag, HRESULT hr)
    {
        if (hr != D3D_OK) {
            char b[256];
            _snprintf(b, sizeof(b), "[ZeroMod] HR %s = 0x%08lX\n", tag, (unsigned long)hr);
            OutputDebugStringA(b);
            printf("[ZeroMod] HR %s = 0x%08lX\n", tag, (unsigned long)hr);
        }
    }


    HMODULE base_dll = nullptr;

    // ===== IDirect3D9 capability/format hooks (surgical) =====

    using CheckDeviceType_t = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DevType,
        D3DFORMAT AdapterFormat,
        D3DFORMAT BackBufferFormat,
        BOOL bWindowed
        );

    using CheckDeviceFormat_t = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        D3DFORMAT AdapterFormat,
        DWORD Usage,
        D3DRESOURCETYPE RType,
        D3DFORMAT CheckFormat
        );

    using CheckDeviceMultiSampleType_t = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        D3DFORMAT SurfaceFormat,
        BOOL Windowed,
        D3DMULTISAMPLE_TYPE MultiSampleType,
        DWORD* pQualityLevels
        );

    using CheckDepthStencilMatch_t = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        D3DFORMAT AdapterFormat,
        D3DFORMAT RenderTargetFormat,
        D3DFORMAT DepthStencilFormat
        );

    using CheckDeviceFormatConversion_t = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        D3DFORMAT SourceFormat,
        D3DFORMAT TargetFormat
        );

    using GetDeviceCaps_t = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        D3DCAPS9* pCaps
        );

    // MinHook trampolines
    static CheckDeviceType_t              g_CheckDeviceType_Orig = nullptr;
    static CheckDeviceFormat_t            g_CheckDeviceFormat_Orig = nullptr;
    static CheckDeviceMultiSampleType_t   g_CheckDeviceMultiSampleType_Orig = nullptr;
    static CheckDepthStencilMatch_t       g_CheckDepthStencilMatch_Orig = nullptr;
    static CheckDeviceFormatConversion_t  g_CheckDeviceFormatConversion_Orig = nullptr;
    static GetDeviceCaps_t                g_GetDeviceCaps_Orig = nullptr;


    // -------------------------
    // Hook: Direct3DCreate9
    // -------------------------
    using Direct3DCreate9_t = IDirect3D9 * (WINAPI*)(UINT);
    static Direct3DCreate9_t g_Direct3DCreate9_Orig = nullptr;

    // -------------------------
    // Hook: IDirect3D9::CreateDevice
    // -------------------------
    using CreateDevice_t = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        HWND hFocusWindow,
        DWORD BehaviorFlags,
        D3DPRESENT_PARAMETERS* pPresentationParameters,
        IDirect3DDevice9** ppReturnedDeviceInterface
        );

    static CreateDevice_t g_CreateDevice_Target = nullptr; // vtbl slot
    static CreateDevice_t g_CreateDevice_Orig = nullptr; // MinHook trampoline

    // --- vtbl[16] hook (Reset) ---
    using DeviceReset_t = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3DDevice9* self,
        D3DPRESENT_PARAMETERS* pp
        );

    static DeviceReset_t g_DeviceReset_Orig = nullptr;
    static DeviceReset_t g_DeviceReset_Target = nullptr;

    static MyID3D9Device* g_wrap = nullptr;


    static void dump_pp_all(const D3DPRESENT_PARAMETERS* pp, const char* tag)
    {
        if (!pp) { printf("[PP %s] pp=null\n", tag); return; }

        printf(
            "[PP %s] BB=%ux%u Count=%u Fmt=%u Win=%u Swap=%u Flags=0x%08lX "
            "hDevWnd=%p AutoDS=%u DSFmt=%u MS=%u MSQ=%lu Interval=0x%08lX RR=%u\n",
            tag,
            (unsigned)pp->BackBufferWidth,
            (unsigned)pp->BackBufferHeight,
            (unsigned)pp->BackBufferCount,
            (unsigned)pp->BackBufferFormat,
            (unsigned)pp->Windowed,
            (unsigned)pp->SwapEffect,
            (unsigned long)pp->Flags,
            (void*)pp->hDeviceWindow,
            (unsigned)pp->EnableAutoDepthStencil,
            (unsigned)pp->AutoDepthStencilFormat,
            (unsigned)pp->MultiSampleType,
            (unsigned long)pp->MultiSampleQuality,
            (unsigned long)pp->PresentationInterval,
            (unsigned)pp->FullScreen_RefreshRateInHz
        );
    }

   // static void sanitize_pp_windowed(D3DPRESENT_PARAMETERS* pp)
  //  {
  //      if (!pp) return;

        // Windowed reset rules that frequently trip INVALIDCALL on some drivers
  //      if (pp->Windowed) {
  //          pp->FullScreen_RefreshRateInHz = 0;
   //         pp->BackBufferFormat = D3DFMT_UNKNOWN;
  //      }
 //   }



    // -------------------------
    // Hook: IDirect3DDevice9::Present
    // -------------------------
    using DevicePresent_t = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3DDevice9* self,
        const RECT* src_rect,
        const RECT* dst_rect,
        HWND dst_window_override,
        const RGNDATA* dirty_region
        );

    static DevicePresent_t g_DevicePresent_Target = nullptr; // vtbl slot
    static DevicePresent_t g_DevicePresent_Orig = nullptr; // MinHook trampoline

    // -------------------------
    // Optional: IDirect3DDevice9::GetSwapChain + SwapChain::Present probe
    // -------------------------
    using GetSwapChain_t = HRESULT(STDMETHODCALLTYPE*)(
        IDirect3DDevice9* self,
        UINT iSwapChain,
        IDirect3DSwapChain9** ppSwapChain
        );

    using SwapChainPresent_t = HRESULT(WINAPI*)(
        IDirect3DSwapChain9* self,
        const RECT* src_rect,
        const RECT* dst_rect,
        HWND dst_window_override,
        const RGNDATA* dirty_region,
        DWORD flags
        );

    static GetSwapChain_t       g_GetSwapChain = nullptr;

  //  static SwapChainPresent_t   g_SwapPresent_Target = nullptr;
 //   static SwapChainPresent_t   g_SwapPresent_Orig = nullptr;

    static IDirect3DSwapChain9* g_SwapChain0 = nullptr;

    static HWND g_hDeviceWindow = nullptr;

    static D3DPRESENT_PARAMETERS g_last_good_pp = {};
    static bool g_have_last_good_pp = false;
    

    static HRESULT STDMETHODCALLTYPE HookedDeviceReset(
        IDirect3DDevice9* self,
        D3DPRESENT_PARAMETERS* pp
    ) {

        static thread_local bool in_reset = false;

        if (in_reset) return D3DERR_INVALIDCALL;

        struct ResetGuard {
            bool* flag;
            ResetGuard(bool* f) : flag(f) { *flag = true; }
            ~ResetGuard() { *flag = false; }
        } guard(&in_reset);

      //  static bool dumped_first_fail = false;

        if (!pp) {
            HRESULT hr = g_DeviceReset_Orig(self, pp);
            printf("[RESET] pp=null hr=0x%08lX\n", (unsigned long)hr);
            return hr;
        }
        dump_pp_all(pp, "IN");

        D3DPRESENT_PARAMETERS pp_fixed = *pp;

        // Minimal sanitation: if game passes null hwnd, use last-good / cached
        if (!pp_fixed.hDeviceWindow) {
            if (g_have_last_good_pp && g_last_good_pp.hDeviceWindow) {
                pp_fixed.hDeviceWindow = g_last_good_pp.hDeviceWindow;
            }
            else if (g_hDeviceWindow) {
                pp_fixed.hDeviceWindow = g_hDeviceWindow;
            }
            else {
                pp_fixed.hDeviceWindow = GetForegroundWindow();
            }
        }

        printf("[RESET] enter self=%p pp=%p &pp->hDeviceWindow=%p hDevWnd(before)=%p g_hDeviceWindow=%p fg=%p\n",
            (void*)self, (void*)pp, (void*)&pp->hDeviceWindow,
            (void*)pp->hDeviceWindow, (void*)g_hDeviceWindow, (void*)GetForegroundWindow());

        //if (!pp->hDeviceWindow) {
          //  pp->hDeviceWindow = g_hDeviceWindow ? g_hDeviceWindow : GetForegroundWindow();
       // }

        printf("[RESET] after patch pp=%p hDevWnd(after)=%p\n",
            (void*)pp, (void*)pp->hDeviceWindow);

        // Do NOT touch BackBufferFormat at all during Reset
  //      if (pp->Windowed) {
    //        pp->FullScreen_RefreshRateInHz = 0;
      //  }

   //     if (!pp->EnableAutoDepthStencil) {
   //         pp->AutoDepthStencilFormat = D3DFMT_UNKNOWN;
   //     }

//        if (pp->MultiSampleType == D3DMULTISAMPLE_NONE)
  //          pp->MultiSampleQuality = 0;

    //    switch (pp->PresentationInterval) {
      //  case D3DPRESENT_INTERVAL_IMMEDIATE:
      //  case D3DPRESENT_INTERVAL_ONE:
      //  case D3DPRESENT_INTERVAL_TWO:
      //  case D3DPRESENT_INTERVAL_THREE:
      //  case D3DPRESENT_INTERVAL_FOUR:
      //      break;
       // default:
     //       pp->PresentationInterval = D3DPRESENT_INTERVAL_ONE;
       //     break;
     //   }

    //    if (pp->BackBufferWidth == 0 || pp->BackBufferHeight == 0) {
   //         pp->BackBufferWidth = 1920;
    //        pp->BackBufferHeight = 1080;
   //     }

        if (g_wrap) g_wrap->on_pre_reset();

        if (g_wrap) DBG("about to call real Reset");


        HRESULT hr = g_DeviceReset_Orig(self, pp);


        if (SUCCEEDED(hr)) {
            g_last_good_pp = pp_fixed;
            g_have_last_good_pp = true;
            if (g_wrap) g_wrap->on_post_reset(&pp_fixed);
        }
        else {
            dump_pp_all(&pp_fixed, "FAIL_FIXED");
        }

        printf("[RESET] hr=0x%08lX\n", (unsigned long)hr);
        return hr;
    }


    // guard flags
   // static bool g_tried_swap_hook = false;

    // ---- hooks ----

 //   static HRESULT WINAPI HookedSwapChainPresent(
 //       IDirect3DSwapChain9* self,
 //       const RECT* src_rect,
 //       const RECT* dst_rect,
//        HWND dst_window_override,
//        const RGNDATA* dirty_region,
//        DWORD flags
//    ) {
 //       static int hits = 0;
 //       if (hits < 10) {
 //           printf(">>> SwapChain::Present hit %d self=%p flags=0x%08lX <<<\n",
 //               hits++, (void*)self, (unsigned long)flags);
 //       }
 //       return g_SwapPresent_Orig(self, src_rect, dst_rect, dst_window_override, dirty_region, flags);
 //   }

    static HRESULT STDMETHODCALLTYPE HookedDevicePresent(
        IDirect3DDevice9* self,
        const RECT* src_rect,
        const RECT* dst_rect,
        HWND dst_window_override,
        const RGNDATA* dirty_region
    ) {
        // Resolve a window we can actually present into
        HWND use_hwnd = dst_window_override;
        if (!use_hwnd) use_hwnd = g_hDeviceWindow;


        // Optional: inspect swapchain params (safe)
        IDirect3DSwapChain9* sc2 = nullptr;
        D3DPRESENT_PARAMETERS pp2 = {};
        HRESULT a = self->GetSwapChain(0, &sc2);

        if (SUCCEEDED(a) && sc2) {
            sc2->GetPresentParameters(&pp2);
            sc2->Release();
        }

        if (!use_hwnd && pp2.hDeviceWindow) use_hwnd = pp2.hDeviceWindow;
        if (!use_hwnd) use_hwnd = GetForegroundWindow();

       //// printf("[PRESENT] in_override=%p use_hwnd=%p GetSwapChain=0x%08lX GetPP=0x%08lX sc_hwnd=%p cached_hwnd=%p\n",
          ///  (void*)dst_window_override,
          //  (void*)use_hwnd,
           // (unsigned long)a,
           // (unsigned long)b,
           // (void*)pp2.hDeviceWindow,
           // (void*)g_hDeviceWindow);

        HRESULT phr = g_DevicePresent_Orig(self, src_rect, dst_rect, dst_window_override, dirty_region);
       // printf("[PRESENT] hr=0x%08lX\n", (unsigned long)phr);
        return phr;

    }

    static HRESULT STDMETHODCALLTYPE HookedCreateDevice(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        HWND hFocusWindow,
        DWORD BehaviorFlags,
        D3DPRESENT_PARAMETERS* pPresentationParameters,
        IDirect3DDevice9** ppReturnedDeviceInterface
    ) {
        UINT bbw = pPresentationParameters ? pPresentationParameters->BackBufferWidth : 0;
        UINT bbh = pPresentationParameters ? pPresentationParameters->BackBufferHeight : 0;
        int  fmt = pPresentationParameters ? (int)pPresentationParameters->BackBufferFormat : -1;

        printf(">>> Hooked CreateDevice hFocus=%p bb=%ux%u fmt=%d pp=%p <<<\n",
            (void*)hFocusWindow, (unsigned)bbw, (unsigned)bbh, fmt, (void*)pPresentationParameters);

        if (pPresentationParameters) {
            printf(">>> CreateDevice pre: hFocus=%p hDeviceWindow=%p <<<\n",
                (void*)hFocusWindow, (void*)pPresentationParameters->hDeviceWindow);

            HWND before = pPresentationParameters->hDeviceWindow;

            if (!pPresentationParameters->hDeviceWindow)
                pPresentationParameters->hDeviceWindow = hFocusWindow;

            if (!pPresentationParameters->hDeviceWindow)
                pPresentationParameters->hDeviceWindow = GetForegroundWindow();

            printf("[CD patch] hDevWnd before=%p after=%p hFocus=%p\n",
                (void*)before,
                (void*)pPresentationParameters->hDeviceWindow,
                (void*)hFocusWindow);

            g_hDeviceWindow = pPresentationParameters->hDeviceWindow;

            printf(">>> CreateDevice patched: hDeviceWindow=%p <<<\n",
                (void*)pPresentationParameters->hDeviceWindow);
        }
        else {
            if (!g_hDeviceWindow)
                g_hDeviceWindow = hFocusWindow ? hFocusWindow : GetForegroundWindow();
        }

        if (pPresentationParameters) {
            printf(
                "[CD pre] BB=%ux%u BBFmt=%d Win=%d Swap=%d Flags=0x%08lX "
                "hFocus=%p hDevWnd=%p AutoDepth=%d DepthFmt=%d "
                "MS=%d MSQ=%lu Interval=%lu\n",
                (unsigned)pPresentationParameters->BackBufferWidth,
                (unsigned)pPresentationParameters->BackBufferHeight,
                (int)pPresentationParameters->BackBufferFormat,
                (int)pPresentationParameters->Windowed,
                (int)pPresentationParameters->SwapEffect,
                (unsigned long)pPresentationParameters->Flags,
                (void*)hFocusWindow,
                (void*)pPresentationParameters->hDeviceWindow,
                (int)pPresentationParameters->EnableAutoDepthStencil,
                (int)pPresentationParameters->AutoDepthStencilFormat,
                (int)pPresentationParameters->MultiSampleType,
                (unsigned long)pPresentationParameters->MultiSampleQuality,
                (unsigned long)pPresentationParameters->PresentationInterval
            );
        }
        else {
            printf("[CD pre] pPresentationParameters=null hFocus=%p\n", (void*)hFocusWindow);
        }

        HRESULT hr = g_CreateDevice_Orig(self, Adapter, DeviceType, hFocusWindow,
            BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

        IDirect3DDevice9* realDev =
            (SUCCEEDED(hr) && ppReturnedDeviceInterface) ? *ppReturnedDeviceInterface : nullptr;

        printf(">>> CreateDevice returned hr=0x%08lX dev=%p <<<\n",
            (unsigned long)hr, (void*)realDev);

        if (!realDev) return hr;
        // One-time init: uses std::call_once inside.
        librashader_init_once(g_hThisModule);

        if (!librashader_is_loaded()) {
            printf("[ZeroMod] librashader not loaded (shaders disabled)\n");
        }
        else {
            printf("[ZeroMod] librashader loaded (ready)\n");
        }
        // Hook Present BEFORE wrapping (real vtbl)
        if (!g_DevicePresent_Target) {
            void** vtbl = *(void***)realDev;

            g_GetSwapChain = (GetSwapChain_t)vtbl[14];
            printf("[MH] Device::GetSwapChain ptr=%p\n", (void*)g_GetSwapChain);

            g_DevicePresent_Target = (DevicePresent_t)vtbl[17];
            printf("[MH] Hooking Device::Present target=%p\n", (void*)g_DevicePresent_Target);

            if (MH_CreateHook((LPVOID)g_DevicePresent_Target,
                (LPVOID)&HookedDevicePresent,
                (LPVOID*)&g_DevicePresent_Orig) == MH_OK)
            {
                MH_EnableHook((LPVOID)g_DevicePresent_Target);
                printf("[MH] Device::Present hooked orig=%p\n", (void*)g_DevicePresent_Orig);
            }
            else {
                printf("[MH] FAILED to hook Device::Present\n");
            }
        }

        // Hook Reset too (typically vtbl[16])
        if (!g_DeviceReset_Target) {
            void** vtbl = *(void***)realDev;

            g_DeviceReset_Target = (DeviceReset_t)vtbl[16];
            printf("[MH] Hooking Device::Reset target=%p\n", (void*)g_DeviceReset_Target);

            if (MH_CreateHook((LPVOID)g_DeviceReset_Target,
                (LPVOID)&HookedDeviceReset,
                (LPVOID*)&g_DeviceReset_Orig) == MH_OK)
            {
                MH_EnableHook((LPVOID)g_DeviceReset_Target);
                printf("[MH] Device::Reset hooked orig=%p\n", (void*)g_DeviceReset_Orig);
            }
            else {
                printf("[MH] FAILED to hook Device::Reset\n");
            }
        }


        // Wrap ONCE
        g_wrap = new MyID3D9Device(ppReturnedDeviceInterface,
            pPresentationParameters ? pPresentationParameters->BackBufferWidth : 0,
            pPresentationParameters ? pPresentationParameters->BackBufferHeight : 0);


        return hr;
    }

    static HRESULT STDMETHODCALLTYPE Hooked_CheckDeviceType(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DevType,
        D3DFORMAT AdapterFormat,
        D3DFORMAT BackBufferFormat,
        BOOL bWindowed)
    {
        HRESULT hr = g_CheckDeviceType_Orig(self, Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed);
        if (hr != D3D_OK) {
            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod] FAIL IDirect3D9::CheckDeviceType(A=%u Dev=%d AF=%d BBF=%d W=%d) hr=0x%08lX\n",
                (unsigned)Adapter, (int)DevType, (int)AdapterFormat, (int)BackBufferFormat, (int)bWindowed, (unsigned long)hr);
            OutputDebugStringA(b);
            printf("%s", b);
        }
        return hr;
    }

    static HRESULT STDMETHODCALLTYPE Hooked_CheckDeviceFormat(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        D3DFORMAT AdapterFormat,
        DWORD Usage,
        D3DRESOURCETYPE RType,
        D3DFORMAT CheckFormat)
    {
        HRESULT hr = g_CheckDeviceFormat_Orig(self, Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
        if (hr != D3D_OK) {
            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod] FAIL IDirect3D9::CheckDeviceFormat(A=%u Dev=%d AF=%d Usage=0x%08lX R=%d CF=%d) hr=0x%08lX\n",
                (unsigned)Adapter, (int)DeviceType, (int)AdapterFormat, (unsigned long)Usage, (int)RType, (int)CheckFormat, (unsigned long)hr);
            OutputDebugStringA(b);
            printf("%s", b);
        }
        return hr;
    }

    static HRESULT STDMETHODCALLTYPE Hooked_CheckDeviceMultiSampleType(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        D3DFORMAT SurfaceFormat,
        BOOL Windowed,
        D3DMULTISAMPLE_TYPE MultiSampleType,
        DWORD* pQualityLevels)
    {
        HRESULT hr = g_CheckDeviceMultiSampleType_Orig(
            self, Adapter, DeviceType, SurfaceFormat, Windowed,
            MultiSampleType, pQualityLevels
        );

        DWORD q = (pQualityLevels ? *pQualityLevels : 0xDEADBEEF);

        if (hr != D3D_OK) {
            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod] HR IDirect3D9::CheckDeviceMultiSampleType("
                "A=%u Dev=%d Fmt=%d W=%d MS=%d Q=%lu) hr=0x%08lX\n",
                (unsigned)Adapter,
                (int)DeviceType,
                (int)SurfaceFormat,
                (int)Windowed,
                (int)MultiSampleType,
                (unsigned long)q,
                (unsigned long)hr
            );
            OutputDebugStringA(b);
            printf("%s", b);
        }

        return hr;
    }

    static HRESULT STDMETHODCALLTYPE Hooked_CheckDepthStencilMatch(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        D3DFORMAT AdapterFormat,
        D3DFORMAT RenderTargetFormat,
        D3DFORMAT DepthStencilFormat)
    {
        HRESULT hr = g_CheckDepthStencilMatch_Orig(self, Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);
        if (hr != D3D_OK) {
            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod] FAIL IDirect3D9::CheckDepthStencilMatch(A=%u Dev=%d AF=%d RT=%d DS=%d) hr=0x%08lX\n",
                (unsigned)Adapter, (int)DeviceType, (int)AdapterFormat, (int)RenderTargetFormat, (int)DepthStencilFormat, (unsigned long)hr);
            OutputDebugStringA(b);
            printf("%s", b);
        }
        return hr;
    }

    static HRESULT STDMETHODCALLTYPE Hooked_CheckDeviceFormatConversion(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        D3DFORMAT SourceFormat,
        D3DFORMAT TargetFormat)
    {
        HRESULT hr = g_CheckDeviceFormatConversion_Orig(self, Adapter, DeviceType, SourceFormat, TargetFormat);
        if (hr != D3D_OK) {
            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod] FAIL IDirect3D9::CheckDeviceFormatConversion(A=%u Dev=%d Src=%d Dst=%d) hr=0x%08lX\n",
                (unsigned)Adapter, (int)DeviceType, (int)SourceFormat, (int)TargetFormat, (unsigned long)hr);
            OutputDebugStringA(b);
            printf("%s", b);
        }
        return hr;
    }

    static HRESULT STDMETHODCALLTYPE Hooked_GetDeviceCaps(
        IDirect3D9* self,
        UINT Adapter,
        D3DDEVTYPE DeviceType,
        D3DCAPS9* pCaps)
    {
        HRESULT hr = g_GetDeviceCaps_Orig(self, Adapter, DeviceType, pCaps);
        if (hr != D3D_OK) {
            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod] FAIL IDirect3D9::GetDeviceCaps(A=%u Dev=%d pCaps=%p) hr=0x%08lX\n",
                (unsigned)Adapter, (int)DeviceType, (void*)pCaps, (unsigned long)hr);
            OutputDebugStringA(b);
            printf("%s", b);
        }
        return hr;
    }

    static IDirect3D9* WINAPI HookedDirect3DCreate9(UINT SDKVersion)
    {
        IDirect3D9* d3d9 = g_Direct3DCreate9_Orig(SDKVersion);
        printf(">>> Hooked Direct3DCreate9 (SDK=%u) d3d9=%p <<<\n", SDKVersion, (void*)d3d9);
        DBG("HookedDirect3DCreate9 hit");

        if (!d3d9) return d3d9;

        void** vtbl = *(void***)d3d9;

        // 1) Caps/format hooks (run once)
        if (!g_d3d9_caps_hooks_installed) {
            g_d3d9_caps_hooks_installed = true;

            auto hook_one = [](void* target, void* detour, void** orig_out, const char* name) {
                if (MH_CreateHook(target, detour, orig_out) == MH_OK) {
                    MH_EnableHook(target);
                    printf("[MH] Hooked %s target=%p orig=%p\n", name, target, *orig_out);
                }
                else {
                    printf("[MH] FAILED to hook %s target=%p\n", name, target);
                }
                };
            hook_one(vtbl[9], (void*)&Hooked_CheckDeviceType, (void**)&g_CheckDeviceType_Orig, "IDirect3D9::CheckDeviceType");
            hook_one(vtbl[10], (void*)&Hooked_CheckDeviceFormat, (void**)&g_CheckDeviceFormat_Orig, "IDirect3D9::CheckDeviceFormat");
            hook_one(vtbl[11], (void*)&Hooked_CheckDeviceMultiSampleType, (void**)&g_CheckDeviceMultiSampleType_Orig, "IDirect3D9::CheckDeviceMultiSampleType");
            hook_one(vtbl[12], (void*)&Hooked_CheckDepthStencilMatch, (void**)&g_CheckDepthStencilMatch_Orig, "IDirect3D9::CheckDepthStencilMatch");
            hook_one(vtbl[13], (void*)&Hooked_CheckDeviceFormatConversion, (void**)&g_CheckDeviceFormatConversion_Orig, "IDirect3D9::CheckDeviceFormatConversion");
            hook_one(vtbl[14], (void*)&Hooked_GetDeviceCaps, (void**)&g_GetDeviceCaps_Orig, "IDirect3D9::GetDeviceCaps");
        }

        // 2) CreateDevice hook (your existing logic)
        if (!g_CreateDevice_Target) {
            g_CreateDevice_Target = (CreateDevice_t)vtbl[16];

            if (MH_CreateHook((LPVOID)g_CreateDevice_Target,
                (LPVOID)&HookedCreateDevice,
                (LPVOID*)&g_CreateDevice_Orig) == MH_OK)
            {
                MH_EnableHook((LPVOID)g_CreateDevice_Target);
                printf("[MH] Hooked IDirect3D9::CreateDevice target=%p orig=%p\n",
                    (void*)g_CreateDevice_Target, (void*)g_CreateDevice_Orig);
            }
            else {
                printf("[MH] Failed to hook IDirect3D9::CreateDevice\n");
            }
        }

        return d3d9;
    }

    static void minhook_init() {
        printf("[MH] MH_Initialize...\n");


        if (MH_Initialize() != MH_OK) {
            printf("[MH] MH_Initialize FAILED\n");
            return;
        }

        printf("[MH] MH_Initialize OK\n");
        DBG("MH_Initialize OK");
        MinHook_Initialized = true;

        printf("[MH] Creating hook: Direct3DCreate9\n");
        if (MH_CreateHookApiEx(L"d3d9", "Direct3DCreate9",
            (LPVOID)&HookedDirect3DCreate9,
            (LPVOID*)&g_Direct3DCreate9_Orig,
            nullptr) != MH_OK)
        {
            printf("[MH] Failed to hook Direct3DCreate9\n");
            return;
        }

        DBG("HookApiEx(d3d9!Direct3DCreate9) installed");

        printf("[MH] Enabling hooks\n");
        MH_EnableHook(MH_ALL_HOOKS);
        printf("[MH] Hooks enabled\n");
    }

    static void minhook_shutdown() {
        if (!MinHook_Initialized) return;

        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        MinHook_Initialized = false;

        if (g_SwapChain0) {
            g_SwapChain0->Release();
            g_SwapChain0 = nullptr;
        }
    }
} // namespace




// ------------------------------------------------------------
// base_dll_init / base_dll_shutdown (keep your existing bodies)
// ------------------------------------------------------------
void base_dll_init(HINSTANCE hinstDLL) {

    DBG("dinput8 proxy loaded: base_dll_init()");

    wchar_t BASE_DLL_NAME_FULL[MAX_PATH] = {};
    UINT len = GetSystemDirectoryW(BASE_DLL_NAME_FULL, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        wcscat(BASE_DLL_NAME_FULL, BASE_DLL_NAME);
    }

    base_dll = LoadLibraryW(BASE_DLL_NAME_FULL);
    if (!base_dll) {
        return;
    }
    else if (base_dll == hinstDLL) {
        FreeLibrary(base_dll);
        base_dll = nullptr;
        return;
    }

#define LOAD_PROC(n) do { p##n = (n##_t)GetProcAddress(base_dll, #n); } while(0)
    LOAD_PROC(DirectInput8Create);
    LOAD_PROC(DllCanUnloadNow);
    LOAD_PROC(DllGetClassObject);
    LOAD_PROC(DllRegisterServer);
    LOAD_PROC(DllUnregisterServer);
    LOAD_PROC(GetdfDIJoystick);

    minhook_init();
}

void base_dll_shutdown() {
    if (!base_dll) return;

    minhook_shutdown();

    pDirectInput8Create = nullptr;
    pDllCanUnloadNow = nullptr;
    pDllGetClassObject = nullptr;
    pDllRegisterServer = nullptr;
    pDllUnregisterServer = nullptr;
    pGetdfDIJoystick = nullptr;

    FreeLibrary(base_dll);
    base_dll = nullptr;
}

// ------------------------------------------------------------
// Forwarded exports (keep these; yours are fine)
// ------------------------------------------------------------
_Check_return_
STDAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, const IID& riidltf,
    LPVOID* ppvOut, LPUNKNOWN punkOuter)
{
    HRESULT ret = pDirectInput8Create
        ? pDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter)
        : E_NOTIMPL;

    if (ret == S_OK) {
        new MyIDirectInput8A((IDirectInput8A**)ppvOut);
    }
    return ret;
}

_Check_return_
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR* ppv) {
    return pDllGetClassObject ? pDllGetClassObject(rclsid, riid, ppv) : E_NOTIMPL;
}

_Check_return_
STDAPI DllCanUnloadNow(void) {
    return pDllCanUnloadNow ? pDllCanUnloadNow() : S_FALSE;
}

DEFINE_PROC(HRESULT, DllRegisterServer, ()) {
    return pDllRegisterServer ? pDllRegisterServer() : E_NOTIMPL;
}

DEFINE_PROC(HRESULT, DllUnregisterServer, ()) {
    return pDllUnregisterServer ? pDllUnregisterServer() : E_NOTIMPL;
}

DEFINE_PROC(LPCDIDATAFORMAT, GetdfDIJoystick, (void)) {
    return pGetdfDIJoystick ? pGetdfDIJoystick() : nullptr;
}
