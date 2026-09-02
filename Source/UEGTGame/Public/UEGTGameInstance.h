#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Campaign/CampaignSaveStore.h"
#include "Content/ContentPackageCatalog.h"
#include "Engine/GameInstance.h"
#include "Strategic/StrategicCommandService.h"
#include "Strategic/StrategicPresentationService.h"
#include "Tactical/TacticalAiService.h"
#include "Tactical/TacticalCombatService.h"
#include "Tactical/TacticalNavigationService.h"
#include "Tactical/TacticalPresentationService.h"

#include "UEGTGameInstance.generated.h"

UENUM(BlueprintType)
enum class EUEGTFundingModel : uint8
{
	BalancedMandate,
	RapidMobilization,
	SustainedCharter
};

USTRUCT(BlueprintType)
struct UEGTGAME_API FUEGTFundingProjection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign")
	int64 StartingFunds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign")
	int64 MonthlyFunding = 0;
};

/** Unreal lifecycle adapter for the deterministic UEGT campaign domain. */
UCLASS(BlueprintType)
class UEGTGAME_API UUEGTGameInstance final : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UFUNCTION(BlueprintCallable, Category = "UEGT|Content")
	bool ReloadContent();

	/** Explicit-root entrypoint for editor tools and deterministic integration tests. */
	bool ReloadContentFromDirectories(const TArray<FString>& ContentDirectories);

	UFUNCTION(BlueprintPure, Category = "UEGT|Content")
	bool IsContentReady() const { return bContentReady; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Content")
	TArray<FContentDiagnostic> GetContentDiagnostics() const { return ContentDiagnostics; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Content")
	TArray<FString> GetLoadedContentFiles() const { return LoadedContentFiles; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Content")
	TArray<FString> GetLoadedContentDirectories() const { return LoadedContentDirectories; }

	/** Default player-managed mod root used unless -UEGTModsDir or -UEGTNoUserMods is supplied. */
	UFUNCTION(BlueprintPure, Category = "UEGT|Content")
	FString GetDefaultUserModsDirectory() const;

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	bool StartNewCampaign(
		ECampaignDifficulty Difficulty,
		int64 Seed,
		EUEGTFundingModel FundingModel);

	/** Pure setup projection; funding choice consumes no random draw and persists as numeric economy state. */
	static bool CalculateFundingProjection(
		ECampaignDifficulty Difficulty,
		EUEGTFundingModel FundingModel,
		FUEGTFundingProjection& OutProjection);

	/** Resolves the active simulation configuration used by campaign setup and adversary commands. */
	bool GetAdversaryDifficultyTuning(
		ECampaignDifficulty Difficulty,
		FAdversaryDifficultyTuning& OutTuning) const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Campaign")
	bool HasActiveCampaign() const { return bCampaignActive; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Campaign")
	FCampaignState GetCampaignState() const { return CampaignState; }

#if !UE_BUILD_SHIPPING
	/** Installs a coherent command-ready coalition-recovery fixture for packaged presentation checks. */
	bool PrepareHorizonCompactEmergencyVoteDemo(FName TargetRegionId);

	/** Installs one detected authored coalition-counterplay operation for packaged presentation checks. */
	bool PrepareCoalitionCounterplayDemo(FName MissionRuleId);
#endif

	/** Builds an immutable command-ready strategic dashboard. */
	UFUNCTION(BlueprintPure, Category = "UEGT|Campaign|Presentation")
	FStrategicDashboardSnapshot BuildStrategicDashboard() const;

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult EstablishBase(const FEstablishBaseCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult StartResearch(const FStartResearchCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult SetResearchStaff(const FSetResearchStaffCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult CancelResearch(const FCancelResearchCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult AdvanceStrategicTime(const FAdvanceStrategicTimeCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult ExecuteRegionalDiplomacy(const FRegionalDiplomacyCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult SignRegionalCharter(const FSignRegionalCharterCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult RatifyHorizonCompact(const FRatifyHorizonCompactCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult DeployReciprocalAid(const FDeployReciprocalAidCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult RestoreHorizonCompactMember(
		const FRestoreHorizonCompactMemberCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult CallHorizonCompactEmergencyVote(
		const FCallHorizonCompactEmergencyVoteCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult StartManufacturing(const FStartManufacturingCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult SetManufacturingStaff(const FSetManufacturingStaffCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult AdjustManufacturingUnits(const FAdjustManufacturingUnitsCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult CancelManufacturing(const FCancelManufacturingCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult SellInventory(const FSellInventoryCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult DispatchMutualAidConvoy(
		const FDispatchMutualAidConvoyCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult RetuneMutualAidConvoy(
		const FRetuneMutualAidConvoyCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult CommissionMutualAidSignalEscort(
		const FCommissionMutualAidSignalEscortCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult PrioritizeMutualAidConvoy(
		const FPrioritizeMutualAidConvoyCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult StandDownMutualAidConvoy(
		const FStandDownMutualAidConvoyCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult DivertMutualAidConvoy(
		const FDivertMutualAidConvoyCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult ConfigureMutualAidRelayWaypoint(
		const FConfigureMutualAidRelayWaypointCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult ConfigureMutualAidBalancedHandoff(
		const FConfigureMutualAidBalancedHandoffCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult SetSignalWatchStaff(
		const FSetSignalWatchStaffCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult SetWorksCadreStaff(
		const FSetWorksCadreStaffCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult SetWorksCadreCharter(
		const FSetWorksCadreCharterCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult StartFacilityConstruction(const FStartFacilityConstructionCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult CancelFacilityConstruction(const FCancelFacilityConstructionCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult DismantleFacility(const FDismantleFacilityCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult ApplyFacilityDamage(const FApplyFacilityDamageCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult StartFacilityRepair(const FStartFacilityRepairCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult CancelFacilityRepair(const FCancelFacilityRepairCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult RecruitPersonnel(const FRecruitPersonnelCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult TransferPersonnel(const FTransferPersonnelCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult DismissPersonnel(const FDismissPersonnelCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult ApplyPersonnelDamage(const FApplyPersonnelDamageCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult SelectPersonnelRecoveryPlan(const FSelectPersonnelRecoveryPlanCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult BeginPersonnelStewardship(const FBeginPersonnelStewardshipCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult BeginPersonnelTraining(const FBeginPersonnelTrainingCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult SelectPersonnelDoctrine(const FSelectPersonnelDoctrineCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult SetPersonnelEquipment(const FSetPersonnelEquipmentCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult AcquireCraft(const FAcquireCraftCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult TransferCraft(const FTransferCraftCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult AssignCraftPilot(const FAssignCraftPilotCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult SetCraftEquipment(const FSetCraftEquipmentCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult BeginCraftService(const FBeginCraftServiceCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult CancelCraftService(const FCancelCraftServiceCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult RearmCraft(const FRearmCraftCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult LaunchCraft(const FLaunchCraftCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult RecoverCraft(const FRecoverCraftCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult SetCraftAgents(const FSetCraftAgentsCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult SetCraftCargo(const FSetCraftCargoCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult ResolveCraftSalvage(const FResolveCraftSalvageCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult DeployCraftToSite(const FDeployCraftToSiteCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult ResolveTacticalOperation(const FResolveTacticalOperationCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult GenerateTacticalBattle(const FGenerateTacticalBattleCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical")
	FStrategicCommandResult ConfirmTacticalDeployment(const FConfirmTacticalDeploymentCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical")
	FStrategicCommandResult MoveTacticalUnit(const FMoveTacticalUnitCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical")
	FStrategicCommandResult ChangeTacticalStance(const FChangeTacticalStanceCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical")
	FStrategicCommandResult SetTacticalDoor(const FSetTacticalDoorCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical")
	FStrategicCommandResult AttackTacticalUnit(const FAttackTacticalUnitCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical")
	FStrategicCommandResult AttackTacticalTerrain(const FAttackTacticalTerrainCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical")
	FStrategicCommandResult ProjectTacticalSignal(const FProjectTacticalSignalCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical")
	FStrategicCommandResult ReloadTacticalWeapon(const FReloadTacticalWeaponCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical")
	FStrategicCommandResult EjectTacticalMagazine(const FEjectTacticalMagazineCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical")
	FStrategicCommandResult DeployTacticalDevice(const FDeployTacticalDeviceCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical")
	FStrategicCommandResult InteractTacticalObjective(const FInteractTacticalObjectiveCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical")
	FStrategicCommandResult ExtractTacticalUnit(const FExtractTacticalUnitCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical")
	FStrategicCommandResult EndTacticalTurn(const FEndTacticalTurnCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|AI")
	FStrategicCommandResult RunTacticalAiTurn(const FRunTacticalAiTurnCommand& Command);

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|AI")
	FTacticalAiDecision ChooseTacticalAiAction(FGuid BattleId, FGuid UnitId) const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical")
	FTacticalPathResult FindTacticalPath(
		FGuid BattleId,
		FGuid UnitId,
		int32 DestinationX,
		int32 DestinationY,
		int32 DestinationZ = 0) const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical")
	FTacticalVisibilityResult ComputeTacticalVisibility(FGuid BattleId) const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical")
	TArray<FGuid> GetTacticalBattleIds() const;

	/** Builds a fog-safe, command-ready tactical HUD snapshot for Blueprint presentation. */
	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|Presentation")
	FTacticalHudSnapshot BuildTacticalHudSnapshot(FGuid BattleId, const FTacticalHudQuery& Query) const;

	/** Last accepted tactical resolution, retained after authoritative battle state is removed. */
	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|Presentation")
	FTacticalDebriefView GetLastTacticalDebrief() const { return LastTacticalDebrief; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical")
	FTacticalAttackPreview PreviewTacticalUnitAttack(
		FGuid BattleId,
		FGuid AttackerUnitId,
		FGuid TargetUnitId,
		FName WeaponItemId,
		ETacticalFireMode FireMode = ETacticalFireMode::Single) const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical")
	FTacticalAttackPreview PreviewTacticalTerrainAttack(
		FGuid BattleId,
		FGuid AttackerUnitId,
		int32 TargetX,
		int32 TargetY,
		FName WeaponItemId,
		ETacticalFireMode FireMode = ETacticalFireMode::Single,
		int32 TargetZ = 0) const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical")
	FTacticalSignalPreview PreviewTacticalSignal(
		FGuid BattleId,
		FGuid AttackerUnitId,
		FGuid TargetUnitId,
		FName ProjectorItemId = NAME_None) const;

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult CreateStrategicContact(const FCreateStrategicContactCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult DispatchCraft(const FDispatchCraftCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult WithdrawInterception(const FWithdrawInterceptionCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult ResolveInterceptionRound(const FResolveInterceptionRoundCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult ResolveBaseAssault(const FResolveBaseAssaultCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign")
	FStrategicCommandResult DeployBaseDefenseOperation(const FDeployBaseDefenseOperationCommand& Command);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign Save")
	FCampaignSaveStoreResult SaveCampaign(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign Save")
	FCampaignSaveStoreResult LoadCampaign(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Campaign Save")
	FCampaignSaveSlotListResult ListCampaignSaves() const;

	const FResolvedRuleSet& GetLoadedRules() const { return LoadedRules; }
	const TArray<FCampaignContentVersion>& GetLoadedContentVersions() const { return LoadedContentVersions; }

private:
	FString GetProjectBuildVersion() const;
	FString GetCampaignSaveDirectory() const;
	FStrategicCommandResult MakeUnavailableCommandResult() const;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content", meta = (AllowPrivateAccess = "true"))
	bool bContentReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content", meta = (AllowPrivateAccess = "true"))
	FResolvedRuleSet LoadedRules;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content", meta = (AllowPrivateAccess = "true"))
	TArray<FCampaignContentVersion> LoadedContentVersions;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content", meta = (AllowPrivateAccess = "true"))
	TArray<FContentDiagnostic> ContentDiagnostics;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content", meta = (AllowPrivateAccess = "true"))
	TArray<FString> LoadedContentFiles;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Content", meta = (AllowPrivateAccess = "true"))
	TArray<FString> LoadedContentDirectories;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign", meta = (AllowPrivateAccess = "true"))
	bool bCampaignActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign", meta = (AllowPrivateAccess = "true"))
	FCampaignState CampaignState;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign", meta = (AllowPrivateAccess = "true"))
	FGuid ActiveCampaignId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Campaign", meta = (AllowPrivateAccess = "true"))
	FDateTime CampaignCreatedUtc;

	UPROPERTY(EditDefaultsOnly, Category = "UEGT|Campaign")
	FStrategicSimulationConfig SimulationConfig;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "UEGT|Tactical|Presentation", meta = (AllowPrivateAccess = "true"))
	FTacticalDebriefView LastTacticalDebrief;
};
