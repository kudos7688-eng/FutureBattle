param([ValidateSet('H','I','J','K')][string]$Map='I',[int]$Seed=7,[string]$OutputName='defend-smoke',[int]$Fps=30)
# Fixed-scene self-test of the defensive deployment and the hot sectors (2026-11-25, Docs/design-defense-deployment.md):
# one headless match on a sector map with -BattleDefendSmoke=1; UBattleLeaderRuntime::RunDefendSmoke checks the hot-sector
# boundaries (player circle, defender before observation, the teams taking turns, the 10 s linger) and one placement of a
# team-0 rifle squad on the sector with the most cover, 3 s after the combat starts. Passes when the run passed and the log
# has BATTLE_DEFEND_SMOKE_RESULT failures=0.
$ErrorActionPreference='Stop'
$Root=Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'Test-WideSight.ps1') -Map $Map -Seed $Seed -OutputName $OutputName -Area -Experiment -Fps $Fps -ExtraArgs '-BattleDefendSmoke=1 -BattleHotSectors=1 -BattleDefendSpread=1'
$Log=Join-Path $Root "Setup\Benchmarks\SightTrial-20261111\$OutputName\run.log"
if(-not (Test-Path $Log)){Write-Host "Defend smoke: no run log ($Log)";exit 1}
$Lines=Select-String -Path $Log -Pattern 'BATTLE_DEFEND_SMOKE' | ForEach-Object { $_.Line -replace '^.*(BATTLE_DEFEND_SMOKE)','$1' }
$Lines | ForEach-Object { Write-Host $_ }
$Result=$Lines | Where-Object { $_ -match '^BATTLE_DEFEND_SMOKE_RESULT failures=(\d+)' } | Select-Object -Last 1
if(-not $Result){Write-Host 'Defend smoke: no result line';exit 1}
if($Result -notmatch 'failures=0$'){Write-Host 'Defend smoke failed.';exit 1}
Write-Host 'Defend smoke passed.'
