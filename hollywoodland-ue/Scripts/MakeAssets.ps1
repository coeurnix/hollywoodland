# Runs Scripts\make_assets.py in the editor commandlet (creates /Game/HL materials). Needs the editor target built.
param([string]$EngineRoot = 'C:\Users\theco\Projects\unreal59\UnrealEngine')
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'Hollywoodland.uproject'
$script = Join-Path $PSScriptRoot 'make_assets.py'
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $project "-run=pythonscript" "-script=$script" -unattended -nosplash -AllowCommandletRendering '-DisablePlugins=AllToolsets,Terminal,UnrealCanvasBridge' 2>&1 |
    Select-String -Pattern 'HL_ASSET|Error|Traceback' | ForEach-Object { $_.Line }
Get-ChildItem (Join-Path $root 'Content\HL') -ErrorAction SilentlyContinue | Select-Object Name, Length
