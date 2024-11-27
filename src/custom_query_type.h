#ifndef CUSTOM_QUERY_TYPE_H
#define CUSTOM_QUERY_TYPE_H

#include <Windows.h> // Include this header for UINT

enum class CustomQueryType {
    EVENT,
    OCCLUSION,
    // Add other query types as necessary
};

struct CustomQueryDesc {
    CustomQueryType Query;
    UINT MiscFlags;
};

#endif // CUSTOM_QUERY_TYPE_H
