// Copyright 2026 UEGT contributors. MIT License.

#include "Tactical/UEGTTacticalBoardActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AUEGTTacticalBoardActor::AUEGTTacticalBoardActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	auto MakeInstances = [this](const FName Name)
	{
		UInstancedStaticMeshComponent* Component = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Component->SetupAttachment(SceneRoot);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCastShadow(true);
		return Component;
	};
	GroundInstances = MakeInstances(TEXT("GroundInstances"));
	FogMemoryInstances = MakeInstances(TEXT("FogMemoryInstances"));
	BlockerInstances = MakeInstances(TEXT("BlockerInstances"));
	DoorInstances = MakeInstances(TEXT("DoorInstances"));
	PlayerUnitInstances = MakeInstances(TEXT("PlayerUnitInstances"));
	AdversaryUnitInstances = MakeInstances(TEXT("AdversaryUnitInstances"));
	LastKnownAdversaryInstances = MakeInstances(TEXT("LastKnownAdversaryInstances"));
	ObjectiveInstances = MakeInstances(TEXT("ObjectiveInstances"));
	PathInstances = MakeInstances(TEXT("PathInstances"));
	HoverInstances = MakeInstances(TEXT("HoverInstances"));
	SelectionInstances = MakeInstances(TEXT("SelectionInstances"));
	SmokeInstances = MakeInstances(TEXT("SmokeInstances"));
	FireInstances = MakeInstances(TEXT("FireInstances"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (CubeMesh.Succeeded())
	{
		for (UInstancedStaticMeshComponent* Component : {
			GroundInstances.Get(), FogMemoryInstances.Get(), BlockerInstances.Get(), DoorInstances.Get(), PathInstances.Get(), HoverInstances.Get(),
			SelectionInstances.Get() })
		{
			Component->SetStaticMesh(CubeMesh.Object);
		}
	}
	if (CylinderMesh.Succeeded())
	{
		PlayerUnitInstances->SetStaticMesh(CylinderMesh.Object);
		ObjectiveInstances->SetStaticMesh(CylinderMesh.Object);
	}
	if (ConeMesh.Succeeded())
	{
		AdversaryUnitInstances->SetStaticMesh(ConeMesh.Object);
		FireInstances->SetStaticMesh(ConeMesh.Object);
	}
	if (SphereMesh.Succeeded())
	{
		LastKnownAdversaryInstances->SetStaticMesh(SphereMesh.Object);
		SmokeInstances->SetStaticMesh(SphereMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		BaseShapeMaterial = ShapeMaterial.Object;
	}

	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(SceneRoot);
	KeyLight->SetRelativeRotation(FRotator(-55.0f, -35.0f, 0.0f));
	KeyLight->SetIntensity(5.0f);
	KeyLight->SetLightColor(FLinearColor(0.78f, 0.86f, 1.0f));
	KeyLight->SetCastShadows(true);

	FillLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(SceneRoot);
	FillLight->SetIntensity(0.8f);
	FillLight->SetLightColor(FLinearColor(0.25f, 0.34f, 0.5f));
}

void AUEGTTacticalBoardActor::BeginPlay()
{
	Super::BeginPlay();
	ConfigureMeshComponent(GroundInstances, true);
	ConfigureMeshComponent(FogMemoryInstances, true);
	ConfigureMeshComponent(BlockerInstances, true);
	ConfigureMeshComponent(DoorInstances, true);
	ConfigureMeshComponent(PlayerUnitInstances, true);
	ConfigureMeshComponent(AdversaryUnitInstances, true);
	ConfigureMeshComponent(LastKnownAdversaryInstances, false);
	ConfigureMeshComponent(ObjectiveInstances, true);
	ConfigureMeshComponent(PathInstances, false);
	ConfigureMeshComponent(HoverInstances, false);
	ConfigureMeshComponent(SelectionInstances, false);
	ConfigureMeshComponent(SmokeInstances, false);
	ConfigureMeshComponent(FireInstances, false);
	ApplyPalette();
}

void AUEGTTacticalBoardActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bReduceMotion || DeltaSeconds <= 0.0f)
	{
		return;
	}
	PresentationAnimationTimeSeconds = FMath::Fmod(
		PresentationAnimationTimeSeconds + DeltaSeconds, 100000.0f);
	UpdateAnimatedEffects(true);
}

