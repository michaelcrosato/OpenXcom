#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tactical/TacticalPresentationService.h"
#include "UEGTUserSettings.h"

#include "UEGTTacticalBoardActor.generated.h"

class UDirectionalLightComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class USkyLightComponent;

/** Semantic result of a cursor/controller trace against the generated tactical board. */
USTRUCT(BlueprintType)
struct UEGTGAME_API FUEGTTacticalBoardHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Board")
	bool bHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Board")
	bool bHasCell = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Board")
	int32 X = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Board")
	int32 Y = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Board")
	int32 Z = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Board")
	FGuid UnitId;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Tactical|Board")
	FName ObjectiveId;
};

/** Runtime-generated, asset-light tactical board driven exclusively by fog-safe presentation snapshots. */
UCLASS(BlueprintType)
class UEGTGAME_API AUEGTTacticalBoardActor final : public AActor
{
	GENERATED_BODY()

public:
	AUEGTTacticalBoardActor();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|Board")
	void ApplySnapshot(const FTacticalHudSnapshot& Snapshot);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|Board")
	void ClearBoard();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Accessibility")
	void ApplyAccessibilityPalette(EUEGTColorVisionMode Mode, bool bHighContrast);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Accessibility")
	void SetReducedMotionEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|Board")
	FVector GridToWorld(int32 X, int32 Y, int32 Z, float HeightOffset = 0.0f) const;

	bool ResolveHit(const FHitResult& Hit, FUEGTTacticalBoardHit& OutHit) const;

	float GetCellSize() const { return CellSize; }
	float GetLevelHeight() const { return LevelHeight; }
	int32 GetRenderedGroundCount() const;
	int32 GetRenderedFogMemoryCount() const;
	int32 GetRenderedPlayerUnitCount() const;
	int32 GetRenderedAdversaryUnitCount() const;
	int32 GetRenderedLastKnownAdversaryCount() const;
	int32 GetRenderedObjectiveCount() const;
	int32 GetRenderedSelectionCount() const;
	int32 GetRenderedConnectorCount() const;
	int32 GetRenderedCoverCount() const;
	int32 GetRenderedDeploymentCount() const;
	int32 GetRenderedExtractionCount() const;
	/** Returns whether the board has loaded the semantic primitive profile for tactical markers. */
	bool UsesSemanticMarkerGeometry() const;
	/** Deterministic height encoding for a currently visible unit's stance and health. */
	static float CalculateUnitMarkerHeightScale(const FTacticalHudUnitView& Unit);
	/** Deterministic height encoding for active objective interaction progress. */
	static float CalculateObjectiveMarkerHeightScale(const FTacticalHudObjectiveView& Objective);
	/** Deterministic height encoding for visible terrain integrity, with legacy full-health fallback. */
	static float CalculateTerrainMarkerHeightScale(const FTacticalHudCellView& Cell);
	/** Deterministic footprint encoding for visible terrain cover percentage. */
	static float CalculateCoverMarkerScale(const FTacticalHudCellView& Cell);
	EUEGTColorVisionMode GetColorVisionMode() const { return ColorVisionMode; }
	bool IsHighContrastPaletteEnabled() const { return bUseHighContrast; }
	bool IsReducedMotionEnabled() const { return bReduceMotion; }
	float GetPresentationAnimationTimeSeconds() const { return PresentationAnimationTimeSeconds; }

protected:
	virtual void BeginPlay() override;

private:
	void ConfigureMeshComponent(UInstancedStaticMeshComponent* Component, bool bCollisionEnabled) const;
	void ApplyPalette();
	void UpdateAnimatedEffects(bool bAnimate);
	UMaterialInstanceDynamic* MakeColorMaterial(FName ObjectName, const FLinearColor& Color);
	void AddCellMarker(UInstancedStaticMeshComponent* Component, const FIntVector& Cell, float Height, float XYScale, float ZScale);
	void AddUnitMarker(UInstancedStaticMeshComponent* Component, const FTacticalHudUnitView& Unit);

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> GroundInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> FogMemoryInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> BlockerInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> DoorInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> ConnectorInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> CoverInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> DeploymentInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> ExtractionInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> PlayerUnitInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> AdversaryUnitInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> LastKnownAdversaryInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> ObjectiveInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> PathInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> HoverInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> SelectionInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> SmokeInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UInstancedStaticMeshComponent> FireInstances;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<UDirectionalLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Board")
	TObjectPtr<USkyLightComponent> FillLight;

	UPROPERTY(EditDefaultsOnly, Category = "UEGT|Tactical|Board")
	float CellSize = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "UEGT|Tactical|Board")
	float LevelHeight = 180.0f;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseShapeMaterial;

	TArray<FIntVector> GroundCells;
	TArray<FIntVector> FogMemoryCells;
	TArray<FIntVector> BlockerCells;
	TArray<FIntVector> DoorCells;
	TArray<FIntVector> SmokeCells;
	TArray<int32> SmokeIntensities;
	TArray<FIntVector> FireCells;
	TArray<int32> FireIntensities;
	TArray<FGuid> PlayerUnitIds;
	TArray<FIntVector> PlayerUnitCells;
	TArray<FGuid> AdversaryUnitIds;
	TArray<FIntVector> AdversaryUnitCells;
	TArray<FName> ObjectiveIds;
	TArray<FIntVector> ObjectiveCells;
	TArray<FIntVector> SelectionCells;
	EUEGTColorVisionMode ColorVisionMode = EUEGTColorVisionMode::Standard;
	bool bUseHighContrast = true;
	bool bReduceMotion = true;
	float PresentationAnimationTimeSeconds = 0.0f;
};
