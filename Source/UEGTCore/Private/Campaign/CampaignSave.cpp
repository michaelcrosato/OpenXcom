// Copyright 2026 UEGT contributors. MIT License.

#include "Campaign/CampaignSave.h"

#include "Content/ContentPackageResolver.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace CampaignSavePrivate
{
	void AddDiagnostic(
		TArray<FCampaignSaveDiagnostic>& Diagnostics,
		const ECampaignSaveDiagnosticSeverity Severity,
		const FName Code,
		FString Message)
	{
		FCampaignSaveDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Severity = Severity;
		Diagnostic.Code = Code;
		Diagnostic.Message = MoveTemp(Message);
	}

	bool HasErrors(const TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		return Diagnostics.ContainsByPredicate(
			[](const FCampaignSaveDiagnostic& Diagnostic)
			{
				return Diagnostic.Severity == ECampaignSaveDiagnosticSeverity::Error;
			});
	}

	void NormalizeContentPackages(TArray<FCampaignContentVersion>& Packages)
	{
		Packages.Sort(
			[](const FCampaignContentVersion& Left, const FCampaignContentVersion& Right)
			{
				if (Left.PackageId != Right.PackageId)
				{
					return Left.PackageId.LexicalLess(Right.PackageId);
				}
				return Left.Version < Right.Version;
			});
	}

	void NormalizeEnvelope(FCampaignSaveEnvelope& Envelope)
	{
		NormalizeContentPackages(Envelope.Header.ContentPackages);
		Envelope.State.CompletedResearch.Sort(FNameLexicalLess());
		Envelope.State.Bases.Sort(
			[](const FStrategicBaseState& Left, const FStrategicBaseState& Right)
			{
				return Left.BaseId.ToString(EGuidFormats::Digits) < Right.BaseId.ToString(EGuidFormats::Digits);
			});
		for (FStrategicBaseState& Base : Envelope.State.Bases)
		{
			Base.BuiltFacilities.Sort(FNameLexicalLess());
			Base.Facilities.Sort(
				[](const FBaseFacilityState& Left, const FBaseFacilityState& Right)
				{
					return Left.InstanceId.ToString(EGuidFormats::Digits) < Right.InstanceId.ToString(EGuidFormats::Digits);
				});
			Base.Inventory.Sort(
				[](const FInventoryStack& Left, const FInventoryStack& Right)
				{
					return Left.ItemId.LexicalLess(Right.ItemId);
			});
		}
		Envelope.State.MutualAidConvoys.Sort(
			[](const FMutualAidConvoyState& Left, const FMutualAidConvoyState& Right)
			{
				return Left.ConvoyId.ToString(EGuidFormats::Digits)
					< Right.ConvoyId.ToString(EGuidFormats::Digits);
			});
		Envelope.State.ResearchProjects.Sort(
			[](const FResearchProjectState& Left, const FResearchProjectState& Right)
			{
				return Left.ResearchId.LexicalLess(Right.ResearchId);
			});
		Envelope.State.ManufacturingProjects.Sort(
			[](const FManufacturingProjectState& Left, const FManufacturingProjectState& Right)
			{
				return Left.ProjectId.ToString(EGuidFormats::Digits) < Right.ProjectId.ToString(EGuidFormats::Digits);
			});
		Envelope.State.FacilityConstructionProjects.Sort(
			[](const FFacilityConstructionProjectState& Left, const FFacilityConstructionProjectState& Right)
			{
				return Left.ProjectId.ToString(EGuidFormats::Digits) < Right.ProjectId.ToString(EGuidFormats::Digits);
			});
		Envelope.State.Personnel.Sort(
			[](const FPersonnelState& Left, const FPersonnelState& Right)
			{
				return Left.PersonnelId.ToString(EGuidFormats::Digits) < Right.PersonnelId.ToString(EGuidFormats::Digits);
			});
		for (FPersonnelState& Person : Envelope.State.Personnel)
		{
			Person.EquippedItems.Sort(FNameLexicalLess());
			Person.DoctrineSelections.Sort(FNameLexicalLess());
			Person.Commendations.Sort(FNameLexicalLess());
		}
		Envelope.State.PersonnelSquadBonds.Sort(
			[](const FPersonnelSquadBondState& Left, const FPersonnelSquadBondState& Right)
			{
				const FString LeftFirst = Left.FirstPersonnelId.ToString(EGuidFormats::Digits);
				const FString RightFirst = Right.FirstPersonnelId.ToString(EGuidFormats::Digits);
				if (LeftFirst != RightFirst)
				{
					return LeftFirst < RightFirst;
				}
				return Left.SecondPersonnelId.ToString(EGuidFormats::Digits)
					< Right.SecondPersonnelId.ToString(EGuidFormats::Digits);
			});
		Envelope.State.RecruitmentOrders.Sort(
			[](const FRecruitmentOrderState& Left, const FRecruitmentOrderState& Right)
			{
				return Left.OrderId.ToString(EGuidFormats::Digits) < Right.OrderId.ToString(EGuidFormats::Digits);
			});
		Envelope.State.Memorial.Sort(
			[](const FMemorialRecord& Left, const FMemorialRecord& Right)
			{
				if (Left.DeathUtc != Right.DeathUtc)
				{
					return Left.DeathUtc < Right.DeathUtc;
				}
				return Left.PersonnelId.ToString(EGuidFormats::Digits) < Right.PersonnelId.ToString(EGuidFormats::Digits);
			});
		for (FMemorialRecord& Record : Envelope.State.Memorial)
		{
			Record.DoctrineSelections.Sort(FNameLexicalLess());
			Record.Commendations.Sort(FNameLexicalLess());
		}
		Envelope.State.Craft.Sort(
			[](const FCraftState& Left, const FCraftState& Right)
			{
				return Left.CraftId.ToString(EGuidFormats::Digits) < Right.CraftId.ToString(EGuidFormats::Digits);
			});
		for (FCraftState& Craft : Envelope.State.Craft)
		{
			Craft.EquipmentItems.Sort(FNameLexicalLess());
			Craft.AssignedAgentIds.Sort(
				[](const FGuid& Left, const FGuid& Right)
				{
					return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
				});
			Craft.Cargo.Sort(
				[](const FInventoryStack& Left, const FInventoryStack& Right)
				{
					return Left.ItemId.LexicalLess(Right.ItemId);
				});
			Craft.PendingSalvage.Sort(
				[](const FInventoryStack& Left, const FInventoryStack& Right)
				{
					return Left.ItemId.LexicalLess(Right.ItemId);
				});
			Craft.WeaponStates.Sort(
				[](const FCraftWeaponState& Left, const FCraftWeaponState& Right)
				{
					return Left.WeaponItemId.LexicalLess(Right.WeaponItemId);
				});
		}
		Envelope.State.CraftAcquisitionOrders.Sort(
			[](const FCraftAcquisitionOrderState& Left, const FCraftAcquisitionOrderState& Right)
			{
				return Left.OrderId.ToString(EGuidFormats::Digits) < Right.OrderId.ToString(EGuidFormats::Digits);
			});
		Envelope.State.StrategicContacts.Sort(
			[](const FStrategicContactState& Left, const FStrategicContactState& Right)
			{
				return Left.ContactId.ToString(EGuidFormats::Digits) < Right.ContactId.ToString(EGuidFormats::Digits);
			});
		Envelope.State.StrategicSites.Sort(
			[](const FStrategicSiteState& Left, const FStrategicSiteState& Right)
			{
				return Left.SiteId.ToString(EGuidFormats::Digits) < Right.SiteId.ToString(EGuidFormats::Digits);
			});
		Envelope.State.TacticalOperations.Sort(
			[](const FTacticalOperationState& Left, const FTacticalOperationState& Right)
			{
				return Left.OperationId.ToString(EGuidFormats::Digits) < Right.OperationId.ToString(EGuidFormats::Digits);
			});
		for (FTacticalOperationState& Operation : Envelope.State.TacticalOperations)
		{
			Operation.AgentIds.Sort(
				[](const FGuid& Left, const FGuid& Right)
				{
					return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
				});
			Operation.Cargo.Sort(
				[](const FInventoryStack& Left, const FInventoryStack& Right)
				{
					return Left.ItemId.LexicalLess(Right.ItemId);
				});
		}
		Envelope.State.TacticalBattles.Sort(
			[](const FTacticalBattleState& Left, const FTacticalBattleState& Right)
			{
				return Left.BattleId.ToString(EGuidFormats::Digits) < Right.BattleId.ToString(EGuidFormats::Digits);
			});
		for (FTacticalBattleState& Battle : Envelope.State.TacticalBattles)
		{
			Battle.Cells.Sort(
				[](const FTacticalCellState& Left, const FTacticalCellState& Right)
				{
					if (Left.Z != Right.Z)
					{
						return Left.Z < Right.Z;
					}
					return Left.Y != Right.Y ? Left.Y < Right.Y : Left.X < Right.X;
				});
			Battle.PlayerDiscoveredCellIndices.Sort();
			Battle.Units.Sort(
				[](const FTacticalUnitState& Left, const FTacticalUnitState& Right)
				{
					return Left.UnitId.ToString(EGuidFormats::Digits) < Right.UnitId.ToString(EGuidFormats::Digits);
				});
			for (FTacticalUnitState& Unit : Battle.Units)
			{
				Unit.WeaponStates.Sort(
					[](const FTacticalWeaponState& Left, const FTacticalWeaponState& Right)
					{
						return Left.WeaponItemId.LexicalLess(Right.WeaponItemId);
					});
				Unit.CarriedItems.Sort(
					[](const FInventoryStack& Left, const FInventoryStack& Right)
					{
						return Left.ItemId.LexicalLess(Right.ItemId);
					});
				Unit.EjectedMagazines.Sort(
					[](const FTacticalMagazineState& Left, const FTacticalMagazineState& Right)
					{
						if (Left.WeaponItemId != Right.WeaponItemId)
						{
							return Left.WeaponItemId.LexicalLess(Right.WeaponItemId);
						}
						if (Left.AmmunitionItemId != Right.AmmunitionItemId)
						{
							return Left.AmmunitionItemId.LexicalLess(Right.AmmunitionItemId);
						}
						return Left.LoadedAmmunition > Right.LoadedAmmunition;
					});
			}
			Battle.Objectives.Sort(
				[](const FTacticalObjectiveState& Left, const FTacticalObjectiveState& Right)
				{
					return Left.ObjectiveId.LexicalLess(Right.ObjectiveId);
				});
			Battle.Cargo.Sort(
				[](const FInventoryStack& Left, const FInventoryStack& Right)
				{
					return Left.ItemId.LexicalLess(Right.ItemId);
				});
		}
		Envelope.State.RegionalPressure.Sort(
			[](const FRegionalPressureState& Left, const FRegionalPressureState& Right)
			{
				return Left.RegionId.LexicalLess(Right.RegionId);
			});
		Envelope.State.RegionalMandates.Sort(
			[](const FRegionalMandateState& Left, const FRegionalMandateState& Right)
			{
				return Left.RegionId.LexicalLess(Right.RegionId);
			});
		Envelope.State.AdversaryMissions.Sort(
			[](const FAdversaryMissionState& Left, const FAdversaryMissionState& Right)
			{
				return Left.MissionId.ToString(EGuidFormats::Digits) < Right.MissionId.ToString(EGuidFormats::Digits);
			});
		Envelope.State.BaseAssaults.Sort(
			[](const FBaseAssaultState& Left, const FBaseAssaultState& Right)
			{
				return Left.AssaultId.ToString(EGuidFormats::Digits) < Right.AssaultId.ToString(EGuidFormats::Digits);
			});
	}

	void AppendCanonicalField(FString& Output, const TCHAR* Name, const FString& Value)
	{
		Output += Name;
		Output += TEXT(":");
		Output += LexToString(Value.Len());
		Output += TEXT(":");
		Output += Value;
		Output += TEXT("\n");
	}

	bool MigrateRegionalMandates(FCampaignState& State)
	{
		if (!State.RegionalMandates.IsEmpty() || State.RegionalPressure.IsEmpty())
		{
			return true;
		}
		if (State.MonthlyFunding < 0)
		{
			return false;
		}
		const int64 EqualContribution = State.MonthlyFunding / State.RegionalPressure.Num();
		const int64 Remainder = State.MonthlyFunding % State.RegionalPressure.Num();
		for (int32 Index = 0; Index < State.RegionalPressure.Num(); ++Index)
		{
			FRegionalMandateState& Mandate = State.RegionalMandates.AddDefaulted_GetRef();
			Mandate.RegionId = State.RegionalPressure[Index].RegionId;
			Mandate.Support = 50;
			Mandate.BaselineMonthlyFunding = EqualContribution
				+ (Index == State.RegionalPressure.Num() - 1 ? Remainder : 0);
			Mandate.CurrentMonthlyFunding = Mandate.BaselineMonthlyFunding;
			Mandate.LastDiplomaticActionMonth = 0;
			Mandate.bResilienceCharterSigned = false;
		}
		return true;
	}

	bool MigratePersonnelProgression(FCampaignState& State)
	{
		for (FPersonnelState& Person : State.Personnel)
		{
			if (Person.PendingDoctrineChoices != 0
				|| !Person.DoctrineSelections.IsEmpty() || !Person.Commendations.IsEmpty())
			{
				return false;
			}
			Person.PendingDoctrineChoices = FMath::Max(0, Person.Rank - 1);
		}
		for (const FMemorialRecord& Record : State.Memorial)
		{
			if (!Record.DoctrineSelections.IsEmpty() || !Record.Commendations.IsEmpty())
			{
				return false;
			}
		}
		return true;
	}

	void MigratePersonnelRecoveryPlans(FCampaignState& State)
	{
		for (FPersonnelState& Person : State.Personnel)
		{
			Person.RecoveryPlan = Person.Status == EPersonnelStatus::Recovering
				? EPersonnelRecoveryPlan::MeasuredReturn
				: EPersonnelRecoveryPlan::None;
		}
	}

	void MigratePersonnelStewardship(FCampaignState& State)
	{
		for (FPersonnelState& Person : State.Personnel)
		{
			Person.StewardshipFocus = EPersonnelStewardshipFocus::None;
			Person.RemainingStewardshipSeconds = 0;
			Person.StewardshipToursCompleted = 0;
		}
		for (FMemorialRecord& Record : State.Memorial)
		{
			Record.StewardshipToursCompleted = 0;
		}
	}

	void MigrateMutualAidConvoys(FCampaignState& State)
	{
		State.MutualAidConvoys.Reset();
	}

	void MigrateMutualAidRouting(FCampaignState& State)
	{
		constexpr int64 LegacyTransitSeconds = 72LL * 3600LL;
		constexpr int64 DefaultForecastDelaySeconds = 24LL * 3600LL;
		for (FMutualAidConvoyState& Convoy : State.MutualAidConvoys)
		{
			Convoy.RoutePolicy = EMutualAidRoutePolicy::OpenRelay;
			Convoy.TotalTransitSeconds = FMath::Max(
				LegacyTransitSeconds, Convoy.RemainingTransitSeconds);
			Convoy.RoutePressure = 0;
			Convoy.bSignalEscort = false;
			Convoy.SignalEscortCost = 0;
			Convoy.bInterdictionResolved = true;
			Convoy.ForecastInterdictionDelaySeconds = DefaultForecastDelaySeconds;
			Convoy.InterdictionDelaySeconds = 0;
		}
	}

	void MigrateMutualAidRelayQueue(FCampaignState& State)
	{
		const int64 ConvoyCount = State.MutualAidConvoys.Num();
		if (ConvoyCount <= 0)
		{
			return;
		}
		const int64 FirstSequence = State.CommandSequence >= ConvoyCount
			? State.CommandSequence - ConvoyCount + 1
			: 1;
		for (int32 Index = 0; Index < State.MutualAidConvoys.Num(); ++Index)
		{
			State.MutualAidConvoys[Index].DispatchSequence = FirstSequence + Index;
		}
		State.CommandSequence = FMath::Max(
			State.CommandSequence, FirstSequence + ConvoyCount - 1);
	}

	void MigrateSignalWatchStaffing(FCampaignState& State)
	{
		for (FStrategicBaseState& Base : State.Bases)
		{
			Base.SignalWatchScientists = 0;
		}
	}

	void MigrateMutualAidRelayWaypoints(FCampaignState& State)
	{
		for (FMutualAidConvoyState& Convoy : State.MutualAidConvoys)
		{
			Convoy.CurrentLegOriginBaseId.Invalidate();
			Convoy.RelayWaypointBaseId.Invalidate();
			Convoy.OnwardRoutePolicy = EMutualAidRoutePolicy::OpenRelay;
			Convoy.OnwardTotalTransitSeconds = 0;
			Convoy.OnwardRoutePressure = 0;
			Convoy.bOnwardInterdictionResolved = true;
			Convoy.OnwardForecastInterdictionDelaySeconds = 0;
		}
	}

	void MigrateMutualAidBalancedHandoffs(FCampaignState& State)
	{
		for (FMutualAidConvoyState& Convoy : State.MutualAidConvoys)
		{
			Convoy.BalancedHandoffQuantity = 0;
		}
	}

	void MigrateWorksCadreStaffing(FCampaignState& State)
	{
		for (FStrategicBaseState& Base : State.Bases)
		{
			Base.WorksCadreEngineers = 0;
		}
	}

	void MigrateWorksCadreCharters(FCampaignState& State)
	{
		for (FStrategicBaseState& Base : State.Bases)
		{
			Base.WorksCadreCharter = EWorksCadreCharter::CommonCadence;
		}
	}

	uint32 RotateRight(const uint32 Value, const uint32 Bits)
	{
		return (Value >> Bits) | (Value << (32U - Bits));
	}

	FString HashString(const FString& Input)
	{
		static constexpr uint32 RoundConstants[64] = {
			0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
			0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
			0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
			0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
			0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
			0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
			0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
			0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
		};

		uint32 Hash[8] = {
			0x6a09e667U,
			0xbb67ae85U,
			0x3c6ef372U,
			0xa54ff53aU,
			0x510e527fU,
			0x9b05688cU,
			0x1f83d9abU,
			0x5be0cd19U
		};

		const FTCHARToUTF8 Utf8(*Input);
		TArray<uint8> Message;
		Message.Reserve(Utf8.Length() + 72);
		Message.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		const uint64 BitLength = static_cast<uint64>(Utf8.Length()) * 8ULL;
		Message.Add(0x80U);
		while (Message.Num() % 64 != 56)
		{
			Message.Add(0U);
		}
		for (int32 Shift = 56; Shift >= 0; Shift -= 8)
		{
			Message.Add(static_cast<uint8>(BitLength >> Shift));
		}

		for (int32 Offset = 0; Offset < Message.Num(); Offset += 64)
		{
			uint32 Words[64];
			for (int32 Index = 0; Index < 16; ++Index)
			{
				const int32 ByteIndex = Offset + Index * 4;
				Words[Index] = (static_cast<uint32>(Message[ByteIndex]) << 24)
					| (static_cast<uint32>(Message[ByteIndex + 1]) << 16)
					| (static_cast<uint32>(Message[ByteIndex + 2]) << 8)
					| static_cast<uint32>(Message[ByteIndex + 3]);
			}
			for (int32 Index = 16; Index < 64; ++Index)
			{
				const uint32 S0 = RotateRight(Words[Index - 15], 7) ^ RotateRight(Words[Index - 15], 18) ^ (Words[Index - 15] >> 3);
				const uint32 S1 = RotateRight(Words[Index - 2], 17) ^ RotateRight(Words[Index - 2], 19) ^ (Words[Index - 2] >> 10);
				Words[Index] = Words[Index - 16] + S0 + Words[Index - 7] + S1;
			}

			uint32 A = Hash[0];
			uint32 B = Hash[1];
			uint32 C = Hash[2];
			uint32 D = Hash[3];
			uint32 E = Hash[4];
			uint32 F = Hash[5];
			uint32 G = Hash[6];
			uint32 H = Hash[7];
			for (int32 Index = 0; Index < 64; ++Index)
			{
				const uint32 Sum1 = RotateRight(E, 6) ^ RotateRight(E, 11) ^ RotateRight(E, 25);
				const uint32 Choice = (E & F) ^ (~E & G);
				const uint32 Temp1 = H + Sum1 + Choice + RoundConstants[Index] + Words[Index];
				const uint32 Sum0 = RotateRight(A, 2) ^ RotateRight(A, 13) ^ RotateRight(A, 22);
				const uint32 Majority = (A & B) ^ (A & C) ^ (B & C);
				const uint32 Temp2 = Sum0 + Majority;

				H = G;
				G = F;
				F = E;
				E = D + Temp1;
				D = C;
				C = B;
				B = A;
				A = Temp1 + Temp2;
			}

			Hash[0] += A;
			Hash[1] += B;
			Hash[2] += C;
			Hash[3] += D;
			Hash[4] += E;
			Hash[5] += F;
			Hash[6] += G;
			Hash[7] += H;
		}

		return FString::Printf(
			TEXT("%08x%08x%08x%08x%08x%08x%08x%08x"),
			Hash[0], Hash[1], Hash[2], Hash[3], Hash[4], Hash[5], Hash[6], Hash[7]);
	}

	FString BuildContentCanonical(const TArray<FCampaignContentVersion>& InputPackages)
	{
		TArray<FCampaignContentVersion> Packages = InputPackages;
		NormalizeContentPackages(Packages);

		FString Canonical;
		AppendCanonicalField(Canonical, TEXT("packageCount"), LexToString(Packages.Num()));
		for (const FCampaignContentVersion& Package : Packages)
		{
			AppendCanonicalField(Canonical, TEXT("packageId"), Package.PackageId.ToString());
			AppendCanonicalField(Canonical, TEXT("packageVersion"), Package.Version);
		}
		return Canonical;
	}

	FString DifficultyToString(const ECampaignDifficulty Difficulty)
	{
		switch (Difficulty)
		{
		case ECampaignDifficulty::Cadet:
			return TEXT("cadet");
		case ECampaignDifficulty::Standard:
			return TEXT("standard");
		case ECampaignDifficulty::Veteran:
			return TEXT("veteran");
		case ECampaignDifficulty::Apex:
			return TEXT("apex");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseDifficulty(const FString& Value, ECampaignDifficulty& OutDifficulty)
	{
		if (Value == TEXT("cadet"))
		{
			OutDifficulty = ECampaignDifficulty::Cadet;
			return true;
		}
		if (Value == TEXT("standard"))
		{
			OutDifficulty = ECampaignDifficulty::Standard;
			return true;
		}
		if (Value == TEXT("veteran"))
		{
			OutDifficulty = ECampaignDifficulty::Veteran;
			return true;
		}
		if (Value == TEXT("apex"))
		{
			OutDifficulty = ECampaignDifficulty::Apex;
			return true;
		}
		return false;
	}

	FString PersonnelStatusToString(const EPersonnelStatus Status)
	{
		switch (Status)
		{
		case EPersonnelStatus::Available:
			return TEXT("available");
		case EPersonnelStatus::Recovering:
			return TEXT("recovering");
		case EPersonnelStatus::Training:
			return TEXT("training");
		case EPersonnelStatus::Deployed:
			return TEXT("deployed");
		case EPersonnelStatus::Stewarding:
			return TEXT("stewarding");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParsePersonnelStatus(const FString& Value, EPersonnelStatus& OutStatus)
	{
		if (Value == TEXT("available"))
		{
			OutStatus = EPersonnelStatus::Available;
			return true;
		}
		if (Value == TEXT("recovering"))
		{
			OutStatus = EPersonnelStatus::Recovering;
			return true;
		}
		if (Value == TEXT("training"))
		{
			OutStatus = EPersonnelStatus::Training;
			return true;
		}
		if (Value == TEXT("deployed"))
		{
			OutStatus = EPersonnelStatus::Deployed;
			return true;
		}
		if (Value == TEXT("stewarding"))
		{
			OutStatus = EPersonnelStatus::Stewarding;
			return true;
		}
		return false;
	}

	FString StewardshipFocusToString(const EPersonnelStewardshipFocus Focus)
	{
		switch (Focus)
		{
		case EPersonnelStewardshipFocus::None:
			return TEXT("none");
		case EPersonnelStewardshipFocus::RecoveryAdvocacy:
			return TEXT("recovery-advocacy");
		case EPersonnelStewardshipFocus::TrainingCadre:
			return TEXT("training-cadre");
		case EPersonnelStewardshipFocus::RecruitmentLiaison:
			return TEXT("recruitment-liaison");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseStewardshipFocus(const FString& Value, EPersonnelStewardshipFocus& OutFocus)
	{
		if (Value == TEXT("none"))
		{
			OutFocus = EPersonnelStewardshipFocus::None;
			return true;
		}
		if (Value == TEXT("recovery-advocacy"))
		{
			OutFocus = EPersonnelStewardshipFocus::RecoveryAdvocacy;
			return true;
		}
		if (Value == TEXT("training-cadre"))
		{
			OutFocus = EPersonnelStewardshipFocus::TrainingCadre;
			return true;
		}
		if (Value == TEXT("recruitment-liaison"))
		{
			OutFocus = EPersonnelStewardshipFocus::RecruitmentLiaison;
			return true;
		}
		return false;
	}

	FString RecoveryPlanToString(const EPersonnelRecoveryPlan Plan)
	{
		switch (Plan)
		{
		case EPersonnelRecoveryPlan::None:
			return TEXT("none");
		case EPersonnelRecoveryPlan::DecisionRequired:
			return TEXT("decision-required");
		case EPersonnelRecoveryPlan::MeasuredReturn:
			return TEXT("measured-return");
		case EPersonnelRecoveryPlan::SurgeCare:
			return TEXT("surge-care");
		case EPersonnelRecoveryPlan::ReflectionCycle:
			return TEXT("reflection-cycle");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseRecoveryPlan(const FString& Value, EPersonnelRecoveryPlan& OutPlan)
	{
		if (Value == TEXT("none"))
		{
			OutPlan = EPersonnelRecoveryPlan::None;
			return true;
		}
		if (Value == TEXT("decision-required"))
		{
			OutPlan = EPersonnelRecoveryPlan::DecisionRequired;
			return true;
		}
		if (Value == TEXT("measured-return"))
		{
			OutPlan = EPersonnelRecoveryPlan::MeasuredReturn;
			return true;
		}
		if (Value == TEXT("surge-care"))
		{
			OutPlan = EPersonnelRecoveryPlan::SurgeCare;
			return true;
		}
		if (Value == TEXT("reflection-cycle"))
		{
			OutPlan = EPersonnelRecoveryPlan::ReflectionCycle;
			return true;
		}
		return false;
	}

	FString TrainingFocusToString(const EPersonnelTrainingFocus Focus)
	{
		switch (Focus)
		{
		case EPersonnelTrainingFocus::Accuracy:
			return TEXT("accuracy");
		case EPersonnelTrainingFocus::Resolve:
			return TEXT("resolve");
		case EPersonnelTrainingFocus::Mobility:
			return TEXT("mobility");
		case EPersonnelTrainingFocus::Strength:
			return TEXT("strength");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseTrainingFocus(const FString& Value, EPersonnelTrainingFocus& OutFocus)
	{
		if (Value == TEXT("accuracy"))
		{
			OutFocus = EPersonnelTrainingFocus::Accuracy;
			return true;
		}
		if (Value == TEXT("resolve"))
		{
			OutFocus = EPersonnelTrainingFocus::Resolve;
			return true;
		}
		if (Value == TEXT("mobility"))
		{
			OutFocus = EPersonnelTrainingFocus::Mobility;
			return true;
		}
		if (Value == TEXT("strength"))
		{
			OutFocus = EPersonnelTrainingFocus::Strength;
			return true;
		}
		return false;
	}

	FString CraftStatusToString(const ECraftStatus Status)
	{
		switch (Status)
		{
		case ECraftStatus::Grounded:
			return TEXT("grounded");
		case ECraftStatus::Servicing:
			return TEXT("servicing");
		case ECraftStatus::Airborne:
			return TEXT("airborne");
		case ECraftStatus::Intercepting:
			return TEXT("intercepting");
		case ECraftStatus::Returning:
			return TEXT("returning");
		case ECraftStatus::Deploying:
			return TEXT("deploying");
		case ECraftStatus::OnSite:
			return TEXT("on-site");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseCraftStatus(const FString& Value, ECraftStatus& OutStatus)
	{
		if (Value == TEXT("grounded"))
		{
			OutStatus = ECraftStatus::Grounded;
			return true;
		}
		if (Value == TEXT("servicing"))
		{
			OutStatus = ECraftStatus::Servicing;
			return true;
		}
		if (Value == TEXT("airborne"))
		{
			OutStatus = ECraftStatus::Airborne;
			return true;
		}
		if (Value == TEXT("intercepting"))
		{
			OutStatus = ECraftStatus::Intercepting;
			return true;
		}
		if (Value == TEXT("returning"))
		{
			OutStatus = ECraftStatus::Returning;
			return true;
		}
		if (Value == TEXT("deploying"))
		{
			OutStatus = ECraftStatus::Deploying;
			return true;
		}
		if (Value == TEXT("on-site"))
		{
			OutStatus = ECraftStatus::OnSite;
			return true;
		}
		return false;
	}

	FString MutualAidRoutePolicyToString(const EMutualAidRoutePolicy Policy)
	{
		switch (Policy)
		{
		case EMutualAidRoutePolicy::OpenRelay:
			return TEXT("open-relay");
		case EMutualAidRoutePolicy::RapidThread:
			return TEXT("rapid-thread");
		case EMutualAidRoutePolicy::VeiledChain:
			return TEXT("veiled-chain");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseMutualAidRoutePolicy(
		const FString& Value,
		EMutualAidRoutePolicy& OutPolicy)
	{
		if (Value == TEXT("open-relay"))
		{
			OutPolicy = EMutualAidRoutePolicy::OpenRelay;
			return true;
		}
		if (Value == TEXT("rapid-thread"))
		{
			OutPolicy = EMutualAidRoutePolicy::RapidThread;
			return true;
		}
		if (Value == TEXT("veiled-chain"))
		{
			OutPolicy = EMutualAidRoutePolicy::VeiledChain;
			return true;
		}
		return false;
	}

	FString WorksCadreCharterToString(const EWorksCadreCharter Charter)
	{
		switch (Charter)
		{
		case EWorksCadreCharter::CommonCadence:
			return TEXT("common-cadence");
		case EWorksCadreCharter::AssemblyCadence:
			return TEXT("assembly-cadence");
		case EWorksCadreCharter::RestorationCadence:
			return TEXT("restoration-cadence");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseWorksCadreCharter(
		const FString& Value,
		EWorksCadreCharter& OutCharter)
	{
		if (Value == TEXT("common-cadence"))
		{
			OutCharter = EWorksCadreCharter::CommonCadence;
			return true;
		}
		if (Value == TEXT("assembly-cadence"))
		{
			OutCharter = EWorksCadreCharter::AssemblyCadence;
			return true;
		}
		if (Value == TEXT("restoration-cadence"))
		{
			OutCharter = EWorksCadreCharter::RestorationCadence;
			return true;
		}
		return false;
	}

	FString ContactStatusToString(const EStrategicContactStatus Status)
	{
		switch (Status)
		{
		case EStrategicContactStatus::Hidden:
			return TEXT("hidden");
		case EStrategicContactStatus::Detected:
			return TEXT("detected");
		case EStrategicContactStatus::Engaged:
			return TEXT("engaged");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseContactStatus(const FString& Value, EStrategicContactStatus& OutStatus)
	{
		if (Value == TEXT("hidden"))
		{
			OutStatus = EStrategicContactStatus::Hidden;
			return true;
		}
		if (Value == TEXT("detected"))
		{
			OutStatus = EStrategicContactStatus::Detected;
			return true;
		}
		if (Value == TEXT("engaged"))
		{
			OutStatus = EStrategicContactStatus::Engaged;
			return true;
		}
		return false;
	}

	FString SiteTypeToString(const EStrategicSiteType Type)
	{
		switch (Type)
		{
		case EStrategicSiteType::Wreckage:
			return TEXT("wreckage");
		case EStrategicSiteType::Landing:
			return TEXT("landing");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseSiteType(const FString& Value, EStrategicSiteType& OutType)
	{
		if (Value == TEXT("wreckage"))
		{
			OutType = EStrategicSiteType::Wreckage;
			return true;
		}
		if (Value == TEXT("landing"))
		{
			OutType = EStrategicSiteType::Landing;
			return true;
		}
		return false;
	}

	FString TacticalOperationTypeToString(const ETacticalOperationType Type)
	{
		switch (Type)
		{
		case ETacticalOperationType::SiteRecovery:
			return TEXT("site-recovery");
		case ETacticalOperationType::BaseDefense:
			return TEXT("base-defense");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseTacticalOperationType(const FString& Value, ETacticalOperationType& OutType)
	{
		if (Value == TEXT("site-recovery"))
		{
			OutType = ETacticalOperationType::SiteRecovery;
			return true;
		}
		if (Value == TEXT("base-defense"))
		{
			OutType = ETacticalOperationType::BaseDefense;
			return true;
		}
		return false;
	}

	FString TacticalTeamToString(const ETacticalTeam Team)
	{
		switch (Team)
		{
		case ETacticalTeam::Player:
			return TEXT("player");
		case ETacticalTeam::Adversary:
			return TEXT("adversary");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseTacticalTeam(const FString& Value, ETacticalTeam& OutTeam)
	{
		if (Value == TEXT("player"))
		{
			OutTeam = ETacticalTeam::Player;
			return true;
		}
		if (Value == TEXT("adversary"))
		{
			OutTeam = ETacticalTeam::Adversary;
			return true;
		}
		return false;
	}

	FString TacticalStanceToString(const ETacticalStance Stance)
	{
		switch (Stance)
		{
		case ETacticalStance::Standing:
			return TEXT("standing");
		case ETacticalStance::Crouched:
			return TEXT("crouched");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseTacticalStance(const FString& Value, ETacticalStance& OutStance)
	{
		if (Value == TEXT("standing"))
		{
			OutStance = ETacticalStance::Standing;
			return true;
		}
		if (Value == TEXT("crouched"))
		{
			OutStance = ETacticalStance::Crouched;
			return true;
		}
		return false;
	}

	FString TacticalWindDirectionToString(const ETacticalWindDirection Direction)
	{
		switch (Direction)
		{
		case ETacticalWindDirection::Calm:
			return TEXT("calm");
		case ETacticalWindDirection::North:
			return TEXT("north");
		case ETacticalWindDirection::East:
			return TEXT("east");
		case ETacticalWindDirection::South:
			return TEXT("south");
		case ETacticalWindDirection::West:
			return TEXT("west");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseTacticalWindDirection(const FString& Value, ETacticalWindDirection& OutDirection)
	{
		if (Value == TEXT("calm"))
		{
			OutDirection = ETacticalWindDirection::Calm;
			return true;
		}
		if (Value == TEXT("north"))
		{
			OutDirection = ETacticalWindDirection::North;
			return true;
		}
		if (Value == TEXT("east"))
		{
			OutDirection = ETacticalWindDirection::East;
			return true;
		}
		if (Value == TEXT("south"))
		{
			OutDirection = ETacticalWindDirection::South;
			return true;
		}
		if (Value == TEXT("west"))
		{
			OutDirection = ETacticalWindDirection::West;
			return true;
		}
		return false;
	}

	FString TacticalPhaseToString(const ETacticalBattlePhase Phase)
	{
		switch (Phase)
		{
		case ETacticalBattlePhase::Deployment:
			return TEXT("deployment");
		case ETacticalBattlePhase::PlayerTurn:
			return TEXT("player-turn");
		case ETacticalBattlePhase::AdversaryTurn:
			return TEXT("adversary-turn");
		case ETacticalBattlePhase::Resolved:
			return TEXT("resolved");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseTacticalPhase(const FString& Value, ETacticalBattlePhase& OutPhase)
	{
		if (Value == TEXT("deployment"))
		{
			OutPhase = ETacticalBattlePhase::Deployment;
			return true;
		}
		if (Value == TEXT("player-turn"))
		{
			OutPhase = ETacticalBattlePhase::PlayerTurn;
			return true;
		}
		if (Value == TEXT("adversary-turn"))
		{
			OutPhase = ETacticalBattlePhase::AdversaryTurn;
			return true;
		}
		if (Value == TEXT("resolved"))
		{
			OutPhase = ETacticalBattlePhase::Resolved;
			return true;
		}
		return false;
	}

	FString TacticalObjectiveStatusToString(const ETacticalObjectiveStatus Status)
	{
		switch (Status)
		{
		case ETacticalObjectiveStatus::Active:
			return TEXT("active");
		case ETacticalObjectiveStatus::Completed:
			return TEXT("completed");
		case ETacticalObjectiveStatus::Failed:
			return TEXT("failed");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseTacticalObjectiveStatus(const FString& Value, ETacticalObjectiveStatus& OutStatus)
	{
		if (Value == TEXT("active"))
		{
			OutStatus = ETacticalObjectiveStatus::Active;
			return true;
		}
		if (Value == TEXT("completed"))
		{
			OutStatus = ETacticalObjectiveStatus::Completed;
			return true;
		}
		if (Value == TEXT("failed"))
		{
			OutStatus = ETacticalObjectiveStatus::Failed;
			return true;
		}
		return false;
	}

	FString TacticalObjectiveTypeToString(const ETacticalObjectiveType Type)
	{
		switch (Type)
		{
		case ETacticalObjectiveType::Disrupt:
			return TEXT("disrupt");
		case ETacticalObjectiveType::Recover:
			return TEXT("recover");
		case ETacticalObjectiveType::Control:
			return TEXT("control");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseTacticalObjectiveType(const FString& Value, ETacticalObjectiveType& OutType)
	{
		if (Value == TEXT("disrupt"))
		{
			OutType = ETacticalObjectiveType::Disrupt;
			return true;
		}
		if (Value == TEXT("recover"))
		{
			OutType = ETacticalObjectiveType::Recover;
			return true;
		}
		if (Value == TEXT("control"))
		{
			OutType = ETacticalObjectiveType::Control;
			return true;
		}
		return false;
	}

	FString CampaignOutcomeToString(const ECampaignOutcome Outcome)
	{
		switch (Outcome)
		{
		case ECampaignOutcome::Ongoing:
			return TEXT("ongoing");
		case ECampaignOutcome::Victory:
			return TEXT("victory");
		case ECampaignOutcome::Failure:
			return TEXT("failure");
		default:
			return TEXT("invalid");
		}
	}

	bool TryParseCampaignOutcome(const FString& Value, ECampaignOutcome& OutOutcome)
	{
		if (Value == TEXT("ongoing"))
		{
			OutOutcome = ECampaignOutcome::Ongoing;
			return true;
		}
		if (Value == TEXT("victory"))
		{
			OutOutcome = ECampaignOutcome::Victory;
			return true;
		}
		if (Value == TEXT("failure"))
		{
			OutOutcome = ECampaignOutcome::Failure;
			return true;
		}
		return false;
	}

	FString UInt64ToHex(const uint64 Value)
	{
		return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Value));
	}

	FString BuildSaveCanonical(const FCampaignSaveEnvelope& InputEnvelope)
	{
		FCampaignSaveEnvelope Envelope = InputEnvelope;
		NormalizeEnvelope(Envelope);

		FString Canonical;
		AppendCanonicalField(Canonical, TEXT("formatVersion"), LexToString(Envelope.Header.FormatVersion));
		AppendCanonicalField(Canonical, TEXT("campaignId"), Envelope.Header.CampaignId.ToString(EGuidFormats::DigitsWithHyphensLower));
		AppendCanonicalField(Canonical, TEXT("createdTicks"), LexToString(Envelope.Header.CreatedUtc.GetTicks()));
		AppendCanonicalField(Canonical, TEXT("savedTicks"), LexToString(Envelope.Header.LastSavedUtc.GetTicks()));
		AppendCanonicalField(Canonical, TEXT("buildVersion"), Envelope.Header.BuildVersion);
		Canonical += BuildContentCanonical(Envelope.Header.ContentPackages);
		AppendCanonicalField(Canonical, TEXT("contentFingerprint"), Envelope.Header.ContentFingerprint);
		AppendCanonicalField(Canonical, TEXT("strategicTicks"), LexToString(Envelope.State.StrategicTime.Utc.GetTicks()));
		AppendCanonicalField(Canonical, TEXT("randomInitialSeed"), LexToString(Envelope.State.SimulationRandom.InitialSeed));
		AppendCanonicalField(Canonical, TEXT("randomDrawCount"), LexToString(Envelope.State.SimulationRandom.DrawCount));
		AppendCanonicalField(Canonical, TEXT("randomState"), UInt64ToHex(Envelope.State.SimulationRandom.GetStateForSave()));
		AppendCanonicalField(Canonical, TEXT("funds"), LexToString(Envelope.State.Funds));
		AppendCanonicalField(Canonical, TEXT("campaignScore"), LexToString(Envelope.State.CampaignScore));
		AppendCanonicalField(Canonical, TEXT("difficulty"), DifficultyToString(Envelope.State.Difficulty));
		AppendCanonicalField(Canonical, TEXT("commandSequence"), LexToString(Envelope.State.CommandSequence));
		AppendCanonicalField(Canonical, TEXT("completedResearchCount"), LexToString(Envelope.State.CompletedResearch.Num()));
		for (const FName ResearchId : Envelope.State.CompletedResearch)
		{
			AppendCanonicalField(Canonical, TEXT("completedResearch"), ResearchId.ToString());
		}
		if (Envelope.Header.FormatVersion >= 3)
		{
			AppendCanonicalField(Canonical, TEXT("monthlyFunding"), LexToString(Envelope.State.MonthlyFunding));
			AppendCanonicalField(Canonical, TEXT("baseCount"), LexToString(Envelope.State.Bases.Num()));
			for (const FStrategicBaseState& Base : Envelope.State.Bases)
			{
				AppendCanonicalField(Canonical, TEXT("baseId"), Base.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
				AppendCanonicalField(Canonical, TEXT("baseName"), Base.Name);
				AppendCanonicalField(Canonical, TEXT("baseRegion"), Base.RegionId.ToString());
				AppendCanonicalField(Canonical, TEXT("baseLongitude"), LexToString(Base.LongitudeMilliDegrees));
				AppendCanonicalField(Canonical, TEXT("baseLatitude"), LexToString(Base.LatitudeMilliDegrees));
				AppendCanonicalField(Canonical, TEXT("scientistCapacity"), LexToString(Base.ScientistCapacity));
				AppendCanonicalField(Canonical, TEXT("engineerCapacity"), LexToString(Base.EngineerCapacity));
				if (Envelope.Header.FormatVersion >= 39)
				{
					AppendCanonicalField(Canonical, TEXT("signalWatchScientists"),
						LexToString(Base.SignalWatchScientists));
				}
				if (Envelope.Header.FormatVersion >= 42)
				{
					AppendCanonicalField(Canonical, TEXT("worksCadreEngineers"),
						LexToString(Base.WorksCadreEngineers));
				}
				if (Envelope.Header.FormatVersion >= 43)
				{
					AppendCanonicalField(Canonical, TEXT("worksCadreCharter"),
						WorksCadreCharterToString(Base.WorksCadreCharter));
				}
				AppendCanonicalField(Canonical, TEXT("facilityCount"), LexToString(Base.BuiltFacilities.Num()));
				for (const FName FacilityId : Base.BuiltFacilities)
				{
					AppendCanonicalField(Canonical, TEXT("facility"), FacilityId.ToString());
				}
				if (Envelope.Header.FormatVersion >= 4)
				{
					AppendCanonicalField(Canonical, TEXT("inventoryCount"), LexToString(Base.Inventory.Num()));
					for (const FInventoryStack& Stack : Base.Inventory)
					{
						AppendCanonicalField(Canonical, TEXT("inventoryItem"), Stack.ItemId.ToString());
						AppendCanonicalField(Canonical, TEXT("inventoryQuantity"), LexToString(Stack.Quantity));
					}
				}
				if (Envelope.Header.FormatVersion >= 5)
				{
					AppendCanonicalField(Canonical, TEXT("facilityPlacementCount"), LexToString(Base.Facilities.Num()));
					for (const FBaseFacilityState& Facility : Base.Facilities)
					{
						AppendCanonicalField(Canonical, TEXT("facilityInstanceId"), Facility.InstanceId.ToString(EGuidFormats::DigitsWithHyphensLower));
						AppendCanonicalField(Canonical, TEXT("positionedFacility"), Facility.FacilityId.ToString());
						AppendCanonicalField(Canonical, TEXT("facilityGridX"), LexToString(Facility.GridX));
						AppendCanonicalField(Canonical, TEXT("facilityGridY"), LexToString(Facility.GridY));
						if (Envelope.Header.FormatVersion >= 19)
						{
							AppendCanonicalField(Canonical, TEXT("facilityDamage"), LexToString(Facility.Damage));
							AppendCanonicalField(Canonical, TEXT("facilityReservedRepairDamage"), LexToString(Facility.ReservedRepairDamage));
							AppendCanonicalField(Canonical, TEXT("facilityRemainingRepairSeconds"), LexToString(Facility.RemainingRepairSeconds));
						}
					}
				}
			}
			if (Envelope.Header.FormatVersion >= 36)
			{
				AppendCanonicalField(Canonical, TEXT("mutualAidConvoyCount"),
					LexToString(Envelope.State.MutualAidConvoys.Num()));
				for (const FMutualAidConvoyState& Convoy : Envelope.State.MutualAidConvoys)
				{
					AppendCanonicalField(Canonical, TEXT("mutualAidConvoyId"),
						Convoy.ConvoyId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("mutualAidSourceBase"),
						Convoy.SourceBaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("mutualAidDestinationBase"),
						Convoy.DestinationBaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("mutualAidItem"), Convoy.ItemId.ToString());
					AppendCanonicalField(Canonical, TEXT("mutualAidQuantity"), LexToString(Convoy.Quantity));
					AppendCanonicalField(Canonical, TEXT("mutualAidTransitSeconds"),
						LexToString(Convoy.RemainingTransitSeconds));
					if (Envelope.Header.FormatVersion >= 37)
					{
						AppendCanonicalField(Canonical, TEXT("mutualAidRoutePolicy"),
							MutualAidRoutePolicyToString(Convoy.RoutePolicy));
						AppendCanonicalField(Canonical, TEXT("mutualAidTotalTransitSeconds"),
							LexToString(Convoy.TotalTransitSeconds));
						AppendCanonicalField(Canonical, TEXT("mutualAidRoutePressure"),
							LexToString(Convoy.RoutePressure));
						AppendCanonicalField(Canonical, TEXT("mutualAidSignalEscort"),
							Convoy.bSignalEscort ? TEXT("1") : TEXT("0"));
						AppendCanonicalField(Canonical, TEXT("mutualAidSignalEscortCost"),
							LexToString(Convoy.SignalEscortCost));
						AppendCanonicalField(Canonical, TEXT("mutualAidInterdictionResolved"),
							Convoy.bInterdictionResolved ? TEXT("1") : TEXT("0"));
						AppendCanonicalField(Canonical, TEXT("mutualAidForecastDelaySeconds"),
							LexToString(Convoy.ForecastInterdictionDelaySeconds));
						AppendCanonicalField(Canonical, TEXT("mutualAidAppliedDelaySeconds"),
							LexToString(Convoy.InterdictionDelaySeconds));
					}
					if (Envelope.Header.FormatVersion >= 38)
					{
						AppendCanonicalField(Canonical, TEXT("mutualAidDispatchSequence"),
							LexToString(Convoy.DispatchSequence));
					}
					if (Envelope.Header.FormatVersion >= 40)
					{
						AppendCanonicalField(Canonical, TEXT("mutualAidCurrentLegOriginBase"),
							Convoy.CurrentLegOriginBaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
						AppendCanonicalField(Canonical, TEXT("mutualAidRelayWaypointBase"),
							Convoy.RelayWaypointBaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
						AppendCanonicalField(Canonical, TEXT("mutualAidOnwardRoutePolicy"),
							MutualAidRoutePolicyToString(Convoy.OnwardRoutePolicy));
						AppendCanonicalField(Canonical, TEXT("mutualAidOnwardTotalTransitSeconds"),
							LexToString(Convoy.OnwardTotalTransitSeconds));
						AppendCanonicalField(Canonical, TEXT("mutualAidOnwardRoutePressure"),
							LexToString(Convoy.OnwardRoutePressure));
						AppendCanonicalField(Canonical, TEXT("mutualAidOnwardInterdictionResolved"),
							Convoy.bOnwardInterdictionResolved ? TEXT("1") : TEXT("0"));
						AppendCanonicalField(Canonical, TEXT("mutualAidOnwardForecastDelaySeconds"),
							LexToString(Convoy.OnwardForecastInterdictionDelaySeconds));
					}
					if (Envelope.Header.FormatVersion >= 41)
					{
						AppendCanonicalField(Canonical, TEXT("mutualAidBalancedHandoffQuantity"),
							LexToString(Convoy.BalancedHandoffQuantity));
					}
				}
			}
			AppendCanonicalField(Canonical, TEXT("researchProjectCount"), LexToString(Envelope.State.ResearchProjects.Num()));
			for (const FResearchProjectState& Project : Envelope.State.ResearchProjects)
			{
				AppendCanonicalField(Canonical, TEXT("projectResearch"), Project.ResearchId.ToString());
				AppendCanonicalField(Canonical, TEXT("projectBase"), Project.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
				AppendCanonicalField(Canonical, TEXT("projectScientists"), LexToString(Project.AssignedScientists));
				AppendCanonicalField(Canonical, TEXT("projectWorkSeconds"), LexToString(Project.AccumulatedWorkSeconds));
			}
			if (Envelope.Header.FormatVersion >= 4)
			{
				AppendCanonicalField(Canonical, TEXT("manufacturingProjectCount"), LexToString(Envelope.State.ManufacturingProjects.Num()));
				for (const FManufacturingProjectState& Project : Envelope.State.ManufacturingProjects)
				{
					AppendCanonicalField(Canonical, TEXT("manufacturingProjectId"), Project.ProjectId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("manufacturingItem"), Project.ItemId.ToString());
					AppendCanonicalField(Canonical, TEXT("manufacturingBase"), Project.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("manufacturingEngineers"), LexToString(Project.AssignedEngineers));
					AppendCanonicalField(Canonical, TEXT("manufacturingUnitsRemaining"), LexToString(Project.UnitsRemaining));
					AppendCanonicalField(Canonical, TEXT("manufacturingWorkSeconds"), LexToString(Project.AccumulatedWorkSeconds));
				}
			}
			if (Envelope.Header.FormatVersion >= 5)
			{
				AppendCanonicalField(Canonical, TEXT("facilityConstructionCount"), LexToString(Envelope.State.FacilityConstructionProjects.Num()));
				for (const FFacilityConstructionProjectState& Project : Envelope.State.FacilityConstructionProjects)
				{
					AppendCanonicalField(Canonical, TEXT("constructionProjectId"), Project.ProjectId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("constructionInstanceId"), Project.FacilityInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("constructionBaseId"), Project.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("constructionFacilityId"), Project.FacilityId.ToString());
					AppendCanonicalField(Canonical, TEXT("constructionGridX"), LexToString(Project.GridX));
					AppendCanonicalField(Canonical, TEXT("constructionGridY"), LexToString(Project.GridY));
					AppendCanonicalField(Canonical, TEXT("constructionRemainingSeconds"), LexToString(Project.RemainingBuildSeconds));
				}
			}
			if (Envelope.Header.FormatVersion >= 6)
			{
				AppendCanonicalField(Canonical, TEXT("personnelCount"), LexToString(Envelope.State.Personnel.Num()));
				for (const FPersonnelState& Person : Envelope.State.Personnel)
				{
					AppendCanonicalField(Canonical, TEXT("personnelId"), Person.PersonnelId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("personnelName"), Person.DisplayName);
					AppendCanonicalField(Canonical, TEXT("personnelRole"), Person.RoleId.ToString());
					AppendCanonicalField(Canonical, TEXT("personnelBase"), Person.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("personnelStatus"), PersonnelStatusToString(Person.Status));
					AppendCanonicalField(Canonical, TEXT("personnelRank"), LexToString(Person.Rank));
					AppendCanonicalField(Canonical, TEXT("personnelMissions"), LexToString(Person.Missions));
					AppendCanonicalField(Canonical, TEXT("personnelKills"), LexToString(Person.Kills));
					if (Envelope.Header.FormatVersion >= 18)
					{
						AppendCanonicalField(Canonical, TEXT("personnelExperience"), LexToString(Person.Experience));
					}
					AppendCanonicalField(Canonical, TEXT("personnelMaxHealth"), LexToString(Person.MaxHealth));
					AppendCanonicalField(Canonical, TEXT("personnelCurrentHealth"), LexToString(Person.CurrentHealth));
					AppendCanonicalField(Canonical, TEXT("personnelAccuracy"), LexToString(Person.Accuracy));
					AppendCanonicalField(Canonical, TEXT("personnelResolve"), LexToString(Person.Resolve));
					AppendCanonicalField(Canonical, TEXT("personnelMobility"), LexToString(Person.Mobility));
					AppendCanonicalField(Canonical, TEXT("personnelStrength"), LexToString(Person.Strength));
					AppendCanonicalField(Canonical, TEXT("personnelRecoverySeconds"), LexToString(Person.RemainingRecoverySeconds));
					if (Envelope.Header.FormatVersion >= 34)
					{
						AppendCanonicalField(Canonical, TEXT("personnelRecoveryPlan"),
							RecoveryPlanToString(Person.RecoveryPlan));
					}
					AppendCanonicalField(Canonical, TEXT("personnelTrainingSeconds"), LexToString(Person.RemainingTrainingSeconds));
					AppendCanonicalField(Canonical, TEXT("personnelTrainingFocus"), TrainingFocusToString(Person.TrainingFocus));
					if (Envelope.Header.FormatVersion >= 35)
					{
						AppendCanonicalField(Canonical, TEXT("personnelStewardshipFocus"),
							StewardshipFocusToString(Person.StewardshipFocus));
						AppendCanonicalField(Canonical, TEXT("personnelStewardshipSeconds"),
							LexToString(Person.RemainingStewardshipSeconds));
						AppendCanonicalField(Canonical, TEXT("personnelStewardshipTours"),
							LexToString(Person.StewardshipToursCompleted));
					}
					AppendCanonicalField(Canonical, TEXT("personnelEquipmentCount"), LexToString(Person.EquippedItems.Num()));
					for (const FName ItemId : Person.EquippedItems)
					{
						AppendCanonicalField(Canonical, TEXT("personnelEquipment"), ItemId.ToString());
					}
					if (Envelope.Header.FormatVersion >= 24)
					{
						AppendCanonicalField(Canonical, TEXT("personnelPendingDoctrineChoices"), LexToString(Person.PendingDoctrineChoices));
						AppendCanonicalField(Canonical, TEXT("personnelDoctrineSelectionCount"), LexToString(Person.DoctrineSelections.Num()));
						for (const FName DoctrineId : Person.DoctrineSelections)
						{
							AppendCanonicalField(Canonical, TEXT("personnelDoctrineSelection"), DoctrineId.ToString());
						}
						AppendCanonicalField(Canonical, TEXT("personnelCommendationCount"), LexToString(Person.Commendations.Num()));
						for (const FName CommendationId : Person.Commendations)
						{
							AppendCanonicalField(Canonical, TEXT("personnelCommendation"), CommendationId.ToString());
						}
					}
				}
				if (Envelope.Header.FormatVersion >= 33)
				{
					AppendCanonicalField(Canonical, TEXT("personnelSquadBondCount"),
						LexToString(Envelope.State.PersonnelSquadBonds.Num()));
					for (const FPersonnelSquadBondState& Bond : Envelope.State.PersonnelSquadBonds)
					{
						AppendCanonicalField(Canonical, TEXT("personnelSquadBondFirst"),
							Bond.FirstPersonnelId.ToString(EGuidFormats::DigitsWithHyphensLower));
						AppendCanonicalField(Canonical, TEXT("personnelSquadBondSecond"),
							Bond.SecondPersonnelId.ToString(EGuidFormats::DigitsWithHyphensLower));
						AppendCanonicalField(Canonical, TEXT("personnelSquadBondSharedVictories"),
							LexToString(Bond.SharedVictories));
					}
				}

				AppendCanonicalField(Canonical, TEXT("recruitmentOrderCount"), LexToString(Envelope.State.RecruitmentOrders.Num()));
				for (const FRecruitmentOrderState& Order : Envelope.State.RecruitmentOrders)
				{
					AppendCanonicalField(Canonical, TEXT("recruitmentOrderId"), Order.OrderId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("recruitmentPersonnelId"), Order.PersonnelId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("recruitmentName"), Order.DisplayName);
					AppendCanonicalField(Canonical, TEXT("recruitmentRole"), Order.RoleId.ToString());
					AppendCanonicalField(Canonical, TEXT("recruitmentBase"), Order.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("recruitmentTransitSeconds"), LexToString(Order.RemainingTransitSeconds));
				}

				AppendCanonicalField(Canonical, TEXT("memorialCount"), LexToString(Envelope.State.Memorial.Num()));
				for (const FMemorialRecord& Record : Envelope.State.Memorial)
				{
					AppendCanonicalField(Canonical, TEXT("memorialPersonnelId"), Record.PersonnelId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("memorialName"), Record.DisplayName);
					AppendCanonicalField(Canonical, TEXT("memorialRole"), Record.RoleId.ToString());
					AppendCanonicalField(Canonical, TEXT("memorialRank"), LexToString(Record.Rank));
					AppendCanonicalField(Canonical, TEXT("memorialMissions"), LexToString(Record.Missions));
					AppendCanonicalField(Canonical, TEXT("memorialKills"), LexToString(Record.Kills));
					AppendCanonicalField(Canonical, TEXT("memorialDeathTicks"), LexToString(Record.DeathUtc.GetTicks()));
					AppendCanonicalField(Canonical, TEXT("memorialCause"), Record.CauseId.ToString());
					if (Envelope.Header.FormatVersion >= 35)
					{
						AppendCanonicalField(Canonical, TEXT("memorialStewardshipTours"),
							LexToString(Record.StewardshipToursCompleted));
					}
					if (Envelope.Header.FormatVersion >= 24)
					{
						AppendCanonicalField(Canonical, TEXT("memorialDoctrineSelectionCount"), LexToString(Record.DoctrineSelections.Num()));
						for (const FName DoctrineId : Record.DoctrineSelections)
						{
							AppendCanonicalField(Canonical, TEXT("memorialDoctrineSelection"), DoctrineId.ToString());
						}
						AppendCanonicalField(Canonical, TEXT("memorialCommendationCount"), LexToString(Record.Commendations.Num()));
						for (const FName CommendationId : Record.Commendations)
						{
							AppendCanonicalField(Canonical, TEXT("memorialCommendation"), CommendationId.ToString());
						}
					}
				}
			}
			if (Envelope.Header.FormatVersion >= 7)
			{
				AppendCanonicalField(Canonical, TEXT("craftCount"), LexToString(Envelope.State.Craft.Num()));
				for (const FCraftState& Craft : Envelope.State.Craft)
				{
					AppendCanonicalField(Canonical, TEXT("craftId"), Craft.CraftId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("craftName"), Craft.DisplayName);
					AppendCanonicalField(Canonical, TEXT("craftRule"), Craft.CraftRuleId.ToString());
					AppendCanonicalField(Canonical, TEXT("craftBase"), Craft.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("craftPilot"), Craft.AssignedPilotId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("craftStatus"), CraftStatusToString(Craft.Status));
					AppendCanonicalField(Canonical, TEXT("craftHull"), LexToString(Craft.CurrentHull));
					AppendCanonicalField(Canonical, TEXT("craftFuel"), LexToString(Craft.CurrentFuel));
					AppendCanonicalField(Canonical, TEXT("craftRepairSeconds"), LexToString(Craft.RemainingRepairSeconds));
					AppendCanonicalField(Canonical, TEXT("craftRefuelSeconds"), LexToString(Craft.RemainingRefuelSeconds));
					AppendCanonicalField(Canonical, TEXT("craftSorties"), LexToString(Craft.CompletedSorties));
					AppendCanonicalField(Canonical, TEXT("craftEquipmentCount"), LexToString(Craft.EquipmentItems.Num()));
					for (const FName ItemId : Craft.EquipmentItems)
					{
						AppendCanonicalField(Canonical, TEXT("craftEquipment"), ItemId.ToString());
					}
					if (Envelope.Header.FormatVersion >= 8)
					{
						AppendCanonicalField(Canonical, TEXT("craftTargetContact"), Craft.TargetContactId.ToString(EGuidFormats::DigitsWithHyphensLower));
						AppendCanonicalField(Canonical, TEXT("craftRouteSeconds"), LexToString(Craft.RemainingRouteSeconds));
						AppendCanonicalField(Canonical, TEXT("craftReturnSeconds"), LexToString(Craft.ReservedReturnSeconds));
					}
					if (Envelope.Header.FormatVersion >= 9)
					{
						AppendCanonicalField(Canonical, TEXT("craftWeaponStateCount"), LexToString(Craft.WeaponStates.Num()));
						for (const FCraftWeaponState& WeaponState : Craft.WeaponStates)
						{
							AppendCanonicalField(Canonical, TEXT("craftWeaponItem"), WeaponState.WeaponItemId.ToString());
							AppendCanonicalField(Canonical, TEXT("craftWeaponAmmunition"), LexToString(WeaponState.Ammunition));
							AppendCanonicalField(Canonical, TEXT("craftWeaponCooldownSeconds"), LexToString(WeaponState.RemainingCooldownSeconds));
						}
					}
					if (Envelope.Header.FormatVersion >= 11)
					{
						AppendCanonicalField(Canonical, TEXT("craftAgentCount"), LexToString(Craft.AssignedAgentIds.Num()));
						for (const FGuid& AgentId : Craft.AssignedAgentIds)
						{
							AppendCanonicalField(Canonical, TEXT("craftAgent"), AgentId.ToString(EGuidFormats::DigitsWithHyphensLower));
						}
						AppendCanonicalField(Canonical, TEXT("craftCargoCount"), LexToString(Craft.Cargo.Num()));
						for (const FInventoryStack& Stack : Craft.Cargo)
						{
							AppendCanonicalField(Canonical, TEXT("craftCargoItem"), Stack.ItemId.ToString());
							AppendCanonicalField(Canonical, TEXT("craftCargoQuantity"), LexToString(Stack.Quantity));
						}
						AppendCanonicalField(Canonical, TEXT("craftTargetSite"), Craft.TargetSiteId.ToString(EGuidFormats::DigitsWithHyphensLower));
						if (Envelope.Header.FormatVersion >= 26)
						{
							AppendCanonicalField(Canonical, TEXT("craftPendingSalvageCount"), LexToString(Craft.PendingSalvage.Num()));
							for (const FInventoryStack& Stack : Craft.PendingSalvage)
							{
								AppendCanonicalField(Canonical, TEXT("craftPendingSalvageItem"), Stack.ItemId.ToString());
								AppendCanonicalField(Canonical, TEXT("craftPendingSalvageQuantity"), LexToString(Stack.Quantity));
							}
						}
					}
				}
				AppendCanonicalField(Canonical, TEXT("craftAcquisitionCount"), LexToString(Envelope.State.CraftAcquisitionOrders.Num()));
				for (const FCraftAcquisitionOrderState& Order : Envelope.State.CraftAcquisitionOrders)
				{
					AppendCanonicalField(Canonical, TEXT("craftOrderId"), Order.OrderId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("craftOrderCraftId"), Order.CraftId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("craftOrderName"), Order.DisplayName);
					AppendCanonicalField(Canonical, TEXT("craftOrderRule"), Order.CraftRuleId.ToString());
					AppendCanonicalField(Canonical, TEXT("craftOrderBase"), Order.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("craftOrderTransitSeconds"), LexToString(Order.RemainingTransitSeconds));
				}
				if (Envelope.Header.FormatVersion >= 8)
				{
					AppendCanonicalField(Canonical, TEXT("strategicContactCount"), LexToString(Envelope.State.StrategicContacts.Num()));
					for (const FStrategicContactState& Contact : Envelope.State.StrategicContacts)
					{
						AppendCanonicalField(Canonical, TEXT("contactId"), Contact.ContactId.ToString(EGuidFormats::DigitsWithHyphensLower));
						AppendCanonicalField(Canonical, TEXT("contactRule"), Contact.ContactRuleId.ToString());
						AppendCanonicalField(Canonical, TEXT("contactStatus"), ContactStatusToString(Contact.Status));
						AppendCanonicalField(Canonical, TEXT("contactOriginLongitude"), LexToString(Contact.OriginLongitudeMilliDegrees));
						AppendCanonicalField(Canonical, TEXT("contactOriginLatitude"), LexToString(Contact.OriginLatitudeMilliDegrees));
						AppendCanonicalField(Canonical, TEXT("contactLongitude"), LexToString(Contact.LongitudeMilliDegrees));
						AppendCanonicalField(Canonical, TEXT("contactLatitude"), LexToString(Contact.LatitudeMilliDegrees));
						AppendCanonicalField(Canonical, TEXT("contactDestinationLongitude"), LexToString(Contact.DestinationLongitudeMilliDegrees));
						AppendCanonicalField(Canonical, TEXT("contactDestinationLatitude"), LexToString(Contact.DestinationLatitudeMilliDegrees));
						AppendCanonicalField(Canonical, TEXT("contactTotalRouteSeconds"), LexToString(Contact.TotalRouteSeconds));
						AppendCanonicalField(Canonical, TEXT("contactElapsedRouteSeconds"), LexToString(Contact.ElapsedRouteSeconds));
						AppendCanonicalField(Canonical, TEXT("contactHull"), LexToString(Contact.CurrentHull));
						if (Envelope.Header.FormatVersion >= 9)
						{
							AppendCanonicalField(Canonical, TEXT("contactCombatRounds"), LexToString(Contact.CompletedCombatRounds));
							AppendCanonicalField(Canonical, TEXT("contactAttackCooldownSeconds"), LexToString(Contact.RemainingAttackCooldownSeconds));
						}
					}
					if (Envelope.Header.FormatVersion >= 9)
					{
						AppendCanonicalField(Canonical, TEXT("strategicSiteCount"), LexToString(Envelope.State.StrategicSites.Num()));
						for (const FStrategicSiteState& Site : Envelope.State.StrategicSites)
						{
							AppendCanonicalField(Canonical, TEXT("siteId"), Site.SiteId.ToString(EGuidFormats::DigitsWithHyphensLower));
							AppendCanonicalField(Canonical, TEXT("siteType"), SiteTypeToString(Site.Type));
							AppendCanonicalField(Canonical, TEXT("siteSourceContactRule"), Site.SourceContactRuleId.ToString());
							AppendCanonicalField(Canonical, TEXT("siteLongitude"), LexToString(Site.LongitudeMilliDegrees));
							AppendCanonicalField(Canonical, TEXT("siteLatitude"), LexToString(Site.LatitudeMilliDegrees));
							AppendCanonicalField(Canonical, TEXT("siteThreat"), LexToString(Site.ThreatRating));
							AppendCanonicalField(Canonical, TEXT("siteLifetimeSeconds"), LexToString(Site.RemainingLifetimeSeconds));
						}
					}
				}
			}
		}
		if (Envelope.Header.FormatVersion >= 10)
		{
			AppendCanonicalField(Canonical, TEXT("adversaryEscalation"), LexToString(Envelope.State.AdversaryEscalationLevel));
			AppendCanonicalField(Canonical, TEXT("nextAdversaryMissionSeconds"), LexToString(Envelope.State.NextAdversaryMissionSeconds));
			AppendCanonicalField(Canonical, TEXT("nextAdversaryMissionSerial"), LexToString(Envelope.State.NextAdversaryMissionSerial));
			AppendCanonicalField(Canonical, TEXT("adversaryMissionsLaunched"), LexToString(Envelope.State.AdversaryMissionsLaunched));
			AppendCanonicalField(Canonical, TEXT("adversaryMissionsEscaped"), LexToString(Envelope.State.AdversaryMissionsEscaped));
			AppendCanonicalField(Canonical, TEXT("adversaryMissionsThwarted"), LexToString(Envelope.State.AdversaryMissionsThwarted));
			AppendCanonicalField(Canonical, TEXT("regionalPressureCount"), LexToString(Envelope.State.RegionalPressure.Num()));
			for (const FRegionalPressureState& Pressure : Envelope.State.RegionalPressure)
			{
				AppendCanonicalField(Canonical, TEXT("pressureRegion"), Pressure.RegionId.ToString());
				AppendCanonicalField(Canonical, TEXT("pressureValue"), LexToString(Pressure.Pressure));
			}
			if (Envelope.Header.FormatVersion >= 23)
			{
				AppendCanonicalField(Canonical, TEXT("regionalMandateCount"), LexToString(Envelope.State.RegionalMandates.Num()));
				for (const FRegionalMandateState& Mandate : Envelope.State.RegionalMandates)
				{
					AppendCanonicalField(Canonical, TEXT("mandateRegion"), Mandate.RegionId.ToString());
					AppendCanonicalField(Canonical, TEXT("mandateSupport"), LexToString(Mandate.Support));
					AppendCanonicalField(Canonical, TEXT("mandateBaselineFunding"), LexToString(Mandate.BaselineMonthlyFunding));
					AppendCanonicalField(Canonical, TEXT("mandateCurrentFunding"), LexToString(Mandate.CurrentMonthlyFunding));
					AppendCanonicalField(Canonical, TEXT("mandateLastActionMonth"), LexToString(Mandate.LastDiplomaticActionMonth));
					if (Envelope.Header.FormatVersion >= 28)
					{
						AppendCanonicalField(Canonical, TEXT("mandateResilienceCharterSigned"),
							Mandate.bResilienceCharterSigned ? TEXT("true") : TEXT("false"));
					}
					if (Envelope.Header.FormatVersion >= 31)
					{
						AppendCanonicalField(Canonical, TEXT("mandateHorizonCompactMemberWithdrawn"),
							Mandate.bHorizonCompactMemberWithdrawn ? TEXT("true") : TEXT("false"));
					}
				}
			}
			if (Envelope.Header.FormatVersion >= 29)
			{
				AppendCanonicalField(Canonical, TEXT("horizonCompactRatified"),
					Envelope.State.bHorizonCompactRatified ? TEXT("true") : TEXT("false"));
			}
			if (Envelope.Header.FormatVersion >= 30)
			{
				AppendCanonicalField(Canonical, TEXT("lastCoalitionAidMonth"),
					LexToString(Envelope.State.LastCoalitionAidMonth));
			}
			if (Envelope.Header.FormatVersion >= 32)
			{
				AppendCanonicalField(Canonical, TEXT("lastCoalitionEmergencyVoteMonth"),
					LexToString(Envelope.State.LastCoalitionEmergencyVoteMonth));
			}
			AppendCanonicalField(Canonical, TEXT("adversaryMissionCount"), LexToString(Envelope.State.AdversaryMissions.Num()));
			for (const FAdversaryMissionState& Mission : Envelope.State.AdversaryMissions)
			{
				AppendCanonicalField(Canonical, TEXT("adversaryMissionId"), Mission.MissionId.ToString(EGuidFormats::DigitsWithHyphensLower));
				AppendCanonicalField(Canonical, TEXT("adversaryMissionContactId"), Mission.ContactId.ToString(EGuidFormats::DigitsWithHyphensLower));
				AppendCanonicalField(Canonical, TEXT("adversaryMissionRule"), Mission.MissionRuleId.ToString());
				if (Envelope.Header.FormatVersion >= 20)
				{
					AppendCanonicalField(Canonical, TEXT("adversaryMissionTargetBaseId"),
						Mission.TargetBaseId.IsValid()
							? Mission.TargetBaseId.ToString(EGuidFormats::DigitsWithHyphensLower)
							: FString());
				}
				AppendCanonicalField(Canonical, TEXT("adversaryMissionStartedTicks"), LexToString(Mission.StartedUtc.GetTicks()));
			}
			if (Envelope.Header.FormatVersion >= 20)
			{
				AppendCanonicalField(Canonical, TEXT("baseAssaultCount"), LexToString(Envelope.State.BaseAssaults.Num()));
				for (const FBaseAssaultState& Assault : Envelope.State.BaseAssaults)
				{
					AppendCanonicalField(Canonical, TEXT("baseAssaultId"), Assault.AssaultId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("baseAssaultMissionId"), Assault.MissionId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("baseAssaultContactId"), Assault.ContactId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("baseAssaultBaseId"), Assault.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("baseAssaultArrivedTicks"), LexToString(Assault.ArrivedUtc.GetTicks()));
				}
			}
			AppendCanonicalField(Canonical, TEXT("campaignOutcome"), CampaignOutcomeToString(Envelope.State.Outcome));
			AppendCanonicalField(Canonical, TEXT("campaignOutcomeReason"), Envelope.State.OutcomeReasonId.IsNone() ? FString() : Envelope.State.OutcomeReasonId.ToString());
		}
		if (Envelope.Header.FormatVersion >= 11)
		{
			AppendCanonicalField(Canonical, TEXT("tacticalOperationCount"), LexToString(Envelope.State.TacticalOperations.Num()));
			for (const FTacticalOperationState& Operation : Envelope.State.TacticalOperations)
			{
				AppendCanonicalField(Canonical, TEXT("tacticalOperationId"), Operation.OperationId.ToString(EGuidFormats::DigitsWithHyphensLower));
				if (Envelope.Header.FormatVersion >= 21)
				{
					AppendCanonicalField(Canonical, TEXT("tacticalOperationType"), TacticalOperationTypeToString(Operation.Type));
					AppendCanonicalField(Canonical, TEXT("tacticalBaseId"), Operation.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("tacticalAssaultId"), Operation.AssaultId.ToString(EGuidFormats::DigitsWithHyphensLower));
				}
				AppendCanonicalField(Canonical, TEXT("tacticalSiteId"), Operation.SiteId.ToString(EGuidFormats::DigitsWithHyphensLower));
				AppendCanonicalField(Canonical, TEXT("tacticalCraftId"), Operation.CraftId.ToString(EGuidFormats::DigitsWithHyphensLower));
				AppendCanonicalField(Canonical, TEXT("tacticalSeed"), LexToString(Operation.TacticalSeed));
				AppendCanonicalField(Canonical, TEXT("tacticalCreatedTicks"), LexToString(Operation.CreatedUtc.GetTicks()));
				AppendCanonicalField(Canonical, TEXT("tacticalAgentCount"), LexToString(Operation.AgentIds.Num()));
				for (const FGuid& AgentId : Operation.AgentIds)
				{
					AppendCanonicalField(Canonical, TEXT("tacticalAgent"), AgentId.ToString(EGuidFormats::DigitsWithHyphensLower));
				}
				AppendCanonicalField(Canonical, TEXT("tacticalCargoCount"), LexToString(Operation.Cargo.Num()));
				for (const FInventoryStack& Stack : Operation.Cargo)
				{
					AppendCanonicalField(Canonical, TEXT("tacticalCargoItem"), Stack.ItemId.ToString());
					AppendCanonicalField(Canonical, TEXT("tacticalCargoQuantity"), LexToString(Stack.Quantity));
				}
			}
		}
		if (Envelope.Header.FormatVersion >= 12)
		{
			AppendCanonicalField(Canonical, TEXT("tacticalBattleCount"), LexToString(Envelope.State.TacticalBattles.Num()));
			for (const FTacticalBattleState& Battle : Envelope.State.TacticalBattles)
			{
				AppendCanonicalField(Canonical, TEXT("battleId"), Battle.BattleId.ToString(EGuidFormats::DigitsWithHyphensLower));
				AppendCanonicalField(Canonical, TEXT("battleOperationId"), Battle.OperationId.ToString(EGuidFormats::DigitsWithHyphensLower));
				AppendCanonicalField(Canonical, TEXT("battleSiteId"), Battle.SiteId.ToString(EGuidFormats::DigitsWithHyphensLower));
				AppendCanonicalField(Canonical, TEXT("battleMissionRule"), Battle.MissionRuleId.ToString());
				AppendCanonicalField(Canonical, TEXT("battleCreatedTicks"), LexToString(Battle.CreatedUtc.GetTicks()));
				AppendCanonicalField(Canonical, TEXT("battleWidth"), LexToString(Battle.Width));
				AppendCanonicalField(Canonical, TEXT("battleHeight"), LexToString(Battle.Height));
				if (Envelope.Header.FormatVersion >= 17)
				{
					AppendCanonicalField(Canonical, TEXT("battleLevels"), LexToString(Battle.Levels));
				}
				AppendCanonicalField(Canonical, TEXT("battleTurnLimit"), LexToString(Battle.TurnLimit));
				AppendCanonicalField(Canonical, TEXT("battleTurnNumber"), LexToString(Battle.TurnNumber));
				if (Envelope.Header.FormatVersion >= 21)
				{
					AppendCanonicalField(Canonical, TEXT("battleRequiresExtraction"), Battle.bRequiresExtraction ? TEXT("1") : TEXT("0"));
				}
				AppendCanonicalField(Canonical, TEXT("battlePhase"), TacticalPhaseToString(Battle.Phase));
				AppendCanonicalField(Canonical, TEXT("battleActiveTeam"), TacticalTeamToString(Battle.ActiveTeam));
				if (Envelope.Header.FormatVersion >= 16)
				{
					AppendCanonicalField(Canonical, TEXT("battleWindDirection"), TacticalWindDirectionToString(Battle.WindDirection));
					AppendCanonicalField(Canonical, TEXT("battleWindStrength"), LexToString(Battle.WindStrength));
				}
				AppendCanonicalField(Canonical, TEXT("battleRandomInitialSeed"), LexToString(Battle.TacticalRandom.InitialSeed));
				AppendCanonicalField(Canonical, TEXT("battleRandomDrawCount"), LexToString(Battle.TacticalRandom.DrawCount));
				AppendCanonicalField(Canonical, TEXT("battleRandomState"), UInt64ToHex(Battle.TacticalRandom.GetStateForSave()));
				AppendCanonicalField(Canonical, TEXT("battleCellCount"), LexToString(Battle.Cells.Num()));
				for (const FTacticalCellState& Cell : Battle.Cells)
				{
					AppendCanonicalField(Canonical, TEXT("battleCellX"), LexToString(Cell.X));
					AppendCanonicalField(Canonical, TEXT("battleCellY"), LexToString(Cell.Y));
					if (Envelope.Header.FormatVersion >= 17)
					{
						AppendCanonicalField(Canonical, TEXT("battleCellZ"), LexToString(Cell.Z));
					}
					AppendCanonicalField(Canonical, TEXT("battleCellTerrain"), Cell.TerrainRuleId.ToString());
					AppendCanonicalField(Canonical, TEXT("battleCellIntegrity"), LexToString(Cell.CurrentIntegrity));
					AppendCanonicalField(Canonical, TEXT("battleCellDeployment"), Cell.bPlayerDeployment ? TEXT("1") : TEXT("0"));
					AppendCanonicalField(Canonical, TEXT("battleCellExtraction"), Cell.bExtraction ? TEXT("1") : TEXT("0"));
					if (Envelope.Header.FormatVersion >= 15)
					{
						AppendCanonicalField(Canonical, TEXT("battleCellDoorOpen"), Cell.bDoorOpen ? TEXT("1") : TEXT("0"));
					}
					if (Envelope.Header.FormatVersion >= 13)
					{
						AppendCanonicalField(Canonical, TEXT("battleCellSmoke"), LexToString(Cell.Smoke));
						AppendCanonicalField(Canonical, TEXT("battleCellFire"), LexToString(Cell.Fire));
					}
				}
				if (Envelope.Header.FormatVersion >= 25)
				{
					AppendCanonicalField(Canonical, TEXT("battlePlayerDiscoveredCellCount"), LexToString(Battle.PlayerDiscoveredCellIndices.Num()));
					for (const int32 CellIndex : Battle.PlayerDiscoveredCellIndices)
					{
						AppendCanonicalField(Canonical, TEXT("battlePlayerDiscoveredCellIndex"), LexToString(CellIndex));
					}
				}
				AppendCanonicalField(Canonical, TEXT("battleUnitCount"), LexToString(Battle.Units.Num()));
				for (const FTacticalUnitState& Unit : Battle.Units)
				{
					AppendCanonicalField(Canonical, TEXT("battleUnitId"), Unit.UnitId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("battleUnitPersonnelId"), Unit.PersonnelId.ToString(EGuidFormats::DigitsWithHyphensLower));
					AppendCanonicalField(Canonical, TEXT("battleUnitRule"), Unit.SourceRuleId.ToString());
					AppendCanonicalField(Canonical, TEXT("battleUnitName"), Unit.DisplayName);
					AppendCanonicalField(Canonical, TEXT("battleUnitTeam"), TacticalTeamToString(Unit.Team));
					if (Envelope.Header.FormatVersion >= 14)
					{
						AppendCanonicalField(Canonical, TEXT("battleUnitStance"), TacticalStanceToString(Unit.Stance));
					}
					AppendCanonicalField(Canonical, TEXT("battleUnitX"), LexToString(Unit.X));
					AppendCanonicalField(Canonical, TEXT("battleUnitY"), LexToString(Unit.Y));
					if (Envelope.Header.FormatVersion >= 17)
					{
						AppendCanonicalField(Canonical, TEXT("battleUnitZ"), LexToString(Unit.Z));
					}
					AppendCanonicalField(Canonical, TEXT("battleUnitMaxHealth"), LexToString(Unit.MaxHealth));
					AppendCanonicalField(Canonical, TEXT("battleUnitCurrentHealth"), LexToString(Unit.CurrentHealth));
					AppendCanonicalField(Canonical, TEXT("battleUnitAccuracy"), LexToString(Unit.Accuracy));
					AppendCanonicalField(Canonical, TEXT("battleUnitResolve"), LexToString(Unit.Resolve));
					AppendCanonicalField(Canonical, TEXT("battleUnitMobility"), LexToString(Unit.Mobility));
					AppendCanonicalField(Canonical, TEXT("battleUnitStrength"), LexToString(Unit.Strength));
					AppendCanonicalField(Canonical, TEXT("battleUnitMaxActionPoints"), LexToString(Unit.MaxActionPoints));
					AppendCanonicalField(Canonical, TEXT("battleUnitRemainingActionPoints"), LexToString(Unit.RemainingActionPoints));
					AppendCanonicalField(Canonical, TEXT("battleUnitExtracted"), Unit.bExtracted ? TEXT("1") : TEXT("0"));
					if (Envelope.Header.FormatVersion >= 13)
					{
						AppendCanonicalField(Canonical, TEXT("battleUnitKineticArmor"), LexToString(Unit.KineticArmor));
						AppendCanonicalField(Canonical, TEXT("battleUnitThermalArmor"), LexToString(Unit.ThermalArmor));
						AppendCanonicalField(Canonical, TEXT("battleUnitArcArmor"), LexToString(Unit.ArcArmor));
						AppendCanonicalField(Canonical, TEXT("battleUnitMaxMorale"), LexToString(Unit.MaxMorale));
						AppendCanonicalField(Canonical, TEXT("battleUnitCurrentMorale"), LexToString(Unit.CurrentMorale));
						AppendCanonicalField(Canonical, TEXT("battleUnitSuppression"), LexToString(Unit.Suppression));
						AppendCanonicalField(Canonical, TEXT("battleUnitWeaponStateCount"), LexToString(Unit.WeaponStates.Num()));
						for (const FTacticalWeaponState& WeaponState : Unit.WeaponStates)
						{
							AppendCanonicalField(Canonical, TEXT("battleUnitWeaponItem"), WeaponState.WeaponItemId.ToString());
							AppendCanonicalField(Canonical, TEXT("battleUnitWeaponLoadedAmmunition"), LexToString(WeaponState.LoadedAmmunition));
						}
						AppendCanonicalField(Canonical, TEXT("battleUnitCarriedItemCount"), LexToString(Unit.CarriedItems.Num()));
						for (const FInventoryStack& Stack : Unit.CarriedItems)
						{
							AppendCanonicalField(Canonical, TEXT("battleUnitCarriedItem"), Stack.ItemId.ToString());
							AppendCanonicalField(Canonical, TEXT("battleUnitCarriedQuantity"), LexToString(Stack.Quantity));
						}
						if (Envelope.Header.FormatVersion >= 27)
						{
							AppendCanonicalField(Canonical, TEXT("battleUnitEjectedMagazineCount"), LexToString(Unit.EjectedMagazines.Num()));
							for (const FTacticalMagazineState& Magazine : Unit.EjectedMagazines)
							{
								AppendCanonicalField(Canonical, TEXT("battleUnitEjectedMagazineWeapon"), Magazine.WeaponItemId.ToString());
								AppendCanonicalField(Canonical, TEXT("battleUnitEjectedMagazineItem"), Magazine.AmmunitionItemId.ToString());
								AppendCanonicalField(Canonical, TEXT("battleUnitEjectedMagazineAmmunition"), LexToString(Magazine.LoadedAmmunition));
							}
						}
					}
				}
				AppendCanonicalField(Canonical, TEXT("battleObjectiveCount"), LexToString(Battle.Objectives.Num()));
				for (const FTacticalObjectiveState& Objective : Battle.Objectives)
				{
					AppendCanonicalField(Canonical, TEXT("battleObjectiveId"), Objective.ObjectiveId.ToString());
					AppendCanonicalField(Canonical, TEXT("battleObjectiveX"), LexToString(Objective.X));
					AppendCanonicalField(Canonical, TEXT("battleObjectiveY"), LexToString(Objective.Y));
					if (Envelope.Header.FormatVersion >= 17)
					{
						AppendCanonicalField(Canonical, TEXT("battleObjectiveZ"), LexToString(Objective.Z));
					}
					AppendCanonicalField(Canonical, TEXT("battleObjectiveStatus"), TacticalObjectiveStatusToString(Objective.Status));
					if (Envelope.Header.FormatVersion >= 18)
					{
						AppendCanonicalField(Canonical, TEXT("battleObjectiveType"), TacticalObjectiveTypeToString(Objective.Type));
					}
					AppendCanonicalField(Canonical, TEXT("battleObjectiveRequired"), LexToString(Objective.RequiredInteractions));
					AppendCanonicalField(Canonical, TEXT("battleObjectiveCompleted"), LexToString(Objective.CompletedInteractions));
					if (Envelope.Header.FormatVersion >= 18)
					{
						AppendCanonicalField(Canonical, TEXT("battleObjectiveAdversary"), LexToString(Objective.AdversaryInteractions));
					}
				}
				AppendCanonicalField(Canonical, TEXT("battleCargoCount"), LexToString(Battle.Cargo.Num()));
				for (const FInventoryStack& Stack : Battle.Cargo)
				{
					AppendCanonicalField(Canonical, TEXT("battleCargoItem"), Stack.ItemId.ToString());
					AppendCanonicalField(Canonical, TEXT("battleCargoQuantity"), LexToString(Stack.Quantity));
				}
			}
		}
		return Canonical;
	}

	FString ComputeSaveChecksum(const FCampaignSaveEnvelope& Envelope)
	{
		return HashString(BuildSaveCanonical(Envelope));
	}

	bool IsUsableWallClock(const FDateTime& Value)
	{
		return Value > FDateTime::MinValue() && Value < FDateTime::MaxValue();
	}

	bool IsValidDifficulty(const ECampaignDifficulty Difficulty)
	{
		return Difficulty == ECampaignDifficulty::Cadet
			|| Difficulty == ECampaignDifficulty::Standard
			|| Difficulty == ECampaignDifficulty::Veteran
			|| Difficulty == ECampaignDifficulty::Apex;
	}

	bool AreValidCoordinates(const int32 LongitudeMilliDegrees, const int32 LatitudeMilliDegrees)
	{
		return LongitudeMilliDegrees >= -180000 && LongitudeMilliDegrees <= 180000
			&& LatitudeMilliDegrees >= -90000 && LatitudeMilliDegrees <= 90000;
	}

	FCampaignSaveValidationResult ValidateInternal(
		const FCampaignSaveEnvelope& Envelope,
		const TArray<FCampaignContentVersion>* ExpectedContentPackages,
		const bool bAllowLegacyMissingChecksum)
	{
		FCampaignSaveValidationResult Result;
		const FCampaignSaveHeader& Header = Envelope.Header;
		const FCampaignState& State = Envelope.State;

		if (Header.FormatVersion < FCampaignSaveCodec::OldestSupportedFormatVersion
			|| Header.FormatVersion > FCampaignSaveCodec::CurrentFormatVersion)
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("unsupported_format_version"), FString::Printf(TEXT("Campaign save format %d is unsupported; supported range is %d-%d."), Header.FormatVersion, FCampaignSaveCodec::OldestSupportedFormatVersion, FCampaignSaveCodec::CurrentFormatVersion));
		}
		if (!Header.CampaignId.IsValid())
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_campaign_id"), TEXT("Campaign id is missing or invalid."));
		}
		if (!IsUsableWallClock(Header.CreatedUtc) || !IsUsableWallClock(Header.LastSavedUtc) || Header.LastSavedUtc < Header.CreatedUtc)
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_save_time"), TEXT("Creation and save timestamps must be usable UTC values in chronological order."));
		}
		if (Header.BuildVersion.TrimStartAndEnd().IsEmpty())
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_build_version"), TEXT("Build version cannot be empty."));
		}

		TSet<FName> SeenPackages;
		if (Header.ContentPackages.IsEmpty())
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_content_packages"), TEXT("Campaign save must declare at least one content package."));
		}
		for (const FCampaignContentVersion& Package : Header.ContentPackages)
		{
			if (!FContentPackageResolver::IsValidPackageId(Package.PackageId))
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_content_package"), FString::Printf(TEXT("Content package id '%s' is invalid."), *Package.PackageId.ToString()));
			}
			if (Package.Version.TrimStartAndEnd().IsEmpty())
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_content_package"), FString::Printf(TEXT("Content package '%s' has an empty version."), *Package.PackageId.ToString()));
			}
			if (SeenPackages.Contains(Package.PackageId))
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("duplicate_content_package"), FString::Printf(TEXT("Content package '%s' appears more than once."), *Package.PackageId.ToString()));
			}
			SeenPackages.Add(Package.PackageId);
		}

		const FString ComputedContentFingerprint = FCampaignSaveCodec::ComputeContentFingerprint(Header.ContentPackages);
		if (ComputedContentFingerprint.IsEmpty() || Header.ContentFingerprint != ComputedContentFingerprint)
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("content_fingerprint_mismatch"), TEXT("The save's content-package fingerprint does not match its package list."));
		}
		if (ExpectedContentPackages != nullptr
			&& Header.ContentFingerprint != FCampaignSaveCodec::ComputeContentFingerprint(*ExpectedContentPackages))
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("incompatible_content"), TEXT("The active content-package set does not match the campaign save."));
		}

		if (!State.StrategicTime.IsUsable())
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_strategic_time"), TEXT("Strategic timestamp is invalid."));
		}
		if (!State.SimulationRandom.IsValid())
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_random_state"), TEXT("Deterministic random state is invalid."));
		}
		if (!IsValidDifficulty(State.Difficulty))
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_difficulty"), TEXT("Campaign difficulty is outside the save schema."));
		}
		if (State.CommandSequence < 0)
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_command_sequence"), TEXT("Command sequence cannot be negative."));
		}

		TSet<FName> SeenResearch;
		for (const FName ResearchId : State.CompletedResearch)
		{
			if (!FContentPackageResolver::IsValidPackageId(ResearchId))
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_research_id"), FString::Printf(TEXT("Completed research id '%s' is invalid."), *ResearchId.ToString()));
			}
			if (SeenResearch.Contains(ResearchId))
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("duplicate_research_id"), FString::Printf(TEXT("Completed research id '%s' appears more than once."), *ResearchId.ToString()));
			}
			SeenResearch.Add(ResearchId);
		}

		if (Header.FormatVersion >= 3)
		{
			TSet<FGuid> SeenBases;
			TSet<FGuid> SeenFacilityInstances;
			for (const FStrategicBaseState& Base : State.Bases)
			{
				if (!Base.BaseId.IsValid() || SeenBases.Contains(Base.BaseId))
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_base_id"), TEXT("Strategic base ids must be valid and unique."));
				}
				SeenBases.Add(Base.BaseId);
				if (Base.Name.TrimStartAndEnd().IsEmpty() || Base.Name.Len() > 64)
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_base_name"), TEXT("Strategic base names must contain 1-64 non-whitespace characters."));
				}
				if (!FContentPackageResolver::IsValidPackageId(Base.RegionId))
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_region_id"), FString::Printf(TEXT("Base '%s' has an invalid region id."), *Base.Name));
				}
				if (Base.LongitudeMilliDegrees < -180000 || Base.LongitudeMilliDegrees > 180000
					|| Base.LatitudeMilliDegrees < -90000 || Base.LatitudeMilliDegrees > 90000)
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_base_coordinates"), FString::Printf(TEXT("Base '%s' has out-of-range coordinates."), *Base.Name));
				}
				if (Base.ScientistCapacity < 0 || Base.EngineerCapacity < 0
					|| (Header.FormatVersion >= 39 && Base.SignalWatchScientists < 0))
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_base_capacity"), FString::Printf(TEXT("Base '%s' has a negative staff capacity."), *Base.Name));
				}
				if (Header.FormatVersion >= 42
					&& (Base.WorksCadreEngineers < 0 || Base.WorksCadreEngineers > 3))
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
						TEXT("invalid_base_capacity"), FString::Printf(
							TEXT("Base '%s' has Works Cadre staffing outside the supported range 0-3."),
							*Base.Name));
				}
				const bool bKnownWorksCadreCharter =
					Base.WorksCadreCharter == EWorksCadreCharter::CommonCadence
					|| Base.WorksCadreCharter == EWorksCadreCharter::AssemblyCadence
					|| Base.WorksCadreCharter == EWorksCadreCharter::RestorationCadence;
				if (Header.FormatVersion >= 43 && !bKnownWorksCadreCharter)
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
						TEXT("invalid_works_cadre_charter"), FString::Printf(
							TEXT("Base '%s' has an unknown Works Charter."), *Base.Name));
				}
				if (Header.FormatVersion < 39 && Base.SignalWatchScientists != 0)
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
						TEXT("invalid_base_capacity"), FString::Printf(
							TEXT("Base '%s' contains Signal Watch staffing before campaign-save format 39."),
							*Base.Name));
				}
				if (Header.FormatVersion < 42 && Base.WorksCadreEngineers != 0)
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
						TEXT("invalid_base_capacity"), FString::Printf(
							TEXT("Base '%s' contains Works Cadre staffing before campaign-save format 42."),
							*Base.Name));
				}
				if (Header.FormatVersion < 43
					&& Base.WorksCadreCharter != EWorksCadreCharter::CommonCadence)
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
						TEXT("invalid_works_cadre_charter"), FString::Printf(
							TEXT("Base '%s' contains a Works Charter before campaign-save format 43."),
							*Base.Name));
				}
				TSet<FName> SeenFacilities;
				for (const FName FacilityId : Base.BuiltFacilities)
				{
					if (!FContentPackageResolver::IsValidPackageId(FacilityId) || SeenFacilities.Contains(FacilityId))
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_base_facility"), FString::Printf(TEXT("Base '%s' has an invalid or duplicate facility id '%s'."), *Base.Name, *FacilityId.ToString()));
					}
					SeenFacilities.Add(FacilityId);
				}
				if (Header.FormatVersion >= 5)
				{
					for (const FBaseFacilityState& Facility : Base.Facilities)
					{
						if (!Facility.InstanceId.IsValid() || SeenFacilityInstances.Contains(Facility.InstanceId)
							|| !FContentPackageResolver::IsValidPackageId(Facility.FacilityId)
							|| Facility.GridX < 0 || Facility.GridY < 0
							|| (Header.FormatVersion >= 19
								&& (Facility.Damage < 0 || Facility.ReservedRepairDamage < 0
									|| Facility.ReservedRepairDamage > Facility.Damage
									|| Facility.RemainingRepairSeconds < 0
									|| ((Facility.ReservedRepairDamage == 0) != (Facility.RemainingRepairSeconds == 0)))))
						{
							AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_facility_placement"), FString::Printf(TEXT("Base '%s' has an invalid positioned facility '%s'."), *Base.Name, *Facility.FacilityId.ToString()));
						}
						SeenFacilityInstances.Add(Facility.InstanceId);
					}
				}
				if (Header.FormatVersion >= 4)
				{
					TSet<FName> SeenItems;
					for (const FInventoryStack& Stack : Base.Inventory)
					{
						if (!FContentPackageResolver::IsValidPackageId(Stack.ItemId) || SeenItems.Contains(Stack.ItemId) || Stack.Quantity <= 0)
						{
							AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_inventory_stack"), FString::Printf(TEXT("Base '%s' has invalid inventory for '%s'."), *Base.Name, *Stack.ItemId.ToString()));
						}
						SeenItems.Add(Stack.ItemId);
					}
				}
			}

			if (Header.FormatVersion < 36 && !State.MutualAidConvoys.IsEmpty())
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
					TEXT("invalid_mutual_aid_convoy"),
					TEXT("Mutual Aid Convoy state is not valid before campaign-save format 36."));
			}
			if (Header.FormatVersion >= 36)
			{
				if (State.MutualAidConvoys.Num() > 10000)
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
						TEXT("invalid_mutual_aid_convoy"),
						TEXT("The Mutual Aid Convoy ledger exceeds its supported record count."));
				}
				TSet<FGuid> SeenConvoyIds;
				TSet<int64> SeenConvoyDispatchSequences;
				for (const FMutualAidConvoyState& Convoy : State.MutualAidConvoys)
				{
					const bool bKnownRoutePolicy =
						Convoy.RoutePolicy == EMutualAidRoutePolicy::OpenRelay
						|| Convoy.RoutePolicy == EMutualAidRoutePolicy::RapidThread
						|| Convoy.RoutePolicy == EMutualAidRoutePolicy::VeiledChain;
					const bool bKnownOnwardRoutePolicy =
						Convoy.OnwardRoutePolicy == EMutualAidRoutePolicy::OpenRelay
						|| Convoy.OnwardRoutePolicy == EMutualAidRoutePolicy::RapidThread
						|| Convoy.OnwardRoutePolicy == EMutualAidRoutePolicy::VeiledChain;
					const bool bRoutingValid = Header.FormatVersion < 37
						|| (bKnownRoutePolicy
							&& Convoy.TotalTransitSeconds > 0
							&& Convoy.RemainingTransitSeconds <= Convoy.TotalTransitSeconds
							&& Convoy.RoutePressure >= 0 && Convoy.RoutePressure <= 100
							&& Convoy.SignalEscortCost >= 0
							&& (Convoy.bSignalEscort || Convoy.SignalEscortCost == 0)
							&& Convoy.ForecastInterdictionDelaySeconds > 0
							&& Convoy.ForecastInterdictionDelaySeconds
								<= Convoy.TotalTransitSeconds / 2
							&& Convoy.InterdictionDelaySeconds >= 0
							&& Convoy.InterdictionDelaySeconds <= Convoy.TotalTransitSeconds
							&& (Convoy.InterdictionDelaySeconds == 0
								|| Convoy.InterdictionDelaySeconds
									== Convoy.ForecastInterdictionDelaySeconds)
							&& (Convoy.InterdictionDelaySeconds == 0
								|| (Convoy.bInterdictionResolved && !Convoy.bSignalEscort))
							&& (Convoy.bInterdictionResolved
								|| (Convoy.InterdictionDelaySeconds == 0
									&& Convoy.RemainingTransitSeconds
										> Convoy.TotalTransitSeconds / 2)));
					const bool bRelayQueueValid = Header.FormatVersion < 38
						|| (Convoy.DispatchSequence > 0
							&& Convoy.DispatchSequence <= State.CommandSequence
							&& !SeenConvoyDispatchSequences.Contains(Convoy.DispatchSequence));
					const bool bNeutralWaypointState =
						!Convoy.CurrentLegOriginBaseId.IsValid()
						&& !Convoy.RelayWaypointBaseId.IsValid()
						&& Convoy.OnwardRoutePolicy == EMutualAidRoutePolicy::OpenRelay
						&& Convoy.OnwardTotalTransitSeconds == 0
						&& Convoy.OnwardRoutePressure == 0
						&& Convoy.bOnwardInterdictionResolved
						&& Convoy.OnwardForecastInterdictionDelaySeconds == 0;
					const FGuid CurrentLegOriginBaseId =
						Convoy.CurrentLegOriginBaseId.IsValid()
							? Convoy.CurrentLegOriginBaseId
							: Convoy.SourceBaseId;
					const bool bWaypointStateValid = Header.FormatVersion < 40
						? bNeutralWaypointState
						: (SeenBases.Contains(CurrentLegOriginBaseId)
							&& CurrentLegOriginBaseId != Convoy.DestinationBaseId
							&& (Convoy.RelayWaypointBaseId.IsValid()
								? (CurrentLegOriginBaseId == Convoy.SourceBaseId
									&& SeenBases.Contains(Convoy.RelayWaypointBaseId)
									&& Convoy.RelayWaypointBaseId != Convoy.SourceBaseId
									&& Convoy.RelayWaypointBaseId != Convoy.DestinationBaseId
									&& bKnownOnwardRoutePolicy
									&& Convoy.OnwardTotalTransitSeconds > 0
									&& Convoy.OnwardRoutePressure >= 0
									&& Convoy.OnwardRoutePressure <= 100
									&& Convoy.OnwardForecastInterdictionDelaySeconds > 0
									&& Convoy.OnwardForecastInterdictionDelaySeconds
										<= Convoy.OnwardTotalTransitSeconds / 2)
								: (Convoy.OnwardRoutePolicy == EMutualAidRoutePolicy::OpenRelay
									&& Convoy.OnwardTotalTransitSeconds == 0
									&& Convoy.OnwardRoutePressure == 0
									&& Convoy.bOnwardInterdictionResolved
									&& Convoy.OnwardForecastInterdictionDelaySeconds == 0)));
					const bool bHandoffStateValid = Header.FormatVersion < 41
						? Convoy.BalancedHandoffQuantity == 0
						: (Convoy.BalancedHandoffQuantity == 0
							|| (Convoy.RelayWaypointBaseId.IsValid()
								&& Convoy.Quantity >= 2
								&& Convoy.BalancedHandoffQuantity == Convoy.Quantity / 2));
					if (!Convoy.ConvoyId.IsValid() || SeenConvoyIds.Contains(Convoy.ConvoyId)
						|| !SeenBases.Contains(Convoy.SourceBaseId)
						|| !SeenBases.Contains(Convoy.DestinationBaseId)
						|| Convoy.SourceBaseId == Convoy.DestinationBaseId
						|| !FContentPackageResolver::IsValidPackageId(Convoy.ItemId)
						|| Convoy.Quantity <= 0 || Convoy.RemainingTransitSeconds <= 0
						|| !bRoutingValid || !bRelayQueueValid || !bWaypointStateValid
						|| !bHandoffStateValid)
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
							TEXT("invalid_mutual_aid_convoy"),
							TEXT("A Mutual Aid Convoy has invalid identity, bases, cargo, or transit time."));
					}
					SeenConvoyIds.Add(Convoy.ConvoyId);
					if (Header.FormatVersion >= 38)
					{
						SeenConvoyDispatchSequences.Add(Convoy.DispatchSequence);
					}
				}
			}

			TSet<FName> SeenProjects;
			for (const FResearchProjectState& Project : State.ResearchProjects)
			{
				if (!FContentPackageResolver::IsValidPackageId(Project.ResearchId) || SeenProjects.Contains(Project.ResearchId))
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_research_project"), FString::Printf(TEXT("Active research id '%s' is invalid or duplicated."), *Project.ResearchId.ToString()));
				}
				SeenProjects.Add(Project.ResearchId);
				if (!SeenBases.Contains(Project.BaseId))
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("research_base_missing"), FString::Printf(TEXT("Research '%s' references a missing base."), *Project.ResearchId.ToString()));
				}
				if (Project.AssignedScientists < 0 || Project.AccumulatedWorkSeconds < 0)
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_research_project"), FString::Printf(TEXT("Research '%s' has negative staff or progress."), *Project.ResearchId.ToString()));
				}
				if (State.CompletedResearch.Contains(Project.ResearchId))
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("research_state_conflict"), FString::Printf(TEXT("Research '%s' is both active and completed."), *Project.ResearchId.ToString()));
				}
			}
			// Assignment totals may validly exceed the saved base-local allowance: active rules add
			// integrity-scaled facility capacity, and forced damage can leave a lossless overcapacity.
			// Rules-aware commands enforce the no-worsening policy after the save has loaded.

			if (Header.FormatVersion >= 4)
			{
				TSet<FGuid> SeenManufacturingProjects;
				for (const FManufacturingProjectState& Project : State.ManufacturingProjects)
				{
					if (!Project.ProjectId.IsValid() || SeenManufacturingProjects.Contains(Project.ProjectId))
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_manufacturing_project_id"), TEXT("Manufacturing project ids must be valid and unique."));
					}
					SeenManufacturingProjects.Add(Project.ProjectId);
					if (!FContentPackageResolver::IsValidPackageId(Project.ItemId)
						|| !SeenBases.Contains(Project.BaseId)
						|| Project.AssignedEngineers < 0
						|| Project.UnitsRemaining <= 0
						|| Project.AccumulatedWorkSeconds < 0)
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_manufacturing_project"), FString::Printf(TEXT("Manufacturing project '%s' has invalid item, base, staffing, quantity, or progress."), *Project.ProjectId.ToString()));
					}
				}
			}

			if (Header.FormatVersion >= 5)
			{
				TSet<FGuid> SeenConstructionProjects;
				for (const FFacilityConstructionProjectState& Project : State.FacilityConstructionProjects)
				{
					if (!Project.ProjectId.IsValid() || SeenConstructionProjects.Contains(Project.ProjectId)
						|| !Project.FacilityInstanceId.IsValid() || SeenFacilityInstances.Contains(Project.FacilityInstanceId)
						|| !SeenBases.Contains(Project.BaseId)
						|| !FContentPackageResolver::IsValidPackageId(Project.FacilityId)
						|| Project.GridX < 0 || Project.GridY < 0
						|| Project.RemainingBuildSeconds <= 0)
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_construction_project"), FString::Printf(TEXT("Facility construction project '%s' has invalid ids, placement, or progress."), *Project.ProjectId.ToString()));
					}
					SeenConstructionProjects.Add(Project.ProjectId);
					SeenFacilityInstances.Add(Project.FacilityInstanceId);
				}
			}

			if (Header.FormatVersion < 33 && !State.PersonnelSquadBonds.IsEmpty())
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
					TEXT("invalid_personnel_squad_bond"),
					TEXT("Personnel squad-bond records are not valid before campaign-save format 33."));
			}
			if (Header.FormatVersion < 34
				&& State.Personnel.ContainsByPredicate(
					[](const FPersonnelState& Person)
					{
						return Person.RecoveryPlan != EPersonnelRecoveryPlan::None;
					}))
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
					TEXT("invalid_personnel_recovery_plan"),
					TEXT("Personnel Return Path state is not valid before campaign-save format 34."));
			}
			if (Header.FormatVersion < 35
				&& (State.Personnel.ContainsByPredicate(
					[](const FPersonnelState& Person)
					{
						return Person.Status == EPersonnelStatus::Stewarding
							|| Person.StewardshipFocus != EPersonnelStewardshipFocus::None
							|| Person.RemainingStewardshipSeconds != 0
							|| Person.StewardshipToursCompleted != 0;
					})
					|| State.Memorial.ContainsByPredicate(
						[](const FMemorialRecord& Record)
						{
							return Record.StewardshipToursCompleted != 0;
						})))
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
					TEXT("invalid_personnel_stewardship"),
					TEXT("Personnel Stewardship Rotation state is not valid before campaign-save format 35."));
			}

			if (Header.FormatVersion >= 6)
			{
				TSet<FGuid> SeenPersonnelIds;
				TSet<FGuid> SeenRecruitmentOrderIds;
				TSet<FGuid> ActiveStewardBaseIds;
				TMap<FGuid, int32> CareerMissionCounts;
				for (const FPersonnelState& Person : State.Personnel)
				{
					TSet<FName> SeenCommendations;
					bool bProgressionIdsValid = true;
					for (const FName DoctrineId : Person.DoctrineSelections)
					{
						bProgressionIdsValid &= FContentPackageResolver::IsValidPackageId(DoctrineId);
					}
					for (const FName CommendationId : Person.Commendations)
					{
						bProgressionIdsValid &= FContentPackageResolver::IsValidPackageId(CommendationId)
							&& !SeenCommendations.Contains(CommendationId);
						SeenCommendations.Add(CommendationId);
					}
					const bool bHasProgression = Person.PendingDoctrineChoices != 0
						|| !Person.DoctrineSelections.IsEmpty() || !Person.Commendations.IsEmpty();
					const bool bProgressionValid = Person.PendingDoctrineChoices >= 0
						&& static_cast<int64>(Person.PendingDoctrineChoices) + Person.DoctrineSelections.Num()
							<= FMath::Max(0, Person.Rank - 1)
						&& bProgressionIdsValid;
					const bool bKnownStatus = Person.Status == EPersonnelStatus::Available
						|| Person.Status == EPersonnelStatus::Recovering
						|| Person.Status == EPersonnelStatus::Training
						|| Person.Status == EPersonnelStatus::Deployed
						|| (Header.FormatVersion >= 35 && Person.Status == EPersonnelStatus::Stewarding);
					const bool bKnownFocus = Person.TrainingFocus == EPersonnelTrainingFocus::Accuracy
						|| Person.TrainingFocus == EPersonnelTrainingFocus::Resolve
						|| Person.TrainingFocus == EPersonnelTrainingFocus::Mobility
						|| Person.TrainingFocus == EPersonnelTrainingFocus::Strength;
					const bool bKnownRecoveryPlan = Person.RecoveryPlan == EPersonnelRecoveryPlan::None
						|| Person.RecoveryPlan == EPersonnelRecoveryPlan::DecisionRequired
						|| Person.RecoveryPlan == EPersonnelRecoveryPlan::MeasuredReturn
						|| Person.RecoveryPlan == EPersonnelRecoveryPlan::SurgeCare
						|| Person.RecoveryPlan == EPersonnelRecoveryPlan::ReflectionCycle;
					const bool bRecoveryPlanStateValid = bKnownRecoveryPlan
						&& (Person.Status == EPersonnelStatus::Recovering
							? true
							: Person.RecoveryPlan == EPersonnelRecoveryPlan::None);
					const bool bKnownStewardshipFocus = Person.StewardshipFocus == EPersonnelStewardshipFocus::None
						|| Person.StewardshipFocus == EPersonnelStewardshipFocus::RecoveryAdvocacy
						|| Person.StewardshipFocus == EPersonnelStewardshipFocus::TrainingCadre
						|| Person.StewardshipFocus == EPersonnelStewardshipFocus::RecruitmentLiaison;
					const bool bStewardshipStateValid = bKnownStewardshipFocus
						&& Person.StewardshipToursCompleted >= 0
						&& (Person.Status == EPersonnelStatus::Stewarding
							? Header.FormatVersion >= 35
								&& Person.StewardshipFocus != EPersonnelStewardshipFocus::None
								&& Person.RemainingStewardshipSeconds > 0
								&& !ActiveStewardBaseIds.Contains(Person.BaseId)
							: Person.StewardshipFocus == EPersonnelStewardshipFocus::None
								&& Person.RemainingStewardshipSeconds == 0);
					const bool bStatusTimersValid =
						(Person.Status == EPersonnelStatus::Available && Person.RemainingRecoverySeconds == 0 && Person.RemainingTrainingSeconds == 0 && Person.RemainingStewardshipSeconds == 0 && Person.CurrentHealth == Person.MaxHealth)
						|| (Person.Status == EPersonnelStatus::Recovering && Person.RemainingRecoverySeconds > 0 && Person.RemainingTrainingSeconds == 0 && Person.RemainingStewardshipSeconds == 0 && Person.CurrentHealth < Person.MaxHealth)
						|| (Person.Status == EPersonnelStatus::Training && Person.RemainingRecoverySeconds == 0 && Person.RemainingTrainingSeconds > 0 && Person.RemainingStewardshipSeconds == 0 && Person.CurrentHealth == Person.MaxHealth)
						|| (Person.Status == EPersonnelStatus::Deployed && Person.RemainingRecoverySeconds == 0 && Person.RemainingTrainingSeconds == 0 && Person.RemainingStewardshipSeconds == 0)
						|| (Person.Status == EPersonnelStatus::Stewarding && Person.RemainingRecoverySeconds == 0 && Person.RemainingTrainingSeconds == 0 && Person.RemainingStewardshipSeconds > 0 && Person.CurrentHealth == Person.MaxHealth);
					if (!Person.PersonnelId.IsValid() || SeenPersonnelIds.Contains(Person.PersonnelId)
						|| Person.DisplayName.TrimStartAndEnd().IsEmpty() || Person.DisplayName.Len() > 64
						|| !FContentPackageResolver::IsValidPackageId(Person.RoleId)
						|| !SeenBases.Contains(Person.BaseId)
						|| !bKnownStatus || !bKnownFocus || !bStatusTimersValid || !bRecoveryPlanStateValid
						|| !bStewardshipStateValid
						|| Person.Rank <= 0 || Person.Rank > 100 || Person.Missions < 0 || Person.Kills < 0 || Person.Experience < 0
						|| Person.MaxHealth <= 0 || Person.MaxHealth > 200
						|| Person.CurrentHealth <= 0 || Person.CurrentHealth > Person.MaxHealth
						|| Person.Accuracy <= 0 || Person.Accuracy > 100
						|| Person.Resolve <= 0 || Person.Resolve > 100
						|| Person.Mobility <= 0 || Person.Mobility > 100
						|| Person.Strength <= 0 || Person.Strength > 100
						|| Person.EquippedItems.Num() > 16)
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_personnel_state"), FString::Printf(TEXT("Personnel '%s' has invalid identity, role, base, attributes, status, timers, or equipment capacity."), *Person.DisplayName));
					}
					if ((Header.FormatVersion < 24 && bHasProgression)
						|| (Header.FormatVersion >= 24 && !bProgressionValid))
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_personnel_progression"), FString::Printf(TEXT("Personnel '%s' has invalid or version-incompatible doctrine and commendation state."), *Person.DisplayName));
					}
					for (const FName ItemId : Person.EquippedItems)
					{
						if (!FContentPackageResolver::IsValidPackageId(ItemId))
						{
							AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_personnel_equipment"), FString::Printf(TEXT("Personnel '%s' has invalid equipped item id '%s'."), *Person.DisplayName, *ItemId.ToString()));
						}
					}
					SeenPersonnelIds.Add(Person.PersonnelId);
					if (Person.Status == EPersonnelStatus::Stewarding)
					{
						ActiveStewardBaseIds.Add(Person.BaseId);
					}
					CareerMissionCounts.Add(Person.PersonnelId, Person.Missions);
				}

				for (const FRecruitmentOrderState& Order : State.RecruitmentOrders)
				{
					if (!Order.OrderId.IsValid() || SeenRecruitmentOrderIds.Contains(Order.OrderId)
						|| !Order.PersonnelId.IsValid() || SeenPersonnelIds.Contains(Order.PersonnelId)
						|| Order.DisplayName.TrimStartAndEnd().IsEmpty() || Order.DisplayName.Len() > 64
						|| !FContentPackageResolver::IsValidPackageId(Order.RoleId)
						|| !SeenBases.Contains(Order.BaseId)
						|| Order.RemainingTransitSeconds <= 0)
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_recruitment_order"), FString::Printf(TEXT("Recruitment order '%s' has invalid ids, name, role, base, or transit time."), *Order.OrderId.ToString()));
					}
					SeenRecruitmentOrderIds.Add(Order.OrderId);
					SeenPersonnelIds.Add(Order.PersonnelId);
				}

				for (const FMemorialRecord& Record : State.Memorial)
				{
					TSet<FName> SeenCommendations;
					bool bProgressionIdsValid = true;
					for (const FName DoctrineId : Record.DoctrineSelections)
					{
						bProgressionIdsValid &= FContentPackageResolver::IsValidPackageId(DoctrineId);
					}
					for (const FName CommendationId : Record.Commendations)
					{
						bProgressionIdsValid &= FContentPackageResolver::IsValidPackageId(CommendationId)
							&& !SeenCommendations.Contains(CommendationId);
						SeenCommendations.Add(CommendationId);
					}
					const bool bHasProgression = !Record.DoctrineSelections.IsEmpty() || !Record.Commendations.IsEmpty();
					const bool bProgressionValid = Record.DoctrineSelections.Num() <= FMath::Max(0, Record.Rank - 1)
						&& bProgressionIdsValid;
					if (!Record.PersonnelId.IsValid() || SeenPersonnelIds.Contains(Record.PersonnelId)
						|| Record.DisplayName.TrimStartAndEnd().IsEmpty() || Record.DisplayName.Len() > 64
						|| !FContentPackageResolver::IsValidPackageId(Record.RoleId)
						|| !FContentPackageResolver::IsValidPackageId(Record.CauseId)
						|| Record.Rank <= 0 || Record.Missions < 0 || Record.Kills < 0
						|| Record.StewardshipToursCompleted < 0
						|| !IsUsableWallClock(Record.DeathUtc) || Record.DeathUtc > State.StrategicTime.Utc)
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_memorial_record"), FString::Printf(TEXT("Memorial record for '%s' has invalid identity, role, service record, death time, or cause."), *Record.DisplayName));
					}
					if ((Header.FormatVersion < 24 && bHasProgression)
						|| (Header.FormatVersion >= 24 && !bProgressionValid))
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_memorial_progression"), FString::Printf(TEXT("Memorial record for '%s' has invalid or version-incompatible doctrine and commendation history."), *Record.DisplayName));
					}
					SeenPersonnelIds.Add(Record.PersonnelId);
					CareerMissionCounts.Add(Record.PersonnelId, Record.Missions);
				}

				if (Header.FormatVersion >= 33)
				{
					if (State.PersonnelSquadBonds.Num() > 10000)
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
							TEXT("invalid_personnel_squad_bond"),
							TEXT("The personnel squad-bond ledger exceeds its supported record count."));
					}
					TSet<FString> SeenSquadBondPairs;
					for (const FPersonnelSquadBondState& Bond : State.PersonnelSquadBonds)
					{
						const FString FirstId = Bond.FirstPersonnelId.ToString(EGuidFormats::Digits);
						const FString SecondId = Bond.SecondPersonnelId.ToString(EGuidFormats::Digits);
						const FString PairKey = FirstId + TEXT(":") + SecondId;
						const int32* FirstMissions = CareerMissionCounts.Find(Bond.FirstPersonnelId);
						const int32* SecondMissions = CareerMissionCounts.Find(Bond.SecondPersonnelId);
						if (!Bond.FirstPersonnelId.IsValid() || !Bond.SecondPersonnelId.IsValid()
							|| Bond.FirstPersonnelId == Bond.SecondPersonnelId || !(FirstId < SecondId)
							|| FirstMissions == nullptr || SecondMissions == nullptr
							|| Bond.SharedVictories <= 0
							|| Bond.SharedVictories > FMath::Min(*FirstMissions, *SecondMissions)
							|| SeenSquadBondPairs.Contains(PairKey))
						{
							AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
								TEXT("invalid_personnel_squad_bond"),
								TEXT("A personnel squad-bond record has invalid canonical identities, history, or shared victories."));
						}
						SeenSquadBondPairs.Add(PairKey);
					}
				}
			}

			if (Header.FormatVersion >= 7)
			{
				TSet<FGuid> SeenCraftIds;
				TSet<FGuid> SeenCraftOrderIds;
				TSet<FGuid> SeenAssignedPilots;
				TSet<FGuid> SeenAssignedAgents;
				TSet<FGuid> TargetedSiteIds;
				TSet<FGuid> SiteIds;
				if (Header.FormatVersion >= 11)
				{
					for (const FStrategicSiteState& Site : State.StrategicSites)
					{
						SiteIds.Add(Site.SiteId);
					}
				}
				TMap<FGuid, EStrategicContactStatus> ContactStatuses;
				TSet<FGuid> PendingAssaultContactIds;
				if (Header.FormatVersion >= 20)
				{
					for (const FBaseAssaultState& Assault : State.BaseAssaults)
					{
						PendingAssaultContactIds.Add(Assault.ContactId);
					}
				}
				if (Header.FormatVersion >= 8)
				{
					for (const FStrategicContactState& Contact : State.StrategicContacts)
					{
						const bool bPendingAssault = PendingAssaultContactIds.Contains(Contact.ContactId);
						const bool bKnownContactStatus = Contact.Status == EStrategicContactStatus::Hidden
							|| Contact.Status == EStrategicContactStatus::Detected
							|| Contact.Status == EStrategicContactStatus::Engaged;
						if (!Contact.ContactId.IsValid() || ContactStatuses.Contains(Contact.ContactId)
							|| !FContentPackageResolver::IsValidPackageId(Contact.ContactRuleId)
							|| !bKnownContactStatus
							|| !AreValidCoordinates(Contact.OriginLongitudeMilliDegrees, Contact.OriginLatitudeMilliDegrees)
							|| !AreValidCoordinates(Contact.LongitudeMilliDegrees, Contact.LatitudeMilliDegrees)
							|| !AreValidCoordinates(Contact.DestinationLongitudeMilliDegrees, Contact.DestinationLatitudeMilliDegrees)
							|| (Contact.OriginLongitudeMilliDegrees == Contact.DestinationLongitudeMilliDegrees
								&& Contact.OriginLatitudeMilliDegrees == Contact.DestinationLatitudeMilliDegrees)
							|| Contact.TotalRouteSeconds <= 0
							|| Contact.ElapsedRouteSeconds < 0
							|| (bPendingAssault
								? Contact.ElapsedRouteSeconds != Contact.TotalRouteSeconds
								: Contact.ElapsedRouteSeconds >= Contact.TotalRouteSeconds)
							|| Contact.CurrentHull <= 0
							|| (Header.FormatVersion >= 9
								&& (Contact.CompletedCombatRounds < 0 || Contact.RemainingAttackCooldownSeconds < 0)))
						{
							AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_strategic_contact"), FString::Printf(TEXT("Strategic contact '%s' has invalid identity, rule, status, coordinates, route, or condition."), *Contact.ContactId.ToString()));
						}
						ContactStatuses.Add(Contact.ContactId, Contact.Status);
					}
				}
				for (const FCraftState& Craft : State.Craft)
				{
					const bool bKnownStatus = Craft.Status == ECraftStatus::Grounded
						|| Craft.Status == ECraftStatus::Servicing
						|| Craft.Status == ECraftStatus::Airborne
						|| (Header.FormatVersion >= 8
							&& (Craft.Status == ECraftStatus::Intercepting || Craft.Status == ECraftStatus::Returning))
						|| (Header.FormatVersion >= 11
							&& (Craft.Status == ECraftStatus::Deploying || Craft.Status == ECraftStatus::OnSite));
					const bool bFlying = Craft.Status == ECraftStatus::Airborne
						|| (Header.FormatVersion >= 8
							&& (Craft.Status == ECraftStatus::Intercepting || Craft.Status == ECraftStatus::Returning))
						|| (Header.FormatVersion >= 11
							&& (Craft.Status == ECraftStatus::Deploying || Craft.Status == ECraftStatus::OnSite));
					const bool bTimersValid =
						((Craft.Status == ECraftStatus::Grounded || bFlying)
							&& Craft.RemainingRepairSeconds == 0 && Craft.RemainingRefuelSeconds == 0)
						|| (Craft.Status == ECraftStatus::Servicing
							&& Craft.RemainingRepairSeconds >= 0 && Craft.RemainingRefuelSeconds >= 0
							&& (Craft.RemainingRepairSeconds > 0 || Craft.RemainingRefuelSeconds > 0));
					const EStrategicContactStatus* TargetStatus = ContactStatuses.Find(Craft.TargetContactId);
					const bool bRouteValid = Header.FormatVersion < 8
						|| ((Craft.Status == ECraftStatus::Grounded || Craft.Status == ECraftStatus::Servicing)
							&& !Craft.TargetContactId.IsValid() && !Craft.TargetSiteId.IsValid()
							&& Craft.RemainingRouteSeconds == 0 && Craft.ReservedReturnSeconds == 0)
						|| (Craft.Status == ECraftStatus::Airborne
							&& (Header.FormatVersion < 11 || !Craft.TargetSiteId.IsValid())
							&& Craft.RemainingRouteSeconds == 0
							&& ((!Craft.TargetContactId.IsValid() && Craft.ReservedReturnSeconds == 0)
								|| (Craft.TargetContactId.IsValid() && Craft.ReservedReturnSeconds > 0
									&& TargetStatus != nullptr && *TargetStatus == EStrategicContactStatus::Engaged)))
						|| (Craft.Status == ECraftStatus::Intercepting
							&& Craft.TargetContactId.IsValid() && (Header.FormatVersion < 11 || !Craft.TargetSiteId.IsValid())
							&& Craft.RemainingRouteSeconds > 0 && Craft.ReservedReturnSeconds > 0
							&& TargetStatus != nullptr && *TargetStatus != EStrategicContactStatus::Hidden)
						|| (Craft.Status == ECraftStatus::Returning
							&& !Craft.TargetContactId.IsValid() && (Header.FormatVersion < 11 || !Craft.TargetSiteId.IsValid())
							&& Craft.RemainingRouteSeconds > 0 && Craft.ReservedReturnSeconds > 0)
						|| (Header.FormatVersion >= 11 && Craft.Status == ECraftStatus::Deploying
							&& !Craft.TargetContactId.IsValid() && Craft.TargetSiteId.IsValid() && SiteIds.Contains(Craft.TargetSiteId)
							&& Craft.RemainingRouteSeconds > 0 && Craft.ReservedReturnSeconds > 0)
						|| (Header.FormatVersion >= 11 && Craft.Status == ECraftStatus::OnSite
							&& !Craft.TargetContactId.IsValid() && Craft.TargetSiteId.IsValid() && SiteIds.Contains(Craft.TargetSiteId)
							&& Craft.RemainingRouteSeconds == 0 && Craft.ReservedReturnSeconds > 0);
					if (!Craft.CraftId.IsValid() || SeenCraftIds.Contains(Craft.CraftId)
						|| Craft.DisplayName.TrimStartAndEnd().IsEmpty() || Craft.DisplayName.Len() > 64
						|| !FContentPackageResolver::IsValidPackageId(Craft.CraftRuleId)
						|| !SeenBases.Contains(Craft.BaseId)
						|| !bKnownStatus || !bTimersValid || !bRouteValid
						|| Craft.CurrentHull <= 0 || Craft.CurrentFuel < 0 || Craft.CompletedSorties < 0
						|| Craft.EquipmentItems.Num() > 16
						|| (Header.FormatVersion >= 11
							&& (Craft.AssignedAgentIds.Num() > 64 || Craft.Cargo.Num() > 64
								|| (Header.FormatVersion >= 26 && Craft.PendingSalvage.Num() > 64)
								|| ((Craft.Status == ECraftStatus::Deploying || Craft.Status == ECraftStatus::OnSite) && Craft.AssignedAgentIds.IsEmpty())
								|| (Craft.TargetSiteId.IsValid() && TargetedSiteIds.Contains(Craft.TargetSiteId))
								|| (Header.FormatVersion >= 26 && !Craft.PendingSalvage.IsEmpty()
									&& Craft.Status != ECraftStatus::OnSite
									&& Craft.Status != ECraftStatus::Returning
									&& Craft.Status != ECraftStatus::Grounded))))
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_craft_state"), FString::Printf(TEXT("Craft '%s' has invalid identity, rule, base, status, condition, service timers, or equipment capacity."), *Craft.DisplayName));
					}
					for (const FName ItemId : Craft.EquipmentItems)
					{
						if (!FContentPackageResolver::IsValidPackageId(ItemId))
						{
							AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_craft_equipment"), FString::Printf(TEXT("Craft '%s' has invalid equipment item id '%s'."), *Craft.DisplayName, *ItemId.ToString()));
						}
					}
					if (Header.FormatVersion >= 9)
					{
						TSet<FName> SeenWeaponIds;
						for (const FCraftWeaponState& WeaponState : Craft.WeaponStates)
						{
							if (!FContentPackageResolver::IsValidPackageId(WeaponState.WeaponItemId)
								|| SeenWeaponIds.Contains(WeaponState.WeaponItemId)
								|| !Craft.EquipmentItems.Contains(WeaponState.WeaponItemId)
								|| WeaponState.Ammunition < 0 || WeaponState.RemainingCooldownSeconds < 0)
							{
								AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_craft_weapon_state"), FString::Printf(TEXT("Craft '%s' has invalid weapon state for '%s'."), *Craft.DisplayName, *WeaponState.WeaponItemId.ToString()));
							}
							SeenWeaponIds.Add(WeaponState.WeaponItemId);
						}
					}
					if (Header.FormatVersion >= 11)
					{
						TSet<FGuid> CraftAgentIds;
						for (const FGuid& AgentId : Craft.AssignedAgentIds)
						{
							const FPersonnelState* Agent = State.Personnel.FindByPredicate(
								[&AgentId](const FPersonnelState& Person) { return Person.PersonnelId == AgentId; });
							if (!AgentId.IsValid() || CraftAgentIds.Contains(AgentId) || SeenAssignedAgents.Contains(AgentId)
								|| Agent == nullptr || Agent->BaseId != Craft.BaseId
								|| (bFlying && Agent->Status != EPersonnelStatus::Deployed)
								|| (!bFlying && Agent->Status != EPersonnelStatus::Available))
							{
								AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_craft_agent"), FString::Printf(TEXT("Craft '%s' has an invalid or conflicting field-agent assignment."), *Craft.DisplayName));
							}
							CraftAgentIds.Add(AgentId);
							SeenAssignedAgents.Add(AgentId);
						}
						TSet<FName> CargoItemIds;
						for (const FInventoryStack& Stack : Craft.Cargo)
						{
							if (!FContentPackageResolver::IsValidPackageId(Stack.ItemId)
								|| Stack.Quantity <= 0 || CargoItemIds.Contains(Stack.ItemId))
							{
								AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_craft_cargo"), FString::Printf(TEXT("Craft '%s' has an invalid or duplicate cargo stack for '%s'."), *Craft.DisplayName, *Stack.ItemId.ToString()));
							}
							CargoItemIds.Add(Stack.ItemId);
						}
						if (Header.FormatVersion >= 26)
						{
							TSet<FName> SalvageItemIds;
							for (const FInventoryStack& Stack : Craft.PendingSalvage)
							{
								const FInventoryStack* CargoStack = Craft.Cargo.FindByPredicate(
									[&Stack](const FInventoryStack& Entry) { return Entry.ItemId == Stack.ItemId; });
								if (!FContentPackageResolver::IsValidPackageId(Stack.ItemId)
									|| Stack.Quantity <= 0 || SalvageItemIds.Contains(Stack.ItemId)
									|| CargoStack == nullptr || CargoStack->Quantity < Stack.Quantity)
								{
									AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_craft_cargo"), FString::Printf(TEXT("Craft '%s' has invalid pending salvage for '%s'."), *Craft.DisplayName, *Stack.ItemId.ToString()));
								}
								SalvageItemIds.Add(Stack.ItemId);
							}
						}
						if (Craft.TargetSiteId.IsValid())
						{
							TargetedSiteIds.Add(Craft.TargetSiteId);
						}
					}
					if (Craft.AssignedPilotId.IsValid())
					{
						const FPersonnelState* Pilot = State.Personnel.FindByPredicate(
							[&Craft](const FPersonnelState& Person) { return Person.PersonnelId == Craft.AssignedPilotId; });
						if (Pilot == nullptr || Pilot->BaseId != Craft.BaseId || SeenAssignedPilots.Contains(Craft.AssignedPilotId)
							|| (bFlying && Pilot->Status != EPersonnelStatus::Deployed)
							|| (!bFlying && Pilot->Status == EPersonnelStatus::Deployed))
						{
							AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_craft_pilot"), FString::Printf(TEXT("Craft '%s' has an invalid or conflicting pilot assignment."), *Craft.DisplayName));
						}
						SeenAssignedPilots.Add(Craft.AssignedPilotId);
					}
					else if (bFlying)
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_craft_pilot"), FString::Printf(TEXT("Flying craft '%s' has no assigned pilot."), *Craft.DisplayName));
					}
					SeenCraftIds.Add(Craft.CraftId);
				}

				for (const FCraftAcquisitionOrderState& Order : State.CraftAcquisitionOrders)
				{
					if (!Order.OrderId.IsValid() || SeenCraftOrderIds.Contains(Order.OrderId)
						|| !Order.CraftId.IsValid() || SeenCraftIds.Contains(Order.CraftId)
						|| Order.DisplayName.TrimStartAndEnd().IsEmpty() || Order.DisplayName.Len() > 64
						|| !FContentPackageResolver::IsValidPackageId(Order.CraftRuleId)
						|| !SeenBases.Contains(Order.BaseId)
						|| Order.RemainingTransitSeconds <= 0)
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_craft_acquisition"), FString::Printf(TEXT("Craft acquisition order '%s' has invalid ids, name, rule, base, or transit time."), *Order.OrderId.ToString()));
					}
					SeenCraftOrderIds.Add(Order.OrderId);
					SeenCraftIds.Add(Order.CraftId);
				}

				if (Header.FormatVersion >= 8)
				{
					for (const FStrategicContactState& Contact : State.StrategicContacts)
					{
						const bool bHasOnStationCraft = State.Craft.ContainsByPredicate(
							[&Contact](const FCraftState& Craft)
							{
								return Craft.Status == ECraftStatus::Airborne && Craft.TargetContactId == Contact.ContactId;
							});
						if ((Contact.Status == EStrategicContactStatus::Engaged) != bHasOnStationCraft)
						{
							AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_contact_engagement"), FString::Printf(TEXT("Strategic contact '%s' has inconsistent craft engagement state."), *Contact.ContactId.ToString()));
						}
					}
				}
				if (Header.FormatVersion >= 9)
				{
					TSet<FGuid> SeenSiteIds;
					for (const FStrategicSiteState& Site : State.StrategicSites)
					{
						if (!Site.SiteId.IsValid() || SeenSiteIds.Contains(Site.SiteId)
							|| ContactStatuses.Contains(Site.SiteId)
							|| (Site.Type != EStrategicSiteType::Wreckage && Site.Type != EStrategicSiteType::Landing)
							|| (Site.Type == EStrategicSiteType::Landing && Header.FormatVersion < 22)
							|| !FContentPackageResolver::IsValidPackageId(Site.SourceContactRuleId)
							|| !AreValidCoordinates(Site.LongitudeMilliDegrees, Site.LatitudeMilliDegrees)
							|| Site.ThreatRating <= 0 || Site.ThreatRating > 10
							|| Site.RemainingLifetimeSeconds <= 0)
						{
							AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_strategic_site"), FString::Printf(TEXT("Strategic site '%s' has invalid identity, source, type, coordinates, threat, or lifetime."), *Site.SiteId.ToString()));
						}
						SeenSiteIds.Add(Site.SiteId);
					}
					if (Header.FormatVersion >= 11)
					{
						TSet<FGuid> SeenOperationIds;
						TSet<FGuid> OperationSiteIds;
						TSet<FGuid> OperationCraftIds;
						TSet<FGuid> OperationAssaultIds;
						TSet<FGuid> OperationAgentIds;
						for (const FTacticalOperationState& Operation : State.TacticalOperations)
						{
							const bool bSiteRecovery = Operation.Type == ETacticalOperationType::SiteRecovery;
							const bool bBaseDefense = Operation.Type == ETacticalOperationType::BaseDefense;
							const FCraftState* Craft = bSiteRecovery ? State.Craft.FindByPredicate(
								[&Operation](const FCraftState& Entry) { return Entry.CraftId == Operation.CraftId; }) : nullptr;
							const FBaseAssaultState* Assault = bBaseDefense ? State.BaseAssaults.FindByPredicate(
								[&Operation](const FBaseAssaultState& Entry) { return Entry.AssaultId == Operation.AssaultId; }) : nullptr;
							bool bAgentsMatch = Craft != nullptr && !Operation.AgentIds.IsEmpty()
								&& Operation.AgentIds.Num() == Craft->AssignedAgentIds.Num();
							if (bAgentsMatch)
							{
								for (const FGuid& AgentId : Operation.AgentIds)
								{
									if (!Craft->AssignedAgentIds.Contains(AgentId))
									{
										bAgentsMatch = false;
										break;
									}
								}
							}
							bool bCargoMatches = Craft != nullptr && Operation.Cargo.Num() == Craft->Cargo.Num();
							if (bCargoMatches)
							{
								for (const FInventoryStack& Stack : Operation.Cargo)
								{
									const FInventoryStack* CraftStack = Craft->Cargo.FindByPredicate(
										[&Stack](const FInventoryStack& Entry) { return Entry.ItemId == Stack.ItemId; });
									if (CraftStack == nullptr || CraftStack->Quantity != Stack.Quantity)
									{
										bCargoMatches = false;
										break;
									}
								}
							}
							bool bBaseAgentsValid = bBaseDefense && !Operation.AgentIds.IsEmpty();
							for (const FGuid& AgentId : Operation.AgentIds)
							{
								const FPersonnelState* Person = State.Personnel.FindByPredicate(
									[&AgentId](const FPersonnelState& Entry) { return Entry.PersonnelId == AgentId; });
								if (!AgentId.IsValid() || OperationAgentIds.Contains(AgentId) || Person == nullptr
									|| Person->Status != EPersonnelStatus::Deployed
									|| (bBaseDefense && (Person->BaseId != Operation.BaseId
										|| State.Craft.ContainsByPredicate([&AgentId](const FCraftState& Entry) { return Entry.AssignedAgentIds.Contains(AgentId); }))))
								{
									bBaseAgentsValid = false;
									bAgentsMatch = false;
								}
							}
							const bool bContextValid =
								(bSiteRecovery
									&& Operation.SiteId.IsValid() && !OperationSiteIds.Contains(Operation.SiteId) && SeenSiteIds.Contains(Operation.SiteId)
									&& Operation.CraftId.IsValid() && !OperationCraftIds.Contains(Operation.CraftId)
									&& !Operation.BaseId.IsValid() && !Operation.AssaultId.IsValid()
									&& Craft != nullptr && Craft->Status == ECraftStatus::OnSite && Craft->TargetSiteId == Operation.SiteId
									&& bAgentsMatch && bCargoMatches)
								|| (bBaseDefense
									&& !Operation.SiteId.IsValid() && !Operation.CraftId.IsValid()
									&& Operation.BaseId.IsValid() && State.Bases.ContainsByPredicate(
										[&Operation](const FStrategicBaseState& Base) { return Base.BaseId == Operation.BaseId; })
									&& Operation.AssaultId.IsValid() && !OperationAssaultIds.Contains(Operation.AssaultId)
									&& Assault != nullptr && Assault->BaseId == Operation.BaseId
									&& Operation.Cargo.IsEmpty() && bBaseAgentsValid);
							if (!Operation.OperationId.IsValid() || SeenOperationIds.Contains(Operation.OperationId)
								|| !IsUsableWallClock(Operation.CreatedUtc) || Operation.CreatedUtc > State.StrategicTime.Utc
								|| !bContextValid)
							{
								AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_tactical_operation"), FString::Printf(TEXT("Tactical operation '%s' has invalid identity, context, strategic links, roster, cargo, or creation time."), *Operation.OperationId.ToString()));
							}
							SeenOperationIds.Add(Operation.OperationId);
							for (const FGuid& AgentId : Operation.AgentIds)
							{
								OperationAgentIds.Add(AgentId);
							}
							if (bSiteRecovery)
							{
								OperationSiteIds.Add(Operation.SiteId);
								OperationCraftIds.Add(Operation.CraftId);
							}
							else if (bBaseDefense)
							{
								OperationAssaultIds.Add(Operation.AssaultId);
							}
						}
						for (const FCraftState& Craft : State.Craft)
						{
							if ((Craft.Status == ECraftStatus::OnSite) != OperationCraftIds.Contains(Craft.CraftId))
							{
								AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_tactical_operation_link"), FString::Printf(TEXT("Craft '%s' has inconsistent on-site tactical-operation state."), *Craft.DisplayName));
							}
						}
					}
				}
			}
		}

		if (Header.FormatVersion >= 10)
		{
			const bool bKnownOutcome = State.Outcome == ECampaignOutcome::Ongoing
				|| State.Outcome == ECampaignOutcome::Victory
				|| State.Outcome == ECampaignOutcome::Failure;
			const bool bOutcomeMetadataValid = State.Outcome == ECampaignOutcome::Ongoing
				? State.OutcomeReasonId.IsNone() && State.NextAdversaryMissionSeconds > 0
				: FContentPackageResolver::IsValidPackageId(State.OutcomeReasonId) && State.NextAdversaryMissionSeconds == 0;
			const int64 ResolvedMissionCount = static_cast<int64>(State.AdversaryMissions.Num())
				+ State.AdversaryMissionsEscaped + State.AdversaryMissionsThwarted;
			if (!bKnownOutcome || !bOutcomeMetadataValid
				|| State.AdversaryEscalationLevel <= 0 || State.AdversaryEscalationLevel > 10
				|| State.NextAdversaryMissionSerial <= 0
				|| State.AdversaryMissionsLaunched < 0
				|| State.AdversaryMissionsEscaped < 0
				|| State.AdversaryMissionsThwarted < 0
				|| ResolvedMissionCount != State.AdversaryMissionsLaunched)
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_adversary_state"), TEXT("Adversary escalation, cadence, counters, or campaign outcome metadata is invalid."));
			}

			TSet<FName> SeenPressureRegions;
			for (const FRegionalPressureState& Pressure : State.RegionalPressure)
			{
				if (!FContentPackageResolver::IsValidPackageId(Pressure.RegionId)
					|| SeenPressureRegions.Contains(Pressure.RegionId)
					|| Pressure.Pressure < 0 || Pressure.Pressure > 100)
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_regional_pressure"), FString::Printf(TEXT("Regional pressure '%s' is invalid or duplicated."), *Pressure.RegionId.ToString()));
				}
				SeenPressureRegions.Add(Pressure.RegionId);
			}
			int32 SignedCharterCount = 0;
			if (Header.FormatVersion < 23 && !State.RegionalMandates.IsEmpty())
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_regional_mandate"),
					TEXT("Regional mandate state cannot be embedded in a save format that predates it."));
			}
			else if (Header.FormatVersion >= 23 && !State.RegionalMandates.IsEmpty())
			{
				TSet<FName> SeenMandateRegions;
				int64 RegionalFunding = 0;
				const int32 CurrentMonth = State.StrategicTime.Utc.GetYear() * 12 + State.StrategicTime.Utc.GetMonth() - 1;
				for (const FRegionalMandateState& Mandate : State.RegionalMandates)
				{
					SignedCharterCount += Mandate.bResilienceCharterSigned ? 1 : 0;
					const bool bFundingWouldOverflow = Mandate.CurrentMonthlyFunding >= 0
						&& Mandate.CurrentMonthlyFunding > MAX_int64 - RegionalFunding;
					if (!FContentPackageResolver::IsValidPackageId(Mandate.RegionId)
						|| SeenMandateRegions.Contains(Mandate.RegionId)
						|| !SeenPressureRegions.Contains(Mandate.RegionId)
						|| Mandate.Support < 0 || Mandate.Support > 100
						|| Mandate.BaselineMonthlyFunding < 0 || Mandate.CurrentMonthlyFunding < 0
						|| Mandate.LastDiplomaticActionMonth < 0 || Mandate.LastDiplomaticActionMonth > CurrentMonth
						|| (Header.FormatVersion < 28 && Mandate.bResilienceCharterSigned)
						|| (Header.FormatVersion < 31 && Mandate.bHorizonCompactMemberWithdrawn)
						|| (Mandate.bHorizonCompactMemberWithdrawn
							&& (!State.bHorizonCompactRatified || !Mandate.bResilienceCharterSigned))
						|| bFundingWouldOverflow)
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_regional_mandate"),
							FString::Printf(TEXT("Regional mandate '%s' has invalid or duplicate support, funding, outreach, or coalition state."), *Mandate.RegionId.ToString()));
					}
					else
					{
						RegionalFunding += Mandate.CurrentMonthlyFunding;
					}
					SeenMandateRegions.Add(Mandate.RegionId);
				}
				if (SeenMandateRegions.Num() != SeenPressureRegions.Num() || RegionalFunding != State.MonthlyFunding)
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_regional_mandate"),
						TEXT("Regional mandates must cover every pressure region and sum to recurring campaign funding."));
				}
			}
			if ((Header.FormatVersion < 29 && State.bHorizonCompactRatified)
				|| (State.bHorizonCompactRatified && SignedCharterCount < 2))
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
					TEXT("invalid_coalition_compact_state"),
					TEXT("A ratified Horizon Compact requires save format 29 and at least two signed regional charters."));
			}
			const int32 CurrentCoalitionMonth =
				State.StrategicTime.Utc.GetYear() * 12 + State.StrategicTime.Utc.GetMonth() - 1;
			if ((Header.FormatVersion < 30 && State.LastCoalitionAidMonth != 0)
				|| State.LastCoalitionAidMonth < 0
				|| State.LastCoalitionAidMonth > CurrentCoalitionMonth
				|| (!State.bHorizonCompactRatified && State.LastCoalitionAidMonth != 0))
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
					TEXT("invalid_coalition_aid_state"),
					TEXT("Reciprocal Aid history requires save format 30, a ratified compact, and a month no later than the campaign month."));
			}
			if ((Header.FormatVersion < 32 && State.LastCoalitionEmergencyVoteMonth != 0)
				|| State.LastCoalitionEmergencyVoteMonth < 0
				|| State.LastCoalitionEmergencyVoteMonth > CurrentCoalitionMonth
				|| (!State.bHorizonCompactRatified
					&& State.LastCoalitionEmergencyVoteMonth != 0))
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
					TEXT("invalid_coalition_emergency_vote_state"),
					TEXT("Emergency solidarity vote history requires save format 32, a ratified compact, and a month no later than the campaign month."));
			}

			TSet<FGuid> ContactIds;
			for (const FStrategicContactState& Contact : State.StrategicContacts)
			{
				ContactIds.Add(Contact.ContactId);
			}
			TSet<FGuid> SeenMissionIds;
			TSet<FGuid> SeenMissionContactIds;
			for (const FAdversaryMissionState& Mission : State.AdversaryMissions)
			{
				if (!Mission.MissionId.IsValid() || SeenMissionIds.Contains(Mission.MissionId)
					|| !Mission.ContactId.IsValid() || SeenMissionContactIds.Contains(Mission.ContactId)
					|| Mission.MissionId == Mission.ContactId || !ContactIds.Contains(Mission.ContactId)
					|| !FContentPackageResolver::IsValidPackageId(Mission.MissionRuleId)
					|| (Header.FormatVersion >= 20 && Mission.TargetBaseId.IsValid()
						&& !State.Bases.ContainsByPredicate([&Mission](const FStrategicBaseState& Base) { return Base.BaseId == Mission.TargetBaseId; }))
					|| !IsUsableWallClock(Mission.StartedUtc) || Mission.StartedUtc > State.StrategicTime.Utc)
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_adversary_mission"), FString::Printf(TEXT("Adversary mission '%s' has invalid identity, rule, contact, or start time."), *Mission.MissionId.ToString()));
				}
				SeenMissionIds.Add(Mission.MissionId);
				SeenMissionContactIds.Add(Mission.ContactId);
			}

			if (Header.FormatVersion >= 20)
			{
				TSet<FGuid> SeenAssaultIds;
				TSet<FGuid> AssaultMissionIds;
				TSet<FGuid> AssaultContactIds;
				for (const FBaseAssaultState& Assault : State.BaseAssaults)
				{
					const FAdversaryMissionState* Mission = State.AdversaryMissions.FindByPredicate(
						[&Assault](const FAdversaryMissionState& Entry) { return Entry.MissionId == Assault.MissionId; });
					const FStrategicContactState* Contact = State.StrategicContacts.FindByPredicate(
						[&Assault](const FStrategicContactState& Entry) { return Entry.ContactId == Assault.ContactId; });
					if (!Assault.AssaultId.IsValid() || SeenAssaultIds.Contains(Assault.AssaultId)
						|| !Assault.MissionId.IsValid() || AssaultMissionIds.Contains(Assault.MissionId)
						|| !Assault.ContactId.IsValid() || AssaultContactIds.Contains(Assault.ContactId)
						|| !Assault.BaseId.IsValid()
						|| !State.Bases.ContainsByPredicate([&Assault](const FStrategicBaseState& Base) { return Base.BaseId == Assault.BaseId; })
						|| Mission == nullptr || Contact == nullptr
						|| (Mission != nullptr && (Mission->ContactId != Assault.ContactId || Mission->TargetBaseId != Assault.BaseId))
						|| (Contact != nullptr && (Contact->Status != EStrategicContactStatus::Detected
							|| Contact->ElapsedRouteSeconds != Contact->TotalRouteSeconds
							|| Contact->LongitudeMilliDegrees != Contact->DestinationLongitudeMilliDegrees
							|| Contact->LatitudeMilliDegrees != Contact->DestinationLatitudeMilliDegrees))
						|| !IsUsableWallClock(Assault.ArrivedUtc) || Assault.ArrivedUtc > State.StrategicTime.Utc
						|| (Mission != nullptr && Assault.ArrivedUtc < Mission->StartedUtc))
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_base_assault"),
							FString::Printf(TEXT("Base assault '%s' has invalid or inconsistent mission, contact, base, or arrival state."), *Assault.AssaultId.ToString()));
					}
					SeenAssaultIds.Add(Assault.AssaultId);
					AssaultMissionIds.Add(Assault.MissionId);
					AssaultContactIds.Add(Assault.ContactId);
				}
			}
		}

		if (Header.FormatVersion >= 12)
		{
			TSet<FGuid> SeenBattleIds;
			TSet<FGuid> SeenBattleOperationIds;
			for (const FTacticalBattleState& Battle : State.TacticalBattles)
			{
				const FTacticalOperationState* Operation = State.TacticalOperations.FindByPredicate(
					[&Battle](const FTacticalOperationState& Entry) { return Entry.OperationId == Battle.OperationId; });
				const bool bKnownPhase = Battle.Phase == ETacticalBattlePhase::Deployment
					|| Battle.Phase == ETacticalBattlePhase::PlayerTurn
					|| Battle.Phase == ETacticalBattlePhase::AdversaryTurn
					|| Battle.Phase == ETacticalBattlePhase::Resolved;
				const bool bKnownActiveTeam = Battle.ActiveTeam == ETacticalTeam::Player
					|| Battle.ActiveTeam == ETacticalTeam::Adversary;
				const bool bKnownWindDirection = Battle.WindDirection == ETacticalWindDirection::Calm
					|| Battle.WindDirection == ETacticalWindDirection::North
					|| Battle.WindDirection == ETacticalWindDirection::East
					|| Battle.WindDirection == ETacticalWindDirection::South
					|| Battle.WindDirection == ETacticalWindDirection::West;
				const bool bWindValid = bKnownWindDirection && Battle.WindStrength >= 0 && Battle.WindStrength <= 3
					&& ((Battle.WindStrength == 0) == (Battle.WindDirection == ETacticalWindDirection::Calm));
				const bool bPhaseTeamValid = (Battle.Phase == ETacticalBattlePhase::Deployment && Battle.ActiveTeam == ETacticalTeam::Player)
					|| (Battle.Phase == ETacticalBattlePhase::PlayerTurn && Battle.ActiveTeam == ETacticalTeam::Player)
					|| (Battle.Phase == ETacticalBattlePhase::AdversaryTurn && Battle.ActiveTeam == ETacticalTeam::Adversary)
					|| Battle.Phase == ETacticalBattlePhase::Resolved;
				const bool bOperationContextValid = Operation != nullptr && (
					(Operation->Type == ETacticalOperationType::SiteRecovery
						&& Battle.SiteId.IsValid() && Battle.SiteId == Operation->SiteId && Battle.bRequiresExtraction)
					|| (Operation->Type == ETacticalOperationType::BaseDefense
						&& !Battle.SiteId.IsValid() && !Battle.bRequiresExtraction));
				bool bCargoMatches = Operation != nullptr && Battle.Cargo.Num() == Operation->Cargo.Num();
				if (bCargoMatches)
				{
					for (const FInventoryStack& Stack : Battle.Cargo)
					{
						const FInventoryStack* OperationStack = Operation->Cargo.FindByPredicate(
							[&Stack](const FInventoryStack& Entry) { return Entry.ItemId == Stack.ItemId; });
						if (OperationStack == nullptr || OperationStack->Quantity != Stack.Quantity)
						{
							bCargoMatches = false;
							break;
						}
					}
				}
				if (!Battle.BattleId.IsValid() || SeenBattleIds.Contains(Battle.BattleId)
					|| !Battle.OperationId.IsValid() || SeenBattleOperationIds.Contains(Battle.OperationId)
					|| Operation == nullptr || !bOperationContextValid
					|| (Operation != nullptr && (Battle.CreatedUtc != Operation->CreatedUtc
						|| Battle.TacticalRandom.InitialSeed != Operation->TacticalSeed))
					|| !FContentPackageResolver::IsValidPackageId(Battle.MissionRuleId)
					|| !IsUsableWallClock(Battle.CreatedUtc) || Battle.CreatedUtc > State.StrategicTime.Utc
					|| Battle.Width <= 0 || Battle.Width > 64 || Battle.Height <= 0 || Battle.Height > 96
					|| Battle.Levels <= 0 || Battle.Levels > 4
					|| static_cast<int64>(Battle.Width) * Battle.Height * Battle.Levels > 8192
					|| Battle.TurnLimit <= 0 || Battle.TurnLimit > 500
					|| Battle.TurnNumber <= 0 || Battle.TurnNumber > Battle.TurnLimit
					|| !bKnownPhase || !bKnownActiveTeam || !bPhaseTeamValid || !bWindValid
					|| !Battle.TacticalRandom.IsValid()
					|| static_cast<int64>(Battle.Cells.Num()) != static_cast<int64>(Battle.Width) * Battle.Height * Battle.Levels
					|| !bCargoMatches || Battle.Cargo.Num() > 64)
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_tactical_battle"), FString::Printf(TEXT("Tactical battle '%s' has invalid identity, operation link, dimensions, phase, weather, random state, or cargo."), *Battle.BattleId.ToString()));
				}

				TSet<int32> SeenCellIndices;
				int32 DeploymentCellCount = 0;
				int32 ExtractionCellCount = 0;
				int32 CellArrayIndex = 0;
				for (const FTacticalCellState& Cell : Battle.Cells)
				{
					const bool bInBounds = Battle.IsWithinGrid(Cell.X, Cell.Y, Cell.Z);
					const int32 CellIndex = bInBounds ? Battle.GetCellIndex(Cell.X, Cell.Y, Cell.Z) : -1;
					if (!bInBounds || CellIndex != CellArrayIndex || SeenCellIndices.Contains(CellIndex)
						|| !FContentPackageResolver::IsValidPackageId(Cell.TerrainRuleId)
						|| Cell.CurrentIntegrity < 0
						|| (Cell.bDoorOpen && Cell.CurrentIntegrity <= 0)
						|| Cell.Smoke < 0 || Cell.Smoke > 100 || Cell.Fire < 0 || Cell.Fire > 100
						|| (Cell.bExtraction && !Cell.bPlayerDeployment))
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_tactical_cell"), FString::Printf(TEXT("Tactical battle '%s' contains an invalid cell."), *Battle.BattleId.ToString()));
					}
					SeenCellIndices.Add(CellIndex);
					DeploymentCellCount += Cell.bPlayerDeployment ? 1 : 0;
					ExtractionCellCount += Cell.bExtraction ? 1 : 0;
					++CellArrayIndex;
				}
				if (Header.FormatVersion >= 25)
				{
					int32 PreviousDiscoveredCellIndex = INDEX_NONE;
					for (const int32 CellIndex : Battle.PlayerDiscoveredCellIndices)
					{
						if (!Battle.Cells.IsValidIndex(CellIndex) || CellIndex <= PreviousDiscoveredCellIndex)
						{
							AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_tactical_discovery"), FString::Printf(TEXT("Tactical battle '%s' contains invalid discovery indices."), *Battle.BattleId.ToString()));
							break;
						}
						PreviousDiscoveredCellIndex = CellIndex;
					}
				}

				TSet<FGuid> SeenUnitIds;
				TSet<int32> OccupiedCells;
				TSet<FGuid> PlayerPersonnelIds;
				for (const FTacticalUnitState& Unit : Battle.Units)
				{
					const bool bInBounds = Battle.IsWithinGrid(Unit.X, Unit.Y, Unit.Z);
					const int32 CellIndex = bInBounds ? Battle.GetCellIndex(Unit.X, Unit.Y, Unit.Z) : -1;
					const bool bKnownTeam = Unit.Team == ETacticalTeam::Player || Unit.Team == ETacticalTeam::Adversary;
					const bool bKnownStance = Unit.Stance == ETacticalStance::Standing || Unit.Stance == ETacticalStance::Crouched;
					TSet<FName> WeaponItemIds;
					bool bLoadoutValid = Unit.WeaponStates.Num() <= 16 && Unit.CarriedItems.Num() <= 16
						&& Unit.EjectedMagazines.Num() <= 16;
					for (const FTacticalWeaponState& WeaponState : Unit.WeaponStates)
					{
						bLoadoutValid &= FContentPackageResolver::IsValidPackageId(WeaponState.WeaponItemId)
							&& !WeaponItemIds.Contains(WeaponState.WeaponItemId)
							&& WeaponState.LoadedAmmunition >= 0 && WeaponState.LoadedAmmunition <= 200;
						WeaponItemIds.Add(WeaponState.WeaponItemId);
					}
					TSet<FName> CarriedItemIds;
					for (const FInventoryStack& Stack : Unit.CarriedItems)
					{
						bLoadoutValid &= FContentPackageResolver::IsValidPackageId(Stack.ItemId)
							&& !CarriedItemIds.Contains(Stack.ItemId) && Stack.Quantity > 0;
						CarriedItemIds.Add(Stack.ItemId);
					}
					for (const FTacticalMagazineState& Magazine : Unit.EjectedMagazines)
					{
						bLoadoutValid &= FContentPackageResolver::IsValidPackageId(Magazine.WeaponItemId)
							&& FContentPackageResolver::IsValidPackageId(Magazine.AmmunitionItemId)
							&& Magazine.LoadedAmmunition > 0 && Magazine.LoadedAmmunition <= 200;
					}
					if (Header.FormatVersion < 27 && !Unit.EjectedMagazines.IsEmpty())
					{
						bLoadoutValid = false;
					}
					const bool bAttributesValid = Unit.MaxHealth > 0 && Unit.MaxHealth <= 200
						&& Unit.CurrentHealth >= 0 && Unit.CurrentHealth <= Unit.MaxHealth
						&& Unit.Accuracy > 0 && Unit.Accuracy <= 100
						&& Unit.Resolve > 0 && Unit.Resolve <= 100
						&& Unit.Mobility > 0 && Unit.Mobility <= 100
						&& Unit.Strength > 0 && Unit.Strength <= 100
						&& Unit.MaxActionPoints > 0 && Unit.MaxActionPoints <= 20
						&& Unit.RemainingActionPoints >= 0 && Unit.RemainingActionPoints <= Unit.MaxActionPoints
						&& Unit.KineticArmor >= 0 && Unit.KineticArmor <= 100
						&& Unit.ThermalArmor >= 0 && Unit.ThermalArmor <= 100
						&& Unit.ArcArmor >= 0 && Unit.ArcArmor <= 100
						&& Unit.MaxMorale > 0 && Unit.MaxMorale <= 100
						&& Unit.CurrentMorale >= 0 && Unit.CurrentMorale <= Unit.MaxMorale
						&& Unit.Suppression >= 0 && Unit.Suppression <= 100
						&& bLoadoutValid
						&& (!Unit.bExtracted || (Battle.bRequiresExtraction && Unit.CurrentHealth > 0));
					const bool bOccupiesCell = Unit.CurrentHealth > 0 && !Unit.bExtracted;
					if (!Unit.UnitId.IsValid() || SeenUnitIds.Contains(Unit.UnitId)
						|| !bInBounds || (bOccupiesCell && OccupiedCells.Contains(CellIndex))
						|| !FContentPackageResolver::IsValidPackageId(Unit.SourceRuleId)
						|| Unit.DisplayName.TrimStartAndEnd().IsEmpty() || Unit.DisplayName.Len() > 64
						|| !bKnownTeam || !bKnownStance || !bAttributesValid)
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_tactical_unit"), FString::Printf(TEXT("Tactical battle '%s' contains an invalid unit."), *Battle.BattleId.ToString()));
					}
					if (Unit.Team == ETacticalTeam::Player)
					{
						const FPersonnelState* Person = State.Personnel.FindByPredicate(
							[&Unit](const FPersonnelState& Entry) { return Entry.PersonnelId == Unit.PersonnelId; });
						if (!Unit.PersonnelId.IsValid() || PlayerPersonnelIds.Contains(Unit.PersonnelId)
							|| Operation == nullptr || !Operation->AgentIds.Contains(Unit.PersonnelId) || Person == nullptr)
						{
							AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_tactical_player_unit"), FString::Printf(TEXT("Tactical battle '%s' contains a player unit outside its operation roster."), *Battle.BattleId.ToString()));
						}
						PlayerPersonnelIds.Add(Unit.PersonnelId);
					}
					else if (Unit.PersonnelId.IsValid() || Unit.bExtracted
						|| !Unit.WeaponStates.IsEmpty() || !Unit.CarriedItems.IsEmpty())
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_tactical_adversary_unit"), FString::Printf(TEXT("Tactical battle '%s' adversary unit has a strategic personnel id."), *Battle.BattleId.ToString()));
					}
					SeenUnitIds.Add(Unit.UnitId);
					if (bOccupiesCell)
					{
						OccupiedCells.Add(CellIndex);
					}
				}
				if (Operation != nullptr && (PlayerPersonnelIds.Num() != Operation->AgentIds.Num()
					|| DeploymentCellCount < Operation->AgentIds.Num()
					|| (Battle.bRequiresExtraction ? ExtractionCellCount <= 0 : ExtractionCellCount != 0)))
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_tactical_population"), FString::Printf(TEXT("Tactical battle '%s' has inconsistent player population or zones."), *Battle.BattleId.ToString()));
				}

				TSet<FName> SeenObjectiveIds;
				if (Battle.Objectives.IsEmpty() || Battle.Objectives.Num() > 16)
				{
					AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_tactical_objective"), FString::Printf(TEXT("Tactical battle '%s' has no supported objectives."), *Battle.BattleId.ToString()));
				}
				for (const FTacticalObjectiveState& Objective : Battle.Objectives)
				{
					const bool bKnownStatus = Objective.Status == ETacticalObjectiveStatus::Active
						|| Objective.Status == ETacticalObjectiveStatus::Completed
						|| Objective.Status == ETacticalObjectiveStatus::Failed;
					const bool bKnownType = Objective.Type == ETacticalObjectiveType::Disrupt
						|| Objective.Type == ETacticalObjectiveType::Recover
						|| Objective.Type == ETacticalObjectiveType::Control;
					if (!FContentPackageResolver::IsValidPackageId(Objective.ObjectiveId)
						|| SeenObjectiveIds.Contains(Objective.ObjectiveId)
						|| !Battle.IsWithinGrid(Objective.X, Objective.Y, Objective.Z)
						|| !bKnownStatus || !bKnownType || Objective.RequiredInteractions <= 0
						|| Objective.CompletedInteractions < 0 || Objective.CompletedInteractions > Objective.RequiredInteractions
						|| Objective.AdversaryInteractions < 0 || Objective.AdversaryInteractions > Objective.RequiredInteractions
						|| (Objective.Type != ETacticalObjectiveType::Control && Objective.AdversaryInteractions != 0)
						|| (Objective.CompletedInteractions > 0 && Objective.AdversaryInteractions > 0))
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_tactical_objective"), FString::Printf(TEXT("Tactical battle '%s' contains an invalid objective."), *Battle.BattleId.ToString()));
					}
					SeenObjectiveIds.Add(Objective.ObjectiveId);
				}
				TSet<FName> CargoItemIds;
				for (const FInventoryStack& Stack : Battle.Cargo)
				{
					if (!FContentPackageResolver::IsValidPackageId(Stack.ItemId)
						|| Stack.Quantity <= 0 || CargoItemIds.Contains(Stack.ItemId))
					{
						AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_tactical_cargo"), FString::Printf(TEXT("Tactical battle '%s' contains invalid cargo."), *Battle.BattleId.ToString()));
					}
					CargoItemIds.Add(Stack.ItemId);
				}
				SeenBattleIds.Add(Battle.BattleId);
				SeenBattleOperationIds.Add(Battle.OperationId);
			}
		}

		if (Header.FormatVersion >= 2 || !bAllowLegacyMissingChecksum)
		{
			const FString ComputedChecksum = ComputeSaveChecksum(Envelope);
			if (ComputedChecksum.IsEmpty() || Header.SaveChecksum != ComputedChecksum)
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("save_checksum_mismatch"), TEXT("Campaign save checksum does not match its persisted state."));
			}
		}
		else if (Header.SaveChecksum.IsEmpty())
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Warning, TEXT("legacy_save_without_checksum"), TEXT("Legacy format predates whole-save checksum protection."));
		}

		Result.bSucceeded = !HasErrors(Result.Diagnostics);
		return Result;
	}

	void WarnUnknownFields(
		const TSharedPtr<FJsonObject>& Object,
		const TSet<FString>& AllowedFields,
		const FString& Context,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		TArray<FString> UnknownFields;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
		{
			if (!AllowedFields.Contains(Pair.Key))
			{
				UnknownFields.Add(Pair.Key);
			}
		}
		UnknownFields.Sort();
		for (const FString& Field : UnknownFields)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Warning, TEXT("unknown_field"), FString::Printf(TEXT("%s contains unknown field '%s'."), *Context, *Field));
		}
	}

	bool ReadRequiredString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		FString& OutValue,
		const FString& Context,
		TArray<FCampaignSaveDiagnostic>& Diagnostics,
		const bool bAllowEmpty = false)
	{
		if (!Object->HasField(Field))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required string '%s'."), *Context, Field));
			return false;
		}
		if (!Object->TryGetStringField(Field, OutValue))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s must be a string."), *Context, Field));
			return false;
		}
		if (!bAllowEmpty && OutValue.TrimStartAndEnd().IsEmpty())
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s cannot be empty."), *Context, Field));
			return false;
		}
		return true;
	}

	bool ReadOptionalString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		FString& OutValue,
		const FString& Context,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		if (!Object->HasField(Field))
		{
			return true;
		}
		if (!Object->TryGetStringField(Field, OutValue))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s must be a string."), *Context, Field));
			return false;
		}
		return true;
	}

	bool ReadInt32(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		int32& OutValue,
		const FString& Context,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		double Number = 0.0;
		if (!Object->HasField(Field))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required integer '%s'."), *Context, Field));
			return false;
		}
		if (!Object->TryGetNumberField(Field, Number)
			|| !FMath::IsFinite(Number)
			|| Number != FMath::TruncToDouble(Number)
			|| Number < static_cast<double>(MIN_int32)
			|| Number > static_cast<double>(MAX_int32))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s must be a 32-bit integer."), *Context, Field));
			return false;
		}
		OutValue = static_cast<int32>(Number);
		return true;
	}

	bool ReadBool(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		bool& OutValue,
		const FString& Context,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		if (!Object->HasField(Field))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required boolean '%s'."), *Context, Field));
			return false;
		}
		const TSharedPtr<FJsonValue>* Value = Object->Values.Find(Field);
		if (Value == nullptr || !Value->IsValid() || (*Value)->Type != EJson::Boolean)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s must be a boolean."), *Context, Field));
			return false;
		}
		OutValue = (*Value)->AsBool();
		return true;
	}

	bool IsCanonicalInt64(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return false;
		}
		int32 Index = 0;
		if (Value[0] == TEXT('-'))
		{
			if (Value.Len() == 1 || Value[1] == TEXT('0'))
			{
				return false;
			}
			Index = 1;
		}
		else if (Value.Len() > 1 && Value[0] == TEXT('0'))
		{
			return false;
		}
		for (; Index < Value.Len(); ++Index)
		{
			if (Value[Index] < TEXT('0') || Value[Index] > TEXT('9'))
			{
				return false;
			}
		}
		return true;
	}

	bool ReadInt64String(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		int64& OutValue,
		const FString& Context,
		TArray<FCampaignSaveDiagnostic>& Diagnostics,
		const bool bRequired = true)
	{
		if (!Object->HasField(Field) && !bRequired)
		{
			return true;
		}
		FString Text;
		if (!ReadRequiredString(Object, Field, Text, Context, Diagnostics))
		{
			return false;
		}
		if (!IsCanonicalInt64(Text) || !LexTryParseString(OutValue, *Text))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s must be a canonical signed 64-bit decimal string."), *Context, Field));
			return false;
		}
		return true;
	}

	bool ReadUInt64Hex(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		uint64& OutValue,
		const FString& Context,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		FString Text;
		if (!ReadRequiredString(Object, Field, Text, Context, Diagnostics))
		{
			return false;
		}
		if (Text.Len() != 16)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s must contain exactly 16 hexadecimal digits."), *Context, Field));
			return false;
		}

		uint64 Value = 0;
		for (const TCHAR Character : Text)
		{
			uint8 Digit = 0;
			if (Character >= TEXT('0') && Character <= TEXT('9'))
			{
				Digit = static_cast<uint8>(Character - TEXT('0'));
			}
			else if (Character >= TEXT('a') && Character <= TEXT('f'))
			{
				Digit = static_cast<uint8>(Character - TEXT('a') + 10);
			}
			else
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s must use lowercase hexadecimal digits."), *Context, Field));
				return false;
			}
			Value = (Value << 4) | Digit;
		}
		OutValue = Value;
		return true;
	}

	bool ReadIsoDate(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		FDateTime& OutValue,
		const FString& Context,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		FString Text;
		if (!ReadRequiredString(Object, Field, Text, Context, Diagnostics)
			|| !FDateTime::ParseIso8601(*Text, OutValue))
		{
			if (!Text.IsEmpty())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s must be an ISO-8601 UTC timestamp."), *Context, Field));
			}
			return false;
		}
		return true;
	}

	bool ReadNameArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		TArray<FName>& OutValues,
		const FString& Context,
		TArray<FCampaignSaveDiagnostic>& Diagnostics,
		const bool bRequired)
	{
		if (!Object->HasField(Field))
		{
			if (bRequired)
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required array '%s'."), *Context, Field));
				return false;
			}
			return true;
		}
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(Field, Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s must be an array of id strings."), *Context, Field));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			FString Value;
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetString(Value))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s[%d] must be a string."), *Context, Field, Index));
				bValid = false;
				continue;
			}
			OutValues.Add(FName(*Value));
		}
		return bValid;
	}

	bool ReadInt32Array(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		TArray<int32>& OutValues,
		const FString& Context,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->HasField(Field))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required array '%s'."), *Context, Field));
			return false;
		}
		if (!Object->TryGetArrayField(Field, Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s must be an array of integers."), *Context, Field));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			double Number = 0.0;
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetNumber(Number))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s[%d] must be an integer."), *Context, Field, Index));
				bValid = false;
				continue;
			}
			if (!FMath::IsFinite(Number) || Number != FMath::TruncToDouble(Number)
				|| Number < static_cast<double>(MIN_int32) || Number > static_cast<double>(MAX_int32))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s[%d] must fit an integer."), *Context, Field, Index));
				bValid = false;
				continue;
			}
			OutValues.Add(static_cast<int32>(Number));
		}
		return bValid;
	}

	bool ReadContentPackages(
		const TSharedPtr<FJsonObject>& HeaderObject,
		TArray<FCampaignContentVersion>& OutPackages,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!HeaderObject->HasField(TEXT("contentPackages")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.header is missing required array 'contentPackages'."));
			return false;
		}
		if (!HeaderObject->TryGetArrayField(TEXT("contentPackages"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.header.contentPackages must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* PackageObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.header.contentPackages[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(PackageObject) || PackageObject == nullptr || !PackageObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*PackageObject, { TEXT("id"), TEXT("version") }, Context, Diagnostics);
			FString Id;
			FCampaignContentVersion Package;
			const bool bIdValid = ReadRequiredString(*PackageObject, TEXT("id"), Id, Context, Diagnostics);
			const bool bVersionValid = ReadRequiredString(*PackageObject, TEXT("version"), Package.Version, Context, Diagnostics);
			if (bIdValid)
			{
				Package.PackageId = FName(*Id);
			}
			if (bIdValid && bVersionValid)
			{
				OutPackages.Add(MoveTemp(Package));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadGuidString(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		FGuid& OutValue,
		const FString& Context,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		FString Text;
		if (!ReadRequiredString(Object, Field, Text, Context, Diagnostics))
		{
			return false;
		}
		if (!FGuid::Parse(Text, OutValue))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s must be a GUID."), *Context, Field));
			return false;
		}
		return true;
	}

	bool ReadGuidArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		TArray<FGuid>& OutValues,
		const FString& Context,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->HasField(Field))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required array '%s'."), *Context, Field));
			return false;
		}
		if (!Object->TryGetArrayField(Field, Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s must be an array of GUID strings."), *Context, Field));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			FString Value;
			FGuid Guid;
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetString(Value) || !FGuid::Parse(Value, Guid))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), FString::Printf(TEXT("%s.%s[%d] must be a GUID string."), *Context, Field, Index));
				bValid = false;
				continue;
			}
			OutValues.Add(Guid);
		}
		return bValid;
	}

	bool ReadInventoryArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		TArray<FInventoryStack>& OutInventory,
		const FString& Context,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->HasField(Field))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required array '%s'."), *Context, Field));
			return false;
		}
		if (!Object->TryGetArrayField(Field, Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.%s must be an array."), *Context, Field));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* StackObject = nullptr;
			const FString StackContext = FString::Printf(TEXT("%s.%s[%d]"), *Context, Field, Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(StackObject) || StackObject == nullptr || !StackObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), StackContext + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*StackObject, { TEXT("itemId"), TEXT("quantity") }, StackContext, Diagnostics);
			FInventoryStack Stack;
			FString ItemId;
			bool bStackValid = ReadRequiredString(*StackObject, TEXT("itemId"), ItemId, StackContext, Diagnostics);
			Stack.ItemId = FName(*ItemId);
			bStackValid &= ReadInt32(*StackObject, TEXT("quantity"), Stack.Quantity, StackContext, Diagnostics);
			if (bStackValid)
			{
				OutInventory.Add(MoveTemp(Stack));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadInventory(
		const TSharedPtr<FJsonObject>& BaseObject,
		TArray<FInventoryStack>& OutInventory,
		const FString& BaseContext,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		return ReadInventoryArray(BaseObject, TEXT("inventory"), OutInventory, BaseContext, Diagnostics);
	}

	bool ReadFacilityPlacements(
		const TSharedPtr<FJsonObject>& BaseObject,
		TArray<FBaseFacilityState>& OutFacilities,
		const FString& BaseContext,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!BaseObject->HasField(TEXT("facilityPlacements")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required array 'facilityPlacements'."), *BaseContext));
			return false;
		}
		if (!BaseObject->TryGetArrayField(TEXT("facilityPlacements"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.facilityPlacements must be an array."), *BaseContext));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* FacilityObject = nullptr;
			const FString Context = FString::Printf(TEXT("%s.facilityPlacements[%d]"), *BaseContext, Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(FacilityObject) || FacilityObject == nullptr || !FacilityObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*FacilityObject, { TEXT("instanceId"), TEXT("facilityId"), TEXT("gridX"), TEXT("gridY"), TEXT("damage"), TEXT("reservedRepairDamage"), TEXT("remainingRepairSeconds") }, Context, Diagnostics);
			FBaseFacilityState Facility;
			FString FacilityId;
			bool bFacilityValid = ReadGuidString(*FacilityObject, TEXT("instanceId"), Facility.InstanceId, Context, Diagnostics);
			bFacilityValid &= ReadRequiredString(*FacilityObject, TEXT("facilityId"), FacilityId, Context, Diagnostics);
			Facility.FacilityId = FName(*FacilityId);
			bFacilityValid &= ReadInt32(*FacilityObject, TEXT("gridX"), Facility.GridX, Context, Diagnostics);
			bFacilityValid &= ReadInt32(*FacilityObject, TEXT("gridY"), Facility.GridY, Context, Diagnostics);
			if (FormatVersion >= 19)
			{
				bFacilityValid &= ReadInt32(*FacilityObject, TEXT("damage"), Facility.Damage, Context, Diagnostics);
				bFacilityValid &= ReadInt32(*FacilityObject, TEXT("reservedRepairDamage"), Facility.ReservedRepairDamage, Context, Diagnostics);
				bFacilityValid &= ReadInt64String(*FacilityObject, TEXT("remainingRepairSeconds"), Facility.RemainingRepairSeconds, Context, Diagnostics);
			}
			if (bFacilityValid)
			{
				OutFacilities.Add(MoveTemp(Facility));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadBases(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FStrategicBaseState>& OutBases,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("bases")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'bases'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("bases"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.bases must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* BaseObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.bases[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(BaseObject) || BaseObject == nullptr || !BaseObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*BaseObject, { TEXT("id"), TEXT("name"), TEXT("regionId"), TEXT("longitudeMilliDegrees"), TEXT("latitudeMilliDegrees"), TEXT("scientistCapacity"), TEXT("engineerCapacity"), TEXT("signalWatchScientists"), TEXT("worksCadreEngineers"), TEXT("worksCadreCharter"), TEXT("facilities"), TEXT("facilityPlacements"), TEXT("inventory") }, Context, Diagnostics);

			FStrategicBaseState Base;
			FString RegionId;
			bool bBaseValid = ReadGuidString(*BaseObject, TEXT("id"), Base.BaseId, Context, Diagnostics);
			bBaseValid &= ReadRequiredString(*BaseObject, TEXT("name"), Base.Name, Context, Diagnostics);
			bBaseValid &= ReadRequiredString(*BaseObject, TEXT("regionId"), RegionId, Context, Diagnostics);
			Base.RegionId = FName(*RegionId);
			bBaseValid &= ReadInt32(*BaseObject, TEXT("longitudeMilliDegrees"), Base.LongitudeMilliDegrees, Context, Diagnostics);
			bBaseValid &= ReadInt32(*BaseObject, TEXT("latitudeMilliDegrees"), Base.LatitudeMilliDegrees, Context, Diagnostics);
			bBaseValid &= ReadInt32(*BaseObject, TEXT("scientistCapacity"), Base.ScientistCapacity, Context, Diagnostics);
			bBaseValid &= ReadInt32(*BaseObject, TEXT("engineerCapacity"), Base.EngineerCapacity, Context, Diagnostics);
			if (FormatVersion >= 39)
			{
				bBaseValid &= ReadInt32(*BaseObject, TEXT("signalWatchScientists"),
					Base.SignalWatchScientists, Context, Diagnostics);
			}
			if (FormatVersion >= 42)
			{
				bBaseValid &= ReadInt32(*BaseObject, TEXT("worksCadreEngineers"),
					Base.WorksCadreEngineers, Context, Diagnostics);
			}
			if (FormatVersion >= 43)
			{
				FString WorksCadreCharter;
				bBaseValid &= ReadRequiredString(*BaseObject, TEXT("worksCadreCharter"),
					WorksCadreCharter, Context, Diagnostics);
				if (!WorksCadreCharter.IsEmpty()
					&& !TryParseWorksCadreCharter(
						WorksCadreCharter, Base.WorksCadreCharter))
				{
					AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
						TEXT("invalid_field_value"),
						Context + TEXT(".worksCadreCharter is unknown."));
					bBaseValid = false;
				}
			}
			bBaseValid &= ReadNameArray(*BaseObject, TEXT("facilities"), Base.BuiltFacilities, Context, Diagnostics, true);
			if (FormatVersion >= 4)
			{
				bBaseValid &= ReadInventory(*BaseObject, Base.Inventory, Context, Diagnostics);
			}
			if (FormatVersion >= 5)
			{
				bBaseValid &= ReadFacilityPlacements(*BaseObject, Base.Facilities, Context, FormatVersion, Diagnostics);
			}
			if (bBaseValid)
			{
				OutBases.Add(MoveTemp(Base));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadMutualAidConvoys(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FMutualAidConvoyState>& OutConvoys,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("mutualAidConvoys")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"),
				TEXT("save.state is missing required array 'mutualAidConvoys'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("mutualAidConvoys"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"),
				TEXT("save.state.mutualAidConvoys must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* ConvoyObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.mutualAidConvoys[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(ConvoyObject)
				|| ConvoyObject == nullptr || !ConvoyObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
					TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			TSet<FString> AllowedFields = {
				TEXT("convoyId"), TEXT("sourceBaseId"), TEXT("destinationBaseId"),
				TEXT("itemId"), TEXT("quantity"), TEXT("remainingTransitSeconds") };
			if (FormatVersion >= 37)
			{
				AllowedFields.Add(TEXT("routePolicy"));
				AllowedFields.Add(TEXT("totalTransitSeconds"));
				AllowedFields.Add(TEXT("routePressure"));
				AllowedFields.Add(TEXT("signalEscort"));
				AllowedFields.Add(TEXT("signalEscortCost"));
				AllowedFields.Add(TEXT("interdictionResolved"));
				AllowedFields.Add(TEXT("forecastInterdictionDelaySeconds"));
				AllowedFields.Add(TEXT("interdictionDelaySeconds"));
			}
			if (FormatVersion >= 38)
			{
				AllowedFields.Add(TEXT("dispatchSequence"));
			}
			if (FormatVersion >= 40)
			{
				AllowedFields.Add(TEXT("currentLegOriginBaseId"));
				AllowedFields.Add(TEXT("relayWaypointBaseId"));
				AllowedFields.Add(TEXT("onwardRoutePolicy"));
				AllowedFields.Add(TEXT("onwardTotalTransitSeconds"));
				AllowedFields.Add(TEXT("onwardRoutePressure"));
				AllowedFields.Add(TEXT("onwardInterdictionResolved"));
				AllowedFields.Add(TEXT("onwardForecastInterdictionDelaySeconds"));
			}
			if (FormatVersion >= 41)
			{
				AllowedFields.Add(TEXT("balancedHandoffQuantity"));
			}
			WarnUnknownFields(*ConvoyObject, AllowedFields, Context, Diagnostics);

			FMutualAidConvoyState Convoy;
			FString ItemId;
			bool bConvoyValid = ReadGuidString(
				*ConvoyObject, TEXT("convoyId"), Convoy.ConvoyId, Context, Diagnostics);
			bConvoyValid &= ReadGuidString(
				*ConvoyObject, TEXT("sourceBaseId"), Convoy.SourceBaseId, Context, Diagnostics);
			bConvoyValid &= ReadGuidString(
				*ConvoyObject, TEXT("destinationBaseId"), Convoy.DestinationBaseId, Context, Diagnostics);
			bConvoyValid &= ReadRequiredString(
				*ConvoyObject, TEXT("itemId"), ItemId, Context, Diagnostics);
			Convoy.ItemId = FName(*ItemId);
			bConvoyValid &= ReadInt32(
				*ConvoyObject, TEXT("quantity"), Convoy.Quantity, Context, Diagnostics);
			bConvoyValid &= ReadInt64String(
				*ConvoyObject, TEXT("remainingTransitSeconds"),
				Convoy.RemainingTransitSeconds, Context, Diagnostics);
			if (FormatVersion >= 37)
			{
				FString RoutePolicy;
				bConvoyValid &= ReadRequiredString(
					*ConvoyObject, TEXT("routePolicy"), RoutePolicy, Context, Diagnostics);
				if (!RoutePolicy.IsEmpty()
					&& !TryParseMutualAidRoutePolicy(RoutePolicy, Convoy.RoutePolicy))
				{
					AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
						TEXT("invalid_field_value"), Context + TEXT(".routePolicy is unknown."));
					bConvoyValid = false;
				}
				bConvoyValid &= ReadInt64String(
					*ConvoyObject, TEXT("totalTransitSeconds"),
					Convoy.TotalTransitSeconds, Context, Diagnostics);
				bConvoyValid &= ReadInt32(
					*ConvoyObject, TEXT("routePressure"),
					Convoy.RoutePressure, Context, Diagnostics);
				bConvoyValid &= ReadBool(
					*ConvoyObject, TEXT("signalEscort"),
					Convoy.bSignalEscort, Context, Diagnostics);
				bConvoyValid &= ReadInt64String(
					*ConvoyObject, TEXT("signalEscortCost"),
					Convoy.SignalEscortCost, Context, Diagnostics);
				bConvoyValid &= ReadBool(
					*ConvoyObject, TEXT("interdictionResolved"),
					Convoy.bInterdictionResolved, Context, Diagnostics);
				bConvoyValid &= ReadInt64String(
					*ConvoyObject, TEXT("forecastInterdictionDelaySeconds"),
					Convoy.ForecastInterdictionDelaySeconds, Context, Diagnostics);
				bConvoyValid &= ReadInt64String(
					*ConvoyObject, TEXT("interdictionDelaySeconds"),
					Convoy.InterdictionDelaySeconds, Context, Diagnostics);
			}
			if (FormatVersion >= 38)
			{
				bConvoyValid &= ReadInt64String(
					*ConvoyObject, TEXT("dispatchSequence"),
					Convoy.DispatchSequence, Context, Diagnostics);
			}
			if (FormatVersion >= 40)
			{
				bConvoyValid &= ReadGuidString(
					*ConvoyObject, TEXT("currentLegOriginBaseId"),
					Convoy.CurrentLegOriginBaseId, Context, Diagnostics);
				bConvoyValid &= ReadGuidString(
					*ConvoyObject, TEXT("relayWaypointBaseId"),
					Convoy.RelayWaypointBaseId, Context, Diagnostics);
				FString OnwardRoutePolicy;
				bConvoyValid &= ReadRequiredString(
					*ConvoyObject, TEXT("onwardRoutePolicy"),
					OnwardRoutePolicy, Context, Diagnostics);
				if (!OnwardRoutePolicy.IsEmpty()
					&& !TryParseMutualAidRoutePolicy(
						OnwardRoutePolicy, Convoy.OnwardRoutePolicy))
				{
					AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
						TEXT("invalid_field_value"),
						Context + TEXT(".onwardRoutePolicy is unknown."));
					bConvoyValid = false;
				}
				bConvoyValid &= ReadInt64String(
					*ConvoyObject, TEXT("onwardTotalTransitSeconds"),
					Convoy.OnwardTotalTransitSeconds, Context, Diagnostics);
				bConvoyValid &= ReadInt32(
					*ConvoyObject, TEXT("onwardRoutePressure"),
					Convoy.OnwardRoutePressure, Context, Diagnostics);
				bConvoyValid &= ReadBool(
					*ConvoyObject, TEXT("onwardInterdictionResolved"),
					Convoy.bOnwardInterdictionResolved, Context, Diagnostics);
				bConvoyValid &= ReadInt64String(
					*ConvoyObject, TEXT("onwardForecastInterdictionDelaySeconds"),
					Convoy.OnwardForecastInterdictionDelaySeconds,
					Context, Diagnostics);
			}
			if (FormatVersion >= 41)
			{
				bConvoyValid &= ReadInt32(
					*ConvoyObject, TEXT("balancedHandoffQuantity"),
					Convoy.BalancedHandoffQuantity, Context, Diagnostics);
			}
			if (bConvoyValid)
			{
				OutConvoys.Add(MoveTemp(Convoy));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadResearchProjects(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FResearchProjectState>& OutProjects,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("researchProjects")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'researchProjects'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("researchProjects"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.researchProjects must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* ProjectObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.researchProjects[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(ProjectObject) || ProjectObject == nullptr || !ProjectObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*ProjectObject, { TEXT("researchId"), TEXT("baseId"), TEXT("assignedScientists"), TEXT("accumulatedWorkSeconds") }, Context, Diagnostics);

			FResearchProjectState Project;
			FString ResearchId;
			bool bProjectValid = ReadRequiredString(*ProjectObject, TEXT("researchId"), ResearchId, Context, Diagnostics);
			Project.ResearchId = FName(*ResearchId);
			bProjectValid &= ReadGuidString(*ProjectObject, TEXT("baseId"), Project.BaseId, Context, Diagnostics);
			bProjectValid &= ReadInt32(*ProjectObject, TEXT("assignedScientists"), Project.AssignedScientists, Context, Diagnostics);
			bProjectValid &= ReadInt64String(*ProjectObject, TEXT("accumulatedWorkSeconds"), Project.AccumulatedWorkSeconds, Context, Diagnostics);
			if (bProjectValid)
			{
				OutProjects.Add(MoveTemp(Project));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadManufacturingProjects(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FManufacturingProjectState>& OutProjects,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("manufacturingProjects")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'manufacturingProjects'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("manufacturingProjects"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.manufacturingProjects must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* ProjectObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.manufacturingProjects[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(ProjectObject) || ProjectObject == nullptr || !ProjectObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*ProjectObject, { TEXT("projectId"), TEXT("itemId"), TEXT("baseId"), TEXT("assignedEngineers"), TEXT("unitsRemaining"), TEXT("accumulatedWorkSeconds") }, Context, Diagnostics);

			FManufacturingProjectState Project;
			FString ItemId;
			bool bProjectValid = ReadGuidString(*ProjectObject, TEXT("projectId"), Project.ProjectId, Context, Diagnostics);
			bProjectValid &= ReadRequiredString(*ProjectObject, TEXT("itemId"), ItemId, Context, Diagnostics);
			Project.ItemId = FName(*ItemId);
			bProjectValid &= ReadGuidString(*ProjectObject, TEXT("baseId"), Project.BaseId, Context, Diagnostics);
			bProjectValid &= ReadInt32(*ProjectObject, TEXT("assignedEngineers"), Project.AssignedEngineers, Context, Diagnostics);
			bProjectValid &= ReadInt32(*ProjectObject, TEXT("unitsRemaining"), Project.UnitsRemaining, Context, Diagnostics);
			bProjectValid &= ReadInt64String(*ProjectObject, TEXT("accumulatedWorkSeconds"), Project.AccumulatedWorkSeconds, Context, Diagnostics);
			if (bProjectValid)
			{
				OutProjects.Add(MoveTemp(Project));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadFacilityConstructionProjects(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FFacilityConstructionProjectState>& OutProjects,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("facilityConstructionProjects")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'facilityConstructionProjects'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("facilityConstructionProjects"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.facilityConstructionProjects must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* ProjectObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.facilityConstructionProjects[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(ProjectObject) || ProjectObject == nullptr || !ProjectObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*ProjectObject, { TEXT("projectId"), TEXT("facilityInstanceId"), TEXT("baseId"), TEXT("facilityId"), TEXT("gridX"), TEXT("gridY"), TEXT("remainingBuildSeconds") }, Context, Diagnostics);
			FFacilityConstructionProjectState Project;
			FString FacilityId;
			bool bProjectValid = ReadGuidString(*ProjectObject, TEXT("projectId"), Project.ProjectId, Context, Diagnostics);
			bProjectValid &= ReadGuidString(*ProjectObject, TEXT("facilityInstanceId"), Project.FacilityInstanceId, Context, Diagnostics);
			bProjectValid &= ReadGuidString(*ProjectObject, TEXT("baseId"), Project.BaseId, Context, Diagnostics);
			bProjectValid &= ReadRequiredString(*ProjectObject, TEXT("facilityId"), FacilityId, Context, Diagnostics);
			Project.FacilityId = FName(*FacilityId);
			bProjectValid &= ReadInt32(*ProjectObject, TEXT("gridX"), Project.GridX, Context, Diagnostics);
			bProjectValid &= ReadInt32(*ProjectObject, TEXT("gridY"), Project.GridY, Context, Diagnostics);
			bProjectValid &= ReadInt64String(*ProjectObject, TEXT("remainingBuildSeconds"), Project.RemainingBuildSeconds, Context, Diagnostics);
			if (bProjectValid)
			{
				OutProjects.Add(MoveTemp(Project));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadPersonnel(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FPersonnelState>& OutPersonnel,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("personnel")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'personnel'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("personnel"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.personnel must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* PersonObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.personnel[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(PersonObject) || PersonObject == nullptr || !PersonObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			TSet<FString> AllowedFields = {
				TEXT("id"), TEXT("displayName"), TEXT("roleId"), TEXT("baseId"), TEXT("status"), TEXT("rank"), TEXT("missions"), TEXT("kills"),
				TEXT("maxHealth"), TEXT("currentHealth"), TEXT("accuracy"), TEXT("resolve"), TEXT("mobility"), TEXT("strength"),
				TEXT("remainingRecoverySeconds"), TEXT("remainingTrainingSeconds"), TEXT("trainingFocus"), TEXT("equippedItems"),
				TEXT("pendingDoctrineChoices"), TEXT("doctrineSelections"), TEXT("commendations") };
			if (FormatVersion >= 18)
			{
				AllowedFields.Add(TEXT("experience"));
			}
			if (FormatVersion >= 34)
			{
				AllowedFields.Add(TEXT("recoveryPlan"));
			}
			if (FormatVersion >= 35)
			{
				AllowedFields.Add(TEXT("stewardshipFocus"));
				AllowedFields.Add(TEXT("remainingStewardshipSeconds"));
				AllowedFields.Add(TEXT("stewardshipToursCompleted"));
			}
			WarnUnknownFields(*PersonObject, AllowedFields, Context, Diagnostics);

			FPersonnelState Person;
			FString RoleId;
			FString Status;
			FString RecoveryPlan;
			FString TrainingFocus;
			FString StewardshipFocus;
			bool bPersonValid = ReadGuidString(*PersonObject, TEXT("id"), Person.PersonnelId, Context, Diagnostics);
			bPersonValid &= ReadRequiredString(*PersonObject, TEXT("displayName"), Person.DisplayName, Context, Diagnostics);
			bPersonValid &= ReadRequiredString(*PersonObject, TEXT("roleId"), RoleId, Context, Diagnostics);
			Person.RoleId = FName(*RoleId);
			bPersonValid &= ReadGuidString(*PersonObject, TEXT("baseId"), Person.BaseId, Context, Diagnostics);
			bPersonValid &= ReadRequiredString(*PersonObject, TEXT("status"), Status, Context, Diagnostics);
			if (!Status.IsEmpty() && !TryParsePersonnelStatus(Status, Person.Status))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), Context + TEXT(".status is unknown."));
				bPersonValid = false;
			}
			bPersonValid &= ReadInt32(*PersonObject, TEXT("rank"), Person.Rank, Context, Diagnostics);
			bPersonValid &= ReadInt32(*PersonObject, TEXT("missions"), Person.Missions, Context, Diagnostics);
			bPersonValid &= ReadInt32(*PersonObject, TEXT("kills"), Person.Kills, Context, Diagnostics);
			if (FormatVersion >= 18)
			{
				bPersonValid &= ReadInt32(*PersonObject, TEXT("experience"), Person.Experience, Context, Diagnostics);
			}
			bPersonValid &= ReadInt32(*PersonObject, TEXT("maxHealth"), Person.MaxHealth, Context, Diagnostics);
			bPersonValid &= ReadInt32(*PersonObject, TEXT("currentHealth"), Person.CurrentHealth, Context, Diagnostics);
			bPersonValid &= ReadInt32(*PersonObject, TEXT("accuracy"), Person.Accuracy, Context, Diagnostics);
			bPersonValid &= ReadInt32(*PersonObject, TEXT("resolve"), Person.Resolve, Context, Diagnostics);
			bPersonValid &= ReadInt32(*PersonObject, TEXT("mobility"), Person.Mobility, Context, Diagnostics);
			bPersonValid &= ReadInt32(*PersonObject, TEXT("strength"), Person.Strength, Context, Diagnostics);
			bPersonValid &= ReadInt64String(*PersonObject, TEXT("remainingRecoverySeconds"), Person.RemainingRecoverySeconds, Context, Diagnostics);
			if (FormatVersion >= 34)
			{
				bPersonValid &= ReadRequiredString(*PersonObject, TEXT("recoveryPlan"), RecoveryPlan, Context, Diagnostics);
				if (!RecoveryPlan.IsEmpty() && !TryParseRecoveryPlan(RecoveryPlan, Person.RecoveryPlan))
				{
					AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
						TEXT("invalid_field_value"), Context + TEXT(".recoveryPlan is unknown."));
					bPersonValid = false;
				}
			}
			bPersonValid &= ReadInt64String(*PersonObject, TEXT("remainingTrainingSeconds"), Person.RemainingTrainingSeconds, Context, Diagnostics);
			bPersonValid &= ReadRequiredString(*PersonObject, TEXT("trainingFocus"), TrainingFocus, Context, Diagnostics);
			if (!TrainingFocus.IsEmpty() && !TryParseTrainingFocus(TrainingFocus, Person.TrainingFocus))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), Context + TEXT(".trainingFocus is unknown."));
				bPersonValid = false;
			}
			if (FormatVersion >= 35)
			{
				bPersonValid &= ReadRequiredString(*PersonObject, TEXT("stewardshipFocus"),
					StewardshipFocus, Context, Diagnostics);
				if (!StewardshipFocus.IsEmpty()
					&& !TryParseStewardshipFocus(StewardshipFocus, Person.StewardshipFocus))
				{
					AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error,
						TEXT("invalid_field_value"), Context + TEXT(".stewardshipFocus is unknown."));
					bPersonValid = false;
				}
				bPersonValid &= ReadInt64String(*PersonObject, TEXT("remainingStewardshipSeconds"),
					Person.RemainingStewardshipSeconds, Context, Diagnostics);
				bPersonValid &= ReadInt32(*PersonObject, TEXT("stewardshipToursCompleted"),
					Person.StewardshipToursCompleted, Context, Diagnostics);
			}
			bPersonValid &= ReadNameArray(*PersonObject, TEXT("equippedItems"), Person.EquippedItems, Context, Diagnostics, true);
			const bool bHasProgressionFields = (*PersonObject)->HasField(TEXT("pendingDoctrineChoices"))
				|| (*PersonObject)->HasField(TEXT("doctrineSelections")) || (*PersonObject)->HasField(TEXT("commendations"));
			if (FormatVersion >= 24 || bHasProgressionFields)
			{
				bPersonValid &= ReadInt32(*PersonObject, TEXT("pendingDoctrineChoices"), Person.PendingDoctrineChoices, Context, Diagnostics);
				bPersonValid &= ReadNameArray(*PersonObject, TEXT("doctrineSelections"), Person.DoctrineSelections, Context, Diagnostics, true);
				bPersonValid &= ReadNameArray(*PersonObject, TEXT("commendations"), Person.Commendations, Context, Diagnostics, true);
			}
			if (bPersonValid)
			{
				OutPersonnel.Add(MoveTemp(Person));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadPersonnelSquadBonds(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FPersonnelSquadBondState>& OutBonds,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("personnelSquadBonds")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"),
				TEXT("save.state is missing required array 'personnelSquadBonds'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("personnelSquadBonds"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"),
				TEXT("save.state.personnelSquadBonds must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* BondObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.personnelSquadBonds[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(BondObject)
				|| BondObject == nullptr || !BondObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"),
					Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*BondObject,
				{ TEXT("firstPersonnelId"), TEXT("secondPersonnelId"), TEXT("sharedVictories") },
				Context, Diagnostics);

			FPersonnelSquadBondState Bond;
			bool bBondValid = ReadGuidString(*BondObject, TEXT("firstPersonnelId"),
				Bond.FirstPersonnelId, Context, Diagnostics);
			bBondValid &= ReadGuidString(*BondObject, TEXT("secondPersonnelId"),
				Bond.SecondPersonnelId, Context, Diagnostics);
			bBondValid &= ReadInt32(*BondObject, TEXT("sharedVictories"),
				Bond.SharedVictories, Context, Diagnostics);
			if (bBondValid)
			{
				OutBonds.Add(MoveTemp(Bond));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadRecruitmentOrders(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FRecruitmentOrderState>& OutOrders,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("recruitmentOrders")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'recruitmentOrders'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("recruitmentOrders"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.recruitmentOrders must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* OrderObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.recruitmentOrders[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(OrderObject) || OrderObject == nullptr || !OrderObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*OrderObject, { TEXT("orderId"), TEXT("personnelId"), TEXT("displayName"), TEXT("roleId"), TEXT("baseId"), TEXT("remainingTransitSeconds") }, Context, Diagnostics);

			FRecruitmentOrderState Order;
			FString RoleId;
			bool bOrderValid = ReadGuidString(*OrderObject, TEXT("orderId"), Order.OrderId, Context, Diagnostics);
			bOrderValid &= ReadGuidString(*OrderObject, TEXT("personnelId"), Order.PersonnelId, Context, Diagnostics);
			bOrderValid &= ReadRequiredString(*OrderObject, TEXT("displayName"), Order.DisplayName, Context, Diagnostics);
			bOrderValid &= ReadRequiredString(*OrderObject, TEXT("roleId"), RoleId, Context, Diagnostics);
			Order.RoleId = FName(*RoleId);
			bOrderValid &= ReadGuidString(*OrderObject, TEXT("baseId"), Order.BaseId, Context, Diagnostics);
			bOrderValid &= ReadInt64String(*OrderObject, TEXT("remainingTransitSeconds"), Order.RemainingTransitSeconds, Context, Diagnostics);
			if (bOrderValid)
			{
				OutOrders.Add(MoveTemp(Order));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadMemorial(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FMemorialRecord>& OutMemorial,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("memorial")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'memorial'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("memorial"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.memorial must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* RecordObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.memorial[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(RecordObject) || RecordObject == nullptr || !RecordObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			TSet<FString> AllowedFields = { TEXT("personnelId"), TEXT("displayName"), TEXT("roleId"), TEXT("rank"), TEXT("missions"), TEXT("kills"), TEXT("deathUtc"), TEXT("causeId"), TEXT("doctrineSelections"), TEXT("commendations") };
			if (FormatVersion >= 35)
			{
				AllowedFields.Add(TEXT("stewardshipToursCompleted"));
			}
			WarnUnknownFields(*RecordObject, AllowedFields, Context, Diagnostics);

			FMemorialRecord Record;
			FString RoleId;
			FString CauseId;
			bool bRecordValid = ReadGuidString(*RecordObject, TEXT("personnelId"), Record.PersonnelId, Context, Diagnostics);
			bRecordValid &= ReadRequiredString(*RecordObject, TEXT("displayName"), Record.DisplayName, Context, Diagnostics);
			bRecordValid &= ReadRequiredString(*RecordObject, TEXT("roleId"), RoleId, Context, Diagnostics);
			Record.RoleId = FName(*RoleId);
			bRecordValid &= ReadInt32(*RecordObject, TEXT("rank"), Record.Rank, Context, Diagnostics);
			bRecordValid &= ReadInt32(*RecordObject, TEXT("missions"), Record.Missions, Context, Diagnostics);
			bRecordValid &= ReadInt32(*RecordObject, TEXT("kills"), Record.Kills, Context, Diagnostics);
			bRecordValid &= ReadIsoDate(*RecordObject, TEXT("deathUtc"), Record.DeathUtc, Context, Diagnostics);
			bRecordValid &= ReadRequiredString(*RecordObject, TEXT("causeId"), CauseId, Context, Diagnostics);
			Record.CauseId = FName(*CauseId);
			if (FormatVersion >= 35)
			{
				bRecordValid &= ReadInt32(*RecordObject, TEXT("stewardshipToursCompleted"),
					Record.StewardshipToursCompleted, Context, Diagnostics);
			}
			const bool bHasProgressionFields = (*RecordObject)->HasField(TEXT("doctrineSelections"))
				|| (*RecordObject)->HasField(TEXT("commendations"));
			if (FormatVersion >= 24 || bHasProgressionFields)
			{
				bRecordValid &= ReadNameArray(*RecordObject, TEXT("doctrineSelections"), Record.DoctrineSelections, Context, Diagnostics, true);
				bRecordValid &= ReadNameArray(*RecordObject, TEXT("commendations"), Record.Commendations, Context, Diagnostics, true);
			}
			if (bRecordValid)
			{
				OutMemorial.Add(MoveTemp(Record));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadCraftWeaponStates(
		const TSharedPtr<FJsonObject>& CraftObject,
		TArray<FCraftWeaponState>& OutWeaponStates,
		const FString& CraftContext,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!CraftObject->HasField(TEXT("weaponStates")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), CraftContext + TEXT(" is missing required array 'weaponStates'."));
			return false;
		}
		if (!CraftObject->TryGetArrayField(TEXT("weaponStates"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), CraftContext + TEXT(".weaponStates must be an array."));
			return false;
		}
		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* WeaponObject = nullptr;
			const FString Context = FString::Printf(TEXT("%s.weaponStates[%d]"), *CraftContext, Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(WeaponObject) || WeaponObject == nullptr || !WeaponObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*WeaponObject, { TEXT("weaponItemId"), TEXT("ammunition"), TEXT("remainingCooldownSeconds") }, Context, Diagnostics);
			FCraftWeaponState WeaponState;
			FString WeaponItemId;
			bool bWeaponValid = ReadRequiredString(*WeaponObject, TEXT("weaponItemId"), WeaponItemId, Context, Diagnostics);
			WeaponState.WeaponItemId = FName(*WeaponItemId);
			bWeaponValid &= ReadInt32(*WeaponObject, TEXT("ammunition"), WeaponState.Ammunition, Context, Diagnostics);
			bWeaponValid &= ReadInt64String(*WeaponObject, TEXT("remainingCooldownSeconds"), WeaponState.RemainingCooldownSeconds, Context, Diagnostics);
			if (bWeaponValid)
			{
				OutWeaponStates.Add(MoveTemp(WeaponState));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadCraft(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FCraftState>& OutCraft,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("craft")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'craft'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("craft"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.craft must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* CraftObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.craft[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(CraftObject) || CraftObject == nullptr || !CraftObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			TSet<FString> AllowedFields = {
				TEXT("craftId"), TEXT("displayName"), TEXT("craftRuleId"), TEXT("baseId"), TEXT("assignedPilotId"), TEXT("status"),
				TEXT("currentHull"), TEXT("currentFuel"), TEXT("remainingRepairSeconds"), TEXT("remainingRefuelSeconds"),
				TEXT("completedSorties"), TEXT("equipmentItems") };
			if (FormatVersion >= 8)
			{
				AllowedFields.Add(TEXT("targetContactId"));
				AllowedFields.Add(TEXT("remainingRouteSeconds"));
				AllowedFields.Add(TEXT("reservedReturnSeconds"));
			}
			if (FormatVersion >= 9)
			{
				AllowedFields.Add(TEXT("weaponStates"));
			}
			if (FormatVersion >= 11)
			{
				AllowedFields.Add(TEXT("assignedAgentIds"));
				AllowedFields.Add(TEXT("cargo"));
				AllowedFields.Add(TEXT("targetSiteId"));
			}
			if (FormatVersion >= 26)
			{
				AllowedFields.Add(TEXT("pendingSalvage"));
			}
			WarnUnknownFields(*CraftObject, AllowedFields, Context, Diagnostics);

			FCraftState Craft;
			FString CraftRuleId;
			FString Status;
			bool bCraftValid = ReadGuidString(*CraftObject, TEXT("craftId"), Craft.CraftId, Context, Diagnostics);
			bCraftValid &= ReadRequiredString(*CraftObject, TEXT("displayName"), Craft.DisplayName, Context, Diagnostics);
			bCraftValid &= ReadRequiredString(*CraftObject, TEXT("craftRuleId"), CraftRuleId, Context, Diagnostics);
			Craft.CraftRuleId = FName(*CraftRuleId);
			bCraftValid &= ReadGuidString(*CraftObject, TEXT("baseId"), Craft.BaseId, Context, Diagnostics);
			bCraftValid &= ReadGuidString(*CraftObject, TEXT("assignedPilotId"), Craft.AssignedPilotId, Context, Diagnostics);
			bCraftValid &= ReadRequiredString(*CraftObject, TEXT("status"), Status, Context, Diagnostics);
			if (!Status.IsEmpty() && !TryParseCraftStatus(Status, Craft.Status))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), Context + TEXT(".status is unknown."));
				bCraftValid = false;
			}
			bCraftValid &= ReadInt32(*CraftObject, TEXT("currentHull"), Craft.CurrentHull, Context, Diagnostics);
			bCraftValid &= ReadInt32(*CraftObject, TEXT("currentFuel"), Craft.CurrentFuel, Context, Diagnostics);
			bCraftValid &= ReadInt64String(*CraftObject, TEXT("remainingRepairSeconds"), Craft.RemainingRepairSeconds, Context, Diagnostics);
			bCraftValid &= ReadInt64String(*CraftObject, TEXT("remainingRefuelSeconds"), Craft.RemainingRefuelSeconds, Context, Diagnostics);
			bCraftValid &= ReadInt32(*CraftObject, TEXT("completedSorties"), Craft.CompletedSorties, Context, Diagnostics);
			bCraftValid &= ReadNameArray(*CraftObject, TEXT("equipmentItems"), Craft.EquipmentItems, Context, Diagnostics, true);
			if (FormatVersion >= 8)
			{
				bCraftValid &= ReadGuidString(*CraftObject, TEXT("targetContactId"), Craft.TargetContactId, Context, Diagnostics);
				bCraftValid &= ReadInt64String(*CraftObject, TEXT("remainingRouteSeconds"), Craft.RemainingRouteSeconds, Context, Diagnostics);
				bCraftValid &= ReadInt64String(*CraftObject, TEXT("reservedReturnSeconds"), Craft.ReservedReturnSeconds, Context, Diagnostics);
			}
			if (FormatVersion >= 9)
			{
				bCraftValid &= ReadCraftWeaponStates(*CraftObject, Craft.WeaponStates, Context, Diagnostics);
			}
			if (FormatVersion >= 11)
			{
				bCraftValid &= ReadGuidArray(*CraftObject, TEXT("assignedAgentIds"), Craft.AssignedAgentIds, Context, Diagnostics);
				bCraftValid &= ReadInventoryArray(*CraftObject, TEXT("cargo"), Craft.Cargo, Context, Diagnostics);
				bCraftValid &= ReadGuidString(*CraftObject, TEXT("targetSiteId"), Craft.TargetSiteId, Context, Diagnostics);
			}
			if (FormatVersion >= 26)
			{
				bCraftValid &= ReadInventoryArray(*CraftObject, TEXT("pendingSalvage"), Craft.PendingSalvage, Context, Diagnostics);
			}
			if (bCraftValid)
			{
				OutCraft.Add(MoveTemp(Craft));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadStrategicContacts(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FStrategicContactState>& OutContacts,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("strategicContacts")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'strategicContacts'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("strategicContacts"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.strategicContacts must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* ContactObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.strategicContacts[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(ContactObject) || ContactObject == nullptr || !ContactObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			TSet<FString> AllowedFields = {
				TEXT("contactId"), TEXT("contactRuleId"), TEXT("status"),
				TEXT("originLongitudeMilliDegrees"), TEXT("originLatitudeMilliDegrees"),
				TEXT("longitudeMilliDegrees"), TEXT("latitudeMilliDegrees"),
				TEXT("destinationLongitudeMilliDegrees"), TEXT("destinationLatitudeMilliDegrees"),
				TEXT("totalRouteSeconds"), TEXT("elapsedRouteSeconds"), TEXT("currentHull") };
			if (FormatVersion >= 9)
			{
				AllowedFields.Add(TEXT("completedCombatRounds"));
				AllowedFields.Add(TEXT("remainingAttackCooldownSeconds"));
			}
			WarnUnknownFields(*ContactObject, AllowedFields, Context, Diagnostics);

			FStrategicContactState Contact;
			FString ContactRuleId;
			FString Status;
			bool bContactValid = ReadGuidString(*ContactObject, TEXT("contactId"), Contact.ContactId, Context, Diagnostics);
			bContactValid &= ReadRequiredString(*ContactObject, TEXT("contactRuleId"), ContactRuleId, Context, Diagnostics);
			Contact.ContactRuleId = FName(*ContactRuleId);
			bContactValid &= ReadRequiredString(*ContactObject, TEXT("status"), Status, Context, Diagnostics);
			if (!Status.IsEmpty() && !TryParseContactStatus(Status, Contact.Status))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), Context + TEXT(".status is unknown."));
				bContactValid = false;
			}
			bContactValid &= ReadInt32(*ContactObject, TEXT("originLongitudeMilliDegrees"), Contact.OriginLongitudeMilliDegrees, Context, Diagnostics);
			bContactValid &= ReadInt32(*ContactObject, TEXT("originLatitudeMilliDegrees"), Contact.OriginLatitudeMilliDegrees, Context, Diagnostics);
			bContactValid &= ReadInt32(*ContactObject, TEXT("longitudeMilliDegrees"), Contact.LongitudeMilliDegrees, Context, Diagnostics);
			bContactValid &= ReadInt32(*ContactObject, TEXT("latitudeMilliDegrees"), Contact.LatitudeMilliDegrees, Context, Diagnostics);
			bContactValid &= ReadInt32(*ContactObject, TEXT("destinationLongitudeMilliDegrees"), Contact.DestinationLongitudeMilliDegrees, Context, Diagnostics);
			bContactValid &= ReadInt32(*ContactObject, TEXT("destinationLatitudeMilliDegrees"), Contact.DestinationLatitudeMilliDegrees, Context, Diagnostics);
			bContactValid &= ReadInt64String(*ContactObject, TEXT("totalRouteSeconds"), Contact.TotalRouteSeconds, Context, Diagnostics);
			bContactValid &= ReadInt64String(*ContactObject, TEXT("elapsedRouteSeconds"), Contact.ElapsedRouteSeconds, Context, Diagnostics);
			bContactValid &= ReadInt32(*ContactObject, TEXT("currentHull"), Contact.CurrentHull, Context, Diagnostics);
			if (FormatVersion >= 9)
			{
				bContactValid &= ReadInt32(*ContactObject, TEXT("completedCombatRounds"), Contact.CompletedCombatRounds, Context, Diagnostics);
				bContactValid &= ReadInt64String(*ContactObject, TEXT("remainingAttackCooldownSeconds"), Contact.RemainingAttackCooldownSeconds, Context, Diagnostics);
			}
			if (bContactValid)
			{
				OutContacts.Add(MoveTemp(Contact));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadStrategicSites(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FStrategicSiteState>& OutSites,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("strategicSites")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'strategicSites'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("strategicSites"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.strategicSites must be an array."));
			return false;
		}
		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* SiteObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.strategicSites[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(SiteObject) || SiteObject == nullptr || !SiteObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*SiteObject, { TEXT("siteId"), TEXT("type"), TEXT("sourceContactRuleId"), TEXT("longitudeMilliDegrees"), TEXT("latitudeMilliDegrees"), TEXT("threatRating"), TEXT("remainingLifetimeSeconds") }, Context, Diagnostics);
			FStrategicSiteState Site;
			FString Type;
			FString SourceContactRuleId;
			bool bSiteValid = ReadGuidString(*SiteObject, TEXT("siteId"), Site.SiteId, Context, Diagnostics);
			bSiteValid &= ReadRequiredString(*SiteObject, TEXT("type"), Type, Context, Diagnostics);
			if (!Type.IsEmpty() && !TryParseSiteType(Type, Site.Type))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), Context + TEXT(".type is unknown."));
				bSiteValid = false;
			}
			bSiteValid &= ReadRequiredString(*SiteObject, TEXT("sourceContactRuleId"), SourceContactRuleId, Context, Diagnostics);
			Site.SourceContactRuleId = FName(*SourceContactRuleId);
			bSiteValid &= ReadInt32(*SiteObject, TEXT("longitudeMilliDegrees"), Site.LongitudeMilliDegrees, Context, Diagnostics);
			bSiteValid &= ReadInt32(*SiteObject, TEXT("latitudeMilliDegrees"), Site.LatitudeMilliDegrees, Context, Diagnostics);
			bSiteValid &= ReadInt32(*SiteObject, TEXT("threatRating"), Site.ThreatRating, Context, Diagnostics);
			bSiteValid &= ReadInt64String(*SiteObject, TEXT("remainingLifetimeSeconds"), Site.RemainingLifetimeSeconds, Context, Diagnostics);
			if (bSiteValid)
			{
				OutSites.Add(MoveTemp(Site));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadTacticalOperations(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FTacticalOperationState>& OutOperations,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("tacticalOperations")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'tacticalOperations'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("tacticalOperations"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.tacticalOperations must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* OperationObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.tacticalOperations[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(OperationObject) || OperationObject == nullptr || !OperationObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			TSet<FString> AllowedFields = { TEXT("operationId"), TEXT("siteId"), TEXT("craftId"), TEXT("tacticalSeed"), TEXT("createdUtc"), TEXT("agentIds"), TEXT("cargo") };
			if (FormatVersion >= 21)
			{
				AllowedFields.Add(TEXT("type"));
				AllowedFields.Add(TEXT("baseId"));
				AllowedFields.Add(TEXT("assaultId"));
			}
			WarnUnknownFields(*OperationObject, AllowedFields, Context, Diagnostics);
			FTacticalOperationState Operation;
			bool bOperationValid = ReadGuidString(*OperationObject, TEXT("operationId"), Operation.OperationId, Context, Diagnostics);
			if (FormatVersion >= 21)
			{
				FString OperationType;
				bOperationValid &= ReadRequiredString(*OperationObject, TEXT("type"), OperationType, Context, Diagnostics);
				if (!OperationType.IsEmpty() && !TryParseTacticalOperationType(OperationType, Operation.Type))
				{
					AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), Context + TEXT(".type is unknown."));
					bOperationValid = false;
				}
				bOperationValid &= ReadGuidString(*OperationObject, TEXT("baseId"), Operation.BaseId, Context, Diagnostics);
				bOperationValid &= ReadGuidString(*OperationObject, TEXT("assaultId"), Operation.AssaultId, Context, Diagnostics);
			}
			bOperationValid &= ReadGuidString(*OperationObject, TEXT("siteId"), Operation.SiteId, Context, Diagnostics);
			bOperationValid &= ReadGuidString(*OperationObject, TEXT("craftId"), Operation.CraftId, Context, Diagnostics);
			bOperationValid &= ReadInt64String(*OperationObject, TEXT("tacticalSeed"), Operation.TacticalSeed, Context, Diagnostics);
			bOperationValid &= ReadIsoDate(*OperationObject, TEXT("createdUtc"), Operation.CreatedUtc, Context, Diagnostics);
			bOperationValid &= ReadGuidArray(*OperationObject, TEXT("agentIds"), Operation.AgentIds, Context, Diagnostics);
			bOperationValid &= ReadInventoryArray(*OperationObject, TEXT("cargo"), Operation.Cargo, Context, Diagnostics);
			if (bOperationValid)
			{
				OutOperations.Add(MoveTemp(Operation));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadTacticalCells(
		const TSharedPtr<FJsonObject>& BattleObject,
		TArray<FTacticalCellState>& OutCells,
		const FString& BattleContext,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!BattleObject->HasField(TEXT("cells")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required array 'cells'."), *BattleContext));
			return false;
		}
		if (!BattleObject->TryGetArrayField(TEXT("cells"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.cells must be an array."), *BattleContext));
			return false;
		}
		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* CellObject = nullptr;
			const FString Context = FString::Printf(TEXT("%s.cells[%d]"), *BattleContext, Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(CellObject) || CellObject == nullptr || !CellObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			TSet<FString> AllowedFields = { TEXT("x"), TEXT("y"), TEXT("terrainRuleId"), TEXT("currentIntegrity"), TEXT("playerDeployment"), TEXT("extraction") };
			if (FormatVersion >= 17)
			{
				AllowedFields.Add(TEXT("z"));
			}
			if (FormatVersion >= 13)
			{
				AllowedFields.Add(TEXT("smoke"));
				AllowedFields.Add(TEXT("fire"));
			}
			if (FormatVersion >= 15)
			{
				AllowedFields.Add(TEXT("doorOpen"));
			}
			WarnUnknownFields(*CellObject, AllowedFields, Context, Diagnostics);
			FTacticalCellState Cell;
			FString TerrainRuleId;
			bool bCellValid = ReadInt32(*CellObject, TEXT("x"), Cell.X, Context, Diagnostics);
			bCellValid &= ReadInt32(*CellObject, TEXT("y"), Cell.Y, Context, Diagnostics);
			if (FormatVersion >= 17)
			{
				bCellValid &= ReadInt32(*CellObject, TEXT("z"), Cell.Z, Context, Diagnostics);
			}
			bCellValid &= ReadRequiredString(*CellObject, TEXT("terrainRuleId"), TerrainRuleId, Context, Diagnostics);
			Cell.TerrainRuleId = FName(*TerrainRuleId);
			bCellValid &= ReadInt32(*CellObject, TEXT("currentIntegrity"), Cell.CurrentIntegrity, Context, Diagnostics);
			bCellValid &= ReadBool(*CellObject, TEXT("playerDeployment"), Cell.bPlayerDeployment, Context, Diagnostics);
			bCellValid &= ReadBool(*CellObject, TEXT("extraction"), Cell.bExtraction, Context, Diagnostics);
			if (FormatVersion >= 15)
			{
				bCellValid &= ReadBool(*CellObject, TEXT("doorOpen"), Cell.bDoorOpen, Context, Diagnostics);
			}
			if (FormatVersion >= 13)
			{
				bCellValid &= ReadInt32(*CellObject, TEXT("smoke"), Cell.Smoke, Context, Diagnostics);
				bCellValid &= ReadInt32(*CellObject, TEXT("fire"), Cell.Fire, Context, Diagnostics);
			}
			if (bCellValid)
			{
				OutCells.Add(MoveTemp(Cell));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadTacticalWeaponStates(
		const TSharedPtr<FJsonObject>& UnitObject,
		TArray<FTacticalWeaponState>& OutWeaponStates,
		const FString& UnitContext,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!UnitObject->HasField(TEXT("weaponStates")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required array 'weaponStates'."), *UnitContext));
			return false;
		}
		if (!UnitObject->TryGetArrayField(TEXT("weaponStates"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.weaponStates must be an array."), *UnitContext));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* WeaponObject = nullptr;
			const FString Context = FString::Printf(TEXT("%s.weaponStates[%d]"), *UnitContext, Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(WeaponObject) || WeaponObject == nullptr || !WeaponObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*WeaponObject, { TEXT("weaponItemId"), TEXT("loadedAmmunition") }, Context, Diagnostics);
			FTacticalWeaponState WeaponState;
			FString WeaponItemId;
			bool bWeaponValid = ReadRequiredString(*WeaponObject, TEXT("weaponItemId"), WeaponItemId, Context, Diagnostics);
			WeaponState.WeaponItemId = FName(*WeaponItemId);
			bWeaponValid &= ReadInt32(*WeaponObject, TEXT("loadedAmmunition"), WeaponState.LoadedAmmunition, Context, Diagnostics);
			if (bWeaponValid)
			{
				OutWeaponStates.Add(MoveTemp(WeaponState));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadTacticalMagazineStates(
		const TSharedPtr<FJsonObject>& UnitObject,
		TArray<FTacticalMagazineState>& OutMagazines,
		const FString& UnitContext,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!UnitObject->HasField(TEXT("ejectedMagazines")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"),
				FString::Printf(TEXT("%s is missing required array 'ejectedMagazines'."), *UnitContext));
			return false;
		}
		if (!UnitObject->TryGetArrayField(TEXT("ejectedMagazines"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"),
				FString::Printf(TEXT("%s.ejectedMagazines must be an array."), *UnitContext));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* MagazineObject = nullptr;
			const FString Context = FString::Printf(TEXT("%s.ejectedMagazines[%d]"), *UnitContext, Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(MagazineObject)
				|| MagazineObject == nullptr || !MagazineObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"),
					Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*MagazineObject,
				{ TEXT("weaponItemId"), TEXT("ammunitionItemId"), TEXT("loadedAmmunition") },
				Context, Diagnostics);
			FTacticalMagazineState Magazine;
			FString WeaponItemId;
			FString AmmunitionItemId;
			bool bMagazineValid = ReadRequiredString(*MagazineObject, TEXT("weaponItemId"), WeaponItemId, Context, Diagnostics);
			bMagazineValid &= ReadRequiredString(*MagazineObject, TEXT("ammunitionItemId"), AmmunitionItemId, Context, Diagnostics);
			Magazine.WeaponItemId = FName(*WeaponItemId);
			Magazine.AmmunitionItemId = FName(*AmmunitionItemId);
			bMagazineValid &= ReadInt32(*MagazineObject, TEXT("loadedAmmunition"), Magazine.LoadedAmmunition, Context, Diagnostics);
			if (bMagazineValid)
			{
				OutMagazines.Add(MoveTemp(Magazine));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadTacticalUnits(
		const TSharedPtr<FJsonObject>& BattleObject,
		TArray<FTacticalUnitState>& OutUnits,
		const FString& BattleContext,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!BattleObject->HasField(TEXT("units")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required array 'units'."), *BattleContext));
			return false;
		}
		if (!BattleObject->TryGetArrayField(TEXT("units"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.units must be an array."), *BattleContext));
			return false;
		}
		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* UnitObject = nullptr;
			const FString Context = FString::Printf(TEXT("%s.units[%d]"), *BattleContext, Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(UnitObject) || UnitObject == nullptr || !UnitObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			TSet<FString> AllowedFields = {
				TEXT("unitId"), TEXT("personnelId"), TEXT("sourceRuleId"), TEXT("displayName"), TEXT("team"),
				TEXT("x"), TEXT("y"), TEXT("maxHealth"), TEXT("currentHealth"), TEXT("accuracy"), TEXT("resolve"),
				TEXT("mobility"), TEXT("strength"), TEXT("maxActionPoints"), TEXT("remainingActionPoints"), TEXT("extracted") };
			if (FormatVersion >= 17)
			{
				AllowedFields.Add(TEXT("z"));
			}
			if (FormatVersion >= 13)
			{
				AllowedFields.Add(TEXT("kineticArmor"));
				AllowedFields.Add(TEXT("thermalArmor"));
				AllowedFields.Add(TEXT("arcArmor"));
				AllowedFields.Add(TEXT("maxMorale"));
				AllowedFields.Add(TEXT("currentMorale"));
				AllowedFields.Add(TEXT("suppression"));
				AllowedFields.Add(TEXT("weaponStates"));
				AllowedFields.Add(TEXT("carriedItems"));
			}
			if (FormatVersion >= 27)
			{
				AllowedFields.Add(TEXT("ejectedMagazines"));
			}
			if (FormatVersion >= 14)
			{
				AllowedFields.Add(TEXT("stance"));
			}
			WarnUnknownFields(*UnitObject, AllowedFields, Context, Diagnostics);
			FTacticalUnitState Unit;
			FString SourceRuleId;
			FString Team;
			FString Stance;
			bool bUnitValid = ReadGuidString(*UnitObject, TEXT("unitId"), Unit.UnitId, Context, Diagnostics);
			bUnitValid &= ReadGuidString(*UnitObject, TEXT("personnelId"), Unit.PersonnelId, Context, Diagnostics);
			bUnitValid &= ReadRequiredString(*UnitObject, TEXT("sourceRuleId"), SourceRuleId, Context, Diagnostics);
			Unit.SourceRuleId = FName(*SourceRuleId);
			bUnitValid &= ReadRequiredString(*UnitObject, TEXT("displayName"), Unit.DisplayName, Context, Diagnostics);
			bUnitValid &= ReadRequiredString(*UnitObject, TEXT("team"), Team, Context, Diagnostics);
			if (!Team.IsEmpty() && !TryParseTacticalTeam(Team, Unit.Team))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), Context + TEXT(".team is unknown."));
				bUnitValid = false;
			}
			if (FormatVersion >= 14)
			{
				bUnitValid &= ReadRequiredString(*UnitObject, TEXT("stance"), Stance, Context, Diagnostics);
				if (!Stance.IsEmpty() && !TryParseTacticalStance(Stance, Unit.Stance))
				{
					AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), Context + TEXT(".stance is unknown."));
					bUnitValid = false;
				}
			}
			bUnitValid &= ReadInt32(*UnitObject, TEXT("x"), Unit.X, Context, Diagnostics);
			bUnitValid &= ReadInt32(*UnitObject, TEXT("y"), Unit.Y, Context, Diagnostics);
			if (FormatVersion >= 17)
			{
				bUnitValid &= ReadInt32(*UnitObject, TEXT("z"), Unit.Z, Context, Diagnostics);
			}
			bUnitValid &= ReadInt32(*UnitObject, TEXT("maxHealth"), Unit.MaxHealth, Context, Diagnostics);
			bUnitValid &= ReadInt32(*UnitObject, TEXT("currentHealth"), Unit.CurrentHealth, Context, Diagnostics);
			bUnitValid &= ReadInt32(*UnitObject, TEXT("accuracy"), Unit.Accuracy, Context, Diagnostics);
			bUnitValid &= ReadInt32(*UnitObject, TEXT("resolve"), Unit.Resolve, Context, Diagnostics);
			bUnitValid &= ReadInt32(*UnitObject, TEXT("mobility"), Unit.Mobility, Context, Diagnostics);
			bUnitValid &= ReadInt32(*UnitObject, TEXT("strength"), Unit.Strength, Context, Diagnostics);
			bUnitValid &= ReadInt32(*UnitObject, TEXT("maxActionPoints"), Unit.MaxActionPoints, Context, Diagnostics);
			bUnitValid &= ReadInt32(*UnitObject, TEXT("remainingActionPoints"), Unit.RemainingActionPoints, Context, Diagnostics);
			bUnitValid &= ReadBool(*UnitObject, TEXT("extracted"), Unit.bExtracted, Context, Diagnostics);
			if (FormatVersion >= 13)
			{
				bUnitValid &= ReadInt32(*UnitObject, TEXT("kineticArmor"), Unit.KineticArmor, Context, Diagnostics);
				bUnitValid &= ReadInt32(*UnitObject, TEXT("thermalArmor"), Unit.ThermalArmor, Context, Diagnostics);
				bUnitValid &= ReadInt32(*UnitObject, TEXT("arcArmor"), Unit.ArcArmor, Context, Diagnostics);
				bUnitValid &= ReadInt32(*UnitObject, TEXT("maxMorale"), Unit.MaxMorale, Context, Diagnostics);
				bUnitValid &= ReadInt32(*UnitObject, TEXT("currentMorale"), Unit.CurrentMorale, Context, Diagnostics);
				bUnitValid &= ReadInt32(*UnitObject, TEXT("suppression"), Unit.Suppression, Context, Diagnostics);
				bUnitValid &= ReadTacticalWeaponStates(*UnitObject, Unit.WeaponStates, Context, Diagnostics);
				bUnitValid &= ReadInventoryArray(*UnitObject, TEXT("carriedItems"), Unit.CarriedItems, Context, Diagnostics);
			}
			if (FormatVersion >= 27)
			{
				bUnitValid &= ReadTacticalMagazineStates(*UnitObject, Unit.EjectedMagazines, Context, Diagnostics);
			}
			if (bUnitValid)
			{
				OutUnits.Add(MoveTemp(Unit));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadTacticalObjectives(
		const TSharedPtr<FJsonObject>& BattleObject,
		TArray<FTacticalObjectiveState>& OutObjectives,
		const FString& BattleContext,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!BattleObject->HasField(TEXT("objectives")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), FString::Printf(TEXT("%s is missing required array 'objectives'."), *BattleContext));
			return false;
		}
		if (!BattleObject->TryGetArrayField(TEXT("objectives"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), FString::Printf(TEXT("%s.objectives must be an array."), *BattleContext));
			return false;
		}
		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* ObjectiveObject = nullptr;
			const FString Context = FString::Printf(TEXT("%s.objectives[%d]"), *BattleContext, Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(ObjectiveObject) || ObjectiveObject == nullptr || !ObjectiveObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			TSet<FString> AllowedFields = { TEXT("objectiveId"), TEXT("x"), TEXT("y"), TEXT("status"), TEXT("requiredInteractions"), TEXT("completedInteractions") };
			if (FormatVersion >= 17)
			{
				AllowedFields.Add(TEXT("z"));
			}
			if (FormatVersion >= 18)
			{
				AllowedFields.Add(TEXT("type"));
				AllowedFields.Add(TEXT("adversaryInteractions"));
			}
			WarnUnknownFields(*ObjectiveObject, AllowedFields, Context, Diagnostics);
			FTacticalObjectiveState Objective;
			FString ObjectiveId;
			FString Status;
			FString Type;
			bool bObjectiveValid = ReadRequiredString(*ObjectiveObject, TEXT("objectiveId"), ObjectiveId, Context, Diagnostics);
			Objective.ObjectiveId = FName(*ObjectiveId);
			bObjectiveValid &= ReadInt32(*ObjectiveObject, TEXT("x"), Objective.X, Context, Diagnostics);
			bObjectiveValid &= ReadInt32(*ObjectiveObject, TEXT("y"), Objective.Y, Context, Diagnostics);
			if (FormatVersion >= 17)
			{
				bObjectiveValid &= ReadInt32(*ObjectiveObject, TEXT("z"), Objective.Z, Context, Diagnostics);
			}
			bObjectiveValid &= ReadRequiredString(*ObjectiveObject, TEXT("status"), Status, Context, Diagnostics);
			if (!Status.IsEmpty() && !TryParseTacticalObjectiveStatus(Status, Objective.Status))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), Context + TEXT(".status is unknown."));
				bObjectiveValid = false;
			}
			if (FormatVersion >= 18)
			{
				bObjectiveValid &= ReadRequiredString(*ObjectiveObject, TEXT("type"), Type, Context, Diagnostics);
				if (!Type.IsEmpty() && !TryParseTacticalObjectiveType(Type, Objective.Type))
				{
					AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), Context + TEXT(".type is unknown."));
					bObjectiveValid = false;
				}
			}
			bObjectiveValid &= ReadInt32(*ObjectiveObject, TEXT("requiredInteractions"), Objective.RequiredInteractions, Context, Diagnostics);
			bObjectiveValid &= ReadInt32(*ObjectiveObject, TEXT("completedInteractions"), Objective.CompletedInteractions, Context, Diagnostics);
			if (FormatVersion >= 18)
			{
				bObjectiveValid &= ReadInt32(*ObjectiveObject, TEXT("adversaryInteractions"), Objective.AdversaryInteractions, Context, Diagnostics);
			}
			if (bObjectiveValid)
			{
				OutObjectives.Add(MoveTemp(Objective));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadTacticalBattles(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FTacticalBattleState>& OutBattles,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("tacticalBattles")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'tacticalBattles'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("tacticalBattles"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.tacticalBattles must be an array."));
			return false;
		}
		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* BattleObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.tacticalBattles[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(BattleObject) || BattleObject == nullptr || !BattleObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			TSet<FString> AllowedBattleFields = {
				TEXT("battleId"), TEXT("operationId"), TEXT("siteId"), TEXT("missionRuleId"), TEXT("createdUtc"),
				TEXT("width"), TEXT("height"), TEXT("turnLimit"), TEXT("turnNumber"), TEXT("phase"), TEXT("activeTeam"),
				TEXT("randomInitialSeed"), TEXT("randomDrawCount"), TEXT("randomState"), TEXT("cells"), TEXT("units"), TEXT("objectives"), TEXT("cargo") };
			if (FormatVersion >= 16)
			{
				AllowedBattleFields.Add(TEXT("windDirection"));
				AllowedBattleFields.Add(TEXT("windStrength"));
			}
			if (FormatVersion >= 17)
			{
				AllowedBattleFields.Add(TEXT("levels"));
			}
			if (FormatVersion >= 21)
			{
				AllowedBattleFields.Add(TEXT("requiresExtraction"));
			}
			if (FormatVersion >= 25)
			{
				AllowedBattleFields.Add(TEXT("playerDiscoveredCellIndices"));
			}
			WarnUnknownFields(*BattleObject, AllowedBattleFields, Context, Diagnostics);
			FTacticalBattleState Battle;
			FString MissionRuleId;
			FString Phase;
			FString ActiveTeam;
			FString WindDirection;
			int64 RandomInitialSeed = 0;
			int64 RandomDrawCount = 0;
			uint64 RandomState = 0;
			bool bBattleValid = ReadGuidString(*BattleObject, TEXT("battleId"), Battle.BattleId, Context, Diagnostics);
			bBattleValid &= ReadGuidString(*BattleObject, TEXT("operationId"), Battle.OperationId, Context, Diagnostics);
			bBattleValid &= ReadGuidString(*BattleObject, TEXT("siteId"), Battle.SiteId, Context, Diagnostics);
			bBattleValid &= ReadRequiredString(*BattleObject, TEXT("missionRuleId"), MissionRuleId, Context, Diagnostics);
			Battle.MissionRuleId = FName(*MissionRuleId);
			bBattleValid &= ReadIsoDate(*BattleObject, TEXT("createdUtc"), Battle.CreatedUtc, Context, Diagnostics);
			bBattleValid &= ReadInt32(*BattleObject, TEXT("width"), Battle.Width, Context, Diagnostics);
			bBattleValid &= ReadInt32(*BattleObject, TEXT("height"), Battle.Height, Context, Diagnostics);
			if (FormatVersion >= 17)
			{
				bBattleValid &= ReadInt32(*BattleObject, TEXT("levels"), Battle.Levels, Context, Diagnostics);
			}
			bBattleValid &= ReadInt32(*BattleObject, TEXT("turnLimit"), Battle.TurnLimit, Context, Diagnostics);
			bBattleValid &= ReadInt32(*BattleObject, TEXT("turnNumber"), Battle.TurnNumber, Context, Diagnostics);
			if (FormatVersion >= 21)
			{
				bBattleValid &= ReadBool(*BattleObject, TEXT("requiresExtraction"), Battle.bRequiresExtraction, Context, Diagnostics);
			}
			bBattleValid &= ReadRequiredString(*BattleObject, TEXT("phase"), Phase, Context, Diagnostics);
			if (!Phase.IsEmpty() && !TryParseTacticalPhase(Phase, Battle.Phase))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), Context + TEXT(".phase is unknown."));
				bBattleValid = false;
			}
			bBattleValid &= ReadRequiredString(*BattleObject, TEXT("activeTeam"), ActiveTeam, Context, Diagnostics);
			if (!ActiveTeam.IsEmpty() && !TryParseTacticalTeam(ActiveTeam, Battle.ActiveTeam))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), Context + TEXT(".activeTeam is unknown."));
				bBattleValid = false;
			}
			if (FormatVersion >= 16)
			{
				bBattleValid &= ReadRequiredString(*BattleObject, TEXT("windDirection"), WindDirection, Context, Diagnostics);
				if (!WindDirection.IsEmpty() && !TryParseTacticalWindDirection(WindDirection, Battle.WindDirection))
				{
					AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), Context + TEXT(".windDirection is unknown."));
					bBattleValid = false;
				}
				bBattleValid &= ReadInt32(*BattleObject, TEXT("windStrength"), Battle.WindStrength, Context, Diagnostics);
			}
			bBattleValid &= ReadInt64String(*BattleObject, TEXT("randomInitialSeed"), RandomInitialSeed, Context, Diagnostics);
			bBattleValid &= ReadInt64String(*BattleObject, TEXT("randomDrawCount"), RandomDrawCount, Context, Diagnostics);
			bBattleValid &= ReadUInt64Hex(*BattleObject, TEXT("randomState"), RandomState, Context, Diagnostics);
			bBattleValid &= ReadTacticalCells(*BattleObject, Battle.Cells, Context, FormatVersion, Diagnostics);
			if (FormatVersion >= 25)
			{
				bBattleValid &= ReadInt32Array(*BattleObject, TEXT("playerDiscoveredCellIndices"), Battle.PlayerDiscoveredCellIndices, Context, Diagnostics);
			}
			bBattleValid &= ReadTacticalUnits(*BattleObject, Battle.Units, Context, FormatVersion, Diagnostics);
			bBattleValid &= ReadTacticalObjectives(*BattleObject, Battle.Objectives, Context, FormatVersion, Diagnostics);
			bBattleValid &= ReadInventoryArray(*BattleObject, TEXT("cargo"), Battle.Cargo, Context, Diagnostics);
			if (bBattleValid && !Battle.TacticalRandom.RestoreFromSave(RandomInitialSeed, RandomDrawCount, RandomState))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_random_state"), Context + TEXT(" contains an impossible tactical random snapshot."));
				bBattleValid = false;
			}
			if (bBattleValid)
			{
				OutBattles.Add(MoveTemp(Battle));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadRegionalPressure(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FRegionalPressureState>& OutPressure,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("regionalPressure")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'regionalPressure'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("regionalPressure"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.regionalPressure must be an array."));
			return false;
		}
		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* PressureObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.regionalPressure[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(PressureObject) || PressureObject == nullptr || !PressureObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*PressureObject, { TEXT("regionId"), TEXT("pressure") }, Context, Diagnostics);
			FRegionalPressureState Pressure;
			FString RegionId;
			bool bPressureValid = ReadRequiredString(*PressureObject, TEXT("regionId"), RegionId, Context, Diagnostics);
			Pressure.RegionId = FName(*RegionId);
			bPressureValid &= ReadInt32(*PressureObject, TEXT("pressure"), Pressure.Pressure, Context, Diagnostics);
			if (bPressureValid)
			{
				OutPressure.Add(MoveTemp(Pressure));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadRegionalMandates(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FRegionalMandateState>& OutMandates,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("regionalMandates")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'regionalMandates'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("regionalMandates"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.regionalMandates must be an array."));
			return false;
		}
		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* MandateObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.regionalMandates[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(MandateObject)
				|| MandateObject == nullptr || !MandateObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*MandateObject,
				{ TEXT("regionId"), TEXT("support"), TEXT("baselineMonthlyFunding"), TEXT("currentMonthlyFunding"), TEXT("lastDiplomaticActionMonth"), TEXT("resilienceCharterSigned"), TEXT("horizonCompactMemberWithdrawn") },
				Context, Diagnostics);
			FRegionalMandateState Mandate;
			FString RegionId;
			bool bMandateValid = ReadRequiredString(*MandateObject, TEXT("regionId"), RegionId, Context, Diagnostics);
			Mandate.RegionId = FName(*RegionId);
			bMandateValid &= ReadInt32(*MandateObject, TEXT("support"), Mandate.Support, Context, Diagnostics);
			bMandateValid &= ReadInt64String(*MandateObject, TEXT("baselineMonthlyFunding"), Mandate.BaselineMonthlyFunding, Context, Diagnostics);
			bMandateValid &= ReadInt64String(*MandateObject, TEXT("currentMonthlyFunding"), Mandate.CurrentMonthlyFunding, Context, Diagnostics);
			bMandateValid &= ReadInt32(*MandateObject, TEXT("lastDiplomaticActionMonth"), Mandate.LastDiplomaticActionMonth, Context, Diagnostics);
			if (FormatVersion >= 28)
			{
				bMandateValid &= ReadBool(*MandateObject, TEXT("resilienceCharterSigned"),
					Mandate.bResilienceCharterSigned, Context, Diagnostics);
			}
			if (FormatVersion >= 31)
			{
				bMandateValid &= ReadBool(*MandateObject, TEXT("horizonCompactMemberWithdrawn"),
					Mandate.bHorizonCompactMemberWithdrawn, Context, Diagnostics);
			}
			if (bMandateValid)
			{
				OutMandates.Add(MoveTemp(Mandate));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadAdversaryMissions(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FAdversaryMissionState>& OutMissions,
		const int32 FormatVersion,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("adversaryMissions")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'adversaryMissions'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("adversaryMissions"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.adversaryMissions must be an array."));
			return false;
		}
		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* MissionObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.adversaryMissions[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(MissionObject) || MissionObject == nullptr || !MissionObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			TSet<FString> AllowedFields = { TEXT("missionId"), TEXT("contactId"), TEXT("missionRuleId"), TEXT("startedUtc") };
			if (FormatVersion >= 20)
			{
				AllowedFields.Add(TEXT("targetBaseId"));
			}
			WarnUnknownFields(*MissionObject, AllowedFields, Context, Diagnostics);
			FAdversaryMissionState Mission;
			FString MissionRuleId;
			bool bMissionValid = ReadGuidString(*MissionObject, TEXT("missionId"), Mission.MissionId, Context, Diagnostics);
			bMissionValid &= ReadGuidString(*MissionObject, TEXT("contactId"), Mission.ContactId, Context, Diagnostics);
			bMissionValid &= ReadRequiredString(*MissionObject, TEXT("missionRuleId"), MissionRuleId, Context, Diagnostics);
			Mission.MissionRuleId = FName(*MissionRuleId);
			if (FormatVersion >= 20)
			{
				bMissionValid &= ReadGuidString(*MissionObject, TEXT("targetBaseId"), Mission.TargetBaseId, Context, Diagnostics);
			}
			bMissionValid &= ReadIsoDate(*MissionObject, TEXT("startedUtc"), Mission.StartedUtc, Context, Diagnostics);
			if (bMissionValid)
			{
				OutMissions.Add(MoveTemp(Mission));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadBaseAssaults(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FBaseAssaultState>& OutAssaults,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("baseAssaults")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'baseAssaults'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("baseAssaults"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.baseAssaults must be an array."));
			return false;
		}
		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* AssaultObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.baseAssaults[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(AssaultObject)
				|| AssaultObject == nullptr || !AssaultObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*AssaultObject,
				{ TEXT("assaultId"), TEXT("missionId"), TEXT("contactId"), TEXT("baseId"), TEXT("arrivedUtc") },
				Context, Diagnostics);
			FBaseAssaultState Assault;
			bool bAssaultValid = ReadGuidString(*AssaultObject, TEXT("assaultId"), Assault.AssaultId, Context, Diagnostics);
			bAssaultValid &= ReadGuidString(*AssaultObject, TEXT("missionId"), Assault.MissionId, Context, Diagnostics);
			bAssaultValid &= ReadGuidString(*AssaultObject, TEXT("contactId"), Assault.ContactId, Context, Diagnostics);
			bAssaultValid &= ReadGuidString(*AssaultObject, TEXT("baseId"), Assault.BaseId, Context, Diagnostics);
			bAssaultValid &= ReadIsoDate(*AssaultObject, TEXT("arrivedUtc"), Assault.ArrivedUtc, Context, Diagnostics);
			if (bAssaultValid)
			{
				OutAssaults.Add(MoveTemp(Assault));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	bool ReadCraftAcquisitionOrders(
		const TSharedPtr<FJsonObject>& StateObject,
		TArray<FCraftAcquisitionOrderState>& OutOrders,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!StateObject->HasField(TEXT("craftAcquisitionOrders")))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("missing_field"), TEXT("save.state is missing required array 'craftAcquisitionOrders'."));
			return false;
		}
		if (!StateObject->TryGetArrayField(TEXT("craftAcquisitionOrders"), Values) || Values == nullptr)
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state.craftAcquisitionOrders must be an array."));
			return false;
		}

		bool bValid = true;
		for (int32 Index = 0; Index < Values->Num(); ++Index)
		{
			const TSharedPtr<FJsonObject>* OrderObject = nullptr;
			const FString Context = FString::Printf(TEXT("save.state.craftAcquisitionOrders[%d]"), Index);
			if (!(*Values)[Index].IsValid() || !(*Values)[Index]->TryGetObject(OrderObject) || OrderObject == nullptr || !OrderObject->IsValid())
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), Context + TEXT(" must be an object."));
				bValid = false;
				continue;
			}
			WarnUnknownFields(*OrderObject, { TEXT("orderId"), TEXT("craftId"), TEXT("displayName"), TEXT("craftRuleId"), TEXT("baseId"), TEXT("remainingTransitSeconds") }, Context, Diagnostics);

			FCraftAcquisitionOrderState Order;
			FString CraftRuleId;
			bool bOrderValid = ReadGuidString(*OrderObject, TEXT("orderId"), Order.OrderId, Context, Diagnostics);
			bOrderValid &= ReadGuidString(*OrderObject, TEXT("craftId"), Order.CraftId, Context, Diagnostics);
			bOrderValid &= ReadRequiredString(*OrderObject, TEXT("displayName"), Order.DisplayName, Context, Diagnostics);
			bOrderValid &= ReadRequiredString(*OrderObject, TEXT("craftRuleId"), CraftRuleId, Context, Diagnostics);
			Order.CraftRuleId = FName(*CraftRuleId);
			bOrderValid &= ReadGuidString(*OrderObject, TEXT("baseId"), Order.BaseId, Context, Diagnostics);
			bOrderValid &= ReadInt64String(*OrderObject, TEXT("remainingTransitSeconds"), Order.RemainingTransitSeconds, Context, Diagnostics);
			if (bOrderValid)
			{
				OutOrders.Add(MoveTemp(Order));
			}
			else
			{
				bValid = false;
			}
		}
		return bValid;
	}

	TSharedRef<FJsonObject> MakeHeaderJson(const FCampaignSaveHeader& Header)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("formatVersion"), Header.FormatVersion);
		Object->SetStringField(TEXT("campaignId"), Header.CampaignId.ToString(EGuidFormats::DigitsWithHyphensLower));
		Object->SetStringField(TEXT("createdUtc"), Header.CreatedUtc.ToIso8601());
		Object->SetStringField(TEXT("lastSavedUtc"), Header.LastSavedUtc.ToIso8601());
		Object->SetStringField(TEXT("buildVersion"), Header.BuildVersion);

		TArray<TSharedPtr<FJsonValue>> Packages;
		for (const FCampaignContentVersion& Package : Header.ContentPackages)
		{
			const TSharedRef<FJsonObject> PackageObject = MakeShared<FJsonObject>();
			PackageObject->SetStringField(TEXT("id"), Package.PackageId.ToString());
			PackageObject->SetStringField(TEXT("version"), Package.Version);
			Packages.Add(MakeShared<FJsonValueObject>(PackageObject));
		}
		Object->SetArrayField(TEXT("contentPackages"), Packages);
		Object->SetStringField(TEXT("contentFingerprint"), Header.ContentFingerprint);
		Object->SetStringField(TEXT("saveChecksum"), Header.SaveChecksum);
		return Object;
	}

	TSharedRef<FJsonObject> MakeStateJson(const FCampaignState& State)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("strategicUtc"), State.StrategicTime.Utc.ToIso8601());
		Object->SetStringField(TEXT("randomInitialSeed"), LexToString(State.SimulationRandom.InitialSeed));
		Object->SetStringField(TEXT("randomDrawCount"), LexToString(State.SimulationRandom.DrawCount));
		Object->SetStringField(TEXT("randomState"), UInt64ToHex(State.SimulationRandom.GetStateForSave()));
		Object->SetStringField(TEXT("funds"), LexToString(State.Funds));
		Object->SetStringField(TEXT("campaignScore"), LexToString(State.CampaignScore));
		Object->SetStringField(TEXT("difficulty"), DifficultyToString(State.Difficulty));
		Object->SetStringField(TEXT("commandSequence"), LexToString(State.CommandSequence));

		TArray<TSharedPtr<FJsonValue>> CompletedResearch;
		for (const FName ResearchId : State.CompletedResearch)
		{
			CompletedResearch.Add(MakeShared<FJsonValueString>(ResearchId.ToString()));
		}
		Object->SetArrayField(TEXT("completedResearch"), CompletedResearch);
		Object->SetStringField(TEXT("monthlyFunding"), LexToString(State.MonthlyFunding));

		TArray<TSharedPtr<FJsonValue>> Bases;
		for (const FStrategicBaseState& Base : State.Bases)
		{
			const TSharedRef<FJsonObject> BaseObject = MakeShared<FJsonObject>();
			BaseObject->SetStringField(TEXT("id"), Base.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			BaseObject->SetStringField(TEXT("name"), Base.Name);
			BaseObject->SetStringField(TEXT("regionId"), Base.RegionId.ToString());
			BaseObject->SetNumberField(TEXT("longitudeMilliDegrees"), Base.LongitudeMilliDegrees);
			BaseObject->SetNumberField(TEXT("latitudeMilliDegrees"), Base.LatitudeMilliDegrees);
			BaseObject->SetNumberField(TEXT("scientistCapacity"), Base.ScientistCapacity);
			BaseObject->SetNumberField(TEXT("engineerCapacity"), Base.EngineerCapacity);
			BaseObject->SetNumberField(TEXT("signalWatchScientists"), Base.SignalWatchScientists);
			BaseObject->SetNumberField(TEXT("worksCadreEngineers"), Base.WorksCadreEngineers);
			BaseObject->SetStringField(TEXT("worksCadreCharter"),
				WorksCadreCharterToString(Base.WorksCadreCharter));
			TArray<TSharedPtr<FJsonValue>> Facilities;
			for (const FName FacilityId : Base.BuiltFacilities)
			{
				Facilities.Add(MakeShared<FJsonValueString>(FacilityId.ToString()));
			}
			BaseObject->SetArrayField(TEXT("facilities"), Facilities);
			TArray<TSharedPtr<FJsonValue>> FacilityPlacements;
			for (const FBaseFacilityState& Facility : Base.Facilities)
			{
				const TSharedRef<FJsonObject> FacilityObject = MakeShared<FJsonObject>();
				FacilityObject->SetStringField(TEXT("instanceId"), Facility.InstanceId.ToString(EGuidFormats::DigitsWithHyphensLower));
				FacilityObject->SetStringField(TEXT("facilityId"), Facility.FacilityId.ToString());
				FacilityObject->SetNumberField(TEXT("gridX"), Facility.GridX);
				FacilityObject->SetNumberField(TEXT("gridY"), Facility.GridY);
				FacilityObject->SetNumberField(TEXT("damage"), Facility.Damage);
				FacilityObject->SetNumberField(TEXT("reservedRepairDamage"), Facility.ReservedRepairDamage);
				FacilityObject->SetStringField(TEXT("remainingRepairSeconds"), LexToString(Facility.RemainingRepairSeconds));
				FacilityPlacements.Add(MakeShared<FJsonValueObject>(FacilityObject));
			}
			BaseObject->SetArrayField(TEXT("facilityPlacements"), FacilityPlacements);
			TArray<TSharedPtr<FJsonValue>> Inventory;
			for (const FInventoryStack& Stack : Base.Inventory)
			{
				const TSharedRef<FJsonObject> StackObject = MakeShared<FJsonObject>();
				StackObject->SetStringField(TEXT("itemId"), Stack.ItemId.ToString());
				StackObject->SetNumberField(TEXT("quantity"), Stack.Quantity);
				Inventory.Add(MakeShared<FJsonValueObject>(StackObject));
			}
			BaseObject->SetArrayField(TEXT("inventory"), Inventory);
			Bases.Add(MakeShared<FJsonValueObject>(BaseObject));
		}
		Object->SetArrayField(TEXT("bases"), Bases);

		TArray<TSharedPtr<FJsonValue>> MutualAidConvoys;
		for (const FMutualAidConvoyState& Convoy : State.MutualAidConvoys)
		{
			const TSharedRef<FJsonObject> ConvoyObject = MakeShared<FJsonObject>();
			ConvoyObject->SetStringField(TEXT("convoyId"),
				Convoy.ConvoyId.ToString(EGuidFormats::DigitsWithHyphensLower));
			ConvoyObject->SetStringField(TEXT("sourceBaseId"),
				Convoy.SourceBaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			ConvoyObject->SetStringField(TEXT("destinationBaseId"),
				Convoy.DestinationBaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			ConvoyObject->SetStringField(TEXT("itemId"), Convoy.ItemId.ToString());
			ConvoyObject->SetNumberField(TEXT("quantity"), Convoy.Quantity);
			ConvoyObject->SetStringField(TEXT("dispatchSequence"),
				LexToString(Convoy.DispatchSequence));
			ConvoyObject->SetStringField(TEXT("remainingTransitSeconds"),
				LexToString(Convoy.RemainingTransitSeconds));
			ConvoyObject->SetStringField(TEXT("routePolicy"),
				MutualAidRoutePolicyToString(Convoy.RoutePolicy));
			ConvoyObject->SetStringField(TEXT("totalTransitSeconds"),
				LexToString(Convoy.TotalTransitSeconds));
			ConvoyObject->SetNumberField(TEXT("routePressure"), Convoy.RoutePressure);
			ConvoyObject->SetBoolField(TEXT("signalEscort"), Convoy.bSignalEscort);
			ConvoyObject->SetStringField(TEXT("signalEscortCost"),
				LexToString(Convoy.SignalEscortCost));
			ConvoyObject->SetBoolField(
				TEXT("interdictionResolved"), Convoy.bInterdictionResolved);
			ConvoyObject->SetStringField(TEXT("forecastInterdictionDelaySeconds"),
				LexToString(Convoy.ForecastInterdictionDelaySeconds));
			ConvoyObject->SetStringField(TEXT("interdictionDelaySeconds"),
				LexToString(Convoy.InterdictionDelaySeconds));
			ConvoyObject->SetStringField(TEXT("currentLegOriginBaseId"),
				Convoy.CurrentLegOriginBaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			ConvoyObject->SetStringField(TEXT("relayWaypointBaseId"),
				Convoy.RelayWaypointBaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			ConvoyObject->SetStringField(TEXT("onwardRoutePolicy"),
				MutualAidRoutePolicyToString(Convoy.OnwardRoutePolicy));
			ConvoyObject->SetStringField(TEXT("onwardTotalTransitSeconds"),
				LexToString(Convoy.OnwardTotalTransitSeconds));
			ConvoyObject->SetNumberField(
				TEXT("onwardRoutePressure"), Convoy.OnwardRoutePressure);
			ConvoyObject->SetBoolField(
				TEXT("onwardInterdictionResolved"),
				Convoy.bOnwardInterdictionResolved);
			ConvoyObject->SetStringField(
				TEXT("onwardForecastInterdictionDelaySeconds"),
				LexToString(Convoy.OnwardForecastInterdictionDelaySeconds));
			ConvoyObject->SetNumberField(
				TEXT("balancedHandoffQuantity"), Convoy.BalancedHandoffQuantity);
			MutualAidConvoys.Add(MakeShared<FJsonValueObject>(ConvoyObject));
		}
		Object->SetArrayField(TEXT("mutualAidConvoys"), MutualAidConvoys);

		TArray<TSharedPtr<FJsonValue>> ResearchProjects;
		for (const FResearchProjectState& Project : State.ResearchProjects)
		{
			const TSharedRef<FJsonObject> ProjectObject = MakeShared<FJsonObject>();
			ProjectObject->SetStringField(TEXT("researchId"), Project.ResearchId.ToString());
			ProjectObject->SetStringField(TEXT("baseId"), Project.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			ProjectObject->SetNumberField(TEXT("assignedScientists"), Project.AssignedScientists);
			ProjectObject->SetStringField(TEXT("accumulatedWorkSeconds"), LexToString(Project.AccumulatedWorkSeconds));
			ResearchProjects.Add(MakeShared<FJsonValueObject>(ProjectObject));
		}
		Object->SetArrayField(TEXT("researchProjects"), ResearchProjects);

		TArray<TSharedPtr<FJsonValue>> ManufacturingProjects;
		for (const FManufacturingProjectState& Project : State.ManufacturingProjects)
		{
			const TSharedRef<FJsonObject> ProjectObject = MakeShared<FJsonObject>();
			ProjectObject->SetStringField(TEXT("projectId"), Project.ProjectId.ToString(EGuidFormats::DigitsWithHyphensLower));
			ProjectObject->SetStringField(TEXT("itemId"), Project.ItemId.ToString());
			ProjectObject->SetStringField(TEXT("baseId"), Project.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			ProjectObject->SetNumberField(TEXT("assignedEngineers"), Project.AssignedEngineers);
			ProjectObject->SetNumberField(TEXT("unitsRemaining"), Project.UnitsRemaining);
			ProjectObject->SetStringField(TEXT("accumulatedWorkSeconds"), LexToString(Project.AccumulatedWorkSeconds));
			ManufacturingProjects.Add(MakeShared<FJsonValueObject>(ProjectObject));
		}
		Object->SetArrayField(TEXT("manufacturingProjects"), ManufacturingProjects);

		TArray<TSharedPtr<FJsonValue>> FacilityConstructionProjects;
		for (const FFacilityConstructionProjectState& Project : State.FacilityConstructionProjects)
		{
			const TSharedRef<FJsonObject> ProjectObject = MakeShared<FJsonObject>();
			ProjectObject->SetStringField(TEXT("projectId"), Project.ProjectId.ToString(EGuidFormats::DigitsWithHyphensLower));
			ProjectObject->SetStringField(TEXT("facilityInstanceId"), Project.FacilityInstanceId.ToString(EGuidFormats::DigitsWithHyphensLower));
			ProjectObject->SetStringField(TEXT("baseId"), Project.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			ProjectObject->SetStringField(TEXT("facilityId"), Project.FacilityId.ToString());
			ProjectObject->SetNumberField(TEXT("gridX"), Project.GridX);
			ProjectObject->SetNumberField(TEXT("gridY"), Project.GridY);
			ProjectObject->SetStringField(TEXT("remainingBuildSeconds"), LexToString(Project.RemainingBuildSeconds));
			FacilityConstructionProjects.Add(MakeShared<FJsonValueObject>(ProjectObject));
		}
		Object->SetArrayField(TEXT("facilityConstructionProjects"), FacilityConstructionProjects);

		TArray<TSharedPtr<FJsonValue>> Personnel;
		for (const FPersonnelState& Person : State.Personnel)
		{
			const TSharedRef<FJsonObject> PersonObject = MakeShared<FJsonObject>();
			PersonObject->SetStringField(TEXT("id"), Person.PersonnelId.ToString(EGuidFormats::DigitsWithHyphensLower));
			PersonObject->SetStringField(TEXT("displayName"), Person.DisplayName);
			PersonObject->SetStringField(TEXT("roleId"), Person.RoleId.ToString());
			PersonObject->SetStringField(TEXT("baseId"), Person.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			PersonObject->SetStringField(TEXT("status"), PersonnelStatusToString(Person.Status));
			PersonObject->SetNumberField(TEXT("rank"), Person.Rank);
			PersonObject->SetNumberField(TEXT("missions"), Person.Missions);
			PersonObject->SetNumberField(TEXT("kills"), Person.Kills);
			PersonObject->SetNumberField(TEXT("experience"), Person.Experience);
			PersonObject->SetNumberField(TEXT("maxHealth"), Person.MaxHealth);
			PersonObject->SetNumberField(TEXT("currentHealth"), Person.CurrentHealth);
			PersonObject->SetNumberField(TEXT("accuracy"), Person.Accuracy);
			PersonObject->SetNumberField(TEXT("resolve"), Person.Resolve);
			PersonObject->SetNumberField(TEXT("mobility"), Person.Mobility);
			PersonObject->SetNumberField(TEXT("strength"), Person.Strength);
			PersonObject->SetStringField(TEXT("remainingRecoverySeconds"), LexToString(Person.RemainingRecoverySeconds));
			PersonObject->SetStringField(TEXT("recoveryPlan"), RecoveryPlanToString(Person.RecoveryPlan));
			PersonObject->SetStringField(TEXT("remainingTrainingSeconds"), LexToString(Person.RemainingTrainingSeconds));
			PersonObject->SetStringField(TEXT("trainingFocus"), TrainingFocusToString(Person.TrainingFocus));
			PersonObject->SetStringField(TEXT("stewardshipFocus"), StewardshipFocusToString(Person.StewardshipFocus));
			PersonObject->SetStringField(TEXT("remainingStewardshipSeconds"), LexToString(Person.RemainingStewardshipSeconds));
			PersonObject->SetNumberField(TEXT("stewardshipToursCompleted"), Person.StewardshipToursCompleted);
			TArray<TSharedPtr<FJsonValue>> EquippedItems;
			for (const FName ItemId : Person.EquippedItems)
			{
				EquippedItems.Add(MakeShared<FJsonValueString>(ItemId.ToString()));
			}
			PersonObject->SetArrayField(TEXT("equippedItems"), EquippedItems);
			PersonObject->SetNumberField(TEXT("pendingDoctrineChoices"), Person.PendingDoctrineChoices);
			TArray<TSharedPtr<FJsonValue>> DoctrineSelections;
			for (const FName DoctrineId : Person.DoctrineSelections)
			{
				DoctrineSelections.Add(MakeShared<FJsonValueString>(DoctrineId.ToString()));
			}
			PersonObject->SetArrayField(TEXT("doctrineSelections"), DoctrineSelections);
			TArray<TSharedPtr<FJsonValue>> Commendations;
			for (const FName CommendationId : Person.Commendations)
			{
				Commendations.Add(MakeShared<FJsonValueString>(CommendationId.ToString()));
			}
			PersonObject->SetArrayField(TEXT("commendations"), Commendations);
			Personnel.Add(MakeShared<FJsonValueObject>(PersonObject));
		}
		Object->SetArrayField(TEXT("personnel"), Personnel);

		TArray<TSharedPtr<FJsonValue>> PersonnelSquadBonds;
		for (const FPersonnelSquadBondState& Bond : State.PersonnelSquadBonds)
		{
			const TSharedRef<FJsonObject> BondObject = MakeShared<FJsonObject>();
			BondObject->SetStringField(TEXT("firstPersonnelId"),
				Bond.FirstPersonnelId.ToString(EGuidFormats::DigitsWithHyphensLower));
			BondObject->SetStringField(TEXT("secondPersonnelId"),
				Bond.SecondPersonnelId.ToString(EGuidFormats::DigitsWithHyphensLower));
			BondObject->SetNumberField(TEXT("sharedVictories"), Bond.SharedVictories);
			PersonnelSquadBonds.Add(MakeShared<FJsonValueObject>(BondObject));
		}
		Object->SetArrayField(TEXT("personnelSquadBonds"), PersonnelSquadBonds);

		TArray<TSharedPtr<FJsonValue>> RecruitmentOrders;
		for (const FRecruitmentOrderState& Order : State.RecruitmentOrders)
		{
			const TSharedRef<FJsonObject> OrderObject = MakeShared<FJsonObject>();
			OrderObject->SetStringField(TEXT("orderId"), Order.OrderId.ToString(EGuidFormats::DigitsWithHyphensLower));
			OrderObject->SetStringField(TEXT("personnelId"), Order.PersonnelId.ToString(EGuidFormats::DigitsWithHyphensLower));
			OrderObject->SetStringField(TEXT("displayName"), Order.DisplayName);
			OrderObject->SetStringField(TEXT("roleId"), Order.RoleId.ToString());
			OrderObject->SetStringField(TEXT("baseId"), Order.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			OrderObject->SetStringField(TEXT("remainingTransitSeconds"), LexToString(Order.RemainingTransitSeconds));
			RecruitmentOrders.Add(MakeShared<FJsonValueObject>(OrderObject));
		}
		Object->SetArrayField(TEXT("recruitmentOrders"), RecruitmentOrders);

		TArray<TSharedPtr<FJsonValue>> Memorial;
		for (const FMemorialRecord& Record : State.Memorial)
		{
			const TSharedRef<FJsonObject> RecordObject = MakeShared<FJsonObject>();
			RecordObject->SetStringField(TEXT("personnelId"), Record.PersonnelId.ToString(EGuidFormats::DigitsWithHyphensLower));
			RecordObject->SetStringField(TEXT("displayName"), Record.DisplayName);
			RecordObject->SetStringField(TEXT("roleId"), Record.RoleId.ToString());
			RecordObject->SetNumberField(TEXT("rank"), Record.Rank);
			RecordObject->SetNumberField(TEXT("missions"), Record.Missions);
			RecordObject->SetNumberField(TEXT("kills"), Record.Kills);
			RecordObject->SetStringField(TEXT("deathUtc"), Record.DeathUtc.ToIso8601());
			RecordObject->SetStringField(TEXT("causeId"), Record.CauseId.ToString());
			RecordObject->SetNumberField(TEXT("stewardshipToursCompleted"), Record.StewardshipToursCompleted);
			TArray<TSharedPtr<FJsonValue>> DoctrineSelections;
			for (const FName DoctrineId : Record.DoctrineSelections)
			{
				DoctrineSelections.Add(MakeShared<FJsonValueString>(DoctrineId.ToString()));
			}
			RecordObject->SetArrayField(TEXT("doctrineSelections"), DoctrineSelections);
			TArray<TSharedPtr<FJsonValue>> Commendations;
			for (const FName CommendationId : Record.Commendations)
			{
				Commendations.Add(MakeShared<FJsonValueString>(CommendationId.ToString()));
			}
			RecordObject->SetArrayField(TEXT("commendations"), Commendations);
			Memorial.Add(MakeShared<FJsonValueObject>(RecordObject));
		}
		Object->SetArrayField(TEXT("memorial"), Memorial);

		TArray<TSharedPtr<FJsonValue>> Craft;
		for (const FCraftState& CraftState : State.Craft)
		{
			const TSharedRef<FJsonObject> CraftObject = MakeShared<FJsonObject>();
			CraftObject->SetStringField(TEXT("craftId"), CraftState.CraftId.ToString(EGuidFormats::DigitsWithHyphensLower));
			CraftObject->SetStringField(TEXT("displayName"), CraftState.DisplayName);
			CraftObject->SetStringField(TEXT("craftRuleId"), CraftState.CraftRuleId.ToString());
			CraftObject->SetStringField(TEXT("baseId"), CraftState.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			CraftObject->SetStringField(TEXT("assignedPilotId"), CraftState.AssignedPilotId.ToString(EGuidFormats::DigitsWithHyphensLower));
			CraftObject->SetStringField(TEXT("status"), CraftStatusToString(CraftState.Status));
			CraftObject->SetNumberField(TEXT("currentHull"), CraftState.CurrentHull);
			CraftObject->SetNumberField(TEXT("currentFuel"), CraftState.CurrentFuel);
			CraftObject->SetStringField(TEXT("remainingRepairSeconds"), LexToString(CraftState.RemainingRepairSeconds));
			CraftObject->SetStringField(TEXT("remainingRefuelSeconds"), LexToString(CraftState.RemainingRefuelSeconds));
			CraftObject->SetNumberField(TEXT("completedSorties"), CraftState.CompletedSorties);
			TArray<TSharedPtr<FJsonValue>> EquipmentItems;
			for (const FName ItemId : CraftState.EquipmentItems)
			{
				EquipmentItems.Add(MakeShared<FJsonValueString>(ItemId.ToString()));
			}
			CraftObject->SetArrayField(TEXT("equipmentItems"), EquipmentItems);
			CraftObject->SetStringField(TEXT("targetContactId"), CraftState.TargetContactId.ToString(EGuidFormats::DigitsWithHyphensLower));
			CraftObject->SetStringField(TEXT("remainingRouteSeconds"), LexToString(CraftState.RemainingRouteSeconds));
			CraftObject->SetStringField(TEXT("reservedReturnSeconds"), LexToString(CraftState.ReservedReturnSeconds));
			TArray<TSharedPtr<FJsonValue>> WeaponStates;
			for (const FCraftWeaponState& WeaponState : CraftState.WeaponStates)
			{
				const TSharedRef<FJsonObject> WeaponObject = MakeShared<FJsonObject>();
				WeaponObject->SetStringField(TEXT("weaponItemId"), WeaponState.WeaponItemId.ToString());
				WeaponObject->SetNumberField(TEXT("ammunition"), WeaponState.Ammunition);
				WeaponObject->SetStringField(TEXT("remainingCooldownSeconds"), LexToString(WeaponState.RemainingCooldownSeconds));
				WeaponStates.Add(MakeShared<FJsonValueObject>(WeaponObject));
			}
			CraftObject->SetArrayField(TEXT("weaponStates"), WeaponStates);
			TArray<TSharedPtr<FJsonValue>> AssignedAgentIds;
			for (const FGuid& AgentId : CraftState.AssignedAgentIds)
			{
				AssignedAgentIds.Add(MakeShared<FJsonValueString>(AgentId.ToString(EGuidFormats::DigitsWithHyphensLower)));
			}
			CraftObject->SetArrayField(TEXT("assignedAgentIds"), AssignedAgentIds);
			TArray<TSharedPtr<FJsonValue>> Cargo;
			for (const FInventoryStack& Stack : CraftState.Cargo)
			{
				const TSharedRef<FJsonObject> StackObject = MakeShared<FJsonObject>();
				StackObject->SetStringField(TEXT("itemId"), Stack.ItemId.ToString());
				StackObject->SetNumberField(TEXT("quantity"), Stack.Quantity);
				Cargo.Add(MakeShared<FJsonValueObject>(StackObject));
			}
			CraftObject->SetArrayField(TEXT("cargo"), Cargo);
			TArray<TSharedPtr<FJsonValue>> PendingSalvage;
			for (const FInventoryStack& Stack : CraftState.PendingSalvage)
			{
				const TSharedRef<FJsonObject> StackObject = MakeShared<FJsonObject>();
				StackObject->SetStringField(TEXT("itemId"), Stack.ItemId.ToString());
				StackObject->SetNumberField(TEXT("quantity"), Stack.Quantity);
				PendingSalvage.Add(MakeShared<FJsonValueObject>(StackObject));
			}
			CraftObject->SetArrayField(TEXT("pendingSalvage"), PendingSalvage);
			CraftObject->SetStringField(TEXT("targetSiteId"), CraftState.TargetSiteId.ToString(EGuidFormats::DigitsWithHyphensLower));
			Craft.Add(MakeShared<FJsonValueObject>(CraftObject));
		}
		Object->SetArrayField(TEXT("craft"), Craft);

		TArray<TSharedPtr<FJsonValue>> CraftAcquisitionOrders;
		for (const FCraftAcquisitionOrderState& Order : State.CraftAcquisitionOrders)
		{
			const TSharedRef<FJsonObject> OrderObject = MakeShared<FJsonObject>();
			OrderObject->SetStringField(TEXT("orderId"), Order.OrderId.ToString(EGuidFormats::DigitsWithHyphensLower));
			OrderObject->SetStringField(TEXT("craftId"), Order.CraftId.ToString(EGuidFormats::DigitsWithHyphensLower));
			OrderObject->SetStringField(TEXT("displayName"), Order.DisplayName);
			OrderObject->SetStringField(TEXT("craftRuleId"), Order.CraftRuleId.ToString());
			OrderObject->SetStringField(TEXT("baseId"), Order.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			OrderObject->SetStringField(TEXT("remainingTransitSeconds"), LexToString(Order.RemainingTransitSeconds));
			CraftAcquisitionOrders.Add(MakeShared<FJsonValueObject>(OrderObject));
		}
		Object->SetArrayField(TEXT("craftAcquisitionOrders"), CraftAcquisitionOrders);

		TArray<TSharedPtr<FJsonValue>> StrategicContacts;
		for (const FStrategicContactState& Contact : State.StrategicContacts)
		{
			const TSharedRef<FJsonObject> ContactObject = MakeShared<FJsonObject>();
			ContactObject->SetStringField(TEXT("contactId"), Contact.ContactId.ToString(EGuidFormats::DigitsWithHyphensLower));
			ContactObject->SetStringField(TEXT("contactRuleId"), Contact.ContactRuleId.ToString());
			ContactObject->SetStringField(TEXT("status"), ContactStatusToString(Contact.Status));
			ContactObject->SetNumberField(TEXT("originLongitudeMilliDegrees"), Contact.OriginLongitudeMilliDegrees);
			ContactObject->SetNumberField(TEXT("originLatitudeMilliDegrees"), Contact.OriginLatitudeMilliDegrees);
			ContactObject->SetNumberField(TEXT("longitudeMilliDegrees"), Contact.LongitudeMilliDegrees);
			ContactObject->SetNumberField(TEXT("latitudeMilliDegrees"), Contact.LatitudeMilliDegrees);
			ContactObject->SetNumberField(TEXT("destinationLongitudeMilliDegrees"), Contact.DestinationLongitudeMilliDegrees);
			ContactObject->SetNumberField(TEXT("destinationLatitudeMilliDegrees"), Contact.DestinationLatitudeMilliDegrees);
			ContactObject->SetStringField(TEXT("totalRouteSeconds"), LexToString(Contact.TotalRouteSeconds));
			ContactObject->SetStringField(TEXT("elapsedRouteSeconds"), LexToString(Contact.ElapsedRouteSeconds));
			ContactObject->SetNumberField(TEXT("currentHull"), Contact.CurrentHull);
			ContactObject->SetNumberField(TEXT("completedCombatRounds"), Contact.CompletedCombatRounds);
			ContactObject->SetStringField(TEXT("remainingAttackCooldownSeconds"), LexToString(Contact.RemainingAttackCooldownSeconds));
			StrategicContacts.Add(MakeShared<FJsonValueObject>(ContactObject));
		}
		Object->SetArrayField(TEXT("strategicContacts"), StrategicContacts);

		TArray<TSharedPtr<FJsonValue>> StrategicSites;
		for (const FStrategicSiteState& Site : State.StrategicSites)
		{
			const TSharedRef<FJsonObject> SiteObject = MakeShared<FJsonObject>();
			SiteObject->SetStringField(TEXT("siteId"), Site.SiteId.ToString(EGuidFormats::DigitsWithHyphensLower));
			SiteObject->SetStringField(TEXT("type"), SiteTypeToString(Site.Type));
			SiteObject->SetStringField(TEXT("sourceContactRuleId"), Site.SourceContactRuleId.ToString());
			SiteObject->SetNumberField(TEXT("longitudeMilliDegrees"), Site.LongitudeMilliDegrees);
			SiteObject->SetNumberField(TEXT("latitudeMilliDegrees"), Site.LatitudeMilliDegrees);
			SiteObject->SetNumberField(TEXT("threatRating"), Site.ThreatRating);
			SiteObject->SetStringField(TEXT("remainingLifetimeSeconds"), LexToString(Site.RemainingLifetimeSeconds));
			StrategicSites.Add(MakeShared<FJsonValueObject>(SiteObject));
		}
		Object->SetArrayField(TEXT("strategicSites"), StrategicSites);

		TArray<TSharedPtr<FJsonValue>> TacticalOperations;
		for (const FTacticalOperationState& Operation : State.TacticalOperations)
		{
			const TSharedRef<FJsonObject> OperationObject = MakeShared<FJsonObject>();
			OperationObject->SetStringField(TEXT("operationId"), Operation.OperationId.ToString(EGuidFormats::DigitsWithHyphensLower));
			OperationObject->SetStringField(TEXT("type"), TacticalOperationTypeToString(Operation.Type));
			OperationObject->SetStringField(TEXT("siteId"), Operation.SiteId.ToString(EGuidFormats::DigitsWithHyphensLower));
			OperationObject->SetStringField(TEXT("craftId"), Operation.CraftId.ToString(EGuidFormats::DigitsWithHyphensLower));
			OperationObject->SetStringField(TEXT("baseId"), Operation.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			OperationObject->SetStringField(TEXT("assaultId"), Operation.AssaultId.ToString(EGuidFormats::DigitsWithHyphensLower));
			OperationObject->SetStringField(TEXT("tacticalSeed"), LexToString(Operation.TacticalSeed));
			OperationObject->SetStringField(TEXT("createdUtc"), Operation.CreatedUtc.ToIso8601());
			TArray<TSharedPtr<FJsonValue>> AgentIds;
			for (const FGuid& AgentId : Operation.AgentIds)
			{
				AgentIds.Add(MakeShared<FJsonValueString>(AgentId.ToString(EGuidFormats::DigitsWithHyphensLower)));
			}
			OperationObject->SetArrayField(TEXT("agentIds"), AgentIds);
			TArray<TSharedPtr<FJsonValue>> Cargo;
			for (const FInventoryStack& Stack : Operation.Cargo)
			{
				const TSharedRef<FJsonObject> StackObject = MakeShared<FJsonObject>();
				StackObject->SetStringField(TEXT("itemId"), Stack.ItemId.ToString());
				StackObject->SetNumberField(TEXT("quantity"), Stack.Quantity);
				Cargo.Add(MakeShared<FJsonValueObject>(StackObject));
			}
			OperationObject->SetArrayField(TEXT("cargo"), Cargo);
			TacticalOperations.Add(MakeShared<FJsonValueObject>(OperationObject));
		}
		Object->SetArrayField(TEXT("tacticalOperations"), TacticalOperations);

		TArray<TSharedPtr<FJsonValue>> TacticalBattles;
		for (const FTacticalBattleState& Battle : State.TacticalBattles)
		{
			const TSharedRef<FJsonObject> BattleObject = MakeShared<FJsonObject>();
			BattleObject->SetStringField(TEXT("battleId"), Battle.BattleId.ToString(EGuidFormats::DigitsWithHyphensLower));
			BattleObject->SetStringField(TEXT("operationId"), Battle.OperationId.ToString(EGuidFormats::DigitsWithHyphensLower));
			BattleObject->SetStringField(TEXT("siteId"), Battle.SiteId.ToString(EGuidFormats::DigitsWithHyphensLower));
			BattleObject->SetStringField(TEXT("missionRuleId"), Battle.MissionRuleId.ToString());
			BattleObject->SetStringField(TEXT("createdUtc"), Battle.CreatedUtc.ToIso8601());
			BattleObject->SetNumberField(TEXT("width"), Battle.Width);
			BattleObject->SetNumberField(TEXT("height"), Battle.Height);
			BattleObject->SetNumberField(TEXT("levels"), Battle.Levels);
			BattleObject->SetNumberField(TEXT("turnLimit"), Battle.TurnLimit);
			BattleObject->SetNumberField(TEXT("turnNumber"), Battle.TurnNumber);
			BattleObject->SetBoolField(TEXT("requiresExtraction"), Battle.bRequiresExtraction);
			BattleObject->SetStringField(TEXT("phase"), TacticalPhaseToString(Battle.Phase));
			BattleObject->SetStringField(TEXT("activeTeam"), TacticalTeamToString(Battle.ActiveTeam));
			BattleObject->SetStringField(TEXT("windDirection"), TacticalWindDirectionToString(Battle.WindDirection));
			BattleObject->SetNumberField(TEXT("windStrength"), Battle.WindStrength);
			BattleObject->SetStringField(TEXT("randomInitialSeed"), LexToString(Battle.TacticalRandom.InitialSeed));
			BattleObject->SetStringField(TEXT("randomDrawCount"), LexToString(Battle.TacticalRandom.DrawCount));
			BattleObject->SetStringField(TEXT("randomState"), UInt64ToHex(Battle.TacticalRandom.GetStateForSave()));
			TArray<TSharedPtr<FJsonValue>> Cells;
			for (const FTacticalCellState& Cell : Battle.Cells)
			{
				const TSharedRef<FJsonObject> CellObject = MakeShared<FJsonObject>();
				CellObject->SetNumberField(TEXT("x"), Cell.X);
				CellObject->SetNumberField(TEXT("y"), Cell.Y);
				CellObject->SetNumberField(TEXT("z"), Cell.Z);
				CellObject->SetStringField(TEXT("terrainRuleId"), Cell.TerrainRuleId.ToString());
				CellObject->SetNumberField(TEXT("currentIntegrity"), Cell.CurrentIntegrity);
				CellObject->SetBoolField(TEXT("playerDeployment"), Cell.bPlayerDeployment);
				CellObject->SetBoolField(TEXT("extraction"), Cell.bExtraction);
				CellObject->SetBoolField(TEXT("doorOpen"), Cell.bDoorOpen);
				CellObject->SetNumberField(TEXT("smoke"), Cell.Smoke);
				CellObject->SetNumberField(TEXT("fire"), Cell.Fire);
				Cells.Add(MakeShared<FJsonValueObject>(CellObject));
			}
			BattleObject->SetArrayField(TEXT("cells"), Cells);
			TArray<TSharedPtr<FJsonValue>> PlayerDiscoveredCellIndices;
			for (const int32 CellIndex : Battle.PlayerDiscoveredCellIndices)
			{
				PlayerDiscoveredCellIndices.Add(MakeShared<FJsonValueNumber>(CellIndex));
			}
			BattleObject->SetArrayField(TEXT("playerDiscoveredCellIndices"), PlayerDiscoveredCellIndices);
			TArray<TSharedPtr<FJsonValue>> Units;
			for (const FTacticalUnitState& Unit : Battle.Units)
			{
				const TSharedRef<FJsonObject> UnitObject = MakeShared<FJsonObject>();
				UnitObject->SetStringField(TEXT("unitId"), Unit.UnitId.ToString(EGuidFormats::DigitsWithHyphensLower));
				UnitObject->SetStringField(TEXT("personnelId"), Unit.PersonnelId.ToString(EGuidFormats::DigitsWithHyphensLower));
				UnitObject->SetStringField(TEXT("sourceRuleId"), Unit.SourceRuleId.ToString());
				UnitObject->SetStringField(TEXT("displayName"), Unit.DisplayName);
				UnitObject->SetStringField(TEXT("team"), TacticalTeamToString(Unit.Team));
				UnitObject->SetStringField(TEXT("stance"), TacticalStanceToString(Unit.Stance));
				UnitObject->SetNumberField(TEXT("x"), Unit.X);
				UnitObject->SetNumberField(TEXT("y"), Unit.Y);
				UnitObject->SetNumberField(TEXT("z"), Unit.Z);
				UnitObject->SetNumberField(TEXT("maxHealth"), Unit.MaxHealth);
				UnitObject->SetNumberField(TEXT("currentHealth"), Unit.CurrentHealth);
				UnitObject->SetNumberField(TEXT("accuracy"), Unit.Accuracy);
				UnitObject->SetNumberField(TEXT("resolve"), Unit.Resolve);
				UnitObject->SetNumberField(TEXT("mobility"), Unit.Mobility);
				UnitObject->SetNumberField(TEXT("strength"), Unit.Strength);
				UnitObject->SetNumberField(TEXT("maxActionPoints"), Unit.MaxActionPoints);
				UnitObject->SetNumberField(TEXT("remainingActionPoints"), Unit.RemainingActionPoints);
				UnitObject->SetBoolField(TEXT("extracted"), Unit.bExtracted);
				UnitObject->SetNumberField(TEXT("kineticArmor"), Unit.KineticArmor);
				UnitObject->SetNumberField(TEXT("thermalArmor"), Unit.ThermalArmor);
				UnitObject->SetNumberField(TEXT("arcArmor"), Unit.ArcArmor);
				UnitObject->SetNumberField(TEXT("maxMorale"), Unit.MaxMorale);
				UnitObject->SetNumberField(TEXT("currentMorale"), Unit.CurrentMorale);
				UnitObject->SetNumberField(TEXT("suppression"), Unit.Suppression);
				TArray<TSharedPtr<FJsonValue>> WeaponStates;
				for (const FTacticalWeaponState& WeaponState : Unit.WeaponStates)
				{
					const TSharedRef<FJsonObject> WeaponObject = MakeShared<FJsonObject>();
					WeaponObject->SetStringField(TEXT("weaponItemId"), WeaponState.WeaponItemId.ToString());
					WeaponObject->SetNumberField(TEXT("loadedAmmunition"), WeaponState.LoadedAmmunition);
					WeaponStates.Add(MakeShared<FJsonValueObject>(WeaponObject));
				}
				UnitObject->SetArrayField(TEXT("weaponStates"), WeaponStates);
				TArray<TSharedPtr<FJsonValue>> CarriedItems;
				for (const FInventoryStack& Stack : Unit.CarriedItems)
				{
					const TSharedRef<FJsonObject> StackObject = MakeShared<FJsonObject>();
					StackObject->SetStringField(TEXT("itemId"), Stack.ItemId.ToString());
					StackObject->SetNumberField(TEXT("quantity"), Stack.Quantity);
					CarriedItems.Add(MakeShared<FJsonValueObject>(StackObject));
				}
				UnitObject->SetArrayField(TEXT("carriedItems"), CarriedItems);
				TArray<TSharedPtr<FJsonValue>> EjectedMagazines;
				for (const FTacticalMagazineState& Magazine : Unit.EjectedMagazines)
				{
					const TSharedRef<FJsonObject> MagazineObject = MakeShared<FJsonObject>();
					MagazineObject->SetStringField(TEXT("weaponItemId"), Magazine.WeaponItemId.ToString());
					MagazineObject->SetStringField(TEXT("ammunitionItemId"), Magazine.AmmunitionItemId.ToString());
					MagazineObject->SetNumberField(TEXT("loadedAmmunition"), Magazine.LoadedAmmunition);
					EjectedMagazines.Add(MakeShared<FJsonValueObject>(MagazineObject));
				}
				UnitObject->SetArrayField(TEXT("ejectedMagazines"), EjectedMagazines);
				Units.Add(MakeShared<FJsonValueObject>(UnitObject));
			}
			BattleObject->SetArrayField(TEXT("units"), Units);
			TArray<TSharedPtr<FJsonValue>> Objectives;
			for (const FTacticalObjectiveState& Objective : Battle.Objectives)
			{
				const TSharedRef<FJsonObject> ObjectiveObject = MakeShared<FJsonObject>();
				ObjectiveObject->SetStringField(TEXT("objectiveId"), Objective.ObjectiveId.ToString());
				ObjectiveObject->SetNumberField(TEXT("x"), Objective.X);
				ObjectiveObject->SetNumberField(TEXT("y"), Objective.Y);
				ObjectiveObject->SetNumberField(TEXT("z"), Objective.Z);
				ObjectiveObject->SetStringField(TEXT("status"), TacticalObjectiveStatusToString(Objective.Status));
				ObjectiveObject->SetStringField(TEXT("type"), TacticalObjectiveTypeToString(Objective.Type));
				ObjectiveObject->SetNumberField(TEXT("requiredInteractions"), Objective.RequiredInteractions);
				ObjectiveObject->SetNumberField(TEXT("completedInteractions"), Objective.CompletedInteractions);
				ObjectiveObject->SetNumberField(TEXT("adversaryInteractions"), Objective.AdversaryInteractions);
				Objectives.Add(MakeShared<FJsonValueObject>(ObjectiveObject));
			}
			BattleObject->SetArrayField(TEXT("objectives"), Objectives);
			TArray<TSharedPtr<FJsonValue>> Cargo;
			for (const FInventoryStack& Stack : Battle.Cargo)
			{
				const TSharedRef<FJsonObject> StackObject = MakeShared<FJsonObject>();
				StackObject->SetStringField(TEXT("itemId"), Stack.ItemId.ToString());
				StackObject->SetNumberField(TEXT("quantity"), Stack.Quantity);
				Cargo.Add(MakeShared<FJsonValueObject>(StackObject));
			}
			BattleObject->SetArrayField(TEXT("cargo"), Cargo);
			TacticalBattles.Add(MakeShared<FJsonValueObject>(BattleObject));
		}
		Object->SetArrayField(TEXT("tacticalBattles"), TacticalBattles);

		Object->SetNumberField(TEXT("adversaryEscalationLevel"), State.AdversaryEscalationLevel);
		Object->SetStringField(TEXT("nextAdversaryMissionSeconds"), LexToString(State.NextAdversaryMissionSeconds));
		Object->SetStringField(TEXT("nextAdversaryMissionSerial"), LexToString(State.NextAdversaryMissionSerial));
		Object->SetNumberField(TEXT("adversaryMissionsLaunched"), State.AdversaryMissionsLaunched);
		Object->SetNumberField(TEXT("adversaryMissionsEscaped"), State.AdversaryMissionsEscaped);
		Object->SetNumberField(TEXT("adversaryMissionsThwarted"), State.AdversaryMissionsThwarted);
		TArray<TSharedPtr<FJsonValue>> RegionalPressure;
		for (const FRegionalPressureState& Pressure : State.RegionalPressure)
		{
			const TSharedRef<FJsonObject> PressureObject = MakeShared<FJsonObject>();
			PressureObject->SetStringField(TEXT("regionId"), Pressure.RegionId.ToString());
			PressureObject->SetNumberField(TEXT("pressure"), Pressure.Pressure);
			RegionalPressure.Add(MakeShared<FJsonValueObject>(PressureObject));
		}
		Object->SetArrayField(TEXT("regionalPressure"), RegionalPressure);
		TArray<TSharedPtr<FJsonValue>> RegionalMandates;
		for (const FRegionalMandateState& Mandate : State.RegionalMandates)
		{
			const TSharedRef<FJsonObject> MandateObject = MakeShared<FJsonObject>();
			MandateObject->SetStringField(TEXT("regionId"), Mandate.RegionId.ToString());
			MandateObject->SetNumberField(TEXT("support"), Mandate.Support);
			MandateObject->SetStringField(TEXT("baselineMonthlyFunding"), LexToString(Mandate.BaselineMonthlyFunding));
			MandateObject->SetStringField(TEXT("currentMonthlyFunding"), LexToString(Mandate.CurrentMonthlyFunding));
			MandateObject->SetNumberField(TEXT("lastDiplomaticActionMonth"), Mandate.LastDiplomaticActionMonth);
			MandateObject->SetBoolField(TEXT("resilienceCharterSigned"), Mandate.bResilienceCharterSigned);
			MandateObject->SetBoolField(TEXT("horizonCompactMemberWithdrawn"), Mandate.bHorizonCompactMemberWithdrawn);
			RegionalMandates.Add(MakeShared<FJsonValueObject>(MandateObject));
		}
		Object->SetArrayField(TEXT("regionalMandates"), RegionalMandates);
		Object->SetBoolField(TEXT("horizonCompactRatified"), State.bHorizonCompactRatified);
		Object->SetNumberField(TEXT("lastCoalitionAidMonth"), State.LastCoalitionAidMonth);
		Object->SetNumberField(TEXT("lastCoalitionEmergencyVoteMonth"),
			State.LastCoalitionEmergencyVoteMonth);
		TArray<TSharedPtr<FJsonValue>> AdversaryMissions;
		for (const FAdversaryMissionState& Mission : State.AdversaryMissions)
		{
			const TSharedRef<FJsonObject> MissionObject = MakeShared<FJsonObject>();
			MissionObject->SetStringField(TEXT("missionId"), Mission.MissionId.ToString(EGuidFormats::DigitsWithHyphensLower));
			MissionObject->SetStringField(TEXT("contactId"), Mission.ContactId.ToString(EGuidFormats::DigitsWithHyphensLower));
			MissionObject->SetStringField(TEXT("missionRuleId"), Mission.MissionRuleId.ToString());
			MissionObject->SetStringField(TEXT("targetBaseId"), Mission.TargetBaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			MissionObject->SetStringField(TEXT("startedUtc"), Mission.StartedUtc.ToIso8601());
			AdversaryMissions.Add(MakeShared<FJsonValueObject>(MissionObject));
		}
		Object->SetArrayField(TEXT("adversaryMissions"), AdversaryMissions);
		TArray<TSharedPtr<FJsonValue>> BaseAssaults;
		for (const FBaseAssaultState& Assault : State.BaseAssaults)
		{
			const TSharedRef<FJsonObject> AssaultObject = MakeShared<FJsonObject>();
			AssaultObject->SetStringField(TEXT("assaultId"), Assault.AssaultId.ToString(EGuidFormats::DigitsWithHyphensLower));
			AssaultObject->SetStringField(TEXT("missionId"), Assault.MissionId.ToString(EGuidFormats::DigitsWithHyphensLower));
			AssaultObject->SetStringField(TEXT("contactId"), Assault.ContactId.ToString(EGuidFormats::DigitsWithHyphensLower));
			AssaultObject->SetStringField(TEXT("baseId"), Assault.BaseId.ToString(EGuidFormats::DigitsWithHyphensLower));
			AssaultObject->SetStringField(TEXT("arrivedUtc"), Assault.ArrivedUtc.ToIso8601());
			BaseAssaults.Add(MakeShared<FJsonValueObject>(AssaultObject));
		}
		Object->SetArrayField(TEXT("baseAssaults"), BaseAssaults);
		Object->SetStringField(TEXT("outcome"), CampaignOutcomeToString(State.Outcome));
		Object->SetStringField(TEXT("outcomeReasonId"), State.OutcomeReasonId.IsNone() ? FString() : State.OutcomeReasonId.ToString());
		return Object;
	}

	bool ReadHeader(
		const TSharedPtr<FJsonObject>& Object,
		FCampaignSaveHeader& Header,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		WarnUnknownFields(Object, { TEXT("formatVersion"), TEXT("campaignId"), TEXT("createdUtc"), TEXT("lastSavedUtc"), TEXT("buildVersion"), TEXT("contentPackages"), TEXT("contentFingerprint"), TEXT("saveChecksum") }, TEXT("save.header"), Diagnostics);

		bool bValid = ReadInt32(Object, TEXT("formatVersion"), Header.FormatVersion, TEXT("save.header"), Diagnostics);
		FString CampaignId;
		bValid &= ReadRequiredString(Object, TEXT("campaignId"), CampaignId, TEXT("save.header"), Diagnostics);
		if (!CampaignId.IsEmpty() && !FGuid::Parse(CampaignId, Header.CampaignId))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), TEXT("save.header.campaignId must be a GUID."));
			bValid = false;
		}
		bValid &= ReadIsoDate(Object, TEXT("createdUtc"), Header.CreatedUtc, TEXT("save.header"), Diagnostics);
		bValid &= ReadIsoDate(Object, TEXT("lastSavedUtc"), Header.LastSavedUtc, TEXT("save.header"), Diagnostics);
		bValid &= ReadRequiredString(Object, TEXT("buildVersion"), Header.BuildVersion, TEXT("save.header"), Diagnostics);
		bValid &= ReadContentPackages(Object, Header.ContentPackages, Diagnostics);
		bValid &= ReadRequiredString(Object, TEXT("contentFingerprint"), Header.ContentFingerprint, TEXT("save.header"), Diagnostics);
		if (Header.FormatVersion >= 2)
		{
			bValid &= ReadRequiredString(Object, TEXT("saveChecksum"), Header.SaveChecksum, TEXT("save.header"), Diagnostics);
		}
		else
		{
			bValid &= ReadOptionalString(Object, TEXT("saveChecksum"), Header.SaveChecksum, TEXT("save.header"), Diagnostics);
		}
		return bValid;
	}

	bool ReadState(
		const TSharedPtr<FJsonObject>& Object,
		const int32 FormatVersion,
		FCampaignState& State,
		TArray<FCampaignSaveDiagnostic>& Diagnostics)
	{
		WarnUnknownFields(Object, { TEXT("strategicUtc"), TEXT("randomInitialSeed"), TEXT("randomDrawCount"), TEXT("randomState"), TEXT("funds"), TEXT("campaignScore"), TEXT("difficulty"), TEXT("commandSequence"), TEXT("completedResearch"), TEXT("monthlyFunding"), TEXT("bases"), TEXT("mutualAidConvoys"), TEXT("researchProjects"), TEXT("manufacturingProjects"), TEXT("facilityConstructionProjects"), TEXT("personnel"), TEXT("personnelSquadBonds"), TEXT("recruitmentOrders"), TEXT("memorial"), TEXT("craft"), TEXT("craftAcquisitionOrders"), TEXT("strategicContacts"), TEXT("strategicSites"), TEXT("tacticalOperations"), TEXT("tacticalBattles"), TEXT("adversaryEscalationLevel"), TEXT("nextAdversaryMissionSeconds"), TEXT("nextAdversaryMissionSerial"), TEXT("adversaryMissionsLaunched"), TEXT("adversaryMissionsEscaped"), TEXT("adversaryMissionsThwarted"), TEXT("regionalPressure"), TEXT("regionalMandates"), TEXT("horizonCompactRatified"), TEXT("lastCoalitionAidMonth"), TEXT("lastCoalitionEmergencyVoteMonth"), TEXT("adversaryMissions"), TEXT("baseAssaults"), TEXT("outcome"), TEXT("outcomeReasonId") }, TEXT("save.state"), Diagnostics);

		bool bValid = ReadIsoDate(Object, TEXT("strategicUtc"), State.StrategicTime.Utc, TEXT("save.state"), Diagnostics);
		int64 InitialSeed = 0;
		int64 DrawCount = 0;
		uint64 RandomState = 0;
		bValid &= ReadInt64String(Object, TEXT("randomInitialSeed"), InitialSeed, TEXT("save.state"), Diagnostics);
		bValid &= ReadInt64String(Object, TEXT("randomDrawCount"), DrawCount, TEXT("save.state"), Diagnostics);
		bValid &= ReadUInt64Hex(Object, TEXT("randomState"), RandomState, TEXT("save.state"), Diagnostics);
		bValid &= ReadInt64String(Object, TEXT("funds"), State.Funds, TEXT("save.state"), Diagnostics);

		if (FormatVersion >= 2)
		{
			bValid &= ReadInt64String(Object, TEXT("campaignScore"), State.CampaignScore, TEXT("save.state"), Diagnostics);
			FString Difficulty;
			bValid &= ReadRequiredString(Object, TEXT("difficulty"), Difficulty, TEXT("save.state"), Diagnostics);
			if (!Difficulty.IsEmpty() && !TryParseDifficulty(Difficulty, State.Difficulty))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), TEXT("save.state.difficulty is unknown."));
				bValid = false;
			}
			bValid &= ReadInt64String(Object, TEXT("commandSequence"), State.CommandSequence, TEXT("save.state"), Diagnostics);
			bValid &= ReadNameArray(Object, TEXT("completedResearch"), State.CompletedResearch, TEXT("save.state"), Diagnostics, true);
		}
		else
		{
			ReadInt64String(Object, TEXT("campaignScore"), State.CampaignScore, TEXT("save.state"), Diagnostics, false);
			FString Difficulty;
			if (Object->HasField(TEXT("difficulty")))
			{
				bValid &= ReadRequiredString(Object, TEXT("difficulty"), Difficulty, TEXT("save.state"), Diagnostics);
				if (!Difficulty.IsEmpty() && !TryParseDifficulty(Difficulty, State.Difficulty))
				{
					AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), TEXT("save.state.difficulty is unknown."));
					bValid = false;
				}
			}
			bValid &= ReadInt64String(Object, TEXT("commandSequence"), State.CommandSequence, TEXT("save.state"), Diagnostics, false);
			bValid &= ReadNameArray(Object, TEXT("completedResearch"), State.CompletedResearch, TEXT("save.state"), Diagnostics, false);
		}

		if (FormatVersion >= 3)
		{
			bValid &= ReadInt64String(Object, TEXT("monthlyFunding"), State.MonthlyFunding, TEXT("save.state"), Diagnostics);
			bValid &= ReadBases(Object, State.Bases, FormatVersion, Diagnostics);
			if (FormatVersion >= 36)
			{
				bValid &= ReadMutualAidConvoys(
					Object, State.MutualAidConvoys, FormatVersion, Diagnostics);
			}
			bValid &= ReadResearchProjects(Object, State.ResearchProjects, Diagnostics);
		}
		if (FormatVersion >= 4)
		{
			bValid &= ReadManufacturingProjects(Object, State.ManufacturingProjects, Diagnostics);
		}
		if (FormatVersion >= 5)
		{
			bValid &= ReadFacilityConstructionProjects(Object, State.FacilityConstructionProjects, Diagnostics);
		}
		if (FormatVersion >= 6)
		{
			bValid &= ReadPersonnel(Object, State.Personnel, FormatVersion, Diagnostics);
			if (FormatVersion >= 33)
			{
				bValid &= ReadPersonnelSquadBonds(Object, State.PersonnelSquadBonds, Diagnostics);
			}
			bValid &= ReadRecruitmentOrders(Object, State.RecruitmentOrders, Diagnostics);
			bValid &= ReadMemorial(Object, State.Memorial, FormatVersion, Diagnostics);
		}
		if (FormatVersion >= 7)
		{
			bValid &= ReadCraft(Object, State.Craft, FormatVersion, Diagnostics);
			bValid &= ReadCraftAcquisitionOrders(Object, State.CraftAcquisitionOrders, Diagnostics);
		}
		if (FormatVersion >= 8)
		{
			bValid &= ReadStrategicContacts(Object, State.StrategicContacts, FormatVersion, Diagnostics);
		}
		if (FormatVersion >= 9)
		{
			bValid &= ReadStrategicSites(Object, State.StrategicSites, Diagnostics);
		}
		if (FormatVersion >= 10)
		{
			bValid &= ReadInt32(Object, TEXT("adversaryEscalationLevel"), State.AdversaryEscalationLevel, TEXT("save.state"), Diagnostics);
			bValid &= ReadInt64String(Object, TEXT("nextAdversaryMissionSeconds"), State.NextAdversaryMissionSeconds, TEXT("save.state"), Diagnostics);
			bValid &= ReadInt64String(Object, TEXT("nextAdversaryMissionSerial"), State.NextAdversaryMissionSerial, TEXT("save.state"), Diagnostics);
			bValid &= ReadInt32(Object, TEXT("adversaryMissionsLaunched"), State.AdversaryMissionsLaunched, TEXT("save.state"), Diagnostics);
			bValid &= ReadInt32(Object, TEXT("adversaryMissionsEscaped"), State.AdversaryMissionsEscaped, TEXT("save.state"), Diagnostics);
			bValid &= ReadInt32(Object, TEXT("adversaryMissionsThwarted"), State.AdversaryMissionsThwarted, TEXT("save.state"), Diagnostics);
			bValid &= ReadRegionalPressure(Object, State.RegionalPressure, Diagnostics);
			if (FormatVersion >= 23 || Object->HasField(TEXT("regionalMandates")))
			{
				bValid &= ReadRegionalMandates(Object, State.RegionalMandates, FormatVersion, Diagnostics);
			}
			if (FormatVersion >= 29)
			{
				bValid &= ReadBool(Object, TEXT("horizonCompactRatified"),
					State.bHorizonCompactRatified, TEXT("save.state"), Diagnostics);
			}
			if (FormatVersion >= 30)
			{
				bValid &= ReadInt32(Object, TEXT("lastCoalitionAidMonth"),
					State.LastCoalitionAidMonth, TEXT("save.state"), Diagnostics);
			}
			if (FormatVersion >= 32)
			{
				bValid &= ReadInt32(Object, TEXT("lastCoalitionEmergencyVoteMonth"),
					State.LastCoalitionEmergencyVoteMonth, TEXT("save.state"), Diagnostics);
			}
			bValid &= ReadAdversaryMissions(Object, State.AdversaryMissions, FormatVersion, Diagnostics);
			FString Outcome;
			bValid &= ReadRequiredString(Object, TEXT("outcome"), Outcome, TEXT("save.state"), Diagnostics);
			if (!Outcome.IsEmpty() && !TryParseCampaignOutcome(Outcome, State.Outcome))
			{
				AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_value"), TEXT("save.state.outcome is unknown."));
				bValid = false;
			}
			FString OutcomeReasonId;
			bValid &= ReadRequiredString(Object, TEXT("outcomeReasonId"), OutcomeReasonId, TEXT("save.state"), Diagnostics, true);
			State.OutcomeReasonId = OutcomeReasonId.IsEmpty() ? NAME_None : FName(*OutcomeReasonId);
		}
		if (FormatVersion >= 20)
		{
			bValid &= ReadBaseAssaults(Object, State.BaseAssaults, Diagnostics);
		}
		if (FormatVersion >= 11)
		{
			bValid &= ReadTacticalOperations(Object, State.TacticalOperations, FormatVersion, Diagnostics);
		}
		if (FormatVersion >= 12)
		{
			bValid &= ReadTacticalBattles(Object, State.TacticalBattles, FormatVersion, Diagnostics);
		}

		if (bValid && !State.SimulationRandom.RestoreFromSave(InitialSeed, DrawCount, RandomState))
		{
			AddDiagnostic(Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_random_state"), TEXT("save.state contains an impossible deterministic random snapshot."));
			bValid = false;
		}
		return bValid;
	}

	FCampaignSaveReadResult DeserializeInternal(
		const FString& Json,
		const TArray<FCampaignContentVersion>* ExpectedContentPackages)
	{
		FCampaignSaveReadResult Result;
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_json"), FString::Printf(TEXT("Campaign save is not valid JSON: %s"), *Reader->GetErrorMessage()));
			return Result;
		}

		WarnUnknownFields(Root, { TEXT("header"), TEXT("state") }, TEXT("save"), Result.Diagnostics);
		const TSharedPtr<FJsonObject>* HeaderObject = nullptr;
		const TSharedPtr<FJsonObject>* StateObject = nullptr;
		if (!Root->TryGetObjectField(TEXT("header"), HeaderObject) || HeaderObject == nullptr || !HeaderObject->IsValid())
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.header must be an object."));
		}
		if (!Root->TryGetObjectField(TEXT("state"), StateObject) || StateObject == nullptr || !StateObject->IsValid())
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_field_type"), TEXT("save.state must be an object."));
		}
		if (HasErrors(Result.Diagnostics))
		{
			return Result;
		}

		ReadHeader(*HeaderObject, Result.Envelope.Header, Result.Diagnostics);
		if (Result.Envelope.Header.FormatVersion >= FCampaignSaveCodec::OldestSupportedFormatVersion
			&& Result.Envelope.Header.FormatVersion <= FCampaignSaveCodec::CurrentFormatVersion)
		{
			ReadState(*StateObject, Result.Envelope.Header.FormatVersion, Result.Envelope.State, Result.Diagnostics);
		}
		else
		{
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("unsupported_format_version"), FString::Printf(TEXT("Campaign save format %d is unsupported."), Result.Envelope.Header.FormatVersion));
		}
		if (HasErrors(Result.Diagnostics))
		{
			return Result;
		}

		NormalizeEnvelope(Result.Envelope);
		FCampaignSaveValidationResult Validation = ValidateInternal(
			Result.Envelope,
			ExpectedContentPackages,
			Result.Envelope.Header.FormatVersion < FCampaignSaveCodec::CurrentFormatVersion);
		Result.Diagnostics.Append(Validation.Diagnostics);
		if (!Validation.bSucceeded)
		{
			return Result;
		}

		if (Result.Envelope.Header.FormatVersion < FCampaignSaveCodec::CurrentFormatVersion)
		{
			if (Result.Envelope.Header.FormatVersion < 23
				&& !MigrateRegionalMandates(Result.Envelope.State))
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_regional_mandate"),
					TEXT("Legacy regional pressure could not be migrated into non-negative mandate funding."));
				return Result;
			}
			if (Result.Envelope.Header.FormatVersion < 24
				&& !MigratePersonnelProgression(Result.Envelope.State))
			{
				AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("invalid_personnel_progression"),
					TEXT("Legacy personnel service records could not be migrated into doctrine choices."));
				return Result;
			}
			if (Result.Envelope.Header.FormatVersion < 34)
			{
				MigratePersonnelRecoveryPlans(Result.Envelope.State);
			}
			if (Result.Envelope.Header.FormatVersion < 35)
			{
				MigratePersonnelStewardship(Result.Envelope.State);
			}
			if (Result.Envelope.Header.FormatVersion < 36)
			{
				MigrateMutualAidConvoys(Result.Envelope.State);
			}
			if (Result.Envelope.Header.FormatVersion < 37)
			{
				MigrateMutualAidRouting(Result.Envelope.State);
			}
			if (Result.Envelope.Header.FormatVersion < 38)
			{
				MigrateMutualAidRelayQueue(Result.Envelope.State);
			}
			if (Result.Envelope.Header.FormatVersion < 39)
			{
				MigrateSignalWatchStaffing(Result.Envelope.State);
			}
			if (Result.Envelope.Header.FormatVersion < 40)
			{
				MigrateMutualAidRelayWaypoints(Result.Envelope.State);
			}
			if (Result.Envelope.Header.FormatVersion < 41)
			{
				MigrateMutualAidBalancedHandoffs(Result.Envelope.State);
			}
			if (Result.Envelope.Header.FormatVersion < 42)
			{
				MigrateWorksCadreStaffing(Result.Envelope.State);
			}
			if (Result.Envelope.Header.FormatVersion < 43)
			{
				MigrateWorksCadreCharters(Result.Envelope.State);
			}
			NormalizeEnvelope(Result.Envelope);
			Result.Envelope.Header.FormatVersion = FCampaignSaveCodec::CurrentFormatVersion;
			Result.Envelope.Header.ContentFingerprint = FCampaignSaveCodec::ComputeContentFingerprint(Result.Envelope.Header.ContentPackages);
			Result.Envelope.Header.SaveChecksum = ComputeSaveChecksum(Result.Envelope);
			Result.bMigrated = true;
			AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Warning, TEXT("save_migrated"), TEXT("Campaign save was migrated to the current in-memory format."));
		}

		Result.bSucceeded = true;
		return Result;
	}
}

bool FCampaignSaveValidationResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate([Code](const FCampaignSaveDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

bool FCampaignSaveWriteResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate([Code](const FCampaignSaveDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

bool FCampaignSaveReadResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate([Code](const FCampaignSaveDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

FCampaignSaveEnvelope FCampaignSaveCodec::CreateNew(
	const FCampaignState& State,
	const TArray<FCampaignContentVersion>& ContentPackages,
	const FString& BuildVersion,
	const FDateTime& WallClockUtc,
	FGuid CampaignId)
{
	using namespace CampaignSavePrivate;

	FCampaignSaveEnvelope Envelope;
	Envelope.Header.FormatVersion = CurrentFormatVersion;
	Envelope.Header.CampaignId = CampaignId.IsValid() ? CampaignId : FGuid::NewGuid();
	Envelope.Header.CreatedUtc = WallClockUtc;
	Envelope.Header.LastSavedUtc = WallClockUtc;
	Envelope.Header.BuildVersion = BuildVersion;
	Envelope.Header.ContentPackages = ContentPackages;
	Envelope.State = State;
	NormalizeEnvelope(Envelope);
	Envelope.Header.ContentFingerprint = ComputeContentFingerprint(Envelope.Header.ContentPackages);
	Envelope.Header.SaveChecksum = ComputeSaveChecksum(Envelope);
	return Envelope;
}

FCampaignSaveWriteResult FCampaignSaveCodec::Serialize(const FCampaignSaveEnvelope& InputEnvelope)
{
	using namespace CampaignSavePrivate;

	FCampaignSaveWriteResult Result;
	Result.Envelope = InputEnvelope;
	if (Result.Envelope.Header.FormatVersion != CurrentFormatVersion)
	{
		AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("unsupported_write_version"), FString::Printf(TEXT("Only current campaign-save format %d can be written."), CurrentFormatVersion));
		return Result;
	}

	NormalizeEnvelope(Result.Envelope);
	Result.Envelope.Header.ContentFingerprint = ComputeContentFingerprint(Result.Envelope.Header.ContentPackages);
	Result.Envelope.Header.SaveChecksum = ComputeSaveChecksum(Result.Envelope);
	const FCampaignSaveValidationResult Validation = Validate(Result.Envelope);
	Result.Diagnostics = Validation.Diagnostics;
	if (!Validation.bSucceeded)
	{
		return Result;
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetObjectField(TEXT("header"), MakeHeaderJson(Result.Envelope.Header));
	Root->SetObjectField(TEXT("state"), MakeStateJson(Result.Envelope.State));
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result.Json);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		AddDiagnostic(Result.Diagnostics, ECampaignSaveDiagnosticSeverity::Error, TEXT("serialization_failed"), TEXT("Could not serialize campaign save JSON."));
		Result.Json.Reset();
		return Result;
	}

	Result.bSucceeded = true;
	return Result;
}

FCampaignSaveReadResult FCampaignSaveCodec::Deserialize(const FString& Json)
{
	return CampaignSavePrivate::DeserializeInternal(Json, nullptr);
}

FCampaignSaveReadResult FCampaignSaveCodec::Deserialize(
	const FString& Json,
	const TArray<FCampaignContentVersion>& ExpectedContentPackages)
{
	return CampaignSavePrivate::DeserializeInternal(Json, &ExpectedContentPackages);
}

FCampaignSaveValidationResult FCampaignSaveCodec::Validate(const FCampaignSaveEnvelope& Envelope)
{
	return CampaignSavePrivate::ValidateInternal(Envelope, nullptr, false);
}

FCampaignSaveValidationResult FCampaignSaveCodec::Validate(
	const FCampaignSaveEnvelope& Envelope,
	const TArray<FCampaignContentVersion>& ExpectedContentPackages)
{
	return CampaignSavePrivate::ValidateInternal(Envelope, &ExpectedContentPackages, false);
}

FString FCampaignSaveCodec::ComputeContentFingerprint(const TArray<FCampaignContentVersion>& ContentPackages)
{
	return CampaignSavePrivate::HashString(CampaignSavePrivate::BuildContentCanonical(ContentPackages));
}

FString FCampaignSaveCodec::ComputeEnvelopeChecksum(const FCampaignSaveEnvelope& Envelope)
{
	return CampaignSavePrivate::ComputeSaveChecksum(Envelope);
}
