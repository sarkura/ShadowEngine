#pragma once

#include "Framework/Config/Public/UIConfigSetting.h"

namespace ShadowEngine
{
    class UIConfigManager final
    {
        public:
            void SubmitDeferred(UIConfigSetting::DeferredUIParameter Setting);
            void SubmitForward(UIConfigSetting::ForwardUIParameter Setting);
            void SubmitBlinnPhong(UIConfigSetting::BlinnPhongUIParameter Setting);
            void SubmitDebug(UIConfigSetting::DebugUIParameter Setting);

            [[nodiscard]] UIConfigSetting::DeferredUIParameter GetDeferred() const;
            [[nodiscard]] UIConfigSetting::ForwardUIParameter GetForward() const;
            [[nodiscard]] UIConfigSetting::BlinnPhongUIParameter GetBlinnPhong() const;
            [[nodiscard]] UIConfigSetting::DebugUIParameter GetDebug() const;

            UIConfigManager(const UIConfigManager&) = delete;
            UIConfigManager& operator=(const UIConfigManager&) = delete;
            UIConfigManager(UIConfigManager&&) = delete;
            UIConfigManager& operator=(UIConfigManager&&) = delete;

        private:
            friend class ConfigManager;

            UIConfigManager() = default;

            UIConfigSetting::DeferredUIParameter DeferredSetting;
            UIConfigSetting::ForwardUIParameter ForwardSetting;
            UIConfigSetting::BlinnPhongUIParameter BlinnPhongSetting;
            UIConfigSetting::DebugUIParameter DebugSetting;
    };
}
