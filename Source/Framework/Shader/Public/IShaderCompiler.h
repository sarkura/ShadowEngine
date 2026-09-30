#pragma once

#include "Framework/Shader/Public/Shader.h"
#include "Interface/Public/Interface.hpp"

#include <string>

namespace ShadowEngine
{
    IInterface IShaderCompiler
    {
        public:
            virtual ~IShaderCompiler() = default;

            virtual bool Initialize(std::string* ErrorMessage = nullptr) = 0;
            virtual void Finalize() = 0;

            virtual bool Compile(
                const ShaderCompileRequest& Request,
                ShaderBytecode& Bytecode,
                std::string* ErrorMessage = nullptr) = 0;
    };
}
