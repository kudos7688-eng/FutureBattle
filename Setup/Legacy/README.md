# 使い終わった試験・計測スクリプト

2026-11-18 に `Setup/` から移した。どれも 11-04〜11-11 に終わった個別の調査で使ったもので、今の手順（`MEASURE.md`）では使わない。消さずに残しているのは、過去の記録（`Docs/journal/`）を確かめ直すため。

- **そのままでは動かないことがある**。多くは `$PSScriptRoot` や `__file__` を基準に、`Setup/` にあるほかのスクリプト（`Test-Battle.ps1`、`Get-MeasurementMachine.ps1` など）や `Setup/Benchmarks/` を探す。使うときは、必要なものを `git mv` で `Setup/` へ戻してから実行する。
- 移したスクリプトどうしの呼び出し（例：`Test-T2a.ps1` → `Check-T2aRegression.py`）は、同じフォルダにあるので保たれる。
- 移す前に、`Setup/` に残したスクリプトがどれもここのスクリプトを参照していないことを確かめた。

| まとまり | スクリプト |
|---|---|
| T2a/T2b（共有候補への射撃配分、11-06） | `Test-T2a/T2b.ps1`、`Summarize-T2a/T2b.py`、`Check-T2a/T2bRegression.py`、`Plot-T2b.py` |
| T5（描画の費用・影・LOD・材質、11-06〜11-09） | `Test-T5*.ps1`、`Summarize-T5*.py`、`Plot-T5.py`、`Report-T5A.py`、`Inspect-*.py`、`*LODTrial.py`、`Analyze-LODBoundaries.py`、`Report-MaterialTrial.py`、`Report-ShadowMaterialCosts.py`、`Report-CullingRuntime.py` |
| T1/T4 と配置の診断（11-06） | `Test-RenderReplay.ps1`、`Summarize-RenderReplay.py`、`Check-RenderReplayProgress.py`、`Test-ConeDistribution.ps1`、`Summarize-ConeDistribution.py`、`Test-FieldTrial.ps1`、`Summarize-FieldDiagnostics.py` |
| 認識とゲームスレッドの最適化の試験（11-10） | `*PerceptionAudit*`、`*PerceptionGrid*`、`*SightContext*`、`*HiddenBones*`、`*AnimTasks*`、`*GTBothOn*`、`*GTBreakdown*`、`Measure-GTLight.ps1`、`Summarize-PerceptionRuns.py` |
| 古い総当たり | `Run-SightMatrix.ps1`、`Summarize-SightMatrix.py`、`Run-WeaponMatrix.ps1`、`Run-AreaMatrix.ps1`、`Run-FlankSpaceComparison.ps1` |

`Setup/Benchmarks/*-source-*` にある過去版の写しは、計測した時点の証拠なので、移さずに置いてある。
