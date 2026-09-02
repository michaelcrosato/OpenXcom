// Copyright 2026 UEGT contributors. MIT License.

#include "Tactical/UEGTTacticalPlayerController.h"

#include "Audio/UEGTAudioDirector.h"
#include "UEGTGameInstance.h"
#include "UEGTUserSettings.h"
#include "Localization/UEGTLocalizationService.h"
#include "Strategic/PersonnelLegacyRelay.h"
#include "Strategic/PersonnelRecoveryPlan.h"
#include "Strategic/PersonnelSquadBond.h"
#include "Strategic/UEGTStrategicGlobeActor.h"
#include "Strategic/UEGTStrategicHudWidget.h"
#include "Tactical/UEGTTacticalBoardActor.h"
#include "Tactical/UEGTTacticalCameraPawn.h"
#include "Tactical/UEGTTacticalHudWidget.h"

#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace UEGTTacticalControllerPrivate
{
	FString Localized(const TCHAR* Key, const TCHAR* EnglishFallback)
	{
		return FUEGTLocalizationService::Text(Key, EnglishFallback);
	}

	FString LocalizedFormat(
		const TCHAR* Key,
		const TCHAR* EnglishFallback,
		const FStringFormatOrderedArguments& Arguments)
	{
		return FString::Format(*Localized(Key, EnglishFallback), Arguments);
	}

	FString CompactMinutesSeconds(const int64 Seconds)
	{
		const int64 ClampedSeconds = FMath::Max<int64>(0, Seconds);
		return FString::Printf(
			TEXT("%lld:%02lld"),
			ClampedSeconds / 60,
			ClampedSeconds % 60);
	}

	int64 CeilHours(const int64 Seconds)
	{
		return Seconds <= 0 ? 0 : Seconds / 3600 + (Seconds % 3600 == 0 ? 0 : 1);
	}

	FString LocalizedDiagnostic(const FName Code, const FString& EnglishFallback)
	{
		return FUEGTLocalizationService::DiagnosticText(Code, EnglishFallback);
	}

	FString LocalizedInterceptionPosture(const EInterceptionPosture Posture)
	{
		switch (Posture)
		{
		case EInterceptionPosture::StandOffScreen:
			return Localized(TEXT("strategic.interception-posture-stand-off"), TEXT("STAND-OFF SCREEN"));
		case EInterceptionPosture::BalancedVector:
			return Localized(TEXT("strategic.interception-posture-balanced"), TEXT("BALANCED VECTOR"));
		case EInterceptionPosture::CloseAssault:
			return Localized(TEXT("strategic.interception-posture-close"), TEXT("CLOSE ASSAULT"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString LocalizedMutualAidRoute(const EMutualAidRoutePolicy Policy)
	{
		switch (Policy)
		{
		case EMutualAidRoutePolicy::OpenRelay:
			return Localized(TEXT("strategic.mutual-aid-route-open-relay"), TEXT("OPEN RELAY"));
		case EMutualAidRoutePolicy::RapidThread:
			return Localized(TEXT("strategic.mutual-aid-route-rapid-thread"), TEXT("RAPID THREAD"));
		case EMutualAidRoutePolicy::VeiledChain:
			return Localized(TEXT("strategic.mutual-aid-route-veiled-chain"), TEXT("VEILED CHAIN"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString LocalizedWorksCadreCharter(const EWorksCadreCharter Charter)
	{
		switch (Charter)
		{
		case EWorksCadreCharter::CommonCadence:
			return Localized(
				TEXT("strategic.works-charter-common-cadence"),
				TEXT("COMMON CADENCE"));
		case EWorksCadreCharter::AssemblyCadence:
			return Localized(
				TEXT("strategic.works-charter-assembly-cadence"),
				TEXT("ASSEMBLY CADENCE"));
		case EWorksCadreCharter::RestorationCadence:
			return Localized(
				TEXT("strategic.works-charter-restoration-cadence"),
				TEXT("RESTORATION CADENCE"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString LocalizedManufacturingMaterialStockSummary(
		const TArray<FStrategicMaterialRequirementView>& Requirements,
		const int32 Units)
	{
		TArray<FString> Parts;
		Parts.Reserve(Requirements.Num());
		for (const FStrategicMaterialRequirementView& Requirement : Requirements)
		{
			Parts.Add(LocalizedFormat(
				TEXT("strategic.manufacturing-material-stock-format"),
				TEXT("{0} {1} ({2} stock)"),
				{
					LexToString(static_cast<int64>(Requirement.PerUnitQuantity) * Units),
					FUEGTLocalizationService::ContentName(Requirement.ItemId, Requirement.DisplayName),
					FString::FromInt(Requirement.AvailableQuantity)
				}));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	FString LocalizedManufacturingUnavailableReason(
		const FStrategicActionOptionView& Option,
		const FStrategicDashboardSnapshot& Snapshot,
		const int32 Units)
	{
		if (Option.UnavailableReasonCode == FName(TEXT("manufacturing_facility_missing")))
		{
			return Localized(
				TEXT("strategic.manufacturing-facility-missing"),
				TEXT("An operational fabrication facility is required."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("manufacturing_materials_missing")))
		{
			return LocalizedFormat(
				TEXT("strategic.manufacturing-materials-missing-format"),
				TEXT("Required production materials are unavailable: {0}."),
				{ LocalizedManufacturingMaterialStockSummary(Option.MaterialRequirements, FMath::Max(Units, 1)) });
		}
		if (Option.UnavailableReasonCode == FName(TEXT("storage_capacity_exceeded")))
		{
			const FStrategicBaseView* Base = Snapshot.Bases.FindByPredicate(
				[&Snapshot](const FStrategicBaseView& View) { return View.BaseId == Snapshot.PrimaryBaseId; });
			if (Base != nullptr && Base->StorageOverflow > 0)
			{
				return LocalizedFormat(
					TEXT("strategic.storage-change-overflow"),
					TEXT("Resolve the existing storage overflow ({0} units) before making this change."),
					{ LexToString(Base->StorageOverflow) });
			}
			const int64 SafeUnits = FMath::Max(Units, 1);
			const int64 BatchDelta = Option.StorageDeltaPerUnit > 0
				&& Option.StorageDeltaPerUnit > MAX_int64 / SafeUnits
				? MAX_int64
				: Option.StorageDeltaPerUnit * SafeUnits;
			const int64 Available = Base != nullptr ? FMath::Max<int64>(0, Base->StorageAvailable) : 0;
			return LocalizedFormat(
				TEXT("strategic.storage-free-required-format"),
				TEXT("Free {0} storage units before making this change."),
				{ LexToString(FMath::Max<int64>(1, BatchDelta - Available)) });
		}
		if (Option.UnavailableReasonCode == FName(TEXT("insufficient_funds")))
		{
			return Localized(
				TEXT("strategic.insufficient-funds"),
				TEXT("Current funds do not cover this order."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("research_required")))
		{
			return Localized(TEXT("strategic.research-required"), TEXT("Required research is incomplete."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("base_required")))
		{
			return Localized(
				TEXT("strategic.base-required"),
				TEXT("Establish a base before ordering this program."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("campaign_concluded")))
		{
			return Localized(TEXT("strategic.campaign-concluded"), TEXT("The campaign has concluded."));
		}
		const FString Fallback = Option.UnavailableReason.IsEmpty()
			? Localized(
				TEXT("strategic.manufacturing-batch-unavailable"),
				TEXT("The selected manufacturing batch is not currently available."))
			: Option.UnavailableReason;
		return LocalizedDiagnostic(Option.UnavailableReasonCode, Fallback);
	}

	FString LocalizedCraftUnavailableReason(const FStrategicActionOptionView& Option)
	{
		if (Option.UnavailableReasonCode == FName(TEXT("craft_capacity_full")))
		{
			return Localized(
				TEXT("strategic.craft-berth-full"),
				TEXT("The primary base has no open craft berth."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("insufficient_funds")))
		{
			return Localized(
				TEXT("strategic.insufficient-funds"),
				TEXT("Current funds do not cover this order."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("research_required")))
		{
			return Localized(TEXT("strategic.research-required"), TEXT("Required research is incomplete."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("base_required")))
		{
			return Localized(
				TEXT("strategic.base-required"),
				TEXT("Establish a base before ordering this program."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("campaign_concluded")))
		{
			return Localized(TEXT("strategic.campaign-concluded"), TEXT("The campaign has concluded."));
		}
		const FString Fallback = Option.UnavailableReason.IsEmpty()
			? Localized(
				TEXT("strategic.craft-order-unavailable"),
				TEXT("The selected craft order is not currently available."))
			: Option.UnavailableReason;
		return LocalizedDiagnostic(Option.UnavailableReasonCode, Fallback);
	}
}

AUEGTTacticalPlayerController::AUEGTTacticalPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	BoardActorClass = AUEGTTacticalBoardActor::StaticClass();
	HudWidgetClass = UUEGTTacticalHudWidget::StaticClass();
	GlobeActorClass = AUEGTStrategicGlobeActor::StaticClass();
	StrategicHudWidgetClass = UUEGTStrategicHudWidget::StaticClass();
}

int32 AUEGTTacticalPlayerController::CountRemainingPlayerActionPoints(
	const FTacticalHudSnapshot& Snapshot,
	int32& OutReadyAgentCount)
{
	OutReadyAgentCount = 0;
	int32 RemainingActionPoints = 0;
	for (const FTacticalHudUnitView& Unit : Snapshot.Units)
	{
		if (Unit.Team != ETacticalTeam::Player || Unit.bIncapacitated || Unit.bExtracted
			|| Unit.RemainingActionPoints <= 0)
		{
			continue;
		}
		++OutReadyAgentCount;
		RemainingActionPoints = Unit.RemainingActionPoints > MAX_int32 - RemainingActionPoints
			? MAX_int32
			: RemainingActionPoints + Unit.RemainingActionPoints;
	}
	return RemainingActionPoints;
}

FGuid AUEGTTacticalPlayerController::FindNextReadyPlayerUnit(
	const FTacticalHudSnapshot& Snapshot,
	const FGuid& CurrentUnitId)
{
	if (Snapshot.Units.IsEmpty())
	{
		return FGuid();
	}
	const int32 CurrentIndex = Snapshot.Units.IndexOfByPredicate(
		[&CurrentUnitId](const FTacticalHudUnitView& Unit) { return Unit.UnitId == CurrentUnitId; });
	const int32 StartIndex = CurrentIndex == INDEX_NONE ? 0 : CurrentIndex + 1;
	for (int32 Offset = 0; Offset < Snapshot.Units.Num(); ++Offset)
	{
		const FTacticalHudUnitView& Unit = Snapshot.Units[(StartIndex + Offset) % Snapshot.Units.Num()];
		if (Unit.Team == ETacticalTeam::Player && !Unit.bIncapacitated && !Unit.bExtracted
			&& Unit.RemainingActionPoints > 0)
		{
			return Unit.UnitId;
		}
	}
	return FGuid();
}

bool FUEGTEndTurnConfirmationState::ShouldDefer(
	const bool bConfirmationRequired,
	const int64 CurrentSequence)
{
	if (!bConfirmationRequired)
	{
		Reset();
		return false;
	}
	if (bArmed && ArmedSequence == CurrentSequence)
	{
		Reset();
		return false;
	}
	bArmed = true;
	ArmedSequence = CurrentSequence;
	return true;
}

void FUEGTEndTurnConfirmationState::Reset()
{
	bArmed = false;
	ArmedSequence = MIN_int64;
}

void AUEGTTacticalPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	if (GetWorld() != nullptr && BoardActorClass != nullptr)
	{
		BoardActor = GetWorld()->SpawnActor<AUEGTTacticalBoardActor>(
			BoardActorClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator);
	}
	if (GetWorld() != nullptr && GlobeActorClass != nullptr)
	{
		GlobeActor = GetWorld()->SpawnActor<AUEGTStrategicGlobeActor>(
			GlobeActorClass,
			FVector::ZeroVector,
			FRotator::ZeroRotator);
	}
	if (HudWidgetClass != nullptr)
	{
		HudWidget = CreateWidget<UUEGTTacticalHudWidget>(this, HudWidgetClass);
		if (HudWidget != nullptr)
		{
			HudWidget->AddToViewport(20);
		}
	}
	if (StrategicHudWidgetClass != nullptr)
	{
		StrategicHudWidget = CreateWidget<UUEGTStrategicHudWidget>(this, StrategicHudWidgetClass);
		if (StrategicHudWidget != nullptr)
		{
			StrategicHudWidget->AddToViewport(19);
		}
	}
	AudioDirector = NewObject<UUEGTAudioDirector>(this);
	if (AudioDirector != nullptr)
	{
		AudioDirector->Initialize(GetWorld());
	}
	ApplyPresentationAccessibilitySettings();
	RefreshTacticalPresentation();
#if !UE_BUILD_SHIPPING
	const bool bHistoricalFogDemo = FParse::Param(FCommandLine::Get(), TEXT("UEGTHistoricalFogDemo"));
	const bool bBaseDefenseSupplyDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTBaseDefenseSupplyDemo"));
	const bool bBaseDefenseDemo = bHistoricalFogDemo
		|| FParse::Param(FCommandLine::Get(), TEXT("UEGTBaseDefenseDemo"))
		|| bBaseDefenseSupplyDemo;
	const bool bSalvageDispositionDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTSalvageDispositionDemo"));
	const bool bPartialRearmDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTPartialRearmDemo"));
	const bool bCraftServiceDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTCraftServiceDemo"));
	const bool bServiceHistoryDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTServiceHistoryDemo"));
	const bool bWatchkeeperDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTWatchkeeperDemo"));
	const bool bLegacyRelayDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTLegacyRelayDemo"));
	const bool bSquadBondDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTSquadBondDemo"));
	const bool bRecoveryPlanDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTRecoveryPlanDemo"));
	const bool bStewardshipDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTStewardshipDemo"));
	const bool bWorksCadreDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTWorksCadreDemo"));
	const bool bWorksCharterDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTWorksCharterDemo"));
	const bool bSignalWatchDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTSignalWatchDemo"));
	const bool bThreadlineRetuneDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTThreadlineRetuneDemo"));
	const bool bSignalSuretyDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTSignalSuretyDemo"));
	const bool bReliefPriorityDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTReliefPriorityDemo"));
	const bool bReliefStandDownDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTReliefStandDownDemo"));
	const bool bReliefDiversionDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTReliefDiversionDemo"));
	const bool bRelayWaypointDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTRelayWaypointDemo"));
	const bool bBalancedHandoffDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTBalancedHandoffDemo"));
	const bool bRelayWeaveDemo = bSignalWatchDemo || bThreadlineRetuneDemo
		|| bSignalSuretyDemo || bReliefPriorityDemo || bReliefStandDownDemo
		|| bReliefDiversionDemo || bRelayWaypointDemo || bBalancedHandoffDemo
		|| FParse::Param(FCommandLine::Get(), TEXT("UEGTRelayWeaveDemo"));
	const bool bThreadlineDemo = bRelayWeaveDemo
		|| FParse::Param(FCommandLine::Get(), TEXT("UEGTThreadlineDemo"));
	const bool bMutualAidDemo = bThreadlineDemo
		|| FParse::Param(FCommandLine::Get(), TEXT("UEGTMutualAidDemo"));
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTDismantleDemo")))
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 20350101, EUEGTFundingModel::BalancedMandate);
		if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
		{
			FEstablishBaseCommand Command;
			Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			Command.BaseId = FGuid::NewGuid();
			Command.Name = TEXT("Cascadia Command");
			Command.RegionId = TEXT("region.cascadia");
			Command.LongitudeMilliDegrees = -123120;
			Command.LatitudeMilliDegrees = 49280;
			Command.StartingFacilities = {
				TEXT("facility.operations-hub"),
				TEXT("facility.secure-storage")
			};
			PresentStrategicCommandResult(
				Instance->EstablishBase(Command),
				TEXT("Dismantling review fixture established through the strategic domain."));
			if (StrategicHudWidget != nullptr && !CurrentStrategicSnapshot.Bases.IsEmpty())
			{
				const FStrategicFacilityView* Storage =
					CurrentStrategicSnapshot.Bases[0].FacilityLayout.FindByPredicate(
						[](const FStrategicFacilityView& Facility)
						{
							return Facility.bOperational
								&& Facility.FacilityId == FName(TEXT("facility.secure-storage"));
						});
				if (Storage != nullptr)
				{
					StrategicHudWidget->SelectFacilityForDismantle(
						CurrentStrategicSnapshot.Bases[0].BaseId,
						Storage->FacilityInstanceId);
				}
			}
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTFacilityRepairDemo")))
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 20350830, EUEGTFundingModel::BalancedMandate);
		if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
		{
			FEstablishBaseCommand Command;
			Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			Command.BaseId = FGuid::NewGuid();
			Command.Name = TEXT("Cascadia Repair Command");
			Command.RegionId = TEXT("region.cascadia");
			Command.LongitudeMilliDegrees = -123120;
			Command.LatitudeMilliDegrees = 49280;
			Command.StartingFacilities = {
				TEXT("facility.operations-hub"),
				TEXT("facility.fabrication-bay"),
				TEXT("facility.secure-storage")
			};
			PresentStrategicCommandResult(
				Instance->EstablishBase(Command),
				TEXT("Facility integrity review fixture established through the strategic domain."));
			if (!CurrentStrategicSnapshot.Bases.IsEmpty())
			{
				const FStrategicBaseView& Base = CurrentStrategicSnapshot.Bases[0];
				const FGuid DemoBaseId = Base.BaseId;
				const FStrategicFacilityView* Fabrication = Base.FacilityLayout.FindByPredicate(
					[](const FStrategicFacilityView& Facility)
					{
						return Facility.FacilityId == FName(TEXT("facility.fabrication-bay"));
					});
				const FStrategicFacilityView* Storage = Base.FacilityLayout.FindByPredicate(
					[](const FStrategicFacilityView& Facility)
					{
						return Facility.FacilityId == FName(TEXT("facility.secure-storage"));
					});
				const FGuid FabricationId = Fabrication != nullptr
					? Fabrication->FacilityInstanceId : FGuid();
				const FGuid StorageId = Storage != nullptr ? Storage->FacilityInstanceId : FGuid();
				if (FabricationId.IsValid())
				{
					FApplyFacilityDamageCommand Damage;
					Damage.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
					Damage.BaseId = DemoBaseId;
					Damage.FacilityInstanceId = FabricationId;
					Damage.Damage = 120;
					PresentStrategicCommandResult(
						Instance->ApplyFacilityDamage(Damage),
						TEXT("Fabrication Bay integrity reduced for review."));
				}
				if (StorageId.IsValid())
				{
					FApplyFacilityDamageCommand Damage;
					Damage.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
					Damage.BaseId = DemoBaseId;
					Damage.FacilityInstanceId = StorageId;
					Damage.Damage = 250;
					PresentStrategicCommandResult(
						Instance->ApplyFacilityDamage(Damage),
						TEXT("Secure Storage disabled for review."));
					RepairStrategicFacility(DemoBaseId, StorageId);
				}
			}
		}
	}
	if (bBaseDefenseDemo)
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 20350831, EUEGTFundingModel::RapidMobilization);
		if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
		{
			FEstablishBaseCommand Command;
			Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			Command.BaseId = FGuid(0x0a11ce01, 0x0a11ce02, 0x0a11ce03, 0x0a11ce04);
			Command.Name = TEXT("Cascadia Shield Command");
			Command.RegionId = TEXT("region.cascadia");
			Command.LongitudeMilliDegrees = -123120;
			Command.LatitudeMilliDegrees = 49280;
			Command.StartingFacilities = {
				TEXT("facility.operations-hub"),
				TEXT("facility.fabrication-bay"),
				TEXT("facility.parallax-interceptor"),
				TEXT("facility.aegis-battery")
			};
			PresentStrategicCommandResult(
				Instance->EstablishBase(Command),
				TEXT("Base-defense review fixture established through the strategic domain."));
			for (int32 DefenderIndex = 0; DefenderIndex < 3; ++DefenderIndex)
			{
				FRecruitPersonnelCommand Recruit;
				Recruit.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
				Recruit.OrderId = FGuid(0x0a11ce10 + DefenderIndex, 0x0a11ce20, 0x0a11ce30, 0x0a11ce40);
				Recruit.PersonnelId = FGuid(0x0a11ce50 + DefenderIndex, 0x0a11ce60, 0x0a11ce70, 0x0a11ce80);
				Recruit.BaseId = Command.BaseId;
				Recruit.RoleId = TEXT("role.field-agent");
				Recruit.DisplayName = FString::Printf(TEXT("Aegis Defender %02d"), DefenderIndex + 1);
				PresentStrategicCommandResult(
					Instance->RecruitPersonnel(Recruit),
					FString::Printf(TEXT("%s recruitment ordered for the defense review."), *Recruit.DisplayName));
			}
			FRecruitPersonnelCommand EngineerRecruit;
			EngineerRecruit.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			EngineerRecruit.OrderId = FGuid(0x0a11ce90, 0x0a11ce91, 0x0a11ce92, 0x0a11ce93);
			EngineerRecruit.PersonnelId = FGuid(0x0a11cea0, 0x0a11cea1, 0x0a11cea2, 0x0a11cea3);
			EngineerRecruit.BaseId = Command.BaseId;
			EngineerRecruit.RoleId = TEXT("role.engineer");
			EngineerRecruit.DisplayName = TEXT("Capacitor Engineer 01");
			PresentStrategicCommandResult(
				Instance->RecruitPersonnel(EngineerRecruit),
				TEXT("Capacitor engineer recruitment ordered for the defense review."));
			for (int32 Attempt = 0;
				Attempt < 16 && CurrentStrategicSnapshot.Personnel.Num() < 4
					&& CurrentStrategicSnapshot.Outcome == ECampaignOutcome::Ongoing;
				++Attempt)
			{
				if (!CurrentStrategicSnapshot.BaseAssaults.IsEmpty())
				{
					ResolveBaseAssault(CurrentStrategicSnapshot.BaseAssaults[0].AssaultId,
						EBaseDefenseFireDoctrine::CoordinatedLine);
				}
				AdvanceStrategicClock(EStrategicTimeRate::OneDay);
			}
			if (!CurrentStrategicSnapshot.BaseAssaults.IsEmpty())
			{
				ResolveBaseAssault(CurrentStrategicSnapshot.BaseAssaults[0].AssaultId,
					EBaseDefenseFireDoctrine::CoordinatedLine);
			}
			const FGuid SupplyProjectId(0x0a11ceb0, 0x0a11ceb1, 0x0a11ceb2, 0x0a11ceb3);
			FStartManufacturingCommand ManufactureSupply;
			ManufactureSupply.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			ManufactureSupply.ProjectId = SupplyProjectId;
			ManufactureSupply.BaseId = Command.BaseId;
			ManufactureSupply.ItemId = TEXT("item.perimeter-capacitor");
			ManufactureSupply.Units = 2;
			const FStrategicCommandResult SupplyStarted = Instance->StartManufacturing(ManufactureSupply);
			PresentStrategicCommandResult(
				SupplyStarted, TEXT("Two perimeter capacitors entered authoritative production."));
			if (SupplyStarted.bAccepted)
			{
				FSetManufacturingStaffCommand StaffSupply;
				StaffSupply.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
				StaffSupply.ProjectId = SupplyProjectId;
				StaffSupply.AssignedEngineers = 1;
				PresentStrategicCommandResult(
					Instance->SetManufacturingStaff(StaffSupply),
					TEXT("One engineer assigned to perimeter-capacitor production."));
			}
			for (int32 Attempt = 0; Attempt < 8
				&& CurrentStrategicSnapshot.Outcome == ECampaignOutcome::Ongoing; ++Attempt)
			{
				const FStrategicBaseView* DemoBase = CurrentStrategicSnapshot.Bases.FindByPredicate(
					[&Command](const FStrategicBaseView& Base) { return Base.BaseId == Command.BaseId; });
				const bool bSupplyReady = DemoBase != nullptr
					&& DemoBase->Inventory.ContainsByPredicate([](const FStrategicInventoryView& Stack)
					{
						return Stack.ItemId == FName(TEXT("item.perimeter-capacitor"))
							&& Stack.Quantity >= 2;
					});
				if (bSupplyReady)
				{
					break;
				}
				if (!CurrentStrategicSnapshot.BaseAssaults.IsEmpty())
				{
					ResolveBaseAssault(CurrentStrategicSnapshot.BaseAssaults[0].AssaultId,
						EBaseDefenseFireDoctrine::CoordinatedLine);
				}
				AdvanceStrategicClock(EStrategicTimeRate::OneDay);
			}
			for (int32 Attempt = 0;
				Attempt < 128 && CurrentStrategicSnapshot.BaseAssaults.IsEmpty()
					&& CurrentStrategicSnapshot.Outcome == ECampaignOutcome::Ongoing;
				++Attempt)
			{
				AdvanceStrategicClock(EStrategicTimeRate::OneDay);
			}
			if (bBaseDefenseSupplyDemo)
			{
				const FStrategicBaseAssaultView* SupplyAssault =
					CurrentStrategicSnapshot.BaseAssaults.IsEmpty()
						? nullptr : &CurrentStrategicSnapshot.BaseAssaults[0];
				const FStrategicBaseDefenseSupplyView* Supply = SupplyAssault != nullptr
					&& !SupplyAssault->DefenseSupplies.IsEmpty()
						? &SupplyAssault->DefenseSupplies[0] : nullptr;
				const FStrategicBaseDefenseDoctrineView* Precision = SupplyAssault != nullptr
					? SupplyAssault->FireDoctrines.FindByPredicate(
						[](const FStrategicBaseDefenseDoctrineView& Option)
						{
							return Option.Doctrine == EBaseDefenseFireDoctrine::PrecisionScreen;
						})
					: nullptr;
				const FStrategicBaseDefenseDoctrineView* Breach = SupplyAssault != nullptr
					? SupplyAssault->FireDoctrines.FindByPredicate(
						[](const FStrategicBaseDefenseDoctrineView& Option)
						{
							return Option.Doctrine == EBaseDefenseFireDoctrine::BreachBreaker;
						})
					: nullptr;
				const FStrategicBaseDefenseDoctrineView* Overcharge = SupplyAssault != nullptr
					? SupplyAssault->FireDoctrines.FindByPredicate(
						[](const FStrategicBaseDefenseDoctrineView& Option)
						{
							return Option.Doctrine == EBaseDefenseFireDoctrine::GridOvercharge;
						})
					: nullptr;
				UE_LOG(LogTemp, Display,
					TEXT("UEGTBaseDefenseSupplyDemo final state: source=authoritative-domain assault=%s doctrines=%d operational=%d ready=%d supply=%s available=%d required=%d allocated=%d maximum=%d expected=%d precision_ready=%d precision_maximum=%d breach_ready=%d breach_maximum=%d overcharge_ready=%d overcharge_maximum=%d overcharge_expected=%d overcharge_cost=%lld overcharge_affordable=%s sequence=%lld"),
					SupplyAssault != nullptr ? TEXT("true") : TEXT("false"),
					SupplyAssault != nullptr ? SupplyAssault->FireDoctrines.Num() : 0,
					SupplyAssault != nullptr ? SupplyAssault->DefenseBatteryCount : 0,
					SupplyAssault != nullptr ? SupplyAssault->ReadyDefenseBatteryCount : 0,
					Supply != nullptr ? *Supply->ItemId.ToString() : TEXT("none"),
					Supply != nullptr ? Supply->AvailableQuantity : 0,
					Supply != nullptr ? Supply->RequiredQuantity : 0,
					Supply != nullptr ? Supply->AllocatedQuantity : 0,
					SupplyAssault != nullptr ? SupplyAssault->MaximumDefenseDamage : 0,
					SupplyAssault != nullptr ? SupplyAssault->ExpectedDefenseDamage : 0,
					Precision != nullptr ? Precision->ReadyDefenseBatteryCount : 0,
					Precision != nullptr ? Precision->MaximumDefenseDamage : 0,
					Breach != nullptr ? Breach->ReadyDefenseBatteryCount : 0,
					Breach != nullptr ? Breach->MaximumDefenseDamage : 0,
					Overcharge != nullptr ? Overcharge->ReadyDefenseBatteryCount : 0,
					Overcharge != nullptr ? Overcharge->MaximumDefenseDamage : 0,
					Overcharge != nullptr ? Overcharge->ExpectedDefenseDamage : 0,
					Overcharge != nullptr ? Overcharge->FundingCost : 0,
					Overcharge != nullptr && Overcharge->bAffordable ? TEXT("true") : TEXT("false"),
					CurrentStrategicSnapshot.ExpectedCommandSequence);
			}
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTStrategicDemo")))
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 20350101, EUEGTFundingModel::BalancedMandate);
		EstablishStarterBase(TEXT("region.cascadia"));
		ExecuteStrategicOption(EStrategicActionOptionType::Facility, TEXT("facility.flight-deck"));
		ExecuteStrategicOption(EStrategicActionOptionType::Research, TEXT("research.signal-analysis"));
		ExecuteStrategicOption(EStrategicActionOptionType::Personnel, TEXT("role.field-agent"));
	}
	const bool bCoalitionEmergencyVoteDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTCoalitionEmergencyVoteDemo"));
	const bool bCoalitionCounterplayDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTCoalitionCounterplayDemo"));
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTRegionalDiplomacyDemo"))
		|| bCoalitionEmergencyVoteDemo || bCoalitionCounterplayDemo)
	{
		StartStrategicCampaign(
			bCoalitionCounterplayDemo
				? ECampaignDifficulty::Standard : ECampaignDifficulty::Cadet,
			20350831,
			bCoalitionCounterplayDemo
				? EUEGTFundingModel::RapidMobilization
				: EUEGTFundingModel::BalancedMandate);
		EstablishStarterBase(TEXT("region.cascadia"));
		ExecuteRegionalDiplomacy(
			TEXT("region.cascadia"), ERegionalDiplomacyActionType::CivicRelief);
		SignRegionalCharter(TEXT("region.cascadia"));
		SignRegionalCharter(TEXT("region.north-atlantic"));
		RatifyHorizonCompact();
		bool bEmergencyVoteReady = false;
		bool bCounterplayReady = false;
		if (bCoalitionEmergencyVoteDemo)
		{
			if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
			{
				bEmergencyVoteReady = Instance->PrepareHorizonCompactEmergencyVoteDemo(
					TEXT("region.north-atlantic"));
				RefreshStrategicPresentation();
			}
		}
		if (bCoalitionCounterplayDemo)
		{
			if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
			{
				bCounterplayReady = Instance->PrepareCoalitionCounterplayDemo(
					TEXT("mission.ashen-accord-severance"));
				RefreshStrategicPresentation();
			}
		}
		if (StrategicHudWidget != nullptr)
		{
			StrategicHudWidget->ShowStatusMessage(FString());
		}
		UE_LOG(LogTemp, Display,
			TEXT("UEGTRegionalDiplomacyDemo final state: regions=%d funding=%lld funds=%lld"),
			CurrentStrategicSnapshot.Regions.Num(), CurrentStrategicSnapshot.MonthlyFunding,
			CurrentStrategicSnapshot.Funds);
		UE_LOG(LogTemp, Display,
			TEXT("UEGTRegionalDiplomacyDemo compact: compact_ratified=%s compact_enabled=%s members=%d active=%d withdrawn=%d funding_percent=%d shared_escape_pressure=%d aid_options=%d"),
			CurrentStrategicSnapshot.HorizonCompact.bRatified ? TEXT("true") : TEXT("false"),
			CurrentStrategicSnapshot.HorizonCompact.bEnabled ? TEXT("true") : TEXT("false"),
			CurrentStrategicSnapshot.HorizonCompact.SignedCharters,
			CurrentStrategicSnapshot.HorizonCompact.ActiveMemberRegionIds.Num(),
			CurrentStrategicSnapshot.HorizonCompact.WithdrawnMemberRegionIds.Num(),
			CurrentStrategicSnapshot.HorizonCompact.FundingPercent,
			CurrentStrategicSnapshot.HorizonCompact.SharedEscapePressurePercent,
			CurrentStrategicSnapshot.HorizonCompact.AidOptions.Num());
		for (const FStrategicCoalitionAidView& Aid :
			CurrentStrategicSnapshot.HorizonCompact.AidOptions)
		{
			UE_LOG(LogTemp, Display,
				TEXT("UEGTRegionalDiplomacyDemo aid: target=%s donor=%s current_pressure=%d projected_pressure=%d transfer=%d enabled=%s reason=%s"),
				*Aid.TargetRegionId.ToString(), *Aid.DonorRegionId.ToString(),
				Aid.TargetCurrentPressure, Aid.TargetProjectedPressure,
				Aid.PressureTransfer, Aid.bEnabled ? TEXT("true") : TEXT("false"),
				*Aid.UnavailableReasonCode.ToString());
		}
		for (const FStrategicRegionView& Region : CurrentStrategicSnapshot.Regions)
		{
			UE_LOG(LogTemp, Display,
				TEXT("UEGTRegionalDiplomacyDemo region: id=%s support=%d pressure=%d current=%lld projected=%lld actions=%d charter_signed=%s charter_enabled=%s charter_projected=%lld mission_weight=%d escape_pressure=%d"),
				*Region.RegionId.ToString(), Region.Support, Region.Pressure,
				Region.CurrentMonthlyFunding, Region.ProjectedMonthlyFunding,
				Region.ActionOptions.Num(),
				Region.ResilienceCharter.bSigned ? TEXT("true") : TEXT("false"),
				Region.ResilienceCharter.bEnabled ? TEXT("true") : TEXT("false"),
				Region.ResilienceCharter.ProjectedMonthlyFunding,
				Region.ResilienceCharter.MissionWeightPercent,
				Region.ResilienceCharter.EscapePressurePercent);
		}
		if (bCoalitionEmergencyVoteDemo)
		{
			const FStrategicRegionView* VoteTarget =
				CurrentStrategicSnapshot.Regions.FindByPredicate(
					[](const FStrategicRegionView& Region)
					{
						return Region.RegionId
							== FName(TEXT("region.north-atlantic"));
					});
			const FStrategicCompactEmergencyVoteView* Vote = VoteTarget != nullptr
				? &VoteTarget->HorizonCompactEmergencyVote : nullptr;
			UE_LOG(LogTemp, Display,
				TEXT("UEGTCoalitionEmergencyVoteDemo final state: source=development-fixture ready=%s target=%s withdrawn=%s enabled=%s yes=%d no=%d required=%d support=%d projected_support=%d pressure=%d projected_pressure=%d cost=%lld voter_support_cost=%d voter_pressure_limit=%d funding_delta=%lld"),
				bEmergencyVoteReady ? TEXT("true") : TEXT("false"),
				VoteTarget != nullptr ? *VoteTarget->RegionId.ToString() : TEXT("none"),
				Vote != nullptr && Vote->bTargetWithdrawn ? TEXT("true") : TEXT("false"),
				Vote != nullptr && Vote->bEnabled ? TEXT("true") : TEXT("false"),
				Vote != nullptr ? Vote->SupportingMemberRegionIds.Num() : 0,
				Vote != nullptr ? Vote->OpposingMemberRegionIds.Num() : 0,
				Vote != nullptr ? Vote->RequiredVotes : 0,
				Vote != nullptr ? Vote->TargetCurrentSupport : 0,
				Vote != nullptr ? Vote->TargetProjectedSupport : 0,
				Vote != nullptr ? Vote->TargetCurrentPressure : 0,
				Vote != nullptr ? Vote->TargetProjectedPressure : 0,
				Vote != nullptr ? Vote->Cost : 0,
				Vote != nullptr ? Vote->VoterSupportCost : 0,
				Vote != nullptr ? Vote->MaximumVoterPressure : 0,
				Vote != nullptr ? Vote->MonthlyFundingDelta : 0);
		}
		if (bCoalitionCounterplayDemo)
		{
			const FStrategicContactView* Counterplay =
				CurrentStrategicSnapshot.Contacts.IsEmpty()
					? nullptr : &CurrentStrategicSnapshot.Contacts[0];
			const FStrategicCoalitionCounterplayMemberView* EscapeMember =
				Counterplay != nullptr && !Counterplay->EscapeStrainMembers.IsEmpty()
					? &Counterplay->EscapeStrainMembers[0] : nullptr;
			const FStrategicCoalitionCounterplayMemberView* RecoveryMember =
				Counterplay != nullptr && !Counterplay->ThwartRecoveryMembers.IsEmpty()
					? &Counterplay->ThwartRecoveryMembers[0] : nullptr;
			UE_LOG(LogTemp, Display,
				TEXT("UEGTCoalitionCounterplayDemo final state: source=development-fixture ready=%s plan=%s stage=%d escape_members=%d escape_region=%s support=%d projected_support=%d withdrawal=%s recovery_members=%d recovery_region=%s recovery_support=%d recovery_projected_support=%d remains_withdrawn=%s"),
				bCounterplayReady ? TEXT("true") : TEXT("false"),
				Counterplay != nullptr ? *Counterplay->PlanId.ToString() : TEXT("none"),
				Counterplay != nullptr ? Counterplay->PlanStage : 0,
				Counterplay != nullptr ? Counterplay->EscapeStrainMembers.Num() : 0,
				EscapeMember != nullptr ? *EscapeMember->RegionId.ToString() : TEXT("none"),
				EscapeMember != nullptr ? EscapeMember->CurrentSupport : 0,
				EscapeMember != nullptr ? EscapeMember->ProjectedSupport : 0,
				EscapeMember != nullptr && EscapeMember->bWouldWithdraw
					? TEXT("true") : TEXT("false"),
				Counterplay != nullptr ? Counterplay->ThwartRecoveryMembers.Num() : 0,
				RecoveryMember != nullptr ? *RecoveryMember->RegionId.ToString() : TEXT("none"),
				RecoveryMember != nullptr ? RecoveryMember->CurrentSupport : 0,
				RecoveryMember != nullptr ? RecoveryMember->ProjectedSupport : 0,
				RecoveryMember != nullptr && RecoveryMember->bRemainsWithdrawn
					? TEXT("true") : TEXT("false"));
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTAdversaryPlanDemo")))
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 0x4d495252, EUEGTFundingModel::BalancedMandate);
		EstablishStarterBase(TEXT("region.cascadia"));
		for (int32 Attempt = 0; Attempt < 48; ++Attempt)
		{
			const bool bPlanDetected = CurrentStrategicSnapshot.Contacts.ContainsByPredicate(
				[](const FStrategicContactView& Contact)
				{
					return Contact.PlanId == FName(TEXT("plan.mirror-rain"));
				});
			if (bPlanDetected || CurrentStrategicSnapshot.Outcome != ECampaignOutcome::Ongoing)
			{
				break;
			}
			AdvanceStrategicClock(EStrategicTimeRate::OneHour);
		}
		const FStrategicContactView* PlanContact = CurrentStrategicSnapshot.Contacts.FindByPredicate(
			[](const FStrategicContactView& Contact)
			{
				return Contact.PlanId == FName(TEXT("plan.mirror-rain"));
			});
		if (StrategicHudWidget != nullptr)
		{
			StrategicHudWidget->ShowStatusMessage(FString());
		}
		UE_LOG(LogTemp, Display,
			TEXT("UEGTAdversaryPlanDemo final state: detected=%s plan=%s stage=%d escape=%s thwart=%s landingChoice=%s landingThreat=%d landingLifetime=%lld wreckageLifetime=%lld contacts=%d launched=%d"),
			PlanContact != nullptr ? TEXT("true") : TEXT("false"),
			PlanContact != nullptr ? *PlanContact->PlanId.ToString() : TEXT("none"),
			PlanContact != nullptr ? PlanContact->PlanStage : 0,
			PlanContact != nullptr ? *PlanContact->EscapeBranchMissionRuleId.ToString() : TEXT("none"),
			PlanContact != nullptr ? *PlanContact->ThwartBranchMissionRuleId.ToString() : TEXT("none"),
			PlanContact != nullptr && PlanContact->bCanShadowToLanding ? TEXT("true") : TEXT("false"),
			PlanContact != nullptr ? PlanContact->LandingSiteThreatRating : 0,
			PlanContact != nullptr ? PlanContact->LandingSiteLifetimeSeconds : 0,
			PlanContact != nullptr ? PlanContact->WreckageSiteLifetimeSeconds : 0,
			CurrentStrategicSnapshot.Contacts.Num(), CurrentStrategicSnapshot.AdversaryMissionsLaunched);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTLandingSiteDemo")))
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 0x4d495252, EUEGTFundingModel::BalancedMandate);
		EstablishStarterBase(TEXT("region.cascadia"));
		for (int32 Attempt = 0; Attempt < 96
			&& !CurrentStrategicSnapshot.Sites.ContainsByPredicate(
				[](const FStrategicSiteView& Site) { return Site.Type == EStrategicSiteType::Landing; })
			&& CurrentStrategicSnapshot.Outcome == ECampaignOutcome::Ongoing;
			++Attempt)
		{
			AdvanceStrategicClock(EStrategicTimeRate::OneHour);
		}
		const FStrategicSiteView* LandingSite = CurrentStrategicSnapshot.Sites.FindByPredicate(
			[](const FStrategicSiteView& Site) { return Site.Type == EStrategicSiteType::Landing; });
		if (StrategicHudWidget != nullptr)
		{
			StrategicHudWidget->ShowStatusMessage(FString());
		}
		UE_LOG(LogTemp, Display,
			TEXT("UEGTLandingSiteDemo final state: landed=%s type=%d threat=%d lifetime=%lld source=%s sites=%d contacts=%d escaped=%d"),
			LandingSite != nullptr ? TEXT("true") : TEXT("false"),
			LandingSite != nullptr ? static_cast<int32>(LandingSite->Type) : -1,
			LandingSite != nullptr ? LandingSite->ThreatRating : 0,
			LandingSite != nullptr ? LandingSite->RemainingLifetimeSeconds : 0,
			LandingSite != nullptr ? *LandingSite->SourceContactRuleId.ToString() : TEXT("none"),
			CurrentStrategicSnapshot.Sites.Num(), CurrentStrategicSnapshot.Contacts.Num(),
			CurrentStrategicSnapshot.AdversaryMissionsEscaped);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTResearchLabDemo")))
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 20350901, EUEGTFundingModel::BalancedMandate);
		EstablishStarterBase(TEXT("region.cascadia"));
		ExecuteStrategicOption(EStrategicActionOptionType::Research, TEXT("research.signal-analysis"));
		if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
		{
			if (!CurrentStrategicSnapshot.Bases.IsEmpty())
			{
				const FStrategicBaseView& Base = CurrentStrategicSnapshot.Bases[0];
				const FStrategicFacilityView* OperationsHub = Base.FacilityLayout.FindByPredicate(
					[](const FStrategicFacilityView& Facility)
					{
						return Facility.FacilityId == FName(TEXT("facility.operations-hub"));
					});
				if (OperationsHub != nullptr)
				{
					FApplyFacilityDamageCommand Damage;
					Damage.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
					Damage.BaseId = Base.BaseId;
					Damage.FacilityInstanceId = OperationsHub->FacilityInstanceId;
					Damage.Damage = OperationsHub->MaxIntegrity;
					PresentStrategicCommandResult(
						Instance->ApplyFacilityDamage(Damage),
						FUEGTLocalizationService::Text(
							TEXT("strategic.research-outage-status"),
							TEXT("Operations Hub disabled; its active research contract is paused.")));
				}
			}
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTItemPersonnelDemo")))
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 20350101, EUEGTFundingModel::BalancedMandate);
		if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
		{
			FEstablishBaseCommand Establish;
			Establish.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			Establish.BaseId = FGuid(0x17e00101, 0x17e00102, 0x17e00103, 0x17e00104);
			Establish.Name = TEXT("Cascadia Logistics Command");
			Establish.RegionId = TEXT("region.cascadia");
			Establish.LongitudeMilliDegrees = -123120;
			Establish.LatitudeMilliDegrees = 49280;
			Establish.StartingFacilities = {
				TEXT("facility.operations-hub"),
				TEXT("facility.fabrication-bay")
			};
			PresentStrategicCommandResult(
				Instance->EstablishBase(Establish),
				TEXT("Localized inventory and roster fixture established."));
			ExecuteStrategicOption(EStrategicActionOptionType::Personnel, TEXT("role.field-agent"));
			ExecuteStrategicOption(EStrategicActionOptionType::Personnel, TEXT("role.engineer"));
			for (int32 Attempt = 0; Attempt < 6 && CurrentStrategicSnapshot.Personnel.Num() < 2; ++Attempt)
			{
				AdvanceStrategicClock(EStrategicTimeRate::OneDay);
			}
			StartStrategicManufacturing(TEXT("item.service-rifle"), 2);
			for (int32 Attempt = 0; Attempt < 8; ++Attempt)
			{
				const FStrategicBaseView* Base = CurrentStrategicSnapshot.Bases.FindByPredicate(
					[&Establish](const FStrategicBaseView& View) { return View.BaseId == Establish.BaseId; });
				const FStrategicInventoryView* Rifle = Base != nullptr
					? Base->Inventory.FindByPredicate(
						[](const FStrategicInventoryView& Item)
						{
							return Item.ItemId == FName(TEXT("item.service-rifle"));
						})
					: nullptr;
				if (Rifle != nullptr && Rifle->Quantity >= 2)
				{
					break;
				}
				AdvanceStrategicClock(EStrategicTimeRate::OneDay);
			}
			const FStrategicPersonnelView* Agent = CurrentStrategicSnapshot.Personnel.FindByPredicate(
				[](const FStrategicPersonnelView& Person)
				{
					return Person.RoleId == FName(TEXT("role.field-agent"));
				});
			if (Agent != nullptr)
			{
				AdjustStrategicPersonnelEquipment(Agent->PersonnelId, TEXT("item.service-rifle"), 1);
			}
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTPersonnelProgressionDemo")))
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 0x50455253, EUEGTFundingModel::BalancedMandate);
		if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
		{
			FEstablishBaseCommand Establish;
			Establish.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			Establish.BaseId = FGuid(0x50300011, 0x50300012, 0x50300013, 0x50300014);
			Establish.Name = TEXT("Cascadia Doctrine Command");
			Establish.RegionId = TEXT("region.cascadia");
			Establish.LongitudeMilliDegrees = -123120;
			Establish.LatitudeMilliDegrees = 49280;
			Establish.StartingFacilities = { TEXT("facility.operations-hub") };
			PresentStrategicCommandResult(
				Instance->EstablishBase(Establish),
				TEXT("Personnel-progression presentation fixture established."));

			FStrategicPersonnelView& DemoAgent = CurrentStrategicSnapshot.Personnel.AddDefaulted_GetRef();
			DemoAgent.PersonnelId = FGuid(0x50300031, 0x50300032, 0x50300033, 0x50300034);
			DemoAgent.BaseId = Establish.BaseId;
			DemoAgent.DisplayName = TEXT("Maëlle Venn");
			DemoAgent.RoleId = TEXT("role.field-agent");
			DemoAgent.RoleDisplayName = TEXT("Field Agent");
			DemoAgent.RoleCategory = EPersonnelRoleCategory::FieldAgent;
			DemoAgent.Status = TEXT("Available");
			DemoAgent.StatusType = EPersonnelStatus::Available;
			DemoAgent.CurrentHealth = 62;
			DemoAgent.MaxHealth = 62;
			DemoAgent.Accuracy = 59;
			DemoAgent.Resolve = 57;
			DemoAgent.Mobility = 57;
			DemoAgent.Strength = 57;
			FStrategicPersonnelView* Agent = &DemoAgent;
			if (Agent != nullptr)
			{
				// Domain progression is covered by automation; this deterministic overlay keeps the
				// runtime capture focused on the complete localized personnel surface.
				Agent->Rank = 4;
				Agent->Missions = 5;
				Agent->Kills = 3;
				Agent->Experience = 900;
				Agent->Accuracy = FMath::Min(100, Agent->Accuracy + 4);
				Agent->PendingDoctrineChoices = 2;
				Agent->DoctrineOptions.Reset();
				TArray<FName> DoctrineIds;
				Instance->GetLoadedRules().PersonnelDoctrines.GenerateKeyArray(DoctrineIds);
				DoctrineIds.Sort(FNameLexicalLess());
				for (const FName DoctrineId : DoctrineIds)
				{
					const FPersonnelDoctrineRule& Rule =
						Instance->GetLoadedRules().PersonnelDoctrines.FindChecked(DoctrineId);
					FStrategicPersonnelDoctrineView& Option = Agent->DoctrineOptions.AddDefaulted_GetRef();
					Option.DoctrineId = DoctrineId;
					Option.DisplayName = Rule.DisplayName;
					Option.Summary = Rule.Summary;
					Option.CurrentSelections = DoctrineId == FName(TEXT("doctrine.clear-sight")) ? 1 : 0;
					Option.MaximumSelections = Rule.MaxSelections;
					Option.MaxHealthBonus = Rule.MaxHealthBonus;
					Option.AccuracyBonus = Rule.AccuracyBonus;
					Option.ResolveBonus = Rule.ResolveBonus;
					Option.MobilityBonus = Rule.MobilityBonus;
					Option.StrengthBonus = Rule.StrengthBonus;
					Option.bEnabled = true;
				}
				Agent->Commendations.Reset();
				const TArray<FName> EarnedIds = {
					TEXT("commendation.contact-breaker"),
					TEXT("commendation.first-response"),
					TEXT("commendation.steady-hand")
				};
				for (const FName CommendationId : EarnedIds)
				{
					const FPersonnelCommendationRule* Rule =
						Instance->GetLoadedRules().PersonnelCommendations.Find(CommendationId);
					if (Rule == nullptr)
					{
						continue;
					}
					FStrategicPersonnelCommendationView& Citation =
						Agent->Commendations.AddDefaulted_GetRef();
					Citation.CommendationId = CommendationId;
					Citation.DisplayName = Rule->DisplayName;
					Citation.Summary = Rule->Summary;
				}
				if (StrategicHudWidget != nullptr)
				{
					StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
				}
				UE_LOG(LogTemp, Display,
					TEXT("UEGTPersonnelProgressionDemo final state: source=presentation-fixture personnel=%d rank=%d missions=%d kills=%d pending=%d selected=%d commendations=%d options=%d"),
					CurrentStrategicSnapshot.Personnel.Num(), Agent->Rank, Agent->Missions,
					Agent->Kills, Agent->PendingDoctrineChoices, 1,
					Agent->Commendations.Num(), Agent->DoctrineOptions.Num());
			}
		}
	}
	if (bServiceHistoryDemo || bWatchkeeperDemo || bLegacyRelayDemo || bSquadBondDemo)
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 0x53455256, EUEGTFundingModel::BalancedMandate);
		if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
		{
			FEstablishBaseCommand Establish;
			Establish.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			Establish.BaseId = FGuid(0x53400011, 0x53400012, 0x53400013, 0x53400014);
			Establish.Name = TEXT("Cascadia Service Command");
			Establish.RegionId = TEXT("region.cascadia");
			Establish.LongitudeMilliDegrees = -123120;
			Establish.LatitudeMilliDegrees = 49280;
			Establish.StartingFacilities = { TEXT("facility.operations-hub") };
			PresentStrategicCommandResult(
				Instance->EstablishBase(Establish),
				TEXT("Service-history presentation fixture established."));

			// This non-shipping overlay proves the derived presentation without mutating
			// persisted personnel or memorial state.
			CurrentStrategicSnapshot.Personnel.Reserve(
				CurrentStrategicSnapshot.Personnel.Num()
				+ ((bWatchkeeperDemo || bLegacyRelayDemo || bSquadBondDemo) ? 2 : 1));
			FStrategicPersonnelView& Agent = CurrentStrategicSnapshot.Personnel.AddDefaulted_GetRef();
			Agent.PersonnelId = FGuid(0x53400021, 0x53400022, 0x53400023, 0x53400024);
			Agent.BaseId = Establish.BaseId;
			Agent.DisplayName = TEXT("Maëlle Venn");
			Agent.RoleId = TEXT("role.field-agent");
			Agent.RoleDisplayName = TEXT("Field Agent");
			Agent.RoleCategory = EPersonnelRoleCategory::FieldAgent;
			Agent.Status = TEXT("Deployed");
			Agent.StatusType = EPersonnelStatus::Deployed;
			Agent.Rank = 3;
			Agent.Missions = 5;
			Agent.Kills = 4;
			Agent.Experience = 700;
			Agent.ServiceHistory = FPersonnelServiceHistory::Project(Agent.Missions);
			Agent.CurrentHealth = 58;
			Agent.MaxHealth = 58;
			Agent.Accuracy = 61;
			Agent.Resolve = 59;
			Agent.Mobility = 60;
			Agent.Strength = 55;

			FStrategicPersonnelCommendationView& AgentCitation =
				Agent.Commendations.AddDefaulted_GetRef();
			AgentCitation.CommendationId = TEXT("commendation.first-response");
			if (const FPersonnelCommendationRule* Rule =
				Instance->GetLoadedRules().PersonnelCommendations.Find(AgentCitation.CommendationId))
			{
				AgentCitation.DisplayName = Rule->DisplayName;
				AgentCitation.Summary = Rule->Summary;
			}

			FStrategicCraftView* ServiceTeamCraft = nullptr;
			if (bWatchkeeperDemo || bLegacyRelayDemo || bSquadBondDemo)
			{
				Agent.Status = TEXT("Available");
				Agent.StatusType = EPersonnelStatus::Available;
				const FGuid MentorId(0x53400041, 0x53400042, 0x53400043, 0x53400044);
				FStrategicPersonnelView& Mentor =
					CurrentStrategicSnapshot.Personnel.AddDefaulted_GetRef();
				Mentor.PersonnelId = MentorId;
				Mentor.BaseId = Establish.BaseId;
				Mentor.DisplayName = TEXT("Mara Sol");
				Mentor.RoleId = TEXT("role.field-agent");
				Mentor.RoleDisplayName = TEXT("Field Agent");
				Mentor.RoleCategory = EPersonnelRoleCategory::FieldAgent;
				Mentor.Status = TEXT("Available");
				Mentor.StatusType = EPersonnelStatus::Available;
				Mentor.Rank = bSquadBondDemo ? 3 : 5;
				Mentor.Missions = bSquadBondDemo ? 8 : 20;
				Mentor.Kills = 13;
				Mentor.Experience = 2400;
				Mentor.ServiceHistory = FPersonnelServiceHistory::Project(Mentor.Missions);
				Mentor.CurrentHealth = 64;
				Mentor.MaxHealth = 64;
				Mentor.Accuracy = 68;
				Mentor.Resolve = 72;
				Mentor.Mobility = 61;
				Mentor.Strength = 63;

				FCampaignState MentorshipCampaign;
				FPersonnelState& MentorState = MentorshipCampaign.Personnel.AddDefaulted_GetRef();
				MentorState.PersonnelId = Mentor.PersonnelId;
				MentorState.DisplayName = Mentor.DisplayName;
				MentorState.Missions = Mentor.Missions;
				MentorState.Rank = bSquadBondDemo ? 3 : 4;
				if (bLegacyRelayDemo)
				{
					MentorState.DoctrineSelections = {
						TEXT("doctrine.clear-sight"),
						TEXT("doctrine.clear-sight"),
						TEXT("doctrine.clear-sight") };
				}
				FPersonnelState& RecipientState = MentorshipCampaign.Personnel.AddDefaulted_GetRef();
				RecipientState.PersonnelId = Agent.PersonnelId;
				RecipientState.DisplayName = Agent.DisplayName;
				RecipientState.Missions = Agent.Missions;
				if (bSquadBondDemo)
				{
					FPersonnelSquadBondState& Bond =
						MentorshipCampaign.PersonnelSquadBonds.AddDefaulted_GetRef();
					Bond.FirstPersonnelId = RecipientState.PersonnelId;
					Bond.SecondPersonnelId = MentorState.PersonnelId;
					Bond.SharedVictories = FPersonnelSquadBond::InterlockedVictories;
				}

				FStrategicCraftView& Craft = CurrentStrategicSnapshot.Craft.AddDefaulted_GetRef();
				Craft.CraftId = FGuid(0x53400051, 0x53400052, 0x53400053, 0x53400054);
				Craft.BaseId = Establish.BaseId;
				Craft.CraftRuleId = TEXT("craft.heron-transport");
				Craft.DisplayName = bSquadBondDemo
					? TEXT("Field Cadence One")
					: (bLegacyRelayDemo ? TEXT("Legacy Relay One") : TEXT("Watchkeeper One"));
				Craft.TypeDisplayName = TEXT("Heron Transport");
				Craft.Status = TEXT("Grounded");
				Craft.StatusType = ECraftStatus::Grounded;
				Craft.CurrentHull = 100;
				Craft.MaxHull = 100;
				Craft.CurrentFuel = 500;
				Craft.FuelCapacity = 500;
				Craft.AssignedAgents = 2;
				Craft.AgentCapacity = 4;
				Craft.AssignedAgentIds = { Mentor.PersonnelId, Agent.PersonnelId };
				Craft.Mentorship = FPersonnelMentorship::Evaluate(
					MentorshipCampaign, Craft.AssignedAgentIds);
				Craft.LegacyRelay = FPersonnelLegacyRelay::Evaluate(
					MentorshipCampaign, Instance->GetLoadedRules(), Craft.AssignedAgentIds);
				Craft.SquadBonds = FPersonnelSquadBond::Evaluate(
					MentorshipCampaign, Craft.AssignedAgentIds);
				ServiceTeamCraft = &Craft;
			}

			FStrategicMemorialView& Memorial =
				CurrentStrategicSnapshot.Memorial.AddDefaulted_GetRef();
			Memorial.PersonnelId = FGuid(0x53400031, 0x53400032, 0x53400033, 0x53400034);
			Memorial.DisplayName = TEXT("Mara Sol");
			Memorial.RoleId = TEXT("role.field-agent");
			Memorial.RoleDisplayName = TEXT("Field Agent");
			Memorial.Rank = 4;
			Memorial.Missions = 10;
			Memorial.Kills = 8;
			Memorial.ServiceHistory = FPersonnelServiceHistory::Project(Memorial.Missions);
			Memorial.DoctrineSelections.Add(TEXT("doctrine.clear-sight"));
			Memorial.DeathUtc = FDateTime(2042, 4, 6, 9, 30, 0);
			Memorial.CauseId = TEXT("cause.tactical-casualty");
			Memorial.CauseDisplayName = TEXT("Tactical operation casualty");
			FStrategicPersonnelCommendationView& MemorialCitation =
				Memorial.Commendations.AddDefaulted_GetRef();
			MemorialCitation.CommendationId = TEXT("commendation.long-watch");
			if (const FPersonnelCommendationRule* Rule =
				Instance->GetLoadedRules().PersonnelCommendations.Find(MemorialCitation.CommendationId))
			{
				MemorialCitation.DisplayName = Rule->DisplayName;
				MemorialCitation.Summary = Rule->Summary;
			}

			if (StrategicHudWidget != nullptr)
			{
				StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
			}
			// Command-line culture application refreshes the authoritative dashboard later
			// in BeginPlay. Reapply this presentation-only snapshot on the next tick so
			// localized packaged captures retain the fixture.
			const FStrategicDashboardSnapshot ServiceHistoryFixture = CurrentStrategicSnapshot;
			FTimerDelegate ServiceHistoryFixtureDelegate;
			ServiceHistoryFixtureDelegate.BindWeakLambda(
				this, [this, ServiceHistoryFixture]()
				{
					CurrentStrategicSnapshot = ServiceHistoryFixture;
					if (StrategicHudWidget != nullptr)
					{
						StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
					}
				});
			FTimerHandle ServiceHistoryFixtureTimer;
			GetWorldTimerManager().SetTimer(
				ServiceHistoryFixtureTimer, ServiceHistoryFixtureDelegate, 0.1f, false);
			if (bSquadBondDemo && ServiceTeamCraft != nullptr)
			{
				const FPersonnelSquadBondPairView* Pair =
					ServiceTeamCraft->SquadBonds.ActivePairs.IsEmpty()
						? nullptr
						: &ServiceTeamCraft->SquadBonds.ActivePairs[0];
				UE_LOG(LogTemp, Display,
					TEXT("UEGT_FIELD_CADENCE_RUNTIME_OK source=presentation-fixture personnel=%d craft=%d pairs=%d shared=%d tier=%d ap=%d morale=%d active=%s"),
					CurrentStrategicSnapshot.Personnel.Num(),
					CurrentStrategicSnapshot.Craft.Num(),
					ServiceTeamCraft->SquadBonds.ActivePairs.Num(),
					Pair != nullptr ? Pair->SharedVictories : 0,
					Pair != nullptr ? static_cast<int32>(Pair->Tier) : 0,
					Pair != nullptr ? Pair->ActionPointBonus : 0,
					Pair != nullptr ? Pair->MoraleBonus : 0,
					ServiceTeamCraft->SquadBonds.bActive ? TEXT("true") : TEXT("false"));
			}
			else if (bLegacyRelayDemo && ServiceTeamCraft != nullptr)
			{
				UE_LOG(LogTemp, Display,
					TEXT("UEGTLegacyRelayDemo final state: source=presentation-fixture personnel=%d craft=%d specialist=%s band=%d doctrine=%s accuracy=%d resolve=%d mobility=%d strength=%d recipients=%d active=%s"),
					CurrentStrategicSnapshot.Personnel.Num(),
					CurrentStrategicSnapshot.Craft.Num(),
					*ServiceTeamCraft->LegacyRelay.SpecialistId.ToString(EGuidFormats::Digits),
					static_cast<int32>(ServiceTeamCraft->LegacyRelay.SpecialistServiceHistory.Band),
					*ServiceTeamCraft->LegacyRelay.DoctrineId.ToString(),
					ServiceTeamCraft->LegacyRelay.AccuracyBonus,
					ServiceTeamCraft->LegacyRelay.ResolveBonus,
					ServiceTeamCraft->LegacyRelay.MobilityBonus,
					ServiceTeamCraft->LegacyRelay.StrengthBonus,
					ServiceTeamCraft->LegacyRelay.RecipientCount,
					ServiceTeamCraft->LegacyRelay.bActive ? TEXT("true") : TEXT("false"));
			}
			else if (bWatchkeeperDemo && ServiceTeamCraft != nullptr)
			{
				UE_LOG(LogTemp, Display,
					TEXT("UEGTWatchkeeperDemo final state: source=presentation-fixture personnel=%d craft=%d mentor=%s band=%d bonus=%d recipients=%d active=%s"),
					CurrentStrategicSnapshot.Personnel.Num(),
					CurrentStrategicSnapshot.Craft.Num(),
					*ServiceTeamCraft->Mentorship.MentorId.ToString(EGuidFormats::Digits),
					static_cast<int32>(ServiceTeamCraft->Mentorship.MentorServiceHistory.Band),
					ServiceTeamCraft->Mentorship.MoraleBonus,
					ServiceTeamCraft->Mentorship.RecipientCount,
					ServiceTeamCraft->Mentorship.bActive ? TEXT("true") : TEXT("false"));
			}
			else
			{
				UE_LOG(LogTemp, Display,
					TEXT("UEGTServiceHistoryDemo final state: source=presentation-fixture personnel=%d service=%d next=%d remaining=%d memorial=%d memorialService=%d memorialNext=%d memorialRemaining=%d"),
					CurrentStrategicSnapshot.Personnel.Num(),
					static_cast<int32>(Agent.ServiceHistory.Band),
					static_cast<int32>(Agent.ServiceHistory.NextBand),
					Agent.ServiceHistory.MissionsUntilNextBand,
					CurrentStrategicSnapshot.Memorial.Num(),
					static_cast<int32>(Memorial.ServiceHistory.Band),
					static_cast<int32>(Memorial.ServiceHistory.NextBand),
					Memorial.ServiceHistory.MissionsUntilNextBand);
			}
		}
	}
	if (bRecoveryPlanDemo)
	{
		PrepareRecoveryPlanDemo();
	}
	if (bStewardshipDemo)
	{
		PrepareStewardshipDemo();
	}
	if (bWorksCadreDemo || bWorksCharterDemo)
	{
		PrepareWorksCadreDemo(bWorksCharterDemo);
	}
	if (bMutualAidDemo)
	{
		PrepareMutualAidConvoyDemo();
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTManufacturingDemo")))
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 20350101, EUEGTFundingModel::BalancedMandate);
		if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
		{
			FEstablishBaseCommand Establish;
			Establish.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			Establish.BaseId = FGuid(0x17e00201, 0x17e00202, 0x17e00203, 0x17e00204);
			Establish.Name = TEXT("Cascadia Fabrication Command");
			Establish.RegionId = TEXT("region.cascadia");
			Establish.LongitudeMilliDegrees = -123120;
			Establish.LatitudeMilliDegrees = 49280;
			Establish.StartingFacilities = {
				TEXT("facility.operations-hub"),
				TEXT("facility.fabrication-bay")
			};
			PresentStrategicCommandResult(
				Instance->EstablishBase(Establish),
				TEXT("Localized active-production fixture established."));
			StartStrategicManufacturing(TEXT("item.service-rifle"), 3);
		}
	}
	const bool bFleetContactDemo = FParse::Param(FCommandLine::Get(), TEXT("UEGTFleetContactDemo"));
	const bool bInterceptionPostureDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTInterceptionPostureDemo"));
	if (bFleetContactDemo || bInterceptionPostureDemo)
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 20351012, EUEGTFundingModel::RapidMobilization);
		if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
		{
			FEstablishBaseCommand Establish;
			Establish.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			Establish.BaseId = FGuid(0x17e00301, 0x17e00302, 0x17e00303, 0x17e00304);
			Establish.Name = TEXT("Cascadia Flight Command");
			Establish.RegionId = TEXT("region.cascadia");
			Establish.LongitudeMilliDegrees = -123120;
			Establish.LatitudeMilliDegrees = 49280;
			Establish.StartingFacilities = {
				TEXT("facility.operations-hub"),
				TEXT("facility.flight-deck")
			};
			if (!bInterceptionPostureDemo)
			{
				Establish.StartingFacilities.Add(TEXT("facility.fabrication-bay"));
			}
			PresentStrategicCommandResult(
				Instance->EstablishBase(Establish),
				TEXT("Localized fleet-and-contact fixture established."));
			FRecruitPersonnelCommand RecruitPilot;
			RecruitPilot.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			RecruitPilot.OrderId = FGuid(0x17e00321, 0x17e00322, 0x17e00323, 0x17e00324);
			RecruitPilot.PersonnelId = FGuid(0x17e00325, 0x17e00326, 0x17e00327, 0x17e00328);
			RecruitPilot.BaseId = Establish.BaseId;
			RecruitPilot.RoleId = TEXT("role.pilot");
			RecruitPilot.DisplayName = TEXT("Mara Voss");
			PresentStrategicCommandResult(
				Instance->RecruitPersonnel(RecruitPilot),
				TEXT("Deterministic pilot recruitment placed through the strategic domain."));
			if (bInterceptionPostureDemo)
			{
				FRecruitPersonnelCommand RecruitRelayPilot;
				RecruitRelayPilot.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
				RecruitRelayPilot.OrderId = FGuid(0x17e00341, 0x17e00342, 0x17e00343, 0x17e00344);
				RecruitRelayPilot.PersonnelId = FGuid(0x17e00345, 0x17e00346, 0x17e00347, 0x17e00348);
				RecruitRelayPilot.BaseId = Establish.BaseId;
				RecruitRelayPilot.RoleId = TEXT("role.pilot");
				RecruitRelayPilot.DisplayName = TEXT("Luc Renard");
				PresentStrategicCommandResult(
					Instance->RecruitPersonnel(RecruitRelayPilot),
					TEXT("Relay pilot recruitment placed through the strategic domain."));
			}
			FAcquireCraftCommand AcquireCraft;
			AcquireCraft.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			AcquireCraft.OrderId = FGuid(0x17e00331, 0x17e00332, 0x17e00333, 0x17e00334);
			AcquireCraft.CraftId = FGuid(0x17e00335, 0x17e00336, 0x17e00337, 0x17e00338);
			AcquireCraft.BaseId = Establish.BaseId;
			AcquireCraft.CraftRuleId = TEXT("craft.sparrow-interceptor");
			AcquireCraft.DisplayName = TEXT("Aiguille 01");
			PresentStrategicCommandResult(
				Instance->AcquireCraft(AcquireCraft),
				TEXT("Deterministic interceptor acquisition placed through the strategic domain."));
			if (bInterceptionPostureDemo)
			{
				FAcquireCraftCommand AcquireRelayCraft;
				AcquireRelayCraft.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
				AcquireRelayCraft.OrderId = FGuid(0x17e00351, 0x17e00352, 0x17e00353, 0x17e00354);
				AcquireRelayCraft.CraftId = FGuid(0x17e00355, 0x17e00356, 0x17e00357, 0x17e00358);
				AcquireRelayCraft.BaseId = Establish.BaseId;
				AcquireRelayCraft.CraftRuleId = TEXT("craft.sparrow-interceptor");
				AcquireRelayCraft.DisplayName = TEXT("Aiguille 02");
				PresentStrategicCommandResult(
					Instance->AcquireCraft(AcquireRelayCraft),
					TEXT("Relay interceptor acquisition placed through the strategic domain."));
			}
			const int32 RequiredInterceptors = bInterceptionPostureDemo ? 2 : 1;
			const int32 RequiredWeapons = bInterceptionPostureDemo ? 0 : 1;
			const int32 RequiredAmmunition = RequiredWeapons * 12;
			if (RequiredWeapons > 0)
			{
				StartStrategicManufacturing(TEXT("item.sky-lance"), RequiredWeapons);
				StartStrategicManufacturing(TEXT("item.sky-lance-rounds"), RequiredAmmunition);
			}
			for (int32 Attempt = 0; Attempt < 64; ++Attempt)
			{
				if (CurrentStrategicSnapshot.Outcome != ECampaignOutcome::Ongoing
					|| CurrentStrategicSnapshot.Bases.IsEmpty())
				{
					break;
				}
				int32 PilotCount = 0;
				for (const FStrategicPersonnelView& Person : CurrentStrategicSnapshot.Personnel)
				{
					if (Person.RoleId == FName(TEXT("role.pilot")))
					{
						++PilotCount;
					}
				}
				int32 InterceptorCount = 0;
				for (const FStrategicCraftView& Craft : CurrentStrategicSnapshot.Craft)
				{
					if (Craft.CraftRuleId == FName(TEXT("craft.sparrow-interceptor")))
					{
						++InterceptorCount;
					}
				}
				const FStrategicBaseView* Base = CurrentStrategicSnapshot.Bases.FindByPredicate(
					[&Establish](const FStrategicBaseView& View) { return View.BaseId == Establish.BaseId; });
				const FStrategicInventoryView* Weapons = Base != nullptr
					? Base->Inventory.FindByPredicate(
						[](const FStrategicInventoryView& Item)
						{
							return Item.ItemId == FName(TEXT("item.sky-lance"));
						})
					: nullptr;
				const FStrategicInventoryView* Ammunition = Base != nullptr
					? Base->Inventory.FindByPredicate(
						[](const FStrategicInventoryView& Item)
						{
							return Item.ItemId == FName(TEXT("item.sky-lance-rounds"));
						})
					: nullptr;
				const int32 WeaponCount = Weapons != nullptr ? Weapons->Quantity : 0;
				const int32 AmmunitionCount = Ammunition != nullptr ? Ammunition->Quantity : 0;
				if (PilotCount >= RequiredInterceptors
					&& InterceptorCount >= RequiredInterceptors
					&& WeaponCount >= RequiredWeapons
					&& AmmunitionCount >= RequiredAmmunition)
				{
					break;
				}
				if (!CurrentStrategicSnapshot.BaseAssaults.IsEmpty())
				{
					ResolveBaseAssault(CurrentStrategicSnapshot.BaseAssaults[0].AssaultId,
						EBaseDefenseFireDoctrine::CoordinatedLine);
					continue;
				}
				AdvanceStrategicClock(EStrategicTimeRate::OneDay);
			}
			TArray<FGuid> InterceptorIds;
			for (const FStrategicCraftView& Craft : CurrentStrategicSnapshot.Craft)
			{
				if (Craft.CraftRuleId == FName(TEXT("craft.sparrow-interceptor")))
				{
					InterceptorIds.Add(Craft.CraftId);
				}
			}
			InterceptorIds.Sort(
				[](const FGuid& Left, const FGuid& Right)
				{
					return Left.ToString(EGuidFormats::Digits)
						< Right.ToString(EGuidFormats::Digits);
				});
			for (int32 Index = 0; Index < FMath::Min(RequiredInterceptors, InterceptorIds.Num()); ++Index)
			{
				AutoPrepareCraft(InterceptorIds[Index]);
			}
			if (const FStrategicBaseView* Base = CurrentStrategicSnapshot.Bases.FindByPredicate(
				[&Establish](const FStrategicBaseView& View) { return View.BaseId == Establish.BaseId; }))
			{
				if (const FStrategicFacilityView* Fabrication = Base->FacilityLayout.FindByPredicate(
					[](const FStrategicFacilityView& Facility)
					{
						return Facility.FacilityId == FName(TEXT("facility.fabrication-bay"));
					}); Fabrication != nullptr && Fabrication->bCanDismantle)
				{
					DismantleStrategicFacility(Establish.BaseId, Fabrication->FacilityInstanceId);
				}
			}

			FGuid VisibleContactId;
			for (int32 Trial = 0; Trial < 8 && !VisibleContactId.IsValid(); ++Trial)
			{
				FCreateStrategicContactCommand Contact;
				Contact.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
				Contact.ContactId = FGuid(0x17e00311 + Trial, 0x17e00312, 0x17e00313, 0x17e00314);
				Contact.ContactRuleId = TEXT("contact.skimmer");
				Contact.OriginLongitudeMilliDegrees = Establish.LongitudeMilliDegrees;
				Contact.OriginLatitudeMilliDegrees = Establish.LatitudeMilliDegrees - 7650;
				Contact.DestinationLongitudeMilliDegrees = Establish.LongitudeMilliDegrees;
				Contact.DestinationLatitudeMilliDegrees = Establish.LatitudeMilliDegrees + 7650;
				PresentStrategicCommandResult(
					Instance->CreateStrategicContact(Contact),
					TEXT("Deterministic contact route injected through the strategic domain."));
				AdvanceStrategicClock(EStrategicTimeRate::OneHour);
				if (CurrentStrategicSnapshot.Contacts.ContainsByPredicate(
					[&Contact](const FStrategicContactView& View) { return View.ContactId == Contact.ContactId; }))
				{
					VisibleContactId = Contact.ContactId;
				}
				else
				{
					AdvanceStrategicClock(EStrategicTimeRate::OneHour);
				}
			}
			if (bInterceptionPostureDemo && VisibleContactId.IsValid())
			{
				for (int32 Index = 0; Index < FMath::Min(RequiredInterceptors, InterceptorIds.Num()); ++Index)
				{
					FDispatchCraftCommand Dispatch;
					Dispatch.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
					Dispatch.CraftId = InterceptorIds[Index];
					Dispatch.ContactId = VisibleContactId;
					PresentStrategicCommandResult(
						Instance->DispatchCraft(Dispatch),
						TEXT("Deterministic withdrawal-demo interceptor dispatched through the strategic domain."));
				}
				for (int32 Attempt = 0; Attempt < 36; ++Attempt)
				{
					const FStrategicContactView* PursuedContact =
						CurrentStrategicSnapshot.Contacts.FindByPredicate(
							[VisibleContactId](const FStrategicContactView& View)
							{
								return View.ContactId == VisibleContactId;
							});
					if (PursuedContact == nullptr
						|| PursuedContact->StatusType == EStrategicContactStatus::Engaged
						|| CurrentStrategicSnapshot.Outcome != ECampaignOutcome::Ongoing)
					{
						break;
					}
					AdvanceStrategicClock(EStrategicTimeRate::FiveMinutes);
				}
				const FStrategicContactView* EngagedContact =
					CurrentStrategicSnapshot.Contacts.FindByPredicate(
						[VisibleContactId](const FStrategicContactView& View)
						{
							return View.ContactId == VisibleContactId;
						});
				const FStrategicInterceptionWithdrawalView* FormationWithdrawal =
					EngagedContact != nullptr
						? EngagedContact->InterceptionWithdrawals.FindByPredicate(
							[](const FStrategicInterceptionWithdrawalView& Option)
							{
								return Option.Doctrine
									== EInterceptionWithdrawalDoctrine::FormationBreak;
							})
						: nullptr;
				const FStrategicInterceptionWithdrawalView* RelayWithdrawal =
					EngagedContact != nullptr
						? EngagedContact->InterceptionWithdrawals.FindByPredicate(
							[](const FStrategicInterceptionWithdrawalView& Option)
							{
								return Option.Doctrine
									== EInterceptionWithdrawalDoctrine::EvasiveRelay;
							})
						: nullptr;
				const FStrategicInterceptionWithdrawalView* WakeWithdrawal =
					EngagedContact != nullptr
						? EngagedContact->InterceptionWithdrawals.FindByPredicate(
							[](const FStrategicInterceptionWithdrawalView& Option)
							{
								return Option.Doctrine
									== EInterceptionWithdrawalDoctrine::WakeSnare;
							})
						: nullptr;
				UE_LOG(LogTemp, Display,
					TEXT("UEGTInterceptionPostureDemo final state: source=authoritative-domain engaged=%s contact=%s postures=%d coordination=%s support=%d outgoing=%+d incoming=%+d maneuver=%s maneuverOutgoing=%+d maneuverIncoming=%+d rounds=%d withdrawals=%d formation=%s relay=%s relayCraft=\"%s\" relayHull=%d/%d wake=%s wakeDelay=%lld wakeRounds=%d/%d withdrawalCraft=%d craft=%d sequence=%lld"),
					EngagedContact != nullptr
						&& EngagedContact->StatusType == EStrategicContactStatus::Engaged
						? TEXT("true") : TEXT("false"),
					*VisibleContactId.ToString(EGuidFormats::Digits),
					EngagedContact != nullptr ? EngagedContact->InterceptionPostures.Num() : 0,
					EngagedContact != nullptr
						? *EngagedContact->InterceptionCoordination.PolicyId.ToString()
						: TEXT("none"),
					EngagedContact != nullptr
						? EngagedContact->InterceptionCoordination.SupportingCraftCount : 0,
					EngagedContact != nullptr
						? EngagedContact->InterceptionCoordination.OutgoingAccuracyModifier : 0,
					EngagedContact != nullptr
						? EngagedContact->InterceptionCoordination.IncomingAccuracyModifier : 0,
					EngagedContact != nullptr
						? *EngagedContact->InterceptionContactManeuver.PolicyId.ToString()
						: TEXT("none"),
					EngagedContact != nullptr
						? EngagedContact->InterceptionContactManeuver.OutgoingAccuracyModifier : 0,
					EngagedContact != nullptr
						? EngagedContact->InterceptionContactManeuver.IncomingAccuracyModifier : 0,
					EngagedContact != nullptr
						? EngagedContact->InterceptionContactManeuver.CompletedCombatRounds : 0,
					EngagedContact != nullptr ? EngagedContact->InterceptionWithdrawals.Num() : 0,
					FormationWithdrawal != nullptr && FormationWithdrawal->bEnabled
						? TEXT("true") : TEXT("false"),
					RelayWithdrawal != nullptr && RelayWithdrawal->bEnabled
						? TEXT("true") : TEXT("false"),
					RelayWithdrawal != nullptr
						? *RelayWithdrawal->PriorityCraftDisplayName : TEXT("none"),
					RelayWithdrawal != nullptr ? RelayWithdrawal->PriorityCraftCurrentHull : 0,
					RelayWithdrawal != nullptr ? RelayWithdrawal->PriorityCraftMaximumHull : 0,
					WakeWithdrawal != nullptr && WakeWithdrawal->bEnabled
						? TEXT("true") : TEXT("false"),
					WakeWithdrawal != nullptr ? WakeWithdrawal->ContactRouteDelaySeconds : 0,
					WakeWithdrawal != nullptr ? WakeWithdrawal->CompletedCombatRounds : 0,
					WakeWithdrawal != nullptr ? WakeWithdrawal->RequiredCombatRounds : 0,
					EngagedContact != nullptr ? EngagedContact->InterceptionCraftCount : 0,
					CurrentStrategicSnapshot.Craft.Num(),
					CurrentStrategicSnapshot.ExpectedCommandSequence);
				const FCampaignState& BeforeWithdrawal = Instance->GetCampaignState();
				const FStrategicContactState* BeforeContact =
					BeforeWithdrawal.StrategicContacts.FindByPredicate(
						[VisibleContactId](const FStrategicContactState& Contact)
						{
							return Contact.ContactId == VisibleContactId;
						});
				if (RelayWithdrawal != nullptr && RelayWithdrawal->bEnabled
					&& BeforeContact != nullptr)
				{
					const int64 DrawsBefore = BeforeWithdrawal.SimulationRandom.DrawCount;
					const int32 RoundsBefore = BeforeContact->CompletedCombatRounds;
					const int64 CooldownBefore = BeforeContact->RemainingAttackCooldownSeconds;
					const int64 SequenceBefore = BeforeWithdrawal.CommandSequence;
					FTimerDelegate WithdrawalDelegate;
					WithdrawalDelegate.BindWeakLambda(
						this,
						[this, VisibleContactId, DrawsBefore, RoundsBefore, CooldownBefore, SequenceBefore]()
						{
							WithdrawContactInterceptionWithDoctrine(
								VisibleContactId, EInterceptionWithdrawalDoctrine::EvasiveRelay);
							const UUEGTGameInstance* RuntimeInstance = GetGameInstance<UUEGTGameInstance>();
							if (RuntimeInstance == nullptr)
							{
								return;
							}
							const FCampaignState& After = RuntimeInstance->GetCampaignState();
							const FStrategicContactState* Contact = After.StrategicContacts.FindByPredicate(
								[VisibleContactId](const FStrategicContactState& Candidate)
								{
									return Candidate.ContactId == VisibleContactId;
								});
							int32 ReturningCraft = 0;
							int32 OnStationCraft = 0;
							for (const FCraftState& Craft : After.Craft)
							{
								ReturningCraft += Craft.Status == ECraftStatus::Returning ? 1 : 0;
								OnStationCraft += Craft.Status == ECraftStatus::Airborne
									&& Craft.TargetContactId == VisibleContactId ? 1 : 0;
							}
							const FString LocalizedStatus = StrategicHudWidget != nullptr
								? StrategicHudWidget->GetRenderedStatusText()
								: FString();
							UE_LOG(LogTemp, Display,
								TEXT("UEGTInterceptionRelayDemo step=1: source=controller-route engaged=%s detected=%s returning=%d onStation=%d policy=interception.withdrawal-evasive-relay localized=\"%s\" drawDelta=%lld roundDelta=%d cooldownDelta=%lld sequenceDelta=%lld"),
								Contact != nullptr && Contact->Status == EStrategicContactStatus::Engaged
									? TEXT("true") : TEXT("false"),
								Contact != nullptr && Contact->Status == EStrategicContactStatus::Detected
									? TEXT("true") : TEXT("false"),
								ReturningCraft,
								OnStationCraft,
								*LocalizedStatus,
								After.SimulationRandom.DrawCount - DrawsBefore,
								Contact != nullptr ? Contact->CompletedCombatRounds - RoundsBefore : -1,
								Contact != nullptr
									? Contact->RemainingAttackCooldownSeconds - CooldownBefore : -1,
								After.CommandSequence - SequenceBefore);

							FTimerDelegate FinalRelayDelegate;
							FinalRelayDelegate.BindWeakLambda(
								this,
								[this, VisibleContactId, DrawsBefore, RoundsBefore, CooldownBefore, SequenceBefore]()
								{
									WithdrawContactInterceptionWithDoctrine(
										VisibleContactId, EInterceptionWithdrawalDoctrine::EvasiveRelay);
									const UUEGTGameInstance* FinalInstance =
										GetGameInstance<UUEGTGameInstance>();
									if (FinalInstance == nullptr)
									{
										return;
									}
									const FCampaignState& FinalState = FinalInstance->GetCampaignState();
									const FStrategicContactState* FinalContact =
										FinalState.StrategicContacts.FindByPredicate(
											[VisibleContactId](const FStrategicContactState& Candidate)
											{
												return Candidate.ContactId == VisibleContactId;
											});
									int32 FinalReturningCraft = 0;
									int32 FinalOnStationCraft = 0;
									for (const FCraftState& Craft : FinalState.Craft)
									{
										FinalReturningCraft +=
											Craft.Status == ECraftStatus::Returning ? 1 : 0;
										FinalOnStationCraft += Craft.Status == ECraftStatus::Airborne
											&& Craft.TargetContactId == VisibleContactId ? 1 : 0;
									}
									const FString FinalLocalizedStatus = StrategicHudWidget != nullptr
										? StrategicHudWidget->GetRenderedStatusText()
										: FString();
									UE_LOG(LogTemp, Display,
										TEXT("UEGTInterceptionRelayDemo step=2: source=controller-route engaged=%s detected=%s returning=%d onStation=%d policy=interception.withdrawal-evasive-relay localized=\"%s\" drawDelta=%lld roundDelta=%d cooldownDelta=%lld sequenceDelta=%lld"),
										FinalContact != nullptr
											&& FinalContact->Status == EStrategicContactStatus::Engaged
											? TEXT("true") : TEXT("false"),
										FinalContact != nullptr
											&& FinalContact->Status == EStrategicContactStatus::Detected
											? TEXT("true") : TEXT("false"),
										FinalReturningCraft,
										FinalOnStationCraft,
										*FinalLocalizedStatus,
										FinalState.SimulationRandom.DrawCount - DrawsBefore,
										FinalContact != nullptr
											? FinalContact->CompletedCombatRounds - RoundsBefore : -1,
										FinalContact != nullptr
											? FinalContact->RemainingAttackCooldownSeconds - CooldownBefore : -1,
										FinalState.CommandSequence - SequenceBefore);
								});
							FTimerHandle FinalRelayTimer;
							GetWorldTimerManager().SetTimer(
								FinalRelayTimer, FinalRelayDelegate, 1.0f, false);
						});
					FTimerHandle WithdrawalTimer;
					GetWorldTimerManager().SetTimer(
						WithdrawalTimer, WithdrawalDelegate, 4.0f, false);
				}
			}
			UE_LOG(LogTemp, Display,
				TEXT("UEGTFleetContactDemo final state: bases=%d personnel=%d craft=%d projects=%d contacts=%d assaults=%d outcome=%d funds=%lld"),
				CurrentStrategicSnapshot.Bases.Num(), CurrentStrategicSnapshot.Personnel.Num(),
				CurrentStrategicSnapshot.Craft.Num(), CurrentStrategicSnapshot.Projects.Num(),
				CurrentStrategicSnapshot.Contacts.Num(), CurrentStrategicSnapshot.BaseAssaults.Num(),
				static_cast<int32>(CurrentStrategicSnapshot.Outcome), CurrentStrategicSnapshot.Funds);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTRosterDemo")))
	{
		for (int32 Attempt = 0; Attempt < 16 && CurrentStrategicSnapshot.Personnel.IsEmpty(); ++Attempt)
		{
			AdvanceStrategicClock(EStrategicTimeRate::OneDay);
		}
	}
	FString DemoCulture;
	if (FParse::Value(FCommandLine::Get(), TEXT("UEGTCulture="), DemoCulture))
	{
		if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
		{
			Settings->SetInterfaceCulture(DemoCulture);
			Settings->ApplyLanguageSettings();
			RefreshTacticalPresentation();
		}
	}
	if (bSalvageDispositionDemo)
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 0x53414c56, EUEGTFundingModel::BalancedMandate);
		if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
		{
			FEstablishBaseCommand Establish;
			Establish.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			Establish.BaseId = FGuid(0x5a1a0011, 0x5a1a0012, 0x5a1a0013, 0x5a1a0014);
			Establish.Name = TEXT("Cascadia Recovery Command");
			Establish.RegionId = TEXT("region.cascadia");
			Establish.LongitudeMilliDegrees = -123120;
			Establish.LatitudeMilliDegrees = 49280;
			Establish.StartingFacilities = {
				TEXT("facility.operations-hub"),
				TEXT("facility.flight-deck"),
				TEXT("facility.secure-storage")
			};
			PresentStrategicCommandResult(
				Instance->EstablishBase(Establish),
				TEXT("Salvage-disposition presentation fixture established."));

			if (!CurrentStrategicSnapshot.Bases.IsEmpty())
			{
				// Authoritative recovery, landing, disposition, and replay are covered by automation.
				// This non-shipping overlay keeps the runtime capture focused on the complete HUD surface.
				FStrategicBaseView& Base = CurrentStrategicSnapshot.Bases[0];
				Base.CraftOccupied = 1;
				FStrategicCraftView& Craft = CurrentStrategicSnapshot.Craft.AddDefaulted_GetRef();
				Craft.CraftId = FGuid(0x5a1a0031, 0x5a1a0032, 0x5a1a0033, 0x5a1a0034);
				Craft.BaseId = Base.BaseId;
				Craft.CraftRuleId = TEXT("craft.heron-transport");
				Craft.DisplayName = TEXT("Courier 07");
				Craft.TypeDisplayName = TEXT("Heron Transport");
				Craft.Status = TEXT("Grounded");
				Craft.StatusType = ECraftStatus::Grounded;
				Craft.CurrentHull = 140;
				Craft.MaxHull = 140;
				Craft.CurrentFuel = 720;
				Craft.FuelCapacity = 900;
				Craft.AgentCapacity = 4;
				Craft.bSalvageDispositionAvailable = true;

				const FName SalvageItemId(TEXT("item.resonance-shard"));
				const FItemRule* SalvageRule = Instance->GetLoadedRules().Items.Find(SalvageItemId);
				FStrategicCraftSalvageView& Salvage = Craft.PendingSalvage.AddDefaulted_GetRef();
				Salvage.ItemId = SalvageItemId;
				Salvage.DisplayName = SalvageRule != nullptr
					? SalvageRule->DisplayName : FString(TEXT("Resonance Shard"));
				Salvage.Quantity = 2;
				Salvage.UnitStorage = SalvageRule != nullptr ? SalvageRule->Mass : 2;
				Salvage.TotalStorage = int64(Salvage.UnitStorage) * Salvage.Quantity;
				Salvage.UnitSellValue = SalvageRule != nullptr ? SalvageRule->SellValue : 24000;
				Salvage.TotalSellValue = int64(Salvage.UnitSellValue) * Salvage.Quantity;
				Salvage.bCanRetainAtBase = true;
				Salvage.bCanSell = true;

				if (StrategicHudWidget != nullptr)
				{
					StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
					StrategicHudWidget->ShowStatusMessage(FString());
				}
				UE_LOG(LogTemp, Display,
					TEXT("UEGTSalvageDispositionDemo final state: source=presentation-fixture grounded=true item=%s quantity=%d storage=%lld sale=%lld retain=%s sell=%s sequence=%lld"),
					*Salvage.ItemId.ToString(), Salvage.Quantity, Salvage.TotalStorage,
					Salvage.TotalSellValue, Salvage.bCanRetainAtBase ? TEXT("true") : TEXT("false"),
					Salvage.bCanSell ? TEXT("true") : TEXT("false"),
					CurrentStrategicSnapshot.ExpectedCommandSequence);
			}
		}
	}
	if (bPartialRearmDemo)
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 0x52454152, EUEGTFundingModel::BalancedMandate);
		if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
		{
			FEstablishBaseCommand Establish;
			Establish.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			Establish.BaseId = FGuid(0x2ea20011, 0x2ea20012, 0x2ea20013, 0x2ea20014);
			Establish.Name = TEXT("Cascadia Flight Command");
			Establish.RegionId = TEXT("region.cascadia");
			Establish.LongitudeMilliDegrees = -123120;
			Establish.LatitudeMilliDegrees = 49280;
			Establish.StartingFacilities = {
				TEXT("facility.operations-hub"),
				TEXT("facility.flight-deck")
			};
			PresentStrategicCommandResult(
				Instance->EstablishBase(Establish),
				TEXT("Partial-rearm presentation fixture established."));

			if (!CurrentStrategicSnapshot.Bases.IsEmpty())
			{
				// Transactional rearm behavior and shared-stock ordering are covered by automation.
				// This non-shipping overlay keeps the runtime capture focused on exact readiness controls.
				FStrategicBaseView& Base = CurrentStrategicSnapshot.Bases[0];
				Base.CraftOccupied = 1;
				FStrategicCraftView& Craft = CurrentStrategicSnapshot.Craft.AddDefaulted_GetRef();
				Craft.CraftId = FGuid(0x2ea20031, 0x2ea20032, 0x2ea20033, 0x2ea20034);
				Craft.BaseId = Base.BaseId;
				Craft.CraftRuleId = TEXT("craft.sparrow-interceptor");
				Craft.DisplayName = TEXT("Needle 04");
				Craft.TypeDisplayName = TEXT("Sparrow Interceptor");
				Craft.Status = TEXT("Grounded");
				Craft.StatusType = ECraftStatus::Grounded;
				Craft.CurrentHull = 120;
				Craft.MaxHull = 120;
				Craft.CurrentFuel = 900;
				Craft.FuelCapacity = 900;
				FStrategicCraftWeaponView& Weapon = Craft.Weapons.AddDefaulted_GetRef();
				Weapon.WeaponItemId = TEXT("item.sky-lance");
				Weapon.AmmunitionItemId = TEXT("item.sky-lance-rounds");
				if (const FItemRule* Rule = Instance->GetLoadedRules().Items.Find(Weapon.WeaponItemId))
				{
					Weapon.WeaponDisplayName = Rule->DisplayName;
				}
				if (const FItemRule* Rule = Instance->GetLoadedRules().Items.Find(Weapon.AmmunitionItemId))
				{
					Weapon.AmmunitionDisplayName = Rule->DisplayName;
				}
				Weapon.MountCount = 1;
				Weapon.LoadedAmmunition = 4;
				Weapon.Capacity = 12;
				Weapon.MissingAmmunition = 8;
				Weapon.BaseAvailableAmmunition = 5;
				Weapon.LoadableAmmunition = 5;
				Craft.TotalAmmunitionLoaded = 4;
				Craft.TotalAmmunitionCapacity = 12;
				Craft.TotalAmmunitionMissing = 8;
				Craft.TotalAmmunitionLoadable = 5;
				Craft.bCanRearmFully = false;
				Craft.bCanLoadAvailableAmmunition = true;

				if (StrategicHudWidget != nullptr)
				{
					StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
					StrategicHudWidget->ShowStatusMessage(FString());
				}
				UE_LOG(LogTemp, Display,
					TEXT("UEGTPartialRearmDemo final state: source=presentation-fixture loaded=%lld capacity=%lld base=%lld loadable=%lld full=%s available=%s sequence=%lld"),
					Craft.TotalAmmunitionLoaded, Craft.TotalAmmunitionCapacity,
					Weapon.BaseAvailableAmmunition, Craft.TotalAmmunitionLoadable,
					Craft.bCanRearmFully ? TEXT("true") : TEXT("false"),
					Craft.bCanLoadAvailableAmmunition ? TEXT("true") : TEXT("false"),
					CurrentStrategicSnapshot.ExpectedCommandSequence);
			}
		}
	}
	if (bCraftServiceDemo)
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 0x53455256, EUEGTFundingModel::BalancedMandate);
		if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
		{
			FEstablishBaseCommand Establish;
			Establish.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
			Establish.BaseId = FGuid(0x5e2b0011, 0x5e2b0012, 0x5e2b0013, 0x5e2b0014);
			Establish.Name = TEXT("Cascadia Turnaround Command");
			Establish.RegionId = TEXT("region.cascadia");
			Establish.LongitudeMilliDegrees = -123120;
			Establish.LatitudeMilliDegrees = 49280;
			Establish.StartingFacilities = {
				TEXT("facility.operations-hub"),
				TEXT("facility.flight-deck")
			};
			PresentStrategicCommandResult(
				Instance->EstablishBase(Establish),
				TEXT("Craft-service presentation fixture established."));

			if (!CurrentStrategicSnapshot.Bases.IsEmpty())
			{
				// Authoritative queue progression, reservation accounting, cancellation, and replay safety are covered by automation.
				// This non-shipping overlay keeps the runtime capture focused on both active and waiting rotation states.
				FStrategicBaseView& Base = CurrentStrategicSnapshot.Bases[0];
				Base.CraftOccupied = 2;
				const FGuid ActiveCraftId(0x5e2b0031, 0x5e2b0032, 0x5e2b0033, 0x5e2b0034);
				const FGuid QueuedCraftId(0x5e2b0041, 0x5e2b0042, 0x5e2b0043, 0x5e2b0044);
				FStrategicCraftView& ActiveCraft = CurrentStrategicSnapshot.Craft.AddDefaulted_GetRef();
				ActiveCraft.CraftId = ActiveCraftId;
				ActiveCraft.BaseId = Base.BaseId;
				ActiveCraft.CraftRuleId = TEXT("craft.sparrow-interceptor");
				ActiveCraft.DisplayName = TEXT("Relay 12");
				ActiveCraft.TypeDisplayName = TEXT("Sparrow Interceptor");
				ActiveCraft.Status = TEXT("Servicing");
				ActiveCraft.StatusType = ECraftStatus::Servicing;
				ActiveCraft.CurrentHull = 119;
				ActiveCraft.MaxHull = 120;
				ActiveCraft.CurrentFuel = 900;
				ActiveCraft.FuelCapacity = 900;
				ActiveCraft.RemainingRepairSeconds = 3600;
				ActiveCraft.RemainingServiceSeconds = 3600;
				ActiveCraft.ServiceCancellationRefund = 3000;
				ActiveCraft.bCanCancelService = true;
				ActiveCraft.ServiceQueue.bValid = true;
				ActiveCraft.ServiceQueue.PolicyId = TEXT("craft.service-rapid-turnaround");
				ActiveCraft.ServiceQueue.BaseId = Base.BaseId;
				ActiveCraft.ServiceQueue.CraftId = ActiveCraftId;
				ActiveCraft.ServiceQueue.ServiceLaneCount = 1;
				ActiveCraft.ServiceQueue.ActiveServiceCraftCount = 1;
				ActiveCraft.ServiceQueue.TotalServiceCraftCount = 2;
				ActiveCraft.ServiceQueue.QueuePosition = 1;
				ActiveCraft.ServiceQueue.ServiceLaneNumber = 1;
				ActiveCraft.ServiceQueue.bInServiceLane = true;
				ActiveCraft.ServiceQueue.EstimatedReadySeconds = 3600;

				FStrategicCraftView& QueuedCraft = CurrentStrategicSnapshot.Craft.AddDefaulted_GetRef();
				QueuedCraft.CraftId = QueuedCraftId;
				QueuedCraft.BaseId = Base.BaseId;
				QueuedCraft.CraftRuleId = TEXT("craft.sparrow-interceptor");
				QueuedCraft.DisplayName = TEXT("Relay 27");
				QueuedCraft.TypeDisplayName = TEXT("Sparrow Interceptor");
				QueuedCraft.Status = TEXT("Servicing");
				QueuedCraft.StatusType = ECraftStatus::Servicing;
				QueuedCraft.CurrentHull = 117;
				QueuedCraft.MaxHull = 120;
				QueuedCraft.CurrentFuel = 750;
				QueuedCraft.FuelCapacity = 900;
				QueuedCraft.RemainingRepairSeconds = 2 * 3600;
				QueuedCraft.RemainingRefuelSeconds = 3600;
				QueuedCraft.RemainingServiceSeconds = 2 * 3600;
				QueuedCraft.ServiceCancellationRefund = 9300;
				QueuedCraft.bCanCancelService = true;
				QueuedCraft.ServiceQueue.bValid = true;
				QueuedCraft.ServiceQueue.PolicyId = TEXT("craft.service-rapid-turnaround");
				QueuedCraft.ServiceQueue.BaseId = Base.BaseId;
				QueuedCraft.ServiceQueue.CraftId = QueuedCraftId;
				QueuedCraft.ServiceQueue.ServiceLaneCount = 1;
				QueuedCraft.ServiceQueue.ActiveServiceCraftCount = 1;
				QueuedCraft.ServiceQueue.TotalServiceCraftCount = 2;
				QueuedCraft.ServiceQueue.QueuePosition = 2;
				QueuedCraft.ServiceQueue.WaitingPosition = 1;
				QueuedCraft.ServiceQueue.ServiceLaneNumber = 1;
				QueuedCraft.ServiceQueue.EstimatedWaitSeconds = 3600;
				QueuedCraft.ServiceQueue.EstimatedReadySeconds = 3 * 3600;

				if (StrategicHudWidget != nullptr)
				{
					StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
					StrategicHudWidget->ShowStatusMessage(FString());
				}
				UE_LOG(LogTemp, Display,
					TEXT("UEGTCraftServiceDemo final state: source=presentation-fixture craft=2 policy=%s lanes=%d active=%s active_ready=%lld queued=%s waiting_position=%d queued_wait=%lld queued_ready=%lld sequence=%lld"),
					*CurrentStrategicSnapshot.Craft[0].ServiceQueue.PolicyId.ToString(),
					CurrentStrategicSnapshot.Craft[0].ServiceQueue.ServiceLaneCount,
					*ActiveCraftId.ToString(EGuidFormats::Digits),
					CurrentStrategicSnapshot.Craft[0].ServiceQueue.EstimatedReadySeconds,
					*QueuedCraftId.ToString(EGuidFormats::Digits),
					QueuedCraft.ServiceQueue.WaitingPosition,
					QueuedCraft.ServiceQueue.EstimatedWaitSeconds,
					QueuedCraft.ServiceQueue.EstimatedReadySeconds,
					CurrentStrategicSnapshot.ExpectedCommandSequence);
			}
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTModCatalogDemo")))
	{
		const UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
		UE_LOG(LogTemp, Display,
			TEXT("UEGTModCatalogDemo final state: source=multi-root-catalog ready=%s packages=%d files=%d sample=%s reloadControl=%d"),
			Instance != nullptr && Instance->IsContentReady() ? TEXT("true") : TEXT("false"),
			Instance != nullptr ? Instance->GetLoadedContentVersions().Num() : 0,
			Instance != nullptr ? Instance->GetLoadedContentFiles().Num() : 0,
			Instance != nullptr && Instance->GetLoadedRules().Items.Contains(TEXT("item.aurora-relay"))
				? TEXT("true") : TEXT("false"),
			StrategicHudWidget != nullptr
				? StrategicHudWidget->GetRenderedContentReloadControlCount() : 0);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTDiagnosticDemo")))
	{
		if (UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>())
		{
			if (!Instance->HasActiveCampaign())
			{
				StartStrategicCampaign(
					ECampaignDifficulty::Cadet, 20350101, EUEGTFundingModel::BalancedMandate);
			}
			FAdvanceStrategicTimeCommand StaleCommand;
			StaleCommand.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence + 1;
			StaleCommand.Rate = EStrategicTimeRate::OneHour;
			const FStrategicCommandResult Result = Instance->AdvanceStrategicTime(StaleCommand);
			PresentStrategicCommandResult(Result, FString());
			const FString LocalizedMessage = Result.Diagnostics.IsEmpty()
				? UEGTTacticalControllerPrivate::Localized(
					TEXT("strategic.command-rejected"), TEXT("The strategic command was rejected."))
				: UEGTTacticalControllerPrivate::LocalizedDiagnostic(
					Result.Diagnostics[0].Code, Result.Diagnostics[0].Message);
			UE_LOG(LogTemp, Display,
				TEXT("UEGTDiagnosticDemo: accepted=%s code=%s localized=\"%s\""),
				Result.bAccepted ? TEXT("true") : TEXT("false"),
				Result.Diagnostics.IsEmpty() ? TEXT("none") : *Result.Diagnostics[0].Code.ToString(),
				*LocalizedMessage);
		}
	}
	if ((FParse::Param(FCommandLine::Get(), TEXT("UEGTItemPersonnelDemo"))
		|| FParse::Param(FCommandLine::Get(), TEXT("UEGTPersonnelProgressionDemo"))
		|| bServiceHistoryDemo
		|| bWatchkeeperDemo
		|| bLegacyRelayDemo
		|| bSquadBondDemo
		|| bRecoveryPlanDemo
		|| bStewardshipDemo
		|| bMutualAidDemo
		|| FParse::Param(FCommandLine::Get(), TEXT("UEGTManufacturingDemo"))
		|| bFleetContactDemo
		|| bInterceptionPostureDemo
		|| bSalvageDispositionDemo
		|| bPartialRearmDemo
		|| bCraftServiceDemo
		|| bBaseDefenseSupplyDemo
		|| bCoalitionEmergencyVoteDemo
		|| bCoalitionCounterplayDemo)
		&& StrategicHudWidget != nullptr)
	{
		StrategicHudWidget->ShowStatusMessage(FString());
		if (bBaseDefenseSupplyDemo)
		{
			StrategicHudWidget->FocusBaseDefensePanel();
			FTimerDelegate BaseDefenseFocusDelegate;
			BaseDefenseFocusDelegate.BindWeakLambda(this, [this]()
			{
				if (StrategicHudWidget != nullptr)
				{
					StrategicHudWidget->FocusBaseDefensePanel();
				}
			});
			FTimerHandle BaseDefenseFocusTimer;
			GetWorldTimerManager().SetTimer(
				BaseDefenseFocusTimer, BaseDefenseFocusDelegate, 1.0f, false);
		}
		else if (bCoalitionEmergencyVoteDemo)
		{
			StrategicHudWidget->FocusCoalitionEmergencyVotePanel();
			FTimerDelegate VoteFocusDelegate;
			VoteFocusDelegate.BindWeakLambda(this, [this]()
			{
				if (StrategicHudWidget != nullptr)
				{
					StrategicHudWidget->FocusCoalitionEmergencyVotePanel();
				}
			});
			FTimerHandle VoteFocusTimer;
			GetWorldTimerManager().SetTimer(VoteFocusTimer, VoteFocusDelegate, 1.0f, false);
		}
		else if (bCoalitionCounterplayDemo)
		{
			StrategicHudWidget->FocusContactPanel();
			FTimerDelegate CounterplayFocusDelegate;
			CounterplayFocusDelegate.BindWeakLambda(this, [this]()
			{
				if (StrategicHudWidget != nullptr)
				{
					StrategicHudWidget->FocusContactPanel();
				}
			});
			FTimerHandle CounterplayFocusTimer;
			GetWorldTimerManager().SetTimer(
				CounterplayFocusTimer, CounterplayFocusDelegate, 1.0f, false);
		}
		else if (bWatchkeeperDemo || bLegacyRelayDemo || bSquadBondDemo
			|| bSalvageDispositionDemo || bPartialRearmDemo || bCraftServiceDemo)
		{
			StrategicHudWidget->FocusFleetPanel();
			FTimerDelegate SalvageFocusDelegate;
			SalvageFocusDelegate.BindWeakLambda(this, [this]()
			{
				if (StrategicHudWidget != nullptr)
				{
					StrategicHudWidget->FocusFleetPanel();
				}
			});
			FTimerHandle SalvageFocusTimer;
			GetWorldTimerManager().SetTimer(SalvageFocusTimer, SalvageFocusDelegate, 1.0f, false);
		}
		else if (bMutualAidDemo && !bSignalWatchDemo)
		{
			StrategicHudWidget->FocusMutualAidPanel();
			FTimerDelegate MutualAidFocusDelegate;
			MutualAidFocusDelegate.BindWeakLambda(this, [this]()
			{
				if (StrategicHudWidget != nullptr)
				{
					StrategicHudWidget->FocusMutualAidPanel();
				}
			});
			FTimerHandle MutualAidFocusTimer;
			GetWorldTimerManager().SetTimer(
				MutualAidFocusTimer, MutualAidFocusDelegate, 1.0f, false);
		}
		else if (bRecoveryPlanDemo || bStewardshipDemo || bServiceHistoryDemo
			|| FParse::Param(FCommandLine::Get(), TEXT("UEGTPersonnelProgressionDemo")))
		{
			StrategicHudWidget->FocusPersonnelPanel();
			FTimerDelegate PersonnelFocusDelegate;
			PersonnelFocusDelegate.BindWeakLambda(this, [this]()
			{
				if (StrategicHudWidget != nullptr)
				{
					StrategicHudWidget->FocusPersonnelPanel();
				}
			});
			FTimerHandle PersonnelFocusTimer;
			GetWorldTimerManager().SetTimer(PersonnelFocusTimer, PersonnelFocusDelegate, 1.0f, false);
		}
		else if (bInterceptionPostureDemo)
		{
			StrategicHudWidget->FocusContactPanel();
			FTimerDelegate ContactFocusDelegate;
			ContactFocusDelegate.BindWeakLambda(this, [this]()
			{
				if (StrategicHudWidget != nullptr)
				{
					StrategicHudWidget->FocusContactPanel();
				}
			});
			FTimerHandle ContactFocusTimer;
			GetWorldTimerManager().SetTimer(ContactFocusTimer, ContactFocusDelegate, 1.0f, false);
		}
		else if (bFleetContactDemo)
		{
			StrategicHudWidget->FocusFleetPanel();
			FTimerDelegate FleetFocusDelegate;
			FleetFocusDelegate.BindWeakLambda(this, [this]()
			{
				if (StrategicHudWidget != nullptr)
				{
					StrategicHudWidget->FocusFleetPanel();
				}
			});
			FTimerHandle FleetFocusTimer;
			GetWorldTimerManager().SetTimer(FleetFocusTimer, FleetFocusDelegate, 1.0f, false);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTRuntimeCapture")))
	{
		FTimerDelegate CaptureDelegate;
		CaptureDelegate.BindWeakLambda(this, [this]()
		{
			if (GetWorld() != nullptr)
			{
				FScreenshotRequest::RequestScreenshot(true, true);
			}
		});
		FTimerHandle CaptureTimer;
		// Packaged offscreen builds can still be warming Slate PSOs during the first
		// couple of seconds; wait for a settled frame before recording review evidence.
		GetWorldTimerManager().SetTimer(CaptureTimer, CaptureDelegate, 5.0f, false);
	}
	if ((FParse::Param(FCommandLine::Get(), TEXT("UEGTTacticalDefenseDemo")) || bHistoricalFogDemo)
		&& !CurrentStrategicSnapshot.BaseAssaults.IsEmpty())
	{
		DeployBaseDefense(CurrentStrategicSnapshot.BaseAssaults[0].AssaultId);
		if (bHistoricalFogDemo && CurrentSnapshot.bSucceeded)
		{
			UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
			bool bDoorCycleCompleted = false;
			if (Instance != nullptr)
			{
				FConfirmTacticalDeploymentCommand Confirm;
				Confirm.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
				Confirm.BattleId = CurrentSnapshot.BattleId;
				PresentStrategicCommandResult(
					Instance->ConfirmTacticalDeployment(Confirm),
					TEXT("Historical-fog review deployment confirmed through the tactical domain."));

				FCampaignState DemoState = Instance->GetCampaignState();
				const FResolvedRuleSet& Rules = Instance->GetLoadedRules();
				const FTacticalBattleState* Battle = DemoState.TacticalBattles.FindByPredicate(
					[this](const FTacticalBattleState& Entry) { return Entry.BattleId == CurrentSnapshot.BattleId; });
				FGuid UnitId;
				int32 DoorX = INDEX_NONE;
				int32 DoorY = INDEX_NONE;
				int32 DoorZ = INDEX_NONE;
				int32 DestinationX = INDEX_NONE;
				int32 DestinationY = INDEX_NONE;
				int32 DestinationZ = INDEX_NONE;
				int32 BestPathCost = MAX_int32;
				int32 DoorActionPointCost = 0;
				if (Battle != nullptr)
				{
					static constexpr int32 OffsetX[] = { 0, -1, 1, 0 };
					static constexpr int32 OffsetY[] = { -1, 0, 0, 1 };
					for (const FTacticalCellState& Cell : Battle->Cells)
					{
						const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
						if (Terrain == nullptr || !Terrain->IsDoor() || Cell.CurrentIntegrity <= 0 || Cell.bDoorOpen)
						{
							continue;
						}
						for (const FTacticalUnitState& Unit : Battle->Units)
						{
							if (Unit.Team != ETacticalTeam::Player || Unit.CurrentHealth <= 0 || Unit.bExtracted)
							{
								continue;
							}
							for (int32 Direction = 0; Direction < 4; ++Direction)
							{
								const int32 CandidateX = Cell.X + OffsetX[Direction];
								const int32 CandidateY = Cell.Y + OffsetY[Direction];
								if (!Battle->IsWithinGrid(CandidateX, CandidateY, Cell.Z))
								{
									continue;
								}
								const FTacticalPathResult Path = FTacticalNavigationService::FindPath(
									*Battle, Rules, Unit.UnitId, CandidateX, CandidateY, Cell.Z);
								if (Path.bSucceeded && Path.TotalCost < BestPathCost)
								{
									UnitId = Unit.UnitId;
									DoorX = Cell.X;
									DoorY = Cell.Y;
									DoorZ = Cell.Z;
									DestinationX = CandidateX;
									DestinationY = CandidateY;
									DestinationZ = Cell.Z;
									BestPathCost = Path.TotalCost;
									DoorActionPointCost = Terrain->DoorActionPointCost;
								}
							}
						}
					}
				}

				bool bReadyToCycleDoor = false;
				static constexpr int32 MaximumApproachTurns = 4;
				for (int32 ApproachTurn = 0;
					UnitId.IsValid() && ApproachTurn < MaximumApproachTurns && !bReadyToCycleDoor;
					++ApproachTurn)
				{
					DemoState = Instance->GetCampaignState();
					Battle = DemoState.TacticalBattles.FindByPredicate(
						[this](const FTacticalBattleState& Entry) { return Entry.BattleId == CurrentSnapshot.BattleId; });
					const FTacticalUnitState* DoorUser = Battle != nullptr
						? Battle->Units.FindByPredicate([UnitId](const FTacticalUnitState& Unit) { return Unit.UnitId == UnitId; })
						: nullptr;
					if (Battle == nullptr || DoorUser == nullptr || DoorUser->CurrentHealth <= 0 || DoorUser->bExtracted
						|| Battle->Phase != ETacticalBattlePhase::PlayerTurn || Battle->ActiveTeam != ETacticalTeam::Player
						|| !Battle->IsWithinGrid(DoorX, DoorY, DoorZ))
					{
						break;
					}

					const FTacticalCellState& DoorCell = Battle->Cells[Battle->GetCellIndex(DoorX, DoorY, DoorZ)];
					const bool bAdjacentToDoor = FMath::Abs(DoorX - DoorUser->X)
						+ FMath::Abs(DoorY - DoorUser->Y)
						+ FMath::Abs(DoorZ - DoorUser->Z) == 1;
					const int32 RequiredDoorActions = DoorCell.bDoorOpen ? 1 : 2;
					if (bAdjacentToDoor && DoorUser->RemainingActionPoints >= DoorActionPointCost * RequiredDoorActions)
					{
						bReadyToCycleDoor = true;
						break;
					}

					if (!bAdjacentToDoor && DoorUser->RemainingActionPoints > 0)
					{
						const FTacticalPathResult Path = FTacticalNavigationService::FindPath(
							*Battle, Rules, UnitId, DestinationX, DestinationY, DestinationZ);
						int32 AffordableCost = 0;
						const FTacticalPathStep* AffordableDestination = nullptr;
						if (Path.bSucceeded)
						{
							for (const FTacticalPathStep& Step : Path.Steps)
							{
								if (AffordableCost + Step.MoveCost > DoorUser->RemainingActionPoints)
								{
									break;
								}
								AffordableCost += Step.MoveCost;
								AffordableDestination = &Step;
							}
						}
						if (AffordableDestination != nullptr)
						{
							FMoveTacticalUnitCommand Move;
							Move.ExpectedSequence = DemoState.CommandSequence;
							Move.BattleId = CurrentSnapshot.BattleId;
							Move.UnitId = UnitId;
							Move.DestinationX = AffordableDestination->X;
							Move.DestinationY = AffordableDestination->Y;
							Move.DestinationZ = AffordableDestination->Z;
							const FStrategicCommandResult Moved = Instance->MoveTacticalUnit(Move);
							PresentStrategicCommandResult(
								Moved,
								TEXT("Defender advanced toward the signal barrier through the tactical domain."));
							if (!Moved.bAccepted)
							{
								break;
							}
						}
					}

					DemoState = Instance->GetCampaignState();
					Battle = DemoState.TacticalBattles.FindByPredicate(
						[this](const FTacticalBattleState& Entry) { return Entry.BattleId == CurrentSnapshot.BattleId; });
					DoorUser = Battle != nullptr
						? Battle->Units.FindByPredicate([UnitId](const FTacticalUnitState& Unit) { return Unit.UnitId == UnitId; })
						: nullptr;
					if (Battle == nullptr || DoorUser == nullptr || DoorUser->CurrentHealth <= 0 || DoorUser->bExtracted
						|| Battle->Phase != ETacticalBattlePhase::PlayerTurn)
					{
						break;
					}

					const FTacticalCellState& UpdatedDoorCell = Battle->Cells[Battle->GetCellIndex(DoorX, DoorY, DoorZ)];
					const bool bNowAdjacent = FMath::Abs(DoorX - DoorUser->X)
						+ FMath::Abs(DoorY - DoorUser->Y)
						+ FMath::Abs(DoorZ - DoorUser->Z) == 1;
					const int32 UpdatedRequiredDoorActions = UpdatedDoorCell.bDoorOpen ? 1 : 2;
					if (bNowAdjacent && DoorUser->RemainingActionPoints >= DoorActionPointCost * UpdatedRequiredDoorActions)
					{
						bReadyToCycleDoor = true;
						break;
					}

					FEndTacticalTurnCommand EndTurn;
					EndTurn.ExpectedSequence = DemoState.CommandSequence;
					EndTurn.BattleId = CurrentSnapshot.BattleId;
					const FStrategicCommandResult Ended = Instance->EndTacticalTurn(EndTurn);
					PresentStrategicCommandResult(
						Ended,
						TEXT("Historical-fog review handed control to the adversary."));
					if (!Ended.bAccepted)
					{
						break;
					}

					DemoState = Instance->GetCampaignState();
					Battle = DemoState.TacticalBattles.FindByPredicate(
						[this](const FTacticalBattleState& Entry) { return Entry.BattleId == CurrentSnapshot.BattleId; });
					if (Battle != nullptr && Battle->Phase == ETacticalBattlePhase::AdversaryTurn)
					{
						FRunTacticalAiTurnCommand RunAi;
						RunAi.ExpectedSequence = DemoState.CommandSequence;
						RunAi.BattleId = CurrentSnapshot.BattleId;
						const FStrategicCommandResult AiResolved = Instance->RunTacticalAiTurn(RunAi);
						PresentStrategicCommandResult(
							AiResolved,
							TEXT("Historical-fog review adversary phase resolved."));
						if (!AiResolved.bAccepted)
						{
							break;
						}
					}
				}

				if (bReadyToCycleDoor)
				{
					DemoState = Instance->GetCampaignState();
					Battle = DemoState.TacticalBattles.FindByPredicate(
						[this](const FTacticalBattleState& Entry) { return Entry.BattleId == CurrentSnapshot.BattleId; });
					const FTacticalCellState* DoorCell = Battle != nullptr && Battle->IsWithinGrid(DoorX, DoorY, DoorZ)
						? &Battle->Cells[Battle->GetCellIndex(DoorX, DoorY, DoorZ)]
						: nullptr;
					FSetTacticalDoorCommand DoorCommand;
					DoorCommand.ExpectedSequence = DemoState.CommandSequence;
					DoorCommand.BattleId = CurrentSnapshot.BattleId;
					DoorCommand.UnitId = UnitId;
					DoorCommand.TargetX = DoorX;
					DoorCommand.TargetY = DoorY;
					DoorCommand.TargetZ = DoorZ;
					DoorCommand.bOpen = DoorCell != nullptr && !DoorCell->bDoorOpen;
					const FStrategicCommandResult FirstDoorAction = Instance->SetTacticalDoor(DoorCommand);
					PresentStrategicCommandResult(
						FirstDoorAction,
						DoorCommand.bOpen
							? TEXT("Signal barrier opened; far-side terrain acquired.")
							: TEXT("Signal barrier closed; acquired terrain retained as memory."));
					if (FirstDoorAction.bAccepted && DoorCommand.bOpen)
					{
						DoorCommand.ExpectedSequence = Instance->GetCampaignState().CommandSequence;
						DoorCommand.bOpen = false;
						const FStrategicCommandResult Closed = Instance->SetTacticalDoor(DoorCommand);
						PresentStrategicCommandResult(
							Closed,
							TEXT("Signal barrier closed; acquired terrain retained as memory."));
						bDoorCycleCompleted = Closed.bAccepted;
					}
					else
					{
						bDoorCycleCompleted = FirstDoorAction.bAccepted;
					}
				}
			}
			if (HudWidget != nullptr)
			{
				HudWidget->ShowStatusMessage(FString());
			}
			UE_LOG(LogTemp, Display,
				TEXT("UEGTHistoricalFogDemo final state: source=authoritative-domain battle=%s doorCycle=%s visible=%d known=%d memory=%d boardMemory=%d sequence=%lld"),
				CurrentSnapshot.bSucceeded ? TEXT("true") : TEXT("false"),
				bDoorCycleCompleted ? TEXT("true") : TEXT("false"),
				CurrentSnapshot.VisibleCellCount,
				CurrentSnapshot.KnownCellCount,
				FMath::Max(0, CurrentSnapshot.KnownCellCount - CurrentSnapshot.VisibleCellCount),
				BoardActor != nullptr ? BoardActor->GetRenderedFogMemoryCount() : 0,
				CurrentSnapshot.ExpectedCommandSequence);
		}
	}
	const bool bMagazineDemo = FParse::Param(FCommandLine::Get(), TEXT("UEGTMagazineDemo"));
	const bool bSignalDemo = FParse::Param(FCommandLine::Get(), TEXT("UEGTSignalDemo"));
	if (bMagazineDemo || bSignalDemo)
	{
		CurrentSnapshot = FTacticalHudSnapshot();
		CurrentSnapshot.bSucceeded = true;
		CurrentSnapshot.BattleId = FGuid(0x4d41475a, 0x494e4544, 0x454d4f00, 0x00000001);
		CurrentSnapshot.OperationId = FGuid(0x4d41475a, 0x4f504552, 0x4154494f, 0x4e000001);
		CurrentSnapshot.OperationType = ETacticalOperationType::SiteRecovery;
		CurrentSnapshot.ExpectedCommandSequence = 1;
		CurrentSnapshot.MissionRuleId = TEXT("tactical.glass-wreck-recovery");
		CurrentSnapshot.MissionDisplayName = bSignalDemo
			? TEXT("Resonance Pressure Sweep")
			: TEXT("Glass Wreck Recovery");
		CurrentSnapshot.bRequiresExtraction = true;
		CurrentSnapshot.Width = 14;
		CurrentSnapshot.Height = 10;
		CurrentSnapshot.Levels = 1;
		CurrentSnapshot.ViewedLevel = 0;
		CurrentSnapshot.TurnNumber = 4;
		CurrentSnapshot.TurnLimit = 40;
		CurrentSnapshot.Phase = ETacticalBattlePhase::PlayerTurn;
		CurrentSnapshot.ActiveTeam = ETacticalTeam::Player;
		CurrentSnapshot.WindDirection = ETacticalWindDirection::East;
		CurrentSnapshot.WindStrength = 2;
		CurrentSnapshot.CargoMass = 4;
		CurrentSnapshot.CargoCapacity = 80;
		CurrentSnapshot.EffectiveWeaponItemId = TEXT("item.service-rifle");
		CurrentSnapshot.EffectiveSignalProjectorItemId = bSignalDemo
			? FName(TEXT("item.field-scanner"))
			: NAME_None;

		for (int32 Y = 0; Y < CurrentSnapshot.Height; ++Y)
		{
			for (int32 X = 0; X < CurrentSnapshot.Width; ++X)
			{
				FTacticalHudCellView Cell;
				Cell.CellIndex = Y * CurrentSnapshot.Width + X;
				Cell.X = X;
				Cell.Y = Y;
				Cell.Z = 0;
				Cell.bCurrentlyVisible = true;
				Cell.TerrainRuleId = TEXT("terrain.slate-mesh-deck");
				Cell.TerrainDisplayName = TEXT("Slate Mesh Deck");
				Cell.MoveCost = 1;
				const bool bPerimeter = X == 0 || Y == 0
					|| X == CurrentSnapshot.Width - 1 || Y == CurrentSnapshot.Height - 1;
				const bool bInteriorCover = (X == 7 && Y >= 2 && Y <= 7 && Y != 5)
					|| (Y == 7 && X >= 9 && X <= 11);
				if (bPerimeter || bInteriorCover)
				{
					Cell.TerrainRuleId = TEXT("terrain.prismatic-bulkhead");
					Cell.TerrainDisplayName = TEXT("Prismatic Bulkhead");
					Cell.CurrentIntegrity = 100;
					Cell.MaxIntegrity = 100;
					Cell.CoverPercent = 35;
					Cell.bBlocksMovement = true;
					Cell.bBlocksVision = true;
				}
				if (X == 7 && Y == 5)
				{
					Cell.TerrainRuleId = TEXT("terrain.resonance-hatch");
					Cell.TerrainDisplayName = TEXT("Resonance Hatch");
					Cell.CurrentIntegrity = 80;
					Cell.MaxIntegrity = 80;
					Cell.CoverPercent = 20;
					Cell.bBlocksMovement = false;
					Cell.bBlocksVision = false;
					Cell.bIsDoor = true;
					Cell.bDoorOpen = true;
				}
				if ((X == 10 || X == 11) && Y == 4)
				{
					Cell.Smoke = X == 10 ? 45 : 25;
					Cell.Fire = X == 10 ? 35 : 0;
				}
				CurrentSnapshot.VisibleCells.Add(Cell);
				CurrentSnapshot.KnownCells.Add(Cell);
			}
		}
		CurrentSnapshot.VisibleCellCount = CurrentSnapshot.VisibleCells.Num();
		CurrentSnapshot.KnownCellCount = CurrentSnapshot.KnownCells.Num();

		FTacticalHudUnitView SelectedAgent;
		SelectedAgent.UnitId = FGuid(0x41524957, 0x45535400, 0x00000000, 0x00000001);
		SelectedAgent.PersonnelId = FGuid(0x41524957, 0x45535400, 0x50455253, 0x00000001);
		SelectedAgent.DisplayName = TEXT("Ari West");
		SelectedAgent.Team = ETacticalTeam::Player;
		SelectedAgent.Stance = ETacticalStance::Crouched;
		SelectedAgent.X = 4;
		SelectedAgent.Y = 5;
		SelectedAgent.CurrentHealth = 44;
		SelectedAgent.MaxHealth = 48;
		SelectedAgent.RemainingActionPoints = 8;
		SelectedAgent.MaxActionPoints = 12;
		SelectedAgent.CurrentMorale = 78;
		SelectedAgent.MaxMorale = 100;
		SelectedAgent.Suppression = 8;
		SelectedAgent.bSelected = true;
		SelectedAgent.bControllable = true;
		FTacticalHudWeaponView Rifle;
		Rifle.ItemId = TEXT("item.service-rifle");
		Rifle.DisplayName = TEXT("Service Rifle");
		Rifle.Range = 14;
		Rifle.SingleActionPointCost = 4;
		Rifle.LoadedAmmunition = 4;
		Rifle.MagazineCapacity = 6;
		Rifle.AmmunitionItemId = TEXT("item.service-rifle-magazine");
		Rifle.ReserveMagazines = 2;
		Rifle.FullReserveMagazines = 1;
		Rifle.PartialReserveMagazines = 1;
		Rifle.ReserveAmmunition = 9;
		Rifle.NextReloadAmmunition = 6;
		Rifle.ReloadActionPointCost = 2;
		SelectedAgent.Weapons.Add(Rifle);
		CurrentSnapshot.Units.Add(SelectedAgent);

		FTacticalHudUnitView Wingmate;
		Wingmate.UnitId = FGuid(0x4d415241, 0x56454e4e, 0x00000000, 0x00000001);
		Wingmate.PersonnelId = FGuid(0x4d415241, 0x56454e4e, 0x50455253, 0x00000001);
		Wingmate.DisplayName = TEXT("Mara Venn");
		Wingmate.Team = ETacticalTeam::Player;
		Wingmate.X = 3;
		Wingmate.Y = 7;
		Wingmate.CurrentHealth = 51;
		Wingmate.MaxHealth = 51;
		Wingmate.RemainingActionPoints = 5;
		Wingmate.MaxActionPoints = 12;
		Wingmate.CurrentMorale = 84;
		Wingmate.MaxMorale = 100;
		Wingmate.bControllable = true;
		CurrentSnapshot.Units.Add(Wingmate);

		for (int32 ContactIndex = 0; ContactIndex < 2; ++ContactIndex)
		{
			FTacticalHudUnitView Contact;
			Contact.UnitId = FGuid(0x434f4e54, 0x41435400, 0x00000000, ContactIndex + 1);
			Contact.SourceRuleId = TEXT("unit.glass-tide-scout");
			Contact.DisplayName = FString::Printf(TEXT("CONTACT %02d"), ContactIndex + 3);
			Contact.Team = ETacticalTeam::Adversary;
			Contact.X = 10 + ContactIndex;
			Contact.Y = 5 + ContactIndex * 2;
			Contact.CurrentHealth = ContactIndex == 0 ? 31 : 42;
			Contact.MaxHealth = 42;
			Contact.RemainingActionPoints = 10;
			Contact.MaxActionPoints = 10;
			Contact.CurrentMorale = 62;
			Contact.MaxMorale = 100;
			Contact.Suppression = ContactIndex == 0 ? 26 : 4;
			CurrentSnapshot.Units.Add(Contact);
		}
		CurrentSnapshot.LivingPlayerUnitCount = 2;
		CurrentSnapshot.VisibleAdversaryUnitCount = 2;

		FTacticalHudObjectiveView Objective;
		Objective.ObjectiveId = TEXT("objective.secure-resonance-core");
		Objective.Type = ETacticalObjectiveType::Recover;
		Objective.Status = ETacticalObjectiveStatus::Active;
		Objective.X = 11;
		Objective.Y = 3;
		Objective.PlayerInteractions = 1;
		Objective.RequiredInteractions = 2;
		Objective.RewardItemId = TEXT("item.resonance-shard");
		Objective.RewardDisplayName = FUEGTLocalizationService::ContentName(
			Objective.RewardItemId, TEXT("Resonance Shard"));
		Objective.RewardQuantity = 2;
		CurrentSnapshot.Objectives.Add(Objective);

		FTacticalHudItemView Cargo;
		Cargo.ItemId = TEXT("item.resonance-shard");
		Cargo.DisplayName = FUEGTLocalizationService::ContentName(Cargo.ItemId, TEXT("Resonance Shard"));
		Cargo.Quantity = 1;
		Cargo.UnitMass = 4;
		CurrentSnapshot.Cargo.Add(Cargo);

		const TArray<ETacticalHudActionType> ActionTypes = {
			ETacticalHudActionType::ConfirmDeployment,
			ETacticalHudActionType::Move,
			ETacticalHudActionType::AttackUnit,
			ETacticalHudActionType::ProjectSignal,
			ETacticalHudActionType::AttackTerrain,
			ETacticalHudActionType::Reload,
			ETacticalHudActionType::EjectMagazine,
			ETacticalHudActionType::ChangeStance,
			ETacticalHudActionType::OperateDoor,
			ETacticalHudActionType::DeployDevice,
			ETacticalHudActionType::InteractObjective,
			ETacticalHudActionType::Extract,
			ETacticalHudActionType::EndTurn
		};
		for (const ETacticalHudActionType ActionType : ActionTypes)
		{
			FTacticalHudActionAvailability Action;
			Action.ActionType = ActionType;
			Action.UnitId = SelectedAgent.UnitId;
			Action.UnavailableReasonCode = TEXT("demo_action_unavailable");
			Action.UnavailableReason = TEXT("This presentation fixture highlights exact magazine controls.");
			CurrentSnapshot.Actions.Add(Action);
		}
		auto EnableAction = [this, &SelectedAgent, &Rifle](const ETacticalHudActionType Type, const int32 Cost)
		{
			FTacticalHudActionAvailability* Action = CurrentSnapshot.Actions.FindByPredicate(
				[Type](const FTacticalHudActionAvailability& Candidate)
				{
					return Candidate.ActionType == Type;
				});
			if (Action != nullptr)
			{
				Action->bAvailable = true;
				Action->ActionPointCost = Cost;
				Action->UnavailableReasonCode = NAME_None;
				Action->UnavailableReason.Reset();
				Action->UnitId = SelectedAgent.UnitId;
				Action->ItemId = Rifle.ItemId;
			}
		};
		EnableAction(ETacticalHudActionType::Reload, Rifle.ReloadActionPointCost);
		EnableAction(ETacticalHudActionType::EjectMagazine, Rifle.ReloadActionPointCost);
		EnableAction(ETacticalHudActionType::ChangeStance, 1);
		EnableAction(ETacticalHudActionType::EndTurn, 0);
		if (bSignalDemo)
		{
			EnableAction(ETacticalHudActionType::ProjectSignal, 4);
			if (FTacticalHudActionAvailability* SignalAction = CurrentSnapshot.Actions.FindByPredicate(
				[](const FTacticalHudActionAvailability& Candidate)
				{
					return Candidate.ActionType == ETacticalHudActionType::ProjectSignal;
				}))
			{
				SignalAction->ItemId = TEXT("item.field-scanner");
				SignalAction->TargetUnitId = CurrentSnapshot.Units[2].UnitId;
				SignalAction->TargetX = CurrentSnapshot.Units[2].X;
				SignalAction->TargetY = CurrentSnapshot.Units[2].Y;
				SignalAction->TargetZ = CurrentSnapshot.Units[2].Z;
			}
			CurrentSnapshot.Hover.bHasCell = true;
			CurrentSnapshot.Hover.bCellVisible = true;
			CurrentSnapshot.Hover.X = CurrentSnapshot.Units[2].X;
			CurrentSnapshot.Hover.Y = CurrentSnapshot.Units[2].Y;
			CurrentSnapshot.Hover.Z = CurrentSnapshot.Units[2].Z;
			CurrentSnapshot.Hover.bHasSignalPreview = true;
			CurrentSnapshot.Hover.Signal.bSucceeded = true;
			CurrentSnapshot.Hover.Signal.AttackerUnitId = SelectedAgent.UnitId;
			CurrentSnapshot.Hover.Signal.TargetUnitId = CurrentSnapshot.Units[2].UnitId;
			CurrentSnapshot.Hover.Signal.SignalRuleId = TEXT("item.field-scanner");
			CurrentSnapshot.Hover.Signal.Distance = 6;
			CurrentSnapshot.Hover.Signal.MaximumRange = 8;
			CurrentSnapshot.Hover.Signal.ActionPointCost = 4;
			CurrentSnapshot.Hover.Signal.SignalPower = 16;
			CurrentSnapshot.Hover.Signal.HitChance = 71;
			CurrentSnapshot.Hover.Signal.MoraleDamage = 13;
			CurrentSnapshot.Hover.Signal.SuppressionGain = 9;
		}
		if (FTacticalHudActionAvailability* StanceAction = CurrentSnapshot.Actions.FindByPredicate(
			[](const FTacticalHudActionAvailability& Candidate)
			{
				return Candidate.ActionType == ETacticalHudActionType::ChangeStance;
			}))
		{
			StanceAction->RequestedStance = ETacticalStance::Standing;
		}

		bStrategicMode = false;
		bViewingDebrief = false;
		bGlobeFocused = false;
		ActiveBattleId = CurrentSnapshot.BattleId;
		SelectedUnitId = SelectedAgent.UnitId;
		SelectedWeaponItemId = Rifle.ItemId;
		ViewedLevel = 0;
		bHasHoveredCell = bSignalDemo;
		HoveredX = bSignalDemo ? CurrentSnapshot.Units[2].X : 0;
		HoveredY = bSignalDemo ? CurrentSnapshot.Units[2].Y : 0;
		HoveredZ = 0;
		HoveredUnitId = bSignalDemo ? CurrentSnapshot.Units[2].UnitId : FGuid();
		HoveredObjectiveId = NAME_None;
		if (BoardActor != nullptr)
		{
			BoardActor->SetActorHiddenInGame(false);
			BoardActor->SetActorEnableCollision(false);
			BoardActor->ApplySnapshot(CurrentSnapshot);
		}
		if (GlobeActor != nullptr)
		{
			GlobeActor->SetPresentationEnabled(false);
		}
		if (StrategicHudWidget != nullptr)
		{
			StrategicHudWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (HudWidget != nullptr)
		{
			HudWidget->SetVisibility(ESlateVisibility::Visible);
			HudWidget->ApplySnapshot(CurrentSnapshot);
			if (bSignalDemo)
			{
				HudWidget->ShowStatusMessage(UEGTTacticalControllerPrivate::LocalizedFormat(
					TEXT("tactical.signal-result-format"),
					TEXT("Signal pressure {0} • roll {1} / {2} • morale −{3}."),
					{
						FUEGTLocalizationService::Text(
							TEXT("tactical.signal-lock-held"), TEXT("LOCK HELD")),
						TEXT("43"),
						TEXT("71"),
						TEXT("13")
					}));
			}
			else
			{
				HudWidget->ShowStatusMessage(UEGTTacticalControllerPrivate::LocalizedFormat(
					TEXT("tactical.magazine-ejected-format"),
					TEXT("Ejected {0} with {1} rounds retained."),
					{
						FUEGTLocalizationService::ContentName(Rifle.ItemId, Rifle.DisplayName),
						TEXT("3")
					}));
			}
		}
		if (AUEGTTacticalCameraPawn* CameraPawn = GetTacticalCameraPawn())
		{
			CameraPawn->FocusBoard(
				CurrentSnapshot.Width,
				CurrentSnapshot.Height,
				CurrentSnapshot.ViewedLevel,
				BoardActor != nullptr ? BoardActor->GetCellSize() : 100.0f,
				BoardActor != nullptr ? BoardActor->GetLevelHeight() : 180.0f);
		}
		LastFocusedBattleId = CurrentSnapshot.BattleId;
		LastPresentedSequence = CurrentSnapshot.ExpectedCommandSequence;
		if (bSignalDemo)
		{
			const FTacticalHudActionAvailability* SignalAction =
				CurrentSnapshot.FindAction(ETacticalHudActionType::ProjectSignal);
			UE_LOG(LogTemp, Display,
				TEXT("UEGTSignalDemo final state: source=presentation-fixture projector=%s target=%s chance=%d morale=%d suppression=%d action=%s hudUnits=%d hudActions=%d sequence=%lld"),
				*CurrentSnapshot.EffectiveSignalProjectorItemId.ToString(),
				*CurrentSnapshot.Units[2].UnitId.ToString(),
				CurrentSnapshot.Hover.Signal.HitChance,
				CurrentSnapshot.Hover.Signal.MoraleDamage,
				CurrentSnapshot.Hover.Signal.SuppressionGain,
				SignalAction != nullptr && SignalAction->bAvailable ? TEXT("true") : TEXT("false"),
				HudWidget != nullptr ? HudWidget->GetRenderedUnitSummaries().Num() : 0,
				HudWidget != nullptr ? HudWidget->GetRenderedActionLabels().Num() : 0,
				CurrentSnapshot.ExpectedCommandSequence);
		}
		else
		{
			const FTacticalHudActionAvailability* BlockedAction =
				CurrentSnapshot.FindAction(ETacticalHudActionType::Move);
			UE_LOG(LogTemp, Display,
				TEXT("UEGTMagazineDemo final state: source=presentation-fixture loaded=%d capacity=%d reserves=%d reserveRounds=%d partial=%d next=%d eject=%s hudUnits=%d hudActions=%d blocked_code=%s blocked_localized=\"%s\" sequence=%lld"),
				Rifle.LoadedAmmunition, Rifle.MagazineCapacity, Rifle.ReserveMagazines,
				Rifle.ReserveAmmunition, Rifle.PartialReserveMagazines, Rifle.NextReloadAmmunition,
				CurrentSnapshot.FindAction(ETacticalHudActionType::EjectMagazine) != nullptr
					&& CurrentSnapshot.FindAction(ETacticalHudActionType::EjectMagazine)->bAvailable
					? TEXT("true") : TEXT("false"),
				HudWidget != nullptr ? HudWidget->GetRenderedUnitSummaries().Num() : 0,
				HudWidget != nullptr ? HudWidget->GetRenderedActionLabels().Num() : 0,
				BlockedAction != nullptr ? *BlockedAction->UnavailableReasonCode.ToString() : TEXT("none"),
				BlockedAction != nullptr
					? *UEGTTacticalControllerPrivate::LocalizedDiagnostic(
						BlockedAction->UnavailableReasonCode, BlockedAction->UnavailableReason)
					: TEXT("none"),
				CurrentSnapshot.ExpectedCommandSequence);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTFoundingDemo")))
	{
		StartStrategicCampaign(
			ECampaignDifficulty::Cadet, 20350101, EUEGTFundingModel::BalancedMandate);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTSettings")))
	{
		ToggleSettings();
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTControlSettings")) && StrategicHudWidget != nullptr)
	{
		StrategicHudWidget->ShowControlSettings();
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTGameplaySettings")) && StrategicHudWidget != nullptr)
	{
		StrategicHudWidget->ShowGameplaySettings();
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTSaveBrowser")) && StrategicHudWidget != nullptr)
	{
		const UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
		StrategicHudWidget->ShowSaveBrowser(Instance != nullptr && Instance->HasActiveCampaign());
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTArchiveDemo")) && StrategicHudWidget != nullptr)
	{
		StrategicHudWidget->ShowKnowledgeArchive();
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTPlacementDemo")) && StrategicHudWidget != nullptr)
	{
		StrategicHudWidget->SelectFacilityForPlacement(TEXT("facility.secure-storage"));
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTAudioDemo")))
	{
		if (StrategicHudWidget != nullptr)
		{
			StrategicHudWidget->ShowSettings();
		}
		PreviewAudioCue();
		GetWorldTimerManager().SetTimer(
			RuntimeAudioDemoTimerHandle,
			this,
			&AUEGTTacticalPlayerController::LogRuntimeAudioDemo,
			0.25f,
			false);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("UEGTAutoScreenshot")))
	{
		GetWorldTimerManager().SetTimer(
			RuntimeScreenshotTimerHandle,
			this,
			&AUEGTTacticalPlayerController::CaptureRuntimeScreenshot,
			2.0f,
			false);
	}
#endif
}

void AUEGTTacticalPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RuntimeAudioDemoTimerHandle);
	if (AudioDirector != nullptr)
	{
		AudioDirector->Shutdown();
		AudioDirector = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AUEGTTacticalPlayerController::CaptureRuntimeScreenshot()
{
	FScreenshotRequest::RequestScreenshot(true, true);
}

void AUEGTTacticalPlayerController::PrepareRecoveryPlanDemo()
{
#if !UE_BUILD_SHIPPING
	StartStrategicCampaign(
		ECampaignDifficulty::Cadet, 0x71555041, EUEGTFundingModel::BalancedMandate);
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	FEstablishBaseCommand Establish;
	Establish.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Establish.BaseId = FGuid(0x71550011, 0x71550012, 0x71550013, 0x71550014);
	Establish.Name = TEXT("Cascadia Return Command");
	Establish.RegionId = TEXT("region.cascadia");
	Establish.LongitudeMilliDegrees = -123120;
	Establish.LatitudeMilliDegrees = 49280;
	Establish.StartingFacilities = { TEXT("facility.operations-hub") };
	const FStrategicCommandResult Established = Instance->EstablishBase(Establish);
	PresentStrategicCommandResult(
		Established, TEXT("Return Path presentation fixture established."));
	if (!Established.bAccepted || CurrentStrategicSnapshot.Bases.IsEmpty())
	{
		return;
	}

	const FGuid PersonnelId(0x71550021, 0x71550022, 0x71550023, 0x71550024);
	FCampaignState RecoveryCampaign;
	RecoveryCampaign.Funds = 100000;
	FPersonnelState& RecoveryState = RecoveryCampaign.Personnel.AddDefaulted_GetRef();
	RecoveryState.PersonnelId = PersonnelId;
	RecoveryState.BaseId = Establish.BaseId;
	RecoveryState.DisplayName = TEXT("Maëlle Venn");
	RecoveryState.RoleId = TEXT("role.field-agent");
	RecoveryState.Status = EPersonnelStatus::Recovering;
	RecoveryState.MaxHealth = 55;
	RecoveryState.CurrentHealth = 45;
	RecoveryState.Accuracy = 61;
	RecoveryState.Resolve = 59;
	RecoveryState.Mobility = 60;
	RecoveryState.Strength = 55;
	RecoveryState.RemainingRecoverySeconds = int64(60) * 3600;
	RecoveryState.RecoveryPlan = EPersonnelRecoveryPlan::DecisionRequired;
	FStrategicSimulationConfig RecoveryConfig;

	CurrentStrategicSnapshot.Funds = RecoveryCampaign.Funds;
	CurrentStrategicSnapshot.Personnel.Reset();
	CurrentStrategicSnapshot.bDecisionRequired = true;
	CurrentStrategicSnapshot.bCanAdvanceTime = false;
	FStrategicPersonnelView& Agent = CurrentStrategicSnapshot.Personnel.AddDefaulted_GetRef();
	Agent.PersonnelId = PersonnelId;
	Agent.BaseId = Establish.BaseId;
	Agent.DisplayName = RecoveryState.DisplayName;
	Agent.RoleId = RecoveryState.RoleId;
	Agent.RoleDisplayName = TEXT("Field Agent");
	Agent.RoleCategory = EPersonnelRoleCategory::FieldAgent;
	Agent.Status = TEXT("Recovering");
	Agent.StatusType = EPersonnelStatus::Recovering;
	Agent.ServiceHistory = FPersonnelServiceHistory::Project(0);
	Agent.CurrentHealth = RecoveryState.CurrentHealth;
	Agent.MaxHealth = RecoveryState.MaxHealth;
	Agent.Accuracy = RecoveryState.Accuracy;
	Agent.Resolve = RecoveryState.Resolve;
	Agent.Mobility = RecoveryState.Mobility;
	Agent.Strength = RecoveryState.Strength;
	Agent.RemainingRecoverySeconds = RecoveryState.RemainingRecoverySeconds;
	Agent.RecoveryPlan = FPersonnelRecoveryPlan::Evaluate(
		RecoveryCampaign, RecoveryConfig, PersonnelId);

	if (StrategicHudWidget != nullptr)
	{
		StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
		StrategicHudWidget->ShowStatusMessage(FString());
	}
	// Command-line culture is applied after BeginPlay. Reapply the immutable fixture once
	// localization has settled so packaged captures retain the exact Return Path view.
	const FStrategicDashboardSnapshot RecoveryFixture = CurrentStrategicSnapshot;
	FTimerDelegate RecoveryFixtureDelegate;
	RecoveryFixtureDelegate.BindWeakLambda(this, [this, RecoveryFixture]()
	{
		CurrentStrategicSnapshot = RecoveryFixture;
		if (StrategicHudWidget != nullptr)
		{
			StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
			StrategicHudWidget->ShowStatusMessage(FString());
		}
	});
	FTimerHandle RecoveryFixtureTimer;
	GetWorldTimerManager().SetTimer(
		RecoveryFixtureTimer, RecoveryFixtureDelegate, 0.1f, false);

	const FPersonnelRecoveryPlanOptionView* Measured = Agent.RecoveryPlan.Options.FindByPredicate(
		[](const FPersonnelRecoveryPlanOptionView& Option)
		{
			return Option.Plan == EPersonnelRecoveryPlan::MeasuredReturn;
		});
	const FPersonnelRecoveryPlanOptionView* Surge = Agent.RecoveryPlan.Options.FindByPredicate(
		[](const FPersonnelRecoveryPlanOptionView& Option)
		{
			return Option.Plan == EPersonnelRecoveryPlan::SurgeCare;
		});
	const FPersonnelRecoveryPlanOptionView* Reflection = Agent.RecoveryPlan.Options.FindByPredicate(
		[](const FPersonnelRecoveryPlanOptionView& Option)
		{
			return Option.Plan == EPersonnelRecoveryPlan::ReflectionCycle;
		});
	UE_LOG(LogTemp, Display,
		TEXT("UEGT_RETURN_PATH_RUNTIME_OK source=presentation-fixture personnel=%d options=%d measured=%lld surge=%lld surgeCost=%lld reflection=%lld resolve=%d decision=%s"),
		CurrentStrategicSnapshot.Personnel.Num(),
		Agent.RecoveryPlan.Options.Num(),
		Measured != nullptr ? Measured->DurationSeconds : 0,
		Surge != nullptr ? Surge->DurationSeconds : 0,
		Surge != nullptr ? Surge->FundingCost : 0,
		Reflection != nullptr ? Reflection->DurationSeconds : 0,
		Reflection != nullptr ? Reflection->ResolveBonus : 0,
		Agent.RecoveryPlan.bDecisionRequired ? TEXT("true") : TEXT("false"));
#endif
}

void AUEGTTacticalPlayerController::PrepareStewardshipDemo()
{
#if !UE_BUILD_SHIPPING
	StartStrategicCampaign(
		ECampaignDifficulty::Cadet, 0x57e54041, EUEGTFundingModel::BalancedMandate);
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}

	FEstablishBaseCommand Establish;
	Establish.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Establish.BaseId = FGuid(0x57e50011, 0x57e50012, 0x57e50013, 0x57e50014);
	Establish.Name = TEXT("Cascadia Stewardship Command");
	Establish.RegionId = TEXT("region.cascadia");
	Establish.LongitudeMilliDegrees = -123120;
	Establish.LatitudeMilliDegrees = 49280;
	Establish.StartingFacilities = { TEXT("facility.operations-hub") };
	const FStrategicCommandResult Established = Instance->EstablishBase(Establish);
	PresentStrategicCommandResult(
		Established, TEXT("Stewardship Rotation presentation fixture established."));
	if (!Established.bAccepted || CurrentStrategicSnapshot.Bases.IsEmpty())
	{
		return;
	}

	const FGuid PersonnelId(0x57e50021, 0x57e50022, 0x57e50023, 0x57e50024);
	FCampaignState StewardshipCampaign;
	FPersonnelState& StewardState = StewardshipCampaign.Personnel.AddDefaulted_GetRef();
	StewardState.PersonnelId = PersonnelId;
	StewardState.BaseId = Establish.BaseId;
	StewardState.DisplayName = TEXT("Maëlle Venn");
	StewardState.RoleId = TEXT("role.field-agent");
	StewardState.Status = EPersonnelStatus::Stewarding;
	StewardState.Rank = 5;
	StewardState.Missions = 18;
	StewardState.Kills = 11;
	StewardState.Experience = 2300;
	StewardState.MaxHealth = 62;
	StewardState.CurrentHealth = 62;
	StewardState.Accuracy = 68;
	StewardState.Resolve = 72;
	StewardState.Mobility = 61;
	StewardState.Strength = 63;
	StewardState.StewardshipFocus = EPersonnelStewardshipFocus::TrainingCadre;
	StewardState.RemainingStewardshipSeconds = int64(12) * 86400;
	StewardState.StewardshipToursCompleted = 1;
	FStrategicSimulationConfig StewardshipConfig;

	CurrentStrategicSnapshot.Personnel.Reset();
	CurrentStrategicSnapshot.bDecisionRequired = false;
	CurrentStrategicSnapshot.bCanAdvanceTime = true;
	FStrategicPersonnelView& Agent = CurrentStrategicSnapshot.Personnel.AddDefaulted_GetRef();
	Agent.PersonnelId = PersonnelId;
	Agent.BaseId = Establish.BaseId;
	Agent.DisplayName = StewardState.DisplayName;
	Agent.RoleId = StewardState.RoleId;
	Agent.RoleDisplayName = TEXT("Field Agent");
	Agent.RoleCategory = EPersonnelRoleCategory::FieldAgent;
	Agent.Status = TEXT("Stewarding");
	Agent.StatusType = EPersonnelStatus::Stewarding;
	Agent.Rank = StewardState.Rank;
	Agent.Missions = StewardState.Missions;
	Agent.Kills = StewardState.Kills;
	Agent.Experience = StewardState.Experience;
	Agent.ServiceHistory = FPersonnelServiceHistory::Project(StewardState.Missions);
	Agent.CurrentHealth = StewardState.CurrentHealth;
	Agent.MaxHealth = StewardState.MaxHealth;
	Agent.Accuracy = StewardState.Accuracy;
	Agent.Resolve = StewardState.Resolve;
	Agent.Mobility = StewardState.Mobility;
	Agent.Strength = StewardState.Strength;
	Agent.Stewardship = FPersonnelStewardship::Evaluate(
		StewardshipCampaign, Instance->GetLoadedRules(), StewardshipConfig, PersonnelId);
	Agent.StewardshipToursCompleted = StewardState.StewardshipToursCompleted;

	if (StrategicHudWidget != nullptr)
	{
		StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
		StrategicHudWidget->ShowStatusMessage(FString());
	}
	const FStrategicDashboardSnapshot StewardshipFixture = CurrentStrategicSnapshot;
	FTimerDelegate StewardshipFixtureDelegate;
	StewardshipFixtureDelegate.BindWeakLambda(this, [this, StewardshipFixture]()
	{
		CurrentStrategicSnapshot = StewardshipFixture;
		if (StrategicHudWidget != nullptr)
		{
			StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
			StrategicHudWidget->ShowStatusMessage(FString());
			StrategicHudWidget->FocusPersonnelPanel();
		}
	});
	FTimerHandle StewardshipFixtureTimer;
	GetWorldTimerManager().SetTimer(
		StewardshipFixtureTimer, StewardshipFixtureDelegate, 0.1f, false);

	UE_LOG(LogTemp, Display,
		TEXT("UEGT_STEWARDSHIP_RUNTIME_OK source=presentation-fixture personnel=%d status=%d focus=%d remaining=%lld reduction=%d tours=%d reward=%d policy=%s"),
		CurrentStrategicSnapshot.Personnel.Num(),
		static_cast<int32>(Agent.StatusType),
		static_cast<int32>(Agent.Stewardship.ActiveFocus),
		Agent.Stewardship.RemainingSeconds,
		Agent.Stewardship.ReductionPercent,
		Agent.Stewardship.ToursCompleted,
		Agent.Stewardship.ResolveBonusOnCompletion,
		*Agent.Stewardship.ActivePolicyId.ToString());
#endif
}

void AUEGTTacticalPlayerController::PrepareWorksCadreDemo(
	const bool bWorksCharterDemo)
{
#if !UE_BUILD_SHIPPING
	StartStrategicCampaign(
		ECampaignDifficulty::Cadet,
		bWorksCharterDemo ? 0x058ca443 : 0x057cad41,
		EUEGTFundingModel::BalancedMandate);
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}

	const FGuid BaseId(0x057c0011, 0x057c0012, 0x057c0013, 0x057c0014);
	FEstablishBaseCommand Establish;
	Establish.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Establish.BaseId = BaseId;
	Establish.Name = TEXT("Cascadia Works Command");
	Establish.RegionId = TEXT("region.cascadia");
	Establish.LongitudeMilliDegrees = -123120;
	Establish.LatitudeMilliDegrees = 49280;
	Establish.StartingFacilities = { TEXT("facility.operations-hub") };
	const FStrategicCommandResult Established = Instance->EstablishBase(Establish);
	PresentStrategicCommandResult(
		Established, TEXT("Works Cadre runtime fixture established through the strategic domain."));
	if (!Established.bAccepted || CurrentStrategicSnapshot.Bases.IsEmpty())
	{
		return;
	}

	FSetWorksCadreStaffCommand Staff;
	Staff.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Staff.BaseId = BaseId;
	Staff.AssignedEngineers = FStrategicCommandService::WorksCadreMaximumEngineers();
	const FStrategicCommandResult Staffed = Instance->SetWorksCadreStaff(Staff);
	PresentStrategicCommandResult(
		Staffed, TEXT("Three engineers assigned to the Works Cadre."));
	if (!Staffed.bAccepted)
	{
		return;
	}
	if (bWorksCharterDemo)
	{
		FSetWorksCadreCharterCommand Assembly;
		Assembly.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
		Assembly.BaseId = BaseId;
		Assembly.Charter = EWorksCadreCharter::AssemblyCadence;
		const FStrategicCommandResult AssemblySelected =
			Instance->SetWorksCadreCharter(Assembly);
		PresentStrategicCommandResult(
			AssemblySelected,
			TEXT("Assembly Cadence selected for future Works Cadre commitments."));
		if (!AssemblySelected.bAccepted)
		{
			return;
		}
	}

	FStartFacilityConstructionCommand Construction;
	Construction.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Construction.ProjectId = FGuid(0x057c0021, 0x057c0022, 0x057c0023, 0x057c0024);
	Construction.FacilityInstanceId =
		FGuid(0x057c0031, 0x057c0032, 0x057c0033, 0x057c0034);
	Construction.BaseId = BaseId;
	Construction.FacilityId = TEXT("facility.secure-storage");
	Construction.GridX = 2;
	Construction.GridY = 0;
	const FStrategicCommandResult ConstructionStarted =
		Instance->StartFacilityConstruction(Construction);
	PresentStrategicCommandResult(
		ConstructionStarted, TEXT("Secure Storage mobilization started with Works Cadre support."));
	if (!ConstructionStarted.bAccepted)
	{
		return;
	}
	if (bWorksCharterDemo)
	{
		FSetWorksCadreCharterCommand Restoration;
		Restoration.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
		Restoration.BaseId = BaseId;
		Restoration.Charter = EWorksCadreCharter::RestorationCadence;
		const FStrategicCommandResult RestorationSelected =
			Instance->SetWorksCadreCharter(Restoration);
		PresentStrategicCommandResult(
			RestorationSelected,
			TEXT("Restoration Cadence selected without changing the committed build clock."));
		if (!RestorationSelected.bAccepted)
		{
			return;
		}
	}

	const FStrategicFacilityView* Operations =
		CurrentStrategicSnapshot.Bases[0].FacilityLayout.FindByPredicate(
			[](const FStrategicFacilityView& Facility)
			{
				return Facility.bOperational
					&& Facility.FacilityId == FName(TEXT("facility.operations-hub"));
			});
	if (Operations == nullptr)
	{
		return;
	}
	const FGuid OperationsId = Operations->FacilityInstanceId;
	FApplyFacilityDamageCommand Damage;
	Damage.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Damage.BaseId = BaseId;
	Damage.FacilityInstanceId = OperationsId;
	Damage.Damage = 20;
	const FStrategicCommandResult Damaged = Instance->ApplyFacilityDamage(Damage);
	PresentStrategicCommandResult(
		Damaged, TEXT("Operations Hub damage prepared for the Works Cadre repair projection."));
	if (!Damaged.bAccepted)
	{
		return;
	}

	const FCampaignState Campaign = Instance->GetCampaignState();
	FStartFacilityRepairCommand Repair;
	Repair.ExpectedSequence = Campaign.CommandSequence;
	Repair.BaseId = BaseId;
	Repair.FacilityInstanceId = OperationsId;
	const FFacilityRepairEvaluation RepairEvaluation =
		FStrategicCommandService::EvaluateFacilityRepair(
			Campaign, Instance->GetLoadedRules(), Repair);
	const FFacilityConstructionProjectState* Project =
		Campaign.FacilityConstructionProjects.FindByPredicate(
			[&Construction](const FFacilityConstructionProjectState& Entry)
			{
				return Entry.ProjectId == Construction.ProjectId;
			});
	const FStrategicBaseView* Base = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[BaseId](const FStrategicBaseView& Entry) { return Entry.BaseId == BaseId; });
	const FStrategicEvent* ConstructionEvent = ConstructionStarted.Events.FindByPredicate(
		[](const FStrategicEvent& Event)
		{
			return Event.Type == EStrategicEventType::FacilityConstructionStarted;
		});
	if (Project == nullptr || Base == nullptr || ConstructionEvent == nullptr
		|| !RepairEvaluation.bAllowed)
	{
		if (bWorksCharterDemo)
		{
			UE_LOG(LogTemp, Error,
				TEXT("UEGT_WORKS_CHARTER_RUNTIME_ERROR source=authoritative-domain project=%s base=%s event=%s repair=%s"),
				Project != nullptr ? TEXT("true") : TEXT("false"),
				Base != nullptr ? TEXT("true") : TEXT("false"),
				ConstructionEvent != nullptr ? TEXT("true") : TEXT("false"),
				RepairEvaluation.bAllowed ? TEXT("true") : TEXT("false"));
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("UEGT_WORKS_CADRE_RUNTIME_ERROR source=authoritative-domain project=%s base=%s event=%s repair=%s"),
				Project != nullptr ? TEXT("true") : TEXT("false"),
				Base != nullptr ? TEXT("true") : TEXT("false"),
				ConstructionEvent != nullptr ? TEXT("true") : TEXT("false"),
				RepairEvaluation.bAllowed ? TEXT("true") : TEXT("false"));
		}
		return;
	}

	if (StrategicHudWidget != nullptr)
	{
		StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
		StrategicHudWidget->ShowStatusMessage(FString());
	}
	const FStrategicDashboardSnapshot WorksCadreFixture = CurrentStrategicSnapshot;
	FTimerDelegate WorksCadreFixtureDelegate;
	WorksCadreFixtureDelegate.BindWeakLambda(this, [this, WorksCadreFixture]()
	{
		CurrentStrategicSnapshot = WorksCadreFixture;
		if (StrategicHudWidget != nullptr)
		{
			StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
			StrategicHudWidget->ShowStatusMessage(FString());
		}
	});
	FTimerHandle WorksCadreFixtureTimer;
	GetWorldTimerManager().SetTimer(
		WorksCadreFixtureTimer, WorksCadreFixtureDelegate, 0.1f, false);

	if (bWorksCharterDemo)
	{
		UE_LOG(LogTemp, Display,
			TEXT("UEGT_WORKS_CHARTER_RUNTIME_OK source=authoritative-domain charter=%d policy=%s constructionPolicy=%s options=%d engineers=%d constructionFrontload=%d repairFrontload=%d constructionBaseline=%lld constructionCommitted=%lld constructionRemaining=%lld repairBaseline=%lld repairCommitted=%lld assigned=%d capacity=%d sequence=%lld"),
			static_cast<int32>(Base->WorksCadreCharter),
			*Base->WorksCadreCharterPolicyId.ToString(),
			*ConstructionEvent->PolicyId.ToString(),
			Base->WorksCadreCharterOptions.Num(),
			Base->WorksCadreEngineers,
			Base->WorksCadreConstructionFrontloadPercent,
			Base->WorksCadreRepairFrontloadPercent,
			ConstructionEvent->FacilityBaselineDurationSeconds,
			ConstructionEvent->FacilityCommittedDurationSeconds,
			Project->RemainingBuildSeconds,
			RepairEvaluation.BaselineDurationSeconds,
			RepairEvaluation.DurationSeconds,
			Base->AssignedEngineers,
			Base->EngineerCapacity,
			Campaign.CommandSequence);
	}
	else
	{
		UE_LOG(LogTemp, Display,
			TEXT("UEGT_WORKS_CADRE_RUNTIME_OK source=authoritative-domain engineers=%d max=%d frontload=%d constructionBaseline=%lld constructionCommitted=%lld constructionRemaining=%lld repairBaseline=%lld repairCommitted=%lld assigned=%d capacity=%d policy=%s sequence=%lld"),
			Base->WorksCadreEngineers,
			Base->WorksCadreMaximumEngineers,
			Base->WorksCadreConstructionFrontloadPercent,
			ConstructionEvent->FacilityBaselineDurationSeconds,
			ConstructionEvent->FacilityCommittedDurationSeconds,
			Project->RemainingBuildSeconds,
			RepairEvaluation.BaselineDurationSeconds,
			RepairEvaluation.DurationSeconds,
			Base->AssignedEngineers,
			Base->EngineerCapacity,
			*Base->WorksCadrePolicyId.ToString(),
			Campaign.CommandSequence);
	}
#endif
}

void AUEGTTacticalPlayerController::PrepareMutualAidConvoyDemo()
{
#if !UE_BUILD_SHIPPING
	const bool bSignalWatchDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTSignalWatchDemo"));
	const bool bThreadlineRetuneDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTThreadlineRetuneDemo"));
	const bool bSignalSuretyDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTSignalSuretyDemo"));
	const bool bReliefPriorityDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTReliefPriorityDemo"));
	const bool bReliefStandDownDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTReliefStandDownDemo"));
	const bool bReliefDiversionDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTReliefDiversionDemo"));
	const bool bRelayWaypointDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTRelayWaypointDemo"));
	const bool bBalancedHandoffDemo =
		FParse::Param(FCommandLine::Get(), TEXT("UEGTBalancedHandoffDemo"));
	const bool bRelayWaypointFixtureDemo =
		bRelayWaypointDemo || bBalancedHandoffDemo;
	StartStrategicCampaign(
		ECampaignDifficulty::Cadet, 0x46a1d041,
		bReliefDiversionDemo || bRelayWaypointFixtureDemo
			? EUEGTFundingModel::RapidMobilization
			: EUEGTFundingModel::BalancedMandate);
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}

	const FGuid SourceBaseId(0x46a10011, 0x46a10012, 0x46a10013, 0x46a10014);
	const FGuid DestinationBaseId(0x46a20011, 0x46a20012, 0x46a20013, 0x46a20014);
	const FGuid AlternateDestinationBaseId(
		0x46a30011, 0x46a30012, 0x46a30013, 0x46a30014);
	FEstablishBaseCommand EstablishSource;
	EstablishSource.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	EstablishSource.BaseId = SourceBaseId;
	EstablishSource.Name = TEXT("Cascadia Mutual Aid Hub");
	EstablishSource.RegionId = TEXT("region.cascadia");
	EstablishSource.LongitudeMilliDegrees = -123120;
	EstablishSource.LatitudeMilliDegrees = 49280;
	EstablishSource.StartingFacilities = { TEXT("facility.operations-hub") };
	const FStrategicCommandResult SourceEstablished = Instance->EstablishBase(EstablishSource);
	PresentStrategicCommandResult(
		SourceEstablished, TEXT("Mutual Aid source fixture established."));
	if (!SourceEstablished.bAccepted)
	{
		return;
	}

	FEstablishBaseCommand EstablishDestination;
	EstablishDestination.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	EstablishDestination.BaseId = DestinationBaseId;
	EstablishDestination.Name = TEXT("Patagonia Care Annex");
	EstablishDestination.RegionId = TEXT("region.patagonia");
	EstablishDestination.LongitudeMilliDegrees = -72000;
	EstablishDestination.LatitudeMilliDegrees = -45000;
	EstablishDestination.StartingFacilities = { TEXT("facility.operations-hub") };
	const FStrategicCommandResult DestinationEstablished =
		Instance->EstablishBase(EstablishDestination);
	PresentStrategicCommandResult(
		DestinationEstablished, TEXT("Mutual Aid destination fixture established."));
	if (!DestinationEstablished.bAccepted)
	{
		return;
	}
	if (bReliefDiversionDemo || bRelayWaypointFixtureDemo)
	{
		FEstablishBaseCommand EstablishAlternate;
		EstablishAlternate.ExpectedSequence =
			CurrentStrategicSnapshot.ExpectedCommandSequence;
		EstablishAlternate.BaseId = AlternateDestinationBaseId;
		EstablishAlternate.Name = TEXT("North Atlantic Relief Pier");
		EstablishAlternate.RegionId = TEXT("region.north-atlantic");
		EstablishAlternate.LongitudeMilliDegrees = -25000;
		EstablishAlternate.LatitudeMilliDegrees = 50000;
		EstablishAlternate.StartingFacilities = { TEXT("facility.operations-hub") };
		const FStrategicCommandResult AlternateEstablished =
			Instance->EstablishBase(EstablishAlternate);
		PresentStrategicCommandResult(
			AlternateEstablished,
			TEXT("Mutual Aid third-base fixture established."));
		if (!AlternateEstablished.bAccepted)
		{
			return;
		}
	}

	FStrategicBaseView* Source = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[SourceBaseId](const FStrategicBaseView& Base) { return Base.BaseId == SourceBaseId; });
	FStrategicBaseView* Destination = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[DestinationBaseId](const FStrategicBaseView& Base)
		{
			return Base.BaseId == DestinationBaseId;
		});
	FStrategicBaseView* AlternateDestination =
		CurrentStrategicSnapshot.Bases.FindByPredicate(
			[AlternateDestinationBaseId](const FStrategicBaseView& Base)
			{
				return Base.BaseId == AlternateDestinationBaseId;
			});
	const FName ItemId(TEXT("item.med-gel"));
	const FItemRule* ItemRule = Instance->GetLoadedRules().Items.Find(ItemId);
	if (Source == nullptr || Destination == nullptr || ItemRule == nullptr
		|| ((bReliefDiversionDemo || bRelayWaypointFixtureDemo)
			&& AlternateDestination == nullptr))
	{
		return;
	}
	if (bSignalWatchDemo)
	{
		Source->ScientistCapacity = 2;
		Source->AssignedScientists = 1;
		Source->SignalWatchPolicyId = TEXT("logistics.signal-watch");
		Source->SignalWatchScientists = 1;
		Source->SignalWatchMaximumScientists = 1;
		Source->FacilityRelayChannelCount = 1;
		Source->SignalWatchBonusChannelCount = 1;
		Source->RelayChannelCount = 2;
		Source->bCanIncreaseSignalWatch = false;
		Source->SignalWatchIncreaseUnavailableReasonCode =
			TEXT("signal_watch_channel_capacity_exceeded");
		Source->SignalWatchIncreaseUnavailableReason =
			TEXT("Signal Watch needs one operational facility channel per scientist; this base currently supports fewer.");
	}

	constexpr int32 RemainingSourceQuantity = 8;
	constexpr int32 ConvoyQuantity = 6;
	constexpr int32 SurgeConvoyQuantity = 2;
	constexpr int64 FullTransitSeconds = int64(72) * 3600;
	constexpr int64 RemainingTransitSeconds = int64(36) * 3600;
	const int64 SourceStorage = static_cast<int64>(ItemRule->Mass) * RemainingSourceQuantity;
	const int64 ConvoyStorage = static_cast<int64>(ItemRule->Mass) * ConvoyQuantity;
	const int64 SurgeConvoyStorage =
		static_cast<int64>(ItemRule->Mass) * SurgeConvoyQuantity;
	Source->Inventory.Reset();
	FStrategicInventoryView& Inventory = Source->Inventory.AddDefaulted_GetRef();
	Inventory.ItemId = ItemId;
	Inventory.DisplayName = ItemRule->DisplayName;
	Inventory.Quantity = RemainingSourceQuantity;
	Inventory.UnitSellValue = FMath::Max(0, ItemRule->SellValue);
	Inventory.UnitStorage = FMath::Max(0, ItemRule->Mass);
	Inventory.TotalStorage = SourceStorage;
	FStrategicMutualAidDispatchOptionView& Option =
		Inventory.MutualAidOptions.AddDefaulted_GetRef();
	Option.DestinationBaseId = DestinationBaseId;
	Option.DestinationBaseName = Destination->Name;
	Option.MaximumQuantity = RemainingSourceQuantity;
	Option.TransitSeconds = FullTransitSeconds;
	const auto AddRoute = [&Option, SourceBaseId, RemainingTransitSeconds, bSignalWatchDemo,
		ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence](
		const EMutualAidRoutePolicy Policy,
		const FName PolicyId,
		const int64 TransitSeconds,
		const int32 ExposureModifier,
		const int32 RoutePressure,
		const bool bInterdictionExpected)
	{
		FStrategicMutualAidRouteOptionView& Route = Option.Routes.AddDefaulted_GetRef();
		Route.Policy = Policy;
		Route.PolicyId = PolicyId;
		Route.TransitSeconds = TransitSeconds;
		Route.BaselinePressure = 75;
		Route.ExposureModifier = ExposureModifier;
		Route.RoutePressure = RoutePressure;
		Route.bInterdictionExpected = bInterdictionExpected;
		Route.InterdictionDelaySeconds = int64(24) * 3600;
		Route.SignalEscortCost = 25000;
		Route.bSignalEscortAffordable = true;
		Route.RelayQueue.bValid = true;
		Route.RelayQueue.PolicyId = TEXT("logistics.mutual-aid-relay-weave");
		Route.RelayQueue.SourceBaseId = SourceBaseId;
		Route.RelayQueue.DispatchSequence = ExpectedSequence + 1;
		Route.RelayQueue.RelayChannelCount = bSignalWatchDemo ? 2 : 1;
		Route.RelayQueue.FacilityRelayChannelCount = 1;
		Route.RelayQueue.SignalWatchScientistCount = bSignalWatchDemo ? 1 : 0;
		Route.RelayQueue.SignalWatchBonusChannelCount = bSignalWatchDemo ? 1 : 0;
		Route.RelayQueue.ActiveConvoyCount = bSignalWatchDemo ? 2 : 1;
		Route.RelayQueue.TotalConvoyCount = 2;
		Route.RelayQueue.QueuePosition = 2;
		Route.RelayQueue.WaitingPosition = bSignalWatchDemo ? 0 : 1;
		Route.RelayQueue.RelayChannelNumber = bSignalWatchDemo ? 2 : 1;
		Route.RelayQueue.bRelayAvailable = true;
		Route.RelayQueue.bInTransit = bSignalWatchDemo;
		Route.RelayQueue.EstimatedWaitSeconds =
			bSignalWatchDemo ? 0 : RemainingTransitSeconds;
		Route.RelayQueue.EstimatedArrivalSeconds =
			(bSignalWatchDemo ? 0 : RemainingTransitSeconds)
			+ TransitSeconds + (bInterdictionExpected ? int64(24) * 3600 : 0);
		Route.EscortedEstimatedArrivalSeconds =
			(bSignalWatchDemo ? 0 : RemainingTransitSeconds) + TransitSeconds;
	};
	AddRoute(
		EMutualAidRoutePolicy::OpenRelay,
		TEXT("logistics.mutual-aid-open-relay"), int64(72) * 3600, 0, 75, true);
	AddRoute(
		EMutualAidRoutePolicy::RapidThread,
		TEXT("logistics.mutual-aid-rapid-thread"), int64(48) * 3600, 25, 100, true);
	AddRoute(
		EMutualAidRoutePolicy::VeiledChain,
		TEXT("logistics.mutual-aid-veiled-chain"), int64(96) * 3600, -25, 50, false);
	Option.bEnabled = true;
	Source->StorageUsed = SourceStorage;
	Source->StorageReserved = 0;
	Source->StorageProductionReserved = 0;
	Source->StorageMutualAidReserved = 0;
	Source->StorageCommitted = SourceStorage;
	Source->StorageAvailable = FMath::Max<int64>(0, Source->StorageCapacity - SourceStorage);
	Destination->StorageUsed = 0;
	Destination->StorageReserved = ConvoyStorage
		+ (bSignalWatchDemo || bThreadlineRetuneDemo || bSignalSuretyDemo
			|| bReliefPriorityDemo || bReliefStandDownDemo
			|| bReliefDiversionDemo || bRelayWaypointFixtureDemo
			? SurgeConvoyStorage : 0)
		+ (bReliefPriorityDemo || bReliefStandDownDemo || bReliefDiversionDemo
			|| bRelayWaypointFixtureDemo
			? SurgeConvoyStorage : 0);
	Destination->StorageProductionReserved = 0;
	Destination->StorageMutualAidReserved = Destination->StorageReserved;
	Destination->StorageCommitted = Destination->StorageReserved;
	Destination->StorageAvailable =
		FMath::Max<int64>(0, Destination->StorageCapacity - Destination->StorageReserved);
	if (AlternateDestination != nullptr)
	{
		AlternateDestination->StorageUsed = 0;
		AlternateDestination->StorageReserved = 0;
		AlternateDestination->StorageProductionReserved = 0;
		AlternateDestination->StorageMutualAidReserved = 0;
		AlternateDestination->StorageCommitted = 0;
		AlternateDestination->StorageAvailable = AlternateDestination->StorageCapacity;
	}

	CurrentStrategicSnapshot.MutualAidConvoys.Reset();
	CurrentStrategicSnapshot.MutualAidConvoys.Reserve(
		bReliefPriorityDemo || bReliefStandDownDemo || bReliefDiversionDemo
			|| bRelayWaypointFixtureDemo
		? 3
		: bSignalWatchDemo || bThreadlineRetuneDemo || bSignalSuretyDemo ? 2 : 1);
	FStrategicMutualAidConvoyView& Convoy =
		CurrentStrategicSnapshot.MutualAidConvoys.AddDefaulted_GetRef();
	Convoy.ConvoyId = FGuid(0x46a1d001, 0x46a1d002, 0x46a1d003, 0x46a1d004);
	Convoy.SourceBaseId = SourceBaseId;
	Convoy.SourceBaseName = Source->Name;
	Convoy.CurrentLegOriginBaseId = SourceBaseId;
	Convoy.CurrentLegOriginBaseName = Source->Name;
	Convoy.DestinationBaseId = DestinationBaseId;
	Convoy.DestinationBaseName = Destination->Name;
	Convoy.ItemId = ItemId;
	Convoy.ItemDisplayName = ItemRule->DisplayName;
	Convoy.Quantity = ConvoyQuantity;
	Convoy.DispatchSequence = FMath::Max<int64>(
		1, CurrentStrategicSnapshot.ExpectedCommandSequence);
	Convoy.TotalStorage = ConvoyStorage;
	Convoy.RemainingTransitSeconds = RemainingTransitSeconds;
	Convoy.RoutePolicy = EMutualAidRoutePolicy::RapidThread;
	Convoy.RoutePolicyId = TEXT("logistics.mutual-aid-rapid-thread");
	Convoy.TotalTransitSeconds = int64(48) * 3600;
	Convoy.RoutePressure = 100;
	Convoy.bSignalEscort = false;
	Convoy.SignalEscortCost = 0;
	Convoy.bInterdictionResolved = true;
	Convoy.ForecastInterdictionDelaySeconds = int64(24) * 3600;
	Convoy.InterdictionDelaySeconds = int64(24) * 3600;
	Convoy.RelayQueue.bValid = true;
	Convoy.RelayQueue.PolicyId = TEXT("logistics.mutual-aid-relay-weave");
	Convoy.RelayQueue.SourceBaseId = SourceBaseId;
	Convoy.RelayQueue.ConvoyId = Convoy.ConvoyId;
	Convoy.RelayQueue.DispatchSequence = Convoy.DispatchSequence;
	Convoy.RelayQueue.RelayChannelCount = bSignalWatchDemo ? 2 : 1;
	Convoy.RelayQueue.FacilityRelayChannelCount = 1;
	Convoy.RelayQueue.SignalWatchScientistCount = bSignalWatchDemo ? 1 : 0;
	Convoy.RelayQueue.SignalWatchBonusChannelCount = bSignalWatchDemo ? 1 : 0;
	Convoy.RelayQueue.ActiveConvoyCount = bSignalWatchDemo ? 2 : 1;
	Convoy.RelayQueue.TotalConvoyCount =
		bReliefPriorityDemo || bReliefStandDownDemo || bReliefDiversionDemo
			|| bRelayWaypointFixtureDemo ? 3
		: bSignalWatchDemo || bThreadlineRetuneDemo || bSignalSuretyDemo ? 2 : 1;
	Convoy.RelayQueue.QueuePosition = 1;
	Convoy.RelayQueue.RelayChannelNumber = 1;
	Convoy.RelayQueue.bRelayAvailable = true;
	Convoy.RelayQueue.bInTransit = true;
	Convoy.RelayQueue.EstimatedArrivalSeconds = RemainingTransitSeconds;
	if (bSignalWatchDemo)
	{
		FStrategicMutualAidConvoyView SurgeConvoyTemplate = Convoy;
		FStrategicMutualAidConvoyView& SurgeConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys.Add_GetRef(
				MoveTemp(SurgeConvoyTemplate));
		SurgeConvoy.ConvoyId = FGuid(0x46a1d011, 0x46a1d012, 0x46a1d013, 0x46a1d014);
		SurgeConvoy.Quantity = SurgeConvoyQuantity;
		SurgeConvoy.DispatchSequence = Convoy.DispatchSequence + 1;
		SurgeConvoy.TotalStorage = SurgeConvoyStorage;
		SurgeConvoy.RemainingTransitSeconds = int64(48) * 3600;
		SurgeConvoy.InterdictionDelaySeconds = 0;
		SurgeConvoy.RelayQueue.ConvoyId = SurgeConvoy.ConvoyId;
		SurgeConvoy.RelayQueue.DispatchSequence = SurgeConvoy.DispatchSequence;
		SurgeConvoy.RelayQueue.QueuePosition = 2;
		SurgeConvoy.RelayQueue.RelayChannelNumber = 2;
		SurgeConvoy.RelayQueue.EstimatedArrivalSeconds =
			SurgeConvoy.RemainingTransitSeconds;
	}
	else if (bThreadlineRetuneDemo)
	{
		FStrategicMutualAidConvoyView HeldConvoyTemplate = Convoy;
		FStrategicMutualAidConvoyView& HeldConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys.Add_GetRef(
				MoveTemp(HeldConvoyTemplate));
		HeldConvoy.ConvoyId =
			FGuid(0x46a1d021, 0x46a1d022, 0x46a1d023, 0x46a1d024);
		HeldConvoy.Quantity = SurgeConvoyQuantity;
		HeldConvoy.DispatchSequence = Convoy.DispatchSequence + 1;
		HeldConvoy.TotalStorage = SurgeConvoyStorage;
		HeldConvoy.RemainingTransitSeconds = int64(96) * 3600;
		HeldConvoy.RoutePolicy = EMutualAidRoutePolicy::VeiledChain;
		HeldConvoy.RoutePolicyId = TEXT("logistics.mutual-aid-veiled-chain");
		HeldConvoy.TotalTransitSeconds = int64(96) * 3600;
		HeldConvoy.RoutePressure = 50;
		HeldConvoy.bSignalEscort = true;
		HeldConvoy.SignalEscortCost = 25000;
		HeldConvoy.bInterdictionResolved = true;
		HeldConvoy.InterdictionDelaySeconds = 0;
		HeldConvoy.RelayQueue.ConvoyId = HeldConvoy.ConvoyId;
		HeldConvoy.RelayQueue.DispatchSequence = HeldConvoy.DispatchSequence;
		HeldConvoy.RelayQueue.ActiveConvoyCount = 1;
		HeldConvoy.RelayQueue.TotalConvoyCount = 2;
		HeldConvoy.RelayQueue.QueuePosition = 2;
		HeldConvoy.RelayQueue.WaitingPosition = 1;
		HeldConvoy.RelayQueue.RelayChannelNumber = 1;
		HeldConvoy.RelayQueue.bInTransit = false;
		HeldConvoy.RelayQueue.EstimatedWaitSeconds = RemainingTransitSeconds;
		HeldConvoy.RelayQueue.EstimatedArrivalSeconds =
			RemainingTransitSeconds + HeldConvoy.RemainingTransitSeconds;
		HeldConvoy.bCanRetune = true;
		const auto AddRetuneRoute = [&HeldConvoy, RemainingTransitSeconds](
			const EMutualAidRoutePolicy Policy,
			const FName PolicyId,
			const int64 TransitSeconds,
			const int32 ExposureModifier,
			const int32 RoutePressure,
			const bool bInterdictionExpected,
			const bool bEnabled)
		{
			FStrategicMutualAidRouteOptionView& Route =
				HeldConvoy.RetuneRoutes.AddDefaulted_GetRef();
			Route.Policy = Policy;
			Route.PolicyId = PolicyId;
			Route.TransitSeconds = TransitSeconds;
			Route.BaselinePressure = 75;
			Route.ExposureModifier = ExposureModifier;
			Route.RoutePressure = RoutePressure;
			Route.bInterdictionExpected = bInterdictionExpected;
			Route.InterdictionDelaySeconds = int64(24) * 3600;
			Route.SignalEscortCost = HeldConvoy.SignalEscortCost;
			Route.bSignalEscortAffordable = true;
			Route.bEnabled = bEnabled;
			Route.RelayQueue = HeldConvoy.RelayQueue;
			Route.RelayQueue.EstimatedArrivalSeconds =
				RemainingTransitSeconds + TransitSeconds;
			Route.EscortedEstimatedArrivalSeconds =
				Route.RelayQueue.EstimatedArrivalSeconds;
			if (!bEnabled)
			{
				Route.UnavailableReasonCode = TEXT("mutual_aid_retune_same_policy");
				Route.UnavailableReason =
					TEXT("Select a different Threadline route before retuning this convoy.");
			}
		};
		AddRetuneRoute(
			EMutualAidRoutePolicy::OpenRelay,
			TEXT("logistics.mutual-aid-open-relay"), int64(72) * 3600,
			0, 75, true, true);
		AddRetuneRoute(
			EMutualAidRoutePolicy::RapidThread,
			TEXT("logistics.mutual-aid-rapid-thread"), int64(48) * 3600,
			25, 100, true, true);
		AddRetuneRoute(
			EMutualAidRoutePolicy::VeiledChain,
			TEXT("logistics.mutual-aid-veiled-chain"), int64(96) * 3600,
			-25, 50, false, false);
		Inventory.MutualAidOptions.Reset();
	}
	else if (bSignalSuretyDemo)
	{
		FStrategicMutualAidConvoyView HeldConvoyTemplate = Convoy;
		FStrategicMutualAidConvoyView& HeldConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys.Add_GetRef(
				MoveTemp(HeldConvoyTemplate));
		HeldConvoy.ConvoyId =
			FGuid(0x46a1d031, 0x46a1d032, 0x46a1d033, 0x46a1d034);
		HeldConvoy.Quantity = SurgeConvoyQuantity;
		HeldConvoy.DispatchSequence = Convoy.DispatchSequence + 1;
		HeldConvoy.TotalStorage = SurgeConvoyStorage;
		HeldConvoy.RemainingTransitSeconds = int64(48) * 3600;
		HeldConvoy.RoutePolicy = EMutualAidRoutePolicy::RapidThread;
		HeldConvoy.RoutePolicyId = TEXT("logistics.mutual-aid-rapid-thread");
		HeldConvoy.TotalTransitSeconds = int64(48) * 3600;
		HeldConvoy.RoutePressure = 100;
		HeldConvoy.bSignalEscort = false;
		HeldConvoy.SignalEscortCost = 0;
		HeldConvoy.bInterdictionResolved = false;
		HeldConvoy.InterdictionDelaySeconds = 0;
		HeldConvoy.RelayQueue.ConvoyId = HeldConvoy.ConvoyId;
		HeldConvoy.RelayQueue.DispatchSequence = HeldConvoy.DispatchSequence;
		HeldConvoy.RelayQueue.ActiveConvoyCount = 1;
		HeldConvoy.RelayQueue.TotalConvoyCount = 2;
		HeldConvoy.RelayQueue.QueuePosition = 2;
		HeldConvoy.RelayQueue.WaitingPosition = 1;
		HeldConvoy.RelayQueue.RelayChannelNumber = 1;
		HeldConvoy.RelayQueue.bInTransit = false;
		HeldConvoy.RelayQueue.EstimatedWaitSeconds = RemainingTransitSeconds;
		HeldConvoy.RelayQueue.EstimatedArrivalSeconds =
			RemainingTransitSeconds + HeldConvoy.RemainingTransitSeconds
			+ HeldConvoy.ForecastInterdictionDelaySeconds;
		HeldConvoy.bCanRetune = false;
		HeldConvoy.RetuneRoutes.Reset();
		HeldConvoy.bCanCommissionSignalEscort = true;
		HeldConvoy.SignalEscortCommissionCost = 25000;
		HeldConvoy.SignalEscortPreventedDelaySeconds =
			HeldConvoy.ForecastInterdictionDelaySeconds;
		HeldConvoy.SignalEscortProjectedRelayQueue = HeldConvoy.RelayQueue;
		HeldConvoy.SignalEscortProjectedRelayQueue.EstimatedArrivalSeconds =
			RemainingTransitSeconds + HeldConvoy.RemainingTransitSeconds;
		Inventory.MutualAidOptions.Reset();
	}
	else if (bReliefPriorityDemo)
	{
		FStrategicMutualAidConvoyView EarlierTemplate = Convoy;
		FStrategicMutualAidConvoyView& EarlierHeld =
			CurrentStrategicSnapshot.MutualAidConvoys.Add_GetRef(
				MoveTemp(EarlierTemplate));
		EarlierHeld.ConvoyId =
			FGuid(0x46a1d041, 0x46a1d042, 0x46a1d043, 0x46a1d044);
		EarlierHeld.Quantity = SurgeConvoyQuantity;
		EarlierHeld.DispatchSequence = Convoy.DispatchSequence + 1;
		EarlierHeld.TotalStorage = SurgeConvoyStorage;
		EarlierHeld.RemainingTransitSeconds = FullTransitSeconds;
		EarlierHeld.RoutePolicy = EMutualAidRoutePolicy::OpenRelay;
		EarlierHeld.RoutePolicyId = TEXT("logistics.mutual-aid-open-relay");
		EarlierHeld.TotalTransitSeconds = FullTransitSeconds;
		EarlierHeld.RoutePressure = 75;
		EarlierHeld.bSignalEscort = true;
		EarlierHeld.SignalEscortCost = 25000;
		EarlierHeld.bInterdictionResolved = true;
		EarlierHeld.InterdictionDelaySeconds = 0;
		EarlierHeld.RelayQueue.ConvoyId = EarlierHeld.ConvoyId;
		EarlierHeld.RelayQueue.DispatchSequence = EarlierHeld.DispatchSequence;
		EarlierHeld.RelayQueue.ActiveConvoyCount = 1;
		EarlierHeld.RelayQueue.TotalConvoyCount = 3;
		EarlierHeld.RelayQueue.QueuePosition = 2;
		EarlierHeld.RelayQueue.WaitingPosition = 1;
		EarlierHeld.RelayQueue.RelayChannelNumber = 1;
		EarlierHeld.RelayQueue.bInTransit = false;
		EarlierHeld.RelayQueue.EstimatedWaitSeconds = RemainingTransitSeconds;
		EarlierHeld.RelayQueue.EstimatedArrivalSeconds =
			RemainingTransitSeconds + EarlierHeld.RemainingTransitSeconds;
		EarlierHeld.bCanRetune = false;
		EarlierHeld.RetuneRoutes.Reset();
		EarlierHeld.bCanCommissionSignalEscort = false;

		FStrategicMutualAidConvoyView TargetTemplate = Convoy;
		FStrategicMutualAidConvoyView& TargetHeld =
			CurrentStrategicSnapshot.MutualAidConvoys.Add_GetRef(
				MoveTemp(TargetTemplate));
		TargetHeld.ConvoyId =
			FGuid(0x46a1d051, 0x46a1d052, 0x46a1d053, 0x46a1d054);
		TargetHeld.Quantity = SurgeConvoyQuantity;
		TargetHeld.DispatchSequence = Convoy.DispatchSequence + 2;
		TargetHeld.TotalStorage = SurgeConvoyStorage;
		TargetHeld.RemainingTransitSeconds = int64(48) * 3600;
		TargetHeld.RoutePolicy = EMutualAidRoutePolicy::RapidThread;
		TargetHeld.RoutePolicyId = TEXT("logistics.mutual-aid-rapid-thread");
		TargetHeld.TotalTransitSeconds = int64(48) * 3600;
		TargetHeld.RoutePressure = 100;
		TargetHeld.bSignalEscort = true;
		TargetHeld.SignalEscortCost = 25000;
		TargetHeld.bInterdictionResolved = true;
		TargetHeld.InterdictionDelaySeconds = 0;
		TargetHeld.RelayQueue.ConvoyId = TargetHeld.ConvoyId;
		TargetHeld.RelayQueue.DispatchSequence = TargetHeld.DispatchSequence;
		TargetHeld.RelayQueue.ActiveConvoyCount = 1;
		TargetHeld.RelayQueue.TotalConvoyCount = 3;
		TargetHeld.RelayQueue.QueuePosition = 3;
		TargetHeld.RelayQueue.WaitingPosition = 2;
		TargetHeld.RelayQueue.RelayChannelNumber = 1;
		TargetHeld.RelayQueue.bInTransit = false;
		TargetHeld.RelayQueue.EstimatedWaitSeconds =
			RemainingTransitSeconds + EarlierHeld.RemainingTransitSeconds;
		TargetHeld.RelayQueue.EstimatedArrivalSeconds =
			TargetHeld.RelayQueue.EstimatedWaitSeconds
			+ TargetHeld.RemainingTransitSeconds;
		TargetHeld.bCanRetune = false;
		TargetHeld.RetuneRoutes.Reset();
		TargetHeld.bCanCommissionSignalEscort = false;
		TargetHeld.bCanPrioritizeRelief = true;
		TargetHeld.ReliefPriorityPolicyId = TEXT("logistics.relief-priority");
		TargetHeld.ReliefPriorityBypassedConvoyCount = 1;
		TargetHeld.ReliefPriorityRecoveredWaitSeconds =
			EarlierHeld.RemainingTransitSeconds;
		TargetHeld.ReliefPriorityProjectedRelayQueue = TargetHeld.RelayQueue;
		TargetHeld.ReliefPriorityProjectedRelayQueue.DispatchSequence =
			EarlierHeld.DispatchSequence;
		TargetHeld.ReliefPriorityProjectedRelayQueue.QueuePosition = 2;
		TargetHeld.ReliefPriorityProjectedRelayQueue.WaitingPosition = 1;
		TargetHeld.ReliefPriorityProjectedRelayQueue.EstimatedWaitSeconds =
			RemainingTransitSeconds;
		TargetHeld.ReliefPriorityProjectedRelayQueue.EstimatedArrivalSeconds =
			RemainingTransitSeconds + TargetHeld.RemainingTransitSeconds;
		Inventory.MutualAidOptions.Reset();
	}
	else if (bReliefStandDownDemo)
	{
		FStrategicMutualAidConvoyView TargetTemplate = Convoy;
		FStrategicMutualAidConvoyView& TargetHeld =
			CurrentStrategicSnapshot.MutualAidConvoys.Add_GetRef(
				MoveTemp(TargetTemplate));
		TargetHeld.ConvoyId =
			FGuid(0x46a1d061, 0x46a1d062, 0x46a1d063, 0x46a1d064);
		TargetHeld.Quantity = SurgeConvoyQuantity;
		TargetHeld.DispatchSequence = Convoy.DispatchSequence + 1;
		TargetHeld.TotalStorage = SurgeConvoyStorage;
		TargetHeld.RemainingTransitSeconds = int64(48) * 3600;
		TargetHeld.RoutePolicy = EMutualAidRoutePolicy::RapidThread;
		TargetHeld.RoutePolicyId = TEXT("logistics.mutual-aid-rapid-thread");
		TargetHeld.TotalTransitSeconds = int64(48) * 3600;
		TargetHeld.RoutePressure = 100;
		TargetHeld.bSignalEscort = true;
		TargetHeld.SignalEscortCost = 25000;
		TargetHeld.bInterdictionResolved = true;
		TargetHeld.InterdictionDelaySeconds = 0;
		TargetHeld.RelayQueue.ConvoyId = TargetHeld.ConvoyId;
		TargetHeld.RelayQueue.DispatchSequence = TargetHeld.DispatchSequence;
		TargetHeld.RelayQueue.ActiveConvoyCount = 1;
		TargetHeld.RelayQueue.TotalConvoyCount = 3;
		TargetHeld.RelayQueue.QueuePosition = 2;
		TargetHeld.RelayQueue.WaitingPosition = 1;
		TargetHeld.RelayQueue.RelayChannelNumber = 1;
		TargetHeld.RelayQueue.bInTransit = false;
		TargetHeld.RelayQueue.EstimatedWaitSeconds = RemainingTransitSeconds;
		TargetHeld.RelayQueue.EstimatedArrivalSeconds =
			RemainingTransitSeconds + TargetHeld.RemainingTransitSeconds;
		TargetHeld.bCanRetune = false;
		TargetHeld.RetuneRoutes.Reset();
		TargetHeld.bCanCommissionSignalEscort = false;
		TargetHeld.bCanPrioritizeRelief = false;
		TargetHeld.bCanStandDownRelief = true;
		TargetHeld.ReliefStandDownPolicyId = TEXT("logistics.relief-stand-down");
		TargetHeld.ReliefStandDownReleasedStorage = SurgeConvoyStorage;
		TargetHeld.ReliefStandDownSunkSignalEscortCost =
			TargetHeld.SignalEscortCost;
		TargetHeld.ReliefStandDownAdvancedConvoyCount = 1;
		TargetHeld.ReliefStandDownRecoveredWaitSeconds = int64(48) * 3600;

		FStrategicMutualAidConvoyView LaterTemplate = Convoy;
		FStrategicMutualAidConvoyView& LaterHeld =
			CurrentStrategicSnapshot.MutualAidConvoys.Add_GetRef(
				MoveTemp(LaterTemplate));
		LaterHeld.ConvoyId =
			FGuid(0x46a1d071, 0x46a1d072, 0x46a1d073, 0x46a1d074);
		LaterHeld.Quantity = SurgeConvoyQuantity;
		LaterHeld.DispatchSequence = Convoy.DispatchSequence + 2;
		LaterHeld.TotalStorage = SurgeConvoyStorage;
		LaterHeld.RemainingTransitSeconds = FullTransitSeconds;
		LaterHeld.RoutePolicy = EMutualAidRoutePolicy::OpenRelay;
		LaterHeld.RoutePolicyId = TEXT("logistics.mutual-aid-open-relay");
		LaterHeld.TotalTransitSeconds = FullTransitSeconds;
		LaterHeld.RoutePressure = 75;
		LaterHeld.bSignalEscort = false;
		LaterHeld.SignalEscortCost = 0;
		LaterHeld.bInterdictionResolved = true;
		LaterHeld.InterdictionDelaySeconds = 0;
		LaterHeld.RelayQueue.ConvoyId = LaterHeld.ConvoyId;
		LaterHeld.RelayQueue.DispatchSequence = LaterHeld.DispatchSequence;
		LaterHeld.RelayQueue.ActiveConvoyCount = 1;
		LaterHeld.RelayQueue.TotalConvoyCount = 3;
		LaterHeld.RelayQueue.QueuePosition = 3;
		LaterHeld.RelayQueue.WaitingPosition = 2;
		LaterHeld.RelayQueue.RelayChannelNumber = 1;
		LaterHeld.RelayQueue.bInTransit = false;
		LaterHeld.RelayQueue.EstimatedWaitSeconds =
			RemainingTransitSeconds + TargetHeld.RemainingTransitSeconds;
		LaterHeld.RelayQueue.EstimatedArrivalSeconds =
			LaterHeld.RelayQueue.EstimatedWaitSeconds
			+ LaterHeld.RemainingTransitSeconds;
		LaterHeld.bCanRetune = false;
		LaterHeld.RetuneRoutes.Reset();
		LaterHeld.bCanCommissionSignalEscort = false;
		LaterHeld.bCanPrioritizeRelief = false;
		LaterHeld.bCanStandDownRelief = false;
		Inventory.MutualAidOptions.Reset();
	}
	else if (bRelayWaypointFixtureDemo)
	{
		check(AlternateDestination != nullptr);
		FStrategicMutualAidConvoyView TargetTemplate = Convoy;
		FStrategicMutualAidConvoyView& TargetHeld =
			CurrentStrategicSnapshot.MutualAidConvoys.Add_GetRef(
				MoveTemp(TargetTemplate));
		const int32 TargetQuantity =
			bBalancedHandoffDemo ? ConvoyQuantity : SurgeConvoyQuantity;
		const int64 TargetStorage =
			bBalancedHandoffDemo ? ConvoyStorage : SurgeConvoyStorage;
		TargetHeld.ConvoyId =
			FGuid(0x46a1d0a1, 0x46a1d0a2, 0x46a1d0a3, 0x46a1d0a4);
		TargetHeld.Quantity = TargetQuantity;
		TargetHeld.DispatchSequence = Convoy.DispatchSequence + 1;
		TargetHeld.TotalStorage = TargetStorage;
		TargetHeld.RemainingTransitSeconds = FullTransitSeconds;
		TargetHeld.RoutePolicy = EMutualAidRoutePolicy::OpenRelay;
		TargetHeld.RoutePolicyId = TEXT("logistics.mutual-aid-open-relay");
		TargetHeld.TotalTransitSeconds = FullTransitSeconds;
		TargetHeld.RoutePressure = 50;
		TargetHeld.bSignalEscort = false;
		TargetHeld.SignalEscortCost = 0;
		TargetHeld.bInterdictionResolved = true;
		TargetHeld.InterdictionDelaySeconds = 0;
		TargetHeld.RelayWaypointBaseId = AlternateDestinationBaseId;
		TargetHeld.RelayWaypointBaseName = AlternateDestination->Name;
		TargetHeld.OnwardRoutePolicy = EMutualAidRoutePolicy::RapidThread;
		TargetHeld.OnwardRoutePolicyId =
			TEXT("logistics.mutual-aid-rapid-thread");
		TargetHeld.OnwardTransitSeconds = int64(48) * 3600;
		TargetHeld.OnwardRoutePressure = 75;
		TargetHeld.bOnwardInterdictionResolved = false;
		TargetHeld.OnwardForecastInterdictionDelaySeconds = int64(24) * 3600;
		TargetHeld.RelayQueue.ConvoyId = TargetHeld.ConvoyId;
		TargetHeld.RelayQueue.DispatchSequence = TargetHeld.DispatchSequence;
		TargetHeld.RelayQueue.ActiveConvoyCount = 1;
		TargetHeld.RelayQueue.TotalConvoyCount = 3;
		TargetHeld.RelayQueue.QueuePosition = 2;
		TargetHeld.RelayQueue.WaitingPosition = 1;
		TargetHeld.RelayQueue.RelayChannelNumber = 1;
		TargetHeld.RelayQueue.bInTransit = false;
		TargetHeld.RelayQueue.EstimatedWaitSeconds = RemainingTransitSeconds;
		TargetHeld.RelayQueue.EstimatedArrivalSeconds = int64(180) * 3600;
		TargetHeld.bCanRetune = false;
		TargetHeld.RetuneRoutes.Reset();
		TargetHeld.bCanCommissionSignalEscort = false;
		TargetHeld.bCanPrioritizeRelief = false;
		TargetHeld.bCanStandDownRelief = false;
		TargetHeld.bCanDivertRelief = false;
		TargetHeld.ReliefDiversionOptions.Reset();
		TargetHeld.bCanConfigureRelayWaypoint = true;

		FStrategicMutualAidWaypointOptionView& Direct =
			TargetHeld.RelayWaypointOptions.AddDefaulted_GetRef();
		Direct.bDirectRoute = true;
		Direct.PolicyId = TEXT("logistics.relay-waypoint");
		Direct.OnwardRoutePolicy = TargetHeld.RoutePolicy;
		Direct.OnwardRoutePolicyId = TargetHeld.RoutePolicyId;
		Direct.JourneySeconds = FullTransitSeconds;
		Direct.ProjectedRelayQueue = TargetHeld.RelayQueue;
		Direct.ProjectedRelayQueue.EstimatedArrivalSeconds = int64(108) * 3600;
		Direct.ArrivalShiftSeconds = -int64(72) * 3600;
		Direct.AffectedConvoyCount = 1;
		Direct.TotalArrivalShiftSeconds = -int64(144) * 3600;
		Direct.bEnabled = true;
		const auto AddWaypointOption =
			[&TargetHeld, AlternateDestinationBaseId, AlternateDestination,
			RemainingTransitSeconds](
				const EMutualAidRoutePolicy Policy, const FName PolicyId,
				const int64 JourneySeconds, const int32 OnwardPressure,
				const bool bInterdictionExpected, const int64 ArrivalShiftSeconds,
				const bool bEnabled)
		{
			FStrategicMutualAidWaypointOptionView& Route =
				TargetHeld.RelayWaypointOptions.AddDefaulted_GetRef();
			Route.WaypointBaseId = AlternateDestinationBaseId;
			Route.WaypointBaseName = AlternateDestination->Name;
			Route.PolicyId = TEXT("logistics.relay-waypoint");
			Route.OnwardRoutePolicy = Policy;
			Route.OnwardRoutePolicyId = PolicyId;
			Route.FirstLegRoutePressure = 50;
			Route.OnwardRoutePressure = OnwardPressure;
			Route.bOnwardInterdictionExpected = bInterdictionExpected;
			Route.JourneySeconds = JourneySeconds;
			Route.WaypointArrivalSeconds =
				RemainingTransitSeconds + int64(72) * 3600;
			Route.ProjectedRelayQueue = TargetHeld.RelayQueue;
			Route.ProjectedRelayQueue.EstimatedArrivalSeconds =
				RemainingTransitSeconds + JourneySeconds;
			Route.ArrivalShiftSeconds = ArrivalShiftSeconds;
			Route.AffectedConvoyCount = ArrivalShiftSeconds == 0 ? 0 : 1;
			Route.TotalArrivalShiftSeconds = ArrivalShiftSeconds * 2;
			Route.bEnabled = bEnabled;
			if (!bEnabled)
			{
				Route.UnavailableReasonCode =
					TEXT("mutual_aid_relay_waypoint_same_plan");
				Route.UnavailableReason =
					TEXT("Select a different waypoint or onward route before changing this convoy.");
			}
		};
		AddWaypointOption(
			EMutualAidRoutePolicy::OpenRelay,
			TEXT("logistics.mutual-aid-open-relay"), int64(144) * 3600,
			50, false, 0, true);
		AddWaypointOption(
			EMutualAidRoutePolicy::RapidThread,
			TEXT("logistics.mutual-aid-rapid-thread"), int64(144) * 3600,
			75, true, 0, false);
		AddWaypointOption(
			EMutualAidRoutePolicy::VeiledChain,
			TEXT("logistics.mutual-aid-veiled-chain"), int64(168) * 3600,
			25, false, int64(24) * 3600, true);
		if (bBalancedHandoffDemo)
		{
			TargetHeld.BalancedHandoffQuantity = TargetQuantity / 2;
			TargetHeld.FinalDeliveryQuantity =
				TargetQuantity - TargetHeld.BalancedHandoffQuantity;
			TargetHeld.BalancedHandoffStorage =
				static_cast<int64>(ItemRule->Mass)
				* TargetHeld.BalancedHandoffQuantity;
			TargetHeld.bCanConfigureBalancedHandoff = true;
			TargetHeld.bCanConfigureRelayWaypoint = false;
			TargetHeld.RelayWaypointOptions.Reset();

			Destination->StorageReserved = ConvoyStorage
				+ (TargetStorage - TargetHeld.BalancedHandoffStorage)
				+ SurgeConvoyStorage;
			Destination->StorageMutualAidReserved = Destination->StorageReserved;
			Destination->StorageCommitted = Destination->StorageReserved;
			Destination->StorageAvailable = FMath::Max<int64>(
				0, Destination->StorageCapacity - Destination->StorageReserved);
			AlternateDestination->StorageReserved =
				TargetHeld.BalancedHandoffStorage;
			AlternateDestination->StorageMutualAidReserved =
				AlternateDestination->StorageReserved;
			AlternateDestination->StorageCommitted =
				AlternateDestination->StorageReserved;
			AlternateDestination->StorageAvailable = FMath::Max<int64>(
				0, AlternateDestination->StorageCapacity
					- AlternateDestination->StorageReserved);

			FStrategicMutualAidBalancedHandoffOptionView& ThroughCargo =
				TargetHeld.BalancedHandoffOptions.AddDefaulted_GetRef();
			ThroughCargo.bEnabledChoice = false;
			ThroughCargo.PolicyId = TEXT("logistics.mutual-aid-through-cargo");
			ThroughCargo.WaypointQuantity = 0;
			ThroughCargo.FinalQuantity = TargetQuantity;
			ThroughCargo.HandoffStorage = 0;
			ThroughCargo.WaypointReservedStorage = 0;
			ThroughCargo.DestinationReservedStorage =
				Destination->StorageReserved + TargetHeld.BalancedHandoffStorage;
			ThroughCargo.bEnabled = true;

			FStrategicMutualAidBalancedHandoffOptionView& BalancedHandoff =
				TargetHeld.BalancedHandoffOptions.AddDefaulted_GetRef();
			BalancedHandoff.bEnabledChoice = true;
			BalancedHandoff.PolicyId =
				TEXT("logistics.mutual-aid-balanced-handoff");
			BalancedHandoff.WaypointQuantity =
				TargetHeld.BalancedHandoffQuantity;
			BalancedHandoff.FinalQuantity = TargetHeld.FinalDeliveryQuantity;
			BalancedHandoff.HandoffStorage = TargetHeld.BalancedHandoffStorage;
			BalancedHandoff.WaypointReservedStorage =
				AlternateDestination->StorageReserved;
			BalancedHandoff.DestinationReservedStorage =
				Destination->StorageReserved;
			BalancedHandoff.UnavailableReasonCode =
				TEXT("mutual_aid_balanced_handoff_same_plan");
			BalancedHandoff.UnavailableReason =
				TEXT("Select a different cargo plan before changing this convoy.");
		}

		FStrategicMutualAidConvoyView LaterTemplate = Convoy;
		FStrategicMutualAidConvoyView& LaterHeld =
			CurrentStrategicSnapshot.MutualAidConvoys.Add_GetRef(
				MoveTemp(LaterTemplate));
		LaterHeld.ConvoyId =
			FGuid(0x46a1d0b1, 0x46a1d0b2, 0x46a1d0b3, 0x46a1d0b4);
		LaterHeld.Quantity = SurgeConvoyQuantity;
		LaterHeld.DispatchSequence = Convoy.DispatchSequence + 2;
		LaterHeld.TotalStorage = SurgeConvoyStorage;
		LaterHeld.RemainingTransitSeconds = int64(48) * 3600;
		LaterHeld.RoutePolicy = EMutualAidRoutePolicy::RapidThread;
		LaterHeld.RoutePolicyId = TEXT("logistics.mutual-aid-rapid-thread");
		LaterHeld.TotalTransitSeconds = int64(48) * 3600;
		LaterHeld.RoutePressure = 40;
		LaterHeld.bSignalEscort = true;
		LaterHeld.SignalEscortCost = 25000;
		LaterHeld.bInterdictionResolved = true;
		LaterHeld.InterdictionDelaySeconds = 0;
		LaterHeld.RelayQueue.ConvoyId = LaterHeld.ConvoyId;
		LaterHeld.RelayQueue.DispatchSequence = LaterHeld.DispatchSequence;
		LaterHeld.RelayQueue.ActiveConvoyCount = 1;
		LaterHeld.RelayQueue.TotalConvoyCount = 3;
		LaterHeld.RelayQueue.QueuePosition = 3;
		LaterHeld.RelayQueue.WaitingPosition = 2;
		LaterHeld.RelayQueue.RelayChannelNumber = 1;
		LaterHeld.RelayQueue.bInTransit = false;
		LaterHeld.RelayQueue.EstimatedWaitSeconds = int64(180) * 3600;
		LaterHeld.RelayQueue.EstimatedArrivalSeconds = int64(228) * 3600;
		LaterHeld.bCanRetune = false;
		LaterHeld.RetuneRoutes.Reset();
		LaterHeld.bCanCommissionSignalEscort = false;
		LaterHeld.bCanPrioritizeRelief = false;
		LaterHeld.bCanStandDownRelief = false;
		LaterHeld.bCanDivertRelief = false;
		LaterHeld.ReliefDiversionOptions.Reset();
		LaterHeld.bCanConfigureRelayWaypoint = false;
		LaterHeld.RelayWaypointOptions.Reset();
		Inventory.MutualAidOptions.Reset();
	}
	else if (bReliefDiversionDemo)
	{
		check(AlternateDestination != nullptr);
		FStrategicMutualAidConvoyView TargetTemplate = Convoy;
		FStrategicMutualAidConvoyView& TargetHeld =
			CurrentStrategicSnapshot.MutualAidConvoys.Add_GetRef(
				MoveTemp(TargetTemplate));
		TargetHeld.ConvoyId =
			FGuid(0x46a1d081, 0x46a1d082, 0x46a1d083, 0x46a1d084);
		TargetHeld.Quantity = SurgeConvoyQuantity;
		TargetHeld.DispatchSequence = Convoy.DispatchSequence + 1;
		TargetHeld.TotalStorage = SurgeConvoyStorage;
		TargetHeld.RemainingTransitSeconds = FullTransitSeconds;
		TargetHeld.RoutePolicy = EMutualAidRoutePolicy::OpenRelay;
		TargetHeld.RoutePolicyId = TEXT("logistics.mutual-aid-open-relay");
		TargetHeld.TotalTransitSeconds = FullTransitSeconds;
		TargetHeld.RoutePressure = 75;
		TargetHeld.bSignalEscort = false;
		TargetHeld.SignalEscortCost = 0;
		TargetHeld.bInterdictionResolved = false;
		TargetHeld.InterdictionDelaySeconds = 0;
		TargetHeld.RelayQueue.ConvoyId = TargetHeld.ConvoyId;
		TargetHeld.RelayQueue.DispatchSequence = TargetHeld.DispatchSequence;
		TargetHeld.RelayQueue.ActiveConvoyCount = 1;
		TargetHeld.RelayQueue.TotalConvoyCount = 3;
		TargetHeld.RelayQueue.QueuePosition = 2;
		TargetHeld.RelayQueue.WaitingPosition = 1;
		TargetHeld.RelayQueue.RelayChannelNumber = 1;
		TargetHeld.RelayQueue.bInTransit = false;
		TargetHeld.RelayQueue.EstimatedWaitSeconds = RemainingTransitSeconds;
		TargetHeld.RelayQueue.EstimatedArrivalSeconds =
			RemainingTransitSeconds + TargetHeld.RemainingTransitSeconds
			+ TargetHeld.ForecastInterdictionDelaySeconds;
		TargetHeld.bCanRetune = false;
		TargetHeld.RetuneRoutes.Reset();
		TargetHeld.bCanCommissionSignalEscort = false;
		TargetHeld.bCanPrioritizeRelief = false;
		TargetHeld.bCanStandDownRelief = false;
		TargetHeld.bCanDivertRelief = true;

		FStrategicMutualAidDiversionOptionView& CurrentDiversion =
			TargetHeld.ReliefDiversionOptions.AddDefaulted_GetRef();
		CurrentDiversion.DestinationBaseId = DestinationBaseId;
		CurrentDiversion.DestinationBaseName = Destination->Name;
		CurrentDiversion.PolicyId = TEXT("logistics.relief-diversion");
		CurrentDiversion.DivertedStorage = SurgeConvoyStorage;
		CurrentDiversion.CurrentRoutePressure = TargetHeld.RoutePressure;
		CurrentDiversion.ProjectedRoutePressure = TargetHeld.RoutePressure;
		CurrentDiversion.bInterdictionExpected = true;
		CurrentDiversion.bSignalEscort = TargetHeld.bSignalEscort;
		CurrentDiversion.RetainedSignalEscortCost = TargetHeld.SignalEscortCost;
		CurrentDiversion.ProjectedRelayQueue = TargetHeld.RelayQueue;
		CurrentDiversion.UnavailableReasonCode =
			TEXT("mutual_aid_relief_diversion_same_destination");
		CurrentDiversion.UnavailableReason =
			TEXT("Select a different destination before diverting this relief convoy.");

		FStrategicMutualAidDiversionOptionView& AlternateDiversion =
			TargetHeld.ReliefDiversionOptions.AddDefaulted_GetRef();
		AlternateDiversion.DestinationBaseId = AlternateDestinationBaseId;
		AlternateDiversion.DestinationBaseName = AlternateDestination->Name;
		AlternateDiversion.PolicyId = TEXT("logistics.relief-diversion");
		AlternateDiversion.DivertedStorage = SurgeConvoyStorage;
		AlternateDiversion.CurrentRoutePressure = TargetHeld.RoutePressure;
		AlternateDiversion.ProjectedRoutePressure = 40;
		AlternateDiversion.bInterdictionExpected = false;
		AlternateDiversion.bSignalEscort = TargetHeld.bSignalEscort;
		AlternateDiversion.RetainedSignalEscortCost = TargetHeld.SignalEscortCost;
		AlternateDiversion.ProjectedRelayQueue = TargetHeld.RelayQueue;
		AlternateDiversion.ProjectedRelayQueue.EstimatedArrivalSeconds -=
			TargetHeld.ForecastInterdictionDelaySeconds;
		AlternateDiversion.ArrivalShiftSeconds =
			-TargetHeld.ForecastInterdictionDelaySeconds;
		AlternateDiversion.AffectedConvoyCount = 1;
		AlternateDiversion.TotalArrivalShiftSeconds =
			-int64(48) * 3600;
		AlternateDiversion.bEnabled = true;

		FStrategicMutualAidConvoyView LaterTemplate = Convoy;
		FStrategicMutualAidConvoyView& LaterHeld =
			CurrentStrategicSnapshot.MutualAidConvoys.Add_GetRef(
				MoveTemp(LaterTemplate));
		LaterHeld.ConvoyId =
			FGuid(0x46a1d091, 0x46a1d092, 0x46a1d093, 0x46a1d094);
		LaterHeld.Quantity = SurgeConvoyQuantity;
		LaterHeld.DispatchSequence = Convoy.DispatchSequence + 2;
		LaterHeld.TotalStorage = SurgeConvoyStorage;
		LaterHeld.RemainingTransitSeconds = int64(48) * 3600;
		LaterHeld.RoutePolicy = EMutualAidRoutePolicy::RapidThread;
		LaterHeld.RoutePolicyId = TEXT("logistics.mutual-aid-rapid-thread");
		LaterHeld.TotalTransitSeconds = int64(48) * 3600;
		LaterHeld.RoutePressure = 100;
		LaterHeld.bSignalEscort = true;
		LaterHeld.SignalEscortCost = 25000;
		LaterHeld.bInterdictionResolved = true;
		LaterHeld.InterdictionDelaySeconds = 0;
		LaterHeld.RelayQueue.ConvoyId = LaterHeld.ConvoyId;
		LaterHeld.RelayQueue.DispatchSequence = LaterHeld.DispatchSequence;
		LaterHeld.RelayQueue.ActiveConvoyCount = 1;
		LaterHeld.RelayQueue.TotalConvoyCount = 3;
		LaterHeld.RelayQueue.QueuePosition = 3;
		LaterHeld.RelayQueue.WaitingPosition = 2;
		LaterHeld.RelayQueue.RelayChannelNumber = 1;
		LaterHeld.RelayQueue.bInTransit = false;
		LaterHeld.RelayQueue.EstimatedWaitSeconds =
			TargetHeld.RelayQueue.EstimatedArrivalSeconds;
		LaterHeld.RelayQueue.EstimatedArrivalSeconds =
			LaterHeld.RelayQueue.EstimatedWaitSeconds
			+ LaterHeld.RemainingTransitSeconds;
		LaterHeld.bCanRetune = false;
		LaterHeld.RetuneRoutes.Reset();
		LaterHeld.bCanCommissionSignalEscort = false;
		LaterHeld.bCanPrioritizeRelief = false;
		LaterHeld.bCanStandDownRelief = false;
		LaterHeld.bCanDivertRelief = false;
		LaterHeld.ReliefDiversionOptions.Reset();
		Inventory.MutualAidOptions.Reset();
	}

	if (StrategicHudWidget != nullptr)
	{
		StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
		StrategicHudWidget->ShowStatusMessage(FString());
		if (!bSignalWatchDemo)
		{
			StrategicHudWidget->FocusMutualAidPanel();
		}
	}
	const FStrategicDashboardSnapshot MutualAidFixture = CurrentStrategicSnapshot;
	FTimerDelegate MutualAidFixtureDelegate;
	MutualAidFixtureDelegate.BindWeakLambda(
		this, [this, MutualAidFixture, bSignalWatchDemo]()
	{
		CurrentStrategicSnapshot = MutualAidFixture;
		if (StrategicHudWidget != nullptr)
		{
			StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
			StrategicHudWidget->ShowStatusMessage(FString());
			if (!bSignalWatchDemo)
			{
				StrategicHudWidget->FocusMutualAidPanel();
			}
		}
	});
	FTimerHandle MutualAidFixtureTimer;
	GetWorldTimerManager().SetTimer(
		MutualAidFixtureTimer, MutualAidFixtureDelegate, 0.1f, false);

	if (!bThreadlineRetuneDemo && !bSignalSuretyDemo && !bReliefPriorityDemo
		&& !bReliefStandDownDemo && !bReliefDiversionDemo
		&& !bRelayWaypointFixtureDemo)
	{
		UE_LOG(LogTemp, Display,
			TEXT("UEGT_MUTUAL_AID_RUNTIME_OK source=presentation-fixture bases=%d inventory=%d options=%d max=%d convoys=%d quantity=%d remaining=%lld storage=%lld reserved=%lld transit=%lld routes=%d"),
			CurrentStrategicSnapshot.Bases.Num(), Inventory.Quantity,
			Inventory.MutualAidOptions.Num(), Option.MaximumQuantity,
			CurrentStrategicSnapshot.MutualAidConvoys.Num(), Convoy.Quantity,
			Convoy.RemainingTransitSeconds, Convoy.TotalStorage,
			Destination->StorageMutualAidReserved, Option.TransitSeconds,
			Option.Routes.Num());
		UE_LOG(LogTemp, Display,
			TEXT("UEGT_THREADLINE_RUNTIME_OK source=presentation-fixture routes=%d openPressure=%d rapidPressure=%d veiledPressure=%d convoyPolicy=%s convoyPressure=%d resolved=%s delay=%lld escortCost=%lld"),
			Option.Routes.Num(), Option.Routes[0].RoutePressure,
			Option.Routes[1].RoutePressure, Option.Routes[2].RoutePressure,
			*Convoy.RoutePolicyId.ToString(), Convoy.RoutePressure,
			Convoy.bInterdictionResolved ? TEXT("true") : TEXT("false"),
			Convoy.InterdictionDelaySeconds, Option.Routes[1].SignalEscortCost);
		UE_LOG(LogTemp, Display,
			TEXT("UEGT_RELAY_WEAVE_RUNTIME_OK source=presentation-fixture channels=%d active=%s convoyQueue=%d previewActive=%s previewQueue=%d previewWait=%lld previewArrival=%lld policy=%s"),
			Convoy.RelayQueue.RelayChannelCount,
			Convoy.RelayQueue.bInTransit ? TEXT("true") : TEXT("false"),
			Convoy.RelayQueue.QueuePosition,
			Option.Routes[1].RelayQueue.bInTransit ? TEXT("true") : TEXT("false"),
			Option.Routes[1].RelayQueue.WaitingPosition,
			Option.Routes[1].RelayQueue.EstimatedWaitSeconds,
			Option.Routes[1].RelayQueue.EstimatedArrivalSeconds,
			*Convoy.RelayQueue.PolicyId.ToString());
		if (bSignalWatchDemo)
		{
			UE_LOG(LogTemp, Display,
				TEXT("UEGT_SIGNAL_WATCH_RUNTIME_OK source=presentation-fixture scientists=%d max=%d facilityChannels=%d bonusChannels=%d totalChannels=%d activeConvoys=%d researchFree=%d policy=%s"),
				Source->SignalWatchScientists,
				Source->SignalWatchMaximumScientists,
				Source->FacilityRelayChannelCount,
				Source->SignalWatchBonusChannelCount,
				Source->RelayChannelCount,
				CurrentStrategicSnapshot.MutualAidConvoys.Num(),
				FMath::Max(0, Source->ScientistCapacity - Source->AssignedScientists),
				*Source->SignalWatchPolicyId.ToString());
		}
	}
	else if (bThreadlineRetuneDemo
		&& CurrentStrategicSnapshot.MutualAidConvoys.Num() == 2)
	{
		const FStrategicMutualAidConvoyView& HeldConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[1];
		int32 EnabledRoutes = 0;
		for (const FStrategicMutualAidRouteOptionView& Route : HeldConvoy.RetuneRoutes)
		{
			EnabledRoutes += Route.bEnabled ? 1 : 0;
		}
		UE_LOG(LogTemp, Display,
			TEXT("UEGT_THREADLINE_RETUNE_RUNTIME_OK source=presentation-fixture convoyQueue=%d priorPolicy=logistics.mutual-aid-rapid-thread currentPolicy=%s options=%d enabled=%d wait=%lld arrival=%lld cargo=%d storage=%lld escort=%s"),
			CurrentStrategicSnapshot.MutualAidConvoys.Num(),
			*HeldConvoy.RoutePolicyId.ToString(), HeldConvoy.RetuneRoutes.Num(),
			EnabledRoutes, HeldConvoy.RelayQueue.EstimatedWaitSeconds,
			HeldConvoy.RelayQueue.EstimatedArrivalSeconds,
			HeldConvoy.Quantity, Destination->StorageMutualAidReserved,
			HeldConvoy.bSignalEscort ? TEXT("true") : TEXT("false"));
	}
	else if (bSignalSuretyDemo
		&& CurrentStrategicSnapshot.MutualAidConvoys.Num() == 2)
	{
		const FStrategicMutualAidConvoyView& HeldConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[1];
		UE_LOG(LogTemp, Display,
			TEXT("UEGT_SIGNAL_SURETY_RUNTIME_OK source=presentation-fixture convoyQueue=%d policy=%s wait=%lld beforeArrival=%lld afterArrival=%lld recovered=%lld cost=%lld funds=%lld cargo=%d storage=%lld escort=%s enabled=%s"),
			CurrentStrategicSnapshot.MutualAidConvoys.Num(),
			*HeldConvoy.RoutePolicyId.ToString(),
			HeldConvoy.RelayQueue.EstimatedWaitSeconds,
			HeldConvoy.RelayQueue.EstimatedArrivalSeconds,
			HeldConvoy.SignalEscortProjectedRelayQueue.EstimatedArrivalSeconds,
			HeldConvoy.SignalEscortPreventedDelaySeconds,
			HeldConvoy.SignalEscortCommissionCost,
			CurrentStrategicSnapshot.Funds, HeldConvoy.Quantity,
			Destination->StorageMutualAidReserved,
			HeldConvoy.bSignalEscort ? TEXT("true") : TEXT("false"),
			HeldConvoy.bCanCommissionSignalEscort ? TEXT("true") : TEXT("false"));
	}
	else if (bReliefPriorityDemo
		&& CurrentStrategicSnapshot.MutualAidConvoys.Num() == 3)
	{
		const FStrategicMutualAidConvoyView& ActiveConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[0];
		const FStrategicMutualAidConvoyView& TargetConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[2];
		UE_LOG(LogTemp, Display,
			TEXT("UEGT_RELIEF_PRIORITY_RUNTIME_OK source=presentation-fixture convoyQueue=%d policy=%s currentPosition=%d projectedPosition=%d wait=%lld beforeArrival=%lld afterArrival=%lld recovered=%lld bypassed=%d cargo=%d storage=%lld activeArrival=%lld enabled=%s"),
			CurrentStrategicSnapshot.MutualAidConvoys.Num(),
			*TargetConvoy.ReliefPriorityPolicyId.ToString(),
			TargetConvoy.RelayQueue.QueuePosition,
			TargetConvoy.ReliefPriorityProjectedRelayQueue.QueuePosition,
			TargetConvoy.RelayQueue.EstimatedWaitSeconds,
			TargetConvoy.RelayQueue.EstimatedArrivalSeconds,
			TargetConvoy.ReliefPriorityProjectedRelayQueue.EstimatedArrivalSeconds,
			TargetConvoy.ReliefPriorityRecoveredWaitSeconds,
			TargetConvoy.ReliefPriorityBypassedConvoyCount,
			TargetConvoy.Quantity, Destination->StorageMutualAidReserved,
			ActiveConvoy.RelayQueue.EstimatedArrivalSeconds,
			TargetConvoy.bCanPrioritizeRelief ? TEXT("true") : TEXT("false"));
	}
	else if (bReliefStandDownDemo
		&& CurrentStrategicSnapshot.MutualAidConvoys.Num() == 3)
	{
		const FStrategicMutualAidConvoyView& ActiveConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[0];
		const FStrategicMutualAidConvoyView& TargetConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[1];
		const FStrategicMutualAidConvoyView& NextConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[2];
		const int64 NextProjectedArrival =
			RemainingTransitSeconds + NextConvoy.RemainingTransitSeconds;
		UE_LOG(LogTemp, Display,
			TEXT("UEGT_RELIEF_STAND_DOWN_RUNTIME_OK source=presentation-fixture convoyQueue=%d policy=%s targetPosition=%d targetWait=%lld targetArrival=%lld released=%lld returned=%d advanced=%d recovered=%lld nextBeforeArrival=%lld nextAfterArrival=%lld storage=%lld sunkEscort=%lld activeArrival=%lld enabled=%s"),
			CurrentStrategicSnapshot.MutualAidConvoys.Num(),
			*TargetConvoy.ReliefStandDownPolicyId.ToString(),
			TargetConvoy.RelayQueue.QueuePosition,
			TargetConvoy.RelayQueue.EstimatedWaitSeconds,
			TargetConvoy.RelayQueue.EstimatedArrivalSeconds,
			TargetConvoy.ReliefStandDownReleasedStorage,
			TargetConvoy.Quantity,
			TargetConvoy.ReliefStandDownAdvancedConvoyCount,
			TargetConvoy.ReliefStandDownRecoveredWaitSeconds,
			NextConvoy.RelayQueue.EstimatedArrivalSeconds,
			NextProjectedArrival,
			Destination->StorageMutualAidReserved,
			TargetConvoy.ReliefStandDownSunkSignalEscortCost,
			ActiveConvoy.RelayQueue.EstimatedArrivalSeconds,
			TargetConvoy.bCanStandDownRelief ? TEXT("true") : TEXT("false"));
	}
	else if (bBalancedHandoffDemo
		&& CurrentStrategicSnapshot.MutualAidConvoys.Num() == 3
		&& AlternateDestination != nullptr)
	{
		const FStrategicMutualAidConvoyView& ActiveConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[0];
		const FStrategicMutualAidConvoyView& TargetConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[1];
		const FStrategicMutualAidConvoyView& NextConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[2];
		const FStrategicMutualAidBalancedHandoffOptionView* ThroughCargo =
			TargetConvoy.BalancedHandoffOptions.FindByPredicate(
				[](const FStrategicMutualAidBalancedHandoffOptionView& Candidate)
				{
					return !Candidate.bEnabledChoice;
				});
		const FStrategicMutualAidBalancedHandoffOptionView* BalancedHandoff =
			TargetConvoy.BalancedHandoffOptions.FindByPredicate(
				[](const FStrategicMutualAidBalancedHandoffOptionView& Candidate)
				{
					return Candidate.bEnabledChoice;
				});
		int32 EnabledPlans = 0;
		for (const FStrategicMutualAidBalancedHandoffOptionView& Candidate :
			TargetConvoy.BalancedHandoffOptions)
		{
			EnabledPlans += Candidate.bEnabled ? 1 : 0;
		}
		if (ThroughCargo != nullptr && BalancedHandoff != nullptr)
		{
			UE_LOG(LogTemp, Display,
				TEXT("UEGT_BALANCED_HANDOFF_RUNTIME_OK source=presentation-fixture bases=%d convoyQueue=%d policy=%s waypoint=%s options=%d enabled=%d quantity=%d handoff=%d final=%d handoffStorage=%lld waypointReserved=%lld destinationReserved=%lld throughWaypointReserved=%lld throughDestinationReserved=%lld targetPosition=%d targetWait=%lld finalArrival=%lld activeArrival=%lld nextArrival=%lld escort=%s sourceChannelHeld=true timingUnchanged=true"),
				CurrentStrategicSnapshot.Bases.Num(),
				CurrentStrategicSnapshot.MutualAidConvoys.Num(),
				*BalancedHandoff->PolicyId.ToString(),
				*TargetConvoy.RelayWaypointBaseName,
				TargetConvoy.BalancedHandoffOptions.Num(), EnabledPlans,
				TargetConvoy.Quantity,
				TargetConvoy.BalancedHandoffQuantity,
				TargetConvoy.FinalDeliveryQuantity,
				TargetConvoy.BalancedHandoffStorage,
				BalancedHandoff->WaypointReservedStorage,
				BalancedHandoff->DestinationReservedStorage,
				ThroughCargo->WaypointReservedStorage,
				ThroughCargo->DestinationReservedStorage,
				TargetConvoy.RelayQueue.QueuePosition,
				TargetConvoy.RelayQueue.EstimatedWaitSeconds,
				TargetConvoy.RelayQueue.EstimatedArrivalSeconds,
				ActiveConvoy.RelayQueue.EstimatedArrivalSeconds,
				NextConvoy.RelayQueue.EstimatedArrivalSeconds,
				TargetConvoy.bSignalEscort ? TEXT("true") : TEXT("false"));
		}
	}
	else if (bRelayWaypointDemo
		&& CurrentStrategicSnapshot.MutualAidConvoys.Num() == 3
		&& AlternateDestination != nullptr)
	{
		const FStrategicMutualAidConvoyView& ActiveConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[0];
		const FStrategicMutualAidConvoyView& TargetConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[1];
		const FStrategicMutualAidConvoyView& NextConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[2];
		const FStrategicMutualAidWaypointOptionView* DirectOption =
			TargetConvoy.RelayWaypointOptions.FindByPredicate(
				[](const FStrategicMutualAidWaypointOptionView& Candidate)
				{
					return Candidate.bDirectRoute;
				});
		const FStrategicMutualAidWaypointOptionView* CurrentOption =
			TargetConvoy.RelayWaypointOptions.FindByPredicate(
				[AlternateDestinationBaseId](
					const FStrategicMutualAidWaypointOptionView& Candidate)
				{
					return Candidate.WaypointBaseId == AlternateDestinationBaseId
						&& Candidate.OnwardRoutePolicy
							== EMutualAidRoutePolicy::RapidThread;
				});
		int32 EnabledOptions = 0;
		for (const FStrategicMutualAidWaypointOptionView& Candidate :
			TargetConvoy.RelayWaypointOptions)
		{
			EnabledOptions += Candidate.bEnabled ? 1 : 0;
		}
		if (DirectOption != nullptr && CurrentOption != nullptr)
		{
			UE_LOG(LogTemp, Display,
				TEXT("UEGT_RELAY_WAYPOINT_RUNTIME_OK source=presentation-fixture bases=%d convoyQueue=%d policy=%s waypoint=%s options=%d enabled=%d targetPosition=%d targetWait=%lld waypointArrival=%lld finalArrival=%lld directArrival=%lld directShift=%lld affected=%d totalShift=%lld nextBeforeArrival=%lld nextDirectArrival=%lld storage=%lld firstTransit=%lld onwardTransit=%lld firstPressure=%d onwardPressure=%d onwardDelay=%lld activeArrival=%lld sourceChannelHeld=true"),
				CurrentStrategicSnapshot.Bases.Num(),
				CurrentStrategicSnapshot.MutualAidConvoys.Num(),
				*CurrentOption->PolicyId.ToString(),
				*TargetConvoy.RelayWaypointBaseName,
				TargetConvoy.RelayWaypointOptions.Num(), EnabledOptions,
				TargetConvoy.RelayQueue.QueuePosition,
				TargetConvoy.RelayQueue.EstimatedWaitSeconds,
				CurrentOption->WaypointArrivalSeconds,
				TargetConvoy.RelayQueue.EstimatedArrivalSeconds,
				DirectOption->ProjectedRelayQueue.EstimatedArrivalSeconds,
				DirectOption->ArrivalShiftSeconds,
				DirectOption->AffectedConvoyCount,
				DirectOption->TotalArrivalShiftSeconds,
				NextConvoy.RelayQueue.EstimatedArrivalSeconds,
				NextConvoy.RelayQueue.EstimatedArrivalSeconds
					+ DirectOption->ArrivalShiftSeconds,
				Destination->StorageMutualAidReserved,
				TargetConvoy.TotalTransitSeconds,
				TargetConvoy.OnwardTransitSeconds,
				TargetConvoy.RoutePressure,
				TargetConvoy.OnwardRoutePressure,
				TargetConvoy.OnwardForecastInterdictionDelaySeconds,
				ActiveConvoy.RelayQueue.EstimatedArrivalSeconds);
		}
	}
	else if (bReliefDiversionDemo
		&& CurrentStrategicSnapshot.MutualAidConvoys.Num() == 3
		&& AlternateDestination != nullptr)
	{
		const FStrategicMutualAidConvoyView& ActiveConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[0];
		const FStrategicMutualAidConvoyView& TargetConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[1];
		const FStrategicMutualAidConvoyView& NextConvoy =
			CurrentStrategicSnapshot.MutualAidConvoys[2];
		const FStrategicMutualAidDiversionOptionView* DiversionOption =
			TargetConvoy.ReliefDiversionOptions.FindByPredicate(
				[AlternateDestinationBaseId](
					const FStrategicMutualAidDiversionOptionView& Candidate)
				{
					return Candidate.DestinationBaseId == AlternateDestinationBaseId;
				});
		if (DiversionOption != nullptr)
		{
			const int64 NextProjectedArrival =
				NextConvoy.RelayQueue.EstimatedArrivalSeconds
				+ DiversionOption->ArrivalShiftSeconds;
			UE_LOG(LogTemp, Display,
				TEXT("UEGT_RELIEF_DIVERSION_RUNTIME_OK source=presentation-fixture bases=%d convoyQueue=%d policy=%s targetPosition=%d targetWait=%lld beforeArrival=%lld afterArrival=%lld shift=%lld affected=%d totalShift=%lld nextBeforeArrival=%lld nextAfterArrival=%lld storage=%lld moved=%lld oldAfter=%lld newAfter=%lld pressureBefore=%d pressureAfter=%d retainedEscort=%lld activeArrival=%lld enabled=%s"),
				CurrentStrategicSnapshot.Bases.Num(),
				CurrentStrategicSnapshot.MutualAidConvoys.Num(),
				*DiversionOption->PolicyId.ToString(),
				TargetConvoy.RelayQueue.QueuePosition,
				TargetConvoy.RelayQueue.EstimatedWaitSeconds,
				TargetConvoy.RelayQueue.EstimatedArrivalSeconds,
				DiversionOption->ProjectedRelayQueue.EstimatedArrivalSeconds,
				DiversionOption->ArrivalShiftSeconds,
				DiversionOption->AffectedConvoyCount,
				DiversionOption->TotalArrivalShiftSeconds,
				NextConvoy.RelayQueue.EstimatedArrivalSeconds,
				NextProjectedArrival,
				Destination->StorageMutualAidReserved,
				DiversionOption->DivertedStorage,
				Destination->StorageMutualAidReserved
					- DiversionOption->DivertedStorage,
				AlternateDestination->StorageMutualAidReserved
					+ DiversionOption->DivertedStorage,
				DiversionOption->CurrentRoutePressure,
				DiversionOption->ProjectedRoutePressure,
				DiversionOption->RetainedSignalEscortCost,
				ActiveConvoy.RelayQueue.EstimatedArrivalSeconds,
				DiversionOption->bEnabled ? TEXT("true") : TEXT("false"));
		}
	}
#endif
}

void AUEGTTacticalPlayerController::ApplyUserSettings()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		Settings->ApplySettings(false);
	}
	ApplyPresentationAccessibilitySettings();
	RebuildInputBindings();
}

void AUEGTTacticalPlayerController::PreviewAudioCue()
{
	const bool bQueued = AudioDirector != nullptr
		&& AudioDirector->PlayCue(EUEGTAudioCue::InterfaceConfirm);
	SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
		bQueued ? TEXT("status.audio-preview") : TEXT("status.audio-preview-unavailable"),
		bQueued
			? TEXT("Original procedural audio cue previewed at the current master volume.")
			: TEXT("Audio preview is unavailable because no playback device is active.")),
		!bQueued);
}

void AUEGTTacticalPlayerController::PlayCommandAudio(
	const FStrategicCommandResult& Result,
	const bool bTacticalContext)
{
	if (AudioDirector != nullptr)
	{
		const EUEGTAudioCue Cue = UUEGTAudioDirector::SelectCommandCue(Result, bTacticalContext);
		FIntVector TacticalCell;
		if (bTacticalContext
			&& BoardActor != nullptr
			&& (Cue == EUEGTAudioCue::CommandAccepted
				|| Cue == EUEGTAudioCue::CommandRejected
				|| Cue == EUEGTAudioCue::TacticalImpact
				|| Cue == EUEGTAudioCue::TacticalSignal)
			&& UUEGTAudioDirector::TryGetLatestTacticalEventCell(Result, TacticalCell))
		{
			AudioDirector->PlayCueAtLocation(
				Cue,
				BoardActor->GridToWorld(
					TacticalCell.X,
					TacticalCell.Y,
					TacticalCell.Z,
					30.0f));
		}
		else
		{
			AudioDirector->PlayCue(Cue);
		}
	}
}

void AUEGTTacticalPlayerController::LogRuntimeAudioDemo()
{
	if (AudioDirector == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("UEGTAudioDemo state: source=procedural-runtime director=false"));
		return;
	}
	const FUEGTAudioDirectorDiagnostics Audio = AudioDirector->GetDiagnostics();
	const UUEGTUserSettings* Settings = UUEGTUserSettings::Get();
	UE_LOG(LogTemp, Display,
		TEXT("UEGTAudioDemo state: source=procedural-runtime cues=%d mode=%s requests=%d components=%d component=%s playing=%s cue=%s frames=%d bytes=%d fingerprint=%u ambient=%s ducked=%s duckRequests=%d master=%d muteUnfocused=%s"),
		FUEGTAudioSynthesisService::GetCueTypes().Num(),
		*UUEGTAudioDirector::GetModeName(Audio.Mode).ToString(),
		Audio.PlayRequests,
		Audio.PlaybackComponentsCreated,
		Audio.bLastPlaybackComponentCreated ? TEXT("true") : TEXT("false"),
		Audio.bLastPlaybackStarted ? TEXT("true") : TEXT("false"),
		*FUEGTAudioSynthesisService::GetCueName(Audio.LastCue).ToString(),
		Audio.LastFrameCount,
		Audio.LastQueuedBytes,
		Audio.LastFingerprint,
		Audio.bAmbientScheduled ? TEXT("true") : TEXT("false"),
		Audio.bAmbientDucked ? TEXT("true") : TEXT("false"),
		Audio.AmbientDuckRequests,
		Settings != nullptr ? Settings->GetMasterVolumePercent() : -1,
		Settings != nullptr && Settings->ShouldMuteWhenUnfocused() ? TEXT("true") : TEXT("false"));
}

void AUEGTTacticalPlayerController::ApplyPresentationAccessibilitySettings()
{
	const UUEGTUserSettings* Settings = UUEGTUserSettings::Get();
	if (Settings == nullptr)
	{
		return;
	}
	Settings->ApplyAccessibilitySettings();
	if (AUEGTTacticalCameraPawn* CameraPawn = GetTacticalCameraPawn())
	{
		CameraPawn->ApplyAccessibilitySettings(
			Settings->IsReducedMotionEnabled(),
			Settings->GetCameraSpeedMultiplier());
	}
	if (BoardActor != nullptr)
	{
		BoardActor->ApplyAccessibilityPalette(
			Settings->GetColorVisionMode(),
			Settings->IsHighContrastEnabled());
		BoardActor->SetReducedMotionEnabled(Settings->IsReducedMotionEnabled());
	}
	if (GlobeActor != nullptr)
	{
		GlobeActor->ApplyAccessibilityPalette(
			Settings->GetColorVisionMode(),
			Settings->IsHighContrastEnabled());
	}
}

void AUEGTTacticalPlayerController::ToggleSettings()
{
	if (StrategicHudWidget == nullptr || !bStrategicMode)
	{
		return;
	}
	if (StrategicHudWidget->IsShowingSettings())
	{
		StrategicHudWidget->CloseSettings();
	}
	else
	{
		StrategicHudWidget->ShowSettings();
	}
}

void AUEGTTacticalPlayerController::HandleToggleSettings()
{
	ToggleSettings();
}

void AUEGTTacticalPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	RebuildInputBindings();
}

void AUEGTTacticalPlayerController::RebuildInputBindings()
{
	if (InputComponent == nullptr)
	{
		return;
	}
	InputComponent->ClearBindingsForObject(this);
	BindInputKeys();
}

void AUEGTTacticalPlayerController::BindInputKeys()
{
	if (InputComponent == nullptr)
	{
		return;
	}
	const UUEGTUserSettings* Settings = UUEGTUserSettings::Get();
	const auto ConfiguredKey = [Settings](const EUEGTInputCommand Command)
	{
		return Settings != nullptr
			? Settings->GetInputKey(Command)
			: UUEGTUserSettings::GetDefaultInputKey(Command);
	};
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AUEGTTacticalPlayerController::HandlePrimaryClick);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleContextClick);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::Confirm), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleConfirmOrContinue);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::EndTurn), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleEndTurn);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::ToggleStance), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleStance);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::Reload), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleReload);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::Objective), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleObjective);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::Extract), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleExtract);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::Door), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleDoor);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::UseDevice), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleDevice);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::TerrainAttack), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleTerrainAttack);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::ProjectSignal), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleSignal);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::NextUnit), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleNextUnit);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::LevelUp), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleLevelUp);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::LevelDown), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleLevelDown);
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleZoomIn);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleZoomOut);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::CycleWeapon), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleCycleWeapon);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::ToggleFireMode), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleToggleFireMode);
	InputComponent->BindKey(ConfiguredKey(EUEGTInputCommand::CycleDevice), IE_Pressed, this, &AUEGTTacticalPlayerController::HandleCycleDevice);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleStrategicTimeFour);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleStrategicTimeFive);
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleStrategicTimeSix);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleToggleSettings);
	InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleCursorUp);
	InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleCursorDown);
	InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleCursorLeft);
	InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleCursorRight);

	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleContextClick);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Top, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleStance);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleObjective);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleConfirmOrContinue);
	InputComponent->BindKey(EKeys::Gamepad_DPad_Up, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleCursorUp);
	InputComponent->BindKey(EKeys::Gamepad_DPad_Down, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleCursorDown);
	InputComponent->BindKey(EKeys::Gamepad_DPad_Left, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleCursorLeft);
	InputComponent->BindKey(EKeys::Gamepad_DPad_Right, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleCursorRight);
	InputComponent->BindKey(EKeys::Gamepad_LeftThumbstick, IE_Pressed, this, &AUEGTTacticalPlayerController::HandlePreviousUnit);
	InputComponent->BindKey(EKeys::Gamepad_RightThumbstick, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleNextUnit);
	InputComponent->BindKey(EKeys::Gamepad_LeftShoulder, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleLevelDown);
	InputComponent->BindKey(EKeys::Gamepad_RightShoulder, IE_Pressed, this, &AUEGTTacticalPlayerController::HandleLevelUp);
}

void AUEGTTacticalPlayerController::PlayerTick(const float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (AUEGTTacticalCameraPawn* CameraPawn = GetTacticalCameraPawn())
	{
		if (bStrategicMode)
		{
			const float YawAxis = (IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::E) ? 1.0f : 0.0f)
				- (IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Q) ? 1.0f : 0.0f)
				+ GetInputAnalogKeyState(EKeys::Gamepad_LeftX)
				+ GetInputAnalogKeyState(EKeys::Gamepad_RightX);
			const float PitchAxis = (IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f)
				- (IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f)
				+ GetInputAnalogKeyState(EKeys::Gamepad_LeftY)
				+ GetInputAnalogKeyState(EKeys::Gamepad_RightY);
			if (!FMath::IsNearlyZero(YawAxis) || !FMath::IsNearlyZero(PitchAxis))
			{
				CameraPawn->OrbitGlobe(YawAxis * 55.0f * DeltaTime, PitchAxis * 35.0f * DeltaTime);
			}
			const float ZoomAxis = GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis)
				- GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
			if (!FMath::IsNearlyZero(ZoomAxis))
			{
				CameraPawn->Zoom(ZoomAxis * DeltaTime * 4.0f);
			}
			return;
		}
		const float Forward = (IsInputKeyDown(EKeys::W) ? 1.0f : 0.0f)
			- (IsInputKeyDown(EKeys::S) ? 1.0f : 0.0f)
			+ GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
		const float Right = (IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f)
			- (IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f)
			+ GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
		CameraPawn->Pan(Forward, Right, DeltaTime);
		const float OrbitAxis = (IsInputKeyDown(EKeys::E) ? 1.0f : 0.0f)
			- (IsInputKeyDown(EKeys::Q) ? 1.0f : 0.0f)
			+ GetInputAnalogKeyState(EKeys::Gamepad_RightX);
		if (!FMath::IsNearlyZero(OrbitAxis))
		{
			CameraPawn->Orbit(OrbitAxis * 70.0f * DeltaTime);
		}
		const float ZoomAxis = GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis)
			- GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
		if (!FMath::IsNearlyZero(ZoomAxis))
		{
			CameraPawn->Zoom(ZoomAxis * DeltaTime * 4.0f);
		}
	}
	UpdatePointerTarget();
}

AUEGTTacticalCameraPawn* AUEGTTacticalPlayerController::GetTacticalCameraPawn() const
{
	return Cast<AUEGTTacticalCameraPawn>(GetPawn());
}

bool AUEGTTacticalPlayerController::TraceBoard(FUEGTTacticalBoardHit& OutHit) const
{
	OutHit = FUEGTTacticalBoardHit();
	if (BoardActor == nullptr)
	{
		return false;
	}
	FHitResult Hit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
	{
		return false;
	}
	return BoardActor->ResolveHit(Hit, OutHit);
}

bool AUEGTTacticalPlayerController::TraceGlobe(FUEGTStrategicGlobeHit& OutHit) const
{
	OutHit = FUEGTStrategicGlobeHit();
	if (GlobeActor == nullptr)
	{
		return false;
	}
	FHitResult Hit;
	if (!GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), true, Hit))
	{
		return false;
	}
	return GlobeActor->ResolveHit(Hit, OutHit);
}

void AUEGTTacticalPlayerController::UpdatePointerTarget()
{
	if (bStrategicMode)
	{
		return;
	}
	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}
	const FVector2D Pointer(MouseX, MouseY);
	if (bHasPointerPosition && Pointer.Equals(LastPointerPosition, 0.25f))
	{
		return;
	}
	bHasPointerPosition = true;
	LastPointerPosition = Pointer;

	FUEGTTacticalBoardHit Hit;
	const bool bHit = TraceBoard(Hit);
	const bool bChanged = bHasHoveredCell != (bHit && Hit.bHasCell)
		|| (bHit && Hit.bHasCell && (HoveredX != Hit.X || HoveredY != Hit.Y || HoveredZ != Hit.Z
			|| HoveredUnitId != Hit.UnitId || HoveredObjectiveId != Hit.ObjectiveId));
	if (!bChanged)
	{
		return;
	}
	bHasHoveredCell = bHit && Hit.bHasCell;
	HoveredX = Hit.X;
	HoveredY = Hit.Y;
	HoveredZ = Hit.Z;
	HoveredUnitId = Hit.UnitId;
	HoveredObjectiveId = Hit.ObjectiveId;
	RefreshTacticalPresentation();
}

void AUEGTTacticalPlayerController::RefreshTacticalPresentation()
{
	// A confirmation remains valid only while the player makes no other presentation choice.
	EndTurnConfirmation.Reset();
	const bool bEnteringTacticalMode = bStrategicMode;
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	const TArray<FGuid> BattleIds = Instance->GetTacticalBattleIds();
	if (!ActiveBattleId.IsValid() || !BattleIds.Contains(ActiveBattleId))
	{
		ActiveBattleId = BattleIds.IsEmpty() ? FGuid() : BattleIds[0];
		SelectedUnitId.Invalidate();
		HoveredUnitId.Invalidate();
		HoveredObjectiveId = NAME_None;
		SelectedWeaponItemId = NAME_None;
		SelectedDeviceItemId = NAME_None;
		FireMode = ETacticalFireMode::Single;
		ViewedLevel = 0;
	}
	if (!ActiveBattleId.IsValid())
	{
		CurrentSnapshot = FTacticalHudSnapshot();
		if (BoardActor != nullptr)
		{
			BoardActor->ClearBoard();
		}
		const FTacticalDebriefView Debrief = Instance->GetLastTacticalDebrief();
		if (bViewingDebrief && Debrief.bAvailable)
		{
			bStrategicMode = false;
			if (AudioDirector != nullptr)
			{
				AudioDirector->SetPresentationMode(EUEGTAudioPresentationMode::Debrief);
			}
			if (BoardActor != nullptr)
			{
				BoardActor->SetActorHiddenInGame(true);
				BoardActor->SetActorEnableCollision(false);
			}
			if (GlobeActor != nullptr)
			{
				GlobeActor->SetPresentationEnabled(false);
			}
			if (StrategicHudWidget != nullptr)
			{
				StrategicHudWidget->SetVisibility(ESlateVisibility::Collapsed);
			}
			if (HudWidget != nullptr)
			{
				HudWidget->SetVisibility(ESlateVisibility::Visible);
				HudWidget->ApplyDebrief(Debrief);
			}
			return;
		}
		bViewingDebrief = false;
		RefreshStrategicPresentation();
		return;
	}

	bStrategicMode = false;
	bViewingDebrief = false;
	bGlobeFocused = false;
	if (AudioDirector != nullptr)
	{
		AudioDirector->SetPresentationMode(EUEGTAudioPresentationMode::Tactical);
	}
	if (BoardActor != nullptr)
	{
		BoardActor->SetActorHiddenInGame(false);
		BoardActor->SetActorEnableCollision(true);
	}
	if (GlobeActor != nullptr)
	{
		GlobeActor->SetPresentationEnabled(false);
	}
	if (StrategicHudWidget != nullptr)
	{
		StrategicHudWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (HudWidget != nullptr)
	{
		HudWidget->SetVisibility(ESlateVisibility::Visible);
	}

	FTacticalHudQuery Query;
	Query.SelectedUnitId = SelectedUnitId;
	Query.ViewedLevel = ViewedLevel;
	Query.bHasHoveredCell = bHasHoveredCell;
	Query.HoveredX = HoveredX;
	Query.HoveredY = HoveredY;
	Query.HoveredZ = HoveredZ;
	Query.HoveredUnitId = HoveredUnitId;
	Query.HoveredObjectiveId = HoveredObjectiveId;
	Query.SelectedWeaponItemId = SelectedWeaponItemId;
	Query.SelectedDeviceItemId = SelectedDeviceItemId;
	Query.FireMode = FireMode;
	CurrentSnapshot = Instance->BuildTacticalHudSnapshot(ActiveBattleId, Query);

	const bool bSelectedUnitPresent = CurrentSnapshot.Units.ContainsByPredicate(
		[this](const FTacticalHudUnitView& Unit)
		{
			return Unit.UnitId == SelectedUnitId && Unit.Team == ETacticalTeam::Player
				&& !Unit.bIncapacitated && !Unit.bExtracted;
		});
	if (CurrentSnapshot.bSucceeded && !bSelectedUnitPresent)
	{
		const FTacticalHudUnitView* FirstPlayer = CurrentSnapshot.Units.FindByPredicate(
			[](const FTacticalHudUnitView& Unit)
			{
				return Unit.Team == ETacticalTeam::Player && !Unit.bIncapacitated && !Unit.bExtracted;
			});
		if (FirstPlayer != nullptr)
		{
			SelectedUnitId = FirstPlayer->UnitId;
			ViewedLevel = FirstPlayer->Z;
			Query.SelectedUnitId = SelectedUnitId;
			Query.ViewedLevel = ViewedLevel;
			CurrentSnapshot = Instance->BuildTacticalHudSnapshot(ActiveBattleId, Query);
		}
	}
	if (SelectedWeaponItemId.IsNone())
	{
		SelectedWeaponItemId = CurrentSnapshot.EffectiveWeaponItemId;
	}
	if (SelectedDeviceItemId.IsNone())
	{
		SelectedDeviceItemId = CurrentSnapshot.EffectiveDeviceItemId;
	}
	if (BoardActor != nullptr)
	{
		BoardActor->ApplySnapshot(CurrentSnapshot);
	}
	if (HudWidget != nullptr)
	{
		HudWidget->ApplySnapshot(CurrentSnapshot);
	}
	if (CurrentSnapshot.bSucceeded && (bEnteringTacticalMode || LastFocusedBattleId != ActiveBattleId))
	{
		if (AUEGTTacticalCameraPawn* CameraPawn = GetTacticalCameraPawn())
		{
			CameraPawn->FocusBoard(
				CurrentSnapshot.Width,
				CurrentSnapshot.Height,
				CurrentSnapshot.ViewedLevel,
				BoardActor != nullptr ? BoardActor->GetCellSize() : 100.0f,
				BoardActor != nullptr ? BoardActor->GetLevelHeight() : 180.0f);
		}
		LastFocusedBattleId = ActiveBattleId;
	}
	LastPresentedSequence = CurrentSnapshot.ExpectedCommandSequence;
}

void AUEGTTacticalPlayerController::RefreshStrategicPresentation()
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	bStrategicMode = true;
	if (AudioDirector != nullptr)
	{
		AudioDirector->SetPresentationMode(Instance->HasActiveCampaign()
			? EUEGTAudioPresentationMode::Strategic
			: EUEGTAudioPresentationMode::MainMenu);
	}
	if (BoardActor != nullptr)
	{
		BoardActor->ClearBoard();
		BoardActor->SetActorHiddenInGame(true);
		BoardActor->SetActorEnableCollision(false);
	}
	if (GlobeActor != nullptr)
	{
		GlobeActor->SetPresentationEnabled(true);
	}
	if (HudWidget != nullptr)
	{
		HudWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (StrategicHudWidget != nullptr)
	{
		StrategicHudWidget->SetVisibility(ESlateVisibility::Visible);
	}

	if (!Instance->HasActiveCampaign())
	{
		CurrentStrategicSnapshot = FStrategicDashboardSnapshot();
		if (GlobeActor != nullptr)
		{
			GlobeActor->ClearGlobe();
		}
		FString ContentStatus;
		const TArray<FContentDiagnostic> Diagnostics = Instance->GetContentDiagnostics();
		if (!Diagnostics.IsEmpty())
		{
			TArray<FString> Messages;
			for (const FContentDiagnostic& Diagnostic : Diagnostics)
			{
				Messages.Add(FString::Printf(TEXT("[%s] %s"), *Diagnostic.Code.ToString(),
					*UEGTTacticalControllerPrivate::LocalizedDiagnostic(
						Diagnostic.Code, Diagnostic.Message)));
			}
			ContentStatus = FString::Join(Messages, TEXT("\n"));
		}
		else
		{
			TArray<FString> Packages;
			for (const FCampaignContentVersion& Version : Instance->GetLoadedContentVersions())
			{
				Packages.Add(FString::Printf(TEXT("%s %s"), *Version.PackageId.ToString(), *Version.Version));
			}
			ContentStatus = Packages.IsEmpty()
				? UEGTTacticalControllerPrivate::Localized(
					TEXT("menu.no-content-packages"), TEXT("No content packages loaded"))
				: FString::Join(Packages, TEXT("  •  "));
		}
		if (StrategicHudWidget != nullptr)
		{
			StrategicHudWidget->ShowMainMenu(Instance->IsContentReady(), ContentStatus);
		}
	}
	else
	{
		CurrentStrategicSnapshot = Instance->BuildStrategicDashboard();
		if (GlobeActor != nullptr)
		{
			GlobeActor->ApplySnapshot(CurrentStrategicSnapshot);
		}
		if (StrategicHudWidget != nullptr)
		{
			StrategicHudWidget->ApplySnapshot(CurrentStrategicSnapshot);
		}
	}
	if (!bGlobeFocused)
	{
		if (AUEGTTacticalCameraPawn* CameraPawn = GetTacticalCameraPawn())
		{
			CameraPawn->FocusGlobe(GlobeActor != nullptr ? GlobeActor->GetGlobeRadius() : 520.0f);
		}
		bGlobeFocused = true;
	}
}

void AUEGTTacticalPlayerController::PresentCommandResult(
	const FStrategicCommandResult& Result,
	const FString& AcceptedMessage)
{
	if (HudWidget != nullptr)
	{
		if (Result.bAccepted)
		{
			HudWidget->ShowStatusMessage(AcceptedMessage, false);
		}
		else
		{
			HudWidget->ShowStatusMessage(
				Result.Diagnostics.IsEmpty()
					? UEGTTacticalControllerPrivate::Localized(
						TEXT("tactical.command-rejected"), TEXT("The tactical command was rejected."))
					: UEGTTacticalControllerPrivate::LocalizedDiagnostic(
						Result.Diagnostics[0].Code, Result.Diagnostics[0].Message),
				true);
		}
	}
	PlayCommandAudio(Result, true);
	RefreshTacticalPresentation();
	if (Result.bAccepted && AutoSelectReadyTacticalUnit())
	{
		RefreshTacticalPresentation();
		if (const FTacticalHudUnitView* Unit = CurrentSnapshot.Units.FindByPredicate(
			[this](const FTacticalHudUnitView& Entry) { return Entry.UnitId == SelectedUnitId; }))
		{
			FocusCameraOnTacticalUnit(*Unit);
		}
	}
}

void AUEGTTacticalPlayerController::SetStrategicStatus(
	const FString& Message,
	const bool bIsError)
{
	if (StrategicHudWidget != nullptr)
	{
		StrategicHudWidget->ShowStatusMessage(Message, bIsError);
	}
}

void AUEGTTacticalPlayerController::PresentStrategicCommandResult(
	const FStrategicCommandResult& Result,
	const FString& AcceptedMessage)
{
	SetStrategicStatus(
		Result.bAccepted
			? AcceptedMessage
			: (Result.Diagnostics.IsEmpty()
				? UEGTTacticalControllerPrivate::Localized(
					TEXT("strategic.command-rejected"), TEXT("The strategic command was rejected."))
				: UEGTTacticalControllerPrivate::LocalizedDiagnostic(
					Result.Diagnostics[0].Code, Result.Diagnostics[0].Message)),
		!Result.bAccepted);
	PlayCommandAudio(Result, false);
	RefreshTacticalPresentation();
}

void AUEGTTacticalPlayerController::StartStrategicCampaign(
	const ECampaignDifficulty Difficulty,
	const int64 Seed,
	const EUEGTFundingModel FundingModel)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr || !Instance->StartNewCampaign(Difficulty, Seed, FundingModel))
	{
		SetStrategicStatus(FUEGTLocalizationService::Text(
			TEXT("strategic.campaign-start-failed"),
			TEXT("A new campaign could not start. Verify the content catalog and campaign settings.")), true);
		if (AudioDirector != nullptr)
		{
			AudioDirector->PlayCue(EUEGTAudioCue::CommandRejected);
		}
		return;
	}
	bViewingDebrief = false;
	const FString InitializedFormat = FUEGTLocalizationService::Text(
		TEXT("strategic.campaign-initialized-format"),
		TEXT("Campaign initialized with deterministic seed {0}."));
	SetStrategicStatus(FString::Format(
		*InitializedFormat,
		{ FString::Printf(TEXT("%lld"), Seed) }), false);
	if (AudioDirector != nullptr)
	{
		AudioDirector->PlayCue(EUEGTAudioCue::CommandAccepted);
	}
	RefreshTacticalPresentation();
}

void AUEGTTacticalPlayerController::ReloadContentCatalog()
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const bool bReloaded = Instance != nullptr && Instance->ReloadContent();
	RefreshStrategicPresentation();
	SetStrategicStatus(
		UEGTTacticalControllerPrivate::Localized(
			bReloaded ? TEXT("status.content-reloaded") : TEXT("status.content-reload-failed"),
			bReloaded
				? TEXT("Content and user mods reloaded successfully.")
				: TEXT("Content reload failed. Fix the listed diagnostics before starting a campaign.")),
		!bReloaded);
}

void AUEGTTacticalPlayerController::LoadDefaultCampaign()
{
	LoadCampaignSlot(TEXT("Campaign1"));
}

void AUEGTTacticalPlayerController::LoadCampaignSlot(const FString& SlotName)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	const FCampaignSaveStoreResult Result = Instance->LoadCampaign(SlotName);
	if (Result.bSucceeded)
	{
		bViewingDebrief = false;
		const FString LoadedFormat = FUEGTLocalizationService::Text(
			Result.bRecovered
				? TEXT("save.loaded-recovered-format")
				: TEXT("save.loaded-verified-format"),
			Result.bRecovered
				? TEXT("{0} recovered from a verified fallback and loaded.")
				: TEXT("{0} loaded and verified."));
		SetStrategicStatus(FString::Format(*LoadedFormat, { SlotName }), false);
	}
	else
	{
		const FString LoadFailedFormat = FUEGTLocalizationService::Text(
			TEXT("save.load-failed-format"), TEXT("{0} could not be loaded."));
		SetStrategicStatus(Result.Diagnostics.IsEmpty()
			? FString::Format(*LoadFailedFormat, { SlotName })
			: UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Result.Diagnostics[0].Code, Result.Diagnostics[0].Message), true);
	}
	RefreshTacticalPresentation();
}

void AUEGTTacticalPlayerController::SaveDefaultCampaign()
{
	SaveCampaignSlot(TEXT("Campaign1"));
}

void AUEGTTacticalPlayerController::SaveCampaignSlot(const FString& SlotName)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	const FCampaignSaveStoreResult Result = Instance->SaveCampaign(SlotName);
	const FString SavedFormat = FUEGTLocalizationService::Text(
		TEXT("save.saved-format"),
		TEXT("{0} saved with integrity verification and backup rotation."));
	const FString SaveFailedFormat = FUEGTLocalizationService::Text(
		TEXT("save.save-failed-format"), TEXT("{0} could not be saved."));
	SetStrategicStatus(Result.bSucceeded
		? FString::Format(*SavedFormat, { SlotName })
		: (Result.Diagnostics.IsEmpty()
			? FString::Format(*SaveFailedFormat, { SlotName })
			: UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Result.Diagnostics[0].Code, Result.Diagnostics[0].Message)),
		!Result.bSucceeded);
	RefreshTacticalPresentation();
}

void AUEGTTacticalPlayerController::EstablishStarterBase(const FName RegionId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicRegionView* Region = CurrentStrategicSnapshot.Regions.FindByPredicate(
		[RegionId](const FStrategicRegionView& View) { return View.RegionId == RegionId; });
	if (Instance == nullptr || Region == nullptr || !CurrentStrategicSnapshot.bRequiresBase)
	{
		SetStrategicStatus(FUEGTLocalizationService::Text(
			TEXT("strategic.founding-region-unavailable"),
			TEXT("The selected founding region is no longer available.")), true);
		return;
	}
	FEstablishBaseCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.BaseId = FGuid::NewGuid();
	Command.Name = Region->DisplayName + TEXT(" Command");
	Command.RegionId = Region->RegionId;
	Command.LongitudeMilliDegrees = Region->LongitudeMilliDegrees;
	Command.LatitudeMilliDegrees = Region->LatitudeMilliDegrees;
	Command.StartingFacilities.Add(TEXT("facility.operations-hub"));
	const FString EstablishedFormat = FUEGTLocalizationService::Text(
		TEXT("strategic.founding-established-format"),
		TEXT("{0} established. Expand the grid with a flight deck to begin air operations."));
	PresentStrategicCommandResult(
		Instance->EstablishBase(Command),
		FString::Format(*EstablishedFormat, { Command.Name }));
}

void AUEGTTacticalPlayerController::AdvanceStrategicClock(const EStrategicTimeRate Rate)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr || !CurrentStrategicSnapshot.bSucceeded)
	{
		return;
	}
	FAdvanceStrategicTimeCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.Rate = Rate;
	const FStrategicCommandResult Result = Instance->AdvanceStrategicTime(Command);
	const FString Message = Result.bDecisionPause
		? UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.time-advanced-paused-format"),
			TEXT("Simulation advanced through {0} slices and paused for a decision."),
			{ FString::FromInt(Result.ExecutedSlices) })
		: UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.time-advanced-format"),
			TEXT("Simulation advanced through {0} deterministic slices."),
			{ FString::FromInt(Result.ExecutedSlices) });
	PresentStrategicCommandResult(Result, Message);
}

void AUEGTTacticalPlayerController::ExecuteRegionalDiplomacy(
	const FName RegionId,
	const ERegionalDiplomacyActionType ActionType)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr || !CurrentStrategicSnapshot.bSucceeded)
	{
		return;
	}
	FRegionalDiplomacyCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.RegionId = RegionId;
	Command.ActionType = ActionType;
	FString ActionName;
	switch (ActionType)
	{
	case ERegionalDiplomacyActionType::CivicRelief:
		ActionName = FUEGTLocalizationService::Text(
			TEXT("strategic.region-action-civic-relief"), TEXT("Civic relief"));
		break;
	case ERegionalDiplomacyActionType::SecurityAccord:
		ActionName = FUEGTLocalizationService::Text(
			TEXT("strategic.region-action-security-accord"), TEXT("Security accord"));
		break;
	case ERegionalDiplomacyActionType::CrisisMobilization:
		ActionName = FUEGTLocalizationService::Text(
			TEXT("strategic.region-action-crisis-mobilization"), TEXT("Crisis mobilization"));
		break;
	default:
		ActionName = FUEGTLocalizationService::Text(TEXT("common.unknown"), TEXT("Unknown"));
		break;
	}
	const FString CompletedFormat = FUEGTLocalizationService::Text(
		TEXT("strategic.region-action-completed-format"), TEXT("{0} outreach completed."));
	PresentStrategicCommandResult(
		Instance->ExecuteRegionalDiplomacy(Command),
		FString::Format(*CompletedFormat, { ActionName }));
}

void AUEGTTacticalPlayerController::SignRegionalCharter(const FName RegionId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr || !CurrentStrategicSnapshot.bSucceeded)
	{
		return;
	}
	FSignRegionalCharterCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.RegionId = RegionId;
	PresentStrategicCommandResult(
		Instance->SignRegionalCharter(Command),
		FUEGTLocalizationService::Text(
			TEXT("strategic.region-charter-completed"),
			TEXT("Resilience Charter signed.")));
}

void AUEGTTacticalPlayerController::RatifyHorizonCompact()
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr || !CurrentStrategicSnapshot.bSucceeded)
	{
		return;
	}
	FRatifyHorizonCompactCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	PresentStrategicCommandResult(
		Instance->RatifyHorizonCompact(Command),
		FUEGTLocalizationService::Text(
			TEXT("strategic.coalition-compact-completed"),
			TEXT("Horizon Compact ratified.")));
}

void AUEGTTacticalPlayerController::DeployReciprocalAid(const FName TargetRegionId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr || !CurrentStrategicSnapshot.bSucceeded)
	{
		return;
	}
	FDeployReciprocalAidCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.TargetRegionId = TargetRegionId;
	PresentStrategicCommandResult(
		Instance->DeployReciprocalAid(Command),
		FUEGTLocalizationService::Text(
			TEXT("strategic.coalition-aid-completed"),
			TEXT("Reciprocal Aid deployed.")));
}

void AUEGTTacticalPlayerController::RestoreHorizonCompactMember(const FName RegionId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr || !CurrentStrategicSnapshot.bSucceeded)
	{
		return;
	}
	FRestoreHorizonCompactMemberCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.RegionId = RegionId;
	PresentStrategicCommandResult(
		Instance->RestoreHorizonCompactMember(Command),
		FUEGTLocalizationService::Text(
			TEXT("strategic.coalition-restoration-completed"),
			TEXT("Horizon Compact member restored.")));
}

void AUEGTTacticalPlayerController::CallHorizonCompactEmergencyVote(
	const FName TargetRegionId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr || !CurrentStrategicSnapshot.bSucceeded)
	{
		return;
	}
	FCallHorizonCompactEmergencyVoteCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.TargetRegionId = TargetRegionId;
	PresentStrategicCommandResult(
		Instance->CallHorizonCompactEmergencyVote(Command),
		FUEGTLocalizationService::Text(
			TEXT("strategic.coalition-emergency-vote-completed"),
			TEXT("Emergency solidarity motion passed.")));
}

void AUEGTTacticalPlayerController::StartStrategicFacilityConstruction(
	const FName FacilityId,
	const FGuid BaseId,
	const int32 GridX,
	const int32 GridY)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicActionOptionView* Option = CurrentStrategicSnapshot.ActionOptions.FindByPredicate(
		[FacilityId](const FStrategicActionOptionView& View)
		{
			return View.Type == EStrategicActionOptionType::Facility && View.RuleId == FacilityId;
		});
	const FStrategicBaseView* Base = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[BaseId](const FStrategicBaseView& View) { return View.BaseId == BaseId; });
	if (Instance == nullptr || Option == nullptr || Base == nullptr || !Option->bAvailable)
	{
		const FString Fallback = UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.facility-placement-unavailable"),
			TEXT("The selected facility placement is no longer available."));
		SetStrategicStatus(Option != nullptr
			? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Option->UnavailableReasonCode,
				Option->UnavailableReason.IsEmpty() ? Fallback : Option->UnavailableReason)
			: Fallback, true);
		return;
	}
	FStartFacilityConstructionCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.ProjectId = FGuid::NewGuid();
	Command.FacilityInstanceId = FGuid::NewGuid();
	Command.BaseId = BaseId;
	Command.FacilityId = FacilityId;
	Command.GridX = GridX;
	Command.GridY = GridY;
	PresentStrategicCommandResult(
		Instance->StartFacilityConstruction(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.facility-construction-started-format"),
			TEXT("Construction started: {0} at {1} grid {2},{3}."),
			{
				FUEGTLocalizationService::ContentName(FacilityId, Option->DisplayName),
				Base->Name,
				FString::FromInt(GridX),
				FString::FromInt(GridY)
			}));
}

void AUEGTTacticalPlayerController::DismantleStrategicFacility(
	const FGuid BaseId,
	const FGuid FacilityInstanceId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicBaseView* Base = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[BaseId](const FStrategicBaseView& View) { return View.BaseId == BaseId; });
	const FStrategicFacilityView* Facility = Base != nullptr
		? Base->FacilityLayout.FindByPredicate(
			[FacilityInstanceId](const FStrategicFacilityView& View)
			{
				return !View.bConstructing && View.FacilityInstanceId == FacilityInstanceId;
			})
		: nullptr;
	if (Instance == nullptr || Facility == nullptr || !Facility->bCanDismantle)
	{
		const FString Fallback = UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.facility-dismantle-unavailable"),
			TEXT("The selected installed facility cannot currently be dismantled."));
		SetStrategicStatus(Facility != nullptr
			? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Facility->DismantleUnavailableReasonCode,
				Facility->DismantleUnavailableReason.IsEmpty()
					? Fallback : Facility->DismantleUnavailableReason)
			: Fallback, true);
		return;
	}
	const FString FacilityName = FUEGTLocalizationService::ContentName(
		Facility->FacilityId, Facility->DisplayName);
	const int64 Refund = Facility->DismantleRefund;
	FDismantleFacilityCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.BaseId = BaseId;
	Command.FacilityInstanceId = FacilityInstanceId;
	PresentStrategicCommandResult(
		Instance->DismantleFacility(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.facility-dismantled-format"),
			TEXT("{0} dismantled • {1} salvage recovered."),
			{ FacilityName, LexToString(Refund) }));
}

void AUEGTTacticalPlayerController::RepairStrategicFacility(
	const FGuid BaseId,
	const FGuid FacilityInstanceId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicBaseView* Base = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[BaseId](const FStrategicBaseView& View) { return View.BaseId == BaseId; });
	const FStrategicFacilityView* Facility = Base != nullptr
		? Base->FacilityLayout.FindByPredicate(
			[FacilityInstanceId](const FStrategicFacilityView& View)
			{
				return !View.bConstructing && View.FacilityInstanceId == FacilityInstanceId;
			})
		: nullptr;
	if (Instance == nullptr || Facility == nullptr || !Facility->bCanRepair)
	{
		const FString Fallback = UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.facility-repair-unavailable"),
			TEXT("The selected facility cannot currently be repaired."));
		SetStrategicStatus(Facility != nullptr
			? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Facility->RepairUnavailableReasonCode,
				Facility->RepairUnavailableReason.IsEmpty()
					? Fallback : Facility->RepairUnavailableReason)
			: Fallback, true);
		return;
	}
	const FString FacilityName = FUEGTLocalizationService::ContentName(
		Facility->FacilityId, Facility->DisplayName);
	const int64 Cost = Facility->RepairCost;
	const int64 DurationHours = UEGTTacticalControllerPrivate::CeilHours(
		Facility->RepairDurationSeconds);
	FStartFacilityRepairCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.BaseId = BaseId;
	Command.FacilityInstanceId = FacilityInstanceId;
	PresentStrategicCommandResult(
		Instance->StartFacilityRepair(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.facility-repair-started-format"),
			TEXT("{0} repair started • {1} reserved • {2} h."),
			{ FacilityName, LexToString(Cost), LexToString(DurationHours) }));
}

void AUEGTTacticalPlayerController::CancelStrategicFacilityRepair(
	const FGuid BaseId,
	const FGuid FacilityInstanceId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicBaseView* Base = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[BaseId](const FStrategicBaseView& View) { return View.BaseId == BaseId; });
	const FStrategicFacilityView* Facility = Base != nullptr
		? Base->FacilityLayout.FindByPredicate(
			[FacilityInstanceId](const FStrategicFacilityView& View)
			{
				return !View.bConstructing && View.FacilityInstanceId == FacilityInstanceId;
			})
		: nullptr;
	if (Instance == nullptr || Facility == nullptr || !Facility->bRepairing)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.facility-repair-cancel-unavailable"),
			TEXT("The selected facility has no active repair to cancel.")), true);
		return;
	}
	const FString FacilityName = FUEGTLocalizationService::ContentName(
		Facility->FacilityId, Facility->DisplayName);
	const int64 Refund = Facility->RepairCancellationRefund;
	FCancelFacilityRepairCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.BaseId = BaseId;
	Command.FacilityInstanceId = FacilityInstanceId;
	PresentStrategicCommandResult(
		Instance->CancelFacilityRepair(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.facility-repair-cancelled-format"),
			TEXT("{0} repair cancelled • {1} returned."),
			{ FacilityName, LexToString(Refund) }));
}

void AUEGTTacticalPlayerController::ExecuteStrategicOption(
	const EStrategicActionOptionType Type,
	const FName RuleId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicActionOptionView* Option = CurrentStrategicSnapshot.ActionOptions.FindByPredicate(
		[Type, RuleId](const FStrategicActionOptionView& View)
		{
			return View.Type == Type && View.RuleId == RuleId;
		});
	if (Instance == nullptr || Option == nullptr || !Option->bAvailable
		|| !CurrentStrategicSnapshot.PrimaryBaseId.IsValid())
	{
		const FString Fallback = UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.order-unavailable"),
			TEXT("The selected strategic order is not currently available."));
		SetStrategicStatus(Option != nullptr && !Option->bAvailable
			? Type == EStrategicActionOptionType::Craft
				? UEGTTacticalControllerPrivate::LocalizedCraftUnavailableReason(*Option)
				: UEGTTacticalControllerPrivate::LocalizedDiagnostic(
					Option->UnavailableReasonCode,
					Option->UnavailableReason.IsEmpty() ? Fallback : Option->UnavailableReason)
			: Fallback, true);
		return;
	}
	const FGuid BaseId = CurrentStrategicSnapshot.PrimaryBaseId;
	FStrategicCommandResult Result;
	FString AcceptedMessage;
	switch (Type)
	{
	case EStrategicActionOptionType::Research:
	{
		const FString ResearchDisplayName =
			FUEGTLocalizationService::ContentName(RuleId, Option->DisplayName);
		FStartResearchCommand Command;
		Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
		Command.BaseId = BaseId;
		Command.ResearchId = RuleId;
		Result = Instance->StartResearch(Command);
		if (!Result.bAccepted)
		{
			PresentStrategicCommandResult(Result, FString());
			return;
		}
		const FStrategicDashboardSnapshot AfterStart = Instance->BuildStrategicDashboard();
		const FStrategicBaseView* Base = AfterStart.Bases.FindByPredicate(
			[BaseId](const FStrategicBaseView& View) { return View.BaseId == BaseId; });
		const int32 AvailableStaff = Base != nullptr
			? FMath::Max(0, Base->ScientistCapacity - Base->AssignedScientists)
			: 0;
		if (AvailableStaff > 0)
		{
			FSetResearchStaffCommand Staff;
			Staff.ExpectedSequence = AfterStart.ExpectedCommandSequence;
			Staff.ResearchId = RuleId;
			Staff.AssignedScientists = FMath::Min(5, AvailableStaff);
			const FStrategicCommandResult StaffResult = Instance->SetResearchStaff(Staff);
			if (!StaffResult.bAccepted)
			{
				SetStrategicStatus(StaffResult.Diagnostics.IsEmpty()
					? UEGTTacticalControllerPrivate::Localized(
						TEXT("strategic.research-auto-staffing-rejected"),
						TEXT("Research began, but automatic staffing was rejected."))
					: UEGTTacticalControllerPrivate::LocalizedFormat(
						TEXT("strategic.research-staffing-failed-format"),
						TEXT("Research began, but staffing failed: {0}"),
						{ UEGTTacticalControllerPrivate::LocalizedDiagnostic(
							StaffResult.Diagnostics[0].Code, StaffResult.Diagnostics[0].Message) }), true);
				RefreshTacticalPresentation();
				return;
			}
			Result = StaffResult;
			AcceptedMessage = UEGTTacticalControllerPrivate::LocalizedFormat(
				TEXT("strategic.research-started-format"),
				TEXT("{0} started with {1} scientists."),
				{ ResearchDisplayName, FString::FromInt(Staff.AssignedScientists) });
		}
		else
		{
			AcceptedMessage = UEGTTacticalControllerPrivate::LocalizedFormat(
				TEXT("strategic.research-started-unstaffed-format"),
				TEXT("{0} started unstaffed; reassign scientists when capacity opens."),
				{ ResearchDisplayName });
		}
		break;
	}
	case EStrategicActionOptionType::Facility:
	{
		StartStrategicFacilityConstruction(RuleId, BaseId, Option->SuggestedGridX, Option->SuggestedGridY);
		return;
	}
	case EStrategicActionOptionType::Personnel:
	{
		int32 Inbound = 0;
		for (const FStrategicProjectView& Project : CurrentStrategicSnapshot.Projects)
		{
			Inbound += Project.Type == EStrategicProjectType::Recruitment ? 1 : 0;
		}
		FRecruitPersonnelCommand Command;
		Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
		Command.OrderId = FGuid::NewGuid();
		Command.PersonnelId = FGuid::NewGuid();
		Command.BaseId = BaseId;
		Command.RoleId = RuleId;
		Command.DisplayName = FString::Printf(TEXT("%s %03d"), *Option->DisplayName, CurrentStrategicSnapshot.Personnel.Num() + Inbound + 1);
		Result = Instance->RecruitPersonnel(Command);
		AcceptedMessage = UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.recruitment-order-placed-format"),
			TEXT("Recruitment order placed for {0}."),
			{ Command.DisplayName });
		break;
	}
	case EStrategicActionOptionType::Craft:
	{
		int32 Inbound = 0;
		for (const FStrategicProjectView& Project : CurrentStrategicSnapshot.Projects)
		{
			Inbound += Project.Type == EStrategicProjectType::CraftAcquisition ? 1 : 0;
		}
		FAcquireCraftCommand Command;
		Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
		Command.OrderId = FGuid::NewGuid();
		Command.CraftId = FGuid::NewGuid();
		Command.BaseId = BaseId;
		Command.CraftRuleId = RuleId;
		const FString CraftTypeName = FUEGTLocalizationService::ContentName(RuleId, Option->DisplayName);
		Command.DisplayName = FString::Printf(
			TEXT("%s %02d"), *CraftTypeName, CurrentStrategicSnapshot.Craft.Num() + Inbound + 1);
		Result = Instance->AcquireCraft(Command);
		AcceptedMessage = UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.craft-acquisition-placed-format"),
			TEXT("Acquisition order placed for {0}."),
			{ Command.DisplayName });
		break;
	}
	case EStrategicActionOptionType::Manufacturing:
	{
		const FItemRule* ItemRule = Instance->GetLoadedRules().Items.Find(RuleId);
		const int32 Units = ItemRule != nullptr && ItemRule->Category == FName(TEXT("craft-ammunition")) ? 12
			: ItemRule != nullptr && ItemRule->Category == FName(TEXT("ammunition")) ? 4
			: ItemRule != nullptr && ItemRule->Category == FName(TEXT("device")) ? 2
			: 1;
		StartStrategicManufacturing(RuleId, Units);
		return;
	}
	default:
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.option-unknown"), TEXT("Unknown strategic option type.")), true);
		return;
	}
	PresentStrategicCommandResult(Result, AcceptedMessage);
}

void AUEGTTacticalPlayerController::AdjustStrategicProjectStaff(
	const EStrategicProjectType Type,
	const FGuid ProjectId,
	const FName RuleId,
	const int32 Delta)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicProjectView* Project = CurrentStrategicSnapshot.Projects.FindByPredicate(
		[Type, ProjectId, RuleId](const FStrategicProjectView& View)
		{
			return View.Type == Type && (Type == EStrategicProjectType::Research
				? View.RuleId == RuleId
				: Type == EStrategicProjectType::Manufacturing && View.ProjectId == ProjectId);
		});
	if (Instance == nullptr || Project == nullptr || Delta == 0
		|| (Type != EStrategicProjectType::Research && Type != EStrategicProjectType::Manufacturing))
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.project-staffing-unavailable"),
			TEXT("The selected program cannot accept that staffing order.")), true);
		return;
	}
	const int64 Desired64 = static_cast<int64>(Project->AssignedStaff) + static_cast<int64>(Delta);
	if (Desired64 < 0 || Desired64 > MAX_int32)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.project-staffing-range"),
			TEXT("The requested staffing level is outside the supported range.")), true);
		return;
	}

	FStrategicCommandResult Result;
	if (Type == EStrategicProjectType::Research)
	{
		FSetResearchStaffCommand Command;
		Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
		Command.ResearchId = Project->RuleId;
		Command.AssignedScientists = static_cast<int32>(Desired64);
		Result = Instance->SetResearchStaff(Command);
	}
	else
	{
		FSetManufacturingStaffCommand Command;
		Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
		Command.ProjectId = Project->ProjectId;
		Command.AssignedEngineers = static_cast<int32>(Desired64);
		Result = Instance->SetManufacturingStaff(Command);
	}
	PresentStrategicCommandResult(Result, UEGTTacticalControllerPrivate::LocalizedFormat(
		TEXT("strategic.project-staffing-set-format"),
		TEXT("{0} staffing set to {1}."),
		{ FUEGTLocalizationService::ContentName(Project->RuleId, Project->DisplayName), LexToString(Desired64) }));
}

void AUEGTTacticalPlayerController::StartStrategicManufacturing(
	const FName ItemId,
	const int32 Units)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicActionOptionView* Option = CurrentStrategicSnapshot.ActionOptions.FindByPredicate(
		[ItemId](const FStrategicActionOptionView& View)
		{
			return View.Type == EStrategicActionOptionType::Manufacturing && View.RuleId == ItemId;
		});
	if (Instance == nullptr || Option == nullptr || !Option->bAvailable
		|| !CurrentStrategicSnapshot.PrimaryBaseId.IsValid() || Units <= 0)
	{
		SetStrategicStatus(Option != nullptr && !Option->bAvailable
			? UEGTTacticalControllerPrivate::LocalizedManufacturingUnavailableReason(
				*Option, CurrentStrategicSnapshot, Units)
			: UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.manufacturing-batch-unavailable"),
				TEXT("The selected manufacturing batch is not currently available.")), true);
		return;
	}
	FStartManufacturingCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.ProjectId = FGuid::NewGuid();
	Command.BaseId = CurrentStrategicSnapshot.PrimaryBaseId;
	Command.ItemId = ItemId;
	Command.Units = Units;
	FStrategicCommandResult Result = Instance->StartManufacturing(Command);
	if (!Result.bAccepted)
	{
		PresentStrategicCommandResult(Result, FString());
		return;
	}

	const FStrategicDashboardSnapshot AfterStart = Instance->BuildStrategicDashboard();
	const FStrategicBaseView* Base = AfterStart.Bases.FindByPredicate(
		[&Command](const FStrategicBaseView& View) { return View.BaseId == Command.BaseId; });
	const int32 AvailableStaff = Base != nullptr
		? FMath::Max(0, Base->EngineerCapacity - Base->AssignedEngineers)
		: 0;
	TArray<FString> ReservedMaterials;
	for (const FStrategicMaterialRequirementView& Requirement : Option->MaterialRequirements)
	{
		const FString MaterialName = FUEGTLocalizationService::ContentName(
			Requirement.ItemId, Requirement.DisplayName);
		ReservedMaterials.Add(UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.manufacturing-material-quantity-format"), TEXT("{0} {1}"),
			{ LexToString(static_cast<int64>(Requirement.PerUnitQuantity) * Units), MaterialName }));
	}
	const FString OptionDisplayName = FUEGTLocalizationService::ContentName(ItemId, Option->DisplayName);
	const FString MaterialSuffix = ReservedMaterials.IsEmpty()
		? FString()
		: UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.production-inputs-reserved-format"), TEXT(" Inputs reserved: {0}."),
			{ FString::Join(ReservedMaterials, TEXT(", ")) });
	FString AcceptedMessage;
	if (AvailableStaff > 0)
	{
		FSetManufacturingStaffCommand Staff;
		Staff.ExpectedSequence = AfterStart.ExpectedCommandSequence;
		Staff.ProjectId = Command.ProjectId;
		Staff.AssignedEngineers = FMath::Min(5, AvailableStaff);
		const FStrategicCommandResult StaffResult = Instance->SetManufacturingStaff(Staff);
		if (!StaffResult.bAccepted)
		{
			SetStrategicStatus(StaffResult.Diagnostics.IsEmpty()
				? UEGTTacticalControllerPrivate::Localized(
					TEXT("strategic.production-auto-staffing-rejected"),
					TEXT("Production began, but automatic staffing was rejected."))
				: UEGTTacticalControllerPrivate::LocalizedFormat(
					TEXT("strategic.production-staffing-failed-format"),
					TEXT("Production began, but staffing failed: {0}"),
					{ UEGTTacticalControllerPrivate::LocalizedDiagnostic(
						StaffResult.Diagnostics[0].Code, StaffResult.Diagnostics[0].Message) }), true);
			RefreshTacticalPresentation();
			return;
		}
		Result = StaffResult;
		AcceptedMessage = UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.production-started-format"),
			TEXT("{0} ×{1} entered production with {2} engineers.{3}"),
			{ OptionDisplayName, FString::FromInt(Units), FString::FromInt(Staff.AssignedEngineers), MaterialSuffix });
	}
	else
	{
		AcceptedMessage = UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.production-started-unstaffed-format"),
			TEXT("{0} ×{1} entered the production queue unstaffed.{2}"),
			{ OptionDisplayName, FString::FromInt(Units), MaterialSuffix });
	}
	PresentStrategicCommandResult(Result, AcceptedMessage);
}

void AUEGTTacticalPlayerController::AdjustStrategicManufacturingUnits(
	const FGuid ProjectId,
	const int32 DeltaUnits)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicProjectView* Project = CurrentStrategicSnapshot.Projects.FindByPredicate(
		[ProjectId](const FStrategicProjectView& View)
		{
			return View.Type == EStrategicProjectType::Manufacturing && View.ProjectId == ProjectId;
		});
	if (Instance == nullptr || Project == nullptr || DeltaUnits == 0)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.production-quantity-unavailable"),
			TEXT("The selected production run cannot accept that quantity change.")), true);
		return;
	}
	const int64 NewUnits = static_cast<int64>(Project->UnitsRemaining) + static_cast<int64>(DeltaUnits);
	const int64 Delta64 = DeltaUnits;
	const int64 Funds = (Delta64 > 0 ? Delta64 : -Delta64) * Project->UnitCost;
	TArray<FString> ChangedMaterials;
	for (const FStrategicMaterialRequirementView& Requirement : Project->MaterialRequirements)
	{
		const FString MaterialName = FUEGTLocalizationService::ContentName(
			Requirement.ItemId, Requirement.DisplayName);
		ChangedMaterials.Add(UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.manufacturing-material-quantity-format"), TEXT("{0} {1}"),
			{
				LexToString((Delta64 > 0 ? Delta64 : -Delta64) * Requirement.PerUnitQuantity),
				MaterialName
			}));
	}
	const FString MaterialChange = ChangedMaterials.IsEmpty()
		? FString()
		: UEGTTacticalControllerPrivate::LocalizedFormat(
			DeltaUnits > 0
				? TEXT("strategic.production-materials-reserved-format")
				: TEXT("strategic.production-materials-returned-format"),
			DeltaUnits > 0 ? TEXT(" and reserved {0}") : TEXT(" and returned {0}"),
			{ FString::Join(ChangedMaterials, TEXT(", ")) });
	const FString DisplayName = FUEGTLocalizationService::ContentName(Project->RuleId, Project->DisplayName);
	FAdjustManufacturingUnitsCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.ProjectId = ProjectId;
	Command.DeltaUnits = DeltaUnits;
	PresentStrategicCommandResult(
		Instance->AdjustManufacturingUnits(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.production-run-updated-format"),
			TEXT("{0} production run set to {1} units • {2} {3}{4}."),
			{
				DisplayName,
				LexToString(NewUnits),
				DeltaUnits > 0
					? UEGTTacticalControllerPrivate::Localized(
						TEXT("strategic.production-reserved"), TEXT("reserved"))
					: UEGTTacticalControllerPrivate::Localized(
						TEXT("strategic.production-refunded"), TEXT("refunded")),
				LexToString(Funds),
				MaterialChange
			}));
}

void AUEGTTacticalPlayerController::CancelStrategicProject(
	const EStrategicProjectType Type,
	const FGuid ProjectId,
	const FName RuleId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicProjectView* Project = CurrentStrategicSnapshot.Projects.FindByPredicate(
		[Type, ProjectId, RuleId](const FStrategicProjectView& View)
		{
			return View.Type == Type && (Type == EStrategicProjectType::Research
				? View.RuleId == RuleId
				: View.ProjectId == ProjectId);
		});
	if (Instance == nullptr || Project == nullptr
		|| (Type != EStrategicProjectType::Research
			&& Type != EStrategicProjectType::Manufacturing
			&& Type != EStrategicProjectType::Construction))
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.project-cancel-unavailable"),
			TEXT("The selected program cannot be cancelled.")), true);
		return;
	}
	FStrategicCommandResult Result;
	if (Type == EStrategicProjectType::Research)
	{
		FCancelResearchCommand Command;
		Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
		Command.ResearchId = Project->RuleId;
		Result = Instance->CancelResearch(Command);
	}
	else if (Type == EStrategicProjectType::Manufacturing)
	{
		FCancelManufacturingCommand Command;
		Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
		Command.ProjectId = Project->ProjectId;
		Result = Instance->CancelManufacturing(Command);
	}
	else
	{
		FCancelFacilityConstructionCommand Command;
		Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
		Command.ProjectId = Project->ProjectId;
		Result = Instance->CancelFacilityConstruction(Command);
	}
	TArray<FString> ReturnedMaterials;
	if (Type == EStrategicProjectType::Manufacturing)
	{
		for (const FStrategicMaterialRequirementView& Requirement : Project->MaterialRequirements)
		{
			if (Requirement.RefundableQuantity > 0)
			{
				const FString MaterialName = FUEGTLocalizationService::ContentName(
					Requirement.ItemId, Requirement.DisplayName);
				ReturnedMaterials.Add(UEGTTacticalControllerPrivate::LocalizedFormat(
					TEXT("strategic.manufacturing-material-quantity-format"), TEXT("{0} {1}"),
					{ LexToString(Requirement.RefundableQuantity), MaterialName }));
			}
		}
	}
	const FString ProjectDisplayName = FUEGTLocalizationService::ContentName(Project->RuleId, Project->DisplayName);
	const FString Message = Type == EStrategicProjectType::Manufacturing
		? ReturnedMaterials.IsEmpty()
			? UEGTTacticalControllerPrivate::LocalizedFormat(
				TEXT("strategic.production-cancelled-format"),
				TEXT("{0} cancelled • refund {1}."),
				{ ProjectDisplayName, LexToString(Project->CancellationRefund) })
			: UEGTTacticalControllerPrivate::LocalizedFormat(
				TEXT("strategic.production-cancelled-materials-format"),
				TEXT("{0} cancelled • refund {1} and return {2}."),
				{
					ProjectDisplayName,
					LexToString(Project->CancellationRefund),
					FString::Join(ReturnedMaterials, TEXT(", "))
				})
		: Type == EStrategicProjectType::Research
			? UEGTTacticalControllerPrivate::LocalizedFormat(
				TEXT("strategic.research-cancelled-format"),
				TEXT("{0} cancelled; assigned scientists released."),
				{ ProjectDisplayName })
			: UEGTTacticalControllerPrivate::LocalizedFormat(
				TEXT("strategic.construction-cancelled-format"),
				TEXT("{0} cancelled • refund {1}."),
				{ ProjectDisplayName, LexToString(Project->CancellationRefund) });
	PresentStrategicCommandResult(Result, Message);
}

void AUEGTTacticalPlayerController::SellStrategicInventory(
	const FGuid BaseId,
	const FName ItemId,
	const int32 Quantity)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicBaseView* Base = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[BaseId](const FStrategicBaseView& View) { return View.BaseId == BaseId; });
	const FStrategicInventoryView* Item = Base != nullptr
		? Base->Inventory.FindByPredicate(
			[ItemId](const FStrategicInventoryView& View) { return View.ItemId == ItemId; })
		: nullptr;
	if (Instance == nullptr || Item == nullptr || Item->UnitSellValue <= 0
		|| Quantity <= 0 || Quantity > Item->Quantity)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.inventory-unavailable"),
			TEXT("The selected inventory disposition is no longer available.")), true);
		return;
	}
	FSellInventoryCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.BaseId = BaseId;
	Command.ItemId = ItemId;
	Command.Quantity = Quantity;
	PresentStrategicCommandResult(
		Instance->SellInventory(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.inventory-sold-format"), TEXT("Sold {0} × {1} for {2}."),
			{
				FString::FromInt(Quantity),
				FUEGTLocalizationService::ContentName(Item->ItemId, Item->DisplayName),
				LexToString(static_cast<int64>(Quantity) * Item->UnitSellValue)
			}));
}

void AUEGTTacticalPlayerController::DispatchStrategicMutualAidConvoy(
	const FGuid SourceBaseId,
	const FGuid DestinationBaseId,
	const FName ItemId,
	const int32 Quantity,
	const EMutualAidRoutePolicy RoutePolicy,
	const bool bSignalEscort)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicBaseView* Source = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[SourceBaseId](const FStrategicBaseView& View) { return View.BaseId == SourceBaseId; });
	const FStrategicBaseView* Destination = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[DestinationBaseId](const FStrategicBaseView& View)
		{
			return View.BaseId == DestinationBaseId;
		});
	const FStrategicInventoryView* Item = Source != nullptr
		? Source->Inventory.FindByPredicate(
			[ItemId](const FStrategicInventoryView& View) { return View.ItemId == ItemId; })
		: nullptr;
	const FStrategicMutualAidDispatchOptionView* Option = Item != nullptr
		? Item->MutualAidOptions.FindByPredicate(
			[DestinationBaseId](const FStrategicMutualAidDispatchOptionView& View)
			{
				return View.DestinationBaseId == DestinationBaseId;
			})
		: nullptr;
	const FStrategicMutualAidRouteOptionView* Route = Option != nullptr
		? Option->Routes.FindByPredicate(
			[RoutePolicy](const FStrategicMutualAidRouteOptionView& View)
			{
				return View.Policy == RoutePolicy;
			})
		: nullptr;
	if (Instance == nullptr || Source == nullptr || Destination == nullptr || Item == nullptr
		|| Option == nullptr || Route == nullptr || !Option->bEnabled || Quantity <= 0
		|| Quantity > Option->MaximumQuantity
		|| (bSignalEscort && !Route->bSignalEscortAffordable))
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.mutual-aid-unavailable"),
			TEXT("The selected Mutual Aid Convoy is no longer available.")), true);
		return;
	}

	FDispatchMutualAidConvoyCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.SourceBaseId = SourceBaseId;
	Command.DestinationBaseId = DestinationBaseId;
	Command.ItemId = ItemId;
	Command.Quantity = Quantity;
	Command.RoutePolicy = RoutePolicy;
	Command.bSignalEscort = bSignalEscort;
	const FString SourceName = Source->Name;
	const FString DestinationName = Destination->Name;
	const FString ItemName = FUEGTLocalizationService::ContentName(Item->ItemId, Item->DisplayName);
	const int64 TransitHours = UEGTTacticalControllerPrivate::CeilHours(Route->TransitSeconds);
	const FString RouteName =
		UEGTTacticalControllerPrivate::LocalizedMutualAidRoute(RoutePolicy);
	const FString EscortState = bSignalEscort
		? UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.mutual-aid-escort-confirmed"), TEXT("Signal Escort confirmed"))
		: UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.mutual-aid-unescorted"), TEXT("unescorted"));
	PresentStrategicCommandResult(
		Instance->DispatchMutualAidConvoy(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.mutual-aid-dispatched-format"),
			TEXT("Dispatched {0} × {1} from {2} to {3} via {4}; {5} hours, exposure {6}/100, {7}."),
			{
				FString::FromInt(Quantity), ItemName, SourceName,
				DestinationName, RouteName, LexToString(TransitHours),
				FString::FromInt(Route->RoutePressure), EscortState
			}));
}

void AUEGTTacticalPlayerController::RetuneStrategicMutualAidConvoy(
	const FGuid ConvoyId,
	const EMutualAidRoutePolicy RoutePolicy)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicMutualAidConvoyView* Convoy =
		CurrentStrategicSnapshot.MutualAidConvoys.FindByPredicate(
			[ConvoyId](const FStrategicMutualAidConvoyView& View)
			{
				return View.ConvoyId == ConvoyId;
			});
	const FStrategicMutualAidRouteOptionView* Route = Convoy != nullptr
		? Convoy->RetuneRoutes.FindByPredicate(
			[RoutePolicy](const FStrategicMutualAidRouteOptionView& View)
			{
				return View.Policy == RoutePolicy;
			})
		: nullptr;
	if (Instance == nullptr || Convoy == nullptr || Route == nullptr || !Route->bEnabled)
	{
		const FString Reason = Route != nullptr && !Route->UnavailableReason.IsEmpty()
			? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Route->UnavailableReasonCode, Route->UnavailableReason)
			: Convoy != nullptr && !Convoy->RetuneUnavailableReason.IsEmpty()
				? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
					Convoy->RetuneUnavailableReasonCode,
					Convoy->RetuneUnavailableReason)
				: UEGTTacticalControllerPrivate::Localized(
					TEXT("strategic.mutual-aid-unavailable"),
					TEXT("The selected Mutual Aid Convoy is no longer available."));
		SetStrategicStatus(Reason, true);
		return;
	}

	FRetuneMutualAidConvoyCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.ConvoyId = ConvoyId;
	Command.RoutePolicy = RoutePolicy;
	const int64 TransitHours = Route->TransitSeconds <= 0
		? 0
		: 1 + (Route->TransitSeconds - 1) / 3600;
	const int64 ArrivalHours = Route->RelayQueue.EstimatedArrivalSeconds <= 0
		? 0
		: 1 + (Route->RelayQueue.EstimatedArrivalSeconds - 1) / 3600;
	PresentStrategicCommandResult(
		Instance->RetuneMutualAidConvoy(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.mutual-aid-retuned-format"),
			TEXT("Retuned aid to {0} via {1}; {2} hours, exposure {3}/100, projected arrival {4} hours. Cargo, storage, escort, and funds are unchanged."),
			{
				Convoy->DestinationBaseName,
				UEGTTacticalControllerPrivate::LocalizedMutualAidRoute(RoutePolicy),
				LexToString(TransitHours), FString::FromInt(Route->RoutePressure),
				LexToString(ArrivalHours)
			}));
}

void AUEGTTacticalPlayerController::CommissionStrategicMutualAidSignalEscort(
	const FGuid ConvoyId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicMutualAidConvoyView* Convoy =
		CurrentStrategicSnapshot.MutualAidConvoys.FindByPredicate(
			[ConvoyId](const FStrategicMutualAidConvoyView& View)
			{
				return View.ConvoyId == ConvoyId;
			});
	if (Instance == nullptr || Convoy == nullptr
		|| !Convoy->bCanCommissionSignalEscort)
	{
		const FString Reason = Convoy != nullptr
			&& !Convoy->SignalEscortCommissionUnavailableReason.IsEmpty()
			? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Convoy->SignalEscortCommissionUnavailableReasonCode,
				Convoy->SignalEscortCommissionUnavailableReason)
			: UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.mutual-aid-unavailable"),
				TEXT("The selected Mutual Aid Convoy is no longer available."));
		SetStrategicStatus(Reason, true);
		return;
	}

	FCommissionMutualAidSignalEscortCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.ConvoyId = ConvoyId;
	const int64 PreventedHours = Convoy->SignalEscortPreventedDelaySeconds <= 0
		? 0
		: 1 + (Convoy->SignalEscortPreventedDelaySeconds - 1) / 3600;
	const int64 ArrivalHours =
		Convoy->SignalEscortProjectedRelayQueue.EstimatedArrivalSeconds <= 0
		? 0
		: 1 + (Convoy->SignalEscortProjectedRelayQueue.EstimatedArrivalSeconds - 1) / 3600;
	PresentStrategicCommandResult(
		Instance->CommissionMutualAidSignalEscort(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.mutual-aid-signal-surety-commissioned-format"),
			TEXT("Signal Surety commissioned for aid to {0}; {1} funds committed, forecast delay {2} hours prevented, projected arrival {3} hours. Cargo, storage, route, identity, and FIFO order are unchanged."),
			{
				Convoy->DestinationBaseName,
				LexToString(Convoy->SignalEscortCommissionCost),
				LexToString(PreventedHours), LexToString(ArrivalHours)
			}));
}

void AUEGTTacticalPlayerController::PrioritizeStrategicMutualAidConvoy(
	const FGuid ConvoyId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicMutualAidConvoyView* Convoy =
		CurrentStrategicSnapshot.MutualAidConvoys.FindByPredicate(
			[ConvoyId](const FStrategicMutualAidConvoyView& View)
			{
				return View.ConvoyId == ConvoyId;
			});
	if (Instance == nullptr || Convoy == nullptr || !Convoy->bCanPrioritizeRelief)
	{
		const FString Reason = Convoy != nullptr
			&& !Convoy->ReliefPriorityUnavailableReason.IsEmpty()
			? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Convoy->ReliefPriorityUnavailableReasonCode,
				Convoy->ReliefPriorityUnavailableReason)
			: UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.mutual-aid-unavailable"),
				TEXT("The selected Mutual Aid Convoy is no longer available."));
		SetStrategicStatus(Reason, true);
		return;
	}

	FPrioritizeMutualAidConvoyCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.ConvoyId = ConvoyId;
	const int64 RecoveredHours = Convoy->ReliefPriorityRecoveredWaitSeconds <= 0
		? 0
		: 1 + (Convoy->ReliefPriorityRecoveredWaitSeconds - 1) / 3600;
	const int64 ArrivalHours =
		Convoy->ReliefPriorityProjectedRelayQueue.EstimatedArrivalSeconds <= 0
		? 0
		: 1 + (Convoy->ReliefPriorityProjectedRelayQueue.EstimatedArrivalSeconds - 1) / 3600;
	PresentStrategicCommandResult(
		Instance->PrioritizeMutualAidConvoy(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.mutual-aid-relief-priority-set-format"),
			TEXT("Relief Priority set for aid to {0}; advanced past {1} held commitments, recovered {2} queue hours, projected arrival {3} hours. Cargo, storage, route, escort, funds, and active relay work are unchanged."),
			{
				Convoy->DestinationBaseName,
				FString::FromInt(Convoy->ReliefPriorityBypassedConvoyCount),
				LexToString(RecoveredHours), LexToString(ArrivalHours)
			}));
}

void AUEGTTacticalPlayerController::StandDownStrategicMutualAidConvoy(
	const FGuid ConvoyId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicMutualAidConvoyView* Convoy =
		CurrentStrategicSnapshot.MutualAidConvoys.FindByPredicate(
			[ConvoyId](const FStrategicMutualAidConvoyView& View)
			{
				return View.ConvoyId == ConvoyId;
			});
	if (Instance == nullptr || Convoy == nullptr || !Convoy->bCanStandDownRelief)
	{
		const FString Reason = Convoy != nullptr
			&& !Convoy->ReliefStandDownUnavailableReason.IsEmpty()
			? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Convoy->ReliefStandDownUnavailableReasonCode,
				Convoy->ReliefStandDownUnavailableReason)
			: UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.mutual-aid-unavailable"),
				TEXT("The selected Mutual Aid Convoy is no longer available."));
		SetStrategicStatus(Reason, true);
		return;
	}

	FStandDownMutualAidConvoyCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.ConvoyId = ConvoyId;
	PresentStrategicCommandResult(
		Instance->StandDownMutualAidConvoy(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.mutual-aid-relief-stand-down-completed-format"),
			TEXT("Relief convoy to {0} stood down; {1} × {2} returned to {3}, {4} destination storage released, and {5} later held commitments advanced. Any Signal Escort funding already spent ({6}) is not refunded; active relay work, other cargo, identities, routes, funds, and random state are unchanged."),
			{
				Convoy->DestinationBaseName,
				FString::FromInt(Convoy->Quantity),
				FUEGTLocalizationService::ContentName(
					Convoy->ItemId, Convoy->ItemDisplayName),
				Convoy->SourceBaseName,
				LexToString(Convoy->ReliefStandDownReleasedStorage),
				FString::FromInt(Convoy->ReliefStandDownAdvancedConvoyCount),
				LexToString(Convoy->ReliefStandDownSunkSignalEscortCost)
			}));
}

void AUEGTTacticalPlayerController::DivertStrategicMutualAidConvoy(
	const FGuid ConvoyId,
	const FGuid DestinationBaseId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicMutualAidConvoyView* Convoy =
		CurrentStrategicSnapshot.MutualAidConvoys.FindByPredicate(
			[ConvoyId](const FStrategicMutualAidConvoyView& View)
			{
				return View.ConvoyId == ConvoyId;
			});
	const FStrategicMutualAidDiversionOptionView* Option = Convoy != nullptr
		? Convoy->ReliefDiversionOptions.FindByPredicate(
			[DestinationBaseId](const FStrategicMutualAidDiversionOptionView& View)
			{
				return View.DestinationBaseId == DestinationBaseId;
			})
		: nullptr;
	if (Instance == nullptr || Convoy == nullptr || Option == nullptr
		|| !Option->bEnabled)
	{
		const FString Reason = Option != nullptr && !Option->UnavailableReason.IsEmpty()
			? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Option->UnavailableReasonCode, Option->UnavailableReason)
			: Convoy != nullptr && !Convoy->ReliefDiversionUnavailableReason.IsEmpty()
				? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
					Convoy->ReliefDiversionUnavailableReasonCode,
					Convoy->ReliefDiversionUnavailableReason)
				: UEGTTacticalControllerPrivate::Localized(
					TEXT("strategic.mutual-aid-unavailable"),
					TEXT("The selected Mutual Aid Convoy is no longer available."));
		SetStrategicStatus(Reason, true);
		return;
	}

	FDivertMutualAidConvoyCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.ConvoyId = ConvoyId;
	Command.DestinationBaseId = DestinationBaseId;
	const int64 ArrivalHours =
		Option->ProjectedRelayQueue.EstimatedArrivalSeconds <= 0
		? 0
		: 1 + (Option->ProjectedRelayQueue.EstimatedArrivalSeconds - 1) / 3600;
	const int64 ShiftMagnitude = Option->ArrivalShiftSeconds < 0
		? -Option->ArrivalShiftSeconds
		: Option->ArrivalShiftSeconds;
	const int64 ShiftHours = ShiftMagnitude == 0
		? 0
		: 1 + (ShiftMagnitude - 1) / 3600;
	const FString SignedShift = Option->ArrivalShiftSeconds < 0
		? FString::Printf(TEXT("-%lld"), ShiftHours)
		: Option->ArrivalShiftSeconds > 0
			? FString::Printf(TEXT("+%lld"), ShiftHours)
			: TEXT("0");
	PresentStrategicCommandResult(
		Instance->DivertMutualAidConvoy(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.mutual-aid-relief-diversion-completed-format"),
			TEXT("Relief convoy diverted from {0} to {1}; {2} × {3} and {4} storage moved, exposure {5}/100 → {6}/100, projected arrival {7} hours ({8} hours), and {9} later commitments changed. The paid Signal Escort ({10}) remains attached; source stock, cargo, identity, relay order, funds, active work, and random state are unchanged."),
			{
				Convoy->DestinationBaseName,
				Option->DestinationBaseName,
				FString::FromInt(Convoy->Quantity),
				FUEGTLocalizationService::ContentName(
					Convoy->ItemId, Convoy->ItemDisplayName),
				LexToString(Option->DivertedStorage),
				FString::FromInt(Option->CurrentRoutePressure),
				FString::FromInt(Option->ProjectedRoutePressure),
				LexToString(ArrivalHours), SignedShift,
				FString::FromInt(Option->AffectedConvoyCount),
				LexToString(Option->RetainedSignalEscortCost)
			}));
}

void AUEGTTacticalPlayerController::ConfigureStrategicMutualAidRelayWaypoint(
	const FGuid ConvoyId,
	const FGuid WaypointBaseId,
	const EMutualAidRoutePolicy OnwardRoutePolicy)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicMutualAidConvoyView* Convoy =
		CurrentStrategicSnapshot.MutualAidConvoys.FindByPredicate(
			[ConvoyId](const FStrategicMutualAidConvoyView& View)
			{
				return View.ConvoyId == ConvoyId;
			});
	const FStrategicMutualAidWaypointOptionView* Option = Convoy != nullptr
		? Convoy->RelayWaypointOptions.FindByPredicate(
			[WaypointBaseId, OnwardRoutePolicy](
				const FStrategicMutualAidWaypointOptionView& View)
			{
				return WaypointBaseId.IsValid()
					? !View.bDirectRoute
						&& View.WaypointBaseId == WaypointBaseId
						&& View.OnwardRoutePolicy == OnwardRoutePolicy
					: View.bDirectRoute;
			})
		: nullptr;
	if (Instance == nullptr || Convoy == nullptr || Option == nullptr
		|| !Option->bEnabled)
	{
		const FString Reason = Option != nullptr && !Option->UnavailableReason.IsEmpty()
			? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Option->UnavailableReasonCode, Option->UnavailableReason)
			: Convoy != nullptr && !Convoy->RelayWaypointUnavailableReason.IsEmpty()
				? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
					Convoy->RelayWaypointUnavailableReasonCode,
					Convoy->RelayWaypointUnavailableReason)
				: UEGTTacticalControllerPrivate::Localized(
					TEXT("strategic.mutual-aid-unavailable"),
					TEXT("The selected Mutual Aid Convoy is no longer available."));
		SetStrategicStatus(Reason, true);
		return;
	}

	FConfigureMutualAidRelayWaypointCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.ConvoyId = ConvoyId;
	Command.WaypointBaseId = WaypointBaseId;
	Command.OnwardRoutePolicy = OnwardRoutePolicy;
	const int64 ArrivalHours =
		Option->ProjectedRelayQueue.EstimatedArrivalSeconds <= 0
			? 0
			: 1 + (Option->ProjectedRelayQueue.EstimatedArrivalSeconds - 1) / 3600;
	const int64 ShiftMagnitude = Option->ArrivalShiftSeconds < 0
		? -Option->ArrivalShiftSeconds
		: Option->ArrivalShiftSeconds;
	const int64 ShiftHours = ShiftMagnitude == 0
		? 0
		: 1 + (ShiftMagnitude - 1) / 3600;
	const FString SignedShift = Option->ArrivalShiftSeconds < 0
		? FString::Printf(TEXT("-%lld"), ShiftHours)
		: Option->ArrivalShiftSeconds > 0
			? FString::Printf(TEXT("+%lld"), ShiftHours)
			: TEXT("0");
	const FString PlanLabel = Option->bDirectRoute
		? UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.mutual-aid-relay-waypoint-direct"),
			TEXT("DIRECT ROUTE"))
		: FString::Printf(
			TEXT("%s • %s"),
			*Option->WaypointBaseName,
			*UEGTTacticalControllerPrivate::LocalizedMutualAidRoute(
				Option->OnwardRoutePolicy));
	PresentStrategicCommandResult(
		Instance->ConfigureMutualAidRelayWaypoint(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.mutual-aid-relay-waypoint-completed-format"),
			TEXT("Route for aid to {0} set to {1}; first-leg exposure {2}/100, onward exposure {3}/100, projected arrival {4} hours ({5} hours), and {6} later commitments changed. The source relay channel remains reserved end-to-end; cargo, final storage, identity, paid escort, funds, active work, and random state are unchanged."),
			{
				Convoy->DestinationBaseName,
				PlanLabel,
				FString::FromInt(Option->FirstLegRoutePressure),
				FString::FromInt(Option->OnwardRoutePressure),
				LexToString(ArrivalHours),
				SignedShift,
				FString::FromInt(Option->AffectedConvoyCount)
			}));
}

void AUEGTTacticalPlayerController::ConfigureStrategicMutualAidBalancedHandoff(
	const FGuid ConvoyId,
	const bool bEnabled)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicMutualAidConvoyView* Convoy =
		CurrentStrategicSnapshot.MutualAidConvoys.FindByPredicate(
			[ConvoyId](const FStrategicMutualAidConvoyView& View)
			{
				return View.ConvoyId == ConvoyId;
			});
	const FStrategicMutualAidBalancedHandoffOptionView* Option = Convoy != nullptr
		? Convoy->BalancedHandoffOptions.FindByPredicate(
			[bEnabled](const FStrategicMutualAidBalancedHandoffOptionView& View)
			{
				return View.bEnabledChoice == bEnabled;
			})
		: nullptr;
	if (Instance == nullptr || Convoy == nullptr || Option == nullptr
		|| !Option->bEnabled)
	{
		const FString Reason = Option != nullptr && !Option->UnavailableReason.IsEmpty()
			? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Option->UnavailableReasonCode, Option->UnavailableReason)
			: Convoy != nullptr && !Convoy->BalancedHandoffUnavailableReason.IsEmpty()
				? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
					Convoy->BalancedHandoffUnavailableReasonCode,
					Convoy->BalancedHandoffUnavailableReason)
				: UEGTTacticalControllerPrivate::Localized(
					TEXT("strategic.mutual-aid-unavailable"),
					TEXT("The selected Mutual Aid Convoy is no longer available."));
		SetStrategicStatus(Reason, true);
		return;
	}

	FConfigureMutualAidBalancedHandoffCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.ConvoyId = ConvoyId;
	Command.bEnabled = bEnabled;
	const FString PlanLabel = UEGTTacticalControllerPrivate::Localized(
		bEnabled
			? TEXT("strategic.mutual-aid-balanced-handoff-balanced")
			: TEXT("strategic.mutual-aid-balanced-handoff-through"),
		bEnabled ? TEXT("BALANCED HANDOFF") : TEXT("THROUGH CARGO"));
	PresentStrategicCommandResult(
		Instance->ConfigureMutualAidBalancedHandoff(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.mutual-aid-balanced-handoff-completed-format"),
			TEXT("Cargo plan via {0} set to {1}; {2} × {3} and {4} storage are committed to the waypoint, while {5} continue to {6}. Relay timing, source channel, identity, paid escort, funds, active work, and random state are unchanged."),
			{
				Convoy->RelayWaypointBaseName,
				PlanLabel,
				FString::FromInt(Option->WaypointQuantity),
				FUEGTLocalizationService::ContentName(
					Convoy->ItemId, Convoy->ItemDisplayName),
				LexToString(Option->HandoffStorage),
				FString::FromInt(Option->FinalQuantity),
				Convoy->DestinationBaseName
			}));
}

void AUEGTTacticalPlayerController::AdjustStrategicSignalWatch(
	const FGuid BaseId,
	const int32 DeltaScientists)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicBaseView* Base = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[BaseId](const FStrategicBaseView& View) { return View.BaseId == BaseId; });
	const int64 Requested = Base != nullptr
		? static_cast<int64>(Base->SignalWatchScientists) + DeltaScientists
		: -1;
	if (Instance == nullptr || Base == nullptr || DeltaScientists == 0
		|| Requested < 0 || Requested > MAX_int32
		|| (DeltaScientists > 0 && !Base->bCanIncreaseSignalWatch))
	{
		const FString Reason = Base != nullptr && DeltaScientists > 0
			&& !Base->SignalWatchIncreaseUnavailableReason.IsEmpty()
			? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Base->SignalWatchIncreaseUnavailableReasonCode,
				Base->SignalWatchIncreaseUnavailableReason)
			: UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.signal-watch-unavailable"),
				TEXT("The selected Signal Watch staffing change is no longer available."));
		SetStrategicStatus(Reason, true);
		return;
	}

	FSetSignalWatchStaffCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.BaseId = BaseId;
	Command.AssignedScientists = static_cast<int32>(Requested);
	const int32 EffectiveWatchScientists = FMath::Min(
		Command.AssignedScientists, Base->FacilityRelayChannelCount);
	const int32 TotalChannels = Base->FacilityRelayChannelCount + EffectiveWatchScientists;
	PresentStrategicCommandResult(
		Instance->SetSignalWatchStaff(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.signal-watch-updated-format"),
			TEXT("Signal Watch staffing set to {0}; {1} relay channels active."),
			{ FString::FromInt(Command.AssignedScientists), FString::FromInt(TotalChannels) }));
}

void AUEGTTacticalPlayerController::AdjustStrategicWorksCadre(
	const FGuid BaseId,
	const int32 DeltaEngineers)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicBaseView* Base = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[BaseId](const FStrategicBaseView& View) { return View.BaseId == BaseId; });
	const int64 Requested = Base != nullptr
		? static_cast<int64>(Base->WorksCadreEngineers) + DeltaEngineers
		: -1;
	if (Instance == nullptr || Base == nullptr || DeltaEngineers == 0
		|| Requested < 0 || Requested > MAX_int32
		|| (DeltaEngineers > 0 && !Base->bCanIncreaseWorksCadre))
	{
		const FString Reason = Base != nullptr && DeltaEngineers > 0
			&& !Base->WorksCadreIncreaseUnavailableReason.IsEmpty()
			? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Base->WorksCadreIncreaseUnavailableReasonCode,
				Base->WorksCadreIncreaseUnavailableReason)
			: UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.works-cadre-unavailable"),
				TEXT("The selected Works Cadre staffing change is no longer available."));
		SetStrategicStatus(Reason, true);
		return;
	}

	FSetWorksCadreStaffCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.BaseId = BaseId;
	Command.AssignedEngineers = static_cast<int32>(Requested);
	const FWorksCadreCharterPolicy CharterPolicy =
		FStrategicCommandService::GetWorksCadreCharterPolicy(
			Base->WorksCadreCharter);
	const int32 ConstructionFrontloadPercent = Command.AssignedEngineers
		* CharterPolicy.ConstructionFrontloadPercentPerEngineer;
	const int32 RepairFrontloadPercent = Command.AssignedEngineers
		* CharterPolicy.RepairFrontloadPercentPerEngineer;
	PresentStrategicCommandResult(
		Instance->SetWorksCadreStaff(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.works-cadre-updated-format"),
			TEXT("Works Cadre staffing set to {0}; future construction front-loads {1}% and repairs {2}%."),
			{
				FString::FromInt(Command.AssignedEngineers),
				FString::FromInt(ConstructionFrontloadPercent),
				FString::FromInt(RepairFrontloadPercent)
			}));
}

void AUEGTTacticalPlayerController::SetStrategicWorksCadreCharter(
	const FGuid BaseId,
	const EWorksCadreCharter Charter)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicBaseView* Base = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[BaseId](const FStrategicBaseView& View) { return View.BaseId == BaseId; });
	const FStrategicWorksCadreCharterOptionView* Option = Base != nullptr
		? Base->WorksCadreCharterOptions.FindByPredicate(
			[Charter](const FStrategicWorksCadreCharterOptionView& View)
			{
				return View.Charter == Charter;
			})
		: nullptr;
	if (Instance == nullptr || Base == nullptr || Option == nullptr
		|| !Option->bEnabled)
	{
		const FString Reason = Option != nullptr
			&& !Option->UnavailableReason.IsEmpty()
			? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
				Option->UnavailableReasonCode, Option->UnavailableReason)
			: UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.works-charter-unavailable"),
				TEXT("The selected Works Charter is no longer available."));
		SetStrategicStatus(Reason, true);
		return;
	}

	FSetWorksCadreCharterCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.BaseId = BaseId;
	Command.Charter = Charter;
	PresentStrategicCommandResult(
		Instance->SetWorksCadreCharter(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.works-charter-updated-format"),
			TEXT("Works Charter set to {0}; future construction front-loads {1}% and repairs {2}%. Existing clocks are unchanged."),
			{
				UEGTTacticalControllerPrivate::LocalizedWorksCadreCharter(Charter),
				FString::FromInt(Option->ConstructionFrontloadPercent),
				FString::FromInt(Option->RepairFrontloadPercent)
			}));
}

void AUEGTTacticalPlayerController::BeginStrategicPersonnelTraining(
	const FGuid PersonnelId,
	const EPersonnelTrainingFocus Focus)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicPersonnelView* Person = CurrentStrategicSnapshot.Personnel.FindByPredicate(
		[PersonnelId](const FStrategicPersonnelView& View) { return View.PersonnelId == PersonnelId; });
	if (Instance == nullptr || Person == nullptr)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.personnel-unavailable"),
			TEXT("The selected personnel member is no longer available.")), true);
		return;
	}
	FBeginPersonnelTrainingCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.PersonnelId = PersonnelId;
	Command.Focus = Focus;
	const FString FocusLabel = Focus == EPersonnelTrainingFocus::Accuracy
		? UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.training-focus-accuracy"), TEXT("ACCURACY"))
		: Focus == EPersonnelTrainingFocus::Resolve
			? UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.training-focus-resolve"), TEXT("RESOLVE"))
			: Focus == EPersonnelTrainingFocus::Mobility
				? UEGTTacticalControllerPrivate::Localized(
					TEXT("strategic.training-focus-mobility"), TEXT("MOBILITY"))
				: UEGTTacticalControllerPrivate::Localized(
					TEXT("strategic.training-focus-strength"), TEXT("STRENGTH"));
	PresentStrategicCommandResult(
		Instance->BeginPersonnelTraining(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.training-started-format"), TEXT("{0} began {1} training."),
			{ Person->DisplayName, FocusLabel }));
}

void AUEGTTacticalPlayerController::SelectStrategicPersonnelDoctrine(
	const FGuid PersonnelId,
	const FName DoctrineId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicPersonnelView* Person = CurrentStrategicSnapshot.Personnel.FindByPredicate(
		[PersonnelId](const FStrategicPersonnelView& View) { return View.PersonnelId == PersonnelId; });
	const FStrategicPersonnelDoctrineView* Doctrine = Person != nullptr
		? Person->DoctrineOptions.FindByPredicate(
			[DoctrineId](const FStrategicPersonnelDoctrineView& View) { return View.DoctrineId == DoctrineId; })
		: nullptr;
	if (Instance == nullptr || Person == nullptr || Doctrine == nullptr)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.personnel-doctrine-unavailable"),
			TEXT("The selected field doctrine is no longer available.")), true);
		return;
	}
	FSelectPersonnelDoctrineCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.PersonnelId = PersonnelId;
	Command.DoctrineId = DoctrineId;
	PresentStrategicCommandResult(
		Instance->SelectPersonnelDoctrine(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.personnel-doctrine-selected-format"),
			TEXT("{0} selected {1}."),
			{ Person->DisplayName,
				FUEGTLocalizationService::ContentName(DoctrineId, Doctrine->DisplayName) }));
}

void AUEGTTacticalPlayerController::SelectStrategicPersonnelRecoveryPlan(
	const FGuid PersonnelId,
	const EPersonnelRecoveryPlan Plan)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicPersonnelView* Person = CurrentStrategicSnapshot.Personnel.FindByPredicate(
		[PersonnelId](const FStrategicPersonnelView& View) { return View.PersonnelId == PersonnelId; });
	const FPersonnelRecoveryPlanOptionView* Option = Person != nullptr
		? Person->RecoveryPlan.Options.FindByPredicate(
			[Plan](const FPersonnelRecoveryPlanOptionView& View) { return View.Plan == Plan; })
		: nullptr;
	if (Instance == nullptr || Person == nullptr || Option == nullptr)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.recovery-plan-unavailable"),
			TEXT("The selected Return Path is no longer available.")), true);
		return;
	}

	const FString PlanName = Plan == EPersonnelRecoveryPlan::MeasuredReturn
		? UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.recovery-plan-measured"), TEXT("MEASURED RETURN"))
		: Plan == EPersonnelRecoveryPlan::SurgeCare
			? UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.recovery-plan-surge"), TEXT("SURGE CARE"))
			: UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.recovery-plan-reflection"), TEXT("REFLECTION CYCLE"));
	FSelectPersonnelRecoveryPlanCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.PersonnelId = PersonnelId;
	Command.Plan = Plan;
	PresentStrategicCommandResult(
		Instance->SelectPersonnelRecoveryPlan(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.recovery-plan-selected-format"),
			TEXT("{0} committed to {1}."),
			{ Person->DisplayName, PlanName }));
}

void AUEGTTacticalPlayerController::BeginStrategicPersonnelStewardship(
	const FGuid PersonnelId,
	const EPersonnelStewardshipFocus Focus)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicPersonnelView* Person = CurrentStrategicSnapshot.Personnel.FindByPredicate(
		[&PersonnelId](const FStrategicPersonnelView& Candidate)
		{
			return Candidate.PersonnelId == PersonnelId;
		});
	if (Instance == nullptr || Person == nullptr)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.personnel-unavailable"),
			TEXT("The selected personnel member is no longer available.")), true);
		return;
	}
	const FString FocusName = Focus == EPersonnelStewardshipFocus::RecoveryAdvocacy
		? UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.stewardship-focus-recovery"), TEXT("RECOVERY ADVOCACY"))
		: Focus == EPersonnelStewardshipFocus::TrainingCadre
			? UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.stewardship-focus-training"), TEXT("TRAINING CADRE"))
			: UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.stewardship-focus-recruitment"), TEXT("RECRUITMENT LIAISON"));
	FBeginPersonnelStewardshipCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.PersonnelId = PersonnelId;
	Command.Focus = Focus;
	PresentStrategicCommandResult(
		Instance->BeginPersonnelStewardship(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.stewardship-started-format"),
			TEXT("{0} began a {1} Stewardship Rotation."),
			{ Person->DisplayName, FocusName }));
}

void AUEGTTacticalPlayerController::AdjustStrategicPersonnelEquipment(
	const FGuid PersonnelId,
	const FName ItemId,
	const int32 Delta)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicPersonnelView* Person = CurrentStrategicSnapshot.Personnel.FindByPredicate(
		[PersonnelId](const FStrategicPersonnelView& View) { return View.PersonnelId == PersonnelId; });
	const FStrategicBaseView* Base = Person != nullptr
		? CurrentStrategicSnapshot.Bases.FindByPredicate(
			[Person](const FStrategicBaseView& View) { return View.BaseId == Person->BaseId; })
		: nullptr;
	if (Instance == nullptr || Person == nullptr || Base == nullptr || (Delta != -1 && Delta != 1))
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.loadout-unavailable"),
			TEXT("The selected personnel loadout is no longer available.")), true);
		return;
	}
	TArray<FName> ItemIds = Person->EquippedItemIds;
	const bool bRemoving = Delta < 0;
	if (bRemoving)
	{
		if (ItemIds.RemoveSingle(ItemId) == 0)
		{
			SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.loadout-item-missing"),
				TEXT("The selected item is no longer equipped.")), true);
			return;
		}
	}
	else
	{
		ItemIds.Add(ItemId);
	}
	FString ItemName = ItemId.ToString();
	if (const FStrategicInventoryView* StoredItem = Base->Inventory.FindByPredicate(
		[ItemId](const FStrategicInventoryView& Item) { return Item.ItemId == ItemId; }))
	{
		ItemName = StoredItem->DisplayName;
	}
	else
	{
		const int32 EquippedIndex = Person->EquippedItemIds.IndexOfByKey(ItemId);
		if (Person->EquippedItemNames.IsValidIndex(EquippedIndex))
		{
			ItemName = Person->EquippedItemNames[EquippedIndex];
		}
	}
	ItemName = FUEGTLocalizationService::ContentName(ItemId, ItemName);
	const FString PersonnelName = Person->DisplayName;
	FSetPersonnelEquipmentCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.PersonnelId = PersonnelId;
	Command.ItemIds = MoveTemp(ItemIds);
	const FStrategicCommandResult Result = Instance->SetPersonnelEquipment(Command);
	PresentStrategicCommandResult(Result, bRemoving
		? UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.loadout-removed-format"),
			TEXT("{0} removed from {1}'s loadout."), { ItemName, PersonnelName })
		: UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.loadout-added-format"),
			TEXT("{0} added to {1}'s loadout."), { ItemName, PersonnelName }));
}

void AUEGTTacticalPlayerController::TransferStrategicPersonnel(
	const FGuid PersonnelId,
	const FGuid DestinationBaseId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicPersonnelView* Person = CurrentStrategicSnapshot.Personnel.FindByPredicate(
		[PersonnelId](const FStrategicPersonnelView& View) { return View.PersonnelId == PersonnelId; });
	const FStrategicBaseView* Destination = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[DestinationBaseId](const FStrategicBaseView& View) { return View.BaseId == DestinationBaseId; });
	if (Instance == nullptr || Person == nullptr || Destination == nullptr || Person->BaseId == DestinationBaseId)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.transfer-unavailable"),
			TEXT("The selected personnel transfer is no longer available.")), true);
		return;
	}
	FTransferPersonnelCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.PersonnelId = PersonnelId;
	Command.DestinationBaseId = DestinationBaseId;
	const FString PersonnelName = Person->DisplayName;
	const FString DestinationName = Destination->Name;
	const FStrategicCommandResult Result = Instance->TransferPersonnel(Command);
	PresentStrategicCommandResult(
		Result,
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.transfer-complete-format"),
			TEXT("{0} transferred to {1} with their assigned equipment."),
			{ PersonnelName, DestinationName }));
}

void AUEGTTacticalPlayerController::DismissStrategicPersonnel(const FGuid PersonnelId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicPersonnelView* Person = CurrentStrategicSnapshot.Personnel.FindByPredicate(
		[PersonnelId](const FStrategicPersonnelView& View) { return View.PersonnelId == PersonnelId; });
	if (Instance == nullptr || Person == nullptr)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.personnel-unavailable"),
			TEXT("The selected personnel member is no longer available.")), true);
		return;
	}
	const FString DisplayName = Person->DisplayName;
	FDismissPersonnelCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.PersonnelId = PersonnelId;
	PresentStrategicCommandResult(
		Instance->DismissPersonnel(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.dismissed-format"),
			TEXT("{0} dismissed; assigned equipment returned to base stores."),
			{ DisplayName }));
}

void AUEGTTacticalPlayerController::BeginPendingTacticalOperation(const FGuid OperationId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr || !CurrentStrategicSnapshot.PendingOperationIds.Contains(OperationId))
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.tactical-operation-unavailable"),
			TEXT("The tactical operation is no longer pending.")), true);
		return;
	}
	FGenerateTacticalBattleCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.OperationId = OperationId;
	PresentStrategicCommandResult(
		Instance->GenerateTacticalBattle(Command),
		UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.tactical-battlefield-generated"),
			TEXT("Tactical battlefield generated. Transferring to field command.")));
}

void AUEGTTacticalPlayerController::AutoPrepareCraft(const FGuid CraftId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	const FResolvedRuleSet& Rules = Instance->GetLoadedRules();
	int32 AcceptedCommands = 0;
	TArray<FString> Notes;
	auto FindCraft = [CraftId](const FCampaignState& State) -> const FCraftState*
	{
		return State.Craft.FindByPredicate([CraftId](const FCraftState& Craft) { return Craft.CraftId == CraftId; });
	};
	auto RecordFailure = [&Notes](const FStrategicCommandResult& Result, const FString& Fallback)
	{
		Notes.Add(Result.Diagnostics.IsEmpty()
			? Fallback
			: FUEGTLocalizationService::DiagnosticText(
				Result.Diagnostics[0].Code, Result.Diagnostics[0].Message));
	};

	FCampaignState State = Instance->GetCampaignState();
	const FCraftState* Craft = FindCraft(State);
	const FCraftRule* CraftRule = Craft != nullptr ? Rules.Craft.Find(Craft->CraftRuleId) : nullptr;
	if (Craft == nullptr || CraftRule == nullptr || Craft->Status != ECraftStatus::Grounded)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.craft-prepare-grounded-required"),
			TEXT("Automatic preparation requires a valid grounded craft.")), true);
		return;
	}

	if (!Craft->AssignedPilotId.IsValid())
	{
		const FPersonnelState* Pilot = State.Personnel.FindByPredicate(
			[&State, &Rules, Craft](const FPersonnelState& Person)
			{
				const FPersonnelRoleRule* PersonnelRule = Rules.PersonnelRoles.Find(Person.RoleId);
				const bool bAssignedElsewhere = State.Craft.ContainsByPredicate(
					[&Person, Craft](const FCraftState& Other)
					{
						return Other.CraftId != Craft->CraftId && Other.AssignedPilotId == Person.PersonnelId;
					});
				return PersonnelRule != nullptr && PersonnelRule->Category == EPersonnelRoleCategory::Pilot
					&& Person.BaseId == Craft->BaseId && Person.Status == EPersonnelStatus::Available
					&& !bAssignedElsewhere;
			});
		if (Pilot != nullptr)
		{
			FAssignCraftPilotCommand Command;
			Command.ExpectedSequence = State.CommandSequence;
			Command.CraftId = CraftId;
			Command.PersonnelId = Pilot->PersonnelId;
			const FStrategicCommandResult Result = Instance->AssignCraftPilot(Command);
			if (Result.bAccepted)
			{
				++AcceptedCommands;
				State = Instance->GetCampaignState();
				Craft = FindCraft(State);
			}
			else
			{
				RecordFailure(Result, UEGTTacticalControllerPrivate::Localized(
					TEXT("strategic.craft-prepare-pilot-failed"), TEXT("Pilot assignment failed.")));
			}
		}
		else
		{
			Notes.Add(UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.craft-prepare-no-pilot"),
				TEXT("No available unassigned pilot at the craft's base.")));
		}
	}

	Craft = FindCraft(State);
	if (Craft != nullptr && CraftRule->AgentCapacity > 0)
	{
		TArray<FGuid> AgentIds;
		for (const FGuid ExistingId : Craft->AssignedAgentIds)
		{
			const FPersonnelState* Existing = State.Personnel.FindByPredicate(
				[ExistingId](const FPersonnelState& Person) { return Person.PersonnelId == ExistingId; });
			if (Existing != nullptr && Existing->Status == EPersonnelStatus::Available)
			{
				AgentIds.Add(ExistingId);
			}
		}
		for (const FPersonnelState& Person : State.Personnel)
		{
			if (AgentIds.Num() >= CraftRule->AgentCapacity || AgentIds.Contains(Person.PersonnelId))
			{
				continue;
			}
			const FPersonnelRoleRule* PersonnelRule = Rules.PersonnelRoles.Find(Person.RoleId);
			const bool bAssignedElsewhere = State.Craft.ContainsByPredicate(
				[&Person, Craft](const FCraftState& Other)
				{
					return Other.CraftId != Craft->CraftId && Other.AssignedAgentIds.Contains(Person.PersonnelId);
				});
			if (PersonnelRule != nullptr && PersonnelRule->Category == EPersonnelRoleCategory::FieldAgent
				&& Person.BaseId == Craft->BaseId && Person.Status == EPersonnelStatus::Available
				&& !bAssignedElsewhere)
			{
				AgentIds.Add(Person.PersonnelId);
			}
		}
		if (!AgentIds.IsEmpty() && AgentIds != Craft->AssignedAgentIds)
		{
			FSetCraftAgentsCommand Command;
			Command.ExpectedSequence = State.CommandSequence;
			Command.CraftId = CraftId;
			Command.PersonnelIds = AgentIds;
			const FStrategicCommandResult Result = Instance->SetCraftAgents(Command);
			if (Result.bAccepted)
			{
				++AcceptedCommands;
				State = Instance->GetCampaignState();
				Craft = FindCraft(State);
			}
			else
			{
				RecordFailure(Result, UEGTTacticalControllerPrivate::Localized(
					TEXT("strategic.craft-prepare-team-failed"), TEXT("Field-team assignment failed.")));
			}
		}
		else if (AgentIds.IsEmpty())
		{
			Notes.Add(UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.craft-prepare-no-agents"),
				TEXT("No available unassigned field agents at the craft's base.")));
		}
	}

	Craft = FindCraft(State);
	if (Craft != nullptr && CraftRule->EquipmentSlots > 0)
	{
		const FStrategicBaseState* Base = State.Bases.FindByPredicate(
			[Craft](const FStrategicBaseState& Entry) { return Entry.BaseId == Craft->BaseId; });
		TMap<FName, int32> Available;
		if (Base != nullptr)
		{
			for (const FInventoryStack& Stack : Base->Inventory)
			{
				Available.FindOrAdd(Stack.ItemId) += Stack.Quantity;
			}
		}
		for (const FName ItemId : Craft->EquipmentItems)
		{
			++Available.FindOrAdd(ItemId);
		}
		TArray<FName> Candidates;
		for (const TPair<FName, int32>& Pair : Available)
		{
			const FItemRule* Item = Rules.Items.Find(Pair.Key);
			const bool bUnlocked = Item != nullptr && !Item->RequiredResearch.ContainsByPredicate(
				[&State](const FName Requirement) { return !State.CompletedResearch.Contains(Requirement); });
			if (Pair.Value > 0 && bUnlocked && (Item->Category == FName(TEXT("craft-weapon"))
				|| Item->Category == FName(TEXT("craft-defense"))
				|| Item->Category == FName(TEXT("craft-component"))))
			{
				Candidates.Add(Pair.Key);
			}
		}
		Candidates.Sort([&Rules](const FName Left, const FName Right)
		{
			const FItemRule* LeftRule = Rules.Items.Find(Left);
			const FItemRule* RightRule = Rules.Items.Find(Right);
			auto Priority = [](const FItemRule* Item)
			{
				return Item != nullptr && Item->Category == FName(TEXT("craft-weapon")) ? 0
					: Item != nullptr && Item->Category == FName(TEXT("craft-defense")) ? 1 : 2;
			};
			const int32 LeftPriority = Priority(LeftRule);
			const int32 RightPriority = Priority(RightRule);
			return LeftPriority == RightPriority ? Left.LexicalLess(Right) : LeftPriority < RightPriority;
		});
		TArray<FName> Loadout;
		for (const FName ItemId : Candidates)
		{
			for (int32 Count = 0; Count < Available.FindRef(ItemId) && Loadout.Num() < CraftRule->EquipmentSlots; ++Count)
			{
				Loadout.Add(ItemId);
			}
		}
		if (Loadout != Craft->EquipmentItems)
		{
			FSetCraftEquipmentCommand Command;
			Command.ExpectedSequence = State.CommandSequence;
			Command.CraftId = CraftId;
			Command.ItemIds = Loadout;
			const FStrategicCommandResult Result = Instance->SetCraftEquipment(Command);
			if (Result.bAccepted)
			{
				++AcceptedCommands;
				State = Instance->GetCampaignState();
				Craft = FindCraft(State);
			}
			else
			{
				RecordFailure(Result, UEGTTacticalControllerPrivate::Localized(
					TEXT("strategic.craft-prepare-equipment-failed"),
					TEXT("Craft equipment installation failed.")));
			}
		}
		if (Loadout.IsEmpty())
		{
			Notes.Add(UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.craft-prepare-no-equipment"),
				TEXT("No compatible craft equipment is present in base inventory.")));
		}
	}

	Craft = FindCraft(State);
	if (Craft != nullptr)
	{
		const FStrategicBaseState* Base = State.Bases.FindByPredicate(
			[Craft](const FStrategicBaseState& Entry) { return Entry.BaseId == Craft->BaseId; });
		bool bCanRearm = false;
		for (const FCraftWeaponState& WeaponState : Craft->WeaponStates)
		{
			const FItemRule* Weapon = Rules.Items.Find(WeaponState.WeaponItemId);
			int32 Mounts = 0;
			for (const FName ItemId : Craft->EquipmentItems)
			{
				Mounts += ItemId == WeaponState.WeaponItemId ? 1 : 0;
			}
			const int32 Maximum = Weapon != nullptr ? Weapon->MagazineCapacity * Mounts : 0;
			const FInventoryStack* Ammunition = Base != nullptr && Weapon != nullptr
				? Base->Inventory.FindByPredicate(
					[Weapon](const FInventoryStack& Stack) { return Stack.ItemId == Weapon->AmmunitionItemId; })
				: nullptr;
			bCanRearm |= WeaponState.Ammunition < Maximum && Ammunition != nullptr && Ammunition->Quantity > 0;
		}
		if (bCanRearm)
		{
			FRearmCraftCommand Command;
			Command.ExpectedSequence = State.CommandSequence;
			Command.CraftId = CraftId;
			Command.Policy = ECraftRearmPolicy::LoadAvailable;
			const FStrategicCommandResult Result = Instance->RearmCraft(Command);
			if (Result.bAccepted)
			{
				++AcceptedCommands;
				State = Instance->GetCampaignState();
				Craft = FindCraft(State);
			}
			else
			{
				RecordFailure(Result, UEGTTacticalControllerPrivate::Localized(
					TEXT("strategic.craft-prepare-rearm-failed"), TEXT("Craft rearming failed.")));
			}
		}
	}

	Craft = FindCraft(State);
	if (Craft != nullptr && (Craft->CurrentHull < CraftRule->MaxHull || Craft->CurrentFuel < CraftRule->FuelCapacity))
	{
		FBeginCraftServiceCommand Command;
		Command.ExpectedSequence = State.CommandSequence;
		Command.CraftId = CraftId;
		const FStrategicCommandResult Result = Instance->BeginCraftService(Command);
		if (Result.bAccepted)
		{
			++AcceptedCommands;
		}
		else
		{
			RecordFailure(Result, UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.craft-prepare-service-failed"), TEXT("Craft service could not begin.")));
		}
	}

	FString Message = AcceptedCommands > 0
		? UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.craft-prepare-updated-format"),
			TEXT("Validated craft-preparation updates: {0}."),
			{ FString::FromInt(AcceptedCommands) })
		: UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.craft-prepare-current"),
			TEXT("Craft configuration was already current or no preparation step was possible."));
	if (!Notes.IsEmpty())
	{
		Message += TEXT("  ") + FString::Join(Notes, TEXT("  "));
	}
	SetStrategicStatus(Message, AcceptedCommands == 0 && !Notes.IsEmpty());
	RefreshTacticalPresentation();
}

void AUEGTTacticalPlayerController::CancelStrategicCraftService(const FGuid CraftId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicCraftView* Craft = CurrentStrategicSnapshot.Craft.FindByPredicate(
		[CraftId](const FStrategicCraftView& View) { return View.CraftId == CraftId; });
	if (Instance == nullptr || Craft == nullptr || !Craft->bCanCancelService)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.craft-service-cancel-unavailable"),
			TEXT("The selected craft has no active service to cancel.")), true);
		return;
	}
	const FString CraftName = Craft->DisplayName;
	const int64 Refund = Craft->ServiceCancellationRefund;
	FCancelCraftServiceCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.CraftId = CraftId;
	PresentStrategicCommandResult(
		Instance->CancelCraftService(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.craft-service-cancelled-format"),
			TEXT("{0} service cancelled • {1} returned."),
			{ CraftName, LexToString(Refund) }));
}

void AUEGTTacticalPlayerController::RearmStrategicCraft(
	const FGuid CraftId,
	const ECraftRearmPolicy Policy)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicCraftView* Craft = CurrentStrategicSnapshot.Craft.FindByPredicate(
		[CraftId](const FStrategicCraftView& View) { return View.CraftId == CraftId; });
	const bool bPolicyValid = Policy == ECraftRearmPolicy::FullLoad
		|| Policy == ECraftRearmPolicy::LoadAvailable;
	const bool bAvailable = Craft != nullptr && bPolicyValid
		&& (Policy == ECraftRearmPolicy::FullLoad
			? Craft->bCanRearmFully
			: Craft->bCanLoadAvailableAmmunition);
	if (Instance == nullptr || !bAvailable)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.craft-rearm-unavailable"),
			TEXT("The selected craft rearm action is no longer available.")), true);
		return;
	}

	FRearmCraftCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.CraftId = CraftId;
	Command.Policy = Policy;
	const int64 Loaded = Policy == ECraftRearmPolicy::FullLoad
		? Craft->TotalAmmunitionMissing
		: Craft->TotalAmmunitionLoadable;
	const int64 Remaining = FMath::Max<int64>(0, Craft->TotalAmmunitionMissing - Loaded);
	const FString Message = Policy == ECraftRearmPolicy::FullLoad
		? UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.craft-rearmed-full-format"),
			TEXT("Loaded {0} rounds aboard {1}; every weapon is ready."),
			{ LexToString(Loaded), Craft->DisplayName })
		: UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.craft-rearmed-available-format"),
			TEXT("Loaded {0} available rounds aboard {1}; {2} rounds remain unfilled."),
			{ LexToString(Loaded), Craft->DisplayName, LexToString(Remaining) });
	PresentStrategicCommandResult(Instance->RearmCraft(Command), Message);
}

void AUEGTTacticalPlayerController::TransferStrategicCraft(
	const FGuid CraftId,
	const FGuid DestinationBaseId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicCraftView* Craft = CurrentStrategicSnapshot.Craft.FindByPredicate(
		[CraftId](const FStrategicCraftView& View) { return View.CraftId == CraftId; });
	const FStrategicBaseView* Destination = CurrentStrategicSnapshot.Bases.FindByPredicate(
		[DestinationBaseId](const FStrategicBaseView& View) { return View.BaseId == DestinationBaseId; });
	if (Instance == nullptr || Craft == nullptr || Destination == nullptr || Craft->BaseId == DestinationBaseId)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.craft-transfer-unavailable"),
			TEXT("The selected craft transfer is no longer available.")), true);
		return;
	}
	const FString CraftName = Craft->DisplayName;
	const FString DestinationName = Destination->Name;
	FTransferCraftCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.CraftId = CraftId;
	Command.DestinationBaseId = DestinationBaseId;
	PresentStrategicCommandResult(
		Instance->TransferCraft(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.craft-transferred-format"),
			TEXT("{0} rebased to {1} with equipment and cargo aboard."),
			{ CraftName, DestinationName }));
}

void AUEGTTacticalPlayerController::ResolveStrategicCraftSalvage(
	const FGuid CraftId,
	const FName ItemId,
	const int32 Quantity,
	const ECraftSalvageDisposition Disposition)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicCraftView* Craft = CurrentStrategicSnapshot.Craft.FindByPredicate(
		[CraftId](const FStrategicCraftView& View) { return View.CraftId == CraftId; });
	const FStrategicCraftSalvageView* Salvage = Craft != nullptr
		? Craft->PendingSalvage.FindByPredicate(
			[ItemId](const FStrategicCraftSalvageView& View) { return View.ItemId == ItemId; })
		: nullptr;
	if (Instance == nullptr || Craft == nullptr || Salvage == nullptr
		|| !Craft->bSalvageDispositionAvailable || Quantity <= 0 || Quantity > Salvage->Quantity)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.salvage-unavailable"),
			TEXT("The selected salvage disposition is no longer available.")), true);
		return;
	}

	FResolveCraftSalvageCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.CraftId = CraftId;
	Command.ItemId = ItemId;
	Command.Quantity = Quantity;
	Command.Disposition = Disposition;
	const FString ItemName = FUEGTLocalizationService::ContentName(Salvage->ItemId, Salvage->DisplayName);
	const FString Message = Disposition == ECraftSalvageDisposition::RetainAtBase
		? UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.salvage-retained-format"),
			TEXT("Retained {0} × {1} in base inventory."),
			{ FString::FromInt(Quantity), ItemName })
		: UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.salvage-sold-format"),
			TEXT("Sold {0} × {1} directly from recovery for {2}."),
			{
				FString::FromInt(Quantity),
				ItemName,
				LexToString(static_cast<int64>(Quantity) * Salvage->UnitSellValue)
			});
	PresentStrategicCommandResult(Instance->ResolveCraftSalvage(Command), Message);
}

void AUEGTTacticalPlayerController::AssignStrategicCraftPilot(
	const FGuid CraftId,
	const FGuid PersonnelId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicCraftView* Craft = CurrentStrategicSnapshot.Craft.FindByPredicate(
		[CraftId](const FStrategicCraftView& View) { return View.CraftId == CraftId; });
	const FStrategicPersonnelView* Pilot = PersonnelId.IsValid()
		? CurrentStrategicSnapshot.Personnel.FindByPredicate(
			[PersonnelId](const FStrategicPersonnelView& View) { return View.PersonnelId == PersonnelId; })
		: nullptr;
	if (Instance == nullptr || Craft == nullptr || (PersonnelId.IsValid() && Pilot == nullptr))
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.craft-pilot-unavailable"),
			TEXT("The selected pilot assignment is no longer available.")), true);
		return;
	}
	const FString CraftName = Craft->DisplayName;
	const FString PilotName = Pilot != nullptr ? Pilot->DisplayName : FString();
	FAssignCraftPilotCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.CraftId = CraftId;
	Command.PersonnelId = PersonnelId;
	const FStrategicCommandResult Result = Instance->AssignCraftPilot(Command);
	PresentStrategicCommandResult(Result, PersonnelId.IsValid()
		? UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.craft-pilot-assigned-format"),
			TEXT("{0} assigned as pilot of {1}."),
			{ PilotName, CraftName })
		: UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.craft-pilot-cleared-format"),
			TEXT("Pilot assignment cleared from {0}."),
			{ CraftName }));
}

void AUEGTTacticalPlayerController::ToggleStrategicCraftAgent(
	const FGuid CraftId,
	const FGuid PersonnelId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	const FStrategicCraftView* Craft = CurrentStrategicSnapshot.Craft.FindByPredicate(
		[CraftId](const FStrategicCraftView& View) { return View.CraftId == CraftId; });
	const FStrategicPersonnelView* Person = CurrentStrategicSnapshot.Personnel.FindByPredicate(
		[PersonnelId](const FStrategicPersonnelView& View) { return View.PersonnelId == PersonnelId; });
	if (Instance == nullptr || Craft == nullptr || Person == nullptr)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.craft-agent-unavailable"),
			TEXT("The selected field-team assignment is no longer available.")), true);
		return;
	}
	TArray<FGuid> PersonnelIds = Craft->AssignedAgentIds;
	const bool bRemoving = PersonnelIds.Remove(PersonnelId) > 0;
	if (!bRemoving)
	{
		PersonnelIds.Add(PersonnelId);
	}
	const FString CraftName = Craft->DisplayName;
	const FString PersonnelName = Person->DisplayName;
	FSetCraftAgentsCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.CraftId = CraftId;
	Command.PersonnelIds = MoveTemp(PersonnelIds);
	const FStrategicCommandResult Result = Instance->SetCraftAgents(Command);
	PresentStrategicCommandResult(Result, UEGTTacticalControllerPrivate::LocalizedFormat(
		bRemoving
			? TEXT("strategic.craft-agent-removed-format")
			: TEXT("strategic.craft-agent-assigned-format"),
		bRemoving
			? TEXT("{0} removed from {1} field team.")
			: TEXT("{0} assigned to {1} field team."),
		{ PersonnelName, CraftName }));
}

void AUEGTTacticalPlayerController::AutoEquipFieldTeam()
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	const FResolvedRuleSet& Rules = Instance->GetLoadedRules();
	int32 EquippedAgents = 0;
	int32 SkippedAgents = 0;
	TArray<FGuid> AgentIds;
	for (const FPersonnelState& Person : Instance->GetCampaignState().Personnel)
	{
		const FPersonnelRoleRule* PersonnelRule = Rules.PersonnelRoles.Find(Person.RoleId);
		if (PersonnelRule != nullptr && PersonnelRule->Category == EPersonnelRoleCategory::FieldAgent
			&& Person.Status == EPersonnelStatus::Available)
		{
			AgentIds.Add(Person.PersonnelId);
		}
	}
	for (const FGuid AgentId : AgentIds)
	{
		const FCampaignState State = Instance->GetCampaignState();
		const FPersonnelState* Person = State.Personnel.FindByPredicate(
			[AgentId](const FPersonnelState& Entry) { return Entry.PersonnelId == AgentId; });
		if (Person == nullptr || Person->Status != EPersonnelStatus::Available)
		{
			++SkippedAgents;
			continue;
		}
		const FStrategicBaseState* Base = State.Bases.FindByPredicate(
			[Person](const FStrategicBaseState& Entry) { return Entry.BaseId == Person->BaseId; });
		TMap<FName, int32> Available;
		if (Base != nullptr)
		{
			for (const FInventoryStack& Stack : Base->Inventory)
			{
				Available.FindOrAdd(Stack.ItemId) += Stack.Quantity;
			}
		}
		for (const FName ItemId : Person->EquippedItems)
		{
			++Available.FindOrAdd(ItemId);
		}
		auto IsUnlocked = [&State](const FItemRule& Item)
		{
			return !Item.RequiredResearch.ContainsByPredicate(
				[&State](const FName Requirement) { return !State.CompletedResearch.Contains(Requirement); });
		};
		TArray<FName> WeaponIds;
		for (const TPair<FName, int32>& Pair : Available)
		{
			const FItemRule* Item = Rules.Items.Find(Pair.Key);
			if (Pair.Value > 0 && Item != nullptr && Item->IsTacticalWeapon() && IsUnlocked(*Item))
			{
				WeaponIds.Add(Pair.Key);
			}
		}
		WeaponIds.Sort(FNameLexicalLess());
		if (WeaponIds.IsEmpty())
		{
			++SkippedAgents;
			continue;
		}
		TArray<FName> Loadout;
		const FName WeaponId = WeaponIds[0];
		Loadout.Add(WeaponId);
		const FItemRule& Weapon = Rules.Items.FindChecked(WeaponId);
		if (!Weapon.TacticalAmmunitionItemId.IsNone())
		{
			const int32 Magazines = FMath::Min(4, Available.FindRef(Weapon.TacticalAmmunitionItemId));
			for (int32 Index = 0; Index < Magazines; ++Index)
			{
				Loadout.Add(Weapon.TacticalAmmunitionItemId);
			}
		}
		for (const FName Category : {
			FName(TEXT("armor")), FName(TEXT("device")), FName(TEXT("medical")), FName(TEXT("sensor")) })
		{
			TArray<FName> CategoryItems;
			for (const TPair<FName, int32>& Pair : Available)
			{
				const FItemRule* Item = Rules.Items.Find(Pair.Key);
				if (Pair.Value > 0 && Item != nullptr && Item->Category == Category && IsUnlocked(*Item))
				{
					CategoryItems.Add(Pair.Key);
				}
			}
			CategoryItems.Sort(FNameLexicalLess());
			if (!CategoryItems.IsEmpty() && Loadout.Num() < 16)
			{
				Loadout.Add(CategoryItems[0]);
			}
		}
		if (Loadout == Person->EquippedItems)
		{
			continue;
		}
		FSetPersonnelEquipmentCommand Command;
		Command.ExpectedSequence = State.CommandSequence;
		Command.PersonnelId = AgentId;
		Command.ItemIds = Loadout;
		const FStrategicCommandResult Result = Instance->SetPersonnelEquipment(Command);
		if (Result.bAccepted)
		{
			++EquippedAgents;
		}
		else
		{
			++SkippedAgents;
		}
	}
	SetStrategicStatus(
		AgentIds.IsEmpty()
			? UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.auto-equip-no-agents"),
				TEXT("No available field agents are on station."))
			: UEGTTacticalControllerPrivate::LocalizedFormat(
				TEXT("strategic.auto-equip-result-format"),
				TEXT("Field agents auto-equipped: {0}; without a complete compatible loadout: {1}."),
				{ FString::FromInt(EquippedAgents), FString::FromInt(SkippedAgents) }),
		AgentIds.IsEmpty() || (EquippedAgents == 0 && SkippedAgents > 0));
	RefreshTacticalPresentation();
}

void AUEGTTacticalPlayerController::DispatchReadyCraftToContact(const FGuid ContactId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	const FCampaignState State = Instance->GetCampaignState();
	const FResolvedRuleSet& Rules = Instance->GetLoadedRules();
	const auto IsReadyInterceptor = [&Rules](const FCraftState& Craft)
	{
		if (Craft.Status != ECraftStatus::Grounded || !Craft.AssignedPilotId.IsValid())
		{
			return false;
		}
		return Craft.WeaponStates.ContainsByPredicate(
			[&Rules](const FCraftWeaponState& WeaponState)
			{
				const FItemRule* Weapon = Rules.Items.Find(WeaponState.WeaponItemId);
				return Weapon != nullptr && Weapon->IsCraftWeapon() && WeaponState.Ammunition > 0;
			});
	};
	const FCraftState* Candidate = State.Craft.FindByPredicate(
		[&Rules, &IsReadyInterceptor](const FCraftState& Craft)
		{
			const FCraftRule* Rule = Rules.Craft.Find(Craft.CraftRuleId);
			return Rule != nullptr && Rule->AgentCapacity == 0 && IsReadyInterceptor(Craft);
		});
	if (Candidate == nullptr)
	{
		Candidate = State.Craft.FindByPredicate(IsReadyInterceptor);
	}
	if (Candidate == nullptr)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.contact-no-ready-interceptor"),
			TEXT("No grounded, piloted, armed craft is ready. Recruit a pilot, manufacture a craft weapon and ammunition, then auto-prepare the craft.")), true);
		return;
	}
	FDispatchCraftCommand Command;
	Command.ExpectedSequence = State.CommandSequence;
	Command.CraftId = Candidate->CraftId;
	Command.ContactId = ContactId;
	PresentStrategicCommandResult(
		Instance->DispatchCraft(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.contact-craft-dispatched-format"),
			TEXT("{0} dispatched to the detected contact."),
			{ Candidate->DisplayName }));
}

void AUEGTTacticalPlayerController::ResolveContactInterception(
	const FGuid ContactId,
	const EInterceptionPosture Posture)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	FResolveInterceptionRoundCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.ContactId = ContactId;
	Command.Posture = Posture;
	const FStrategicCommandResult Result = Instance->ResolveInterceptionRound(Command);
	const FStrategicEvent* Aftershock = Result.Events.FindByPredicate(
		[](const FStrategicEvent& Event)
		{
			return Event.Type == EStrategicEventType::InterceptionAftershockApplied;
		});
	if (Aftershock != nullptr)
	{
		PresentStrategicCommandResult(Result, UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.contact-interception-aftershock-format"),
			TEXT("Interception round resolved • {0} • next adversary wave delayed by {1}."),
			{
				UEGTTacticalControllerPrivate::LocalizedInterceptionPosture(Posture),
				UEGTTacticalControllerPrivate::CompactMinutesSeconds(
					Aftershock->AdversaryMissionDelaySeconds)
			}));
		return;
	}
	PresentStrategicCommandResult(Result, UEGTTacticalControllerPrivate::LocalizedFormat(
		TEXT("strategic.contact-interception-posture-resolved-format"),
		TEXT("Interception round resolved • {0} • events: {1}."),
		{
			UEGTTacticalControllerPrivate::LocalizedInterceptionPosture(Posture),
			FString::FromInt(Result.Events.Num())
		}));
}

void AUEGTTacticalPlayerController::WithdrawContactInterception(const FGuid ContactId)
{
	WithdrawContactInterceptionWithDoctrine(
		ContactId, EInterceptionWithdrawalDoctrine::FormationBreak);
}

void AUEGTTacticalPlayerController::WithdrawContactInterceptionWithDoctrine(
	const FGuid ContactId,
	const EInterceptionWithdrawalDoctrine Doctrine)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	int32 ReturningCraft = 0;
	int32 RemainingCraft = 0;
	int64 ContactRouteDelaySeconds = 0;
	FString PriorityCraftName;
	if (const FStrategicContactView* Contact = CurrentStrategicSnapshot.Contacts.FindByPredicate(
		[ContactId](const FStrategicContactView& Candidate)
		{
			return Candidate.ContactId == ContactId;
		}))
	{
		if (const FStrategicInterceptionWithdrawalView* Option =
			Contact->InterceptionWithdrawals.FindByPredicate(
				[Doctrine](const FStrategicInterceptionWithdrawalView& Candidate)
				{
					return Candidate.Doctrine == Doctrine;
				}))
		{
			ReturningCraft = Option->WithdrawingCraftCount;
			RemainingCraft = Option->RemainingCraftCount;
			ContactRouteDelaySeconds = Option->ContactRouteDelaySeconds;
			PriorityCraftName = Option->PriorityCraftDisplayName;
		}
		else
		{
			ReturningCraft = Contact->InterceptionCraftCount;
		}
	}
	FWithdrawInterceptionCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.ContactId = ContactId;
	Command.Doctrine = Doctrine;
	const FStrategicCommandResult Result = Instance->WithdrawInterception(Command);
	if (const FStrategicEvent* Event = Result.Events.FindByPredicate(
		[](const FStrategicEvent& Candidate)
		{
			return Candidate.Type == EStrategicEventType::InterceptionWithdrawn;
		}))
	{
		ReturningCraft = Event->Quantity;
		RemainingCraft = static_cast<int32>(FMath::Clamp<int64>(Event->Amount, 0, MAX_int32));
		ContactRouteDelaySeconds = Event->ContactRouteDelaySeconds;
		if (PriorityCraftName.IsEmpty() && Event->CraftId.IsValid())
		{
			if (const FStrategicCraftView* Craft = CurrentStrategicSnapshot.Craft.FindByPredicate(
				[Event](const FStrategicCraftView& Candidate)
				{
					return Candidate.CraftId == Event->CraftId;
				}))
			{
				PriorityCraftName = Craft->DisplayName;
			}
		}
	}
	FString SuccessMessage;
	if (Doctrine == EInterceptionWithdrawalDoctrine::EvasiveRelay)
	{
		SuccessMessage = UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.contact-interception-relay-withdrawn-format"),
			TEXT("Evasive relay ordered • {0} returning • {1} craft remain engaged."),
			{
				PriorityCraftName.IsEmpty() ? TEXT("Priority craft") : PriorityCraftName,
				FString::FromInt(RemainingCraft)
			});
	}
	else if (Doctrine == EInterceptionWithdrawalDoctrine::WakeSnare)
	{
		SuccessMessage = UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.contact-interception-wake-snare-withdrawn-format"),
			TEXT("Wake Snare deployed • contact route delayed {0} • returning craft: {1}."),
			{
				UEGTTacticalControllerPrivate::CompactMinutesSeconds(ContactRouteDelaySeconds),
				FString::FromInt(ReturningCraft)
			});
	}
	else
	{
		SuccessMessage = UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.contact-interception-withdrawn-format"),
			TEXT("Formation withdrawal ordered • returning craft: {0}."),
			{ FString::FromInt(ReturningCraft) });
	}
	PresentStrategicCommandResult(Result, SuccessMessage);
}

void AUEGTTacticalPlayerController::ResolveBaseAssault(
	const FGuid AssaultId,
	const EBaseDefenseFireDoctrine FireDoctrine)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	FResolveBaseAssaultCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.AssaultId = AssaultId;
	Command.FireDoctrine = FireDoctrine;
	const FStrategicCommandResult Result = Instance->ResolveBaseAssault(Command);
	const bool bRepelled = Result.HasEvent(EStrategicEventType::BaseAssaultRepelled);
	const bool bBreached = Result.HasEvent(EStrategicEventType::BaseAssaultBreached);
	const FString Summary = bRepelled
		? UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.base-defense-repelled"), TEXT("Base defenses repelled the assault."))
		: (bBreached
			? UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.base-defense-breached"),
				TEXT("Base perimeter breached; facilities sustained damage."))
			: UEGTTacticalControllerPrivate::Localized(
				TEXT("strategic.base-defense-resolved"), TEXT("Base defense resolved.")));
	PresentStrategicCommandResult(Result, Summary);
}

void AUEGTTacticalPlayerController::DeployBaseDefense(const FGuid AssaultId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	FDeployBaseDefenseOperationCommand Command;
	Command.ExpectedSequence = CurrentStrategicSnapshot.ExpectedCommandSequence;
	Command.AssaultId = AssaultId;
	const FStrategicCommandResult Result = Instance->DeployBaseDefenseOperation(Command);
	FGuid OperationId;
	if (const FStrategicEvent* Ready = Result.Events.FindByPredicate(
		[](const FStrategicEvent& Event)
		{
			return Event.Type == EStrategicEventType::BaseDefenseTacticalOperationReady;
		}))
	{
		OperationId = Ready->OperationId;
	}
	PresentStrategicCommandResult(Result, UEGTTacticalControllerPrivate::Localized(
		TEXT("strategic.base-defense-ground-committed"),
		TEXT("Ground defenders committed. Transferring to tactical command.")));
	if (Result.bAccepted && OperationId.IsValid())
	{
		BeginPendingTacticalOperation(OperationId);
	}
}

void AUEGTTacticalPlayerController::DeployReadyCraftToSite(const FGuid SiteId)
{
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	const FCampaignState State = Instance->GetCampaignState();
	const FResolvedRuleSet& Rules = Instance->GetLoadedRules();
	const FCraftState* Candidate = State.Craft.FindByPredicate(
		[&State, &Rules](const FCraftState& Craft)
		{
			const FCraftRule* Rule = Rules.Craft.Find(Craft.CraftRuleId);
			if (Rule == nullptr || Rule->AgentCapacity <= 0 || Craft.Status != ECraftStatus::Grounded
				|| !Craft.AssignedPilotId.IsValid() || Craft.AssignedAgentIds.IsEmpty())
			{
				return false;
			}
			return Craft.AssignedAgentIds.ContainsByPredicate(
				[&State, &Rules](const FGuid AgentId)
				{
					const FPersonnelState* Agent = State.Personnel.FindByPredicate(
						[AgentId](const FPersonnelState& Person) { return Person.PersonnelId == AgentId; });
					return Agent != nullptr && Agent->Status == EPersonnelStatus::Available
						&& Agent->EquippedItems.ContainsByPredicate(
							[&Rules](const FName ItemId)
							{
								const FItemRule* Item = Rules.Items.Find(ItemId);
								return Item != nullptr && Item->IsTacticalWeapon();
							});
				});
		});
	if (Candidate == nullptr)
	{
		SetStrategicStatus(UEGTTacticalControllerPrivate::Localized(
			TEXT("strategic.site-no-ready-transport"),
			TEXT("No grounded transport has a pilot and armed field team. Auto-equip agents and auto-prepare a transport first.")), true);
		return;
	}
	FDeployCraftToSiteCommand Command;
	Command.ExpectedSequence = State.CommandSequence;
	Command.CraftId = Candidate->CraftId;
	Command.SiteId = SiteId;
	PresentStrategicCommandResult(
		Instance->DeployCraftToSite(Command),
		UEGTTacticalControllerPrivate::LocalizedFormat(
			TEXT("strategic.site-craft-deployed-format"),
			TEXT("{0} deployed to the tactical site."),
			{ Candidate->DisplayName }));
}

void AUEGTTacticalPlayerController::ReturnToStrategicCommand()
{
	bViewingDebrief = false;
	RefreshTacticalPresentation();
}

void AUEGTTacticalPlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AUEGTTacticalPlayerController::ExecuteHudAction(const ETacticalHudActionType ActionType)
{
	if (bStrategicMode)
	{
		return;
	}
	const FTacticalHudActionAvailability* Action = CurrentSnapshot.FindAction(ActionType);
	if (Action == nullptr || !Action->bAvailable)
	{
		if (HudWidget != nullptr)
		{
			const FString Fallback = UEGTTacticalControllerPrivate::Localized(
				TEXT("tactical.hud-action-missing"),
				TEXT("The requested action is not part of this HUD snapshot."));
			HudWidget->ShowStatusMessage(
				Action != nullptr
					? UEGTTacticalControllerPrivate::LocalizedDiagnostic(
						Action->UnavailableReasonCode,
						Action->UnavailableReason.IsEmpty() ? Fallback : Action->UnavailableReason)
					: Fallback,
				true);
		}
		if (AudioDirector != nullptr)
		{
			AudioDirector->PlayCue(EUEGTAudioCue::CommandRejected);
		}
		return;
	}
	if (ActionType == ETacticalHudActionType::EndTurn)
	{
		if (DeferEndTurnForConfirmation())
		{
			return;
		}
	}
	else
	{
		EndTurnConfirmation.Reset();
	}
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}

	FStrategicCommandResult Result;
	switch (ActionType)
	{
	case ETacticalHudActionType::ConfirmDeployment:
	{
		FConfirmTacticalDeploymentCommand Command;
		Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
		Command.BattleId = CurrentSnapshot.BattleId;
		Result = Instance->ConfirmTacticalDeployment(Command);
		break;
	}
	case ETacticalHudActionType::Move:
	{
		FMoveTacticalUnitCommand Command;
		Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
		Command.BattleId = CurrentSnapshot.BattleId;
		Command.UnitId = Action->UnitId;
		Command.DestinationX = Action->TargetX;
		Command.DestinationY = Action->TargetY;
		Command.DestinationZ = Action->TargetZ;
		Result = Instance->MoveTacticalUnit(Command);
		break;
	}
	case ETacticalHudActionType::AttackUnit:
	{
		FAttackTacticalUnitCommand Command;
		Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
		Command.BattleId = CurrentSnapshot.BattleId;
		Command.AttackerUnitId = Action->UnitId;
		Command.TargetUnitId = Action->TargetUnitId;
		Command.WeaponItemId = Action->ItemId;
		Command.FireMode = Action->FireMode;
		Result = Instance->AttackTacticalUnit(Command);
		break;
	}
	case ETacticalHudActionType::ProjectSignal:
	{
		FProjectTacticalSignalCommand Command;
		Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
		Command.BattleId = CurrentSnapshot.BattleId;
		Command.AttackerUnitId = Action->UnitId;
		Command.TargetUnitId = Action->TargetUnitId;
		Command.ProjectorItemId = Action->ItemId;
		Result = Instance->ProjectTacticalSignal(Command);
		break;
	}
	case ETacticalHudActionType::AttackTerrain:
	{
		FAttackTacticalTerrainCommand Command;
		Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
		Command.BattleId = CurrentSnapshot.BattleId;
		Command.AttackerUnitId = Action->UnitId;
		Command.TargetX = Action->TargetX;
		Command.TargetY = Action->TargetY;
		Command.TargetZ = Action->TargetZ;
		Command.WeaponItemId = Action->ItemId;
		Command.FireMode = Action->FireMode;
		Result = Instance->AttackTacticalTerrain(Command);
		break;
	}
	case ETacticalHudActionType::Reload:
	{
		FReloadTacticalWeaponCommand Command;
		Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
		Command.BattleId = CurrentSnapshot.BattleId;
		Command.UnitId = Action->UnitId;
		Command.WeaponItemId = Action->ItemId;
		Result = Instance->ReloadTacticalWeapon(Command);
		break;
	}
	case ETacticalHudActionType::EjectMagazine:
	{
		FEjectTacticalMagazineCommand Command;
		Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
		Command.BattleId = CurrentSnapshot.BattleId;
		Command.UnitId = Action->UnitId;
		Command.WeaponItemId = Action->ItemId;
		Result = Instance->EjectTacticalMagazine(Command);
		break;
	}
	case ETacticalHudActionType::ChangeStance:
	{
		FChangeTacticalStanceCommand Command;
		Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
		Command.BattleId = CurrentSnapshot.BattleId;
		Command.UnitId = Action->UnitId;
		Command.Stance = Action->RequestedStance;
		Result = Instance->ChangeTacticalStance(Command);
		break;
	}
	case ETacticalHudActionType::OperateDoor:
	{
		FSetTacticalDoorCommand Command;
		Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
		Command.BattleId = CurrentSnapshot.BattleId;
		Command.UnitId = Action->UnitId;
		Command.TargetX = Action->TargetX;
		Command.TargetY = Action->TargetY;
		Command.TargetZ = Action->TargetZ;
		Command.bOpen = Action->bRequestedDoorOpen;
		Result = Instance->SetTacticalDoor(Command);
		break;
	}
	case ETacticalHudActionType::DeployDevice:
	{
		FDeployTacticalDeviceCommand Command;
		Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
		Command.BattleId = CurrentSnapshot.BattleId;
		Command.UnitId = Action->UnitId;
		Command.DeviceItemId = Action->ItemId;
		Command.TargetX = Action->TargetX;
		Command.TargetY = Action->TargetY;
		Command.TargetZ = Action->TargetZ;
		Result = Instance->DeployTacticalDevice(Command);
		break;
	}
	case ETacticalHudActionType::InteractObjective:
	{
		FInteractTacticalObjectiveCommand Command;
		Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
		Command.BattleId = CurrentSnapshot.BattleId;
		Command.UnitId = Action->UnitId;
		Command.ObjectiveId = Action->ObjectiveId;
		Result = Instance->InteractTacticalObjective(Command);
		break;
	}
	case ETacticalHudActionType::Extract:
	{
		FExtractTacticalUnitCommand Command;
		Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
		Command.BattleId = CurrentSnapshot.BattleId;
		Command.UnitId = Action->UnitId;
		Result = Instance->ExtractTacticalUnit(Command);
		break;
	}
	case ETacticalHudActionType::EndTurn:
	{
		FEndTacticalTurnCommand Command;
		Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
		Command.BattleId = CurrentSnapshot.BattleId;
		Result = Instance->EndTacticalTurn(Command);
		break;
	}
	default:
		return;
	}
	FString AcceptedFeedback = UEGTTacticalControllerPrivate::LocalizedFormat(
		Result.Events.Num() == 1
			? TEXT("tactical.command-accepted-one-format")
			: TEXT("tactical.command-accepted-many-format"),
		Result.Events.Num() == 1
			? TEXT("Command accepted • {0} event")
			: TEXT("Command accepted • {0} events"),
		{ FString::FromInt(Result.Events.Num()) });
	if (Result.bAccepted && ActionType == ETacticalHudActionType::EjectMagazine)
	{
		const FStrategicEvent* Event = Result.Events.FindByPredicate(
			[](const FStrategicEvent& Entry)
			{
				return Entry.Type == EStrategicEventType::TacticalMagazineEjected;
			});
		if (Event != nullptr)
		{
			AcceptedFeedback = UEGTTacticalControllerPrivate::LocalizedFormat(
				TEXT("tactical.magazine-ejected-format"),
				TEXT("Ejected {0} with {1} rounds retained."),
				{
					FUEGTLocalizationService::ContentName(Event->RuleId, Event->RuleId.ToString()),
					FString::FromInt(Event->Quantity)
				});
		}
	}
	else if (Result.bAccepted && ActionType == ETacticalHudActionType::ProjectSignal)
	{
		const FStrategicEvent* Event = Result.Events.FindByPredicate(
			[](const FStrategicEvent& Entry)
			{
				return Entry.Type == EStrategicEventType::TacticalSignalProjected;
			});
		if (Event != nullptr)
		{
			AcceptedFeedback = UEGTTacticalControllerPrivate::LocalizedFormat(
				TEXT("tactical.signal-result-format"),
				TEXT("Signal pressure {0} • roll {1} / {2} • morale −{3}."),
				{
					Event->bSuccessful
						? UEGTTacticalControllerPrivate::Localized(TEXT("tactical.signal-lock-held"), TEXT("LOCK HELD"))
						: UEGTTacticalControllerPrivate::Localized(TEXT("tactical.signal-lock-lost"), TEXT("LOCK LOST")),
					FString::FromInt(Event->Roll),
					FString::FromInt(Event->HitChance),
					FString::FromInt(Event->Quantity)
				});
		}
	}
	PresentCommandResult(Result, AcceptedFeedback);
	if (Result.bAccepted && ActionType == ETacticalHudActionType::EndTurn)
	{
		RunAdversaryTurnIfNeeded();
	}
}

void AUEGTTacticalPlayerController::RunAdversaryTurnIfNeeded()
{
	if (!CurrentSnapshot.bSucceeded || CurrentSnapshot.Phase != ETacticalBattlePhase::AdversaryTurn)
	{
		return;
	}
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	FRunTacticalAiTurnCommand Command;
	Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
	Command.BattleId = CurrentSnapshot.BattleId;
	const FStrategicCommandResult Result = Instance->RunTacticalAiTurn(Command);
	PresentCommandResult(Result, UEGTTacticalControllerPrivate::LocalizedFormat(
		Result.Events.Num() == 1
			? TEXT("tactical.adversary-turn-complete-one-format")
			: TEXT("tactical.adversary-turn-complete-many-format"),
		Result.Events.Num() == 1
			? TEXT("Adversary turn complete • {0} event")
			: TEXT("Adversary turn complete • {0} events"),
		{ FString::FromInt(Result.Events.Num()) }));
}

bool AUEGTTacticalPlayerController::DeferEndTurnForConfirmation()
{
	UUEGTUserSettings* Settings = UUEGTUserSettings::Get();
	int32 ReadyAgentCount = 0;
	const int32 RemainingActionPoints = CountRemainingPlayerActionPoints(CurrentSnapshot, ReadyAgentCount);
	const bool bConfirmationRequired = Settings != nullptr
		&& Settings->ShouldConfirmEndTurn(ReadyAgentCount, RemainingActionPoints);
	if (!EndTurnConfirmation.ShouldDefer(
		bConfirmationRequired,
		CurrentSnapshot.ExpectedCommandSequence))
	{
		return false;
	}
	if (HudWidget != nullptr)
	{
		HudWidget->ShowStatusMessage(
			RemainingActionPoints > 0
				? UEGTTacticalControllerPrivate::LocalizedFormat(
					ReadyAgentCount == 1
						? TEXT("tactical.end-turn-confirm-one-format")
						: TEXT("tactical.end-turn-confirm-many-format"),
					ReadyAgentCount == 1
						? TEXT("{0} ready agent still holds {1} AP. Press END TURN again to confirm.")
						: TEXT("{0} ready agents still hold {1} AP. Press END TURN again to confirm."),
					{ FString::FromInt(ReadyAgentCount), FString::FromInt(RemainingActionPoints) })
				: UEGTTacticalControllerPrivate::Localized(
					TEXT("tactical.end-turn-confirm"),
					TEXT("Press END TURN again to confirm.")),
			true);
	}
	return true;
}

bool AUEGTTacticalPlayerController::AutoSelectReadyTacticalUnit()
{
	const UUEGTUserSettings* Settings = UUEGTUserSettings::Get();
	if (Settings == nullptr || !Settings->ShouldAutoSelectReadyAgent()
		|| !CurrentSnapshot.bSucceeded || CurrentSnapshot.Phase != ETacticalBattlePhase::PlayerTurn)
	{
		return false;
	}
	const FTacticalHudUnitView* Selected = CurrentSnapshot.Units.FindByPredicate(
		[this](const FTacticalHudUnitView& Unit) { return Unit.UnitId == SelectedUnitId; });
	if (Selected != nullptr && Selected->Team == ETacticalTeam::Player
		&& !Selected->bIncapacitated && !Selected->bExtracted
		&& Selected->RemainingActionPoints > 0)
	{
		return false;
	}
	const FGuid NextUnitId = FindNextReadyPlayerUnit(CurrentSnapshot, SelectedUnitId);
	if (!NextUnitId.IsValid() || NextUnitId == SelectedUnitId)
	{
		return false;
	}
	const FTacticalHudUnitView* Next = CurrentSnapshot.Units.FindByPredicate(
		[&NextUnitId](const FTacticalHudUnitView& Unit) { return Unit.UnitId == NextUnitId; });
	if (Next == nullptr)
	{
		return false;
	}
	SelectedUnitId = NextUnitId;
	SelectedWeaponItemId = NAME_None;
	SelectedDeviceItemId = NAME_None;
	FireMode = ETacticalFireMode::Single;
	ViewedLevel = Next->Z;
	EndTurnConfirmation.Reset();
	return true;
}

void AUEGTTacticalPlayerController::FocusCameraOnTacticalUnit(const FTacticalHudUnitView& Unit)
{
	const UUEGTUserSettings* Settings = UUEGTUserSettings::Get();
	if (Settings != nullptr && !Settings->ShouldCenterCameraOnSelection())
	{
		return;
	}
	if (AUEGTTacticalCameraPawn* CameraPawn = GetTacticalCameraPawn())
	{
		CameraPawn->FocusCell(
			Unit.X,
			Unit.Y,
			Unit.Z,
			BoardActor != nullptr ? BoardActor->GetCellSize() : 100.0f,
			BoardActor != nullptr ? BoardActor->GetLevelHeight() : 180.0f);
	}
}

void AUEGTTacticalPlayerController::SelectOrTargetTacticalUnit(const FGuid UnitId)
{
	const FTacticalHudUnitView* Unit = CurrentSnapshot.Units.FindByPredicate(
		[&UnitId](const FTacticalHudUnitView& Entry) { return Entry.UnitId == UnitId; });
	if (Unit == nullptr)
	{
		return;
	}
	const int32 CueX = Unit->X;
	const int32 CueY = Unit->Y;
	const int32 CueZ = Unit->Z;
	const bool bChanged = Unit->Team == ETacticalTeam::Player
		? SelectedUnitId != UnitId
		: HoveredUnitId != UnitId;
	if (Unit->Team == ETacticalTeam::Player)
	{
		SelectedUnitId = UnitId;
		SelectedWeaponItemId = NAME_None;
		SelectedDeviceItemId = NAME_None;
		FireMode = ETacticalFireMode::Single;
		EndTurnConfirmation.Reset();
	}
	else
	{
		HoveredUnitId = UnitId;
		HoveredObjectiveId = NAME_None;
		bHasHoveredCell = true;
		HoveredX = Unit->X;
		HoveredY = Unit->Y;
		HoveredZ = Unit->Z;
	}
	ViewedLevel = Unit->Z;
	RefreshTacticalPresentation();
	if (bChanged && AudioDirector != nullptr)
	{
		if (BoardActor != nullptr)
		{
			AudioDirector->PlayCueAtLocation(
				EUEGTAudioCue::TacticalSelection,
				BoardActor->GridToWorld(CueX, CueY, CueZ, 42.0f));
		}
		else
		{
			AudioDirector->PlayCue(EUEGTAudioCue::TacticalSelection);
		}
	}
	if (UnitId == SelectedUnitId)
	{
		if (const FTacticalHudUnitView* Selected = CurrentSnapshot.Units.FindByPredicate(
			[this](const FTacticalHudUnitView& Entry) { return Entry.UnitId == SelectedUnitId; }))
		{
			FocusCameraOnTacticalUnit(*Selected);
		}
	}
}

void AUEGTTacticalPlayerController::TargetTacticalObjective(const FName ObjectiveId)
{
	const FTacticalHudObjectiveView* Objective = CurrentSnapshot.Objectives.FindByPredicate(
		[ObjectiveId](const FTacticalHudObjectiveView& Entry) { return Entry.ObjectiveId == ObjectiveId; });
	if (Objective == nullptr)
	{
		return;
	}
	const int32 CueX = Objective->X;
	const int32 CueY = Objective->Y;
	const int32 CueZ = Objective->Z;
	const bool bChanged = HoveredObjectiveId != ObjectiveId;
	HoveredObjectiveId = ObjectiveId;
	HoveredUnitId.Invalidate();
	bHasHoveredCell = true;
	HoveredX = Objective->X;
	HoveredY = Objective->Y;
	HoveredZ = Objective->Z;
	ViewedLevel = Objective->Z;
	RefreshTacticalPresentation();
	if (bChanged && AudioDirector != nullptr)
	{
		if (BoardActor != nullptr)
		{
			AudioDirector->PlayCueAtLocation(
				EUEGTAudioCue::TacticalSelection,
				BoardActor->GridToWorld(CueX, CueY, CueZ, 30.0f));
		}
		else
		{
			AudioDirector->PlayCue(EUEGTAudioCue::TacticalSelection);
		}
	}
}

void AUEGTTacticalPlayerController::HandlePrimaryClick()
{
	if (bStrategicMode)
	{
		FUEGTStrategicGlobeHit GlobeHit;
		if (TraceGlobe(GlobeHit) && StrategicHudWidget != nullptr)
		{
			StrategicHudWidget->SelectGlobeMarker(GlobeHit.Marker);
		}
		return;
	}
	FUEGTTacticalBoardHit Hit;
	if (!TraceBoard(Hit))
	{
		return;
	}
	bHasHoveredCell = Hit.bHasCell;
	HoveredX = Hit.X;
	HoveredY = Hit.Y;
	HoveredZ = Hit.Z;
	HoveredUnitId = Hit.UnitId;
	HoveredObjectiveId = Hit.ObjectiveId;
	if (Hit.UnitId.IsValid())
	{
		SelectOrTargetTacticalUnit(Hit.UnitId);
		return;
	}
	if (!Hit.ObjectiveId.IsNone())
	{
		TargetTacticalObjective(Hit.ObjectiveId);
		return;
	}
	RefreshTacticalPresentation();
}

void AUEGTTacticalPlayerController::HandleContextClick()
{
	if (bStrategicMode)
	{
		return;
	}
	if (CurrentSnapshot.Phase == ETacticalBattlePhase::Deployment)
	{
		ExecuteHudAction(ETacticalHudActionType::ConfirmDeployment);
		return;
	}
	if (CurrentSnapshot.Phase == ETacticalBattlePhase::Resolved)
	{
		ResolveTacticalDebrief();
		return;
	}
	const FTacticalHudActionAvailability* Attack = CurrentSnapshot.FindAction(ETacticalHudActionType::AttackUnit);
	if (HoveredUnitId.IsValid() && Attack != nullptr && Attack->bAvailable)
	{
		ExecuteHudAction(ETacticalHudActionType::AttackUnit);
		return;
	}
	const FTacticalHudActionAvailability* Objective = CurrentSnapshot.FindAction(ETacticalHudActionType::InteractObjective);
	if (!HoveredObjectiveId.IsNone() && Objective != nullptr && Objective->bAvailable)
	{
		ExecuteHudAction(ETacticalHudActionType::InteractObjective);
		return;
	}
	const FTacticalHudActionAvailability* Door = CurrentSnapshot.FindAction(ETacticalHudActionType::OperateDoor);
	if (Door != nullptr && Door->bAvailable)
	{
		ExecuteHudAction(ETacticalHudActionType::OperateDoor);
		return;
	}
	ExecuteHudAction(ETacticalHudActionType::Move);
}

void AUEGTTacticalPlayerController::ResolveTacticalDebrief()
{
	if (!CurrentSnapshot.bSucceeded || CurrentSnapshot.Phase != ETacticalBattlePhase::Resolved)
	{
		return;
	}
	UUEGTGameInstance* Instance = GetGameInstance<UUEGTGameInstance>();
	if (Instance == nullptr)
	{
		return;
	}
	FResolveTacticalOperationCommand Command;
	Command.ExpectedSequence = CurrentSnapshot.ExpectedCommandSequence;
	Command.OperationId = CurrentSnapshot.OperationId;
	Command.bObjectiveCompleted = !CurrentSnapshot.Objectives.IsEmpty()
		&& !CurrentSnapshot.Objectives.ContainsByPredicate(
			[](const FTacticalHudObjectiveView& Objective)
			{
				return Objective.Status != ETacticalObjectiveStatus::Completed;
			});
	const FStrategicCommandResult Result = Instance->ResolveTacticalOperation(Command);
	bViewingDebrief = Result.bAccepted;
	PresentCommandResult(Result, UEGTTacticalControllerPrivate::Localized(
		TEXT("tactical.operation-resolved"), TEXT("Tactical operation resolved. Debrief ready.")));
}

void AUEGTTacticalPlayerController::HandleConfirmOrContinue()
{
	if (bStrategicMode)
	{
		return;
	}
	if (CurrentSnapshot.Phase == ETacticalBattlePhase::Resolved)
	{
		ResolveTacticalDebrief();
	}
	else if (CurrentSnapshot.Phase == ETacticalBattlePhase::Deployment)
	{
		ExecuteHudAction(ETacticalHudActionType::ConfirmDeployment);
	}
	else
	{
		HandleContextClick();
	}
}

void AUEGTTacticalPlayerController::HandleEndTurn() { ExecuteHudAction(ETacticalHudActionType::EndTurn); }
void AUEGTTacticalPlayerController::HandleStance() { ExecuteHudAction(ETacticalHudActionType::ChangeStance); }
void AUEGTTacticalPlayerController::HandleReload() { ExecuteHudAction(ETacticalHudActionType::Reload); }
void AUEGTTacticalPlayerController::HandleObjective() { ExecuteHudAction(ETacticalHudActionType::InteractObjective); }
void AUEGTTacticalPlayerController::HandleExtract() { ExecuteHudAction(ETacticalHudActionType::Extract); }
void AUEGTTacticalPlayerController::HandleDoor() { ExecuteHudAction(ETacticalHudActionType::OperateDoor); }
void AUEGTTacticalPlayerController::HandleDevice() { ExecuteHudAction(ETacticalHudActionType::DeployDevice); }
void AUEGTTacticalPlayerController::HandleTerrainAttack() { ExecuteHudAction(ETacticalHudActionType::AttackTerrain); }
void AUEGTTacticalPlayerController::HandleSignal() { ExecuteHudAction(ETacticalHudActionType::ProjectSignal); }
void AUEGTTacticalPlayerController::HandleNextUnit() { SelectRelativeUnit(1); }
void AUEGTTacticalPlayerController::HandlePreviousUnit() { SelectRelativeUnit(-1); }
void AUEGTTacticalPlayerController::HandleLevelUp() { ChangeViewedLevel(1); }
void AUEGTTacticalPlayerController::HandleLevelDown() { ChangeViewedLevel(-1); }
void AUEGTTacticalPlayerController::HandleZoomIn() { if (AUEGTTacticalCameraPawn* CameraPawn = GetTacticalCameraPawn()) CameraPawn->Zoom(1.0f); }
void AUEGTTacticalPlayerController::HandleZoomOut() { if (AUEGTTacticalCameraPawn* CameraPawn = GetTacticalCameraPawn()) CameraPawn->Zoom(-1.0f); }

void AUEGTTacticalPlayerController::SelectRelativeUnit(const int32 Direction)
{
	TArray<const FTacticalHudUnitView*> Players;
	for (const FTacticalHudUnitView& Unit : CurrentSnapshot.Units)
	{
		if (Unit.Team == ETacticalTeam::Player && !Unit.bIncapacitated && !Unit.bExtracted)
		{
			Players.Add(&Unit);
		}
	}
	if (Players.IsEmpty())
	{
		return;
	}
	int32 CurrentIndex = Players.IndexOfByPredicate(
		[this](const FTacticalHudUnitView* Unit) { return Unit != nullptr && Unit->UnitId == SelectedUnitId; });
	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = Direction >= 0 ? -1 : 0;
	}
	const int32 NewIndex = (CurrentIndex + Direction + Players.Num()) % Players.Num();
	SelectOrTargetTacticalUnit(Players[NewIndex]->UnitId);
}

void AUEGTTacticalPlayerController::ChangeViewedLevel(const int32 Delta)
{
	if (!CurrentSnapshot.bSucceeded)
	{
		return;
	}
	ViewedLevel = FMath::Clamp(ViewedLevel + Delta, 0, FMath::Max(0, CurrentSnapshot.Levels - 1));
	RefreshTacticalPresentation();
	if (AUEGTTacticalCameraPawn* CameraPawn = GetTacticalCameraPawn())
	{
		FVector Location = CameraPawn->GetActorLocation();
		Location.Z = ViewedLevel * (BoardActor != nullptr ? BoardActor->GetLevelHeight() : 180.0f);
		CameraPawn->SetActorLocation(Location);
	}
}

void AUEGTTacticalPlayerController::HandleCycleWeapon()
{
	if (bStrategicMode)
	{
		AdvanceStrategicClock(EStrategicTimeRate::FiveSeconds);
		return;
	}
	const FTacticalHudUnitView* Selected = CurrentSnapshot.Units.FindByPredicate(
		[this](const FTacticalHudUnitView& Unit) { return Unit.UnitId == SelectedUnitId; });
	if (Selected == nullptr || Selected->Weapons.IsEmpty())
	{
		return;
	}
	int32 Index = Selected->Weapons.IndexOfByPredicate(
		[this](const FTacticalHudWeaponView& Weapon) { return Weapon.ItemId == SelectedWeaponItemId; });
	Index = (Index + 1 + Selected->Weapons.Num()) % Selected->Weapons.Num();
	SelectedWeaponItemId = Selected->Weapons[Index].ItemId;
	FireMode = ETacticalFireMode::Single;
	RefreshTacticalPresentation();
}

void AUEGTTacticalPlayerController::HandleToggleFireMode()
{
	if (bStrategicMode)
	{
		AdvanceStrategicClock(EStrategicTimeRate::OneMinute);
		return;
	}
	FireMode = FireMode == ETacticalFireMode::Single ? ETacticalFireMode::Burst : ETacticalFireMode::Single;
	RefreshTacticalPresentation();
}

void AUEGTTacticalPlayerController::HandleCycleDevice()
{
	if (bStrategicMode)
	{
		AdvanceStrategicClock(EStrategicTimeRate::FiveMinutes);
		return;
	}
	const FTacticalHudUnitView* Selected = CurrentSnapshot.Units.FindByPredicate(
		[this](const FTacticalHudUnitView& Unit) { return Unit.UnitId == SelectedUnitId; });
	if (Selected == nullptr)
	{
		return;
	}
	TArray<FName> Devices;
	for (const FTacticalHudItemView& Item : Selected->CarriedItems)
	{
		if (Item.Category == FName(TEXT("device")) && Item.Quantity > 0)
		{
			Devices.Add(Item.ItemId);
		}
	}
	if (Devices.IsEmpty())
	{
		return;
	}
	int32 Index = Devices.IndexOfByKey(SelectedDeviceItemId);
	Index = (Index + 1 + Devices.Num()) % Devices.Num();
	SelectedDeviceItemId = Devices[Index];
	RefreshTacticalPresentation();
}

void AUEGTTacticalPlayerController::HandleCursorUp() { MoveTargetCursor(0, -1); }
void AUEGTTacticalPlayerController::HandleCursorDown() { MoveTargetCursor(0, 1); }
void AUEGTTacticalPlayerController::HandleCursorLeft() { MoveTargetCursor(-1, 0); }
void AUEGTTacticalPlayerController::HandleCursorRight() { MoveTargetCursor(1, 0); }
void AUEGTTacticalPlayerController::HandleStrategicTimeFour() { if (bStrategicMode) AdvanceStrategicClock(EStrategicTimeRate::ThirtyMinutes); }
void AUEGTTacticalPlayerController::HandleStrategicTimeFive() { if (bStrategicMode) AdvanceStrategicClock(EStrategicTimeRate::OneHour); }
void AUEGTTacticalPlayerController::HandleStrategicTimeSix() { if (bStrategicMode) AdvanceStrategicClock(EStrategicTimeRate::OneDay); }

void AUEGTTacticalPlayerController::MoveTargetCursor(const int32 DeltaX, const int32 DeltaY)
{
	if (!CurrentSnapshot.bSucceeded)
	{
		return;
	}
	if (!bHasHoveredCell)
	{
		const FTacticalHudUnitView* Selected = CurrentSnapshot.Units.FindByPredicate(
			[this](const FTacticalHudUnitView& Unit) { return Unit.UnitId == SelectedUnitId; });
		HoveredX = Selected != nullptr ? Selected->X : CurrentSnapshot.Width / 2;
		HoveredY = Selected != nullptr ? Selected->Y : CurrentSnapshot.Height / 2;
		HoveredZ = ViewedLevel;
		bHasHoveredCell = true;
	}
	HoveredX = FMath::Clamp(HoveredX + DeltaX, 0, FMath::Max(0, CurrentSnapshot.Width - 1));
	HoveredY = FMath::Clamp(HoveredY + DeltaY, 0, FMath::Max(0, CurrentSnapshot.Height - 1));
	HoveredZ = ViewedLevel;
	HoveredUnitId.Invalidate();
	HoveredObjectiveId = NAME_None;
	if (const FTacticalHudUnitView* Unit = CurrentSnapshot.Units.FindByPredicate(
		[this](const FTacticalHudUnitView& Entry)
		{
			return Entry.X == HoveredX && Entry.Y == HoveredY && Entry.Z == HoveredZ
				&& !Entry.bExtracted;
		}))
	{
		HoveredUnitId = Unit->UnitId;
	}
	if (const FTacticalHudObjectiveView* Objective = CurrentSnapshot.Objectives.FindByPredicate(
		[this](const FTacticalHudObjectiveView& Entry)
		{
			return Entry.X == HoveredX && Entry.Y == HoveredY && Entry.Z == HoveredZ;
		}))
	{
		HoveredObjectiveId = Objective->ObjectiveId;
	}
	RefreshTacticalPresentation();
}
