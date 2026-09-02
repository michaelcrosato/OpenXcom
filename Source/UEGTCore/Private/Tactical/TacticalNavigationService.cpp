// Copyright 2026 UEGT contributors. MIT License.

#include "Tactical/TacticalNavigationService.h"

#include "Algo/Reverse.h"

namespace TacticalNavigationPrivate
{
	struct FOpenNode
	{
		int32 CellIndex = INDEX_NONE;
		int32 Cost = 0;
	};

	void AddDiagnostic(TArray<FTacticalNavigationDiagnostic>& Diagnostics, const FName Code, FString Message)
	{
		FTacticalNavigationDiagnostic& Diagnostic = Diagnostics.AddDefaulted_GetRef();
		Diagnostic.Code = Code;
		Diagnostic.Message = MoveTemp(Message);
	}

	bool ValidateGrid(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		TArray<FTacticalNavigationDiagnostic>& Diagnostics)
	{
		const int64 CellCount = static_cast<int64>(Battle.Width) * Battle.Height * Battle.Levels;
		if (Battle.Width <= 0 || Battle.Height <= 0 || Battle.Levels <= 0
			|| Battle.Levels > 4 || CellCount > 8192 || Battle.Cells.Num() != CellCount)
		{
			AddDiagnostic(Diagnostics, TEXT("invalid_tactical_grid"), TEXT("Tactical grid dimensions and cell population are inconsistent."));
			return false;
		}
		const int32 LayerArea = Battle.Width * Battle.Height;
		for (int32 Index = 0; Index < Battle.Cells.Num(); ++Index)
		{
			const FTacticalCellState& Cell = Battle.Cells[Index];
			const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
			const int32 ExpectedZ = Index / LayerArea;
			const int32 WithinLayer = Index % LayerArea;
			if (Cell.X != WithinLayer % Battle.Width || Cell.Y != WithinLayer / Battle.Width || Cell.Z != ExpectedZ
				|| Terrain == nullptr || Cell.CurrentIntegrity < 0
				|| (Terrain != nullptr && Cell.CurrentIntegrity > Terrain->MaxIntegrity)
				|| Cell.Smoke < 0 || Cell.Smoke > 100 || Cell.Fire < 0 || Cell.Fire > 100
				|| (Cell.bDoorOpen && (Terrain == nullptr || !Terrain->IsDoor() || Cell.CurrentIntegrity <= 0)))
			{
				AddDiagnostic(Diagnostics, TEXT("invalid_tactical_grid"), TEXT("Tactical grid cells must be level-major row-major and reference valid terrain state."));
				return false;
			}
		}
		return true;
	}

	const FTacticalUnitState* FindUnit(const FTacticalBattleState& Battle, const FGuid& UnitId)
	{
		return Battle.Units.FindByPredicate(
			[&UnitId](const FTacticalUnitState& Unit) { return Unit.UnitId == UnitId; });
	}

	bool ComesBefore(const FOpenNode& Left, const FOpenNode& Right)
	{
		return Left.Cost != Right.Cost ? Left.Cost < Right.Cost : Left.CellIndex < Right.CellIndex;
	}

	void HeapPush(TArray<FOpenNode>& Heap, const FOpenNode Node)
	{
		int32 Index = Heap.Add(Node);
		while (Index > 0)
		{
			const int32 Parent = (Index - 1) / 2;
			if (!ComesBefore(Heap[Index], Heap[Parent]))
			{
				break;
			}
			Heap.Swap(Index, Parent);
			Index = Parent;
		}
	}

	bool HeapPop(TArray<FOpenNode>& Heap, FOpenNode& OutNode)
	{
		if (Heap.IsEmpty())
		{
			return false;
		}
		OutNode = Heap[0];
		const FOpenNode Tail = Heap.Pop(EAllowShrinking::No);
		if (Heap.IsEmpty())
		{
			return true;
		}
		Heap[0] = Tail;
		int32 Index = 0;
		while (true)
		{
			const int32 LeftChild = Index * 2 + 1;
			if (LeftChild >= Heap.Num())
			{
				break;
			}
			const int32 RightChild = LeftChild + 1;
			int32 EarlierChild = LeftChild;
			if (RightChild < Heap.Num() && ComesBefore(Heap[RightChild], Heap[LeftChild]))
			{
				EarlierChild = RightChild;
			}
			if (!ComesBefore(Heap[EarlierChild], Heap[Index]))
			{
				break;
			}
			Heap.Swap(Index, EarlierChild);
			Index = EarlierChild;
		}
		return true;
	}

	bool BlocksMovement(const FTacticalCellState& Cell, const FResolvedRuleSet& Rules)
	{
		const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
		return Terrain == nullptr || (Terrain->bBlocksMovement && Cell.CurrentIntegrity > 0 && !Cell.bDoorOpen);
	}

