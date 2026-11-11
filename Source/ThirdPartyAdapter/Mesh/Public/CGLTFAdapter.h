#pragma once

#include "Framework/Common/Public/Types.h"

#include <string>
#include <vector>

namespace ShadowEngine
{
    struct CGLTFPrimitive
    {
        uint32 MaterialSlot = 0;
        std::vector<float> Positions;
        std::vector<float> Normals;
        std::vector<float> TexCoords;
        std::vector<uint32> Indices;
    };

    struct CGLTFMeshNode
    {
        float World[16] = {
            1.0F, 0.0F, 0.0F, 0.0F,
            0.0F, 1.0F, 0.0F, 0.0F,
            0.0F, 0.0F, 1.0F, 0.0F,
            0.0F, 0.0F, 0.0F, 1.0F};
        std::vector<CGLTFPrimitive> Primitives;
    };

    class CGLTFAdapter final
    {
        public:
            static bool LoadMeshNodes(
                const char* Path,
                std::vector<CGLTFMeshNode>& Nodes,
                std::string* ErrorMessage);
    };
}
