#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Tactical/TacticalPresentationService.h"

#include "UEGTTacticalHudWidget.generated.h"

class STextBlock;
class SBorder;
class SVerticalBox;
class SWrapBox;

/** Native Slate/UMG tactical HUD that requires no authored widget asset to remain package-safe. */
UCLASS(BlueprintType)
class UEGTGAME_API UUEGTTacticalHudWidget final : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|HUD")
	void ApplySnapshot(const FTacticalHudSnapshot& Snapshot);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|HUD")
	void ApplyDebrief(const FTacticalDebriefView& Debrief);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|HUD")
	void ShowStatusMessage(const FString& Message, bool bIsError = false);

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|HUD")
	FTacticalHudSnapshot GetCurrentSnapshot() const { return CurrentSnapshot; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|HUD")
	FString GetRenderedMissionText() const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|HUD")
	FString GetRenderedPhaseText() const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|HUD")
	FString GetRenderedFogText() const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|HUD")
	FString GetRenderedStatusText() const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|HUD")
	FString GetRenderedHoverText() const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|HUD")
	TArray<FString> GetRenderedSectionLabels() const { return RenderedSectionLabels; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|HUD")
	TArray<FString> GetRenderedActionLabels() const { return RenderedActionLabels; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|HUD")
	TArray<FString> GetRenderedActionTooltips() const { return RenderedActionTooltips; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|HUD")
	TArray<FString> GetRenderedUnitSummaries() const { return RenderedUnitSummaries; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void RefreshSlate();
	FReply HandleActionClicked(ETacticalHudActionType ActionType);
	FReply HandleDebriefClicked();
	FReply HandleStrategicReturnClicked();
	FReply HandleUnitClicked(FGuid UnitId);
	FReply HandleObjectiveClicked(FName ObjectiveId);
	static FString ActionLabel(const FTacticalHudActionAvailability& Action);

	UPROPERTY(Transient)
	FTacticalHudSnapshot CurrentSnapshot;

	UPROPERTY(Transient)
	FTacticalDebriefView CurrentDebrief;

	FString StatusMessage;
	bool bStatusIsError = false;
	TArray<FString> RenderedSectionLabels;
	TArray<FString> RenderedActionLabels;
	TArray<FString> RenderedActionTooltips;
	TArray<FString> RenderedUnitSummaries;
	TSharedPtr<STextBlock> MissionText;
	TSharedPtr<STextBlock> PhaseText;
	TSharedPtr<STextBlock> FogText;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> HoverText;
	TSharedPtr<SBorder> HoverPanel;
	TSharedPtr<SVerticalBox> RosterBox;
	TSharedPtr<SVerticalBox> ObjectiveBox;
	TSharedPtr<SWrapBox> ActionBox;
};
