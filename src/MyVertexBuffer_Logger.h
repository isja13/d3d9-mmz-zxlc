#ifndef MY_VERTEX_BUFFER_LOGGER_H
#define MY_VERTEX_BUFFER_LOGGER_H

#include <d3d9.h>
#include "d3d9inputlayout.h"

struct MyVertexBuffer_Logger {
    const MyID3D9InputLayout* input_layout;
    IDirect3DVertexBuffer9* vertex_buffer;
    UINT stride;
    UINT offset;
    UINT VertexCount;
    UINT StartVertexLocation;
};

#endif // MY_VERTEX_BUFFER_LOGGER_H

