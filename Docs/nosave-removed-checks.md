# 保存・読み込み削除で除いたスモーク確認

2026-11-25。以下は削除前の Check / CheckSmoke / CompositionCheck と検査用ラッパーへ渡していた TEXT の文言。繰り返し実行される確認はソース上の呼び出し単位で記載。ビルド・試合実行はしていない。

## BattleAreaSmoke.cpp

- `TEXT("version three validates pending replenishment")`
- `TEXT("oversubscribed replacement resources rejected")`
- `TEXT("invalid leader replacement rejected without mutation")`
- `TEXT("F8 F9 preserve pending clocks and unspent resources")`
- `TEXT("retired contacts and new roster restore atomically")`
- `TEXT("save with a hired squad validates")`
- `TEXT("hire spending outside conservation rejected")`
- `TEXT("hired squad survives restore")`
- `TEXT("planner actual snapshot restore preserves full authoritative team state")`
- `TEXT("composition stage one rebuilds budget roster and time from existing save")`
- `TEXT("two hires and a reservation validate")`
- `TEXT("heavy infantry survives save and restore")`
- `TEXT("handed-over scout pair survives save and restore")`
- `TEXT("tank survives save and restore")`
- `TEXT("armoured car survives save and restore")`
- `TEXT("group persistence fixture formed")`
- `TEXT("artillery extension written to disk")`
- `TEXT("mortar setup sequence and in-flight shell survive serialized restore")`
- `TEXT("serialized restore preserves all group history membership and armour choices")`
- `TEXT("invalid weighted strength baseline rejected")`
- `TEXT("group key identity mismatch rejected atomically")`
- `TEXT("duplicate group member rejected atomically")`
- `TEXT("missing reciprocal membership rejected atomically")`
- `TEXT("foreign group reference rejected atomically")`
- `TEXT("reused next group identity rejected atomically")`
- `TEXT("enemy group membership rejected atomically")`
- `TEXT("invalid role rejected atomically")`
- `TEXT("invalid group phase rejected atomically")`
- `TEXT("invalid avoided objective rejected atomically")`
- `TEXT("non-finite group clock rejected atomically")`
- `TEXT("future arrival rejected atomically")`
- `TEXT("excessive avoid deadline rejected atomically")`
- `TEXT("negative failure history rejected atomically")`
- `TEXT("invalid armour objective rejected atomically")`
- `TEXT("non-finite armour clock rejected atomically")`
- `TEXT("invalid unset armour clock rejected atomically")`
- `TEXT("strategic payload without its extension version rejected")`
- `TEXT("body staging failure retains existing groups and choices")`
- `TEXT("restore failure reaches body staging")`
- `TEXT("failed restore retains unsaved route choice")`
- `TEXT("successful restore discards unsaved route choices")`
- `TEXT("group and armour clocks rebase on restored match time")`
- `TEXT("expired deadline and unset armour choice survive without clamping")`
- `TEXT("legacy snapshot without group extension clears unsaved strategy")`
- `TEXT("revision two restores strategy after legacy loads")`
- `TEXT("restored shell waits remaining flight time")`
- `TEXT("restored shell impacts exactly once")`
- `TEXT("artillery revision 3 serializes its trial configuration")`
- `TEXT("self-propelled gun survives save and restore")`
- `TEXT("self-propelled shell survives restore")`
- `TEXT("SPG empty ammunition survives restore without refill")`
- `TEXT("different blast trial configuration rejected without mutation")`
- `TEXT("negative SPG ammunition rejected")`
- `TEXT("revision 2 save migrates SPG ammunition only when trial enabled")`
- `TEXT("legacy load explicitly disables planner without implicit conversion")`
- `TEXT("revision four restores planner after legacy load")`
- `TEXT("identified and unknown contact metadata survive serialization")`
- `TEXT("strength/shield trial mismatch rejected before mutation")`
- `TEXT("non-finite exoskeleton resources rejected")`
- `TEXT("negative exoskeleton resources rejected")`
- `TEXT("exoskeleton resource upper bounds enforced")`
- `TEXT("exoskeleton soldier survives save and restore with its shield, launcher rounds and look")`
- `TEXT("income joins resource conservation")`
- `TEXT("income outside conservation rejected")`
- `TEXT("war score joins the snapshot")`
- `TEXT("invalid war score rejected")`

## BattleCompositionSmoke.cpp

- `TEXT("planner can capture area fixture")`
- `TEXT("planner revision four serializes full saving and income state")`
- `TEXT("planner post-save decision and state match uninterrupted calculation")`
- `TEXT("planner rejects old policy one v4 instead of guessing emergency provenance")`
- `TEXT("planner rejects unknown emergency squad reference")`
- `TEXT("planner rejects emergency provenance without matching hire purpose")`
- `TEXT("planner rejects emergency deadline beyond thirty seconds")`
- `TEXT("planner save rejects invalid type")`
- `TEXT("planner save rejects inverted deadline")`
- `TEXT("planner save rejects unknown observation ID")`
- `TEXT("planner save rejects own-team vehicle memory")`
- `TEXT("planner rule mismatch rejected before mutating live match")`
- `TEXT("planner state cannot be smuggled into older extension")`
- `TEXT("legacy revision three remains readable without planner data")`

