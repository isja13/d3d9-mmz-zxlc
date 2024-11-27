#include "dinput8_dll.h"
#include "dxgiswapchain.h"
#include "directinput8a.h"
#include "overlay.h"
#include "conf.h"
#include "log.h"
#include "../minhook/include/MinHook.h"
#include <tchar.h>
#include <string.h>
#include <d3d9.h>
#include <d3d9types.h>
#include "globals.h"


// Define function pointers
DirectInput8Create_t pDirectInput8Create = nullptr;
DllCanUnloadNow_t pDllCanUnloadNow = nullptr;
DllGetClassObject_t pDllGetClassObject = nullptr;
DllRegisterServer_t pDllRegisterServer = nullptr;
DllUnregisterServer_t pDllUnregisterServer = nullptr;
GetdfDIJoystick_t pGetdfDIJoystick = nullptr;

// Define global or static variables for these pointers
IDirect3D9* pD3D9 = nullptr;
IDirect3DDevice9* pD3D9Device = nullptr;
bool MinHook_Initialized = false;

namespace {
    HMODULE base_dll = nullptr;

    // Placeholder for the original Direct3DCreate9 function pointer
    typedef IDirect3D9* (WINAPI* pDirect3DCreate9)(UINT SDKVersion);
    pDirect3DCreate9 pOriginalDirect3DCreate9 = nullptr;

    // Our custom Direct3DCreate9 hook function
    IDirect3D9* WINAPI HookedDirect3DCreate9(UINT SDKVersion) {
        // Add any custom logic here
        return pOriginalDirect3DCreate9(SDKVersion);
    }

    void minhook_init() {
        if (MH_Initialize() != MH_OK) {
            // Initialization failed
        }
        else {
            MinHook_Initialized = true;
            MH_CreateHookApiEx(
                L"d3d9", "Direct3DCreate9",
                (LPVOID)&HookedDirect3DCreate9,
                (LPVOID*)&pOriginalDirect3DCreate9,
                nullptr
            );
            MH_EnableHook(MH_ALL_HOOKS);
        }
    }

    void minhook_shutdown() {
        if (MinHook_Initialized) {
            MH_DisableHook(MH_ALL_HOOKS);
            MH_Uninitialize();
            MinHook_Initialized = false;
        }
    }
}

void base_dll_init(HINSTANCE hinstDLL) {
    wchar_t BASE_DLL_NAME_FULL[MAX_PATH] = {};
    UINT len = GetSystemDirectoryW(BASE_DLL_NAME_FULL, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        wcscat(BASE_DLL_NAME_FULL, BASE_DLL_NAME);
    }

    base_dll = LoadLibraryW(BASE_DLL_NAME_FULL);
    if (!base_dll) {
        // Handle error if needed
    }
    else if (base_dll == hinstDLL) {
        FreeLibrary(base_dll);
        base_dll = nullptr;
    }
    else {
#define LOAD_PROC(n) do { \
                p ## n = (n ## _t)GetProcAddress(base_dll, #n); \
            } while (0)

        LOAD_PROC(DirectInput8Create);
        LOAD_PROC(DllCanUnloadNow);
        LOAD_PROC(DllGetClassObject);
        LOAD_PROC(DllRegisterServer);
        LOAD_PROC(DllUnregisterServer);
        LOAD_PROC(GetdfDIJoystick);

        minhook_init();
    }
}

void base_dll_shutdown() {
    if (base_dll) {
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
}

// Definition using DEFINE_PROC macro
_Check_return_
STDAPI DirectInput8Create(
    HINSTANCE hinst,
    DWORD dwVersion,
    const IID& riidltf,
    LPVOID* ppvOut,
    LPUNKNOWN punkOuter
) {
    HRESULT ret = pDirectInput8Create ?
        pDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter) :
        E_NOTIMPL;

    if (ret == S_OK) {
        new MyIDirectInput8A(
            (IDirectInput8A**)ppvOut
        );
    }
    return ret;
}



_Check_return_
STDAPI DllGetClassObject(
    _In_ REFCLSID rclsid,
    _In_ REFIID riid,
    _Outptr_ LPVOID FAR* ppv
) {
    if (pDllGetClassObject) {
        return pDllGetClassObject(rclsid, riid, ppv);
    }
    else {
        return E_NOTIMPL;
    }
}

// Definition
_Check_return_
STDAPI DllCanUnloadNow(void) {
    if (pDllCanUnloadNow) {
        return pDllCanUnloadNow();
    }
    else {
        return S_FALSE;
    }
}



DEFINE_PROC(HRESULT, DllRegisterServer, ()) {
    if (pDllRegisterServer) {
        return pDllRegisterServer();
    }
    else {
        return E_NOTIMPL;
    }
}

DEFINE_PROC(HRESULT, DllUnregisterServer, ()) {
    if (pDllUnregisterServer) {
        return pDllUnregisterServer();
    }
    else {
        return E_NOTIMPL;
    }
}

DEFINE_PROC(LPCDIDATAFORMAT, GetdfDIJoystick, (void)) {
    if (pGetdfDIJoystick) {
        return pGetdfDIJoystick();
    }
    else {
        return nullptr;
    }
}

DEFINE_PROC(HRESULT, D3D9CreateDeviceAndSwapChain, (
    D3DPRESENT_PARAMETERS* pPresentParams,
    IDirect3DSwapChain9** ppSwapChain,
    IDirect3DDevice9** ppDevice
    )) {
    HRESULT ret = E_NOTIMPL;
    if (pD3D9 && pD3D9Device) {
        ret = pD3D9Device->CreateAdditionalSwapChain(pPresentParams, ppSwapChain);
        if (ret == S_OK) {
            auto sc = new MyIDXGISwapChain(pPresentParams, *ppSwapChain, *ppDevice);
            default_config->hwnd = pPresentParams->hDeviceWindow;
            sc->set_overlay(default_overlay);
            sc->set_config(default_config);
        }
    }
    return ret;
}