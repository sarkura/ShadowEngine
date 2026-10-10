#include "Framework/Render/Public/RenderItem.h"

#include "Framework/Asset/Public/AssetManager.h"
#include "Framework/Asset/Public/MeshAsset.h"
#include "Framework/Common/Public/Log.h"
#include "Framework/Material/Public/Material.h"
#include "Framework/Material/Public/MaterialInstance.h"
#include "Framework/Render/Public/RenderQueue.h"

#include <cstring>

namespace ShadowEngine
{
    bool RenderItemFilter::Accept(const MeshRenderProxy&, uint32, MaterialInstanceHandle) const
    {
        return true;
    }

    bool FillRenderQueue(
        const RenderProxy& Proxy,
        const RenderItemFilter& Filter,
        AssetManager& Assets,
        std::string_view RendererName,
        const std::vector<MeshBatch>& Batches,
        RenderQueue& Queue)
    {
        Queue.Clear();
        for (const MeshRenderProxy& MeshProxy : Proxy.Meshes)
        {
            const MeshAsset* Mesh = Assets.ResolveMesh(MeshProxy.Mesh);
            if (Mesh == nullptr)
            {
                Log::Error("Render proxy has no mesh");
                return false;
            }

            const std::vector<MeshSection>& Sections = Mesh->GetSections();
            if (MeshProxy.Materials.size() != Sections.size())
            {
                Log::Error("Render proxy mesh was not uploaded");
                return false;
            }

            for (uint32 SectionIndex = 0; SectionIndex < static_cast<uint32>(Sections.size()); ++SectionIndex)
            {
                const MaterialInstanceHandle MaterialHandle = MeshProxy.Materials[SectionIndex];
                if (!MaterialHandle.IsValid())
                {
                    Log::Error("Render proxy section has no material");
                    return false;
                }

                if (!Filter.Accept(MeshProxy, SectionIndex, MaterialHandle))
                {
                    continue;
                }

                const MaterialInstance* Instance = Assets.ResolveMaterialInstance(MaterialHandle);
                const Material* BoundMaterial = Instance == nullptr ? nullptr : Assets.ResolveMaterial(Instance->GetBaseMaterial());
                if (BoundMaterial == nullptr)
                {
                    Log::Error("Render proxy material is missing");
                    return false;
                }

                if (BoundMaterial->FindShader(RendererName) == nullptr)
                {
                    continue;
                }

                if (FindMeshBatch(Batches, MeshProxy.Mesh, SectionIndex) == nullptr)
                {
                    Log::Error("Render proxy mesh was not uploaded");
                    return false;
                }

                RenderItem Item;
                Item.Mesh = MeshProxy.Mesh;
                Item.Material = MaterialHandle;
                Item.Section = SectionIndex;
                std::memcpy(Item.World, MeshProxy.World, sizeof(Item.World));
                std::memcpy(Item.NormalMatrix, MeshProxy.NormalMatrix, sizeof(Item.NormalMatrix));
                Queue.Add(Item);
            }
        }

        return true;
    }
}
