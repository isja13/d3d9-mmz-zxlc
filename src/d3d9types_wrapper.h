// RetroArch/d3d9types_wrapper.h

#ifndef D3D9TYPES_WRAPPER_H
#define D3D9TYPES_WRAPPER_H

// Include the original d3d9types.h
#include <d3d9types.h>

// Define alias to avoid conflict with D3D10
#ifndef D3DRTYPE_TEXTURE
#define D3DRTYPE_TEXTURE_ALIAS D3DRTYPE_TEXTURE
#endif

#endif // D3D9TYPES_WRAPPER_H