void AUEGTTacticalBoardActor::ConfigureMeshComponent(
	UInstancedStaticMeshComponent* Component,
	const bool bCollisionEnabled) const
{
	if (Component == nullptr)
	{
		return;
	}
	Component->SetCollisionEnabled(bCollisionEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	Component->SetCollisionObjectType(ECC_WorldStatic);
	Component->SetCollisionResponseToAllChannels(ECR_Ignore);
	if (bCollisionEnabled)
	{
		Component->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}
}

UMaterialInstanceDynamic* AUEGTTacticalBoardActor::MakeColorMaterial(
	const FName ObjectName,
	const FLinearColor& Color)
{
	if (BaseShapeMaterial == nullptr)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseShapeMaterial, this, ObjectName);
	if (Material != nullptr)
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return Material;
}

void AUEGTTacticalBoardActor::ApplyPalette()
{
	FLinearColor PlayerColor(0.08f, 0.48f, 0.95f);
	FLinearColor AdversaryColor(0.95f, 0.18f, 0.28f);
	FLinearColor ObjectiveColor(1.0f, 0.68f, 0.08f);
	switch (ColorVisionMode)
	{
	case EUEGTColorVisionMode::Deuteranopia:
		PlayerColor = FLinearColor(0.0f, 0.68f, 1.0f);
		AdversaryColor = FLinearColor(0.95f, 0.28f, 0.78f);
		ObjectiveColor = FLinearColor(1.0f, 0.86f, 0.08f);
		break;
	case EUEGTColorVisionMode::Protanopia:
		PlayerColor = FLinearColor(0.0f, 0.78f, 0.95f);
		AdversaryColor = FLinearColor(1.0f, 0.55f, 0.05f);
		ObjectiveColor = FLinearColor(0.68f, 0.4f, 1.0f);
		break;
	case EUEGTColorVisionMode::Tritanopia:
		PlayerColor = FLinearColor(0.16f, 0.9f, 0.42f);
		AdversaryColor = FLinearColor(1.0f, 0.18f, 0.35f);
		ObjectiveColor = FLinearColor(0.0f, 0.78f, 1.0f);
		break;
	default:
		break;
	}
	const auto Tone = [this](const FLinearColor& Color)
	{
		return bUseHighContrast
			? Color
			: FLinearColor(Color.R * 0.72f, Color.G * 0.72f, Color.B * 0.72f, 1.0f);
	};
	struct FPaletteEntry
	{
		UInstancedStaticMeshComponent* Component;
		FName Name;
		FLinearColor Color;
	};
	const FPaletteEntry Palette[] = {
		{ GroundInstances, TEXT("GroundMaterial"), FLinearColor(0.045f, 0.075f, 0.12f) },
		{ FogMemoryInstances, TEXT("FogMemoryMaterial"), FLinearColor(0.012f, 0.022f, 0.034f) },
		{ BlockerInstances, TEXT("BlockerMaterial"), FLinearColor(0.22f, 0.19f, 0.31f) },
		{ DoorInstances, TEXT("DoorMaterial"), FLinearColor(0.08f, 0.55f, 0.62f) },
		{ PlayerUnitInstances, TEXT("PlayerMaterial"), Tone(PlayerColor) },
		{ AdversaryUnitInstances, TEXT("AdversaryMaterial"), Tone(AdversaryColor) },
		{ LastKnownAdversaryInstances, TEXT("LastKnownAdversaryMaterial"), Tone(FLinearColor(AdversaryColor.R * 0.38f, AdversaryColor.G * 0.38f, AdversaryColor.B * 0.38f)) },
		{ ObjectiveInstances, TEXT("ObjectiveMaterial"), Tone(ObjectiveColor) },
		{ PathInstances, TEXT("PathMaterial"), FLinearColor(0.1f, 0.9f, 0.55f) },
		{ HoverInstances, TEXT("HoverMaterial"), FLinearColor(0.85f, 0.95f, 1.0f) },
		{ SelectionInstances, TEXT("SelectionMaterial"), FLinearColor(0.0f, 0.95f, 1.0f) },
		{ SmokeInstances, TEXT("SmokeMaterial"), FLinearColor(0.38f, 0.45f, 0.58f) },
		{ FireInstances, TEXT("FireMaterial"), FLinearColor(1.0f, 0.23f, 0.035f) }
	};
	for (const FPaletteEntry& Entry : Palette)
	{
		if (Entry.Component != nullptr)
		{
			if (UMaterialInstanceDynamic* Material = MakeColorMaterial(Entry.Name, Entry.Color))
			{
				Entry.Component->SetMaterial(0, Material);
			}
		}
	}
}

