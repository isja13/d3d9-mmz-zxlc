#include "main.h"
#include "overlay.h"
#include "dinput8_dll.h"
#include "conf.h"
#include "ini.h"
#include "globals.h"
#include "dxgiswapchain.h"
#include "log.h"
#include "../RetroArch/retroarch.h"
#include "../RetroArch/RetroArch/retroarch.h"
//#include "../RetroArch/RetroArch/retroarch.c"
#include <algorithm> // Ensure this is included for std::min
#include <cstdio>


inline size_t min_size_t(size_t a, size_t b) {
    return (a < b) ? a : b;
}

bool _tstring_view_icmp::operator()(const _tstring_view& a, const _tstring_view& b) const {
    int ret = _tcsnicmp(a.data(), b.data(), static_cast<int>(min_size_t(a.size(), b.size())));
    if (ret == 0) return a.size() < b.size();
    return ret < 0;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved) {
    switch (fdwReason) {

    case DLL_PROCESS_ATTACH:

        AllocConsole();
        FILE* f;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        printf(">>> dinput8.dll loaded: DllMain PROCESS_ATTACH reached <<<\n");

        DisableThreadLibraryCalls(hinstDLL);
        try {
            my_config_init();

            default_overlay = new Overlay();
            default_overlay->push_text(MOD_NAME " loaded");
            OutputDebugStringA("[ZeroMod] default_overlay created\n");


            default_config = new Config();
            default_ini = new Ini(INI_FILE_NAME, default_overlay, default_config);
            default_logger = new Logger(LOG_FILE_NAME, default_config, default_overlay);

            char b[256];
            _snprintf(b, sizeof(b),
                "[ZeroMod] DllMain init: overlay=%p config=%p ini=%p logger=%p\n",
                (void*)default_overlay, (void*)default_config,
                (void*)default_ini, (void*)default_logger);
            OutputDebugStringA(b);


            printf("[globals] overlay=%p config=%p ini=%p logger=%p\n",
                (void*)default_overlay, (void*)default_config,
                (void*)default_ini, (void*)default_logger);

            base_dll_init(hinstDLL);
        }
        catch (const std::exception& e) {
            if (default_logger) {
                default_logger->log_error("Exception during DLL_PROCESS_ATTACH: ", e.what());
            }
            return FALSE;
        }
        catch (...) {
            if (default_logger) {
                default_logger->log_error("Unknown exception during DLL_PROCESS_ATTACH");
            }
            return FALSE;
        }
        break;

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        break;

    case DLL_PROCESS_DETACH:
        try {
            base_dll_shutdown();

            delete default_logger;
            default_logger = nullptr;

            delete default_ini;
            default_ini = nullptr;

            delete default_config;
            default_config = nullptr;

            delete default_overlay;
            default_overlay = nullptr;


            my_config_free();
        }
        catch (const std::exception& e) {
            // Don't reference dummy_logger here (main.cpp can't see it).
            // If default_logger still exists, use it; otherwise fall back to debugger.
            if (default_logger) {
                default_logger->log_error("Exception during DLL_PROCESS_DETACH: ", e.what());
            }
            else {
                OutputDebugStringA("[ZeroMod] Exception during DLL_PROCESS_DETACH\n");
                OutputDebugStringA(e.what());
                OutputDebugStringA("\n");
            }
        }
        catch (...) {
            if (default_logger) {
                default_logger->log_error("Unknown exception during DLL_PROCESS_DETACH");
            }
            else {
                OutputDebugStringA("[ZeroMod] Unknown exception during DLL_PROCESS_DETACH\n");
            }
        }
        break;

    }
    return TRUE;
}


class cs_wrapper::Impl {
    CRITICAL_SECTION cs;
    friend class cs_wrapper;
};

cs_wrapper::cs_wrapper() : impl(new Impl()) {
    InitializeCriticalSection(&impl->cs);
}

cs_wrapper::~cs_wrapper() {
    DeleteCriticalSection(&impl->cs);
    delete impl;
}

void cs_wrapper::begin_cs() {
    EnterCriticalSection(&impl->cs);
}

bool cs_wrapper::try_begin_cs() {
    return TryEnterCriticalSection(&impl->cs);
}

void cs_wrapper::end_cs() {
    LeaveCriticalSection(&impl->cs);
}
