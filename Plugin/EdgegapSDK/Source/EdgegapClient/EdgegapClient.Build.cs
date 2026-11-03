using UnrealBuildTool;

public class EdgegapClient : ModuleRules
{
	public EdgegapClient(ReadOnlyTargetRules Target) : base(Target)
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
