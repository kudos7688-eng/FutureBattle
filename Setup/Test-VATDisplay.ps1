[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$OutputFolder,
 [ValidateSet(40,500,1000)][int]$Units=500,
 [ValidateSet('Individual','Leader')][string]$Decision='Leader',
 [ValidateSet('RTS','TPS','FPS')][string]$View='RTS',
 [ValidateSet('Open','Cross','Gate')][string]$Layout='Open',
 [int]$Seed=1,[double]$Warmup=10,[double]$Duration=60,[switch]$Stable,[switch]$NullRHI,[switch]$Capture,[switch]$Functional,[ValidateSet('A','B')][string]$Sense='A',[switch]$SenseAudit,[switch]$CpuTrace,[switch]$LightTrace,[ValidateSet(0,1)][int]$ContextReuse=0,[switch]$ContextAudit,[switch]$ContextReplay,[ValidateSet(0,1)][int]$HiddenBones=0,[switch]$BoneAudit,[ValidateSet(0,1)][int]$PerceptionGrid=0,[switch]$GridAudit,[switch]$GridFixture,[ValidateSet(0,1)][int]$AnimTasks=0,[switch]$AnimTaskAudit,[ValidateSet(0,1)][int]$DisplayLOD=0,[int]$SightRange=0,[switch]$VATAudit,[switch]$VATQuality)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
