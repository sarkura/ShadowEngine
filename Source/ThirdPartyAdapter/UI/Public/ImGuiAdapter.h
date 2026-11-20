#pragma once

#include <imgui.h>

namespace ShadowEngine
{
    class ImGuiAdapter final
    {
        public:
            static bool HasContext();
            static void CreateContext();
            static void DestroyContext();
            static ImGuiIO& GetIO();
            static ImGuiPlatformIO& GetPlatformIO();
            static ImDrawData* GetDrawData();
            static const ImGuiViewport* GetMainViewport();

            static void NewFrame();
            static void Render();
            static void SetNextWindowPos(const ImVec2& Position, ImGuiCond Condition);
            static void SetNextWindowSize(const ImVec2& Size, ImGuiCond Condition);
            static void SetNextWindowBgAlpha(float Alpha);
            static void SetNextItemWidth(float Width);
            static bool Begin(const char* Title, bool* Open, ImGuiWindowFlags Flags);
            static void End();
            static bool CollapsingHeader(const char* Label, ImGuiTreeNodeFlags Flags);
            static bool TreeNodeEx(const char* Label, ImGuiTreeNodeFlags Flags);
            static void TreePop();
            static bool Checkbox(const char* Label, bool* Value);
            static bool SliderFloat(const char* Label, float* Value, float Minimum, float Maximum, const char* Format);
            static void BeginDisabled(bool bDisabled);
            static void EndDisabled();
            static void PushID(const char* Identifier);
            static void PopID();
            static void TextUnformatted(const char* Text);
            static void Text(const char* Format, ...);
    };
}
