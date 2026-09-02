// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Strategic/PersonnelSquadBond.h"

#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPersonnelSquadBondEvaluationTest,
	"UEGT.Core.Strategic.Personnel.SquadBond",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPersonnelSquadBondEvaluationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Field Cadence thresholds and bounded bonuses are exact"),
		FPersonnelSquadBond::GetTier(2) == EPersonnelSquadBondTier::None
		&& FPersonnelSquadBond::GetTier(3) == EPersonnelSquadBondTier::Aligned
		&& FPersonnelSquadBond::GetTier(8) == EPersonnelSquadBondTier::Interlocked
		&& FPersonnelSquadBond::GetTier(15) == EPersonnelSquadBondTier::Unbroken
		&& FPersonnelSquadBond::GetNextTierVictories(EPersonnelSquadBondTier::None) == 3
		&& FPersonnelSquadBond::GetNextTierVictories(EPersonnelSquadBondTier::Aligned) == 8
		&& FPersonnelSquadBond::GetNextTierVictories(EPersonnelSquadBondTier::Interlocked) == 15
		&& FPersonnelSquadBond::GetNextTierVictories(EPersonnelSquadBondTier::Unbroken) == 0
		&& FPersonnelSquadBond::GetActionPointBonus(EPersonnelSquadBondTier::Aligned) == 1
		&& FPersonnelSquadBond::GetActionPointBonus(EPersonnelSquadBondTier::Interlocked) == 1
		&& FPersonnelSquadBond::GetActionPointBonus(EPersonnelSquadBondTier::Unbroken) == 2
		&& FPersonnelSquadBond::GetMoraleBonus(EPersonnelSquadBondTier::Aligned) == 0
		&& FPersonnelSquadBond::GetMoraleBonus(EPersonnelSquadBondTier::Interlocked) == 5
		&& FPersonnelSquadBond::GetMoraleBonus(EPersonnelSquadBondTier::Unbroken) == 5);

	FCampaignState Campaign;
	const FGuid A(10, 1, 1, 1);
	const FGuid B(20, 1, 1, 1);
	const FGuid C(30, 1, 1, 1);
	const FGuid D(40, 1, 1, 1);
	auto AddPerson = [&Campaign](const FGuid Id, const TCHAR* Name)
	{
		FPersonnelState& Person = Campaign.Personnel.AddDefaulted_GetRef();
		Person.PersonnelId = Id;
		Person.DisplayName = Name;
	};
	AddPerson(A, TEXT("Aster Vale"));
	AddPerson(B, TEXT("Bram Sol"));
	AddPerson(C, TEXT("Cinder Quill"));
	AddPerson(D, TEXT("Dara Neris"));
	auto AddBond = [&Campaign](const FGuid First, const FGuid Second, const int32 Victories)
	{
		FPersonnelSquadBondState& Bond = Campaign.PersonnelSquadBonds.AddDefaulted_GetRef();
		Bond.FirstPersonnelId = First;
		Bond.SecondPersonnelId = Second;
		Bond.SharedVictories = Victories;
	};
	AddBond(A, B, 15);
	AddBond(A, C, 16);
	AddBond(B, D, 15);
	AddBond(C, D, 8);
	AddBond(A, D, 2);
	AddBond(D, C, 99); // Non-canonical persisted input is ignored defensively by projection.

	TArray<FGuid> Roster = { D, A, C, FGuid(), B, A, FGuid(999, 1, 1, 1) };
	const FPersonnelSquadBondView View = FPersonnelSquadBond::Evaluate(Campaign, Roster);
	TestTrue(TEXT("Strongest non-overlapping pairs win before lexical tie-breaks"),
		View.PolicyId == FName(TEXT("personnel.squad-bond-field-cadence"))
		&& View.bActive
		&& View.ResolvedPersonnelCount == 4
		&& View.EligiblePairCount == 4
		&& View.ActivePairs.Num() == 2
		&& View.ActivePairs[0].FirstPersonnelId == A
		&& View.ActivePairs[0].SecondPersonnelId == C
		&& View.ActivePairs[0].SharedVictories == 16
		&& View.ActivePairs[0].Tier == EPersonnelSquadBondTier::Unbroken
		&& View.ActivePairs[0].ActionPointBonus == 2
		&& View.ActivePairs[0].MoraleBonus == 5
		&& View.ActivePairs[1].FirstPersonnelId == B
		&& View.ActivePairs[1].SecondPersonnelId == D
		&& View.ActivePairs[1].SharedVictories == 15
		&& View.DevelopingPairs.Num() == 1
		&& View.DevelopingPairs[0].FirstPersonnelId == A
		&& View.DevelopingPairs[0].SecondPersonnelId == D
		&& View.DevelopingPairs[0].NextTierVictories == 3);

	Algo::Reverse(Roster);
	Algo::Reverse(Campaign.PersonnelSquadBonds);
	const FPersonnelSquadBondView Reordered = FPersonnelSquadBond::Evaluate(Campaign, Roster);
	TestTrue(TEXT("Roster and persisted collection order cannot change pair selection"),
		Reordered.ResolvedPersonnelCount == View.ResolvedPersonnelCount
		&& Reordered.EligiblePairCount == View.EligiblePairCount
		&& Reordered.ActivePairs.Num() == View.ActivePairs.Num()
		&& Reordered.ActivePairs[0].FirstPersonnelId == View.ActivePairs[0].FirstPersonnelId
		&& Reordered.ActivePairs[0].SecondPersonnelId == View.ActivePairs[0].SecondPersonnelId
		&& Reordered.ActivePairs[1].FirstPersonnelId == View.ActivePairs[1].FirstPersonnelId
		&& Reordered.ActivePairs[1].SecondPersonnelId == View.ActivePairs[1].SecondPersonnelId);

	const FPersonnelSquadBondView Dormant = FPersonnelSquadBond::Evaluate(Campaign, { A, D });
	TestTrue(TEXT("A developing pair remains explicit but supplies no tactical bonus"),
		!Dormant.bActive
		&& Dormant.EligiblePairCount == 0
		&& Dormant.ActivePairs.IsEmpty()
		&& Dormant.DevelopingPairs.Num() == 1
		&& Dormant.DevelopingPairs[0].SharedVictories == 2
		&& Dormant.DevelopingPairs[0].ActionPointBonus == 0
		&& Dormant.DevelopingPairs[0].MoraleBonus == 0);
	return true;
}

#endif
