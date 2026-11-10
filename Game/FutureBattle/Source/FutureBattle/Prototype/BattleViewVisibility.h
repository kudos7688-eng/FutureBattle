#pragma once
#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "PrimitiveComponentId.h"

struct FT5ViewCount { int32 Frustum[2]={}, Drawn[2]={}; int64 BodyTriangles[2]={}; double ObserverMS=0.; uint32 OriginalFrustum[16]={},OriginalDrawn[16]={},WeaponFrustum[16]={},ShadowBodies[16]={},ShadowWeapons[16]={}; };
// Test-only read-only observer. UE 5.8 renderer-private visibility map is intentional:
// LastRenderTimeOnScreen is also updated for off-screen ray tracing geometry.
class FT5Visibility : public FSceneViewExtensionBase {
public:
 FT5Visibility(const FAutoRegister& R):FSceneViewExtensionBase(R) {}
 TMap<FPrimitiveComponentId,int32> Groups; // immutable after creation, before first render
 TMap<FPrimitiveComponentId,int32> OriginalSlots,WeaponSlots; // populated only for T5 staged tests
 bool InspectShadows=false;
 FCriticalSection Mutex;
 TMap<int64,FT5ViewCount> Counts;
 virtual void SetupViewFamily(FSceneViewFamily&) override {}
 virtual void SetupView(FSceneViewFamily&,FSceneView&) override {}
 virtual void BeginRenderViewFamily(FSceneViewFamily&) override {}
 virtual void PostRenderView_RenderThread(FRDGBuilder&,FSceneView&) override;
};
