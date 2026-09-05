using UnrealBuildTool;

public class UEGTEditorTarget : TargetRules
{
	public UEGTEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.AddRange(new string[] { "UEGTCore", "UEGTGame" });
	}
}
