#pragma once

#include "Framework/Common/Public/NonCopyable.h"
#include "Framework/Common/Public/Types.h"

#include <functional>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

namespace ShadowEngine
{
    struct RenderGraphResource
    {
        uint32 Index = 0xffffffff;

        [[nodiscard]] bool IsValid() const
        {
            return Index != 0xffffffff;
        }
    };

    class RenderGraph final : public NonCopyable
    {
        public:
            using ExecuteFunction = std::function<bool()>;

            RenderGraphResource ImportResource(std::string Name);
            RenderGraphResource CreateResource(std::string Name);

            void AddPass(
                std::string Name,
                std::span<const RenderGraphResource> Reads,
                std::span<const RenderGraphResource> Writes,
                ExecuteFunction Execute);

            void AddPass(
                std::string Name,
                std::initializer_list<RenderGraphResource> Reads,
                std::initializer_list<RenderGraphResource> Writes,
                ExecuteFunction Execute)
            {
                AddPass(
                    std::move(Name),
                    std::span<const RenderGraphResource>(Reads.begin(), Reads.size()),
                    std::span<const RenderGraphResource>(Writes.begin(), Writes.size()),
                    std::move(Execute));
            }

            [[nodiscard]] bool Compile(std::string* ErrorMessage = nullptr);
            [[nodiscard]] bool Execute(std::string* ErrorMessage = nullptr);

        private:
            struct Resource
            {
                std::string Name;
                bool bImported = false;
            };

            struct Pass
            {
                std::string Name;
                std::vector<RenderGraphResource> Reads;
                std::vector<RenderGraphResource> Writes;
                ExecuteFunction Function;
            };

            RenderGraphResource AddResource(std::string Name, bool bImported);

            std::vector<Resource> Resources;
            std::vector<Pass> Passes;
            std::vector<uint32> ExecutionOrder;
            bool bCompiled = false;
    };
}
