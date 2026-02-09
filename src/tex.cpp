#include "tex.h"

TextureAndViews::TextureAndViews() {}

TextureAndViews::~TextureAndViews() {
    if (rtv) { rtv->Release(); rtv = NULL; }
    if (srv) { srv->Release(); srv = NULL; }
    if (tex) { tex->Release(); tex = NULL; }
    width = height = 0;
}

TextureAndDepthViews::TextureAndDepthViews() {}

TextureAndDepthViews::~TextureAndDepthViews() {
    if (ds) { ds->Release(); ds = NULL; }
    // base dtor releases rtv/srv/tex
}

TextureViewsAndBuffer::TextureViewsAndBuffer() {}

TextureViewsAndBuffer::~TextureViewsAndBuffer() {
    if (ps_cb) { ps_cb->Release(); ps_cb = NULL; }
    // base dtor releases rtv/srv/tex
}
