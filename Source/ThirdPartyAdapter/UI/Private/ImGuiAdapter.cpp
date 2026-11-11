#include "ThirdPartyAdapter/UI/Public/ImGuiAdapter.h"

#include <cstdarg>

#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);

#include <imgui_impl_win32.h>

namespace ShadowEngine
{
    bool ImGuiAdapter::HasContext()
    {
        return ImGui::GetCurrentContext() != nullptr;
    }

    void ImGuiAdapter::CreateContext()
    {
        ImGui::CreateContext();
    }

    void ImGuiAdapter::DestroyContext()
    {
        ImGui::DestroyContext();
    }

    ImGuiIO& ImGuiAdapter::GetIO()
    {
        return ImGui::GetIO();
    }

    ImGuiPlatformIO& ImGuiAdapter::GetPlatformIO()
    {
        return ImGui::GetPlatformIO();
    }

    ImDrawData* ImGuiAdapter::GetDrawData()
    {
        return ImGui::GetDrawData();
    }

    const ImGuiViewport* ImGuiAdapter::GetMainViewport()
    {
        return ImGui::GetMainViewport();
    }

    void ImGuiAdapter::NewFrame()
    {
        ImGui::NewFrame();
    }

    void ImGuiAdapter::Render()
    {
        ImGui::Render();
    }

    void ImGuiAdapter::SetNextWindowPos(const ImVec2& Position, ImGuiCond Condition)
    {
        ImGui::SetNextWindowPos(Position, Condition);
    }

    void ImGuiAdapter::SetNextWindowSize(const ImVec2& Size, ImGuiCond Condition)
    {
        ImGui::SetNextWindowSize(Size, Condition);
    }

    void ImGuiAdapter::SetNextWindowBgAlpha(float Alpha)
    {
        ImGui::SetNextWindowBgAlpha(Alpha);
    }

    void ImGuiAdapter::SetNextItemWidth(float Width)
    {
        ImGui::SetNextItemWidth(Width);
    }

    bool ImGuiAdapter::Begin(const char* Title, bool* Open, ImGuiWindowFlags Flags)
    {
        return ImGui::Begin(Title, Open, Flags);
    }

    void ImGuiAdapter::End()
    {
        ImGui::End();
    }

    bool ImGuiAdapter::CollapsingHeader(const char* Label, ImGuiTreeNodeFlags Flags)
    {
        return ImGui::CollapsingHeader(Label, Flags);
    }

    bool ImGuiAdapter::TreeNodeEx(const char* Label, ImGuiTreeNodeFlags Flags)
    {
        return ImGui::TreeNodeEx(Label, Flags);
    }

    void ImGuiAdapter::TreePop()
    {
        ImGui::TreePop();
    }

    bool ImGuiAdapter::Checkbox(const char* Label, bool* Value)
    {
        return ImGui::Checkbox(Label, Value);
    }

    bool ImGuiAdapter::SliderFloat(const char* Label, float* Value, float Minimum, float Maximum, const char* Format)
    {
        return ImGui::SliderFloat(Label, Value, Minimum, Maximum, Format);
    }

    void ImGuiAdapter::BeginDisabled(bool bDisabled)
    {
        ImGui::BeginDisabled(bDisabled);
    }

    void ImGuiAdapter::EndDisabled()
    {
        ImGui::EndDisabled();
    }

    void ImGuiAdapter::PushID(const char* Identifier)
    {
        ImGui::PushID(Identifier);
    }

    void ImGuiAdapter::PopID()
    {
        ImGui::PopID();
    }

    void ImGuiAdapter::TextUnformatted(const char* Text)
    {
        ImGui::TextUnformatted(Text);
    }

    void ImGuiAdapter::Text(const char* Format, ...)
    {
        va_list Arguments;
        va_start(Arguments, Format);
        ImGui::TextV(Format, Arguments);
        va_end(Arguments);
    }

    bool ImGuiAdapter::Win32Init(void* Window)
    {
        return ImGui_ImplWin32_Init(static_cast<HWND>(Window));
    }

    void ImGuiAdapter::Win32Shutdown()
    {
        ImGui_ImplWin32_Shutdown();
    }

    void ImGuiAdapter::Win32NewFrame()
    {
        ImGui_ImplWin32_NewFrame();
    }

    std::intptr_t ImGuiAdapter::Win32WndProc(void* Window, unsigned int Message, std::uintptr_t WParam, std::intptr_t LParam)
    {
        return ImGui_ImplWin32_WndProcHandler(
            static_cast<HWND>(Window),
            Message,
            static_cast<WPARAM>(WParam),
            static_cast<LPARAM>(LParam));
    }
}