$out=[IO.Path]::GetFullPath($OutputFolder)
if(Test-Path (Join-Path $out 'manifest.json')) { throw 'Use a fresh output folder' }
if(Get-Process UnrealEditor* -ErrorAction SilentlyContinue) { throw 'An Unreal process is already running' }
$lock=[IO.File]::Open((Join-Path $PSScriptRoot 'Benchmarks/measurement.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
$userSettingsPath=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/Saved/Config/WindowsEditor/GameUserSettings.ini'))
$savedUserSettings=[IO.File]::ReadAllBytes($userSettingsPath)
try {
[IO.Directory]::CreateDirectory($out)|Out-Null
if($VATQuality){
 if(!$VATAudit){throw 'VATQuality is a separate visual fixture only'}
 $qaSettings=[IO.File]::ReadAllText($userSettingsPath)
 foreach($key in @('sg.ShadowQuality','sg.GlobalIlluminationQuality','sg.ReflectionQuality')){$qaSettings=[regex]::Replace($qaSettings,'(?m)^'+[regex]::Escape($key)+'=\d+',($key+'=3'))}
 [IO.File]::WriteAllText($userSettingsPath,$qaSettings)
}
[IO.File]::Copy($userSettingsPath,(Join-Path $out 'GameUserSettings-start.ini'))
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Game/FutureBattle/FutureBattle.uproject'))
$dll=Join-Path $PSScriptRoot '../Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
$log=Join-Path $out 'run.log'
$args=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-BattleLeaderCommand=1',"-BattleLeaderUnits=$Units",('-abslog="'+$log+'"'),('-BattlePhase3="'+(Join-Path $out 'frames')+'"'),"-P3Decision=$Decision","-P3View=$View","-P3Layout=$Layout","-P3Seed=$Seed","-P3Warmup=$Warmup","-P3Duration=$Duration",'-ResX=1920','-ResY=1080','-Windowed','-ForceRes','-NoVSync')
if($SightRange -gt 0){$args+="-BattleSightRange=$SightRange"}
if($NullRHI) { $args+='-nullrhi' } else { $args+='-RenderOffscreen' }
 $args+="-P3Sense=$Sense"; if($SenseAudit) { $args+="-P3SenseAudit" }
if($Stable) { $args+='-P3Stable' }
if($Capture) { $args+='-P3Capture' }
if($Functional) { $args+='-P3Functional' }
$args+="-BattleReuseSightContext=$ContextReuse"
$args+="-BattleReduceHiddenBones=$HiddenBones"
$args+="-BattlePerceptionGrid=$PerceptionGrid"
$args+="-BattleReduceAnimTasks=$AnimTasks"
$args+="-BattleDisplayLOD=$DisplayLOD"
if($VATAudit){$args+=@('-P3VATAudit','-Deterministic','-FPS=30')}
if($AnimTaskAudit) { $args+=@('-P3AnimTaskAudit','-Deterministic','-FPS=30') }
if($GridAudit) { $args+='-P3GridAudit' }
if($GridFixture) { $args+='-P3GridFixture' }
if($BoneAudit) { $args+=@('-P3BoneAudit','-Deterministic','-FPS=30') }
if($ContextAudit) { $args+='-P3ContextAudit' }
if($ContextReplay) { $args+=@('-P3ContextReplay','-Deterministic','-FPS=30') }
if($LightTrace -and !$CpuTrace) { throw 'LightTrace requires CpuTrace' }
if($CpuTrace) { $args+=@('-trace=cpu,frame,bookmark,log',('-tracefile="'+(Join-Path $out 'cpu.utrace')+'"')); if(!$LightTrace) { $args+='-statnamedevents' } }
$m=@{schema=1;status='running';machine=(Get-MeasurementMachine);dll_sha256=(Get-FileHash $dll).Hash;created=(Get-Date -Format o);units=$Units;decision=$Decision;view=$View;layout=$Layout;seed=$Seed;warmup=$Warmup;duration=$Duration;stable=[bool]$Stable;nullrhi=[bool]$NullRHI;arguments=$args;lod='Screen120_48 H20 controllable Actor LOD0';scope='fixed roster, flat authored field, UE shared RVO; not full match economy'}
$m.functional=[bool]$Functional; $m.sense=$Sense; $m.sense_audit=[bool]$SenseAudit; $m.performance_measurement=![bool]$SenseAudit
$m.cpu_trace=[bool]$CpuTrace
$m.light_trace=[bool]$LightTrace
$m.context_reuse=$ContextReuse; $m.context_audit=[bool]$ContextAudit; $m.context_replay=[bool]$ContextReplay
$m.hidden_bones=$HiddenBones; $m.bone_audit=[bool]$BoneAudit
$m.display_lod=$DisplayLOD; $m.vat_audit=[bool]$VATAudit; $m.vat_quality=[bool]$VATQuality; $m.anim_tasks=$AnimTasks; $m.anim_task_audit=[bool]$AnimTaskAudit
$m.perception_grid=$PerceptionGrid; $m.grid_audit=[bool]$GridAudit; $m.grid_fixture=[bool]$GridFixture
$m.performance_measurement=(!$SenseAudit -and !$ContextAudit -and !$ContextReplay -and !$Functional -and !$BoneAudit -and !$GridAudit -and !$GridFixture -and !$AnimTaskAudit -and !$VATAudit)
$m.runner_hashes=@(Get-FileHash $PSCommandPath,(Join-Path $PSScriptRoot 'Measure-VATDisplay.ps1'),(Join-Path $PSScriptRoot 'Report-VATDisplay.py') | Select-Object Path,Hash)
$m.source_hashes=@(Get-ChildItem (Join-Path $PSScriptRoot '../Game/FutureBattle/Source') -Recurse -File | Get-FileHash | Select-Object Path,Hash)
$m.config_hashes=@(Get-ChildItem (Join-Path $PSScriptRoot '../Game/FutureBattle/Config') -Filter '*.ini' | Get-FileHash | Select-Object Path,Hash)
$m.asset_hashes=@(Get-ChildItem (Join-Path $PSScriptRoot '../Game/FutureBattle/Content/Prototype/DisplayLODTrial') -File | Get-FileHash | Select-Object Path,Hash)
$m.map_hash=(Get-FileHash (Join-Path $PSScriptRoot '../Game/FutureBattle/Content/Prototype/Lvl_BattleField500.umap')).Hash
$m|ConvertTo-Json -Depth 9|Set-Content (Join-Path $out 'manifest.json')
$proc=Start-Process 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' -ArgumentList $args -WindowStyle Hidden -PassThru
$timeoutMargin=if($ContextReplay){600}else{180}
if(!$proc.WaitForExit([int](($Warmup+$Duration+$timeoutMargin)*1000))) { Stop-Process -Id $proc.Id; $m.status='timeout' } else {
 $text=Get-Content $log -Raw
 $ok=$proc.ExitCode -eq 0 -and $text -match 'P3_RESULT failures=0' -and $text -notmatch 'Fatal error:|Assertion failed:|Ensure condition failed:' -and (Test-Path (Join-Path $out 'frames.csv'))
 $m.status=if($ok){'passed'}else{'failed'}
 $m.exit_code=$proc.ExitCode
 $m.result=($text -split "`n" | Where-Object {$_ -match 'P3_RESULT'})
 $m.adapter=($text -split "`n" | Where-Object {$_ -match 'Chosen D3D12 Adapter'})
}
$m.completed=Get-Date -Format o
$m|ConvertTo-Json -Depth 9|Set-Content (Join-Path $out 'manifest.json')
Write-Output "$Units $Decision $View $Layout seed=$Seed : $($m.status)"
if($m.status -ne 'passed') { throw "Phase3 run failed: $log" }

} finally { [IO.File]::WriteAllBytes($userSettingsPath,$savedUserSettings); $lock.Dispose() }




