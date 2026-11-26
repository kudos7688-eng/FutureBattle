# 素材キット（/Game/Kit）の素材と出典（2026-11-27、系統「グラフィック」）

どのマップでも使い回せる見た目の素材一式。元データは `Art/Kit/`（git の外）、UE への取り込みは `Setup/Import-Kit.ps1`（`/Game/Kit/` に入る）。ゲーム内では `-BattleKitLook=1`（`BattleKitLook.cpp`、既定0）で使う。表示だけで、衝突・判定は変えない。

## 出典ごとのまとめ

| 出典 | 使ったもの | 利用条件 |
|---|---|---|
| **Tripo**（tripo3d.ai、文章からの生成 text-to-model v3.1。ユーザーの契約の API キーを Blender のアドオン設定から使用） | 武器11種・小物16種の形とテクスチャ | Tripo の契約の条件に従う（生成物の利用条件はユーザーの契約プランによる。要確認の点：商用利用の可否はプランで異なる）。指示文と task id は各 `Source/*.glb.task.json` |
| **Poly Haven**（polyhaven.com） | 地面・壁などの表面26種（1K、Diffuse / nor_dx / arm） | **CC0 1.0**（表示・許諾不要）。ID・作者・ハッシュは `Art/Kit/Surfaces/surfaces.json` |
| **自作**（Claude が Blender で作成、外部素材なし） | エフェクトのコマ送り画像（爆炎・煙・土ぼこり・発砲煙、8×8）。雑音で形を付けたボリュームをレンダリング（`Art/Kit/Scripts/make_flipbook.py`） | 制限なし |

## 武器（`/Game/Kit/Weapons/SM_Kit_<名前>`。原点＝握り、銃身はローカル -Y、実寸）

| 名前 | 全長 m | 三角形 | Tripo task | 指示文 |
|---|---|---|---|---|
| AntiTank | 1.0 | 8,000 | `04562724-1c64-4e63-be84-41720e633528` | near-future shoulder-fired anti-tank rocket launcher, long tube with optical sight and pistol grip |
| Carbine | 0.8 | 8,000 | `416525c5-0499-46a0-a11c-62104884e2e0` | near-future military assault rifle seen from the side: one barrel with a muzzle brake at the front end only, a solid fixed shoulder stock with a flat butt pad at the rear end, one pistol grip, one curved magazine in front of the trigger guard, red dot sight on a top rail |
| Grenade | 0.11 | 3,000 | `d98d4b06-51a4-445c-9f7e-2078184c3793` | near-future fragmentation hand grenade with safety lever and pin |
| GrenadeLauncher | 0.75 | 8,000 | `6489fa12-7bc6-454a-b489-f46b342a6050` | near-future multiple grenade launcher with six-round revolver cylinder drum and short stock |
| Marksman | 1.05 | 7,998 | `6dc3d820-9ee1-42cb-b20a-600ce0a42a54` | near-future designated marksman rifle with long barrel and telescopic scope |
| Pistol | 0.21 | 3,998 | `7d8a5b60-80a5-40af-9b25-5d51a23a991a` | near-future military semi-automatic pistol |
| SMG | 0.6 | 7,998 | `860558c3-51af-4439-9868-fe3859235fe3` | near-future compact submachine gun with short stock and long straight magazine |
| Shotgun | 0.85 | 8,000 | `3e6e5d77-d458-4245-9422-5041a20517a4` | near-future pump-action combat shotgun with tube magazine and short stock |
| Sniper | 1.35 | 8,000 | `bee93e58-8634-4482-8957-fa38a2b7ace0` | near-future long-range anti-materiel sniper rifle with large scope, very long heavy barrel with muzzle brake and bipod |
| SupportGun | 1.1 | 8,000 | `e2f43a6a-d014-4628-9986-c82086180307` | near-future belt-fed light machine gun seen from the side: one long heavy barrel with carrying handle at the front end only, a large square ammunition box under the receiver, an open bipod under the barrel, a solid shoulder stock at the rear end |

武器の共通の指示：`matte white ceramic armor panels over a black metal frame, near-future military, hard-surface game asset ...`（AC01・RB01 の白い板と黒いフレームに合わせた）。小銃は1回目が両端に銃口の付いた形だったので作り直した（ユーザー指摘。1回目は `Carbine_Tripo_v1_two_muzzles.glb` として残してある）。支援火器も1回目が小銃に似ていたので作り直した。

