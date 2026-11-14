# Self-play run of the simulated-player mind, with a summary of what it did.
#   tools\dev\aitest.ps1 -Mode observe|party|memory|plain [-Class nurse] [-Seconds 200] [-Extra '-HLAutoDungeon']
param([string]$Mode = 'plain', [string]$Class = 'nurse', [int]$Seconds = 200, [string]$Extra = '', [int]$Show = 45)
$root = Split-Path (Split-Path $PSScriptRoot)
$ue = 'C:\Users\theco\Projects\unreal59\UnrealEngine\Engine\Binaries\Win64\UnrealEditor.exe'
$a = @("`"$root\hollywoodland-ue\Hollywoodland.uproject`"", '-game', '-windowed', '-ResX=1280', '-ResY=720', "-HLAuto=$Class", "-HLAutoQuit=$Seconds", '"-LogCmds=LogTemp Verbose"')
if ($Mode -ne 'plain') { $a += "-HLAiTest=$Mode" }
if ($Extra) { $a += $Extra.Split(' ') }
Start-Process $ue -ArgumentList $a
Start-Sleep 5
Wait-Process UnrealEditor -ErrorAction SilentlyContinue
$log = "$root\hollywoodland-ue\Saved\Logs\Hollywoodland.log"
$cut = { param($l, $k) $s = $l.Substring($l.IndexOf($k)); $s.Substring(0, [Math]::Min(230, $s.Length)) }
(Select-String -Path $log -Pattern 'HL_STATUS' | Select-Object -Last 1).Line | ForEach-Object { & $cut $_ 'HL_STATUS' }
$pats = 'HL_MIND', 'HL_AI ', 'misread|misses|wrong target', 'notices', 'ignores', 'annoyed|fed up', 'pulls ', 'overwhelmed', 'plan for', 'Error:', 'Ensure condition', 'Fatal'
'counts: ' + (($pats | ForEach-Object { '{0}={1}' -f $_, (Select-String -Path $log -Pattern $_).Count }) -join '  ')
'ai: ' + ((Select-String -Path $log -Pattern 'HL_AI ' | Group-Object { ($_.Line -split 'HL_AI ')[1].Split(' ')[0] } | ForEach-Object { '{0} {1}' -f $_.Name, $_.Count }) -join ', ')
'goals: ' + ((Select-String -Path $log -Pattern 'HL_MIND .* -> ' | Group-Object { ($_.Line -split ' -> ')[1].Split(' ')[0] } | ForEach-Object { '{0} {1}' -f $_.Name, $_.Count }) -join ', ')
'--- mind'
(Select-String -Path $log -Pattern 'HL_MIND' | Where-Object { $_.Line -notmatch 'remembers \[| -> |overwhelmed' } | Select-Object -First $Show).Line | ForEach-Object { & $cut $_ 'HL_MIND' }
'--- chat (say, whisper, party, guild)'
(Select-String -Path $log -Pattern 'HL_CHAT' | Select-Object -Last $Show).Line | ForEach-Object { & $cut $_ 'HL_CHAT' }
