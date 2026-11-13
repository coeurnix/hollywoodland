# Builds, cooks and packages a standalone Windows executable into win-dist-<Configuration>\Windows\Hollywoodland.exe.
# Development is the default (logs, console, faster link). Same code and data as the web build.
[CmdletBinding()]
param(
    [ValidateSet('Development','Shipping')][string]$Configuration = 'Development',
    [string]$EngineRoot = 'C:\Users\theco\Projects\unreal59\UnrealEngine'
)
$projectRoot = Split-Path $PSScriptRoot -Parent
$project = Join-Path $projectRoot 'Hollywoodland.uproject'
$archive = Join-Path $projectRoot "win-dist-$Configuration"
New-Item -ItemType Directory -Force (Join-Path $projectRoot 'Saved\Logs') | Out-Null
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') HollywoodlandEditor Win64 Development "-Project=$project" '-CompilerVersion=14.50.35739' '-MaxParallelActions=8' -WaitMutex
if ($LASTEXITCODE -ne 0) { throw "Editor build failed ($LASTEXITCODE)." }
$arguments = @('BuildCookRun', "-project=$project", '-platform=Win64', "-clientconfig=$Configuration",
    '-build','-cook','-stage','-pak','-iostore','-package','-archive', "-archivedirectory=$archive",
    '-map=/Engine/Maps/Entry','-compressed','-unattended','-utf8output',
    '-ubtargs=-CompilerVersion=14.50.35739 -MaxParallelActions=8',
    '-AdditionalCookerOptions=-AgentMode -ExitTimeout=1800 -NoProgressTimeout=300 -unattended -nopause -nosplash -nullrhi -SkipZenStore -DisablePlugins=AllToolsets,Terminal,UnrealCanvasBridge')
$log = Join-Path $projectRoot "Saved\Logs\win-$Configuration.log"
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat') @arguments 2>&1 | Tee-Object -FilePath $log | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Windows packaging failed ($LASTEXITCODE). See $log" }
# Music is not cooked: the game plays <ProjectDir>\Music\*.mp3 natively (the web build streams them from the Worker).
$music = Join-Path (Split-Path $projectRoot -Parent) 'music'
$musicOut = Join-Path $archive 'Windows\Hollywoodland\Music'
New-Item -ItemType Directory -Force $musicOut | Out-Null
Copy-Item (Join-Path $music '*.mp3') $musicOut -Force
Write-Host "Windows build: $archive"
