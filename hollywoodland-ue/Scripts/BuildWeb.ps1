# Builds, cooks and packages Hollywoodland for the web (Wasm, mobile-forward WebGPU tier) into web-dist-<Configuration>\Wasm.
[CmdletBinding()]
param(
    [ValidateSet('Development','Test','Shipping')][string]$Configuration = 'Shipping',
    [string]$ArchiveDirectory = '',
    [string]$EngineRoot = 'C:\Users\theco\Projects\unreal59\UnrealEngine',
    [switch]$SkipBuild,
    [switch]$SkipEditor
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$project = Join-Path $projectRoot 'Hollywoodland.uproject'
if (!$ArchiveDirectory) { $ArchiveDirectory = Join-Path $projectRoot "web-dist-$Configuration" }
$ArchiveDirectory = [IO.Path]::GetFullPath($ArchiveDirectory)
$env:WASM_EMSDK_ROOT = Join-Path (Split-Path $EngineRoot -Parent) 'emsdk'
$env:EMCC_CORES = '6'
$env:BINARYEN_CORES = '6'
New-Item -ItemType Directory -Force (Join-Path $projectRoot 'Saved\Logs') | Out-Null
$ErrorActionPreference = 'Continue'
if (!$SkipBuild) {
    if (!$SkipEditor) {
        & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') HollywoodlandEditor Win64 Development "-Project=$project" '-CompilerVersion=14.50.35739' '-MaxParallelActions=8' -WaitMutex
        if ($LASTEXITCODE -ne 0) { throw "Editor build failed ($LASTEXITCODE)." }
    }
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') Hollywoodland Wasm $Configuration "-Project=$project" '-MaxParallelActions=6' '-WaitMutex' '-NoHotReload'
    if ($LASTEXITCODE -ne 0) { throw "WebAssembly build failed ($LASTEXITCODE)." }
}
# AllToolsets/Terminal/Canvas are editor-only AI tooling; they crash a Wasm cook, so the cook disables them explicitly.
$arguments = @('BuildCookRun', "-project=$project", '-platform=Wasm', "-clientconfig=$Configuration",
    '-cook','-stage','-pak','-iostore','-package','-archive', "-archivedirectory=$ArchiveDirectory",
    '-map=/Engine/Maps/Entry','-compressed','-nodebuginfo','-unattended','-utf8output',
    '-AdditionalCookerOptions=-AgentMode -ExitTimeout=1800 -NoProgressTimeout=300 -unattended -nopause -nosplash -nullrhi -SkipZenStore -DisablePlugins=AllToolsets,Terminal,UnrealCanvasBridge')
$log = Join-Path $projectRoot "Saved\Logs\web-$Configuration.log"
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat') @arguments 2>&1 | Tee-Object -FilePath $log | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Web packaging failed ($LASTEXITCODE). See $log" }
Write-Host "Web archive: $ArchiveDirectory"
