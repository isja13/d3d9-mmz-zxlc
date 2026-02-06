#ifndef UNKNOWN_IMPL_H
#define UNKNOWN_IMPL_H

#include "unknown.h"

// Define IUNKNOWN_PRIV, IUNKNOWN_INIT, and IUNKNOWN_IMPL here
#define IUNKNOWN_PRIV(b) \
    b *inner = nullptr;

#define IUNKNOWN_INIT(n) \
    inner(n)

#define IUNKNOWN_IMPL(d, b) \
    b *&d::get_inner_ref() { \
        return impl->inner; \
    } \
    const b *d::get_inner_ptr() const { \
        return impl->inner; \
    } \
 \
    HRESULT STDMETHODCALLTYPE d::QueryInterface( \
        REFIID riid, \
        void   **ppvObject \
    ) { \
        HRESULT ret = impl->inner->QueryInterface(riid, ppvObject); \
        if (ret == S_OK) { \
            LOG_MFUN(_; \
                LOG_ARG(riid); \
                LOG_ARG(*ppvObject); \
                ret \
            ); \
        } else { \
            LOG_MFUN(_; \
                LOG_ARG(riid); \
                ret \
            ); \
        } \
        return ret; \
    } \
 \
    ULONG STDMETHODCALLTYPE d::AddRef() { \
        ULONG ret = impl->inner->AddRef(); \
        LOG_MFUN(_, ret); \
        return ret; \
    } \
 \
    ULONG STDMETHODCALLTYPE d::Release() { \
        ULONG ret = impl->inner->Release(); \
        LOG_MFUN(_, ret); \
        if (!ret) { \
            delete this; \
        } \
        return ret; \
    }

#endif // UNKNOWN_IMPL_H
