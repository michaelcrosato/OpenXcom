using UnrealBuildTool;

public class UEGTCore : ModuleRules
{
	public UEGTCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Keep deterministic domain and automation sources in this dependency-light module.

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Json"
		});
	}
}
