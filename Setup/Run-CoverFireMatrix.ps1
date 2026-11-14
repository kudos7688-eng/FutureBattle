[CmdletBinding()]
param([Parameter(Mandatory)][string]$Frozen,[string]$Prefix='coverfire',[string]$Suffix='r1',[ValidateRange(1,4)][int]$Parallel=4,[string[]]$Only=@())
# Cover firing window matrix on the adopted defaults (leader HP x3, reacquire grace ON).
# Conditions: base / F (-CoverFire 1) / FP (+ -AICoverProbe 1) / FPA (+ -CoverApproach 1).
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
# S: adopted defaults + blind peek exit trial (-CoverPeekSight 1).
# V0 / FV0: the same with cover validation OFF (behaviour before 2026-11-14).
$conds=[ordered]@{V0=@('-CoverValidation','0');FV0=@('-CoverFire','1','-CoverValidation','0');base=@();F=@('-CoverFire','1');FP=@('-CoverFire','1','-AICoverProbe','1');FPA=@('-CoverFire','1','-AICoverProbe','1','-CoverApproach','1');S=@('-CoverPeekSight','1')}
$jobs=New-Object System.Collections.Generic.List[object]
foreach($seed in 7,19,43){foreach($map in 'D','A'){foreach($swap in $false,$true){foreach($c in $conds.Keys){if($Only.Count -and $c -notin $Only){continue}
 $name="$Prefix-$map-s$seed$(if($swap){'-swap'})-$c-$Suffix"
 $a=@('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $PSScriptRoot 'Test-WideSight.ps1'),'-Map',$map,'-OutputName',$name,'-SightRange','10000','-WeaponRange','5000','-Area','-Experiment','-RoundTripSave','-CoverActions','1','-FlankCoordination','1','-ObserveTactics','-Seed',"$seed",'-BehaviorConcurrency',"$Parallel",'-FrozenBuild',$Frozen)+$conds[$c]
 if($swap){$a+='-Swap'}
 $jobs.Add(@{name=$name;args=$a})}}}}
$running=@{}; $i=0; $log=Join-Path $PSScriptRoot "Benchmarks/SightTrial-20261111/$Prefix-matrix-$Suffix.txt"
while($i -lt $jobs.Count -or $running.Count){
 while($running.Count -lt $Parallel -and $i -lt $jobs.Count){$j=$jobs[$i];$out=Join-Path $PSScriptRoot "Benchmarks/SightTrial-20261111/$($j.name).console.txt"
  $p=Start-Process powershell.exe -ArgumentList $j.args -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError "$out.err";$running[$p.Id]=@{p=$p;name=$j.name};$i++
  Add-Content $log "$(Get-Date -Format HH:mm:ss) start $($j.name)"}
 foreach($k in @($running.Keys)){$r=$running[$k];if($r.p.HasExited){Add-Content $log "$(Get-Date -Format HH:mm:ss) end $($r.name) exit=$($r.p.ExitCode)";$running.Remove($k)}}
 Start-Sleep -Seconds 5
}
Add-Content $log "$(Get-Date -Format HH:mm:ss) all done"
