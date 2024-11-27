#ifndef PRESENT_PARAMETERS_STORAGE_H
#define PRESENT_PARAMETERS_STORAGE_H

#include <d3d9.h>

class PresentParametersStorage {
public:
    static void SetPresentParameters(const D3DPRESENT_PARAMETERS& params);
    static void GetPresentParameters(D3DPRESENT_PARAMETERS* pPresentationParameters);

private:
    static D3DPRESENT_PARAMETERS s_presentParameters;
};

#endif // PRESENT_PARAMETERS_STORAGE_H
