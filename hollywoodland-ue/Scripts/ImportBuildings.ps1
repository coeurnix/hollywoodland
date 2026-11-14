# Imports the building kit into /Game/Buildings: run `python -I tools/buildprep.py` first (it stages Saved/BuildStage).
param([string]$EngineRoot = 'C:\Users\theco\Projects\unreal59\UnrealEngine')
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'Hollywoodland.uproject'
$py = Join-Path $PSScriptRoot 'import_props.py'
$log = Join-Path $root 'Saved\Logs\import-buildings.log'
$env:HL_IMPORT_STAGE = Join-Path $root 'Saved\BuildStage\stage.json'
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $project "-run=pythonscript" "-script=$py" -unattended -nosplash -AllowCommandletRendering '-DisablePlugins=AllToolsets,Terminal,UnrealCanvasBridge' 2>&1 |
    Tee-Object -FilePath $log | Select-String -Pattern 'Traceback|rror:' | Select-Object -First 40 | ForEach-Object { $_.Line }
Select-String (Join-Path $root 'Saved\Logs\Hollywoodland.log') -Pattern 'HL_ASSET' | ForEach-Object { $_.Line }
Remove-Item Env:HL_IMPORT_STAGE # else a later ImportProps.ps1 in the same shell re-imports the buildings

# the master materials get their hemisphere ambient term back (Scripts\PatchAmbient.ps1)
& (Join-Path $PSScriptRoot 'PatchAmbient.ps1') -EngineRoot $EngineRoot