## 小物（`/Game/Kit/Props/SM_Kit_<名前>`。原点＝底面の中心、長い辺がローカル X、実寸）

| 名前 | 大きさ m（X×Y×Z） | 三角形 | Tripo task | 指示文 |
|---|---|---|---|---|
| Boulder | 2.5×2.449×2.564 | 5,000 | `fd9e6915-29d7-410e-a813-a21db89cf461` | large natural granite boulder rock with some moss |
| CarWreck | 4.4×1.803×1.545 | 10,000 | `27fd76ba-3818-4ec1-8441-58d8b91019f0` | burnt-out wrecked civilian sedan car, rusted body, flat tires, broken windows |
| Container | 3.662×2.287×2.6 | 5,000 | `e9a6e87d-aad9-4c0a-8c68-b521df2d2e0f` | 20 foot steel shipping container, weathered dark green paint, closed doors, corrugated sides |
| Hedge | 3.0×0.869×0.927 | 6,000 | `13d4ec95-3553-4c96-90fe-967b215ae714` | dense trimmed green garden hedge, one straight rectangular block segment with flat ends |
| Hesco | 1.2×0.903×0.867 | 7,860 | `a25b86ed-5a69-4488-a631-c16a0c8e2ace` | military hesco bastion barrier, one square wire mesh and fabric container filled with earth |
| JerseyBarrier | 3.0×1.222×1.189 | 5,000 | `5559cca6-ad82-4994-80c2-56c62ba72c8e` | weathered gray concrete jersey barrier road block, one straight segment with chipped edges |
| Mortar | 0.9×0.623×0.525 | 8,000 | `f95c3903-e1d2-4f1c-8125-8e8a7e41ec92` | near-future portable infantry mortar, short launch tube with round baseplate and bipod legs |
| OilBarrel | 0.63×0.628×0.9 | 3,000 | `54fa6806-6ebb-48a8-9c38-2a88b3ab4437` | rusty dented steel 200 litre oil drum barrel standing upright |
| RuinWall | 4.0×1.71×2.931 | 6,000 | `b73c2bc1-6a43-436b-87c0-f07a8e678fda` | ruined broken red brick wall segment with remains of plaster and a jagged broken top, war damage |
| SandbagWall | 2.5×0.781×0.894 | 6,000 | `7f054e83-cbc8-40d0-9666-4d5dee71d092` | straight military sandbag wall segment, burlap sandbags stacked four rows high, flat ends |
| StoneWall | 3.0×0.456×0.749 | 6,000 | `a5667b54-b622-4b87-b146-b8a8d07e8fc0` | low dry stone field wall, one straight segment with flat ends, grey stacked stones |
| TankTrap | 1.75×0.931×1.4 | 3,000 | `3ba1cd7f-94d9-4156-b36f-af50bf10e132` | rusty steel czech hedgehog anti-tank obstacle made of three crossed I-beams |
| TreeBroadleaf | 8.199×7.494×9.0 | 12,000 | `f4044c74-0794-4b44-9e0b-4590f8454160` | stylized broadleaf oak tree, thick trunk, dense solid green leaf canopy made of chunky leaf clusters |
| TreeConifer | 6.405×6.398×12.0 | 12,000 | `592d4234-4f2b-4199-ae5d-2b6efc686df7` | stylized tall pine conifer tree, straight trunk, dense solid layered green needle canopy |
| Truck | 6.082×3.089×2.9 | 12,000 | `fa329b81-7352-494a-aeb8-cd382381b094` | near-future military six-wheel cargo truck with canvas-covered cargo bed, olive drab |
| WoodCrate | 1.0×0.767×0.64 | 3,000 | `6bb33c2d-9d96-49b8-b52a-57e83e3069cd` | military wooden supply crate with metal corner brackets and stencil markings, olive green paint |

迫撃砲（Mortar）は据え置きの形なので小物に入れた（手に持つ武器の見た目は従来のまま）。

## 表面（`/Game/Kit/Surfaces/MI_Kit_<名前>`。親 `M_Kit_Surface`＝ワールド座標の3面投影。拡大した立方体でも UV の無いメッシュでも実寸で並ぶ）

