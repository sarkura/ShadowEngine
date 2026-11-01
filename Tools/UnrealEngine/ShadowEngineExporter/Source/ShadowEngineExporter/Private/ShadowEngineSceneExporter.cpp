#include "ShadowEngineSceneExporter.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Exporters/GLTFExporter.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ImageUtils.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "ShadowEngineExporterSettings.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Options/GLTFExportOptions.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Utilities/GLTFCoreUtilities.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogShadowEngineExport, Log, All);

namespace
{
	constexpr float PointIntensityScale = 0.01f;
	constexpr float DefaultPointRadius = 10.f;
	constexpr double DefaultSkyIntensity = 0.25;
	const TCHAR* DefaultMaterialPath = TEXT("Assets/EngineAsset/DefaultAsset.material.json");

	struct FEngineTransform
	{
		double Translation[3] = {};
		double Rotation[3] = {};
		double Scale[3] = {};
	};

	double Round6(double Value)
	{
		const double Rounded = FMath::RoundToDouble(Value * 1000000.0) / 1000000.0;
		return Rounded == 0.0 ? 0.0 : Rounded;
	}

	void Notify(const FString& Message, bool bSuccess)
	{
		UE_LOG(LogShadowEngineExport, Display, TEXT("%s"), *Message);
		FNotificationInfo Info(FText::FromString(Message));
		Info.ExpireDuration = 8.f;
		Info.bFireAndForget = true;
		const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
		if (Item.IsValid())
		{
			Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		}
	}

