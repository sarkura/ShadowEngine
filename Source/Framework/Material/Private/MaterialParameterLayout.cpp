#include "Framework/Material/Public/MaterialParameterLayout.h"

namespace ShadowEngine
{
    bool MaterialParameterLayout::Has(const std::string& Name) const
    {
        for (const Entry& Item : Entries)
        {
            if (Item.Name == Name)
            {
                return true;
            }
        }

        return false;
    }

    bool MaterialParameterLayout::HasTexture(const std::string& Name) const
    {
        for (const Entry& Item : Entries)
        {
            if (Item.Name == Name && Item.Type == EMaterialParameterType::Texture2D)
            {
                return true;
            }
        }

        return false;
    }
}
