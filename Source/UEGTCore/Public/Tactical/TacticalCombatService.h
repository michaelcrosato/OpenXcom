#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Content/RuleTypes.h"
#include "Strategic/StrategicCampaignState.h"

#include "TacticalCombatService.generated.h"

USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalCombatDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FName Code;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FString Message;
};

/** Deterministic attack facts that presentation may show before committing a command. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalAttackPreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FGuid AttackerUnitId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FGuid TargetUnitId;

	/** Player weapon item id or adversary unit-rule id. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FName AttackRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 TargetX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 TargetY = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 TargetZ = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 Distance = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 MaximumRange = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 ActionPointCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 AttackPower = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	ETacticalDamageType DamageType = ETacticalDamageType::Kinetic;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 AmmunitionCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	ETacticalFireMode FireMode = ETacticalFireMode::Single;

	/** Independent projectiles resolved by this action. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 ProjectileCount = 1;

	/** Zero for direct-fire weapons; positive profiles must target a ground cell. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 BlastRadius = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 ScatterRadius = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 BlastFalloffPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 TerrainDamagePercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 BlastSmoke = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 BlastFire = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 BlastSuppression = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 CoverPercent = 0;

	/** Accuracy gained by a crouched attacker. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 StanceAccuracyModifier = 0;

	/** Additional protection granted by a crouched target's smaller profile. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 StanceCoverModifier = 0;

	/** Accuracy adjustment from firing across tactical levels. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 ElevationAccuracyModifier = 0;

	/** Accuracy penalty from smoke traced through the attacker and target cells. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 SmokePenalty = 0;

	/** Unit-target chance in percent; destructible-terrain attacks are guaranteed. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 HitChance = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	TArray<FTacticalCombatDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

/** Resolve-versus-resolve pressure projection that never deals physical damage. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FTacticalSignalPreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FGuid AttackerUnitId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FGuid TargetUnitId;

	/** Player projector item id or adversary unit-rule id. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FName SignalRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 Distance = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 MaximumRange = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 ActionPointCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 SignalPower = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 SmokePenalty = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 HitChance = 0;

	/** Exact pressure applied on success after clamping to the target's current state. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 SuppressionGain = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 MoraleDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	TArray<FTacticalCombatDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
};

/** Pure attack preview and integer damage rules over persisted tactical state. */
class UEGTCORE_API FTacticalCombatService final
{
public:
	static FTacticalAttackPreview PreviewUnitAttack(
		const FTacticalBattleState& Battle,
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		FGuid AttackerUnitId,
		FGuid TargetUnitId,
		FName WeaponItemId,
		ETacticalFireMode FireMode = ETacticalFireMode::Single);

	static FTacticalAttackPreview PreviewTerrainAttack(
		const FTacticalBattleState& Battle,
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		FGuid AttackerUnitId,
		int32 TargetX,
		int32 TargetY,
		FName WeaponItemId,
		ETacticalFireMode FireMode = ETacticalFireMode::Single,
		int32 TargetZ = 0);

	/** ProjectorItemId is required for players and must be unset for intrinsic adversary projections. */
	static FTacticalSignalPreview PreviewSignalProjection(
		const FTacticalBattleState& Battle,
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		FGuid AttackerUnitId,
		FGuid TargetUnitId,
		FName ProjectorItemId = NAME_None);

	/** VariancePercent is an inclusive deterministic roll from 80 through 120. */
	static int32 ComputeUnitDamage(int32 AttackPower, int32 AttackerStrength, int32 DefenderStrength, int32 DefenderArmor, int32 VariancePercent);
	static int32 ComputeTerrainDamage(int32 AttackPower, int32 AttackerStrength, int32 VariancePercent);

	/** Linear integer falloff; returns 100 at impact and zero once falloff consumes the effect. */
	static int32 ComputeBlastEffectPercent(int32 Distance, int32 FalloffPercentPerCell);

	/** Multiplicative transmission through intact terrain between impact and a target cell. */
	static int32 ComputeBlastTransmissionPercent(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		int32 ImpactX,
		int32 ImpactY,
		int32 TargetX,
		int32 TargetY,
		int32 ImpactZ = 0,
		int32 TargetZ = 0);
};
