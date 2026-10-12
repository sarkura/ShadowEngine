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

    void RenderQueue::Assign(std::vector<RenderItem> Next)
    {
        Items = std::move(Next);
    }

    const std::vector<RenderItem>& RenderQueue::GetItems() const
    {
        return Items;
    }
}
