#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "UEGTGameMode.generated.h"

/** Asset-independent runtime shell; tactical presentation activates whenever campaign battle state exists. */
UCLASS(BlueprintType)
class UEGTGAME_API AUEGTGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	AUEGTGameMode();
};
