// Copyright 2026 UEGT contributors. MIT License.

#include "UEGTGameInstance.h"

#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace UEGTGameInstancePrivate
{
	void AddSaveError(FCampaignSaveStoreResult& Result, const FName Code, FString Message)
	{
		FCampaignSaveDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = ECampaignSaveDiagnosticSeverity::Error;
		Diagnostic.Code = Code;
		Diagnostic.Message = MoveTemp(Message);
	}
}

void UUEGTGameInstance::Init()
{
	Super::Init();
	ReloadContent();
}

bool UUEGTGameInstance::ReloadContent()
{
	if (bCampaignActive)
	{
		return false;
	}

	const FString RulesDirectory = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Rules"));
	TArray<FString> ContentDirectories = { RulesDirectory };
	if (!FParse::Param(FCommandLine::Get(), TEXT("UEGTNoUserMods")))
	{
		FString ModsDirectory;
		const bool bExplicitModsDirectory = FParse::Value(
			FCommandLine::Get(), TEXT("UEGTModsDir="), ModsDirectory);
		ModsDirectory = ModsDirectory.TrimQuotes().TrimStartAndEnd();
		if (!bExplicitModsDirectory)
		{
			ModsDirectory = GetDefaultUserModsDirectory();
		}
		else if (!ModsDirectory.IsEmpty() && FPaths::IsRelative(ModsDirectory))
		{
			ModsDirectory = FPaths::Combine(FPaths::ProjectDir(), ModsDirectory);
		}
		if (bExplicitModsDirectory || IFileManager::Get().DirectoryExists(*ModsDirectory))
		{
			ContentDirectories.Add(ModsDirectory);
		}
	}
	return ReloadContentFromDirectories(ContentDirectories);
}

bool UUEGTGameInstance::ReloadContentFromDirectories(const TArray<FString>& ContentDirectories)
{
	if (bCampaignActive)
	{
		return false;
	}

	FContentCatalogLoadResult Catalog = FContentPackageCatalog::LoadDirectories(ContentDirectories);
	ContentDiagnostics = MoveTemp(Catalog.Diagnostics);
	if (!Catalog.bSucceeded)
	{
		bContentReady = false;
		return false;
	}

	TArray<FCampaignContentVersion> NextContentVersions;
	NextContentVersions.Reserve(Catalog.Packages.Num());
	for (const FContentPackage& Package : Catalog.Packages)
	{
		FCampaignContentVersion& Version = NextContentVersions.AddDefaulted_GetRef();
		Version.PackageId = Package.Descriptor.PackageId;
		Version.Version = Package.Descriptor.Version;
	}
	NextContentVersions.Sort(
		[](const FCampaignContentVersion& Left, const FCampaignContentVersion& Right)
		{
			return Left.PackageId.LexicalLess(Right.PackageId);
		});

	LoadedRules = MoveTemp(Catalog.RuleSet);
	LoadedContentVersions = MoveTemp(NextContentVersions);
	LoadedContentFiles = MoveTemp(Catalog.LoadedFiles);
	TArray<FString> NextContentDirectories;
	for (const FString& ContentDirectory : ContentDirectories)
	{
		FString Directory = ContentDirectory;
		Directory = FPaths::ConvertRelativePathToFull(Directory.TrimStartAndEnd());
		FPaths::NormalizeDirectoryName(Directory);
		FPaths::CollapseRelativeDirectories(Directory);
		if (!NextContentDirectories.ContainsByPredicate(
			[&Directory](const FString& Existing)
			{
				return FPaths::IsSamePath(Existing, Directory);
			}))
		{
			NextContentDirectories.Add(MoveTemp(Directory));
		}
	}
	NextContentDirectories.Sort();
	LoadedContentDirectories = MoveTemp(NextContentDirectories);
	bContentReady = true;
	return true;
}

FString UUEGTGameInstance::GetDefaultUserModsDirectory() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Mods"));
}

