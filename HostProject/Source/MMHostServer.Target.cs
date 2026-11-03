using UnrealBuildTool;

public class MMHostServerTarget : TargetRules
{
	public MMHostServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("MMHost");
	}
}