void AUEGTTacticalBoardActor::ApplyAccessibilityPalette(
	const EUEGTColorVisionMode Mode,
	const bool bHighContrast)
{
	ColorVisionMode = Mode;
	bUseHighContrast = bHighContrast;
	ApplyPalette();
}

void AUEGTTacticalBoardActor::SetReducedMotionEnabled(const bool bEnabled)
{
	bReduceMotion = bEnabled;
	PresentationAnimationTimeSeconds = 0.0f;
	SetActorTickEnabled(!bReduceMotion);
	UpdateAnimatedEffects(false);
}

void AUEGTTacticalBoardActor::UpdateAnimatedEffects(const bool bAnimate)
{
	const auto UpdateEffect = [this, bAnimate](
		UInstancedStaticMeshComponent* Component,
		const TArray<FIntVector>& Cells,
		const TArray<int32>& Intensities,
		const float BaseHeight,
		const float BaseXYScale,
		const float BaseZScale,
		const float XYAmplitude,
		const float ZAmplitude,
		const float HeightAmplitude,
		const float Frequency,
		const float PhaseStep)
	{
		if (Component == nullptr)
		{
			return;
		}
		const int32 Count = FMath::Min(
			Component->GetInstanceCount(), FMath::Min(Cells.Num(), Intensities.Num()));
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Pulse = bAnimate
				? FMath::Sin(PresentationAnimationTimeSeconds * Frequency + Index * PhaseStep)
				: 0.0f;
			const float Intensity = FMath::Clamp(static_cast<float>(Intensities[Index]), 0.0f, 100.0f) / 100.0f;
			const FIntVector& Cell = Cells[Index];
			const FVector Local(
				(static_cast<float>(Cell.X) + 0.5f) * CellSize,
				(static_cast<float>(Cell.Y) + 0.5f) * CellSize,
				static_cast<float>(Cell.Z) * LevelHeight + BaseHeight + Pulse * HeightAmplitude * (0.5f + Intensity * 0.5f));
			const float XYScale = BaseXYScale * (1.0f + Pulse * XYAmplitude);
			const float ZScale = BaseZScale * (1.0f + Pulse * ZAmplitude);
			Component->UpdateInstanceTransform(
				Index,
				FTransform(
					FRotator::ZeroRotator,
					Local,
					FVector(XYScale * CellSize / 100.0f, XYScale * CellSize / 100.0f, ZScale)),
				false,
				true,
				false);
		}
	};
	UpdateEffect(
		SmokeInstances,
		SmokeCells,
		SmokeIntensities,
		8.0f,
		0.72f,
		0.04f,
		0.06f,
		0.12f,
		5.0f,
		1.6f,
		0.71f);
	UpdateEffect(
		FireInstances,
		FireCells,
		FireIntensities,
		12.0f,
		0.42f,
		0.06f,
		0.10f,
		0.18f,
		7.0f,
		2.25f,
		1.17f);
	const auto UpdateEmphasis = [this, bAnimate](
		UInstancedStaticMeshComponent* Component,
		const TArray<FIntVector>& Cells,
		const float BaseHeight,
		const float BaseXYScale,
		const float BaseZScale,
		const float XYAmplitude,
		const float ZAmplitude,
		const float HeightAmplitude,
		const float Frequency,
		const float PhaseStep)
	{
		if (Component == nullptr)
		{
			return;
		}
		const int32 Count = FMath::Min(Component->GetInstanceCount(), Cells.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Pulse = bAnimate
				? FMath::Sin(PresentationAnimationTimeSeconds * Frequency + Index * PhaseStep)
				: 0.0f;
			const FIntVector& Cell = Cells[Index];
			const FVector Local(
				(static_cast<float>(Cell.X) + 0.5f) * CellSize,
				(static_cast<float>(Cell.Y) + 0.5f) * CellSize,
				static_cast<float>(Cell.Z) * LevelHeight + BaseHeight + Pulse * HeightAmplitude);
			const float XYScale = BaseXYScale * (1.0f + Pulse * XYAmplitude);
			const float ZScale = BaseZScale * (1.0f + Pulse * ZAmplitude);
			Component->UpdateInstanceTransform(
				Index,
				FTransform(
					FRotator::ZeroRotator,
					Local,
					FVector(XYScale * CellSize / 100.0f, XYScale * CellSize / 100.0f, ZScale)),
				false,
				true,
				false);
		}
	};
	UpdateEmphasis(
		ObjectiveInstances,
		ObjectiveCells,
		30.0f,
		0.34f,
		0.55f,
		0.08f,
		0.12f,
		3.0f,
		1.8f,
		0.83f);
	UpdateEmphasis(
		SelectionInstances,
		SelectionCells,
		5.0f,
		0.76f,
		0.025f,
		0.06f,
		0.08f,
		1.0f,
		2.4f,
		1.13f);
}