	bool BlocksVision(const FTacticalCellState& Cell, const FResolvedRuleSet& Rules)
	{
		const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
		return Terrain == nullptr || (Terrain->bBlocksVision && Cell.CurrentIntegrity > 0 && !Cell.bDoorOpen);
	}

	int32 RoundRatio(const int64 Numerator, const int32 Denominator)
	{
		check(Denominator > 0);
		return Numerator >= 0
			? static_cast<int32>((Numerator + Denominator / 2) / Denominator)
			: -static_cast<int32>((-Numerator + Denominator / 2) / Denominator);
	}

	template <typename AllocatorType>
	void BuildTraceIndices(
		const FTacticalBattleState& Battle,
		const int32 OriginX,
		const int32 OriginY,
		const int32 OriginZ,
		const int32 TargetX,
		const int32 TargetY,
		const int32 TargetZ,
		TArray<int32, AllocatorType>& OutIndices)
	{
		OutIndices.Reset();
		if (OriginZ == TargetZ)
		{
			int32 X = OriginX;
			int32 Y = OriginY;
			const int32 DeltaX = FMath::Abs(TargetX - OriginX);
			const int32 StepX = OriginX < TargetX ? 1 : -1;
			const int32 DeltaY = FMath::Abs(TargetY - OriginY);
			const int32 StepY = OriginY < TargetY ? 1 : -1;
			int32 Error = DeltaX - DeltaY;
			while (true)
			{
				OutIndices.Add(Battle.GetCellIndex(X, Y, OriginZ));
				if (X == TargetX && Y == TargetY)
				{
					return;
				}
				const int32 TwiceError = 2 * Error;
				if (TwiceError > -DeltaY)
				{
					Error -= DeltaY;
					X += StepX;
				}
				if (TwiceError < DeltaX)
				{
					Error += DeltaX;
					Y += StepY;
				}
			}
		}

		const int32 DeltaX = TargetX - OriginX;
		const int32 DeltaY = TargetY - OriginY;
		const int32 DeltaZ = TargetZ - OriginZ;
		const int32 StepCount = FMath::Max3(FMath::Abs(DeltaX), FMath::Abs(DeltaY), FMath::Abs(DeltaZ));
		for (int32 Step = 0; Step <= StepCount; ++Step)
		{
			const int32 X = OriginX + RoundRatio(static_cast<int64>(DeltaX) * Step, StepCount);
			const int32 Y = OriginY + RoundRatio(static_cast<int64>(DeltaY) * Step, StepCount);
			const int32 Z = OriginZ + RoundRatio(static_cast<int64>(DeltaZ) * Step, StepCount);
			const int32 Index = Battle.GetCellIndex(X, Y, Z);
			if (OutIndices.IsEmpty() || OutIndices.Last() != Index)
			{
				OutIndices.Add(Index);
			}
		}
	}

	bool TraceLineOfSight(
		const FTacticalBattleState& Battle,
		const FResolvedRuleSet& Rules,
		const int32 OriginX,
		const int32 OriginY,
		const int32 OriginZ,
		const int32 TargetX,
		const int32 TargetY,
		const int32 TargetZ)
	{
		TArray<int32, TInlineAllocator<64>> Trace;
		BuildTraceIndices(Battle, OriginX, OriginY, OriginZ, TargetX, TargetY, TargetZ, Trace);
		for (int32 TraceIndex = 1; TraceIndex + 1 < Trace.Num(); ++TraceIndex)
		{
			if (BlocksVision(Battle.Cells[Trace[TraceIndex]], Rules))
			{
				return false;
			}
		}
		return true;
	}

	int32 TraceSmokeObscuration(
		const FTacticalBattleState& Battle,
		const int32 OriginX,
		const int32 OriginY,
		const int32 OriginZ,
		const int32 TargetX,
		const int32 TargetY,
		const int32 TargetZ)
	{
		if (OriginX == TargetX && OriginY == TargetY && OriginZ == TargetZ)
		{
			return 0;
		}
		TArray<int32, TInlineAllocator<64>> Trace;
		BuildTraceIndices(Battle, OriginX, OriginY, OriginZ, TargetX, TargetY, TargetZ, Trace);
		int32 Obscuration = Battle.Cells[Trace[0]].Smoke * 3 / 8;
		for (int32 TraceIndex = 1; TraceIndex < Trace.Num(); ++TraceIndex)
		{
			Obscuration = FMath::Min(100, Obscuration + Battle.Cells[Trace[TraceIndex]].Smoke * 3 / 4);
		}
		return Obscuration;
	}

	int32 VisionRangeFor(const FTacticalUnitState& Unit)
	{
		return FMath::Clamp(5 + Unit.Resolve / 12, 6, 14);
	}
}

