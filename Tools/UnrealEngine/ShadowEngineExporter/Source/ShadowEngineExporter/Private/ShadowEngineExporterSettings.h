#pragma once

#include "Engine/DeveloperSettings.h"
#include "Materials/Material.h"
#include "ShadowEngineExporterSettings.generated.h"

UCLASS(config = ShadowEngineExporter, defaultconfig, meta = (DisplayName = "ShadowEngine Exporter"))
class UShadowEngineExporterSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }

	UPROPERTY(config, EditAnywhere, Category = "Principled", meta = (DisplayName = "Pure"))
	TSoftObjectPtr<UMaterial> Pure;

	UPROPERTY(config, EditAnywhere, Category = "Principled", meta = (DisplayName = "Texture"))
	TSoftObjectPtr<UMaterial> Texture;

	UPROPERTY(config, EditAnywhere, Category = "Principled", meta = (DisplayName = "Translucent"))
	TSoftObjectPtr<UMaterial> Translucent;
};