FVector AUEGTTacticalBoardActor::GridToWorld(
	const int32 X,
	const int32 Y,
	const int32 Z,
	const float HeightOffset) const
{
	const FVector Local(
		(static_cast<float>(X) + 0.5f) * CellSize,
		(static_cast<float>(Y) + 0.5f) * CellSize,
		static_cast<float>(Z) * LevelHeight + HeightOffset);
	return GetActorTransform().TransformPosition(Local);
}

void AUEGTTacticalBoardActor::AddCellMarker(
	UInstancedStaticMeshComponent* Component,
	const FIntVector& Cell,
	const float Height,
	const float XYScale,
	const float ZScale)
{
	if (Component == nullptr)
	{
		return;
	}
	const FVector Local(
		(static_cast<float>(Cell.X) + 0.5f) * CellSize,
		(static_cast<float>(Cell.Y) + 0.5f) * CellSize,
		static_cast<float>(Cell.Z) * LevelHeight + Height);
	Component->AddInstance(FTransform(
		FRotator::ZeroRotator,
		Local,
		FVector(XYScale * CellSize / 100.0f, XYScale * CellSize / 100.0f, ZScale)));
}

float AUEGTTacticalBoardActor::CalculateUnitMarkerHeightScale(const FTacticalHudUnitView& Unit)
{
	if (Unit.bIncapacitated)
	{
		return 0.18f;
	}
	const float HealthRatio = Unit.MaxHealth > 0
		? FMath::Clamp(static_cast<float>(Unit.CurrentHealth) / Unit.MaxHealth, 0.0f, 1.0f)
		: 1.0f;
	const float StanceScale = Unit.Stance == ETacticalStance::Crouched ? 0.52f : 0.74f;
	return StanceScale * (0.62f + HealthRatio * 0.38f);
}

