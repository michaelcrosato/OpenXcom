// Copyright 2026 UEGT contributors. MIT License.

#include "Strategic/PersonnelStewardship.h"

#include "Strategic/StrategicCommandService.h"

namespace PersonnelStewardshipPrivate
{
	bool GuidLess(const FGuid& Left, const FGuid& Right)
	{
		return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
	}

	bool IsAssignedToCraft(const FCampaignState& Campaign, const FGuid& PersonnelId)
	{
		return Campaign.Craft.ContainsByPredicate(
			[&PersonnelId](const FCraftState& Craft)
			{
				return Craft.AssignedPilotId == PersonnelId || Craft.AssignedAgentIds.Contains(PersonnelId);
			});
	}

	FPersonnelStewardshipOptionView MakeOption(
		const EPersonnelStewardshipFocus Focus,
		const int64 DurationSeconds,
		const int32 ReductionPercent,
		const bool bAvailable,
		const FName UnavailableReasonCode,
		const FString& UnavailableReason)
	{
		FPersonnelStewardshipOptionView Option;
		Option.Focus = Focus;
		Option.PolicyId = FPersonnelStewardship::PolicyId(Focus);
		Option.DurationSeconds = DurationSeconds;
		Option.ReductionPercent = ReductionPercent;
		Option.bAvailable = bAvailable;
		Option.UnavailableReasonCode = bAvailable ? NAME_None : UnavailableReasonCode;
		Option.UnavailableReason = bAvailable ? FString() : UnavailableReason;
		return Option;
	}
}

FName FPersonnelStewardship::PolicyId(const EPersonnelStewardshipFocus Focus)
{
	switch (Focus)
	{
	case EPersonnelStewardshipFocus::RecoveryAdvocacy:
		return TEXT("personnel.stewardship-recovery-advocacy");
	case EPersonnelStewardshipFocus::TrainingCadre:
		return TEXT("personnel.stewardship-training-cadre");
	case EPersonnelStewardshipFocus::RecruitmentLiaison:
		return TEXT("personnel.stewardship-recruitment-liaison");
	default:
		return NAME_None;
	}
}

bool FPersonnelStewardship::IsKnown(const EPersonnelStewardshipFocus Focus)
{
	return Focus == EPersonnelStewardshipFocus::None || IsSelected(Focus);
}

bool FPersonnelStewardship::IsSelected(const EPersonnelStewardshipFocus Focus)
{
	return Focus == EPersonnelStewardshipFocus::RecoveryAdvocacy
		|| Focus == EPersonnelStewardshipFocus::TrainingCadre
		|| Focus == EPersonnelStewardshipFocus::RecruitmentLiaison;
}

bool FPersonnelStewardship::IsConfigValid(const FStrategicSimulationConfig& Config)
{
	return Config.StewardshipDurationDays > 0 && Config.StewardshipDurationDays <= 365
		&& Config.StewardshipMinimumMissions > 0 && Config.StewardshipMinimumMissions <= 10000
		&& Config.StewardshipReductionPercent > 0 && Config.StewardshipReductionPercent < 100
		&& Config.StewardshipResolveBonus > 0 && Config.StewardshipResolveBonus <= 100
		&& Config.StewardshipResolveAwardTourCap > 0 && Config.StewardshipResolveAwardTourCap <= 100;
}

const FPersonnelState* FPersonnelStewardship::FindActiveSteward(
	const FCampaignState& Campaign,
	const FGuid& BaseId)
{
	using namespace PersonnelStewardshipPrivate;

	const FPersonnelState* Selected = nullptr;
	for (const FPersonnelState& Person : Campaign.Personnel)
	{
		if (Person.BaseId != BaseId || Person.Status != EPersonnelStatus::Stewarding
			|| !IsSelected(Person.StewardshipFocus) || Person.RemainingStewardshipSeconds <= 0)
		{
			continue;
		}
		if (Selected == nullptr || GuidLess(Person.PersonnelId, Selected->PersonnelId))
		{
			Selected = &Person;
		}
	}
	return Selected;
}

bool FPersonnelStewardship::HasActiveFocus(
	const FCampaignState& Campaign,
	const FGuid& BaseId,
	const EPersonnelStewardshipFocus Focus)
{
	const FPersonnelState* Steward = FindActiveSteward(Campaign, BaseId);
	return Steward != nullptr && Steward->StewardshipFocus == Focus;
}

bool FPersonnelStewardship::TryApplyReductionCeil(
	const int64 Baseline,
	const int32 ReductionPercent,
	int64& OutReduced)
{
	if (Baseline < 0 || ReductionPercent <= 0 || ReductionPercent >= 100)
	{
		return false;
	}
	if (Baseline == 0)
	{
		OutReduced = 0;
		return true;
	}
	const int32 RetainedPercent = 100 - ReductionPercent;
	if (Baseline > (MAX_int64 - 99) / RetainedPercent)
	{
		return false;
	}
	OutReduced = FMath::Max<int64>(1, (Baseline * RetainedPercent + 99) / 100);
	return true;
}

