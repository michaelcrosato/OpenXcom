// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "UEGTGameMode.h"
#include "UEGTGameInstance.h"
#include "Strategic/UEGTStrategicGlobeActor.h"
#include "Strategic/UEGTStrategicHudWidget.h"
#include "Tactical/UEGTTacticalBoardActor.h"
#include "Tactical/UEGTTacticalCameraPawn.h"
#include "Tactical/UEGTTacticalHudWidget.h"
#include "Tactical/UEGTTacticalPlayerController.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTTacticalRuntimePresentationTest,
	"UEGT.Core.Game.TacticalRuntimeBoardCameraHud",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTTacticalRuntimePresentationTest::RunTest(const FString& Parameters)
{
	const AUEGTGameMode* GameMode = GetDefault<AUEGTGameMode>();
	TestNotNull(TEXT("Native game mode has a class default object"), GameMode);
	TestTrue(TEXT("Game mode installs the tactical camera pawn"), GameMode != nullptr
		&& GameMode->DefaultPawnClass == AUEGTTacticalCameraPawn::StaticClass());
	TestTrue(TEXT("Game mode installs the tactical command controller"), GameMode != nullptr
		&& GameMode->PlayerControllerClass == AUEGTTacticalPlayerController::StaticClass());
	TestTrue(TEXT("Native tactical HUD is a UMG widget class"),
		UUEGTTacticalHudWidget::StaticClass()->IsChildOf(UUserWidget::StaticClass()));

	FTestWorldWrapper TestWorld;
	TestTrue(TEXT("Temporary game world is available"), TestWorld.CreateTestWorld(EWorldType::Game));
	UWorld* World = TestWorld.GetTestWorld();
	if (World == nullptr)
	{
		return false;
	}
	AUEGTTacticalBoardActor* Board = World->SpawnActor<AUEGTTacticalBoardActor>();
	AUEGTTacticalCameraPawn* Camera = World->SpawnActor<AUEGTTacticalCameraPawn>();
	TestNotNull(TEXT("Runtime board actor spawns without authored assets"), Board);
	TestNotNull(TEXT("Runtime tactical camera spawns"), Camera);
	if (Board == nullptr || Camera == nullptr)
	{
		TestWorld.DestroyTestWorld(false);
		return false;
	}
	TestTrue(TEXT("Tactical board loads semantic primitive geometry for marker and effect layers"),
		Board->UsesSemanticMarkerGeometry());
	TestTrue(TEXT("Tactical board defaults to reduced motion for effect pulses"),
		Board->IsReducedMotionEnabled());
	Board->SetReducedMotionEnabled(false);
	TestFalse(TEXT("Tactical board enables presentation pulses when reduced motion is disabled"),
		Board->IsReducedMotionEnabled());
	FTacticalHudUnitView MarkerUnit;
	MarkerUnit.MaxHealth = 100;
	MarkerUnit.CurrentHealth = 100;
	const float StandingHeight = AUEGTTacticalBoardActor::CalculateUnitMarkerHeightScale(MarkerUnit);
	MarkerUnit.Stance = ETacticalStance::Crouched;
	const float CrouchedHeight = AUEGTTacticalBoardActor::CalculateUnitMarkerHeightScale(MarkerUnit);
	MarkerUnit.Stance = ETacticalStance::Standing;
	MarkerUnit.CurrentHealth = 20;
	const float DamagedHeight = AUEGTTacticalBoardActor::CalculateUnitMarkerHeightScale(MarkerUnit);
	TestTrue(TEXT("Tactical marker silhouette shortens for crouched stance"), CrouchedHeight < StandingHeight);
	TestTrue(TEXT("Tactical marker silhouette shortens for damaged health"), DamagedHeight < StandingHeight);
	MarkerUnit.bIncapacitated = true;
	TestEqual(TEXT("Incapacitated marker silhouette retains the low profile"),
		AUEGTTacticalBoardActor::CalculateUnitMarkerHeightScale(MarkerUnit), 0.18f);
	FTacticalHudObjectiveView MarkerObjective;
	MarkerObjective.RequiredInteractions = 4;
	TestTrue(TEXT("Objective marker starts at its bounded base height"),
		FMath::IsNearlyEqual(
			AUEGTTacticalBoardActor::CalculateObjectiveMarkerHeightScale(MarkerObjective),
			0.45f));
	MarkerObjective.PlayerInteractions = 2;
	MarkerObjective.AdversaryInteractions = 1;
	TestTrue(TEXT("Objective marker height reflects combined interaction progress"),
		AUEGTTacticalBoardActor::CalculateObjectiveMarkerHeightScale(MarkerObjective) > 0.45f
		&& AUEGTTacticalBoardActor::CalculateObjectiveMarkerHeightScale(MarkerObjective) < 1.0f);
	MarkerObjective.PlayerInteractions = MAX_int32;
	MarkerObjective.AdversaryInteractions = MAX_int32;
	TestTrue(TEXT("Objective marker progress clamps at full height without overflow"),
		FMath::IsNearlyEqual(
			AUEGTTacticalBoardActor::CalculateObjectiveMarkerHeightScale(MarkerObjective),
			1.0f));
	FTacticalHudCellView MarkerTerrain;
	MarkerTerrain.CurrentIntegrity = 100;
	MarkerTerrain.MaxIntegrity = 100;
	MarkerTerrain.CoverPercent = 0;
	TestTrue(TEXT("Cover markers retain a bounded base footprint"),
		FMath::IsNearlyEqual(
			AUEGTTacticalBoardActor::CalculateCoverMarkerScale(MarkerTerrain),
			0.38f));
	MarkerTerrain.CoverPercent = 45;
	TestTrue(TEXT("Cover marker footprint reflects authored cover"),
		AUEGTTacticalBoardActor::CalculateCoverMarkerScale(MarkerTerrain) > 0.38f
		&& AUEGTTacticalBoardActor::CalculateCoverMarkerScale(MarkerTerrain) < 0.9f);
	MarkerTerrain.CoverPercent = MAX_int32;
	TestTrue(TEXT("Cover marker footprint clamps at full cover"),
		FMath::IsNearlyEqual(
			AUEGTTacticalBoardActor::CalculateCoverMarkerScale(MarkerTerrain),
			0.9f));
	FTacticalPathStep MarkerPathStep;
	MarkerPathStep.MoveCost = 1;
	const float LowCostPathScale = AUEGTTacticalBoardActor::CalculatePathMarkerScale(MarkerPathStep);
	MarkerPathStep.MoveCost = 10;
	const float HighCostPathScale = AUEGTTacticalBoardActor::CalculatePathMarkerScale(MarkerPathStep);
	TestTrue(TEXT("Path marker footprint increases with action-point cost"),
		HighCostPathScale > LowCostPathScale);
	MarkerPathStep.MoveCost = MAX_int32;
	TestTrue(TEXT("Path marker footprint clamps at bounded maximum cost"),
		FMath::IsNearlyEqual(
			AUEGTTacticalBoardActor::CalculatePathMarkerScale(MarkerPathStep),
			0.76f));
	FTacticalHudUnitView MarkerSuppressedUnit;
	MarkerSuppressedUnit.Suppression = 0;
	TestTrue(TEXT("Suppression halos retain a bounded base footprint"),
		FMath::IsNearlyEqual(
			AUEGTTacticalBoardActor::CalculateSuppressionMarkerScale(MarkerSuppressedUnit),
			0.42f));
	MarkerSuppressedUnit.Suppression = 50;
	TestTrue(TEXT("Suppression halo footprint reflects current suppression"),
		AUEGTTacticalBoardActor::CalculateSuppressionMarkerScale(MarkerSuppressedUnit) > 0.42f
		&& AUEGTTacticalBoardActor::CalculateSuppressionMarkerScale(MarkerSuppressedUnit) < 0.76f);
	MarkerSuppressedUnit.Suppression = MAX_int32;
	TestTrue(TEXT("Suppression halo footprint clamps at full suppression"),
		FMath::IsNearlyEqual(
			AUEGTTacticalBoardActor::CalculateSuppressionMarkerScale(MarkerSuppressedUnit),
			0.76f));
	TestTrue(TEXT("Intact terrain markers retain their full silhouette"),
		FMath::IsNearlyEqual(
			AUEGTTacticalBoardActor::CalculateTerrainMarkerHeightScale(MarkerTerrain),
			1.0f));
	MarkerTerrain.CurrentIntegrity = 25;
	TestTrue(TEXT("Damaged terrain markers shorten with integrity"),
		AUEGTTacticalBoardActor::CalculateTerrainMarkerHeightScale(MarkerTerrain) > 0.32f
		&& AUEGTTacticalBoardActor::CalculateTerrainMarkerHeightScale(MarkerTerrain) < 1.0f);
	MarkerTerrain.CurrentIntegrity = MAX_int32;
	MarkerTerrain.MaxIntegrity = 1;
	TestTrue(TEXT("Terrain marker integrity clamps without overflow"),
		FMath::IsNearlyEqual(
			AUEGTTacticalBoardActor::CalculateTerrainMarkerHeightScale(MarkerTerrain),
			1.0f));
	MarkerTerrain.CurrentIntegrity = 40;
	MarkerTerrain.MaxIntegrity = 0;
	TestTrue(TEXT("Legacy terrain snapshots without maximum integrity retain full height"),
		FMath::IsNearlyEqual(
			AUEGTTacticalBoardActor::CalculateTerrainMarkerHeightScale(MarkerTerrain),
			1.0f));
	Board->ApplyAccessibilityPalette(EUEGTColorVisionMode::Tritanopia, false);
	TestEqual(TEXT("Tactical board accepts the selected color-vision palette"),
		Board->GetColorVisionMode(), EUEGTColorVisionMode::Tritanopia);
	TestFalse(TEXT("Tactical board accepts the softer marker contrast setting"),
		Board->IsHighContrastPaletteEnabled());

	FTacticalHudSnapshot Snapshot;
	Snapshot.bSucceeded = true;
	Snapshot.Width = 4;
	Snapshot.Height = 5;
	Snapshot.Levels = 2;
	Snapshot.ViewedLevel = 0;
	for (int32 X = 0; X < 3; ++X)
	{
		FTacticalHudCellView& Cell = Snapshot.VisibleCells.AddDefaulted_GetRef();
		Cell.CellIndex = X;
		Cell.X = X;
		Cell.Y = 1;
		Cell.Z = 0;
		Cell.bCurrentlyVisible = true;
		Cell.CurrentIntegrity = X == 1 ? 40 : 0;
		Cell.bBlocksMovement = X == 1;
		Cell.bIsVerticalConnector = X == 2;
		Cell.CoverPercent = X == 1 ? 60 : 0;
		Cell.bPlayerDeployment = X == 0;
		Cell.bExtraction = X == 2;
		Snapshot.KnownCells.Add(Cell);
	}
	FTacticalHudCellView& HistoricalCell = Snapshot.KnownCells.AddDefaulted_GetRef();
	HistoricalCell.CellIndex = 11;
	HistoricalCell.X = 3;
	HistoricalCell.Y = 2;
	HistoricalCell.Z = 0;
	Snapshot.VisibleCellCount = Snapshot.VisibleCells.Num();
	Snapshot.KnownCellCount = Snapshot.KnownCells.Num();
	FTacticalHudUnitView& Player = Snapshot.Units.AddDefaulted_GetRef();
	Player.UnitId = FGuid(1, 2, 3, 4);
	Player.Team = ETacticalTeam::Player;
	Player.X = 0;
	Player.Y = 1;
	Player.Suppression = 25;
	Player.bSelected = true;
	Player.DisplayName = TEXT("Ari Venn");
	FTacticalHudWeaponView& Weapon = Player.Weapons.AddDefaulted_GetRef();
	Weapon.ItemId = TEXT("item.service-rifle");
	Weapon.DisplayName = TEXT("Service Rifle");
	Weapon.LoadedAmmunition = 4;
	Weapon.MagazineCapacity = 6;
	Weapon.ReserveMagazines = 2;
	Weapon.FullReserveMagazines = 1;
	Weapon.PartialReserveMagazines = 1;
	Weapon.ReserveAmmunition = 9;
	Weapon.NextReloadAmmunition = 6;
	Weapon.ReloadActionPointCost = 2;
	Snapshot.EffectiveWeaponItemId = Weapon.ItemId;
	FTacticalHudUnitView& Adversary = Snapshot.Units.AddDefaulted_GetRef();
	Adversary.UnitId = FGuid(5, 6, 7, 8);
	Adversary.Team = ETacticalTeam::Adversary;
	Adversary.X = 2;
	Adversary.Y = 1;
	FTacticalHudUnitView& LastKnownAdversary = Snapshot.Units.AddDefaulted_GetRef();
	LastKnownAdversary.UnitId = FGuid(9, 10, 11, 12);
	LastKnownAdversary.Team = ETacticalTeam::Adversary;
	LastKnownAdversary.X = 3;
	LastKnownAdversary.Y = 2;
	LastKnownAdversary.CurrentHealth = 21;
	LastKnownAdversary.MaxHealth = 35;
	LastKnownAdversary.CurrentMorale = 67;
	LastKnownAdversary.MaxMorale = 100;
	LastKnownAdversary.Suppression = 14;
	LastKnownAdversary.LastSeenTurnNumber = 1;
	LastKnownAdversary.bLastKnown = true;
	LastKnownAdversary.bCurrentlyVisible = false;
	Snapshot.LastKnownAdversaryUnitCount = 1;
	FTacticalHudObjectiveView& Objective = Snapshot.Objectives.AddDefaulted_GetRef();
	Objective.ObjectiveId = TEXT("objective.runtime-test");
	Objective.X = 3;
	Objective.Y = 1;
	Objective.PlayerInteractions = 1;
	Objective.AdversaryInteractions = 1;
	Objective.RequiredInteractions = 3;
	Objective.Status = ETacticalObjectiveStatus::Active;
	FTacticalHudObjectiveView& CompletedObjective = Snapshot.Objectives.AddDefaulted_GetRef();
	CompletedObjective.ObjectiveId = TEXT("objective.runtime-complete");
	CompletedObjective.X = 1;
	CompletedObjective.Y = 1;
	CompletedObjective.Status = ETacticalObjectiveStatus::Completed;
	FTacticalHudObjectiveView& FailedObjective = Snapshot.Objectives.AddDefaulted_GetRef();
	FailedObjective.ObjectiveId = TEXT("objective.runtime-failed");
	FailedObjective.X = 2;
	FailedObjective.Y = 1;
	FailedObjective.Status = ETacticalObjectiveStatus::Failed;
	Snapshot.Hover.bHasCell = true;
	Snapshot.Hover.bCellVisible = true;
	Snapshot.Hover.X = 2;
	Snapshot.Hover.Y = 1;
	Snapshot.Hover.bHasPathPreview = true;
	Snapshot.Hover.Path.bSucceeded = true;
	Snapshot.Hover.Path.Steps.Add({ 1, 1, 0, 1 });
	Snapshot.Hover.Path.Steps.Add({ 2, 1, 0, 10 });

	Board->ApplySnapshot(Snapshot);
	const float InitialPresentationTime = Board->GetPresentationAnimationTimeSeconds();
	Board->Tick(0.5f);
	TestTrue(TEXT("Tactical board advances effect presentation time independently of tactical state"),
		Board->GetPresentationAnimationTimeSeconds() > InitialPresentationTime);
	Board->SetReducedMotionEnabled(true);
	const float ReducedMotionPresentationTime = Board->GetPresentationAnimationTimeSeconds();
	Board->Tick(0.5f);
	TestTrue(TEXT("Reduced motion freezes tactical effect presentation"),
		FMath::IsNearlyEqual(Board->GetPresentationAnimationTimeSeconds(), ReducedMotionPresentationTime));
	TestEqual(TEXT("Every fog-safe cell becomes one ground instance"), Board->GetRenderedGroundCount(), 3);
	TestEqual(TEXT("Historical coordinates use a separate fog-memory instance"), Board->GetRenderedFogMemoryCount(), 1);
	TestEqual(TEXT("Friendly presentation uses its own instance set"), Board->GetRenderedPlayerUnitCount(), 1);
	TestEqual(TEXT("Visible adversary presentation uses its own instance set"), Board->GetRenderedAdversaryUnitCount(), 1);
	TestEqual(TEXT("Last-known adversary presentation uses a separate subdued instance set"), Board->GetRenderedLastKnownAdversaryCount(), 1);
	TestEqual(TEXT("Active objective receives a board marker"), Board->GetRenderedObjectiveCount(), 1);
	TestEqual(TEXT("Completed objectives retain a non-interactive terminal marker"), Board->GetRenderedCompletedObjectiveCount(), 1);
	TestEqual(TEXT("Failed objectives retain a non-interactive terminal marker"), Board->GetRenderedFailedObjectiveCount(), 1);
	TestEqual(TEXT("Path previews render every visible-level step"), Board->GetRenderedPathCount(), 2);
	TestEqual(TEXT("Selected units receive a dedicated emphasis marker"), Board->GetRenderedSelectionCount(), 1);
	TestEqual(TEXT("Vertical connector cells receive a dedicated traversal marker"), Board->GetRenderedConnectorCount(), 1);
	TestEqual(TEXT("Covered cells receive a dedicated cover marker"), Board->GetRenderedCoverCount(), 1);
	TestEqual(TEXT("Visible suppressed units receive a dedicated suppression halo"), Board->GetRenderedSuppressionCount(), 1);
	TestEqual(TEXT("Player deployment cells receive a dedicated zone marker"), Board->GetRenderedDeploymentCount(), 1);
	TestEqual(TEXT("Extraction cells receive a dedicated zone marker"), Board->GetRenderedExtractionCount(), 1);
	UUEGTTacticalHudWidget* Hud = CreateWidget<UUEGTTacticalHudWidget>(
		World, UUEGTTacticalHudWidget::StaticClass());
	TestNotNull(TEXT("Native tactical HUD constructs without a widget blueprint"), Hud);
	if (Hud != nullptr)
	{
		Hud->TakeWidget();
		Hud->ApplySnapshot(Snapshot);
		TestTrue(TEXT("Native tactical HUD retains its source snapshot"),
			Hud->GetCurrentSnapshot().bSucceeded && Hud->GetCurrentSnapshot().Width == 4);

		const FString OriginalCulture = FInternationalization::Get().GetCurrentLanguage()->GetName();
		TestTrue(TEXT("Tactical HUD locale fixture activates French"),
			FInternationalization::Get().SetCurrentLanguageAndLocale(TEXT("fr")));
		FTacticalHudSnapshot LocalizedSnapshot = Snapshot;
		LocalizedSnapshot.MissionDisplayName = TEXT("Runtime Mission");
		LocalizedSnapshot.OperationType = ETacticalOperationType::SiteRecovery;
		LocalizedSnapshot.TurnNumber = 2;
		LocalizedSnapshot.TurnLimit = 20;
		LocalizedSnapshot.Phase = ETacticalBattlePhase::PlayerTurn;
		LocalizedSnapshot.WindDirection = ETacticalWindDirection::North;
		LocalizedSnapshot.WindStrength = 2;
		LocalizedSnapshot.CargoMass = 3;
		LocalizedSnapshot.CargoCapacity = 10;
		LocalizedSnapshot.Objectives[0].Type = ETacticalObjectiveType::Control;
		LocalizedSnapshot.Objectives[0].PlayerInteractions = 1;
		LocalizedSnapshot.Objectives[0].AdversaryInteractions = 2;
		LocalizedSnapshot.Objectives[0].RequiredInteractions = 3;
		LocalizedSnapshot.Hover.Path.TotalCost = 1;
		LocalizedSnapshot.Hover.bHasSignalPreview = true;
		LocalizedSnapshot.Hover.Signal.bSucceeded = true;
		LocalizedSnapshot.Hover.Signal.HitChance = 71;
		LocalizedSnapshot.Hover.Signal.MoraleDamage = 13;
		LocalizedSnapshot.Hover.Signal.SuppressionGain = 10;
		LocalizedSnapshot.Mentorship.bHasMentor = true;
		LocalizedSnapshot.Mentorship.bActive = true;
		LocalizedSnapshot.Mentorship.PolicyId = TEXT("personnel.mentorship-watchkeeper");
		LocalizedSnapshot.Mentorship.MentorId = Player.UnitId;
		LocalizedSnapshot.Mentorship.MentorDisplayName = TEXT("Ari Venn");
		LocalizedSnapshot.Mentorship.MentorServiceHistory = FPersonnelServiceHistory::Project(10);
		LocalizedSnapshot.Mentorship.MoraleBonus = 5;
		LocalizedSnapshot.Mentorship.RecipientCount = 1;
		LocalizedSnapshot.Mentorship.RecipientIds.Add(FGuid(9, 10, 11, 12));
		LocalizedSnapshot.LegacyRelay.bHasSpecialist = true;
		LocalizedSnapshot.LegacyRelay.bActive = true;
		LocalizedSnapshot.LegacyRelay.PolicyId = TEXT("personnel.specialization-legacy-relay");
		LocalizedSnapshot.LegacyRelay.SpecialistId = Player.UnitId;
		LocalizedSnapshot.LegacyRelay.SpecialistDisplayName = TEXT("Ari Venn");
		LocalizedSnapshot.LegacyRelay.SpecialistServiceHistory = FPersonnelServiceHistory::Project(20);
		LocalizedSnapshot.LegacyRelay.DoctrineId = TEXT("doctrine.clear-sight");
		LocalizedSnapshot.LegacyRelay.DoctrineDisplayName = TEXT("Clear Sight");
		LocalizedSnapshot.LegacyRelay.AccuracyBonus = 2;
		LocalizedSnapshot.LegacyRelay.RecipientCount = 1;
		LocalizedSnapshot.LegacyRelay.RecipientIds.Add(FGuid(9, 10, 11, 12));
		LocalizedSnapshot.SquadBonds.bActive = true;
		LocalizedSnapshot.SquadBonds.PolicyId = TEXT("personnel.squad-bond-field-cadence");
		LocalizedSnapshot.SquadBonds.ResolvedPersonnelCount = 2;
		LocalizedSnapshot.SquadBonds.EligiblePairCount = 1;
		FPersonnelSquadBondPairView& ActiveTacticalBond =
			LocalizedSnapshot.SquadBonds.ActivePairs.AddDefaulted_GetRef();
		ActiveTacticalBond.FirstPersonnelId = Player.UnitId;
		ActiveTacticalBond.SecondPersonnelId = FGuid(9, 10, 11, 12);
		ActiveTacticalBond.FirstDisplayName = TEXT("Ari Venn");
		ActiveTacticalBond.SecondDisplayName = TEXT("Pavel Orin");
		ActiveTacticalBond.SharedVictories = 8;
		ActiveTacticalBond.Tier = EPersonnelSquadBondTier::Interlocked;
		ActiveTacticalBond.NextTierVictories = 15;
		ActiveTacticalBond.ActionPointBonus = 1;
		ActiveTacticalBond.MoraleBonus = 5;
		FTacticalHudActionAvailability& MoveAction = LocalizedSnapshot.Actions.AddDefaulted_GetRef();
		MoveAction.ActionType = ETacticalHudActionType::Move;
		MoveAction.bAvailable = true;
		MoveAction.ActionPointCost = 2;
		FTacticalHudActionAvailability& EndTurnAction = LocalizedSnapshot.Actions.AddDefaulted_GetRef();
		EndTurnAction.ActionType = ETacticalHudActionType::EndTurn;
		EndTurnAction.bAvailable = true;
		FTacticalHudActionAvailability& EjectAction = LocalizedSnapshot.Actions.AddDefaulted_GetRef();
		EjectAction.ActionType = ETacticalHudActionType::EjectMagazine;
		EjectAction.bAvailable = true;
		EjectAction.ActionPointCost = 2;
		FTacticalHudActionAvailability& SignalAction = LocalizedSnapshot.Actions.AddDefaulted_GetRef();
		SignalAction.ActionType = ETacticalHudActionType::ProjectSignal;
		SignalAction.bAvailable = true;
		SignalAction.ActionPointCost = 4;
		FTacticalHudActionAvailability& DoorAction = LocalizedSnapshot.Actions.AddDefaulted_GetRef();
		DoorAction.ActionType = ETacticalHudActionType::OperateDoor;
		DoorAction.bAvailable = false;
		DoorAction.UnavailableReasonCode = TEXT("tactical_door_out_of_reach");
		DoorAction.UnavailableReason = TEXT("Raw tactical door range diagnostic.");
		FTacticalHudActionAvailability& DemoAction = LocalizedSnapshot.Actions.AddDefaulted_GetRef();
		DemoAction.ActionType = ETacticalHudActionType::Reload;
		DemoAction.bAvailable = false;
		DemoAction.UnavailableReasonCode = TEXT("demo_action_unavailable");
		DemoAction.UnavailableReason = TEXT("Raw presentation-fixture diagnostic.");
		Hud->ApplySnapshot(LocalizedSnapshot);
		TestEqual(TEXT("French tactical mission heading renders through native Slate"),
			Hud->GetRenderedMissionText(),
			FString(TEXT("RUNTIME MISSION  //  OPÉRATION DE TERRAIN  //  NIVEAU 1 SUR 2")));
		TestEqual(TEXT("French tactical phase retains authoritative turn, wind, and cargo values"),
			Hud->GetRenderedPhaseText(),
			FString(TEXT("TOUR 2 / 20  •  TOUR DU JOUEUR  •  VENT NORD 2  •  CARGAISON 3 / 10")));
		TestEqual(TEXT("French tactical fog summary distinguishes sight from memory"),
			Hud->GetRenderedFogText(),
			FString(TEXT("VUE ACTUELLE 3  •  MÉMOIRE DU SIGNAL 1")));
		TestTrue(TEXT("French tactical navigation guidance renders through native Slate"),
			Hud->GetRenderedStatusText().StartsWith(TEXT("CLIC G. sélectionner / cibler")));
		TestTrue(TEXT("French tactical section labels render through native Slate"),
			Hud->GetRenderedSectionLabels().Contains(TEXT("ÉQUIPE DE TERRAIN / CONTACTS"))
			&& Hud->GetRenderedSectionLabels().Contains(TEXT("OBJECTIF DE MISSION"))
			&& Hud->GetRenderedSectionLabels().Contains(TEXT("MANIFESTE DE TRANSPORT"))
			&& Hud->GetRenderedSectionLabels().Contains(
				TEXT("CONSEIL DE LA VIGIE  //  ARI VENN  •  LONGUE VEILLE\nMORAL INITIAL +5  •  BÉNÉFICIAIRES DE PALIER INFÉRIEUR 1"))
			&& Hud->GetRenderedSectionLabels().Contains(
				TEXT("Les mentors Longue veille accordent +5 au moral initial ; les Piliers d’héritage +10 ; les Balises persistantes +15. Seuls les coéquipiers d’un palier inférieur en bénéficient, avec un plafond de 100 ; la sélection est stable et n’utilise aucun tirage aléatoire."))
			&& Hud->GetRenderedSectionLabels().Contains(
				TEXT("RELAIS D’HÉRITAGE  //  ARI VENN  •  VISION NETTE\nRELAIS DE TERRAIN  •  PRÉC +2  RÉS +0  MOB +0  FOR +0  •  BÉNÉFICIAIRES 1"))
			&& Hud->GetRenderedSectionLabels().Contains(
				TEXT("Un Pilier d’héritage ou d’un palier supérieur ayant atteint le maximum d’une doctrine transmet la moitié arrondie au supérieur de ses bonus PRÉC/RÉS/MOB/FOR. Les missions puis les identifiants choisissent le spécialiste ; le bonus total puis l’identifiant choisissent la doctrine. Aucun tirage aléatoire.")));
		TestTrue(TEXT("French tactical HUD exposes exact active Field Cadence"),
			Hud->GetRenderedSectionLabels().Contains(TEXT("CADENCE DE TERRAIN"))
			&& Hud->GetRenderedSectionLabels().Contains(
				TEXT("ARI VENN + PAVEL ORIN  //  IMBRIQUÉS  •  VICTOIRES COMMUNES 8\nPA +1  •  MORAL +5"))
			&& Hud->GetRenderedSectionLabels().Contains(
				TEXT("Chaque opération réussie fait progresser tous les duos survivants. Le déploiement choisit les duos distincts les plus forts selon le palier, les victoires communes puis l’identité stable ; aucun tirage aléatoire n’est utilisé.")));
		TestTrue(TEXT("French tactical actions render through native Slate with localized AP cost"),
			Hud->GetRenderedActionLabels().Contains(TEXT("DÉPLACER  2 PA"))
			&& Hud->GetRenderedActionLabels().Contains(TEXT("FIN DU TOUR"))
			&& Hud->GetRenderedActionLabels().Contains(TEXT("ÉJECTER LE CHARGEUR  2 PA"))
			&& Hud->GetRenderedActionLabels().Contains(TEXT("PRESSION DE SIGNAL  4 PA")));
		TestTrue(TEXT("French tactical weapon summary exposes exact partial-magazine accounting"),
			Hud->GetRenderedUnitSummaries().ContainsByPredicate(
				[](const FString& Summary)
				{
					return Summary.Contains(
						TEXT("Fusil de service  •  CHARGEUR 4/6  •  RÉSERVE 2 (9 MUN.)  •  PARTIELS 1  •  SUIVANT 6"));
				}));
		TestTrue(TEXT("French tactical HUD labels last-known contacts without live AP details"),
			Hud->GetRenderedUnitSummaries().ContainsByPredicate(
				[](const FString& Summary)
				{
					return Summary.Contains(TEXT("DERNIER CONTACT CONNU  •  TOUR 1"))
						&& !Summary.Contains(TEXT("PA 0/0"));
				}));
		TestTrue(TEXT("French tactical HUD resolves stable unavailable-action codes into exact tooltips"),
			Hud->GetRenderedActionTooltips().Contains(
				TEXT("Placez d'abord l'unité sélectionnée à côté de la porte."))
			&& Hud->GetRenderedActionTooltips().Contains(
				TEXT("Cette démonstration de présentation n'active que les commandes exactes prévues.")));
		TestEqual(TEXT("French tactical hover preview renders through native Slate"),
			Hud->GetRenderedHoverText(),
			FString(TEXT("CASE 2 · 1 · N1  •  TRAJET 1 PA  •  SIGNAL 71%  −13 MORAL  +10 SUP")));

		FTacticalHudSnapshot DormantMentorshipSnapshot = LocalizedSnapshot;
		DormantMentorshipSnapshot.Mentorship.bActive = false;
		DormantMentorshipSnapshot.Mentorship.RecipientCount = 0;
		DormantMentorshipSnapshot.Mentorship.RecipientIds.Reset();
		Hud->ApplySnapshot(DormantMentorshipSnapshot);
		TestTrue(TEXT("French tactical HUD distinguishes a veteran whose guidance has no recipient"),
			Hud->GetRenderedSectionLabels().Contains(
				TEXT("CONSEIL DE LA VIGIE  //  ARI VENN  •  LONGUE VEILLE\nEN ATTENTE  •  AUCUN COÉQUIPIER DE PALIER INFÉRIEUR")));
		Hud->ApplySnapshot(LocalizedSnapshot);

		FTacticalHudSnapshot DormantRelaySnapshot = LocalizedSnapshot;
		DormantRelaySnapshot.LegacyRelay.bActive = false;
		DormantRelaySnapshot.LegacyRelay.RecipientCount = 0;
		DormantRelaySnapshot.LegacyRelay.RecipientIds.Reset();
		Hud->ApplySnapshot(DormantRelaySnapshot);
		TestTrue(TEXT("French tactical HUD distinguishes a Legacy Relay specialist with no recipient"),
			Hud->GetRenderedSectionLabels().Contains(
				TEXT("RELAIS D’HÉRITAGE  //  ARI VENN  •  VISION NETTE\nEN ATTENTE  •  AUCUN COÉQUIPIER POUR RECEVOIR LE RELAIS")));
		Hud->ApplySnapshot(LocalizedSnapshot);

		FTacticalHudSnapshot DevelopingSquadBondSnapshot = LocalizedSnapshot;
		DevelopingSquadBondSnapshot.SquadBonds.bActive = false;
		DevelopingSquadBondSnapshot.SquadBonds.EligiblePairCount = 0;
		DevelopingSquadBondSnapshot.SquadBonds.ActivePairs.Reset();
		FPersonnelSquadBondPairView& DevelopingTacticalBond =
			DevelopingSquadBondSnapshot.SquadBonds.DevelopingPairs.AddDefaulted_GetRef();
		DevelopingTacticalBond.FirstPersonnelId = Player.UnitId;
		DevelopingTacticalBond.SecondPersonnelId = FGuid(9, 10, 11, 12);
		DevelopingTacticalBond.FirstDisplayName = TEXT("Ari Venn");
		DevelopingTacticalBond.SecondDisplayName = TEXT("Pavel Orin");
		DevelopingTacticalBond.SharedVictories = 2;
		DevelopingTacticalBond.Tier = EPersonnelSquadBondTier::None;
		DevelopingTacticalBond.NextTierVictories = 3;
		Hud->ApplySnapshot(DevelopingSquadBondSnapshot);
		TestTrue(TEXT("French tactical HUD distinguishes a developing Field Cadence"),
			Hud->GetRenderedSectionLabels().Contains(
				TEXT("AUCUNE CADENCE ACTIVE  •  DUOS EN FORMATION 1"))
			&& Hud->GetRenderedSectionLabels().Contains(
				TEXT("ARI VENN + PAVEL ORIN  //  FORMATION 2/3")));
		Hud->ApplySnapshot(LocalizedSnapshot);

		FTacticalHudSnapshot MissingBattleSnapshot;
		FTacticalPresentationDiagnostic& MissingBattleDiagnostic =
			MissingBattleSnapshot.Diagnostics.AddDefaulted_GetRef();
		MissingBattleDiagnostic.Code = TEXT("unknown_tactical_battle");
		MissingBattleDiagnostic.Message = TEXT("Raw unknown battlefield diagnostic.");
		Hud->ApplySnapshot(MissingBattleSnapshot);
		TestEqual(TEXT("French tactical HUD resolves game-layer missing-battle diagnostics exactly"),
			Hud->GetRenderedStatusText(),
			FString(TEXT("Le champ de bataille tactique demandé n'est plus disponible.")));
		Hud->ApplySnapshot(LocalizedSnapshot);

		FTacticalDebriefView Debrief;
		Debrief.bAvailable = true;
		Debrief.bMissionSucceeded = true;
		Debrief.OperationType = ETacticalOperationType::BaseDefense;
		Debrief.BaseDisplayName = TEXT("Runtime Station");
		Debrief.ScoreAwarded = 120;
		Debrief.CampaignScore = 420;
		FTacticalDebriefPersonnelView& Person = Debrief.Personnel.AddDefaulted_GetRef();
		Person.DisplayName = TEXT("Ari Vega");
		Person.StartingHealth = 36;
		Person.EndingHealth = 27;
		Person.ExperienceGained = 8;
		Person.NewRank = 2;
		Person.Missions = 5;
		Person.PreviousServiceBand = EPersonnelServiceBand::FirstWatch;
		Person.ServiceHistory = FPersonnelServiceHistory::Project(Person.Missions);
		Person.bServiceBandAdvanced = true;
		Person.bInjured = true;
		Person.AwardedCommendationIds.Add(TEXT("commendation.first-response"));
		Hud->ApplyDebrief(Debrief);
		TestEqual(TEXT("French base-defense debrief title renders through native Slate"),
			Hud->GetRenderedMissionText(),
			FString(TEXT("DÉBRIEFING TACTIQUE  //  RUNTIME STATION")));
		TestEqual(TEXT("French base-defense debrief preserves exact score values"),
			Hud->GetRenderedPhaseText(),
			FString(TEXT("MISSION RÉUSSIE  •  SCORE +120  •  CAMPAGNE 420")));
		TestTrue(TEXT("French base-defense debrief status renders through native Slate"),
			Hud->GetRenderedStatusText().StartsWith(TEXT("Le relais de commandement a tenu.")));
		TestTrue(TEXT("French base-defense debrief sections render through native Slate"),
			Hud->GetRenderedSectionLabels().Contains(TEXT("PERSONNEL"))
			&& Hud->GetRenderedSectionLabels().Contains(TEXT("ÉTAT DE LA BASE"))
			&& Hud->GetRenderedSectionLabels().Contains(
				TEXT("SERVICE  AGUERRI\nPROCHAIN PALIER LONGUE VEILLE À 10 MISSIONS  •  RESTE 5"))
			&& Hud->GetRenderedSectionLabels().Contains(TEXT("JALON DE SERVICE  •  AGUERRI"))
			&& Hud->GetRenderedSectionLabels().Contains(
				TEXT("CITATION DÉCERNÉE  •  Citation de première intervention")));
		TestTrue(TEXT("French strategic-return action renders through native Slate"),
			Hud->GetRenderedActionLabels().Contains(TEXT("RETOUR AU COMMANDEMENT STRATÉGIQUE")));

		FTacticalDebriefView RecoveryDebrief = Debrief;
		RecoveryDebrief.OperationType = ETacticalOperationType::SiteRecovery;
		RecoveryDebrief.BaseDisplayName.Reset();
		RecoveryDebrief.CraftDisplayName = TEXT("Runtime Skiff");
		FTacticalHudItemView& RecoveredItem = RecoveryDebrief.RecoveredCargo.AddDefaulted_GetRef();
		RecoveredItem.ItemId = TEXT("item.runtime-salvage");
		RecoveredItem.DisplayName = TEXT("Runtime Salvage");
		RecoveredItem.Quantity = 2;
		RecoveredItem.UnitMass = 3;
		RecoveredItem.UnitSellValue = 250;
		Hud->ApplyDebrief(RecoveryDebrief);
		TestTrue(TEXT("French recovery debrief explains the delayed retain-or-sell decision"),
			Hud->GetRenderedSectionLabels().Contains(TEXT("RÉCUPÉRATION"))
			&& Hud->GetRenderedSectionLabels().Contains(
				TEXT("Le matériel récupéré reste à bord de Runtime Skiff. Conservez-le ou vendez-le après l'atterrissage depuis le commandement stratégique.")));
		TestTrue(TEXT("Tactical HUD locale fixture restores the original culture"),
			FInternationalization::Get().SetCurrentLanguageAndLocale(OriginalCulture));
	}
	const FVector GridLocation = Board->GridToWorld(2, 3, 1, 10.0f);
	TestTrue(TEXT("Grid projection uses stable cell and level spacing"), GridLocation.Equals(FVector(250.0f, 350.0f, 190.0f)));

	Camera->FocusBoard(20, 28, 0, Board->GetCellSize(), Board->GetLevelHeight());
	TestTrue(TEXT("Board focus selects a bounded useful zoom"), Camera->GetZoomDistance() >= 650.0f
		&& Camera->GetZoomDistance() <= 3600.0f);
	const float FocusedZoom = Camera->GetZoomDistance();
	Camera->FocusCell(2, 3, 1, Board->GetCellSize(), Board->GetLevelHeight());
	TestTrue(TEXT("Selection focus centers the exact tactical cell"),
		Camera->GetActorLocation().Equals(FVector(250.0f, 350.0f, 180.0f)));
	TestEqual(TEXT("Selection focus preserves the player's tactical zoom"),
		Camera->GetZoomDistance(), FocusedZoom);
	Camera->Zoom(100.0f);
	TestEqual(TEXT("Camera zoom clamps at its near limit"), Camera->GetZoomDistance(), 650.0f);
	const FVector CameraBeforePan = Camera->GetActorLocation();
	Camera->Pan(1.0f, 0.0f, 1.0f);
	TestFalse(TEXT("Camera pan changes its tactical focus"), Camera->GetActorLocation().Equals(CameraBeforePan));

	Board->ClearBoard();
	TestEqual(TEXT("Board clears generated instances atomically"), Board->GetRenderedGroundCount(), 0);
	TestEqual(TEXT("Board also clears fog-memory instances atomically"), Board->GetRenderedFogMemoryCount(), 0);
	TestEqual(TEXT("Board also clears last-known adversary markers atomically"), Board->GetRenderedLastKnownAdversaryCount(), 0);
	TestEqual(TEXT("Board clears completed objective markers atomically"), Board->GetRenderedCompletedObjectiveCount(), 0);
	TestEqual(TEXT("Board clears failed objective markers atomically"), Board->GetRenderedFailedObjectiveCount(), 0);
	TestTrue(TEXT("Temporary game world shuts down cleanly"), TestWorld.DestroyTestWorld(false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTStrategicGlobeDayNightModelTest,
	"UEGT.Core.Game.StrategicGlobeDayNightModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTStrategicGlobeDayNightModelTest::RunTest(const FString& Parameters)
{
	const FDateTime EquinoxNoonUtc(2035, 3, 21, 12, 0, 0);
	const FVector EquinoxNoonSun = AUEGTStrategicGlobeActor::CalculateSunDirection(EquinoxNoonUtc);
	TestTrue(TEXT("The simplified equinox places the subsolar latitude on the equator"),
		FMath::IsNearlyZero(AUEGTStrategicGlobeActor::CalculateSolarDeclinationDegrees(EquinoxNoonUtc), 0.01));
	TestTrue(TEXT("UTC noon places the subsolar longitude on the prime meridian"),
		FMath::IsNearlyZero(AUEGTStrategicGlobeActor::CalculateSubsolarLongitudeDegrees(EquinoxNoonUtc), 0.001));
	TestTrue(TEXT("Equinox noon produces a normalized prime-meridian sun direction"),
		EquinoxNoonSun.Equals(FVector::ForwardVector, 0.001f));

	const FDateTime EquinoxMidnightUtc(2035, 3, 21, 0, 0, 0);
	TestTrue(TEXT("UTC midnight places the subsolar longitude on the anti-meridian"),
		FMath::IsNearlyEqual(
			AUEGTStrategicGlobeActor::CalculateSubsolarLongitudeDegrees(EquinoxMidnightUtc),
			180.0,
			0.001));
	TestTrue(TEXT("Equinox midnight reverses the sun direction without changing campaign state"),
		AUEGTStrategicGlobeActor::CalculateSunDirection(EquinoxMidnightUtc).Equals(
			-FVector::ForwardVector,
			0.001f));
	TestTrue(TEXT("06:00 UTC advances the subsolar longitude east by one quarter turn"),
		FMath::IsNearlyEqual(
			AUEGTStrategicGlobeActor::CalculateSubsolarLongitudeDegrees(FDateTime(2035, 3, 21, 6, 0, 0)),
			90.0,
			0.001));
	TestTrue(TEXT("18:00 UTC advances the subsolar longitude west by one quarter turn"),
		FMath::IsNearlyEqual(
			AUEGTStrategicGlobeActor::CalculateSubsolarLongitudeDegrees(FDateTime(2035, 3, 21, 18, 0, 0)),
			-90.0,
			0.001));

	const double NorthernSolsticeDeclination =
		AUEGTStrategicGlobeActor::CalculateSolarDeclinationDegrees(FDateTime(2035, 6, 21, 12, 0, 0));
	const double SouthernSolsticeDeclination =
		AUEGTStrategicGlobeActor::CalculateSolarDeclinationDegrees(FDateTime(2035, 12, 21, 12, 0, 0));
	TestTrue(TEXT("Northern summer tilts daylight toward positive latitude"),
		NorthernSolsticeDeclination > 23.3 && NorthernSolsticeDeclination < 23.5);
	TestTrue(TEXT("Northern winter tilts daylight toward negative latitude"),
		SouthernSolsticeDeclination < -23.3 && SouthernSolsticeDeclination > -23.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTStrategicGlobeRegionalPressureModelTest,
	"UEGT.Core.Game.StrategicGlobeRegionalPressureModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTStrategicGlobeRegionalPressureModelTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Pressure below the elevated boundary uses the stable geometry"),
		AUEGTStrategicGlobeActor::ClassifyRegionalPressure(29),
		EUEGTRegionalPressureTier::Stable);
	TestEqual(TEXT("Pressure at the elevated boundary uses the guarded geometry"),
		AUEGTStrategicGlobeActor::ClassifyRegionalPressure(30),
		EUEGTRegionalPressureTier::Elevated);
	TestEqual(TEXT("Pressure below the critical boundary remains elevated"),
		AUEGTStrategicGlobeActor::ClassifyRegionalPressure(69),
		EUEGTRegionalPressureTier::Elevated);
	TestEqual(TEXT("Pressure at the critical boundary uses the alarm geometry"),
		AUEGTStrategicGlobeActor::ClassifyRegionalPressure(70),
		EUEGTRegionalPressureTier::Critical);
	TestEqual(TEXT("Stable, elevated, and critical rings have distinct sample silhouettes"),
		AUEGTStrategicGlobeActor::GetRegionalPressureSampleCount(EUEGTRegionalPressureTier::Stable)
			+ AUEGTStrategicGlobeActor::GetRegionalPressureSampleCount(EUEGTRegionalPressureTier::Elevated)
			+ AUEGTStrategicGlobeActor::GetRegionalPressureSampleCount(EUEGTRegionalPressureTier::Critical),
		36);
	TestTrue(TEXT("Negative pressure clamps to the minimum ring radius"),
		FMath::IsNearlyEqual(
			AUEGTStrategicGlobeActor::CalculateRegionPressureRingRadiusDegrees(-1),
			4.5,
			0.001));
	TestTrue(TEXT("Pressure above its domain clamps to the maximum ring radius"),
		FMath::IsNearlyEqual(
			AUEGTStrategicGlobeActor::CalculateRegionPressureRingRadiusDegrees(101),
			8.0,
			0.001));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTStrategicRuntimePresentationTest,
	"UEGT.Core.Game.StrategicRuntimeGlobeCameraHud",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTStrategicRuntimePresentationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Native strategic HUD is a UMG widget class"),
		UUEGTStrategicHudWidget::StaticClass()->IsChildOf(UUserWidget::StaticClass()));

	FTestWorldWrapper TestWorld;
	TestTrue(TEXT("Temporary strategic world is available"), TestWorld.CreateTestWorld(EWorldType::Game));
	UWorld* World = TestWorld.GetTestWorld();
	if (World == nullptr)
	{
		return false;
	}
	AUEGTStrategicGlobeActor* Globe = World->SpawnActor<AUEGTStrategicGlobeActor>();
	AUEGTTacticalCameraPawn* Camera = World->SpawnActor<AUEGTTacticalCameraPawn>();
	TestNotNull(TEXT("Runtime strategic globe spawns without authored assets"), Globe);
	TestNotNull(TEXT("Shared command camera spawns for globe presentation"), Camera);
	if (Globe == nullptr || Camera == nullptr)
	{
		TestWorld.DestroyTestWorld(false);
		return false;
	}

	FStrategicDashboardSnapshot Snapshot;
	Snapshot.bSucceeded = true;
	Snapshot.CampaignTimeUtc = FDateTime(2035, 1, 2, 3, 4, 5);
	Snapshot.Funds = 900000;
	Snapshot.MonthlyFunding = 1800000;
	Snapshot.MonthlyOutgoings = 650000;
	Snapshot.NetMonthlyFunding = 1150000;
	Snapshot.AdversaryMissionsLaunched = 7;
	Snapshot.AdversaryMissionsThwarted = 3;
	Snapshot.AdversaryMissionsEscaped = 2;
	Snapshot.AdversaryResolvedMissions = 5;
	Snapshot.AdversaryEscalationLevel = 4;
	Snapshot.VictoryThwartedMissionTarget = 12;
	Snapshot.VictoryEscalationTarget = 5;
	Snapshot.HighestRegionalPressure = 90;
	Snapshot.RegionalCollapsePressureThreshold = 100;
	Snapshot.ResolvedMissionsUntilNextEscalation = 1;
	Snapshot.NextAdversaryMissionSeconds = 72 * 3600;
	Snapshot.bCanAdvanceTime = true;
	FStrategicBaseView& Base = Snapshot.Bases.AddDefaulted_GetRef();
	Base.BaseId = FGuid(101, 102, 103, 104);
	Base.Name = TEXT("Runtime Station");
	Base.RegionDisplayName = TEXT("Test Reach");
	Base.GridWidth = 8;
	Base.GridHeight = 8;
	Snapshot.PrimaryBaseId = Base.BaseId;
	Base.ScientistCapacity = 6;
	Base.AssignedScientists = 2;
	Base.EngineerCapacity = 6;
	Base.SignalWatchPolicyId = TEXT("logistics.signal-watch");
	Base.SignalWatchScientists = 1;
	Base.SignalWatchMaximumScientists = 1;
	Base.FacilityRelayChannelCount = 1;
	Base.SignalWatchBonusChannelCount = 1;
	Base.RelayChannelCount = 2;
	Base.RelayQueueActiveConvoyCount = 2;
	Base.RelayQueueTotalConvoyCount = 3;
	Base.RelayQueueWaitingConvoyCount = 1;
	Base.RelayQueuePressurePercent = 34;
	Base.RelayQueueTailArrivalSeconds = int64(18) * 3600;
	Base.Specialization.SpecializationId = TEXT("base.specialization.flight-operations");
	Base.Specialization.Score = 100;
	Base.Specialization.SecondaryScore = 40;
	Base.Specialization.BenefitMetricId = TEXT("base.specialization.craft-berths");
	Base.Specialization.BenefitValue = 2;
	Base.Specialization.bSpecialized = true;
	Base.bCanIncreaseSignalWatch = false;
	Base.SignalWatchIncreaseUnavailableReasonCode =
		TEXT("signal_watch_channel_capacity_exceeded");
	Base.SignalWatchIncreaseUnavailableReason =
		TEXT("Signal Watch needs one operational facility channel per scientist; this base currently supports fewer.");
	Base.WorksCadrePolicyId = TEXT("facilities.works-cadre");
	Base.WorksCadreEngineers = 2;
	Base.WorksCadreMaximumEngineers = 3;
	Base.WorksCadreCharter = EWorksCadreCharter::AssemblyCadence;
	Base.WorksCadreCharterPolicyId =
		TEXT("facilities.works-charter-assembly-cadence");
	Base.WorksCadreConstructionFrontloadPercent = 30;
	Base.WorksCadreRepairFrontloadPercent = 10;
	Base.bCanIncreaseWorksCadre = true;
	for (const FWorksCadreCharterPolicy& Policy :
		FStrategicCommandService::GetWorksCadreCharterPolicies())
	{
		FStrategicWorksCadreCharterOptionView& Option =
			Base.WorksCadreCharterOptions.AddDefaulted_GetRef();
		Option.Charter = Policy.Charter;
		Option.PolicyId = Policy.PolicyId;
		Option.ConstructionFrontloadPercentPerEngineer =
			Policy.ConstructionFrontloadPercentPerEngineer;
		Option.RepairFrontloadPercentPerEngineer =
			Policy.RepairFrontloadPercentPerEngineer;
		Option.ConstructionFrontloadPercent =
			2 * Policy.ConstructionFrontloadPercentPerEngineer;
		Option.RepairFrontloadPercent =
			2 * Policy.RepairFrontloadPercentPerEngineer;
		Option.bSelected = Policy.Charter == Base.WorksCadreCharter;
		Option.bEnabled = !Option.bSelected;
	}
	FStrategicFacilityView& Facility = Base.FacilityLayout.AddDefaulted_GetRef();
	Facility.FacilityInstanceId = FGuid(105, 106, 107, 108);
	Facility.FacilityId = TEXT("facility.runtime-hub");
	Facility.DisplayName = TEXT("Runtime Hub");
	Facility.GridWidth = 2;
	Facility.GridHeight = 2;
	Facility.bCanDismantle = true;
	Facility.DismantleRefund = 250;
	Base.Facilities.Add(Facility.DisplayName);
	FStrategicInventoryView& Inventory = Base.Inventory.AddDefaulted_GetRef();
	Inventory.ItemId = TEXT("item.field-scanner");
	Inventory.DisplayName = TEXT("Field Scanner");
	Inventory.Quantity = 3;
	Inventory.UnitSellValue = 125;
	Inventory.bPersonnelEquippable = true;
	FStrategicMutualAidConvoyView& RetuneConvoy =
		Snapshot.MutualAidConvoys.AddDefaulted_GetRef();
	RetuneConvoy.ConvoyId = FGuid(0x7e700001, 0x7e700002, 0x7e700003, 0x7e700004);
	RetuneConvoy.SourceBaseId = Base.BaseId;
	RetuneConvoy.SourceBaseName = Base.Name;
	RetuneConvoy.DestinationBaseId =
		FGuid(0x7e710001, 0x7e710002, 0x7e710003, 0x7e710004);
	RetuneConvoy.DestinationBaseName = TEXT("Care Annex");
	RetuneConvoy.ItemId = Inventory.ItemId;
	RetuneConvoy.ItemDisplayName = Inventory.DisplayName;
	RetuneConvoy.Quantity = 2;
	RetuneConvoy.DispatchSequence = 1;
	RetuneConvoy.TotalStorage = 6;
	RetuneConvoy.RemainingTransitSeconds = int64(48) * 3600;
	RetuneConvoy.RoutePolicy = EMutualAidRoutePolicy::RapidThread;
	RetuneConvoy.RoutePolicyId = TEXT("logistics.mutual-aid-rapid-thread");
	RetuneConvoy.TotalTransitSeconds = int64(48) * 3600;
	RetuneConvoy.RoutePressure = 100;
	RetuneConvoy.bSignalEscort = false;
	RetuneConvoy.SignalEscortCost = 0;
	RetuneConvoy.bInterdictionResolved = false;
	RetuneConvoy.ForecastInterdictionDelaySeconds = int64(24) * 3600;
	RetuneConvoy.RelayQueue.bValid = true;
	RetuneConvoy.RelayQueue.bRelayAvailable = true;
	RetuneConvoy.RelayQueue.PolicyId = TEXT("logistics.mutual-aid-relay-weave");
	RetuneConvoy.RelayQueue.SourceBaseId = Base.BaseId;
	RetuneConvoy.RelayQueue.ConvoyId = RetuneConvoy.ConvoyId;
	RetuneConvoy.RelayQueue.DispatchSequence = RetuneConvoy.DispatchSequence;
	RetuneConvoy.RelayQueue.RelayChannelCount = 1;
	RetuneConvoy.RelayQueue.FacilityRelayChannelCount = 1;
	RetuneConvoy.RelayQueue.ActiveConvoyCount = 1;
	RetuneConvoy.RelayQueue.TotalConvoyCount = 2;
	RetuneConvoy.RelayQueue.QueuePosition = 2;
	RetuneConvoy.RelayQueue.WaitingPosition = 1;
	RetuneConvoy.RelayQueue.RelayChannelNumber = 1;
	RetuneConvoy.RelayQueue.EstimatedWaitSeconds = int64(36) * 3600;
	RetuneConvoy.RelayQueue.EstimatedArrivalSeconds = int64(108) * 3600;
	RetuneConvoy.bCanRetune = true;
	RetuneConvoy.bCanCommissionSignalEscort = true;
	RetuneConvoy.SignalEscortCommissionCost = 25000;
	RetuneConvoy.SignalEscortPreventedDelaySeconds = int64(24) * 3600;
	RetuneConvoy.SignalEscortProjectedRelayQueue = RetuneConvoy.RelayQueue;
	RetuneConvoy.SignalEscortProjectedRelayQueue.EstimatedArrivalSeconds =
		int64(84) * 3600;
	RetuneConvoy.bCanPrioritizeRelief = true;
	RetuneConvoy.ReliefPriorityBypassedConvoyCount = 1;
	RetuneConvoy.ReliefPriorityRecoveredWaitSeconds = int64(24) * 3600;
	RetuneConvoy.ReliefPriorityProjectedRelayQueue = RetuneConvoy.RelayQueue;
	RetuneConvoy.ReliefPriorityProjectedRelayQueue.EstimatedArrivalSeconds =
		int64(84) * 3600;
	RetuneConvoy.bCanStandDownRelief = true;
	RetuneConvoy.ReliefStandDownPolicyId = TEXT("logistics.relief-stand-down");
	RetuneConvoy.ReliefStandDownReleasedStorage = 6;
	RetuneConvoy.ReliefStandDownSunkSignalEscortCost = 0;
	RetuneConvoy.ReliefStandDownAdvancedConvoyCount = 1;
	RetuneConvoy.ReliefStandDownRecoveredWaitSeconds = int64(48) * 3600;
	RetuneConvoy.bCanDivertRelief = true;
	FStrategicMutualAidDiversionOptionView& CurrentDiversion =
		RetuneConvoy.ReliefDiversionOptions.AddDefaulted_GetRef();
	CurrentDiversion.DestinationBaseId = RetuneConvoy.DestinationBaseId;
	CurrentDiversion.DestinationBaseName = RetuneConvoy.DestinationBaseName;
	CurrentDiversion.PolicyId = TEXT("logistics.relief-diversion");
	CurrentDiversion.DivertedStorage = RetuneConvoy.TotalStorage;
	CurrentDiversion.CurrentRoutePressure = RetuneConvoy.RoutePressure;
	CurrentDiversion.ProjectedRoutePressure = RetuneConvoy.RoutePressure;
	CurrentDiversion.ProjectedRelayQueue = RetuneConvoy.RelayQueue;
	CurrentDiversion.UnavailableReasonCode =
		TEXT("mutual_aid_relief_diversion_same_destination");
	CurrentDiversion.UnavailableReason =
		TEXT("Select a different destination before diverting this relief convoy.");
	FStrategicMutualAidDiversionOptionView& AlternateDiversion =
		RetuneConvoy.ReliefDiversionOptions.AddDefaulted_GetRef();
	AlternateDiversion.DestinationBaseId =
		FGuid(0x7e720001, 0x7e720002, 0x7e720003, 0x7e720004);
	AlternateDiversion.DestinationBaseName = TEXT("Atlantic Relief Pier");
	AlternateDiversion.PolicyId = TEXT("logistics.relief-diversion");
	AlternateDiversion.DivertedStorage = RetuneConvoy.TotalStorage;
	AlternateDiversion.CurrentRoutePressure = RetuneConvoy.RoutePressure;
	AlternateDiversion.ProjectedRoutePressure = 40;
	AlternateDiversion.ProjectedRelayQueue = RetuneConvoy.RelayQueue;
	AlternateDiversion.ProjectedRelayQueue.EstimatedArrivalSeconds =
		int64(84) * 3600;
	AlternateDiversion.ArrivalShiftSeconds = -int64(24) * 3600;
	AlternateDiversion.AffectedConvoyCount = 1;
	AlternateDiversion.TotalArrivalShiftSeconds = -int64(48) * 3600;
	AlternateDiversion.bEnabled = true;
	RetuneConvoy.CurrentLegOriginBaseId = Base.BaseId;
	RetuneConvoy.CurrentLegOriginBaseName = Base.Name;
	RetuneConvoy.RelayWaypointBaseId = AlternateDiversion.DestinationBaseId;
	RetuneConvoy.RelayWaypointBaseName = AlternateDiversion.DestinationBaseName;
	RetuneConvoy.OnwardRoutePolicy = EMutualAidRoutePolicy::RapidThread;
	RetuneConvoy.OnwardRoutePolicyId = TEXT("logistics.mutual-aid-rapid-thread");
	RetuneConvoy.OnwardTransitSeconds = int64(48) * 3600;
	RetuneConvoy.OnwardRoutePressure = 75;
	RetuneConvoy.bOnwardInterdictionResolved = false;
	RetuneConvoy.OnwardForecastInterdictionDelaySeconds = int64(24) * 3600;
	RetuneConvoy.bCanConfigureRelayWaypoint = true;
	FStrategicMutualAidWaypointOptionView& DirectWaypoint =
		RetuneConvoy.RelayWaypointOptions.AddDefaulted_GetRef();
	DirectWaypoint.bDirectRoute = true;
	DirectWaypoint.PolicyId = TEXT("logistics.relay-waypoint");
	DirectWaypoint.OnwardRoutePolicy = RetuneConvoy.RoutePolicy;
	DirectWaypoint.ProjectedRelayQueue = RetuneConvoy.RelayQueue;
	DirectWaypoint.ProjectedRelayQueue.EstimatedArrivalSeconds = int64(84) * 3600;
	DirectWaypoint.JourneySeconds = int64(48) * 3600;
	DirectWaypoint.ArrivalShiftSeconds = -int64(24) * 3600;
	DirectWaypoint.AffectedConvoyCount = 1;
	DirectWaypoint.bEnabled = true;
	FStrategicMutualAidWaypointOptionView& CurrentWaypoint =
		RetuneConvoy.RelayWaypointOptions.AddDefaulted_GetRef();
	CurrentWaypoint.WaypointBaseId = RetuneConvoy.RelayWaypointBaseId;
	CurrentWaypoint.WaypointBaseName = RetuneConvoy.RelayWaypointBaseName;
	CurrentWaypoint.PolicyId = TEXT("logistics.relay-waypoint");
	CurrentWaypoint.OnwardRoutePolicy = EMutualAidRoutePolicy::RapidThread;
	CurrentWaypoint.OnwardRoutePolicyId = TEXT("logistics.mutual-aid-rapid-thread");
	CurrentWaypoint.FirstLegRoutePressure = 60;
	CurrentWaypoint.OnwardRoutePressure = 75;
	CurrentWaypoint.bOnwardInterdictionExpected = true;
	CurrentWaypoint.JourneySeconds = int64(120) * 3600;
	CurrentWaypoint.WaypointArrivalSeconds = int64(84) * 3600;
	CurrentWaypoint.ProjectedRelayQueue = RetuneConvoy.RelayQueue;
	CurrentWaypoint.ProjectedRelayQueue.EstimatedArrivalSeconds = int64(156) * 3600;
	CurrentWaypoint.ArrivalShiftSeconds = int64(48) * 3600;
	CurrentWaypoint.AffectedConvoyCount = 1;
	CurrentWaypoint.UnavailableReasonCode = TEXT("mutual_aid_relay_waypoint_same_plan");
	CurrentWaypoint.UnavailableReason =
		TEXT("Select a different waypoint or onward route before changing this convoy.");
	FStrategicMutualAidWaypointOptionView& VeiledWaypoint =
		RetuneConvoy.RelayWaypointOptions.AddDefaulted_GetRef();
	VeiledWaypoint.WaypointBaseId = RetuneConvoy.RelayWaypointBaseId;
	VeiledWaypoint.WaypointBaseName = RetuneConvoy.RelayWaypointBaseName;
	VeiledWaypoint.PolicyId = TEXT("logistics.relay-waypoint");
	VeiledWaypoint.OnwardRoutePolicy = EMutualAidRoutePolicy::VeiledChain;
	VeiledWaypoint.OnwardRoutePolicyId = TEXT("logistics.mutual-aid-veiled-chain");
	VeiledWaypoint.FirstLegRoutePressure = 60;
	VeiledWaypoint.OnwardRoutePressure = 50;
	VeiledWaypoint.JourneySeconds = int64(144) * 3600;
	VeiledWaypoint.WaypointArrivalSeconds = int64(84) * 3600;
	VeiledWaypoint.ProjectedRelayQueue = RetuneConvoy.RelayQueue;
	VeiledWaypoint.ProjectedRelayQueue.EstimatedArrivalSeconds = int64(180) * 3600;
	VeiledWaypoint.ArrivalShiftSeconds = int64(72) * 3600;
	VeiledWaypoint.AffectedConvoyCount = 1;
	VeiledWaypoint.bEnabled = true;
	RetuneConvoy.BalancedHandoffQuantity = 1;
	RetuneConvoy.FinalDeliveryQuantity = 1;
	RetuneConvoy.BalancedHandoffStorage = 3;
	RetuneConvoy.bCanConfigureBalancedHandoff = true;
	FStrategicMutualAidBalancedHandoffOptionView& ThroughCargo =
		RetuneConvoy.BalancedHandoffOptions.AddDefaulted_GetRef();
	ThroughCargo.bEnabledChoice = false;
	ThroughCargo.PolicyId = TEXT("logistics.mutual-aid-through-cargo");
	ThroughCargo.WaypointQuantity = 0;
	ThroughCargo.FinalQuantity = 2;
	ThroughCargo.HandoffStorage = 0;
	ThroughCargo.WaypointReservedStorage = 0;
	ThroughCargo.DestinationReservedStorage = 6;
	ThroughCargo.bEnabled = true;
	FStrategicMutualAidBalancedHandoffOptionView& BalancedHandoff =
		RetuneConvoy.BalancedHandoffOptions.AddDefaulted_GetRef();
	BalancedHandoff.bEnabledChoice = true;
	BalancedHandoff.PolicyId = TEXT("logistics.mutual-aid-balanced-handoff");
	BalancedHandoff.WaypointQuantity = 1;
	BalancedHandoff.FinalQuantity = 1;
	BalancedHandoff.HandoffStorage = 3;
	BalancedHandoff.WaypointReservedStorage = 3;
	BalancedHandoff.DestinationReservedStorage = 3;
	BalancedHandoff.UnavailableReasonCode =
		TEXT("mutual_aid_balanced_handoff_same_plan");
	BalancedHandoff.UnavailableReason =
		TEXT("Select a different cargo plan before changing this convoy.");
	const auto AddRetuneRoute = [&RetuneConvoy](
		const EMutualAidRoutePolicy Policy,
		const FName PolicyId,
		const int64 TransitHours,
		const int32 RoutePressure,
		const bool bEnabled)
	{
		FStrategicMutualAidRouteOptionView& Route =
			RetuneConvoy.RetuneRoutes.AddDefaulted_GetRef();
		Route.Policy = Policy;
		Route.PolicyId = PolicyId;
		Route.TransitSeconds = TransitHours * 3600;
		Route.BaselinePressure = 75;
		Route.RoutePressure = RoutePressure;
		Route.bInterdictionExpected = RoutePressure >= 75;
		Route.InterdictionDelaySeconds = int64(24) * 3600;
		Route.SignalEscortCost = RetuneConvoy.SignalEscortCost;
		Route.bSignalEscortAffordable = true;
		Route.bEnabled = bEnabled;
		Route.RelayQueue = RetuneConvoy.RelayQueue;
		Route.RelayQueue.EstimatedArrivalSeconds =
			Route.RelayQueue.EstimatedWaitSeconds + Route.TransitSeconds
			+ (Route.bInterdictionExpected ? int64(24) * 3600 : 0);
		Route.EscortedEstimatedArrivalSeconds =
			Route.RelayQueue.EstimatedWaitSeconds + Route.TransitSeconds;
		if (!bEnabled)
		{
			Route.UnavailableReasonCode = TEXT("mutual_aid_retune_same_policy");
			Route.UnavailableReason =
				TEXT("Select a different Threadline route before retuning this convoy.");
		}
	};
	AddRetuneRoute(
		EMutualAidRoutePolicy::OpenRelay,
		TEXT("logistics.mutual-aid-open-relay"), 72, 75, true);
	AddRetuneRoute(
		EMutualAidRoutePolicy::RapidThread,
		TEXT("logistics.mutual-aid-rapid-thread"), 48, 100, false);
	AddRetuneRoute(
		EMutualAidRoutePolicy::VeiledChain,
		TEXT("logistics.mutual-aid-veiled-chain"), 96, 50, true);
	const FGuid RuntimeAgentId(109, 110, 111, 112);
	const FGuid RuntimePilotId(113, 114, 115, 116);
	FStrategicPersonnelView& Person = Snapshot.Personnel.AddDefaulted_GetRef();
	Person.PersonnelId = RuntimeAgentId;
	Person.BaseId = Base.BaseId;
	Person.DisplayName = TEXT("Ari West");
	Person.RoleId = TEXT("role.field-agent");
	Person.RoleDisplayName = TEXT("Field Agent");
	Person.RoleCategory = EPersonnelRoleCategory::FieldAgent;
	Person.Status = TEXT("Available");
	Person.StatusType = EPersonnelStatus::Available;
	Person.CurrentHealth = 48;
	Person.MaxHealth = 48;
	Person.Rank = 3;
	Person.Missions = 5;
	Person.Kills = 3;
	Person.Experience = 700;
	Person.ServiceHistory = FPersonnelServiceHistory::Project(Person.Missions);
	Person.Accuracy = 61;
	Person.Resolve = 57;
	Person.Mobility = 64;
	Person.Strength = 53;
	Person.PendingDoctrineChoices = 2;
	FStrategicPersonnelCommendationView& Commendation = Person.Commendations.AddDefaulted_GetRef();
	Commendation.CommendationId = TEXT("commendation.first-response");
	Commendation.DisplayName = TEXT("First Response Citation");
	Commendation.Summary = TEXT("Completed a first successful field operation.");
	FStrategicPersonnelDoctrineView& ClearSight = Person.DoctrineOptions.AddDefaulted_GetRef();
	ClearSight.DoctrineId = TEXT("doctrine.clear-sight");
	ClearSight.DisplayName = TEXT("Clear Sight");
	ClearSight.Summary = TEXT("Disciplined target reading improves accuracy under field pressure.");
	ClearSight.CurrentSelections = 1;
	ClearSight.MaximumSelections = 3;
	ClearSight.AccuracyBonus = 4;
	ClearSight.bEnabled = true;
	FStrategicPersonnelDoctrineView& Steadfast = Person.DoctrineOptions.AddDefaulted_GetRef();
	Steadfast.DoctrineId = TEXT("doctrine.steadfast");
	Steadfast.DisplayName = TEXT("Steadfast");
	Steadfast.Summary = TEXT("Conditioned focus improves resolve and physical resilience.");
	Steadfast.MaximumSelections = 3;
	Steadfast.MaxHealthBonus = 2;
	Steadfast.ResolveBonus = 4;
	Steadfast.bEnabled = true;
	Person.bAssignedToCraft = true;
	Person.EquippedItemIds.Add(Inventory.ItemId);
	Person.EquippedItemNames.Add(Inventory.DisplayName);
	FStrategicMemorialView& Memorial = Snapshot.Memorial.AddDefaulted_GetRef();
	Memorial.PersonnelId = FGuid(121, 122, 123, 124);
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
	FStrategicPersonnelCommendationView& MemorialCommendation = Memorial.Commendations.AddDefaulted_GetRef();
	MemorialCommendation.CommendationId = TEXT("commendation.long-watch");
	MemorialCommendation.DisplayName = TEXT("Long Watch Star");
	MemorialCommendation.Summary = TEXT("Completed ten operations, reached rank three, and recorded eight eliminations.");
	FStrategicPersonnelView& Pilot = Snapshot.Personnel.AddDefaulted_GetRef();
	Pilot.PersonnelId = RuntimePilotId;
	Pilot.BaseId = Base.BaseId;
	Pilot.DisplayName = TEXT("Kai North");
	Pilot.RoleId = TEXT("role.pilot");
	Pilot.RoleDisplayName = TEXT("Interceptor Pilot");
	Pilot.RoleCategory = EPersonnelRoleCategory::Pilot;
	Pilot.Status = TEXT("Available");
	Pilot.StatusType = EPersonnelStatus::Available;
	Pilot.CurrentHealth = 50;
	Pilot.MaxHealth = 50;
	Pilot.Accuracy = 58;
	Pilot.Resolve = 62;
	Pilot.Mobility = 55;
	Pilot.Strength = 49;
	Pilot.bAssignedToCraft = true;
	FStrategicCraftView& CraftView = Snapshot.Craft.AddDefaulted_GetRef();
	CraftView.CraftId = FGuid(117, 118, 119, 120);
	CraftView.BaseId = Base.BaseId;
	CraftView.CraftRuleId = TEXT("craft.heron-transport");
	CraftView.DisplayName = TEXT("Runtime Skiff");
	CraftView.TypeDisplayName = TEXT("Heron Transport");
	CraftView.Status = TEXT("Grounded");
	CraftView.StatusType = ECraftStatus::Grounded;
	CraftView.CurrentHull = 100;
	CraftView.MaxHull = 100;
	CraftView.CurrentFuel = 400;
	CraftView.FuelCapacity = 500;
	CraftView.AgentCapacity = 4;
	CraftView.AssignedAgents = 1;
	CraftView.bHasPilot = true;
	CraftView.AssignedPilotId = RuntimePilotId;
	CraftView.AssignedAgentIds.Add(RuntimeAgentId);
	CraftView.Mentorship.bHasMentor = true;
	CraftView.Mentorship.bActive = true;
	CraftView.Mentorship.PolicyId = TEXT("personnel.mentorship-watchkeeper");
	CraftView.Mentorship.MentorId = RuntimeAgentId;
	CraftView.Mentorship.MentorDisplayName = TEXT("Ari West");
	CraftView.Mentorship.MentorServiceHistory = FPersonnelServiceHistory::Project(20);
	CraftView.Mentorship.MoraleBonus = 10;
	CraftView.Mentorship.RecipientCount = 1;
	CraftView.Mentorship.RecipientIds.Add(FGuid(133, 134, 135, 136));
	CraftView.LegacyRelay.bHasSpecialist = true;
	CraftView.LegacyRelay.bActive = true;
	CraftView.LegacyRelay.PolicyId = TEXT("personnel.specialization-legacy-relay");
	CraftView.LegacyRelay.SpecialistId = RuntimeAgentId;
	CraftView.LegacyRelay.SpecialistDisplayName = TEXT("Ari West");
	CraftView.LegacyRelay.SpecialistServiceHistory = FPersonnelServiceHistory::Project(20);
	CraftView.LegacyRelay.DoctrineId = TEXT("doctrine.clear-sight");
	CraftView.LegacyRelay.DoctrineDisplayName = TEXT("Clear Sight");
	CraftView.LegacyRelay.AccuracyBonus = 2;
	CraftView.LegacyRelay.RecipientCount = 1;
	CraftView.LegacyRelay.RecipientIds.Add(FGuid(133, 134, 135, 136));
	CraftView.SquadBonds.bActive = true;
	CraftView.SquadBonds.PolicyId = TEXT("personnel.squad-bond-field-cadence");
	CraftView.SquadBonds.ResolvedPersonnelCount = 2;
	CraftView.SquadBonds.EligiblePairCount = 1;
	FPersonnelSquadBondPairView& ActiveStrategicBond =
		CraftView.SquadBonds.ActivePairs.AddDefaulted_GetRef();
	ActiveStrategicBond.FirstPersonnelId = RuntimeAgentId;
	ActiveStrategicBond.SecondPersonnelId = FGuid(133, 134, 135, 136);
	ActiveStrategicBond.FirstDisplayName = TEXT("Ari West");
	ActiveStrategicBond.SecondDisplayName = TEXT("Oren Pax");
	ActiveStrategicBond.SharedVictories = 8;
	ActiveStrategicBond.Tier = EPersonnelSquadBondTier::Interlocked;
	ActiveStrategicBond.NextTierVictories = 15;
	ActiveStrategicBond.ActionPointBonus = 1;
	ActiveStrategicBond.MoraleBonus = 5;
	FStrategicCraftWeaponView& WeaponView = CraftView.Weapons.AddDefaulted_GetRef();
	WeaponView.WeaponItemId = TEXT("item.runtime-cannon");
	WeaponView.WeaponDisplayName = TEXT("Runtime Cannon");
	WeaponView.AmmunitionItemId = TEXT("item.runtime-rounds");
	WeaponView.AmmunitionDisplayName = TEXT("Runtime Rounds");
	WeaponView.MountCount = 1;
	WeaponView.LoadedAmmunition = 3;
	WeaponView.Capacity = 6;
	WeaponView.MissingAmmunition = 3;
	WeaponView.BaseAvailableAmmunition = 2;
	WeaponView.LoadableAmmunition = 2;
	CraftView.TotalAmmunitionLoaded = 3;
	CraftView.TotalAmmunitionCapacity = 6;
	CraftView.TotalAmmunitionMissing = 3;
	CraftView.TotalAmmunitionLoadable = 2;
	CraftView.bSalvageDispositionAvailable = true;
	FStrategicCraftSalvageView& SalvageView = CraftView.PendingSalvage.AddDefaulted_GetRef();
	SalvageView.ItemId = TEXT("item.runtime-salvage");
	SalvageView.DisplayName = TEXT("Runtime Salvage");
	SalvageView.Quantity = 2;
	SalvageView.UnitStorage = 3;
	SalvageView.TotalStorage = 6;
	SalvageView.UnitSellValue = 250;
	SalvageView.TotalSellValue = 500;
	SalvageView.bCanRetainAtBase = true;
	SalvageView.bCanSell = true;
	const FName RuntimeFacilityId(TEXT("facility.runtime-annex"));
	FStrategicProjectView& Project = Snapshot.Projects.AddDefaulted_GetRef();
	Project.Type = EStrategicProjectType::Research;
	Project.BaseId = Base.BaseId;
	Project.RuleId = TEXT("research.runtime-test");
	Project.DisplayName = TEXT("Runtime Study");
	Project.Detail = TEXT("2 scientists • 4 h remaining");
	Project.AssignedStaff = 2;
	Project.Progress = 0.5f;
	FStrategicProjectView& ProductionProject = Snapshot.Projects.AddDefaulted_GetRef();
	ProductionProject.Type = EStrategicProjectType::Manufacturing;
	ProductionProject.ProjectId = FGuid(121, 122, 123, 124);
	ProductionProject.BaseId = Base.BaseId;
	ProductionProject.RuleId = Inventory.ItemId;
	ProductionProject.DisplayName = Inventory.DisplayName;
	ProductionProject.Detail = TEXT("3 units • 2 engineers • 8 h remaining");
	ProductionProject.AssignedStaff = 2;
	ProductionProject.UnitsRemaining = 3;
	ProductionProject.UnitCost = 250;
	ProductionProject.CancellationRefund = 500;
	ProductionProject.RemainingSeconds = 8 * 3600;
	ProductionProject.StorageDeltaPerUnit = 3;
	ProductionProject.Progress = 0.25f;
	FStrategicMaterialRequirementView& ProductionMaterial =
		ProductionProject.MaterialRequirements.AddDefaulted_GetRef();
	ProductionMaterial.ItemId = TEXT("item.resonance-shard");
	ProductionMaterial.DisplayName = TEXT("Resonance Shard");
	ProductionMaterial.PerUnitQuantity = 2;
	ProductionMaterial.AvailableQuantity = 6;
	ProductionMaterial.RefundableQuantity = 4;
	FStrategicProjectView& ConstructionProject = Snapshot.Projects.AddDefaulted_GetRef();
	ConstructionProject.Type = EStrategicProjectType::Construction;
	ConstructionProject.ProjectId = FGuid(125, 126, 127, 128);
	ConstructionProject.BaseId = Base.BaseId;
	ConstructionProject.RuleId = RuntimeFacilityId;
	ConstructionProject.DisplayName = TEXT("Runtime Annex");
	ConstructionProject.Detail = TEXT("Grid 2,0 • 8 h remaining");
	ConstructionProject.CancellationRefund = 300;
	ConstructionProject.Progress = 0.33f;
	FStrategicProjectView& CraftProject = Snapshot.Projects.AddDefaulted_GetRef();
	CraftProject.Type = EStrategicProjectType::CraftAcquisition;
	CraftProject.ProjectId = FGuid(129, 130, 131, 132);
	CraftProject.BaseId = Base.BaseId;
	CraftProject.RuleId = TEXT("craft.heron-transport");
	CraftProject.DisplayName = TEXT("Heron Transport 02");
	CraftProject.Detail = TEXT("IN TRANSIT • 30 h remaining");
	CraftProject.RemainingSeconds = 30 * 3600;
	CraftProject.Progress = 0.25f;
	FStrategicActionOptionView& FacilityOption = Snapshot.ActionOptions.AddDefaulted_GetRef();
	FacilityOption.Type = EStrategicActionOptionType::Facility;
	FacilityOption.RuleId = RuntimeFacilityId;
	FacilityOption.DisplayName = TEXT("Runtime Annex");
	FacilityOption.Detail = TEXT("1x1 • 12 h");
	FacilityOption.Cost = 500;
	FacilityOption.FacilityGridWidth = 1;
	FacilityOption.FacilityGridHeight = 1;
	FacilityOption.ValidFacilityPlacements.Add(FIntPoint(2, 0));
	FacilityOption.SuggestedGridX = 2;
	FacilityOption.SuggestedGridY = 0;
	FacilityOption.bUnlocked = true;
	FacilityOption.bAffordable = true;
	FacilityOption.bAvailable = true;
	FStrategicActionOptionView& ManufacturingOption = Snapshot.ActionOptions.AddDefaulted_GetRef();
	ManufacturingOption.Type = EStrategicActionOptionType::Manufacturing;
	ManufacturingOption.RuleId = Inventory.ItemId;
	ManufacturingOption.DisplayName = Inventory.DisplayName;
	ManufacturingOption.Detail = TEXT("1 engineer-hour");
	ManufacturingOption.Cost = 250;
	ManufacturingOption.DurationHours = 12;
	ManufacturingOption.StorageDeltaPerUnit = 3;
	ManufacturingOption.bUnlocked = true;
	ManufacturingOption.bAffordable = true;
	ManufacturingOption.bAvailable = true;
	FStrategicMaterialRequirementView& ManufacturingMaterial =
		ManufacturingOption.MaterialRequirements.AddDefaulted_GetRef();
	ManufacturingMaterial.ItemId = TEXT("item.resonance-shard");
	ManufacturingMaterial.DisplayName = TEXT("Resonance Shard");
	ManufacturingMaterial.PerUnitQuantity = 2;
	ManufacturingMaterial.AvailableQuantity = 6;
	FStrategicActionOptionView& CraftOption = Snapshot.ActionOptions.AddDefaulted_GetRef();
	CraftOption.Type = EStrategicActionOptionType::Craft;
	CraftOption.RuleId = TEXT("craft.sparrow-interceptor");
	CraftOption.DisplayName = TEXT("Sparrow Interceptor");
	CraftOption.Detail = TEXT("Acquire • 48 h • hull 120 • crew 0");
	CraftOption.Cost = 120000;
	CraftOption.DurationHours = 48;
	CraftOption.CraftMaxHull = 120;
	CraftOption.CraftAgentCapacity = 0;
	CraftOption.bUnlocked = true;
	CraftOption.bAffordable = true;
	CraftOption.bAvailable = true;
	FStrategicContactView& Contact = Snapshot.Contacts.AddDefaulted_GetRef();
	Contact.ContactId = FGuid(133, 134, 135, 136);
	Contact.ContactRuleId = TEXT("contact.skimmer");
	Contact.DisplayName = TEXT("Skimmer Contact");
	Contact.Status = TEXT("Detected");
	Contact.StatusType = EStrategicContactStatus::Detected;
	Contact.LongitudeMilliDegrees = 60000;
	Contact.LatitudeMilliDegrees = 20000;
	Contact.CurrentHull = 70;
	Contact.MaxHull = 80;
	Contact.ThreatRating = 2;
	Contact.RouteProgress = 0.25f;
	Contact.bTargetsBase = true;
	Contact.TargetBaseId = Base.BaseId;
	Contact.TargetBaseName = Base.Name;
	Contact.PlanId = TEXT("plan.mirror-rain");
	Contact.PlanDisplayName = TEXT("Mirror Rain Pattern");
	Contact.PlanStage = 1;
	Contact.EscapeBranchMissionRuleId = TEXT("mission.nightglass-raid");
	Contact.EscapeBranchMissionName = TEXT("Nightglass Raid");
	Contact.ThwartBranchMissionRuleId = TEXT("mission.saffron-incursion");
	Contact.ThwartBranchMissionName = TEXT("Saffron Incursion");
	Contact.bHasCoalitionCounterplay = true;
	FStrategicCoalitionCounterplayMemberView& EscapeStrain =
		Contact.EscapeStrainMembers.AddDefaulted_GetRef();
	EscapeStrain.RegionId = TEXT("region.runtime-elevated");
	EscapeStrain.CurrentSupport = 27;
	EscapeStrain.ProjectedSupport = 19;
	EscapeStrain.bWouldWithdraw = true;
	FStrategicCoalitionCounterplayMemberView& ThwartRecovery =
		Contact.ThwartRecoveryMembers.AddDefaulted_GetRef();
	ThwartRecovery.RegionId = TEXT("region.runtime-stable");
	ThwartRecovery.CurrentSupport = 30;
	ThwartRecovery.ProjectedSupport = 40;
	ThwartRecovery.bRemainsWithdrawn = true;
	Contact.bCanShadowToLanding = true;
	Contact.LandingSiteThreatRating = 4;
	Contact.LandingSiteLifetimeSeconds = 36 * 3600;
	Contact.WreckageSiteLifetimeSeconds = 72 * 3600;
	FStrategicSiteView& Site = Snapshot.Sites.AddDefaulted_GetRef();
	Site.SiteId = FGuid(137, 138, 139, 140);
	Site.Type = EStrategicSiteType::Landing;
	Site.SourceContactRuleId = TEXT("contact.harvester");
	Site.DisplayName = TEXT("Harvester Contact Landing Site");
	Site.LongitudeMilliDegrees = 90000;
	Site.LatitudeMilliDegrees = 30000;
	Site.ThreatRating = 4;
	Site.RemainingLifetimeSeconds = 36 * 3600;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FStrategicGlobeMarkerView& Marker = Snapshot.GlobeMarkers.AddDefaulted_GetRef();
		Marker.Type = static_cast<EStrategicGlobeMarkerType>(Index);
		Marker.EntityId = Index == 0 ? Base.BaseId
			: Index == 1 ? CraftView.CraftId
			: Index == 2 ? Contact.ContactId
			: Site.SiteId;
		Marker.DisplayName = FString::Printf(TEXT("Marker %d"), Index);
		Marker.LongitudeMilliDegrees = Index * 30000;
		Marker.LatitudeMilliDegrees = Index * 10000;
	}
	FStrategicGlobeRouteView& AdversaryRoute = Snapshot.GlobeRoutes.AddDefaulted_GetRef();
	AdversaryRoute.EntityId = FGuid(10, 11, 12, 13);
	AdversaryRoute.OriginLongitudeMilliDegrees = -90000;
	AdversaryRoute.DestinationLongitudeMilliDegrees = 30000;
	AdversaryRoute.Progress = 0.25f;
	FStrategicGlobeRouteView PlayerRoute = AdversaryRoute;
	PlayerRoute.EntityId = FGuid(20, 21, 22, 23);
	PlayerRoute.bPlayerControlled = true;
	Snapshot.GlobeRoutes.Add(PlayerRoute);
	FStrategicRegionView& StableRegion = Snapshot.Regions.AddDefaulted_GetRef();
	StableRegion.RegionId = TEXT("region.runtime-stable");
	StableRegion.DisplayName = TEXT("Runtime Stable");
	StableRegion.LongitudeMilliDegrees = -118000;
	StableRegion.LatitudeMilliDegrees = 49000;
	StableRegion.Pressure = 0;
	FStrategicRegionView& ElevatedRegion = Snapshot.Regions.AddDefaulted_GetRef();
	ElevatedRegion.RegionId = TEXT("region.runtime-elevated");
	ElevatedRegion.DisplayName = TEXT("Runtime Elevated");
	ElevatedRegion.LongitudeMilliDegrees = 15000;
	ElevatedRegion.LatitudeMilliDegrees = 52000;
	ElevatedRegion.Pressure = 45;
	FStrategicRegionView& CriticalRegion = Snapshot.Regions.AddDefaulted_GetRef();
	CriticalRegion.RegionId = TEXT("region.runtime-critical");
	CriticalRegion.DisplayName = TEXT("Runtime Critical");
	CriticalRegion.LongitudeMilliDegrees = 110000;
	CriticalRegion.LatitudeMilliDegrees = 35000;
	CriticalRegion.Pressure = 90;
	Snapshot.ArchiveTotalCount = 3;
	Snapshot.ArchiveLockedCount = 1;
	FStrategicArchiveEntryView& CommandArchiveEntry = Snapshot.ArchiveEntries.AddDefaulted_GetRef();
	CommandArchiveEntry.EntryId = TEXT("archive.signal-front-charter");
	CommandArchiveEntry.CategoryId = TEXT("category.command");
	CommandArchiveEntry.CategoryDisplayName = TEXT("Command");
	CommandArchiveEntry.DisplayName = TEXT("The Signal Front Charter");
	CommandArchiveEntry.Summary = TEXT("Mandate, limits, and operating doctrine for the distributed defense network.");
	CommandArchiveEntry.Body = TEXT("UEGT exists to keep regional governments connected when anomalous incursions fracture ordinary command. It coordinates evidence, logistics, and defensive action without replacing civil authority.\n\nEvery operation is governed by three constraints: preserve civilian continuity, record the evidence chain, and expose uncertainty instead of disguising it as certainty. Commanders are expected to win trust as carefully as they win battles.");
	CommandArchiveEntry.SortOrder = 10;
	CommandArchiveEntry.RelatedEntryIds.Add(TEXT("archive.perimeter-doctrine"));
	FStrategicArchiveEntryView& OperationsArchiveEntry = Snapshot.ArchiveEntries.AddDefaulted_GetRef();
	OperationsArchiveEntry.EntryId = TEXT("archive.perimeter-doctrine");
	OperationsArchiveEntry.CategoryId = TEXT("category.operations");
	OperationsArchiveEntry.CategoryDisplayName = TEXT("Operations");
	OperationsArchiveEntry.DisplayName = TEXT("Perimeter Defense Doctrine");
	OperationsArchiveEntry.Summary = TEXT("Mutually exclusive battery fire and ground-defense commitments at threatened bases.");
	OperationsArchiveEntry.Body = TEXT("A hostile carrier that reaches a base perimeter creates a mandatory response. Operational batteries with complete capacitor loads can engage in stable installation order, or every ready unassigned field agent at that base can be committed to a ground defense.\n\nEach battery consumes its authored load only when it fires; scarce stock can leave an otherwise operational emplacement unready. Choosing either response closes the other path. Ground teams defend the local command relay without an extraction route; a failed defense can damage distinct installed facilities and permanently affect the campaign.");
	OperationsArchiveEntry.SortOrder = 10;
	OperationsArchiveEntry.RelatedEntryIds.Add(CommandArchiveEntry.EntryId);

	Globe->ApplySnapshot(Snapshot);
	TestEqual(TEXT("Base markers have a dedicated interactive layer"), Globe->GetRenderedBaseCount(), 1);
	TestEqual(TEXT("Craft markers have a dedicated interactive layer"), Globe->GetRenderedCraftCount(), 1);
	TestEqual(TEXT("Contact markers have a dedicated interactive layer"), Globe->GetRenderedContactCount(), 1);
	TestEqual(TEXT("Site markers have a dedicated interactive layer"), Globe->GetRenderedSiteCount(), 1);
	TestEqual(TEXT("Both route arcs render deterministic point samples"), Globe->GetRenderedRoutePointCount(), 34);
	TestEqual(TEXT("Procedural latitude and longitude references remain deterministic"),
		Globe->GetRenderedReferencePointCount(), 756);
	TestEqual(TEXT("The geometric day-night boundary uses a fixed deterministic sample count"),
		Globe->GetRenderedTerminatorPointCount(), 96);
	TestEqual(TEXT("Stable regions use sparse spherical pressure rings"),
		Globe->GetRenderedRegionPressurePointCount(EUEGTRegionalPressureTier::Stable), 8);
	TestEqual(TEXT("Elevated regions use denser square pressure rings"),
		Globe->GetRenderedRegionPressurePointCount(EUEGTRegionalPressureTier::Elevated), 12);
	TestEqual(TEXT("Critical regions use the densest radial-bar pressure rings"),
		Globe->GetRenderedRegionPressurePointCount(EUEGTRegionalPressureTier::Critical), 16);
	TestEqual(TEXT("The globe retains the exact immutable campaign time used for lighting"),
		Globe->GetCurrentCampaignTimeUtc(), Snapshot.CampaignTimeUtc);
	TestTrue(TEXT("The campaign-time sun direction remains normalized"),
		FMath::IsNearlyEqual(Globe->GetCurrentSunDirection().Size(), 1.0f, 0.001f));
	TestFalse(TEXT("UTC lighting is snapshot-driven and never requires continuous globe animation"),
		Globe->IsActorTickEnabled());
	TestTrue(TEXT("Prime-meridian equator projection is stable"),
		Globe->LongitudeLatitudeToWorld(0, 0).Equals(FVector(520.0f, 0.0f, 0.0f), 0.1f));
	TestTrue(TEXT("Quarter-turn longitude projection is stable"),
		Globe->LongitudeLatitudeToWorld(90000, 0).Equals(FVector(0.0f, 520.0f, 0.0f), 0.1f));
	TestTrue(TEXT("North-pole projection is stable"),
		Globe->LongitudeLatitudeToWorld(0, 90000).Equals(FVector(0.0f, 0.0f, 520.0f), 0.1f));

	Snapshot.CampaignTimeUtc = FDateTime(2035, 3, 21, 12, 0, 0);
	Globe->ApplySnapshot(Snapshot);
	TestTrue(TEXT("Equinox-noon prime-meridian locations render on the daylight hemisphere"),
		Globe->IsLocationInDaylight(0, 0));
	TestFalse(TEXT("Equinox-noon anti-meridian locations render on the night hemisphere"),
		Globe->IsLocationInDaylight(180000, 0));
	TestEqual(TEXT("Snapshot refresh rebuilds rather than accumulates terminator geometry"),
		Globe->GetRenderedTerminatorPointCount(), 96);
	int32 RetrievedTerminatorPoints = 0;
	double MaximumRadiusError = 0.0;
	double MaximumSunAlignment = 0.0;
	for (int32 Index = 0; Index < Globe->GetRenderedTerminatorPointCount(); ++Index)
	{
		FVector Point;
		if (Globe->GetRenderedTerminatorPointLocalPosition(Index, Point))
		{
			++RetrievedTerminatorPoints;
			MaximumRadiusError = FMath::Max(
				MaximumRadiusError,
				FMath::Abs(static_cast<double>(Point.Size()) - (Globe->GetGlobeRadius() + 8.0)));
			MaximumSunAlignment = FMath::Max(
				MaximumSunAlignment,
				FMath::Abs(static_cast<double>(FVector::DotProduct(
					Point.GetSafeNormal(), Globe->GetCurrentSunDirection()))));
		}
	}
	TestEqual(TEXT("Every deterministic terminator sample exposes valid geometry"),
		RetrievedTerminatorPoints, 96);
	TestTrue(TEXT("Every terminator sample stays on the raised globe boundary"),
		MaximumRadiusError < 0.01);
	TestTrue(TEXT("Every terminator sample remains perpendicular to the sun direction"),
		MaximumSunAlignment < 0.0001);

	const auto VerifyRegionRing = [this, Globe](
		const TCHAR* Label,
		const EUEGTRegionalPressureTier Tier,
		const FStrategicRegionView& Region)
	{
		const int32 ExpectedCount = AUEGTStrategicGlobeActor::GetRegionalPressureSampleCount(Tier);
		const FVector CenterDirection = Globe->LongitudeLatitudeToWorld(
			Region.LongitudeMilliDegrees,
			Region.LatitudeMilliDegrees).GetSafeNormal();
		const double ExpectedAngle =
			AUEGTStrategicGlobeActor::CalculateRegionPressureRingRadiusDegrees(Region.Pressure);
		int32 RetrievedPoints = 0;
		double MaximumRegionRadiusError = 0.0;
		double MaximumAngularError = 0.0;
		for (int32 Index = 0;
			Index < Globe->GetRenderedRegionPressurePointCount(Tier);
			++Index)
		{
			FVector Point;
			if (Globe->GetRenderedRegionPressurePointLocalPosition(Tier, Index, Point))
			{
				++RetrievedPoints;
				MaximumRegionRadiusError = FMath::Max(
					MaximumRegionRadiusError,
					FMath::Abs(static_cast<double>(Point.Size()) - (Globe->GetGlobeRadius() + 14.0)));
				const double Dot = FMath::Clamp(
					static_cast<double>(FVector::DotProduct(CenterDirection, Point.GetSafeNormal())),
					-1.0,
					1.0);
				const double AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));
				MaximumAngularError = FMath::Max(
					MaximumAngularError,
					FMath::Abs(AngleDegrees - ExpectedAngle));
			}
		}
		TestEqual(*FString::Printf(TEXT("%s exposes every generated sample"), Label),
			RetrievedPoints, ExpectedCount);
		TestTrue(*FString::Printf(TEXT("%s stays on the raised globe surface"), Label),
			MaximumRegionRadiusError < 0.01);
		TestTrue(*FString::Printf(TEXT("%s keeps its exact pressure-scaled angular radius"), Label),
			MaximumAngularError < 0.01);
	};
	VerifyRegionRing(TEXT("Stable pressure geometry"), EUEGTRegionalPressureTier::Stable, Snapshot.Regions[0]);
	VerifyRegionRing(TEXT("Elevated pressure geometry"), EUEGTRegionalPressureTier::Elevated, Snapshot.Regions[1]);
	VerifyRegionRing(TEXT("Critical pressure geometry"), EUEGTRegionalPressureTier::Critical, Snapshot.Regions[2]);

	Globe->ApplyAccessibilityPalette(EUEGTColorVisionMode::Deuteranopia, true);
	TestEqual(TEXT("Accessibility palette changes preserve stable ring geometry"),
		Globe->GetRenderedRegionPressurePointCount(EUEGTRegionalPressureTier::Stable), 8);
	TestEqual(TEXT("Accessibility palette changes preserve elevated ring geometry"),
		Globe->GetRenderedRegionPressurePointCount(EUEGTRegionalPressureTier::Elevated), 12);
	TestEqual(TEXT("Accessibility palette changes preserve critical ring geometry"),
		Globe->GetRenderedRegionPressurePointCount(EUEGTRegionalPressureTier::Critical), 16);

	FStrategicDashboardSnapshot FailedSnapshot = Snapshot;
	FailedSnapshot.bSucceeded = false;
	FailedSnapshot.CampaignTimeUtc = FDateTime(2035, 3, 21, 0, 0, 0);
	Globe->ApplySnapshot(FailedSnapshot);
	TestEqual(TEXT("A failed dashboard cannot replace the last accepted presentation time"),
		Globe->GetCurrentCampaignTimeUtc(), Snapshot.CampaignTimeUtc);
	TestEqual(TEXT("A failed dashboard preserves the last valid geometric boundary"),
		Globe->GetRenderedTerminatorPointCount(), 96);
	TestEqual(TEXT("A failed dashboard clears stale regional-pressure geometry"),
		Globe->GetRenderedRegionPressurePointCount(EUEGTRegionalPressureTier::Stable)
			+ Globe->GetRenderedRegionPressurePointCount(EUEGTRegionalPressureTier::Elevated)
			+ Globe->GetRenderedRegionPressurePointCount(EUEGTRegionalPressureTier::Critical),
		0);

	UUEGTStrategicHudWidget* Hud = CreateWidget<UUEGTStrategicHudWidget>(
		World, UUEGTStrategicHudWidget::StaticClass());
	TestNotNull(TEXT("Native strategic HUD constructs without a widget blueprint"), Hud);
	if (Hud != nullptr)
	{
		Hud->TakeWidget();
		Hud->ApplySnapshot(Snapshot);
		TestTrue(TEXT("Strategic HUD retains its immutable dashboard"),
			Hud->GetCurrentSnapshot().bSucceeded && Hud->GetCurrentSnapshot().Funds == 900000
			&& Hud->GetCurrentSnapshot().Bases[0].FacilityLayout.Num() == 1
			&& Hud->GetCurrentSnapshot().Bases[0].Inventory[0].UnitSellValue == 125
			&& Hud->GetCurrentSnapshot().Projects[0].AssignedStaff == 2
			&& Hud->GetCurrentSnapshot().Craft[0].AssignedPilotId == RuntimePilotId
			&& Hud->GetCurrentSnapshot().Craft[0].AssignedAgentIds.Contains(RuntimeAgentId));
		Hud->SelectFacilityForPlacement(RuntimeFacilityId);
		TestTrue(TEXT("Native strategic HUD enters manual facility-placement mode"), Hud->IsPlacingFacility());
		Hud->CancelFacilityPlacement();
		TestFalse(TEXT("Facility-placement mode can be cancelled without a campaign mutation"), Hud->IsPlacingFacility());
		Hud->SelectFacilityForDismantle(Base.BaseId, Facility.FacilityInstanceId);
		TestTrue(TEXT("Operational base-grid facilities enter a two-step dismantling review"),
			Hud->IsReviewingFacilityDismantle());
		Hud->CancelFacilityDismantle();
		TestFalse(TEXT("Facility dismantling review can be cancelled without a campaign mutation"),
			Hud->IsReviewingFacilityDismantle());

		const FString OriginalCulture = FInternationalization::Get().GetCurrentLanguage()->GetName();
		TestTrue(TEXT("Unreal accepts French for strategic chrome rendering"),
			FInternationalization::Get().SetCurrentLanguageAndLocale(TEXT("fr")));
		FStrategicDashboardSnapshot FoundingSnapshot = Snapshot;
		FoundingSnapshot.bRequiresBase = true;
		FoundingSnapshot.Bases.Reset();
		Hud->ApplySnapshot(FoundingSnapshot);
		Hud->ShowStatusMessage(FString());
		TestEqual(TEXT("Founding screen renders its localized title"),
			Hud->GetRenderedTitleText(), FString(TEXT("UEGT  //  FONDER LE PREMIER QG")));
		TestTrue(TEXT("Founding screen renders localized economic chrome around exact state"),
			Hud->GetRenderedSubtitleText().Contains(TEXT("FONDS 900000"))
			&& Hud->GetRenderedSubtitleText().Contains(TEXT("GRAINE VERROUILLÉE")));
		TestTrue(TEXT("Founding screen renders its localized default guidance"),
			Hud->GetRenderedStatusText().StartsWith(TEXT("Choisissez un site de commandement régional.")));
		const TArray<FString> FoundingActions = Hud->GetRenderedCommandActionLabels();
		TestTrue(TEXT("Founding screen renders both localized persistent actions"),
			FoundingActions.Num() == 2
			&& FoundingActions[0] == TEXT("SAUVER  CAMPAIGN1")
			&& FoundingActions[1] == TEXT("PARAMÈTRES"));

		Hud->ApplySnapshot(Snapshot);
		Hud->ShowStatusMessage(FString());
		TestEqual(TEXT("Command dashboard renders its localized title"),
			Hud->GetRenderedTitleText(), FString(TEXT("UEGT  //  COMMANDEMENT STRATÉGIQUE")));
		TestTrue(TEXT("Command dashboard renders localized state and navigation chrome"),
			Hud->GetRenderedSubtitleText().Contains(TEXT("FONDS 900000"))
			&& Hud->GetRenderedSubtitleText().Contains(TEXT("OPÉRATIONS ACTIVES"))
			&& Hud->GetRenderedStatusText().StartsWith(TEXT("CLIC G.")));
		const TArray<FString> SalvageLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French command dashboard renders exact containment, escalation, regional-risk, and adaptation readiness"),
			SalvageLabels.Contains(
				TEXT("ENDIGUEMENT 3/12 DÉJOUÉES  •  ESCALADE 4/5\nTENSION RÉGIONALE 90/100  •  PROCHAINE ADAPTATION DANS 1 RÉSOLUTION")));
		TestTrue(TEXT("French base card renders the exact Signal Watch staffing and channel derivation"),
			SalvageLabels.Contains(
				TEXT("VEILLE SIGNAL  •  SCI 1/1  •  CANAUX RELAIS 1+1=2")));
		TestTrue(TEXT("French base card renders Works Cadre staffing and all exact Works Charter tradeoffs"),
			SalvageLabels.Contains(
				TEXT("CADRE TRAVAUX  •  ING 2/3  •  CONSTR. 30 %  •  RÉPAR. 10 %"))
			&& SalvageLabels.Contains(
				TEXT("CHARTE DES TRAVAUX  •  HORLOGES FUTURES"))
			&& SalvageLabels.Contains(
				TEXT("CADENCE COMMUNE\nCONSTR. 20 %  •  RÉPAR. 20 %"))
			&& SalvageLabels.Contains(
				TEXT("CADENCE D’ASSEMBLAGE\nCONSTR. 30 %  •  RÉPAR. 10 %"))
			&& SalvageLabels.Contains(
				TEXT("CADENCE DE RESTAURATION\nCONSTR. 10 %  •  RÉPAR. 30 %")));
		TestTrue(TEXT("French Mutual Aid card exposes all held-convoy Threadline Retune choices"),
			SalvageLabels.Contains(
				TEXT("RÉACCORD THREADLINE  •  RETENU AVANT DÉPART"))
			&& SalvageLabels.Contains(TEXT("RELAIS OUVERT\n72 h • EXP 75"))
			&& SalvageLabels.Contains(TEXT("FIL RAPIDE\n48 h • EXP 100"))
			&& SalvageLabels.Contains(TEXT("CHAÎNE VOILÉE\n96 h • EXP 50")));
		TestTrue(TEXT("French Mutual Aid card exposes the exact post-dispatch Signal Surety action"),
			SalvageLabels.Contains(
				TEXT("GARANTIE SIGNAL  •  RETENU AVANT DÉPART"))
			&& SalvageLabels.Contains(
				TEXT("COMMANDER LA GARANTIE SIGNAL\nFONDS 25000 • ARRIVÉE 84 h • GAIN 24 h")));
		TestTrue(TEXT("French Mutual Aid card exposes the exact Relief Priority projection"),
			SalvageLabels.Contains(
				TEXT("PRIORITÉ SECOURS  •  FILE D’ATTENTE +1"))
			&& SalvageLabels.Contains(
				TEXT("ÉLEVER LA PRIORITÉ SECOURS\nARRIVÉE 84 h • GAIN 24 h • DEVANCE 1")));
		TestTrue(TEXT("French Mutual Aid card exposes the exact Relief Stand-Down contract"),
			SalvageLabels.Contains(
				TEXT("RETRAIT SECOURS  •  LIBÈRE 6 STOCKAGE"))
			&& SalvageLabels.Contains(
				TEXT("RETIRER LE CONVOI DE SECOURS\nRETOUR 2 • LIBÈRE 6 • AVANCE 1")));
		TestTrue(TEXT("French Mutual Aid card exposes the exact Relief Diversion choice"),
			SalvageLabels.Contains(
				TEXT("DÉROUTEMENT SECOURS  •  RETENU AVANT DÉPART"))
			&& SalvageLabels.Contains(
				TEXT("DÉROUTER VERS ATLANTIC RELIEF PIER\nRÉSERVE 6 • ARRIVÉE 84 h • DÉCALAGE -24 h • SUIVANTS 1")));
		TestTrue(TEXT("French Mutual Aid card exposes the active two-leg waypoint and exact alternatives"),
			SalvageLabels.Contains(
				TEXT("POINT RELAIS  •  VIA Atlantic Relief Pier  •  PUIS FIL RAPIDE  •  EXPOSITION 75/100  •  CANAL SOURCE RÉSERVÉ DE BOUT EN BOUT"))
			&& SalvageLabels.Contains(
				TEXT("POINT RELAIS  •  ITINÉRAIRE EN DEUX ÉTAPES"))
			&& SalvageLabels.Contains(
				TEXT("RÉTABLIR L’ITINÉRAIRE DIRECT\nFINALE 84 h • DÉCALAGE -24 h • SUIVANTS 1"))
			&& SalvageLabels.Contains(
				TEXT("VIA Atlantic Relief Pier • PUIS CHAÎNE VOILÉE\nRELAIS 84 h • FINALE 180 h • DÉCALAGE +72 h • EXP 60/50 • SUIVANTS 1")));
		TestTrue(TEXT("French Mutual Aid card exposes the exact active Balanced Handoff and both stable cargo plans"),
			SalvageLabels.Contains(
				TEXT("RELAIS ÉQUILIBRÉ  •  1 VERS Atlantic Relief Pier  •  1 VERS Care Annex  •  3 STOCKAGE RÉSERVÉ"))
			&& SalvageLabels.Contains(
				TEXT("RELAIS ÉQUILIBRÉ  •  CARGAISON DU POINT RELAIS"))
			&& SalvageLabels.Contains(
				TEXT("CARGAISON DIRECTE\nPOINT RELAIS 0 • FINALE 2 • STOCKAGE RELAIS 0"))
			&& SalvageLabels.Contains(
				TEXT("RELAIS ÉQUILIBRÉ\nPOINT RELAIS 1 • FINALE 1 • STOCKAGE RELAIS 3")));
		TestTrue(TEXT("French fleet card renders exact post-landing salvage choices"),
			SalvageLabels.Contains(TEXT("AFFECTATION DU MATÉRIEL RÉCUPÉRÉ"))
			&& SalvageLabels.Contains(
				TEXT("La cargaison récupérée est prête. Stockez-la à la base ou vendez-la directement."))
			&& SalvageLabels.Contains(TEXT("Runtime Salvage  ×2  •  STOCKAGE 6  •  VENTE 500"))
			&& SalvageLabels.Contains(TEXT("TOUT CONSERVER"))
			&& SalvageLabels.Contains(TEXT("TOUT VENDRE  •  500")));
		TestTrue(TEXT("French fleet card exposes exact active Watchkeeper Guidance"),
			SalvageLabels.Contains(
				TEXT("CONSEIL DE LA VIGIE  //  ARI WEST  •  PILIER D’HÉRITAGE\nMORAL INITIAL +10  •  BÉNÉFICIAIRES DE PALIER INFÉRIEUR 1"))
			&& SalvageLabels.Contains(
				TEXT("Les mentors Longue veille accordent +5 au moral initial ; les Piliers d’héritage +10 ; les Balises persistantes +15. Seuls les coéquipiers d’un palier inférieur en bénéficient, avec un plafond de 100 ; la sélection est stable et n’utilise aucun tirage aléatoire.")));
		TestTrue(TEXT("French fleet card exposes exact active Legacy Relay"),
			SalvageLabels.Contains(
				TEXT("RELAIS D’HÉRITAGE  //  ARI WEST  •  VISION NETTE\nRELAIS DE TERRAIN  •  PRÉC +2  RÉS +0  MOB +0  FOR +0  •  BÉNÉFICIAIRES 1"))
			&& SalvageLabels.Contains(
				TEXT("Un Pilier d’héritage ou d’un palier supérieur ayant atteint le maximum d’une doctrine transmet la moitié arrondie au supérieur de ses bonus PRÉC/RÉS/MOB/FOR. Les missions puis les identifiants choisissent le spécialiste ; le bonus total puis l’identifiant choisissent la doctrine. Aucun tirage aléatoire.")));
		TestTrue(TEXT("French fleet card exposes exact active Field Cadence"),
			SalvageLabels.Contains(TEXT("CADENCE DE TERRAIN"))
			&& SalvageLabels.Contains(
				TEXT("ARI WEST + OREN PAX  //  IMBRIQUÉS  •  VICTOIRES COMMUNES 8\nPA +1  •  MORAL +5"))
			&& SalvageLabels.Contains(
				TEXT("Chaque opération réussie fait progresser tous les duos survivants. Le déploiement choisit les duos distincts les plus forts selon le palier, les victoires communes puis l’identité stable ; aucun tirage aléatoire n’est utilisé.")));
		FStrategicDashboardSnapshot RecoveryDecisionSnapshot = Snapshot;
		RecoveryDecisionSnapshot.bDecisionRequired = true;
		RecoveryDecisionSnapshot.bCanAdvanceTime = false;
		FStrategicPersonnelView& RecoveringPerson =
			RecoveryDecisionSnapshot.Personnel.AddDefaulted_GetRef();
		RecoveringPerson.PersonnelId = FGuid(0x7a520001, 0x7a520002, 0x7a520003, 0x7a520004);
		RecoveringPerson.BaseId = Base.BaseId;
		RecoveringPerson.DisplayName = TEXT("Maëlle Venn");
		RecoveringPerson.RoleId = TEXT("role.field-agent");
		RecoveringPerson.RoleDisplayName = TEXT("Field Agent");
		RecoveringPerson.RoleCategory = EPersonnelRoleCategory::FieldAgent;
		RecoveringPerson.Status = TEXT("Recovering");
		RecoveringPerson.StatusType = EPersonnelStatus::Recovering;
		RecoveringPerson.CurrentHealth = 45;
		RecoveringPerson.MaxHealth = 55;
		RecoveringPerson.Resolve = 51;
		RecoveringPerson.RemainingRecoverySeconds = int64(10) * 3600;
		RecoveringPerson.RecoveryPlan.bRecovering = true;
		RecoveringPerson.RecoveryPlan.bDecisionRequired = true;
		RecoveringPerson.RecoveryPlan.SelectedPlan = EPersonnelRecoveryPlan::DecisionRequired;
		RecoveringPerson.RecoveryPlan.BaselineRemainingSeconds = int64(10) * 3600;
		FPersonnelRecoveryPlanOptionView& MeasuredReturn =
			RecoveringPerson.RecoveryPlan.Options.AddDefaulted_GetRef();
		MeasuredReturn.Plan = EPersonnelRecoveryPlan::MeasuredReturn;
		MeasuredReturn.PolicyId = TEXT("personnel.recovery-measured-return");
		MeasuredReturn.DurationSeconds = int64(10) * 3600;
		MeasuredReturn.bAvailable = true;
		FPersonnelRecoveryPlanOptionView& SurgeCare =
			RecoveringPerson.RecoveryPlan.Options.AddDefaulted_GetRef();
		SurgeCare.Plan = EPersonnelRecoveryPlan::SurgeCare;
		SurgeCare.PolicyId = TEXT("personnel.recovery-surge-care");
		SurgeCare.DurationSeconds = int64(5) * 3600;
		SurgeCare.FundingCost = 20000;
		SurgeCare.bAvailable = true;
		FPersonnelRecoveryPlanOptionView& ReflectionCycle =
			RecoveringPerson.RecoveryPlan.Options.AddDefaulted_GetRef();
		ReflectionCycle.Plan = EPersonnelRecoveryPlan::ReflectionCycle;
		ReflectionCycle.PolicyId = TEXT("personnel.recovery-reflection-cycle");
		ReflectionCycle.DurationSeconds = int64(15) * 3600;
		ReflectionCycle.ResolveBonus = 1;
		ReflectionCycle.bAvailable = true;
		Hud->ApplySnapshot(RecoveryDecisionSnapshot);
		const TArray<FString> RecoveryLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French strategic Slate exposes the complete pending Return Path decision"),
			RecoveryLabels.Contains(TEXT("PARCOURS DE RETOUR"))
			&& RecoveryLabels.Contains(
				TEXT("Choisissez la remise en condition, des soins accélérés financés ou une récupération réflexive plus longue. Le temps stratégique attend cette décision."))
			&& RecoveryLabels.Contains(TEXT("RETOUR MESURÉ\n10 H • SANS FRAIS"))
			&& RecoveryLabels.Contains(TEXT("SOINS INTENSIFS\n5 H • 20000 FONDS"))
			&& RecoveryLabels.Contains(TEXT("CYCLE DE RÉFLEXION\n15 H • RÉS +1")));
		TestEqual(TEXT("French strategic Slate identifies the Return Path pause instead of a tactical deployment"),
			Hud->GetRenderedStatusText(),
			FString(TEXT("PAUSE DÉCISIONNELLE : choisissez un parcours de retour pour chaque nouvelle personne blessée.")));
		FStrategicDashboardSnapshot ActiveRecoverySnapshot = RecoveryDecisionSnapshot;
		ActiveRecoverySnapshot.bDecisionRequired = false;
		ActiveRecoverySnapshot.bCanAdvanceTime = true;
		FStrategicPersonnelView& ActiveRecovery = ActiveRecoverySnapshot.Personnel.Last();
		ActiveRecovery.RemainingRecoverySeconds = int64(15) * 3600;
		ActiveRecovery.RecoveryPlan.bDecisionRequired = false;
		ActiveRecovery.RecoveryPlan.SelectedPlan = EPersonnelRecoveryPlan::ReflectionCycle;
		ActiveRecovery.RecoveryPlan.SelectedPolicyId = TEXT("personnel.recovery-reflection-cycle");
		ActiveRecovery.RecoveryPlan.Options.Reset();
		Hud->ApplySnapshot(ActiveRecoverySnapshot);
		TestTrue(TEXT("French strategic Slate distinguishes a committed Return Path"),
			Hud->GetRenderedDynamicLabels().Contains(
				TEXT("CYCLE DE RÉFLEXION • ENCORE 15 H")));
		FStrategicDashboardSnapshot StewardshipSnapshot = Snapshot;
		FStrategicPersonnelView& Steward = StewardshipSnapshot.Personnel.AddDefaulted_GetRef();
		Steward.PersonnelId = FGuid(0x57e50021, 0x57e50022, 0x57e50023, 0x57e50024);
		Steward.BaseId = Base.BaseId;
		Steward.DisplayName = TEXT("Maëlle Venn");
		Steward.RoleId = TEXT("role.field-agent");
		Steward.RoleDisplayName = TEXT("Field Agent");
		Steward.RoleCategory = EPersonnelRoleCategory::FieldAgent;
		Steward.Status = TEXT("Stewarding");
		Steward.StatusType = EPersonnelStatus::Stewarding;
		Steward.Rank = 5;
		Steward.Missions = 18;
		Steward.Kills = 11;
		Steward.Experience = 2300;
		Steward.CurrentHealth = 62;
		Steward.MaxHealth = 62;
		Steward.Accuracy = 68;
		Steward.Resolve = 72;
		Steward.Mobility = 61;
		Steward.Strength = 63;
		Steward.StewardshipToursCompleted = 1;
		Steward.Stewardship.bBaseHasActiveSteward = true;
		Steward.Stewardship.bSelectedPersonnelIsSteward = true;
		Steward.Stewardship.StewardId = Steward.PersonnelId;
		Steward.Stewardship.StewardDisplayName = Steward.DisplayName;
		Steward.Stewardship.ActiveFocus = EPersonnelStewardshipFocus::TrainingCadre;
		Steward.Stewardship.ActivePolicyId = TEXT("personnel.stewardship-training-cadre");
		Steward.Stewardship.RemainingSeconds = int64(12) * 86400;
		Steward.Stewardship.DurationSeconds = int64(30) * 86400;
		Steward.Stewardship.MinimumMissions = 10;
		Steward.Stewardship.ReductionPercent = 25;
		Steward.Stewardship.ToursCompleted = 1;
		Steward.Stewardship.ResolveAwardTourCap = 3;
		Steward.Stewardship.ResolveBonusOnCompletion = 1;
		Hud->ApplySnapshot(StewardshipSnapshot);
		const TArray<FString> ActiveStewardshipLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French strategic Slate exposes the exact active Stewardship Rotation"),
			ActiveStewardshipLabels.Contains(TEXT("ROTATION D’INTENDANCE"))
			&& ActiveStewardshipLabels.Contains(
				TEXT("CADRE DE FORMATION • ENCORE 12 J • AVANTAGE 25 %"))
			&& ActiveStewardshipLabels.Contains(
				TEXT("Les formations lancées dans cette base prennent 25 % de temps en moins."))
			&& ActiveStewardshipLabels.Contains(
				TEXT("ROTATIONS TERMINÉES 1 • RÉSOLUTION À LA FIN +1 • ROTATIONS PRIMÉES 3")));
		FStrategicDashboardSnapshot EligibleStewardshipSnapshot = StewardshipSnapshot;
		FStrategicPersonnelView& EligibleSteward = EligibleStewardshipSnapshot.Personnel.Last();
		EligibleSteward.Status = TEXT("Available");
		EligibleSteward.StatusType = EPersonnelStatus::Available;
		EligibleSteward.Stewardship.bBaseHasActiveSteward = false;
		EligibleSteward.Stewardship.bSelectedPersonnelIsSteward = false;
		EligibleSteward.Stewardship.bEligible = true;
		EligibleSteward.Stewardship.ActiveFocus = EPersonnelStewardshipFocus::None;
		EligibleSteward.Stewardship.RemainingSeconds = 0;
		EligibleSteward.Stewardship.Options.Reset();
		for (const EPersonnelStewardshipFocus Focus : {
			EPersonnelStewardshipFocus::RecoveryAdvocacy,
			EPersonnelStewardshipFocus::TrainingCadre,
			EPersonnelStewardshipFocus::RecruitmentLiaison })
		{
			FPersonnelStewardshipOptionView& Option =
				EligibleSteward.Stewardship.Options.AddDefaulted_GetRef();
			Option.Focus = Focus;
			Option.DurationSeconds = int64(30) * 86400;
			Option.ReductionPercent = 25;
			Option.bAvailable = true;
		}
		Hud->ApplySnapshot(EligibleStewardshipSnapshot);
		const TArray<FString> EligibleStewardshipLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French strategic Slate exposes all three stable Stewardship choices"),
			EligibleStewardshipLabels.Contains(
				TEXT("SOUTIEN AU RÉTABLISSEMENT\n-25 % COÛT DES SOINS"))
			&& EligibleStewardshipLabels.Contains(
				TEXT("CADRE DE FORMATION\n-25 % TEMPS DE FORMATION"))
			&& EligibleStewardshipLabels.Contains(
				TEXT("LIAISON RECRUTEMENT\n-25 % TRANSIT DU RECRUTEMENT"))
			&& EligibleStewardshipLabels.Contains(
				TEXT("MISSIONS 18/10 • ROTATIONS TERMINÉES 1 • PROCHAINE FIN : RÉSOLUTION +1")));
		Hud->ApplySnapshot(Snapshot);
		FStrategicDashboardSnapshot DevelopingSquadBondSnapshot = Snapshot;
		FPersonnelSquadBondView& DevelopingSquadBonds =
			DevelopingSquadBondSnapshot.Craft[0].SquadBonds;
		DevelopingSquadBonds.bActive = false;
		DevelopingSquadBonds.EligiblePairCount = 0;
		DevelopingSquadBonds.ActivePairs.Reset();
		FPersonnelSquadBondPairView& DevelopingStrategicBond =
			DevelopingSquadBonds.DevelopingPairs.AddDefaulted_GetRef();
		DevelopingStrategicBond.FirstPersonnelId = RuntimeAgentId;
		DevelopingStrategicBond.SecondPersonnelId = FGuid(133, 134, 135, 136);
		DevelopingStrategicBond.FirstDisplayName = TEXT("Ari West");
		DevelopingStrategicBond.SecondDisplayName = TEXT("Oren Pax");
		DevelopingStrategicBond.SharedVictories = 2;
		DevelopingStrategicBond.Tier = EPersonnelSquadBondTier::None;
		DevelopingStrategicBond.NextTierVictories = 3;
		Hud->ApplySnapshot(DevelopingSquadBondSnapshot);
		TestTrue(TEXT("French fleet card distinguishes a developing Field Cadence"),
			Hud->GetRenderedDynamicLabels().Contains(
				TEXT("AUCUNE CADENCE ACTIVE  •  DUOS EN FORMATION 1"))
			&& Hud->GetRenderedDynamicLabels().Contains(
				TEXT("ARI WEST + OREN PAX  //  FORMATION 2/3")));
		Hud->ApplySnapshot(Snapshot);
		TestTrue(TEXT("French fleet card exposes exact craft ammunition while salvage locks turnaround"),
			SalvageLabels.Contains(TEXT("MUNITIONS DE L'APPAREIL  3/6"))
			&& SalvageLabels.Contains(
				TEXT("Runtime Cannon ×1  •  Runtime Rounds 3/6  •  BASE 2  •  CHARGEABLE 2"))
			&& SalvageLabels.Contains(TEXT("RÉARMEMENT COMPLET  •  3"))
			&& SalvageLabels.Contains(TEXT("CHARGER LE DISPONIBLE  •  2/3"))
			&& Hud->GetRenderedCraftRearmControlCount() == 2
			&& Hud->GetRenderedEnabledCraftRearmControlCount() == 0);
		FStrategicDashboardSnapshot PartialRearmSnapshot = Snapshot;
		PartialRearmSnapshot.Craft[0].PendingSalvage.Reset();
		PartialRearmSnapshot.Craft[0].bSalvageDispositionAvailable = false;
		PartialRearmSnapshot.Craft[0].bCanLoadAvailableAmmunition = true;
		Hud->ApplySnapshot(PartialRearmSnapshot);
		TestTrue(TEXT("French Slate enables only load-available when base stores cannot fill the craft"),
			Hud->GetRenderedCraftRearmControlCount() == 2
			&& Hud->GetRenderedEnabledCraftRearmControlCount() == 1
			&& Hud->GetRenderedDynamicLabels().Contains(TEXT("CHARGER LE DISPONIBLE  •  2/3")));
		FStrategicDashboardSnapshot ServicingSnapshot = Snapshot;
		ServicingSnapshot.Craft[0].PendingSalvage.Reset();
		ServicingSnapshot.Craft[0].bSalvageDispositionAvailable = false;
		ServicingSnapshot.Craft[0].StatusType = ECraftStatus::Servicing;
		ServicingSnapshot.Craft[0].Status = TEXT("Servicing");
		ServicingSnapshot.Craft[0].RemainingRepairSeconds = 2 * 3600;
		ServicingSnapshot.Craft[0].RemainingRefuelSeconds = 1 * 3600;
		ServicingSnapshot.Craft[0].RemainingServiceSeconds = 2 * 3600;
		ServicingSnapshot.Craft[0].ServiceQueue.bValid = true;
		ServicingSnapshot.Craft[0].ServiceQueue.PolicyId = TEXT("craft.service-rapid-turnaround");
		ServicingSnapshot.Craft[0].ServiceQueue.ServiceLaneCount = 1;
		ServicingSnapshot.Craft[0].ServiceQueue.ActiveServiceCraftCount = 1;
		ServicingSnapshot.Craft[0].ServiceQueue.TotalServiceCraftCount = 2;
		ServicingSnapshot.Craft[0].ServiceQueue.QueuePosition = 1;
		ServicingSnapshot.Craft[0].ServiceQueue.ServiceLaneNumber = 1;
		ServicingSnapshot.Craft[0].ServiceQueue.bInServiceLane = true;
		ServicingSnapshot.Craft[0].ServiceQueue.EstimatedReadySeconds = 2 * 3600;
		ServicingSnapshot.Craft[0].ServiceCancellationRefund = 300;
		ServicingSnapshot.Craft[0].bCanCancelService = true;
		ServicingSnapshot.Craft[0].bCanRearmFully = false;
		ServicingSnapshot.Craft[0].bCanLoadAvailableAmmunition = false;
		Hud->ApplySnapshot(ServicingSnapshot);
		const TArray<FString>& ServiceLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French Slate exposes exact craft turnaround clocks, refund, and cancellation control"),
			ServiceLabels.Contains(TEXT("ENTRETIEN DE ROTATION"))
			&& ServiceLabels.Contains(TEXT("ROTATION RAPIDE"))
			&& ServiceLabels.Contains(TEXT("POSTE D'ENTRETIEN 1/1 • PRÊT DANS 2 h"))
			&& ServiceLabels.ContainsByPredicate(
				[](const FString& Label) { return Label.Contains(TEXT("sans tirage aléatoire")); })
			&& ServiceLabels.Contains(
				TEXT("RÉPARATION 2 h • RAVITAILLEMENT 1 h\nPRÊT DANS 2 h • REMBOURSEMENT 300"))
			&& ServiceLabels.Contains(TEXT("ANNULER L'ENTRETIEN • +300"))
			&& Hud->GetRenderedCraftServiceControlCount() == 1
			&& Hud->GetRenderedEnabledCraftServiceControlCount() == 1);
		FStrategicDashboardSnapshot QueuedServiceSnapshot = ServicingSnapshot;
		QueuedServiceSnapshot.Craft[0].ServiceQueue.bInServiceLane = false;
		QueuedServiceSnapshot.Craft[0].ServiceQueue.QueuePosition = 2;
		QueuedServiceSnapshot.Craft[0].ServiceQueue.WaitingPosition = 1;
		QueuedServiceSnapshot.Craft[0].ServiceQueue.EstimatedWaitSeconds = 3600;
		QueuedServiceSnapshot.Craft[0].ServiceQueue.EstimatedReadySeconds = 3 * 3600;
		Hud->ApplySnapshot(QueuedServiceSnapshot);
		TestTrue(TEXT("French Slate distinguishes queued service and folds wait into readiness"),
			Hud->GetRenderedDynamicLabels().Contains(TEXT("FILE 1 • ATTENTE 1 h • PRÊT DANS 3 h"))
			&& Hud->GetRenderedDynamicLabels().Contains(
				TEXT("RÉPARATION 2 h • RAVITAILLEMENT 1 h\nPRÊT DANS 3 h • REMBOURSEMENT 300")));
		Hud->ApplySnapshot(Snapshot);
		Hud->ShowStatusMessage(FString());
		const TArray<FString> CommandActions = Hud->GetRenderedCommandActionLabels();
		TestTrue(TEXT("Command dashboard renders all localized time and durable actions in stable order"),
			CommandActions.Num() == 10
			&& CommandActions[0] == TEXT("5 S")
			&& CommandActions[4] == TEXT("1 HEURE")
			&& CommandActions[5] == TEXT("1 JOUR")
			&& CommandActions[6] == TEXT("ARCHIVES")
			&& CommandActions[7] == TEXT("SAUVER")
			&& CommandActions[8] == TEXT("CHARGER")
			&& CommandActions[9] == TEXT("PARAMÈTRES"));
		FStrategicDashboardSnapshot MandateSnapshot = Snapshot;
		FStrategicRegionView& MandateRegion = MandateSnapshot.Regions[0];
		MandateRegion.Pressure = 20;
		MandateRegion.bHasMandate = true;
		MandateRegion.Support = 55;
		MandateRegion.SupportTier = ERegionalSupportTier::Committed;
		MandateRegion.BaselineMonthlyFunding = 100000;
		MandateRegion.CurrentMonthlyFunding = 100000;
		MandateRegion.ProjectedMonthlyFunding = 110000;
		FStrategicRegionalActionView& CivicRelief = MandateRegion.ActionOptions.AddDefaulted_GetRef();
		CivicRelief.ActionType = ERegionalDiplomacyActionType::CivicRelief;
		CivicRelief.Cost = 120000;
		CivicRelief.SupportDelta = 12;
		CivicRelief.PressureReduction = 4;
		CivicRelief.bEnabled = true;
		FStrategicRegionalActionView& SecurityAccord = MandateRegion.ActionOptions.AddDefaulted_GetRef();
		SecurityAccord.ActionType = ERegionalDiplomacyActionType::SecurityAccord;
		SecurityAccord.Cost = 180000;
		SecurityAccord.SupportDelta = 5;
		SecurityAccord.PressureReduction = 12;
		SecurityAccord.UnavailableReasonCode = TEXT("regional_action_already_used");
		SecurityAccord.UnavailableReason = TEXT("Raw monthly outreach guard.");
		FStrategicRegionalActionView& CrisisMobilization = MandateRegion.ActionOptions.AddDefaulted_GetRef();
		CrisisMobilization.ActionType = ERegionalDiplomacyActionType::CrisisMobilization;
		CrisisMobilization.Cost = 0;
		CrisisMobilization.SupportDelta = -15;
		CrisisMobilization.PressureReduction = 25;
		CrisisMobilization.MinimumPressure = 60;
		CrisisMobilization.bEnabled = true;
		MandateRegion.ResilienceCharter.Cost = 250000;
		MandateRegion.ResilienceCharter.SupportCost = 10;
		MandateRegion.ResilienceCharter.MinimumSupport = 60;
		MandateRegion.ResilienceCharter.FundingPercent = 90;
		MandateRegion.ResilienceCharter.MissionWeightPercent = 50;
		MandateRegion.ResilienceCharter.EscapePressurePercent = 75;
		MandateRegion.ResilienceCharter.ProjectedMonthlyFunding = 90000;
		MandateRegion.ResilienceCharter.MonthlyFundingDelta = -10000;
		MandateRegion.ResilienceCharter.UnavailableReasonCode = TEXT("regional_charter_support_required");
		MandateRegion.ResilienceCharter.UnavailableReason = TEXT("Raw charter support gate.");
		MandateSnapshot.HorizonCompact.Cost = 400000;
		MandateSnapshot.HorizonCompact.RequiredCharters = 2;
		MandateSnapshot.HorizonCompact.SignedCharters = 1;
		MandateSnapshot.HorizonCompact.MinimumMemberSupport = 50;
		MandateSnapshot.HorizonCompact.MemberSupportCost = 5;
		MandateSnapshot.HorizonCompact.FundingPercent = 95;
		MandateSnapshot.HorizonCompact.SharedEscapePressurePercent = 33;
		MandateSnapshot.HorizonCompact.WithdrawalSupportThreshold = 25;
		MandateSnapshot.HorizonCompact.RestorationMinimumSupport = 40;
		MandateSnapshot.HorizonCompact.CurrentMonthlyFunding = 100000;
		MandateSnapshot.HorizonCompact.ProjectedMonthlyFunding = 100000;
		MandateSnapshot.HorizonCompact.UnavailableReasonCode =
			TEXT("coalition_compact_charters_required");
		MandateSnapshot.HorizonCompact.UnavailableReason = TEXT("Raw compact charter gate.");
		Hud->ApplySnapshot(MandateSnapshot);
		TestTrue(TEXT("French Slate renders regional support, all three outreach choices, and exact charter tradeoff"),
			Hud->GetRenderedDynamicLabels().Contains(
				TEXT("RUNTIME STABLE  •  PRESSION 20  •  SOUTIEN 55 ENGAGÉ\nFINANCEMENT +100000 → +110000 / MOIS"))
			&& Hud->GetRenderedCommandActionLabels().Contains(
				TEXT("PACTE HORIZON  •  FONDS 400000 • SIGNÉES 1/2 • SOUTIEN MIN 50\nCHAQUE MEMBRE -5 SOUTIEN • FINANCEMENT 95% • REDIRECTION 33%\nFINANCEMENT TOTAL 100000 → 100000\nLe Pacte Horizon exige davantage de chartes de résilience signées."))
			&& Hud->GetRenderedCommandActionLabels().Contains(
				TEXT("AIDE CIVIQUE  •  FONDS 120000 • SOUTIEN +12 • PRESSION -4"))
			&& Hud->GetRenderedCommandActionLabels().Contains(
				TEXT("ACCORD DE SÉCURITÉ  •  FONDS 180000 • SOUTIEN +5 • PRESSION -12\nCe partenaire régional a déjà bénéficié d'une action ce mois-ci."))
			&& Hud->GetRenderedCommandActionLabels().Contains(
				TEXT("MOBILISATION DE CRISE  •  FONDS 0\nSOUTIEN -15 • PRESSION -25 • SEUIL 60"))
			&& Hud->GetRenderedCommandActionLabels().Contains(
				TEXT("CHARTE DE RÉSILIENCE  •  FONDS 250000 • SOUTIEN -10 • MIN 60\nFINANCEMENT 100000 → 90000 • POIDS MISSION 50% • PRESSION DE FUITE 75%\nCe partenaire régional n'a pas atteint le soutien requis pour une charte de résilience.")));
		FStrategicDashboardSnapshot SignedCharterSnapshot = MandateSnapshot;
		SignedCharterSnapshot.Regions[0].ResilienceCharter.bSigned = true;
		SignedCharterSnapshot.Regions[0].ResilienceCharter.UnavailableReasonCode =
			TEXT("regional_charter_already_signed");
		Hud->ApplySnapshot(SignedCharterSnapshot);
		TestTrue(TEXT("French Slate renders signed charter status without a redundant rejection"),
			Hud->GetRenderedCommandActionLabels().Contains(
				TEXT("CHARTE DE RÉSILIENCE  •  ACTIVE\nFINANCEMENT 90% • POIDS MISSION 50% • PRESSION DE FUITE 75%")));
		FStrategicDashboardSnapshot RatifiedCompactSnapshot = SignedCharterSnapshot;
		RatifiedCompactSnapshot.HorizonCompact.bRatified = true;
		RatifiedCompactSnapshot.HorizonCompact.bEnabled = false;
		RatifiedCompactSnapshot.HorizonCompact.SignedCharters = 2;
		RatifiedCompactSnapshot.HorizonCompact.CurrentMonthlyFunding = 190000;
		RatifiedCompactSnapshot.HorizonCompact.ProjectedMonthlyFunding = 190000;
		RatifiedCompactSnapshot.HorizonCompact.ActiveMemberRegionIds = {
			TEXT("region.runtime-stable"), TEXT("region.runtime-elevated")
		};
		RatifiedCompactSnapshot.HorizonCompact.UnavailableReasonCode =
			TEXT("coalition_compact_already_ratified");
		FStrategicCoalitionAidView& FragileAid =
			RatifiedCompactSnapshot.HorizonCompact.AidOptions.AddDefaulted_GetRef();
		FragileAid.TargetRegionId = TEXT("region.runtime-stable");
		FragileAid.DonorRegionId = TEXT("region.runtime-elevated");
		FragileAid.Cost = 150000;
		FragileAid.TargetCurrentPressure = 90;
		FragileAid.TargetProjectedPressure = 70;
		FragileAid.PressureTransfer = 20;
		FragileAid.TargetSupportGain = 5;
		FragileAid.DonorSupportCost = 5;
		FragileAid.MonthlyFundingDelta = -4;
		FragileAid.bDonorWouldWithdraw = true;
		FragileAid.bEnabled = true;
		Hud->ApplySnapshot(RatifiedCompactSnapshot);
		TestTrue(TEXT("French Slate renders active compact cohesion and warns before aid withdraws its donor"),
			Hud->GetRenderedCommandActionLabels().Contains(
				TEXT("PACTE HORIZON  •  ACTIFS 2 • RETIRÉS 0\nRETRAIT SOUS 25 SOUTIEN • RETOUR À 40\nFINANCEMENT 95% • REDIRECTION 33% • TOTAL 190000/MOIS"))
			&& Hud->GetRenderedCommandActionLabels().Contains(
				TEXT("AIDE RÉCIPROQUE  •  SOULAGER RUNTIME STABLE\nPRESSION 90 → 70 • RUNTIME ELEVATED ACCEPTE +20\nFONDS 150000 • SOUTIEN +5/-5 • FINANCEMENT -4\nATTENTION : CETTE PERTE DE SOUTIEN ENTRAÎNERA LE RETRAIT D'UN MEMBRE")));
		FStrategicDashboardSnapshot CohesionSnapshot = RatifiedCompactSnapshot;
		CohesionSnapshot.HorizonCompact.ActiveMemberRegionIds = {
			TEXT("region.runtime-elevated")
		};
		CohesionSnapshot.HorizonCompact.WithdrawnMemberRegionIds = {
			TEXT("region.runtime-stable")
		};
		CohesionSnapshot.HorizonCompact.AidOptions.Reset();
		CohesionSnapshot.Regions[0].ActionOptions[2].bWouldWithdrawCompactMember = true;
		FStrategicCompactRestorationView& Restoration =
			CohesionSnapshot.Regions[0].HorizonCompactRestoration;
		Restoration.bWithdrawn = true;
		Restoration.Cost = 100000;
		Restoration.CurrentSupport = 30;
		Restoration.MinimumSupport = 40;
		Restoration.CurrentMonthlyFunding = 185000;
		Restoration.ProjectedMonthlyFunding = 185000;
		Restoration.bEnabled = false;
		Restoration.UnavailableReasonCode = TEXT("coalition_restoration_support_required");
		Restoration.UnavailableReason = TEXT("Raw restoration support gate.");
		FStrategicCompactEmergencyVoteView& Vote =
			CohesionSnapshot.Regions[0].HorizonCompactEmergencyVote;
		Vote.bTargetWithdrawn = true;
		Vote.Cost = 200000;
		Vote.TargetCurrentSupport = 30;
		Vote.TargetProjectedSupport = 42;
		Vote.TargetSupportGain = 12;
		Vote.TargetCurrentPressure = 20;
		Vote.TargetProjectedPressure = 5;
		Vote.TargetPressureReduction = 15;
		Vote.VoterSupportCost = 2;
		Vote.MaximumVoterPressure = 70;
		Vote.RequiredVotes = 1;
		Vote.SupportingMemberRegionIds = { TEXT("region.runtime-elevated") };
		Vote.MonthlyFundingDelta = 22500;
		Vote.bEnabled = true;
		Hud->ApplySnapshot(CohesionSnapshot);
		TestTrue(TEXT("French Slate renders withdrawn membership, crisis warning, a negotiated emergency ballot, and restoration gate"),
			Hud->GetRenderedCommandActionLabels().Contains(
				TEXT("PACTE HORIZON  •  ACTIFS 1 • RETIRÉS 1\nRETRAIT SOUS 25 SOUTIEN • RETOUR À 40\nFINANCEMENT 95% • REDIRECTION 33% • TOTAL 190000/MOIS"))
			&& Hud->GetRenderedCommandActionLabels().Contains(
				TEXT("MOBILISATION DE CRISE  •  FONDS 0\nSOUTIEN -15 • PRESSION -25 • SEUIL 60\nATTENTION : CETTE PERTE DE SOUTIEN ENTRAÎNERA LE RETRAIT D'UN MEMBRE"))
			&& Hud->GetRenderedCommandActionLabels().Contains(
				TEXT("VOTE DE SOLIDARITÉ D'URGENCE  •  FONDS 200000 • OUI 1/1\nSOUTIEN 30 → 42 • PRESSION 20 → 5\nPOUR RUNTIME ELEVATED • CONTRE —\nMEMBRES FAVORABLES -2 SOUTIEN • LIMITE 70 PRESSION • FINANCEMENT +22500"))
			&& Hud->GetRenderedCommandActionLabels().Contains(
				TEXT("RÉTABLIR L'ADHÉSION AU PACTE  •  FONDS 100000 • SOUTIEN 30/40\nFINANCEMENT TOTAL 185000 → 185000\nCe membre retiré n'a pas rétabli assez de soutien pour rejoindre le Pacte Horizon.")));
		Hud->ApplySnapshot(Snapshot);
		Hud->ShowStatusMessage(FString());
		const TArray<FString> FacilityChromeLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French Slate localizes the base capacity summary and facility-grid inspection chrome"),
			FacilityChromeLabels.ContainsByPredicate(
				[](const FString& Label)
				{
					return Label.StartsWith(
						TEXT("RUNTIME STATION  //  TEST REACH\nSCI AFFECTÉS 2/6 • EFFECTIF 0/6 • INST +0"));
				})
			&& FacilityChromeLabels.Contains(TEXT("GRILLE DE BASE  8 × 8"))
			&& FacilityChromeLabels.Contains(
				TEXT("CYAN prête  •  AMBRE endommagée/en construction  •  ROUGE hors ligne  •  VIOLET réparation  •  cliquez sur une installation pour la démanteler")));
		TestTrue(TEXT("French Slate localizes the construction-cancellation control around its exact refund"),
			FacilityChromeLabels.Contains(
				TEXT("ANNULER LA CONSTRUCTION  •  REMBOURSEMENT 300")));
		TestEqual(TEXT("French Slate renders exactly one fog-safe adversary-plan intelligence card"),
			Hud->GetRenderedAdversaryPlanIntelligenceCount(), 1);
		TestEqual(TEXT("French Slate renders exactly one fog-safe coalition-counterplay card"),
			Hud->GetRenderedCoalitionCounterplayCount(), 1);
		const FString* RenderedPlanCard = FacilityChromeLabels.FindByPredicate(
			[](const FString& Label)
			{
				return Label.StartsWith(TEXT("PLAN ADVERSE"));
			});
		TestNotNull(TEXT("French Slate exposes the adversary-plan branch card"), RenderedPlanCard);
		if (RenderedPlanCard != nullptr)
		{
			TestEqual(TEXT("French Slate localizes the exact adversary-plan branch card"),
				*RenderedPlanCard,
				FString(TEXT("PLAN ADVERSE  //  SCHÉMA DE PLUIE MIROIR  •  PHASE 1\nEN CAS DE FUITE → RAID DE VERRE NOCTURNE\nSI DÉJOUÉ → INCURSION SAFRAN")));
		}
		TestTrue(TEXT("French Slate exposes exact member-level coalition strain and recovery projections"),
			FacilityChromeLabels.Contains(
				TEXT("CONTRE-MESURE COALITIONNELLE\nEN CAS DE FUITE → RUNTIME ELEVATED SOUTIEN 27→19 • SE RETIRE\nSI DÉJOUÉ → RUNTIME STABLE SOUTIEN 30→40 • RESTE RETIRÉ")));
		TestTrue(TEXT("French Slate exposes the exact wreckage-versus-landing outcome intelligence"),
			FacilityChromeLabels.Contains(
				TEXT("RENSEIGNEMENT SUR LES ISSUES\nDÉTRUIRE → ÉPAVE • MENACE 2 • 3 j 0 h restantes\nSUIVRE JUSQU’À L’ARRIVÉE → ATTERRISSAGE INTACT • MENACE 4 • 36 h restantes\nL’ARRIVÉE APPLIQUE LES CONSÉQUENCES DE LA MISSION")));
		Hud->SelectFacilityForPlacement(RuntimeFacilityId);
		TestTrue(TEXT("French Slate localizes the manual-placement feedback around exact dimensions"),
			Hud->GetRenderedStatusText().StartsWith(TEXT("PLACEMENT DE RUNTIME ANNEX (1×1)"))
			&& Hud->GetRenderedStatusText().Contains(TEXT("grille de la base principale")));
		const TArray<FString> PlacementChromeLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French Slate localizes facility-placement legends and both cancellation controls"),
			PlacementChromeLabels.Contains(
				TEXT("VERT + ancrage valide  •  les cases désactivées ne peuvent contenir ni relier l'empreinte sélectionnée"))
			&& PlacementChromeLabels.Contains(TEXT("ANNULER LE PLACEMENT"))
			&& PlacementChromeLabels.Contains(
				TEXT("ANNULER PLACEMENT  Runtime Annex\nSÉLECTIONNEZ UNE CASE + VERTE SUR LA GRILLE DE BASE")));
		Hud->CancelFacilityPlacement();
		TestEqual(TEXT("French Slate localizes facility-placement cancellation"),
			Hud->GetRenderedStatusText(), FString(TEXT("Placement de l'installation annulé.")));
		Hud->SelectFacilityForDismantle(Base.BaseId, Facility.FacilityInstanceId);
		TestEqual(TEXT("French Slate localizes destructive facility review while preserving exact salvage"),
			Hud->GetRenderedStatusText(),
			FString(TEXT("EXAMEN DU DÉMANTÈLEMENT : RUNTIME HUB rapportera 250 unités de récupération. Sélectionnez CONFIRMER LE DÉMANTÈLEMENT pour continuer.")));
		const TArray<FString> DismantleChromeLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French Slate localizes the two-step facility-dismantling controls around exact salvage"),
			DismantleChromeLabels.Contains(TEXT("DÉMANTELER Runtime Hub  •  RÉCUPÉRATION 250"))
			&& DismantleChromeLabels.Contains(TEXT("CONFIRMER LE DÉMANTÈLEMENT"))
			&& DismantleChromeLabels.Contains(TEXT("CONSERVER L'INSTALLATION")));
		Hud->CancelFacilityDismantle();
		TestEqual(TEXT("French Slate localizes facility-dismantling cancellation"),
			Hud->GetRenderedStatusText(), FString(TEXT("Démantèlement de l'installation annulé.")));
		const TArray<FString> LocalizedContentLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French Slate localizes the Relay Weave queue-pressure horizon"),
			LocalizedContentLabels.Contains(
				TEXT("LIGNE RELAIS 2/3 ACTIVE  •  1 EN ATTENTE  •  PRESSION 34%  •  FIN 18 h")));
		TestTrue(TEXT("French Slate localizes the derived base specialization card"),
			LocalizedContentLabels.Contains(
				TEXT("SPÉCIALISATION DE BASE  •  OPÉRATIONS AÉRIENNES  •  INDICE 100/100  •  POSTES 2")));
		TestTrue(TEXT("French Slate localizes the legacy storage summary"),
			LocalizedContentLabels.Contains(TEXT("STOCKAGE  1 TYPES  •  RÈGLES HÉRITÉES ILLIMITÉES")));
		TestTrue(TEXT("French Slate localizes inventory item names"),
			LocalizedContentLabels.Contains(TEXT("Scanner de terrain  ×3  •  STOCKAGE 0  •  125 L'UNITÉ")));
		TestTrue(TEXT("French Slate localizes personnel roles, availability, and stat chrome without replacing personal names"),
			LocalizedContentLabels.ContainsByPredicate(
				[](const FString& Label)
				{
					return Label.StartsWith(TEXT("Ari West  •  Agent de terrain\nDISPONIBLE  •  BASE RUNTIME STATION"))
						&& Label.Contains(TEXT("RANG 3   PV 48/48"))
						&& Label.Contains(TEXT("PRÉC 61   RÉS 57   MOB 64   FOR 53"));
				}));
		TestTrue(TEXT("French Slate localizes retained citations and authored service-history text"),
			LocalizedContentLabels.Contains(
				TEXT("SERVICE  AGUERRI\nPROCHAIN PALIER LONGUE VEILLE À 10 MISSIONS  •  RESTE 5"))
			&& LocalizedContentLabels.Contains(TEXT("MÉMORIAL  1"))
			&& LocalizedContentLabels.Contains(
				TEXT("Mara Sol  •  Agent de terrain\nRANG 4   MISSIONS 10   ÉLIM. 8   NIV. DOCTRINE 1   CITATIONS 1\nDERNIER SERVICE 2042-04-06 UTC  •  Perte en opération tactique"))
			&& LocalizedContentLabels.Contains(
				TEXT("SERVICE  LONGUE VEILLE\nPROCHAIN PALIER PILIER D’HÉRITAGE À 20 MISSIONS  •  RESTE 10"))
			&& LocalizedContentLabels.Contains(
				TEXT("◆ Étoile de la longue veille — A accompli dix opérations, atteint le rang trois et enregistré huit éliminations."))
			&& LocalizedContentLabels.Contains(TEXT("CITATIONS  1"))
			&& LocalizedContentLabels.Contains(
				TEXT("◆ Citation de première intervention — A mené à bien une première opération de terrain.")));
		TestTrue(TEXT("French Slate localizes open doctrine choices, levels, and exact stat bonuses"),
			LocalizedContentLabels.Contains(TEXT("DOCTRINE DE TERRAIN  •  CHOIX DISPONIBLES : 2"))
			&& LocalizedContentLabels.Contains(
				TEXT("Vision nette  •  NIV 1/3\nPV +0  PRÉ +4  VOL +0  MOB +0  FOR +0"))
			&& LocalizedContentLabels.Contains(
				TEXT("Inébranlable  •  NIV 0/3\nPV +2  PRÉ +0  VOL +4  MOB +0  FOR +0")));
		TestTrue(TEXT("French Slate localizes equipment names in individual loadouts"),
			LocalizedContentLabels.Contains(TEXT("Scanner de terrain  •  ÉQUIPÉS 1  •  STOCKÉS 3")));
		TestTrue(TEXT("French Slate localizes loadout, training, dismissal, and auto-equipment controls"),
			LocalizedContentLabels.Contains(TEXT("ÉQUIPEMENT DE TERRAIN  1/16"))
			&& LocalizedContentLabels.Contains(TEXT("ENT. PRÉC"))
			&& LocalizedContentLabels.Contains(TEXT("RENVOYER"))
			&& LocalizedContentLabels.Contains(TEXT("ÉQUIPER AUTO. L'ÉQUIPE DISPONIBLE")));
		TestTrue(TEXT("French Slate localizes active production details and material-return policy"),
			LocalizedContentLabels.Contains(
				TEXT("PRODUCTION  //  Scanner de terrain\n3 unités • 2 ingénieurs • 8 h restantes • Stockage +3/unité • Entrées/unité : 2 Éclat de résonance • Retour à l'annulation : 4 Éclat de résonance   25%"))
			&& LocalizedContentLabels.Contains(TEXT("2 INGÉNIEURS"))
			&& LocalizedContentLabels.Contains(TEXT("SÉRIE 3  •  250 L'UNITÉ"))
			&& LocalizedContentLabels.Contains(TEXT("ANNULER LA PRODUCTION  •  REMBOURSEMENT 500")));
		TestTrue(TEXT("French Slate localizes manufacturing procurement and aggregate material inputs"),
			LocalizedContentLabels.Contains(TEXT("FABRIQUER ×1  Scanner de terrain  •  250\nFabriquer 1 • 12 heures-ingénieur • Stockage +3/unité • Entrées de la série : 2 Éclat de résonance (stock 6)")));
		TestTrue(TEXT("French Slate localizes the fleet card and crew controls without replacing the personal callsign"),
			LocalizedContentLabels.Contains(TEXT("FLOTTE 1"))
			&& LocalizedContentLabels.Contains(
				TEXT("Runtime Skiff • Transport Héron\nAU SOL   COQUE 100/100   CARBURANT 400/500   ÉQUIPE 1/4   PILOTE KAI NORTH"))
			&& LocalizedContentLabels.Contains(TEXT("AFFECTATION DU PILOTE"))
			&& LocalizedContentLabels.Contains(TEXT("ÉQUIPE DE TERRAIN 1/4"))
			&& LocalizedContentLabels.Contains(TEXT("PRÉPARER L'APPAREIL AUTO.")));
		TestTrue(TEXT("French Slate localizes the inbound craft contract while retaining its persisted display name"),
			LocalizedContentLabels.Contains(
				TEXT("APPAREIL EN TRANSIT  //  Heron Transport 02\nEN TRANSIT • 30 h restantes   25%")));
		TestTrue(TEXT("French Slate localizes craft procurement from stable rule identity and numeric capabilities"),
			LocalizedContentLabels.Contains(
				TEXT("ACQUÉRIR  Intercepteur Moineau  •  120000\nAcquisition • 48 h • coque 120 • équipe 0")));
		TestTrue(TEXT("French Slate localizes global situation values and regional pressure chrome"),
			LocalizedContentLabels.Contains(TEXT("SITUATION MONDIALE"))
			&& LocalizedContentLabels.Contains(
				TEXT("CONTACTS 1   SITES 1   ALERTES DE BASE 0\nMISSIONS : 7 LANCÉES / 3 DÉJOUÉES / 2 ÉCHAPPÉES\nPROCHAINE FENÊTRE 72 h\nFINANCEMENT +1800000  •  DÉPENSES -650000  •  NET +1150000"))
			&& LocalizedContentLabels.Contains(TEXT("RUNTIME CRITICAL  •  PRESSION 90")));
		TestTrue(TEXT("French Slate localizes detected-contact identity, state, route, target, and dispatch control"),
			LocalizedContentLabels.Contains(TEXT("CONTACTS DÉTECTÉS"))
			&& LocalizedContentLabels.Contains(
				TEXT("CONTACT RASE-VAGUE  •  DÉTECTÉ\nMENACE 2   COQUE 70/80   TRAJET 25%\nCIBLE  RUNTIME STATION"))
			&& LocalizedContentLabels.Contains(TEXT("ENVOYER UN INTERCEPTEUR PRÊT")));
		TestTrue(TEXT("French Slate localizes intact-site identity, lifetime, and deployment control"),
			LocalizedContentLabels.Contains(TEXT("SITES TACTIQUES"))
			&& LocalizedContentLabels.Contains(
				TEXT("SITE D’ATTERRISSAGE : CONTACT MOISSONNEUR\nMENACE 4   DURÉE 36 h restantes"))
			&& LocalizedContentLabels.Contains(TEXT("DÉPLOYER UN TRANSPORT PRÊT")));

		FStrategicDashboardSnapshot LocalizedEngagedSnapshot = Snapshot;
		FStrategicContactView& LocalizedEngagedContact = LocalizedEngagedSnapshot.Contacts[0];
		LocalizedEngagedContact.Status = TEXT("Engaged");
		LocalizedEngagedContact.StatusType = EStrategicContactStatus::Engaged;
		LocalizedEngagedContact.InterceptionCraftCount = 2;
		LocalizedEngagedContact.bCanWithdrawInterception = true;
		LocalizedEngagedContact.InterceptionCoordination.bValid = true;
		LocalizedEngagedContact.InterceptionCoordination.bActive = true;
		LocalizedEngagedContact.InterceptionCoordination.PolicyId =
			TEXT("interception.coordination-linked-wing");
		LocalizedEngagedContact.InterceptionCoordination.DisplayName = TEXT("Linked Wing");
		LocalizedEngagedContact.InterceptionCoordination.Summary =
			TEXT("Raw linked-wing coordination guidance.");
		LocalizedEngagedContact.InterceptionCoordination.OnStationCraftCount = 2;
		LocalizedEngagedContact.InterceptionCoordination.SupportingCraftCount = 1;
		LocalizedEngagedContact.InterceptionCoordination.OutgoingAccuracyModifier = 5;
		LocalizedEngagedContact.InterceptionCoordination.IncomingAccuracyModifier = -5;
		LocalizedEngagedContact.InterceptionContactManeuver.bValid = true;
		LocalizedEngagedContact.InterceptionContactManeuver.Maneuver =
			EInterceptionContactManeuver::SignalShear;
		LocalizedEngagedContact.InterceptionContactManeuver.PolicyId =
			TEXT("interception.contact-signal-shear");
		LocalizedEngagedContact.InterceptionContactManeuver.DisplayName = TEXT("Signal Shear");
		LocalizedEngagedContact.InterceptionContactManeuver.Summary =
			TEXT("Raw contact maneuver guidance.");
		LocalizedEngagedContact.InterceptionContactManeuver.CompletedCombatRounds = 2;
		LocalizedEngagedContact.InterceptionContactManeuver.CurrentHull = 70;
		LocalizedEngagedContact.InterceptionContactManeuver.MaximumHull = 80;
		LocalizedEngagedContact.InterceptionContactManeuver.OutgoingAccuracyModifier = -10;
		LocalizedEngagedContact.InterceptionContactManeuver.IncomingAccuracyModifier = -15;
		LocalizedEngagedContact.InterceptionPostures.Reset();
		auto AddInterceptionPosture = [&LocalizedEngagedContact](
			const EInterceptionPosture Posture,
			const TCHAR* PolicyId,
			const TCHAR* DisplayName,
			const TCHAR* Summary,
			const int32 OutgoingModifier,
			const int32 IncomingModifier)
		{
			FStrategicInterceptionPostureView& Option =
				LocalizedEngagedContact.InterceptionPostures.AddDefaulted_GetRef();
			Option.Posture = Posture;
			Option.PolicyId = PolicyId;
			Option.DisplayName = DisplayName;
			Option.Summary = Summary;
			Option.OutgoingAccuracyModifier = OutgoingModifier;
			Option.IncomingAccuracyModifier = IncomingModifier;
		};
		AddInterceptionPosture(
			EInterceptionPosture::StandOffScreen,
			TEXT("interception.stand-off-screen"),
			TEXT("Stand-off Screen"),
			TEXT("Widen separation to reduce both outgoing and incoming accuracy."),
			-20,
			-25);
		AddInterceptionPosture(
			EInterceptionPosture::BalancedVector,
			TEXT("interception.balanced-vector"),
			TEXT("Balanced Vector"),
			TEXT("Hold the current vector with no accuracy modifier."),
			0,
			0);
		AddInterceptionPosture(
			EInterceptionPosture::CloseAssault,
			TEXT("interception.close-assault"),
			TEXT("Close Assault"),
			TEXT("Collapse separation to improve outgoing fire while exposing the formation."),
			20,
			25);
		LocalizedEngagedContact.InterceptionWithdrawals.Reset();
		FStrategicInterceptionWithdrawalView& FormationBreak =
			LocalizedEngagedContact.InterceptionWithdrawals.AddDefaulted_GetRef();
		FormationBreak.Doctrine = EInterceptionWithdrawalDoctrine::FormationBreak;
		FormationBreak.PolicyId = TEXT("interception.withdrawal-formation-break");
		FormationBreak.DisplayName = TEXT("Formation Break");
		FormationBreak.Summary = TEXT("Raw formation withdrawal guidance.");
		FormationBreak.bEnabled = true;
		FormationBreak.OnStationCraftCount = 2;
		FormationBreak.WithdrawingCraftCount = 2;
		FStrategicInterceptionWithdrawalView& EvasiveRelay =
			LocalizedEngagedContact.InterceptionWithdrawals.AddDefaulted_GetRef();
		EvasiveRelay.Doctrine = EInterceptionWithdrawalDoctrine::EvasiveRelay;
		EvasiveRelay.PolicyId = TEXT("interception.withdrawal-evasive-relay");
		EvasiveRelay.DisplayName = TEXT("Evasive Relay");
		EvasiveRelay.Summary = TEXT("Raw relay guidance.");
		EvasiveRelay.bEnabled = true;
		EvasiveRelay.OnStationCraftCount = 2;
		EvasiveRelay.WithdrawingCraftCount = 1;
		EvasiveRelay.RemainingCraftCount = 1;
		EvasiveRelay.PriorityCraftId = FGuid(0x52454c41, 0x59435241, 1, 2);
		EvasiveRelay.PriorityCraftDisplayName = TEXT("Aiguille 02");
		EvasiveRelay.PriorityCraftCurrentHull = 25;
		EvasiveRelay.PriorityCraftMaximumHull = 100;
		FStrategicInterceptionWithdrawalView& WakeSnare =
			LocalizedEngagedContact.InterceptionWithdrawals.AddDefaulted_GetRef();
		WakeSnare.Doctrine = EInterceptionWithdrawalDoctrine::WakeSnare;
		WakeSnare.PolicyId = TEXT("interception.withdrawal-wake-snare");
		WakeSnare.DisplayName = TEXT("Wake Snare");
		WakeSnare.Summary = TEXT("Raw pursuit-delay guidance.");
		WakeSnare.bEnabled = true;
		WakeSnare.OnStationCraftCount = 2;
		WakeSnare.WithdrawingCraftCount = 2;
		WakeSnare.RemainingCraftCount = 0;
		WakeSnare.CompletedCombatRounds = 2;
		WakeSnare.RequiredCombatRounds = 2;
		WakeSnare.ContactRouteDelaySeconds = 1800;
		Hud->ApplySnapshot(LocalizedEngagedSnapshot);
		const TArray<FString> InterceptionPostureLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French Slate exposes linked-wing coordination, Signal Shear, three interception geometries, and three withdrawal doctrines"),
			InterceptionPostureLabels.Contains(TEXT("LIAISON DE FORMATION  //  AUTOMATIQUE"))
			&& InterceptionPostureLabels.Contains(
				TEXT("ESCADRE RELIÉE  •  APPUI 1  •  TIR +5  •  RIPOSTE -5"))
			&& InterceptionPostureLabels.Contains(TEXT("MANŒUVRE DU CONTACT  //  AUTOMATIQUE"))
			&& InterceptionPostureLabels.Contains(
				TEXT("CISAILLEMENT DU SIGNAL  •  TOURS 2  •  TIR -10  •  RIPOSTE -15"))
			&& InterceptionPostureLabels.Contains(TEXT("GÉOMÉTRIE D’ENGAGEMENT  //  CHOISIR UN TOUR"))
			&& InterceptionPostureLabels.Contains(
				TEXT("Les modificateurs de précision s'appliquent avant la limite de sécurité de 5 à 100 %."))
			&& InterceptionPostureLabels.Contains(TEXT("ÉCRAN À DISTANCE\nTIR -20  •  RIPOSTE -25"))
			&& InterceptionPostureLabels.Contains(TEXT("VECTEUR ÉQUILIBRÉ\nTIR +0  •  RIPOSTE +0"))
			&& InterceptionPostureLabels.Contains(TEXT("ASSAUT RAPPROCHÉ\nTIR +20  •  RIPOSTE +25"))
			&& InterceptionPostureLabels.Contains(
				TEXT("DOCTRINE DE RETRAIT  //  CHOISIR UNE COMMANDE"))
			&& InterceptionPostureLabels.Contains(TEXT("ROMPRE LE CONTACT  •  FORMATION 2"))
			&& InterceptionPostureLabels.Contains(
				TEXT("RELAIS D’ÉVASION  •  Aiguille 02  •  COQUE 25/100"))
			&& InterceptionPostureLabels.Contains(
				TEXT("PIÈGE DE SILLAGE  •  TOURS 2/2  •  RETARD 30:00")));
		Hud->ApplySnapshot(Snapshot);

		Hud->SelectGlobeMarker(Snapshot.GlobeMarkers[2]);
		const TArray<FString> ContactMarkerLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French Slate reconstructs selected contact markers from stable snapshot data"),
			ContactMarkerLabels.Contains(
				TEXT("CONTACT  //  Contact Rase-vague  •  DÉTECTÉ • MENACE 2 • COQUE 70/80 • TRAJET 25% • CIBLE RUNTIME STATION  •  +60.000°, +20.000°")));
		Hud->SelectGlobeMarker(Snapshot.GlobeMarkers[3]);
		const TArray<FString> SiteMarkerLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French Slate reconstructs selected intact-site markers from stable snapshot data"),
			SiteMarkerLabels.Contains(
				TEXT("SITE  //  Site d’atterrissage : Contact Moissonneur  •  MENACE 4 • 36 h restantes  •  +90.000°, +30.000°")));

		FStrategicDashboardSnapshot LocalizedAssaultSnapshot = Snapshot;
		FStrategicBaseAssaultView& LocalizedAssault =
			LocalizedAssaultSnapshot.BaseAssaults.AddDefaulted_GetRef();
		LocalizedAssault.AssaultId = FGuid(201, 202, 203, 204);
		LocalizedAssault.MissionRuleId = TEXT("mission.nightglass-raid");
		LocalizedAssault.MissionName = TEXT("Nightglass Raid");
		LocalizedAssault.ContactRuleId = TEXT("contact.skimmer");
		LocalizedAssault.ContactName = TEXT("Skimmer Contact");
		LocalizedAssault.BaseId = Base.BaseId;
		LocalizedAssault.BaseName = Base.Name;
		LocalizedAssault.ThreatRating = 4;
		LocalizedAssault.ContactHull = 170;
		LocalizedAssault.DefenseBatteryCount = 2;
		LocalizedAssault.ReadyDefenseBatteryCount = 2;
		LocalizedAssault.MaximumDefenseDamage = 155;
		LocalizedAssault.ExpectedDefenseDamage = 124;
		LocalizedAssault.BreachDamagePerFacility = 35;
		LocalizedAssault.MaximumFacilitiesHit = 2;
		LocalizedAssault.DefenderCount = 3;
		LocalizedAssault.bCanResolve = true;
		LocalizedAssault.bCanDeployTacticalDefense = true;
		Hud->ApplySnapshot(LocalizedAssaultSnapshot);
		const TArray<FString> BaseDefenseLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French Slate localizes the complete base-defense alert around exact readiness values"),
			BaseDefenseLabels.Contains(TEXT("ALERTE DE DÉFENSE DE BASE"))
			&& BaseDefenseLabels.Contains(
				TEXT("RUNTIME STATION  //  RAID DE VERRE NOCTURNE\nCONTACT RASE-VAGUE  •  MENACE 4  •  COQUE 170\n2 BATTERIES  •  JUSQU'À 155 DÉGÂTS  •  ~124 ATTENDUS\nÉQUIPE AU SOL PRÊTE  •  3 DÉFENSEURS\nRISQUE DE BRÈCHE  35 DÉGÂTS × JUSQU'À 2 INSTALLATIONS"))
			&& BaseDefenseLabels.Contains(TEXT("DÉPLOYER LA DÉFENSE AU SOL"))
			&& BaseDefenseLabels.Contains(TEXT("TIRER AVEC LES BATTERIES"))
			&& BaseDefenseLabels.Contains(
				TEXT("Engagez tous les agents de terrain disponibles et non affectés de cette base, puis prenez le commandement tactique."))
			&& BaseDefenseLabels.ContainsByPredicate(
				[](const FString& Label)
				{
					return Label.StartsWith(TEXT("Faites tirer une fois chaque batterie approvisionnée, en consommant sa charge définie."));
				}));

		FStrategicDashboardSnapshot DoctrineAssaultSnapshot = LocalizedAssaultSnapshot;
		FStrategicBaseAssaultView& DoctrineAssault = DoctrineAssaultSnapshot.BaseAssaults[0];
		auto AddDoctrine = [&DoctrineAssault](
			const EBaseDefenseFireDoctrine Doctrine,
			const FName PolicyId,
			const FString& DisplayName,
			const FString& Summary,
			const int32 Ready,
			const int32 Maximum,
			const int32 Expected,
			const int32 Allocated)
		{
			FStrategicBaseDefenseDoctrineView& Option = DoctrineAssault.FireDoctrines.AddDefaulted_GetRef();
			Option.Doctrine = Doctrine;
			Option.PolicyId = PolicyId;
			Option.DisplayName = DisplayName;
			Option.Summary = Summary;
			Option.bCanResolve = true;
			Option.DefenseBatteryCount = 3;
			Option.ReadyDefenseBatteryCount = Ready;
			Option.MaximumDefenseDamage = Maximum;
			Option.ExpectedDefenseDamage = Expected;
			FStrategicBaseDefenseSupplyView& Supply = Option.DefenseSupplies.AddDefaulted_GetRef();
			Supply.ItemId = TEXT("item.perimeter-capacitor");
			Supply.DisplayName = TEXT("Perimeter Capacitor Bank");
			Supply.RequiredQuantity = 7;
			Supply.AvailableQuantity = 4;
			Supply.AllocatedQuantity = Allocated;
		};
		AddDoctrine(EBaseDefenseFireDoctrine::CoordinatedLine,
			TEXT("base-defense.coordinated-line"), TEXT("Coordinated Line"),
			TEXT("Allocate supply and fire in stable battery identity order."), 2, 155, 124, 3);
		AddDoctrine(EBaseDefenseFireDoctrine::PrecisionScreen,
			TEXT("base-defense.precision-screen"), TEXT("Precision Screen"),
			TEXT("Prioritize higher-accuracy batteries, then higher damage, with stable identity ties."),
			2, 155, 124, 3);
		AddDoctrine(EBaseDefenseFireDoctrine::BreachBreaker,
			TEXT("base-defense.breach-breaker"), TEXT("Breach Breaker"),
			TEXT("Prioritize higher-damage batteries, then higher accuracy, with stable identity ties."),
			1, 180, 83, 4);
		AddDoctrine(EBaseDefenseFireDoctrine::GridOvercharge,
			TEXT("base-defense.grid-overcharge"), TEXT("Grid Overcharge"),
			TEXT("Prioritize higher-damage batteries, add 15 accuracy, scale damage to 125%, and commit threat-indexed emergency funds."),
			2, 194, 171, 3);
		DoctrineAssault.FireDoctrines.Last().FundingCost = 100000;
		DoctrineAssault.FireDoctrines.Last().bAffordable = true;
		DoctrineAssault.FireDoctrines.Last().AccuracyBonus = 15;
		DoctrineAssault.FireDoctrines.Last().DamagePercent = 125;
		Hud->ApplySnapshot(DoctrineAssaultSnapshot);
		const TArray<FString> DoctrineLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French Slate renders all four doctrine commands with exact allocation and economy previews"),
			DoctrineLabels.Contains(TEXT("DOCTRINE DE TIR  //  CHOISIR UNE SALVE"))
			&& DoctrineLabels.Contains(
				TEXT("Les charges sont attribuées dans l'ordre de la doctrine ; les batteries cessent le feu dès que le contact est détruit."))
			&& DoctrineLabels.Contains(
				TEXT("LIGNE COORDONNÉE\nPRÊTES 2/3  •  MAX 155  •  ~124"))
			&& DoctrineLabels.Contains(
				TEXT("ÉCRAN DE PRÉCISION\nPRÊTES 2/3  •  MAX 155  •  ~124"))
			&& DoctrineLabels.Contains(
				TEXT("BRISE-BRÈCHE\nPRÊTES 1/3  •  MAX 180  •  ~83"))
			&& DoctrineLabels.Contains(
				TEXT("SURCHARGE DU RÉSEAU\nPRÊTES 2/3  •  MAX 194  •  ~171\nCOÛT D'URGENCE DU RÉSEAU 100000"))
			&& DoctrineLabels.ContainsByPredicate(
				[](const FString& Label)
				{
					return Label.StartsWith(
						TEXT("Privilégiez les batteries infligeant le plus de dégâts, puis la précision"));
				})
			&& DoctrineLabels.ContainsByPredicate(
				[](const FString& Label)
				{
					return Label.StartsWith(
						TEXT("Privilégiez les batteries les plus puissantes, ajoutez 15 en précision"));
				}));

		FStrategicDashboardSnapshot UnavailableAssaultSnapshot = LocalizedAssaultSnapshot;
		FStrategicBaseAssaultView& UnavailableAssault = UnavailableAssaultSnapshot.BaseAssaults[0];
		UnavailableAssault.bCanResolve = false;
		UnavailableAssault.UnavailableReasonCode = TEXT("base_assault_in_tactical_operation");
		UnavailableAssault.UnavailableReason = TEXT("Raw automatic-defense rejection.");
		UnavailableAssault.bCanDeployTacticalDefense = false;
		UnavailableAssault.TacticalUnavailableReasonCode = TEXT("no_base_defenders");
		UnavailableAssault.TacticalUnavailableReason = TEXT("Raw ground-defense rejection.");
		UnavailableAssault.DefenderCount = 0;
		Hud->ApplySnapshot(UnavailableAssaultSnapshot);
		const TArray<FString> UnavailableBaseDefenseLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French Slate resolves retained base-defense rejection codes instead of raw backend text"),
			UnavailableBaseDefenseLabels.Contains(
				TEXT("Aucun agent de terrain disponible et non affecté n'est stationné dans la base menacée."))
			&& UnavailableBaseDefenseLabels.Contains(
				TEXT("Cet assaut est déjà en cours de résolution sur le champ de bataille tactique."))
			&& !UnavailableBaseDefenseLabels.Contains(TEXT("Raw automatic-defense rejection."))
			&& !UnavailableBaseDefenseLabels.Contains(TEXT("Raw ground-defense rejection.")));

		FStrategicDashboardSnapshot LocalizedResearchSnapshot = Snapshot;
		LocalizedResearchSnapshot.Projects.Reset();
		LocalizedResearchSnapshot.ActionOptions.Reset();
		FStrategicBaseView& LocalizedResearchBase = LocalizedResearchSnapshot.Bases[0];
		LocalizedResearchBase.AssignedScientists = 5;
		FStrategicFacilityView& LocalizedOperationsHub = LocalizedResearchBase.FacilityLayout[0];
		LocalizedOperationsHub.FacilityId = TEXT("facility.operations-hub");
		LocalizedOperationsHub.DisplayName = TEXT("Operations Hub");
		LocalizedOperationsHub.bOperational = false;
		LocalizedOperationsHub.CurrentIntegrity = 0;
		LocalizedOperationsHub.MaxIntegrity = 400;
		LocalizedOperationsHub.Damage = 400;
		LocalizedOperationsHub.EffectivenessPercent = 0;
		LocalizedOperationsHub.bCanRepair = true;
		LocalizedOperationsHub.RepairCost = 240000;
		LocalizedOperationsHub.RepairDurationSeconds = 400 * 3600LL;
		FStrategicProjectView& LocalizedResearchProject =
			LocalizedResearchSnapshot.Projects.AddDefaulted_GetRef();
		LocalizedResearchProject.Type = EStrategicProjectType::Research;
		LocalizedResearchProject.BaseId = LocalizedResearchBase.BaseId;
		LocalizedResearchProject.RuleId = TEXT("research.signal-analysis");
		LocalizedResearchProject.DisplayName = TEXT("Anomalous Signal Analysis");
		LocalizedResearchProject.RequiredFacilityIds.Add(TEXT("facility.operations-hub"));
		LocalizedResearchProject.RequiredFacilityNames.Add(TEXT("Operations Hub"));
		LocalizedResearchProject.MissingFacilityIds.Add(TEXT("facility.operations-hub"));
		LocalizedResearchProject.MissingFacilityNames.Add(TEXT("Operations Hub"));
		LocalizedResearchProject.AssignedStaff = 5;
		LocalizedResearchProject.bPaused = true;
		FStrategicActionOptionView& LocalizedResearchOption =
			LocalizedResearchSnapshot.ActionOptions.AddDefaulted_GetRef();
		LocalizedResearchOption.Type = EStrategicActionOptionType::Research;
		LocalizedResearchOption.RuleId = TEXT("research.directed-energy");
		LocalizedResearchOption.DisplayName = TEXT("Directed Energy Control");
		LocalizedResearchOption.DurationHours = 360;
		LocalizedResearchOption.RequiredFacilityIds.Add(TEXT("facility.energy-lab"));
		LocalizedResearchOption.RequiredFacilityNames.Add(TEXT("Energy Laboratory"));
		LocalizedResearchOption.MissingFacilityIds.Add(TEXT("facility.energy-lab"));
		LocalizedResearchOption.MissingFacilityNames.Add(TEXT("Energy Laboratory"));
		LocalizedResearchOption.bUnlocked = true;
		LocalizedResearchOption.bAffordable = true;
		LocalizedResearchOption.bAvailable = false;
		LocalizedResearchOption.UnavailableReasonCode = TEXT("research_facility_missing");
		LocalizedResearchOption.UnavailableReason =
			TEXT("The primary base requires operational facilities: Energy Laboratory.");
		Hud->ApplySnapshot(LocalizedResearchSnapshot);
		Hud->ShowStatusMessage(FString());
		const TArray<FString> DynamicLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French Slate localizes the active-program count"),
			DynamicLabels.Contains(TEXT("PROGRAMMES ACTIFS 1")));
		TestTrue(TEXT("French Slate localizes the authored project and outage detail"),
			DynamicLabels.Contains(TEXT("RECHERCHE  //  Analyse des signaux anormaux\nLABORATOIRE HORS LIGNE • Centre des opérations • 5 scientifiques   0%")));
		TestTrue(TEXT("French Slate localizes the authored facility and integrity state"),
			DynamicLabels.Contains(TEXT("Centre des opérations  //  HORS LIGNE  //  INTÉGRITÉ 0/400  //  RENDEMENT 0%")));
		TestTrue(TEXT("French Slate localizes the exact facility restoration values"),
			DynamicLabels.Contains(TEXT("RÉTABLIR 400 INTÉGRITÉ  •  240000 FONDS  •  400 h")));
		TestTrue(TEXT("French Slate localizes research staffing"),
			DynamicLabels.Contains(TEXT("5 SCIENTIFIQUES")));
		TestTrue(TEXT("French Slate localizes research cancellation"),
			DynamicLabels.Contains(TEXT("ANNULER LA RECHERCHE")));
		TestTrue(TEXT("French Slate localizes the programs and procurement heading"),
			DynamicLabels.Contains(TEXT("PROGRAMMES + APPROVISIONNEMENT")));
		TestTrue(TEXT("French Slate localizes research option names and structured facility diagnostics"),
			DynamicLabels.Contains(TEXT("RECHERCHE  Contrôle de l'énergie dirigée\nLa base principale requiert des installations opérationnelles : Laboratoire énergétique.")));
		Hud->ApplySnapshot(Snapshot);
		Hud->ShowStatusMessage(FString());

		Hud->ShowKnowledgeArchive();
		TestTrue(TEXT("Native strategic HUD enters the research-aware archive flow"),
			Hud->IsShowingKnowledgeArchive());
		TestEqual(TEXT("Archive browser renders its localized French title"),
			Hud->GetRenderedTitleText(), FString(TEXT("UEGT  //  ARCHIVES DU SIGNAL")));
		TestTrue(TEXT("Archive browser localizes visibility counts and redaction guidance"),
			Hud->GetRenderedSubtitleText() == TEXT("2 DOSSIERS DÉVERROUILLÉS  •  1 CLASSIFIÉS")
			&& Hud->GetRenderedStatusText().StartsWith(TEXT("Parcourez les dossiers autorisés")));
		const TArray<FString> ArchiveLabels = Hud->GetRenderedArchiveLabels();
		TestTrue(TEXT("Archive browser localizes navigation and every field of an authorized authored record"),
			ArchiveLabels.Contains(TEXT("CATÉGORIES"))
			&& ArchiveLabels.Contains(TEXT("TOUS LES DOSSIERS"))
			&& ArchiveLabels.Contains(TEXT("COMMANDEMENT"))
			&& ArchiveLabels.Contains(TEXT("INDEX DES DOSSIERS"))
			&& ArchiveLabels.Contains(TEXT("Charte du Front du signal"))
			&& ArchiveLabels.Contains(TEXT("Mandat, limites et doctrine opérationnelle du réseau de défense distribué."))
			&& ArchiveLabels.Contains(TEXT("L'UEGT existe pour maintenir le lien entre les gouvernements régionaux lorsque des incursions anormales désorganisent le commandement ordinaire. Elle coordonne les preuves, la logistique et l'action défensive sans se substituer à l'autorité civile.\n\nChaque opération obéit à trois contraintes : préserver la continuité civile, consigner la chaîne de preuves et exposer l'incertitude au lieu de la dissimuler sous une certitude de façade. Les commandants doivent gagner la confiance avec autant de soin qu'ils remportent les batailles."))
			&& ArchiveLabels.Contains(TEXT("DOSSIER AUTORISÉ PAR LA RECHERCHE"))
			&& ArchiveLabels.Contains(TEXT("DOSSIERS LIÉS"))
			&& !ArchiveLabels.Contains(TEXT("archive.classified-fixture")));
		const TArray<FString> ArchiveActions = Hud->GetRenderedCommandActionLabels();
		TestTrue(TEXT("Archive browser renders its localized command return action"),
			ArchiveActions.Num() == 1
			&& ArchiveActions[0] == TEXT("RETOUR AU COMMANDEMENT"));
		Hud->SetKnowledgeArchiveSearchText(TEXT("périmètre"));
		const TArray<FString> FilteredArchiveLabels = Hud->GetRenderedArchiveLabels();
		TestTrue(TEXT("Archive search filters localized titles, summaries, and bodies without changing classified counts"),
			Hud->GetRenderedSubtitleText() == TEXT("2 DOSSIERS DÉVERROUILLÉS  •  1 CLASSIFIÉS")
			&& FilteredArchiveLabels.Contains(TEXT("1 SUR 2 DOSSIERS"))
			&& FilteredArchiveLabels.Contains(TEXT("Doctrine de défense du périmètre"))
			&& Hud->GetSelectedKnowledgeArchiveRecordId() == OperationsArchiveEntry.EntryId);
		Hud->SetKnowledgeArchiveSearchText(TEXT("no-authorized-record-matches"));
		TestTrue(TEXT("Archive search exposes a localized deterministic empty result"),
			Hud->GetRenderedArchiveLabels().Contains(
				TEXT("Aucun dossier autorisé ne correspond à la catégorie et à la recherche actuelles.")));
		Hud->SetKnowledgeArchiveSearchText(FString());
		Hud->SetKnowledgeArchiveCategoryFilter(OperationsArchiveEntry.CategoryId);
		const TArray<FString> OperationsArchiveLabels = Hud->GetRenderedArchiveLabels();
		TestTrue(TEXT("Archive category navigation narrows the localized record index"),
			OperationsArchiveLabels.Contains(TEXT("1 SUR 1 DOSSIERS")));
		TestTrue(TEXT("Archive category navigation renders the localized category"),
			OperationsArchiveLabels.Contains(TEXT("OPÉRATIONS")));
		TestTrue(TEXT("Archive category navigation renders the localized record title"),
			OperationsArchiveLabels.Contains(TEXT("Doctrine de défense du périmètre")));
		TestEqual(TEXT("Archive category navigation selects the only localized record"),
			Hud->GetSelectedKnowledgeArchiveRecordId(), OperationsArchiveEntry.EntryId);
		Hud->SelectKnowledgeArchiveRecord(CommandArchiveEntry.EntryId);
		TestTrue(TEXT("Related-record navigation can reveal any authorized record without leaking locked data"),
			Hud->GetSelectedKnowledgeArchiveRecordId() == CommandArchiveEntry.EntryId
			&& Hud->GetRenderedArchiveLabels().Contains(TEXT("Charte du Front du signal")));
		Hud->CloseKnowledgeArchive();
		TestFalse(TEXT("Archive browser returns to command without mutating the dashboard"),
			Hud->IsShowingKnowledgeArchive());

		Hud->ShowSaveBrowser(false);
		TestEqual(TEXT("Load browser renders its localized title"),
			Hud->GetRenderedTitleText(), FString(TEXT("UEGT  //  CHARGER LA CAMPAGNE")));
		TestTrue(TEXT("Load browser renders localized verified-slot guidance"),
			Hud->GetRenderedSubtitleText().StartsWith(TEXT("EMPLACEMENTS VÉRIFIÉS"))
			&& Hud->GetRenderedStatusText().StartsWith(TEXT("Choisissez un emplacement de campagne vérifié.")));
		const TArray<FString> LoadBrowserLabels = Hud->GetRenderedSaveBrowserLabels();
		TestTrue(TEXT("Load browser renders localized slot and integrity panels"),
			LoadBrowserLabels.Contains(TEXT("EMPLACEMENTS DE CAMPAGNE"))
			&& LoadBrowserLabels.Contains(TEXT("INTÉGRITÉ DES SAUVEGARDES"))
			&& LoadBrowserLabels.Contains(TEXT("GARANTIES D'INTÉGRITÉ")));
		const TArray<FString> LoadBrowserActions = Hud->GetRenderedCommandActionLabels();
		TestTrue(TEXT("Load browser renders its localized return action"),
			LoadBrowserActions.Num() == 1
			&& LoadBrowserActions[0] == TEXT("RETOUR AU COMMANDEMENT"));

		Hud->ShowSaveBrowser(true);
		TestEqual(TEXT("Save browser renders its localized title"),
			Hud->GetRenderedTitleText(), FString(TEXT("UEGT  //  SAUVER LA CAMPAGNE")));
		TestTrue(TEXT("Save browser renders localized overwrite guidance"),
			Hud->GetRenderedStatusText().StartsWith(TEXT("Choisissez un nouveau nom")));
		const TArray<FString> SaveBrowserLabels = Hud->GetRenderedSaveBrowserLabels();
		TestTrue(TEXT("Save browser renders localized naming and integrity panels"),
			SaveBrowserLabels.Contains(TEXT("CRÉER UN EMPLACEMENT NOMMÉ"))
			&& SaveBrowserLabels.Contains(TEXT("SAUVER DANS L'EMPLACEMENT NOMMÉ"))
			&& SaveBrowserLabels.Contains(TEXT("GARANTIES D'INTÉGRITÉ")));

		Hud->ShowControlSettings();
		TestEqual(TEXT("Keyboard-remapping page renders its localized title"),
			Hud->GetRenderedTitleText(), FString(TEXT("UEGT  //  COMMANDES CLAVIER")));
		TestEqual(TEXT("Keyboard-remapping page renders its localized immediate-apply contract"),
			Hud->GetRenderedSubtitleText(),
			FString(TEXT("RÉAFFECTATION PERSISTANTE SANS CONFLIT  •  APPLICATION IMMÉDIATE")));
		TestTrue(TEXT("Keyboard-remapping page renders localized conflict-safe guidance"),
			Hud->GetRenderedStatusText().StartsWith(
				TEXT("Sélectionnez une action pour faire défiler ses touches.")));
		const TArray<FString> ControlSettingsLabels = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French Slate localizes keyboard-remapping sections, commands, and durable actions"),
			ControlSettingsLabels.Contains(TEXT("ACTIONS TACTIQUES"))
			&& ControlSettingsLabels.Contains(TEXT("CIBLAGE + ÉQUIPEMENT"))
			&& ControlSettingsLabels.Contains(TEXT("RÉTABLIR LES TOUCHES PAR DÉFAUT"))
			&& ControlSettingsLabels.Contains(TEXT("RETOUR AU COMMANDEMENT"))
			&& ControlSettingsLabels.ContainsByPredicate(
				[](const FString& Label) { return Label.StartsWith(TEXT("CONFIRMER / CONTINUER  //")); })
			&& ControlSettingsLabels.ContainsByPredicate(
				[](const FString& Label) { return Label.StartsWith(TEXT("FIN DU TOUR  //")); })
			&& ControlSettingsLabels.ContainsByPredicate(
				[](const FString& Label) { return Label.StartsWith(TEXT("CHANGER LE MODE DE TIR  //")); }));
		Hud->ShowMainMenu(true, TEXT("test package ready"));
		const TArray<FString> FrenchDifficultyProfiles = Hud->GetRenderedDynamicLabels();
		TestEqual(TEXT("New-campaign menu renders every difficulty policy profile"),
			Hud->GetRenderedDifficultyProfileCount(), 4);
		TestEqual(TEXT("New-campaign menu renders exactly one pre-campaign content reload control"),
			Hud->GetRenderedContentReloadControlCount(), 1);
		TestTrue(TEXT("French Slate explains and labels deterministic user-mod reload"),
			FrenchDifficultyProfiles.Contains(TEXT("RECHARGER CONTENU + MODS"))
			&& FrenchDifficultyProfiles.Contains(
				TEXT("Les paquets utilisateur sont chargés avant le début d'une campagne (depuis Saved/Mods par défaut). Le rechargement est désactivé pendant une campagne active.")));
		TestTrue(TEXT("French Slate exposes exact mission-gap and escape-impact policy for every difficulty"),
			FrenchDifficultyProfiles.Contains(
				TEXT("CADET\nINTERVALLE DES MISSIONS 125% • IMPACT DES FUITES 75%"))
			&& FrenchDifficultyProfiles.Contains(
				TEXT("STANDARD\nINTERVALLE DES MISSIONS 100% • IMPACT DES FUITES 100%"))
			&& FrenchDifficultyProfiles.Contains(
				TEXT("VÉTÉRAN\nINTERVALLE DES MISSIONS 85% • IMPACT DES FUITES 125%"))
			&& FrenchDifficultyProfiles.Contains(
				TEXT("ÉLITE\nINTERVALLE DES MISSIONS 70% • IMPACT DES FUITES 150%")));
		Hud->ShowSettings();
		const TArray<FString> FrenchAudioSettings = Hud->GetRenderedDynamicLabels();
		TestTrue(TEXT("French settings visibly identify and preview the original procedural mix"),
			FrenchAudioSettings.Contains(TEXT("MIXAGE PROCÉDURAL ORIGINAL"))
			&& FrenchAudioSettings.Contains(TEXT("ÉCOUTER LE SIGNAL AUDIO")));
		TestTrue(TEXT("Strategic chrome locale test restores the original Unreal culture"),
			FInternationalization::Get().SetCurrentLanguageAndLocale(OriginalCulture));

		Hud->ShowMainMenu(true, TEXT("test package ready"));
		TestTrue(TEXT("New-campaign menu defaults to the accessible Cadet profile"),
			Hud->GetSelectedDifficulty() == ECampaignDifficulty::Cadet);
		TestEqual(TEXT("New-campaign menu retains all difficulty profiles after culture switching"),
			Hud->GetRenderedDifficultyProfileCount(), 4);
		TestEqual(TEXT("New-campaign menu renders every accessibility preset choice"),
			Hud->GetRenderedAccessibilityPresetOptionCount(), 3);
		TestEqual(TEXT("New-campaign menu defaults to the balanced funding mandate"),
			Hud->GetSelectedFundingModel(), EUEGTFundingModel::BalancedMandate);
		TestEqual(TEXT("New-campaign menu renders every funding mandate choice"),
			Hud->GetRenderedFundingModelOptionCount(), 3);
		Hud->ShowSettings();
		TestTrue(TEXT("Native strategic HUD exposes its persistent settings screen"), Hud->IsShowingSettings());
		Hud->ShowControlSettings();
		TestTrue(TEXT("Native settings expose a dedicated keyboard-remapping page"),
			Hud->IsShowingControlSettings());
		Hud->ShowGameplaySettings();
		TestTrue(TEXT("Native settings expose a dedicated gameplay-preferences page"),
			Hud->IsShowingGameplaySettings());
		TestFalse(TEXT("Gameplay preferences are distinct from keyboard remapping"),
			Hud->IsShowingControlSettings());
		Hud->ShowSettings();
		TestFalse(TEXT("Returning to general settings leaves remapping mode"),
			Hud->IsShowingControlSettings());
		TestFalse(TEXT("Returning to general settings leaves gameplay-preference mode"),
			Hud->IsShowingGameplaySettings());
		Hud->ShowMainMenu(true, TEXT("test package ready"));
		Hud->ShowSaveBrowser(false);
		TestTrue(TEXT("Native strategic HUD exposes its validated campaign-slot browser"), Hud->IsShowingSaveBrowser());
		Hud->CloseSaveBrowser();
		TestFalse(TEXT("Campaign-slot browser returns to command without mutating the campaign"), Hud->IsShowingSaveBrowser());
	}

	Globe->ApplyAccessibilityPalette(EUEGTColorVisionMode::Protanopia, false);
	TestEqual(TEXT("Strategic globe accepts the selected color-vision palette"),
		Globe->GetColorVisionMode(), EUEGTColorVisionMode::Protanopia);
	TestFalse(TEXT("Strategic globe accepts the softer marker contrast setting"),
		Globe->IsHighContrastPaletteEnabled());
	TestEqual(TEXT("Accessibility palette changes preserve the geometric day-night boundary"),
		Globe->GetRenderedTerminatorPointCount(), 96);
	Camera->FocusGlobe(Globe->GetGlobeRadius());
	TestTrue(TEXT("Globe focus leaves the full sphere framed behind strategic side panels"),
		Camera->GetZoomDistance() >= 3000.0f
		&& Camera->GetZoomDistance() <= 3600.0f);
	Camera->OrbitGlobe(20.0f, 1000.0f);
	TestEqual(TEXT("Globe orbit clamps above the horizon"), Camera->GetCameraPitch(), -4.0f);
	Camera->OrbitGlobe(0.0f, -1000.0f);
	TestEqual(TEXT("Globe orbit clamps below the south viewing limit"), Camera->GetCameraPitch(), -78.0f);
	Camera->ApplyAccessibilitySettings(true, 1.5f);
	TestFalse(TEXT("Reduced motion disables camera lag"), Camera->IsCameraLagEnabled());
	TestEqual(TEXT("Camera-speed preference scales pan speed"), Camera->GetCameraPanSpeed(), 1350.0f);

	Globe->ClearGlobe();
	TestEqual(TEXT("Globe clears generated marker instances atomically"), Globe->GetRenderedBaseCount(), 0);
	TestEqual(TEXT("Globe clears generated route instances atomically"), Globe->GetRenderedRoutePointCount(), 0);
	TestEqual(TEXT("Clearing transient contacts preserves the persistent day-night reference boundary"),
		Globe->GetRenderedTerminatorPointCount(), 96);
	TestTrue(TEXT("Temporary strategic world shuts down cleanly"), TestWorld.DestroyTestWorld(false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTStrategicControllerCampaignShellTest,
	"UEGT.Core.Game.StrategicControllerCampaignShell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTStrategicControllerCampaignShellTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper TestWorld;
	TestTrue(TEXT("Temporary campaign-shell world is available"), TestWorld.CreateTestWorld(EWorldType::Game));
	UWorld* World = TestWorld.GetTestWorld();
	if (World == nullptr || GEngine == nullptr)
	{
		return false;
	}
	if (UGameInstance* OriginalInstance = World->GetGameInstance())
	{
		OriginalInstance->Shutdown();
	}
	UUEGTGameInstance* GameInstance = NewObject<UUEGTGameInstance>(GEngine);
	World->SetGameInstance(GameInstance);
	GEngine->GetWorldContextFromWorldChecked(World).OwningGameInstance = GameInstance;
	GameInstance->Init();
	TestTrue(TEXT("Campaign shell test loads packaged content"), GameInstance->IsContentReady());

	AUEGTTacticalPlayerController* Controller = World->SpawnActor<AUEGTTacticalPlayerController>();
	TestNotNull(TEXT("Unified strategic/tactical controller spawns"), Controller);
	if (Controller == nullptr)
	{
		TestWorld.DestroyTestWorld(false);
		return false;
	}
	Controller->StartStrategicCampaign(
		ECampaignDifficulty::Cadet, 998877, EUEGTFundingModel::BalancedMandate);
	TestTrue(TEXT("Native campaign action starts a deterministic campaign"), GameInstance->HasActiveCampaign());
	TestEqual(TEXT("Native campaign action preserves selected seed"),
		GameInstance->GetCampaignState().SimulationRandom.InitialSeed, int64(998877));
	TestTrue(TEXT("Controller exposes the founding dashboard after campaign creation"),
		Controller->GetCurrentStrategicSnapshot().bRequiresBase);

	Controller->EstablishStarterBase(TEXT("region.cascadia"));
	const FCampaignState Founded = GameInstance->GetCampaignState();
	TestEqual(TEXT("Founding action establishes one base"), Founded.Bases.Num(), 1);
	TestTrue(TEXT("Founding action installs the original starter operations hub"),
		Founded.Bases.Num() == 1 && Founded.Bases[0].Facilities.ContainsByPredicate(
			[](const FBaseFacilityState& Facility)
			{
				return Facility.FacilityId == FName(TEXT("facility.operations-hub"));
			}));
	TestTrue(TEXT("Strategic controller refreshes into command mode"),
		Controller->GetCurrentStrategicSnapshot().bSucceeded
		&& !Controller->GetCurrentStrategicSnapshot().bRequiresBase);
	TestTrue(TEXT("Founding dashboard exposes the positioned starter facility"),
		Controller->GetCurrentStrategicSnapshot().Bases.Num() == 1
		&& Controller->GetCurrentStrategicSnapshot().Bases[0].GridWidth == 8
		&& Controller->GetCurrentStrategicSnapshot().Bases[0].FacilityLayout.Num() == 1
		&& Controller->GetCurrentStrategicSnapshot().Bases[0].FacilityLayout[0].bOperational);
	Controller->DismantleStrategicFacility(
		Controller->GetCurrentStrategicSnapshot().PrimaryBaseId,
		Controller->GetCurrentStrategicSnapshot().Bases[0].FacilityLayout[0].FacilityInstanceId);
	TestEqual(TEXT("Native dismantling control honors protected facility read-model guards"),
		GameInstance->GetCampaignState().Bases[0].Facilities.Num(), 1);

	Controller->ExecuteStrategicOption(EStrategicActionOptionType::Research, TEXT("research.signal-analysis"));
	TestTrue(TEXT("Command-ready research receives bounded automatic staffing"),
		GameInstance->GetCampaignState().ResearchProjects.Num() == 1
		&& GameInstance->GetCampaignState().ResearchProjects[0].AssignedScientists == 5);
	Controller->AdjustStrategicProjectStaff(
		EStrategicProjectType::Research,
		FGuid(),
		TEXT("research.signal-analysis"),
		1);
	TestEqual(TEXT("Native project staffing control routes through the domain command"),
		GameInstance->GetCampaignState().ResearchProjects[0].AssignedScientists, 6);
	Controller->CancelStrategicProject(
		EStrategicProjectType::Research,
		FGuid(),
		TEXT("research.signal-analysis"));
	TestTrue(TEXT("Native project cancellation control releases active research"),
		GameInstance->GetCampaignState().ResearchProjects.IsEmpty());
	Controller->ExecuteStrategicOption(EStrategicActionOptionType::Research, TEXT("research.signal-analysis"));
	TestEqual(TEXT("Cancelled research remains eligible to restart"),
		GameInstance->GetCampaignState().ResearchProjects.Num(), 1);

	const FStrategicDashboardSnapshot PlacementSnapshot = Controller->GetCurrentStrategicSnapshot();
	const FStrategicActionOptionView* FacilityOption = PlacementSnapshot.ActionOptions.FindByPredicate(
		[](const FStrategicActionOptionView& Option)
		{
			return Option.Type == EStrategicActionOptionType::Facility
				&& Option.RuleId == FName(TEXT("facility.flight-deck"));
		});
	TestTrue(TEXT("Facility action publishes selectable manual placements"), FacilityOption != nullptr
		&& !FacilityOption->ValidFacilityPlacements.IsEmpty());
	if (FacilityOption != nullptr && !FacilityOption->ValidFacilityPlacements.IsEmpty())
	{
		const FIntPoint ManualPlacement = FacilityOption->ValidFacilityPlacements.Last();
		Controller->StartStrategicFacilityConstruction(
			FacilityOption->RuleId,
			PlacementSnapshot.PrimaryBaseId,
			ManualPlacement.X,
			ManualPlacement.Y);
		TestTrue(TEXT("Manual facility placement routes the chosen anchor through the domain"),
			GameInstance->GetCampaignState().FacilityConstructionProjects.Num() == 1
			&& GameInstance->GetCampaignState().FacilityConstructionProjects[0].GridX == ManualPlacement.X
			&& GameInstance->GetCampaignState().FacilityConstructionProjects[0].GridY == ManualPlacement.Y);
		if (GameInstance->GetCampaignState().FacilityConstructionProjects.Num() == 1)
		{
			const FGuid ConstructionProjectId = GameInstance->GetCampaignState().FacilityConstructionProjects[0].ProjectId;
			Controller->CancelStrategicProject(
				EStrategicProjectType::Construction,
				ConstructionProjectId,
				FacilityOption->RuleId);
			TestTrue(TEXT("Native project controls cancel construction and release its grid footprint"),
				GameInstance->GetCampaignState().FacilityConstructionProjects.IsEmpty());
		}
	}
	Controller->AdvanceStrategicClock(EStrategicTimeRate::FiveMinutes);
	TestTrue(TEXT("Strategic time button advances through the same command adapter"),
		GameInstance->GetCampaignState().StrategicTime.Utc > FDateTime(2035, 1, 1, 12, 0, 0));

	TestTrue(TEXT("Temporary campaign-shell world shuts down cleanly"), TestWorld.DestroyTestWorld(false));
	return true;
}

#endif
