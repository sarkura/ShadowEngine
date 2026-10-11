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

    bool MaterialParameterLayout::Matches(const MaterialParameterLayout& Other) const
    {
        if (Entries.size() != Other.Entries.size())
        {
            return false;
        }

        for (size_t Index = 0; Index < Entries.size(); ++Index)
        {
            if (Entries[Index].Name != Other.Entries[Index].Name || Entries[Index].Type != Other.Entries[Index].Type)
            {
                return false;
            }
        }

        return true;
    }
}
