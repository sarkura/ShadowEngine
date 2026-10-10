#pragma once

#include "Framework/Render/Public/RenderItem.h"

#include <vector>

namespace ShadowEngine
{
    class RenderQueue
    {
        public:
            void Clear();
            void Add(const RenderItem& Item);

            [[nodiscard]] const std::vector<RenderItem>& GetItems() const;

        private:
            std::vector<RenderItem> Items;
    };
}
