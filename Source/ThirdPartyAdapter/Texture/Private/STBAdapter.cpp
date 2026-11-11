#include "ThirdPartyAdapter/Texture/Public/STBAdapter.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace ShadowEngine
{
    unsigned char* STBAdapter::LoadImage(
        const char* FileName,
        int* Width,
        int* Height,
        int* Channels,
        int DesiredChannels)
    {
        return stbi_load(FileName, Width, Height, Channels, DesiredChannels);
    }

    float* STBAdapter::LoadImageFloat(
        const char* FileName,
        int* Width,
        int* Height,
        int* Channels,
        int DesiredChannels)
    {
        return stbi_loadf(FileName, Width, Height, Channels, DesiredChannels);
    }

    const char* STBAdapter::GetFailureReason()
    {
        return stbi_failure_reason();
    }

    void STBAdapter::FreeImage(void* Data)
    {
        stbi_image_free(Data);
    }
}
