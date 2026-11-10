[CmdletBinding()]
param([int[]]$Seeds=(1..10),[string[]]$Stages=@('A2','A1','A3'),[string[]]$Profiles=@(),[string]$OutputFolder='',[int]$Conditions=0,[int]$CaptureFrame=450,[switch]$VisualQA,[switch]$SharpQA,[switch]$ShadowQA,[string]$LODPolicy="Dist20_50",[switch]$LODStress,[double]$Hysteresis=10,[string[]]$Filters=@(),[string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference='Stop'
if($SharpQA -and !$VisualQA){throw 'SharpQA requires VisualQA'}
if($ShadowQA -and !$VisualQA){throw 'ShadowQA requires VisualQA; stress captures are not performance data'}
. (Join-Path $PSScriptRoot 'Get-MeasurementMachine.ps1')
if(!$OutputFolder){$OutputFolder=Join-Path $PSScriptRoot ('Benchmarks/T5A-'+(Get-Date -Format yyyyMMdd-HHmmss))}
[IO.Directory]::CreateDirectory($OutputFolder)|Out-Null
$OutputFolder=[IO.Path]::GetFullPath($OutputFolder)
$project=[IO.Path]::GetFullPath("$PSScriptRoot/../Game/FutureBattle/FutureBattle.uproject")
$dll=Join-Path (Split-Path $project) 'Binaries/Win64/UnrealEditor-FutureBattle.dll'
$editor=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$hash=(Get-FileHash $dll -Algorithm SHA256).Hash
$lock=$null
try {
 $lock=[IO.File]::Open((Join-Path $PSScriptRoot 'Benchmarks/measurement.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
 if(Get-Process UnrealEditor* -ErrorAction SilentlyContinue){throw 'An Unreal Editor is running; measurement requires exclusive use.'}
 if(Test-Path (Join-Path $OutputFolder 'manifest.json')){throw 'Use a fresh output folder'}
 $machine=Get-MeasurementMachine
 $manifest=@{dll_sha256=$hash;machine=$machine;seeds=$Seeds;stages=$Stages;profiles_override=$Profiles;resolution='1920x1080';record_slots=500;actors=50;mass=450;created=(Get-Date -Format o);status='running';records=@{};runs=@();visual_qa=[bool]$VisualQA;sharp_qa=[bool]$SharpQA;shadow_qa=[bool]$ShadowQA;lod_policy=$LODPolicy;lod_stress=[bool]$LODStress;hysteresis_percent=$Hysteresis;filters_override=$Filters}
 Copy-Item -LiteralPath $dll -Destination (Join-Path $OutputFolder 'UnrealEditor-FutureBattle.dll')
 foreach($seed in $Seeds){$record=[IO.Path]::GetFullPath("$PSScriptRoot/Benchmarks/T1-camera-20261106/seed$seed-Individual.fbr");$manifest.records[$record]=(Get-FileHash $record).Hash}
 $manifest|ConvertTo-Json -Depth 8|Set-Content (Join-Path $OutputFolder 'manifest.json')
 foreach($stage in $Stages) {
  if($stage -notin @('A1','A2','A3','Shadow','B','C','D')){throw "Invalid stage $stage"}
  $settings=@(if($Profiles.Count){$Profiles}elseif($stage -eq 'A2'){@('Current','Software','Light','ClassicShadow','NoShadow','Medium','Low')}else{@('Current','Light')})
  foreach($seed in $Seeds) {
   for($k=0;$k -lt $settings.Count;$k++) {
    $profile=$settings[($k+$seed)%$settings.Count]
    if($profile -notin @('Current','Software','Light','ClassicShadow','NoShadow','Medium','Low')){throw "Invalid profile $profile"}
    if((Get-FileHash $dll).Hash -ne $hash){throw 'DLL changed'}
    $record=[IO.Path]::GetFullPath("$PSScriptRoot/Benchmarks/T1-camera-20261106/seed$seed-Individual.fbr")
    $name="seed$seed-$stage-$profile";$capture=Join-Path $OutputFolder "$name.png";$log=Join-Path $OutputFolder "$name.log"
    $argsList=@(('"'+$project+'"'),'/Game/Prototype/Lvl_BattleField500','-game','-unattended','-nosound','-nosplash','-RenderOffscreen','-ResX=1920','-ResY=1080','-ForceRes','-NoVSync','-BattleUnits=500','-BattleWeaponLoadouts',"-BattleFieldSeed=$seed",'-BattleSquadTrial=Individual','-T5-Replay',"-T5-Stage=$stage","-T5-Profile=$profile",'-T2a-Allocation=Off','-T2b-TurnToFace=Off',('-BattleFieldReplay="'+$record+'"'),('-BattleMovingCapture="'+$capture+'"'),('-abslog="'+$log+'"'))
    # UE saves scalability on shutdown: isolate the generated user config per launch.
    $userIni=Join-Path $OutputFolder "$name-GameUserSettings.ini"
    $argsList+=('-GameUserSettingsINI="'+$userIni+'"')
    if($profile -ne 'Current'){$argsList+='-noraytracing'}
    if($VisualQA){$argsList+=@('-T5-VisualQA',"-T5-CaptureFrame=$CaptureFrame")}
    if($SharpQA){$argsList+='-ExecCmds="r.MotionBlurQuality 0"'}
    if($ShadowQA){$argsList+='-T5-ShadowQA'}
    if($stage -in @('C','D')){$argsList+=@("-T5-LODPolicy=$LODPolicy","-T5-Hysteresis=$Hysteresis");if($LODStress){$argsList+='-T5-LODStress'}}
    # Preserve literal quotes through Windows argv and UE's command-line reconstruction.
    if($Filters.Count){$argsList+=('-T5-Filters=\"'+($Filters -join ',')+'\"')}
    if($Conditions -gt 0){$argsList+="-T5-Conditions=$Conditions"}
    $expected=if($stage -eq 'D'){64}elseif($stage -eq 'C'){20}elseif($stage -eq 'B'){64}elseif($stage -eq 'Shadow'){12}elseif($stage -eq 'A2'){16}elseif($stage -eq 'A3'){28}else{24};if($Filters.Count){$viewCount=if($stage -in @('C','Shadow','A3')){2}elseif($stage -eq 'A1'){3}else{4};$expected=$viewCount*$Filters.Count*2};if($Conditions -gt 0){$expected=[Math]::Min($Conditions,$expected)}
    $started=Get-Date;$p=Start-Process $editor -ArgumentList $argsList -WindowStyle Hidden -PassThru
    if(!$p.WaitForExit(1200000)){Stop-Process -Id $p.Id;throw "Timeout $log"}
    $lines=@(Select-String $log -Pattern 'T5_REPLAY seed='|ForEach-Object {$_.Line})
    if($p.ExitCode -ne 0 -or $lines.Count -ne $expected -or @($lines|Where-Object {$_ -notmatch 'success=1'}).Count){throw "Failed $log"}
    if((Get-FileHash $dll).Hash -ne $hash){throw 'DLL changed during run'}
    $sample=Get-ChildItem -LiteralPath $OutputFolder -Filter "$name-*-r*.json" | Select-Object -First 1
    $actual=(Get-Content $sample.FullName -Raw | ConvertFrom-Json).settings
    $quality=if($profile -eq 'Low'){0}elseif($profile -eq 'Medium'){1}else{3}
    foreach($key in @('sg.ViewDistanceQuality','sg.ShadowQuality','sg.GlobalIlluminationQuality','sg.ReflectionQuality','sg.EffectsQuality')){
     if($actual.$key -ne $quality){throw "Incorrect effective setting $name $key = $($actual.$key)"}
    }
    $expectedShadow=if($profile -in @('Low','NoShadow')){0}elseif($profile -eq 'Medium'){3}else{5}
    if($actual.'r.ShadowQuality' -ne $expectedShadow){throw "Incorrect shadow setting $name"}
    $settingsFile=Join-Path $OutputFolder "expected-$profile.json"
    if(Test-Path $settingsFile){
     $prior=Get-Content $settingsFile -Raw | ConvertFrom-Json
     foreach($property in $actual.PSObject.Properties){if($prior.($property.Name) -ne $property.Value){throw "Settings drift $name $($property.Name)"}}
    }else{$actual|ConvertTo-Json -Depth 5|Set-Content $settingsFile}
    $lines|Add-Content (Join-Path $OutputFolder 'results.txt')
    $manifest.runs+=@{seed=$seed;stage=$stage;profile=$profile;conditions=$expected;started=$started.ToString('o');finished=(Get-Date -Format o);args=$argsList}
    $manifest|ConvertTo-Json -Depth 8|Set-Content (Join-Path $OutputFolder 'manifest.json')
    Write-Output "$name complete: $expected conditions"
   }
  }
 }
 $manifest.status='complete';$manifest|ConvertTo-Json -Depth 8|Set-Content (Join-Path $OutputFolder 'manifest.json')
}finally{if($lock){$lock.Dispose()}}
