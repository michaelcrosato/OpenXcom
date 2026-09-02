#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"

#include "TacticalObjectiveTypes.generated.h"

/** Data-driven behavior for an original tactical mission's primary objective. */
UENUM(BlueprintType)
enum class ETacticalObjectiveType : uint8
{
	/** Player units spend action points to disable or stabilize a target. */
	Disrupt,

	/** Player completion also secures the configured item reward aboard the transport. */
	Recover,

	/** Both teams can build or erase control progress; adversary completion fails the objective. */
	Control
};
