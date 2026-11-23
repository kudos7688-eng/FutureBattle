# 効果音・エフェクトの素材と出典（2026-11-22）

ゲームの効果音と見た目の効果（`BattleAudio` / `BattleEffects`）に使う素材の一覧です。確認の手順は `Setup/Test-SoundFX.ps1`（画面の撮影と、スピーカーを鳴らさずに音の混ざりを録音して測る）。**CC0 などネットから新しく取ってきた素材はありません。** すべて (1) ユーザーの手元の素材フォルダ `C:\Users\nanek\Documents\FPS\アセット` にある2つのUEプロジェクトからの写し、または (2) このために合成した自作の音です。

## 出典ごとのまとめ

| 出典 | 手元の場所 | 使ったもの | 数・大きさ | 利用条件（要確認の点） |
|---|---|---|---|---|
| **Lyra Starter Game**（Epic Games） | `アセット\LyraStarterGame`（UE 5.8 版） | 爆発音・着弾音・弾のかすめる音（SoundWave 39個）、爆発の効果 `NS_Grenade_Explosion`（Niagara）とその材質・テクスチャ・曲線・破片の形（34個） | 73個・約51MB | Epic Games が Fab（旧 Unreal マーケットプレイス）で無料配布しているサンプル。Epic のコンテンツは Unreal Engine の利用規約（EULA）に従い、**UE で作る作品の中で使える**（UE 以外の製品へ持ち出すのは不可）とされている。**正確な条件は Fab の Lyra のページ／UE の EULA でご確認ください。** |
| **TPS Multiplayer Shooter with cover**（Fab の素材） | `アセット\TPSMultiplayershooterwith`（UE 5.8 版、中のフォルダ名は `Multiplayercovershooter`） | 銃声（M4・AX50 の近い音／遠い音、各4本）、構え・スコープ・装填・空撃ちの音（SoundWave 22個）。着弾の効果 `P_Wep_BulletImpact_Rifle` も試したが、ゲームの画面に出なかったので外した | 22個・約5MB | Fab の素材の標準ライセンス（Standard License）であれば、入手した人が作る作品の中で使える。**手元のファイルには販売者名・ライセンス文が入っていないので、Fab の入手履歴（掲載ページ）で販売者とライセンスの種類をご確認ください。** この素材の遮蔽アニメはすでにゲームで使っている（`Docs/ASSET_REVIEW_20261119.md`）。 |
| **自作**（Claude が合成、外部素材なし） | `Setup/Make-SynthSounds.py` → `Setup/SynthAudio/*.wav` → `/Game/Audio/Battle/` | 対戦車ロケットの発射音、迫撃砲の発射音、砲弾が飛んでくる音、拠点を取った／取られた合図、勝ち／負けの音（7個） | 7個・約1MB | 雑音と正弦波から数式で作った音。外部の録音・サンプルは使っていない。 |
| CC0 素材 | — | 使っていない | — | — |

## どの場面でどの音・効果が出るか

