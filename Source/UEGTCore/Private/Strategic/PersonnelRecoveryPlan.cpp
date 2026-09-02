// Copyright 2026 UEGT contributors. MIT License.

#include "Strategic/PersonnelRecoveryPlan.h"

#include "Strategic/PersonnelStewardship.h"
#include "Strategic/StrategicCommandService.h"

namespace PersonnelRecoveryPlanPrivate
{
	const FPersonnelState* FindPersonnel(const FCampaignState& Campaign, const FGuid& PersonnelId)
	{
		return Campaign.Personnel.FindByPredicate(
			[&PersonnelId](const FPersonnelState& Person)
			{
				return Person.PersonnelId == PersonnelId;
			});
	}

	bool TryScaleDuration(const int64 Seconds, const int32 Percent, int64& OutSeconds)
	{
		if (Seconds <= 0 || Percent <= 0 || Seconds > (MAX_int64 - 99) / Percent)
		{
			return false;
		}
		OutSeconds = FMath::Max<int64>(1, (Seconds * Percent + 99) / 100);
		return true;
	}

	FPersonnelRecoveryPlanOptionView MakeOption(
		const EPersonnelRecoveryPlan Plan,
		const int64 DurationSeconds,
		const int64 FundingCost,
		const int32 ResolveBonus)
	{
		FPersonnelRecoveryPlanOptionView Option;
		Option.Plan = Plan;
		Option.PolicyId = FPersonnelRecoveryPlan::PolicyId(Plan);
		Option.DurationSeconds = DurationSeconds;
		Option.FundingCost = FundingCost;
		Option.ResolveBonus = ResolveBonus;
		Option.bAvailable = true;
		return Option;
	}
}

FName FPersonnelRecoveryPlan::PolicyId(const EPersonnelRecoveryPlan Plan)
{
	switch (Plan)
	{
	case EPersonnelRecoveryPlan::MeasuredReturn:
		return TEXT("personnel.recovery-measured-return");
	case EPersonnelRecoveryPlan::SurgeCare:
		return TEXT("personnel.recovery-surge-care");
	case EPersonnelRecoveryPlan::ReflectionCycle:
		return TEXT("personnel.recovery-reflection-cycle");
	default:
		return NAME_None;
	}
}

bool FPersonnelRecoveryPlan::IsKnown(const EPersonnelRecoveryPlan Plan)
{
	return Plan == EPersonnelRecoveryPlan::None
		|| Plan == EPersonnelRecoveryPlan::DecisionRequired
		|| IsSelected(Plan);
}

bool FPersonnelRecoveryPlan::IsSelected(const EPersonnelRecoveryPlan Plan)
{
	return Plan == EPersonnelRecoveryPlan::MeasuredReturn
		|| Plan == EPersonnelRecoveryPlan::SurgeCare
		|| Plan == EPersonnelRecoveryPlan::ReflectionCycle;
}

