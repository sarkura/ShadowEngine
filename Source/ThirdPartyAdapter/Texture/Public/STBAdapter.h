#pragma once

namespace ShadowEngine
{
    class STBAdapter final
    {
        public:
            static unsigned char* LoadImage(
                const char* FileName,
                int* Width,
                int* Height,
                int* Channels,
                int DesiredChannels);

            static float* LoadImageFloat(
                const char* FileName,
                int* Width,
                int* Height,
                int* Channels,
                int DesiredChannels);

            static const char* GetFailureReason();
            static void FreeImage(void* Data);
    };
}