void AUEGTTacticalBoardActor::AddUnitMarker(
	UInstancedStaticMeshComponent* Component,
	const FTacticalHudUnitView& Unit)
{
	AddCellMarker(
		Component,
		FIntVector(Unit.X, Unit.Y, Unit.Z),
		Unit.bLastKnown ? 24.0f : 42.0f,
		Unit.bLastKnown ? 0.32f : (Unit.bIncapacitated ? 0.3f : 0.44f),
		Unit.bLastKnown ? 0.28f : CalculateUnitMarkerHeightScale(Unit));
}

void AUEGTTacticalBoardActor::ClearBoard()
{
	for (UInstancedStaticMeshComponent* Component : {
		GroundInstances.Get(), FogMemoryInstances.Get(), BlockerInstances.Get(), DoorInstances.Get(), PlayerUnitInstances.Get(),
		AdversaryUnitInstances.Get(), LastKnownAdversaryInstances.Get(), ObjectiveInstances.Get(), PathInstances.Get(), HoverInstances.Get(),
		SelectionInstances.Get(), SmokeInstances.Get(), FireInstances.Get() })
	{
		if (Component != nullptr)
		{
			Component->ClearInstances();
		}
	}
	GroundCells.Reset();
	FogMemoryCells.Reset();
	BlockerCells.Reset();
	DoorCells.Reset();
	SmokeCells.Reset();
	SmokeIntensities.Reset();
	FireCells.Reset();
	FireIntensities.Reset();
	PlayerUnitIds.Reset();
	PlayerUnitCells.Reset();
	AdversaryUnitIds.Reset();
	AdversaryUnitCells.Reset();
	ObjectiveIds.Reset();
	ObjectiveCells.Reset();
	SelectionCells.Reset();
}