bool FTacticalPathResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate(
		[Code](const FTacticalNavigationDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

bool FTacticalReachabilityResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate(
		[Code](const FTacticalNavigationDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

bool FTacticalVisibilityResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate(
		[Code](const FTacticalNavigationDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

bool FTacticalVisibilityResult::IsCellVisible(const int32 X, const int32 Y, const int32 Z) const
{
	return X >= 0 && X < Width && Y >= 0 && Y < Height && Z >= 0 && Z < Levels
		&& VisibleCellIndices.Contains((Z * Height + Y) * Width + X);
}

bool FTacticalVisibilityResult::IsUnitVisible(const FGuid UnitId) const
{
	return VisibleUnitIds.Contains(UnitId);
}

bool FTacticalThrowTrajectoryResult::HasDiagnostic(const FName Code) const
{
	return Diagnostics.ContainsByPredicate(
		[Code](const FTacticalNavigationDiagnostic& Diagnostic) { return Diagnostic.Code == Code; });
}

FTacticalPathResult FTacticalNavigationService::FindPath(
	const FTacticalBattleState& Battle,
	const FResolvedRuleSet& Rules,
	const FGuid UnitId,
	const int32 DestinationX,
	const int32 DestinationY,
	const int32 DestinationZ)
{
	using namespace TacticalNavigationPrivate;

	FTacticalPathResult Result;
	if (!ValidateGrid(Battle, Rules, Result.Diagnostics))
	{
		return Result;
	}
	const FTacticalUnitState* Unit = FindUnit(Battle, UnitId);
	if (Unit == nullptr)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("unknown_tactical_unit"), TEXT("Tactical path query references an unknown unit."));
		return Result;
	}
	const int32 StanceMoveSurcharge = Unit->Stance == ETacticalStance::Crouched ? 1 : 0;
	if (!Unit->UnitId.IsValid() || Unit->CurrentHealth <= 0 || Unit->bExtracted
		|| !Battle.IsWithinGrid(Unit->X, Unit->Y, Unit->Z))
	{
		AddDiagnostic(Result.Diagnostics, TEXT("invalid_tactical_unit"), TEXT("Tactical path query requires a living, deployed unit on the grid."));
		return Result;
	}
	if (!Battle.IsWithinGrid(DestinationX, DestinationY, DestinationZ))
	{
		AddDiagnostic(Result.Diagnostics, TEXT("invalid_tactical_destination"), TEXT("Tactical destination is outside the battlefield."));
		return Result;
	}

	const int32 StartIndex = Battle.GetCellIndex(Unit->X, Unit->Y, Unit->Z);
	const int32 GoalIndex = Battle.GetCellIndex(DestinationX, DestinationY, DestinationZ);
	if (StartIndex == GoalIndex)
	{
		Result.bSucceeded = true;
		return Result;
	}
	if (BlocksMovement(Battle.Cells[GoalIndex], Rules))
	{
		AddDiagnostic(Result.Diagnostics, TEXT("blocked_tactical_destination"), TEXT("Tactical destination is blocked by intact terrain."));
		return Result;
	}

	TBitArray<> Occupied(false, Battle.Cells.Num());
	for (const FTacticalUnitState& Other : Battle.Units)
	{
		if (Other.UnitId == UnitId || Other.CurrentHealth <= 0 || Other.bExtracted
			|| !Battle.IsWithinGrid(Other.X, Other.Y, Other.Z))
		{
			continue;
		}
		Occupied[Battle.GetCellIndex(Other.X, Other.Y, Other.Z)] = true;
	}
	if (Occupied[GoalIndex])
	{
		AddDiagnostic(Result.Diagnostics, TEXT("occupied_tactical_destination"), TEXT("Tactical destination is occupied by another unit."));
		return Result;
	}

	TArray<int32> Distances;
	Distances.Init(MAX_int32, Battle.Cells.Num());
	TArray<int32> Previous;
	Previous.Init(INDEX_NONE, Battle.Cells.Num());
	TBitArray<> Visited(false, Battle.Cells.Num());
	TArray<FOpenNode> Open;
	Distances[StartIndex] = 0;
	HeapPush(Open, { StartIndex, 0 });
	while (!Open.IsEmpty())
	{
		FOpenNode Current;
		HeapPop(Open, Current);
		if (Visited[Current.CellIndex] || Current.Cost != Distances[Current.CellIndex])
		{
			continue;
		}
		Visited[Current.CellIndex] = true;
		if (Current.CellIndex == GoalIndex)
		{
			break;
		}

		const FTacticalCellState& CurrentCell = Battle.Cells[Current.CellIndex];
		const FTacticalTerrainRule* CurrentTerrain = Rules.TacticalTerrains.Find(CurrentCell.TerrainRuleId);
		check(CurrentTerrain != nullptr);
		static constexpr int32 OffsetX[] = { -1, 1, 0, 0, 0, 0 };
		static constexpr int32 OffsetY[] = { 0, 0, -1, 1, 0, 0 };
		static constexpr int32 OffsetZ[] = { 0, 0, 0, 0, -1, 1 };
		for (int32 Direction = 0; Direction < 6; ++Direction)
		{
			const int32 NextX = CurrentCell.X + OffsetX[Direction];
			const int32 NextY = CurrentCell.Y + OffsetY[Direction];
			const int32 NextZ = CurrentCell.Z + OffsetZ[Direction];
			if (!Battle.IsWithinGrid(NextX, NextY, NextZ))
			{
				continue;
			}
			const int32 NextIndex = Battle.GetCellIndex(NextX, NextY, NextZ);
			if (Visited[NextIndex] || Occupied[NextIndex] || BlocksMovement(Battle.Cells[NextIndex], Rules))
			{
				continue;
			}
			const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Battle.Cells[NextIndex].TerrainRuleId);
			check(Terrain != nullptr);
			const bool bVertical = OffsetZ[Direction] != 0;
			if (bVertical && (!CurrentTerrain->IsVerticalConnector() || !Terrain->IsVerticalConnector()))
			{
				continue;
			}
			const int32 StepCost = bVertical
				? FMath::Max(CurrentTerrain->VerticalMoveCost, Terrain->VerticalMoveCost) + StanceMoveSurcharge
				: Terrain->MoveCost + StanceMoveSurcharge;
			const int32 NewCost = Current.Cost + StepCost;
			if (NewCost < Distances[NextIndex]
				|| (NewCost == Distances[NextIndex]
					&& (Previous[NextIndex] == INDEX_NONE || Current.CellIndex < Previous[NextIndex])))
			{
				Distances[NextIndex] = NewCost;
				Previous[NextIndex] = Current.CellIndex;
				HeapPush(Open, { NextIndex, NewCost });
			}
		}
	}

	if (Distances[GoalIndex] == MAX_int32)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("unreachable_tactical_destination"), TEXT("No traversable path reaches the tactical destination."));
		return Result;
	}
	for (int32 Index = GoalIndex; Index != StartIndex; Index = Previous[Index])
	{
		if (Index == INDEX_NONE || Previous[Index] == INDEX_NONE)
		{
			AddDiagnostic(Result.Diagnostics, TEXT("invalid_tactical_path"), TEXT("Tactical path predecessor chain is incomplete."));
			Result.Steps.Reset();
			return Result;
		}
		const FTacticalCellState& Cell = Battle.Cells[Index];
		FTacticalPathStep& Step = Result.Steps.AddDefaulted_GetRef();
		Step.X = Cell.X;
		Step.Y = Cell.Y;
		Step.Z = Cell.Z;
		Step.MoveCost = Distances[Index] - Distances[Previous[Index]];
	}
	Algo::Reverse(Result.Steps);
	Result.TotalCost = Distances[GoalIndex];
	Result.bSucceeded = true;
	return Result;
}

FTacticalReachabilityResult FTacticalNavigationService::ComputeReachableCells(
	const FTacticalBattleState& Battle,
	const FResolvedRuleSet& Rules,
	const FGuid UnitId,
	const int32 MaximumCost)
{
	using namespace TacticalNavigationPrivate;

	FTacticalReachabilityResult Result;
	if (!ValidateGrid(Battle, Rules, Result.Diagnostics))
	{
		return Result;
	}
	if (MaximumCost < 0 || MaximumCost > 1000)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("invalid_tactical_movement_budget"), TEXT("Tactical reachability requires an action-point budget from zero through 1000."));
		return Result;
	}
	const FTacticalUnitState* Unit = FindUnit(Battle, UnitId);
	if (Unit == nullptr)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("unknown_tactical_unit"), TEXT("Tactical reachability references an unknown unit."));
		return Result;
	}
	if (!Unit->UnitId.IsValid() || Unit->CurrentHealth <= 0 || Unit->bExtracted
		|| !Battle.IsWithinGrid(Unit->X, Unit->Y, Unit->Z))
	{
		AddDiagnostic(Result.Diagnostics, TEXT("invalid_tactical_unit"), TEXT("Tactical reachability requires a living, deployed unit on the grid."));
		return Result;
	}

	const int32 StartIndex = Battle.GetCellIndex(Unit->X, Unit->Y, Unit->Z);
	const int32 StanceMoveSurcharge = Unit->Stance == ETacticalStance::Crouched ? 1 : 0;
	TBitArray<> Occupied(false, Battle.Cells.Num());
	for (const FTacticalUnitState& Other : Battle.Units)
	{
		if (Other.UnitId == UnitId || Other.CurrentHealth <= 0 || Other.bExtracted
			|| !Battle.IsWithinGrid(Other.X, Other.Y, Other.Z))
		{
			continue;
		}
		Occupied[Battle.GetCellIndex(Other.X, Other.Y, Other.Z)] = true;
	}

	TArray<int32> Distances;
	Distances.Init(MAX_int32, Battle.Cells.Num());
	TBitArray<> Visited(false, Battle.Cells.Num());
	TArray<FOpenNode> Open;
	Distances[StartIndex] = 0;
	HeapPush(Open, { StartIndex, 0 });
	while (!Open.IsEmpty())
	{
		FOpenNode Current;
		HeapPop(Open, Current);
		if (Current.Cost > MaximumCost)
		{
			break;
		}
		if (Visited[Current.CellIndex] || Current.Cost != Distances[Current.CellIndex])
		{
			continue;
		}
		Visited[Current.CellIndex] = true;
		const FTacticalCellState& CurrentCell = Battle.Cells[Current.CellIndex];
		const FTacticalTerrainRule* CurrentTerrain = Rules.TacticalTerrains.Find(CurrentCell.TerrainRuleId);
		check(CurrentTerrain != nullptr);
		static constexpr int32 OffsetX[] = { -1, 1, 0, 0, 0, 0 };
		static constexpr int32 OffsetY[] = { 0, 0, -1, 1, 0, 0 };
		static constexpr int32 OffsetZ[] = { 0, 0, 0, 0, -1, 1 };
		for (int32 Direction = 0; Direction < 6; ++Direction)
		{
			const int32 NextX = CurrentCell.X + OffsetX[Direction];
			const int32 NextY = CurrentCell.Y + OffsetY[Direction];
			const int32 NextZ = CurrentCell.Z + OffsetZ[Direction];
			if (!Battle.IsWithinGrid(NextX, NextY, NextZ))
			{
				continue;
			}
			const int32 NextIndex = Battle.GetCellIndex(NextX, NextY, NextZ);
			if (Visited[NextIndex] || Occupied[NextIndex] || BlocksMovement(Battle.Cells[NextIndex], Rules))
			{
				continue;
			}
			const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Battle.Cells[NextIndex].TerrainRuleId);
			check(Terrain != nullptr);
			const bool bVertical = OffsetZ[Direction] != 0;
			if (bVertical && (!CurrentTerrain->IsVerticalConnector() || !Terrain->IsVerticalConnector()))
			{
				continue;
			}
			const int32 StepCost = bVertical
				? FMath::Max(CurrentTerrain->VerticalMoveCost, Terrain->VerticalMoveCost) + StanceMoveSurcharge
				: Terrain->MoveCost + StanceMoveSurcharge;
			const int32 NewCost = Current.Cost + StepCost;
			if (NewCost <= MaximumCost && NewCost < Distances[NextIndex])
			{
				Distances[NextIndex] = NewCost;
				HeapPush(Open, { NextIndex, NewCost });
			}
		}
	}

	for (int32 CellIndex = 0; CellIndex < Distances.Num(); ++CellIndex)
	{
		if (Distances[CellIndex] == MAX_int32 || Distances[CellIndex] > MaximumCost)
		{
			continue;
		}
		const FTacticalCellState& Cell = Battle.Cells[CellIndex];
		FTacticalReachableCell& Reachable = Result.Cells.AddDefaulted_GetRef();
		Reachable.CellIndex = CellIndex;
		Reachable.X = Cell.X;
		Reachable.Y = Cell.Y;
		Reachable.Z = Cell.Z;
		Reachable.TotalCost = Distances[CellIndex];
	}
	Result.bSucceeded = true;
	return Result;
}

