#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Strategic/StrategicPresentationService.h"
#include "UEGTGameInstance.h"

#include "UEGTStrategicHudWidget.generated.h"

class SEditableTextBox;
class SButton;
class SScrollBox;
class STextBlock;
class SVerticalBox;
class SWrapBox;
enum class EUEGTAccessibilityPreset : uint8;
enum class EUEGTInputCommand : uint8;

/** Native main-menu and strategic command HUD; no authored widget asset is required. */
UCLASS(BlueprintType)
class UEGTGAME_API UUEGTStrategicHudWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void ShowMainMenu(bool bInContentReady, const FString& InContentStatus);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void ApplySnapshot(const FStrategicDashboardSnapshot& Snapshot);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void ShowStatusMessage(const FString& Message, bool bIsError = false);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void SelectGlobeMarker(const FStrategicGlobeMarkerView& Marker);

	/** Moves the command roster to the fleet section without changing campaign state. */
	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void FocusFleetPanel();

	/** Moves the situation feed to the active-contact section without changing campaign state. */
	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void FocusContactPanel();

	/** Moves the situation feed to the urgent base-defense section without changing campaign state. */
	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void FocusBaseDefensePanel();

	/** Moves the situation feed to a withdrawn member's Emergency Solidarity Vote. */
	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void FocusCoalitionEmergencyVotePanel();

	/** Moves the command roster to personnel progression without changing campaign state. */
	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void FocusPersonnelPanel();

	/** Moves the command roster to active inter-base Mutual Aid Convoys. */
	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void FocusMutualAidPanel();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings")
	void ShowSettings();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings")
	void CloseSettings();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings")
	void ShowControlSettings();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings")
	void ShowGameplaySettings();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void ShowSaveBrowser(bool bForSaving);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void CloseSaveBrowser();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Archive")
	void ShowKnowledgeArchive();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Archive")
	void CloseKnowledgeArchive();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Archive")
	void SetKnowledgeArchiveCategoryFilter(FName CategoryId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Archive")
	void SetKnowledgeArchiveSearchText(const FString& SearchText);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Archive")
	void SelectKnowledgeArchiveRecord(FName EntryId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void SelectFacilityForPlacement(FName FacilityId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void CancelFacilityPlacement();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void SelectFacilityForDismantle(FGuid BaseId, FGuid FacilityInstanceId);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|HUD")
	void CancelFacilityDismantle();

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	FStrategicDashboardSnapshot GetCurrentSnapshot() const { return CurrentSnapshot; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	ECampaignDifficulty GetSelectedDifficulty() const { return SelectedDifficulty; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	EUEGTFundingModel GetSelectedFundingModel() const { return SelectedFundingModel; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	int32 GetRenderedFundingModelOptionCount() const { return RenderedFundingModelOptionCount; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Content")
	int32 GetRenderedContentReloadControlCount() const { return RenderedContentReloadControlCount; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	int32 GetRenderedDifficultyProfileCount() const { return RenderedDifficultyProfileCount; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	int32 GetRenderedAdversaryPlanIntelligenceCount() const
	{
		return RenderedAdversaryPlanIntelligenceCount;
	}

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	int32 GetRenderedCoalitionCounterplayCount() const
	{
		return RenderedCoalitionCounterplayCount;
	}

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	int32 GetRenderedCraftRearmControlCount() const { return RenderedCraftRearmControlCount; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	int32 GetRenderedEnabledCraftRearmControlCount() const
	{
		return RenderedEnabledCraftRearmControlCount;
	}

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	int32 GetRenderedCraftServiceControlCount() const { return RenderedCraftServiceControlCount; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	int32 GetRenderedEnabledCraftServiceControlCount() const
	{
		return RenderedEnabledCraftServiceControlCount;
	}

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Accessibility")
	int32 GetRenderedAccessibilityPresetOptionCount() const
	{
		return RenderedAccessibilityPresetOptionCount;
	}

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings")
	bool IsShowingSettings() const { return bSettingsMenu; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings")
	bool IsShowingControlSettings() const { return bSettingsMenu && bSettingsControlsPage; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings")
	bool IsShowingGameplaySettings() const { return bSettingsMenu && bSettingsGameplayPage; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	bool IsShowingSaveBrowser() const { return bSaveBrowser; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|Archive")
	bool IsShowingKnowledgeArchive() const { return bKnowledgeArchive; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|Archive")
	FName GetSelectedKnowledgeArchiveRecordId() const { return SelectedArchiveEntryId; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	bool IsPlacingFacility() const { return !PendingFacilityRuleId.IsNone(); }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	bool IsReviewingFacilityDismantle() const { return PendingDismantleFacilityInstanceId.IsValid(); }

	/** Read-only presentation probes used by native UI automation and accessibility QA. */
	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	FString GetRenderedTitleText() const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	FString GetRenderedSubtitleText() const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	FString GetRenderedStatusText() const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	TArray<FString> GetRenderedCommandActionLabels() const { return RenderedCommandActionLabels; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	TArray<FString> GetRenderedDynamicLabels() const { return RenderedDynamicLabels; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|HUD")
	TArray<FString> GetRenderedSaveBrowserLabels() const { return RenderedSaveBrowserLabels; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|Archive")
	TArray<FString> GetRenderedArchiveLabels() const { return RenderedArchiveLabels; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void RefreshSlate();
	void BuildMainMenu();
	void BuildBasePlacement();
	void BuildDashboard();
	void BuildCraftServicePanel(const FStrategicCraftView& Craft);
	void BuildPersonnelRecoveryPlanPanel(const FStrategicPersonnelView& Person);
	void BuildPersonnelStewardshipPanel(const FStrategicPersonnelView& Person);
	void AppendSignalWatchControls(const FStrategicBaseView& Base);
	void AppendRelayQueuePressure(const FStrategicBaseView& Base);
	FORCENOINLINE void AppendWorksCadreControls(const FStrategicBaseView& Base);
	void AppendMutualAidDispatchControls(
		const FStrategicBaseView& Base,
		const FStrategicInventoryView& Item);
	void AppendMutualAidConvoySummary();
	void BuildSettings();
	void BuildControlSettings();
	void BuildGameplaySettings();
	void BuildSaveBrowser();
	void BuildKnowledgeArchive();
	void ApplySettingsAndRefresh(const FString& Message);
	FReply HandleDifficultyClicked(ECampaignDifficulty Difficulty);
	FReply HandleFundingModelClicked(EUEGTFundingModel FundingModel);
	FReply HandleAccessibilityPresetClicked(EUEGTAccessibilityPreset Preset);
	FReply HandleStartCampaignClicked();
	FReply HandleReloadContentClicked();
	FReply HandleLoadClicked();
	FReply HandleSaveClicked();
	FReply HandleSaveSlotClicked(FString SlotName);
	FReply HandleLoadSlotClicked(FString SlotName);
	FReply HandleCreateSaveSlotClicked();
	FReply HandleSaveBrowserBackClicked();
	FReply HandleKnowledgeArchiveClicked();
	FReply HandleArchiveBackClicked();
	FReply HandleArchiveCategoryClicked(FName CategoryId);
	FReply HandleArchiveEntryClicked(FName EntryId);
	FReply HandleArchiveRelatedEntryClicked(FName EntryId);
	FReply HandleArchiveSearchClicked();
	FReply HandleArchiveClearSearchClicked();
	FReply HandleQuitClicked();
	FReply HandleRegionClicked(FName RegionId);
	FReply HandleRegionalDiplomacyClicked(FName RegionId, ERegionalDiplomacyActionType ActionType);
	FReply HandleRegionalCharterClicked(FName RegionId);
	FReply HandleHorizonCompactClicked();
	FReply HandleReciprocalAidClicked(FName TargetRegionId);
	FReply HandleCompactRestorationClicked(FName RegionId);
	FReply HandleCompactEmergencyVoteClicked(FName TargetRegionId);
	FReply HandleTimeClicked(EStrategicTimeRate Rate);
	FReply HandleOptionClicked(EStrategicActionOptionType Type, FName RuleId);
	FReply HandleFacilityPlacementClicked(FName FacilityId, FGuid BaseId, int32 GridX, int32 GridY);
	FReply HandleCancelFacilityPlacementClicked();
	FReply HandleFacilityDismantleReviewClicked(FGuid BaseId, FGuid FacilityInstanceId);
	FReply HandleConfirmFacilityDismantleClicked();
	FReply HandleCancelFacilityDismantleClicked();
	FReply HandleFacilityRepairClicked(FGuid BaseId, FGuid FacilityInstanceId);
	FReply HandleCancelFacilityRepairClicked(FGuid BaseId, FGuid FacilityInstanceId);
	FReply HandleManufacturingQuantityClicked(FName RuleId, int32 Delta);
	FReply HandleManufacturingOrderClicked(FName RuleId);
	FReply HandleManufacturingProjectQuantityClicked(FGuid ProjectId, int32 Delta);
	FReply HandleProjectStaffClicked(EStrategicProjectType Type, FGuid ProjectId, FName RuleId, int32 Delta);
	FReply HandleCancelProjectClicked(EStrategicProjectType Type, FGuid ProjectId, FName RuleId);
	FReply HandleSellInventoryClicked(FGuid BaseId, FName ItemId, int32 Quantity);
	FReply HandleMutualAidConvoyClicked(
		FGuid SourceBaseId,
		FGuid DestinationBaseId,
		FName ItemId,
		int32 Quantity,
		EMutualAidRoutePolicy RoutePolicy,
		bool bSignalEscort);
	FReply HandleMutualAidRetuneClicked(
		FGuid ConvoyId,
		EMutualAidRoutePolicy RoutePolicy);
	FReply HandleMutualAidSignalEscortCommissionClicked(FGuid ConvoyId);
	FReply HandleMutualAidReliefPriorityClicked(FGuid ConvoyId);
	FReply HandleMutualAidReliefStandDownClicked(FGuid ConvoyId);
	FReply HandleMutualAidReliefDiversionClicked(
		FGuid ConvoyId,
		FGuid DestinationBaseId);
	FReply HandleMutualAidRelayWaypointClicked(
		FGuid ConvoyId,
		FGuid WaypointBaseId,
		EMutualAidRoutePolicy OnwardRoutePolicy);
	FReply HandleMutualAidBalancedHandoffClicked(
		FGuid ConvoyId,
		bool bEnabled);
	FReply HandleMutualAidRoutePolicyClicked(EMutualAidRoutePolicy RoutePolicy);
	FReply HandleMutualAidSignalEscortClicked();
	FReply HandleSignalWatchStaffClicked(FGuid BaseId, int32 DeltaScientists);
	FReply HandleWorksCadreStaffClicked(FGuid BaseId, int32 DeltaEngineers);
	FReply HandleWorksCadreCharterClicked(
		FGuid BaseId,
		EWorksCadreCharter Charter);
	FReply HandleTrainPersonnelClicked(FGuid PersonnelId, EPersonnelTrainingFocus Focus);
	FReply HandlePersonnelRecoveryPlanClicked(FGuid PersonnelId, EPersonnelRecoveryPlan Plan);
	FReply HandlePersonnelStewardshipClicked(FGuid PersonnelId, EPersonnelStewardshipFocus Focus);
	FReply HandlePersonnelDoctrineClicked(FGuid PersonnelId, FName DoctrineId);
	FReply HandlePersonnelEquipmentClicked(FGuid PersonnelId, FName ItemId, int32 Delta);
	FReply HandleTransferPersonnelClicked(FGuid PersonnelId, FGuid DestinationBaseId);
	FReply HandleDismissPersonnelClicked(FGuid PersonnelId);
	FReply HandleOperationClicked(FGuid OperationId);
	FReply HandlePrepareCraftClicked(FGuid CraftId);
	FReply HandleCancelCraftServiceClicked(FGuid CraftId);
	FReply HandleCraftRearmClicked(FGuid CraftId, ECraftRearmPolicy Policy);
	FReply HandleTransferCraftClicked(FGuid CraftId, FGuid DestinationBaseId);
	FReply HandleCraftSalvageClicked(
		FGuid CraftId,
		FName ItemId,
		int32 Quantity,
		ECraftSalvageDisposition Disposition);
	FReply HandleCraftPilotClicked(FGuid CraftId, FGuid PersonnelId);
	FReply HandleCraftAgentClicked(FGuid CraftId, FGuid PersonnelId);
	FReply HandleEquipFieldTeamClicked();
	FReply HandleDispatchContactClicked(FGuid ContactId);
	FReply HandleResolveInterceptionClicked(FGuid ContactId, EInterceptionPosture Posture);
	FReply HandleWithdrawInterceptionClicked(
		FGuid ContactId,
		EInterceptionWithdrawalDoctrine Doctrine);
	FReply HandleResolveBaseAssaultClicked(FGuid AssaultId, EBaseDefenseFireDoctrine FireDoctrine);
	FReply HandleDeployBaseDefenseClicked(FGuid AssaultId);
	FReply HandleDeploySiteClicked(FGuid SiteId);
	FReply HandleSettingsClicked();
	FReply HandleSettingsBackClicked();
	FReply HandleUIScaleClicked();
	FReply HandleReducedMotionClicked();
	FReply HandleHighContrastClicked();
	FReply HandleColorVisionClicked();
	FReply HandleCameraSpeedClicked();
	FReply HandleMasterVolumeClicked();
	FReply HandleMuteWhenUnfocusedClicked();
	FReply HandleAudioPreviewClicked();
	FReply HandleInterfaceCultureClicked();
	FReply HandleControlSettingsClicked();
	FReply HandleGameplaySettingsClicked();
	FReply HandleGeneralSettingsClicked();
	FReply HandleEndTurnSafetyClicked();
	FReply HandleAutoSelectReadyAgentClicked();
	FReply HandleCenterCameraOnSelectionClicked();
	FReply HandleInputBindingClicked(EUEGTInputCommand Command);
	FReply HandleResetInputBindingsClicked();
	FReply HandleVSyncClicked();
	FReply HandleFrameLimitClicked();
	FReply HandleQualityClicked();
	FReply HandleResetSettingsClicked();

	UPROPERTY(Transient)
	FStrategicDashboardSnapshot CurrentSnapshot;

	UPROPERTY(Transient)
	FStrategicGlobeMarkerView SelectedMarker;

	bool bMainMenu = true;
	bool bSettingsMenu = false;
	bool bSettingsControlsPage = false;
	bool bSettingsGameplayPage = false;
	bool bSaveBrowser = false;
	bool bSaveBrowserForSaving = false;
	bool bKnowledgeArchive = false;
	bool bContentReady = false;
	bool bHasSelectedMarker = false;
	bool bStatusIsError = false;
	int32 RenderedDifficultyProfileCount = 0;
	int32 RenderedFundingModelOptionCount = 0;
	int32 RenderedAccessibilityPresetOptionCount = 0;
	int32 RenderedAdversaryPlanIntelligenceCount = 0;
	int32 RenderedCoalitionCounterplayCount = 0;
	int32 RenderedContentReloadControlCount = 0;
	int32 RenderedCraftRearmControlCount = 0;
	int32 RenderedEnabledCraftRearmControlCount = 0;
	int32 RenderedCraftServiceControlCount = 0;
	int32 RenderedEnabledCraftServiceControlCount = 0;
	ECampaignDifficulty SelectedDifficulty = ECampaignDifficulty::Cadet;
	EUEGTFundingModel SelectedFundingModel = EUEGTFundingModel::BalancedMandate;
	EMutualAidRoutePolicy SelectedMutualAidRoutePolicy = EMutualAidRoutePolicy::OpenRelay;
	bool bSelectedMutualAidSignalEscort = false;
	FString ContentStatus;
	FString StatusMessage;
	FString SeedText = TEXT("20350101");
	FString SaveSlotText = TEXT("Campaign1");
	FString ArchiveSearchText;
	FString PendingOverwriteSlot;
	FName SelectedArchiveCategoryId;
	FName SelectedArchiveEntryId;
	FGuid PendingDismissPersonnelId;
	FGuid PendingDismantleBaseId;
	FGuid PendingDismantleFacilityInstanceId;
	FName PendingFacilityRuleId;
	TMap<FName, int32> ManufacturingQuantities;
	TArray<FString> RenderedCommandActionLabels;
	TArray<FString> RenderedDynamicLabels;
	TArray<FString> RenderedSaveBrowserLabels;
	TArray<FString> RenderedArchiveLabels;
	TSharedPtr<SScrollBox> LeftScrollBox;
	TSharedPtr<SScrollBox> RightScrollBox;
	TSharedPtr<STextBlock> PersonnelPanelAnchor;
	TSharedPtr<STextBlock> MutualAidPanelAnchor;
	TSharedPtr<STextBlock> FleetPanelAnchor;
	TSharedPtr<STextBlock> ContactPanelAnchor;
	TSharedPtr<STextBlock> BaseDefensePanelAnchor;
	TSharedPtr<SButton> CoalitionEmergencyVotePanelAnchor;

	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> SubtitleText;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> MarkerText;
	TSharedPtr<SVerticalBox> LeftBox;
	TSharedPtr<SVerticalBox> RightBox;
	TSharedPtr<SWrapBox> ActionBox;
	TSharedPtr<SEditableTextBox> SeedTextBox;
	TSharedPtr<SEditableTextBox> SaveSlotTextBox;
	TSharedPtr<SEditableTextBox> ArchiveSearchTextBox;
};
