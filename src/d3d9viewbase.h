#ifndef D3D9VIEWBASE_H
#define D3D9VIEWBASE_H

#include "main.h"
#include "unknown.h"

class MyID3D9View {
public:
    virtual ~MyID3D9View() = default;
    virtual IDirect3DResource9* get_resource() const = 0;
};

#endif // D3D9VIEWBASE_H

