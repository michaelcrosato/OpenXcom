// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Campaign/CampaignSave.h"

#include "Misc/AutomationTest.h"

namespace CampaignSaveTests
{
	const FGuid TestCampaignId(0x13572468, 0x24681357, 0xabcdef01, 0x10203040);
	const FDateTime TestWallClock(2026, 8, 29, 20, 15, 30);

	TArray<FCampaignContentVersion> MakeContentPackages()
	{
		FCampaignContentVersion Balance;
		Balance.PackageId = TEXT("mod.balance");
		Balance.Version = TEXT("1.1.0");
		FCampaignContentVersion Base;
		Base.PackageId = TEXT("uegt.base");
		Base.Version = TEXT("1.0.0");
		return { Balance, Base };
	}

	FCampaignState MakeState()
	{
		FCampaignState State;
		State.StrategicTime = FStrategicTimestamp(FDateTime(2035, 4, 12, 18, 30, 5));
		State.SimulationRandom.Initialize(424242);
		State.SimulationRandom.NextUInt64();
		State.SimulationRandom.NextUInt64();
		State.SimulationRandom.NextUInt64();
		State.Funds = 9007199254740993LL;
		State.CampaignScore = -275;
		State.Difficulty = ECampaignDifficulty::Veteran;
		State.CommandSequence = 9007199254740995LL;
		State.CompletedResearch = { TEXT("research.orbital-signals"), TEXT("research.directed-energy") };
		FStrategicBaseState& Base = State.Bases.AddDefaulted_GetRef();
		Base.BaseId = FGuid(0x11112222, 0x33334444, 0x55556666, 0x77778888);
		Base.Name = TEXT("Integrity Test Base");
		Base.RegionId = TEXT("region.cascadia");
		Base.LongitudeMilliDegrees = -123120;
		Base.LatitudeMilliDegrees = 49280;
		Base.ScientistCapacity = 10;
		Base.EngineerCapacity = 8;
		FBaseFacilityState& Facility = Base.Facilities.AddDefaulted_GetRef();
		Facility.InstanceId = FGuid(0x99990000, 0xaaaabbbb, 0xccccdddd, 0xeeeeffff);
		Facility.FacilityId = TEXT("facility.fabrication-bay");
		Facility.GridX = 1;
		Facility.GridY = 2;
		Facility.Damage = 12;
		Facility.ReservedRepairDamage = 5;
		Facility.RemainingRepairSeconds = 1800;
		return State;
	}

