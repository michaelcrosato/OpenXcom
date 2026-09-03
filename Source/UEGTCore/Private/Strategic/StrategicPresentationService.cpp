// Copyright 2026 UEGT contributors. MIT License.

#include "Strategic/StrategicPresentationService.h"

namespace StrategicPresentationPrivate
{
	FString HumanizeId(const FName Id)
	{
		FString Text = Id.ToString();
		int32 Separator = INDEX_NONE;
		if (Text.FindLastChar(TEXT('.'), Separator))
		{
			Text.RightChopInline(Separator + 1, EAllowShrinking::No);
		}
		Text.ReplaceInline(TEXT("-"), TEXT(" "));
		Text.ReplaceInline(TEXT("_"), TEXT(" "));
		bool bCapitalize = true;
		for (TCHAR& Character : Text)
		{
			if (FChar::IsWhitespace(Character))
			{
				bCapitalize = true;
			}
			else if (bCapitalize)
			{
				Character = FChar::ToUpper(Character);
				bCapitalize = false;
			}
		}
		return Text.IsEmpty() ? Id.ToString() : Text;
	}

	FString RuleName(const FString& DisplayName, const FName Id)
	{
		return DisplayName.TrimStartAndEnd().IsEmpty() ? HumanizeId(Id) : DisplayName;
	}

	void BuildCommendationViews(
		const TArray<FName>& SourceIds,
		const FResolvedRuleSet& Rules,
		TArray<FStrategicPersonnelCommendationView>& OutViews)
	{
		TArray<FName> CommendationIds = SourceIds;
		CommendationIds.Sort(FNameLexicalLess());
		for (const FName CommendationId : CommendationIds)
		{
			FStrategicPersonnelCommendationView& View = OutViews.AddDefaulted_GetRef();
			View.CommendationId = CommendationId;
			if (const FPersonnelCommendationRule* Commendation =
				Rules.PersonnelCommendations.Find(CommendationId))
			{
				View.DisplayName = RuleName(Commendation->DisplayName, CommendationId);
				View.Summary = Commendation->Summary;
			}
			else
			{
				View.DisplayName = HumanizeId(CommendationId);
			}
		}
	}

	ERegionalSupportTier RegionalSupportTier(const int32 Support)
	{
		if (Support < 15)
		{
			return ERegionalSupportTier::Suspended;
		}
		if (Support < 40)
		{
			return ERegionalSupportTier::Strained;
		}
		if (Support < 75)
		{
			return ERegionalSupportTier::Committed;
		}
		return ERegionalSupportTier::Allied;
	}

	FString PersonnelStatus(const EPersonnelStatus Status)
	{
		switch (Status)
		{
		case EPersonnelStatus::Available: return TEXT("Available");
		case EPersonnelStatus::Recovering: return TEXT("Recovering");
		case EPersonnelStatus::Training: return TEXT("Training");
		case EPersonnelStatus::Deployed: return TEXT("Deployed");
		case EPersonnelStatus::Stewarding: return TEXT("Stewarding");
		default: return TEXT("Unknown");
		}
	}

	FString CraftStatus(const ECraftStatus Status)
	{
		switch (Status)
		{
		case ECraftStatus::Grounded: return TEXT("Grounded");
		case ECraftStatus::Servicing: return TEXT("Servicing");
		case ECraftStatus::Airborne: return TEXT("Airborne");
		case ECraftStatus::Intercepting: return TEXT("Intercepting");
		case ECraftStatus::Returning: return TEXT("Returning");
		case ECraftStatus::Deploying: return TEXT("Deploying");
		case ECraftStatus::OnSite: return TEXT("On site");
		default: return TEXT("Unknown");
		}
	}

	FString ContactStatus(const EStrategicContactStatus Status)
	{
		switch (Status)
		{
		case EStrategicContactStatus::Detected: return TEXT("Detected");
		case EStrategicContactStatus::Engaged: return TEXT("Engaged");
		case EStrategicContactStatus::Hidden: return TEXT("Hidden");
		default: return TEXT("Unknown");
		}
	}

	FString DurationDetail(const int64 Seconds)
	{
		if (Seconds <= 0)
		{
			return TEXT("Ready");
		}
		const int64 Hours = Seconds / 3600 + (Seconds % 3600 == 0 ? 0 : 1);
		if (Hours < 48)
		{
			return FString::Printf(TEXT("%lld h remaining"), Hours);
		}
		return FString::Printf(TEXT("%lld d %lld h remaining"), Hours / 24, Hours % 24);
	}

	const FStrategicBaseState* FindBase(const FCampaignState& Campaign, const FGuid BaseId)
	{
		return Campaign.Bases.FindByPredicate(
			[BaseId](const FStrategicBaseState& Base) { return Base.BaseId == BaseId; });
	}

	const FStrategicContactState* FindContact(const FCampaignState& Campaign, const FGuid ContactId)
	{
		return Campaign.StrategicContacts.FindByPredicate(
			[ContactId](const FStrategicContactState& Contact) { return Contact.ContactId == ContactId; });
	}

	const FStrategicSiteState* FindSite(const FCampaignState& Campaign, const FGuid SiteId)
	{
		return Campaign.StrategicSites.FindByPredicate(
			[SiteId](const FStrategicSiteState& Site) { return Site.SiteId == SiteId; });
	}

	TArray<FName> OperationalFacilities(const FStrategicBaseState& Base, const FResolvedRuleSet& Rules)
	{
		TArray<FName> Result;
		if (!Base.Facilities.IsEmpty())
		{
			Result.Reserve(Base.Facilities.Num());
			for (const FBaseFacilityState& Facility : Base.Facilities)
			{
				const FFacilityRule* Rule = Rules.Facilities.Find(Facility.FacilityId);
				if (Rule != nullptr && Rule->MaxIntegrity > 0
					&& Facility.Damage >= 0 && Facility.Damage < Rule->MaxIntegrity)
				{
					Result.Add(Facility.FacilityId);
				}
			}
		}
		else
		{
			Result = Base.BuiltFacilities;
		}
		return Result;
	}

	bool HasRequirements(const TArray<FName>& Requirements, const FCampaignState& Campaign)
	{
		return !Requirements.ContainsByPredicate(
			[&Campaign](const FName Requirement) { return !Campaign.CompletedResearch.Contains(Requirement); });
	}

	bool IsPersonnelEquipment(const FItemRule& Item)
	{
		return Item.Category == FName(TEXT("sensor"))
			|| Item.Category == FName(TEXT("armor"))
			|| Item.Category == FName(TEXT("weapon"))
			|| Item.Category == FName(TEXT("ammunition"))
			|| Item.Category == FName(TEXT("medical"))
			|| Item.Category == FName(TEXT("device"));
	}

	bool RectanglesOverlap(
		const int32 AX,
		const int32 AY,
		const int32 AWidth,
		const int32 AHeight,
		const int32 BX,
		const int32 BY,
		const int32 BWidth,
		const int32 BHeight)
	{
		const int64 ARight = static_cast<int64>(AX) + AWidth;
		const int64 ABottom = static_cast<int64>(AY) + AHeight;
		const int64 BRight = static_cast<int64>(BX) + BWidth;
		const int64 BBottom = static_cast<int64>(BY) + BHeight;
		return static_cast<int64>(AX) < BRight && ARight > BX
			&& static_cast<int64>(AY) < BBottom && ABottom > BY;
	}

	bool RectanglesAreAdjacent(
		const int32 AX,
		const int32 AY,
		const int32 AWidth,
		const int32 AHeight,
		const int32 BX,
		const int32 BY,
		const int32 BWidth,
		const int32 BHeight)
	{
		const int64 ARight = static_cast<int64>(AX) + AWidth;
		const int64 ABottom = static_cast<int64>(AY) + AHeight;
		const int64 BRight = static_cast<int64>(BX) + BWidth;
		const int64 BBottom = static_cast<int64>(BY) + BHeight;
		const bool bVertical = (ARight == BX || BRight == AX)
			&& static_cast<int64>(AY) < BBottom && ABottom > BY;
		const bool bHorizontal = (ABottom == BY || BBottom == AY)
			&& static_cast<int64>(AX) < BRight && ARight > BX;
		return bVertical || bHorizontal;
	}

	bool CanPlaceFacility(
		const FStrategicBaseState& Base,
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FFacilityRule& Candidate,
		const int32 X,
		const int32 Y)
	{
		if (X < 0 || Y < 0 || Candidate.GridWidth <= 0 || Candidate.GridHeight <= 0
			|| static_cast<int64>(X) + Candidate.GridWidth > Config.BaseGridWidth
			|| static_cast<int64>(Y) + Candidate.GridHeight > Config.BaseGridHeight)
		{
			return false;
		}
		bool bAdjacent = Base.Facilities.IsEmpty();
		for (const FBaseFacilityState& Existing : Base.Facilities)
		{
			const FFacilityRule* ExistingRule = Rules.Facilities.Find(Existing.FacilityId);
			if (ExistingRule == nullptr
				|| RectanglesOverlap(X, Y, Candidate.GridWidth, Candidate.GridHeight,
					Existing.GridX, Existing.GridY, ExistingRule->GridWidth, ExistingRule->GridHeight))
			{
				return false;
			}
			bAdjacent |= RectanglesAreAdjacent(X, Y, Candidate.GridWidth, Candidate.GridHeight,
				Existing.GridX, Existing.GridY, ExistingRule->GridWidth, ExistingRule->GridHeight);
		}
		for (const FFacilityConstructionProjectState& Existing : Campaign.FacilityConstructionProjects)
		{
			if (Existing.BaseId != Base.BaseId)
			{
				continue;
			}
			const FFacilityRule* ExistingRule = Rules.Facilities.Find(Existing.FacilityId);
			if (ExistingRule == nullptr
				|| RectanglesOverlap(X, Y, Candidate.GridWidth, Candidate.GridHeight,
					Existing.GridX, Existing.GridY, ExistingRule->GridWidth, ExistingRule->GridHeight))
			{
				return false;
			}
			bAdjacent |= RectanglesAreAdjacent(X, Y, Candidate.GridWidth, Candidate.GridHeight,
				Existing.GridX, Existing.GridY, ExistingRule->GridWidth, ExistingRule->GridHeight);
		}
		return bAdjacent;
	}

	TArray<FIntPoint> FindFacilityPlacements(
		const FStrategicBaseState& Base,
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		const FStrategicSimulationConfig& Config,
		const FFacilityRule& Candidate)
	{
		TArray<FIntPoint> Placements;
		if (Candidate.GridWidth <= 0 || Candidate.GridHeight <= 0
			|| Candidate.GridWidth > Config.BaseGridWidth
			|| Candidate.GridHeight > Config.BaseGridHeight)
		{
			return Placements;
		}
		const int32 MaximumX = Config.BaseGridWidth - Candidate.GridWidth;
		const int32 MaximumY = Config.BaseGridHeight - Candidate.GridHeight;
		for (int32 Y = 0; Y <= MaximumY; ++Y)
		{
			for (int32 X = 0; X <= MaximumX; ++X)
			{
				if (CanPlaceFacility(Base, Campaign, Rules, Config, Candidate, X, Y))
				{
					Placements.Emplace(X, Y);
				}
			}
		}
		return Placements;
	}

	void SetUnavailable(FStrategicActionOptionView& Option, const FName Code, FString Message)
	{
		Option.bAvailable = false;
		Option.UnavailableReasonCode = Code;
		Option.UnavailableReason = MoveTemp(Message);
	}

	void FinishOption(
		FStrategicActionOptionView& Option,
		const FCampaignState& Campaign,
		const bool bHasBase,
		const bool bAdditionalCondition,
		const FName AdditionalCode,
		const FString& AdditionalMessage)
	{
		if (Campaign.Outcome != ECampaignOutcome::Ongoing)
		{
			SetUnavailable(Option, TEXT("campaign_concluded"), TEXT("The campaign has concluded."));
		}
		else if (!bHasBase)
		{
			SetUnavailable(Option, TEXT("base_required"), TEXT("Establish a base before ordering this program."));
		}
		else if (!Option.bUnlocked)
		{
			SetUnavailable(Option, TEXT("research_required"), TEXT("Required research is incomplete."));
		}
		else if (!bAdditionalCondition)
		{
			SetUnavailable(Option, AdditionalCode, AdditionalMessage);
		}
		else if (!Option.bAffordable)
		{
			SetUnavailable(Option, TEXT("insufficient_funds"), TEXT("Current funds do not cover this order."));
		}
		else
		{
			Option.bAvailable = true;
			Option.UnavailableReasonCode = NAME_None;
			Option.UnavailableReason.Reset();
		}
	}

	int32 SaturatingNonNegativeAdd(const int32 Current, const int32 Contribution)
	{
		const int64 Sum = static_cast<int64>(Current) + static_cast<int64>(Contribution);
		return static_cast<int32>(FMath::Clamp<int64>(Sum, 0, MAX_int32));
	}

	int64 SaturatingAdd(const int64 Current, const int64 Contribution)
	{
		if (Contribution > 0 && Current > MAX_int64 - Contribution)
		{
			return MAX_int64;
		}
		if (Contribution < 0 && Current < MIN_int64 - Contribution)
		{
			return MIN_int64;
		}
		return Current + Contribution;
	}