FTacticalVisibilityResult FTacticalNavigationService::ComputePlayerVisibility(
	const FTacticalBattleState& Battle,
	const FResolvedRuleSet& Rules)

{
	return ComputeTeamVisibility(Battle, Rules, ETacticalTeam::Player);
}

FTacticalVisibilityResult FTacticalNavigationService::RefreshPlayerDiscovery(
	FTacticalBattleState& Battle,
	const FResolvedRuleSet& Rules)
{
	using namespace TacticalNavigationPrivate;

	FTacticalVisibilityResult Result = ComputePlayerVisibility(Battle, Rules);
	if (!Result.bSucceeded)
	{
		return Result;
	}

	int32 PreviousCellIndex = INDEX_NONE;
	for (const int32 CellIndex : Battle.PlayerDiscoveredCellIndices)
	{
		if (!Battle.Cells.IsValidIndex(CellIndex) || CellIndex <= PreviousCellIndex)
		{
			TacticalNavigationPrivate::AddDiagnostic(
				Result.Diagnostics,
				TEXT("invalid_tactical_discovery"),
				TEXT("Tactical discovery indices must be unique, sorted, and inside the grid."));
			Result.bSucceeded = false;
			return Result;
		}
		PreviousCellIndex = CellIndex;
	}

	FGuid PreviousLastKnownUnitId;
	bool bHasPreviousLastKnownUnitId = false;
	for (const FTacticalUnitMemoryState& Memory : Battle.PlayerLastKnownAdversaries)
	{
		const FTacticalUnitState* Unit = FindUnit(Battle, Memory.UnitId);
		const bool bKnownStance = Memory.Stance == ETacticalStance::Standing
			|| Memory.Stance == ETacticalStance::Crouched;
		const bool bSorted = !bHasPreviousLastKnownUnitId
			|| PreviousLastKnownUnitId.ToString(EGuidFormats::Digits) < Memory.UnitId.ToString(EGuidFormats::Digits);
		if (!Memory.UnitId.IsValid() || !bSorted || Unit == nullptr || Unit->Team != ETacticalTeam::Adversary)
		{
			AddDiagnostic(
				Result.Diagnostics,
				TEXT("invalid_tactical_memory"),
				TEXT("Tactical last-known adversary memory is sorted, bounded, and tied to a living adversary."));
			Result.bSucceeded = false;
			return Result;
		}
		PreviousLastKnownUnitId = Memory.UnitId;
		bHasPreviousLastKnownUnitId = true;
		if (Unit->CurrentHealth <= 0 || Unit->bExtracted)
		{
			continue;
		}
		if (!Battle.IsWithinGrid(Memory.X, Memory.Y, Memory.Z)
			|| Memory.SourceRuleId != Unit->SourceRuleId || Memory.DisplayName != Unit->DisplayName
			|| !bKnownStance || Memory.MaxHealth <= 0 || Memory.MaxHealth > 200
			|| Memory.CurrentHealth <= 0 || Memory.CurrentHealth > Memory.MaxHealth
			|| Memory.MaxMorale <= 0 || Memory.MaxMorale > 100
			|| Memory.CurrentMorale < 0 || Memory.CurrentMorale > Memory.MaxMorale
			|| Memory.Suppression < 0 || Memory.Suppression > 100
			|| Memory.LastSeenTurnNumber <= 0 || Memory.LastSeenTurnNumber > Battle.TurnNumber)
		{
			AddDiagnostic(
				Result.Diagnostics,
				TEXT("invalid_tactical_memory"),
				TEXT("Tactical last-known adversary memory is sorted, bounded, and tied to a living adversary."));
			Result.bSucceeded = false;
			return Result;
		}
	}

	TSet<int32> Discovered;
	Discovered.Reserve(Battle.PlayerDiscoveredCellIndices.Num() + Result.VisibleCellIndices.Num());
	for (const int32 CellIndex : Battle.PlayerDiscoveredCellIndices)
	{
		if (Battle.Cells.IsValidIndex(CellIndex))
		{
			Discovered.Add(CellIndex);
		}
	}
	for (const int32 CellIndex : Result.VisibleCellIndices)
	{
		Discovered.Add(CellIndex);
	}
	Battle.PlayerDiscoveredCellIndices = Discovered.Array();
	Battle.PlayerDiscoveredCellIndices.Sort();

	TArray<FTacticalUnitMemoryState> UpdatedMemory;
	UpdatedMemory.Reserve(Battle.PlayerLastKnownAdversaries.Num());
	for (const FTacticalUnitState& Unit : Battle.Units)
	{
		if (Unit.Team != ETacticalTeam::Adversary || Unit.CurrentHealth <= 0 || Unit.bExtracted)
		{
			continue;
		}
		if (Result.IsUnitVisible(Unit.UnitId))
		{
			FTacticalUnitMemoryState& Memory = UpdatedMemory.AddDefaulted_GetRef();
			Memory.UnitId = Unit.UnitId;
			Memory.SourceRuleId = Unit.SourceRuleId;
			Memory.DisplayName = Unit.DisplayName;
			Memory.Stance = Unit.Stance;
			Memory.X = Unit.X;
			Memory.Y = Unit.Y;
			Memory.Z = Unit.Z;
			Memory.MaxHealth = Unit.MaxHealth;
			Memory.CurrentHealth = Unit.CurrentHealth;
			Memory.MaxMorale = Unit.MaxMorale;
			Memory.CurrentMorale = Unit.CurrentMorale;
			Memory.Suppression = Unit.Suppression;
			Memory.LastSeenTurnNumber = Battle.TurnNumber;
			continue;
		}
		const FTacticalUnitMemoryState* ExistingMemory = Battle.PlayerLastKnownAdversaries.FindByPredicate(
			[&Unit](const FTacticalUnitMemoryState& Entry) { return Entry.UnitId == Unit.UnitId; });
		if (ExistingMemory != nullptr)
		{
			UpdatedMemory.Add(*ExistingMemory);
		}
	}
	UpdatedMemory.Sort(
		[](const FTacticalUnitMemoryState& Left, const FTacticalUnitMemoryState& Right)
		{
			return Left.UnitId.ToString(EGuidFormats::Digits) < Right.UnitId.ToString(EGuidFormats::Digits);
		});
	Battle.PlayerLastKnownAdversaries = MoveTemp(UpdatedMemory);
	return Result;
}

