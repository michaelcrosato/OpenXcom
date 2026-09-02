// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Tactical/TacticalPresentationService.h"

#include "Misc/AutomationTest.h"
#include "Tactical/TacticalCombatService.h"

namespace TacticalPresentationTests
{
	struct FFixture
	{
		FResolvedRuleSet Rules;
		FCampaignState Campaign;
		FGuid BattleId = FGuid(1101, 1102, 1103, 1104);
		FGuid OperationId = FGuid(1201, 1202, 1203, 1204);
		FGuid CraftId = FGuid(1301, 1302, 1303, 1304);
		FGuid PersonnelId = FGuid(1401, 1402, 1403, 1404);
		FGuid PlayerUnitId = FGuid(1501, 1502, 1503, 1504);
		FGuid VisibleAdversaryId = FGuid(1601, 1602, 1603, 1604);
		FGuid HiddenAdversaryId = FGuid(1701, 1702, 1703, 1704);

		FFixture()
		{
			FItemRule Rifle;
			Rifle.Identity.RuleId = TEXT("item.presentation-rifle");
			Rifle.DisplayName = TEXT("Vector Rifle");
			Rifle.Category = TEXT("weapon");
			Rifle.Mass = 4;
			Rifle.Power = 18;
			Rifle.TacticalRange = 10;
			Rifle.TacticalActionPointCost = 3;
			Rifle.TacticalAmmunitionItemId = TEXT("item.presentation-magazine");
			Rifle.TacticalMagazineCapacity = 6;
			Rifle.TacticalAmmunitionPerAttack = 1;
			Rifle.TacticalReloadActionPointCost = 2;
			Rifle.TacticalBurstShotCount = 3;
			Rifle.TacticalBurstActionPointCost = 5;
			Rifle.TacticalBurstAccuracyModifier = -10;
			Rules.Items.Add(Rifle.Identity.RuleId, Rifle);

			FItemRule Magazine;
			Magazine.Identity.RuleId = Rifle.TacticalAmmunitionItemId;
			Magazine.DisplayName = TEXT("Vector Magazine");
			Magazine.Category = TEXT("ammunition");
			Magazine.Mass = 1;
			Rules.Items.Add(Magazine.Identity.RuleId, Magazine);

			FItemRule Device;
			Device.Identity.RuleId = TEXT("item.presentation-smoke");
			Device.DisplayName = TEXT("Veil Capsule");
			Device.Category = TEXT("device");
			Device.Mass = 1;
			Device.TacticalRange = 6;
			Device.TacticalActionPointCost = 2;
			Device.TacticalRadius = 2;
			Device.TacticalThrowArcHeight = 3;
			Device.TacticalSmoke = 60;
			Rules.Items.Add(Device.Identity.RuleId, Device);

			FItemRule Projector;
			Projector.Identity.RuleId = TEXT("item.presentation-projector");
			Projector.DisplayName = TEXT("Field Projector");
			Projector.Category = TEXT("sensor");
			Projector.Mass = 2;
			Projector.Power = 20;
			Projector.TacticalRange = 8;
			Projector.TacticalActionPointCost = 4;
			Rules.Items.Add(Projector.Identity.RuleId, Projector);

			FItemRule Shard;
			Shard.Identity.RuleId = TEXT("item.presentation-shard");
			Shard.DisplayName = TEXT("Resonance Shard");
			Shard.Category = TEXT("recovered-material");
			Shard.Mass = 2;
			Rules.Items.Add(Shard.Identity.RuleId, Shard);

			FTacticalTerrainRule Floor;
			Floor.Identity.RuleId = TEXT("terrain.presentation-floor");
			Floor.DisplayName = TEXT("Composite Deck");
			Floor.MoveCost = 1;
			Rules.TacticalTerrains.Add(Floor.Identity.RuleId, Floor);

			FTacticalTerrainRule Wall;
			Wall.Identity.RuleId = TEXT("terrain.presentation-wall");
			Wall.DisplayName = TEXT("Signal Bulkhead");
			Wall.MoveCost = 1;
			Wall.CoverPercent = 45;
			Wall.MaxIntegrity = 40;
			Wall.bBlocksMovement = true;
			Wall.bBlocksVision = true;
			Rules.TacticalTerrains.Add(Wall.Identity.RuleId, Wall);

			FTacticalTerrainRule Door = Wall;
			Door.Identity.RuleId = TEXT("terrain.presentation-door");
			Door.DisplayName = TEXT("Iris Door");
			Door.DoorActionPointCost = 1;
			Rules.TacticalTerrains.Add(Door.Identity.RuleId, Door);

			FTacticalUnitRule AdversaryRule;
			AdversaryRule.Identity.RuleId = TEXT("unit.presentation-adversary");
			AdversaryRule.DisplayName = TEXT("Lattice Warden");
			AdversaryRule.AttackRange = 8;
			AdversaryRule.AttackPower = 12;
			AdversaryRule.AttackActionPointCost = 3;
			Rules.TacticalUnits.Add(AdversaryRule.Identity.RuleId, AdversaryRule);

			FTacticalMissionRule Mission;
			Mission.Identity.RuleId = TEXT("tactical.presentation-test");
			Mission.DisplayName = TEXT("Silent Relay");
			Mission.FloorTerrainRuleId = Floor.Identity.RuleId;
			Mission.ObstacleTerrainRuleId = Wall.Identity.RuleId;
			Mission.DoorTerrainRuleId = Door.Identity.RuleId;
			Mission.AdversaryUnitRuleId = AdversaryRule.Identity.RuleId;
			Mission.ObjectiveId = TEXT("objective.presentation-control");
			Mission.ObjectiveType = ETacticalObjectiveType::Control;
			Mission.ObjectiveRequiredInteractions = 3;
			Mission.ObjectiveActionPointCost = 2;
			Mission.ExtractionActionPointCost = 1;
			Mission.MapWidth = 12;
			Mission.MapHeight = 18;
			Rules.TacticalMissions.Add(Mission.Identity.RuleId, Mission);

			FCraftRule CraftRule;
			CraftRule.Identity.RuleId = TEXT("craft.presentation-transport");
			CraftRule.DisplayName = TEXT("Relay Skiff");
			CraftRule.CargoCapacity = 20;
			Rules.Craft.Add(CraftRule.Identity.RuleId, CraftRule);

			FPersonnelDoctrineRule RelayDoctrine;
			RelayDoctrine.Identity.RuleId = TEXT("doctrine.clear-sight");
			RelayDoctrine.DisplayName = TEXT("Clear Sight");
			RelayDoctrine.Summary = TEXT("Improves field accuracy.");
			RelayDoctrine.MaxSelections = 3;
			RelayDoctrine.AccuracyBonus = 4;
			Rules.PersonnelDoctrines.Add(RelayDoctrine.Identity.RuleId, RelayDoctrine);

			FPersonnelState& Person = Campaign.Personnel.AddDefaulted_GetRef();
			Person.PersonnelId = PersonnelId;
			Person.DisplayName = TEXT("Ari Venn");
			Person.Status = EPersonnelStatus::Deployed;
			Person.EquippedItems.Add(Rifle.Identity.RuleId);
			Person.EquippedItems.Add(Projector.Identity.RuleId);

			FCraftState& Craft = Campaign.Craft.AddDefaulted_GetRef();
			Craft.CraftId = CraftId;
			Craft.CraftRuleId = CraftRule.Identity.RuleId;
			Craft.DisplayName = TEXT("Relay One");
			Craft.Cargo.Add({ Shard.Identity.RuleId, 1 });

			FTacticalOperationState& Operation = Campaign.TacticalOperations.AddDefaulted_GetRef();
			Operation.OperationId = OperationId;
			Operation.CraftId = CraftId;
			Operation.AgentIds.Add(PersonnelId);

			FTacticalBattleState& Battle = Campaign.TacticalBattles.AddDefaulted_GetRef();
			Battle.BattleId = BattleId;
			Battle.OperationId = OperationId;
			Battle.MissionRuleId = Mission.Identity.RuleId;
			Battle.Width = 12;
			Battle.Height = 5;
			Battle.Levels = 1;
			Battle.TurnLimit = 30;
			Battle.TurnNumber = 4;
			Battle.Phase = ETacticalBattlePhase::PlayerTurn;
			Battle.ActiveTeam = ETacticalTeam::Player;
			for (int32 Y = 0; Y < Battle.Height; ++Y)
			{
				for (int32 X = 0; X < Battle.Width; ++X)
				{
					FTacticalCellState& Cell = Battle.Cells.AddDefaulted_GetRef();
					Cell.X = X;
					Cell.Y = Y;
					Cell.Z = 0;
					Cell.TerrainRuleId = X == 6 ? Wall.Identity.RuleId : Floor.Identity.RuleId;
					Cell.CurrentIntegrity = X == 6 ? Wall.MaxIntegrity : 0;
				}
			}
			FTacticalCellState& DoorCell = Battle.Cells[Battle.GetCellIndex(1, 1, 0)];
			DoorCell.TerrainRuleId = Door.Identity.RuleId;
			DoorCell.CurrentIntegrity = Door.MaxIntegrity;
			Battle.Cells[Battle.GetCellIndex(1, 2, 0)].bExtraction = true;

			FTacticalUnitState& Player = Battle.Units.AddDefaulted_GetRef();
			Player.UnitId = PlayerUnitId;
			Player.PersonnelId = PersonnelId;
			Player.DisplayName = Person.DisplayName;
			Player.Team = ETacticalTeam::Player;
			Player.X = 1;
			Player.Y = 2;
			Player.MaxHealth = 50;
			Player.CurrentHealth = 50;
			Player.Accuracy = 75;
			Player.Resolve = 60;
			Player.Mobility = 60;
			Player.Strength = 55;
			Player.MaxActionPoints = 8;
			Player.RemainingActionPoints = 8;
			Player.WeaponStates.Add({ Rifle.Identity.RuleId, 4 });
			Player.CarriedItems.Add({ Magazine.Identity.RuleId, 2 });
			Player.CarriedItems.Add({ Device.Identity.RuleId, 1 });
			Player.CarriedItems.Add({ Projector.Identity.RuleId, 1 });
			Player.EjectedMagazines.Add({ Rifle.Identity.RuleId, Magazine.Identity.RuleId, 3 });

			auto AddAdversary = [&Battle, &AdversaryRule](const FGuid UnitId, const int32 X)
			{
				FTacticalUnitState& Unit = Battle.Units.AddDefaulted_GetRef();
				Unit.UnitId = UnitId;
				Unit.SourceRuleId = AdversaryRule.Identity.RuleId;
				Unit.DisplayName = AdversaryRule.DisplayName;
				Unit.Team = ETacticalTeam::Adversary;
				Unit.X = X;
				Unit.Y = 2;
				Unit.MaxHealth = 35;
				Unit.CurrentHealth = 35;
				Unit.Accuracy = 50;
				Unit.Resolve = 50;
				Unit.Mobility = 50;
				Unit.Strength = 50;
				Unit.MaxActionPoints = 7;
				Unit.RemainingActionPoints = 7;
			};
			AddAdversary(VisibleAdversaryId, 4);
			AddAdversary(HiddenAdversaryId, 9);

			FTacticalObjectiveState& Objective = Battle.Objectives.AddDefaulted_GetRef();
			Objective.ObjectiveId = Mission.ObjectiveId;
			Objective.Type = ETacticalObjectiveType::Control;
			Objective.X = 2;
			Objective.Y = 2;
			Objective.RequiredInteractions = 3;
			Objective.CompletedInteractions = 1;

			Battle.Cargo.Add({ Shard.Identity.RuleId, 1 });
			Campaign.CommandSequence = 42;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTacticalHudPresentationFogActionsTest,
	"UEGT.Core.Tactical.Presentation.HudFogActionsAndPreviews",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTacticalHudPresentationFogActionsTest::RunTest(const FString& Parameters)
{
	using namespace TacticalPresentationTests;

	FFixture Fixture;
	const FTacticalBattleState& Battle = Fixture.Campaign.TacticalBattles[0];
	FTacticalHudQuery Query;
	Query.SelectedUnitId = Fixture.PlayerUnitId;
	Query.bHasHoveredCell = true;
	Query.HoveredX = 4;
	Query.HoveredY = 2;
	Query.HoveredUnitId = Fixture.VisibleAdversaryId;
	const FTacticalHudSnapshot Snapshot = FTacticalPresentationService::BuildHudSnapshot(
		Battle, Fixture.Campaign, Fixture.Rules, Query);
	TestTrue(TEXT("HUD snapshot builds from valid tactical state"), Snapshot.bSucceeded);
	TestEqual(TEXT("HUD publishes the optimistic command sequence"), Snapshot.ExpectedCommandSequence, int64(42));
	TestEqual(TEXT("HUD exposes exactly one record per action type"), Snapshot.Actions.Num(), 13);
	TestEqual(TEXT("Only friendly and visible hostile units are exposed"), Snapshot.Units.Num(), 2);
	TestEqual(TEXT("Exactly one hostile is currently visible"), Snapshot.VisibleAdversaryUnitCount, 1);
	TestFalse(TEXT("Hidden hostile identity never reaches the HUD"), Snapshot.Units.ContainsByPredicate(
		[&Fixture](const FTacticalHudUnitView& Unit) { return Unit.UnitId == Fixture.HiddenAdversaryId; }));

	FCampaignState MemoryCampaign = Fixture.Campaign;
	FTacticalBattleState& MemoryBattle = MemoryCampaign.TacticalBattles[0];
	FTacticalUnitState* HiddenUnit = MemoryBattle.Units.FindByPredicate(
		[&Fixture](const FTacticalUnitState& Unit) { return Unit.UnitId == Fixture.HiddenAdversaryId; });
	TestNotNull(TEXT("Last-known fixture has a hidden adversary"), HiddenUnit);
	if (HiddenUnit != nullptr)
	{
		HiddenUnit->CurrentHealth = 11;
	}
	FTacticalUnitMemoryState& LastKnown = MemoryBattle.PlayerLastKnownAdversaries.AddDefaulted_GetRef();
	LastKnown.UnitId = Fixture.HiddenAdversaryId;
	LastKnown.SourceRuleId = FName(TEXT("unit.presentation-adversary"));
	LastKnown.DisplayName = TEXT("Lattice Warden");
	LastKnown.X = 8;
	LastKnown.Y = 2;
	LastKnown.Z = 0;
	LastKnown.MaxHealth = 35;
	LastKnown.CurrentHealth = 29;
	LastKnown.MaxMorale = 100;
	LastKnown.CurrentMorale = 73;
	LastKnown.Suppression = 18;
	LastKnown.LastSeenTurnNumber = 3;
	FTacticalHudQuery MemoryQuery = Query;
	MemoryQuery.HoveredUnitId = Fixture.HiddenAdversaryId;
	const FTacticalHudSnapshot MemorySnapshot = FTacticalPresentationService::BuildHudSnapshot(
		MemoryBattle, MemoryCampaign, Fixture.Rules, MemoryQuery);
	TestTrue(TEXT("HUD preserves a validated last-known contact without exposing live state"), MemorySnapshot.bSucceeded);
	TestEqual(TEXT("Last-known contact is added beside the current roster"), MemorySnapshot.Units.Num(), 3);
	TestEqual(TEXT("Last-known count excludes the currently visible contact"), MemorySnapshot.LastKnownAdversaryUnitCount, 1);
	const FTacticalHudUnitView* LastKnownView = MemorySnapshot.Units.FindByPredicate(
		[&Fixture](const FTacticalHudUnitView& Unit) { return Unit.UnitId == Fixture.HiddenAdversaryId; });
	TestTrue(TEXT("Last-known contact exposes its stale location and observation turn"), LastKnownView != nullptr
		&& LastKnownView->bLastKnown && !LastKnownView->bCurrentlyVisible
		&& LastKnownView->X == 8 && LastKnownView->Y == 2
		&& LastKnownView->CurrentHealth == 29 && LastKnownView->LastSeenTurnNumber == 3);
	TestTrue(TEXT("Last-known contact withholds live action and loadout details"), LastKnownView != nullptr
		&& !LastKnownView->bControllable && LastKnownView->RemainingActionPoints == 0
		&& LastKnownView->MaxActionPoints == 0 && LastKnownView->Weapons.IsEmpty()
		&& LastKnownView->CarriedItems.IsEmpty());
	TestFalse(TEXT("Last-known contact cannot produce a targeting preview"), MemorySnapshot.Hover.bHasUnitAttackPreview
		|| MemorySnapshot.Hover.bHasSignalPreview);
	TestEqual(TEXT("First equipped weapon becomes the deterministic default"), Snapshot.EffectiveWeaponItemId, FName(TEXT("item.presentation-rifle")));
	TestEqual(TEXT("First carried device becomes the deterministic default"), Snapshot.EffectiveDeviceItemId, FName(TEXT("item.presentation-smoke")));
	TestEqual(TEXT("First carried projector becomes the deterministic signal default"),
		Snapshot.EffectiveSignalProjectorItemId, FName(TEXT("item.presentation-projector")));
	TestTrue(TEXT("Visible hostile attack receives a computed preview"), Snapshot.Hover.bHasUnitAttackPreview
		&& Snapshot.Hover.UnitAttack.bSucceeded);
	TestTrue(TEXT("Visible hostile receives an exact signal-pressure preview"),
		Snapshot.Hover.bHasSignalPreview && Snapshot.Hover.Signal.bSucceeded
			&& Snapshot.Hover.Signal.MoraleDamage > 0 && Snapshot.Hover.Signal.SuppressionGain > 0);
	const FTacticalHudActionAvailability* UnitAttack = Snapshot.FindAction(ETacticalHudActionType::AttackUnit);
	TestTrue(TEXT("Visible in-range hostile can be attacked"), UnitAttack != nullptr && UnitAttack->bAvailable
		&& UnitAttack->ActionPointCost == 3);
	const FTacticalHudActionAvailability* Signal = Snapshot.FindAction(ETacticalHudActionType::ProjectSignal);
	TestTrue(TEXT("Visible in-range hostile can receive signal pressure"), Signal != nullptr && Signal->bAvailable
		&& Signal->ActionPointCost == 4 && Signal->ItemId == FName(TEXT("item.presentation-projector"))
		&& Signal->TargetUnitId == Fixture.VisibleAdversaryId);
	const FTacticalHudActionAvailability* Reload = Snapshot.FindAction(ETacticalHudActionType::Reload);
	TestTrue(TEXT("Partially loaded weapon with a reserve magazine can reload"), Reload != nullptr && Reload->bAvailable
		&& Reload->ActionPointCost == 2);
	const FTacticalHudActionAvailability* Eject = Snapshot.FindAction(ETacticalHudActionType::EjectMagazine);
	TestTrue(TEXT("Loaded magazine can be explicitly ejected"), Eject != nullptr && Eject->bAvailable
		&& Eject->ActionPointCost == 2);
	const FTacticalHudUnitView* PlayerView = Snapshot.Units.FindByPredicate(
		[&Fixture](const FTacticalHudUnitView& Unit) { return Unit.UnitId == Fixture.PlayerUnitId; });
	TestTrue(TEXT("Weapon view exposes full, partial, total-round, and next-reload reserves"), PlayerView != nullptr
		&& PlayerView->Weapons.Num() == 1
		&& PlayerView->Weapons[0].ReserveMagazines == 3
		&& PlayerView->Weapons[0].FullReserveMagazines == 2
		&& PlayerView->Weapons[0].PartialReserveMagazines == 1
		&& PlayerView->Weapons[0].ReserveAmmunition == 15
		&& PlayerView->Weapons[0].NextReloadAmmunition == 6);
	FCampaignState ExtremeReserveCampaign = Fixture.Campaign;
	ExtremeReserveCampaign.TacticalBattles[0].Units[0].CarriedItems[0].Quantity = MAX_int32;
	const FTacticalHudSnapshot ExtremeReserve = FTacticalPresentationService::BuildHudSnapshot(
		ExtremeReserveCampaign.TacticalBattles[0], ExtremeReserveCampaign, Fixture.Rules, Query);
	const FTacticalHudUnitView* ExtremeReservePlayer = ExtremeReserve.Units.FindByPredicate(
		[&Fixture](const FTacticalHudUnitView& Unit) { return Unit.UnitId == Fixture.PlayerUnitId; });
	TestTrue(TEXT("HUD reserve totals saturate instead of wrapping for extreme carried ammunition"),
		ExtremeReserve.bSucceeded && ExtremeReservePlayer != nullptr
		&& ExtremeReservePlayer->Weapons.Num() == 1
		&& ExtremeReservePlayer->Weapons[0].ReserveAmmunition == MAX_int32
		&& ExtremeReservePlayer->Weapons[0].FullReserveMagazines == MAX_int32
		&& ExtremeReservePlayer->Weapons[0].ReserveMagazines == MAX_int32);
	const FTacticalHudActionAvailability* Objective = Snapshot.FindAction(ETacticalHudActionType::InteractObjective);
	TestTrue(TEXT("Adjacent active objective is selected without leaking pointer state"), Objective != nullptr && Objective->bAvailable
		&& Objective->ObjectiveId == FName(TEXT("objective.presentation-control")));
	const FTacticalHudActionAvailability* Extract = Snapshot.FindAction(ETacticalHudActionType::Extract);
	TestTrue(TEXT("Unit standing in the extraction zone can extract"), Extract != nullptr && Extract->bAvailable);
	const FTacticalHudActionAvailability* EndTurn = Snapshot.FindAction(ETacticalHudActionType::EndTurn);
	TestTrue(TEXT("Player can end an active player turn"), EndTurn != nullptr && EndTurn->bAvailable);
	const FTacticalHudActionAvailability* Confirm = Snapshot.FindAction(ETacticalHudActionType::ConfirmDeployment);
	TestTrue(TEXT("Confirmed deployment is no longer offered"), Confirm != nullptr && !Confirm->bAvailable
		&& Confirm->UnavailableReasonCode == FName(TEXT("tactical_deployment_already_confirmed")));
	TestEqual(TEXT("Transport cargo utilization includes the strategic manifest"), Snapshot.CargoMass, int64(2));
	TestEqual(TEXT("Transport cargo capacity is available to recovery UI"), Snapshot.CargoCapacity, 20);
	TestEqual(TEXT("Control progress is exposed"), Snapshot.Objectives[0].PlayerInteractions, 1);

	FCampaignState InvalidPositionCampaign = Fixture.Campaign;
	FTacticalUnitState* InvalidPositionPlayer = InvalidPositionCampaign.TacticalBattles[0].Units.FindByPredicate(
		[&Fixture](const FTacticalUnitState& Unit) { return Unit.UnitId == Fixture.PlayerUnitId; });
	TestNotNull(TEXT("HUD boundary fixture has a selected player"), InvalidPositionPlayer);
	if (InvalidPositionPlayer != nullptr)
	{
		InvalidPositionPlayer->X = MIN_int32;
		const FTacticalHudSnapshot InvalidPositionSnapshot = FTacticalPresentationService::BuildHudSnapshot(
			InvalidPositionCampaign.TacticalBattles[0], InvalidPositionCampaign, Fixture.Rules, Query);
		const FTacticalHudActionAvailability* InvalidPositionMove = InvalidPositionSnapshot.FindAction(
			ETacticalHudActionType::Move);
		TestTrue(TEXT("HUD remains renderable when a selected player is outside the battlefield"),
			InvalidPositionSnapshot.bSucceeded);
		TestTrue(TEXT("HUD disables actions for an out-of-grid selected player"),
			InvalidPositionMove != nullptr && !InvalidPositionMove->bAvailable
			&& InvalidPositionMove->UnavailableReasonCode == FName(TEXT("invalid_tactical_unit")));
	}

	FCampaignState MentorshipCampaign = Fixture.Campaign;
	MentorshipCampaign.Personnel[0].Missions = 20;
	MentorshipCampaign.Personnel[0].Rank = 4;
	MentorshipCampaign.Personnel[0].DoctrineSelections = {
		TEXT("doctrine.clear-sight"), TEXT("doctrine.clear-sight"), TEXT("doctrine.clear-sight") };
	const FGuid MentorshipRecipientId(2101, 2102, 2103, 2104);
	FPersonnelState& MentorshipRecipient = MentorshipCampaign.Personnel.AddDefaulted_GetRef();
	MentorshipRecipient.PersonnelId = MentorshipRecipientId;
	MentorshipRecipient.DisplayName = TEXT("Pavel Orin");
	MentorshipRecipient.Status = EPersonnelStatus::Deployed;
	MentorshipRecipient.Missions = 8;
	MentorshipCampaign.TacticalOperations[0].AgentIds.Add(MentorshipRecipientId);
	FPersonnelSquadBondState& SquadBond = MentorshipCampaign.PersonnelSquadBonds.AddDefaulted_GetRef();
	SquadBond.FirstPersonnelId = Fixture.PersonnelId;
	SquadBond.SecondPersonnelId = MentorshipRecipientId;
	SquadBond.SharedVictories = 8;
	const FTacticalHudSnapshot MentorshipSnapshot = FTacticalPresentationService::BuildHudSnapshot(
		MentorshipCampaign.TacticalBattles[0], MentorshipCampaign, Fixture.Rules, Query);
	TestTrue(TEXT("Tactical HUD snapshot exposes the same immutable active mentorship projection"),
		MentorshipSnapshot.bSucceeded
		&& MentorshipSnapshot.Mentorship.bActive
		&& MentorshipSnapshot.Mentorship.MentorId == Fixture.PersonnelId
		&& MentorshipSnapshot.Mentorship.MentorServiceHistory.Band == EPersonnelServiceBand::LegacyAnchor
		&& MentorshipSnapshot.Mentorship.MoraleBonus == 10
		&& MentorshipSnapshot.Mentorship.RecipientIds == TArray<FGuid>{ MentorshipRecipientId });
	TestTrue(TEXT("Tactical HUD snapshot exposes the same immutable active Legacy Relay projection"),
		MentorshipSnapshot.LegacyRelay.bActive
		&& MentorshipSnapshot.LegacyRelay.SpecialistId == Fixture.PersonnelId
		&& MentorshipSnapshot.LegacyRelay.SpecialistServiceHistory.Band == EPersonnelServiceBand::LegacyAnchor
		&& MentorshipSnapshot.LegacyRelay.DoctrineId == FName(TEXT("doctrine.clear-sight"))
		&& MentorshipSnapshot.LegacyRelay.AccuracyBonus == 2
		&& MentorshipSnapshot.LegacyRelay.ResolveBonus == 0
		&& MentorshipSnapshot.LegacyRelay.MobilityBonus == 0
		&& MentorshipSnapshot.LegacyRelay.StrengthBonus == 0
		&& MentorshipSnapshot.LegacyRelay.RecipientIds == TArray<FGuid>{ MentorshipRecipientId });
	TestTrue(TEXT("Tactical HUD snapshot exposes deterministic active Field Cadence pairs"),
		MentorshipSnapshot.SquadBonds.bActive
		&& MentorshipSnapshot.SquadBonds.PolicyId == FName(TEXT("personnel.squad-bond-field-cadence"))
		&& MentorshipSnapshot.SquadBonds.ActivePairs.Num() == 1
		&& MentorshipSnapshot.SquadBonds.ActivePairs[0].FirstPersonnelId == Fixture.PersonnelId
		&& MentorshipSnapshot.SquadBonds.ActivePairs[0].SecondPersonnelId == MentorshipRecipientId
		&& MentorshipSnapshot.SquadBonds.ActivePairs[0].Tier == EPersonnelSquadBondTier::Interlocked
		&& MentorshipSnapshot.SquadBonds.ActivePairs[0].ActionPointBonus == 1
		&& MentorshipSnapshot.SquadBonds.ActivePairs[0].MoraleBonus == 5);

	FCampaignState PartialOnlyCampaign = Fixture.Campaign;
	FTacticalUnitState& PartialOnlyPlayer = PartialOnlyCampaign.TacticalBattles[0].Units[0];
	PartialOnlyPlayer.CarriedItems.RemoveAll(
		[](const FInventoryStack& Stack) { return Stack.ItemId == FName(TEXT("item.presentation-magazine")); });
	const FTacticalHudSnapshot PartialOnly = FTacticalPresentationService::BuildHudSnapshot(
		PartialOnlyCampaign.TacticalBattles[0], PartialOnlyCampaign, Fixture.Rules, Query);
	const FTacticalHudActionAvailability* PartialReload = PartialOnly.FindAction(ETacticalHudActionType::Reload);
	TestTrue(TEXT("Reload rejects a reserve magazine that cannot improve loaded rounds"), PartialReload != nullptr
		&& !PartialReload->bAvailable
		&& PartialReload->UnavailableReasonCode == FName(TEXT("tactical_reload_no_improvement")));

	FResolvedRuleSet RecoveryRules = Fixture.Rules;
	FTacticalMissionRule* RecoveryMission = RecoveryRules.TacticalMissions.Find(TEXT("tactical.presentation-test"));
	TestNotNull(TEXT("Recovery capacity fixture has its mission rule"), RecoveryMission);
	if (RecoveryMission != nullptr)
	{
		RecoveryMission->ObjectiveRewardItemId = FName(TEXT("item.presentation-shard"));
		RecoveryMission->ObjectiveRewardQuantity = 1;
	}
	FCampaignState FullSalvageTableCampaign = Fixture.Campaign;
	FTacticalBattleState& FullSalvageTableBattle = FullSalvageTableCampaign.TacticalBattles[0];
	FullSalvageTableBattle.Objectives[0].Type = ETacticalObjectiveType::Recover;
	FullSalvageTableBattle.Objectives[0].RequiredInteractions = 2;
	FullSalvageTableBattle.Objectives[0].CompletedInteractions = 1;
	FCraftState& FullSalvageTableCraft = FullSalvageTableCampaign.Craft[0];
	FullSalvageTableCraft.PendingSalvage.Reset();
	for (int32 StackIndex = 0; StackIndex < 64; ++StackIndex)
	{
		FullSalvageTableCraft.PendingSalvage.Add({
			FName(FString::Printf(TEXT("item.pending-salvage-%d"), StackIndex)), 1 });
	}
	const FTacticalHudSnapshot FullSalvageTable = FTacticalPresentationService::BuildHudSnapshot(
		FullSalvageTableBattle, FullSalvageTableCampaign, RecoveryRules, Query);
	const FTacticalHudActionAvailability* FullSalvageRecovery = FullSalvageTable.FindAction(
		ETacticalHudActionType::InteractObjective);
	TestTrue(TEXT("Recovery preview rejects a reward when pending salvage has no free stack slot"),
		FullSalvageRecovery != nullptr && !FullSalvageRecovery->bAvailable
		&& FullSalvageRecovery->UnavailableReasonCode == FName(TEXT("tactical_recovery_capacity_exceeded")));

	FCampaignState BaseDefenseCampaign = Fixture.Campaign;
	const FGuid BaseId(1801, 1802, 1803, 1804);
	const FGuid AssaultId(1901, 1902, 1903, 1904);
	FStrategicBaseState& Base = BaseDefenseCampaign.Bases.AddDefaulted_GetRef();
	Base.BaseId = BaseId;
	Base.Name = TEXT("Cascadia Aegis");
	FTacticalOperationState& BaseOperation = BaseDefenseCampaign.TacticalOperations[0];
	BaseOperation.Type = ETacticalOperationType::BaseDefense;
	BaseOperation.BaseId = BaseId;
	BaseOperation.AssaultId = AssaultId;
	BaseOperation.CraftId.Invalidate();
	FTacticalBattleState& BaseBattle = BaseDefenseCampaign.TacticalBattles[0];
	BaseBattle.bRequiresExtraction = false;
	BaseBattle.Cargo.Reset();
	for (FTacticalCellState& Cell : BaseBattle.Cells)
	{
		Cell.bExtraction = false;
	}
	const FTacticalHudSnapshot BaseDefense = FTacticalPresentationService::BuildHudSnapshot(
		BaseBattle, BaseDefenseCampaign, Fixture.Rules, Query);
	const FTacticalHudActionAvailability* BaseExtraction = BaseDefense.FindAction(ETacticalHudActionType::Extract);
	TestTrue(TEXT("Ground-defense HUD exposes its base, assault, and no-extraction mission context"),
		BaseDefense.bSucceeded
		&& BaseDefense.OperationType == ETacticalOperationType::BaseDefense
		&& BaseDefense.BaseId == BaseId
		&& BaseDefense.AssaultId == AssaultId
		&& BaseDefense.BaseDisplayName == TEXT("Cascadia Aegis")
		&& !BaseDefense.bRequiresExtraction
		&& BaseDefense.Cargo.IsEmpty()
		&& BaseExtraction != nullptr && !BaseExtraction->bAvailable
		&& BaseExtraction->UnavailableReasonCode == FName(TEXT("tactical_extraction_not_required")));

	FTacticalHudQuery HiddenQuery = Query;
	HiddenQuery.HoveredX = 9;
	HiddenQuery.HoveredUnitId = Fixture.HiddenAdversaryId;
	const FTacticalHudSnapshot Hidden = FTacticalPresentationService::BuildHudSnapshot(
		Battle, Fixture.Campaign, Fixture.Rules, HiddenQuery);
	TestTrue(TEXT("HUD remains valid when pointer is over fog"), Hidden.bSucceeded);
	TestFalse(TEXT("Fog suppresses hidden-cell path computation"), Hidden.Hover.bHasPathPreview);
	TestFalse(TEXT("Fog suppresses hidden-hostile attack computation"), Hidden.Hover.bHasUnitAttackPreview);
	TestFalse(TEXT("Fog suppresses hidden-hostile signal computation"), Hidden.Hover.bHasSignalPreview);
	const FTacticalHudActionAvailability* HiddenAttack = Hidden.FindAction(ETacticalHudActionType::AttackUnit);
	TestTrue(TEXT("Hidden hostile is indistinguishable from no valid target"), HiddenAttack != nullptr && !HiddenAttack->bAvailable
		&& HiddenAttack->UnavailableReasonCode == FName(TEXT("visible_hostile_target_required"))
		&& !HiddenAttack->TargetUnitId.IsValid());
	const FTacticalHudActionAvailability* HiddenSignal = Hidden.FindAction(ETacticalHudActionType::ProjectSignal);
	TestTrue(TEXT("Hidden hostile is not exposed through signal targeting"), HiddenSignal != nullptr
		&& !HiddenSignal->bAvailable
		&& HiddenSignal->UnavailableReasonCode == FName(TEXT("visible_hostile_target_required"))
		&& !HiddenSignal->TargetUnitId.IsValid());
	const FTacticalHudActionAvailability* HiddenMove = Hidden.FindAction(ETacticalHudActionType::Move);
	TestTrue(TEXT("Fog does not expose pathfinding through unseen terrain"), HiddenMove != nullptr && !HiddenMove->bAvailable
		&& HiddenMove->UnavailableReasonCode == FName(TEXT("tactical_target_not_visible")));

	FTacticalHudQuery DoorQuery = Query;
	DoorQuery.HoveredX = 1;
	DoorQuery.HoveredY = 1;
	DoorQuery.HoveredUnitId.Invalidate();
	const FTacticalHudSnapshot Door = FTacticalPresentationService::BuildHudSnapshot(
		Battle, Fixture.Campaign, Fixture.Rules, DoorQuery);
	const FTacticalHudActionAvailability* DoorAction = Door.FindAction(ETacticalHudActionType::OperateDoor);
	TestTrue(TEXT("Adjacent visible door exposes its exact toggle command"), DoorAction != nullptr && DoorAction->bAvailable
		&& DoorAction->bRequestedDoorOpen && DoorAction->ActionPointCost == 1);

	FTacticalHudQuery MoveQuery = Query;
	MoveQuery.HoveredX = 2;
	MoveQuery.HoveredY = 3;
	MoveQuery.HoveredUnitId.Invalidate();
	const FTacticalHudSnapshot Movement = FTacticalPresentationService::BuildHudSnapshot(
		Battle, Fixture.Campaign, Fixture.Rules, MoveQuery);
	const FTacticalHudActionAvailability* Move = Movement.FindAction(ETacticalHudActionType::Move);
	TestTrue(TEXT("Visible open destination exposes an affordable path"), Movement.Hover.bHasPathPreview
		&& Movement.Hover.Path.bSucceeded && Move != nullptr && Move->bAvailable && Move->ActionPointCost > 0);

	FCampaignState DeploymentCampaign = Fixture.Campaign;
	DeploymentCampaign.TacticalBattles[0].Phase = ETacticalBattlePhase::Deployment;
	const FTacticalHudSnapshot Deployment = FTacticalPresentationService::BuildHudSnapshot(
		DeploymentCampaign.TacticalBattles[0], DeploymentCampaign, Fixture.Rules, Query);
	TestTrue(TEXT("Deployment HUD offers confirmation"), Deployment.FindAction(ETacticalHudActionType::ConfirmDeployment)->bAvailable);
	TestFalse(TEXT("Deployment HUD does not offer movement"), Deployment.FindAction(ETacticalHudActionType::Move)->bAvailable);

	FCampaignState AdversaryCampaign = Fixture.Campaign;
	AdversaryCampaign.TacticalBattles[0].Phase = ETacticalBattlePhase::AdversaryTurn;
	AdversaryCampaign.TacticalBattles[0].ActiveTeam = ETacticalTeam::Adversary;
	const FTacticalHudSnapshot Adversary = FTacticalPresentationService::BuildHudSnapshot(
		AdversaryCampaign.TacticalBattles[0], AdversaryCampaign, Fixture.Rules, Query);
	TestFalse(TEXT("Adversary turn disables player movement"), Adversary.FindAction(ETacticalHudActionType::Move)->bAvailable);
	TestFalse(TEXT("Adversary turn disables player end-turn control"), Adversary.FindAction(ETacticalHudActionType::EndTurn)->bAvailable);

	FTacticalHudQuery InvalidLevel = Query;
	InvalidLevel.ViewedLevel = 1;
	const FTacticalHudSnapshot Invalid = FTacticalPresentationService::BuildHudSnapshot(
		Battle, Fixture.Campaign, Fixture.Rules, InvalidLevel);
	TestFalse(TEXT("Invalid viewed level rejects the snapshot"), Invalid.bSucceeded);
	TestTrue(TEXT("Invalid viewed level has a stable diagnostic"), Invalid.HasDiagnostic(TEXT("invalid_tactical_level")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTacticalHistoricalFogDiscoveryTest,
	"UEGT.Core.Tactical.Presentation.HistoricalFogDiscovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTacticalHistoricalFogDiscoveryTest::RunTest(const FString& Parameters)
{
	using namespace TacticalPresentationTests;

	FFixture Fixture;
	FTacticalBattleState& Battle = Fixture.Campaign.TacticalBattles[0];
	FTacticalUnitState* Player = Battle.Units.FindByPredicate(
		[&Fixture](const FTacticalUnitState& Unit) { return Unit.UnitId == Fixture.PlayerUnitId; });
	TestNotNull(TEXT("Historical-fog fixture has a player observer"), Player);
	if (Player == nullptr)
	{
		return false;
	}

	const int64 DrawCountBeforeDiscovery = Battle.TacticalRandom.DrawCount;
	const FTacticalVisibilityResult Initial = FTacticalNavigationService::RefreshPlayerDiscovery(Battle, Fixture.Rules);
	TestTrue(TEXT("Initial player sight becomes durable discovery"),
		Initial.bSucceeded && !Battle.PlayerDiscoveredCellIndices.IsEmpty());
	const int32 InitialDiscoveryCount = Battle.PlayerDiscoveredCellIndices.Num();
	Player->X = 8;
	Player->Y = 2;
	const FTacticalVisibilityResult Excursion = FTacticalNavigationService::RefreshPlayerDiscovery(Battle, Fixture.Rules);
	TestTrue(TEXT("A remote sight footprint expands durable discovery"),
		Excursion.bSucceeded && Battle.PlayerDiscoveredCellIndices.Num() > InitialDiscoveryCount);
	Player->X = 1;
	Player->Y = 2;
	const FTacticalVisibilityResult Returned = FTacticalNavigationService::RefreshPlayerDiscovery(Battle, Fixture.Rules);
	TestTrue(TEXT("Returning the observer preserves the remote footprint"),
		Returned.bSucceeded && Battle.PlayerDiscoveredCellIndices.Num() > Returned.VisibleCellIndices.Num());
	const FTacticalUnitMemoryState* HiddenMemory = Battle.PlayerLastKnownAdversaries.FindByPredicate(
		[&Fixture](const FTacticalUnitMemoryState& Memory) { return Memory.UnitId == Fixture.HiddenAdversaryId; });
	TestTrue(TEXT("A hostile observed during the excursion leaves a last-known contact"), HiddenMemory != nullptr
		&& HiddenMemory->X == 9 && HiddenMemory->Y == 2 && HiddenMemory->LastSeenTurnNumber == Battle.TurnNumber);
	TestEqual(TEXT("Historical discovery consumes no tactical random draws"),
		Battle.TacticalRandom.DrawCount,
		DrawCountBeforeDiscovery);

	FTacticalHudQuery Query;
	Query.SelectedUnitId = Fixture.PlayerUnitId;
	const FTacticalHudSnapshot Snapshot = FTacticalPresentationService::BuildHudSnapshot(
		Battle, Fixture.Campaign, Fixture.Rules, Query);
	TestTrue(TEXT("Fog-safe HUD projects historical discovery"), Snapshot.bSucceeded);
	TestEqual(TEXT("Snapshot reports authoritative durable discovery"),
		Snapshot.KnownCellCount,
		Battle.PlayerDiscoveredCellIndices.Num());
	TestTrue(TEXT("Historical cells supplement, rather than replace, current sight"),
		Snapshot.KnownCells.Num() > Snapshot.VisibleCells.Num()
		&& Snapshot.KnownCellCount > Snapshot.VisibleCellCount);
	TestFalse(TEXT("Every exact visible cell remains marked as current"),
		Snapshot.VisibleCells.ContainsByPredicate(
			[](const FTacticalHudCellView& Cell) { return !Cell.bCurrentlyVisible; }));
	const FTacticalHudCellView* HistoricalCell = Snapshot.KnownCells.FindByPredicate(
		[](const FTacticalHudCellView& Cell) { return !Cell.bCurrentlyVisible; });
	TestNotNull(TEXT("Snapshot contains a coordinate-only historical cell"), HistoricalCell);
	if (HistoricalCell != nullptr)
	{
		TestTrue(TEXT("Historical cell withholds all dynamic terrain state"),
			HistoricalCell->TerrainRuleId.IsNone()
			&& HistoricalCell->TerrainDisplayName.IsEmpty()
			&& HistoricalCell->CurrentIntegrity == 0
			&& HistoricalCell->MaxIntegrity == 0
			&& HistoricalCell->Smoke == 0
			&& HistoricalCell->Fire == 0
			&& !HistoricalCell->bBlocksMovement
			&& !HistoricalCell->bBlocksVision
			&& !HistoricalCell->bIsDoor);
	}
	const FTacticalHudUnitView* HiddenView = Snapshot.Units.FindByPredicate(
		[&Fixture](const FTacticalHudUnitView& Unit) { return Unit.UnitId == Fixture.HiddenAdversaryId; });
	TestTrue(TEXT("Previously observed hostile remains visible only as last-known memory"), HiddenView != nullptr
		&& HiddenView->bLastKnown && !HiddenView->bCurrentlyVisible && HiddenView->X == 9 && HiddenView->Y == 2);

	FTacticalBattleState InvalidDiscovery = Battle;
	const int32 DuplicateCellIndex = InvalidDiscovery.PlayerDiscoveredCellIndices.Last();
	InvalidDiscovery.PlayerDiscoveredCellIndices.Add(DuplicateCellIndex);
	const FTacticalVisibilityResult Rejected = FTacticalNavigationService::RefreshPlayerDiscovery(
		InvalidDiscovery, Fixture.Rules);
	TestFalse(TEXT("Malformed discovery cannot be normalized silently"), Rejected.bSucceeded);
	TestTrue(TEXT("Malformed discovery has a stable diagnostic"),
		Rejected.HasDiagnostic(TEXT("invalid_tactical_discovery")));

	FTacticalBattleState InvalidMemory = Battle;
	InvalidMemory.PlayerLastKnownAdversaries.Reset();
	InvalidMemory.PlayerLastKnownAdversaries.AddDefaulted_GetRef().UnitId = FGuid(1801, 1802, 1803, 1804);
	const FTacticalVisibilityResult RejectedMemory = FTacticalNavigationService::RefreshPlayerDiscovery(
		InvalidMemory, Fixture.Rules);
	TestFalse(TEXT("Unknown last-known contacts cannot be normalized silently"), RejectedMemory.bSucceeded);
	TestTrue(TEXT("Malformed last-known memory has a stable diagnostic"),
		RejectedMemory.HasDiagnostic(TEXT("invalid_tactical_memory")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTacticalNavigationGridStateValidationTest,
	"UEGT.Core.Tactical.Navigation.GridStateValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTacticalNavigationGridStateValidationTest::RunTest(const FString& Parameters)
{
	using namespace TacticalPresentationTests;

	FFixture Fixture;
	FTacticalBattleState NegativeSmoke = Fixture.Campaign.TacticalBattles[0];
	NegativeSmoke.Cells[0].Smoke = -1;
	const FTacticalVisibilityResult NegativeSmokeVisibility = FTacticalNavigationService::ComputePlayerVisibility(
		NegativeSmoke, Fixture.Rules);
	TestFalse(TEXT("Navigation rejects negative persisted smoke state"), NegativeSmokeVisibility.bSucceeded);
	TestTrue(TEXT("Negative smoke state has the shared grid diagnostic"),
		NegativeSmokeVisibility.HasDiagnostic(TEXT("invalid_tactical_grid")));
	TestEqual(TEXT("Direct smoke query preserves its documented lower bound on malformed state"),
		FTacticalNavigationService::ComputeSmokeObscuration(NegativeSmoke, 0, 0, 1, 0), 0);

	FTacticalBattleState ExtremeSmoke = Fixture.Campaign.TacticalBattles[0];
	ExtremeSmoke.Cells[0].Smoke = MAX_int32 / 3 + 1;
	TestEqual(TEXT("Direct smoke query clamps extreme persisted smoke before multiplication"),
		FTacticalNavigationService::ComputeSmokeObscuration(ExtremeSmoke, 0, 0, 1, 0), 37);

	FResolvedRuleSet ExtremeMovementRules = Fixture.Rules;
	FTacticalTerrainRule* ExtremeMovementFloor = ExtremeMovementRules.TacticalTerrains.Find(TEXT("terrain.presentation-floor"));
	FTacticalBattleState ExtremeMovement = Fixture.Campaign.TacticalBattles[0];
	FTacticalUnitState* ExtremeMovementUnit = ExtremeMovement.Units.FindByPredicate(
		[&Fixture](const FTacticalUnitState& Unit) { return Unit.UnitId == Fixture.PlayerUnitId; });
	if (ExtremeMovementFloor != nullptr && ExtremeMovementUnit != nullptr)
	{
		ExtremeMovementFloor->MoveCost = MAX_int32;
		ExtremeMovementUnit->Stance = ETacticalStance::Crouched;
		const FTacticalReachabilityResult ExtremeReachability = FTacticalNavigationService::ComputeReachableCells(
			ExtremeMovement, ExtremeMovementRules, Fixture.PlayerUnitId, 2);
		TestTrue(TEXT("Extreme movement costs do not wrap into negative reachable costs"),
			ExtremeReachability.bSucceeded
				&& !ExtremeReachability.Cells.ContainsByPredicate(
					[](const FTacticalReachableCell& Cell) { return Cell.TotalCost < 0; }));
	}

	FTacticalBattleState ExcessFire = Fixture.Campaign.TacticalBattles[0];
	ExcessFire.Cells[0].Fire = 101;
	const FTacticalVisibilityResult ExcessFireVisibility = FTacticalNavigationService::ComputePlayerVisibility(
		ExcessFire, Fixture.Rules);
	TestFalse(TEXT("Navigation rejects over-range persisted fire state"), ExcessFireVisibility.bSucceeded);
	TestTrue(TEXT("Over-range fire state has the shared grid diagnostic"),
		ExcessFireVisibility.HasDiagnostic(TEXT("invalid_tactical_grid")));

	FTacticalBattleState ExtremeDimensions;
	ExtremeDimensions.Width = MAX_int32;
	ExtremeDimensions.Height = MAX_int32;
	ExtremeDimensions.Levels = 4;
	const FTacticalVisibilityResult ExtremeVisibility = FTacticalNavigationService::ComputePlayerVisibility(
		ExtremeDimensions, Fixture.Rules);
	TestFalse(TEXT("Navigation rejects extreme dimensions before a cell-count product can wrap"), ExtremeVisibility.bSucceeded);
	TestTrue(TEXT("Extreme tactical dimensions have the shared grid diagnostic"),
		ExtremeVisibility.HasDiagnostic(TEXT("invalid_tactical_grid")));
	TestEqual(TEXT("Smoke queries reject extreme dimensions without indexing cells"),
		FTacticalNavigationService::ComputeSmokeObscuration(ExtremeDimensions, 0, 0, 0, 0), 0);
	TestEqual(TEXT("Blast transmission rejects extreme dimensions without indexing cells"),
		FTacticalCombatService::ComputeBlastTransmissionPercent(ExtremeDimensions, Fixture.Rules, 0, 0, 0, 0), 0);

	FTacticalBattleState InvalidAttackerBattle = Fixture.Campaign.TacticalBattles[0];
	FTacticalUnitState* InvalidAttacker = InvalidAttackerBattle.Units.FindByPredicate(
		[&Fixture](const FTacticalUnitState& Unit) { return Unit.UnitId == Fixture.PlayerUnitId; });
	if (InvalidAttacker != nullptr)
	{
		InvalidAttacker->X = MIN_int32;
		const FTacticalAttackPreview InvalidAttackerPreview = FTacticalCombatService::PreviewUnitAttack(
			InvalidAttackerBattle,
			Fixture.Campaign,
			Fixture.Rules,
			Fixture.PlayerUnitId,
			Fixture.VisibleAdversaryId,
			TEXT("item.presentation-rifle"));
		TestFalse(TEXT("Combat previews reject an attacker outside the battlefield before distance arithmetic"),
			InvalidAttackerPreview.bSucceeded);
		TestTrue(TEXT("Out-of-grid attackers have a stable tactical-unit diagnostic"),
			InvalidAttackerPreview.HasDiagnostic(TEXT("invalid_tactical_unit")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTacticalCombatNumericBoundaryTest,
	"UEGT.Core.Tactical.Combat.NumericBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTacticalCombatNumericBoundaryTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Unit damage saturates large attack inputs instead of wrapping"),
		FTacticalCombatService::ComputeUnitDamage(MAX_int32, MAX_int32, 0, 0, 120), MAX_int32);
	TestEqual(TEXT("Terrain damage saturates large attack inputs instead of wrapping"),
		FTacticalCombatService::ComputeTerrainDamage(MAX_int32, MAX_int32, 120), MAX_int32);
	TestEqual(TEXT("Blast falloff saturates large products at zero effect"),
		FTacticalCombatService::ComputeBlastEffectPercent(MAX_int32, MAX_int32), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTacticalDebriefPresentationTest,
	"UEGT.Core.Tactical.Presentation.DebriefCargoCasualtiesAndProgression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTacticalDebriefPresentationTest::RunTest(const FString& Parameters)
{
	FResolvedRuleSet Rules;
	FItemRule Shard;
	Shard.Identity.RuleId = TEXT("item.debrief-shard");
	Shard.DisplayName = TEXT("Prismatic Shard");
	Shard.Category = TEXT("recovered-material");
	Shard.Mass = 2;
	Shard.SellValue = 900;
	Rules.Items.Add(Shard.Identity.RuleId, Shard);
	FTacticalMissionRule Mission;
	Mission.Identity.RuleId = TEXT("tactical.debrief-test");
	Mission.DisplayName = TEXT("Prism Fall");
	Rules.TacticalMissions.Add(Mission.Identity.RuleId, Mission);
	FPersonnelCommendationRule FirstResponse;
	FirstResponse.Identity.RuleId = TEXT("commendation.first-response-test");
	FirstResponse.DisplayName = TEXT("First Response Test");
	Rules.PersonnelCommendations.Add(FirstResponse.Identity.RuleId, FirstResponse);
	FPersonnelCommendationRule Breaker;
	Breaker.Identity.RuleId = TEXT("commendation.breaker-test");
	Breaker.DisplayName = TEXT("Breaker Test");
	Rules.PersonnelCommendations.Add(Breaker.Identity.RuleId, Breaker);

	const FGuid OperationId(2101, 2102, 2103, 2104);
	const FGuid BattleId(2201, 2202, 2203, 2204);
	const FGuid SiteId(2301, 2302, 2303, 2304);
	const FGuid CraftId(2401, 2402, 2403, 2404);
	const FGuid SurvivorId(2501, 2502, 2503, 2504);
	const FGuid CasualtyId(2601, 2602, 2603, 2604);
	FCampaignState Before;
	Before.CampaignScore = 700;
	FTacticalOperationState& Operation = Before.TacticalOperations.AddDefaulted_GetRef();
	Operation.OperationId = OperationId;
	Operation.SiteId = SiteId;
	Operation.CraftId = CraftId;
	Operation.AgentIds = { SurvivorId, CasualtyId };
	FCraftState& Craft = Before.Craft.AddDefaulted_GetRef();
	Craft.CraftId = CraftId;
	Craft.DisplayName = TEXT("Far Lantern");
	Craft.PendingSalvage.Add({ Shard.Identity.RuleId, 2 });
	FTacticalBattleState& Battle = Before.TacticalBattles.AddDefaulted_GetRef();
	Battle.BattleId = BattleId;
	Battle.OperationId = OperationId;
	Battle.SiteId = SiteId;
	Battle.MissionRuleId = Mission.Identity.RuleId;
	Battle.Phase = ETacticalBattlePhase::Resolved;
	Battle.Cargo.Add({ Shard.Identity.RuleId, 5 });
	auto AddPerson = [&Before](const FGuid Id, const FString& Name, const int32 Health, const int32 Rank,
		const int32 Experience, const int32 Missions)
	{
		FPersonnelState& Person = Before.Personnel.AddDefaulted_GetRef();
		Person.PersonnelId = Id;
		Person.DisplayName = Name;
		Person.Status = EPersonnelStatus::Deployed;
		Person.CurrentHealth = Health;
		Person.MaxHealth = Health;
		Person.Rank = Rank;
		Person.Experience = Experience;
		Person.Missions = Missions;
	};
	AddPerson(SurvivorId, TEXT("Mara Quill"), 50, 1, 240, 2);
	AddPerson(CasualtyId, TEXT("Tao Neris"), 40, 2, 400, 4);
	auto AddUnit = [&Battle](const FGuid PersonnelId, const int32 Health)
	{
		FTacticalUnitState& Unit = Battle.Units.AddDefaulted_GetRef();
		Unit.UnitId = PersonnelId;
		Unit.PersonnelId = PersonnelId;
		Unit.Team = ETacticalTeam::Player;
		Unit.MaxHealth = 50;
		Unit.CurrentHealth = Health;
	};
	AddUnit(SurvivorId, 30);
	AddUnit(CasualtyId, 0);

	FCampaignState After = Before;
	After.CampaignScore = 1000;
	After.TacticalOperations.Reset();
	After.TacticalBattles.Reset();
	FPersonnelState* SurvivorAfter = After.Personnel.FindByPredicate(
		[&SurvivorId](const FPersonnelState& Person) { return Person.PersonnelId == SurvivorId; });
	SurvivorAfter->CurrentHealth = 30;
	SurvivorAfter->Rank = 2;
	SurvivorAfter->Experience = 360;
	SurvivorAfter->Missions = 3;
	SurvivorAfter->RemainingRecoverySeconds = 7200;
	After.Personnel.RemoveAll([&CasualtyId](const FPersonnelState& Person) { return Person.PersonnelId == CasualtyId; });
	FMemorialRecord& Memorial = After.Memorial.AddDefaulted_GetRef();
	Memorial.PersonnelId = CasualtyId;
	Memorial.DisplayName = TEXT("Tao Neris");
	Memorial.Rank = 3;
	Memorial.Missions = 5;

	FStrategicCommandResult Resolution;
	Resolution.bAccepted = true;
	auto AddEvent = [&Resolution, OperationId, BattleId, SiteId, CraftId](const EStrategicEventType Type)
		-> FStrategicEvent&
	{
		FStrategicEvent& Event = Resolution.Events.AddDefaulted_GetRef();
		Event.Type = Type;
		Event.OperationId = OperationId;
		Event.BattleId = BattleId;
		Event.SiteId = SiteId;
		Event.CraftId = CraftId;
		return Event;
	};
	FStrategicEvent& Resolved = AddEvent(EStrategicEventType::TacticalOperationResolved);
	Resolved.Amount = 300;
	Resolved.Quantity = 1;
	FStrategicEvent& SurvivorExperience = AddEvent(EStrategicEventType::PersonnelExperienceGained);
	SurvivorExperience.PersonnelId = SurvivorId;
	SurvivorExperience.Amount = 120;
	SurvivorExperience.Quantity = 360;
	FStrategicEvent& SurvivorPromotion = AddEvent(EStrategicEventType::PersonnelPromoted);
	SurvivorPromotion.PersonnelId = SurvivorId;
	SurvivorPromotion.Amount = 1;
	SurvivorPromotion.Quantity = 2;
	FStrategicEvent& SurvivorFirstResponse = AddEvent(EStrategicEventType::PersonnelCommendationAwarded);
	SurvivorFirstResponse.PersonnelId = SurvivorId;
	SurvivorFirstResponse.RuleId = FirstResponse.Identity.RuleId;
	FStrategicEvent& SurvivorBreaker = AddEvent(EStrategicEventType::PersonnelCommendationAwarded);
	SurvivorBreaker.PersonnelId = SurvivorId;
	SurvivorBreaker.RuleId = Breaker.Identity.RuleId;
	FStrategicEvent& Injury = AddEvent(EStrategicEventType::PersonnelInjured);
	Injury.PersonnelId = SurvivorId;
	Injury.Amount = -20;
	Injury.Quantity = 30;
	FStrategicEvent& CasualtyExperience = AddEvent(EStrategicEventType::PersonnelExperienceGained);
	CasualtyExperience.PersonnelId = CasualtyId;
	CasualtyExperience.Amount = 120;
	CasualtyExperience.Quantity = 520;
	FStrategicEvent& CasualtyPromotion = AddEvent(EStrategicEventType::PersonnelPromoted);
	CasualtyPromotion.PersonnelId = CasualtyId;
	CasualtyPromotion.Amount = 2;
	CasualtyPromotion.Quantity = 3;
	FStrategicEvent& CasualtyFirstResponse = AddEvent(EStrategicEventType::PersonnelCommendationAwarded);
	CasualtyFirstResponse.PersonnelId = CasualtyId;
	CasualtyFirstResponse.RuleId = FirstResponse.Identity.RuleId;
	FStrategicEvent& Death = AddEvent(EStrategicEventType::PersonnelDied);
	Death.PersonnelId = CasualtyId;
	Death.Amount = -40;

	const FTacticalDebriefView Debrief = FTacticalPresentationService::BuildDebrief(
		Before, After, Rules, Resolution, OperationId);
	TestTrue(TEXT("Accepted tactical resolution produces a retained debrief"), Debrief.bAvailable);
	TestTrue(TEXT("Objective result becomes mission outcome"), Debrief.bMissionSucceeded);
	TestEqual(TEXT("Debrief exposes score delta"), Debrief.ScoreAwarded, int64(300));
	TestEqual(TEXT("Debrief exposes new campaign score"), Debrief.CampaignScore, int64(1000));
	TestEqual(TEXT("Debrief resolves mission display name"), Debrief.MissionDisplayName, FString(TEXT("Prism Fall")));
	TestEqual(TEXT("Debrief retains transport display name"), Debrief.CraftDisplayName, FString(TEXT("Far Lantern")));
	TestTrue(TEXT("Site-recovery debrief retains its operation context"),
		Debrief.OperationType == ETacticalOperationType::SiteRecovery
		&& Debrief.SiteId == SiteId && Debrief.CraftId == CraftId
		&& !Debrief.BaseId.IsValid() && !Debrief.AssaultId.IsValid());
	TestEqual(TEXT("Only newly recovered salvage is summarized"), Debrief.RecoveredCargo.Num(), 1);
	TestTrue(TEXT("Recovered item uses exact content disposition data"),
		Debrief.RecoveredCargo[0].DisplayName == TEXT("Prismatic Shard")
		&& Debrief.RecoveredCargo[0].Quantity == 2
		&& Debrief.RecoveredCargo[0].UnitMass == 2
		&& Debrief.RecoveredCargo[0].UnitSellValue == 900);
	TestEqual(TEXT("Every deployed agent receives a debrief row"), Debrief.Personnel.Num(), 2);
	const FTacticalDebriefPersonnelView* Survivor = Debrief.Personnel.FindByPredicate(
		[&SurvivorId](const FTacticalDebriefPersonnelView& Person) { return Person.PersonnelId == SurvivorId; });
	TestTrue(TEXT("Survivor row captures injury, recovery, experience, and promotion"), Survivor != nullptr
		&& Survivor->bInjured && !Survivor->bKilled && Survivor->DamageTaken == 20
		&& Survivor->ExperienceGained == 120 && Survivor->TotalExperience == 360
		&& Survivor->bPromoted && Survivor->PreviousRank == 1 && Survivor->NewRank == 2
		&& Survivor->Missions == 3 && Survivor->RecoverySeconds == 7200
		&& Survivor->ServiceHistory.Band == EPersonnelServiceBand::FirstWatch
		&& Survivor->ServiceHistory.NextBandMissions == 5
		&& Survivor->ServiceHistory.MissionsUntilNextBand == 2
		&& Survivor->PreviousServiceBand == EPersonnelServiceBand::FirstWatch
		&& !Survivor->bServiceBandAdvanced
		&& Survivor->AwardedCommendationIds.Num() == 2
		&& Survivor->AwardedCommendationIds[0] == FirstResponse.Identity.RuleId
		&& Survivor->AwardedCommendationIds[1] == Breaker.Identity.RuleId);
	const FTacticalDebriefPersonnelView* Casualty = Debrief.Personnel.FindByPredicate(
		[&CasualtyId](const FTacticalDebriefPersonnelView& Person) { return Person.PersonnelId == CasualtyId; });
	TestTrue(TEXT("Casualty row survives personnel removal and retains post-mission rank"), Casualty != nullptr
		&& Casualty->bKilled && !Casualty->bInjured && Casualty->EndingHealth == 0
		&& Casualty->ExperienceGained == 120 && Casualty->NewRank == 3 && Casualty->Missions == 5
		&& Casualty->PreviousServiceBand == EPersonnelServiceBand::FirstWatch
		&& Casualty->ServiceHistory.Band == EPersonnelServiceBand::FieldProven
		&& Casualty->ServiceHistory.NextBand == EPersonnelServiceBand::LongWatch
		&& Casualty->ServiceHistory.NextBandMissions == 10
		&& Casualty->ServiceHistory.MissionsUntilNextBand == 5
		&& Casualty->bServiceBandAdvanced
		&& Casualty->AwardedCommendationIds == TArray<FName>{ FirstResponse.Identity.RuleId });

	const FGuid BaseId(2701, 2702, 2703, 2704);
	const FGuid AssaultId(2801, 2802, 2803, 2804);
	FCampaignState BaseDefenseBefore = Before;
	FTacticalOperationState& BaseDefenseOperation = BaseDefenseBefore.TacticalOperations[0];
	BaseDefenseOperation.Type = ETacticalOperationType::BaseDefense;
	BaseDefenseOperation.SiteId.Invalidate();
	BaseDefenseOperation.CraftId.Invalidate();
	BaseDefenseOperation.BaseId = BaseId;
	BaseDefenseOperation.AssaultId = AssaultId;
	BaseDefenseBefore.TacticalBattles[0].SiteId.Invalidate();
	BaseDefenseBefore.TacticalBattles[0].bRequiresExtraction = false;
	BaseDefenseBefore.TacticalBattles[0].Cargo.Reset();
	FStrategicBaseState& DefendedBase = BaseDefenseBefore.Bases.AddDefaulted_GetRef();
	DefendedBase.BaseId = BaseId;
	DefendedBase.Name = TEXT("Cascadia Aegis");
	const FTacticalDebriefView BaseDefenseDebrief = FTacticalPresentationService::BuildDebrief(
		BaseDefenseBefore, After, Rules, Resolution, OperationId);
	TestTrue(TEXT("Base-defense debrief identifies the defended installation without a transport"),
		BaseDefenseDebrief.bAvailable
		&& BaseDefenseDebrief.OperationType == ETacticalOperationType::BaseDefense
		&& BaseDefenseDebrief.BaseId == BaseId
		&& BaseDefenseDebrief.AssaultId == AssaultId
		&& BaseDefenseDebrief.BaseDisplayName == TEXT("Cascadia Aegis")
		&& !BaseDefenseDebrief.SiteId.IsValid()
		&& !BaseDefenseDebrief.CraftId.IsValid()
		&& BaseDefenseDebrief.CraftDisplayName.IsEmpty()
		&& BaseDefenseDebrief.RecoveredCargo.IsEmpty());

	FStrategicCommandResult Rejected;
	const FTacticalDebriefView RejectedDebrief = FTacticalPresentationService::BuildDebrief(
		Before, After, Rules, Rejected, OperationId);
	TestFalse(TEXT("Rejected resolution does not replace a valid debrief"), RejectedDebrief.bAvailable);
	TestTrue(TEXT("Rejected resolution has a stable diagnostic"), RejectedDebrief.HasDiagnostic(TEXT("tactical_resolution_rejected")));
	return true;
}

#endif