FPersonnelStewardshipView FPersonnelStewardship::Evaluate(
	const FCampaignState& Campaign,
	const FResolvedRuleSet& Rules,
	const FStrategicSimulationConfig& Config,
	const FGuid& PersonnelId)
{
	using namespace PersonnelStewardshipPrivate;

	FPersonnelStewardshipView View;
	const FPersonnelState* Person = Campaign.Personnel.FindByPredicate(
		[&PersonnelId](const FPersonnelState& Candidate) { return Candidate.PersonnelId == PersonnelId; });
	if (Person == nullptr)
	{
		View.UnavailableReasonCode = TEXT("unknown_personnel");
		View.UnavailableReason = TEXT("Personnel member does not exist in the active roster.");
		return View;
	}

	View.DurationSeconds = static_cast<int64>(Config.StewardshipDurationDays) * 86400LL;
	View.MinimumMissions = Config.StewardshipMinimumMissions;
	View.ReductionPercent = Config.StewardshipReductionPercent;
	View.ResolveAwardTourCap = Config.StewardshipResolveAwardTourCap;
	const FPersonnelState* ActiveSteward = FindActiveSteward(Campaign, Person->BaseId);
	if (ActiveSteward != nullptr)
	{
		View.bBaseHasActiveSteward = true;
		View.bSelectedPersonnelIsSteward = ActiveSteward->PersonnelId == Person->PersonnelId;
		View.StewardId = ActiveSteward->PersonnelId;
		View.StewardDisplayName = ActiveSteward->DisplayName;
		View.ActiveFocus = ActiveSteward->StewardshipFocus;
		View.ActivePolicyId = PolicyId(ActiveSteward->StewardshipFocus);
		View.RemainingSeconds = ActiveSteward->RemainingStewardshipSeconds;
		View.ToursCompleted = ActiveSteward->StewardshipToursCompleted;
		View.ResolveBonusOnCompletion = ActiveSteward->StewardshipToursCompleted < Config.StewardshipResolveAwardTourCap
			? FMath::Min(Config.StewardshipResolveBonus, 100 - ActiveSteward->Resolve)
			: 0;
		View.UnavailableReasonCode = TEXT("personnel_stewardship_active");
		View.UnavailableReason = TEXT("This base already has an active Stewardship Rotation.");
		return View;
	}

	View.ToursCompleted = Person->StewardshipToursCompleted;
	View.ResolveBonusOnCompletion = Person->StewardshipToursCompleted < Config.StewardshipResolveAwardTourCap
		? FMath::Min(Config.StewardshipResolveBonus, 100 - Person->Resolve)
		: 0;
	if (!IsConfigValid(Config))
	{
		View.UnavailableReasonCode = TEXT("invalid_personnel_stewardship_config");
		View.UnavailableReason = TEXT("Stewardship duration, eligibility, benefit, and completion-reward settings are invalid.");
	}
	else
	{
		const FPersonnelRoleRule* Role = Rules.PersonnelRoles.Find(Person->RoleId);
		if (Role == nullptr || Role->Category != EPersonnelRoleCategory::FieldAgent)
		{
			View.UnavailableReasonCode = TEXT("personnel_stewardship_role_ineligible");
			View.UnavailableReason = TEXT("Only field agents can lead a Stewardship Rotation.");
		}
		else if (Person->Missions < Config.StewardshipMinimumMissions)
		{
			View.UnavailableReasonCode = TEXT("personnel_stewardship_experience_required");
			View.UnavailableReason = TEXT("This field agent has not completed enough missions to lead a rotation.");
		}
		else if (Person->Status != EPersonnelStatus::Available)
		{
			View.UnavailableReasonCode = TEXT("personnel_unavailable");
			View.UnavailableReason = TEXT("Only available personnel can begin a Stewardship Rotation.");
		}
		else if (IsAssignedToCraft(Campaign, Person->PersonnelId))
		{
			View.UnavailableReasonCode = TEXT("personnel_assigned_to_craft");
			View.UnavailableReason = TEXT("Remove this field agent from their craft before beginning a rotation.");
		}
		else
		{
			View.bEligible = true;
		}
	}

	View.Options.Add(MakeOption(EPersonnelStewardshipFocus::RecoveryAdvocacy,
		View.DurationSeconds, View.ReductionPercent, View.bEligible,
		View.UnavailableReasonCode, View.UnavailableReason));
	View.Options.Add(MakeOption(EPersonnelStewardshipFocus::TrainingCadre,
		View.DurationSeconds, View.ReductionPercent, View.bEligible,
		View.UnavailableReasonCode, View.UnavailableReason));
	View.Options.Add(MakeOption(EPersonnelStewardshipFocus::RecruitmentLiaison,
		View.DurationSeconds, View.ReductionPercent, View.bEligible,
		View.UnavailableReasonCode, View.UnavailableReason));
	return View;
}
