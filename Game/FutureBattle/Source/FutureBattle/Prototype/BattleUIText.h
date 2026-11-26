#pragma once
#include "CoreMinimal.h"

// Display-time Japanese text for the HUD (user decision 2026-11-16: UI text is Japanese). Game state, logs and AI
// logic keep their English strings (some code tests prefixes such as "HOME SUPPLY"); only what is drawn is translated.
// Lookup order: whole string, known patterns with numbers, then each " / " segment. Unknown text is shown unchanged.
namespace BattleUIText
{
 FString Ja(const FString& Text);
 inline FString Ja(const TCHAR* Text){return Ja(FString(Text));}
}