bool UUEGTGameInstance::CalculateFundingProjection(
	const ECampaignDifficulty Difficulty,
	const EUEGTFundingModel FundingModel,
	FUEGTFundingProjection& OutProjection)
{
	OutProjection = FUEGTFundingProjection();
	int64 BaseStartingFunds = 0;
	int64 BaseMonthlyFunding = 0;
	switch (Difficulty)
	{
	case ECampaignDifficulty::Cadet:
		BaseStartingFunds = 2000000;
		BaseMonthlyFunding = 650000;
		break;
	case ECampaignDifficulty::Standard:
		BaseStartingFunds = 1500000;
		BaseMonthlyFunding = 500000;
		break;
	case ECampaignDifficulty::Veteran:
		BaseStartingFunds = 1250000;
		BaseMonthlyFunding = 425000;
		break;
	case ECampaignDifficulty::Apex:
		BaseStartingFunds = 1000000;
		BaseMonthlyFunding = 350000;
		break;
	default:
		return false;
	}

	int32 StartingPercent = 100;
	int32 MonthlyPercent = 100;
	switch (FundingModel)
	{
	case EUEGTFundingModel::BalancedMandate:
		break;
	case EUEGTFundingModel::RapidMobilization:
		StartingPercent = 120;
		MonthlyPercent = 80;
		break;
	case EUEGTFundingModel::SustainedCharter:
		StartingPercent = 80;
		MonthlyPercent = 120;
		break;
	default:
		return false;
	}

	OutProjection.StartingFunds = BaseStartingFunds * StartingPercent / 100;
	OutProjection.MonthlyFunding = BaseMonthlyFunding * MonthlyPercent / 100;
	return true;
}

bool UUEGTGameInstance::StartNewCampaign(
	const ECampaignDifficulty Difficulty,
	const int64 Seed,
	const EUEGTFundingModel FundingModel)
{
	if (!bContentReady || SimulationConfig.StartingAdversaryDelayHours <= 0
		|| SimulationConfig.StartingAdversaryDelayHours > MAX_int64 / 3600LL)
	{
		return false;
	}
	FUEGTFundingProjection Funding;
	int64 StartingAdversaryDelaySeconds = 0;
	if (!CalculateFundingProjection(Difficulty, FundingModel, Funding)
		|| !FStrategicCommandService::ScaleAdversaryIntervalSeconds(
			static_cast<int64>(SimulationConfig.StartingAdversaryDelayHours) * 3600LL,
			Difficulty,
			SimulationConfig,
			StartingAdversaryDelaySeconds))
	{
		return false;
	}

	FCampaignState NewState;
	NewState.Difficulty = Difficulty;
	NewState.SimulationRandom.Initialize(Seed);
	NewState.NextAdversaryMissionSeconds = StartingAdversaryDelaySeconds;
	TSet<FName> AdversaryRegions;
	for (const TPair<FName, FStrategicRegionRule>& Pair : LoadedRules.Regions)
	{
		AdversaryRegions.Add(Pair.Key);
	}
	for (const TPair<FName, FAdversaryMissionRule>& Pair : LoadedRules.AdversaryMissions)
	{
		AdversaryRegions.Add(Pair.Value.TargetRegionId);
	}
	TArray<FName> SortedRegionIds = AdversaryRegions.Array();
	SortedRegionIds.Sort(FNameLexicalLess());
	int64 TotalFundingWeight = 0;
	for (const FName RegionId : SortedRegionIds)
	{
		const FStrategicRegionRule* RegionRule = LoadedRules.Regions.Find(RegionId);
		TotalFundingWeight += RegionRule != nullptr ? RegionRule->FundingWeight : 1;
	}
	int64 AllocatedFunding = 0;
	for (int32 RegionIndex = 0; RegionIndex < SortedRegionIds.Num(); ++RegionIndex)
	{
		const FName RegionId = SortedRegionIds[RegionIndex];
		FRegionalPressureState& Pressure = NewState.RegionalPressure.AddDefaulted_GetRef();
		Pressure.RegionId = RegionId;
		const FStrategicRegionRule* RegionRule = LoadedRules.Regions.Find(RegionId);
		const int64 FundingWeight = RegionRule != nullptr ? RegionRule->FundingWeight : 1;
		int64 Contribution = 0;
		if (RegionIndex + 1 == SortedRegionIds.Num())
		{
			Contribution = Funding.MonthlyFunding - AllocatedFunding;
		}
		else if (TotalFundingWeight > 0)
		{
			const int64 Whole = Funding.MonthlyFunding / TotalFundingWeight;
			const int64 Remainder = Funding.MonthlyFunding % TotalFundingWeight;
			Contribution = Whole * FundingWeight + Remainder * FundingWeight / TotalFundingWeight;
		}
		AllocatedFunding += Contribution;
		FRegionalMandateState& Mandate = NewState.RegionalMandates.AddDefaulted_GetRef();
		Mandate.RegionId = RegionId;
		Mandate.Support = RegionRule != nullptr ? RegionRule->InitialSupport : 50;
		Mandate.BaselineMonthlyFunding = Contribution;
		Mandate.CurrentMonthlyFunding = Contribution;
	}
	NewState.RegionalPressure.Sort(
		[](const FRegionalPressureState& Left, const FRegionalPressureState& Right)
		{
			return Left.RegionId.LexicalLess(Right.RegionId);
		});
	NewState.Funds = Funding.StartingFunds;
	NewState.MonthlyFunding = SortedRegionIds.IsEmpty() ? Funding.MonthlyFunding : AllocatedFunding;

	CampaignState = MoveTemp(NewState);
	ActiveCampaignId = FGuid::NewGuid();
	CampaignCreatedUtc = FDateTime::UtcNow();
	LastTacticalDebrief = FTacticalDebriefView();
	bCampaignActive = true;
	return true;
}

