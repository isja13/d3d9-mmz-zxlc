#include "cg_load.h"
#include <windows.h>
#include <mutex>
#include <cstdio>

#include "../Cg/cg.h"
#include "../Cg/cgd3d9.h"

static std::once_flag g_cg_once;
static bool   g_cg_ready = false;
static char   g_cg_err[512] = { 0 };

static HMODULE g_hCg = nullptr;
static HMODULE g_hCgD3D9 = nullptr;
static CGcontext g_ctx = nullptr;

CGcontext cg_load_context() { return g_ctx; }

static void set_err(const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    vsnprintf(g_cg_err, sizeof(g_cg_err), fmt, va);
    va_end(va);
}

static void log_cg_err(const char* tag) {
    CGerror e = cgGetError();
    if (e != CG_NO_ERROR) {
        set_err("%s: CgError=%d (%s)", tag, (int)e, cgGetErrorString(e));
    }
}

static bool load_from_same_dir_as(HMODULE mod, const wchar_t* dllName, HMODULE* out)
{
    wchar_t path[MAX_PATH] = { 0 };
    if (!GetModuleFileNameW(mod, path, MAX_PATH))
        return false;

    // strip filename -> directory
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) *(slash + 1) = 0;

    wchar_t full[MAX_PATH] = { 0 };
    wcsncpy_s(full, path, _TRUNCATE);
    wcsncat_s(full, dllName, _TRUNCATE);

    *out = LoadLibraryW(full);
    return (*out != nullptr);
}

bool cg_load_init_once(HMODULE hostModule, IDirect3DDevice9* dev)
{
    std::call_once(g_cg_once, [&] {
        g_cg_ready = false;
        g_cg_err[0] = 0;

        // Prefer loading DLLs from the same directory as *your injected module*
        // (or host exe if you pass that). Fallback to normal search path.
        if (!load_from_same_dir_as(hostModule, L"cg.dll", &g_hCg))
            g_hCg = LoadLibraryW(L"cg.dll");

        if (!g_hCg) {
            set_err("LoadLibrary cg.dll failed (gle=%lu)", (unsigned long)GetLastError());
            return;
        }

        if (!load_from_same_dir_as(hostModule, L"cgD3D9.dll", &g_hCgD3D9))
            g_hCgD3D9 = LoadLibraryW(L"cgD3D9.dll");

        if (!g_hCgD3D9) {
            set_err("LoadLibrary cgD3D9.dll failed (gle=%lu)", (unsigned long)GetLastError());
            return;
        }

        g_ctx = cgCreateContext();
        if (!g_ctx) {
            log_cg_err("cgCreateContext");
            if (!g_cg_err[0]) set_err("cgCreateContext returned NULL");
            return;
        }

        cgD3D9SetDevice(dev);
        log_cg_err("cgD3D9SetDevice");
        if (g_cg_err[0]) return;

        g_cg_ready = true;
        set_err("OK");
        });

    return g_cg_ready;
}

bool cg_load_is_ready() { return g_cg_ready; }
const char* cg_load_last_error() { return g_cg_err[0] ? g_cg_err : "no error"; }

void cg_load_on_reset_pre()
{
    // Nothing required for a bare loader check.
    // (if create CGprograms, you may want to unbind/release RTs here.)
}

void cg_load_on_reset_post(IDirect3DDevice9* dev)
{
    if (!g_cg_ready) return;
    if (!cg_load_is_ready())
        printf("[ZeroMod] Cg lost after Reset: %s\n", cg_load_last_error());
    // Re-bind device to let Cg rebuild its internal D3D resources as needed.
    cgD3D9SetDevice(dev);
    // If this fails, flip ready off and capture the error.
    CGerror e = cgGetError();
    if (e != CG_NO_ERROR) {
        g_cg_ready = false;
        set_err("reset_post: CgError=%d (%s)", (int)e, cgGetErrorString(e));
    }
}
