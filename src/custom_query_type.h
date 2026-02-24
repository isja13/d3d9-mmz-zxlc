#ifndef CUSTOM_QUERY_TYPE_H
#define CUSTOM_QUERY_TYPE_H

#include <Windows.h> 

enum class CustomQueryType {
    EVENT,
    OCCLUSION,
};

struct CustomQueryDesc {
    CustomQueryType Query;
    UINT MiscFlags;
};

#endif // CUSTOM_QUERY_TYPE_H