## BattleCoverActionSmoke.cpp

- `TEXT("active cover snapshot validates")`
- `TEXT("active cover serializes and restores")`
- `TEXT("cover phase and anchor survive loading")`
- `TEXT("peek destination and Mass target survive serialization")`

## BattleDelegationSmoke.cpp

- `TEXT("Delegation snapshot serializes and validates (%s)")`
- `TEXT("Snapshot restore preserves per-squad command ownership")`

## BattleMixedFastSmoke.cpp

- `TEXT("variable roster validates for persistence")`
- `TEXT("F8 F9 restore purchases resources and dead leader")`
- `TEXT("post-load purchase allocates fresh IDs")`
- `TEXT("live purchased squads remain serializable")`

## BattleSquadTacticsSmoke.cpp

- `TEXT("ST1 refuses unsupported active-plan snapshot rather than dropping it")`
- `TEXT("Mission snapshot validates: %s")`
- `TEXT("Mission survives real binary save and world restore")`
- `TEXT("Legacy missing mission fields migrate from stored own orders")`
- `TEXT("Invalid mission save is rejected before world mutation")`
- `TEXT("Last member death cancels tactical authority immediately")`
- `TEXT("Recent own damage is recorded without enemy state access")`
- `TEXT("Death fixture restores clean live match")`

## BattleSupportSmoke.cpp

- `TEXT("Pending impact delay cooldown and consumed mission count are included in save validation")`
- `TEXT("Restoring an in-flight strike neither duplicates it nor refunds its mission")`
- `TEXT("Corrupt support timer rejects the complete snapshot without altering the current mission")`

## BattleLeaderSmoke.cpp

- `TEXT("unsupported save is rejected")`

## BattlePerceptionSmoke.cpp

- `TEXT("Restored pending transmission is neither duplicated nor delivered early on the next sensor update")`
- `TEXT("Restored report keeps its original rebased observation and delivery time")`

## BattleSaveSmoke.cpp

- `TEXT("Live match snapshot validates before serialization (%s)")`
- `TEXT("Snapshot stores FPS target and remaining cooldowns rather than absolute world time")`
- `TEXT("Binary round trip preserves units economy and pending shared information")`
- `TEXT("Eight corrupt snapshots reject atomically without changing actors control resources or match ID")`
- `TEXT("Dedicated smoke save slot writes successfully without touching the player quicksave")`
- `TEXT("Disk save reload restores a validated match")`
- `TEXT("Cross-sector look permission keeps its time left through a disk reload and a later one is dropped")`
- `TEXT("Disk load re-equips saved support weapon on replacement actor")`
- `TEXT("New actors recover the same authoritative ID health ammunition and purchased capability")`
- `TEXT("Match identity economy score elapsed time and pause survive disk restoration")`
- `TEXT("Persistent orders and FPS view survive while transient navigation is rebuilt")`
- `TEXT("Remembered enemy stays hidden at its saved known location with the same age")`
- `TEXT("Post-load recruitment allocates new IDs without colliding with restored members")`
- `TEXT("Unsupported save slot leaves active match intact")`
- `TEXT("Restored reload finishes once after its remaining duration and conserves ammunition")`

## BattleMixedSaveSmoke.cpp

