#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Content/RuleTypes.h"
#include "Strategic/PersonnelLegacyRelay.h"
#include "Strategic/PersonnelMentorship.h"
#include "Strategic/PersonnelSquadBond.h"
#include "Strategic/PersonnelServiceHistory.h"
#include "Strategic/StrategicCampaignState.h"
#include "Strategic/StrategicCommandService.h"
#include "Tactical/TacticalCombatService.h"
#include "Tactical/TacticalNavigationService.h"

#include "TacticalPresentationService.generated.h"

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalPresentationDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName Code;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FString Message;
};

/** A tactical action that the HUD can offer for the current selection and pointer target. */
UENUM(BlueprintType)
enum class ETacticalHudActionType : uint8
{
	ConfirmDeployment,
	Move,
	AttackUnit,
	ProjectSignal,
	AttackTerrain,
	Reload,
	EjectMagazine,
	ChangeStance,
	OperateDoor,
	DeployDevice,
	InteractObjective,
	Extract,
	EndTurn
};

/** Pointer and equipment selection supplied by presentation without becoming campaign state. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalHudQuery
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical|Presentation")
	FGuid SelectedUnitId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical|Presentation")
	int32 ViewedLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical|Presentation")
	bool bHasHoveredCell = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical|Presentation")
	int32 HoveredX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical|Presentation")
	int32 HoveredY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical|Presentation")
	int32 HoveredZ = 0;

	/** Optional unit under the pointer. Hidden unit ids are intentionally treated as no target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical|Presentation")
	FGuid HoveredUnitId;

	/** Optional objective under the pointer. An adjacent active objective is selected automatically when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical|Presentation")
	FName HoveredObjectiveId;

	/** An unset weapon or device selects the first valid carried option in canonical rule-id order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical|Presentation")
	FName SelectedWeaponItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical|Presentation")
	ETacticalFireMode FireMode = ETacticalFireMode::Single;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical|Presentation")
	FName SelectedDeviceItemId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalHudItemView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName Category;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 UnitMass = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 UnitSellValue = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalHudWeaponView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Range = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 SingleActionPointCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bSupportsBurst = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 BurstActionPointCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 LoadedAmmunition = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 MagazineCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName AmmunitionItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 ReserveMagazines = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 FullReserveMagazines = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 PartialReserveMagazines = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 ReserveAmmunition = 0;

	/** Rounds the deterministic reload policy would place in this weapon, or zero when unavailable. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 NextReloadAmmunition = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 ReloadActionPointCost = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalHudUnitView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FGuid UnitId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FGuid PersonnelId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName SourceRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	ETacticalTeam Team = ETacticalTeam::Player;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	ETacticalStance Stance = ETacticalStance::Standing;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 X = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Y = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Z = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 CurrentHealth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 MaxHealth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 RemainingActionPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 MaxActionPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 CurrentMorale = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 MaxMorale = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Suppression = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bSelected = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bControllable = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bIncapacitated = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bExtracted = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	TArray<FTacticalHudWeaponView> Weapons;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	TArray<FTacticalHudItemView> CarriedItems;
};

/** A known cell on the requested tactical level. Historical entries expose coordinates only. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalHudCellView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 CellIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 X = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Y = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Z = 0;

	/** False for durable map memory; all dynamic terrain fields then remain deliberately empty. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bCurrentlyVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName TerrainRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FString TerrainDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 CurrentIntegrity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 MaxIntegrity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 MoveCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 CoverPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Smoke = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Fire = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bBlocksMovement = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bBlocksVision = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bPlayerDeployment = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bExtraction = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bIsDoor = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bDoorOpen = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bIsVerticalConnector = false;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalHudObjectiveView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName ObjectiveId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	ETacticalObjectiveType Type = ETacticalObjectiveType::Disrupt;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	ETacticalObjectiveStatus Status = ETacticalObjectiveStatus::Active;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 X = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Y = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Z = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 PlayerInteractions = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 AdversaryInteractions = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 RequiredInteractions = 1;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName RewardItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FString RewardDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 RewardQuantity = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalHudActionAvailability
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	ETacticalHudActionType ActionType = ETacticalHudActionType::Move;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 ActionPointCost = 0;

	/** Empty when available; otherwise a stable machine-readable reason. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FString UnavailableReason;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FGuid UnitId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FGuid TargetUnitId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName ObjectiveId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 TargetX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 TargetY = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 TargetZ = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	ETacticalFireMode FireMode = ETacticalFireMode::Single;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	ETacticalStance RequestedStance = ETacticalStance::Standing;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bRequestedDoorOpen = false;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalHudHoverPreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bHasCell = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bCellVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 X = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Y = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Z = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bHasPathPreview = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FTacticalPathResult Path;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bHasUnitAttackPreview = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FTacticalAttackPreview UnitAttack;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bHasSignalPreview = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FTacticalSignalPreview Signal;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bHasTerrainAttackPreview = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FTacticalAttackPreview TerrainAttack;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bHasDeviceTrajectory = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FTacticalThrowTrajectoryResult DeviceTrajectory;
};

/** Fog-safe tactical read model consumed by HUD widgets, input adapters, replays, and accessibility layers. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalHudSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FGuid BattleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FGuid OperationId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	ETacticalOperationType OperationType = ETacticalOperationType::SiteRecovery;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FGuid AssaultId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FString BaseDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FPersonnelMentorshipView Mentorship;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FPersonnelLegacyRelayView LegacyRelay;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FPersonnelSquadBondView SquadBonds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int64 ExpectedCommandSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName MissionRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FString MissionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	bool bRequiresExtraction = true;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Width = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Height = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 Levels = 1;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 ViewedLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 TurnNumber = 1;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 TurnLimit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	ETacticalBattlePhase Phase = ETacticalBattlePhase::Deployment;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	ETacticalTeam ActiveTeam = ETacticalTeam::Player;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	ETacticalWindDirection WindDirection = ETacticalWindDirection::Calm;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 WindStrength = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 VisibleCellCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 KnownCellCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 LivingPlayerUnitCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 VisibleAdversaryUnitCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int64 CargoMass = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	int32 CargoCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName EffectiveWeaponItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName EffectiveDeviceItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FName EffectiveSignalProjectorItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	TArray<FTacticalHudCellView> VisibleCells;

	/** Current cells plus coordinate-only historical memory, filtered to ViewedLevel. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	TArray<FTacticalHudCellView> KnownCells;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	TArray<FTacticalHudUnitView> Units;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	TArray<FTacticalHudObjectiveView> Objectives;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	TArray<FTacticalHudItemView> Cargo;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	FTacticalHudHoverPreview Hover;

	/** Exactly one entry for every ETacticalHudActionType, in enum order. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	TArray<FTacticalHudActionAvailability> Actions;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Presentation")
	TArray<FTacticalPresentationDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
	const FTacticalHudActionAvailability* FindAction(ETacticalHudActionType ActionType) const;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalDebriefPersonnelView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	FGuid PersonnelId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	int32 StartingHealth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	int32 EndingHealth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	int32 DamageTaken = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	bool bInjured = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	bool bKilled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	int32 ExperienceGained = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	int32 TotalExperience = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	int32 PreviousRank = 1;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	int32 NewRank = 1;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	bool bPromoted = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	int32 Missions = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	FPersonnelServiceHistoryView ServiceHistory;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	EPersonnelServiceBand PreviousServiceBand = EPersonnelServiceBand::FirstWatch;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	bool bServiceBandAdvanced = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	int64 RecoverySeconds = 0;

	/** Stable rule ids for citations awarded by this resolved operation, in deterministic event order. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	TArray<FName> AwardedCommendationIds;
};

/** Stable post-command summary retained by the game instance while raw tactical state is removed. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalDebriefView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	bool bMissionSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	FGuid OperationId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	FGuid BattleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	ETacticalOperationType OperationType = ETacticalOperationType::SiteRecovery;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	FGuid SiteId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	FGuid CraftId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	FGuid AssaultId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	FName MissionRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	FString MissionDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	FString CraftDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	FString BaseDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	int64 ScoreAwarded = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	int64 CampaignScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	TArray<FTacticalHudItemView> RecoveredCargo;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	TArray<FTacticalDebriefPersonnelView> Personnel;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Debrief")
	TArray<FTacticalPresentationDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

/** Pure adapters from authoritative tactical/campaign state to player-safe presentation data. */
class UEGTCORE_API FTacticalPresentationService final
{
public:
	static FTacticalHudSnapshot BuildHudSnapshot(
		const FTacticalBattleState& Battle,
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		const FTacticalHudQuery& Query);

	static FTacticalDebriefView BuildDebrief(
		const FCampaignState& Before,
		const FCampaignState& After,
		const FResolvedRuleSet& Rules,
		const FStrategicCommandResult& Resolution,
		FGuid OperationId);
};
