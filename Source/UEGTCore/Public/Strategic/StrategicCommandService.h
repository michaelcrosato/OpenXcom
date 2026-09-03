#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Content/RuleTypes.h"
#include "Strategic/MutualAidRelayQueue.h"
#include "Strategic/StrategicCampaignState.h"

#include "StrategicCommandService.generated.h"

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicSimulationConfig
{
	GENERATED_BODY()

	/** Maximum supported base-grid dimension; placement read models enumerate anchor cells. */
	static constexpr int32 MaximumBaseGridDimension = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 BaseEstablishmentCost = 500000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 DefaultScientistCapacity = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 DefaultEngineerCapacity = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName ManufacturingFacilityId = TEXT("facility.fabrication-bay");

	/** Operational facility that must remain connected to every base structure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName OperationsFacilityId = TEXT("facility.operations-hub");

	/** Percentage of original build cost recovered when an operational facility is dismantled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic", meta = (ClampMin = "0", ClampMax = "100"))
	int32 FacilityDismantleRefundPercent = 25;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic", meta = (ClampMin = "1", ClampMax = "64"))
	int32 BaseGridWidth = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic", meta = (ClampMin = "1", ClampMax = "64"))
	int32 BaseGridHeight = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 RecoveryHoursPerHealth = 6;

	/** Surge Care reserve committed per missing health point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Personnel", meta = (ClampMin = "1"))
	int64 RecoverySurgeCostPerMissingHealth = 2000;

	/** Percent of baseline recovery time used by Surge Care. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Personnel", meta = (ClampMin = "1", ClampMax = "100"))
	int32 RecoverySurgeDurationPercent = 50;

	/** Percent of baseline recovery time used by a Reflection Cycle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Personnel", meta = (ClampMin = "100", ClampMax = "1000"))
	int32 RecoveryReflectionDurationPercent = 150;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Personnel", meta = (ClampMin = "1", ClampMax = "100"))
	int32 RecoveryReflectionResolveBonus = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 TrainingHours = 120;

	/** Fixed duration of one veteran base-leadership commitment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Personnel", meta = (ClampMin = "1", ClampMax = "365"))
	int32 StewardshipDurationDays = 30;

	/** Service-history gate for beginning a Stewardship Rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Personnel", meta = (ClampMin = "1", ClampMax = "10000"))
	int32 StewardshipMinimumMissions = 10;

	/** Percentage removed from the selected care, training, or recruitment value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Personnel", meta = (ClampMin = "1", ClampMax = "99"))
	int32 StewardshipReductionPercent = 25;

	/** Resolve earned when a qualifying rotation completes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Personnel", meta = (ClampMin = "1", ClampMax = "100"))
	int32 StewardshipResolveBonus = 1;

	/** Completed tours eligible for the bounded Resolve reward. Later tours remain historical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Personnel", meta = (ClampMin = "1", ClampMax = "100"))
	int32 StewardshipResolveAwardTourCap = 3;

	/** Fixed deterministic transit duration for player-directed inter-base aid shipments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Logistics", meta = (ClampMin = "1", ClampMax = "8760"))
	int32 MutualAidConvoyTransitHours = 72;

	/** Faster Rapid Thread duration; remains long enough to absorb the configured delay at midpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Logistics", meta = (ClampMin = "1", ClampMax = "8760"))
	int32 MutualAidRapidThreadTransitHours = 48;

	/** Slower Veiled Chain duration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Logistics", meta = (ClampMin = "1", ClampMax = "8760"))
	int32 MutualAidVeiledChainTransitHours = 96;

	/** Exposure added by Rapid Thread before the result is clamped to [0, 100]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Logistics", meta = (ClampMin = "0", ClampMax = "100"))
	int32 MutualAidRapidThreadExposure = 25;

	/** Exposure removed by Veiled Chain before the result is clamped to [0, 100]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Logistics", meta = (ClampMin = "0", ClampMax = "100"))
	int32 MutualAidVeiledChainExposureReduction = 25;

	/** Route pressure at or above this threshold forecasts one midpoint interdiction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Logistics", meta = (ClampMin = "1", ClampMax = "100"))
	int32 MutualAidInterdictionThreshold = 60;

	/** Delay imposed by an unescorted forecast interdiction. Cargo remains lossless. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Logistics", meta = (ClampMin = "1", ClampMax = "8760"))
	int32 MutualAidInterdictionDelayHours = 24;

	/** Fixed funds committed at dispatch for a civilian Signal Escort. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Logistics", meta = (ClampMin = "0"))
	int64 MutualAidSignalEscortCost = 25000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 MaxGeneralPersonnelPerBase = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 InterceptionRoundSeconds = 5;

	/** Emergency-grid funds committed per assault threat rating when Grid Overcharge is selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Base Defense", meta = (ClampMin = "1"))
	int64 BaseDefenseGridOverchargeCostPerThreat = 25000;

	/** Flat accuracy added to every supplied Grid Overcharge shot, capped at 100. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Base Defense", meta = (ClampMin = "0", ClampMax = "100"))
	int32 BaseDefenseGridOverchargeAccuracyBonus = 15;

	/** Damage percentage applied to every supplied Grid Overcharge shot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Base Defense", meta = (ClampMin = "100", ClampMax = "400"))
	int32 BaseDefenseGridOverchargeDamagePercent = 125;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 WreckageSiteLifetimeHours = 72;

	/** Deterministic next-wave delay added per threat point after a successful interception. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic", meta = (ClampMin = "0", ClampMax = "360"))
	int32 InterceptionAftershockMinutesPerThreat = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 StartingAdversaryDelayHours = 24;

	/** Mission-gap percentage relative to authored intervals for Cadet campaigns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Difficulty", meta = (ClampMin = "25", ClampMax = "400"))
	int32 CadetAdversaryIntervalPercent = 125;

	/** Mission-gap percentage relative to authored intervals for Standard campaigns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Difficulty", meta = (ClampMin = "25", ClampMax = "400"))
	int32 StandardAdversaryIntervalPercent = 100;

	/** Mission-gap percentage relative to authored intervals for Veteran campaigns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Difficulty", meta = (ClampMin = "25", ClampMax = "400"))
	int32 VeteranAdversaryIntervalPercent = 85;

	/** Mission-gap percentage relative to authored intervals for Apex campaigns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Difficulty", meta = (ClampMin = "25", ClampMax = "400"))
	int32 ApexAdversaryIntervalPercent = 70;

	/** Escape-consequence percentage relative to authored penalties for Cadet campaigns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Difficulty", meta = (ClampMin = "25", ClampMax = "400"))
	int32 CadetAdversaryConsequencePercent = 75;

	/** Escape-consequence percentage relative to authored penalties for Standard campaigns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Difficulty", meta = (ClampMin = "25", ClampMax = "400"))
	int32 StandardAdversaryConsequencePercent = 100;

	/** Escape-consequence percentage relative to authored penalties for Veteran campaigns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Difficulty", meta = (ClampMin = "25", ClampMax = "400"))
	int32 VeteranAdversaryConsequencePercent = 125;

	/** Escape-consequence percentage relative to authored penalties for Apex campaigns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Difficulty", meta = (ClampMin = "25", ClampMax = "400"))
	int32 ApexAdversaryConsequencePercent = 150;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 MaxActiveAdversaryMissions = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 FailurePressureThreshold = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 VictoryThwartedMissions = 12;

	/** Resolved operations required for the adversary's deterministic adaptation floor to rise one level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 ResolvedMissionsPerEscalationLevel = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 MaxAdversaryEscalation = 10;

	/** Late-game escalation that must be reached in addition to the thwarted-mission victory target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic", meta = (ClampMin = "1", ClampMax = "10"))
	int32 VictoryMinimumEscalationLevel = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "0"))
	int64 CivicReliefCost = 120000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 CivicReliefSupportGain = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 CivicReliefPressureReduction = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "0"))
	int64 SecurityAccordCost = 180000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 SecurityAccordSupportGain = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 SecurityAccordPressureReduction = 12;

	/** Minimum regional pressure required before support can be committed to emergency mobilization. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "99"))
	int32 CrisisMobilizationMinimumPressure = 60;

	/** Regional support spent by emergency mobilization instead of campaign funds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 CrisisMobilizationSupportCost = 15;

	/** Severe regional pressure removed by emergency mobilization. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 CrisisMobilizationPressureReduction = 25;

	/** Minimum partner support required before a durable Resilience Charter can be signed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 ResilienceCharterMinimumSupport = 60;

	/** One-time campaign-funds commitment required to establish local mutual-aid infrastructure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "0"))
	int64 ResilienceCharterCost = 250000;

	/** Partner support committed permanently when the charter is signed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 ResilienceCharterSupportCost = 10;

	/** Percentage of the support-tier contribution retained after local resilience investment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "25", ClampMax = "100"))
	int32 ResilienceCharterFundingPercent = 90;

	/** Effective selection weight for weighted missions targeting a charter partner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "25", ClampMax = "100"))
	int32 ResilienceCharterMissionWeightPercent = 50;

	/** Regional pressure percentage retained when a mission escapes in a charter partner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "25", ClampMax = "100"))
	int32 ResilienceCharterEscapePressurePercent = 75;

	/** Signed regional charters required before the multi-partner Horizon Compact can be ratified. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "2", ClampMax = "100"))
	int32 HorizonCompactRequiredCharters = 2;

	/** Minimum current support required from every charter partner joining the compact at ratification. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 HorizonCompactMinimumMemberSupport = 50;

	/** One-time campaign-funds commitment for the shared coalition network. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "0"))
	int64 HorizonCompactCost = 400000;

	/** Support committed by each currently signed charter partner when the compact is ratified. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 HorizonCompactMemberSupportCost = 5;

	/** Charter-member contribution retained after coalition procurement replaces isolated investment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "25", ClampMax = "100"))
	int32 HorizonCompactFundingPercent = 95;

	/** Share of charter-retained escape pressure redirected to the least-strained other member. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "50"))
	int32 HorizonCompactSharedEscapePressurePercent = 33;

	/** A compact member withdraws when a support loss leaves it below this threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "99"))
	int32 HorizonCompactWithdrawalSupportThreshold = 25;

	/** Support required before a withdrawn member can be restored to the compact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "2", ClampMax = "100"))
	int32 HorizonCompactRestorationMinimumSupport = 40;

	/** Campaign-funds commitment required to restore a withdrawn compact member. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "0"))
	int64 HorizonCompactRestorationCost = 100000;

	/** Campaign funds committed when the compact coordinates one monthly Reciprocal Aid deployment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "0"))
	int64 ReciprocalAidCost = 150000;

	/** Minimum pressure at the member requesting Reciprocal Aid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "99"))
	int32 ReciprocalAidMinimumTargetPressure = 60;

	/** Maximum pressure transferred to the least-strained other compact member. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 ReciprocalAidPressureTransfer = 20;

	/** Support gained by the aided member and committed by the member accepting its pressure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 ReciprocalAidSupportTransfer = 5;

	/** Campaign funds committed when a withdrawn member requests one monthly emergency solidarity vote. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "0"))
	int64 HorizonCompactEmergencyVoteCost = 200000;

	/** Maximum support rebuilt by a passed emergency solidarity motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 HorizonCompactEmergencyTargetSupportGain = 12;

	/** Maximum pressure removed from the withdrawn target by a passed motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 HorizonCompactEmergencyTargetPressureReduction = 15;

	/** Support committed by every active compact member voting for the motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "1", ClampMax = "100"))
	int32 HorizonCompactEmergencyVoterSupportCost = 2;

	/** Maximum current pressure at which an active member will vote for an emergency motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic|Diplomacy", meta = (ClampMin = "0", ClampMax = "99"))
	int32 HorizonCompactEmergencyMaximumVoterPressure = 70;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 TacticalSiteScorePerThreat = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic", meta = (ClampMin = "1"))
	int32 PersonnelExperiencePerRank = 250;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic", meta = (ClampMin = "1", ClampMax = "100"))
	int32 MaxPersonnelRank = 10;
};

/** Player-facing and simulation-facing difficulty multipliers for adversary operations. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FAdversaryDifficultyTuning
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Difficulty")
	int32 MissionIntervalPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Difficulty")
	int32 EscapeConsequencePercent = 0;
};

/** Deterministic campaign adaptation projection derived only from persisted mission counters. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FAdversaryAdaptationProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Adaptation")
	int64 ResolvedMissions = 0;

	/** Minimum escalation implied by the resolved-mission history. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Adaptation")
	int32 EscalationFloor = 1;

	/** Zero when maximum escalation has already been reached. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Adaptation")
	int64 ResolvedMissionsUntilNextEscalation = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Adaptation")
	bool bAtMaximumEscalation = false;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FEstablishBaseCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName RegionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 LongitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 LatitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	TArray<FName> StartingFacilities;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStartResearchCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName ResearchId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FSetResearchStaffCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName ResearchId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 AssignedScientists = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCancelResearchCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName ResearchId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FAdvanceStrategicTimeCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	EStrategicTimeRate Rate = EStrategicTimeRate::Paused;
};

UENUM(BlueprintType)
enum class ERegionalDiplomacyActionType : uint8
{
	CivicRelief,
	SecurityAccord,
	CrisisMobilization
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FRegionalDiplomacyCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName RegionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	ERegionalDiplomacyActionType ActionType = ERegionalDiplomacyActionType::CivicRelief;
};

/** Establishes the original durable mutual-aid compact for one regional mandate partner. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FSignRegionalCharterCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName RegionId;
};

/** Ratifies the original multi-partner Horizon Compact once enough regional charters are active. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FRatifyHorizonCompactCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;
};

/** Moves crisis pressure from one compact member to its least-strained partner once per campaign month. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FDeployReciprocalAidCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	/** Compact member receiving relief from severe pressure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName TargetRegionId;
};

/** Restores a low-support charter partner that previously withdrew from the Horizon Compact. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FRestoreHorizonCompactMemberCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName RegionId;
};

/** Calls a deterministic majority vote to stabilize one withdrawn Horizon Compact member. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FCallHorizonCompactEmergencyVoteCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName TargetRegionId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStartManufacturingCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ProjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 Units = 1;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStartFacilityConstructionCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ProjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid FacilityInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName FacilityId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 GridX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 GridY = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCancelFacilityConstructionCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ProjectId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FDismantleFacilityCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid FacilityInstanceId;
};

/** Simulation-facing damage mutation used by future base-attack resolution. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FApplyFacilityDamageCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid FacilityInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic", meta = (ClampMin = "1"))
	int32 Damage = 1;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStartFacilityRepairCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid FacilityInstanceId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCancelFacilityRepairCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid FacilityInstanceId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FSetManufacturingStaffCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ProjectId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 AssignedEngineers = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FAdjustManufacturingUnitsCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ProjectId;

	/** Positive reserves additional units; negative removes untouched tail units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 DeltaUnits = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCancelManufacturingCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ProjectId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FSellInventoryCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 Quantity = 1;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FDispatchMutualAidConvoyCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid SourceBaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid DestinationBaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	EMutualAidRoutePolicy RoutePolicy = EMutualAidRoutePolicy::OpenRelay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	bool bSignalEscort = false;
};

/** Changes the route doctrine of one held, never-progressed convoy before relay departure. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FRetuneMutualAidConvoyCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ConvoyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	EMutualAidRoutePolicy RoutePolicy = EMutualAidRoutePolicy::OpenRelay;
};

/** Commissions a Signal Escort for one held, never-progressed high-risk convoy. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FCommissionMutualAidSignalEscortCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ConvoyId;
};

/** Moves one held, never-progressed convoy to the front of its held Relay Weave line. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FPrioritizeMutualAidConvoyCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ConvoyId;
};

/** Withdraws one held, never-progressed convoy and restores its cargo to the source base. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FStandDownMutualAidConvoyCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ConvoyId;
};

/** Redirects one held, never-progressed convoy to another established destination. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FDivertMutualAidConvoyCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ConvoyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid DestinationBaseId;
};

/**
 * Configures an optional two-leg Relay Waypoint for one held, never-progressed convoy.
 * An invalid waypoint requests restoration of the direct route.
 */
USTRUCT(BlueprintType)
struct UEGTCORE_API FConfigureMutualAidRelayWaypointCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ConvoyId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid WaypointBaseId;

	/** Ignored for a direct-route request; otherwise committed to the onward leg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	EMutualAidRoutePolicy OnwardRoutePolicy = EMutualAidRoutePolicy::OpenRelay;
};

/** Toggles an even cargo split between a pending waypoint and the final destination. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FConfigureMutualAidBalancedHandoffCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ConvoyId;

	/** When enabled, floor(total / 2) units go to the waypoint and the rest continue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	bool bEnabled = false;
};

/** Reassigns base scientist capacity between research and the original Signal Watch policy. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FSetSignalWatchStaffCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 AssignedScientists = 0;
};

/** Reassigns base engineer capacity between production and the original Works Cadre policy. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FSetWorksCadreStaffCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 AssignedEngineers = 0;
};

/** Selects the construction-versus-repair emphasis for future Works Cadre commitments. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FSetWorksCadreCharterCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	EWorksCadreCharter Charter = EWorksCadreCharter::CommonCadence;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FRecruitPersonnelCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid OrderId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid PersonnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName RoleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FString DisplayName;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTransferPersonnelCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid PersonnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid DestinationBaseId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FDismissPersonnelCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid PersonnelId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FApplyPersonnelDamageCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid PersonnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 Damage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName CauseId = TEXT("cause.field-injury");
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FSelectPersonnelRecoveryPlanCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid PersonnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	EPersonnelRecoveryPlan Plan = EPersonnelRecoveryPlan::MeasuredReturn;
};

/** Commits one experienced field agent to a fixed-term base leadership role. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FBeginPersonnelStewardshipCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid PersonnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	EPersonnelStewardshipFocus Focus = EPersonnelStewardshipFocus::RecoveryAdvocacy;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FBeginPersonnelTrainingCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid PersonnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	EPersonnelTrainingFocus Focus = EPersonnelTrainingFocus::Accuracy;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FSelectPersonnelDoctrineCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid PersonnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName DoctrineId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FSetPersonnelEquipmentCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid PersonnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	TArray<FName> ItemIds;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FAcquireCraftCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid OrderId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName CraftRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FString DisplayName;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FTransferCraftCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid DestinationBaseId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FAssignCraftPilotCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;

	/** Invalid clears the current assignment while the craft is grounded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid PersonnelId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FSetCraftEquipmentCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	TArray<FName> ItemIds;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FBeginCraftServiceCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;
};

/** Cancels every unfinished component of an active craft service and refunds its unapplied reservation. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FCancelCraftServiceCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;
};

/** Determines whether a rearm command requires a complete load or accepts a deterministic partial load. */
UENUM(BlueprintType)
enum class ECraftRearmPolicy : uint8
{
	FullLoad,
	LoadAvailable
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FRearmCraftCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;

	/** FullLoad preserves all-or-nothing rearming; LoadAvailable consumes every usable round in stable weapon order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	ECraftRearmPolicy Policy = ECraftRearmPolicy::FullLoad;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FLaunchCraftCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;

	/** Fuel reserved and consumed for this abstract sortie. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 FuelUnits = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FRecoverCraftCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FSetCraftAgentsCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	TArray<FGuid> PersonnelIds;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FSetCraftCargoCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	TArray<FInventoryStack> Cargo;
};

UENUM(BlueprintType)
enum class ECraftSalvageDisposition : uint8
{
	RetainAtBase,
	Sell
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FResolveCraftSalvageCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 Quantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	ECraftSalvageDisposition Disposition = ECraftSalvageDisposition::RetainAtBase;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FDeployCraftToSiteCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid SiteId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FResolveTacticalOperationCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid OperationId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	bool bObjectiveCompleted = false;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FGenerateTacticalBattleCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid OperationId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FConfirmTacticalDeploymentCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid BattleId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FMoveTacticalUnitCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid BattleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid UnitId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int32 DestinationX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int32 DestinationY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int32 DestinationZ = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FChangeTacticalStanceCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid BattleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid UnitId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	ETacticalStance Stance = ETacticalStance::Standing;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FSetTacticalDoorCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid BattleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid UnitId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int32 TargetX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int32 TargetY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int32 TargetZ = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	bool bOpen = true;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FAttackTacticalUnitCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid BattleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid AttackerUnitId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid TargetUnitId;

	/** Required equipped item for player attacks; leave unset for adversaries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FName WeaponItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	ETacticalFireMode FireMode = ETacticalFireMode::Single;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FAttackTacticalTerrainCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid BattleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid AttackerUnitId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int32 TargetX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int32 TargetY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int32 TargetZ = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FName WeaponItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	ETacticalFireMode FireMode = ETacticalFireMode::Single;
};

/** Apply a non-damaging resolve-pressure projection to one visible hostile. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FProjectTacticalSignalCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid BattleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid AttackerUnitId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid TargetUnitId;

	/** Required carried projector item for player units; unset for intrinsic adversary projections. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FName ProjectorItemId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FReloadTacticalWeaponCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid BattleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid UnitId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FName WeaponItemId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FEjectTacticalMagazineCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid BattleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid UnitId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FName WeaponItemId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FDeployTacticalDeviceCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid BattleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid UnitId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FName DeviceItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int32 TargetX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int32 TargetY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int32 TargetZ = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FInteractTacticalObjectiveCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid BattleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid UnitId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FName ObjectiveId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FExtractTacticalUnitCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid BattleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid UnitId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FEndTacticalTurnCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical")
	FGuid BattleId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FRunTacticalAiTurnCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical|AI")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Tactical|AI")
	FGuid BattleId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FCreateStrategicContactCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ContactId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FName ContactRuleId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 OriginLongitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 OriginLatitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 DestinationLongitudeMilliDegrees = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int32 DestinationLatitudeMilliDegrees = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FDispatchCraftCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid CraftId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ContactId;
};

/** Player-selected pursuit geometry for one deterministic interception combat round. */
UENUM(BlueprintType)
enum class EInterceptionPosture : uint8
{
	StandOffScreen,
	BalancedVector,
	CloseAssault
};

/** Authoritative accuracy tradeoff attached to an interception posture. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FInterceptionPosturePolicy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	EInterceptionPosture Posture = EInterceptionPosture::BalancedVector;

	/** Stable identity retained in the round event and presentation boundary. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 OutgoingAccuracyModifier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 IncomingAccuracyModifier = 0;
};

/** Mutation-free formation benefit derived from the craft currently holding an interception. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FInterceptionCoordinationPolicy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	bool bActive = false;

	/** Stable solo or linked-wing identity retained by interception combat events. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 OnStationCraftCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 SupportingCraftCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 OutgoingAccuracyModifier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 IncomingAccuracyModifier = 0;
};

/** Deterministic adversary response selected from pursuit history and current contact integrity. */
UENUM(BlueprintType)
enum class EInterceptionContactManeuver : uint8
{
	VectorSurvey,
	SignalShear,
	BreaklineCounter
};

/** Mutation-free contact maneuver applied to one interception round. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FInterceptionContactManeuverPolicy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	EInterceptionContactManeuver Maneuver = EInterceptionContactManeuver::VectorSurvey;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 CompletedCombatRounds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 CurrentHull = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 MaximumHull = 0;

	/** Modifier to player craft fire against the maneuvering contact. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 OutgoingAccuracyModifier = 0;

	/** Modifier to the maneuvering contact's return fire. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 IncomingAccuracyModifier = 0;
};

/** Player-selected withdrawal doctrine for an engaged interception formation. */
UENUM(BlueprintType)
enum class EInterceptionWithdrawalDoctrine : uint8
{
	/** Return every on-station craft atomically and restore the contact to detected state. */
	FormationBreak,
	/** Return only the lowest-integrity craft so the remaining formation can hold the engagement. */
	EvasiveRelay,
	/** Return the formation after using accumulated pursuit data to force the contact off-course. */
	WakeSnare
};

/** Stable command policy attached to an interception withdrawal doctrine. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FInterceptionWithdrawalPolicy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	EInterceptionWithdrawalDoctrine Doctrine = EInterceptionWithdrawalDoctrine::FormationBreak;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	bool bWithdrawEntireFormation = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	bool bDelaysContactRoute = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 RequiredCombatRounds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int64 MaximumContactRouteDelaySeconds = 0;
};

/** Mutation-free projection shared by withdrawal commands and the strategic dashboard. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FInterceptionWithdrawalEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	bool bCanExecute = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	EInterceptionWithdrawalDoctrine Doctrine = EInterceptionWithdrawalDoctrine::FormationBreak;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 OnStationCraftCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 WithdrawingCraftCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 RemainingCraftCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 CompletedCombatRounds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 RequiredCombatRounds = 0;

	/** Exact contact route progress removed by Wake Snare; zero for other doctrines. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int64 ContactRouteDelaySeconds = 0;

	/** Lowest-integrity craft selected by Evasive Relay; invalid for full-formation doctrines. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	FGuid PriorityCraftId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 PriorityCraftCurrentHull = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	int32 PriorityCraftMaximumHull = 0;

	/** Canonically ordered craft IDs that will enter their reserved return routes. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	TArray<FGuid> WithdrawingCraftIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	FName UnavailableReasonCode;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Interception")
	FString UnavailableReason;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FResolveInterceptionRoundCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ContactId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	EInterceptionPosture Posture = EInterceptionPosture::BalancedVector;
};

/** Orders an engaged interception formation to execute one deterministic withdrawal doctrine. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FWithdrawInterceptionCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid ContactId;

	/** Defaults to the pre-existing all-craft withdrawal behavior for API and replay compatibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	EInterceptionWithdrawalDoctrine Doctrine = EInterceptionWithdrawalDoctrine::FormationBreak;
};

/** Player-selected priority used to allocate scarce supply and order one automatic-defense volley. */
UENUM(BlueprintType)
enum class EBaseDefenseFireDoctrine : uint8
{
	/** Compatibility policy: batteries are considered in stable facility-instance identity order. */
	CoordinatedLine,
	/** Higher-accuracy batteries are considered first, then higher damage and stable identity. */
	PrecisionScreen,
	/** Higher-damage batteries are considered first, then higher accuracy and stable identity. */
	BreachBreaker,
	/** Higher-damage ordering plus a threat-priced emergency accuracy and damage boost. */
	GridOvercharge
};

/** Stable authoritative identity attached to a supported base-defense fire doctrine. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FBaseDefenseFireDoctrinePolicy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Base Defense")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Base Defense")
	EBaseDefenseFireDoctrine Doctrine = EBaseDefenseFireDoctrine::CoordinatedLine;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Base Defense")
	FName PolicyId;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FResolveBaseAssaultCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid AssaultId;

	/** Defaults to the historical stable-instance ordering for source and replay compatibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	EBaseDefenseFireDoctrine FireDoctrine = EBaseDefenseFireDoctrine::CoordinatedLine;
};

/** Commits the available unassigned field agents at a threatened base to its perimeter battle. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FDeployBaseDefenseOperationCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	int64 ExpectedSequence = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UEGT|Strategic")
	FGuid AssaultId;
};

UENUM(BlueprintType)
enum class EStrategicEventType : uint8
{
	BaseEstablished,
	ResearchStarted,
	ResearchStaffChanged,
	ResearchCompleted,
	FacilityConstructionStarted,
	FacilityConstructionCompleted,
	RecruitmentStarted,
	PersonnelArrived,
	PersonnelInjured,
	PersonnelRecoveryPlanSelected,
	PersonnelRecovered,
	PersonnelTrainingStarted,
	PersonnelTrainingCompleted,
	PersonnelEquipmentChanged,
	PersonnelDied,
	CraftAcquisitionStarted,
	CraftArrived,
	CraftPilotAssigned,
	CraftEquipmentChanged,
	CraftServiceStarted,
	CraftServiceCancelled,
	CraftRepaired,
	CraftRefueled,
	CraftServiceCompleted,
	CraftLaunched,
	CraftRecovered,
	CraftAgentsChanged,
	CraftCargoChanged,
	CraftSalvageRetained,
	CraftSalvageSold,
	CraftDeployedToSite,
	TacticalOperationReady,
	TacticalBattleGenerated,
	TacticalDeploymentConfirmed,
	TacticalStanceChanged,
	TacticalDoorStateChanged,
	TacticalUnitMoved,
	TacticalAttackResolved,
	TacticalSignalProjected,
	TacticalBlastResolved,
	TacticalUnitDamaged,
	TacticalUnitIncapacitated,
	TacticalTerrainDamaged,
	TacticalTerrainDestroyed,
	TacticalWeaponReloaded,
	TacticalMagazineEjected,
	TacticalDeviceDeployed,
	TacticalDeviceIntercepted,
	TacticalEnvironmentSuppressed,
	TacticalUnitSuppressed,
	TacticalMoraleChanged,
	TacticalUnitBurned,
	TacticalUnitPanicked,
	TacticalEnvironmentAdvanced,
	TacticalWindApplied,
	TacticalSmokeVentilated,
	TacticalEnvironmentPropagatedVertically,
	TacticalSmokeDiffused,
	TacticalFireSpread,
	TacticalObjectiveProgressed,
	TacticalObjectiveContested,
	TacticalObjectiveCompleted,
	TacticalObjectiveFailed,
	TacticalLootRecovered,
	TacticalUnitExtracted,
	TacticalBattleResolved,
	TacticalTurnEnded,
	TacticalAiDecisionMade,
	TacticalAiTurnCompleted,
	TacticalTurnLimitReached,
	TacticalOperationResolved,
	PersonnelExperienceGained,
	PersonnelPromoted,
	PersonnelDoctrineSelected,
	PersonnelCommendationAwarded,
	StrategicSiteSecured,
	StrategicContactCreated,
	StrategicContactDetected,
	StrategicContactEscaped,
	CraftDispatched,
	InterceptionReady,
	InterceptionLost,
	InterceptionWithdrawn,
	CraftReturnStarted,
	CraftRearmed,
	InterceptionRoundResolved,
	CraftWeaponFired,
	StrategicContactWeaponFired,
	StrategicContactDamaged,
	CraftDamaged,
	CraftDestroyed,
	StrategicContactDestroyed,
	InterceptionWon,
	InterceptionDefeated,
	StrategicSiteCreated,
	StrategicSiteExpired,
	AdversaryMissionLaunched,
	AdversaryMissionEscaped,
	AdversaryMissionThwarted,
	RegionalPressureChanged,
	MonthlyFundingChanged,
	CampaignWon,
	CampaignLost,
	ManufacturingStarted,
	ManufacturingStaffChanged,
	ItemManufactured,
	ManufacturingCompleted,
	MonthlyFinancesProcessed,
	TimeAdvanced,
	ResearchCancelled,
	ManufacturingCancelled,
	InventorySold,
	PersonnelTransferred,
	PersonnelDismissed,
	CraftTransferred,
	ManufacturingQuantityChanged,
	FacilityConstructionCancelled,
	FacilityDismantled,
	ManufacturingMaterialsReserved,
	ManufacturingMaterialsRefunded,
	FacilityDamaged,
	FacilityDisabled,
	FacilityRepairStarted,
	FacilityRepairCancelled,
	FacilityRepaired,
	BaseAssaultStarted,
	BaseDefenseTacticalOperationReady,
	/** A supplied automatic-defense battery consumed inventory immediately before firing. */
	BaseDefenseSupplyConsumed,
	BaseDefenseWeaponFired,
	BaseAssaultRepelled,
	BaseAssaultBreached,
	AdversaryPlanStarted,
	AdversaryPlanAdvanced,
	AdversaryPlanCompleted,
	/** Visible, landing-capable contact reached its destination and became an intact tactical site. */
	StrategicContactLanded,
	RegionalSupportChanged,
	RegionalFundingChanged,
	RegionalMandateReviewed,
	RegionalDiplomacyActionCompleted,
	RegionalCharterSigned,
	HorizonCompactRatified,
	/** One compact member accepted another member's crisis burden through a player-directed aid deployment. */
	CoalitionAidDeployed,
	/** Low support caused a signed regional partner to withdraw from compact-level benefits. */
	HorizonCompactMemberWithdrawn,
	/** The player rebuilt enough confidence and restored a withdrawn compact member. */
	HorizonCompactMemberRestored,
	/** A withdrawn member's emergency solidarity motion passed its deterministic majority ballot. */
	CoalitionEmergencyVoteResolved,
	/** One active compact member cast its deterministic ballot on an emergency motion. */
	CoalitionEmergencyBallotCast,
	/** An escaped mission redistributed retained pressure to the least-strained other compact member. */
	CoalitionPressureShared,
	/** An authored escaped operation eroded support in one other active compact member. */
	CoalitionCohesionStrained,
	/** An authored thwarted operation rebuilt support in one withdrawn compact member. */
	CoalitionRecoveryInspired,
	/** Resolved operations raised the deterministic adversary adaptation floor or an escape accelerated it. */
	AdversaryEscalationChanged,
	/** Campaign funds were committed before a threat-priced emergency-grid volley. */
	BaseDefenseGridOvercharged,
	/** A veteran's Watchkeeper Guidance raised lower-band teammates' starting tactical morale. */
	PersonnelMentorshipApplied,
	/** A craft entered or was promoted into the deterministic Flight-Deck Rotation schedule. */
	CraftServiceRotationScheduled,
	/** A Legacy Anchor relayed half of one mastered doctrine's field bonuses to teammates. */
	PersonnelLegacyRelayApplied,
	/** One persistent shared-victory pair supplied its current Field Cadence bonuses. */
	PersonnelSquadBondApplied,
	/** Two surviving agents advanced their persistent shared-victory record after success. */
	PersonnelSquadBondAdvanced,
	/** A veteran began a fixed-term player-selected base leadership commitment. */
	PersonnelStewardshipStarted,
	/** A veteran completed a fixed-term base leadership commitment. */
	PersonnelStewardshipCompleted,
	/** Injury or death ended a veteran's active base leadership commitment early. */
	PersonnelStewardshipInterrupted,
	/** Unassigned inventory left one established base with destination storage reserved. */
	MutualAidConvoyDispatched,
	/** An unescorted forecast route interdiction imposed its exact one-time delay. */
	MutualAidConvoyInterdicted,
	/** A paid Signal Escort prevented a forecast route interdiction delay. */
	MutualAidConvoyEscorted,
	/** A lossless inter-base aid commitment reached its destination. */
	MutualAidConvoyArrived,
	/** A convoy entered or was promoted into the deterministic Relay Weave schedule. */
	MutualAidConvoyRelayScheduled,
	/** Scientists were reassigned to or from a base's deterministic Signal Watch. */
	MutualAidSignalWatchStaffChanged,
	/** A held, never-progressed convoy changed Threadline doctrine before departure. */
	MutualAidConvoyThreadlineRetuned,
	/** A held, never-progressed convoy commissioned its Signal Escort before departure. */
	MutualAidConvoySignalEscortCommissioned,
	/** A held, never-progressed convoy moved to the front of its held Relay Weave line. */
	MutualAidConvoyReliefPrioritized,
	/** A held, never-progressed convoy stood down and returned its cargo to its source. */
	MutualAidConvoyReliefStoodDown,
	/** A held, never-progressed convoy redirected its committed cargo before departure. */
	MutualAidConvoyReliefDiverted,
	/** A held, never-progressed convoy configured or cleared a two-leg Relay Waypoint. */
	MutualAidConvoyRelayWaypointConfigured,
	/** A two-leg convoy reached its waypoint and began the committed onward route. */
	MutualAidConvoyRelayWaypointReached,
	/** A held two-leg convoy enabled or cleared its deterministic cargo split. */
	MutualAidConvoyBalancedHandoffConfigured,
	/** A convoy delivered its reserved share while crossing a Relay Waypoint. */
	MutualAidConvoyBalancedHandoffDelivered,
	/** Engineers were reassigned to or from a base's facility-mobilization cadre. */
	WorksCadreStaffChanged,
	/** A base selected a new future-work construction-versus-repair charter. */
	WorksCadreCharterChanged,
	/** A successful interception extended the next adversary-wave countdown. */
	InterceptionAftershockApplied
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	EStrategicEventType Type = EStrategicEventType::TimeAdvanced;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 CommandSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FDateTime TimestampUtc;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid ProjectId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid ConvoyId;

	/** First held convoy displaced by a queue-priority event. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid RelatedConvoyId;

	/** Destination or counterpart base when BaseId identifies the event's source. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid RelatedBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid FacilityInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid PersonnelId;

	/** Second personnel identity for pair-based career and deployment events. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid RelatedPersonnelId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid CraftId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid ContactId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid SiteId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid MissionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid AssaultId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid OperationId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid BattleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FGuid TacticalUnitId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	FGuid TargetTacticalUnitId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 FromX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 FromY = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 FromZ = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 ToX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 ToY = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 ToZ = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	ETacticalWindDirection WindDirection = ETacticalWindDirection::Calm;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 WindStrength = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 HitChance = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	int32 Roll = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical")
	bool bSuccessful = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName RuleId;

	/** Optional command-policy identity retained without overloading the authored rule identity. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName PolicyId;

	/** Previous policy identity for an explicit policy transition event. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName PreviousPolicyId;

	/** Temporary tactical attribute bonuses supplied by a personnel specialization event. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 PersonnelAccuracyBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 PersonnelResolveBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 PersonnelMobilityBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 PersonnelStrengthBonus = 0;

	/** EPersonnelSquadBondTier ordinal retained without coupling the event schema to presentation types. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 PersonnelSquadBondTier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 PersonnelSharedVictories = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 PersonnelActionPointBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 PersonnelMoraleBonus = 0;

	/** Exact Return Path duration committed by a personnel-care event. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 PersonnelRecoverySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 PersonnelRecoveryFundingCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 PersonnelRecoveryResolveBonus = 0;

	/** EPersonnelStewardshipFocus ordinal retained for leadership-history events. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 PersonnelStewardshipFocus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 PersonnelStewardshipToursCompleted = 0;

	/** Exact committed or remaining leadership time, depending on event type. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 PersonnelStewardshipDurationSeconds = 0;

	/** Exact funding or time reduction supplied by an active steward. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 PersonnelStewardshipBenefitAmount = 0;

	/** Exact route exposure committed by a Mutual Aid Convoy event. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ConvoyRoutePressure = 0;

	/** Previous route exposure retained by a Threadline Retune event. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ConvoyPreviousRoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyTransitSeconds = 0;

	/** Previous route duration retained by a Threadline Retune event. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyPreviousTransitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyEscortCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyDelaySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bConvoySignalEscort = false;

	/** Integrity-scaled signal channels available at the convoy source. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ConvoyRelayChannelCount = 0;

	/** One-based FIFO position at the time of Relay Weave scheduling. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ConvoyRelayQueuePosition = 0;

	/** One-based active or projected relay channel. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ConvoyRelayChannelNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bConvoyRelayActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyRelayWaitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyEstimatedArrivalSeconds = 0;

	/** Exact prior arrival projection before a queue-affecting convoy command. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyPreviousEstimatedArrivalSeconds = 0;

	/** Relay ordering token before a queue-priority event. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyPreviousDispatchSequence = 0;

	/** Relay ordering token after a queue-priority event. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyDispatchSequence = 0;

	/** Number of never-departed held convoys bypassed by Relief Priority. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ConvoyPriorityBypassedCount = 0;

	/** Destination storage released by a Relief Stand-Down. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyReleasedStorage = 0;

	/** Number of later held commitments advanced by a Relief Stand-Down. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ConvoyStandDownAdvancedCount = 0;

	/** Sum of exact wait reductions across later held commitments after a stand-down. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyStandDownRecoveredWaitSeconds = 0;

	/** Previous destination released by a Relief Diversion. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid ConvoyPreviousDestinationBaseId;

	/** Exact reservation moved between destinations by a Relief Diversion. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyDivertedStorage = 0;

	/** Number of later held commitments whose queue projection changed after diversion. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ConvoyDiversionAffectedCount = 0;

	/** Signed sum of target and later arrival shifts caused by Relief Diversion. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyDiversionTotalArrivalShiftSeconds = 0;

	/** Optional intermediate base configured for a two-leg Relay Waypoint journey. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid ConvoyRelayWaypointBaseId;

	/** Previous waypoint replaced or cleared by a Relay Waypoint configuration. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid ConvoyPreviousRelayWaypointBaseId;

	/** Stable onward route identity retained by waypoint configuration and arrival events. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName ConvoyOnwardPolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ConvoyOnwardRoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyOnwardTransitSeconds = 0;

	/** Exact projected arrival at the intermediate waypoint, including the source hold. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyWaypointArrivalSeconds = 0;

	/** Number of later held source commitments whose projection changed. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ConvoyWaypointAffectedCount = 0;

	/** Signed sum of target and later arrival shifts caused by waypoint configuration. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyWaypointTotalArrivalShiftSeconds = 0;

	/** Exact cargo delivered or reserved for delivery at the Relay Waypoint. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ConvoyHandoffQuantity = 0;

	/** Cargo retained for the final destination after a Balanced Handoff. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ConvoyFinalDeliveryQuantity = 0;

	/** Mass-weighted waypoint storage committed by a Balanced Handoff. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyHandoffStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyWaypointReservedStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ConvoyDestinationReservedStorage = 0;

	/** Operational facility channels before Signal Watch staffing. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 SignalWatchFacilityChannelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 SignalWatchAssignedScientists = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 SignalWatchBonusChannelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 SignalWatchTotalChannelCount = 0;

	/** Engineers reserved by Works Cadre when this staffing or facility-work event was committed. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 WorksCadreAssignedEngineers = 0;

	/** Exact baseline-work percentage front-loaded by Works Cadre. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 WorksCadreFrontloadPercent = 0;

	/** Works Charter active when this staffing or facility-work event was committed. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	EWorksCadreCharter WorksCadreCharter = EWorksCadreCharter::CommonCadence;

	/** Prior charter for a WorksCadreCharterChanged event. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	EWorksCadreCharter PreviousWorksCadreCharter = EWorksCadreCharter::CommonCadence;

	/** Exact construction front-load projected at the event's staffing level. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 WorksCadreConstructionFrontloadPercent = 0;

	/** Exact repair front-load projected at the event's staffing level. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 WorksCadreRepairFrontloadPercent = 0;

	/** Unmodified facility-work duration before Works Cadre front-loading. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 FacilityBaselineDurationSeconds = 0;

	/** Facility-work clock committed after Works Cadre front-loading. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 FacilityCommittedDurationSeconds = 0;

	/** Operational maintenance lanes available at the event's base. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ServiceLaneCount = 0;

	/** One-based shortest-turnaround-first position at the time of scheduling. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ServiceQueuePosition = 0;

	/** One-based active or projected service lane. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ServiceLaneNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bServiceLaneActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ServiceQueueWaitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ServiceReadySeconds = 0;

	/** Exact formation modifier applied to outgoing interception fire, when relevant. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 OutgoingAccuracyModifier = 0;

	/** Exact formation modifier applied to incoming interception fire, when relevant. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 IncomingAccuracyModifier = 0;

	/** Stable adversary maneuver identity applied to the interception event, when relevant. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName ContactManeuverPolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ContactManeuverOutgoingAccuracyModifier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ContactManeuverIncomingAccuracyModifier = 0;

	/** Exact contact route progress removed by a pursuit consequence, in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ContactRouteDelaySeconds = 0;

	/** Exact adversary-wave countdown before an interception aftershock. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 PreviousAdversaryMissionSeconds = 0;

	/** Exact deterministic delay added to the next adversary-wave countdown. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 AdversaryMissionDelaySeconds = 0;

	/** Exact adversary-wave countdown after an interception aftershock. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 NextAdversaryMissionSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName RegionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Amount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicCommandDiagnostic
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName Code;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FString Message;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicCommandResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bDecisionPause = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ExecutedSlices = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicEvent> Events;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;

	bool HasDiagnostic(FName Code) const;
	bool HasEvent(EStrategicEventType Type) const;
};

/** Derived, lossless storage state. Capacity is mass-weighted and production reserves future output. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FBaseStorageEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bValid = false;

	/** False keeps rule sets authored before storageCapacity backward-compatible and unlimited. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bEnforced = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Capacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Used = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Reserved = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ManufacturingReserved = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 MutualAidReserved = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Committed = 0;

	/** Remaining capacity, clamped to zero while over capacity or when enforcement is disabled. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Available = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Overflow = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Read-only service profile derived from the current integrity-scaled base outputs. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicBaseSpecializationView
{
	GENERATED_BODY()

	/** Stable policy selected from the strongest operational capability. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName SpecializationId = TEXT("base.specialization.integrated-command");

	/** Existing capability value normalized to a 0-100 specialization index. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 Score = 0;

	/** Second-highest normalized capability, exposed to keep close calls explainable. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int32 SecondaryScore = 0;

	/** Existing output represented by the selected specialization. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName BenefitMetricId = TEXT("base.specialization.balanced-capabilities");

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 BenefitValue = 0;

	/** Save-neutral operational consequence currently supplied by the profile. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FName OperationalBenefitMetricId = TEXT("base.specialization.no-operational-benefit");

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	int64 OperationalBenefitValue = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	bool bSpecialized = false;
};

/** Mutation-free policy projection shared by dispatch validation and presentation. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FMutualAidRouteEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	EMutualAidRoutePolicy Policy = EMutualAidRoutePolicy::OpenRelay;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 TransitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 BaselinePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 ExposureModifier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 RoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bInterdictionExpected = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bSignalEscort = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bSignalEscortAffordable = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 SignalEscortCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 InterdictionDelaySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Mutation-free held-convoy retune projection shared by command and presentation paths. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FThreadlineRetuneEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid ConvoyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	EMutualAidRoutePolicy CurrentPolicy = EMutualAidRoutePolicy::OpenRelay;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	EMutualAidRoutePolicy RequestedPolicy = EMutualAidRoutePolicy::OpenRelay;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName CurrentPolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName RequestedPolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 CurrentTransitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 RequestedTransitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 CurrentRoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 RequestedRoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bInterdictionExpected = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bSignalEscort = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 ForecastInterdictionDelaySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FMutualAidRelayQueueView CurrentRelayQueue;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FMutualAidRelayQueueView ProjectedRelayQueue;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Mutation-free post-dispatch Signal Escort projection shared by command and presentation. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FSignalEscortCommissionEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid ConvoyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	EMutualAidRoutePolicy RoutePolicy = EMutualAidRoutePolicy::OpenRelay;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName RoutePolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 RoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 FundingCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 CurrentFunds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 ProjectedFunds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 PreventedDelaySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FMutualAidRelayQueueView CurrentRelayQueue;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FMutualAidRelayQueueView ProjectedRelayQueue;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Mutation-free Relief Priority projection shared by command and presentation paths. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FMutualAidReliefPriorityEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid ConvoyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 CurrentDispatchSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 ProjectedDispatchSequence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 BypassedConvoyCount = 0;

	/** Stable pre-priority order of every held convoy moved back by this command. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FGuid> BypassedConvoyIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 RecoveredWaitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FMutualAidRelayQueueView CurrentRelayQueue;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FMutualAidRelayQueueView ProjectedRelayQueue;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FMutualAidRelayQueueView> CurrentBypassedRelayQueues;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FMutualAidRelayQueueView> ProjectedBypassedRelayQueues;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Mutation-free Relief Stand-Down projection shared by command and presentation paths. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FMutualAidReliefStandDownEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid ConvoyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid SourceBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid DestinationBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 ReleasedStorage = 0;

	/** Already-spent Signal Escort funding; Relief Stand-Down never refunds it. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 SunkSignalEscortCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FMutualAidRelayQueueView CurrentRelayQueue;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 AdvancedConvoyCount = 0;

	/** Stable pre-stand-down order of every later held convoy that advances. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FGuid> AdvancedConvoyIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 TotalRecoveredWaitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FMutualAidRelayQueueView> CurrentAdvancedRelayQueues;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FMutualAidRelayQueueView> ProjectedAdvancedRelayQueues;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Mutation-free Relief Diversion projection shared by command and presentation paths. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FMutualAidReliefDiversionEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid ConvoyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid SourceBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid CurrentDestinationBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid RequestedDestinationBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 Quantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 DivertedStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	EMutualAidRoutePolicy RoutePolicy = EMutualAidRoutePolicy::OpenRelay;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName RoutePolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 CurrentTransitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 ProjectedTransitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 CurrentRoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 ProjectedRoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bInterdictionExpected = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bSignalEscort = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 RetainedSignalEscortCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 ForecastInterdictionDelaySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 CurrentDestinationReservedStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 ProjectedCurrentDestinationReservedStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 RequestedDestinationReservedStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 ProjectedRequestedDestinationReservedStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FMutualAidRelayQueueView CurrentRelayQueue;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FMutualAidRelayQueueView ProjectedRelayQueue;

	/** Signed target-arrival change; negative values arrive sooner. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 TargetArrivalShiftSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 AffectedConvoyCount = 0;

	/** Stable held-line order of later commitments whose wait or arrival changes. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FGuid> AffectedConvoyIds;

	/** Signed sum of target and affected later arrival shifts. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 TotalArrivalShiftSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FMutualAidRelayQueueView> CurrentAffectedRelayQueues;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FMutualAidRelayQueueView> ProjectedAffectedRelayQueues;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Mutation-free two-leg Relay Waypoint projection shared by command and presentation. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FMutualAidRelayWaypointEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bDirectRouteRequested = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid ConvoyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid SourceBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid DestinationBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid CurrentWaypointBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid RequestedWaypointBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	EMutualAidRoutePolicy FirstLegRoutePolicy = EMutualAidRoutePolicy::OpenRelay;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName FirstLegRoutePolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 FirstLegTransitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 FirstLegRoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bFirstLegInterdictionExpected = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	EMutualAidRoutePolicy OnwardRoutePolicy = EMutualAidRoutePolicy::OpenRelay;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName OnwardRoutePolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 OnwardTransitSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 OnwardRoutePressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bOnwardInterdictionExpected = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bSignalEscort = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 RetainedSignalEscortCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 CurrentJourneySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 ProjectedJourneySeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 ProjectedWaypointArrivalSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FMutualAidRelayQueueView CurrentRelayQueue;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FMutualAidRelayQueueView ProjectedRelayQueue;

	/** Signed target-arrival change; positive values arrive later. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 TargetArrivalShiftSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 AffectedConvoyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FGuid> AffectedConvoyIds;

	/** Signed sum of target and affected later arrival shifts. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 TotalArrivalShiftSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FMutualAidRelayQueueView> CurrentAffectedRelayQueues;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FMutualAidRelayQueueView> ProjectedAffectedRelayQueues;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Mutation-free waypoint cargo-split projection shared by command and presentation. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FMutualAidBalancedHandoffEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bEnabled = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid ConvoyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid WaypointBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid DestinationBaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 TotalQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 CurrentHandoffQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 ProjectedHandoffQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 ProjectedFinalQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int64 HandoffStorage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FBaseStorageEvaluation CurrentWaypointStorage;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FBaseStorageEvaluation ProjectedWaypointStorage;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FBaseStorageEvaluation CurrentDestinationStorage;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FBaseStorageEvaluation ProjectedDestinationStorage;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FMutualAidRelayQueueView RelayQueue;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Mutation-free Signal Watch staffing projection shared by command and presentation paths. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FSignalWatchStaffEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 CurrentScientists = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 RequestedScientists = 0;

	/** Largest currently legal assignment after active research commitments. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 MaximumScientists = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 ResearchAssignedScientists = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 ScientistCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 FacilityRelayChannelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 EffectiveWatchScientists = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 BonusRelayChannelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	int32 TotalRelayChannelCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Logistics")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Mutation-free Works Cadre staffing projection shared by command and presentation paths. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FWorksCadreStaffEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 CurrentEngineers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 RequestedEngineers = 0;

	/** Largest currently legal assignment after active manufacturing commitments. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 MaximumEngineers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 ManufacturingAssignedEngineers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 EngineerCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	EWorksCadreCharter Charter = EWorksCadreCharter::CommonCadence;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 ConstructionFrontloadPercentPerEngineer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 RepairFrontloadPercentPerEngineer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 ConstructionFrontloadPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 RepairFrontloadPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** One immutable Works Charter choice in fixed player-facing order. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FWorksCadreCharterPolicy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	EWorksCadreCharter Charter = EWorksCadreCharter::CommonCadence;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 ConstructionFrontloadPercentPerEngineer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 RepairFrontloadPercentPerEngineer = 0;
};

/** Mutation-free future-work projection for one requested Works Charter. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FWorksCadreCharterEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	EWorksCadreCharter CurrentCharter = EWorksCadreCharter::CommonCadence;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	EWorksCadreCharter RequestedCharter = EWorksCadreCharter::CommonCadence;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	FName PolicyId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 AssignedEngineers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 ConstructionFrontloadPercentPerEngineer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 RepairFrontloadPercentPerEngineer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 ConstructionFrontloadPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	int32 RepairFrontloadPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Facilities")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Integrity-scaled facility contributions used consistently by simulation and presentation. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FBaseInfrastructureEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Presentation")
	FStrategicBaseSpecializationView Specialization;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid BaseId;

	/** Persisted base-local allowance before facility bonuses. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 BaseScientistCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 FacilityScientistCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ScientistCapacity = 0;

	/** Persisted base-local allowance before facility bonuses. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 BaseEngineerCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 FacilityEngineerCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 EngineerCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 CraftCapacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 SensorRangeKilometers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 DetectionStrength = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 DefenseBatteryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 MaximumDefenseDamage = 0;

	/** Rounded uncapped expected damage from one complete operational battery volley. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ExpectedDefenseDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Read-only result used by presentation code before offering a destructive facility action. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FFacilityDismantleEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName FacilityId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Refund = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FFacilityRepairEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName FacilityId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 Damage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 WorksCadreEngineers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 WorksCadreFrontloadPercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	EWorksCadreCharter WorksCadreCharter = EWorksCadreCharter::CommonCadence;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 BaselineDurationSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 DurationSeconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** One inventory pool used to ready a deterministic automatic-defense volley. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FBaseDefenseSupplyEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName ItemId;

	/** Stock needed to ready every operational battery that uses this item. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 RequiredQuantity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 AvailableQuantity = 0;

	/** Stock deterministically allocated to batteries that are ready to fire. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 AllocatedQuantity = 0;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FBaseAssaultEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid AssaultId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid ContactId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	EBaseDefenseFireDoctrine FireDoctrine = EBaseDefenseFireDoctrine::CoordinatedLine;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName PolicyId;

	/** Immediate campaign-fund commitment. Zero for doctrines that use normal power routing. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 FundingCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bAffordable = true;

	/** Applied after integrity scaling and before the shot roll. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 AccuracyBonus = 0;

	/** Applied after integrity scaling; 100 leaves authored damage unchanged. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 DamagePercent = 100;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 DefenseBatteryCount = 0;

	/** Operational batteries with either legacy unlimited fire or an allocated supply load. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ReadyDefenseBatteryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 MaximumDefenseDamage = 0;

	/** Rounded uncapped expected damage from one complete operational battery volley. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ExpectedDefenseDamage = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FBaseDefenseSupplyEvaluation> DefenseSupplies;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 ContactHull = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 BreachDamagePerFacility = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 MaximumFacilitiesHit = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

USTRUCT(BlueprintType)
struct UEGTCORE_API FBaseDefenseDeploymentEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid AssaultId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid BaseId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid ContactId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName MissionRuleId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FGuid> AgentIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Read-only authoritative preview for one regional outreach choice. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FRegionalDiplomacyEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName RegionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	ERegionalDiplomacyActionType ActionType = ERegionalDiplomacyActionType::CivicRelief;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 SupportDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 PressureReduction = 0;

	/** Nonzero only for crisis-gated actions. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 MinimumPressure = 0;

	/** True when this accepted support cost would force an active compact member to withdraw. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bWouldWithdrawCompactMember = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Read-only authoritative preview for the durable Resilience Charter. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FRegionalCharterEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName RegionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bSigned = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 SupportCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 MinimumSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 FundingPercent = 100;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 MissionWeightPercent = 100;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 EscapePressurePercent = 100;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ProjectedMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 MonthlyFundingDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Read-only authoritative preview for the durable multi-partner Horizon Compact. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FHorizonCompactEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bRatified = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 RequiredCharters = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 SignedCharters = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 MinimumMemberSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 MemberSupportCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 FundingPercent = 100;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 SharedEscapePressurePercent = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 WithdrawalSupportThreshold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 RestorationMinimumSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 CurrentMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ProjectedMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 MonthlyFundingDelta = 0;

	/** Canonically sorted signed charter partners that would commit support. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FName> MemberRegionIds;

	/** Canonically sorted charter partners currently receiving compact-level benefits. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FName> ActiveMemberRegionIds;

	/** Canonically sorted charter partners that withdrew after a support collapse. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FName> WithdrawnMemberRegionIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Read-only authoritative preview for one monthly Reciprocal Aid deployment. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FReciprocalAidEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName TargetRegionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName DonorRegionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 MinimumTargetPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 MaximumPressureTransfer = 0;

	/** Exact pressure moved after accounting for donor capacity. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 PressureTransfer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 TargetCurrentPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 TargetProjectedPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 DonorCurrentPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 DonorProjectedPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 TargetSupportGain = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 DonorSupportCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 TargetProjectedSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 DonorProjectedSupport = 0;

	/** True when accepting this transfer would push the donor below compact cohesion. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bDonorWouldWithdraw = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 CurrentMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ProjectedMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 MonthlyFundingDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Read-only authoritative preview for rebuilding one withdrawn member's compact confidence. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FHorizonCompactRestorationEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName RegionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bWithdrawn = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 CurrentSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 MinimumSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 CurrentMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ProjectedMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 MonthlyFundingDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Read-only authoritative preview for one withdrawn member's monthly solidarity motion. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FHorizonCompactEmergencyVoteEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName TargetRegionId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bTargetWithdrawn = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 Cost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 TargetCurrentSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 TargetProjectedSupport = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 TargetSupportGain = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 TargetCurrentPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 TargetProjectedPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 TargetPressureReduction = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 VoterSupportCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 MaximumVoterPressure = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 RequiredVotes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FName> SupportingMemberRegionIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FName> OpposingMemberRegionIds;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 CurrentMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 ProjectedMonthlyFunding = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int64 MonthlyFundingDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Read-only authoritative preview for one pending personnel doctrine choice. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FPersonnelDoctrineEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	bool bAllowed = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FGuid PersonnelId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	FName DoctrineId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 CurrentSelections = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	int32 MaximumSelections = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic")
	TArray<FStrategicCommandDiagnostic> Diagnostics;
};

/** Transactional command gateway for deterministic strategic campaign mutations. */
class UEGTCORE_API FStrategicCommandService final
{
public:
	/** Resolves stable, original engagement-geometry tradeoffs without consuming a random draw. */
	static FInterceptionPosturePolicy GetInterceptionPosturePolicy(EInterceptionPosture Posture);

	/** Derives the active solo or linked-wing benefit from on-station craft without mutation. */
	static FInterceptionCoordinationPolicy EvaluateInterceptionCoordination(
		const FCampaignState& State,
		FGuid ContactId);

	/** Selects the contact's current pursuit maneuver without mutation or a random draw. */
	static FInterceptionContactManeuverPolicy EvaluateInterceptionContactManeuver(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		FGuid ContactId);

	/** Resolves stable withdrawal-doctrine identity without reading or mutating campaign state. */
	static FInterceptionWithdrawalPolicy GetInterceptionWithdrawalPolicy(
		EInterceptionWithdrawalDoctrine Doctrine);

	/** Projects the exact craft set and availability for a withdrawal command without mutation. */
	static FInterceptionWithdrawalEvaluation EvaluateInterceptionWithdrawal(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FWithdrawInterceptionCommand& Command);

	/** Resolves a stable automatic-defense allocation policy without consuming a random draw. */
	static FBaseDefenseFireDoctrinePolicy GetBaseDefenseFireDoctrinePolicy(EBaseDefenseFireDoctrine Doctrine);

	/** Resolves the configured mission-gap and escape-impact multipliers for a campaign difficulty. */
	static bool GetAdversaryDifficultyTuning(
		ECampaignDifficulty Difficulty,
		const FStrategicSimulationConfig& Config,
		FAdversaryDifficultyTuning& OutTuning);

	/** Projects adaptation without mutation or a random draw; command and presentation layers share this derivation. */
	static bool GetAdversaryAdaptationProgress(
		const FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		FAdversaryAdaptationProgress& OutProgress);

	/** Applies the campaign mission-gap multiplier with checked deterministic ceiling arithmetic. */
	static bool ScaleAdversaryIntervalSeconds(
		int64 BaseSeconds,
		ECampaignDifficulty Difficulty,
		const FStrategicSimulationConfig& Config,
		int64& OutSeconds);

	/** Applies the campaign escape-impact multiplier with checked deterministic ceiling arithmetic. */
	static bool ScaleAdversaryEscapeConsequence(
		int64 BaseValue,
		ECampaignDifficulty Difficulty,
		const FStrategicSimulationConfig& Config,
		int64& OutValue);

	/** Calculates the exact deterministic next-wave delay for a successful interception. */
	static bool CalculateInterceptionAftershockSeconds(
		int32 ContactThreatRating,
		const FStrategicSimulationConfig& Config,
		int64& OutSeconds);

	/** Support tiers fund at 0/75/100/110 percent for suspended/strained/committed/allied partners. */
	static int32 GetRegionalFundingPercent(int32 Support);

	static bool CalculateRegionalFundingContribution(
		int64 BaselineMonthlyFunding,
		int32 Support,
		int64& OutContribution);

	/** Applies both the support tier and any durable charter funding commitment. */
	static bool CalculateRegionalFundingContribution(
		const FRegionalMandateState& Mandate,
		const FStrategicSimulationConfig& Config,
		int64& OutContribution);

	/** Applies support tier, charter, and—when ratified—coalition funding policy. */
	static bool CalculateRegionalFundingContribution(
		const FRegionalMandateState& Mandate,
		const FStrategicSimulationConfig& Config,
		bool bHorizonCompactRatified,
		int64& OutContribution);

	static FRegionalDiplomacyEvaluation EvaluateRegionalDiplomacy(
		const FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		const FRegionalDiplomacyCommand& Command);

	static FRegionalCharterEvaluation EvaluateRegionalCharter(
		const FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		const FSignRegionalCharterCommand& Command);

	static FHorizonCompactEvaluation EvaluateHorizonCompact(
		const FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		const FRatifyHorizonCompactCommand& Command);

	static FReciprocalAidEvaluation EvaluateReciprocalAid(
		const FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		const FDeployReciprocalAidCommand& Command);

	static FHorizonCompactRestorationEvaluation EvaluateHorizonCompactRestoration(
		const FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		const FRestoreHorizonCompactMemberCommand& Command);

	static FHorizonCompactEmergencyVoteEvaluation EvaluateHorizonCompactEmergencyVote(
		const FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		const FCallHorizonCompactEmergencyVoteCommand& Command);

	static FPersonnelDoctrineEvaluation EvaluatePersonnelDoctrine(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSelectPersonnelDoctrineCommand& Command);

	/** Read-only validation of installed facility placement, grid bounds, and construction projects. */
	static FStrategicCommandResult ValidateFacilityLayout(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config);

	/** Read-only validation of persisted research and manufacturing projects. */
	static FStrategicCommandResult ValidateStrategicProjectState(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules);

	static FBaseInfrastructureEvaluation EvaluateBaseInfrastructure(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		FGuid BaseId);

	static FStrategicBaseSpecializationView EvaluateBaseSpecialization(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules);

	/** Derived research throughput percentage supplied by a specialized Research Enclave base. */
	static int32 EvaluateBaseResearchRatePercent(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules);

	/** Derived manufacturing throughput percentage supplied by a specialized Fabrication Works base. */
	static int32 EvaluateBaseManufacturingRatePercent(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules);

	/** Derived maintenance-lane bonus supplied by a specialized Flight Operations base. */
	static int32 EvaluateBaseServiceLaneBonus(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules);

	/** Derived storage-capacity percentage supplied by a specialized Logistics Depot base. */
	static int32 EvaluateBaseStorageCapacityPercent(
		const FStrategicBaseState& Base,
		const FResolvedRuleSet& Rules);

	static FBaseStorageEvaluation EvaluateBaseStorage(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FGuid BaseId);

	/** Fixed deterministic policy order used by command surfaces and replay tests. */
	static TArray<EMutualAidRoutePolicy> GetMutualAidRoutePolicies();

	static FName GetMutualAidRoutePolicyId(EMutualAidRoutePolicy Policy);

	static FMutualAidRouteEvaluation EvaluateMutualAidRoute(
		const FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		FGuid SourceBaseId,
		FGuid DestinationBaseId,
		EMutualAidRoutePolicy Policy,
		bool bSignalEscort);

	static FThreadlineRetuneEvaluation EvaluateThreadlineRetune(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FRetuneMutualAidConvoyCommand& Command);

	static FSignalEscortCommissionEvaluation EvaluateSignalEscortCommission(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FCommissionMutualAidSignalEscortCommand& Command);

	static FMutualAidReliefPriorityEvaluation EvaluateMutualAidReliefPriority(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FPrioritizeMutualAidConvoyCommand& Command);

	static FMutualAidReliefStandDownEvaluation EvaluateMutualAidReliefStandDown(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStandDownMutualAidConvoyCommand& Command);

	static FMutualAidReliefDiversionEvaluation EvaluateMutualAidReliefDiversion(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FDivertMutualAidConvoyCommand& Command);

	static FMutualAidRelayWaypointEvaluation EvaluateMutualAidRelayWaypoint(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FConfigureMutualAidRelayWaypointCommand& Command);

	static FMutualAidBalancedHandoffEvaluation EvaluateMutualAidBalancedHandoff(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FConfigureMutualAidBalancedHandoffCommand& Command);

	static FSignalWatchStaffEvaluation EvaluateSignalWatchStaff(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSetSignalWatchStaffCommand& Command);

	static FWorksCadreStaffEvaluation EvaluateWorksCadreStaff(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSetWorksCadreStaffCommand& Command);

	static FWorksCadreCharterEvaluation EvaluateWorksCadreCharter(
		const FCampaignState& State,
		const FSetWorksCadreCharterCommand& Command);

	static FName WorksCadrePolicyId();
	static int32 WorksCadreMaximumEngineers();
	static TArray<FWorksCadreCharterPolicy> GetWorksCadreCharterPolicies();
	static FWorksCadreCharterPolicy GetWorksCadreCharterPolicy(EWorksCadreCharter Charter);

	static FFacilityDismantleEvaluation EvaluateFacilityDismantle(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FDismantleFacilityCommand& Command);

	static FFacilityRepairEvaluation EvaluateFacilityRepair(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStartFacilityRepairCommand& Command);

	static FBaseAssaultEvaluation EvaluateBaseAssault(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FResolveBaseAssaultCommand& Command);

	/** Config-aware assault projection used by live command and dashboard paths. */
	static FBaseAssaultEvaluation EvaluateBaseAssault(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FResolveBaseAssaultCommand& Command);

	static FBaseDefenseDeploymentEvaluation EvaluateBaseDefenseDeployment(
		const FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FDeployBaseDefenseOperationCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FEstablishBaseCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStartResearchCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSetResearchStaffCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FCancelResearchCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FAdvanceStrategicTimeCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FAdvanceStrategicTimeCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FRegionalDiplomacyCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FSignRegionalCharterCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FRatifyHorizonCompactCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FDeployReciprocalAidCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FRestoreHorizonCompactMemberCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FCallHorizonCompactEmergencyVoteCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FStartManufacturingCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSetManufacturingStaffCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FAdjustManufacturingUnitsCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FCancelManufacturingCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSellInventoryCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FDispatchMutualAidConvoyCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FRetuneMutualAidConvoyCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FCommissionMutualAidSignalEscortCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FPrioritizeMutualAidConvoyCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStandDownMutualAidConvoyCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FDivertMutualAidConvoyCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FConfigureMutualAidRelayWaypointCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FConfigureMutualAidBalancedHandoffCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSetSignalWatchStaffCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSetWorksCadreStaffCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FSetWorksCadreCharterCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FStartFacilityConstructionCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FCancelFacilityConstructionCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FDismantleFacilityCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FApplyFacilityDamageCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStartFacilityRepairCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FCancelFacilityRepairCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FRecruitPersonnelCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FTransferPersonnelCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FDismissPersonnelCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FApplyPersonnelDamageCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		const FSelectPersonnelRecoveryPlanCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FBeginPersonnelStewardshipCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FStrategicSimulationConfig& Config,
		const FBeginPersonnelTrainingCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSelectPersonnelDoctrineCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSetPersonnelEquipmentCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FAcquireCraftCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FTransferCraftCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FAssignCraftPilotCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSetCraftEquipmentCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FBeginCraftServiceCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FCancelCraftServiceCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FRearmCraftCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FLaunchCraftCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FRecoverCraftCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSetCraftAgentsCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSetCraftCargoCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FResolveCraftSalvageCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FDeployCraftToSiteCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FResolveTacticalOperationCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FGenerateTacticalBattleCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FDeployBaseDefenseOperationCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FConfirmTacticalDeploymentCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FMoveTacticalUnitCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FChangeTacticalStanceCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FSetTacticalDoorCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FAttackTacticalUnitCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FAttackTacticalTerrainCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FProjectTacticalSignalCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FReloadTacticalWeaponCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FEjectTacticalMagazineCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FDeployTacticalDeviceCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FInteractTacticalObjectiveCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FExtractTacticalUnitCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FEndTacticalTurnCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FRunTacticalAiTurnCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FCreateStrategicContactCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FDispatchCraftCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FWithdrawInterceptionCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FResolveInterceptionRoundCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FResolveInterceptionRoundCommand& Command);

	static FStrategicCommandResult Execute(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FResolveBaseAssaultCommand& Command);

	/** Places legacy abstract facilities deterministically using the active rule dimensions. */
	static bool UpgradeLegacyFacilityLayouts(
		FCampaignState& State,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		TArray<FStrategicCommandDiagnostic>& OutDiagnostics);
};
