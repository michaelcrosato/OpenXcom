using UnrealBuildTool;

public class UEGTGame : ModuleRules
{
	public UEGTGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			// Runtime presentation is native Slate/UMG over deterministic UEGTCore snapshots.
			"Core",
			"CoreUObject",
			"Engine",
			"EnhancedInput",
			"InputCore",
			"Json",
			"Slate",
			"SlateCore",
			"UMG",
			"UEGTCore"
		});

		// The private procedural wave subclass uses Unreal's audio proxy interface.
		PrivateDependencyModuleNames.Add("AudioExtensions");
	}
}