| 名前 | Poly Haven の ID | 作者 | 1枚の実寸 m |
|---|---|---|---|
| Asphalt | [asphalt_02](https://polyhaven.com/a/asphalt_02) | Rob Tuytel | 3.0 |
| Bark | [bark_brown_02](https://polyhaven.com/a/bark_brown_02) | Rob Tuytel | 1.0 |
| BrickRed | [red_brick_03](https://polyhaven.com/a/red_brick_03) | Rob Tuytel | 1.0 |
| Cobble | [cobblestone_floor_04](https://polyhaven.com/a/cobblestone_floor_04) | Rob Tuytel | 1.5 |
| Concrete | [concrete_wall_003](https://polyhaven.com/a/concrete_wall_003) | Dimitrios Savva, Rico Cilliers | 3.0 |
| ConcreteCracked | [cracked_concrete_wall](https://polyhaven.com/a/cracked_concrete_wall) | Dimitrios Savva, Rico Cilliers | 1.0 |
| ConcreteDirty | [dirty_concrete](https://polyhaven.com/a/dirty_concrete) | Rob Tuytel | 3.0 |
| DirtPath | [stony_dirt_path](https://polyhaven.com/a/stony_dirt_path) | eye-candy.xyz | 2.17 |
| FarmSoil | [farm_soil](https://polyhaven.com/a/farm_soil) | Amal Kumar | 2.0 |
| ForestFloor | [forest_ground_04](https://polyhaven.com/a/forest_ground_04) | Rob Tuytel, Rico Cilliers | 3.15 |
| Grass | [leafy_grass](https://polyhaven.com/a/leafy_grass) | Charlotte Baglioni | 2.0 |
| GrassSparse | [sparse_grass](https://polyhaven.com/a/sparse_grass) | Amal Kumar | 2.0 |
| Gravel | [gravel_floor_02](https://polyhaven.com/a/gravel_floor_02) | Jenelle van Heerden, Dimitrios Savva | 2.0 |
| MetalPlate | [metal_plate](https://polyhaven.com/a/metal_plate) | Rob Tuytel | 0.5 |
| MetalRust | [rusty_metal_02](https://polyhaven.com/a/rusty_metal_02) | Rob Tuytel | 1.0 |
| Pavement | [concrete_pavement](https://polyhaven.com/a/concrete_pavement) | Charlotte Baglioni | 1.8 |
| PlasterGrey | [grey_plaster](https://polyhaven.com/a/grey_plaster) | Rob Tuytel | 1.0 |
| PlasterPainted | [painted_plaster_wall](https://polyhaven.com/a/painted_plaster_wall) | Amal Kumar | 2.0 |
| PlasterWhite | [white_plaster_02](https://polyhaven.com/a/white_plaster_02) | Rob Tuytel | 1.0 |
| RockFace | [rock_face_03](https://polyhaven.com/a/rock_face_03) | Dario Barresi, Rico Cilliers | 2.7 |
| RockGround | [aerial_rocks_02](https://polyhaven.com/a/aerial_rocks_02) | Rob Tuytel | 50.0 |
| RoofClay | [clay_roof_tiles_02](https://polyhaven.com/a/clay_roof_tiles_02) | Amal Kumar | 2.5 |
| RoofSlate | [roof_slates_02](https://polyhaven.com/a/roof_slates_02) | Rob Tuytel | 3.0 |
| Sand | [sandy_gravel](https://polyhaven.com/a/sandy_gravel) | Charlotte Baglioni | 2.1 |
| StoneBlock | [stone_brick_wall_001](https://polyhaven.com/a/stone_brick_wall_001) | Dimitrios Savva, Rico Cilliers | 2.5 |
| WoodRough | [rough_wood](https://polyhaven.com/a/rough_wood) | Rob Tuytel | 0.5 |

## 作り直しの手順

```
python -I Art/Kit/Scripts/fetch_polyhaven.py Art/Kit/Surfaces          # 表面（CC0）を取得
blender --background --python Art/Kit/Scripts/tripo_text.py -- <out.glb> "<指示文>"   # Tripo で1個生成（1個30クレジット）
bash Art/Kit/Scripts/make_all_weapons.sh    # 武器：縮尺・握りを原点に・8,000三角形・2Kの焼き込み・FBX
bash Art/Kit/Scripts/make_all_props.sh      # 小物：軸にそろえる・底面を原点に・焼き込み・FBX
Setup\Import-Kit.ps1                        # UE へ取り込み（計測ロックが要る）
```

