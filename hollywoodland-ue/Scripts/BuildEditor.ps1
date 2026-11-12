# Builds the Win64 editor target (needed for cooking and for native -game test runs).
param([string]$EngineRoot = 'C:\Users\theco\Projects\unreal59\UnrealEngine')
$project = Join-Path (Split-Path $PSScriptRoot -Parent) 'Hollywoodland.uproject'
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') HollywoodlandEditor Win64 Development "-Project=$project" '-CompilerVersion=14.50.35739' '-MaxParallelActions=8' -WaitMutex
exit $LASTEXITCODE
