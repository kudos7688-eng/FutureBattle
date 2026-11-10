#include "BattleViewVisibility.h"
#include "SceneRendering.h"
#include "PrimitiveSceneInfo.h"
#include "PrimitiveSceneProxy.h"
#include "SceneInterface.h"
#include "StaticMeshBatch.h"

void FT5Visibility::PostRenderView_RenderThread(FRDGBuilder&,FSceneView& Base) {
 if(Base.bIsSceneCapture || !Base.Family || !Base.Family->Scene) return;
 const double Start=FPlatformTime::Seconds();
 const FViewInfo& View=static_cast<const FViewInfo&>(Base);
 auto* Scene=Base.Family->Scene; const auto IDs=Scene->GetScenePrimitiveComponentIds(); FT5ViewCount C; TSet<FPrimitiveComponentId> Submitted;
 for(int32 I=0;I<IDs.Num();++I) {
  if(WeaponSlots.Num()) if(const int32* Slot=WeaponSlots.Find(IDs[I])) {
   const auto* Info=Scene->GetPrimitiveSceneInfo(I);if(Info && Info->Proxy && Info->Proxy->IsShown(&View)) {const auto& B=Info->Proxy->GetBounds();if(View.ViewFrustum.IntersectBox(B.Origin,B.BoxExtent)) C.WeaponFrustum[*Slot/32]|=1u<<(*Slot%32);}
  }
  // Shadow eligibility is separate from main-view visibility (including hidden casters).
  if(InspectShadows) if(const int32* Slot=OriginalSlots.Find(IDs[I])) {
   const auto* Info=Scene->GetPrimitiveSceneInfo(I);
   if(Info && Info->Proxy && Info->Proxy->IsShadowCast(&View)) C.ShadowBodies[*Slot/32]|=1u<<(*Slot%32);
  }
  if(InspectShadows) if(const int32* Slot=WeaponSlots.Find(IDs[I])) {
   const auto* Info=Scene->GetPrimitiveSceneInfo(I);
   if(Info && Info->Proxy && Info->Proxy->IsShadowCast(&View)) C.ShadowWeapons[*Slot/32]|=1u<<(*Slot%32);
  }
  const int32* G=Groups.Find(IDs[I]);if(!G) continue;
  const auto* Info=Scene->GetPrimitiveSceneInfo(I); if(!Info || !Info->Proxy || !Info->Proxy->IsShown(&View)) continue;
  const auto& B=Info->Proxy->GetBounds();
  const bool InFrustum=View.ViewFrustum.IntersectBox(B.Origin,B.BoxExtent);C.Frustum[*G]+=InFrustum;
  if(InFrustum && OriginalSlots.Num()) if(const int32* Slot=OriginalSlots.Find(IDs[I])) C.OriginalFrustum[*Slot/32]|=1u<<(*Slot%32);
  if(View.PrimitiveVisibilityMap[I] && View.PrimitiveViewRelevanceMap[I].bDrawRelevance && View.PrimitiveViewRelevanceMap[I].bRenderInMainPass) {
   for(const auto& Mesh:Info->StaticMeshes) if(View.StaticMeshVisibilityMap[Mesh.Id] && Mesh.bUseForMaterial && Mesh.Type==PT_TriangleList && Mesh.GetNumPrimitives()>0) {
    if(!Submitted.Contains(IDs[I])) {Submitted.Add(IDs[I]);++C.Drawn[*G];}
    C.BodyTriangles[*G]+=Mesh.GetNumPrimitives();
   }
  }
  
 }
 for(const auto& R:View.DynamicMeshElements) {
  const auto ID=R.PrimitiveSceneProxy->GetPrimitiveComponentId();const int32* G=Groups.Find(ID);
  if(!G || !R.Mesh->bUseForMaterial || R.Mesh->Type!=PT_TriangleList || R.Mesh->GetNumPrimitives()==0) continue;
  const int32 I=R.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetIndex();
  if(!View.PrimitiveVisibilityMap[I] || !View.PrimitiveViewRelevanceMap[I].bDrawRelevance || !View.PrimitiveViewRelevanceMap[I].bRenderInMainPass) continue;
  if(!Submitted.Contains(ID)) {Submitted.Add(ID);++C.Drawn[*G];}
  C.BodyTriangles[*G]+=R.Mesh->GetNumPrimitives();
 }
 if(OriginalSlots.Num()) for(const auto& ID:Submitted) if(const int32* Slot=OriginalSlots.Find(ID)) C.OriginalDrawn[*Slot/32]|=1u<<(*Slot%32);
 C.ObserverMS=(FPlatformTime::Seconds()-Start)*1000.;
 FScopeLock Lock(&Mutex); Counts.Add(FMath::RoundToInt64(Base.Family->Time.GetWorldTimeSeconds()*30000.),C);
}
