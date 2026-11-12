using UnrealBuildTool;

public class HollywoodlandEditorTarget : TargetRules
{
	public HollywoodlandEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V8;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_9;
		ExtraModuleNames.Add("Hollywoodland");
		// Editor-only AI/MCP tooling (the web cook disables these explicitly).
		EnablePlugins.AddRange(new string[] { "AllToolsets", "Terminal", "UnrealCanvasBridge" });
	}
}