	FCampaignSaveWriteResult MakeSerializedSave()
	{
		const FCampaignSaveEnvelope Envelope = FCampaignSaveCodec::CreateNew(
			MakeState(),
			MakeContentPackages(),
			TEXT("0.2.0-test"),
			TestWallClock,
			TestCampaignId);
		return FCampaignSaveCodec::Serialize(Envelope);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveRoundTripTest,
	"UEGT.Core.CampaignSave.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	const FCampaignSaveWriteResult Write = MakeSerializedSave();
	TestTrue(TEXT("Valid campaign save serializes"), Write.bSucceeded);
	TestFalse(TEXT("Serialized JSON is populated"), Write.Json.IsEmpty());
	TestEqual(TEXT("SHA-256 save checksum has 64 hexadecimal characters"), Write.Envelope.Header.SaveChecksum.Len(), 64);
	TestTrue(TEXT("64-bit values are encoded losslessly as strings"), Write.Json.Contains(TEXT("\"funds\":\"9007199254740993\"")));

	const FCampaignSaveReadResult Read = FCampaignSaveCodec::Deserialize(Write.Json, MakeContentPackages());
	TestTrue(TEXT("Serialized campaign save reads"), Read.bSucceeded);
	TestFalse(TEXT("Current save needs no migration"), Read.bMigrated);
	TestEqual(TEXT("Campaign id round-trips"), Read.Envelope.Header.CampaignId, TestCampaignId);
	TestEqual(TEXT("Strategic time round-trips"), Read.Envelope.State.StrategicTime.Utc, FDateTime(2035, 4, 12, 18, 30, 5));
	TestEqual(TEXT("Large funds round-trip exactly"), Read.Envelope.State.Funds, 9007199254740993LL);
	TestEqual(TEXT("Signed score round-trips"), Read.Envelope.State.CampaignScore, int64(-275));
	TestEqual(TEXT("Large command sequence round-trips exactly"), Read.Envelope.State.CommandSequence, 9007199254740995LL);
	TestTrue(TEXT("Difficulty round-trips"), Read.Envelope.State.Difficulty == ECampaignDifficulty::Veteran);
	TestEqual(TEXT("Research ids round-trip"), Read.Envelope.State.CompletedResearch.Num(), 2);
	TestTrue(TEXT("Facility durability and active repair state round-trip in current format"),
		Read.Envelope.State.Bases.Num() == 1
		&& Read.Envelope.State.Bases[0].Facilities.Num() == 1
		&& Read.Envelope.State.Bases[0].Facilities[0].Damage == 12
		&& Read.Envelope.State.Bases[0].Facilities[0].ReservedRepairDamage == 5
		&& Read.Envelope.State.Bases[0].Facilities[0].RemainingRepairSeconds == 1800);
	if (Read.Envelope.State.CompletedResearch.Num() == 2)
	{
		TestEqual(TEXT("Research ids are normalized"), Read.Envelope.State.CompletedResearch[0], FName(TEXT("research.directed-energy")));
	}

	FDeterministicRandomStream ExpectedRandom = MakeState().SimulationRandom;
	FDeterministicRandomStream LoadedRandom = Read.Envelope.State.SimulationRandom;
	TestEqual(TEXT("Loaded random stream resumes the exact next draw"), LoadedRandom.NextUInt64(), ExpectedRandom.NextUInt64());
	TestEqual(TEXT("Loaded random draw count is preserved"), Read.Envelope.State.SimulationRandom.DrawCount, int64(3));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveInt64BoundsTest,
	"UEGT.Core.CampaignSave.Int64Bounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveInt64BoundsTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	const FCampaignSaveWriteResult Write = MakeSerializedSave();
	TestTrue(TEXT("Boundary fixture serializes"), Write.bSucceeded);
	const FString PositiveOverflowJson = Write.Json.Replace(
		TEXT("\"funds\":\"9007199254740993\""),
		TEXT("\"funds\":\"9223372036854775808\""));
	TestTrue(TEXT("Positive overflow fixture changes the funds field"),
		PositiveOverflowJson.Contains(TEXT("\"funds\":\"9223372036854775808\"")));
	const FCampaignSaveReadResult PositiveOverflow =
		FCampaignSaveCodec::Deserialize(PositiveOverflowJson);
	TestTrue(TEXT("Positive int64 overflow is rejected while parsing"),
		!PositiveOverflow.bSucceeded
		&& PositiveOverflow.HasDiagnostic(TEXT("invalid_field_value")));

	const FString NegativeOverflowJson = Write.Json.Replace(
		TEXT("\"funds\":\"9007199254740993\""),
		TEXT("\"funds\":\"-9223372036854775809\""));
	TestTrue(TEXT("Negative overflow fixture changes the funds field"),
		NegativeOverflowJson.Contains(TEXT("\"funds\":\"-9223372036854775809\"")));
	const FCampaignSaveReadResult NegativeOverflow =
		FCampaignSaveCodec::Deserialize(NegativeOverflowJson);
	TestTrue(TEXT("Negative int64 overflow is rejected while parsing"),
		!NegativeOverflow.bSucceeded
		&& NegativeOverflow.HasDiagnostic(TEXT("invalid_field_value")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveBaseAssaultRoundTripTest,
	"UEGT.Core.CampaignSave.BaseAssaultRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveBaseAssaultRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	FCampaignState State = MakeState();
	const FGuid ContactId(0x20112233, 0x44556677, 0x8899aabb, 0xccddeeff);
	const FGuid MissionId(0x30112233, 0x44556677, 0x8899aabb, 0xccddeeff);
	const FGuid AssaultId(0x40112233, 0x44556677, 0x8899aabb, 0xccddeeff);
	FStrategicContactState& Contact = State.StrategicContacts.AddDefaulted_GetRef();
	Contact.ContactId = ContactId;
	Contact.ContactRuleId = TEXT("contact.skimmer");
	Contact.Status = EStrategicContactStatus::Detected;
	Contact.OriginLongitudeMilliDegrees = -165000;
	Contact.OriginLatitudeMilliDegrees = 42000;
	Contact.LongitudeMilliDegrees = State.Bases[0].LongitudeMilliDegrees;
	Contact.LatitudeMilliDegrees = State.Bases[0].LatitudeMilliDegrees;
	Contact.DestinationLongitudeMilliDegrees = State.Bases[0].LongitudeMilliDegrees;
	Contact.DestinationLatitudeMilliDegrees = State.Bases[0].LatitudeMilliDegrees;
	Contact.TotalRouteSeconds = 7200;
	Contact.ElapsedRouteSeconds = 7200;
	Contact.CurrentHull = 80;
	FAdversaryMissionState& Mission = State.AdversaryMissions.AddDefaulted_GetRef();
	Mission.MissionId = MissionId;
	Mission.ContactId = ContactId;
	Mission.MissionRuleId = TEXT("mission.nightglass-raid");
	Mission.TargetBaseId = State.Bases[0].BaseId;
	Mission.StartedUtc = State.StrategicTime.Utc - FTimespan::FromHours(2);
	FBaseAssaultState& Assault = State.BaseAssaults.AddDefaulted_GetRef();
	Assault.AssaultId = AssaultId;
	Assault.MissionId = MissionId;
	Assault.ContactId = ContactId;
	Assault.BaseId = State.Bases[0].BaseId;
	Assault.ArrivedUtc = State.StrategicTime.Utc;
	State.AdversaryMissionsLaunched = 1;

	const FCampaignSaveEnvelope Envelope = FCampaignSaveCodec::CreateNew(
		State, MakeContentPackages(), TEXT("0.20.0-test"), TestWallClock,
		FGuid(0x20202020, 0x30303030, 0x40404040, 0x50505050));
	const FCampaignSaveWriteResult Write = FCampaignSaveCodec::Serialize(Envelope);
	TestTrue(TEXT("Active base assault serializes"), Write.bSucceeded);
	TestTrue(TEXT("Current JSON carries the dynamic mission target"), Write.Json.Contains(TEXT("\"targetBaseId\"")));
	TestTrue(TEXT("Current JSON carries the pending assault"), Write.Json.Contains(TEXT("\"baseAssaults\"")));
	const FCampaignSaveReadResult Read = FCampaignSaveCodec::Deserialize(Write.Json, MakeContentPackages());
	TestTrue(TEXT("Active base assault deserializes"), Read.bSucceeded);
	TestTrue(TEXT("Mission target and pending assault round-trip exactly"),
		Read.Envelope.State.AdversaryMissions.Num() == 1
		&& Read.Envelope.State.AdversaryMissions[0].TargetBaseId == State.Bases[0].BaseId
		&& Read.Envelope.State.BaseAssaults.Num() == 1
		&& Read.Envelope.State.BaseAssaults[0].AssaultId == AssaultId
		&& Read.Envelope.State.BaseAssaults[0].ContactId == ContactId
		&& Read.Envelope.State.BaseAssaults[0].ArrivedUtc == State.StrategicTime.Utc);
	FCampaignSaveEnvelope InvalidContactPosition = Write.Envelope;
	InvalidContactPosition.State.BaseAssaults.Reset();
	FStrategicContactState* InFlightContact =
		InvalidContactPosition.State.StrategicContacts.FindByPredicate(
			[&ContactId](FStrategicContactState& Entry)
			{
				return Entry.ContactId == ContactId;
			});
	TestNotNull(TEXT("In-flight contact remains addressable for position validation"),
		InFlightContact);
	if (InFlightContact != nullptr)
	{
		InFlightContact->ElapsedRouteSeconds = InFlightContact->TotalRouteSeconds / 2;
		InFlightContact->LongitudeMilliDegrees =
			InFlightContact->DestinationLongitudeMilliDegrees;
		InFlightContact->LatitudeMilliDegrees =
			InFlightContact->DestinationLatitudeMilliDegrees;
		const FCampaignSaveValidationResult InvalidPositionValidation =
			FCampaignSaveCodec::Validate(InvalidContactPosition);
		TestTrue(TEXT("Persisted in-flight contacts must use their deterministic route position"),
			!InvalidPositionValidation.bSucceeded
			&& InvalidPositionValidation.HasDiagnostic(TEXT("invalid_strategic_contact")));
	}
	FCampaignState MismatchedTargetBase = State;
	MismatchedTargetBase.Bases[0].LongitudeMilliDegrees += 1000;
	const FCampaignSaveEnvelope MismatchedTargetBaseEnvelope = FCampaignSaveCodec::CreateNew(
		MismatchedTargetBase, MakeContentPackages(), TEXT("0.20.0-mismatched-target"), TestWallClock,
		FGuid(0x20202021, 0x30303030, 0x40404040, 0x50505050));
	const FCampaignSaveValidationResult MismatchedTargetBaseValidation = FCampaignSaveCodec::Validate(
		MismatchedTargetBaseEnvelope, MakeContentPackages());
	TestTrue(TEXT("Save validation rejects a mission whose target base coordinates drift from its contact destination"),
		!MismatchedTargetBaseValidation.bSucceeded
		&& MismatchedTargetBaseValidation.HasDiagnostic(TEXT("invalid_adversary_mission")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveDeterminismTest,
	"UEGT.Core.CampaignSave.DeterministicNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	TArray<FCampaignContentVersion> FirstPackages = MakeContentPackages();
	TArray<FCampaignContentVersion> SecondPackages = FirstPackages;
	Algo::Reverse(SecondPackages);
	TestEqual(
		TEXT("Content fingerprint ignores caller ordering"),
		FCampaignSaveCodec::ComputeContentFingerprint(FirstPackages),
		FCampaignSaveCodec::ComputeContentFingerprint(SecondPackages));
	TestEqual(
		TEXT("Content fingerprint matches an independent SHA-256 golden value"),
		FCampaignSaveCodec::ComputeContentFingerprint(FirstPackages),
		FString(TEXT("a0fff8616a3ccaf4bd8fff6520f41e58661e5c2b0ff0782944fca854d2cf19e7")));

	FCampaignSaveEnvelope First = FCampaignSaveCodec::CreateNew(MakeState(), FirstPackages, TEXT("0.2.0-test"), TestWallClock, TestCampaignId);
	FCampaignSaveEnvelope Second = FCampaignSaveCodec::CreateNew(MakeState(), SecondPackages, TEXT("0.2.0-test"), TestWallClock, TestCampaignId);
	Algo::Reverse(Second.State.CompletedResearch);
	const FCampaignSaveWriteResult FirstWrite = FCampaignSaveCodec::Serialize(First);
	const FCampaignSaveWriteResult SecondWrite = FCampaignSaveCodec::Serialize(Second);
	TestTrue(TEXT("First normalized save serializes"), FirstWrite.bSucceeded);
	TestTrue(TEXT("Second normalized save serializes"), SecondWrite.bSucceeded);
	TestEqual(TEXT("Equivalent state produces the same checksum"), FirstWrite.Envelope.Header.SaveChecksum, SecondWrite.Envelope.Header.SaveChecksum);
	TestEqual(TEXT("Equivalent state produces byte-identical compact JSON"), FirstWrite.Json, SecondWrite.Json);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveSquadBondRoundTripTest,
	"UEGT.Core.CampaignSave.PersonnelSquadBondRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveSquadBondRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	FCampaignState State = MakeState();
	const FGuid FirstId(0x10101010, 0x20202020, 0x30303030, 0x40404040);
	const FGuid SecondId(0x20202020, 0x30303030, 0x40404040, 0x50505050);
	const FGuid ThirdId(0x30303030, 0x40404040, 0x50505050, 0x60606060);
	for (const TPair<FGuid, FString>& Entry : {
		TPair<FGuid, FString>(FirstId, TEXT("Aster Vale")),
		TPair<FGuid, FString>(SecondId, TEXT("Bram Sato")),
		TPair<FGuid, FString>(ThirdId, TEXT("Cinder Okafor")) })
	{
		FPersonnelState& Person = State.Personnel.AddDefaulted_GetRef();
		Person.PersonnelId = Entry.Key;
		Person.DisplayName = Entry.Value;
		Person.RoleId = TEXT("personnel.field-agent");
		Person.BaseId = State.Bases[0].BaseId;
		Person.Status = EPersonnelStatus::Available;
		Person.Rank = 2;
		Person.Missions = 20;
		Person.MaxHealth = 40;
		Person.CurrentHealth = 40;
		Person.Accuracy = 60;
		Person.Resolve = 60;
		Person.Mobility = 60;
		Person.Strength = 60;
	}
	FPersonnelSquadBondState& Interlocked = State.PersonnelSquadBonds.AddDefaulted_GetRef();
	Interlocked.FirstPersonnelId = FirstId;
	Interlocked.SecondPersonnelId = SecondId;
	Interlocked.SharedVictories = 8;
	FPersonnelSquadBondState& Developing = State.PersonnelSquadBonds.AddDefaulted_GetRef();
	Developing.FirstPersonnelId = FirstId;
	Developing.SecondPersonnelId = ThirdId;
	Developing.SharedVictories = 2;

	const FCampaignSaveEnvelope Envelope = FCampaignSaveCodec::CreateNew(
		State, MakeContentPackages(), TEXT("0.43.0-test"), TestWallClock,
		FGuid(0x33333333, 0x44444444, 0x55555555, 0x66666666));
	const FCampaignSaveWriteResult Write = FCampaignSaveCodec::Serialize(Envelope);
	TestTrue(TEXT("Current personnel squad bonds serialize"), Write.bSucceeded);
	TestTrue(TEXT("Current JSON carries the explicit squad-bond ledger"),
		Write.Json.Contains(TEXT("\"personnelSquadBonds\""))
		&& Write.Json.Contains(TEXT("\"sharedVictories\":8")));
	const FCampaignSaveReadResult Read = FCampaignSaveCodec::Deserialize(Write.Json, MakeContentPackages());
	TestTrue(TEXT("Current personnel squad bonds deserialize"), Read.bSucceeded);
	TestTrue(TEXT("Canonical pair identities and histories round-trip"),
		Read.Envelope.State.PersonnelSquadBonds.Num() == 2
		&& Read.Envelope.State.PersonnelSquadBonds[0].FirstPersonnelId == FirstId
		&& Read.Envelope.State.PersonnelSquadBonds[0].SecondPersonnelId == SecondId
		&& Read.Envelope.State.PersonnelSquadBonds[0].SharedVictories == 8
		&& Read.Envelope.State.PersonnelSquadBonds[1].SecondPersonnelId == ThirdId
		&& Read.Envelope.State.PersonnelSquadBonds[1].SharedVictories == 2);

	FCampaignSaveEnvelope Reordered = Envelope;
	Algo::Reverse(Reordered.State.PersonnelSquadBonds);
	Algo::Reverse(Reordered.State.Personnel);
	const FCampaignSaveWriteResult ReorderedWrite = FCampaignSaveCodec::Serialize(Reordered);
	TestTrue(TEXT("Reordered bond fixture serializes"), ReorderedWrite.bSucceeded);
	TestEqual(TEXT("Bond and personnel input order do not affect canonical JSON"), ReorderedWrite.Json, Write.Json);

	FCampaignSaveEnvelope Invalid = Write.Envelope;
	Invalid.State.PersonnelSquadBonds[0].FirstPersonnelId = SecondId;
	Invalid.State.PersonnelSquadBonds[0].SecondPersonnelId = FirstId;
	const FCampaignSaveValidationResult InvalidValidation = FCampaignSaveCodec::Validate(Invalid);
	TestFalse(TEXT("Non-canonical persisted bond identity is rejected"), InvalidValidation.bSucceeded);
	TestTrue(TEXT("Invalid bond identity has a stable diagnostic"),
		InvalidValidation.HasDiagnostic(TEXT("invalid_personnel_squad_bond")));

	FCampaignSaveEnvelope LegacyEnvelope = Write.Envelope;
	LegacyEnvelope.Header.FormatVersion = 32;
	LegacyEnvelope.State.PersonnelSquadBonds.Reset();
	LegacyEnvelope.Header.SaveChecksum = FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyEnvelope);
	FString LegacyJson = Write.Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":32"));
	LegacyJson = LegacyJson.Replace(*Write.Envelope.Header.SaveChecksum, *LegacyEnvelope.Header.SaveChecksum);
	const int32 BondFieldStart = LegacyJson.Find(TEXT(",\"personnelSquadBonds\":"));
	const int32 RecruitmentFieldStart = LegacyJson.Find(
		TEXT(",\"recruitmentOrders\":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, BondFieldStart);
	TestTrue(TEXT("v32 migration fixture finds the v33 bond field"),
		BondFieldStart != INDEX_NONE && RecruitmentFieldStart > BondFieldStart);
	if (BondFieldStart != INDEX_NONE && RecruitmentFieldStart > BondFieldStart)
	{
		LegacyJson.RemoveAt(BondFieldStart, RecruitmentFieldStart - BondFieldStart, EAllowShrinking::No);
	}
	const FCampaignSaveReadResult Migrated = FCampaignSaveCodec::Deserialize(LegacyJson, MakeContentPackages());
	TestTrue(TEXT("Verified v32 save migrates to the current format"), Migrated.bSucceeded && Migrated.bMigrated
		&& Migrated.Envelope.Header.FormatVersion == FCampaignSaveCodec::CurrentFormatVersion);
	TestTrue(TEXT("v32 migration initializes an empty squad-bond ledger"),
		Migrated.Envelope.State.PersonnelSquadBonds.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveRecoveryPlanRoundTripTest,
	"UEGT.Core.CampaignSave.PersonnelRecoveryPlanRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveRecoveryPlanRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	FCampaignState State = MakeState();
	FPersonnelState& Person = State.Personnel.AddDefaulted_GetRef();
	Person.PersonnelId = FGuid(0x34343434, 0x45454545, 0x56565656, 0x67676767);
	Person.DisplayName = TEXT("Nia Venn");
	Person.RoleId = TEXT("personnel.field-agent");
	Person.BaseId = State.Bases[0].BaseId;
	Person.Status = EPersonnelStatus::Recovering;
	Person.Rank = 2;
	Person.Missions = 5;
	Person.MaxHealth = 50;
	Person.CurrentHealth = 40;
	Person.Accuracy = 60;
	Person.Resolve = 70;
	Person.Mobility = 55;
	Person.Strength = 58;
	Person.RemainingRecoverySeconds = 54 * 3600;
	Person.RecoveryPlan = EPersonnelRecoveryPlan::ReflectionCycle;

	const FCampaignSaveEnvelope Envelope = FCampaignSaveCodec::CreateNew(
		State, MakeContentPackages(), TEXT("0.44.0-test"), TestWallClock,
		FGuid(0x44444444, 0x55555555, 0x66666666, 0x77777777));
	const FCampaignSaveWriteResult Write = FCampaignSaveCodec::Serialize(Envelope);
	TestTrue(TEXT("v34 Return Path state serializes"), Write.bSucceeded);
	TestTrue(TEXT("Current JSON carries the exact recovery plan"),
		Write.Json.Contains(TEXT("\"recoveryPlan\":\"reflection-cycle\"")));
	const FCampaignSaveReadResult Read = FCampaignSaveCodec::Deserialize(
		Write.Json, MakeContentPackages());
	TestTrue(TEXT("v34 Return Path state deserializes"), Read.bSucceeded);
	TestTrue(TEXT("Recovery plan and exact remaining duration round-trip"),
		Read.Envelope.State.Personnel.Num() == 1
		&& Read.Envelope.State.Personnel[0].RecoveryPlan == EPersonnelRecoveryPlan::ReflectionCycle
		&& Read.Envelope.State.Personnel[0].RemainingRecoverySeconds == 54 * 3600);

	FCampaignSaveEnvelope DifferentPlan = Write.Envelope;
	DifferentPlan.State.Personnel[0].RecoveryPlan = EPersonnelRecoveryPlan::MeasuredReturn;
	TestNotEqual(TEXT("Recovery plan identity participates in the save checksum"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(DifferentPlan),
		Write.Envelope.Header.SaveChecksum);

	FCampaignSaveEnvelope Invalid = Write.Envelope;
	Invalid.State.Personnel[0].Status = EPersonnelStatus::Available;
	Invalid.State.Personnel[0].CurrentHealth = Invalid.State.Personnel[0].MaxHealth;
	Invalid.State.Personnel[0].RemainingRecoverySeconds = 0;
	const FCampaignSaveValidationResult InvalidValidation = FCampaignSaveCodec::Validate(Invalid);
	TestFalse(TEXT("A completed recovery cannot retain an active plan"), InvalidValidation.bSucceeded);
	TestTrue(TEXT("Invalid plan/status pairing has a stable diagnostic"),
		InvalidValidation.HasDiagnostic(TEXT("invalid_personnel_state")));

	FCampaignSaveEnvelope LegacyEnvelope = Write.Envelope;
	LegacyEnvelope.Header.FormatVersion = 33;
	LegacyEnvelope.State.Personnel[0].RecoveryPlan = EPersonnelRecoveryPlan::None;
	LegacyEnvelope.Header.SaveChecksum = FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyEnvelope);
	FString LegacyJson = Write.Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":33"));
	LegacyJson = LegacyJson.Replace(*Write.Envelope.Header.SaveChecksum, *LegacyEnvelope.Header.SaveChecksum);
	const int32 RemovedPlanFields = LegacyJson.ReplaceInline(
		TEXT(",\"recoveryPlan\":\"reflection-cycle\""), TEXT(""));
	const FCampaignSaveReadResult Migrated = FCampaignSaveCodec::Deserialize(
		LegacyJson, MakeContentPackages());
	TestTrue(TEXT("Verified v33 save migrates to v34"),
		RemovedPlanFields == 1 && Migrated.bSucceeded && Migrated.bMigrated
		&& Migrated.Envelope.Header.FormatVersion == FCampaignSaveCodec::CurrentFormatVersion);
	TestTrue(TEXT("Legacy recovery preserves its exact clock with Measured Return"),
		Migrated.Envelope.State.Personnel.Num() == 1
		&& Migrated.Envelope.State.Personnel[0].RecoveryPlan == EPersonnelRecoveryPlan::MeasuredReturn
		&& Migrated.Envelope.State.Personnel[0].RemainingRecoverySeconds == 54 * 3600);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveStewardshipRoundTripTest,
	"UEGT.Core.CampaignSave.PersonnelStewardshipRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveStewardshipRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	FCampaignState State = MakeState();
	FPersonnelState& Steward = State.Personnel.AddDefaulted_GetRef();
	Steward.PersonnelId = FGuid(0x35353535, 0x46464646, 0x57575757, 0x68686868);
	Steward.DisplayName = TEXT("Aster Vale");
	Steward.RoleId = TEXT("personnel.field-agent");
	Steward.BaseId = State.Bases[0].BaseId;
	Steward.Status = EPersonnelStatus::Stewarding;
	Steward.Rank = 4;
	Steward.Missions = 18;
	Steward.MaxHealth = 55;
	Steward.CurrentHealth = 55;
	Steward.Accuracy = 62;
	Steward.Resolve = 73;
	Steward.Mobility = 59;
	Steward.Strength = 61;
	Steward.StewardshipFocus = EPersonnelStewardshipFocus::RecruitmentLiaison;
	Steward.RemainingStewardshipSeconds = 1234567;
	Steward.StewardshipToursCompleted = 2;
	FMemorialRecord& Memorial = State.Memorial.AddDefaulted_GetRef();
	Memorial.PersonnelId = FGuid(0x35353545, 0x46464656, 0x57575767, 0x68686878);
	Memorial.DisplayName = TEXT("Bram Sato");
	Memorial.RoleId = TEXT("personnel.field-agent");
	Memorial.Rank = 5;
	Memorial.Missions = 30;
	Memorial.Kills = 9;
	Memorial.StewardshipToursCompleted = 4;
	Memorial.DeathUtc = State.StrategicTime.Utc - FTimespan::FromDays(2);
	Memorial.CauseId = TEXT("cause.field-injury");

	const FCampaignSaveEnvelope Envelope = FCampaignSaveCodec::CreateNew(
		State, MakeContentPackages(), TEXT("0.45.0-test"), TestWallClock,
		FGuid(0x45454545, 0x56565656, 0x67676767, 0x78787878));
	const FCampaignSaveWriteResult Write = FCampaignSaveCodec::Serialize(Envelope);
	TestTrue(TEXT("v35 Stewardship state serializes"), Write.bSucceeded);
	TestTrue(TEXT("Current JSON carries exact active and historical Stewardship fields"),
		Write.Json.Contains(TEXT("\"status\":\"stewarding\""))
		&& Write.Json.Contains(TEXT("\"stewardshipFocus\":\"recruitment-liaison\""))
		&& Write.Json.Contains(TEXT("\"remainingStewardshipSeconds\":\"1234567\""))
		&& Write.Json.Contains(TEXT("\"stewardshipToursCompleted\":4")));
	const FCampaignSaveReadResult Read = FCampaignSaveCodec::Deserialize(
		Write.Json, MakeContentPackages());
	TestTrue(TEXT("v35 Stewardship state deserializes"), Read.bSucceeded && !Read.bMigrated);
	TestTrue(TEXT("Active focus, exact clock, and career histories round-trip"),
		Read.Envelope.State.Personnel.Num() == 1
		&& Read.Envelope.State.Personnel[0].Status == EPersonnelStatus::Stewarding
		&& Read.Envelope.State.Personnel[0].StewardshipFocus
			== EPersonnelStewardshipFocus::RecruitmentLiaison
		&& Read.Envelope.State.Personnel[0].RemainingStewardshipSeconds == 1234567
		&& Read.Envelope.State.Personnel[0].StewardshipToursCompleted == 2
		&& Read.Envelope.State.Memorial.Num() == 1
		&& Read.Envelope.State.Memorial[0].StewardshipToursCompleted == 4);

	FCampaignSaveEnvelope DifferentFocus = Write.Envelope;
	DifferentFocus.State.Personnel[0].StewardshipFocus = EPersonnelStewardshipFocus::TrainingCadre;
	TestNotEqual(TEXT("Stewardship focus participates in the save checksum"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(DifferentFocus),
		Write.Envelope.Header.SaveChecksum);
	const FString UnknownFocusJson = Write.Json.Replace(
		TEXT("\"stewardshipFocus\":\"recruitment-liaison\""),
		TEXT("\"stewardshipFocus\":\"unknown-focus\""));
	const FCampaignSaveReadResult UnknownFocus = FCampaignSaveCodec::Deserialize(
		UnknownFocusJson, MakeContentPackages());
	TestTrue(TEXT("Unknown persisted Stewardship focus is rejected before checksum acceptance"),
		!UnknownFocus.bSucceeded && UnknownFocus.HasDiagnostic(TEXT("invalid_field_value")));

	FCampaignSaveEnvelope Duplicate = Write.Envelope;
	FPersonnelState SecondSteward = Duplicate.State.Personnel[0];
	SecondSteward.PersonnelId = FGuid(0x35353555, 0x46464666, 0x57575777, 0x68686888);
	SecondSteward.DisplayName = TEXT("Cinder Okafor");
	Duplicate.State.Personnel.Add(SecondSteward);
	const FCampaignSaveValidationResult DuplicateValidation = FCampaignSaveCodec::Validate(Duplicate);
	TestTrue(TEXT("Two persisted active stewards at one base are rejected"),
		!DuplicateValidation.bSucceeded
		&& DuplicateValidation.HasDiagnostic(TEXT("invalid_personnel_state")));

	FCampaignSaveEnvelope LegacyEnvelope = Write.Envelope;
	LegacyEnvelope.Header.FormatVersion = 34;
	LegacyEnvelope.State.Personnel[0].Status = EPersonnelStatus::Available;
	LegacyEnvelope.State.Personnel[0].StewardshipFocus = EPersonnelStewardshipFocus::None;
	LegacyEnvelope.State.Personnel[0].RemainingStewardshipSeconds = 0;
	LegacyEnvelope.State.Personnel[0].StewardshipToursCompleted = 0;
	LegacyEnvelope.State.Memorial[0].StewardshipToursCompleted = 0;
	LegacyEnvelope.Header.SaveChecksum = FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyEnvelope);
	FString LegacyJson = Write.Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":34"));
	LegacyJson = LegacyJson.Replace(*Write.Envelope.Header.SaveChecksum,
		*LegacyEnvelope.Header.SaveChecksum);
	LegacyJson = LegacyJson.Replace(TEXT("\"status\":\"stewarding\""),
		TEXT("\"status\":\"available\""));
	const int32 RemovedFocus = LegacyJson.ReplaceInline(
		TEXT(",\"stewardshipFocus\":\"recruitment-liaison\""), TEXT(""));
	const int32 RemovedTimer = LegacyJson.ReplaceInline(
		TEXT(",\"remainingStewardshipSeconds\":\"1234567\""), TEXT(""));
	const int32 RemovedPersonTours = LegacyJson.ReplaceInline(
		TEXT(",\"stewardshipToursCompleted\":2"), TEXT(""));
	const int32 RemovedMemorialTours = LegacyJson.ReplaceInline(
		TEXT(",\"stewardshipToursCompleted\":4"), TEXT(""));
	const FCampaignSaveReadResult Migrated = FCampaignSaveCodec::Deserialize(
		LegacyJson, MakeContentPackages());
	TestTrue(TEXT("Verified v34 save migrates with absent Stewardship fields"),
		RemovedFocus == 1 && RemovedTimer == 1 && RemovedPersonTours == 1
		&& RemovedMemorialTours == 1 && Migrated.bSucceeded && Migrated.bMigrated
		&& Migrated.Envelope.Header.FormatVersion == FCampaignSaveCodec::CurrentFormatVersion);
	TestTrue(TEXT("v34 migration initializes neutral active and historical leadership state"),
		Migrated.Envelope.State.Personnel.Num() == 1
		&& Migrated.Envelope.State.Personnel[0].Status == EPersonnelStatus::Available
		&& Migrated.Envelope.State.Personnel[0].StewardshipFocus == EPersonnelStewardshipFocus::None
		&& Migrated.Envelope.State.Personnel[0].RemainingStewardshipSeconds == 0
		&& Migrated.Envelope.State.Personnel[0].StewardshipToursCompleted == 0
		&& Migrated.Envelope.State.Memorial.Num() == 1
		&& Migrated.Envelope.State.Memorial[0].StewardshipToursCompleted == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveMutualAidConvoyRoundTripTest,
	"UEGT.Core.CampaignSave.MutualAidConvoyRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveMutualAidConvoyRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	FCampaignState State = MakeState();
	const FGuid DestinationBaseId(0x36363636, 0x47474747, 0x58585858, 0x69696969);
	const FGuid RelayWaypointBaseId(0x36363637, 0x47474748, 0x58585859, 0x69696970);
	FStrategicBaseState& Destination = State.Bases.AddDefaulted_GetRef();
	Destination.BaseId = DestinationBaseId;
	Destination.Name = TEXT("Mutual Aid Annex");
	Destination.RegionId = TEXT("region.patagonia");
	Destination.LongitudeMilliDegrees = -72000;
	Destination.LatitudeMilliDegrees = -45000;
	Destination.ScientistCapacity = 6;
	Destination.EngineerCapacity = 6;
	FStrategicBaseState& Waypoint = State.Bases.AddDefaulted_GetRef();
	Waypoint.BaseId = RelayWaypointBaseId;
	Waypoint.Name = TEXT("Relay Waystation");
	Waypoint.RegionId = TEXT("region.north-atlantic");
	Waypoint.LongitudeMilliDegrees = -36000;
	Waypoint.LatitudeMilliDegrees = 12000;
	Waypoint.ScientistCapacity = 6;
	Waypoint.EngineerCapacity = 6;
	FMutualAidConvoyState& Convoy = State.MutualAidConvoys.AddDefaulted_GetRef();
	Convoy.ConvoyId = FGuid(0x36360001, 0x36360002, 0x36360003, 0x36360004);
	Convoy.SourceBaseId = State.Bases[0].BaseId;
	Convoy.DestinationBaseId = DestinationBaseId;
	Convoy.ItemId = TEXT("item.relief-kits");
	Convoy.Quantity = 7;
	Convoy.DispatchSequence = State.CommandSequence - 5;
	Convoy.RoutePolicy = EMutualAidRoutePolicy::RapidThread;
	Convoy.TotalTransitSeconds = 172800;
	Convoy.RemainingTransitSeconds = 129605;
	Convoy.RoutePressure = 75;
	Convoy.bSignalEscort = true;
	Convoy.SignalEscortCost = 25000;
	Convoy.bInterdictionResolved = false;
	Convoy.ForecastInterdictionDelaySeconds = 86400;
	Convoy.InterdictionDelaySeconds = 0;
	Convoy.RelayWaypointBaseId = RelayWaypointBaseId;
	Convoy.OnwardRoutePolicy = EMutualAidRoutePolicy::VeiledChain;
	Convoy.OnwardTotalTransitSeconds = 345600;
	Convoy.OnwardRoutePressure = 45;
	Convoy.bOnwardInterdictionResolved = false;
	Convoy.OnwardForecastInterdictionDelaySeconds = 86400;
	Convoy.BalancedHandoffQuantity = 3;

	const FCampaignSaveEnvelope Envelope = FCampaignSaveCodec::CreateNew(
		State, MakeContentPackages(), TEXT("0.48.0-test"), TestWallClock,
		FGuid(0x46464646, 0x57575757, 0x68686868, 0x79797979));
	const FCampaignSaveWriteResult Write = FCampaignSaveCodec::Serialize(Envelope);
	TestTrue(TEXT("Current Mutual Aid Convoy route, Relay Weave, waypoint, and handoff state serializes"),
		Write.bSucceeded
		&& Write.Envelope.Header.FormatVersion == FCampaignSaveCodec::CurrentFormatVersion
		&& Write.Json.Contains(TEXT("\"mutualAidConvoys\""))
		&& Write.Json.Contains(TEXT("\"dispatchSequence\":\"9007199254740990\""))
		&& Write.Json.Contains(TEXT("\"remainingTransitSeconds\":\"129605\""))
		&& Write.Json.Contains(TEXT("\"routePolicy\":\"rapid-thread\""))
		&& Write.Json.Contains(TEXT("\"routePressure\":75"))
		&& Write.Json.Contains(TEXT("\"signalEscort\":true"))
		&& Write.Json.Contains(TEXT("\"relayWaypointBaseId\":\"36363637-4747-4748-5858-585969696970\""))
		&& Write.Json.Contains(TEXT("\"onwardRoutePolicy\":\"veiled-chain\""))
		&& Write.Json.Contains(TEXT("\"onwardTotalTransitSeconds\":\"345600\""))
		&& Write.Json.Contains(TEXT("\"onwardRoutePressure\":45"))
		&& Write.Json.Contains(TEXT("\"onwardInterdictionResolved\":false"))
		&& Write.Json.Contains(TEXT("\"onwardForecastInterdictionDelaySeconds\":\"86400\""))
		&& Write.Json.Contains(TEXT("\"balancedHandoffQuantity\":3")));
	const FCampaignSaveReadResult Read = FCampaignSaveCodec::Deserialize(
		Write.Json, MakeContentPackages());
	TestTrue(TEXT("Convoy identities, route, cargo, and exact clock round-trip"),
		Read.bSucceeded && !Read.bMigrated
		&& Read.Envelope.State.MutualAidConvoys.Num() == 1
		&& Read.Envelope.State.MutualAidConvoys[0].ConvoyId == Convoy.ConvoyId
		&& Read.Envelope.State.MutualAidConvoys[0].SourceBaseId == Convoy.SourceBaseId
		&& Read.Envelope.State.MutualAidConvoys[0].DestinationBaseId == Convoy.DestinationBaseId
		&& Read.Envelope.State.MutualAidConvoys[0].ItemId == Convoy.ItemId
		&& Read.Envelope.State.MutualAidConvoys[0].Quantity == 7
		&& Read.Envelope.State.MutualAidConvoys[0].DispatchSequence
			== Convoy.DispatchSequence
		&& Read.Envelope.State.MutualAidConvoys[0].RoutePolicy == EMutualAidRoutePolicy::RapidThread
		&& Read.Envelope.State.MutualAidConvoys[0].TotalTransitSeconds == 172800
		&& Read.Envelope.State.MutualAidConvoys[0].RemainingTransitSeconds == 129605
		&& Read.Envelope.State.MutualAidConvoys[0].RoutePressure == 75
		&& Read.Envelope.State.MutualAidConvoys[0].bSignalEscort
		&& Read.Envelope.State.MutualAidConvoys[0].SignalEscortCost == 25000
		&& !Read.Envelope.State.MutualAidConvoys[0].bInterdictionResolved
		&& Read.Envelope.State.MutualAidConvoys[0].ForecastInterdictionDelaySeconds == 86400
		&& Read.Envelope.State.MutualAidConvoys[0].InterdictionDelaySeconds == 0
		&& !Read.Envelope.State.MutualAidConvoys[0].CurrentLegOriginBaseId.IsValid()
		&& Read.Envelope.State.MutualAidConvoys[0].RelayWaypointBaseId == RelayWaypointBaseId
		&& Read.Envelope.State.MutualAidConvoys[0].OnwardRoutePolicy
			== EMutualAidRoutePolicy::VeiledChain
		&& Read.Envelope.State.MutualAidConvoys[0].OnwardTotalTransitSeconds == 345600
		&& Read.Envelope.State.MutualAidConvoys[0].OnwardRoutePressure == 45
		&& !Read.Envelope.State.MutualAidConvoys[0].bOnwardInterdictionResolved
		&& Read.Envelope.State.MutualAidConvoys[0].OnwardForecastInterdictionDelaySeconds
			== 86400
		&& Read.Envelope.State.MutualAidConvoys[0].BalancedHandoffQuantity == 3);

	FCampaignSaveEnvelope DifferentClock = Write.Envelope;
	DifferentClock.State.MutualAidConvoys[0].RemainingTransitSeconds = 129604;
	TestNotEqual(TEXT("Convoy clock participates in the v41 save checksum"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(DifferentClock),
		Write.Envelope.Header.SaveChecksum);
	FCampaignSaveEnvelope DifferentRoute = Write.Envelope;
	DifferentRoute.State.MutualAidConvoys[0].RoutePressure = 74;
	TestNotEqual(TEXT("Threadline route state participates in the v41 save checksum"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(DifferentRoute),
		Write.Envelope.Header.SaveChecksum);
	FCampaignSaveEnvelope DifferentDispatchOrder = Write.Envelope;
	DifferentDispatchOrder.State.MutualAidConvoys[0].DispatchSequence--;
	TestNotEqual(TEXT("Relay Weave FIFO order participates in the v41 save checksum"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(DifferentDispatchOrder),
		Write.Envelope.Header.SaveChecksum);
	FCampaignSaveEnvelope DifferentWaypoint = Write.Envelope;
	DifferentWaypoint.State.MutualAidConvoys[0].RelayWaypointBaseId = State.Bases[0].BaseId;
	TestNotEqual(TEXT("Relay Waypoint identity participates in the v41 save checksum"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(DifferentWaypoint),
		Write.Envelope.Header.SaveChecksum);
	FCampaignSaveEnvelope DifferentOnwardRoute = Write.Envelope;
	DifferentOnwardRoute.State.MutualAidConvoys[0].OnwardRoutePressure = 44;
	TestNotEqual(TEXT("Relay Waypoint onward route participates in the v41 save checksum"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(DifferentOnwardRoute),
		Write.Envelope.Header.SaveChecksum);
	FCampaignSaveEnvelope DifferentHandoff = Write.Envelope;
	DifferentHandoff.State.MutualAidConvoys[0].BalancedHandoffQuantity = 0;
	TestNotEqual(TEXT("Balanced Handoff quantity participates in the v41 save checksum"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(DifferentHandoff),
		Write.Envelope.Header.SaveChecksum);

	FCampaignSaveEnvelope LegacyV40Envelope = Write.Envelope;
	LegacyV40Envelope.Header.FormatVersion = 40;
	LegacyV40Envelope.State.MutualAidConvoys[0].BalancedHandoffQuantity = 0;
	LegacyV40Envelope.Header.SaveChecksum =
		FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV40Envelope);
	FCampaignSaveEnvelope LegacyV40WithHiddenHandoff = LegacyV40Envelope;
	LegacyV40WithHiddenHandoff.State.MutualAidConvoys[0].BalancedHandoffQuantity = 3;
	TestEqual(TEXT("The verified v40 checksum domain omits future Balanced Handoff state"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV40WithHiddenHandoff),
		LegacyV40Envelope.Header.SaveChecksum);
	const FCampaignSaveValidationResult InvalidLegacyHandoff =
		FCampaignSaveCodec::Validate(LegacyV40WithHiddenHandoff);
	TestTrue(TEXT("Pre-v41 envelopes cannot hide Balanced Handoff state outside their checksum"),
		!InvalidLegacyHandoff.bSucceeded
		&& InvalidLegacyHandoff.HasDiagnostic(TEXT("invalid_mutual_aid_convoy")));
	FString LegacyV40Json = Write.Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":40"));
	LegacyV40Json = LegacyV40Json.Replace(
		*Write.Envelope.Header.SaveChecksum,
		*LegacyV40Envelope.Header.SaveChecksum);
	const int32 RemovedHandoffField = LegacyV40Json.ReplaceInline(
		TEXT(",\"balancedHandoffQuantity\":3"), TEXT(""));
	const FCampaignSaveReadResult MigratedV40 = FCampaignSaveCodec::Deserialize(
		LegacyV40Json, MakeContentPackages());
	TestTrue(TEXT("Checksum-valid v40 waypoint convoys migrate to neutral Through Cargo"),
		RemovedHandoffField == 1
		&& MigratedV40.bSucceeded && MigratedV40.bMigrated
		&& MigratedV40.Envelope.Header.FormatVersion
			== FCampaignSaveCodec::CurrentFormatVersion
		&& MigratedV40.Envelope.State.MutualAidConvoys.Num() == 1
		&& MigratedV40.Envelope.State.MutualAidConvoys[0].RelayWaypointBaseId
			== RelayWaypointBaseId
		&& MigratedV40.Envelope.State.MutualAidConvoys[0].BalancedHandoffQuantity == 0);

	FCampaignSaveEnvelope LegacyV39Envelope = Write.Envelope;
	LegacyV39Envelope.Header.FormatVersion = 39;
	FMutualAidConvoyState& LegacyConvoy = LegacyV39Envelope.State.MutualAidConvoys[0];
	LegacyConvoy.CurrentLegOriginBaseId.Invalidate();
	LegacyConvoy.RelayWaypointBaseId.Invalidate();
	LegacyConvoy.OnwardRoutePolicy = EMutualAidRoutePolicy::OpenRelay;
	LegacyConvoy.OnwardTotalTransitSeconds = 0;
	LegacyConvoy.OnwardRoutePressure = 0;
	LegacyConvoy.bOnwardInterdictionResolved = true;
	LegacyConvoy.OnwardForecastInterdictionDelaySeconds = 0;
	LegacyConvoy.BalancedHandoffQuantity = 0;
	LegacyV39Envelope.Header.SaveChecksum =
		FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV39Envelope);
	FCampaignSaveEnvelope LegacyV39WithHiddenWaypoint = LegacyV39Envelope;
	LegacyV39WithHiddenWaypoint.State.MutualAidConvoys[0].RelayWaypointBaseId =
		RelayWaypointBaseId;
	LegacyV39WithHiddenWaypoint.State.MutualAidConvoys[0].OnwardRoutePolicy =
		EMutualAidRoutePolicy::VeiledChain;
	LegacyV39WithHiddenWaypoint.State.MutualAidConvoys[0].OnwardTotalTransitSeconds = 345600;
	LegacyV39WithHiddenWaypoint.State.MutualAidConvoys[0].OnwardRoutePressure = 45;
	LegacyV39WithHiddenWaypoint.State.MutualAidConvoys[0].bOnwardInterdictionResolved = false;
	LegacyV39WithHiddenWaypoint.State.MutualAidConvoys[0].OnwardForecastInterdictionDelaySeconds =
		86400;
	TestEqual(TEXT("The verified v39 checksum domain omits future Relay Waypoint state"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV39WithHiddenWaypoint),
		LegacyV39Envelope.Header.SaveChecksum);
	const FCampaignSaveValidationResult InvalidLegacyWaypoint =
		FCampaignSaveCodec::Validate(LegacyV39WithHiddenWaypoint);
	TestTrue(TEXT("Pre-v40 envelopes cannot hide Relay Waypoint state outside their checksum"),
		!InvalidLegacyWaypoint.bSucceeded
		&& InvalidLegacyWaypoint.HasDiagnostic(TEXT("invalid_mutual_aid_convoy")));
	FString LegacyV39Json = Write.Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":39"));
	LegacyV39Json = LegacyV39Json.Replace(
		*Write.Envelope.Header.SaveChecksum,
		*LegacyV39Envelope.Header.SaveChecksum);
	const FString WaypointJson =
		TEXT(",\"currentLegOriginBaseId\":\"00000000-0000-0000-0000-000000000000\"")
		TEXT(",\"relayWaypointBaseId\":\"36363637-4747-4748-5858-585969696970\"")
		TEXT(",\"onwardRoutePolicy\":\"veiled-chain\"")
		TEXT(",\"onwardTotalTransitSeconds\":\"345600\",\"onwardRoutePressure\":45")
		TEXT(",\"onwardInterdictionResolved\":false")
		TEXT(",\"onwardForecastInterdictionDelaySeconds\":\"86400\"")
		TEXT(",\"balancedHandoffQuantity\":3");
	const int32 RemovedWaypointFields = LegacyV39Json.ReplaceInline(*WaypointJson, TEXT(""));
	const FCampaignSaveReadResult MigratedV39 = FCampaignSaveCodec::Deserialize(
		LegacyV39Json, MakeContentPackages());
	TestTrue(TEXT("Checksum-valid v39 convoys migrate to a neutral direct journey"),
		RemovedWaypointFields == 1
		&& MigratedV39.bSucceeded && MigratedV39.bMigrated
		&& MigratedV39.Envelope.Header.FormatVersion
			== FCampaignSaveCodec::CurrentFormatVersion
		&& MigratedV39.Envelope.State.MutualAidConvoys.Num() == 1
		&& !MigratedV39.Envelope.State.MutualAidConvoys[0].CurrentLegOriginBaseId.IsValid()
		&& !MigratedV39.Envelope.State.MutualAidConvoys[0].RelayWaypointBaseId.IsValid()
		&& MigratedV39.Envelope.State.MutualAidConvoys[0].OnwardRoutePolicy
			== EMutualAidRoutePolicy::OpenRelay
		&& MigratedV39.Envelope.State.MutualAidConvoys[0].OnwardTotalTransitSeconds == 0
		&& MigratedV39.Envelope.State.MutualAidConvoys[0].OnwardRoutePressure == 0
		&& MigratedV39.Envelope.State.MutualAidConvoys[0].bOnwardInterdictionResolved
		&& MigratedV39.Envelope.State.MutualAidConvoys[0]
			.OnwardForecastInterdictionDelaySeconds == 0);
	FCampaignSaveEnvelope InvalidRoute = Write.Envelope;
	InvalidRoute.State.MutualAidConvoys[0].DestinationBaseId =
		InvalidRoute.State.MutualAidConvoys[0].SourceBaseId;
	const FCampaignSaveValidationResult InvalidValidation =
		FCampaignSaveCodec::Validate(InvalidRoute);
	TestTrue(TEXT("Invalid persisted convoy routes are rejected"),
		!InvalidValidation.bSucceeded
		&& InvalidValidation.HasDiagnostic(TEXT("invalid_mutual_aid_convoy")));
	FCampaignSaveEnvelope InvalidDestinationInventoryCapacity = Write.Envelope;
	FStrategicBaseState* OverflowDestination =
		InvalidDestinationInventoryCapacity.State.Bases.FindByPredicate(
			[&DestinationBaseId](FStrategicBaseState& Base)
			{
				return Base.BaseId == DestinationBaseId;
			});
	TestNotNull(TEXT("Destination base remains addressable for convoy capacity validation"),
		OverflowDestination);
	if (OverflowDestination != nullptr)
	{
		FInventoryStack& OverflowStack = OverflowDestination->Inventory.AddDefaulted_GetRef();
		OverflowStack.ItemId = Convoy.ItemId;
		OverflowStack.Quantity = MAX_int32 - 3;
		const FCampaignSaveValidationResult InvalidDestinationCapacity =
			FCampaignSaveCodec::Validate(InvalidDestinationInventoryCapacity);
		TestTrue(TEXT("Pending final deliveries cannot overflow destination inventory"),
			!InvalidDestinationCapacity.bSucceeded
			&& InvalidDestinationCapacity.HasDiagnostic(
				TEXT("mutual_aid_inventory_overflow")));
	}
	FCampaignSaveEnvelope InvalidWaypointInventoryCapacity = Write.Envelope;
	FStrategicBaseState* OverflowWaypoint =
		InvalidWaypointInventoryCapacity.State.Bases.FindByPredicate(
			[&RelayWaypointBaseId](FStrategicBaseState& Base)
			{
				return Base.BaseId == RelayWaypointBaseId;
			});
	TestNotNull(TEXT("Relay waypoint remains addressable for convoy capacity validation"),
		OverflowWaypoint);
	if (OverflowWaypoint != nullptr)
	{
		FInventoryStack& OverflowStack = OverflowWaypoint->Inventory.AddDefaulted_GetRef();
		OverflowStack.ItemId = Convoy.ItemId;
		OverflowStack.Quantity = MAX_int32 - 2;
		const FCampaignSaveValidationResult InvalidWaypointCapacity =
			FCampaignSaveCodec::Validate(InvalidWaypointInventoryCapacity);
		TestTrue(TEXT("Pending waypoint handoffs cannot overflow relay inventory"),
			!InvalidWaypointCapacity.bSucceeded
			&& InvalidWaypointCapacity.HasDiagnostic(
				TEXT("mutual_aid_inventory_overflow")));
	}

	FCampaignSaveEnvelope LegacyV35Envelope = Write.Envelope;
	LegacyV35Envelope.Header.FormatVersion = 35;
	LegacyV35Envelope.State.MutualAidConvoys.Reset();
	LegacyV35Envelope.Header.SaveChecksum =
		FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV35Envelope);
	FString LegacyV35Json = Write.Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":35"));
	LegacyV35Json = LegacyV35Json.Replace(
		*Write.Envelope.Header.SaveChecksum, *LegacyV35Envelope.Header.SaveChecksum);
	const int32 ConvoyFieldStart = LegacyV35Json.Find(TEXT(",\"mutualAidConvoys\":"));
	const int32 ResearchFieldStart = LegacyV35Json.Find(
		TEXT(",\"researchProjects\":"), ESearchCase::CaseSensitive,
		ESearchDir::FromStart, ConvoyFieldStart);
	TestTrue(TEXT("v35 migration fixture finds the v36 convoy field"),
		ConvoyFieldStart != INDEX_NONE && ResearchFieldStart > ConvoyFieldStart);
	if (ConvoyFieldStart != INDEX_NONE && ResearchFieldStart > ConvoyFieldStart)
	{
		LegacyV35Json.RemoveAt(
			ConvoyFieldStart, ResearchFieldStart - ConvoyFieldStart, EAllowShrinking::No);
	}
	const FCampaignSaveReadResult Migrated = FCampaignSaveCodec::Deserialize(
		LegacyV35Json, MakeContentPackages());
	TestTrue(TEXT("Verified v35 save migrates with a neutral convoy ledger"),
		Migrated.bSucceeded && Migrated.bMigrated
		&& Migrated.Envelope.Header.FormatVersion == FCampaignSaveCodec::CurrentFormatVersion
		&& Migrated.Envelope.State.MutualAidConvoys.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveSignalWatchRoundTripTest,
	"UEGT.Core.CampaignSave.SignalWatchRoundTripAndMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveSignalWatchRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	FCampaignState State = MakeState();
	State.Bases[0].SignalWatchScientists = 1;
	const FCampaignSaveEnvelope Envelope = FCampaignSaveCodec::CreateNew(
		State, MakeContentPackages(), TEXT("0.49.0-test"), TestWallClock,
		FGuid(0x39390001, 0x39390002, 0x39390003, 0x39390004));
	const FCampaignSaveWriteResult Write = FCampaignSaveCodec::Serialize(Envelope);
	TestTrue(TEXT("Current Signal Watch staffing serializes as an explicit base commitment"),
		Write.bSucceeded
		&& Write.Envelope.Header.FormatVersion == FCampaignSaveCodec::CurrentFormatVersion
		&& Write.Json.Contains(TEXT("\"signalWatchScientists\":1")));
	const FCampaignSaveReadResult Read = FCampaignSaveCodec::Deserialize(
		Write.Json, MakeContentPackages());
	TestTrue(TEXT("Current Signal Watch staffing round-trips without migration"),
		Read.bSucceeded && !Read.bMigrated
		&& Read.Envelope.State.Bases.Num() == 1
		&& Read.Envelope.State.Bases[0].SignalWatchScientists == 1);

	FCampaignSaveEnvelope DifferentWatch = Write.Envelope;
	DifferentWatch.State.Bases[0].SignalWatchScientists = 0;
	TestNotEqual(TEXT("Signal Watch staffing participates in the current checksum"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(DifferentWatch),
		Write.Envelope.Header.SaveChecksum);

	FCampaignSaveEnvelope LegacyV38Envelope = Write.Envelope;
	LegacyV38Envelope.Header.FormatVersion = 38;
	LegacyV38Envelope.State.Bases[0].SignalWatchScientists = 0;
	LegacyV38Envelope.Header.SaveChecksum =
		FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV38Envelope);
	FCampaignSaveEnvelope LegacyV38WithUnserializedWatch = LegacyV38Envelope;
	LegacyV38WithUnserializedWatch.State.Bases[0].SignalWatchScientists = 1;
	LegacyV38WithUnserializedWatch.Header.SaveChecksum =
		FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV38WithUnserializedWatch);
	TestEqual(TEXT("The verified v38 checksum domain remains byte-compatible and omits future staffing"),
		LegacyV38WithUnserializedWatch.Header.SaveChecksum,
		LegacyV38Envelope.Header.SaveChecksum);
	const FCampaignSaveValidationResult InvalidLegacyState =
		FCampaignSaveCodec::Validate(LegacyV38WithUnserializedWatch);
	TestTrue(TEXT("Pre-v39 envelopes cannot hide non-neutral Signal Watch state outside their checksum"),
		!InvalidLegacyState.bSucceeded
		&& InvalidLegacyState.HasDiagnostic(TEXT("invalid_base_capacity")));

	FString LegacyV38Json = Write.Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":38"));
	LegacyV38Json = LegacyV38Json.Replace(
		*Write.Envelope.Header.SaveChecksum,
		*LegacyV38Envelope.Header.SaveChecksum);
	const int32 RemovedWatchField = LegacyV38Json.ReplaceInline(
		TEXT(",\"signalWatchScientists\":1"), TEXT(""));
	const FCampaignSaveReadResult MigratedV38 = FCampaignSaveCodec::Deserialize(
		LegacyV38Json, MakeContentPackages());
	TestTrue(TEXT("Checksum-valid v38 bases migrate to a neutral Signal Watch assignment"),
		RemovedWatchField == 1
		&& MigratedV38.bSucceeded && MigratedV38.bMigrated
		&& MigratedV38.Envelope.Header.FormatVersion
			== FCampaignSaveCodec::CurrentFormatVersion
		&& MigratedV38.Envelope.State.Bases.Num() == 1
		&& MigratedV38.Envelope.State.Bases[0].SignalWatchScientists == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveWorksCadreRoundTripTest,
	"UEGT.Core.CampaignSave.WorksCadreRoundTripAndMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveWorksCadreRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	FCampaignState State = MakeState();
	State.Bases[0].WorksCadreEngineers = 2;
	const FCampaignSaveEnvelope Envelope = FCampaignSaveCodec::CreateNew(
		State, MakeContentPackages(), TEXT("0.58.0-test"), TestWallClock,
		FGuid(0x42420001, 0x42420002, 0x42420003, 0x42420004));
	const FCampaignSaveWriteResult Write = FCampaignSaveCodec::Serialize(Envelope);
	TestTrue(TEXT("Current Works Cadre staffing serializes as an explicit base commitment"),
		Write.bSucceeded
		&& Write.Envelope.Header.FormatVersion == FCampaignSaveCodec::CurrentFormatVersion
		&& Write.Json.Contains(TEXT("\"worksCadreEngineers\":2")));
	const FCampaignSaveReadResult Read = FCampaignSaveCodec::Deserialize(
		Write.Json, MakeContentPackages());
	TestTrue(TEXT("Current Works Cadre staffing round-trips without migration"),
		Read.bSucceeded && !Read.bMigrated
		&& Read.Envelope.State.Bases.Num() == 1
		&& Read.Envelope.State.Bases[0].WorksCadreEngineers == 2);

	FCampaignSaveEnvelope DifferentCadre = Write.Envelope;
	DifferentCadre.State.Bases[0].WorksCadreEngineers = 1;
	TestNotEqual(TEXT("Works Cadre staffing participates in the current checksum"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(DifferentCadre),
		Write.Envelope.Header.SaveChecksum);

	FCampaignSaveEnvelope InvalidCurrent = Write.Envelope;
	InvalidCurrent.State.Bases[0].WorksCadreEngineers = 4;
	const FCampaignSaveValidationResult InvalidCurrentValidation =
		FCampaignSaveCodec::Validate(InvalidCurrent);
	TestTrue(TEXT("Current saves reject Works Cadre staffing above its three-engineer limit"),
		!InvalidCurrentValidation.bSucceeded
		&& InvalidCurrentValidation.HasDiagnostic(TEXT("invalid_base_capacity")));

	FCampaignSaveEnvelope LegacyV41Envelope = Write.Envelope;
	LegacyV41Envelope.Header.FormatVersion = 41;
	LegacyV41Envelope.State.Bases[0].WorksCadreEngineers = 0;
	LegacyV41Envelope.State.Bases[0].WorksCadreCharter =
		EWorksCadreCharter::CommonCadence;
	LegacyV41Envelope.Header.SaveChecksum =
		FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV41Envelope);
	FCampaignSaveEnvelope LegacyV41WithUnserializedCadre = LegacyV41Envelope;
	LegacyV41WithUnserializedCadre.State.Bases[0].WorksCadreEngineers = 2;
	LegacyV41WithUnserializedCadre.Header.SaveChecksum =
		FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV41WithUnserializedCadre);
	TestEqual(TEXT("The verified v41 checksum domain omits future Works Cadre staffing"),
		LegacyV41WithUnserializedCadre.Header.SaveChecksum,
		LegacyV41Envelope.Header.SaveChecksum);
	const FCampaignSaveValidationResult InvalidLegacyState =
		FCampaignSaveCodec::Validate(LegacyV41WithUnserializedCadre);
	TestTrue(TEXT("Pre-v42 envelopes cannot hide non-neutral Works Cadre state"),
		!InvalidLegacyState.bSucceeded
		&& InvalidLegacyState.HasDiagnostic(TEXT("invalid_base_capacity")));

	FString LegacyV41Json = Write.Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"),
			FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":41"));
	LegacyV41Json = LegacyV41Json.Replace(
		*Write.Envelope.Header.SaveChecksum,
		*LegacyV41Envelope.Header.SaveChecksum);
	const int32 RemovedCadreField = LegacyV41Json.ReplaceInline(
		TEXT(",\"worksCadreEngineers\":2"), TEXT(""));
	const int32 RemovedCharterField = LegacyV41Json.ReplaceInline(
		TEXT(",\"worksCadreCharter\":\"common-cadence\""), TEXT(""));
	const FCampaignSaveReadResult MigratedV41 = FCampaignSaveCodec::Deserialize(
		LegacyV41Json, MakeContentPackages());
	TestTrue(TEXT("Checksum-valid v41 bases migrate to a neutral Works Cadre assignment"),
		RemovedCadreField == 1 && RemovedCharterField == 1
		&& MigratedV41.bSucceeded && MigratedV41.bMigrated
		&& MigratedV41.Envelope.Header.FormatVersion
			== FCampaignSaveCodec::CurrentFormatVersion
		&& MigratedV41.Envelope.State.Bases.Num() == 1
		&& MigratedV41.Envelope.State.Bases[0].WorksCadreEngineers == 0
		&& MigratedV41.Envelope.State.Bases[0].WorksCadreCharter
			== EWorksCadreCharter::CommonCadence);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveWorksCadreCharterRoundTripTest,
	"UEGT.Core.CampaignSave.WorksCadreCharterRoundTripAndMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveWorksCadreCharterRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	FCampaignState State = MakeState();
	State.Bases[0].WorksCadreEngineers = 2;
	State.Bases[0].WorksCadreCharter = EWorksCadreCharter::AssemblyCadence;
	const FCampaignSaveEnvelope Envelope = FCampaignSaveCodec::CreateNew(
		State, MakeContentPackages(), TEXT("0.58.0-test"), TestWallClock,
		FGuid(0x43430001, 0x43430002, 0x43430003, 0x43430004));
	const FCampaignSaveWriteResult Write = FCampaignSaveCodec::Serialize(Envelope);
	TestTrue(TEXT("Current Works Charter serializes as a stable string"),
		Write.bSucceeded
		&& Write.Envelope.Header.FormatVersion == FCampaignSaveCodec::CurrentFormatVersion
		&& Write.Json.Contains(
			TEXT("\"worksCadreCharter\":\"assembly-cadence\"")));
	const FCampaignSaveReadResult Read = FCampaignSaveCodec::Deserialize(
		Write.Json, MakeContentPackages());
	TestTrue(TEXT("Current Works Charter round-trips without migration"),
		Read.bSucceeded && !Read.bMigrated
		&& Read.Envelope.State.Bases.Num() == 1
		&& Read.Envelope.State.Bases[0].WorksCadreEngineers == 2
		&& Read.Envelope.State.Bases[0].WorksCadreCharter
			== EWorksCadreCharter::AssemblyCadence);

	FCampaignSaveEnvelope DifferentCharter = Write.Envelope;
	DifferentCharter.State.Bases[0].WorksCadreCharter =
		EWorksCadreCharter::RestorationCadence;
	TestNotEqual(TEXT("Works Charter participates in the current checksum"),
		FCampaignSaveCodec::ComputeEnvelopeChecksum(DifferentCharter),
		Write.Envelope.Header.SaveChecksum);

	FCampaignSaveEnvelope InvalidCurrent = Write.Envelope;
	InvalidCurrent.State.Bases[0].WorksCadreCharter =
		static_cast<EWorksCadreCharter>(255);
	const FCampaignSaveValidationResult InvalidCurrentValidation =
		FCampaignSaveCodec::Validate(InvalidCurrent);
	TestTrue(TEXT("Current saves reject unknown Works Charters"),
		!InvalidCurrentValidation.bSucceeded
		&& InvalidCurrentValidation.HasDiagnostic(
			TEXT("invalid_works_cadre_charter")));

	FCampaignSaveEnvelope LegacyV42Envelope = Write.Envelope;
	LegacyV42Envelope.Header.FormatVersion = 42;
	LegacyV42Envelope.State.Bases[0].WorksCadreCharter =
		EWorksCadreCharter::CommonCadence;
	LegacyV42Envelope.Header.SaveChecksum =
		FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV42Envelope);
	FCampaignSaveEnvelope LegacyV42WithUnserializedCharter = LegacyV42Envelope;
	LegacyV42WithUnserializedCharter.State.Bases[0].WorksCadreCharter =
		EWorksCadreCharter::AssemblyCadence;
	LegacyV42WithUnserializedCharter.Header.SaveChecksum =
		FCampaignSaveCodec::ComputeEnvelopeChecksum(
			LegacyV42WithUnserializedCharter);
	TestEqual(TEXT("The verified v42 checksum domain omits future Works Charters"),
		LegacyV42WithUnserializedCharter.Header.SaveChecksum,
		LegacyV42Envelope.Header.SaveChecksum);
	const FCampaignSaveValidationResult InvalidLegacyState =
		FCampaignSaveCodec::Validate(LegacyV42WithUnserializedCharter);
	TestTrue(TEXT("Pre-v43 envelopes cannot hide a non-neutral Works Charter"),
		!InvalidLegacyState.bSucceeded
		&& InvalidLegacyState.HasDiagnostic(
			TEXT("invalid_works_cadre_charter")));

	FString LegacyV42Json = Write.Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":42"));
	LegacyV42Json = LegacyV42Json.Replace(
		*Write.Envelope.Header.SaveChecksum,
		*LegacyV42Envelope.Header.SaveChecksum);
	const int32 RemovedCharterField = LegacyV42Json.ReplaceInline(
		TEXT(",\"worksCadreCharter\":\"assembly-cadence\""), TEXT(""));
	const FCampaignSaveReadResult MigratedV42 = FCampaignSaveCodec::Deserialize(
		LegacyV42Json, MakeContentPackages());
	TestTrue(TEXT("Checksum-valid v42 bases migrate to Common Cadence"),
		RemovedCharterField == 1
		&& MigratedV42.bSucceeded && MigratedV42.bMigrated
		&& MigratedV42.Envelope.Header.FormatVersion == FCampaignSaveCodec::CurrentFormatVersion
		&& MigratedV42.Envelope.State.Bases.Num() == 1
		&& MigratedV42.Envelope.State.Bases[0].WorksCadreEngineers == 2
		&& MigratedV42.Envelope.State.Bases[0].WorksCadreCharter
			== EWorksCadreCharter::CommonCadence);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveMutualAidRoutingMigrationTest,
	"UEGT.Core.CampaignSave.MutualAidRoutingMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveMutualAidRoutingMigrationTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	FCampaignState State = MakeState();
	FStrategicBaseState& Destination = State.Bases.AddDefaulted_GetRef();
	Destination.BaseId = FGuid(0x37370001, 0x37370002, 0x37370003, 0x37370004);
	Destination.Name = TEXT("Threadline Annex");
	Destination.RegionId = TEXT("region.patagonia");
	Destination.LongitudeMilliDegrees = -72000;
	Destination.LatitudeMilliDegrees = -45000;
	Destination.ScientistCapacity = 6;
	Destination.EngineerCapacity = 6;
	FMutualAidConvoyState& Convoy = State.MutualAidConvoys.AddDefaulted_GetRef();
	Convoy.ConvoyId = FGuid(0x37371001, 0x37371002, 0x37371003, 0x37371004);
	Convoy.SourceBaseId = State.Bases[0].BaseId;
	Convoy.DestinationBaseId = Destination.BaseId;
	Convoy.ItemId = TEXT("item.relief-kits");
	Convoy.Quantity = 5;
	Convoy.DispatchSequence = State.CommandSequence - 5;
	Convoy.RoutePolicy = EMutualAidRoutePolicy::RapidThread;
	Convoy.TotalTransitSeconds = 172800;
	Convoy.RemainingTransitSeconds = 129605;
	Convoy.RoutePressure = 75;
	Convoy.bSignalEscort = true;
	Convoy.SignalEscortCost = 25000;
	Convoy.bInterdictionResolved = false;
	Convoy.ForecastInterdictionDelaySeconds = 86400;
	Convoy.InterdictionDelaySeconds = 0;

	const FCampaignSaveEnvelope Envelope = FCampaignSaveCodec::CreateNew(
		State, MakeContentPackages(), TEXT("0.48.0-test"), TestWallClock,
		FGuid(0x47474747, 0x58585858, 0x69696969, 0x7a7a7a7a));
	const FCampaignSaveWriteResult Write = FCampaignSaveCodec::Serialize(Envelope);
	TestTrue(TEXT("Current Mutual Aid routing fixture serializes"), Write.bSucceeded);

	FCampaignSaveEnvelope LegacyV36Envelope = Write.Envelope;
	LegacyV36Envelope.Header.FormatVersion = 36;
	LegacyV36Envelope.Header.SaveChecksum =
		FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV36Envelope);
	FString LegacyV36Json = Write.Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":36"));
	LegacyV36Json = LegacyV36Json.Replace(
		*Write.Envelope.Header.SaveChecksum, *LegacyV36Envelope.Header.SaveChecksum);
	LegacyV36Json = LegacyV36Json.Replace(
		TEXT(",\"dispatchSequence\":\"9007199254740990\""), TEXT(""));
	const FString RoutingJson =
		TEXT(",\"routePolicy\":\"rapid-thread\",\"totalTransitSeconds\":\"172800\",\"routePressure\":75")
		TEXT(",\"signalEscort\":true,\"signalEscortCost\":\"25000\",\"interdictionResolved\":false")
		TEXT(",\"forecastInterdictionDelaySeconds\":\"86400\",\"interdictionDelaySeconds\":\"0\"");
	const int32 RemovedRoutingFields = LegacyV36Json.ReplaceInline(*RoutingJson, TEXT(""));
	const FCampaignSaveReadResult Migrated = FCampaignSaveCodec::Deserialize(
		LegacyV36Json, MakeContentPackages());
	TestTrue(TEXT("Verified v36 checksum accepts a convoy without routing fields"),
		RemovedRoutingFields == 1 && Migrated.bSucceeded && Migrated.bMigrated
		&& Migrated.Envelope.Header.FormatVersion == FCampaignSaveCodec::CurrentFormatVersion);
	TestTrue(TEXT("v36 convoy migrates to a resolved lossless Open Relay"),
		Migrated.Envelope.State.MutualAidConvoys.Num() == 1
		&& Migrated.Envelope.State.MutualAidConvoys[0].ConvoyId == Convoy.ConvoyId
		&& Migrated.Envelope.State.MutualAidConvoys[0].Quantity == 5
		&& Migrated.Envelope.State.MutualAidConvoys[0].RoutePolicy == EMutualAidRoutePolicy::OpenRelay
		&& Migrated.Envelope.State.MutualAidConvoys[0].TotalTransitSeconds == 259200
		&& Migrated.Envelope.State.MutualAidConvoys[0].RemainingTransitSeconds == 129605
		&& Migrated.Envelope.State.MutualAidConvoys[0].RoutePressure == 0
		&& !Migrated.Envelope.State.MutualAidConvoys[0].bSignalEscort
		&& Migrated.Envelope.State.MutualAidConvoys[0].SignalEscortCost == 0
		&& Migrated.Envelope.State.MutualAidConvoys[0].DispatchSequence > 0
		&& Migrated.Envelope.State.MutualAidConvoys[0].DispatchSequence
			<= Migrated.Envelope.State.CommandSequence
		&& Migrated.Envelope.State.MutualAidConvoys[0].bInterdictionResolved
		&& Migrated.Envelope.State.MutualAidConvoys[0].ForecastInterdictionDelaySeconds == 86400
		&& Migrated.Envelope.State.MutualAidConvoys[0].InterdictionDelaySeconds == 0);

	FCampaignSaveEnvelope LegacyV37Envelope = Write.Envelope;
	LegacyV37Envelope.Header.FormatVersion = 37;
	LegacyV37Envelope.Header.SaveChecksum =
		FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV37Envelope);
	FString LegacyV37Json = Write.Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":37"));
	LegacyV37Json = LegacyV37Json.Replace(
		*Write.Envelope.Header.SaveChecksum, *LegacyV37Envelope.Header.SaveChecksum);
	const int32 RemovedDispatchSequence = LegacyV37Json.ReplaceInline(
		TEXT(",\"dispatchSequence\":\"9007199254740990\""), TEXT(""));
	const FCampaignSaveReadResult MigratedV37 = FCampaignSaveCodec::Deserialize(
		LegacyV37Json, MakeContentPackages());
	TestTrue(TEXT("Checksum-valid v37 routing gains deterministic FIFO queue order"),
		RemovedDispatchSequence == 1 && MigratedV37.bSucceeded && MigratedV37.bMigrated
		&& MigratedV37.Envelope.Header.FormatVersion == FCampaignSaveCodec::CurrentFormatVersion
		&& MigratedV37.Envelope.State.MutualAidConvoys.Num() == 1
		&& MigratedV37.Envelope.State.MutualAidConvoys[0].RoutePolicy
			== EMutualAidRoutePolicy::RapidThread
		&& MigratedV37.Envelope.State.MutualAidConvoys[0].RoutePressure == 75
		&& MigratedV37.Envelope.State.MutualAidConvoys[0].DispatchSequence > 0
		&& MigratedV37.Envelope.State.MutualAidConvoys[0].DispatchSequence
			<= MigratedV37.Envelope.State.CommandSequence);

	FCampaignSaveEnvelope LegacyV37TerminalSequenceEnvelope = Write.Envelope;
	LegacyV37TerminalSequenceEnvelope.Header.FormatVersion = 37;
	LegacyV37TerminalSequenceEnvelope.State.CommandSequence = MAX_int64;
	LegacyV37TerminalSequenceEnvelope.Header.SaveChecksum =
		FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV37TerminalSequenceEnvelope);
	FString LegacyV37TerminalSequenceJson = Write.Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":37"));
	LegacyV37TerminalSequenceJson = LegacyV37TerminalSequenceJson.Replace(
		*Write.Envelope.Header.SaveChecksum,
		*LegacyV37TerminalSequenceEnvelope.Header.SaveChecksum);
	LegacyV37TerminalSequenceJson = LegacyV37TerminalSequenceJson.Replace(
		*FString::Printf(TEXT("\"commandSequence\":\"%lld\""), Write.Envelope.State.CommandSequence),
		TEXT("\"commandSequence\":\"9223372036854775807\""));
	LegacyV37TerminalSequenceJson.ReplaceInline(
		TEXT(",\"dispatchSequence\":\"9007199254740990\""), TEXT(""));
	const FCampaignSaveReadResult MigratedV37TerminalSequence =
		FCampaignSaveCodec::Deserialize(LegacyV37TerminalSequenceJson, MakeContentPackages());
	TestTrue(TEXT("A v37 terminal command sequence is rejected after safe convoy-order migration"),
		!MigratedV37TerminalSequence.bSucceeded
		&& MigratedV37TerminalSequence.HasDiagnostic(TEXT("invalid_command_sequence")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveTamperTest,
	"UEGT.Core.CampaignSave.CorruptionDetection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveTamperTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	const FCampaignSaveWriteResult Write = MakeSerializedSave();
	TestTrue(TEXT("Fixture serializes"), Write.bSucceeded);
	const FString TamperedJson = Write.Json.Replace(TEXT("\"funds\":\"9007199254740993\""), TEXT("\"funds\":\"9007199254740994\""));
	TestNotEqual(TEXT("Fixture mutation changes JSON"), TamperedJson, Write.Json);

	const FCampaignSaveReadResult Read = FCampaignSaveCodec::Deserialize(TamperedJson);
	TestFalse(TEXT("Tampered campaign save is rejected"), Read.bSucceeded);
	TestTrue(TEXT("Tampering reports checksum mismatch"), Read.HasDiagnostic(TEXT("save_checksum_mismatch")));

	const FString SavedRandomField = FString::Printf(
		TEXT("\"randomState\":\"%016llx\""),
		static_cast<unsigned long long>(Write.Envelope.State.SimulationRandom.GetStateForSave()));
	const FString InvalidRandomJson = Write.Json.Replace(
		*SavedRandomField,
		TEXT("\"randomState\":\"0000000000000000\""));
	const FCampaignSaveReadResult InvalidRandom = FCampaignSaveCodec::Deserialize(InvalidRandomJson);
	TestFalse(TEXT("Impossible random state is rejected"), InvalidRandom.bSucceeded);
	TestTrue(TEXT("Impossible random state is diagnosed"), InvalidRandom.HasDiagnostic(TEXT("invalid_random_state")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveCompatibilityTest,
	"UEGT.Core.CampaignSave.ContentCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	const FCampaignSaveWriteResult Write = MakeSerializedSave();
	TArray<FCampaignContentVersion> IncompatiblePackages = MakeContentPackages();
	IncompatiblePackages[0].Version = TEXT("2.0.0");
	const FCampaignSaveReadResult Incompatible = FCampaignSaveCodec::Deserialize(Write.Json, IncompatiblePackages);
	TestFalse(TEXT("Different active content is rejected"), Incompatible.bSucceeded);
	TestTrue(TEXT("Content mismatch has a stable diagnostic"), Incompatible.HasDiagnostic(TEXT("incompatible_content")));

	FCampaignSaveEnvelope Invalid = Write.Envelope;
	Invalid.State.CommandSequence = -1;
	const FCampaignSaveValidationResult Validation = FCampaignSaveCodec::Validate(Invalid);
	TestFalse(TEXT("Invalid domain state fails validation"), Validation.bSucceeded);
	TestTrue(TEXT("Negative command sequence is diagnosed"), Validation.HasDiagnostic(TEXT("invalid_command_sequence")));
	Invalid.State.CommandSequence = MAX_int64;
	const FCampaignSaveValidationResult ExhaustedSequenceValidation = FCampaignSaveCodec::Validate(Invalid);
	TestFalse(TEXT("Exhausted command sequence fails validation"), ExhaustedSequenceValidation.bSucceeded);
	TestTrue(TEXT("Exhausted command sequence is diagnosed"), ExhaustedSequenceValidation.HasDiagnostic(TEXT("invalid_command_sequence")));
	Invalid.State.CommandSequence = MAX_int64 - 1;
	const FCampaignSaveValidationResult LastSequenceValidation = FCampaignSaveCodec::Validate(Invalid);
	TestFalse(TEXT("A command sequence with no room for another command fails validation"), LastSequenceValidation.bSucceeded);
	TestTrue(TEXT("A last available command sequence is diagnosed"), LastSequenceValidation.HasDiagnostic(TEXT("invalid_command_sequence")));
	Invalid.State.CommandSequence = MAX_int64 - 3;
	Invalid.Header.SaveChecksum = FCampaignSaveCodec::ComputeEnvelopeChecksum(Invalid);
	const FCampaignSaveValidationResult LastUsableSequenceValidation = FCampaignSaveCodec::Validate(Invalid);
	TestTrue(TEXT("A sequence before the mutation boundary remains save-valid"), LastUsableSequenceValidation.bSucceeded);
	Invalid.State.CommandSequence = MAX_int64 - 2;
	const FCampaignSaveValidationResult PreTerminalSequenceValidation = FCampaignSaveCodec::Validate(Invalid);
	TestFalse(TEXT("A command sequence that would reach the terminal value fails validation"), PreTerminalSequenceValidation.bSucceeded);
	TestTrue(TEXT("A pre-terminal command sequence is diagnosed"), PreTerminalSequenceValidation.HasDiagnostic(TEXT("invalid_command_sequence")));
	Invalid.State.CommandSequence = Write.Envelope.State.CommandSequence;
	Invalid.State.NextAdversaryMissionSerial = MAX_int64;
	const FCampaignSaveValidationResult ExhaustedMissionSerialValidation = FCampaignSaveCodec::Validate(Invalid);
	TestFalse(TEXT("Exhausted adversary mission serial fails validation"), ExhaustedMissionSerialValidation.bSucceeded);
	TestTrue(TEXT("Exhausted adversary mission serial is diagnosed"), ExhaustedMissionSerialValidation.HasDiagnostic(TEXT("invalid_adversary_state")));
	Invalid.State.NextAdversaryMissionSerial = MAX_int64 - 1;
	const FCampaignSaveValidationResult LastMissionSerialValidation = FCampaignSaveCodec::Validate(Invalid);
	TestFalse(TEXT("A mission serial with no room for another launch fails validation"), LastMissionSerialValidation.bSucceeded);
	TestTrue(TEXT("A last available mission serial is diagnosed"), LastMissionSerialValidation.HasDiagnostic(TEXT("invalid_adversary_state")));
	Invalid.State.NextAdversaryMissionSerial = MAX_int64 - 3;
	Invalid.Header.SaveChecksum = FCampaignSaveCodec::ComputeEnvelopeChecksum(Invalid);
	const FCampaignSaveValidationResult LastUsableMissionSerialValidation = FCampaignSaveCodec::Validate(Invalid);
	TestTrue(TEXT("A mission serial before the mutation boundary remains save-valid"), LastUsableMissionSerialValidation.bSucceeded);
	Invalid.State.NextAdversaryMissionSerial = MAX_int64 - 2;
	const FCampaignSaveValidationResult PreTerminalMissionSerialValidation = FCampaignSaveCodec::Validate(Invalid);
	TestFalse(TEXT("A mission serial that would reach the terminal value fails validation"), PreTerminalMissionSerialValidation.bSucceeded);
	TestTrue(TEXT("A pre-terminal mission serial is diagnosed"), PreTerminalMissionSerialValidation.HasDiagnostic(TEXT("invalid_adversary_state")));
	Invalid.State.SimulationRandom.DrawCount = MAX_int64;
	const FCampaignSaveValidationResult ExhaustedRandomValidation = FCampaignSaveCodec::Validate(Invalid);
	TestFalse(TEXT("Exhausted random draw count fails validation"), ExhaustedRandomValidation.bSucceeded);
	TestTrue(TEXT("Exhausted random draw count is diagnosed"), ExhaustedRandomValidation.HasDiagnostic(TEXT("invalid_random_state")));

	FTacticalBattleState ExtremeBattle;
	ExtremeBattle.Width = MAX_int32;
	ExtremeBattle.Height = MAX_int32;
	ExtremeBattle.Levels = 1;
	TestEqual(TEXT("Extreme tactical dimensions cannot overflow a cell index"),
		ExtremeBattle.GetCellIndex(MAX_int32 - 1, MAX_int32 - 1, 0), INDEX_NONE);
	ExtremeBattle.Width = 8;
	ExtremeBattle.Height = 12;
	TestEqual(TEXT("Valid tactical dimensions preserve cell indexing"),
		ExtremeBattle.GetCellIndex(3, 4, 0), 35);

	const FCampaignSaveReadResult Malformed = FCampaignSaveCodec::Deserialize(TEXT("{broken"));
	TestFalse(TEXT("Malformed JSON is rejected"), Malformed.bSucceeded);
	TestTrue(TEXT("Malformed JSON is diagnosed"), Malformed.HasDiagnostic(TEXT("invalid_json")));

	const FString CurrentVersionField = FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion);
	const FString FutureJson = Write.Json.Replace(*CurrentVersionField, TEXT("\"formatVersion\":99"));
	const FCampaignSaveReadResult Future = FCampaignSaveCodec::Deserialize(FutureJson);
	TestFalse(TEXT("Future save is rejected safely"), Future.bSucceeded);
	TestTrue(TEXT("Future save version is diagnosed"), Future.HasDiagnostic(TEXT("unsupported_format_version")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveMigrationTest,
	"UEGT.Core.CampaignSave.LegacyMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveMigrationTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveTests;

	TArray<FCampaignContentVersion> Packages;
	FCampaignContentVersion Base;
	Base.PackageId = TEXT("uegt.base");
	Base.Version = TEXT("1.0.0");
	Packages.Add(Base);
	const FString Fingerprint = FCampaignSaveCodec::ComputeContentFingerprint(Packages);
	const FDeterministicRandomStream Random(7);
	const FString LegacyJson = FString::Printf(
		TEXT("{\"header\":{\"formatVersion\":1,\"campaignId\":\"%s\",\"createdUtc\":\"2026-08-29T20:15:30.000Z\",\"lastSavedUtc\":\"2026-08-29T20:15:30.000Z\",\"buildVersion\":\"0.1.0\",\"contentPackages\":[{\"id\":\"uegt.base\",\"version\":\"1.0.0\"}],\"contentFingerprint\":\"%s\"},\"state\":{\"strategicUtc\":\"2035-01-01T12:00:00.000Z\",\"randomInitialSeed\":\"7\",\"randomDrawCount\":\"0\",\"randomState\":\"%016llx\",\"funds\":\"750000\"}}"),
		*TestCampaignId.ToString(EGuidFormats::DigitsWithHyphensLower),
		*Fingerprint,
		static_cast<unsigned long long>(Random.GetStateForSave()));

	const FCampaignSaveReadResult Migrated = FCampaignSaveCodec::Deserialize(LegacyJson, Packages);
	TestTrue(TEXT("Supported legacy save migrates"), Migrated.bSucceeded);
	TestTrue(TEXT("Migration is reported"), Migrated.bMigrated);
	TestTrue(TEXT("Legacy checksum limitation is reported"), Migrated.HasDiagnostic(TEXT("legacy_save_without_checksum")));
	TestTrue(TEXT("Migration diagnostic is present"), Migrated.HasDiagnostic(TEXT("save_migrated")));
	TestEqual(TEXT("Migrated save uses current format"), Migrated.Envelope.Header.FormatVersion, FCampaignSaveCodec::CurrentFormatVersion);
	TestEqual(TEXT("New score field receives safe default"), Migrated.Envelope.State.CampaignScore, int64(0));
	TestTrue(TEXT("New difficulty field receives safe default"), Migrated.Envelope.State.Difficulty == ECampaignDifficulty::Standard);
	TestEqual(TEXT("New command sequence receives safe default"), Migrated.Envelope.State.CommandSequence, int64(0));
	TestEqual(TEXT("Legacy funds survive migration"), Migrated.Envelope.State.Funds, int64(750000));
	TestTrue(TEXT("Migrated envelope validates with current checksum"), FCampaignSaveCodec::Validate(Migrated.Envelope, Packages).bSucceeded);

	const FCampaignSaveWriteResult Rewritten = FCampaignSaveCodec::Serialize(Migrated.Envelope);
	TestTrue(TEXT("Migrated save can be rewritten"), Rewritten.bSucceeded);
	const FCampaignSaveReadResult ReadAgain = FCampaignSaveCodec::Deserialize(Rewritten.Json, Packages);
	TestTrue(TEXT("Rewritten save loads as current"), ReadAgain.bSucceeded);
	TestFalse(TEXT("Rewritten save does not migrate twice"), ReadAgain.bMigrated);

	FCampaignState Version18State = MakeState();
	Version18State.Bases[0].Facilities[0].Damage = 0;
	Version18State.Bases[0].Facilities[0].ReservedRepairDamage = 0;
	Version18State.Bases[0].Facilities[0].RemainingRepairSeconds = 0;
	const FCampaignSaveEnvelope Version19Envelope = FCampaignSaveCodec::CreateNew(
		Version18State, MakeContentPackages(), TEXT("0.18.0-test"), TestWallClock,
		FGuid(0x18181818, 0x28282828, 0x38383838, 0x48484848));
	const FCampaignSaveWriteResult Version19Write = FCampaignSaveCodec::Serialize(Version19Envelope);
	TestTrue(TEXT("v18 migration fixture first serializes as current"), Version19Write.bSucceeded);
	FCampaignSaveEnvelope Version18Envelope = Version19Write.Envelope;
	Version18Envelope.Header.FormatVersion = 18;
	Version18Envelope.Header.SaveChecksum = FCampaignSaveCodec::ComputeEnvelopeChecksum(Version18Envelope);
	FString Version18Json = Version19Write.Json;
	Version18Json = Version18Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":18"));
	Version18Json = Version18Json.Replace(
		*Version19Write.Envelope.Header.SaveChecksum,
		*Version18Envelope.Header.SaveChecksum);
	const FString Version19FacilityFields =
		TEXT(",\"damage\":0,\"reservedRepairDamage\":0,\"remainingRepairSeconds\":\"0\"");
	const FString WithoutDurability = Version18Json.Replace(*Version19FacilityFields, TEXT(""));
	TestNotEqual(TEXT("v18 migration fixture omits v19 durability fields"), WithoutDurability, Version18Json);
	const FCampaignSaveReadResult MigratedV18 =
		FCampaignSaveCodec::Deserialize(WithoutDurability, MakeContentPackages());
	TestTrue(TEXT("Verified v18 save migrates through v19 to current"),
		MigratedV18.bSucceeded && MigratedV18.bMigrated
		&& MigratedV18.Envelope.Header.FormatVersion == FCampaignSaveCodec::CurrentFormatVersion);
	TestTrue(TEXT("v18 facilities migrate at full integrity with no active repair"),
		MigratedV18.Envelope.State.Bases.Num() == 1
		&& MigratedV18.Envelope.State.Bases[0].Facilities.Num() == 1
		&& MigratedV18.Envelope.State.Bases[0].Facilities[0].Damage == 0
		&& MigratedV18.Envelope.State.Bases[0].Facilities[0].ReservedRepairDamage == 0
		&& MigratedV18.Envelope.State.Bases[0].Facilities[0].RemainingRepairSeconds == 0);

	FCampaignSaveEnvelope LegacyV19Envelope = Version19Write.Envelope;
	LegacyV19Envelope.Header.FormatVersion = 19;
	LegacyV19Envelope.Header.SaveChecksum = FCampaignSaveCodec::ComputeEnvelopeChecksum(LegacyV19Envelope);
	FString LegacyV19Json = Version19Write.Json.Replace(
		*FString::Printf(TEXT("\"formatVersion\":%d"), FCampaignSaveCodec::CurrentFormatVersion),
		TEXT("\"formatVersion\":19"));
	LegacyV19Json = LegacyV19Json.Replace(
		*Version19Write.Envelope.Header.SaveChecksum,
		*LegacyV19Envelope.Header.SaveChecksum);
	const FString WithoutBaseAssaults = LegacyV19Json.Replace(TEXT(",\"baseAssaults\":[]"), TEXT(""));
	TestNotEqual(TEXT("v19 migration fixture omits v20 base-assault state"), WithoutBaseAssaults, LegacyV19Json);
	const FCampaignSaveReadResult MigratedV19 = FCampaignSaveCodec::Deserialize(WithoutBaseAssaults, MakeContentPackages());
	TestTrue(TEXT("Verified v19 save migrates to v20"), MigratedV19.bSucceeded && MigratedV19.bMigrated
		&& MigratedV19.Envelope.Header.FormatVersion == FCampaignSaveCodec::CurrentFormatVersion
		&& MigratedV19.Envelope.State.BaseAssaults.IsEmpty());

	return true;
}

#endif
