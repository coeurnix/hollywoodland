using UnrealBuildTool;

public class Hollywoodland : ModuleRules
{
	public Hollywoodland(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "ProceduralMeshComponent" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "Json" });
		// Web: FHttpModule is the null module; HLBridge.cpp calls fetch()/localStorage through EM_JS. Native uses FHttpModule + a file.
		if (Target.Platform.ToString() != "Wasm")
		{
			PrivateDependencyModuleNames.Add("HTTP");
		}
		// Native music playback (HLBridge::MusicPlay) uses Windows MCI.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("winmm.lib");
			// Native sound effects (HLSfx.cpp) decode MP3 with the engine's header-only dr_mp3 (compiled privately, DRMP3_API static).
			PrivateIncludePaths.Add(System.IO.Path.Combine(EngineDirectory, "Source", "ThirdParty", "dr_libs", "build"));
		}
	}
}
