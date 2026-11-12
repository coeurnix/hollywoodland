using UnrealBuildTool;

public class HollywoodlandTarget : TargetRules
{
	public HollywoodlandTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V8;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_9;
		ExtraModuleNames.Add("Hollywoodland");

		if (Platform.ToString() == "Wasm")
		{
			// Web game: WebMinimal trim profile. All gameplay is C++ + Slate; no nav mesh, no StateTree.
			BuildEnvironment = TargetBuildEnvironment.Unique;
			ApplyTrimProfile("WebMinimal");
			EnablePlugins.Add("WasmDeviceProfileSelector");
			EnablePlugins.Remove("GameplayStateTree");
			bWithServerCode = true; // standalone play needs GameMode/authority code
		}
	}
}
