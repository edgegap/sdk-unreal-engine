using UnrealBuildTool;

public class EdgegapSDKEditor : ModuleRules
{
	public EdgegapSDKEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ApplicationCore",
			"DeveloperSettings",
			"EdgegapSDK",
			"EdgegapClient",
			"EdgegapServer",
			"InputCore",
			"Json",
			"Projects",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"UnrealEd",
		});
	}
}
