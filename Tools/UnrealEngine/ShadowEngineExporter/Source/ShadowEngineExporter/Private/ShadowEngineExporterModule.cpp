#include "ShadowEngineExporterModule.h"

#include "ShadowEngineSceneExporter.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "ShadowEngineExporter"

void FShadowEngineExporterModule::StartupModule()
{
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FShadowEngineExporterModule::RegisterMenus));
}

void FShadowEngineExporterModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
}

void FShadowEngineExporterModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = Menu->FindOrAddSection(
		"ShadowEngine",
		LOCTEXT("ShadowEngineSection", "ShadowEngine"));
	Section.AddMenuEntry(
		"ExportShadowEngineAssets",
		LOCTEXT("ExportAssetsLabel", "Export Assets"),
		LOCTEXT("ExportAssetsTip", "Write StaticMesh actors and Config/Scene.json into this plugin's ExportData folder."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FShadowEngineSceneExporter::ExportAssets)));
	Section.AddMenuEntry(
		"ExportShadowEngineEnvironment",
		LOCTEXT("ExportEnvironmentLabel", "Export Environment"),
		LOCTEXT("ExportEnvironmentTip", "Write directional, point, area, and sky lights to Config/Environment.json."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FShadowEngineSceneExporter::ExportEnvironment)));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FShadowEngineExporterModule, ShadowEngineExporter)