void AUEGTTacticalBoardActor::ApplySnapshot(const FTacticalHudSnapshot& Snapshot)
{
	ClearBoard();
	if (!Snapshot.bSucceeded)
	{
		return;
	}
	const bool bUsesKnownCells = !Snapshot.KnownCells.IsEmpty();
	const TArray<FTacticalHudCellView>& Cells = bUsesKnownCells ? Snapshot.KnownCells : Snapshot.VisibleCells;
	for (const FTacticalHudCellView& Cell : Cells)
	{
		const FIntVector GridCell(Cell.X, Cell.Y, Cell.Z);
		if (bUsesKnownCells && !Cell.bCurrentlyVisible)
		{
			FogMemoryCells.Add(GridCell);
			AddCellMarker(FogMemoryInstances, GridCell, -1.0f, 0.86f, 0.018f);
			continue;
		}
		GroundCells.Add(GridCell);
		AddCellMarker(GroundInstances, GridCell, 0.0f, 0.94f, 0.035f);
		if (Cell.bIsDoor && Cell.CurrentIntegrity > 0)
		{
			DoorCells.Add(GridCell);
			const FVector Local(
				(static_cast<float>(Cell.X) + (Cell.bDoorOpen ? 0.78f : 0.5f)) * CellSize,
				(static_cast<float>(Cell.Y) + 0.5f) * CellSize,
				static_cast<float>(Cell.Z) * LevelHeight + 48.0f);
			DoorInstances->AddInstance(FTransform(
				FRotator(0.0f, Cell.bDoorOpen ? 90.0f : 0.0f, 0.0f),
				Local,
				FVector(0.12f * CellSize / 100.0f, 0.78f * CellSize / 100.0f, 0.92f)));
		}
		else if ((Cell.bBlocksMovement || Cell.bBlocksVision) && Cell.CurrentIntegrity > 0)
		{
			BlockerCells.Add(GridCell);
			AddCellMarker(BlockerInstances, GridCell, 50.0f, 0.82f, 0.96f);
		}
		if (Cell.Smoke > 0)
		{
			SmokeCells.Add(GridCell);
			SmokeIntensities.Add(Cell.Smoke);
			AddCellMarker(SmokeInstances, GridCell, 8.0f + Cell.Smoke * 0.2f, 0.72f, 0.04f + Cell.Smoke / 1000.0f);
		}
		if (Cell.Fire > 0)
		{
			FireCells.Add(GridCell);
			FireIntensities.Add(Cell.Fire);
			AddCellMarker(FireInstances, GridCell, 12.0f + Cell.Fire * 0.18f, 0.42f, 0.06f + Cell.Fire / 900.0f);
		}
	}

	for (const FTacticalHudObjectiveView& Objective : Snapshot.Objectives)
	{
		if (Objective.Z != Snapshot.ViewedLevel || Objective.Status != ETacticalObjectiveStatus::Active)
		{
			continue;
		}
		const FIntVector Cell(Objective.X, Objective.Y, Objective.Z);
		ObjectiveIds.Add(Objective.ObjectiveId);
		ObjectiveCells.Add(Cell);
		AddCellMarker(ObjectiveInstances, Cell, 30.0f, 0.34f, 0.55f);
	}

	for (const FTacticalHudUnitView& Unit : Snapshot.Units)
	{
		if (Unit.Z != Snapshot.ViewedLevel || Unit.bExtracted)
		{
			continue;
		}
		const FIntVector Cell(Unit.X, Unit.Y, Unit.Z);
		if (Unit.Team == ETacticalTeam::Player)
		{
			PlayerUnitIds.Add(Unit.UnitId);
			PlayerUnitCells.Add(Cell);
			AddUnitMarker(PlayerUnitInstances, Unit);
		}
		else if (Unit.bLastKnown)
		{
			AddUnitMarker(LastKnownAdversaryInstances, Unit);
		}
		else
		{
			AdversaryUnitIds.Add(Unit.UnitId);
			AdversaryUnitCells.Add(Cell);
			AddUnitMarker(AdversaryUnitInstances, Unit);
		}
		if (Unit.bSelected)
		{
			SelectionCells.Add(Cell);
			AddCellMarker(SelectionInstances, Cell, 5.0f, 0.76f, 0.025f);
		}
	}

	if (Snapshot.Hover.bHasPathPreview && Snapshot.Hover.Path.bSucceeded)
	{
		for (const FTacticalPathStep& Step : Snapshot.Hover.Path.Steps)
		{
			if (Step.Z == Snapshot.ViewedLevel)
			{
				AddCellMarker(PathInstances, FIntVector(Step.X, Step.Y, Step.Z), 6.0f, 0.24f, 0.035f);
			}
		}
	}
	if (Snapshot.Hover.bHasCell && Snapshot.Hover.bCellVisible && Snapshot.Hover.Z == Snapshot.ViewedLevel)
	{
		AddCellMarker(HoverInstances, FIntVector(Snapshot.Hover.X, Snapshot.Hover.Y, Snapshot.Hover.Z), 4.0f, 0.88f, 0.018f);
	}
	UpdateAnimatedEffects(!bReduceMotion);
}

bool AUEGTTacticalBoardActor::ResolveHit(const FHitResult& Hit, FUEGTTacticalBoardHit& OutHit) const
{
	OutHit = FUEGTTacticalBoardHit();
	const UPrimitiveComponent* Component = Hit.GetComponent();
	const int32 InstanceIndex = Hit.Item;
	auto ResolveCell = [&OutHit, InstanceIndex](const TArray<FIntVector>& Cells)
	{
		if (!Cells.IsValidIndex(InstanceIndex))
		{
			return false;
		}
		OutHit.bHit = true;
		OutHit.bHasCell = true;
		OutHit.X = Cells[InstanceIndex].X;
		OutHit.Y = Cells[InstanceIndex].Y;
		OutHit.Z = Cells[InstanceIndex].Z;
		return true;
	};
	if (Component == GroundInstances && ResolveCell(GroundCells))
	{
		return true;
	}
	if (Component == FogMemoryInstances && ResolveCell(FogMemoryCells))
	{
		return true;
	}
	if (Component == BlockerInstances && ResolveCell(BlockerCells))
	{
		return true;
	}
	if (Component == DoorInstances && ResolveCell(DoorCells))
	{
		return true;
	}
	if (Component == PlayerUnitInstances && ResolveCell(PlayerUnitCells))
	{
		if (PlayerUnitIds.IsValidIndex(InstanceIndex))
		{
			OutHit.UnitId = PlayerUnitIds[InstanceIndex];
		}
		return true;
	}
	if (Component == AdversaryUnitInstances && ResolveCell(AdversaryUnitCells))
	{
		if (AdversaryUnitIds.IsValidIndex(InstanceIndex))
		{
			OutHit.UnitId = AdversaryUnitIds[InstanceIndex];
		}
		return true;
	}
	if (Component == ObjectiveInstances && ResolveCell(ObjectiveCells))
	{
		if (ObjectiveIds.IsValidIndex(InstanceIndex))
		{
			OutHit.ObjectiveId = ObjectiveIds[InstanceIndex];
		}
		return true;
	}
	return false;
}

