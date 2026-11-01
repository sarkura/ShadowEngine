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
		"ExportShadowEngineScene",
		LOCTEXT("ExportLabel", "Export ShadowEngine Scene"),
		LOCTEXT("ExportTip", "Write StaticMesh actors and lights into this plugin's ExportData folder."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FShadowEngineSceneExporter::ExportCurrentLevel)));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FShadowEngineExporterModule, ShadowEngineExporter)
