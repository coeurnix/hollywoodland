# Adds the hemisphere ambient term (Scripts\patch_ambient.py) to the lit master materials. Safe to re-run: patched
# materials are skipped. Run it after any import script that rebuilds a master material (characters, props, enemies).
param([string]$EngineRoot = 'C:\Users\theco\Projects\unreal59\UnrealEngine')
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'Hollywoodland.uproject'
$script = Join-Path $PSScriptRoot 'patch_ambient.py'
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $project "-run=pythonscript" "-script=$script" -unattended -nosplash -AllowCommandletRendering '-DisablePlugins=AllToolsets,Terminal,UnrealCanvasBridge' 2>&1 |
    Tee-Object -FilePath (Join-Path $root 'Saved\Logs\patch-ambient.log') |
    Select-String -Pattern 'HL_AMBIENT|HL_ASSET|Error|Traceback' | ForEach-Object { $_.Line }
