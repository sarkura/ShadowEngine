using UnrealBuildTool;

public class ShadowEngineExporter : ModuleRules
{
	public ShadowEngineExporter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"GLTFExporter",
			"Json",
			"ImageCore",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"Projects",
			"DeveloperSettings"
		});
	}
}