	int32 PersonnelCountForCategory(
		const FCampaignState& Campaign,
		const FResolvedRuleSet& Rules,
		const FGuid BaseId,
		const EPersonnelRoleCategory Category)
	{
		int32 Count = 0;
		for (const FPersonnelState& Person : Campaign.Personnel)
		{
			const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Person.RoleId);
			Count = SaturatingNonNegativeAdd(
				Count,
				Person.BaseId == BaseId && Role != nullptr && Role->Category == Category ? 1 : 0);
		}
		for (const FRecruitmentOrderState& Order : Campaign.RecruitmentOrders)
		{
			const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Order.RoleId);
			Count = SaturatingNonNegativeAdd(
				Count,
				Order.BaseId == BaseId && Role != nullptr && Role->Category == Category ? 1 : 0);
		}
		return Count;
	}

	int32 CraftCapacity(const FStrategicBaseState& Base, const FResolvedRuleSet& Rules)
	{
		int64 Capacity = 0;
		if (!Base.Facilities.IsEmpty())
		{
		for (const FBaseFacilityState& Installed : Base.Facilities)
		{
				if (const FFacilityRule* Facility = Rules.Facilities.Find(Installed.FacilityId))
				{
					Capacity += Facility->ScaleEffectByIntegrity(
						FMath::Max(0, Facility->CraftCapacity), Installed.Damage);
				}
			}
		}
		else
		{
			for (const FName FacilityId : Base.BuiltFacilities)
			{
				if (const FFacilityRule* Facility = Rules.Facilities.Find(FacilityId))
				{
					Capacity += FMath::Max(0, Facility->CraftCapacity);
				}
			}
		}
		return static_cast<int32>(FMath::Min<int64>(Capacity, MAX_int32));
	}

	int32 CraftOccupied(const FCampaignState& Campaign, const FGuid BaseId)
	{
		int32 Count = 0;
		for (const FCraftState& Craft : Campaign.Craft)
		{
			Count = SaturatingNonNegativeAdd(Count, Craft.BaseId == BaseId ? 1 : 0);
		}
		for (const FCraftAcquisitionOrderState& Order : Campaign.CraftAcquisitionOrders)
		{
			Count = SaturatingNonNegativeAdd(Count, Order.BaseId == BaseId ? 1 : 0);
		}
		return Count;
	}

	float Progress(const int64 Completed, const int64 Total)
	{
		return Total > 0
			? FMath::Clamp(static_cast<float>(static_cast<double>(Completed) / static_cast<double>(Total)), 0.0f, 1.0f)
			: 0.0f;
	}

	int64 SafeProduct(const int64 Left, const int64 Right)
	{
		return Left <= 0 || Right <= 0 ? 0
			: Left > MAX_int64 / Right ? MAX_int64
			: Left * Right;
	}

	int64 NonNegativeDifference(const int64 Left, const int64 Right)
	{
		if (Left <= Right)
		{
			return 0;
		}
		if (Right < 0 && Left > MAX_int64 + Right)
		{
			return MAX_int64;
		}
		return Left - Right;
	}

	int64 SaturatingSubtract(const int64 Left, const int64 Right)
	{
		if (Right > 0 && Left < MIN_int64 + Right)
		{
			return MIN_int64;
		}
		if (Right < 0 && Left > MAX_int64 + Right)
		{
			return MAX_int64;
		}
		return Left - Right;
	}

	int64 CeilScaledWorkSeconds(
		const int64 RemainingWork,
		const int32 AssignedStaff,
		const int32 RatePercent)
	{
		const int64 ScaledWork = SafeProduct(RemainingWork, 100);
		const int64 Throughput = SafeProduct(
			FMath::Max<int64>(0, AssignedStaff), FMath::Max<int64>(0, RatePercent));
		return Throughput > 0
			? ScaledWork / Throughput + (ScaledWork % Throughput == 0 ? 0 : 1)
			: 0;
	}

	bool CanApplyStorageDelta(const FStrategicBaseView* Base, const int64 Delta)
	{
		if (Base == nullptr || !Base->bStorageEnforced)
		{
			return true;
		}
		int64 ProjectedCommitted = Base->StorageCommitted;
		if ((Delta > 0 && ProjectedCommitted > MAX_int64 - Delta)
			|| (Delta < 0 && ProjectedCommitted < MIN_int64 - Delta))
		{
			return false;
		}
		ProjectedCommitted = FMath::Max<int64>(0, ProjectedCommitted + Delta);
		const int64 ProjectedOverflow = NonNegativeDifference(
			ProjectedCommitted, Base->StorageCapacity);
		return ProjectedOverflow <= Base->StorageOverflow;
	}

	FString StorageUnavailableReason(const FStrategicBaseView* Base, const int64 Delta)
	{
		if (Base == nullptr)
		{
			return TEXT("The production base is unavailable.");
		}
		const int64 Required = NonNegativeDifference(Delta, Base->StorageAvailable);
		return FString::Printf(TEXT("This change needs %lld more storage units; sell, equip, or relocate inventory first."),
			Required);
	}

	int64 ManufacturingInputStorage(const FItemRule& Product, const FResolvedRuleSet& Rules)
	{
		int64 Storage = 0;
		for (const FManufacturingInputRule& Input : Product.ManufactureInputs)
		{
			const FItemRule* InputRule = Rules.Items.Find(Input.ItemId);
			const int64 Contribution = InputRule != nullptr
				? SafeProduct(FMath::Max(0, InputRule->Mass), FMath::Max(0, Input.Quantity))
				: 0;
			Storage = Storage > MAX_int64 - Contribution ? MAX_int64 : Storage + Contribution;
		}
		return Storage;
	}

	int32 InventoryQuantity(const FStrategicBaseState* Base, const FName ItemId)
	{
		if (Base == nullptr)
		{
			return 0;
		}
		const FInventoryStack* Stack = Base->Inventory.FindByPredicate(
			[ItemId](const FInventoryStack& Entry) { return Entry.ItemId == ItemId; });
		return Stack != nullptr ? FMath::Max(0, Stack->Quantity) : 0;
	}

	FString MaterialSummary(const TArray<FStrategicMaterialRequirementView>& Requirements)
	{
		TArray<FString> Parts;
		Parts.Reserve(Requirements.Num());
		for (const FStrategicMaterialRequirementView& Requirement : Requirements)
		{
			Parts.Add(FString::Printf(TEXT("%d %s (%d stock)"), Requirement.PerUnitQuantity,
				*Requirement.DisplayName, Requirement.AvailableQuantity));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	FString RefundableMaterialSummary(const TArray<FStrategicMaterialRequirementView>& Requirements)
	{
		TArray<FString> Parts;
		for (const FStrategicMaterialRequirementView& Requirement : Requirements)
		{
			if (Requirement.RefundableQuantity > 0)
			{
				Parts.Add(FString::Printf(TEXT("%lld %s"), Requirement.RefundableQuantity,
					*Requirement.DisplayName));
			}
		}
		return FString::Join(Parts, TEXT(", "));
	}

	int64 ProportionalRefund(const int64 Cost, const int64 Remaining, const int64 Total)
	{
		return Total > 0 ? SafeProduct(FMath::Max<int64>(0, Cost), FMath::Clamp<int64>(Remaining, 0, Total)) / Total : 0;
	}

	bool GuidLess(const FGuid& Left, const FGuid& Right)
	{
		return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
	}

	bool IsPersonnelAssignedToCraft(const FCampaignState& Campaign, const FGuid PersonnelId)
	{
		return Campaign.Craft.ContainsByPredicate(
			[PersonnelId](const FCraftState& Craft)
			{
				return Craft.AssignedPilotId == PersonnelId || Craft.AssignedAgentIds.Contains(PersonnelId);
			});
	}

	void AddGlobeMarker(
		FStrategicDashboardSnapshot& Snapshot,
		const EStrategicGlobeMarkerType Type,
		const FGuid EntityId,
		const FString& DisplayName,
		const FString& Detail,
		const int32 Longitude,
		const int32 Latitude,
		const bool bUrgent)
	{
		FStrategicGlobeMarkerView& Marker = Snapshot.GlobeMarkers.AddDefaulted_GetRef();
		Marker.Type = Type;
		Marker.EntityId = EntityId;
		Marker.DisplayName = DisplayName;
		Marker.Detail = Detail;
		Marker.LongitudeMilliDegrees = Longitude;
		Marker.LatitudeMilliDegrees = Latitude;
		Marker.bUrgent = bUrgent;
	}
}

int32 FStrategicPresentationService::CalculateExpectedBaseDefenseDamage(
	const int32 Accuracy,
	const int32 Damage)
{
	if (Accuracy <= 0 || Damage <= 0)
	{
		return 0;
	}

	const int64 ExpectedDamage = (static_cast<int64>(Accuracy) * Damage + 50) / 100;
	return static_cast<int32>(FMath::Clamp<int64>(ExpectedDamage, 0, MAX_int32));
}

FStrategicDashboardSnapshot FStrategicPresentationService::BuildDashboard(
	const FCampaignState& Campaign,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config)
{
	using namespace StrategicPresentationPrivate;

	FStrategicDashboardSnapshot Snapshot;
	Snapshot.CampaignTimeUtc = Campaign.StrategicTime.Utc;
	Snapshot.Difficulty = Campaign.Difficulty;
	Snapshot.Outcome = Campaign.Outcome;
	Snapshot.OutcomeReasonId = Campaign.OutcomeReasonId;
	Snapshot.ExpectedCommandSequence = Campaign.CommandSequence;
	Snapshot.Funds = Campaign.Funds;
	Snapshot.MonthlyFunding = Campaign.MonthlyFunding;
	Snapshot.CampaignScore = Campaign.CampaignScore;
	Snapshot.AdversaryEscalationLevel = Campaign.AdversaryEscalationLevel;
	Snapshot.NextAdversaryMissionSeconds = Campaign.NextAdversaryMissionSeconds;
	Snapshot.AdversaryMissionsLaunched = Campaign.AdversaryMissionsLaunched;
	Snapshot.AdversaryMissionsEscaped = Campaign.AdversaryMissionsEscaped;
	Snapshot.AdversaryMissionsThwarted = Campaign.AdversaryMissionsThwarted;
	Snapshot.VictoryThwartedMissionTarget = Config.VictoryThwartedMissions;
	Snapshot.VictoryEscalationTarget = Config.VictoryMinimumEscalationLevel;
	Snapshot.RegionalCollapsePressureThreshold = Config.FailurePressureThreshold;
	Snapshot.bRequiresBase = Campaign.Bases.IsEmpty();
	const bool bRecoveryPlanRequired = Campaign.Personnel.ContainsByPredicate(
		[](const FPersonnelState& Person)
		{
			return Person.Status == EPersonnelStatus::Recovering
				&& Person.RecoveryPlan == EPersonnelRecoveryPlan::DecisionRequired;
		});
	Snapshot.bDecisionRequired = !Campaign.TacticalOperations.IsEmpty()
		|| !Campaign.BaseAssaults.IsEmpty() || bRecoveryPlanRequired;
	Snapshot.bCanAdvanceTime = Campaign.Outcome == ECampaignOutcome::Ongoing
		&& Campaign.TacticalOperations.IsEmpty()
		&& Campaign.BaseAssaults.IsEmpty()
		&& Campaign.TacticalBattles.IsEmpty()
		&& !bRecoveryPlanRequired;

	if (!Campaign.StrategicTime.IsUsable())
	{
		Snapshot.Diagnostics.Add(TEXT("Campaign time is not usable."));
		return Snapshot;
	}
	if (Config.BaseGridWidth <= 0
		|| Config.BaseGridWidth > FStrategicSimulationConfig::MaximumBaseGridDimension
		|| Config.BaseGridHeight <= 0
		|| Config.BaseGridHeight > FStrategicSimulationConfig::MaximumBaseGridDimension
		|| Config.MaxGeneralPersonnelPerBase <= 0
		|| Config.InterceptionAftershockMinutesPerThreat < 0
		|| Config.InterceptionAftershockMinutesPerThreat > 360
		|| Config.MutualAidConvoyTransitHours <= 0
		|| Config.MutualAidConvoyTransitHours > 8760)
	{
		Snapshot.Diagnostics.Add(TEXT("Strategic presentation requires valid base-grid, personnel-capacity, and logistics settings."));
		return Snapshot;
	}
	FAdversaryAdaptationProgress Adaptation;
	if (Config.VictoryThwartedMissions <= 0
		|| Config.VictoryMinimumEscalationLevel <= 0
		|| Config.VictoryMinimumEscalationLevel > Config.MaxAdversaryEscalation
		|| Config.FailurePressureThreshold <= 0 || Config.FailurePressureThreshold > 100
		|| Config.BaseDefenseGridOverchargeCostPerThreat <= 0
		|| Config.BaseDefenseGridOverchargeCostPerThreat > MAX_int64 / 10
		|| Config.BaseDefenseGridOverchargeAccuracyBonus < 0
		|| Config.BaseDefenseGridOverchargeAccuracyBonus > 100
		|| Config.BaseDefenseGridOverchargeDamagePercent < 100
		|| Config.BaseDefenseGridOverchargeDamagePercent > 400
		|| Config.CrisisMobilizationMinimumPressure <= 0
		|| Config.CrisisMobilizationMinimumPressure > 100
		|| Config.CrisisMobilizationSupportCost <= 0 || Config.CrisisMobilizationSupportCost > 100
		|| Config.CrisisMobilizationPressureReduction <= 0
		|| Config.CrisisMobilizationPressureReduction > Config.CrisisMobilizationMinimumPressure
		|| Config.ResilienceCharterMinimumSupport <= 0
		|| Config.ResilienceCharterMinimumSupport > 100
		|| Config.ResilienceCharterCost < 0
		|| Config.ResilienceCharterSupportCost <= 0
		|| Config.ResilienceCharterSupportCost > Config.ResilienceCharterMinimumSupport
		|| Config.ResilienceCharterFundingPercent < 25
		|| Config.ResilienceCharterFundingPercent > 100
		|| Config.ResilienceCharterMissionWeightPercent < 25
		|| Config.ResilienceCharterMissionWeightPercent > 100
		|| Config.ResilienceCharterEscapePressurePercent < 25
		|| Config.ResilienceCharterEscapePressurePercent > 100
		|| Config.HorizonCompactRequiredCharters < 2
		|| Config.HorizonCompactRequiredCharters > 100
		|| Config.HorizonCompactMinimumMemberSupport <= 0
		|| Config.HorizonCompactMinimumMemberSupport > 100
		|| Config.HorizonCompactCost < 0
		|| Config.HorizonCompactMemberSupportCost <= 0
		|| Config.HorizonCompactMemberSupportCost > Config.HorizonCompactMinimumMemberSupport
		|| Config.HorizonCompactFundingPercent < Config.ResilienceCharterFundingPercent
		|| Config.HorizonCompactFundingPercent > 100
		|| Config.HorizonCompactSharedEscapePressurePercent <= 0
		|| Config.HorizonCompactSharedEscapePressurePercent > 50
		|| Config.HorizonCompactWithdrawalSupportThreshold <= 0
		|| Config.HorizonCompactWithdrawalSupportThreshold
			>= Config.HorizonCompactRestorationMinimumSupport
		|| Config.HorizonCompactRestorationMinimumSupport
			> Config.HorizonCompactMinimumMemberSupport
		|| Config.HorizonCompactRestorationCost < 0
		|| Config.ReciprocalAidCost < 0
		|| Config.ReciprocalAidMinimumTargetPressure <= 0
		|| Config.ReciprocalAidMinimumTargetPressure >= 100
		|| Config.ReciprocalAidPressureTransfer <= 0
		|| Config.ReciprocalAidPressureTransfer > Config.ReciprocalAidMinimumTargetPressure
		|| Config.ReciprocalAidSupportTransfer <= 0
		|| Config.ReciprocalAidSupportTransfer > 100
		|| Config.HorizonCompactEmergencyVoteCost < 0
		|| Config.HorizonCompactEmergencyTargetSupportGain <= 0
		|| Config.HorizonCompactEmergencyTargetSupportGain > 100
		|| Config.HorizonCompactEmergencyTargetPressureReduction <= 0
		|| Config.HorizonCompactEmergencyTargetPressureReduction > 100
		|| Config.HorizonCompactEmergencyVoterSupportCost <= 0
		|| Config.HorizonCompactEmergencyVoterSupportCost
			> 100 - Config.HorizonCompactWithdrawalSupportThreshold
		|| Config.HorizonCompactEmergencyMaximumVoterPressure < 0
		|| Config.HorizonCompactEmergencyMaximumVoterPressure >= 100
		|| !FStrategicCommandService::GetAdversaryAdaptationProgress(Campaign, Config, Adaptation))
	{
		Snapshot.Diagnostics.Add(TEXT("Strategic presentation requires valid adversary adaptation, campaign-outcome, regional-policy, and base-defense economy settings."));
		return Snapshot;
	}
	Snapshot.AdversaryResolvedMissions = Adaptation.ResolvedMissions;
	Snapshot.ResolvedMissionsUntilNextEscalation = Adaptation.ResolvedMissionsUntilNextEscalation;
	Snapshot.bAtMaximumAdversaryEscalation = Adaptation.bAtMaximumEscalation;
	for (const FRegionalPressureState& Pressure : Campaign.RegionalPressure)
	{
		Snapshot.HighestRegionalPressure = FMath::Max(
			Snapshot.HighestRegionalPressure, Pressure.Pressure);
	}

	FRatifyHorizonCompactCommand CompactCommand;
	CompactCommand.ExpectedSequence = Campaign.CommandSequence;
	const FHorizonCompactEvaluation CompactEvaluation =
		FStrategicCommandService::EvaluateHorizonCompact(Campaign, Config, CompactCommand);
	Snapshot.HorizonCompact.bRatified = CompactEvaluation.bRatified;
	Snapshot.HorizonCompact.Cost = CompactEvaluation.Cost;
	Snapshot.HorizonCompact.RequiredCharters = CompactEvaluation.RequiredCharters;
	Snapshot.HorizonCompact.SignedCharters = CompactEvaluation.SignedCharters;
	Snapshot.HorizonCompact.MinimumMemberSupport = CompactEvaluation.MinimumMemberSupport;
	Snapshot.HorizonCompact.MemberSupportCost = CompactEvaluation.MemberSupportCost;
	Snapshot.HorizonCompact.FundingPercent = CompactEvaluation.FundingPercent;
	Snapshot.HorizonCompact.SharedEscapePressurePercent =
		CompactEvaluation.SharedEscapePressurePercent;
	Snapshot.HorizonCompact.WithdrawalSupportThreshold =
		CompactEvaluation.WithdrawalSupportThreshold;
	Snapshot.HorizonCompact.RestorationMinimumSupport =
		CompactEvaluation.RestorationMinimumSupport;
	Snapshot.HorizonCompact.CurrentMonthlyFunding = CompactEvaluation.CurrentMonthlyFunding;
	Snapshot.HorizonCompact.ProjectedMonthlyFunding = CompactEvaluation.ProjectedMonthlyFunding;
	Snapshot.HorizonCompact.MonthlyFundingDelta = CompactEvaluation.MonthlyFundingDelta;
	Snapshot.HorizonCompact.MemberRegionIds = CompactEvaluation.MemberRegionIds;
	Snapshot.HorizonCompact.ActiveMemberRegionIds = CompactEvaluation.ActiveMemberRegionIds;
	Snapshot.HorizonCompact.WithdrawnMemberRegionIds = CompactEvaluation.WithdrawnMemberRegionIds;
	Snapshot.HorizonCompact.bEnabled = CompactEvaluation.bAllowed;
	if (!CompactEvaluation.Diagnostics.IsEmpty())
	{
		Snapshot.HorizonCompact.UnavailableReasonCode = CompactEvaluation.Diagnostics[0].Code;
		Snapshot.HorizonCompact.UnavailableReason = CompactEvaluation.Diagnostics[0].Message;
	}
	for (const FName MemberRegionId : CompactEvaluation.ActiveMemberRegionIds)
	{
		FDeployReciprocalAidCommand AidCommand;
		AidCommand.ExpectedSequence = Campaign.CommandSequence;
		AidCommand.TargetRegionId = MemberRegionId;
		const FReciprocalAidEvaluation AidEvaluation =
			FStrategicCommandService::EvaluateReciprocalAid(Campaign, Config, AidCommand);
		FStrategicCoalitionAidView& AidView =
			Snapshot.HorizonCompact.AidOptions.AddDefaulted_GetRef();
		AidView.TargetRegionId = AidEvaluation.TargetRegionId;
		AidView.DonorRegionId = AidEvaluation.DonorRegionId;
		AidView.Cost = AidEvaluation.Cost;
		AidView.MinimumTargetPressure = AidEvaluation.MinimumTargetPressure;
		AidView.MaximumPressureTransfer = AidEvaluation.MaximumPressureTransfer;
		AidView.PressureTransfer = AidEvaluation.PressureTransfer;
		AidView.TargetCurrentPressure = AidEvaluation.TargetCurrentPressure;
		AidView.TargetProjectedPressure = AidEvaluation.TargetProjectedPressure;
		AidView.DonorCurrentPressure = AidEvaluation.DonorCurrentPressure;
		AidView.DonorProjectedPressure = AidEvaluation.DonorProjectedPressure;
		AidView.TargetSupportGain = AidEvaluation.TargetSupportGain;
		AidView.DonorSupportCost = AidEvaluation.DonorSupportCost;
		AidView.bDonorWouldWithdraw = AidEvaluation.bDonorWouldWithdraw;
		AidView.MonthlyFundingDelta = AidEvaluation.MonthlyFundingDelta;
		AidView.bEnabled = AidEvaluation.bAllowed;
		if (!AidEvaluation.Diagnostics.IsEmpty())
		{
			AidView.UnavailableReasonCode = AidEvaluation.Diagnostics[0].Code;
			AidView.UnavailableReason = AidEvaluation.Diagnostics[0].Message;
		}
	}
	Snapshot.HorizonCompact.AidOptions.Sort(
		[](const FStrategicCoalitionAidView& Left, const FStrategicCoalitionAidView& Right)
		{
			return Left.TargetRegionId.LexicalLess(Right.TargetRegionId);
		});

	Snapshot.ArchiveTotalCount = Rules.ArchiveEntries.Num();
	TSet<FName> UnlockedArchiveIds;
	for (const TPair<FName, FKnowledgeArchiveEntryRule>& Pair : Rules.ArchiveEntries)
	{
		if (HasRequirements(Pair.Value.RequiredResearch, Campaign))
		{
			UnlockedArchiveIds.Add(Pair.Key);
		}
	}
	Snapshot.ArchiveLockedCount = Snapshot.ArchiveTotalCount - UnlockedArchiveIds.Num();
	for (const TPair<FName, FKnowledgeArchiveEntryRule>& Pair : Rules.ArchiveEntries)
	{
		if (!UnlockedArchiveIds.Contains(Pair.Key))
		{
			continue;
		}
		FStrategicArchiveEntryView& View = Snapshot.ArchiveEntries.AddDefaulted_GetRef();
		View.EntryId = Pair.Key;
		View.CategoryId = Pair.Value.CategoryId;
		View.CategoryDisplayName = HumanizeId(Pair.Value.CategoryId);
		View.DisplayName = RuleName(Pair.Value.DisplayName, Pair.Key);
		View.Summary = Pair.Value.Summary;
		View.Body = Pair.Value.Body;
		View.SortOrder = Pair.Value.SortOrder;
		for (const FName RelatedEntryId : Pair.Value.RelatedEntryIds)
		{
			if (UnlockedArchiveIds.Contains(RelatedEntryId))
			{
				View.RelatedEntryIds.Add(RelatedEntryId);
			}
		}
		View.RelatedEntryIds.Sort(FNameLexicalLess());
	}
	Snapshot.ArchiveEntries.Sort([](
		const FStrategicArchiveEntryView& Left,
		const FStrategicArchiveEntryView& Right)
	{
		if (Left.CategoryId != Right.CategoryId)
		{
			return Left.CategoryId.LexicalLess(Right.CategoryId);
		}
		if (Left.SortOrder != Right.SortOrder)
		{
			return Left.SortOrder < Right.SortOrder;
		}
		return Left.EntryId.LexicalLess(Right.EntryId);
	});

	TMap<FName, int64> RegionLongitudeSums;
	TMap<FName, int64> RegionLatitudeSums;
	TMap<FName, int64> RegionCoordinateCounts;
	for (const TPair<FName, FAdversaryMissionRule>& Pair : Rules.AdversaryMissions)
	{
		RegionLongitudeSums.FindOrAdd(Pair.Value.TargetRegionId) +=
			static_cast<int64>(Pair.Value.DestinationLongitudeMilliDegrees);
		RegionLatitudeSums.FindOrAdd(Pair.Value.TargetRegionId) +=
			static_cast<int64>(Pair.Value.DestinationLatitudeMilliDegrees);
		++RegionCoordinateCounts.FindOrAdd(Pair.Value.TargetRegionId);
	}
	for (const FRegionalPressureState& Pressure : Campaign.RegionalPressure)
	{
		FStrategicRegionView& Region = Snapshot.Regions.AddDefaulted_GetRef();
		Region.RegionId = Pressure.RegionId;
		Region.DisplayName = HumanizeId(Pressure.RegionId);
		Region.Pressure = Pressure.Pressure;
		if (const FStrategicRegionRule* RegionRule = Rules.Regions.Find(Pressure.RegionId))
		{
			Region.DisplayName = RuleName(RegionRule->DisplayName, Pressure.RegionId);
			Region.LongitudeMilliDegrees = RegionRule->CenterLongitudeMilliDegrees;
			Region.LatitudeMilliDegrees = RegionRule->CenterLatitudeMilliDegrees;
		}
		else if (const int64* LongitudeSum = RegionLongitudeSums.Find(Pressure.RegionId))
		{
			const int64 Count = FMath::Max<int64>(1,
				RegionCoordinateCounts.FindRef(Pressure.RegionId));
			Region.LongitudeMilliDegrees = static_cast<int32>(*LongitudeSum / Count);
			Region.LatitudeMilliDegrees = static_cast<int32>(
				RegionLatitudeSums.FindRef(Pressure.RegionId) / Count);
		}
		const FRegionalMandateState* Mandate = Campaign.RegionalMandates.FindByPredicate(
			[&Pressure](const FRegionalMandateState& Entry) { return Entry.RegionId == Pressure.RegionId; });
		if (Mandate != nullptr)
		{
			Region.bHasMandate = true;
			Region.Support = Mandate->Support;
			Region.SupportTier = RegionalSupportTier(Mandate->Support);
			Region.BaselineMonthlyFunding = Mandate->BaselineMonthlyFunding;
			Region.CurrentMonthlyFunding = Mandate->CurrentMonthlyFunding;
			if (!FStrategicCommandService::CalculateRegionalFundingContribution(
					*Mandate, Config, Campaign.bHorizonCompactRatified,
					Region.ProjectedMonthlyFunding))
			{
				Region.ProjectedMonthlyFunding = Mandate->CurrentMonthlyFunding;
				Snapshot.Diagnostics.Add(FString::Printf(
					TEXT("Regional mandate '%s' has invalid projected funding."), *Pressure.RegionId.ToString()));
			}

			static constexpr ERegionalDiplomacyActionType ActionTypes[] = {
				ERegionalDiplomacyActionType::CivicRelief,
				ERegionalDiplomacyActionType::SecurityAccord,
				ERegionalDiplomacyActionType::CrisisMobilization
			};
			for (const ERegionalDiplomacyActionType ActionType : ActionTypes)
			{
				FRegionalDiplomacyCommand Command;
				Command.ExpectedSequence = Campaign.CommandSequence;
				Command.RegionId = Pressure.RegionId;
				Command.ActionType = ActionType;
				const FRegionalDiplomacyEvaluation Evaluation =
					FStrategicCommandService::EvaluateRegionalDiplomacy(Campaign, Config, Command);
				FStrategicRegionalActionView& Option = Region.ActionOptions.AddDefaulted_GetRef();
				Option.ActionType = ActionType;
				Option.Cost = Evaluation.Cost;
				Option.SupportDelta = Evaluation.SupportDelta;
				Option.PressureReduction = Evaluation.PressureReduction;
				Option.MinimumPressure = Evaluation.MinimumPressure;
				Option.bWouldWithdrawCompactMember =
					Evaluation.bWouldWithdrawCompactMember;
				Option.bEnabled = Evaluation.bAllowed;
				if (!Evaluation.Diagnostics.IsEmpty())
				{
					Option.UnavailableReasonCode = Evaluation.Diagnostics[0].Code;
					Option.UnavailableReason = Evaluation.Diagnostics[0].Message;
				}
			}

			FSignRegionalCharterCommand CharterCommand;
			CharterCommand.ExpectedSequence = Campaign.CommandSequence;
			CharterCommand.RegionId = Pressure.RegionId;
			const FRegionalCharterEvaluation CharterEvaluation =
				FStrategicCommandService::EvaluateRegionalCharter(Campaign, Config, CharterCommand);
			Region.ResilienceCharter.bSigned = CharterEvaluation.bSigned;
			Region.ResilienceCharter.Cost = CharterEvaluation.Cost;
			Region.ResilienceCharter.SupportCost = CharterEvaluation.SupportCost;
			Region.ResilienceCharter.MinimumSupport = CharterEvaluation.MinimumSupport;
			Region.ResilienceCharter.FundingPercent = CharterEvaluation.FundingPercent;
			Region.ResilienceCharter.MissionWeightPercent = CharterEvaluation.MissionWeightPercent;
			Region.ResilienceCharter.EscapePressurePercent = CharterEvaluation.EscapePressurePercent;
			Region.ResilienceCharter.ProjectedMonthlyFunding = CharterEvaluation.ProjectedMonthlyFunding;
			Region.ResilienceCharter.MonthlyFundingDelta = CharterEvaluation.MonthlyFundingDelta;
			Region.ResilienceCharter.bEnabled = CharterEvaluation.bAllowed;
			if (!CharterEvaluation.Diagnostics.IsEmpty())
			{
				Region.ResilienceCharter.UnavailableReasonCode = CharterEvaluation.Diagnostics[0].Code;
				Region.ResilienceCharter.UnavailableReason = CharterEvaluation.Diagnostics[0].Message;
			}

			FRestoreHorizonCompactMemberCommand RestorationCommand;
			RestorationCommand.ExpectedSequence = Campaign.CommandSequence;
			RestorationCommand.RegionId = Pressure.RegionId;
			const FHorizonCompactRestorationEvaluation RestorationEvaluation =
				FStrategicCommandService::EvaluateHorizonCompactRestoration(
					Campaign, Config, RestorationCommand);
			Region.HorizonCompactRestoration.bWithdrawn =
				RestorationEvaluation.bWithdrawn;
			Region.HorizonCompactRestoration.Cost = RestorationEvaluation.Cost;
			Region.HorizonCompactRestoration.CurrentSupport =
				RestorationEvaluation.CurrentSupport;
			Region.HorizonCompactRestoration.MinimumSupport =
				RestorationEvaluation.MinimumSupport;
			Region.HorizonCompactRestoration.CurrentMonthlyFunding =
				RestorationEvaluation.CurrentMonthlyFunding;
			Region.HorizonCompactRestoration.ProjectedMonthlyFunding =
				RestorationEvaluation.ProjectedMonthlyFunding;
			Region.HorizonCompactRestoration.MonthlyFundingDelta =
				RestorationEvaluation.MonthlyFundingDelta;
			Region.HorizonCompactRestoration.bEnabled =
				RestorationEvaluation.bAllowed;
			if (!RestorationEvaluation.Diagnostics.IsEmpty())
			{
				Region.HorizonCompactRestoration.UnavailableReasonCode =
					RestorationEvaluation.Diagnostics[0].Code;
				Region.HorizonCompactRestoration.UnavailableReason =
					RestorationEvaluation.Diagnostics[0].Message;
			}

			FCallHorizonCompactEmergencyVoteCommand VoteCommand;
			VoteCommand.ExpectedSequence = Campaign.CommandSequence;
			VoteCommand.TargetRegionId = Pressure.RegionId;
			const FHorizonCompactEmergencyVoteEvaluation VoteEvaluation =
				FStrategicCommandService::EvaluateHorizonCompactEmergencyVote(
					Campaign, Config, VoteCommand);
			Region.HorizonCompactEmergencyVote.bTargetWithdrawn =
				VoteEvaluation.bTargetWithdrawn;
			Region.HorizonCompactEmergencyVote.Cost = VoteEvaluation.Cost;
			Region.HorizonCompactEmergencyVote.TargetCurrentSupport =
				VoteEvaluation.TargetCurrentSupport;
			Region.HorizonCompactEmergencyVote.TargetProjectedSupport =
				VoteEvaluation.TargetProjectedSupport;
			Region.HorizonCompactEmergencyVote.TargetSupportGain =
				VoteEvaluation.TargetSupportGain;
			Region.HorizonCompactEmergencyVote.TargetCurrentPressure =
				VoteEvaluation.TargetCurrentPressure;
			Region.HorizonCompactEmergencyVote.TargetProjectedPressure =
				VoteEvaluation.TargetProjectedPressure;
			Region.HorizonCompactEmergencyVote.TargetPressureReduction =
				VoteEvaluation.TargetPressureReduction;
			Region.HorizonCompactEmergencyVote.VoterSupportCost =
				VoteEvaluation.VoterSupportCost;
			Region.HorizonCompactEmergencyVote.MaximumVoterPressure =
				VoteEvaluation.MaximumVoterPressure;
			Region.HorizonCompactEmergencyVote.RequiredVotes =
				VoteEvaluation.RequiredVotes;
			Region.HorizonCompactEmergencyVote.SupportingMemberRegionIds =
				VoteEvaluation.SupportingMemberRegionIds;
			Region.HorizonCompactEmergencyVote.OpposingMemberRegionIds =
				VoteEvaluation.OpposingMemberRegionIds;
			Region.HorizonCompactEmergencyVote.MonthlyFundingDelta =
				VoteEvaluation.MonthlyFundingDelta;
			Region.HorizonCompactEmergencyVote.bEnabled = VoteEvaluation.bAllowed;
			if (!VoteEvaluation.Diagnostics.IsEmpty())
			{
				Region.HorizonCompactEmergencyVote.UnavailableReasonCode =
					VoteEvaluation.Diagnostics[0].Code;
				Region.HorizonCompactEmergencyVote.UnavailableReason =
					VoteEvaluation.Diagnostics[0].Message;
			}
		}
	}
	Snapshot.Regions.Sort([](const FStrategicRegionView& Left, const FStrategicRegionView& Right)
	{
		return Left.RegionId.LexicalLess(Right.RegionId);
	});

	const FMutualAidRelayQueueSnapshot MutualAidRelayQueue =
		FMutualAidRelayQueue::Evaluate(Campaign, Rules);
	const FStrategicCommandResult FacilityLayoutValidation =
		FStrategicCommandService::ValidateFacilityLayout(Campaign, Rules, Config);
	if (!FacilityLayoutValidation.bAccepted
		&& !FacilityLayoutValidation.Diagnostics.IsEmpty())
	{
		Snapshot.Diagnostics.Add(FacilityLayoutValidation.Diagnostics[0].Message);
	}
	TSet<const FStrategicBaseState*> BasesWithValidInfrastructure;
	for (const FStrategicBaseState& Base : Campaign.Bases)
	{
		const FBaseInfrastructureEvaluation Infrastructure =
			FStrategicCommandService::EvaluateBaseInfrastructure(Campaign, Rules, Base.BaseId);
		if (FacilityLayoutValidation.bAccepted && Infrastructure.bValid)
		{
			BasesWithValidInfrastructure.Add(&Base);
		}
	}
	if (BasesWithValidInfrastructure.Num() != Campaign.Bases.Num())
	{
		Snapshot.bCanAdvanceTime = false;
	}
	for (const FStrategicBaseState& Base : Campaign.Bases)
	{
		FStrategicBaseView& View = Snapshot.Bases.AddDefaulted_GetRef();
		View.BaseId = Base.BaseId;
		View.Name = Base.Name;
		View.RegionId = Base.RegionId;
		View.RegionDisplayName = HumanizeId(Base.RegionId);
		View.LongitudeMilliDegrees = Base.LongitudeMilliDegrees;
		View.LatitudeMilliDegrees = Base.LatitudeMilliDegrees;
		const FBaseInfrastructureEvaluation Infrastructure =
			FStrategicCommandService::EvaluateBaseInfrastructure(Campaign, Rules, Base.BaseId);
		const bool bInfrastructureValid = FacilityLayoutValidation.bAccepted
			&& Infrastructure.bValid;
		if (bInfrastructureValid)
		{
			View.BaseScientistCapacity = Infrastructure.BaseScientistCapacity;
			View.FacilityScientistCapacity = Infrastructure.FacilityScientistCapacity;
			View.ScientistCapacity = Infrastructure.ScientistCapacity;
			View.BaseEngineerCapacity = Infrastructure.BaseEngineerCapacity;
			View.FacilityEngineerCapacity = Infrastructure.FacilityEngineerCapacity;
			View.EngineerCapacity = Infrastructure.EngineerCapacity;
			View.CraftCapacity = Infrastructure.CraftCapacity;
			View.SensorRangeKilometers = Infrastructure.SensorRangeKilometers;
			View.DetectionStrength = Infrastructure.DetectionStrength;
			View.DefenseBatteryCount = Infrastructure.DefenseBatteryCount;
			View.MaximumDefenseDamage = Infrastructure.MaximumDefenseDamage;
			View.ExpectedDefenseDamage = Infrastructure.ExpectedDefenseDamage;
		}
		else if (!Infrastructure.Diagnostics.IsEmpty())
		{
			Snapshot.Diagnostics.Add(Infrastructure.Diagnostics[0].Message);
		}
		if (bInfrastructureValid)
		{
			if (const FMutualAidRelayQueueBaseView* RelayBase =
				MutualAidRelayQueue.FindBase(Base.BaseId))
			{
				View.RelayQueueActiveConvoyCount = RelayBase->ActiveConvoyCount;
				View.RelayQueueTotalConvoyCount = RelayBase->TotalConvoyCount;
				View.RelayQueueWaitingConvoyCount = RelayBase->WaitingConvoyCount;
				View.RelayQueuePressurePercent = RelayBase->QueuePressurePercent;
				View.RelayQueueTailArrivalSeconds = RelayBase->QueueTailArrivalSeconds;
				View.FacilityRelayChannelCount = RelayBase->FacilityRelayChannelCount;
				View.SpecializationRelayChannelBonus = RelayBase->SpecializationRelayChannelBonus;
			}
		}
		View.CraftOccupied = CraftOccupied(Campaign, Base.BaseId);
		View.GridWidth = Config.BaseGridWidth;
		View.GridHeight = Config.BaseGridHeight;
		const FBaseStorageEvaluation Storage =
			FStrategicCommandService::EvaluateBaseStorage(Campaign, Rules, Base.BaseId);
		if (bInfrastructureValid && Storage.bValid)
		{
			View.bStorageEnforced = Storage.bEnforced;
			View.StorageCapacity = Storage.Capacity;
			View.StorageUsed = Storage.Used;
			View.StorageReserved = Storage.Reserved;
			View.StorageProductionReserved = Storage.ManufacturingReserved;
			View.StorageMutualAidReserved = Storage.MutualAidReserved;
			View.StorageCommitted = Storage.Committed;
			View.StorageAvailable = Storage.Available;
			View.StorageOverflow = Storage.Overflow;
		}
		else if (bInfrastructureValid && !Storage.Diagnostics.IsEmpty())
		{
			Snapshot.Diagnostics.Add(Storage.Diagnostics[0].Message);
		}
		View.Specialization = bInfrastructureValid
			? Infrastructure.Specialization : FStrategicBaseSpecializationView();
		FSetSignalWatchStaffCommand CurrentSignalWatch;
		CurrentSignalWatch.ExpectedSequence = Campaign.CommandSequence;
		CurrentSignalWatch.BaseId = Base.BaseId;
		CurrentSignalWatch.AssignedScientists = Base.SignalWatchScientists;
		const FSignalWatchStaffEvaluation SignalWatch =
			FStrategicCommandService::EvaluateSignalWatchStaff(
				Campaign, Rules, CurrentSignalWatch);
		if (bInfrastructureValid && SignalWatch.bValid)
		{
			View.SignalWatchPolicyId = SignalWatch.PolicyId;
			View.SignalWatchScientists = SignalWatch.CurrentScientists;
			View.SignalWatchMaximumScientists = SignalWatch.MaximumScientists;
			View.FacilityRelayChannelCount = SignalWatch.FacilityRelayChannelCount;
			View.SignalWatchBonusChannelCount = SignalWatch.BonusRelayChannelCount;
			View.RelayChannelCount = SignalWatch.TotalRelayChannelCount;
			if (Base.SignalWatchScientists < MAX_int32)
			{
				FSetSignalWatchStaffCommand IncreaseSignalWatch = CurrentSignalWatch;
				++IncreaseSignalWatch.AssignedScientists;
				const FSignalWatchStaffEvaluation Increase =
					FStrategicCommandService::EvaluateSignalWatchStaff(
						Campaign, Rules, IncreaseSignalWatch);
				View.bCanIncreaseSignalWatch = Increase.bAllowed;
				if (!Increase.bAllowed && !Increase.Diagnostics.IsEmpty())
				{
					View.SignalWatchIncreaseUnavailableReasonCode =
						Increase.Diagnostics[0].Code;
					View.SignalWatchIncreaseUnavailableReason =
						Increase.Diagnostics[0].Message;
				}
			}
		}
		else if (bInfrastructureValid && !SignalWatch.Diagnostics.IsEmpty())
		{
			Snapshot.Diagnostics.Add(SignalWatch.Diagnostics[0].Message);
		}
		FSetWorksCadreStaffCommand CurrentWorksCadre;
		CurrentWorksCadre.ExpectedSequence = Campaign.CommandSequence;
		CurrentWorksCadre.BaseId = Base.BaseId;
		CurrentWorksCadre.AssignedEngineers = Base.WorksCadreEngineers;
		const FWorksCadreStaffEvaluation WorksCadre =
			FStrategicCommandService::EvaluateWorksCadreStaff(
				Campaign, Rules, CurrentWorksCadre);
		if (bInfrastructureValid && WorksCadre.bValid)
		{
			View.WorksCadrePolicyId = WorksCadre.PolicyId;
			View.WorksCadreEngineers = WorksCadre.CurrentEngineers;
			View.WorksCadreMaximumEngineers = WorksCadre.MaximumEngineers;
			View.WorksCadreCharter = WorksCadre.Charter;
			View.WorksCadreConstructionFrontloadPercent =
				WorksCadre.ConstructionFrontloadPercent;
			View.WorksCadreRepairFrontloadPercent =
				WorksCadre.RepairFrontloadPercent;
			if (Base.WorksCadreEngineers < MAX_int32)
			{
				FSetWorksCadreStaffCommand IncreaseWorksCadre = CurrentWorksCadre;
				++IncreaseWorksCadre.AssignedEngineers;
				const FWorksCadreStaffEvaluation Increase =
					FStrategicCommandService::EvaluateWorksCadreStaff(
						Campaign, Rules, IncreaseWorksCadre);
				View.bCanIncreaseWorksCadre = Increase.bAllowed;
				if (!Increase.bAllowed && !Increase.Diagnostics.IsEmpty())
				{
					View.WorksCadreIncreaseUnavailableReasonCode =
						Increase.Diagnostics[0].Code;
					View.WorksCadreIncreaseUnavailableReason =
						Increase.Diagnostics[0].Message;
				}
			}
			for (const FWorksCadreCharterPolicy& Policy :
				FStrategicCommandService::GetWorksCadreCharterPolicies())
			{
				FSetWorksCadreCharterCommand CharterCommand;
				CharterCommand.ExpectedSequence = Campaign.CommandSequence;
				CharterCommand.BaseId = Base.BaseId;
				CharterCommand.Charter = Policy.Charter;
				const FWorksCadreCharterEvaluation CharterEvaluation =
					FStrategicCommandService::EvaluateWorksCadreCharter(
						Campaign, CharterCommand);
				FStrategicWorksCadreCharterOptionView& Option =
					View.WorksCadreCharterOptions.AddDefaulted_GetRef();
				Option.Charter = Policy.Charter;
				Option.PolicyId = Policy.PolicyId;
				Option.ConstructionFrontloadPercentPerEngineer =
					Policy.ConstructionFrontloadPercentPerEngineer;
				Option.RepairFrontloadPercentPerEngineer =
					Policy.RepairFrontloadPercentPerEngineer;
				Option.ConstructionFrontloadPercent =
					CharterEvaluation.ConstructionFrontloadPercent;
				Option.RepairFrontloadPercent =
					CharterEvaluation.RepairFrontloadPercent;
				Option.bSelected = Policy.Charter == Base.WorksCadreCharter;
				Option.bEnabled = CharterEvaluation.bAllowed;
				if (Option.bSelected)
				{
					View.WorksCadreCharterPolicyId = Policy.PolicyId;
				}
				else if (!CharterEvaluation.bAllowed
					&& !CharterEvaluation.Diagnostics.IsEmpty())
				{
					Option.UnavailableReasonCode =
						CharterEvaluation.Diagnostics[0].Code;
					Option.UnavailableReason =
						CharterEvaluation.Diagnostics[0].Message;
				}
			}
		}
		else if (bInfrastructureValid && !WorksCadre.Diagnostics.IsEmpty())
		{
			Snapshot.Diagnostics.Add(WorksCadre.Diagnostics[0].Message);
		}
		for (const FBaseFacilityState& Installed : Base.Facilities)
		{
			FStrategicFacilityView& FacilityView = View.FacilityLayout.AddDefaulted_GetRef();
			FacilityView.FacilityInstanceId = Installed.InstanceId;
			FacilityView.FacilityId = Installed.FacilityId;
			FacilityView.GridX = Installed.GridX;
			FacilityView.GridY = Installed.GridY;
			if (const FFacilityRule* Facility = Rules.Facilities.Find(Installed.FacilityId))
			{
				FacilityView.DisplayName = RuleName(Facility->DisplayName, Installed.FacilityId);
				FacilityView.GridWidth = FMath::Max(1, Facility->GridWidth);
				FacilityView.GridHeight = FMath::Max(1, Facility->GridHeight);
				FacilityView.MaxIntegrity = FMath::Max(1, Facility->MaxIntegrity);
				FacilityView.Damage = FMath::Clamp(Installed.Damage, 0, FacilityView.MaxIntegrity);
				FacilityView.CurrentIntegrity = FacilityView.MaxIntegrity - FacilityView.Damage;
				FacilityView.bOperational = FacilityView.Damage < FacilityView.MaxIntegrity;
				FacilityView.EffectivenessPercent =
					Facility->ScaleEffectByIntegrity(100, FacilityView.Damage);
				FacilityView.MaximumStorageCapacity = FMath::Max(0, Facility->StorageCapacity);
				FacilityView.StorageCapacity = Facility->ScaleEffectByIntegrity(
					FacilityView.MaximumStorageCapacity, FacilityView.Damage);
				FacilityView.MaximumScientistCapacity = FMath::Max(0, Facility->ScientistCapacity);
				FacilityView.ScientistCapacity = Facility->ScaleEffectByIntegrity(
					FacilityView.MaximumScientistCapacity, FacilityView.Damage);
				FacilityView.MaximumEngineerCapacity = FMath::Max(0, Facility->EngineerCapacity);
				FacilityView.EngineerCapacity = Facility->ScaleEffectByIntegrity(
					FacilityView.MaximumEngineerCapacity, FacilityView.Damage);
				FacilityView.MaximumCraftCapacity = FMath::Max(0, Facility->CraftCapacity);
				FacilityView.CraftCapacity = Facility->ScaleEffectByIntegrity(
					FacilityView.MaximumCraftCapacity, FacilityView.Damage);
				FacilityView.MaximumSensorRangeKilometers = FMath::Max(0, Facility->SensorRangeKilometers);
				FacilityView.SensorRangeKilometers = Facility->ScaleEffectByIntegrity(
					FacilityView.MaximumSensorRangeKilometers, FacilityView.Damage);
				FacilityView.MaximumDetectionStrength = FMath::Max(0, Facility->DetectionStrength);
				FacilityView.DetectionStrength = Facility->ScaleEffectByIntegrity(
					FacilityView.MaximumDetectionStrength, FacilityView.Damage);
				FacilityView.MaximumBaseDefenseAccuracy = FMath::Max(0, Facility->BaseDefenseAccuracy);
				FacilityView.BaseDefenseAccuracy = Facility->ScaleEffectByIntegrity(
					FacilityView.MaximumBaseDefenseAccuracy, FacilityView.Damage);
				FacilityView.MaximumBaseDefenseDamage = FMath::Max(0, Facility->BaseDefenseDamage);
				FacilityView.BaseDefenseDamage = Facility->ScaleEffectByIntegrity(
					FacilityView.MaximumBaseDefenseDamage, FacilityView.Damage);
				FacilityView.BaseDefenseSupplyItemId = Facility->BaseDefenseSupplyItemId;
				FacilityView.BaseDefenseSupplyPerShot = FMath::Max(0, Facility->BaseDefenseSupplyPerShot);
				if (const FItemRule* Supply = Rules.Items.Find(Facility->BaseDefenseSupplyItemId))
				{
					FacilityView.BaseDefenseSupplyDisplayName =
						RuleName(Supply->DisplayName, Facility->BaseDefenseSupplyItemId);
				}
			}
			else
			{
				FacilityView.DisplayName = HumanizeId(Installed.FacilityId);
				FacilityView.bOperational = false;
			}
			FacilityView.bRepairing = Installed.RemainingRepairSeconds > 0;
			FacilityView.RemainingRepairSeconds = FMath::Max<int64>(0, Installed.RemainingRepairSeconds);
			if (const FFacilityRule* Facility = Rules.Facilities.Find(Installed.FacilityId))
			{
				FacilityView.RepairCancellationRefund =
					static_cast<int64>(FMath::Max(0, Installed.ReservedRepairDamage))
					* static_cast<int64>(FMath::Max(0, Facility->RepairCostPerIntegrity));
			}
			FStartFacilityRepairCommand Repair;
			Repair.ExpectedSequence = Campaign.CommandSequence;
			Repair.BaseId = Base.BaseId;
			Repair.FacilityInstanceId = Installed.InstanceId;
			const FFacilityRepairEvaluation RepairEvaluation =
				FStrategicCommandService::EvaluateFacilityRepair(Campaign, Rules, Repair);
			FacilityView.bCanRepair = RepairEvaluation.bAllowed;
			FacilityView.RepairCost = RepairEvaluation.Cost;
			FacilityView.RepairDurationSeconds = RepairEvaluation.DurationSeconds;
			FacilityView.RepairBaselineDurationSeconds = RepairEvaluation.BaselineDurationSeconds;
			FacilityView.RepairWorksCadreFrontloadPercent =
				RepairEvaluation.WorksCadreFrontloadPercent;
			if (!RepairEvaluation.bAllowed && FacilityView.Damage > 0)
			{
				if (RepairEvaluation.Diagnostics.IsEmpty())
				{
					FacilityView.RepairUnavailableReason = TEXT("This facility cannot currently be repaired.");
				}
				else
				{
					FacilityView.RepairUnavailableReasonCode = RepairEvaluation.Diagnostics[0].Code;
					FacilityView.RepairUnavailableReason = RepairEvaluation.Diagnostics[0].Message;
				}
			}
			FDismantleFacilityCommand Dismantle;
			Dismantle.ExpectedSequence = Campaign.CommandSequence;
			Dismantle.BaseId = Base.BaseId;
			Dismantle.FacilityInstanceId = Installed.InstanceId;
			const FFacilityDismantleEvaluation Evaluation =
				FStrategicCommandService::EvaluateFacilityDismantle(Campaign, Rules, Config, Dismantle);
			FacilityView.bCanDismantle = Evaluation.bAllowed;
			FacilityView.DismantleRefund = Evaluation.Refund;
			if (!Evaluation.bAllowed)
			{
				if (Evaluation.Diagnostics.IsEmpty())
				{
					FacilityView.DismantleUnavailableReason =
						TEXT("This operational facility cannot currently be dismantled.");
				}
				else
				{
					FacilityView.DismantleUnavailableReasonCode = Evaluation.Diagnostics[0].Code;
					FacilityView.DismantleUnavailableReason = Evaluation.Diagnostics[0].Message;
				}
			}
		}
		for (const FFacilityConstructionProjectState& Project : Campaign.FacilityConstructionProjects)
		{
			if (Project.BaseId != Base.BaseId)
			{
				continue;
			}
			FStrategicFacilityView& FacilityView = View.FacilityLayout.AddDefaulted_GetRef();
			FacilityView.FacilityInstanceId = Project.FacilityInstanceId;
			FacilityView.ProjectId = Project.ProjectId;
			FacilityView.FacilityId = Project.FacilityId;
			FacilityView.GridX = Project.GridX;
			FacilityView.GridY = Project.GridY;
			FacilityView.bOperational = false;
			FacilityView.bConstructing = true;
			FacilityView.RemainingBuildSeconds = FMath::Max<int64>(0, Project.RemainingBuildSeconds);
			if (const FFacilityRule* Facility = Rules.Facilities.Find(Project.FacilityId))
			{
				FacilityView.DisplayName = RuleName(Facility->DisplayName, Project.FacilityId);
				FacilityView.GridWidth = FMath::Max(1, Facility->GridWidth);
				FacilityView.GridHeight = FMath::Max(1, Facility->GridHeight);
				FacilityView.MaxIntegrity = FMath::Max(1, Facility->MaxIntegrity);
				FacilityView.MaximumStorageCapacity = FMath::Max(0, Facility->StorageCapacity);
				FacilityView.MaximumScientistCapacity = FMath::Max(0, Facility->ScientistCapacity);
				FacilityView.MaximumEngineerCapacity = FMath::Max(0, Facility->EngineerCapacity);
				FacilityView.MaximumCraftCapacity = FMath::Max(0, Facility->CraftCapacity);
				FacilityView.MaximumSensorRangeKilometers = FMath::Max(0, Facility->SensorRangeKilometers);
				FacilityView.MaximumDetectionStrength = FMath::Max(0, Facility->DetectionStrength);
				FacilityView.MaximumBaseDefenseAccuracy = FMath::Max(0, Facility->BaseDefenseAccuracy);
				FacilityView.MaximumBaseDefenseDamage = FMath::Max(0, Facility->BaseDefenseDamage);
				FacilityView.BaseDefenseSupplyItemId = Facility->BaseDefenseSupplyItemId;
				FacilityView.BaseDefenseSupplyPerShot = FMath::Max(0, Facility->BaseDefenseSupplyPerShot);
				if (const FItemRule* Supply = Rules.Items.Find(Facility->BaseDefenseSupplyItemId))
				{
					FacilityView.BaseDefenseSupplyDisplayName =
						RuleName(Supply->DisplayName, Facility->BaseDefenseSupplyItemId);
				}
			}
			else
			{
				FacilityView.DisplayName = HumanizeId(Project.FacilityId);
			}
		}
		View.FacilityLayout.Sort([](const FStrategicFacilityView& Left, const FStrategicFacilityView& Right)
		{
			if (Left.GridY != Right.GridY)
			{
				return Left.GridY < Right.GridY;
			}
			if (Left.GridX != Right.GridX)
			{
				return Left.GridX < Right.GridX;
			}
			if (Left.bOperational != Right.bOperational)
			{
				return Left.bOperational;
			}
			return GuidLess(Left.FacilityInstanceId, Right.FacilityInstanceId);
		});
		View.AssignedScientists = FMath::Max(0, Base.SignalWatchScientists);
		for (const FResearchProjectState& Project : Campaign.ResearchProjects)
		{
			View.AssignedScientists = SaturatingNonNegativeAdd(
				View.AssignedScientists,
				Project.BaseId == Base.BaseId ? Project.AssignedScientists : 0);
		}
		View.AssignedEngineers = FMath::Max(0, Base.WorksCadreEngineers);
		for (const FManufacturingProjectState& Project : Campaign.ManufacturingProjects)
		{
			View.AssignedEngineers = SaturatingNonNegativeAdd(
				View.AssignedEngineers,
				Project.BaseId == Base.BaseId ? Project.AssignedEngineers : 0);
		}
		View.ScientistPersonnel = PersonnelCountForCategory(
			Campaign, Rules, Base.BaseId, EPersonnelRoleCategory::Scientist);
		View.EngineerPersonnel = PersonnelCountForCategory(
			Campaign, Rules, Base.BaseId, EPersonnelRoleCategory::Engineer);
		View.ScientistOverCapacity = bInfrastructureValid
			? FMath::Max(0,
				FMath::Max(View.AssignedScientists, View.ScientistPersonnel) - View.ScientistCapacity)
			: 0;
		View.EngineerOverCapacity = bInfrastructureValid
			? FMath::Max(0,
				FMath::Max(View.AssignedEngineers, View.EngineerPersonnel) - View.EngineerCapacity)
			: 0;
		if (!Base.Facilities.IsEmpty())
		{
			for (const FBaseFacilityState& Installed : Base.Facilities)
			{
			if (const FFacilityRule* Facility = Rules.Facilities.Find(Installed.FacilityId))
			{
					FString FacilityName = RuleName(Facility->DisplayName, Installed.FacilityId);
					const int32 MaxIntegrity = FMath::Max(1, Facility->MaxIntegrity);
					const int32 ClampedDamage = FMath::Clamp(Installed.Damage, 0, MaxIntegrity);
					const int32 CurrentIntegrity = FMath::Clamp(
						MaxIntegrity - ClampedDamage, 0, MaxIntegrity);
					if (Installed.RemainingRepairSeconds > 0)
					{
						FacilityName += FString::Printf(TEXT(" [REPAIR %lld h • %d/%d]"),
							Installed.RemainingRepairSeconds / 3600
								+ (Installed.RemainingRepairSeconds % 3600 == 0 ? 0 : 1),
							CurrentIntegrity, MaxIntegrity);
					}
					else if (Facility->MaxIntegrity > 0 && Installed.Damage >= Facility->MaxIntegrity)
					{
						FacilityName += FString::Printf(TEXT(" [OFFLINE %d/%d]"),
							CurrentIntegrity, MaxIntegrity);
					}
					else if (Installed.Damage > 0)
					{
						FacilityName += FString::Printf(TEXT(" [DEGRADED %d/%d • %d%% OUTPUT]"),
							CurrentIntegrity, MaxIntegrity,
							Facility->ScaleEffectByIntegrity(100, ClampedDamage));
					}
					View.Facilities.Add(MoveTemp(FacilityName));
					if (bInfrastructureValid)
					{
						Snapshot.MonthlyOutgoings = SaturatingAdd(
							Snapshot.MonthlyOutgoings, Facility->MonthlyMaintenance);
					}
				}
				else
				{
					View.Facilities.Add(HumanizeId(Installed.FacilityId));
				}
			}
		}
		else
		{
			for (const FName FacilityId : Base.BuiltFacilities)
			{
				if (const FFacilityRule* Facility = Rules.Facilities.Find(FacilityId))
				{
					View.Facilities.Add(RuleName(Facility->DisplayName, FacilityId));
					if (bInfrastructureValid)
					{
						Snapshot.MonthlyOutgoings = SaturatingAdd(
							Snapshot.MonthlyOutgoings, Facility->MonthlyMaintenance);
					}
				}
				else
				{
					View.Facilities.Add(HumanizeId(FacilityId));
				}
			}
		}
		View.Facilities.Sort();
		for (const FInventoryStack& Stack : Base.Inventory)
		{
			FStrategicInventoryView& Item = View.Inventory.AddDefaulted_GetRef();
			Item.ItemId = Stack.ItemId;
			Item.Quantity = FMath::Max(0, Stack.Quantity);
			const FItemRule* Rule = Rules.Items.Find(Stack.ItemId);
			Item.DisplayName = Rule != nullptr ? RuleName(Rule->DisplayName, Stack.ItemId) : HumanizeId(Stack.ItemId);
			Item.UnitSellValue = Rule != nullptr ? FMath::Max(0, Rule->SellValue) : 0;
			Item.UnitStorage = Rule != nullptr ? FMath::Max(0, Rule->Mass) : 0;
			Item.TotalStorage = SafeProduct(Item.UnitStorage, FMath::Max(0, Stack.Quantity));
			Item.bPersonnelEquippable = Rule != nullptr && IsPersonnelEquipment(*Rule)
				&& HasRequirements(Rule->RequiredResearch, Campaign);
			for (const FStrategicBaseState& Destination : Campaign.Bases)
			{
				if (Destination.BaseId == Base.BaseId)
				{
					continue;
				}
				FStrategicMutualAidDispatchOptionView& Option =
					Item.MutualAidOptions.AddDefaulted_GetRef();
				Option.DestinationBaseId = Destination.BaseId;
				Option.DestinationBaseName = Destination.Name;
				Option.TransitSeconds =
					static_cast<int64>(Config.MutualAidConvoyTransitHours) * 3600LL;
				if (!bInfrastructureValid || !BasesWithValidInfrastructure.Contains(&Destination))
				{
					Option.UnavailableReasonCode = TEXT("invalid_facility_state");
					Option.UnavailableReason = !bInfrastructureValid
						? TEXT("The source base has invalid facility state.")
						: TEXT("The destination base has invalid facility state.");
					continue;
				}
				for (const EMutualAidRoutePolicy Policy :
					FStrategicCommandService::GetMutualAidRoutePolicies())
				{
					const FMutualAidRouteEvaluation Route =
						FStrategicCommandService::EvaluateMutualAidRoute(
							Campaign, Config, Base.BaseId, Destination.BaseId,
							Policy, false);
					if (!Route.bValid)
					{
						continue;
					}
					FStrategicMutualAidRouteOptionView& RouteView =
						Option.Routes.AddDefaulted_GetRef();
					RouteView.Policy = Route.Policy;
					RouteView.PolicyId = Route.PolicyId;
					RouteView.TransitSeconds = Route.TransitSeconds;
					RouteView.BaselinePressure = Route.BaselinePressure;
					RouteView.ExposureModifier = Route.ExposureModifier;
					RouteView.RoutePressure = Route.RoutePressure;
					RouteView.bInterdictionExpected = Route.bInterdictionExpected;
					RouteView.InterdictionDelaySeconds = Route.InterdictionDelaySeconds;
					RouteView.SignalEscortCost = Route.SignalEscortCost;
					RouteView.bSignalEscortAffordable = Route.bSignalEscortAffordable;
					RouteView.bEnabled = true;
					const int64 UnescortedJourney = Route.TransitSeconds
						+ (Route.bInterdictionExpected ? Route.InterdictionDelaySeconds : 0);
					RouteView.RelayQueue = FMutualAidRelayQueue::ProjectNext(
						Campaign, Rules, Base.BaseId, UnescortedJourney);
					const FMutualAidRelayQueueView EscortedRelay =
						FMutualAidRelayQueue::ProjectNext(
							Campaign, Rules, Base.BaseId, Route.TransitSeconds);
					RouteView.EscortedEstimatedArrivalSeconds =
						EscortedRelay.EstimatedArrivalSeconds;
				}
				if (Campaign.Outcome != ECampaignOutcome::Ongoing)
				{
					Option.UnavailableReasonCode = TEXT("campaign_concluded");
					Option.UnavailableReason =
						TEXT("Mutual Aid Convoys are unavailable after the campaign has concluded.");
					continue;
				}
				if (Rule == nullptr || Rule->Mass < 0)
				{
					Option.UnavailableReasonCode = TEXT("unknown_item");
					Option.UnavailableReason =
						TEXT("This inventory item is unavailable in the active rules.");
					continue;
				}
				const FBaseStorageEvaluation DestinationStorage =
					FStrategicCommandService::EvaluateBaseStorage(
						Campaign, Rules, Destination.BaseId);
				if (!DestinationStorage.bValid)
				{
					Option.UnavailableReasonCode = DestinationStorage.Diagnostics.IsEmpty()
						? FName(TEXT("invalid_storage_state"))
						: DestinationStorage.Diagnostics[0].Code;
					Option.UnavailableReason = DestinationStorage.Diagnostics.IsEmpty()
						? TEXT("Destination storage is unavailable.")
						: DestinationStorage.Diagnostics[0].Message;
					continue;
				}
				if (Option.Routes.Num() != 3)
				{
					Option.UnavailableReasonCode = TEXT("invalid_simulation_config");
					Option.UnavailableReason = TEXT("Mutual Aid route policy is unavailable.");
					continue;
				}
				if (!Option.Routes[0].RelayQueue.bValid
					|| !Option.Routes[0].RelayQueue.bRelayAvailable)
				{
					Option.UnavailableReasonCode = TEXT("mutual_aid_relay_unavailable");
					Option.UnavailableReason =
						TEXT("The source base has no operational signal capacity for a Mutual Aid relay channel.");
					continue;
				}
				int64 MaximumQuantity = Item.Quantity;
				if (DestinationStorage.bEnforced && Item.UnitStorage > 0)
				{
					MaximumQuantity = FMath::Min<int64>(
						MaximumQuantity, DestinationStorage.Available / Item.UnitStorage);
				}
				Option.MaximumQuantity = static_cast<int32>(FMath::Clamp<int64>(
					MaximumQuantity, 0, MAX_int32));
				Option.bEnabled = Option.MaximumQuantity > 0;
				if (!Option.bEnabled)
				{
					Option.UnavailableReasonCode = TEXT("storage_capacity_exceeded");
					Option.UnavailableReason = DestinationStorage.Overflow > 0
						? TEXT("Resolve the destination's existing storage overflow first.")
						: TEXT("The destination has no free storage for this cargo.");
				}
			}
			Item.MutualAidOptions.Sort(
				[](const FStrategicMutualAidDispatchOptionView& Left,
					const FStrategicMutualAidDispatchOptionView& Right)
				{
					return GuidLess(Left.DestinationBaseId, Right.DestinationBaseId);
				});
		}
		View.Inventory.Sort([](const FStrategicInventoryView& Left, const FStrategicInventoryView& Right)
		{
			return Left.ItemId.LexicalLess(Right.ItemId);
		});
		AddGlobeMarker(Snapshot, EStrategicGlobeMarkerType::Base, Base.BaseId, Base.Name,
			View.RegionDisplayName, Base.LongitudeMilliDegrees, Base.LatitudeMilliDegrees, false);
	}
	Snapshot.Bases.Sort([](const FStrategicBaseView& Left, const FStrategicBaseView& Right)
	{
		return GuidLess(Left.BaseId, Right.BaseId);
	});
	Snapshot.PrimaryBaseId = Snapshot.Bases.IsEmpty() ? FGuid() : Snapshot.Bases[0].BaseId;

	for (const FMutualAidConvoyState& Convoy : Campaign.MutualAidConvoys)
	{
		const FStrategicBaseState* Source = Campaign.Bases.FindByPredicate(
			[&Convoy](const FStrategicBaseState& Base) { return Base.BaseId == Convoy.SourceBaseId; });
		const FGuid CurrentLegOriginBaseId = Convoy.CurrentLegOriginBaseId.IsValid()
			? Convoy.CurrentLegOriginBaseId
			: Convoy.SourceBaseId;
		const FStrategicBaseState* CurrentLegOrigin = Campaign.Bases.FindByPredicate(
			[CurrentLegOriginBaseId](const FStrategicBaseState& Base)
			{
				return Base.BaseId == CurrentLegOriginBaseId;
			});
		const FStrategicBaseState* Destination = Campaign.Bases.FindByPredicate(
			[&Convoy](const FStrategicBaseState& Base) { return Base.BaseId == Convoy.DestinationBaseId; });
		const FStrategicBaseState* Waypoint = Convoy.RelayWaypointBaseId.IsValid()
			? Campaign.Bases.FindByPredicate(
				[&Convoy](const FStrategicBaseState& Base)
				{
					return Base.BaseId == Convoy.RelayWaypointBaseId;
				})
			: nullptr;
		const FItemRule* Item = Rules.Items.Find(Convoy.ItemId);
		if (Source == nullptr || CurrentLegOrigin == nullptr || Destination == nullptr
			|| (Convoy.RelayWaypointBaseId.IsValid() && Waypoint == nullptr)
			|| Item == nullptr
			|| Item->Mass < 0 || Convoy.Quantity <= 0 || Convoy.DispatchSequence <= 0
			|| Convoy.DispatchSequence > Campaign.CommandSequence
			|| Convoy.TotalTransitSeconds <= 0 || Convoy.RemainingTransitSeconds <= 0
			|| Convoy.RemainingTransitSeconds > Convoy.TotalTransitSeconds
			|| Convoy.RoutePressure < 0 || Convoy.RoutePressure > 100
			|| Convoy.SignalEscortCost < 0
			|| (!Convoy.bSignalEscort && Convoy.SignalEscortCost != 0)
			|| Convoy.ForecastInterdictionDelaySeconds <= 0
			|| Convoy.InterdictionDelaySeconds < 0)
		{
			Snapshot.Diagnostics.Add(TEXT("A persisted Mutual Aid Convoy is invalid."));
			continue;
		}
		FStrategicMutualAidConvoyView& View = Snapshot.MutualAidConvoys.AddDefaulted_GetRef();
		View.ConvoyId = Convoy.ConvoyId;
		View.SourceBaseId = Convoy.SourceBaseId;
		View.SourceBaseName = Source->Name;
		View.CurrentLegOriginBaseId = CurrentLegOriginBaseId;
		View.CurrentLegOriginBaseName = CurrentLegOrigin->Name;
		View.DestinationBaseId = Convoy.DestinationBaseId;
		View.DestinationBaseName = Destination->Name;
		View.ItemId = Convoy.ItemId;
		View.ItemDisplayName = RuleName(Item->DisplayName, Convoy.ItemId);
		View.Quantity = Convoy.Quantity;
		View.DispatchSequence = Convoy.DispatchSequence;
		View.TotalStorage = SafeProduct(FMath::Max(0, Item->Mass), Convoy.Quantity);
		View.RemainingTransitSeconds = Convoy.RemainingTransitSeconds;
		View.RoutePolicy = Convoy.RoutePolicy;
		View.RoutePolicyId =
			FStrategicCommandService::GetMutualAidRoutePolicyId(Convoy.RoutePolicy);
		View.TotalTransitSeconds = Convoy.TotalTransitSeconds;
		View.RoutePressure = Convoy.RoutePressure;
		View.bSignalEscort = Convoy.bSignalEscort;
		View.SignalEscortCost = Convoy.SignalEscortCost;
		View.bInterdictionResolved = Convoy.bInterdictionResolved;
		View.ForecastInterdictionDelaySeconds = Convoy.ForecastInterdictionDelaySeconds;
		View.InterdictionDelaySeconds = Convoy.InterdictionDelaySeconds;
		View.RelayWaypointBaseId = Convoy.RelayWaypointBaseId;
		View.RelayWaypointBaseName = Waypoint != nullptr ? Waypoint->Name : FString();
		View.OnwardRoutePolicy = Convoy.OnwardRoutePolicy;
		View.OnwardRoutePolicyId = Convoy.RelayWaypointBaseId.IsValid()
			? FStrategicCommandService::GetMutualAidRoutePolicyId(
				Convoy.OnwardRoutePolicy)
			: NAME_None;
		View.OnwardTransitSeconds = Convoy.OnwardTotalTransitSeconds;
		View.OnwardRoutePressure = Convoy.OnwardRoutePressure;
		View.bOnwardInterdictionResolved = Convoy.bOnwardInterdictionResolved;
		View.OnwardForecastInterdictionDelaySeconds =
			Convoy.OnwardForecastInterdictionDelaySeconds;
		View.BalancedHandoffQuantity = Convoy.BalancedHandoffQuantity;
		View.FinalDeliveryQuantity = static_cast<int32>(FMath::Clamp<int64>(
			static_cast<int64>(Convoy.Quantity)
				- static_cast<int64>(Convoy.BalancedHandoffQuantity), MIN_int32, MAX_int32));
		View.BalancedHandoffStorage = SafeProduct(
			FMath::Max(0, Item->Mass), Convoy.BalancedHandoffQuantity);
		const bool bConvoyInfrastructureValid =
			BasesWithValidInfrastructure.Contains(Source)
			&& BasesWithValidInfrastructure.Contains(CurrentLegOrigin)
			&& BasesWithValidInfrastructure.Contains(Destination)
			&& (Waypoint == nullptr || BasesWithValidInfrastructure.Contains(Waypoint));
		if (bConvoyInfrastructureValid)
		{
			if (const FMutualAidRelayQueueView* Relay =
				MutualAidRelayQueue.FindConvoy(Convoy.ConvoyId))
			{
				View.RelayQueue = *Relay;
			}
		}
		else
		{
			const FName InvalidFacilityStateCode = TEXT("invalid_facility_state");
			const FString InvalidFacilityStateMessage =
				TEXT("Mutual Aid Convoy actions are unavailable while its route bases have invalid facility state.");
			Snapshot.Diagnostics.Add(
				TEXT("A persisted Mutual Aid Convoy references a base with invalid facility state."));
			View.SignalEscortCommissionUnavailableReasonCode = InvalidFacilityStateCode;
			View.SignalEscortCommissionUnavailableReason = InvalidFacilityStateMessage;
			View.ReliefPriorityUnavailableReasonCode = InvalidFacilityStateCode;
			View.ReliefPriorityUnavailableReason = InvalidFacilityStateMessage;
			View.ReliefStandDownUnavailableReasonCode = InvalidFacilityStateCode;
			View.ReliefStandDownUnavailableReason = InvalidFacilityStateMessage;
			View.ReliefDiversionUnavailableReasonCode = InvalidFacilityStateCode;
			View.ReliefDiversionUnavailableReason = InvalidFacilityStateMessage;
			View.RelayWaypointUnavailableReasonCode = InvalidFacilityStateCode;
			View.RelayWaypointUnavailableReason = InvalidFacilityStateMessage;
			View.BalancedHandoffUnavailableReasonCode = InvalidFacilityStateCode;
			View.BalancedHandoffUnavailableReason = InvalidFacilityStateMessage;
			View.RetuneUnavailableReasonCode = InvalidFacilityStateCode;
			View.RetuneUnavailableReason = InvalidFacilityStateMessage;
			continue;
		}
		FCommissionMutualAidSignalEscortCommand CommissionEscort;
		CommissionEscort.ExpectedSequence = Campaign.CommandSequence;
		CommissionEscort.ConvoyId = Convoy.ConvoyId;
		const FSignalEscortCommissionEvaluation EscortEvaluation =
			FStrategicCommandService::EvaluateSignalEscortCommission(
				Campaign, Rules, Config, CommissionEscort);
		View.bCanCommissionSignalEscort = EscortEvaluation.bAllowed;
		View.SignalEscortCommissionCost = EscortEvaluation.FundingCost;
		View.SignalEscortPreventedDelaySeconds = EscortEvaluation.PreventedDelaySeconds;
		if (EscortEvaluation.bAllowed)
		{
			View.SignalEscortProjectedRelayQueue = EscortEvaluation.ProjectedRelayQueue;
		}
		if (!EscortEvaluation.Diagnostics.IsEmpty())
		{
			View.SignalEscortCommissionUnavailableReasonCode =
				EscortEvaluation.Diagnostics[0].Code;
			View.SignalEscortCommissionUnavailableReason =
				EscortEvaluation.Diagnostics[0].Message;
		}
		FPrioritizeMutualAidConvoyCommand Prioritize;
		Prioritize.ExpectedSequence = Campaign.CommandSequence;
		Prioritize.ConvoyId = Convoy.ConvoyId;
		const FMutualAidReliefPriorityEvaluation PriorityEvaluation =
			FStrategicCommandService::EvaluateMutualAidReliefPriority(
				Campaign, Rules, Prioritize);
		View.bCanPrioritizeRelief = PriorityEvaluation.bAllowed;
		View.ReliefPriorityPolicyId = PriorityEvaluation.PolicyId;
		View.ReliefPriorityBypassedConvoyCount =
			PriorityEvaluation.BypassedConvoyCount;
		View.ReliefPriorityRecoveredWaitSeconds =
			PriorityEvaluation.RecoveredWaitSeconds;
		if (PriorityEvaluation.bAllowed)
		{
			View.ReliefPriorityProjectedRelayQueue =
				PriorityEvaluation.ProjectedRelayQueue;
		}
		if (!PriorityEvaluation.Diagnostics.IsEmpty())
		{
			View.ReliefPriorityUnavailableReasonCode =
				PriorityEvaluation.Diagnostics[0].Code;
			View.ReliefPriorityUnavailableReason =
				PriorityEvaluation.Diagnostics[0].Message;
		}
		FStandDownMutualAidConvoyCommand StandDown;
		StandDown.ExpectedSequence = Campaign.CommandSequence;
		StandDown.ConvoyId = Convoy.ConvoyId;
		const FMutualAidReliefStandDownEvaluation StandDownEvaluation =
			FStrategicCommandService::EvaluateMutualAidReliefStandDown(
				Campaign, Rules, StandDown);
		View.bCanStandDownRelief = StandDownEvaluation.bAllowed;
		View.ReliefStandDownPolicyId = StandDownEvaluation.PolicyId;
		View.ReliefStandDownReleasedStorage = StandDownEvaluation.ReleasedStorage;
		View.ReliefStandDownSunkSignalEscortCost =
			StandDownEvaluation.SunkSignalEscortCost;
		View.ReliefStandDownAdvancedConvoyCount =
			StandDownEvaluation.AdvancedConvoyCount;
		View.ReliefStandDownRecoveredWaitSeconds =
			StandDownEvaluation.TotalRecoveredWaitSeconds;
		if (!StandDownEvaluation.Diagnostics.IsEmpty())
		{
			View.ReliefStandDownUnavailableReasonCode =
				StandDownEvaluation.Diagnostics[0].Code;
			View.ReliefStandDownUnavailableReason =
				StandDownEvaluation.Diagnostics[0].Message;
		}
		for (const FStrategicBaseState& Candidate : Campaign.Bases)
		{
			if (Candidate.BaseId == Convoy.SourceBaseId)
			{
				continue;
			}
			FDivertMutualAidConvoyCommand Divert;
			Divert.ExpectedSequence = Campaign.CommandSequence;
			Divert.ConvoyId = Convoy.ConvoyId;
			Divert.DestinationBaseId = Candidate.BaseId;
			const FMutualAidReliefDiversionEvaluation DiversionEvaluation =
				FStrategicCommandService::EvaluateMutualAidReliefDiversion(
					Campaign, Rules, Config, Divert);
			FStrategicMutualAidDiversionOptionView& Option =
				View.ReliefDiversionOptions.AddDefaulted_GetRef();
			Option.DestinationBaseId = Candidate.BaseId;
			Option.DestinationBaseName = Candidate.Name;
			Option.PolicyId = DiversionEvaluation.PolicyId;
			Option.DivertedStorage = DiversionEvaluation.DivertedStorage;
			Option.CurrentRoutePressure = DiversionEvaluation.CurrentRoutePressure;
			Option.ProjectedRoutePressure = DiversionEvaluation.ProjectedRoutePressure;
			Option.bInterdictionExpected =
				DiversionEvaluation.bInterdictionExpected;
			Option.bSignalEscort = DiversionEvaluation.bSignalEscort;
			Option.RetainedSignalEscortCost =
				DiversionEvaluation.RetainedSignalEscortCost;
			Option.ArrivalShiftSeconds =
				DiversionEvaluation.TargetArrivalShiftSeconds;
			Option.AffectedConvoyCount =
				DiversionEvaluation.AffectedConvoyCount;
			Option.TotalArrivalShiftSeconds =
				DiversionEvaluation.TotalArrivalShiftSeconds;
			Option.bEnabled = DiversionEvaluation.bAllowed;
			if (DiversionEvaluation.bAllowed)
			{
				Option.ProjectedRelayQueue = DiversionEvaluation.ProjectedRelayQueue;
				View.bCanDivertRelief = true;
			}
			if (!DiversionEvaluation.Diagnostics.IsEmpty())
			{
				Option.UnavailableReasonCode =
					DiversionEvaluation.Diagnostics[0].Code;
				Option.UnavailableReason =
					DiversionEvaluation.Diagnostics[0].Message;
			}
		}
		View.ReliefDiversionOptions.Sort(
			[](const FStrategicMutualAidDiversionOptionView& Left,
				const FStrategicMutualAidDiversionOptionView& Right)
			{
				return GuidLess(Left.DestinationBaseId, Right.DestinationBaseId);
			});
		if (!View.bCanDivertRelief)
		{
			const FStrategicMutualAidDiversionOptionView* ReasonOption =
				View.ReliefDiversionOptions.FindByPredicate(
					[&Convoy](const FStrategicMutualAidDiversionOptionView& Option)
					{
						return Option.DestinationBaseId != Convoy.DestinationBaseId
							&& !Option.UnavailableReason.IsEmpty();
					});
			if (ReasonOption == nullptr)
			{
				ReasonOption = View.ReliefDiversionOptions.FindByPredicate(
					[](const FStrategicMutualAidDiversionOptionView& Option)
					{
						return !Option.UnavailableReason.IsEmpty();
					});
			}
			if (ReasonOption != nullptr)
			{
				View.ReliefDiversionUnavailableReasonCode =
					ReasonOption->UnavailableReasonCode;
				View.ReliefDiversionUnavailableReason =
					ReasonOption->UnavailableReason;
			}
		}

		const auto AddWaypointOption =
			[&](const FGuid WaypointBaseId, const FString& WaypointBaseName,
				const EMutualAidRoutePolicy OnwardPolicy, const bool bDirectRoute)
		{
			FConfigureMutualAidRelayWaypointCommand Configure;
			Configure.ExpectedSequence = Campaign.CommandSequence;
			Configure.ConvoyId = Convoy.ConvoyId;
			Configure.WaypointBaseId = WaypointBaseId;
			Configure.OnwardRoutePolicy = OnwardPolicy;
			const FMutualAidRelayWaypointEvaluation WaypointEvaluation =
				FStrategicCommandService::EvaluateMutualAidRelayWaypoint(
					Campaign, Rules, Config, Configure);
			FStrategicMutualAidWaypointOptionView& Option =
				View.RelayWaypointOptions.AddDefaulted_GetRef();
			Option.bDirectRoute = bDirectRoute;
			Option.WaypointBaseId = WaypointBaseId;
			Option.WaypointBaseName = WaypointBaseName;
			Option.PolicyId = WaypointEvaluation.PolicyId;
			Option.OnwardRoutePolicy = OnwardPolicy;
			Option.OnwardRoutePolicyId =
				FStrategicCommandService::GetMutualAidRoutePolicyId(OnwardPolicy);
			Option.FirstLegRoutePressure =
				WaypointEvaluation.FirstLegRoutePressure;
			Option.bFirstLegInterdictionExpected =
				WaypointEvaluation.bFirstLegInterdictionExpected;
			Option.OnwardRoutePressure = WaypointEvaluation.OnwardRoutePressure;
			Option.bOnwardInterdictionExpected =
				WaypointEvaluation.bOnwardInterdictionExpected;
			Option.JourneySeconds = WaypointEvaluation.ProjectedJourneySeconds;
			Option.WaypointArrivalSeconds =
				WaypointEvaluation.ProjectedWaypointArrivalSeconds;
			Option.ArrivalShiftSeconds =
				WaypointEvaluation.TargetArrivalShiftSeconds;
			Option.AffectedConvoyCount = WaypointEvaluation.AffectedConvoyCount;
			Option.TotalArrivalShiftSeconds =
				WaypointEvaluation.TotalArrivalShiftSeconds;
			Option.bEnabled = WaypointEvaluation.bAllowed;
			if (WaypointEvaluation.ProjectedRelayQueue.bValid)
			{
				Option.ProjectedRelayQueue =
					WaypointEvaluation.ProjectedRelayQueue;
			}
			if (WaypointEvaluation.bAllowed)
			{
				View.bCanConfigureRelayWaypoint = true;
			}
			else if (!Option.ProjectedRelayQueue.bValid
				&& bDirectRoute && !Convoy.RelayWaypointBaseId.IsValid())
			{
				Option.ProjectedRelayQueue = View.RelayQueue;
			}
			if (!WaypointEvaluation.Diagnostics.IsEmpty())
			{
				Option.UnavailableReasonCode =
					WaypointEvaluation.Diagnostics[0].Code;
				Option.UnavailableReason =
					WaypointEvaluation.Diagnostics[0].Message;
			}
		};

		AddWaypointOption(
			FGuid(), FString(), Convoy.RoutePolicy, true);
		for (const FStrategicBaseState& Candidate : Campaign.Bases)
		{
			if (Candidate.BaseId == Convoy.SourceBaseId
				|| Candidate.BaseId == Convoy.DestinationBaseId)
			{
				continue;
			}
			for (const EMutualAidRoutePolicy OnwardPolicy :
				FStrategicCommandService::GetMutualAidRoutePolicies())
			{
				AddWaypointOption(
					Candidate.BaseId, Candidate.Name, OnwardPolicy, false);
			}
		}
		View.RelayWaypointOptions.Sort(
			[](const FStrategicMutualAidWaypointOptionView& Left,
				const FStrategicMutualAidWaypointOptionView& Right)
			{
				if (Left.bDirectRoute != Right.bDirectRoute)
				{
					return Left.bDirectRoute;
				}
				if (Left.WaypointBaseId != Right.WaypointBaseId)
				{
					return GuidLess(Left.WaypointBaseId, Right.WaypointBaseId);
				}
				return static_cast<uint8>(Left.OnwardRoutePolicy)
					< static_cast<uint8>(Right.OnwardRoutePolicy);
			});
		if (!View.bCanConfigureRelayWaypoint)
		{
			const FStrategicMutualAidWaypointOptionView* ReasonOption =
				View.RelayWaypointOptions.FindByPredicate(
					[](const FStrategicMutualAidWaypointOptionView& Option)
					{
						return Option.UnavailableReasonCode
							!= FName(TEXT("mutual_aid_relay_waypoint_same_plan"))
							&& !Option.UnavailableReason.IsEmpty();
					});
			if (ReasonOption == nullptr)
			{
				ReasonOption = View.RelayWaypointOptions.FindByPredicate(
					[](const FStrategicMutualAidWaypointOptionView& Option)
					{
						return !Option.UnavailableReason.IsEmpty();
					});
			}
			if (ReasonOption != nullptr)
			{
				View.RelayWaypointUnavailableReasonCode =
					ReasonOption->UnavailableReasonCode;
				View.RelayWaypointUnavailableReason =
					ReasonOption->UnavailableReason;
			}
		}
		for (const bool bEnableHandoff : { false, true })
		{
			FConfigureMutualAidBalancedHandoffCommand ConfigureHandoff;
			ConfigureHandoff.ExpectedSequence = Campaign.CommandSequence;
			ConfigureHandoff.ConvoyId = Convoy.ConvoyId;
			ConfigureHandoff.bEnabled = bEnableHandoff;
			const FMutualAidBalancedHandoffEvaluation HandoffEvaluation =
				FStrategicCommandService::EvaluateMutualAidBalancedHandoff(
					Campaign, Rules, ConfigureHandoff);
			FStrategicMutualAidBalancedHandoffOptionView& Option =
				View.BalancedHandoffOptions.AddDefaulted_GetRef();
			Option.bEnabledChoice = bEnableHandoff;
			Option.PolicyId = HandoffEvaluation.PolicyId;
			Option.WaypointQuantity = HandoffEvaluation.ProjectedHandoffQuantity;
			Option.FinalQuantity = HandoffEvaluation.ProjectedFinalQuantity;
			Option.HandoffStorage = HandoffEvaluation.HandoffStorage;
			Option.WaypointReservedStorage =
				HandoffEvaluation.ProjectedWaypointStorage.MutualAidReserved;
			Option.DestinationReservedStorage =
				HandoffEvaluation.ProjectedDestinationStorage.MutualAidReserved;
			Option.bEnabled = HandoffEvaluation.bAllowed;
			if (HandoffEvaluation.bAllowed)
			{
				View.bCanConfigureBalancedHandoff = true;
			}
			if (!HandoffEvaluation.Diagnostics.IsEmpty())
			{
				Option.UnavailableReasonCode =
					HandoffEvaluation.Diagnostics[0].Code;
				Option.UnavailableReason =
					HandoffEvaluation.Diagnostics[0].Message;
			}
		}
		if (!View.bCanConfigureBalancedHandoff)
		{
			const FStrategicMutualAidBalancedHandoffOptionView* ReasonOption =
				View.BalancedHandoffOptions.FindByPredicate(
					[](const FStrategicMutualAidBalancedHandoffOptionView& Option)
					{
						return Option.UnavailableReasonCode
							!= FName(TEXT("mutual_aid_balanced_handoff_same_plan"))
							&& !Option.UnavailableReason.IsEmpty();
					});
			if (ReasonOption == nullptr)
			{
				ReasonOption = View.BalancedHandoffOptions.FindByPredicate(
					[](const FStrategicMutualAidBalancedHandoffOptionView& Option)
					{
						return !Option.UnavailableReason.IsEmpty();
					});
			}
			if (ReasonOption != nullptr)
			{
				View.BalancedHandoffUnavailableReasonCode =
					ReasonOption->UnavailableReasonCode;
				View.BalancedHandoffUnavailableReason =
					ReasonOption->UnavailableReason;
			}
		}
		for (const EMutualAidRoutePolicy Policy :
			FStrategicCommandService::GetMutualAidRoutePolicies())
		{
			const FGuid CurrentLegDestinationBaseId =
				Convoy.RelayWaypointBaseId.IsValid()
					? Convoy.RelayWaypointBaseId
					: Convoy.DestinationBaseId;
			const FMutualAidRouteEvaluation Route =
				FStrategicCommandService::EvaluateMutualAidRoute(
					Campaign, Config, CurrentLegOriginBaseId,
					CurrentLegDestinationBaseId,
					Policy, Convoy.bSignalEscort);
			if (!Route.bValid)
			{
				continue;
			}
			FRetuneMutualAidConvoyCommand Retune;
			Retune.ExpectedSequence = Campaign.CommandSequence;
			Retune.ConvoyId = Convoy.ConvoyId;
			Retune.RoutePolicy = Policy;
			const FThreadlineRetuneEvaluation RetuneEvaluation =
				FStrategicCommandService::EvaluateThreadlineRetune(
					Campaign, Rules, Config, Retune);
			FStrategicMutualAidRouteOptionView& RouteView =
				View.RetuneRoutes.AddDefaulted_GetRef();
			RouteView.Policy = Route.Policy;
			RouteView.PolicyId = Route.PolicyId;
			RouteView.TransitSeconds = Route.TransitSeconds;
			RouteView.BaselinePressure = Route.BaselinePressure;
			RouteView.ExposureModifier = Route.ExposureModifier;
			RouteView.RoutePressure = Route.RoutePressure;
			RouteView.bInterdictionExpected = Route.bInterdictionExpected;
			RouteView.InterdictionDelaySeconds = Route.InterdictionDelaySeconds;
			RouteView.SignalEscortCost = Convoy.SignalEscortCost;
			RouteView.bSignalEscortAffordable = true;
			RouteView.bEnabled = RetuneEvaluation.bAllowed;
			if (RetuneEvaluation.bAllowed)
			{
				RouteView.RelayQueue = RetuneEvaluation.ProjectedRelayQueue;
				RouteView.EscortedEstimatedArrivalSeconds =
					RetuneEvaluation.ProjectedRelayQueue.EstimatedArrivalSeconds;
				View.bCanRetune = true;
			}
			else if (Policy == Convoy.RoutePolicy)
			{
				RouteView.RelayQueue = View.RelayQueue;
			}
			if (!RetuneEvaluation.Diagnostics.IsEmpty())
			{
				RouteView.UnavailableReasonCode = RetuneEvaluation.Diagnostics[0].Code;
				RouteView.UnavailableReason = RetuneEvaluation.Diagnostics[0].Message;
				if (Policy != Convoy.RoutePolicy
					&& View.RetuneUnavailableReasonCode.IsNone())
				{
					View.RetuneUnavailableReasonCode = RouteView.UnavailableReasonCode;
					View.RetuneUnavailableReason = RouteView.UnavailableReason;
				}
			}
		}
	}
	Snapshot.MutualAidConvoys.Sort(
		[](const FStrategicMutualAidConvoyView& Left, const FStrategicMutualAidConvoyView& Right)
		{
			if (Left.SourceBaseId != Right.SourceBaseId)
			{
				return GuidLess(Left.SourceBaseId, Right.SourceBaseId);
			}
			if (Left.DispatchSequence != Right.DispatchSequence)
			{
				return Left.DispatchSequence < Right.DispatchSequence;
			}
			return GuidLess(Left.ConvoyId, Right.ConvoyId);
		});

	for (const FPersonnelState& Person : Campaign.Personnel)
	{
		FStrategicPersonnelView& View = Snapshot.Personnel.AddDefaulted_GetRef();
		View.PersonnelId = Person.PersonnelId;
		View.BaseId = Person.BaseId;
		View.DisplayName = Person.DisplayName;
		View.RoleId = Person.RoleId;
		View.Status = PersonnelStatus(Person.Status);
		View.StatusType = Person.Status;
		View.Rank = Person.Rank;
		View.Missions = Person.Missions;
		View.Kills = Person.Kills;
		View.Experience = Person.Experience;
		View.ServiceHistory = FPersonnelServiceHistory::Project(Person.Missions);
		View.CurrentHealth = Person.CurrentHealth;
		View.MaxHealth = Person.MaxHealth;
		View.Accuracy = Person.Accuracy;
		View.Resolve = Person.Resolve;
		View.Mobility = Person.Mobility;
		View.Strength = Person.Strength;
		View.RemainingRecoverySeconds = Person.RemainingRecoverySeconds;
		View.RecoveryPlan = FPersonnelRecoveryPlan::Evaluate(Campaign, Config, Person.PersonnelId);
		View.RemainingTrainingSeconds = Person.RemainingTrainingSeconds;
		View.TrainingFocus = Person.TrainingFocus;
		View.Stewardship = FPersonnelStewardship::Evaluate(Campaign, Rules, Config, Person.PersonnelId);
		View.StewardshipToursCompleted = Person.StewardshipToursCompleted;
		View.PendingDoctrineChoices = Person.PendingDoctrineChoices;
		TArray<FName> DoctrineIds;
		Rules.PersonnelDoctrines.GenerateKeyArray(DoctrineIds);
		DoctrineIds.Sort(FNameLexicalLess());
		for (const FName DoctrineId : DoctrineIds)
		{
			if (Person.PendingDoctrineChoices <= 0 && !Person.DoctrineSelections.Contains(DoctrineId))
			{
				continue;
			}
			const FPersonnelDoctrineRule& Doctrine = Rules.PersonnelDoctrines.FindChecked(DoctrineId);
			FStrategicPersonnelDoctrineView& Option = View.DoctrineOptions.AddDefaulted_GetRef();
			Option.DoctrineId = DoctrineId;
			Option.DisplayName = RuleName(Doctrine.DisplayName, DoctrineId);
			Option.Summary = Doctrine.Summary;
			Option.MaximumSelections = Doctrine.MaxSelections;
			for (const FName SelectedDoctrineId : Person.DoctrineSelections)
			{
				Option.CurrentSelections += SelectedDoctrineId == DoctrineId ? 1 : 0;
			}
			Option.MaxHealthBonus = Doctrine.MaxHealthBonus;
			Option.AccuracyBonus = Doctrine.AccuracyBonus;
			Option.ResolveBonus = Doctrine.ResolveBonus;
			Option.MobilityBonus = Doctrine.MobilityBonus;
			Option.StrengthBonus = Doctrine.StrengthBonus;
			FSelectPersonnelDoctrineCommand Command;
			Command.ExpectedSequence = Campaign.CommandSequence;
			Command.PersonnelId = Person.PersonnelId;
			Command.DoctrineId = DoctrineId;
			const FPersonnelDoctrineEvaluation Evaluation =
				FStrategicCommandService::EvaluatePersonnelDoctrine(Campaign, Rules, Command);
			Option.bEnabled = Evaluation.bAllowed;
			if (!Evaluation.bAllowed)
			{
				if (Evaluation.Diagnostics.IsEmpty())
				{
					Option.UnavailableReasonCode = TEXT("personnel_doctrine_choice_unavailable");
					Option.UnavailableReason = TEXT("This field doctrine cannot currently be selected.");
				}
				else
				{
					Option.UnavailableReasonCode = Evaluation.Diagnostics[0].Code;
					Option.UnavailableReason = Evaluation.Diagnostics[0].Message;
				}
			}
		}
		BuildCommendationViews(Person.Commendations, Rules, View.Commendations);
		View.bAssignedToCraft = IsPersonnelAssignedToCraft(Campaign, Person.PersonnelId);
		View.EquippedItemIds = Person.EquippedItems;
		for (const FName ItemId : Person.EquippedItems)
		{
			const FItemRule* Item = Rules.Items.Find(ItemId);
			View.EquippedItemNames.Add(Item != nullptr ? RuleName(Item->DisplayName, ItemId) : HumanizeId(ItemId));
		}
		if (const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Person.RoleId))
		{
			View.RoleDisplayName = RuleName(Role->DisplayName, Person.RoleId);
			View.RoleCategory = Role->Category;
			Snapshot.MonthlyOutgoings = SaturatingAdd(
				Snapshot.MonthlyOutgoings, Role->MonthlySalary);
		}
		else
		{
			View.RoleDisplayName = HumanizeId(Person.RoleId);
		}
	}
	Snapshot.Personnel.Sort([](const FStrategicPersonnelView& Left, const FStrategicPersonnelView& Right)
	{
		return Left.DisplayName == Right.DisplayName
			? GuidLess(Left.PersonnelId, Right.PersonnelId)
			: Left.DisplayName < Right.DisplayName;
	});

	for (const FMemorialRecord& Record : Campaign.Memorial)
	{
		FStrategicMemorialView& View = Snapshot.Memorial.AddDefaulted_GetRef();
		View.PersonnelId = Record.PersonnelId;
		View.DisplayName = Record.DisplayName;
		View.RoleId = Record.RoleId;
		View.RoleDisplayName = HumanizeId(Record.RoleId);
		if (const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Record.RoleId))
		{
			View.RoleDisplayName = RuleName(Role->DisplayName, Record.RoleId);
		}
		View.Rank = Record.Rank;
		View.Missions = Record.Missions;
		View.Kills = Record.Kills;
		View.ServiceHistory = FPersonnelServiceHistory::Project(Record.Missions);
		View.StewardshipToursCompleted = Record.StewardshipToursCompleted;
		View.DoctrineSelections = Record.DoctrineSelections;
		BuildCommendationViews(Record.Commendations, Rules, View.Commendations);
		View.DeathUtc = Record.DeathUtc;
		View.CauseId = Record.CauseId;
		View.CauseDisplayName = HumanizeId(Record.CauseId);
	}
	Snapshot.Memorial.Sort([](const FStrategicMemorialView& Left, const FStrategicMemorialView& Right)
	{
		if (Left.DeathUtc != Right.DeathUtc)
		{
			return Left.DeathUtc > Right.DeathUtc;
		}
		return GuidLess(Left.PersonnelId, Right.PersonnelId);
	});

	const FCraftServiceQueueSnapshot CraftServiceQueue =
		FCraftServiceQueue::Evaluate(Campaign, Rules);
	for (const FCraftState& Craft : Campaign.Craft)
	{
		FStrategicCraftView& View = Snapshot.Craft.AddDefaulted_GetRef();
		View.CraftId = Craft.CraftId;
		View.BaseId = Craft.BaseId;
		View.CraftRuleId = Craft.CraftRuleId;
		View.DisplayName = Craft.DisplayName;
		View.Status = CraftStatus(Craft.Status);
		View.StatusType = Craft.Status;
		View.CurrentHull = Craft.CurrentHull;
		View.CurrentFuel = Craft.CurrentFuel;
		View.AssignedAgents = Craft.AssignedAgentIds.Num();
		View.bHasPilot = Craft.AssignedPilotId.IsValid();
		View.AssignedPilotId = Craft.AssignedPilotId;
		View.AssignedAgentIds = Craft.AssignedAgentIds;
		const FStrategicBaseState* CraftBaseState = FindBase(Campaign, Craft.BaseId);
		View.Mentorship = FPersonnelMentorship::Evaluate(Campaign, Craft.AssignedAgentIds);
		View.LegacyRelay = FPersonnelLegacyRelay::Evaluate(Campaign, Rules, Craft.AssignedAgentIds);
		View.SquadBonds = FPersonnelSquadBond::Evaluate(Campaign, Craft.AssignedAgentIds);
		if (CraftBaseState != nullptr && BasesWithValidInfrastructure.Contains(CraftBaseState))
		{
			if (const FCraftServiceQueueView* ServiceQueue = CraftServiceQueue.FindCraft(Craft.CraftId))
			{
				View.ServiceQueue = *ServiceQueue;
			}
		}
		View.RemainingRouteSeconds = Craft.RemainingRouteSeconds;
		View.RemainingRepairSeconds = FMath::Max<int64>(0, Craft.RemainingRepairSeconds);
		View.RemainingRefuelSeconds = FMath::Max<int64>(0, Craft.RemainingRefuelSeconds);
		View.RemainingServiceSeconds = FMath::Max(
			View.RemainingRepairSeconds, View.RemainingRefuelSeconds);
		View.bSalvageDispositionAvailable = Craft.Status == ECraftStatus::Grounded
			&& !Craft.PendingSalvage.IsEmpty();
		const FCraftRule* Rule = Rules.Craft.Find(Craft.CraftRuleId);
		if (Rule != nullptr)
		{
			View.TypeDisplayName = RuleName(Rule->DisplayName, Craft.CraftRuleId);
			View.MaxHull = Rule->MaxHull;
			View.FuelCapacity = Rule->FuelCapacity;
			View.AgentCapacity = Rule->AgentCapacity;
			const bool bRepairActive = Craft.RemainingRepairSeconds > 0
				&& Craft.CurrentHull > 0 && Craft.CurrentHull < Rule->MaxHull;
			const bool bRefuelActive = Craft.RemainingRefuelSeconds > 0
				&& Craft.CurrentFuel >= 0 && Craft.CurrentFuel < Rule->FuelCapacity;
			const bool bRepairStateConsistent = bRepairActive
				|| (Craft.RemainingRepairSeconds == 0 && Craft.CurrentHull == Rule->MaxHull);
			const bool bRefuelStateConsistent = bRefuelActive
				|| (Craft.RemainingRefuelSeconds == 0 && Craft.CurrentFuel == Rule->FuelCapacity);
			if (Craft.Status == ECraftStatus::Servicing
				&& (bRepairActive || bRefuelActive)
				&& bRepairStateConsistent && bRefuelStateConsistent)
			{
				const int64 RepairRefund = bRepairActive
					? SafeProduct(NonNegativeDifference(
						static_cast<int64>(Rule->MaxHull), static_cast<int64>(Craft.CurrentHull)),
						Rule->RepairCostPerHull)
					: 0;
				const int64 RefuelRefund = bRefuelActive
					? SafeProduct(NonNegativeDifference(
						static_cast<int64>(Rule->FuelCapacity), static_cast<int64>(Craft.CurrentFuel)),
						Rule->RefuelCostPerUnit)
					: 0;
				View.ServiceCancellationRefund = RepairRefund > MAX_int64 - RefuelRefund
					? MAX_int64 : RepairRefund + RefuelRefund;
				View.bCanCancelService = true;
			}
			Snapshot.MonthlyOutgoings = SaturatingAdd(
				Snapshot.MonthlyOutgoings, Rule->MonthlyMaintenance);
		}
		else
		{
			View.TypeDisplayName = HumanizeId(Craft.CraftRuleId);
		}
		const FStrategicBaseView* CraftBaseView = Snapshot.Bases.FindByPredicate(
			[&Craft](const FStrategicBaseView& BaseView) { return BaseView.BaseId == Craft.BaseId; });
		TMap<FName, int32> MountCounts;
		for (const FName ItemId : Craft.EquipmentItems)
		{
			const FItemRule* Item = Rules.Items.Find(ItemId);
			if (Item != nullptr && Item->IsCraftWeapon())
			{
				++MountCounts.FindOrAdd(ItemId);
			}
		}
		TMap<FName, int64> BaseAmmunition;
		if (CraftBaseState != nullptr)
		{
			for (const FInventoryStack& Stack : CraftBaseState->Inventory)
			{
				BaseAmmunition.FindOrAdd(Stack.ItemId) += FMath::Max(0, Stack.Quantity);
			}
		}
		TMap<FName, int64> RemainingAmmunition = BaseAmmunition;
		TArray<FName> WeaponIds;
		MountCounts.GetKeys(WeaponIds);
		WeaponIds.Sort(FNameLexicalLess());
		for (const FName WeaponId : WeaponIds)
		{
			const FItemRule* Weapon = Rules.Items.Find(WeaponId);
			if (Weapon == nullptr || !Weapon->IsCraftWeapon() || Weapon->MagazineCapacity <= 0)
			{
				continue;
			}
			FStrategicCraftWeaponView& WeaponView = View.Weapons.AddDefaulted_GetRef();
			WeaponView.WeaponItemId = WeaponId;
			WeaponView.WeaponDisplayName = RuleName(Weapon->DisplayName, WeaponId);
			WeaponView.AmmunitionItemId = Weapon->AmmunitionItemId;
			if (const FItemRule* Ammunition = Rules.Items.Find(Weapon->AmmunitionItemId))
			{
				WeaponView.AmmunitionDisplayName = RuleName(Ammunition->DisplayName, Weapon->AmmunitionItemId);
			}
			else
			{
				WeaponView.AmmunitionDisplayName = HumanizeId(Weapon->AmmunitionItemId);
			}
			WeaponView.MountCount = MountCounts.FindChecked(WeaponId);
			WeaponView.Capacity = static_cast<int64>(Weapon->MagazineCapacity) * WeaponView.MountCount;
			const FCraftWeaponState* WeaponState = Craft.WeaponStates.FindByPredicate(
				[WeaponId](const FCraftWeaponState& Entry) { return Entry.WeaponItemId == WeaponId; });
			WeaponView.LoadedAmmunition = WeaponState != nullptr
				? FMath::Clamp<int64>(WeaponState->Ammunition, 0, WeaponView.Capacity)
				: 0;
			WeaponView.MissingAmmunition = WeaponView.Capacity - WeaponView.LoadedAmmunition;
			WeaponView.BaseAvailableAmmunition = BaseAmmunition.FindRef(Weapon->AmmunitionItemId);
			int64& Available = RemainingAmmunition.FindOrAdd(Weapon->AmmunitionItemId);
			WeaponView.LoadableAmmunition = FMath::Min(WeaponView.MissingAmmunition, Available);
			Available -= WeaponView.LoadableAmmunition;
			View.TotalAmmunitionLoaded += WeaponView.LoadedAmmunition;
			View.TotalAmmunitionCapacity += WeaponView.Capacity;
			View.TotalAmmunitionMissing += WeaponView.MissingAmmunition;
			View.TotalAmmunitionLoadable += WeaponView.LoadableAmmunition;
		}
		const bool bTurnaroundAvailable = Craft.Status == ECraftStatus::Grounded
			&& Craft.PendingSalvage.IsEmpty();
		View.bCanRearmFully = bTurnaroundAvailable && View.TotalAmmunitionMissing > 0
			&& View.TotalAmmunitionLoadable == View.TotalAmmunitionMissing;
		View.bCanLoadAvailableAmmunition = bTurnaroundAvailable
			&& View.TotalAmmunitionLoadable > 0;
		for (const FInventoryStack& Stack : Craft.PendingSalvage)
		{
			FStrategicCraftSalvageView& Salvage = View.PendingSalvage.AddDefaulted_GetRef();
			Salvage.ItemId = Stack.ItemId;
			Salvage.Quantity = FMath::Max(0, Stack.Quantity);
			if (const FItemRule* Item = Rules.Items.Find(Stack.ItemId))
			{
				Salvage.DisplayName = RuleName(Item->DisplayName, Stack.ItemId);
				Salvage.UnitStorage = FMath::Max(0, Item->Mass);
				Salvage.TotalStorage = SafeProduct(Salvage.UnitStorage, Stack.Quantity);
				Salvage.UnitSellValue = FMath::Max(0, Item->SellValue);
				Salvage.TotalSellValue = SafeProduct(Salvage.UnitSellValue, Stack.Quantity);
				Salvage.bCanRetainAtBase = View.bSalvageDispositionAvailable
					&& CanApplyStorageDelta(CraftBaseView, Salvage.TotalStorage);
				Salvage.bCanSell = View.bSalvageDispositionAvailable && Salvage.UnitSellValue > 0;
			}
			else
			{
				Salvage.DisplayName = HumanizeId(Stack.ItemId);
			}
		}

		const FStrategicBaseState* Base = FindBase(Campaign, Craft.BaseId);
		int32 Longitude = Base != nullptr ? Base->LongitudeMilliDegrees : 0;
		int32 Latitude = Base != nullptr ? Base->LatitudeMilliDegrees : 0;
		int32 TargetLongitude = Longitude;
		int32 TargetLatitude = Latitude;
		bool bHasTarget = false;
		if (const FStrategicContactState* TargetContact = FindContact(Campaign, Craft.TargetContactId))
		{
			if (TargetContact->Status != EStrategicContactStatus::Hidden)
			{
				TargetLongitude = TargetContact->LongitudeMilliDegrees;
				TargetLatitude = TargetContact->LatitudeMilliDegrees;
				bHasTarget = true;
			}
		}
		else if (const FStrategicSiteState* TargetSite = FindSite(Campaign, Craft.TargetSiteId))
		{
			TargetLongitude = TargetSite->LongitudeMilliDegrees;
			TargetLatitude = TargetSite->LatitudeMilliDegrees;
			bHasTarget = true;
		}
		if (bHasTarget && Craft.ReservedReturnSeconds > 0)
		{
			const float RouteProgress = Craft.Status == ECraftStatus::OnSite
				? 1.0f
				: FMath::Clamp(1.0f - static_cast<float>(Craft.RemainingRouteSeconds)
					/ static_cast<float>(Craft.ReservedReturnSeconds), 0.0f, 1.0f);
			Longitude = FMath::RoundToInt(FMath::Lerp(static_cast<float>(Longitude), static_cast<float>(TargetLongitude), RouteProgress));
			Latitude = FMath::RoundToInt(FMath::Lerp(static_cast<float>(Latitude), static_cast<float>(TargetLatitude), RouteProgress));
			FStrategicGlobeRouteView& Route = Snapshot.GlobeRoutes.AddDefaulted_GetRef();
			Route.EntityId = Craft.CraftId;
			Route.OriginLongitudeMilliDegrees = Base != nullptr ? Base->LongitudeMilliDegrees : Longitude;
			Route.OriginLatitudeMilliDegrees = Base != nullptr ? Base->LatitudeMilliDegrees : Latitude;
			Route.DestinationLongitudeMilliDegrees = TargetLongitude;
			Route.DestinationLatitudeMilliDegrees = TargetLatitude;
			Route.Progress = RouteProgress;
			Route.bPlayerControlled = true;
		}
		AddGlobeMarker(Snapshot, EStrategicGlobeMarkerType::Craft, Craft.CraftId, Craft.DisplayName,
			View.Status, Longitude, Latitude, Craft.Status == ECraftStatus::Intercepting || Craft.Status == ECraftStatus::OnSite);
	}
	Snapshot.Craft.Sort([](const FStrategicCraftView& Left, const FStrategicCraftView& Right)
	{
		return Left.DisplayName == Right.DisplayName
			? GuidLess(Left.CraftId, Right.CraftId)
			: Left.DisplayName < Right.DisplayName;
	});

	for (const FStrategicContactState& Contact : Campaign.StrategicContacts)
	{
		if (Contact.Status == EStrategicContactStatus::Hidden)
		{
			continue;
		}
		FStrategicContactView& View = Snapshot.Contacts.AddDefaulted_GetRef();
		View.ContactId = Contact.ContactId;
		View.ContactRuleId = Contact.ContactRuleId;
		View.Status = ContactStatus(Contact.Status);
		View.StatusType = Contact.Status;
		View.LongitudeMilliDegrees = Contact.LongitudeMilliDegrees;
		View.LatitudeMilliDegrees = Contact.LatitudeMilliDegrees;
		View.CurrentHull = Contact.CurrentHull;
		View.RouteProgress = Progress(Contact.ElapsedRouteSeconds, Contact.TotalRouteSeconds);
		const FAdversaryMissionState* Mission = Campaign.AdversaryMissions.FindByPredicate(
			[&Contact](const FAdversaryMissionState& Entry) { return Entry.ContactId == Contact.ContactId; });
		if (Mission != nullptr && Mission->TargetBaseId.IsValid())
		{
			View.bTargetsBase = true;
			View.TargetBaseId = Mission->TargetBaseId;
			if (const FStrategicBaseState* TargetBase = FindBase(Campaign, Mission->TargetBaseId))
			{
				View.TargetBaseName = TargetBase->Name;
			}
		}
		if (Mission != nullptr)
		{
			const FAdversaryMissionRule* MissionRule = Rules.AdversaryMissions.Find(Mission->MissionRuleId);
			if (MissionRule != nullptr && !MissionRule->PlanId.IsNone())
			{
				View.PlanId = MissionRule->PlanId;
				View.PlanStage = MissionRule->PlanStage;
				if (const FAdversaryPlanRule* Plan = Rules.AdversaryPlans.Find(MissionRule->PlanId))
				{
					View.PlanDisplayName = RuleName(Plan->DisplayName, Plan->Identity.RuleId);
				}
				else
				{
					View.PlanDisplayName = HumanizeId(MissionRule->PlanId);
				}

				View.EscapeBranchMissionRuleId = MissionRule->EscapeBranchMissionRuleId;
				if (const FAdversaryMissionRule* Branch = Rules.AdversaryMissions.Find(MissionRule->EscapeBranchMissionRuleId))
				{
					View.EscapeBranchMissionName = RuleName(Branch->DisplayName, Branch->Identity.RuleId);
				}
				View.ThwartBranchMissionRuleId = MissionRule->ThwartBranchMissionRuleId;
				if (const FAdversaryMissionRule* Branch = Rules.AdversaryMissions.Find(MissionRule->ThwartBranchMissionRuleId))
				{
					View.ThwartBranchMissionName = RuleName(Branch->DisplayName, Branch->Identity.RuleId);
				}
			}
			if (MissionRule != nullptr
				&& (MissionRule->CompactPeerSupportLossOnEscape > 0
					|| MissionRule->WithdrawnCompactSupportGainOnThwarted > 0))
			{
				View.bHasCoalitionCounterplay = true;
				FName TargetRegionId = MissionRule->TargetRegionId;
				if (Mission->TargetBaseId.IsValid())
				{
					if (const FStrategicBaseState* TargetBase = FindBase(
						Campaign, Mission->TargetBaseId))
					{
						TargetRegionId = TargetBase->RegionId;
					}
				}

				int64 ScaledPeerSupportLoss = 0;
				if (Campaign.bHorizonCompactRatified
					&& MissionRule->CompactPeerSupportLossOnEscape > 0
					&& FStrategicCommandService::ScaleAdversaryEscapeConsequence(
						MissionRule->CompactPeerSupportLossOnEscape,
						Campaign.Difficulty, Config, ScaledPeerSupportLoss)
					&& ScaledPeerSupportLoss > 0 && ScaledPeerSupportLoss <= MAX_int32)
				{
					for (const FRegionalMandateState& Mandate : Campaign.RegionalMandates)
					{
						if (!Mandate.bResilienceCharterSigned
							|| Mandate.bHorizonCompactMemberWithdrawn
							|| Mandate.RegionId == TargetRegionId)
						{
							continue;
						}
						FStrategicCoalitionCounterplayMemberView& Member =
							View.EscapeStrainMembers.AddDefaulted_GetRef();
						Member.RegionId = Mandate.RegionId;
						Member.CurrentSupport = Mandate.Support;
						Member.ProjectedSupport = static_cast<int32>(FMath::Max<int64>(
							0, static_cast<int64>(Mandate.Support) - ScaledPeerSupportLoss));
						Member.bWouldWithdraw =
							Member.ProjectedSupport < Config.HorizonCompactWithdrawalSupportThreshold;
					}
				}

				if (Campaign.bHorizonCompactRatified
					&& MissionRule->WithdrawnCompactSupportGainOnThwarted > 0)
				{
					for (const FRegionalMandateState& Mandate : Campaign.RegionalMandates)
					{
						if (!Mandate.bResilienceCharterSigned
							|| !Mandate.bHorizonCompactMemberWithdrawn)
						{
							continue;
						}
						const int32 ProjectedSupport = static_cast<int32>(FMath::Clamp<int64>(
							static_cast<int64>(Mandate.Support)
								+ MissionRule->WithdrawnCompactSupportGainOnThwarted,
							0,
							100));
						if (ProjectedSupport == Mandate.Support)
						{
							continue;
						}
						FStrategicCoalitionCounterplayMemberView& Member =
							View.ThwartRecoveryMembers.AddDefaulted_GetRef();
						Member.RegionId = Mandate.RegionId;
						Member.CurrentSupport = Mandate.Support;
						Member.ProjectedSupport = ProjectedSupport;
						Member.bRemainsWithdrawn = true;
					}
				}
				View.EscapeStrainMembers.Sort(
					[](const FStrategicCoalitionCounterplayMemberView& Left,
						const FStrategicCoalitionCounterplayMemberView& Right)
					{
						return Left.RegionId.LexicalLess(Right.RegionId);
					});
				View.ThwartRecoveryMembers.Sort(
					[](const FStrategicCoalitionCounterplayMemberView& Left,
						const FStrategicCoalitionCounterplayMemberView& Right)
					{
						return Left.RegionId.LexicalLess(Right.RegionId);
					});
			}
		}
		View.bAssaultPending = Campaign.BaseAssaults.ContainsByPredicate(
			[&Contact](const FBaseAssaultState& Assault) { return Assault.ContactId == Contact.ContactId; });
		if (View.bAssaultPending)
		{
			View.Status = TEXT("At base perimeter");
		}
		else if (Contact.Status == EStrategicContactStatus::Engaged)
		{
			const FInterceptionCoordinationPolicy Coordination =
				FStrategicCommandService::EvaluateInterceptionCoordination(
					Campaign, Contact.ContactId);
			View.InterceptionCoordination.bValid = Coordination.bValid;
			View.InterceptionCoordination.bActive = Coordination.bActive;
			View.InterceptionCoordination.PolicyId = Coordination.PolicyId;
			View.InterceptionCoordination.OnStationCraftCount = Coordination.OnStationCraftCount;
			View.InterceptionCoordination.SupportingCraftCount = Coordination.SupportingCraftCount;
			View.InterceptionCoordination.OutgoingAccuracyModifier = Coordination.OutgoingAccuracyModifier;
			View.InterceptionCoordination.IncomingAccuracyModifier = Coordination.IncomingAccuracyModifier;
			if (Coordination.bActive)
			{
				View.InterceptionCoordination.DisplayName = TEXT("Linked Wing");
				View.InterceptionCoordination.Summary = TEXT(
					"Each support craft improves formation fire and reduces incoming accuracy, capped at three supports.");
			}
			else if (Coordination.bValid)
			{
				View.InterceptionCoordination.DisplayName = TEXT("Solo Vector");
				View.InterceptionCoordination.Summary = TEXT(
					"A lone interceptor receives no formation coordination modifier.");
			}
			View.InterceptionCraftCount = Coordination.OnStationCraftCount;
			const FInterceptionContactManeuverPolicy ContactManeuver =
				FStrategicCommandService::EvaluateInterceptionContactManeuver(
					Campaign, Rules, Contact.ContactId);
			View.InterceptionContactManeuver.bValid = ContactManeuver.bValid;
			View.InterceptionContactManeuver.Maneuver = ContactManeuver.Maneuver;
			View.InterceptionContactManeuver.PolicyId = ContactManeuver.PolicyId;
			View.InterceptionContactManeuver.CompletedCombatRounds =
				ContactManeuver.CompletedCombatRounds;
			View.InterceptionContactManeuver.CurrentHull = ContactManeuver.CurrentHull;
			View.InterceptionContactManeuver.MaximumHull = ContactManeuver.MaximumHull;
			View.InterceptionContactManeuver.OutgoingAccuracyModifier =
				ContactManeuver.OutgoingAccuracyModifier;
			View.InterceptionContactManeuver.IncomingAccuracyModifier =
				ContactManeuver.IncomingAccuracyModifier;
			switch (ContactManeuver.Maneuver)
			{
			case EInterceptionContactManeuver::VectorSurvey:
				View.InterceptionContactManeuver.DisplayName = TEXT("Vector Survey");
				View.InterceptionContactManeuver.Summary = TEXT(
					"The contact holds a readable opening vector with no accuracy modifier.");
				break;
			case EInterceptionContactManeuver::SignalShear:
				View.InterceptionContactManeuver.DisplayName = TEXT("Signal Shear");
				View.InterceptionContactManeuver.Summary = TEXT(
					"After two rounds above breakline integrity, the contact reduces both sides' accuracy.");
				break;
			case EInterceptionContactManeuver::BreaklineCounter:
				View.InterceptionContactManeuver.DisplayName = TEXT("Breakline Counter");
				View.InterceptionContactManeuver.Summary = TEXT(
					"At 35% hull or lower, the contact exposes itself for an aggressive counterattack.");
				break;
			default:
				break;
			}

			const EInterceptionWithdrawalDoctrine OrderedWithdrawalDoctrines[] = {
				EInterceptionWithdrawalDoctrine::FormationBreak,
				EInterceptionWithdrawalDoctrine::EvasiveRelay,
				EInterceptionWithdrawalDoctrine::WakeSnare
			};
			for (const EInterceptionWithdrawalDoctrine Doctrine : OrderedWithdrawalDoctrines)
			{
				FWithdrawInterceptionCommand WithdrawalCommand;
				WithdrawalCommand.ExpectedSequence = Campaign.CommandSequence;
				WithdrawalCommand.ContactId = Contact.ContactId;
				WithdrawalCommand.Doctrine = Doctrine;
				const FInterceptionWithdrawalEvaluation Evaluation =
					FStrategicCommandService::EvaluateInterceptionWithdrawal(
						Campaign, Rules, WithdrawalCommand);
				FStrategicInterceptionWithdrawalView& Option =
					View.InterceptionWithdrawals.AddDefaulted_GetRef();
				Option.Doctrine = Doctrine;
				Option.PolicyId = Evaluation.PolicyId;
				Option.bEnabled = Evaluation.bCanExecute;
				Option.UnavailableReasonCode = Evaluation.UnavailableReasonCode;
				Option.UnavailableReason = Evaluation.UnavailableReason;
				Option.OnStationCraftCount = Evaluation.OnStationCraftCount;
				Option.WithdrawingCraftCount = Evaluation.WithdrawingCraftCount;
				Option.RemainingCraftCount = Evaluation.RemainingCraftCount;
				Option.CompletedCombatRounds = Evaluation.CompletedCombatRounds;
				Option.RequiredCombatRounds = Evaluation.RequiredCombatRounds;
				Option.ContactRouteDelaySeconds = Evaluation.ContactRouteDelaySeconds;
				Option.PriorityCraftId = Evaluation.PriorityCraftId;
				Option.PriorityCraftCurrentHull = Evaluation.PriorityCraftCurrentHull;
				Option.PriorityCraftMaximumHull = Evaluation.PriorityCraftMaximumHull;
				switch (Doctrine)
				{
				case EInterceptionWithdrawalDoctrine::FormationBreak:
					Option.DisplayName = TEXT("Formation Break");
					Option.Summary = TEXT("Return every on-station craft and restore the contact to detected state.");
					View.InterceptionCraftCount = Evaluation.OnStationCraftCount;
					View.bCanWithdrawInterception = Evaluation.bCanExecute;
					break;
				case EInterceptionWithdrawalDoctrine::EvasiveRelay:
					Option.DisplayName = TEXT("Evasive Relay");
					Option.Summary = TEXT("Return the lowest-integrity craft while the rest hold engagement.");
					if (const FCraftState* PriorityCraft = Campaign.Craft.FindByPredicate(
						[&Evaluation](const FCraftState& Craft)
						{
							return Craft.CraftId == Evaluation.PriorityCraftId;
						}))
					{
						Option.PriorityCraftDisplayName = PriorityCraft->DisplayName;
					}
					break;
				case EInterceptionWithdrawalDoctrine::WakeSnare:
					Option.DisplayName = TEXT("Wake Snare");
					Option.Summary = TEXT(
						"After two combat rounds, return the formation and force the contact off-course by up to 30 minutes.");
					break;
				default:
					break;
				}
			}

			const EInterceptionPosture OrderedPostures[] = {
				EInterceptionPosture::StandOffScreen,
				EInterceptionPosture::BalancedVector,
				EInterceptionPosture::CloseAssault
			};
			for (const EInterceptionPosture Posture : OrderedPostures)
			{
				const FInterceptionPosturePolicy Policy =
					FStrategicCommandService::GetInterceptionPosturePolicy(Posture);
				if (!Policy.bValid)
				{
					continue;
				}
				FStrategicInterceptionPostureView& Option =
					View.InterceptionPostures.AddDefaulted_GetRef();
				Option.Posture = Posture;
				Option.PolicyId = Policy.PolicyId;
				Option.OutgoingAccuracyModifier = Policy.OutgoingAccuracyModifier;
				Option.IncomingAccuracyModifier = Policy.IncomingAccuracyModifier;
				switch (Posture)
				{
				case EInterceptionPosture::StandOffScreen:
					Option.DisplayName = TEXT("Stand-off Screen");
					Option.Summary = TEXT("Widen separation to reduce both outgoing and incoming accuracy.");
					break;
				case EInterceptionPosture::BalancedVector:
					Option.DisplayName = TEXT("Balanced Vector");
					Option.Summary = TEXT("Hold the current vector with no accuracy modifier.");
					break;
				case EInterceptionPosture::CloseAssault:
					Option.DisplayName = TEXT("Close Assault");
					Option.Summary = TEXT("Collapse separation to improve outgoing fire while exposing the formation.");
					break;
				default:
					break;
				}
			}
		}
		const FContactRule* Rule = Rules.Contacts.Find(Contact.ContactRuleId);
		if (Rule != nullptr)
		{
			View.DisplayName = RuleName(Rule->DisplayName, Contact.ContactRuleId);
			View.MaxHull = Rule->MaxHull;
			View.ThreatRating = Rule->ThreatRating;
			if (Mission != nullptr)
			{
				const FAdversaryMissionRule* MissionRule = Rules.AdversaryMissions.Find(Mission->MissionRuleId);
				if (MissionRule != nullptr && MissionRule->bCreatesLandingSiteOnArrival)
				{
					View.bCanShadowToLanding = true;
					View.LandingSiteThreatRating = static_cast<int32>(FMath::Clamp<int64>(
						static_cast<int64>(Rule->ThreatRating)
							+ MissionRule->LandingSiteThreatBonus,
						0,
						MAX_int32));
					View.LandingSiteLifetimeSeconds = static_cast<int64>(MissionRule->LandingSiteLifetimeHours) * 3600LL;
					View.WreckageSiteLifetimeSeconds = static_cast<int64>(Config.WreckageSiteLifetimeHours) * 3600LL;
				}
			}
		}
		else
		{
			View.DisplayName = HumanizeId(Contact.ContactRuleId);
		}
		const FString MarkerDetail = View.bTargetsBase
			? FString::Printf(TEXT("%s • target %s • threat %d"), *View.Status,
				View.TargetBaseName.IsEmpty() ? TEXT("player base") : *View.TargetBaseName, View.ThreatRating)
			: FString::Printf(TEXT("%s • threat %d"), *View.Status, View.ThreatRating);
		AddGlobeMarker(Snapshot, EStrategicGlobeMarkerType::Contact, Contact.ContactId, View.DisplayName,
			MarkerDetail,
			Contact.LongitudeMilliDegrees, Contact.LatitudeMilliDegrees, true);
		FStrategicGlobeRouteView& Route = Snapshot.GlobeRoutes.AddDefaulted_GetRef();
		Route.EntityId = Contact.ContactId;
		Route.OriginLongitudeMilliDegrees = Contact.OriginLongitudeMilliDegrees;
		Route.OriginLatitudeMilliDegrees = Contact.OriginLatitudeMilliDegrees;
		Route.DestinationLongitudeMilliDegrees = Contact.DestinationLongitudeMilliDegrees;
		Route.DestinationLatitudeMilliDegrees = Contact.DestinationLatitudeMilliDegrees;
		Route.Progress = View.RouteProgress;
	}
	Snapshot.Contacts.Sort([](const FStrategicContactView& Left, const FStrategicContactView& Right)
	{
		return GuidLess(Left.ContactId, Right.ContactId);
	});

	for (const FBaseAssaultState& Assault : Campaign.BaseAssaults)
	{
		FStrategicBaseAssaultView& View = Snapshot.BaseAssaults.AddDefaulted_GetRef();
		View.AssaultId = Assault.AssaultId;
		View.MissionId = Assault.MissionId;
		View.ContactId = Assault.ContactId;
		View.BaseId = Assault.BaseId;
		View.ArrivedUtc = Assault.ArrivedUtc;
		if (const FStrategicBaseState* Base = FindBase(Campaign, Assault.BaseId))
		{
			View.BaseName = Base->Name;
		}
		const FAdversaryMissionState* Mission = Campaign.AdversaryMissions.FindByPredicate(
			[&Assault](const FAdversaryMissionState& Entry) { return Entry.MissionId == Assault.MissionId; });
		if (Mission != nullptr)
		{
			View.MissionRuleId = Mission->MissionRuleId;
			const FAdversaryMissionRule* Rule = Rules.AdversaryMissions.Find(Mission->MissionRuleId);
			View.MissionName = Rule != nullptr
				? RuleName(Rule->DisplayName, Mission->MissionRuleId)
				: HumanizeId(Mission->MissionRuleId);
		}
		if (const FStrategicContactState* Contact = FindContact(Campaign, Assault.ContactId))
		{
			View.ContactRuleId = Contact->ContactRuleId;
			View.ContactHull = Contact->CurrentHull;
			const FContactRule* Rule = Rules.Contacts.Find(Contact->ContactRuleId);
			View.ContactName = Rule != nullptr
				? RuleName(Rule->DisplayName, Contact->ContactRuleId)
				: HumanizeId(Contact->ContactRuleId);
			View.ThreatRating = Rule != nullptr ? Rule->ThreatRating : 0;
		}
		auto CopyDefenseSupplies = [&Rules](
			const TArray<FBaseDefenseSupplyEvaluation>& Source,
			TArray<FStrategicBaseDefenseSupplyView>& Destination)
		{
			Destination.Reset();
			for (const FBaseDefenseSupplyEvaluation& Supply : Source)
			{
				FStrategicBaseDefenseSupplyView& SupplyView = Destination.AddDefaulted_GetRef();
				SupplyView.ItemId = Supply.ItemId;
				SupplyView.RequiredQuantity = Supply.RequiredQuantity;
				SupplyView.AvailableQuantity = Supply.AvailableQuantity;
				SupplyView.AllocatedQuantity = Supply.AllocatedQuantity;
				const FItemRule* SupplyRule = Rules.Items.Find(Supply.ItemId);
				SupplyView.DisplayName = SupplyRule != nullptr
					? RuleName(SupplyRule->DisplayName, Supply.ItemId)
					: HumanizeId(Supply.ItemId);
			}
		};
		const EBaseDefenseFireDoctrine OrderedDoctrines[] = {
			EBaseDefenseFireDoctrine::CoordinatedLine,
			EBaseDefenseFireDoctrine::PrecisionScreen,
			EBaseDefenseFireDoctrine::BreachBreaker,
			EBaseDefenseFireDoctrine::GridOvercharge
		};
		for (const EBaseDefenseFireDoctrine Doctrine : OrderedDoctrines)
		{
			FResolveBaseAssaultCommand Command;
			Command.ExpectedSequence = Campaign.CommandSequence;
			Command.AssaultId = Assault.AssaultId;
			Command.FireDoctrine = Doctrine;
			const FBaseAssaultEvaluation Evaluation =
				FStrategicCommandService::EvaluateBaseAssault(Campaign, Rules, Config, Command);
			FStrategicBaseDefenseDoctrineView& Option = View.FireDoctrines.AddDefaulted_GetRef();
			Option.Doctrine = Doctrine;
			Option.PolicyId = Evaluation.PolicyId;
			Option.FundingCost = Evaluation.FundingCost;
			Option.bAffordable = Evaluation.bAffordable;
			Option.AccuracyBonus = Evaluation.AccuracyBonus;
			Option.DamagePercent = Evaluation.DamagePercent;
			Option.bCanResolve = Evaluation.bAllowed;
			Option.DefenseBatteryCount = Evaluation.DefenseBatteryCount;
			Option.ReadyDefenseBatteryCount = Evaluation.ReadyDefenseBatteryCount;
			Option.MaximumDefenseDamage = Evaluation.MaximumDefenseDamage;
			Option.ExpectedDefenseDamage = Evaluation.ExpectedDefenseDamage;
			CopyDefenseSupplies(Evaluation.DefenseSupplies, Option.DefenseSupplies);
			switch (Doctrine)
			{
			case EBaseDefenseFireDoctrine::CoordinatedLine:
				Option.DisplayName = TEXT("Coordinated Line");
				Option.Summary = TEXT("Allocate supply and fire in stable battery identity order.");
				break;
			case EBaseDefenseFireDoctrine::PrecisionScreen:
				Option.DisplayName = TEXT("Precision Screen");
				Option.Summary = TEXT("Prioritize higher-accuracy batteries, then higher damage, with stable identity ties.");
				break;
			case EBaseDefenseFireDoctrine::BreachBreaker:
				Option.DisplayName = TEXT("Breach Breaker");
				Option.Summary = TEXT("Prioritize higher-damage batteries, then higher accuracy, with stable identity ties.");
				break;
			case EBaseDefenseFireDoctrine::GridOvercharge:
				Option.DisplayName = TEXT("Grid Overcharge");
				Option.Summary = FString::Printf(
					TEXT("Prioritize higher-damage batteries, add %d accuracy, scale damage to %d%%, and commit threat-indexed emergency funds."),
					Evaluation.AccuracyBonus,
					Evaluation.DamagePercent);
				break;
			default:
				break;
			}
			if (!Evaluation.bAllowed)
			{
				if (Evaluation.Diagnostics.IsEmpty())
				{
					Option.UnavailableReasonCode = TEXT("base_assault_unavailable");
					Option.UnavailableReason = TEXT("Base defense cannot currently resolve this assault.");
				}
				else
				{
					Option.UnavailableReasonCode = Evaluation.Diagnostics[0].Code;
					Option.UnavailableReason = Evaluation.Diagnostics[0].Message;
				}
			}
			if (Doctrine == EBaseDefenseFireDoctrine::CoordinatedLine)
			{
				View.bCanResolve = Evaluation.bAllowed;
				View.DefenseBatteryCount = Evaluation.DefenseBatteryCount;
				View.ReadyDefenseBatteryCount = Evaluation.ReadyDefenseBatteryCount;
				View.MaximumDefenseDamage = Evaluation.MaximumDefenseDamage;
				View.ExpectedDefenseDamage = Evaluation.ExpectedDefenseDamage;
				CopyDefenseSupplies(Evaluation.DefenseSupplies, View.DefenseSupplies);
				View.BreachDamagePerFacility = Evaluation.BreachDamagePerFacility;
				View.MaximumFacilitiesHit = Evaluation.MaximumFacilitiesHit;
				View.UnavailableReasonCode = Option.UnavailableReasonCode;
				View.UnavailableReason = Option.UnavailableReason;
			}
		}

		const FTacticalOperationState* TacticalOperation = Campaign.TacticalOperations.FindByPredicate(
			[&Assault](const FTacticalOperationState& Operation)
			{
				return Operation.Type == ETacticalOperationType::BaseDefense
					&& Operation.AssaultId == Assault.AssaultId;
			});
		if (TacticalOperation != nullptr)
		{
			View.bTacticalDefensePrepared = true;
			View.TacticalOperationId = TacticalOperation->OperationId;
			View.DefenderCount = TacticalOperation->AgentIds.Num();
			View.TacticalUnavailableReasonCode = TEXT("base_defenders_committed");
			View.TacticalUnavailableReason = TEXT("Ground defenders are already committed to this assault.");
		}
		else
		{
			FDeployBaseDefenseOperationCommand DeployCommand;
			DeployCommand.ExpectedSequence = Campaign.CommandSequence;
			DeployCommand.AssaultId = Assault.AssaultId;
			const FBaseDefenseDeploymentEvaluation Deployment =
				FStrategicCommandService::EvaluateBaseDefenseDeployment(Campaign, Rules, DeployCommand);
			View.bCanDeployTacticalDefense = Deployment.bAllowed;
			View.DefenderCount = Deployment.AgentIds.Num();
			View.TacticalMissionRuleId = Deployment.MissionRuleId;
			if (!Deployment.bAllowed)
			{
				if (Deployment.Diagnostics.IsEmpty())
				{
					View.TacticalUnavailableReasonCode = TEXT("base_defense_deployment_unavailable");
					View.TacticalUnavailableReason = TEXT("A ground-defense operation cannot currently be prepared.");
				}
				else
				{
					View.TacticalUnavailableReasonCode = Deployment.Diagnostics[0].Code;
					View.TacticalUnavailableReason = Deployment.Diagnostics[0].Message;
				}
			}
		}
		if (View.TacticalMissionRuleId.IsNone())
		{
			const FStrategicContactState* Contact = FindContact(Campaign, Assault.ContactId);
			if (Contact != nullptr)
			{
				for (const TPair<FName, FTacticalMissionRule>& Pair : Rules.TacticalMissions)
				{
					if (Pair.Value.Context == ETacticalMissionContext::BaseDefense
						&& Pair.Value.SourceContactRuleId == Contact->ContactRuleId)
					{
						View.TacticalMissionRuleId = Pair.Value.Identity.RuleId;
						break;
					}
				}
			}
		}
	}
	Snapshot.BaseAssaults.Sort([](const FStrategicBaseAssaultView& Left, const FStrategicBaseAssaultView& Right)
	{
		return GuidLess(Left.AssaultId, Right.AssaultId);
	});

	for (const FStrategicSiteState& Site : Campaign.StrategicSites)
	{
		FStrategicSiteView& View = Snapshot.Sites.AddDefaulted_GetRef();
		View.SiteId = Site.SiteId;
		View.Type = Site.Type;
		View.SourceContactRuleId = Site.SourceContactRuleId;
		View.ThreatRating = Site.ThreatRating;
		View.LongitudeMilliDegrees = Site.LongitudeMilliDegrees;
		View.LatitudeMilliDegrees = Site.LatitudeMilliDegrees;
		View.RemainingLifetimeSeconds = Site.RemainingLifetimeSeconds;
		const FContactRule* Rule = Rules.Contacts.Find(Site.SourceContactRuleId);
		const FString SourceName = Rule != nullptr
			? RuleName(Rule->DisplayName, Site.SourceContactRuleId)
			: HumanizeId(Site.SourceContactRuleId);
		View.DisplayName = Site.Type == EStrategicSiteType::Landing
			? FString::Printf(TEXT("%s Landing Site"), *SourceName)
			: FString::Printf(TEXT("%s Wreckage"), *SourceName);
		AddGlobeMarker(Snapshot, EStrategicGlobeMarkerType::Site, Site.SiteId, View.DisplayName,
			DurationDetail(Site.RemainingLifetimeSeconds), Site.LongitudeMilliDegrees, Site.LatitudeMilliDegrees, true);
	}
	Snapshot.Sites.Sort([](const FStrategicSiteView& Left, const FStrategicSiteView& Right)
	{
		return GuidLess(Left.SiteId, Right.SiteId);
	});

	for (const FResearchProjectState& Project : Campaign.ResearchProjects)
	{
		FStrategicProjectView& View = Snapshot.Projects.AddDefaulted_GetRef();
		View.Type = EStrategicProjectType::Research;
		View.BaseId = Project.BaseId;
		View.RuleId = Project.ResearchId;
		const FResearchRule* Rule = Rules.Research.Find(Project.ResearchId);
		View.DisplayName = Rule != nullptr ? RuleName(Rule->DisplayName, Project.ResearchId) : HumanizeId(Project.ResearchId);
		const FStrategicBaseState* ResearchBase = FindBase(Campaign, Project.BaseId);
		const TArray<FName> OperationalResearchFacilities = ResearchBase != nullptr
			&& BasesWithValidInfrastructure.Contains(ResearchBase)
			? OperationalFacilities(*ResearchBase, Rules) : TArray<FName>();
		if (Rule != nullptr)
		{
			for (const FName FacilityId : Rule->RequiredFacilityIds)
			{
				const FFacilityRule* Facility = Rules.Facilities.Find(FacilityId);
				const FString FacilityDisplayName = Facility != nullptr
					? RuleName(Facility->DisplayName, FacilityId) : HumanizeId(FacilityId);
				View.RequiredFacilityIds.Add(FacilityId);
				View.RequiredFacilityNames.Add(FacilityDisplayName);
				if (!OperationalResearchFacilities.Contains(FacilityId))
				{
					View.MissingFacilityIds.Add(FacilityId);
					View.MissingFacilityNames.Add(FacilityDisplayName);
				}
			}
		}
		View.bPaused = !View.MissingFacilityNames.IsEmpty();
		if (View.bPaused)
		{
			View.PauseReason = FString::Printf(TEXT("Requires operational facilities: %s."),
				*FString::Join(View.MissingFacilityNames, TEXT(", ")));
		}
		View.ResearchRatePercent = !View.bPaused && ResearchBase != nullptr
			? FStrategicCommandService::EvaluateBaseResearchRatePercent(*ResearchBase, Rules)
			: 100;
		const int32 AssignedScientists = FMath::Max(0, Project.AssignedScientists);
		const int64 Required = Rule != nullptr ? static_cast<int64>(Rule->Effort) * 3600LL : 0;
		const int64 RemainingWork = NonNegativeDifference(
			Required, Project.AccumulatedWorkSeconds);
		View.Progress = Progress(Project.AccumulatedWorkSeconds, Required);
		View.RemainingSeconds = !View.bPaused && AssignedScientists > 0
			? CeilScaledWorkSeconds(
				RemainingWork, AssignedScientists, View.ResearchRatePercent)
			: 0;
		View.AssignedStaff = AssignedScientists;
		const FString RequiredFacilityDetail = View.RequiredFacilityNames.IsEmpty()
			? FString()
			: FString::Printf(TEXT(" • lab %s"), *FString::Join(View.RequiredFacilityNames, TEXT(" + ")));
		View.Detail = View.bPaused
			? FString::Printf(TEXT("LAB OFFLINE • %s • %d scientists"),
				*FString::Join(View.MissingFacilityNames, TEXT(" + ")), AssignedScientists)
			: AssignedScientists > 0
				? FString::Printf(TEXT("%d scientists • %s%s"), AssignedScientists,
					*DurationDetail(View.RemainingSeconds), *RequiredFacilityDetail)
				: FString::Printf(TEXT("Unstaffed%s"), *RequiredFacilityDetail);
	}
	for (const FManufacturingProjectState& Project : Campaign.ManufacturingProjects)
	{
		FStrategicProjectView& View = Snapshot.Projects.AddDefaulted_GetRef();
		View.Type = EStrategicProjectType::Manufacturing;
		View.ProjectId = Project.ProjectId;
		View.BaseId = Project.BaseId;
		View.RuleId = Project.ItemId;
		const FItemRule* Rule = Rules.Items.Find(Project.ItemId);
		const int32 UnitsRemaining = FMath::Max(0, Project.UnitsRemaining);
		const int32 AssignedEngineers = FMath::Max(0, Project.AssignedEngineers);
		View.DisplayName = Rule != nullptr ? RuleName(Rule->DisplayName, Project.ItemId) : HumanizeId(Project.ItemId);
		const int64 RequiredPerUnit = Rule != nullptr ? static_cast<int64>(Rule->ManufactureHours) * 3600LL : 0;
		const int64 TotalRequiredWork = SafeProduct(
			RequiredPerUnit, UnitsRemaining);
		const int64 TotalRemainingWork = NonNegativeDifference(
			TotalRequiredWork, Project.AccumulatedWorkSeconds);
		const int32 RefundableUnits = UnitsRemaining <= 0
			? 0
			: UnitsRemaining - (Project.AccumulatedWorkSeconds > 0 ? 1 : 0);
		View.UnitsRemaining = UnitsRemaining;
		View.UnitCost = Rule != nullptr ? FMath::Max(0, Rule->ManufactureCost) : 0;
		View.CancellationRefund = Rule != nullptr
			? SafeProduct(View.UnitCost, RefundableUnits)
			: 0;
		const FStrategicBaseState* ProjectBase = FindBase(Campaign, Project.BaseId);
		const TArray<FName> OperationalManufacturingFacilities = ProjectBase != nullptr
			&& BasesWithValidInfrastructure.Contains(ProjectBase)
			? OperationalFacilities(*ProjectBase, Rules) : TArray<FName>();
		const bool bManufacturingFacilityOperational =
			!Config.ManufacturingFacilityId.IsNone()
			&& OperationalManufacturingFacilities.Contains(Config.ManufacturingFacilityId);
		View.bPaused = !bManufacturingFacilityOperational;
		if (View.bPaused)
		{
			if (!Config.ManufacturingFacilityId.IsNone())
			{
				View.MissingFacilityIds.Add(Config.ManufacturingFacilityId);
				const FFacilityRule* Facility = Rules.Facilities.Find(Config.ManufacturingFacilityId);
				View.MissingFacilityNames.Add(Facility != nullptr
					? RuleName(Facility->DisplayName, Config.ManufacturingFacilityId)
					: HumanizeId(Config.ManufacturingFacilityId));
			}
			View.PauseReason = View.MissingFacilityNames.IsEmpty()
				? TEXT("Manufacturing requires a configured operational fabrication facility.")
				: FString::Printf(TEXT("Requires operational facility: %s."),
					*FString::Join(View.MissingFacilityNames, TEXT(", ")));
		}
		const FStrategicBaseView* ProjectBaseView = Snapshot.Bases.FindByPredicate(
			[&Project](const FStrategicBaseView& BaseView) { return BaseView.BaseId == Project.BaseId; });
		View.ManufacturingRatePercent = !View.bPaused && ProjectBase != nullptr
			? FStrategicCommandService::EvaluateBaseManufacturingRatePercent(*ProjectBase, Rules)
			: 100;
		if (Rule != nullptr)
		{
			for (const FManufacturingInputRule& Input : Rule->ManufactureInputs)
			{
				FStrategicMaterialRequirementView& Requirement = View.MaterialRequirements.AddDefaulted_GetRef();
				Requirement.ItemId = Input.ItemId;
				const FItemRule* InputRule = Rules.Items.Find(Input.ItemId);
				Requirement.DisplayName = InputRule != nullptr
					? RuleName(InputRule->DisplayName, Input.ItemId)
					: HumanizeId(Input.ItemId);
				Requirement.PerUnitQuantity = Input.Quantity;
				Requirement.AvailableQuantity = InventoryQuantity(ProjectBase, Input.ItemId);
				Requirement.RefundableQuantity = SafeProduct(Input.Quantity, RefundableUnits);
			}
			View.MaterialRequirements.Sort([](const FStrategicMaterialRequirementView& Left,
				const FStrategicMaterialRequirementView& Right)
			{
				return Left.ItemId.LexicalLess(Right.ItemId);
			});
			const int64 OutputStorage = FMath::Max(0, Rule->Mass);
			const int64 InputStorage = ManufacturingInputStorage(*Rule, Rules);
			View.StorageDeltaPerUnit = OutputStorage - InputStorage;
			const int64 ReservedRelease = SafeProduct(OutputStorage, FMath::Max(0, Project.UnitsRemaining));
			const int64 RefundedInputStorage = SafeProduct(InputStorage, RefundableUnits);
			View.CancellationStorageDelta = RefundedInputStorage >= ReservedRelease
				? RefundedInputStorage - ReservedRelease
				: -(ReservedRelease - RefundedInputStorage);
			const int64 RemoveUnitDelta = View.StorageDeltaPerUnit == MIN_int64
				? MAX_int64
				: -View.StorageDeltaPerUnit;
			View.bCanRemoveManufacturingUnit = CanApplyStorageDelta(ProjectBaseView, RemoveUnitDelta);
			if (!View.bCanRemoveManufacturingUnit)
			{
				View.RemoveManufacturingUnitUnavailableReason =
					StorageUnavailableReason(ProjectBaseView, RemoveUnitDelta);
			}
			View.bCanCancel = CanApplyStorageDelta(ProjectBaseView, View.CancellationStorageDelta);
			if (!View.bCanCancel)
			{
				View.CancellationUnavailableReason =
					StorageUnavailableReason(ProjectBaseView, View.CancellationStorageDelta);
			}
		}
		View.Progress = Progress(Project.AccumulatedWorkSeconds, RequiredPerUnit);
		View.RemainingSeconds = !View.bPaused && AssignedEngineers > 0
			? CeilScaledWorkSeconds(
				TotalRemainingWork, AssignedEngineers, View.ManufacturingRatePercent)
			: 0;
		View.AssignedStaff = AssignedEngineers;
		if (View.bPaused)
		{
			const FString MissingFacilityDetail = View.MissingFacilityNames.IsEmpty()
				? View.PauseReason
				: FString::Join(View.MissingFacilityNames, TEXT(" + "));
			View.Detail = FString::Printf(TEXT("FABRICATION OFFLINE • %s • %d engineers"),
				*MissingFacilityDetail, AssignedEngineers);
		}
		else
		{
			View.Detail = FString::Printf(TEXT("%d units • %d engineers • %s"), UnitsRemaining,
				AssignedEngineers,
				AssignedEngineers > 0 ? *DurationDetail(View.RemainingSeconds) : TEXT("unstaffed"));
		}
		if (Rule != nullptr)
		{
			View.Detail += FString::Printf(TEXT(" • Storage %s%lld/unit"),
				View.StorageDeltaPerUnit >= 0 ? TEXT("+") : TEXT(""), View.StorageDeltaPerUnit);
		}
		if (!View.MaterialRequirements.IsEmpty())
		{
			View.Detail += FString::Printf(TEXT(" • Inputs/unit: %s"), *MaterialSummary(View.MaterialRequirements));
			const FString RefundableMaterials = RefundableMaterialSummary(View.MaterialRequirements);
			if (!RefundableMaterials.IsEmpty())
			{
				View.Detail += FString::Printf(TEXT(" • Cancel returns: %s"), *RefundableMaterials);
			}
		}
	}
	for (const FFacilityConstructionProjectState& Project : Campaign.FacilityConstructionProjects)
	{
		FStrategicProjectView& View = Snapshot.Projects.AddDefaulted_GetRef();
		View.Type = EStrategicProjectType::Construction;
		View.ProjectId = Project.ProjectId;
		View.BaseId = Project.BaseId;
		View.RuleId = Project.FacilityId;
		const FFacilityRule* Rule = Rules.Facilities.Find(Project.FacilityId);
		View.DisplayName = Rule != nullptr ? RuleName(Rule->DisplayName, Project.FacilityId) : HumanizeId(Project.FacilityId);
		const int64 Total = Rule != nullptr ? static_cast<int64>(Rule->BuildHours) * 3600LL : 0;
		View.Progress = Progress(
			NonNegativeDifference(Total, Project.RemainingBuildSeconds), Total);
		View.RemainingSeconds = FMath::Max<int64>(0, Project.RemainingBuildSeconds);
		View.CancellationRefund = Rule != nullptr
			? ProportionalRefund(Rule->BuildCost, Project.RemainingBuildSeconds, Total)
			: 0;
		View.Detail = FString::Printf(TEXT("Grid %d,%d • %s"), Project.GridX, Project.GridY, *DurationDetail(View.RemainingSeconds));
	}
	for (const FRecruitmentOrderState& Order : Campaign.RecruitmentOrders)
	{
		FStrategicProjectView& View = Snapshot.Projects.AddDefaulted_GetRef();
		View.Type = EStrategicProjectType::Recruitment;
		View.ProjectId = Order.OrderId;
		View.BaseId = Order.BaseId;
		View.RuleId = Order.RoleId;
		View.DisplayName = Order.DisplayName;
		const FPersonnelRoleRule* Rule = Rules.PersonnelRoles.Find(Order.RoleId);
		const int64 Total = Rule != nullptr ? static_cast<int64>(Rule->RecruitmentHours) * 3600LL : 0;
		View.Progress = Progress(
			NonNegativeDifference(Total, Order.RemainingTransitSeconds), Total);
		View.RemainingSeconds = FMath::Max<int64>(0, Order.RemainingTransitSeconds);
		View.Detail = FString::Printf(TEXT("%s • %s"),
			Rule != nullptr ? *RuleName(Rule->DisplayName, Order.RoleId) : *HumanizeId(Order.RoleId),
			*DurationDetail(View.RemainingSeconds));
	}
	for (const FCraftAcquisitionOrderState& Order : Campaign.CraftAcquisitionOrders)
	{
		FStrategicProjectView& View = Snapshot.Projects.AddDefaulted_GetRef();
		View.Type = EStrategicProjectType::CraftAcquisition;
		View.ProjectId = Order.OrderId;
		View.BaseId = Order.BaseId;
		View.RuleId = Order.CraftRuleId;
		View.DisplayName = Order.DisplayName;
		const FCraftRule* Rule = Rules.Craft.Find(Order.CraftRuleId);
		const int64 Total = Rule != nullptr ? static_cast<int64>(Rule->AcquisitionHours) * 3600LL : 0;
		View.Progress = Progress(
			NonNegativeDifference(Total, Order.RemainingTransitSeconds), Total);
		View.RemainingSeconds = FMath::Max<int64>(0, Order.RemainingTransitSeconds);
		View.Detail = FString::Printf(TEXT("%s • %s"),
			Rule != nullptr ? *RuleName(Rule->DisplayName, Order.CraftRuleId) : *HumanizeId(Order.CraftRuleId),
			*DurationDetail(View.RemainingSeconds));
	}
	Snapshot.Projects.Sort([](const FStrategicProjectView& Left, const FStrategicProjectView& Right)
	{
		if (Left.Type != Right.Type)
		{
			return static_cast<uint8>(Left.Type) < static_cast<uint8>(Right.Type);
		}
		if (Left.RuleId != Right.RuleId)
		{
			return Left.RuleId.LexicalLess(Right.RuleId);
		}
		return GuidLess(Left.ProjectId, Right.ProjectId);
	});

	const FStrategicBaseState* PrimaryBase = FindBase(Campaign, Snapshot.PrimaryBaseId);
	const bool bHasBase = PrimaryBase != nullptr;
	const bool bPrimaryInfrastructureValid = bHasBase
		&& BasesWithValidInfrastructure.Contains(PrimaryBase);
	const FStrategicBaseView* PrimaryBaseView = Snapshot.Bases.FindByPredicate(
		[&Snapshot](const FStrategicBaseView& BaseView) { return BaseView.BaseId == Snapshot.PrimaryBaseId; });
	const TArray<FName> PrimaryFacilities = bPrimaryInfrastructureValid
		? OperationalFacilities(*PrimaryBase, Rules) : TArray<FName>();
	for (const TPair<FName, FResearchRule>& Pair : Rules.Research)
	{
		FStrategicActionOptionView& Option = Snapshot.ActionOptions.AddDefaulted_GetRef();
		Option.Type = EStrategicActionOptionType::Research;
		Option.RuleId = Pair.Key;
		Option.DisplayName = RuleName(Pair.Value.DisplayName, Pair.Key);
		Option.Detail = FString::Printf(TEXT("%d scientist-hours"), Pair.Value.Effort);
		TArray<FString> RequiredFacilityNames;
		TArray<FString> MissingFacilityNames;
		for (const FName FacilityId : Pair.Value.RequiredFacilityIds)
		{
			const FFacilityRule* Facility = Rules.Facilities.Find(FacilityId);
			const FString FacilityDisplayName = Facility != nullptr
				? RuleName(Facility->DisplayName, FacilityId) : HumanizeId(FacilityId);
			Option.RequiredFacilityIds.Add(FacilityId);
			Option.RequiredFacilityNames.Add(FacilityDisplayName);
			RequiredFacilityNames.Add(FacilityDisplayName);
			if (!PrimaryFacilities.Contains(FacilityId))
			{
				Option.MissingFacilityIds.Add(FacilityId);
				Option.MissingFacilityNames.Add(FacilityDisplayName);
				MissingFacilityNames.Add(FacilityDisplayName);
			}
		}
		if (!RequiredFacilityNames.IsEmpty())
		{
			Option.Detail += FString::Printf(TEXT(" • lab %s"),
				*FString::Join(RequiredFacilityNames, TEXT(" + ")));
		}
		Option.DurationHours = Pair.Value.Effort;
		Option.bUnlocked = HasRequirements(Pair.Value.Prerequisites, Campaign);
		Option.bAffordable = true;
		const bool bUnused = !Campaign.CompletedResearch.Contains(Pair.Key)
			&& !Campaign.ResearchProjects.ContainsByPredicate(
				[&Pair](const FResearchProjectState& Project) { return Project.ResearchId == Pair.Key; });
		const bool bHasRequiredFacilities = MissingFacilityNames.IsEmpty();
		FinishOption(Option, Campaign, bHasBase, bUnused && bHasRequiredFacilities,
			bUnused ? FName(TEXT("research_facility_missing")) : FName(TEXT("research_already_known")),
			bUnused
				? FString::Printf(TEXT("The primary base requires operational facilities: %s."),
					*FString::Join(MissingFacilityNames, TEXT(", ")))
				: TEXT("This topic is complete or already active."));
	}
	for (const TPair<FName, FFacilityRule>& Pair : Rules.Facilities)
	{
		FStrategicActionOptionView& Option = Snapshot.ActionOptions.AddDefaulted_GetRef();
		Option.Type = EStrategicActionOptionType::Facility;
		Option.RuleId = Pair.Key;
		Option.DisplayName = RuleName(Pair.Value.DisplayName, Pair.Key);
		Option.Cost = Pair.Value.BuildCost;
		Option.DurationHours = Pair.Value.BuildHours;
		Option.Detail = FString::Printf(TEXT("%dx%d • %d h"), Pair.Value.GridWidth, Pair.Value.GridHeight, Pair.Value.BuildHours);
		if (Pair.Value.StorageCapacity > 0)
		{
			Option.Detail += FString::Printf(TEXT(" • +%d storage"), Pair.Value.StorageCapacity);
		}
		if (Pair.Value.ScientistCapacity > 0)
		{
			Option.Detail += FString::Printf(TEXT(" • +%d scientist cap"), Pair.Value.ScientistCapacity);
		}
		if (Pair.Value.EngineerCapacity > 0)
		{
			Option.Detail += FString::Printf(TEXT(" • +%d engineer cap"), Pair.Value.EngineerCapacity);
		}
		if (Pair.Value.BaseDefenseAccuracy > 0 && Pair.Value.BaseDefenseDamage > 0)
		{
			const int32 ExpectedDamage = CalculateExpectedBaseDefenseDamage(
				Pair.Value.BaseDefenseAccuracy, Pair.Value.BaseDefenseDamage);
			Option.Detail += FString::Printf(TEXT(" • battery %d%% / %d dmg / ~%d expected"),
				Pair.Value.BaseDefenseAccuracy, Pair.Value.BaseDefenseDamage, ExpectedDamage);
		}
		Option.FacilityGridWidth = Pair.Value.GridWidth;
		Option.FacilityGridHeight = Pair.Value.GridHeight;
		Option.BaseDefenseSupplyItemId = Pair.Value.BaseDefenseSupplyItemId;
		Option.BaseDefenseSupplyPerShot = Pair.Value.BaseDefenseSupplyPerShot;
		if (const FItemRule* Supply = Rules.Items.Find(Pair.Value.BaseDefenseSupplyItemId))
		{
			Option.BaseDefenseSupplyDisplayName = RuleName(
				Supply->DisplayName, Pair.Value.BaseDefenseSupplyItemId);
		}
		Option.bUnlocked = HasRequirements(Pair.Value.RequiredResearch, Campaign);
		Option.bAffordable = Campaign.Funds >= Option.Cost;
		if (bHasBase && bPrimaryInfrastructureValid)
		{
			Option.ValidFacilityPlacements = FindFacilityPlacements(
				*PrimaryBase, Campaign, Rules, Config, Pair.Value);
			if (!Option.ValidFacilityPlacements.IsEmpty())
			{
				Option.SuggestedGridX = Option.ValidFacilityPlacements[0].X;
				Option.SuggestedGridY = Option.ValidFacilityPlacements[0].Y;
			}
		}
		const bool bHasPlacement = !Option.ValidFacilityPlacements.IsEmpty();
		FinishOption(Option, Campaign, bHasBase, bHasPlacement,
			TEXT("facility_no_space"), TEXT("No adjacent grid placement is available at the primary base."));
	}
	for (const TPair<FName, FPersonnelRoleRule>& Pair : Rules.PersonnelRoles)
	{
		FStrategicActionOptionView& Option = Snapshot.ActionOptions.AddDefaulted_GetRef();
		Option.Type = EStrategicActionOptionType::Personnel;
		Option.RuleId = Pair.Key;
		Option.DisplayName = RuleName(Pair.Value.DisplayName, Pair.Key);
		Option.Cost = Pair.Value.RecruitmentCost;
		Option.DurationHours = Pair.Value.RecruitmentHours;
		Option.Detail = FString::Printf(TEXT("Recruit • %d h • salary %d/mo"), Pair.Value.RecruitmentHours, Pair.Value.MonthlySalary);
		Option.bUnlocked = HasRequirements(Pair.Value.RequiredResearch, Campaign);
		Option.bAffordable = Campaign.Funds >= Option.Cost;
		int32 Capacity = 0;
		int32 Occupied = 0;
		if (bHasBase)
		{
			if (Pair.Value.Category == EPersonnelRoleCategory::Scientist)
			{
				Capacity = PrimaryBaseView != nullptr ? PrimaryBaseView->ScientistCapacity : 0;
				Occupied = PersonnelCountForCategory(Campaign, Rules, PrimaryBase->BaseId, EPersonnelRoleCategory::Scientist);
			}
			else if (Pair.Value.Category == EPersonnelRoleCategory::Engineer)
			{
				Capacity = PrimaryBaseView != nullptr ? PrimaryBaseView->EngineerCapacity : 0;
				Occupied = PersonnelCountForCategory(Campaign, Rules, PrimaryBase->BaseId, EPersonnelRoleCategory::Engineer);
			}
			else
			{
				Capacity = Config.MaxGeneralPersonnelPerBase;
				Occupied = SaturatingNonNegativeAdd(
					PersonnelCountForCategory(
						Campaign, Rules, PrimaryBase->BaseId, EPersonnelRoleCategory::FieldAgent),
					PersonnelCountForCategory(
						Campaign, Rules, PrimaryBase->BaseId, EPersonnelRoleCategory::Pilot));
			}
		}
		FinishOption(Option, Campaign, bHasBase, Occupied < Capacity,
			TEXT("personnel_capacity_full"), TEXT("The primary base has no capacity for this role."));
	}
	for (const TPair<FName, FCraftRule>& Pair : Rules.Craft)
	{
		FStrategicActionOptionView& Option = Snapshot.ActionOptions.AddDefaulted_GetRef();
		Option.Type = EStrategicActionOptionType::Craft;
		Option.RuleId = Pair.Key;
		Option.DisplayName = RuleName(Pair.Value.DisplayName, Pair.Key);
		Option.Cost = Pair.Value.PurchaseCost;
		Option.DurationHours = Pair.Value.AcquisitionHours;
		Option.CraftMaxHull = Pair.Value.MaxHull;
		Option.CraftAgentCapacity = Pair.Value.AgentCapacity;
		Option.Detail = FString::Printf(TEXT("Acquire • %d h • hull %d • crew %d"),
			Pair.Value.AcquisitionHours, Pair.Value.MaxHull, Pair.Value.AgentCapacity);
		Option.bUnlocked = HasRequirements(Pair.Value.RequiredResearch, Campaign);
		Option.bAffordable = Campaign.Funds >= Option.Cost;
		const bool bHasBerth = bHasBase && bPrimaryInfrastructureValid
			&& CraftOccupied(Campaign, PrimaryBase->BaseId) < CraftCapacity(*PrimaryBase, Rules);
		FinishOption(Option, Campaign, bHasBase, bHasBerth,
			TEXT("craft_capacity_full"), TEXT("The primary base has no open craft berth."));
	}
	for (const TPair<FName, FItemRule>& Pair : Rules.Items)
	{
		if (!Pair.Value.IsManufacturable())
		{
			continue;
		}
		FStrategicActionOptionView& Option = Snapshot.ActionOptions.AddDefaulted_GetRef();
		Option.Type = EStrategicActionOptionType::Manufacturing;
		Option.RuleId = Pair.Key;
		Option.DisplayName = RuleName(Pair.Value.DisplayName, Pair.Key);
		Option.Cost = Pair.Value.ManufactureCost;
		Option.DurationHours = Pair.Value.ManufactureHours;
		Option.Detail = FString::Printf(TEXT("Manufacture 1 • %d engineer-hours"), Pair.Value.ManufactureHours);
		Option.bUnlocked = HasRequirements(Pair.Value.RequiredResearch, Campaign);
		Option.bAffordable = Campaign.Funds >= Option.Cost;
		const bool bHasFacility = bHasBase && PrimaryFacilities.Contains(Config.ManufacturingFacilityId);
		bool bHasMaterials = true;
		for (const FManufacturingInputRule& Input : Pair.Value.ManufactureInputs)
		{
			FStrategicMaterialRequirementView& Requirement = Option.MaterialRequirements.AddDefaulted_GetRef();
			Requirement.ItemId = Input.ItemId;
			const FItemRule* InputRule = Rules.Items.Find(Input.ItemId);
			Requirement.DisplayName = InputRule != nullptr
				? RuleName(InputRule->DisplayName, Input.ItemId)
				: HumanizeId(Input.ItemId);
			Requirement.PerUnitQuantity = Input.Quantity;
			Requirement.AvailableQuantity = InventoryQuantity(PrimaryBase, Input.ItemId);
			bHasMaterials &= Requirement.AvailableQuantity >= Requirement.PerUnitQuantity;
		}
		Option.MaterialRequirements.Sort([](const FStrategicMaterialRequirementView& Left,
			const FStrategicMaterialRequirementView& Right)
		{
			return Left.ItemId.LexicalLess(Right.ItemId);
		});
		if (!Option.MaterialRequirements.IsEmpty())
		{
			Option.Detail += FString::Printf(TEXT(" • Inputs: %s"), *MaterialSummary(Option.MaterialRequirements));
		}
		Option.StorageDeltaPerUnit = static_cast<int64>(FMath::Max(0, Pair.Value.Mass))
			- ManufacturingInputStorage(Pair.Value, Rules);
		Option.Detail += FString::Printf(TEXT(" • Storage %s%lld/unit"),
			Option.StorageDeltaPerUnit >= 0 ? TEXT("+") : TEXT(""), Option.StorageDeltaPerUnit);
		const bool bHasStorage = CanApplyStorageDelta(PrimaryBaseView, Option.StorageDeltaPerUnit);
		const bool bAdditionalCondition = bHasFacility && bHasMaterials && bHasStorage;
		const FName AdditionalCode = !bHasFacility
			? FName(TEXT("manufacturing_facility_missing"))
			: !bHasMaterials
				? FName(TEXT("manufacturing_materials_missing"))
				: FName(TEXT("storage_capacity_exceeded"));
		const FString AdditionalMessage = !bHasFacility
			? TEXT("An operational fabrication facility is required.")
			: !bHasMaterials
				? FString::Printf(TEXT("Required production materials are unavailable: %s."),
					*MaterialSummary(Option.MaterialRequirements))
				: StorageUnavailableReason(PrimaryBaseView, Option.StorageDeltaPerUnit);
		FinishOption(Option, Campaign, bHasBase, bAdditionalCondition, AdditionalCode, AdditionalMessage);
	}
	Snapshot.ActionOptions.Sort([](const FStrategicActionOptionView& Left, const FStrategicActionOptionView& Right)
	{
		if (Left.Type != Right.Type)
		{
			return static_cast<uint8>(Left.Type) < static_cast<uint8>(Right.Type);
		}
		return Left.RuleId.LexicalLess(Right.RuleId);
	});

	Snapshot.PendingOperationIds.Reserve(Campaign.TacticalOperations.Num());
	for (const FTacticalOperationState& Operation : Campaign.TacticalOperations)
	{
		Snapshot.PendingOperationIds.Add(Operation.OperationId);
	}
	Snapshot.PendingOperationIds.Sort([](const FGuid& Left, const FGuid& Right) { return GuidLess(Left, Right); });
	Snapshot.TacticalBattleIds.Reserve(Campaign.TacticalBattles.Num());
	for (const FTacticalBattleState& Battle : Campaign.TacticalBattles)
	{
		Snapshot.TacticalBattleIds.Add(Battle.BattleId);
	}
	Snapshot.TacticalBattleIds.Sort([](const FGuid& Left, const FGuid& Right) { return GuidLess(Left, Right); });
	Snapshot.GlobeMarkers.Sort([](const FStrategicGlobeMarkerView& Left, const FStrategicGlobeMarkerView& Right)
	{
		if (Left.Type != Right.Type)
		{
			return static_cast<uint8>(Left.Type) < static_cast<uint8>(Right.Type);
		}
		return GuidLess(Left.EntityId, Right.EntityId);
	});
	Snapshot.GlobeRoutes.Sort([](const FStrategicGlobeRouteView& Left, const FStrategicGlobeRouteView& Right)
	{
		return GuidLess(Left.EntityId, Right.EntityId);
	});
	Snapshot.NetMonthlyFunding = SaturatingSubtract(
		Snapshot.MonthlyFunding, Snapshot.MonthlyOutgoings);
	Snapshot.bSucceeded = true;
	return Snapshot;
}
