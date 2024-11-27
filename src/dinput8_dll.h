#ifndef DINPUT8_DLL_H
#define DINPUT8_DLL_H

#include <dinput.h>
#include "main.h"

// Define function pointer types for the DirectInput functions
typedef HRESULT(__stdcall* DirectInput8Create_t)(HINSTANCE hinst, DWORD dwVersion, const IID& riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter);
typedef HRESULT(__stdcall* DllCanUnloadNow_t)(void);
typedef HRESULT(__stdcall* DllGetClassObject_t)(REFCLSID rclsid, REFIID riid, LPVOID* ppv);


// Function declarations
void base_dll_init(HINSTANCE hinstDLL);
void base_dll_shutdown();

#define DEFINE_PROC(r, n, v) \
    typedef r (__stdcall *n ## _t) v; \
    extern n ## _t p ## n; \
    extern "C" r __stdcall n v



_Check_return_
STDAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, const IID& riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter);


_Check_return_
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID FAR* ppv);
//DEFINE_PROC(HRESULT, DllGetClassObject, (const IID&, const IID&, LPVOID*));

// Declaration
_Check_return_
STDAPI DllCanUnloadNow(void);

DEFINE_PROC(HRESULT, DllRegisterServer, ());
DEFINE_PROC(HRESULT, DllUnregisterServer, ());
DEFINE_PROC(LPCDIDATAFORMAT, GetdfDIJoystick, (void));

#endif // DINPUT8_DLL_H
