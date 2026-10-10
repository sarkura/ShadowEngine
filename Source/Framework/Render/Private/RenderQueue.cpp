#include "Framework/Render/Public/RenderQueue.h"

namespace ShadowEngine
{
    void RenderQueue::Clear()
    {
        Items.clear();
    }

    void RenderQueue::Add(const RenderItem& Item)
    {
        Items.push_back(Item);
    }

    const std::vector<RenderItem>& RenderQueue::GetItems() const
    {
        return Items;
    }
}