| 場面 | 音 | 見た目 |
|---|---|---|
| 小銃・カービンの射撃 | TPS `M4_Designed_Oneshot_1〜4`（60m未満）、`M4_Designed_Dist_Oneshot_1〜4`（60m以上） | 既存の銃口の閃光・曳光線（変更なし） |
| 支援火器（機関銃）の射撃 | TPS `AX50_Designed_Oneshot_1〜4`／`AX50_Designed_Dist_Oneshot_1〜4`（少し速め） | 同上 |
| 弾が地面・壁に当たる | Lyra `SFX_BulletImpact_01〜08`（35m以内） | なし（TPS の着弾の効果は画面に出なかったので外した。作り直しは今後） |
| 弾が兵・車両に当たる | Lyra `Lyra_Plyr_BulletImpact_01〜06`（25m以内） | なし |
| 自分（直接操作中の兵）のそばを敵弾がかすめる | Lyra `sfx_AR_Whizbys_Oncoming_Close_01〜04`（1.2m以内）、`…_Mid_01〜02`（3mまで） | なし |
| 対戦車兵・外骨格兵のロケット発射 | 自作 `SW_Synth_RocketLaunch` | なし（着弾は下の爆発） |
| 爆発（対戦車弾・外骨格兵の発射筒・戦車砲弾・迫撃砲弾・自走砲弾） | Lyra `Explosions_Grenade_Punch-Close_01〜04`＋`Noise-Exterior_01〜04`（100m未満）、`Punch-Distant_01〜04`＋`Noise-Exterior-Distant_01〜04`（100m以上）。大きい爆発は `grenade_explode_thump_01〜03` を重ねる。大きいほど低く大きく | Lyra `NS_Grenade_Explosion`（閃光・火球・火花・破片・焦げ跡。爆発半径に合わせて 0.8〜3 倍）。従来の爆発半径の線の球は、効果が出ているときは描かない |
| 戦車砲・自走砲の発射 | Lyra の爆発音を低く（0.6倍の速さ）して `grenade_explode_thump` を重ねる | 戦車は既存の閃光。自走砲・迫撃砲に閃光を追加 |
| 迫撃砲の発射 | 自作 `SW_Synth_MortarLaunch` | 閃光（小） |
| 砲弾が落ちてくる（着弾の1.8秒前から、着弾点が90m以内のとき） | 自作 `SW_Synth_ShellIncoming` | なし |
| 構える（右クリック）／スコープ倍率の切り替え | TPS `SW_aiming_1`／`SW_sniper_zoom_in`・`SW_sniper_zoom_out` | — |
| 装填の始まり／終わり、弾切れで引き金 | TPS `SW_reload_begin`／`SW_reload_end`、`SW_trigger_empty` | — |
| 拠点を取った／取られた | 自作 `SW_Synth_CaptureFriendly`／`SW_Synth_CaptureEnemy` | — |
| 勝ち／負け | 自作 `SW_Synth_Victory`／`SW_Synth_Defeat` | — |

## 取り込み先と個別の一覧

同じ `/Game` のパスのまま写した（効果は材質・テクスチャを名前で参照しているため）。`/Game/Audio/Sounds`・`/Game/Audio/SoundWaves`・`/Game/Effects` が Lyra 由来、`/Game/Multiplayercovershooter/sounds` が TPS 由来、`/Game/Audio/Battle` が自作。

- Lyra の音のうち、Lyra 独自の音の分類・音量調整（`/Game/Audio/Classes/SFX` など。音響の追加プラグインを要する）を参照しているもの（装填の音・拠点の合図など）は**写していない**。参照のない音だけを選んだ。
- `NS_Grenade_Explosion` の中には、Lyra 自身にも無い資産の名前（`/Game/VHE/...`、`/Game/Developer/Fandorra/...` など、作者の制作時の履歴）が文字として残っている。参照ではなく、写す必要はない。

### 写した素材（95個、55.8MB。`python Setup/Import-AudioFXAssets.py --list-md` で再出力できる）

