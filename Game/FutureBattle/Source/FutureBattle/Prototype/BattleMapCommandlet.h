#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BattleMapCommandlet.generated.h"

// Reproducible editor-only arena authoring; never overwrites a map unless -Force is passed.
UCLASS()
class UBattleMapCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UBattleMapCommandlet();
	virtual int32 Main(const FString& Params) override;
};
