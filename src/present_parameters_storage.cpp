#include "present_parameters_storage.h"
#include <cstdio>

D3DPRESENT_PARAMETERS PresentParametersStorage::s_presentParameters;

void PresentParametersStorage::SetPresentParameters(const D3DPRESENT_PARAMETERS& params) {
    s_presentParameters = params;
}

void PresentParametersStorage::GetPresentParameters(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    if (pPresentationParameters) {
        *pPresentationParameters = s_presentParameters;
    }
}

HWND g_hDeviceWindow = nullptr;
D3DPRESENT_PARAMETERS g_last_good_pp = {};
bool g_have_last_good_pp = false;
void dump_pp_all(const D3DPRESENT_PARAMETERS* pp, const char* tag)
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