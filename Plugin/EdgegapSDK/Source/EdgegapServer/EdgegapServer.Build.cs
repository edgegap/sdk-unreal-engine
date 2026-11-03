using UnrealBuildTool;

public class EdgegapServer : ModuleRules
{
	public EdgegapServer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"EdgegapSDK",
			"HTTP",
			"Json",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"DeveloperSettings",
			"Sockets",
		});
	}
}
