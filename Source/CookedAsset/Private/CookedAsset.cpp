#include "CookedAsset/Public/CookedAsset.h"

#include <cstdint>
#include <fstream>
#include <utility>

namespace ShadowEngine
{
    namespace
    {
        constexpr uint32 MeshMagic = 0x534D4553;
        constexpr uint32 TextureMagic = 0x58544553;
        constexpr uint32 CookedVersion = 1;

        void SetError(std::string* ErrorMessage, const std::string& Message)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = Message;
            }
        }

        bool WriteValue(std::ostream& Output, const void* Data, size_t Size)
        {
            Output.write(static_cast<const char*>(Data), static_cast<std::streamsize>(Size));
            return static_cast<bool>(Output);
        }

        template <typename T>
        bool WritePod(std::ostream& Output, const T& Value)
        {
            return WriteValue(Output, &Value, sizeof(T));
        }

        bool ReadValue(std::istream& Input, void* Data, size_t Size)
        {
            Input.read(static_cast<char*>(Data), static_cast<std::streamsize>(Size));
            return static_cast<bool>(Input);
        }

        template <typename T>
        bool ReadPod(std::istream& Input, T& Value)
        {
            return ReadValue(Input, &Value, sizeof(T));
        }

        std::filesystem::path CookedPath(const std::filesystem::path& Source, const char* Extension)
        {
            std::filesystem::path Output = Source;
            Output.replace_extension(Extension);
            return Output;
        }
    }

    std::filesystem::path CookedMeshPath(const std::filesystem::path& Source)
    {
        return CookedPath(Source, ".semesh");
    }

    std::filesystem::path CookedTexturePath(const std::filesystem::path& Source)
    {
        return CookedPath(Source, ".setex");
    }

    bool SaveCookedMesh(const std::filesystem::path& Path, const CookedMesh& Mesh, std::string* ErrorMessage)
    {
        std::error_code Error;
        std::filesystem::create_directories(Path.parent_path(), Error);
        std::ofstream Output(Path, std::ios::binary | std::ios::trunc);
        if (!Output)
        {
            SetError(ErrorMessage, "Unable to write cooked mesh: " + Path.generic_string());
            return false;
        }

        const uint32 SectionCount = static_cast<uint32>(Mesh.Sections.size());
        if (!WritePod(Output, MeshMagic) || !WritePod(Output, CookedVersion) || !WritePod(Output, SectionCount))
        {
            SetError(ErrorMessage, "Unable to write cooked mesh: " + Path.generic_string());
            return false;
        }

        for (const CookedMeshSection& Section : Mesh.Sections)
        {
            const uint32 Flags = Section.bHasTexCoords ? 1U : 0U;
            const uint32 VertexCount = static_cast<uint32>(Section.Vertices.size());
            const uint32 IndexCount = static_cast<uint32>(Section.Indices.size());
            if (!WritePod(Output, Section.Slot) ||
                !WritePod(Output, Flags) ||
                !WritePod(Output, VertexCount) ||
                !WritePod(Output, IndexCount) ||
                !WriteValue(Output, Section.Vertices.data(), Section.Vertices.size() * sizeof(MeshVertex)) ||
                !WriteValue(Output, Section.Indices.data(), Section.Indices.size() * sizeof(uint32)))
            {
                SetError(ErrorMessage, "Unable to write cooked mesh: " + Path.generic_string());
                return false;
            }
        }

        return true;
    }

    bool LoadCookedMesh(const std::filesystem::path& Path, CookedMesh& Mesh, std::string* ErrorMessage)
    {
        std::ifstream Input(Path, std::ios::binary);
        if (!Input)
        {
            SetError(ErrorMessage, "Imported mesh is missing, run AssetImportTool: " + Path.generic_string());
            return false;
        }

        uint32 Magic = 0;
        uint32 Version = 0;
        uint32 SectionCount = 0;
        if (!ReadPod(Input, Magic) || Magic != MeshMagic || !ReadPod(Input, Version) || Version != CookedVersion || !ReadPod(Input, SectionCount))
        {
            SetError(ErrorMessage, "Cooked mesh is invalid: " + Path.generic_string());
            return false;
        }

        Mesh.Sections.clear();
        Mesh.Sections.reserve(SectionCount);
        for (uint32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
        {
            CookedMeshSection Section;
            uint32 Flags = 0;
            uint32 VertexCount = 0;
            uint32 IndexCount = 0;
            if (!ReadPod(Input, Section.Slot) || !ReadPod(Input, Flags) || !ReadPod(Input, VertexCount) || !ReadPod(Input, IndexCount))
            {
                SetError(ErrorMessage, "Cooked mesh is invalid: " + Path.generic_string());
                return false;
            }

            Section.bHasTexCoords = (Flags & 1U) != 0;
            Section.Vertices.resize(VertexCount);
            Section.Indices.resize(IndexCount);
            if (!ReadValue(Input, Section.Vertices.data(), Section.Vertices.size() * sizeof(MeshVertex)) ||
                !ReadValue(Input, Section.Indices.data(), Section.Indices.size() * sizeof(uint32)))
            {
                SetError(ErrorMessage, "Cooked mesh is invalid: " + Path.generic_string());
                return false;
            }

            Mesh.Sections.push_back(std::move(Section));
        }

        return true;
    }

    bool SaveCookedTexture(const std::filesystem::path& Path, const CookedTexture& Texture, std::string* ErrorMessage)
    {
        std::error_code Error;
        std::filesystem::create_directories(Path.parent_path(), Error);
        std::ofstream Output(Path, std::ios::binary | std::ios::trunc);
        if (!Output)
        {
            SetError(ErrorMessage, "Unable to write cooked texture: " + Path.generic_string());
            return false;
        }

        const uint32 Format = static_cast<uint32>(Texture.Format);
        const uint32 Encoding = static_cast<uint32>(Texture.Encoding);
        const uint32 MipCount = static_cast<uint32>(Texture.Mips.size());
        if (!WritePod(Output, TextureMagic) || !WritePod(Output, CookedVersion) || !WritePod(Output, Format) || !WritePod(Output, Encoding) || !WritePod(Output, MipCount))
        {
            SetError(ErrorMessage, "Unable to write cooked texture: " + Path.generic_string());
            return false;
        }

        for (const CookedTextureMip& Mip : Texture.Mips)
        {
            const uint32 ByteCount = static_cast<uint32>(Mip.Pixels.size());
            if (!WritePod(Output, Mip.Width) ||
                !WritePod(Output, Mip.Height) ||
                !WritePod(Output, ByteCount) ||
                !WriteValue(Output, Mip.Pixels.data(), Mip.Pixels.size()))
            {
                SetError(ErrorMessage, "Unable to write cooked texture: " + Path.generic_string());
                return false;
            }
        }

        return true;
    }

    bool LoadCookedTexture(const std::filesystem::path& Path, CookedTexture& Texture, std::string* ErrorMessage)
    {
        std::ifstream Input(Path, std::ios::binary);
        if (!Input)
        {
            SetError(ErrorMessage, "Imported texture is missing, run AssetImportTool: " + Path.generic_string());
            return false;
        }

        uint32 Magic = 0;
        uint32 Version = 0;
        uint32 Format = 0;
        uint32 Encoding = 0;
        uint32 MipCount = 0;
        if (!ReadPod(Input, Magic) || Magic != TextureMagic || !ReadPod(Input, Version) || Version != CookedVersion ||
            !ReadPod(Input, Format) || !ReadPod(Input, Encoding) || !ReadPod(Input, MipCount))
        {
            SetError(ErrorMessage, "Cooked texture is invalid: " + Path.generic_string());
            return false;
        }

        Texture.Format = static_cast<ECookedTextureFormat>(Format);
        Texture.Encoding = static_cast<ECookedTextureEncoding>(Encoding);
        Texture.Mips.clear();
        Texture.Mips.reserve(MipCount);
        for (uint32 MipIndex = 0; MipIndex < MipCount; ++MipIndex)
        {
            CookedTextureMip Mip;
            uint32 ByteCount = 0;
            if (!ReadPod(Input, Mip.Width) || !ReadPod(Input, Mip.Height) || !ReadPod(Input, ByteCount))
            {
                SetError(ErrorMessage, "Cooked texture is invalid: " + Path.generic_string());
                return false;
            }

            Mip.Pixels.resize(ByteCount);
            if (!ReadValue(Input, Mip.Pixels.data(), Mip.Pixels.size()))
            {
                SetError(ErrorMessage, "Cooked texture is invalid: " + Path.generic_string());
                return false;
            }

            Texture.Mips.push_back(std::move(Mip));
        }

        return true;
    }
}
