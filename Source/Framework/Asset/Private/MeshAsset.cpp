#include "Framework/Asset/Public/MeshAsset.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace ShadowEngine
{
    namespace
    {
        void WriteSectionBounds(MeshSection& Section)
        {
            if (Section.Vertices.empty())
            {
                return;
            }

            float MinX = std::numeric_limits<float>::max();
            float MinY = MinX;
            float MinZ = MinX;
            float MaxX = std::numeric_limits<float>::lowest();
            float MaxY = MaxX;
            float MaxZ = MaxX;
            for (const MeshVertex& Vertex : Section.Vertices)
            {
                MinX = std::min(MinX, Vertex.Position[0]);
                MinY = std::min(MinY, Vertex.Position[1]);
                MinZ = std::min(MinZ, Vertex.Position[2]);
                MaxX = std::max(MaxX, Vertex.Position[0]);
                MaxY = std::max(MaxY, Vertex.Position[1]);
                MaxZ = std::max(MaxZ, Vertex.Position[2]);
            }
            Section.BoundsMin[0] = MinX;
            Section.BoundsMin[1] = MinY;
            Section.BoundsMin[2] = MinZ;
            Section.BoundsMax[0] = MaxX;
            Section.BoundsMax[1] = MaxY;
            Section.BoundsMax[2] = MaxZ;
        }
    }

    MeshAsset::MeshAsset(std::filesystem::path InPath, std::vector<MeshSection> InSections)
        : Asset(EAssetType::Mesh, std::move(InPath))
        , Sections(std::move(InSections))
    {
        for (MeshSection& Section : Sections)
        {
            WriteSectionBounds(Section);
        }
    }

    const std::vector<MeshSection>& MeshAsset::GetSections() const
    {
        return Sections;
    }
}
