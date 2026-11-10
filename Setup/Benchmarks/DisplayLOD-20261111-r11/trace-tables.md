# RTS別traceの排他的GT内訳

通常12走行とは別、各モード1走行。trace負荷を含み、通常性能値への加算・補正に使わない。親子scopeの重複を除いた分類。単位ms/engine tick。

|分類|OFF|ON|OFF−ON|
|---|---:|---:|---:|
|アニメTick|2.997|0.072|2.925|
|アニメGT反映|3.047|0.010|3.037|
|描画更新|2.938|0.551|2.387|
|ISM/VAT更新|0.000|0.141|-0.141|
|GameMode（移動・RVO・認識等含む）|9.456|6.901|2.555|
|CharacterMovement|0.962|1.226|-0.264|
|HUD|0.280|0.252|0.028|
|Slate|0.128|0.117|0.011|
|待機scope|0.672|0.421|0.251|
|その他/未分類|3.134|2.206|0.928|

モード0: trace走行GT平均23.612ms、2541tick、負のself time 0、分類合計とengine tickの差0.000000000ms。

モード1: trace走行GT平均11.896ms、5044tick、負のself time 0、分類合計とengine tickの差0.000000000ms。

ONのその他/未分類の主なself費用：
- ProcessUntilTasksComplete: 0.270ms
- Queue Ticks: 0.190ms
- FTickFunctionTask: 0.150ms
- SKM_Quinn_Simple: 0.138ms
- CreateExternalAccelerationStructure: 0.073ms
- FActorComponentTickFunction::ExecuteTick: 0.068ms
- BattleVATSkeletalMeshComponent: 0.063ms
- STAT_DispatchTickGroup: 0.062ms

ONではGameMode約6.90ms、その他約2.21ms、CharacterMovement約1.23msが残る。GameMode内の移動/transform/子更新等の帰属には既存scopeのネストがあるため、通常CSVのfollow/RVO等をこの表へ再加算しない。

TPS/FPSの詳細traceは未測定。RTSの内訳割合をTPS/FPSへ換算しない。通常CSVの視点別scopeはsupplement-tables.mdを参照。

窓はFrameSpikesで確認された1tick早い対応を使用。engine tickとviewportのGT境界には差がある。今回2走行の時系列相関は再測定していない。
