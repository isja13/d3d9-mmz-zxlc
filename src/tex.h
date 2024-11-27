#ifndef TEX_H
#define TEX_H

#include "main.h"
#include <d3d9.h>

struct TextureAndViews {
    IDirect3DTexture9* tex = NULL;
    IDirect3DTexture9* srv = NULL;
    IDirect3DSurface9* rtv = NULL;
    UINT width = 0;
    UINT height = 0;
    TextureAndViews();
    ~TextureAndViews();
};

struct TextureAndDepthViews : TextureAndViews {
    IDirect3DTexture9* tex_ds = NULL;
    IDirect3DSurface9* dsv = NULL;
    TextureAndDepthViews();
    ~TextureAndDepthViews();
};

struct TextureViewsAndBuffer : TextureAndViews {
    IDirect3DVertexBuffer9* ps_cb = NULL;
    TextureViewsAndBuffer();
    ~TextureViewsAndBuffer();
};

#endif // TEX_H