- `TEXT("fixture has real acquired contact")`
- `TEXT("active retreat fixture")`
- `TEXT("active support fixture")`
- `TEXT("active flank fixture")`
- `TEXT("mixed snapshot validates")`
- `TEXT("canonical 50 squads x rifle squad size records")`
- `TEXT("50 Actor leaders and the rest Mass followers encoded")`
- `TEXT("separate mixed smoke slot written")`
- `TEXT("serialized mixed snapshot restored")`
- `TEXT("follower resources survive body replacement")`
- `TEXT("relative reload and shot timers survive")`
- `TEXT("representation counts preserved")`
- `TEXT("three active tactical actions restored")`
- `TEXT("coordination references and elapsed timers restored")`
- `TEXT("decision clocks and recent damage preserved")`
- `TEXT("dead leader stays disabled")`
- `TEXT("all canonical IDs retain their transforms")`
- `TEXT("active routes and arrival reservations rebuilt")`
- `TEXT("failed restore preserves nonempty cover/fire/suppression/stance/lock maps")`
- `TEXT("weapon outside squad type rejected atomically")`
- `TEXT("ammo above magazine capacity rejected atomically")`
- `TEXT("reserve above capacity rejected atomically")`
- `TEXT("health above squad limit rejected atomically")`
- `TEXT("oversize bytes rejected before codec")`
- `TEXT("full health and reserve with reload in progress remain valid")`
- `TEXT("overflow shot counter rejected atomically")`
- `TEXT("inactive mission counter overflow rejected atomically")`
- `TEXT("duplicate damage bucket rejected atomically")`
- `TEXT("fractional damage bucket rejected atomically")`
- `TEXT("descending damage buckets rejected atomically")`
- `TEXT("excessive per-squad damage buckets rejected atomically")`
- `TEXT("invalid tactical sample observed type rejected atomically")`
- `TEXT("duplicate identity rejected atomically")`
- `TEXT("enemy support reference rejected atomically")`
- `TEXT("follower Actor type corruption rejected atomically")`
- `TEXT("foreign map rejected atomically")`
- `TEXT("changed range rules rejected atomically")`
- `TEXT("unknown combat extension rejected")`
- `TEXT("shield on rifle soldier rejected")`
- `TEXT("negative launcher timer rejected")`
- `TEXT("invalid shell shooter rejected")`
- `TEXT("legacy schema cannot replace mixed match")`
- `TEXT("unreachable saved route rejected before body replacement")`
- `TEXT("restore failure reaches route validation")`
- `TEXT("failure after staging Actor and Mass bodies rolls back atomically")`
- `TEXT("restore failure reaches Actor/Mass staging")`
- `TEXT("unapproved slot does not write")`
- `TEXT("reload boundary transfers only due ammunition with switch ON")`
- `TEXT("reload capture leaves live resources and deadline untouched")`
- `TEXT("nonboundary save bytes identical with reload switch ON and OFF")`
- `TEXT("restored due reload transfers ammunition exactly once")`
- `TEXT("due reload snapshot restores")`
- `TEXT("restore fixture after reload boundary check")`
- `TEXT("TPS control restored to recreated leader")`
- `TEXT("restored leader returns to RTS")`
- `TEXT("player route current step fixture")`
- `TEXT("route-free OFF baseline encoded")`
- `TEXT("route-free ON save bytes identical to OFF (AI round trip)")`
- `TEXT("OFF omits route section and keeps route-free save bytes")`
- `TEXT("OFF archive contains no route property tags")`
- `TEXT("player route extension codec and restore")`
- `TEXT("final Defend, goal, two waypoints, step, lane, flags and clocks survive")`
- `TEXT("failed restore retains complete player route and clock")`
- `TEXT("bad player route squad rejected atomically")`
- `TEXT("enemy player route rejected atomically")`
- `TEXT("disabled player route rejected atomically")`
- `TEXT("NaN player waypoint rejected atomically")`
- `TEXT("out-of-map player waypoint rejected atomically")`
- `TEXT("excessive player waypoint count rejected atomically")`
- `TEXT("duplicate player route rejected atomically")`
- `TEXT("invalid final order rejected atomically")`
- `TEXT("nonfinite route deadline rejected atomically")`
- `TEXT("unknown player route version rejected atomically")`
- `TEXT("orphan player route section rejected atomically")`
- `TEXT("expired route deadline and lane-reset sentinels remain valid")`
- `TEXT("route extension rejected atomically when switch OFF")`
- `TEXT("route policy mismatch is explicit")`
- `TEXT("route retained after body staging failure")`
- `TEXT("legacy route-free save still clears routes with switch ON")`
- `TEXT("legacy route-free save still clears routes with switch OFF")`
- `TEXT("pre-extension mixed snapshot remains loadable")`
- `TEXT("restore fixture for live continuation")`
- `TEXT("F8/F9 controller callbacks reach mixed save/load using isolated test slot")`
- `TEXT("restored retreat continues through real Actor movement")`
- `TEXT("restored follower reload completes")`
- `TEXT("live continuation preserves mixed representation")`
- `TEXT("live mixed combat remains serializable after clocks advance")`

## 付記

- BattleObjectiveSmoke.cpp: 保存専用試験の呼び出しを削除（確認文言は専用ファイル欄）。
- BattleMatchSoak.cpp: 20秒以後のディスク往復と、それを必須とする成功条件を削除。save_reload は false。
- BattleMixedFastSmoke.cpp: post-load purchase の確認は、同じ追加購入を行う subsequent purchase allocates fresh IDs に置換。保存を挟まない現在の資金・IDで実行し、後段の50体以上確認を維持。
- BattleSupportSmoke.cpp: 復元で3秒へ巻き戻す代わりに、既に残っている1秒を1.01秒進める。明示的に一時停止を解除し、通常の着弾・遮蔽・味方保護・回数制限の確認を維持。
- 観測情報を注入する Perception::Restore はセーブファイルを使わない試験fixtureとして維持。
- SquadTactics の死亡fixtureは後続の実時間観測試験に生存状態を戻すため保存・復元に依存していたので削除。

文言数: 206