FTacticalVisibilityResult FTacticalNavigationService::ComputeTeamVisibility(
	const FTacticalBattleState& Battle,
	const FResolvedRuleSet& Rules,
	const ETacticalTeam ObserverTeam,
	const int32 VisionRangeModifier)
{
	using namespace TacticalNavigationPrivate;

	FTacticalVisibilityResult Result;
	Result.Width = Battle.Width;
	Result.Height = Battle.Height;
	Result.Levels = Battle.Levels;
	if ((ObserverTeam != ETacticalTeam::Player && ObserverTeam != ETacticalTeam::Adversary)
		|| VisionRangeModifier < -4 || VisionRangeModifier > 4)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("invalid_tactical_visibility_query"), TEXT("Tactical team visibility requires a supported observer team and range modifier."));
		return Result;
	}
	if (!ValidateGrid(Battle, Rules, Result.Diagnostics))
	{
		return Result;
	}
	TBitArray<> Visible(false, Battle.Cells.Num());
	for (const FTacticalUnitState& Observer : Battle.Units)
	{
		if (Observer.Team != ObserverTeam || Observer.CurrentHealth <= 0 || Observer.bExtracted
			|| !Battle.IsWithinGrid(Observer.X, Observer.Y, Observer.Z))
		{
			continue;
		}
		const int32 Range = FMath::Clamp(VisionRangeFor(Observer) + VisionRangeModifier, 1, 18);
		const int32 MinimumX = FMath::Max(0, Observer.X - Range);
		const int32 MaximumX = FMath::Min(Battle.Width - 1, Observer.X + Range);
		const int32 MinimumY = FMath::Max(0, Observer.Y - Range);
		const int32 MaximumY = FMath::Min(Battle.Height - 1, Observer.Y + Range);
		const int32 LevelReach = (Range + 1) / 2;
		const int32 MinimumZ = FMath::Max(0, Observer.Z - LevelReach);
		const int32 MaximumZ = FMath::Min(Battle.Levels - 1, Observer.Z + LevelReach);
		for (int32 Z = MinimumZ; Z <= MaximumZ; ++Z)
		{
			for (int32 Y = MinimumY; Y <= MaximumY; ++Y)
			{
				for (int32 X = MinimumX; X <= MaximumX; ++X)
				{
					const int64 DeltaX = X - Observer.X;
					const int64 DeltaY = Y - Observer.Y;
					const int64 DeltaZ = (Z - Observer.Z) * 2;
					if (DeltaX * DeltaX + DeltaY * DeltaY + DeltaZ * DeltaZ > static_cast<int64>(Range) * Range)
					{
						continue;
					}
					if (TraceLineOfSight(Battle, Rules, Observer.X, Observer.Y, Observer.Z, X, Y, Z)
						&& TraceSmokeObscuration(Battle, Observer.X, Observer.Y, Observer.Z, X, Y, Z) < 60)
					{
						Visible[Battle.GetCellIndex(X, Y, Z)] = true;
					}
				}
			}
		}
	}
	for (int32 Index = 0; Index < Visible.Num(); ++Index)
	{
		if (Visible[Index])
		{
			Result.VisibleCellIndices.Add(Index);
		}
	}
	for (const FTacticalUnitState& Unit : Battle.Units)
	{
		if (!Unit.UnitId.IsValid() || Unit.CurrentHealth <= 0 || Unit.bExtracted
			|| !Battle.IsWithinGrid(Unit.X, Unit.Y, Unit.Z))
		{
			continue;
		}
		if (Unit.Team == ObserverTeam || Visible[Battle.GetCellIndex(Unit.X, Unit.Y, Unit.Z)])
		{
			Result.VisibleUnitIds.Add(Unit.UnitId);
		}
	}
	Result.VisibleUnitIds.Sort(
		[](const FGuid& Left, const FGuid& Right)
		{
			return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
		});
	Result.bSucceeded = true;
	return Result;
}