FPersonnelRecoveryPlanView FPersonnelRecoveryPlan::Evaluate(
	const FCampaignState& Campaign,
	const FStrategicSimulationConfig& Config,
	const FGuid& PersonnelId)
{
	using namespace PersonnelRecoveryPlanPrivate;

	FPersonnelRecoveryPlanView View;
	const FPersonnelState* Person = FindPersonnel(Campaign, PersonnelId);
	if (Person == nullptr || Person->Status != EPersonnelStatus::Recovering
		|| Person->RemainingRecoverySeconds <= 0 || Person->CurrentHealth >= Person->MaxHealth)
	{
		return View;
	}

	View.bRecovering = true;
	View.bDecisionRequired = Person->RecoveryPlan == EPersonnelRecoveryPlan::DecisionRequired;
	View.SelectedPlan = Person->RecoveryPlan;
	View.SelectedPolicyId = PolicyId(Person->RecoveryPlan);
	View.BaselineRemainingSeconds = Person->RemainingRecoverySeconds;
	if (!View.bDecisionRequired)
	{
		return View;
	}

	FPersonnelRecoveryPlanOptionView Measured = MakeOption(
		EPersonnelRecoveryPlan::MeasuredReturn,
		Person->RemainingRecoverySeconds,
		0,
		0);
	View.Options.Add(MoveTemp(Measured));

	int64 SurgeDuration = 0;
	int64 SurgeCost = 0;
	int64 BaselineSurgeCost = 0;
	const int64 MissingHealth = static_cast<int64>(Person->MaxHealth - Person->CurrentHealth);
	const bool bRecoveryStewardship = FPersonnelStewardship::HasActiveFocus(
		Campaign, Person->BaseId, EPersonnelStewardshipFocus::RecoveryAdvocacy);
	const bool bSurgeConfigValid = Config.RecoverySurgeDurationPercent > 0
		&& Config.RecoverySurgeDurationPercent <= 100
		&& Config.RecoverySurgeCostPerMissingHealth > 0;
	const bool bStewardshipConfigValid = !bRecoveryStewardship
		|| FPersonnelStewardship::IsConfigValid(Config);
	bool bSurgeMathValid = bSurgeConfigValid && bStewardshipConfigValid
		&& TryScaleDuration(Person->RemainingRecoverySeconds, Config.RecoverySurgeDurationPercent, SurgeDuration)
		&& MissingHealth <= MAX_int64 / Config.RecoverySurgeCostPerMissingHealth;
	if (bSurgeMathValid)
	{
		BaselineSurgeCost = MissingHealth * Config.RecoverySurgeCostPerMissingHealth;
		SurgeCost = BaselineSurgeCost;
		if (bRecoveryStewardship)
		{
			bSurgeMathValid = FPersonnelStewardship::TryApplyReductionCeil(
				BaselineSurgeCost, Config.StewardshipReductionPercent, SurgeCost);
		}
	}
	FPersonnelRecoveryPlanOptionView Surge = MakeOption(
		EPersonnelRecoveryPlan::SurgeCare,
		SurgeDuration,
		SurgeCost,
		0);
	Surge.bStewardshipBenefitApplied = bRecoveryStewardship && bSurgeMathValid;
	Surge.StewardshipFundingDiscount = Surge.bStewardshipBenefitApplied
		? BaselineSurgeCost - SurgeCost
		: 0;
	if (!bSurgeMathValid)
	{
		Surge.bAvailable = false;
		Surge.UnavailableReasonCode = !bStewardshipConfigValid
			? FName(TEXT("invalid_personnel_stewardship_config"))
			: FName(TEXT("invalid_recovery_plan_config"));
		Surge.UnavailableReason = !bStewardshipConfigValid
			? FString(TEXT("Recovery Advocacy settings cannot produce a bounded Surge Care discount."))
			: FString(TEXT("Surge Care settings cannot produce a bounded duration and funding cost."));
	}
	else if (Campaign.Funds < SurgeCost)
	{
		Surge.bAvailable = false;
		Surge.UnavailableReasonCode = TEXT("recovery_surge_unaffordable");
		Surge.UnavailableReason = TEXT("Current funds do not cover this person's Surge Care plan.");
	}
	View.Options.Add(MoveTemp(Surge));

	int64 ReflectionDuration = 0;
	const bool bReflectionConfigValid = Config.RecoveryReflectionDurationPercent >= 100
		&& Config.RecoveryReflectionDurationPercent <= 1000
		&& Config.RecoveryReflectionResolveBonus > 0
		&& Config.RecoveryReflectionResolveBonus <= 100;
	const bool bReflectionMathValid = bReflectionConfigValid
		&& TryScaleDuration(Person->RemainingRecoverySeconds,
			Config.RecoveryReflectionDurationPercent, ReflectionDuration);
	FPersonnelRecoveryPlanOptionView Reflection = MakeOption(
		EPersonnelRecoveryPlan::ReflectionCycle,
		ReflectionDuration,
		0,
		Config.RecoveryReflectionResolveBonus);
	if (!bReflectionMathValid)
	{
		Reflection.bAvailable = false;
		Reflection.UnavailableReasonCode = TEXT("invalid_recovery_plan_config");
		Reflection.UnavailableReason = TEXT("Reflection Cycle settings cannot produce a bounded duration and Resolve benefit.");
	}
	else if (Person->Resolve >= 100)
	{
		Reflection.bAvailable = false;
		Reflection.UnavailableReasonCode = TEXT("recovery_reflection_no_effect");
		Reflection.UnavailableReason = TEXT("This person's Resolve is already at its maximum.");
	}
	View.Options.Add(MoveTemp(Reflection));
	return View;
}
