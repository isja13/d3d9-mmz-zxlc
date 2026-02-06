#include "present_parameters_storage.h"

// Define the static member
D3DPRESENT_PARAMETERS PresentParametersStorage::s_presentParameters;

void PresentParametersStorage::SetPresentParameters(const D3DPRESENT_PARAMETERS& params) {
    s_presentParameters = params;
}

void PresentParametersStorage::GetPresentParameters(D3DPRESENT_PARAMETERS* pPresentationParameters) {
    if (pPresentationParameters) {
        *pPresentationParameters = s_presentParameters;
    }
}
