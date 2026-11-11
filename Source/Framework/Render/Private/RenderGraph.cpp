#include "Framework/Render/Public/RenderGraph.h"

#include "Framework/Common/Public/Types.h"

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace ShadowEngine
{
    namespace
    {
        void SetGraphError(std::string* ErrorMessage, const std::string& Message)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = Message;
            }
        }
    }

    RenderGraphResource RenderGraph::AddResource(std::string Name, bool bImported)
    {
        const RenderGraphResource Handle{static_cast<uint32>(Resources.size())};
        Resources.push_back({std::move(Name), bImported});
        bCompiled = false;
        return Handle;
    }

    RenderGraphResource RenderGraph::ImportResource(std::string Name)
    {
        return AddResource(std::move(Name), true);
    }

    RenderGraphResource RenderGraph::CreateResource(std::string Name)
    {
        return AddResource(std::move(Name), false);
    }

    void RenderGraph::AddPass(
        std::string Name,
        std::span<const RenderGraphResource> Reads,
        std::span<const RenderGraphResource> Writes,
        ExecuteFunction Execute)
    {
        Pass Node;
        Node.Name = std::move(Name);
        Node.Reads.assign(Reads.begin(), Reads.end());
        Node.Writes.assign(Writes.begin(), Writes.end());
        Node.Function = std::move(Execute);
        Passes.push_back(std::move(Node));
        bCompiled = false;
    }

    bool RenderGraph::Compile(std::string* ErrorMessage)
    {
        ExecutionOrder.clear();
        const uint32 PassCount = static_cast<uint32>(Passes.size());
        std::vector<std::unordered_set<uint32>> Edges(PassCount);
        std::vector<uint32> Incoming(PassCount, 0);
        std::vector<int32> LastWriter(Resources.size(), -1);
        std::vector<std::vector<uint32>> Readers(Resources.size());
        std::unordered_set<std::string> Names;

        const auto AddEdge = [&](uint32 Before, uint32 After)
        {
            if (Before == After || !Edges[Before].insert(After).second)
            {
                return;
            }
            ++Incoming[After];
        };

        for (uint32 PassIndex = 0; PassIndex < PassCount; ++PassIndex)
        {
            const Pass& Node = Passes[PassIndex];
            if (Node.Name.empty() || !Names.insert(Node.Name).second)
            {
                SetGraphError(ErrorMessage, "RenderGraph pass name is empty or duplicated: " + Node.Name);
                return false;
            }
            if (!Node.Function)
            {
                SetGraphError(ErrorMessage, "RenderGraph pass has no execute function: " + Node.Name);
                return false;
            }

            std::unordered_set<uint32> PassReads;
            std::unordered_set<uint32> PassWrites;
            for (const RenderGraphResource Handle : Node.Reads)
            {
                if (!Handle.IsValid() || Handle.Index >= Resources.size() || !PassReads.insert(Handle.Index).second)
                {
                    SetGraphError(ErrorMessage, "RenderGraph pass has an invalid or duplicate read: " + Node.Name);
                    return false;
                }
                const int32 Writer = LastWriter[Handle.Index];
                if (Writer >= 0)
                {
                    AddEdge(static_cast<uint32>(Writer), PassIndex);
                }
                else if (!Resources[Handle.Index].bImported)
                {
                    SetGraphError(
                        ErrorMessage,
                        "RenderGraph resource is read before it is produced: " + Resources[Handle.Index].Name +
                            " in " + Node.Name);
                    return false;
                }
                Readers[Handle.Index].push_back(PassIndex);
            }

            for (const RenderGraphResource Handle : Node.Writes)
            {
                if (!Handle.IsValid() || Handle.Index >= Resources.size() || !PassWrites.insert(Handle.Index).second)
                {
                    SetGraphError(ErrorMessage, "RenderGraph pass has an invalid or duplicate write: " + Node.Name);
                    return false;
                }
                if (PassReads.contains(Handle.Index))
                {
                    SetGraphError(
                        ErrorMessage,
                        "RenderGraph pass must not declare the same resource as read and write: " + Node.Name);
                    return false;
                }

                const int32 Writer = LastWriter[Handle.Index];
                if (Writer >= 0)
                {
                    AddEdge(static_cast<uint32>(Writer), PassIndex);
                }
                for (const uint32 Reader : Readers[Handle.Index])
                {
                    AddEdge(Reader, PassIndex);
                }
                Readers[Handle.Index].clear();
                LastWriter[Handle.Index] = static_cast<int32>(PassIndex);
            }
        }

        std::priority_queue<uint32, std::vector<uint32>, std::greater<uint32>> Ready;
        for (uint32 Index = 0; Index < PassCount; ++Index)
        {
            if (Incoming[Index] == 0)
            {
                Ready.push(Index);
            }
        }

        while (!Ready.empty())
        {
            const uint32 Current = Ready.top();
            Ready.pop();
            ExecutionOrder.push_back(Current);
            for (const uint32 Next : Edges[Current])
            {
                if (--Incoming[Next] == 0)
                {
                    Ready.push(Next);
                }
            }
        }

        if (ExecutionOrder.size() != Passes.size())
        {
            SetGraphError(ErrorMessage, "RenderGraph contains a dependency cycle");
            ExecutionOrder.clear();
            return false;
        }

        bCompiled = true;
        return true;
    }

    bool RenderGraph::Execute(std::string* ErrorMessage)
    {
        if (!bCompiled && !Compile(ErrorMessage))
        {
            return false;
        }

        for (const uint32 PassIndex : ExecutionOrder)
        {
            if (!Passes[PassIndex].Function())
            {
                SetGraphError(ErrorMessage, "RenderGraph pass failed: " + Passes[PassIndex].Name);
                return false;
            }
        }
        return true;
    }
}
