using UnrealBuildTool;

public class UEGTTarget : TargetRules
{
	public UEGTTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.AddRange(new string[] { "UEGTCore", "UEGTGame" });
	}
}
