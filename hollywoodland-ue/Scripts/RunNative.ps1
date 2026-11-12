# Runs the game natively from the editor binary (uncooked, fast iteration). Extra args are passed through,
# e.g.  .\Scripts\RunNative.ps1 -HLAuto=veteran -HLAutoQuit=90   (self-playing smoke test, screenshots in Saved\Screenshots)
param([string]$EngineRoot = 'C:\Users\theco\Projects\unreal59\UnrealEngine', [Parameter(ValueFromRemainingArguments = $true)]$Rest)
$project = Join-Path (Split-Path $PSScriptRoot -Parent) 'Hollywoodland.uproject'
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') $project -game -windowed -ResX=1600 -ResY=900 -log '-DisablePlugins=AllToolsets,Terminal,UnrealCanvasBridge' @Rest
