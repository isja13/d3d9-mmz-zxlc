#ifndef UNKNOWN_H
#define UNKNOWN_H

#include "macros.h"

#ifndef IUNKNOWN_DECL
#define IUNKNOWN_DECL(b) \
    b*& get_inner_ref(); \
    const b* get_inner_ptr() const; \
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override; \
    ULONG STDMETHODCALLTYPE AddRef() override; \
    ULONG STDMETHODCALLTYPE Release() override;
#endif

#endif // UNKNOWN_H