bool FTacticalNavigationService::HasLineOfSight(
	const FTacticalBattleState& Battle,
	const FResolvedRuleSet& Rules,
	const int32 OriginX,
	const int32 OriginY,
	const int32 TargetX,
	const int32 TargetY,
	const int32 OriginZ,
	const int32 TargetZ)
{
	using namespace TacticalNavigationPrivate;

	TArray<FTacticalNavigationDiagnostic> Diagnostics;
	return Battle.IsWithinGrid(OriginX, OriginY, OriginZ)
		&& Battle.IsWithinGrid(TargetX, TargetY, TargetZ)
		&& ValidateGrid(Battle, Rules, Diagnostics)
		&& TraceLineOfSight(Battle, Rules, OriginX, OriginY, OriginZ, TargetX, TargetY, TargetZ);
}

int32 FTacticalNavigationService::ComputeSmokeObscuration(
	const FTacticalBattleState& Battle,
	const int32 OriginX,
	const int32 OriginY,
	const int32 TargetX,
	const int32 TargetY,
	const int32 OriginZ,
	const int32 TargetZ)
{
	using namespace TacticalNavigationPrivate;

	const int64 CellCount = static_cast<int64>(Battle.Width) * Battle.Height * Battle.Levels;
	return Battle.IsWithinGrid(OriginX, OriginY, OriginZ)
		&& Battle.IsWithinGrid(TargetX, TargetY, TargetZ)
		&& CellCount > 0 && CellCount <= 8192 && Battle.Cells.Num() == CellCount
		? FMath::Clamp(
			TraceSmokeObscuration(Battle, OriginX, OriginY, OriginZ, TargetX, TargetY, TargetZ),
			0,
			100)
		: 0;
}

