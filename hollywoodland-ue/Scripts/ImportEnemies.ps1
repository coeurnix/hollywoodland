# Imports art/enemies (rigged enemy models + clips, destroyed hive pieces) into /Game/Enemies via Scripts\import_enemies.py.
# Then: python -I tools/enemyprep.py (writes Content/Data/enemies.json for the game).  -Only InvaderDrone,Coyote re-imports a few.
param([string]$EngineRoot = 'C:\Users\theco\Projects\unreal59\UnrealEngine', [string]$Only = '')
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'Hollywoodland.uproject'
$script = Join-Path $PSScriptRoot 'import_enemies.py'
$env:HL_ENEMIES = $Only
# a reimport over existing assets keeps their old import settings: a full run starts from an empty folder
if (-not $Only) { Remove-Item -Recurse -Force (Join-Path $root 'Content\Enemies') -ErrorAction SilentlyContinue }
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $project "-run=pythonscript" "-script=$script" -unattended -nosplash -AllowCommandletRendering '-DisablePlugins=AllToolsets,Terminal,UnrealCanvasBridge' 2>&1 |
    Tee-Object -FilePath (Join-Path $root 'Saved\Logs\import-enemies.log') |
    Select-String -Pattern 'HL_ASSET|Error|Traceback' | ForEach-Object { $_.Line }

# the master materials get their hemisphere ambient term back (Scripts\PatchAmbient.ps1)
& (Join-Path $PSScriptRoot 'PatchAmbient.ps1') -EngineRoot $EngineRoot