	void Fail(const FString& Message)
	{
		UE_LOG(LogShadowEngineExport, Error, TEXT("%s"), *Message);
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::FromString(Message),
			FText::FromString(TEXT("ShadowEngine Export")));
	}

	FString MakeUniqueName(const FString& RawName, TSet<FString>& UsedNames)
	{
		FString Base = FPaths::MakeValidFileName(RawName, TEXT('_'));
		Base.TrimStartAndEndInline();
		if (Base.IsEmpty())
		{
			Base = TEXT("Object");
		}

		FString Name = Base;
		int32 Suffix = 2;
		while (UsedNames.Contains(Name))
		{
			Name = FString::Printf(TEXT("%s_%d"), *Base, Suffix++);
		}
		UsedNames.Add(Name);
		return Name;
	}

	FString AssetJoin(const FString& Left, const FString& Right)
	{
		return Left + TEXT("/") + Right;
	}

	FString ToDisk(const FString& ExportRoot, const FString& AssetPath)
	{
		TArray<FString> Parts;
		AssetPath.ParseIntoArray(Parts, TEXT("/"), true);
		FString Result = ExportRoot;
		for (const FString& Part : Parts)
		{
			Result = FPaths::Combine(Result, Part);
		}
		return FPaths::ConvertRelativePathToFull(Result);
	}

	bool SaveJson(const FString& AbsolutePath, const TSharedRef<FJsonObject>& Object)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		if (!FJsonSerializer::Serialize(Object, Writer))
		{
			return false;
		}
		return FFileHelper::SaveStringToFile(
			Text,
			*AbsolutePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	TArray<TSharedPtr<FJsonValue>> Number3(double X, double Y, double Z)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		Values.Add(MakeShared<FJsonValueNumber>(Round6(X)));
		Values.Add(MakeShared<FJsonValueNumber>(Round6(Y)));
		Values.Add(MakeShared<FJsonValueNumber>(Round6(Z)));
		return Values;
	}

	TSharedRef<FJsonObject> Float3Value(double X, double Y, double Z)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetArrayField(TEXT("float3"), Number3(X, Y, Z));
		return Object;
	}

	TSharedRef<FJsonObject> FloatValue(double Value)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("float"), Round6(Value));
		return Object;
	}

	TSharedRef<FJsonObject> TextureValue(const FString& TextureJsonPath)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("texture"), TextureJsonPath);
		return Object;
	}

	bool ReadScalar(UMaterialInterface* Material, std::initializer_list<const TCHAR*> Names, float& OutValue)
	{
		for (const TCHAR* Name : Names)
		{
			float Value = 0.f;
			if (Material->GetScalarParameterValue(FHashedMaterialParameterInfo(Name), Value))
			{
				OutValue = Value;
				return true;
			}
		}
		return false;
	}

	bool ReadColor(UMaterialInterface* Material, std::initializer_list<const TCHAR*> Names, FLinearColor& OutValue)
	{
		for (const TCHAR* Name : Names)
		{
			FLinearColor Value = FLinearColor::White;
			if (Material->GetVectorParameterValue(FHashedMaterialParameterInfo(Name), Value))
			{
				OutValue = Value;
				return true;
			}
		}
		return false;
	}

	enum class EPrincipledKind : uint8
	{
		Pure,
		Texture,
		Translucent
	};

	struct FPrincipledParent
	{
		EPrincipledKind Kind = EPrincipledKind::Pure;
		const TCHAR* ShadowEngine = nullptr;
		const TCHAR* Surface = nullptr;
		UMaterial* Material = nullptr;
	};

	struct FParentSet
	{
		FPrincipledParent Pure;
		FPrincipledParent Texture;
		FPrincipledParent Translucent;

		const FPrincipledParent* Find(const UMaterial* Material) const
		{
			if (Material != nullptr && Material == Pure.Material)
			{
				return &Pure;
			}
			if (Material != nullptr && Material == Texture.Material)
			{
				return &Texture;
			}
			if (Material != nullptr && Material == Translucent.Material)
			{
				return &Translucent;
			}
			return nullptr;
		}
	};

	void LoadParent(
		TSoftObjectPtr<UMaterial> Reference,
		FPrincipledParent& Out,
		const TCHAR* Label,
		TArray<FString>& Warnings)
	{
		Out.Material = Reference.LoadSynchronous();
		if (Out.Material == nullptr)
		{
			Warnings.Add(FString::Printf(
				TEXT("Principled parent %s is not set. Slots that should use it are exported as the default material."),
				Label));
		}
	}

	FParentSet LoadParents(TArray<FString>& Warnings)
	{
		const UShadowEngineExporterSettings* Settings = GetDefault<UShadowEngineExporterSettings>();
		FParentSet Parents;
		Parents.Pure.Kind = EPrincipledKind::Pure;
		Parents.Pure.ShadowEngine = TEXT("Assets/BaseMaterial/Pure.material.json");
		Parents.Pure.Surface = TEXT("Opaque");
		Parents.Texture.Kind = EPrincipledKind::Texture;
		Parents.Texture.ShadowEngine = TEXT("Assets/BaseMaterial/Texture.material.json");
		Parents.Texture.Surface = TEXT("Opaque");
		Parents.Translucent.Kind = EPrincipledKind::Translucent;
		Parents.Translucent.ShadowEngine = TEXT("Assets/BaseMaterial/Translucent.material.json");
		Parents.Translucent.Surface = TEXT("Translucent");
		LoadParent(Settings->Pure, Parents.Pure, TEXT("Pure"), Warnings);
		LoadParent(Settings->Texture, Parents.Texture, TEXT("Texture"), Warnings);
		LoadParent(Settings->Translucent, Parents.Translucent, TEXT("Translucent"), Warnings);
		return Parents;
	}

	bool ReadTexture(UMaterialInterface* Material, const TCHAR* Name, UTexture2D*& OutTexture)
	{
		UTexture* Texture = nullptr;
		if (!Material->GetTextureParameterValue(FHashedMaterialParameterInfo(Name), Texture) || Texture == nullptr)
		{
			return false;
		}
		OutTexture = Cast<UTexture2D>(Texture);
		return OutTexture != nullptr;
	}

	float ReadPointRadius(const AActor* Actor)
	{
		if (Actor != nullptr)
		{
			if (const FFloatProperty* Property = FindFProperty<FFloatProperty>(Actor->GetClass(), TEXT("Radius")))
			{
				return FMath::Max(Property->GetPropertyValue_InContainer(Actor), 0.001f);
			}
		}
		return DefaultPointRadius;
	}

	FString ActorLabel(const UActorComponent* Component)
	{
		if (const AActor* Owner = Component != nullptr ? Component->GetOwner() : nullptr)
		{
			return Owner->GetActorLabel();
		}
		return Component != nullptr ? Component->GetName() : TEXT("Light");
	}

	// Engine Transform::WriteWorldMatrix is T * Ry(yaw) * Rx(pitch) * Rz(roll) * S.
	// Scene.json stores that Euler as [pitch, yaw, roll] after the glTF axis conversion.
	FEngineTransform ConvertTransform(const FTransform& World)
	{
		const FGLTFVector3 Translation = FGLTFCoreUtilities::ConvertPosition(FVector3f(World.GetLocation()));
		const FGLTFVector3 Scale = FGLTFCoreUtilities::ConvertScale(FVector3f(World.GetScale3D()));
		const FGLTFQuaternion Quat = FGLTFCoreUtilities::ConvertRotation(FQuat4f(World.GetRotation()));

		const float X = Quat.X;
		const float Y = Quat.Y;
		const float Z = Quat.Z;
		const float W = Quat.W;
		const float XX = X * X;
		const float YY = Y * Y;
		const float ZZ = Z * Z;
		const float XY = X * Y;
		const float XZ = X * Z;
		const float YZ = Y * Z;
		const float WX = W * X;
		const float WY = W * Y;
		const float WZ = W * Z;
		const float M00 = 1.f - 2.f * (YY + ZZ);
		const float M02 = 2.f * (XZ + WY);
		const float M10 = 2.f * (XY + WZ);
		const float M11 = 1.f - 2.f * (XX + ZZ);
		const float M12 = 2.f * (YZ - WX);
		const float M20 = 2.f * (XZ - WY);
		const float M22 = 1.f - 2.f * (XX + YY);

		FEngineTransform Result;
		Result.Translation[0] = Translation.X;
		Result.Translation[1] = Translation.Y;
		Result.Translation[2] = Translation.Z;
		Result.Scale[0] = Scale.X;
		Result.Scale[1] = Scale.Y;
		Result.Scale[2] = Scale.Z;
		Result.Rotation[0] = FMath::Asin(FMath::Clamp(-M12, -1.f, 1.f));
		if (FMath::Abs(M12) < 0.999999f)
		{
			Result.Rotation[1] = FMath::Atan2(M02, M22);
			Result.Rotation[2] = FMath::Atan2(M10, M11);
		}
		else
		{
			Result.Rotation[1] = FMath::Atan2(-M20, M00);
			Result.Rotation[2] = 0.0;
		}
		return Result;
	}

	bool WriteTextureDocument(
		const FString& ExportRoot,
		const FString& JsonPath,
		const FString& ImagePath,
		const TCHAR* Encoding)
	{
		TSharedRef<FJsonObject> Document = MakeShared<FJsonObject>();
		Document->SetStringField(TEXT("Type"), TEXT("Texture"));
		Document->SetStringField(TEXT("Source"), ImagePath);
		Document->SetStringField(TEXT("Encoding"), Encoding);
		return SaveJson(ToDisk(ExportRoot, JsonPath), Document);
	}

	struct FTextureLibrary
	{
		FString ExportRoot;
		FString Directory;
		TArray<FString>& Warnings;
		TSet<FString> UsedNames;
		TMap<UTexture2D*, FString> Written;

		FString WriteMap(UTexture2D* Texture, bool bColorMap)
		{
			if (Texture == nullptr)
			{
				return FString();
			}
			if (const FString* Existing = Written.Find(Texture))
			{
				return *Existing;
			}

			const FString Name = MakeUniqueName(Texture->GetName(), UsedNames);
			const FString ImagePath = AssetJoin(Directory, Name + TEXT(".png"));
			FImage Source;
			const bool bSaved = FImageUtils::GetTexture2DSourceImage(Texture, Source) &&
				FImageUtils::SaveImageByExtension(*ToDisk(ExportRoot, ImagePath), Source);
			if (!bSaved)
			{
				Warnings.Add(FString::Printf(TEXT("%s has no readable source image."), *Texture->GetName()));
				Written.Add(Texture, FString());
				return FString();
			}

			const TCHAR* Encoding = (bColorMap && Texture->SRGB) ? TEXT("sRGB") : TEXT("Linear");
			const FString JsonPath = AssetJoin(Directory, Name + TEXT(".texture.json"));
			if (!WriteTextureDocument(ExportRoot, JsonPath, ImagePath, Encoding))
			{
				Warnings.Add(FString::Printf(TEXT("Could not write the texture description for %s."), *Texture->GetName()));
				Written.Add(Texture, FString());
				return FString();
			}

			Written.Add(Texture, JsonPath);
			return JsonPath;
		}
	};

	bool AddTextureParameter(
		UMaterialInstance* Instance,
		const TCHAR* Name,
		bool bColorMap,
		FTextureLibrary& Textures,
		TSharedRef<FJsonObject>& Parameters,
		FString& OutReason)
	{
		UTexture2D* Texture = nullptr;
		if (!ReadTexture(Instance, Name, Texture))
		{
			OutReason = FString::Printf(TEXT("is missing the %s parameter"), Name);
			return false;
		}

		const FString TextureJson = Textures.WriteMap(Texture, bColorMap);
		if (TextureJson.IsEmpty())
		{
			OutReason = FString::Printf(TEXT("could not save %s"), Name);
			return false;
		}

		Parameters->SetObjectField(Name, TextureValue(TextureJson));
		return true;
	}

	bool WritePrincipledMaterial(
		const FString& ExportRoot,
		const FString& MaterialPath,
		UMaterialInstance* Instance,
		const FPrincipledParent& Parent,
		FTextureLibrary& Textures,
		FString& OutReason)
	{
		FLinearColor BaseColor = FLinearColor::White;
		float Roughness = 0.f;
		if (!ReadColor(Instance, {TEXT("BaseColor")}, BaseColor) || !ReadScalar(Instance, {TEXT("Roughness")}, Roughness))
		{
			OutReason = TEXT("is missing the BaseColor or Roughness parameter");
			return false;
		}

		TSharedRef<FJsonObject> Parameters = MakeShared<FJsonObject>();
		Parameters->SetObjectField(TEXT("BaseColor"), Float3Value(BaseColor.R, BaseColor.G, BaseColor.B));
		if (Parent.Kind == EPrincipledKind::Texture)
		{
			float Metallic = 0.f;
			if (!ReadScalar(Instance, {TEXT("Metallic")}, Metallic))
			{
				OutReason = TEXT("is missing the Metallic parameter");
				return false;
			}
			Parameters->SetObjectField(TEXT("Metallic"), FloatValue(Metallic));
		}
		Parameters->SetObjectField(TEXT("Roughness"), FloatValue(Roughness));

		if (Parent.Kind == EPrincipledKind::Texture)
		{
			if (!AddTextureParameter(Instance, TEXT("BaseColorTexture"), true, Textures, Parameters, OutReason) ||
				!AddTextureParameter(Instance, TEXT("RoughnessTexture"), false, Textures, Parameters, OutReason) ||
				!AddTextureParameter(Instance, TEXT("MetallicTexture"), false, Textures, Parameters, OutReason) ||
				!AddTextureParameter(Instance, TEXT("NormalTexture"), false, Textures, Parameters, OutReason) ||
				!AddTextureParameter(Instance, TEXT("OcclusionTexture"), false, Textures, Parameters, OutReason))
			{
				return false;
			}
		}
		else if (Parent.Kind == EPrincipledKind::Translucent)
		{
			float Alpha = 1.f;
			if (!ReadScalar(Instance, {TEXT("Alpha")}, Alpha))
			{
				OutReason = TEXT("is missing the Alpha parameter");
				return false;
			}
			Parameters->SetObjectField(TEXT("Alpha"), FloatValue(Alpha));
		}

		TSharedRef<FJsonObject> Principled = MakeShared<FJsonObject>();
		Principled->SetStringField(TEXT("Parent"), Parent.ShadowEngine);
		Principled->SetStringField(TEXT("Surface"), Parent.Surface);
		Principled->SetBoolField(TEXT("TwoSided"), false);
		Principled->SetObjectField(TEXT("Parameters"), Parameters);

		TSharedRef<FJsonObject> Document = MakeShared<FJsonObject>();
		Document->SetStringField(TEXT("Type"), TEXT("Material"));
		Document->SetObjectField(TEXT("Principled"), Principled);
		if (!SaveJson(ToDisk(ExportRoot, MaterialPath), Document))
		{
			OutReason = TEXT("could not be written");
			return false;
		}
		return true;
	}

	bool ExportMeshAsset(
		const FString& ExportRoot,
		UStaticMesh* Mesh,
		const TArray<UMaterialInterface*>& Materials,
		const TArray<FString>& ActorLabels,
		const FString& MeshFolder,
		const FString& AssetName,
		const FParentSet& Parents,
		TMap<UStaticMesh*, FString>& GlbPaths,
		TSet<FString>& MaterialNames,
		FString& OutAssetPath,
		TArray<FString>& Warnings)
	{
		const FString Root = FString::Printf(TEXT("Assets/Mesh/%s"), *MeshFolder);
		FString GlbPath;
		if (const FString* Existing = GlbPaths.Find(Mesh))
		{
			GlbPath = *Existing;
		}
		else
		{
			GlbPath = FString::Printf(TEXT("%s/Mesh/%s.glb"), *Root, *MeshFolder);
			const FString GlbAbsolute = ToDisk(ExportRoot, GlbPath);
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(GlbAbsolute), true);

			UGLTFExportOptions* Options = NewObject<UGLTFExportOptions>();
			Options->ExportUniformScale = 0.01f;
			Options->TextureImageFormat = EGLTFTextureImageFormat::None;
			Options->BakeMaterialInputs = EGLTFMaterialBakeMode::Disabled;
			Options->bExportVertexColors = false;
			Options->bExportLights = false;
			Options->bExportCameras = false;
			Options->bExportPreviewMesh = false;

			FGLTFExportMessages Messages;
			TSet<AActor*> SelectedActors;
			if (!UGLTFExporter::ExportToGLTF(Mesh, GlbAbsolute, Options, SelectedActors, Messages))
			{
				FString Detail = FString::Printf(TEXT("glTF export failed for %s."), *Mesh->GetName());
				for (const FString& Error : Messages.Errors)
				{
					Detail += TEXT(" ") + Error;
				}
				Warnings.Add(Detail);
				return false;
			}
			GlbPaths.Add(Mesh, GlbPath);
		}

		OutAssetPath = FString::Printf(TEXT("%s/Mesh/%s.asset.json"), *Root, *AssetName);
		FTextureLibrary Textures{ExportRoot, AssetJoin(Root, TEXT("Texture")), Warnings};
		TArray<TSharedPtr<FJsonValue>> MaterialEntries;
		const FString Actors = FString::Join(ActorLabels, TEXT(", "));
		const int32 SlotCount = FMath::Max(Materials.Num(), 1);
		for (int32 Index = 0; Index < SlotCount; ++Index)
		{
			UMaterialInterface* Material = Materials.IsValidIndex(Index) ? Materials[Index] : nullptr;
			UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
			const FPrincipledParent* Parent = Instance != nullptr ? Parents.Find(Instance->GetBaseMaterial()) : nullptr;
			FString MaterialPath = DefaultMaterialPath;
			if (Parent == nullptr)
			{
				if (Material == nullptr)
				{
					Warnings.Add(FString::Printf(
						TEXT("%s slot %d has no material. The default material is used."),
						*Actors,
						Index));
				}
				else if (Instance == nullptr)
				{
					Warnings.Add(FString::Printf(
						TEXT("%s slot %d material %s is not a material instance. The default material is used."),
						*Actors,
						Index,
						*Material->GetName()));
				}
				else
				{
					Warnings.Add(FString::Printf(
						TEXT("%s slot %d material %s does not use a Principled parent from the plugin settings. The default material is used."),
						*Actors,
						Index,
						*Material->GetName()));
				}
			}
			else
			{
				const FString MaterialName = MakeUniqueName(Material->GetName(), MaterialNames);
				const FString Relative = FString::Printf(TEXT("%s/Material/%s.material.json"), *Root, *MaterialName);
				FString Reason;
				if (WritePrincipledMaterial(ExportRoot, Relative, Instance, *Parent, Textures, Reason))
				{
					MaterialPath = Relative;
				}
				else
				{
					Warnings.Add(FString::Printf(
						TEXT("%s slot %d material %s %s. The default material is used."),
						*Actors,
						Index,
						*Material->GetName(),
						*Reason));
				}
			}

			TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetNumberField(TEXT("slot"), Index);
			Entry->SetStringField(TEXT("material"), MaterialPath);
			MaterialEntries.Add(MakeShared<FJsonValueObject>(Entry));
		}

		TSharedRef<FJsonObject> Asset = MakeShared<FJsonObject>();
		Asset->SetStringField(TEXT("type"), TEXT("Mesh"));
		Asset->SetStringField(TEXT("source"), GlbPath);
		Asset->SetArrayField(TEXT("materials"), MaterialEntries);
		return SaveJson(ToDisk(ExportRoot, OutAssetPath), Asset);
	}

	void CollectLights(UWorld* World, TArray<TSharedPtr<FJsonValue>>& OutLights, TArray<FString>& Warnings)
	{
		bool bWroteDirect = false;
		bool bWroteSky = false;
		int32 SkippedSpots = 0;
		int32 SkippedRects = 0;

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			TInlineComponentArray<ULightComponentBase*> Components;
			It->GetComponents(Components);
			for (ULightComponentBase* Component : Components)
			{
				if (UDirectionalLightComponent* Directional = Cast<UDirectionalLightComponent>(Component))
				{
					if (bWroteDirect)
					{
						Warnings.Add(FString::Printf(
							TEXT("Skipped extra directional light %s; the scene allows one DirectLight."),
							*ActorLabel(Directional)));
						continue;
					}

					const FGLTFVector3 Converted = FGLTFCoreUtilities::ConvertVector(FVector3f(Directional->GetDirection()));
					FVector Direction(Converted.X, Converted.Y, Converted.Z);
					if (!Direction.Normalize())
					{
						Warnings.Add(FString::Printf(
							TEXT("Skipped directional light %s; its direction is zero."),
							*ActorLabel(Directional)));
						continue;
					}

					const FLinearColor Color = Directional->GetLightColor();
					TSharedRef<FJsonObject> Light = MakeShared<FJsonObject>();
					Light->SetStringField(TEXT("Type"), TEXT("DirectLight"));
					Light->SetArrayField(TEXT("Direction"), Number3(Direction.X, Direction.Y, Direction.Z));
					Light->SetArrayField(TEXT("Color"), Number3(Color.R, Color.G, Color.B));
					Light->SetNumberField(TEXT("Intensity"), Round6(Directional->Intensity));
					OutLights.Add(MakeShared<FJsonValueObject>(Light));
					bWroteDirect = true;
				}
				else if (Cast<USkyLightComponent>(Component))
				{
					if (bWroteSky)
					{
						Warnings.Add(FString::Printf(
							TEXT("Skipped extra sky light %s; the scene allows one SkyLight."),
							*ActorLabel(Component)));
						continue;
					}

					const FLinearColor Color = Component->GetLightColor();
					TSharedRef<FJsonObject> Light = MakeShared<FJsonObject>();
					Light->SetStringField(TEXT("Type"), TEXT("SkyLight"));
					Light->SetArrayField(TEXT("Color"), Number3(Color.R, Color.G, Color.B));
					Light->SetNumberField(TEXT("Intensity"), Round6(FMath::Max(Component->Intensity, 0.f)));
					OutLights.Add(MakeShared<FJsonValueObject>(Light));
					bWroteSky = true;
				}
				else if (Cast<USpotLightComponent>(Component))
				{
					++SkippedSpots;
				}
				else if (Cast<URectLightComponent>(Component))
				{
					++SkippedRects;
				}
				else if (UPointLightComponent* Point = Cast<UPointLightComponent>(Component))
				{
					const FGLTFVector3 Position = FGLTFCoreUtilities::ConvertPosition(FVector3f(Point->GetComponentLocation()));
					const FLinearColor Color = Point->GetLightColor();
					TSharedRef<FJsonObject> Light = MakeShared<FJsonObject>();
					Light->SetStringField(TEXT("Type"), TEXT("PointLight"));
					Light->SetArrayField(TEXT("Position"), Number3(Position.X, Position.Y, Position.Z));
					Light->SetArrayField(TEXT("Color"), Number3(Color.R, Color.G, Color.B));
					Light->SetNumberField(TEXT("Intensity"), Round6(FMath::Max(Point->Intensity, 0.f) * PointIntensityScale));
					Light->SetNumberField(TEXT("Radius"), Round6(ReadPointRadius(Point->GetOwner())));
					OutLights.Add(MakeShared<FJsonValueObject>(Light));
				}
			}
		}

		if (!bWroteSky)
		{
			TSharedRef<FJsonObject> Light = MakeShared<FJsonObject>();
			Light->SetStringField(TEXT("Type"), TEXT("SkyLight"));
			Light->SetArrayField(TEXT("Color"), Number3(0.45, 0.55, 0.75));
			Light->SetNumberField(TEXT("Intensity"), DefaultSkyIntensity);
			OutLights.Add(MakeShared<FJsonValueObject>(Light));
		}
		if (SkippedSpots > 0)
		{
			Warnings.Add(FString::Printf(TEXT("Skipped %d spot light(s)."), SkippedSpots));
		}
		if (SkippedRects > 0)
		{
			Warnings.Add(FString::Printf(TEXT("Skipped %d rect light(s)."), SkippedRects));
		}
	}
}