int32 AUEGTTacticalBoardActor::GetRenderedGroundCount() const
{
	return GroundInstances != nullptr ? GroundInstances->GetInstanceCount() : 0;
}

int32 AUEGTTacticalBoardActor::GetRenderedFogMemoryCount() const
{
	return FogMemoryInstances != nullptr ? FogMemoryInstances->GetInstanceCount() : 0;
}

int32 AUEGTTacticalBoardActor::GetRenderedPlayerUnitCount() const
{
	return PlayerUnitInstances != nullptr ? PlayerUnitInstances->GetInstanceCount() : 0;
}

int32 AUEGTTacticalBoardActor::GetRenderedAdversaryUnitCount() const
{
	return AdversaryUnitInstances != nullptr ? AdversaryUnitInstances->GetInstanceCount() : 0;
}

int32 AUEGTTacticalBoardActor::GetRenderedLastKnownAdversaryCount() const
{
	return LastKnownAdversaryInstances != nullptr ? LastKnownAdversaryInstances->GetInstanceCount() : 0;
}

int32 AUEGTTacticalBoardActor::GetRenderedObjectiveCount() const
{
	return ObjectiveInstances != nullptr ? ObjectiveInstances->GetInstanceCount() : 0;
}

int32 AUEGTTacticalBoardActor::GetRenderedSelectionCount() const
{
	return SelectionInstances != nullptr ? SelectionInstances->GetInstanceCount() : 0;
}

bool AUEGTTacticalBoardActor::UsesSemanticMarkerGeometry() const
{
	const UStaticMesh* GroundMesh = GroundInstances != nullptr ? GroundInstances->GetStaticMesh() : nullptr;
	const UStaticMesh* PlayerMesh = PlayerUnitInstances != nullptr ? PlayerUnitInstances->GetStaticMesh() : nullptr;
	const UStaticMesh* AdversaryMesh = AdversaryUnitInstances != nullptr ? AdversaryUnitInstances->GetStaticMesh() : nullptr;
	const UStaticMesh* LastKnownMesh = LastKnownAdversaryInstances != nullptr
		? LastKnownAdversaryInstances->GetStaticMesh()
		: nullptr;
	const UStaticMesh* SmokeMesh = SmokeInstances != nullptr ? SmokeInstances->GetStaticMesh() : nullptr;
	const UStaticMesh* FireMesh = FireInstances != nullptr ? FireInstances->GetStaticMesh() : nullptr;
	return GroundMesh != nullptr
		&& PlayerMesh != nullptr
		&& AdversaryMesh != nullptr
		&& LastKnownMesh != nullptr
		&& SmokeMesh != nullptr
		&& FireMesh != nullptr
		&& GroundMesh != PlayerMesh
		&& PlayerMesh != AdversaryMesh
		&& AdversaryMesh != LastKnownMesh
		&& SmokeMesh == LastKnownMesh
		&& FireMesh == AdversaryMesh;
}