| 取り込み先（/Game） | 出典 | 種類 | 大きさ |
|---|---|---|---|
| `/Game/Multiplayercovershooter/sounds/wav/M4_Designed_Oneshot_1` | TPS | sound | 201 KB |
| `/Game/Multiplayercovershooter/sounds/wav/M4_Designed_Oneshot_2` | TPS | sound | 207 KB |
| `/Game/Multiplayercovershooter/sounds/wav/M4_Designed_Oneshot_3` | TPS | sound | 194 KB |
| `/Game/Multiplayercovershooter/sounds/wav/M4_Designed_Oneshot_4` | TPS | sound | 225 KB |
| `/Game/Multiplayercovershooter/sounds/wav/M4_Designed_Dist_Oneshot_1` | TPS | sound | 203 KB |
| `/Game/Multiplayercovershooter/sounds/wav/M4_Designed_Dist_Oneshot_2` | TPS | sound | 221 KB |
| `/Game/Multiplayercovershooter/sounds/wav/M4_Designed_Dist_Oneshot_3` | TPS | sound | 206 KB |
| `/Game/Multiplayercovershooter/sounds/wav/M4_Designed_Dist_Oneshot_4` | TPS | sound | 244 KB |
| `/Game/Multiplayercovershooter/sounds/wav/AX50_Designed_Oneshot_1` | TPS | sound | 386 KB |
| `/Game/Multiplayercovershooter/sounds/wav/AX50_Designed_Oneshot_2` | TPS | sound | 379 KB |
| `/Game/Multiplayercovershooter/sounds/wav/AX50_Designed_Oneshot_3` | TPS | sound | 398 KB |
| `/Game/Multiplayercovershooter/sounds/wav/AX50_Designed_Oneshot_4` | TPS | sound | 383 KB |
| `/Game/Multiplayercovershooter/sounds/wav/AX50_Designed_Dist_Oneshot_1` | TPS | sound | 371 KB |
| `/Game/Multiplayercovershooter/sounds/wav/AX50_Designed_Dist_Oneshot_2` | TPS | sound | 389 KB |
| `/Game/Multiplayercovershooter/sounds/wav/AX50_Designed_Dist_Oneshot_3` | TPS | sound | 368 KB |
| `/Game/Multiplayercovershooter/sounds/wav/AX50_Designed_Dist_Oneshot_4` | TPS | sound | 421 KB |
| `/Game/Multiplayercovershooter/sounds/wav/SW_reload_begin` | TPS | sound | 87 KB |
| `/Game/Multiplayercovershooter/sounds/wav/SW_reload_end` | TPS | sound | 154 KB |
| `/Game/Multiplayercovershooter/sounds/wav/SW_trigger_empty` | TPS | sound | 28 KB |
| `/Game/Multiplayercovershooter/sounds/wav/SW_aiming_1` | TPS | sound | 98 KB |
| `/Game/Multiplayercovershooter/sounds/wav/SW_sniper_zoom_in` | TPS | sound | 61 KB |
| `/Game/Multiplayercovershooter/sounds/wav/SW_sniper_zoom_out` | TPS | sound | 88 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Close_01` | Lyra | sound | 280 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Close_02` | Lyra | sound | 279 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Close_03` | Lyra | sound | 279 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Close_04` | Lyra | sound | 279 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Noise-Exterior_01` | Lyra | sound | 562 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Noise-Exterior_02` | Lyra | sound | 589 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Noise-Exterior_03` | Lyra | sound | 579 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Noise-Exterior_04` | Lyra | sound | 698 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Distant_01` | Lyra | sound | 91 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Distant_02` | Lyra | sound | 69 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Distant_03` | Lyra | sound | 81 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Punch-Distant_04` | Lyra | sound | 77 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Noise-Exterior-Distant_01` | Lyra | sound | 649 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Noise-Exterior-Distant_02` | Lyra | sound | 473 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Noise-Exterior-Distant_03` | Lyra | sound | 514 KB |
| `/Game/Audio/Sounds/Explosions/Explosions_Grenade_Noise-Exterior-Distant_04` | Lyra | sound | 525 KB |
| `/Game/Audio/Sounds/Grenade/grenade_explode_thump_01` | Lyra | sound | 337 KB |
| `/Game/Audio/Sounds/Grenade/grenade_explode_thump_02` | Lyra | sound | 451 KB |
| `/Game/Audio/Sounds/Grenade/grenade_explode_thump_03` | Lyra | sound | 467 KB |
| `/Game/Audio/SoundWaves/Weapons/SFX_BulletImpact_01` | Lyra | sound | 56 KB |
| `/Game/Audio/SoundWaves/Weapons/SFX_BulletImpact_02` | Lyra | sound | 41 KB |
| `/Game/Audio/SoundWaves/Weapons/SFX_BulletImpact_03` | Lyra | sound | 60 KB |
| `/Game/Audio/SoundWaves/Weapons/SFX_BulletImpact_04` | Lyra | sound | 52 KB |
| `/Game/Audio/SoundWaves/Weapons/SFX_BulletImpact_05` | Lyra | sound | 40 KB |
| `/Game/Audio/SoundWaves/Weapons/SFX_BulletImpact_06` | Lyra | sound | 44 KB |
| `/Game/Audio/SoundWaves/Weapons/SFX_BulletImpact_07` | Lyra | sound | 53 KB |
| `/Game/Audio/SoundWaves/Weapons/SFX_BulletImpact_08` | Lyra | sound | 36 KB |
| `/Game/Audio/Sounds/Impacts/Lyra_Plyr_BulletImpact_01` | Lyra | sound | 123 KB |
| `/Game/Audio/Sounds/Impacts/Lyra_Plyr_BulletImpact_02` | Lyra | sound | 109 KB |
| `/Game/Audio/Sounds/Impacts/Lyra_Plyr_BulletImpact_03` | Lyra | sound | 139 KB |
| `/Game/Audio/Sounds/Impacts/Lyra_Plyr_BulletImpact_04` | Lyra | sound | 142 KB |
| `/Game/Audio/Sounds/Impacts/Lyra_Plyr_BulletImpact_05` | Lyra | sound | 156 KB |
| `/Game/Audio/Sounds/Impacts/Lyra_Plyr_BulletImpact_06` | Lyra | sound | 146 KB |
| `/Game/Audio/Sounds/WhizBys/sfx_AR_Whizbys_Oncoming_Close_01` | Lyra | sound | 39 KB |
| `/Game/Audio/Sounds/WhizBys/sfx_AR_Whizbys_Oncoming_Close_02` | Lyra | sound | 41 KB |
| `/Game/Audio/Sounds/WhizBys/sfx_AR_Whizbys_Oncoming_Close_03` | Lyra | sound | 45 KB |
| `/Game/Audio/Sounds/WhizBys/sfx_AR_Whizbys_Oncoming_Close_04` | Lyra | sound | 38 KB |
| `/Game/Audio/Sounds/WhizBys/sfx_AR_Whizbys_Oncoming_Mid_01` | Lyra | sound | 30 KB |
| `/Game/Audio/Sounds/WhizBys/sfx_AR_Whizbys_Oncoming_Mid_02` | Lyra | sound | 30 KB |
| `/Game/Effects/Curves/FX_Pyro` | Lyra | effect | 7 KB |
| `/Game/Effects/Curves/FX_Pyro_Fire` | Lyra | effect | 5 KB |
| `/Game/Effects/Curves/FX_Pyro_Sparks` | Lyra | effect | 4 KB |
| `/Game/Effects/MaterialFunctions/MF_AngleFade` | Lyra | effect | 11 KB |
| `/Game/Effects/MaterialFunctions/MF_ChannelRemap` | Lyra | effect | 25 KB |
| `/Game/Effects/MaterialFunctions/MF_ChannelRemapRGB` | Lyra | effect | 21 KB |
| `/Game/Effects/MaterialFunctions/MF_DepthFade` | Lyra | effect | 21 KB |
| `/Game/Effects/MaterialFunctions/MF_MotionStretch` | Lyra | effect | 39 KB |
| `/Game/Effects/MaterialFunctions/MF_NearDistanceFade` | Lyra | effect | 12 KB |
| `/Game/Effects/MaterialFunctions/MF_SpriteDiffuse` | Lyra | effect | 22 KB |
| `/Game/Effects/MaterialFunctions/MF_SpriteEmissive` | Lyra | effect | 29 KB |
| `/Game/Effects/MaterialFunctions/MF_SpriteOpacity` | Lyra | effect | 38 KB |
| `/Game/Effects/MaterialFunctions/MF_TextureChannelSelect` | Lyra | effect | 12 KB |
| `/Game/Effects/MaterialFunctions/MF_TextureDiffuse` | Lyra | effect | 38 KB |
| `/Game/Effects/MaterialFunctions/MF_TextureEmissive` | Lyra | effect | 38 KB |
| `/Game/Effects/MaterialFunctions/MF_TextureOpacity` | Lyra | effect | 31 KB |
| `/Game/Effects/Materials/BlastGraphics/M_Grenade_Blast` | Lyra | effect | 27 KB |
| `/Game/Effects/Materials/BlastGraphics/M_Grenade_EX_Flare` | Lyra | effect | 12 KB |
| `/Game/Effects/Materials/Decals/M_ExplosionDecal` | Lyra | effect | 25 KB |
| `/Game/Effects/Materials/Meshes/M_DebrisChunks_1` | Lyra | effect | 10 KB |
| `/Game/Effects/Materials/Pyro/Instances/MI_Flipbook_Pyro_Grenade` | Lyra | effect | 21 KB |
| `/Game/Effects/Materials/Pyro/M_Flipbook_Pyro_Base` | Lyra | effect | 43 KB |
| `/Game/Effects/Materials/Sparks/Instances/MI_Sparks_Metal` | Lyra | effect | 7 KB |
| `/Game/Effects/Materials/Sparks/M_Sparks_Base` | Lyra | effect | 25 KB |
| `/Game/Effects/Meshes/Rocks/Debris_Chunk1` | Lyra | effect | 66 KB |
| `/Game/Effects/Meshes/Rocks/Debris_Chunk2` | Lyra | effect | 60 KB |
| `/Game/Effects/Meshes/Rocks/Debris_Chunk3` | Lyra | effect | 38 KB |
| `/Game/Effects/Meshes/box` | Lyra | effect | 17 KB |
| `/Game/Effects/Particles/Explosion/NS_Grenade_Explosion` | Lyra | effect | 5322 KB |
| `/Game/Effects/Textures/BlastGraphics/T_ExplosionFlare` | Lyra | effect | 728 KB |
| `/Game/Effects/Textures/Decals/T_ExplosionDecal` | Lyra | effect | 3724 KB |
| `/Game/Effects/Textures/Decals/T_ExplsionDigital` | Lyra | effect | 248 KB |
| `/Game/Effects/Textures/Flipbooks/T_Explosion_Areal_E64` | Lyra | effect | 32293 KB |
| `/Game/Effects/Textures/Rocks/Debris_Chunk1_BaseColor` | Lyra | effect | 94 KB |

### 自作の音（7個。`Setup/SynthAudio/`、48kHz・16bit・モノラル）

| 取り込み先（/Game/Audio/Battle） | 長さ | 作り方（`Setup/Make-SynthSounds.py`） |
|---|---|---|
| `SW_Synth_RocketLaunch` | 1.5秒 | 点火の破裂音＋低い衝撃（周波数を下げる正弦波）＋高域・中域の噴射音（帯域を絞った雑音）＋火花の粒 |
| `SW_Synth_MortarLaunch` | 1.0秒 | 低い「ドン」（下がる正弦波）＋破裂音＋筒の響き（170Hz）＋空気の抜け |
| `SW_Synth_ShellIncoming` | 1.9秒 | 1600Hz→550Hz に下がる笛の音（ゆれ付き）を次第に大きく＋風切りの雑音 |
| `SW_Synth_CaptureFriendly` | 1.0秒 | 鐘のような上がる2音（ミ→シ）＋高い余韻 |
| `SW_Synth_CaptureEnemy` | 1.1秒 | 下がる2音（ラ→ミ♭、三全音）を奇数倍音のこもった音色で |
| `SW_Synth_Victory` | 1.8秒 | 金管風のド・ミ・ソ→高いドの和音 |
| `SW_Synth_Defeat` | 2.0秒 | 金管風のラ・ファ・レ→イ短調の和音（暗め） |

## 作り直し・追加の手順

1. 手元の素材を写す：`python Setup/Import-AudioFXAssets.py`（`--dry-run` で一覧だけ）。選ぶ音・効果はスクリプト冒頭の表で変える。参照を持つ音を選ぶと止まる。
2. 自作の音を作る：`python Setup/Make-SynthSounds.py`（毎回同じ音になる）。
3. 自作の音をUEに取り込む：`Setup\Import-SynthSounds.ps1`（UEのコマンドレット。**計測ロックが要る**）。
4. 音の割り当て・音量・聞こえる距離は `BattleAudio.cpp` 冒頭の表、効果の選び方は `BattleEffects.cpp`。