void FShadowEngineSceneExporter::ExportCurrentLevel()
{
	UWorld* World = GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World == nullptr)
	{
		Fail(TEXT("No editor world is open."));
		return;
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ShadowEngineExporter"));
	if (!Plugin.IsValid())
	{
		Fail(TEXT("ShadowEngineExporter plugin was not found."));
		return;
	}

	if (!FModuleManager::Get().IsModuleLoaded(TEXT("GLTFExporter")))
	{
		FModuleManager::Get().LoadModule(TEXT("GLTFExporter"));
	}

	const FString ExportRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(Plugin->GetBaseDir(), TEXT("ExportData")));
	if (IFileManager::Get().DirectoryExists(*ExportRoot) &&
		!IFileManager::Get().DeleteDirectory(*ExportRoot, false, true))
	{
		Fail(FString::Printf(TEXT("Could not clear %s."), *ExportRoot));
		return;
	}
	IFileManager::Get().MakeDirectory(*ExportRoot, true);
	FFileHelper::SaveStringToFile(
		FString(),
		*FPaths::Combine(ExportRoot, TEXT(".gitkeep")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	struct FMeshGroup
	{
		UStaticMesh* Mesh = nullptr;
		TArray<UMaterialInterface*> Materials;
		TArray<FString> ActorLabels;
		TArray<FEngineTransform> Transforms;
	};

	TArray<FString> Warnings;
	const FParentSet Parents = LoadParents(Warnings);

	TMap<FString, int32> GroupIndex;
	TArray<FMeshGroup> Groups;
	int32 InstanceCount = 0;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		UStaticMeshComponent* Component = It->GetStaticMeshComponent();
		UStaticMesh* Mesh = Component != nullptr ? Component->GetStaticMesh() : nullptr;
		if (Mesh == nullptr)
		{
			continue;
		}

		const int32 SlotCount = FMath::Max(Component->GetNumMaterials(), 1);
		FString Key = Mesh->GetPathName();
		TArray<UMaterialInterface*> Materials;
		Materials.Reserve(SlotCount);
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			UMaterialInterface* Material = Component->GetMaterial(Slot);
			Materials.Add(Material);
			Key += TEXT("\n");
			Key += Material != nullptr ? Material->GetPathName() : FString();
		}

		int32 Index = INDEX_NONE;
		if (const int32* Found = GroupIndex.Find(Key))
		{
			Index = *Found;
		}
		else
		{
			FMeshGroup& Added = Groups.AddDefaulted_GetRef();
			Added.Mesh = Mesh;
			Added.Materials = Materials;
			Index = Groups.Num() - 1;
			GroupIndex.Add(Key, Index);
		}
		Groups[Index].ActorLabels.Add(It->GetActorLabel());
		Groups[Index].Transforms.Add(ConvertTransform(Component->GetComponentTransform()));
		++InstanceCount;
	}

	TSharedRef<FJsonObject> MeshObject = MakeShared<FJsonObject>();
	TSet<FString> MeshNames;
	TMap<UStaticMesh*, FString> MeshFolders;
	TMap<UStaticMesh*, int32> AssetCounts;
	TMap<UStaticMesh*, FString> GlbPaths;
	TMap<UStaticMesh*, TSet<FString>> MaterialNames;
	for (const FMeshGroup& Group : Groups)
	{
		FString& Folder = MeshFolders.FindOrAdd(Group.Mesh);
		if (Folder.IsEmpty())
		{
			Folder = MakeUniqueName(Group.Mesh->GetName(), MeshNames);
		}
		int32& Count = AssetCounts.FindOrAdd(Group.Mesh);
		const FString AssetName = Count == 0 ? Folder : FString::Printf(TEXT("%s_%d"), *Folder, Count + 1);
		++Count;

		FString AssetPath;
		if (!ExportMeshAsset(
				ExportRoot,
				Group.Mesh,
				Group.Materials,
				Group.ActorLabels,
				Folder,
				AssetName,
				Parents,
				GlbPaths,
				MaterialNames.FindOrAdd(Group.Mesh),
				AssetPath,
				Warnings))
		{
			Fail(Warnings.Num() > 0 ? FString::Join(Warnings, TEXT("\n")) : TEXT("Mesh export failed."));
			return;
		}

		TArray<TSharedPtr<FJsonValue>> Instances;
		for (const FEngineTransform& Transform : Group.Transforms)
		{
			TSharedRef<FJsonObject> TransformObject = MakeShared<FJsonObject>();
			TransformObject->SetArrayField(
				TEXT("Translation"),
				Number3(Transform.Translation[0], Transform.Translation[1], Transform.Translation[2]));
			TransformObject->SetArrayField(
				TEXT("Rotation"),
				Number3(Transform.Rotation[0], Transform.Rotation[1], Transform.Rotation[2]));
			TransformObject->SetArrayField(
				TEXT("Scale"),
				Number3(Transform.Scale[0], Transform.Scale[1], Transform.Scale[2]));

			TSharedRef<FJsonObject> Instance = MakeShared<FJsonObject>();
			Instance->SetObjectField(TEXT("Transform"), TransformObject);
			Instances.Add(MakeShared<FJsonValueObject>(Instance));
		}
		MeshObject->SetArrayField(AssetPath, Instances);
	}

	TArray<TSharedPtr<FJsonValue>> Lights;
	CollectLights(World, Lights, Warnings);

	TSharedRef<FJsonObject> Scene = MakeShared<FJsonObject>();
	Scene->SetObjectField(TEXT("Mesh"), MeshObject);
	Scene->SetArrayField(TEXT("Light"), Lights);
	const FString ScenePath = ToDisk(ExportRoot, TEXT("Config/Scene.json"));
	if (!SaveJson(ScenePath, Scene))
	{
		Fail(FString::Printf(TEXT("Could not write %s."), *ScenePath));
		return;
	}

	for (const FString& Warning : Warnings)
	{
		UE_LOG(LogShadowEngineExport, Warning, TEXT("%s"), *Warning);
	}

	FString Message = FString::Printf(
		TEXT("Exported %d mesh(es), %d instance(s) to %s."),
		Groups.Num(),
		InstanceCount,
		*ScenePath);
	if (Warnings.Num() > 0)
	{
		Message += FString::Printf(TEXT(" %d warning(s) are in the Output Log."), Warnings.Num());
	}
	Notify(Message, Warnings.Num() == 0);
}
