#pragma once
#include "CoreMinimal.h"

// Display-time Japanese text for the HUD (user decision 2026-11-16: UI text is Japanese). Game state, logs, saves and AI
// logic keep their English strings (some code tests prefixes such as "HOME SUPPLY"); only what is drawn is translated.
// Lookup order: whole string, known patterns with numbers, then each " / " segment. Unknown text is shown unchanged.
namespace BattleUIText
{
 FString Ja(const FString& Text);
 // Save UI uses the actual result; unknown diagnostics remain in the English log.
 FString SaveResult(bool bSuccess,bool bLoading,const FString& Detail);
 inline FString Ja(const TCHAR* Text){return Ja(FString(Text));}
}
