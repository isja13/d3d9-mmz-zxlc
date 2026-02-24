#ifndef PRESENT_PARAMETERS_STORAGE_H
#define PRESENT_PARAMETERS_STORAGE_H

#include <d3d9.h>


extern HWND g_hDeviceWindow;
extern D3DPRESENT_PARAMETERS g_last_good_pp;
extern bool g_have_last_good_pp;
void dump_pp_all(const D3DPRESENT_PARAMETERS* pp, const char* tag);

class PresentParametersStorage {
public:
    static void SetPresentParameters(const D3DPRESENT_PARAMETERS& params);
    static void GetPresentParameters(D3DPRESENT_PARAMETERS* pPresentationParameters);

private:
    static D3DPRESENT_PARAMETERS s_presentParameters;
};

#endif // PRESENT_PARAMETERS_STORAGE_H
