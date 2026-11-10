#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BattleVATBakeCommandlet.generated.h"
UCLASS()
class UBattleVATBakeCommandlet : public UCommandlet
{
 GENERATED_BODY()
public:
 UBattleVATBakeCommandlet();
 virtual int32 Main(const FString& Params) override;
};
