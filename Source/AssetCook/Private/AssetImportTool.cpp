#include "AssetCook/Public/AssetCook.h"

#include "ThirdPartyAdapter/Serialization/Public/RapidJSONAdapter.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    bool ReadText(const std::filesystem::path& Path, std::string& Text, std::string* ErrorMessage)
    {
        std::ifstream Input(Path, std::ios::binary);
        if (!Input)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Unable to open " + Path.generic_string();
            }
            return false;
        }

        Text.assign(std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>());
        return true;
    }

    bool EndsWith(const std::string& Value, const std::string& Suffix)
    {
        return Value.size() >= Suffix.size() && Value.compare(Value.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
    }

    bool CopyImportedFile(const std::filesystem::path& From, const std::filesystem::path& To, std::string* ErrorMessage)
    {
        std::error_code Error;
        std::filesystem::create_directories(To.parent_path(), Error);
        std::filesystem::copy_file(From, To, std::filesystem::copy_options::overwrite_existing, Error);
        if (Error)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Unable to copy " + From.generic_string() + " to " + To.generic_string();
            }
            return false;
        }

        return true;
    }

    bool WriteTextureDescription(
        const std::filesystem::path& Destination,
        const std::filesystem::path& CookedSource,
        const std::string& Encoding,
        std::string* ErrorMessage)
    {
        std::error_code Error;
        std::filesystem::create_directories(Destination.parent_path(), Error);
        std::ofstream Output(Destination, std::ios::binary | std::ios::trunc);
        if (!Output)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Unable to write " + Destination.generic_string();
            }
            return false;
        }

        Output << "{\n"
               << "    \"Type\": \"Texture\",\n"
               << "    \"Source\": \"" << CookedSource.generic_string() << "\",\n"
               << "    \"Encoding\": \"" << Encoding << "\"\n"
               << "}\n";
        if (!Output)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Unable to write " + Destination.generic_string();
            }
            return false;
        }

        return true;
    }

    bool MaterialUsesTexture(const std::filesystem::path& Path)
    {
        std::string Text;
        if (!ReadText(Path, Text, nullptr))
        {
            return false;
        }

        return Text.find("\"texture\"") != std::string::npos;
    }

    bool CleanMeshAssets(const std::filesystem::path& ProjectRoot, std::string* ErrorMessage)
    {
        const std::filesystem::path MeshRoot = ProjectRoot / "Assets" / "Mesh";
        std::error_code Error;
        std::filesystem::remove_all(MeshRoot, Error);
        if (Error)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Unable to clean " + MeshRoot.generic_string();
            }
            return false;
        }

        std::filesystem::create_directories(MeshRoot, Error);
        if (Error)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Unable to create " + MeshRoot.generic_string();
            }
            return false;
        }

        std::cout << "Cleaned " << MeshRoot.generic_string() << '\n';
        return true;
    }

    bool CookExport(const std::filesystem::path& ExportRoot, const std::filesystem::path& ProjectRoot, std::string* ErrorMessage)
    {
        const std::filesystem::path AssetRoot = ExportRoot / "Assets";
        if (!std::filesystem::exists(AssetRoot))
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Export is missing Assets: " + ExportRoot.generic_string();
            }
            return false;
        }

        for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(AssetRoot))
        {
            if (!Entry.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path Relative = std::filesystem::relative(Entry.path(), ExportRoot);
            const std::string Name = Entry.path().generic_string();
            std::string Error;
            if (EndsWith(Name, ".json"))
            {
                if (!CopyImportedFile(Entry.path(), ProjectRoot / Relative, ErrorMessage))
                {
                    return false;
                }
            }

            if (EndsWith(Name, ".asset.json"))
            {
                std::string Text;
                if (!ReadText(Entry.path(), Text, &Error))
                {
                    if (ErrorMessage != nullptr)
                    {
                        *ErrorMessage = Error;
                    }
                    return false;
                }

                ShadowEngine::RapidJSONDocument Document;
                if (!Document.Parse(Text.data(), Text.size(), &Error) || !Document.IsObject() || !Document.HasMember("source") || !Document.HasMember("materials"))
                {
                    if (ErrorMessage != nullptr)
                    {
                        *ErrorMessage = "Unable to read mesh description: " + Name;
                    }
                    return false;
                }

                const ShadowEngine::RapidJSONValue Source = Document["source"];
                const ShadowEngine::RapidJSONValue Materials = Document["materials"];
                if (!Source.IsString() || !Materials.IsArray())
                {
                    if (ErrorMessage != nullptr)
                    {
                        *ErrorMessage = "Mesh description is invalid: " + Name;
                    }
                    return false;
                }

                std::vector<ShadowEngine::CookMeshSlot> Slots;
                Slots.reserve(Materials.Size());
                for (size_t Index = 0; Index < Materials.Size(); ++Index)
                {
                    const ShadowEngine::RapidJSONValue Item = Materials[Index];
                    ShadowEngine::CookMeshSlot Slot;
                    Slot.Slot = Item["slot"].GetUint();
                    const std::string Material = Item["material"].GetString();
                    Slot.bRequireTexCoord = MaterialUsesTexture(ExportRoot / Material);
                    Slots.push_back(Slot);
                }

                const std::filesystem::path SourcePath = ExportRoot / Source.GetString();
                const std::filesystem::path Output = ProjectRoot / ShadowEngine::CookedMeshPath(Source.GetString());
                if (!ShadowEngine::CookMesh(SourcePath, Slots, Output, &Error))
                {
                    if (ErrorMessage != nullptr)
                    {
                        *ErrorMessage = Error;
                    }
                    return false;
                }

                std::cout << "Cooked mesh " << Output.generic_string() << '\n';
            }
            else if (EndsWith(Name, ".texture.json"))
            {
                std::string Text;
                if (!ReadText(Entry.path(), Text, &Error))
                {
                    if (ErrorMessage != nullptr)
                    {
                        *ErrorMessage = Error;
                    }
                    return false;
                }

                ShadowEngine::RapidJSONDocument Document;
                if (!Document.Parse(Text.data(), Text.size(), &Error) || !Document.HasMember("Source"))
                {
                    if (ErrorMessage != nullptr)
                    {
                        *ErrorMessage = "Unable to read texture description: " + Name;
                    }
                    return false;
                }

                const ShadowEngine::RapidJSONValue Source = Document["Source"];
                ShadowEngine::ECookedTextureEncoding Encoding = ShadowEngine::ECookedTextureEncoding::Linear;
                if (Document.HasMember("Encoding") && Document["Encoding"].IsString() && std::string(Document["Encoding"].GetString()) == "sRGB")
                {
                    Encoding = ShadowEngine::ECookedTextureEncoding::sRGB;
                }

                const std::filesystem::path SourcePath = ExportRoot / Source.GetString();
                const std::filesystem::path CookedSource = ShadowEngine::CookedTexturePath(Source.GetString());
                const std::filesystem::path Output = ProjectRoot / CookedSource;
                if (!ShadowEngine::CookTexture(SourcePath, Encoding, Output, &Error))
                {
                    if (ErrorMessage != nullptr)
                    {
                        *ErrorMessage = Error;
                    }
                    return false;
                }

                const std::string EncodingName = Encoding == ShadowEngine::ECookedTextureEncoding::sRGB ? "sRGB" : "Linear";
                if (!WriteTextureDescription(ProjectRoot / Relative, CookedSource, EncodingName, ErrorMessage))
                {
                    return false;
                }

                std::cout << "Cooked texture " << Output.generic_string() << '\n';
            }
        }

        return true;
    }

    bool WriteEnvironmentConfig(const std::filesystem::path& ProjectRoot, const std::filesystem::path& FileName, std::string* ErrorMessage)
    {
        const std::filesystem::path ConfigPath = ProjectRoot / "Config" / "GlobalTexture.json";
        std::error_code Error;
        std::filesystem::create_directories(ConfigPath.parent_path(), Error);
        std::ofstream Output(ConfigPath, std::ios::binary | std::ios::trunc);
        if (!Output)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Unable to write " + ConfigPath.generic_string();
            }
            return false;
        }

        Output << "{\n    \"EnvironmentMap\": \"Assets/Environment/" << FileName.generic_string() << "\"\n}\n";
        if (!Output)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Unable to write " + ConfigPath.generic_string();
            }
            return false;
        }

        std::cout << "Wrote environment config " << ConfigPath.generic_string() << '\n';
        return true;
    }

    bool ImportEnvironment(const std::filesystem::path& InputDirectory, const std::filesystem::path& ProjectRoot, std::string* ErrorMessage)
    {
        if (!InputDirectory.is_absolute())
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Environment input must be an absolute directory: " + InputDirectory.generic_string();
            }
            return false;
        }

        if (!std::filesystem::is_directory(InputDirectory))
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Environment input directory does not exist: " + InputDirectory.generic_string();
            }
            return false;
        }

        std::vector<std::filesystem::path> Sources;
        for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(InputDirectory))
        {
            if (Entry.is_regular_file() && EndsWith(Entry.path().generic_string(), ".hdr"))
            {
                Sources.push_back(Entry.path());
            }
        }

        if (Sources.size() != 1)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Environment input directory must contain one .hdr file: " + InputDirectory.generic_string();
            }
            return false;
        }

        const std::filesystem::path OutputDirectory = ProjectRoot / "Assets" / "Environment";
        const std::filesystem::path Output = OutputDirectory / Sources[0].filename().replace_extension(".setex");
        std::string Error;
        if (!ShadowEngine::CookEnvironment(Sources[0], Output, &Error))
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = Error;
            }
            return false;
        }

        std::cout << "Cooked environment " << Output.generic_string() << '\n';
        return WriteEnvironmentConfig(ProjectRoot, Sources[0].filename(), ErrorMessage);
    }

    bool ClearScene(const std::filesystem::path& ProjectRoot, std::string* ErrorMessage)
    {
        if (!CleanMeshAssets(ProjectRoot, ErrorMessage))
        {
            return false;
        }

        const std::filesystem::path ScenePath = ProjectRoot / "Config" / "Scene.json";
        std::error_code Error;
        std::filesystem::create_directories(ScenePath.parent_path(), Error);
        std::ofstream Output(ScenePath, std::ios::binary | std::ios::trunc);
        if (!Output)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Unable to write " + ScenePath.generic_string();
            }
            return false;
        }

        Output << "{\n    \"Mesh\": {},\n    \"Light\": []\n}\n";
        if (!Output)
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Unable to write " + ScenePath.generic_string();
            }
            return false;
        }

        std::cout << "Cleared scene " << ScenePath.generic_string() << '\n';
        return true;
    }

    bool CopyConfig(const std::filesystem::path& ExportRoot, const std::filesystem::path& ProjectRoot, std::string* ErrorMessage)
    {
        const std::filesystem::path ConfigRoot = ExportRoot / "Config";
        if (!std::filesystem::exists(ConfigRoot))
        {
            if (ErrorMessage != nullptr)
            {
                *ErrorMessage = "Export is missing Config: " + ExportRoot.generic_string();
            }
            return false;
        }

        for (const std::filesystem::directory_entry& Entry : std::filesystem::recursive_directory_iterator(ConfigRoot))
        {
            if (!Entry.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path Relative = std::filesystem::relative(Entry.path(), ExportRoot);
            const std::filesystem::path Destination = ProjectRoot / Relative;
            if (!CopyImportedFile(Entry.path(), Destination, ErrorMessage))
            {
                return false;
            }

            std::cout << "Copied config " << Destination.generic_string() << '\n';
        }

        return true;
    }
}