FTacticalThrowTrajectoryResult FTacticalNavigationService::PreviewThrowTrajectory(
	const FTacticalBattleState& Battle,
	const FResolvedRuleSet& Rules,
	const int32 OriginX,
	const int32 OriginY,
	const int32 TargetX,
	const int32 TargetY,
	const int32 PeakHeight,
	const int32 OriginZ,
	const int32 TargetZ)
{
	using namespace TacticalNavigationPrivate;

	FTacticalThrowTrajectoryResult Result;
	Result.AimedX = TargetX;
	Result.AimedY = TargetY;
	Result.AimedZ = TargetZ;
	Result.LandingX = TargetX;
	Result.LandingY = TargetY;
	Result.LandingZ = TargetZ;
	Result.PeakHeight = PeakHeight;
	if (!ValidateGrid(Battle, Rules, Result.Diagnostics))
	{
		return Result;
	}
	if (!Battle.IsWithinGrid(OriginX, OriginY, OriginZ)
		|| !Battle.IsWithinGrid(TargetX, TargetY, TargetZ))
	{
		AddDiagnostic(Result.Diagnostics, TEXT("invalid_tactical_throw_target"), TEXT("Tactical throw origin and target must lie within the battlefield."));
		return Result;
	}
	if (PeakHeight <= 0 || PeakHeight > 8)
	{
		AddDiagnostic(Result.Diagnostics, TEXT("invalid_tactical_throw_arc"), TEXT("Tactical throw peak height must be between one and eight."));
		return Result;
	}

	TArray<int32, TInlineAllocator<64>> Trace;
	BuildTraceIndices(Battle, OriginX, OriginY, OriginZ, TargetX, TargetY, TargetZ, Trace);
	const int32 StepCount = Trace.Num() - 1;
	if (StepCount <= 0)
	{
		Result.bSucceeded = true;
		return Result;
	}
	for (int32 StepIndex = 1; StepIndex < StepCount; ++StepIndex)
	{
		const FTacticalCellState& Cell = Battle.Cells[Trace[StepIndex]];
		const FTacticalTerrainRule* Terrain = Rules.TacticalTerrains.Find(Cell.TerrainRuleId);
		check(Terrain != nullptr);
		if (Cell.CurrentIntegrity <= 0 || Cell.bDoorOpen || Terrain->ThrowObstacleHeight <= 0)
		{
			continue;
		}
		const int64 BaselineHeightNumerator =
			(static_cast<int64>(OriginZ * 4) * (StepCount - StepIndex)
				+ static_cast<int64>(TargetZ * 4) * StepIndex) * StepCount;
		const int64 ArcHeightNumerator = BaselineHeightNumerator
			+ 4LL * PeakHeight * StepIndex * (StepCount - StepIndex);
		const int64 ObstacleHeightNumerator = static_cast<int64>(Cell.Z * 4 + Terrain->ThrowObstacleHeight)
			* StepCount * StepCount;
		if (ArcHeightNumerator <= ObstacleHeightNumerator)
		{
			Result.bIntercepted = true;
			Result.LandingX = Cell.X;
			Result.LandingY = Cell.Y;
			Result.LandingZ = Cell.Z;
			Result.InterceptedObstacleHeight = Terrain->ThrowObstacleHeight;
			Result.InterceptedTerrainRuleId = Cell.TerrainRuleId;
			break;
		}
	}
	Result.bSucceeded = true;
	return Result;
}