bool UUEGTGameInstance::GetAdversaryDifficultyTuning(
	const ECampaignDifficulty Difficulty,
	FAdversaryDifficultyTuning& OutTuning) const
{
	return FStrategicCommandService::GetAdversaryDifficultyTuning(
		Difficulty, SimulationConfig, OutTuning);
}

FStrategicDashboardSnapshot UUEGTGameInstance::BuildStrategicDashboard() const
{
	if (bCampaignActive && bContentReady)
	{
		return FStrategicPresentationService::BuildDashboard(CampaignState, LoadedRules, SimulationConfig);
	}

	FStrategicDashboardSnapshot Snapshot;
	Snapshot.Diagnostics.Add(bContentReady
		? TEXT("No campaign is active.")
		: TEXT("The content catalog is not ready."));
	return Snapshot;
}

FStrategicCommandResult UUEGTGameInstance::EstablishBase(const FEstablishBaseCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::StartResearch(const FStartResearchCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SetResearchStaff(const FSetResearchStaffCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::CancelResearch(const FCancelResearchCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::AdvanceStrategicTime(const FAdvanceStrategicTimeCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ExecuteRegionalDiplomacy(const FRegionalDiplomacyCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SignRegionalCharter(const FSignRegionalCharterCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::RatifyHorizonCompact(
	const FRatifyHorizonCompactCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::DeployReciprocalAid(
	const FDeployReciprocalAidCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::RestoreHorizonCompactMember(
	const FRestoreHorizonCompactMemberCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::CallHorizonCompactEmergencyVote(
	const FCallHorizonCompactEmergencyVoteCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

#if !UE_BUILD_SHIPPING
bool UUEGTGameInstance::PrepareHorizonCompactEmergencyVoteDemo(const FName TargetRegionId)
{
	if (!bCampaignActive || !bContentReady || !CampaignState.bHorizonCompactRatified)
	{
		return false;
	}
	FRegionalMandateState* TargetMandate = CampaignState.RegionalMandates.FindByPredicate(
		[TargetRegionId](const FRegionalMandateState& Mandate)
		{
			return Mandate.RegionId == TargetRegionId;
		});
	FRegionalPressureState* TargetPressure = CampaignState.RegionalPressure.FindByPredicate(
		[TargetRegionId](const FRegionalPressureState& Pressure)
		{
			return Pressure.RegionId == TargetRegionId;
		});
	if (TargetMandate == nullptr || TargetPressure == nullptr
		|| !TargetMandate->bResilienceCharterSigned)
	{
		return false;
	}

	TargetMandate->Support = 30;
	TargetMandate->bHorizonCompactMemberWithdrawn = true;
	TargetPressure->Pressure = 45;
	for (FRegionalMandateState& Mandate : CampaignState.RegionalMandates)
	{
		if (!Mandate.bResilienceCharterSigned || Mandate.RegionId == TargetRegionId)
		{
			continue;
		}
		Mandate.bHorizonCompactMemberWithdrawn = false;
		Mandate.Support = FMath::Max(
			Mandate.Support,
			SimulationConfig.HorizonCompactWithdrawalSupportThreshold
				+ SimulationConfig.HorizonCompactEmergencyVoterSupportCost);
		if (FRegionalPressureState* Pressure = CampaignState.RegionalPressure.FindByPredicate(
				[&Mandate](const FRegionalPressureState& Entry)
				{
					return Entry.RegionId == Mandate.RegionId;
				}))
		{
			Pressure->Pressure = FMath::Min(
				Pressure->Pressure,
				SimulationConfig.HorizonCompactEmergencyMaximumVoterPressure);
		}
	}
	CampaignState.Funds = FMath::Max(
		CampaignState.Funds, SimulationConfig.HorizonCompactEmergencyVoteCost);
	CampaignState.LastCoalitionEmergencyVoteMonth = 0;
	int64 MonthlyFunding = 0;
	for (FRegionalMandateState& Mandate : CampaignState.RegionalMandates)
	{
		int64 Contribution = 0;
		if (!FStrategicCommandService::CalculateRegionalFundingContribution(
				Mandate, SimulationConfig, true, Contribution)
			|| Contribution > MAX_int64 - MonthlyFunding)
		{
			return false;
		}
		Mandate.CurrentMonthlyFunding = Contribution;
		MonthlyFunding += Contribution;
	}
	CampaignState.MonthlyFunding = MonthlyFunding;

	FCallHorizonCompactEmergencyVoteCommand Vote;
	Vote.ExpectedSequence = CampaignState.CommandSequence;
	Vote.TargetRegionId = TargetRegionId;
	return FStrategicCommandService::EvaluateHorizonCompactEmergencyVote(
		CampaignState, SimulationConfig, Vote).bAllowed;
}

bool UUEGTGameInstance::PrepareCoalitionCounterplayDemo(const FName MissionRuleId)
{
	if (!bCampaignActive || !bContentReady || !CampaignState.bHorizonCompactRatified)
	{
		return false;
	}
	const FAdversaryMissionRule* MissionRule = LoadedRules.AdversaryMissions.Find(MissionRuleId);
	if (MissionRule == nullptr || MissionRule->CompactPeerSupportLossOnEscape <= 0
		|| MissionRule->WithdrawnCompactSupportGainOnThwarted <= 0)
	{
		return false;
	}

	FRegionalMandateState* ActiveMember = CampaignState.RegionalMandates.FindByPredicate(
		[](const FRegionalMandateState& Mandate)
		{
			return Mandate.RegionId == FName(TEXT("region.cascadia"))
				&& Mandate.bResilienceCharterSigned;
		});
	FRegionalMandateState* WithdrawnMember = CampaignState.RegionalMandates.FindByPredicate(
		[](const FRegionalMandateState& Mandate)
		{
			return Mandate.RegionId == FName(TEXT("region.north-atlantic"))
				&& Mandate.bResilienceCharterSigned;
		});
	if (ActiveMember == nullptr || WithdrawnMember == nullptr)
	{
		return false;
	}
	ActiveMember->Support = 27;
	ActiveMember->bHorizonCompactMemberWithdrawn = false;
	WithdrawnMember->Support = 30;
	WithdrawnMember->bHorizonCompactMemberWithdrawn = true;

	int64 MonthlyFunding = 0;
	for (FRegionalMandateState& Mandate : CampaignState.RegionalMandates)
	{
		int64 Contribution = 0;
		if (!FStrategicCommandService::CalculateRegionalFundingContribution(
				Mandate, SimulationConfig, true, Contribution)
			|| Contribution > MAX_int64 - MonthlyFunding)
		{
			return false;
		}
		Mandate.CurrentMonthlyFunding = Contribution;
		MonthlyFunding += Contribution;
	}
	CampaignState.MonthlyFunding = MonthlyFunding;

	CampaignState.StrategicContacts.Reset();
	CampaignState.AdversaryMissions.Reset();
	CampaignState.BaseAssaults.Reset();
	const FGuid ContactId(0x434f414c, 0x4954494f, 0x4e343201, 0x00000001);
	FCreateStrategicContactCommand Create;
	Create.ExpectedSequence = CampaignState.CommandSequence;
	Create.ContactId = ContactId;
	Create.ContactRuleId = MissionRule->ContactRuleId;
	Create.OriginLongitudeMilliDegrees = MissionRule->OriginLongitudeMilliDegrees;
	Create.OriginLatitudeMilliDegrees = MissionRule->OriginLatitudeMilliDegrees;
	Create.DestinationLongitudeMilliDegrees = MissionRule->DestinationLongitudeMilliDegrees;
	Create.DestinationLatitudeMilliDegrees = MissionRule->DestinationLatitudeMilliDegrees;
	const FStrategicCommandResult Created = CreateStrategicContact(Create);
	if (!Created.bAccepted)
	{
		return false;
	}
	FStrategicContactState* Contact = CampaignState.StrategicContacts.FindByPredicate(
		[&ContactId](const FStrategicContactState& Entry)
		{
			return Entry.ContactId == ContactId;
		});
	if (Contact == nullptr || CampaignState.AdversaryMissionsLaunched == MAX_int32)
	{
		return false;
	}
	Contact->Status = EStrategicContactStatus::Detected;
	FAdversaryMissionState& Mission = CampaignState.AdversaryMissions.AddDefaulted_GetRef();
	Mission.MissionId = FGuid(0x434f414c, 0x4954494f, 0x4e343201, 0x00000002);
	Mission.ContactId = ContactId;
	Mission.MissionRuleId = MissionRuleId;
	Mission.StartedUtc = CampaignState.StrategicTime.Utc;
	++CampaignState.AdversaryMissionsLaunched;

	const FStrategicDashboardSnapshot Snapshot = BuildStrategicDashboard();
	const FStrategicContactView* Projected = Snapshot.Contacts.FindByPredicate(
		[](const FStrategicContactView& View)
		{
			return View.PlanId == FName(TEXT("plan.cinder-lattice"))
				&& View.PlanStage == 4
				&& View.bHasCoalitionCounterplay;
		});
	return Snapshot.bSucceeded && Projected != nullptr
		&& Projected->EscapeStrainMembers.Num() == 1
		&& Projected->ThwartRecoveryMembers.Num() == 1;
}
#endif

FStrategicCommandResult UUEGTGameInstance::StartManufacturing(const FStartManufacturingCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SetManufacturingStaff(const FSetManufacturingStaffCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::AdjustManufacturingUnits(const FAdjustManufacturingUnitsCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::CancelManufacturing(const FCancelManufacturingCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SellInventory(const FSellInventoryCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::DispatchMutualAidConvoy(
	const FDispatchMutualAidConvoyCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::RetuneMutualAidConvoy(
	const FRetuneMutualAidConvoyCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(
			CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::CommissionMutualAidSignalEscort(
	const FCommissionMutualAidSignalEscortCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(
			CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::PrioritizeMutualAidConvoy(
	const FPrioritizeMutualAidConvoyCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::StandDownMutualAidConvoy(
	const FStandDownMutualAidConvoyCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::DivertMutualAidConvoy(
	const FDivertMutualAidConvoyCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(
			CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ConfigureMutualAidRelayWaypoint(
	const FConfigureMutualAidRelayWaypointCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(
			CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ConfigureMutualAidBalancedHandoff(
	const FConfigureMutualAidBalancedHandoffCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(
			CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SetSignalWatchStaff(
	const FSetSignalWatchStaffCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SetWorksCadreStaff(
	const FSetWorksCadreStaffCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SetWorksCadreCharter(
	const FSetWorksCadreCharterCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::StartFacilityConstruction(const FStartFacilityConstructionCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::CancelFacilityConstruction(const FCancelFacilityConstructionCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::DismantleFacility(const FDismantleFacilityCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ApplyFacilityDamage(const FApplyFacilityDamageCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::StartFacilityRepair(const FStartFacilityRepairCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::CancelFacilityRepair(const FCancelFacilityRepairCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::RecruitPersonnel(const FRecruitPersonnelCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::TransferPersonnel(const FTransferPersonnelCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::DismissPersonnel(const FDismissPersonnelCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ApplyPersonnelDamage(const FApplyPersonnelDamageCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SelectPersonnelRecoveryPlan(
	const FSelectPersonnelRecoveryPlanCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::BeginPersonnelStewardship(
	const FBeginPersonnelStewardshipCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::BeginPersonnelTraining(const FBeginPersonnelTrainingCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SelectPersonnelDoctrine(const FSelectPersonnelDoctrineCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SetPersonnelEquipment(const FSetPersonnelEquipmentCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::AcquireCraft(const FAcquireCraftCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::TransferCraft(const FTransferCraftCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::AssignCraftPilot(const FAssignCraftPilotCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SetCraftEquipment(const FSetCraftEquipmentCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::BeginCraftService(const FBeginCraftServiceCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::CancelCraftService(const FCancelCraftServiceCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::RearmCraft(const FRearmCraftCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::LaunchCraft(const FLaunchCraftCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::RecoverCraft(const FRecoverCraftCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SetCraftAgents(const FSetCraftAgentsCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SetCraftCargo(const FSetCraftCargoCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ResolveCraftSalvage(const FResolveCraftSalvageCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::DeployCraftToSite(const FDeployCraftToSiteCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ResolveTacticalOperation(const FResolveTacticalOperationCommand& Command)
{
	if (!bCampaignActive || !bContentReady)
	{
		return MakeUnavailableCommandResult();
	}
	const FCampaignState Before = CampaignState;
	FStrategicCommandResult Result = FStrategicCommandService::Execute(
		CampaignState, LoadedRules, SimulationConfig, Command);
	if (Result.bAccepted)
	{
		LastTacticalDebrief = FTacticalPresentationService::BuildDebrief(
			Before, CampaignState, LoadedRules, Result, Command.OperationId);
	}
	return Result;
}

FStrategicCommandResult UUEGTGameInstance::GenerateTacticalBattle(const FGenerateTacticalBattleCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ConfirmTacticalDeployment(const FConfirmTacticalDeploymentCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::MoveTacticalUnit(const FMoveTacticalUnitCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ChangeTacticalStance(const FChangeTacticalStanceCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::SetTacticalDoor(const FSetTacticalDoorCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::AttackTacticalUnit(const FAttackTacticalUnitCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::AttackTacticalTerrain(const FAttackTacticalTerrainCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ProjectTacticalSignal(const FProjectTacticalSignalCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ReloadTacticalWeapon(const FReloadTacticalWeaponCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::EjectTacticalMagazine(const FEjectTacticalMagazineCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::DeployTacticalDevice(const FDeployTacticalDeviceCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::InteractTacticalObjective(const FInteractTacticalObjectiveCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ExtractTacticalUnit(const FExtractTacticalUnitCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::EndTacticalTurn(const FEndTacticalTurnCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::RunTacticalAiTurn(const FRunTacticalAiTurnCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FTacticalAiDecision UUEGTGameInstance::ChooseTacticalAiAction(const FGuid BattleId, const FGuid UnitId) const
{
	FTacticalAiDecision Result;
	if (!bCampaignActive || !bContentReady)
	{
		FTacticalAiDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("campaign_unavailable");
		Diagnostic.Message = TEXT("Tactical AI queries require loaded content and an active campaign.");
		return Result;
	}
	const FTacticalBattleState* Battle = CampaignState.TacticalBattles.FindByPredicate(
		[&BattleId](const FTacticalBattleState& Entry) { return Entry.BattleId == BattleId; });
	if (Battle == nullptr)
	{
		FTacticalAiDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("unknown_tactical_battle");
		Diagnostic.Message = TEXT("Tactical AI query references an unknown battlefield.");
		return Result;
	}
	return FTacticalAiService::ChooseAction(*Battle, CampaignState, LoadedRules, UnitId);
}

FTacticalPathResult UUEGTGameInstance::FindTacticalPath(
	const FGuid BattleId,
	const FGuid UnitId,
	const int32 DestinationX,
	const int32 DestinationY,
	const int32 DestinationZ) const
{
	FTacticalPathResult Result;
	if (!bCampaignActive || !bContentReady)
	{
		FTacticalNavigationDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("campaign_unavailable");
		Diagnostic.Message = TEXT("Tactical path queries require loaded content and an active campaign.");
		return Result;
	}
	const FTacticalBattleState* Battle = CampaignState.TacticalBattles.FindByPredicate(
		[&BattleId](const FTacticalBattleState& Entry) { return Entry.BattleId == BattleId; });
	if (Battle == nullptr)
	{
		FTacticalNavigationDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("unknown_tactical_battle");
		Diagnostic.Message = TEXT("Tactical path query references an unknown battlefield.");
		return Result;
	}
	return FTacticalNavigationService::FindPath(*Battle, LoadedRules, UnitId, DestinationX, DestinationY, DestinationZ);
}

FTacticalVisibilityResult UUEGTGameInstance::ComputeTacticalVisibility(const FGuid BattleId) const
{
	FTacticalVisibilityResult Result;
	if (!bCampaignActive || !bContentReady)
	{
		FTacticalNavigationDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("campaign_unavailable");
		Diagnostic.Message = TEXT("Tactical visibility queries require loaded content and an active campaign.");
		return Result;
	}
	const FTacticalBattleState* Battle = CampaignState.TacticalBattles.FindByPredicate(
		[&BattleId](const FTacticalBattleState& Entry) { return Entry.BattleId == BattleId; });
	if (Battle == nullptr)
	{
		FTacticalNavigationDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("unknown_tactical_battle");
		Diagnostic.Message = TEXT("Tactical visibility query references an unknown battlefield.");
		return Result;
	}
	return FTacticalNavigationService::ComputePlayerVisibility(*Battle, LoadedRules);
}

TArray<FGuid> UUEGTGameInstance::GetTacticalBattleIds() const
{
	TArray<FGuid> Result;
	Result.Reserve(CampaignState.TacticalBattles.Num());
	for (const FTacticalBattleState& Battle : CampaignState.TacticalBattles)
	{
		Result.Add(Battle.BattleId);
	}
	return Result;
}

FTacticalHudSnapshot UUEGTGameInstance::BuildTacticalHudSnapshot(
	const FGuid BattleId,
	const FTacticalHudQuery& Query) const
{
	FTacticalHudSnapshot Result;
	Result.BattleId = BattleId;
	if (!bCampaignActive || !bContentReady)
	{
		FTacticalPresentationDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("campaign_unavailable");
		Diagnostic.Message = TEXT("Tactical HUD snapshots require loaded content and an active campaign.");
		return Result;
	}
	const FTacticalBattleState* Battle = CampaignState.TacticalBattles.FindByPredicate(
		[&BattleId](const FTacticalBattleState& Entry) { return Entry.BattleId == BattleId; });
	if (Battle == nullptr)
	{
		FTacticalPresentationDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("unknown_tactical_battle");
		Diagnostic.Message = TEXT("Tactical HUD snapshot references an unknown battlefield.");
		return Result;
	}
	return FTacticalPresentationService::BuildHudSnapshot(*Battle, CampaignState, LoadedRules, Query);
}

FTacticalAttackPreview UUEGTGameInstance::PreviewTacticalUnitAttack(
	const FGuid BattleId,
	const FGuid AttackerUnitId,
	const FGuid TargetUnitId,
	const FName WeaponItemId,
	const ETacticalFireMode FireMode) const
{
	FTacticalAttackPreview Result;
	if (!bCampaignActive || !bContentReady)
	{
		FTacticalCombatDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("campaign_unavailable");
		Diagnostic.Message = TEXT("Tactical attack previews require loaded content and an active campaign.");
		return Result;
	}
	const FTacticalBattleState* Battle = CampaignState.TacticalBattles.FindByPredicate(
		[&BattleId](const FTacticalBattleState& Entry) { return Entry.BattleId == BattleId; });
	if (Battle == nullptr)
	{
		FTacticalCombatDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("unknown_tactical_battle");
		Diagnostic.Message = TEXT("Tactical attack preview references an unknown battlefield.");
		return Result;
	}
	return FTacticalCombatService::PreviewUnitAttack(
		*Battle, CampaignState, LoadedRules, AttackerUnitId, TargetUnitId, WeaponItemId, FireMode);
}

FTacticalAttackPreview UUEGTGameInstance::PreviewTacticalTerrainAttack(
	const FGuid BattleId,
	const FGuid AttackerUnitId,
	const int32 TargetX,
	const int32 TargetY,
	const FName WeaponItemId,
	const ETacticalFireMode FireMode,
	const int32 TargetZ) const
{
	FTacticalAttackPreview Result;
	if (!bCampaignActive || !bContentReady)
	{
		FTacticalCombatDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("campaign_unavailable");
		Diagnostic.Message = TEXT("Tactical attack previews require loaded content and an active campaign.");
		return Result;
	}
	const FTacticalBattleState* Battle = CampaignState.TacticalBattles.FindByPredicate(
		[&BattleId](const FTacticalBattleState& Entry) { return Entry.BattleId == BattleId; });
	if (Battle == nullptr)
	{
		FTacticalCombatDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("unknown_tactical_battle");
		Diagnostic.Message = TEXT("Tactical terrain attack preview references an unknown battlefield.");
		return Result;
	}
	return FTacticalCombatService::PreviewTerrainAttack(
		*Battle, CampaignState, LoadedRules, AttackerUnitId, TargetX, TargetY, WeaponItemId, FireMode, TargetZ);
}

FTacticalSignalPreview UUEGTGameInstance::PreviewTacticalSignal(
	const FGuid BattleId,
	const FGuid AttackerUnitId,
	const FGuid TargetUnitId,
	const FName ProjectorItemId) const
{
	FTacticalSignalPreview Result;
	if (!bCampaignActive || !bContentReady)
	{
		FTacticalCombatDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("campaign_unavailable");
		Diagnostic.Message = TEXT("Tactical signal previews require loaded content and an active campaign.");
		return Result;
	}
	const FTacticalBattleState* Battle = CampaignState.TacticalBattles.FindByPredicate(
		[&BattleId](const FTacticalBattleState& Entry) { return Entry.BattleId == BattleId; });
	if (Battle == nullptr)
	{
		FTacticalCombatDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = TEXT("unknown_tactical_battle");
		Diagnostic.Message = TEXT("Tactical signal preview references an unknown battlefield.");
		return Result;
	}
	return FTacticalCombatService::PreviewSignalProjection(
		*Battle, CampaignState, LoadedRules, AttackerUnitId, TargetUnitId, ProjectorItemId);
}

FStrategicCommandResult UUEGTGameInstance::CreateStrategicContact(const FCreateStrategicContactCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::DispatchCraft(const FDispatchCraftCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::WithdrawInterception(
	const FWithdrawInterceptionCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ResolveInterceptionRound(const FResolveInterceptionRoundCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::ResolveBaseAssault(const FResolveBaseAssaultCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, SimulationConfig, Command)
		: MakeUnavailableCommandResult();
}

FStrategicCommandResult UUEGTGameInstance::DeployBaseDefenseOperation(
	const FDeployBaseDefenseOperationCommand& Command)
{
	return bCampaignActive && bContentReady
		? FStrategicCommandService::Execute(CampaignState, LoadedRules, Command)
		: MakeUnavailableCommandResult();
}

FCampaignSaveStoreResult UUEGTGameInstance::SaveCampaign(const FString& SlotName)
{
	using namespace UEGTGameInstancePrivate;

	FCampaignSaveStoreResult Result;
	if (!bCampaignActive || !bContentReady)
	{
		AddSaveError(Result, TEXT("no_active_campaign"), TEXT("A loaded campaign and valid content catalog are required before saving."));
		return Result;
	}

	const FCampaignSaveEnvelope Envelope = FCampaignSaveCodec::CreateNew(
		CampaignState,
		LoadedContentVersions,
		GetProjectBuildVersion(),
		CampaignCreatedUtc,
		ActiveCampaignId);
	return FCampaignSaveStore::Save(GetCampaignSaveDirectory(), SlotName, Envelope, FDateTime::UtcNow());
}

FCampaignSaveStoreResult UUEGTGameInstance::LoadCampaign(const FString& SlotName)
{
	using namespace UEGTGameInstancePrivate;

	FCampaignSaveStoreResult Result;
	if (!bContentReady)
	{
		AddSaveError(Result, TEXT("content_not_ready"), TEXT("A valid content catalog is required before loading a campaign."));
		return Result;
	}

	Result = FCampaignSaveStore::Load(GetCampaignSaveDirectory(), SlotName, LoadedContentVersions);
	if (Result.bSucceeded)
	{
		TArray<FStrategicCommandDiagnostic> LayoutDiagnostics;
		if (!FStrategicCommandService::UpgradeLegacyFacilityLayouts(Result.Envelope.State, LoadedRules, SimulationConfig, LayoutDiagnostics))
		{
			Result.bSucceeded = false;
			for (const FStrategicCommandDiagnostic& LayoutDiagnostic : LayoutDiagnostics)
			{
				FCampaignSaveDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
				Diagnostic.Severity = ECampaignSaveDiagnosticSeverity::Error;
				Diagnostic.Code = LayoutDiagnostic.Code;
				Diagnostic.Message = LayoutDiagnostic.Message;
			}
			return Result;
		}
		CampaignState = Result.Envelope.State;
		ActiveCampaignId = Result.Envelope.Header.CampaignId;
		CampaignCreatedUtc = Result.Envelope.Header.CreatedUtc;
		LastTacticalDebrief = FTacticalDebriefView();
		bCampaignActive = true;
	}
	return Result;
}

FCampaignSaveSlotListResult UUEGTGameInstance::ListCampaignSaves() const
{
	return bContentReady
		? FCampaignSaveStore::List(GetCampaignSaveDirectory(), LoadedContentVersions)
		: FCampaignSaveStore::List(GetCampaignSaveDirectory());
}

FString UUEGTGameInstance::GetProjectBuildVersion() const
{
	FString Version;
	if (GConfig != nullptr)
	{
		GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
	}
	return Version.TrimStartAndEnd().IsEmpty() ? TEXT("development") : Version;
}

FString UUEGTGameInstance::GetCampaignSaveDirectory() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"));
}

FStrategicCommandResult UUEGTGameInstance::MakeUnavailableCommandResult() const
{
	FStrategicCommandResult Result;
	FStrategicCommandDiagnostic& Diagnostic = Result.Diagnostics.AddDefaulted_GetRef();
	Diagnostic.Code = TEXT("campaign_unavailable");
	Diagnostic.Message = TEXT("A loaded campaign and valid content catalog are required for strategic commands.");
	return Result;
}