int main(int ArgumentCount, char** Arguments)
{
    if (ArgumentCount < 2)
    {
        std::cerr << "Usage: AssetImportTool scene <ExportDirectory> <ProjectRoot>\n";
        std::cerr << "       AssetImportTool environment <InputDirectory> <ProjectRoot>\n";
        std::cerr << "       AssetImportTool clear <ProjectRoot>\n";
        return 1;
    }

    const std::string Mode = Arguments[1];
    std::string Error;
    if (Mode == "clear")
    {
        if (ArgumentCount < 3)
        {
            std::cerr << "Usage: AssetImportTool clear <ProjectRoot>\n";
            return 1;
        }

        const std::filesystem::path ProjectRoot = Arguments[2];
        if (!ClearScene(ProjectRoot, &Error))
        {
            std::cerr << Error << '\n';
            return 1;
        }

        return 0;
    }

    if (ArgumentCount < 4)
    {
        std::cerr << "Usage: AssetImportTool scene <ExportDirectory> <ProjectRoot>\n";
        std::cerr << "       AssetImportTool environment <InputDirectory> <ProjectRoot>\n";
        std::cerr << "       AssetImportTool clear <ProjectRoot>\n";
        return 1;
    }

    const std::filesystem::path Input = Arguments[2];
    const std::filesystem::path ProjectRoot = Arguments[3];
    if (Mode == "scene")
    {
        if (!std::filesystem::is_directory(Input))
        {
            std::cerr << "Export directory does not exist: " << Input.generic_string() << '\n';
            return 1;
        }

        if (!CleanMeshAssets(ProjectRoot, &Error) || !CookExport(Input, ProjectRoot, &Error) || !CopyConfig(Input, ProjectRoot, &Error))
        {
            std::cerr << Error << '\n';
            return 1;
        }

        return 0;
    }

    if (Mode == "environment")
    {
        if (!ImportEnvironment(Input, ProjectRoot, &Error))
        {
            std::cerr << Error << '\n';
            return 1;
        }

        return 0;
    }

    std::cerr << "Unknown import mode: " << Mode << '\n';
    return 1;
}
