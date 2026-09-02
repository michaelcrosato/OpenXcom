#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Content/RuleTypes.h"
#include "Strategic/StrategicCampaignState.h"

#include "TacticalNavigationService.generated.h"

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalNavigationDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FName Code;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FString Message;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalPathStep
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 X = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 Y = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 Z = 0;

	/** Action-point cost paid when entering this cell. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 MoveCost = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalPathResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 TotalCost = 0;

	/** Ordered cells entered after leaving the unit's current cell. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	TArray<FTacticalPathStep> Steps;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	TArray<FTacticalNavigationDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalReachableCell
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 CellIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 X = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 Y = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 Z = 0;

	/** Minimum action-point cost from the unit's current cell. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 TotalCost = 0;
};

/** Canonically ordered cells a unit can enter within an explicit action-point budget. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalReachabilityResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	TArray<FTacticalReachableCell> Cells;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	TArray<FTacticalNavigationDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

/** Current team visibility is derived from persisted terrain and unit state. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalVisibilityResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 Width = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 Height = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 Levels = 1;

	/** Sorted level-major row-major cell indices visible to at least one living observer. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	TArray<int32> VisibleCellIndices;

	/** Sorted visible unit identities; living observer-team units are always included. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	TArray<FGuid> VisibleUnitIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	TArray<FTacticalNavigationDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
	bool IsCellVisible(int32 X, int32 Y, int32 Z = 0) const;
	bool IsUnitVisible(FGuid UnitId) const;
};

/** Deterministic 2D projection of a parabolic tactical-device throw. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalThrowTrajectoryResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	bool bIntercepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 AimedX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 AimedY = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 AimedZ = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 LandingX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 LandingY = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 LandingZ = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 PeakHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 InterceptedObstacleHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FName InterceptedTerrainRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	TArray<FTacticalNavigationDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

/** Deterministic grid traversal and line-of-sight queries over tactical state. */
class UEGTCORE_API FTacticalNavigationService final
{
public:
	static FTacticalPathResult FindPath(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		FGuid UnitId,
		int32 DestinationX,
		int32 DestinationY,
		int32 DestinationZ = 0);

	static FTacticalReachabilityResult ComputeReachableCells(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		FGuid UnitId,
		int32 MaximumCost);

	static FTacticalVisibilityResult ComputePlayerVisibility(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules);

	/** Unions current player sight into durable, sorted battlefield discovery without consuming random draws. */
	static FTacticalVisibilityResult RefreshPlayerDiscovery(
		FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules);

	static FTacticalVisibilityResult ComputeTeamVisibility(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		ETacticalTeam ObserverTeam,
		int32 VisionRangeModifier = 0);

	/** Returns whether an unobstructed terrain trace reaches the target cell. */
	static bool HasLineOfSight(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		int32 OriginX,
		int32 OriginY,
		int32 TargetX,
		int32 TargetY,
		int32 OriginZ = 0,
		int32 TargetZ = 0);

	/** Returns deterministic smoke obscuration from 0 through 100 along the same grid trace. */
	static int32 ComputeSmokeObscuration(
		const FTacticalBattleState& Battle,
		int32 OriginX,
		int32 OriginY,
		int32 TargetX,
		int32 TargetY,
		int32 OriginZ = 0,
		int32 TargetZ = 0);

	/** Previews a fixed-point parabolic arc; an intact obstacle may deterministically become the landing cell. */
	static FTacticalThrowTrajectoryResult PreviewThrowTrajectory(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		int32 OriginX,
		int32 OriginY,
		int32 TargetX,
		int32 TargetY,
		int32 PeakHeight,
		int32 OriginZ = 0,
		int32 TargetZ = 0);
};
