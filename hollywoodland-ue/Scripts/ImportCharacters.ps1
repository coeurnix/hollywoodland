# Imports the character package into /Game/Characters: run tools/charprep.py first (it stages Saved/CharStage).
# -Only mat,tex,mesh,anim limits the steps. Then Scripts\RetargetAnims.ps1 for the Mannequin-derived clips.
# -Reset first clears the generated meshes, clips and rigs (an import over an existing skeleton keeps its old bind pose).
param([string]$EngineRoot = 'C:\Users\theco\Projects\unreal59\UnrealEngine', [string]$Only = '', [string]$Script = 'import_characters.py', [switch]$Reset)
$root = Split-Path $PSScriptRoot -Parent
if ($Reset) {
    foreach ($f in 'Body', 'Hair', 'Hats', 'Held', 'Anim', 'Rigs') {
        $dir = Join-Path $root "Content\Characters\$f"
        if (Test-Path $dir) { Get-ChildItem $dir -Recurse -File -Filter *.uasset | ForEach-Object { Remove-Item -LiteralPath $_.FullName } }
    }
}
$project = Join-Path $root 'Hollywoodland.uproject'
$py = Join-Path $PSScriptRoot $Script
$env:HL_IMPORT_ONLY = $Only
$log = Join-Path $root 'Saved\Logs\import-characters.log'
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $project "-run=pythonscript" "-script=$py" -unattended -nosplash -AllowCommandletRendering '-DisablePlugins=AllToolsets,Terminal,UnrealCanvasBridge' 2>&1 |
    Tee-Object -FilePath $log | Select-String -Pattern 'Traceback|rror:' | Select-Object -First 40 | ForEach-Object { $_.Line }
Select-String (Join-Path $root 'Saved\Logs\Hollywoodland.log') -Pattern 'HL_ASSET' | ForEach-Object { $_.Line }

