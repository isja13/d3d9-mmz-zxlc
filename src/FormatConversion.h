#ifndef FORMAT_CONVERSION_H
#define FORMAT_CONVERSION_H

#include <d3d9.h>
#include <dxgi.h>

DXGI_FORMAT ConvertD3DFormatToDXGIFormat(D3DFORMAT format);
D3DFORMAT ConvertDXGIFormatToD3DFormat(DXGI_FORMAT format);

#endif // FORMAT_CONVERSION_H