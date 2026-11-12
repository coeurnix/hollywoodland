using UnrealBuildTool;

public class Hollywoodland : ModuleRules
{
	public Hollywoodland(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "Json" });
		// Web: FHttpModule is the null module; HLBridge.cpp calls fetch()/localStorage through EM_JS. Native uses FHttpModule + a file.
		if (Target.Platform.ToString() != "Wasm")
		{
			PrivateDependencyModuleNames.Add("HTTP");
		}
	}
}
