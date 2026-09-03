#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Strategic/StrategicPresentationService.h"
#include "Tactical/TacticalPresentationService.h"
#include "UEGTGameInstance.h"

#include "UEGTTacticalPlayerController.generated.h"

class AUEGTTacticalBoardActor;
class AUEGTTacticalCameraPawn;
class AUEGTStrategicGlobeActor;
class UUEGTAudioDirector;
class UUEGTStrategicHudWidget;
class UUEGTTacticalHudWidget;
struct FUEGTStrategicGlobeHit;
struct FUEGTTacticalBoardHit;

/** Transient two-activation guard keyed to the authoritative command sequence. */
struct UEGTGAME_API FUEGTEndTurnConfirmationState
{
	bool ShouldDefer(bool bConfirmationRequired, int64 CurrentSequence);
	void Reset();
	bool IsArmed() const { return bArmed; }
	int64 GetArmedSequence() const { return ArmedSequence; }

private:
	bool bArmed = false;
	int64 ArmedSequence = MIN_int64;
};

/** Mouse, keyboard, and controller adapter for the deterministic tactical command surface. */
UCLASS(BlueprintType)
class UEGTGAME_API AUEGTTacticalPlayerController final : public APlayerController
{
	GENERATED_BODY()

public:
	AUEGTTacticalPlayerController();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|Controls")
	void RefreshTacticalPresentation();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|Controls")
	void ExecuteHudAction(ETacticalHudActionType ActionType);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|Controls")
	void SelectOrTargetTacticalUnit(FGuid UnitId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|Controls")
	void TargetTacticalObjective(FName ObjectiveId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|Controls")
	void ResolveTacticalDebrief();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void ReturnToStrategicCommand();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void RefreshStrategicPresentation();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void StartStrategicCampaign(
		ECampaignDifficulty Difficulty,
		int64 Seed,
		EUEGTFundingModel FundingModel);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Content")
	void ReloadContentCatalog();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void LoadDefaultCampaign();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void SaveDefaultCampaign();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void LoadCampaignSlot(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void SaveCampaignSlot(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void EstablishStarterBase(FName RegionId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void AdvanceStrategicClock(EStrategicTimeRate Rate);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void ExecuteRegionalDiplomacy(FName RegionId, ERegionalDiplomacyActionType ActionType);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void SignRegionalCharter(FName RegionId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void RatifyHorizonCompact();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void DeployReciprocalAid(FName TargetRegionId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void RestoreHorizonCompactMember(FName RegionId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void CallHorizonCompactEmergencyVote(FName TargetRegionId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void ExecuteStrategicOption(EStrategicActionOptionType Type, FName RuleId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void StartStrategicFacilityConstruction(FName FacilityId, FGuid BaseId, int32 GridX, int32 GridY);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void DismantleStrategicFacility(FGuid BaseId, FGuid FacilityInstanceId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void RepairStrategicFacility(FGuid BaseId, FGuid FacilityInstanceId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void CancelStrategicFacilityRepair(FGuid BaseId, FGuid FacilityInstanceId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void AdjustStrategicProjectStaff(EStrategicProjectType Type, FGuid ProjectId, FName RuleId, int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void StartStrategicManufacturing(FName ItemId, int32 Units);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void AdjustStrategicManufacturingUnits(FGuid ProjectId, int32 DeltaUnits);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void CancelStrategicProject(EStrategicProjectType Type, FGuid ProjectId, FName RuleId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void SellStrategicInventory(FGuid BaseId, FName ItemId, int32 Quantity);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void DispatchStrategicMutualAidConvoy(
		FGuid SourceBaseId,
		FGuid DestinationBaseId,
		FName ItemId,
		int32 Quantity,
		EMutualAidRoutePolicy RoutePolicy,
		bool bSignalEscort);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void RetuneStrategicMutualAidConvoy(
		FGuid ConvoyId,
		EMutualAidRoutePolicy RoutePolicy);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void CommissionStrategicMutualAidSignalEscort(FGuid ConvoyId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void PrioritizeStrategicMutualAidConvoy(FGuid ConvoyId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void StandDownStrategicMutualAidConvoy(FGuid ConvoyId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void DivertStrategicMutualAidConvoy(FGuid ConvoyId, FGuid DestinationBaseId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void ConfigureStrategicMutualAidRelayWaypoint(
		FGuid ConvoyId,
		FGuid WaypointBaseId,
		EMutualAidRoutePolicy OnwardRoutePolicy);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void ConfigureStrategicMutualAidBalancedHandoff(
		FGuid ConvoyId,
		bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void AdjustStrategicSignalWatch(FGuid BaseId, int32 DeltaScientists);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void AdjustStrategicWorksCadre(FGuid BaseId, int32 DeltaEngineers);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void SetStrategicWorksCadreCharter(
		FGuid BaseId,
		EWorksCadreCharter Charter);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void BeginStrategicPersonnelTraining(FGuid PersonnelId, EPersonnelTrainingFocus Focus);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void SelectStrategicPersonnelRecoveryPlan(FGuid PersonnelId, EPersonnelRecoveryPlan Plan);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void BeginStrategicPersonnelStewardship(FGuid PersonnelId, EPersonnelStewardshipFocus Focus);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void SelectStrategicPersonnelDoctrine(FGuid PersonnelId, FName DoctrineId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void AdjustStrategicPersonnelEquipment(FGuid PersonnelId, FName ItemId, int32 Delta);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void TransferStrategicPersonnel(FGuid PersonnelId, FGuid DestinationBaseId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void DismissStrategicPersonnel(FGuid PersonnelId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void BeginPendingTacticalOperation(FGuid OperationId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void AutoPrepareCraft(FGuid CraftId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void CancelStrategicCraftService(FGuid CraftId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void RearmStrategicCraft(FGuid CraftId, ECraftRearmPolicy Policy);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void TransferStrategicCraft(FGuid CraftId, FGuid DestinationBaseId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void ResolveStrategicCraftSalvage(
		FGuid CraftId,
		FName ItemId,
		int32 Quantity,
		ECraftSalvageDisposition Disposition);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void AssignStrategicCraftPilot(FGuid CraftId, FGuid PersonnelId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void ToggleStrategicCraftAgent(FGuid CraftId, FGuid PersonnelId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void AutoEquipFieldTeam();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void DispatchReadyCraftToContact(FGuid ContactId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void ResolveContactInterception(FGuid ContactId, EInterceptionPosture Posture);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void WithdrawContactInterception(FGuid ContactId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void WithdrawContactInterceptionWithDoctrine(
		FGuid ContactId,
		EInterceptionWithdrawalDoctrine Doctrine);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void ResolveBaseAssault(FGuid AssaultId, EBaseDefenseFireDoctrine FireDoctrine);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void DeployBaseDefense(FGuid AssaultId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void DeployReadyCraftToSite(FGuid SiteId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings")
	void ApplyUserSettings();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Audio")
	void PreviewAudioCue();

	UFUNCTION(Exec, BlueprintCallable, Category = "UEGT|Settings")
	void ToggleSettings();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Controls")
	void QuitGame();

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|Controls")
	FTacticalHudSnapshot GetCurrentTacticalSnapshot() const { return CurrentSnapshot; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|Controls")
	FStrategicDashboardSnapshot GetCurrentStrategicSnapshot() const { return CurrentStrategicSnapshot; }

	/** Pure, deterministic preference helpers used by the controller and automation. */
	static int32 CountRemainingPlayerActionPoints(
		const FTacticalHudSnapshot& Snapshot,
		int32& OutReadyAgentCount);
	static FGuid FindNextReadyPlayerUnit(
		const FTacticalHudSnapshot& Snapshot,
		const FGuid& CurrentUnitId);
	static int64 CalculateManufacturingDeltaFunds(
		int64 UnitCost,
		int32 DeltaUnits);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	void UpdatePointerTarget();
	bool TraceBoard(FUEGTTacticalBoardHit& OutHit) const;
	bool TraceGlobe(FUEGTStrategicGlobeHit& OutHit) const;
	void HandlePrimaryClick();
	void HandleContextClick();
	void HandleConfirmOrContinue();
	void HandleEndTurn();
	void HandleStance();
	void HandleReload();
	void HandleObjective();
	void HandleExtract();
	void HandleDoor();
	void HandleDevice();
	void HandleTerrainAttack();
	void HandleSignal();
	void HandleNextUnit();
	void HandlePreviousUnit();
	void HandleLevelUp();
	void HandleLevelDown();
	void HandleZoomIn();
	void HandleZoomOut();
	void HandleCycleWeapon();
	void HandleToggleFireMode();
	void HandleCycleDevice();
	void HandleCursorUp();
	void HandleCursorDown();
	void HandleCursorLeft();
	void HandleCursorRight();
	void HandleStrategicTimeFour();
	void HandleStrategicTimeFive();
	void HandleStrategicTimeSix();
	void HandleToggleSettings();
	void MoveTargetCursor(int32 DeltaX, int32 DeltaY);
	void SelectRelativeUnit(int32 Direction);
	void ChangeViewedLevel(int32 Delta);
	void RunAdversaryTurnIfNeeded();
	bool DeferEndTurnForConfirmation();
	bool AutoSelectReadyTacticalUnit();
	void FocusCameraOnTacticalUnit(const FTacticalHudUnitView& Unit);
	void PresentCommandResult(const FStrategicCommandResult& Result, const FString& AcceptedMessage);
	void PresentStrategicCommandResult(const FStrategicCommandResult& Result, const FString& AcceptedMessage);
	void SetStrategicStatus(const FString& Message, bool bIsError);
	void ApplyPresentationAccessibilitySettings();
	void RebuildInputBindings();
	void BindInputKeys();
	void CaptureRuntimeScreenshot();
	void PlayCommandAudio(const FStrategicCommandResult& Result, bool bTacticalContext);
	void LogRuntimeAudioDemo();
	void PrepareRecoveryPlanDemo();
	void PrepareStewardshipDemo();
	void PrepareWorksCadreDemo(bool bWorksCharterDemo = false);
	void PrepareMutualAidConvoyDemo();
	AUEGTTacticalCameraPawn* GetTacticalCameraPawn() const;

	UPROPERTY(EditDefaultsOnly, Category = "UEGT|Tactical|Controls")
	TSubclassOf<AUEGTTacticalBoardActor> BoardActorClass;

	UPROPERTY(EditDefaultsOnly, Category = "UEGT|Tactical|Controls")
	TSubclassOf<UUEGTTacticalHudWidget> HudWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UEGT|Strategic|Controls")
	TSubclassOf<AUEGTStrategicGlobeActor> GlobeActorClass;

	UPROPERTY(EditDefaultsOnly, Category = "UEGT|Strategic|Controls")
	TSubclassOf<UUEGTStrategicHudWidget> StrategicHudWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<AUEGTTacticalBoardActor> BoardActor;

	UPROPERTY(Transient)
	TObjectPtr<UUEGTTacticalHudWidget> HudWidget;

	UPROPERTY(Transient)
	TObjectPtr<AUEGTStrategicGlobeActor> GlobeActor;

	UPROPERTY(Transient)
	TObjectPtr<UUEGTStrategicHudWidget> StrategicHudWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUEGTAudioDirector> AudioDirector;

	UPROPERTY(Transient)
	FTacticalHudSnapshot CurrentSnapshot;

	UPROPERTY(Transient)
	FStrategicDashboardSnapshot CurrentStrategicSnapshot;

	FGuid ActiveBattleId;
	FGuid SelectedUnitId;
	bool bHasHoveredCell = false;
	int32 HoveredX = 0;
	int32 HoveredY = 0;
	int32 HoveredZ = 0;
	FGuid HoveredUnitId;
	FName HoveredObjectiveId;
	FName SelectedWeaponItemId;
	FName SelectedDeviceItemId;
	ETacticalFireMode FireMode = ETacticalFireMode::Single;
	int32 ViewedLevel = 0;
	int64 LastPresentedSequence = MIN_int64;
	FGuid LastFocusedBattleId;
	FVector2D LastPointerPosition = FVector2D::ZeroVector;
	bool bHasPointerPosition = false;
	bool bStrategicMode = false;
	bool bViewingDebrief = false;
	bool bGlobeFocused = false;
	FUEGTEndTurnConfirmationState EndTurnConfirmation;
	FTimerHandle RuntimeScreenshotTimerHandle;
	FTimerHandle RuntimeAudioDemoTimerHandle;
};
