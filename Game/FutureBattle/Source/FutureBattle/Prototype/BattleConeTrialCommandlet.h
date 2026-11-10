#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BattleConeTrialCommandlet.generated.h"
UCLASS()
class UBattleConeTrialCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UBattleConeTrialCommandlet();
 virtual int32 Main(const FString& Params) override;
};
