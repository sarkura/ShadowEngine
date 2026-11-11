#pragma once

#include "Framework/Common/Public/Types.h"

namespace ShadowEngine
{
    struct MeshVertex
    {
        float Position[3];
        float Normal[3];
        float Color[3];
        float TexCoord[2];
    };

    static_assert(sizeof(MeshVertex) == 44);
}
