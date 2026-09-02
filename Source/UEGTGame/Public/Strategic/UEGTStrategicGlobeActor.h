#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Strategic/StrategicPresentationService.h"
#include "UEGTUserSettings.h"

#include "UEGTStrategicGlobeActor.generated.h"

class UDirectionalLightComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class USkyLightComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EUEGTRegionalPressureTier : uint8
{
	Stable,
	Elevated,
	Critical
};

USTRUCT(BlueprintType)
struct UEGTGAME_API FUEGTStrategicGlobeHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Globe")
	bool bHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic|Globe")
	FStrategicGlobeMarkerView Marker;
};

/** Asset-light interactive globe generated from a visibility-safe strategic dashboard. */
UCLASS(BlueprintType)
class UEGTGAME_API AUEGTStrategicGlobeActor final : public AActor
{
	GENERATED_BODY()

public:
	AUEGTStrategicGlobeActor();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Globe")
	void ApplySnapshot(const FStrategicDashboardSnapshot& Snapshot);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Globe")
	void ClearGlobe();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Globe")
	void SetPresentationEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Accessibility")
	void ApplyAccessibilityPalette(EUEGTColorVisionMode Mode, bool bHighContrast);

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|Globe")
	FVector LongitudeLatitudeToWorld(int32 LongitudeMilliDegrees, int32 LatitudeMilliDegrees, float Altitude = 0.0f) const;

	/** Deterministic presentation-only solar approximation; no simulation randomness is consumed. */
	static double CalculateSolarDeclinationDegrees(const FDateTime& CampaignTimeUtc);
	static double CalculateSubsolarLongitudeDegrees(const FDateTime& CampaignTimeUtc);
	static FVector CalculateSunDirection(const FDateTime& CampaignTimeUtc);
	static EUEGTRegionalPressureTier ClassifyRegionalPressure(int32 Pressure);
	static int32 GetRegionalPressureSampleCount(EUEGTRegionalPressureTier Tier);
	static double CalculateRegionPressureRingRadiusDegrees(int32 Pressure);
	bool IsLocationInDaylight(int32 LongitudeMilliDegrees, int32 LatitudeMilliDegrees) const;

	bool ResolveHit(const FHitResult& Hit, FUEGTStrategicGlobeHit& OutHit) const;

	float GetGlobeRadius() const { return GlobeRadius; }
	int32 GetRenderedBaseCount() const;
	int32 GetRenderedCraftCount() const;
	int32 GetRenderedContactCount() const;
	int32 GetRenderedSiteCount() const;
	int32 GetRenderedRoutePointCount() const;
	int32 GetRenderedReferencePointCount() const;
	int32 GetRenderedTerminatorPointCount() const;
	bool GetRenderedTerminatorPointLocalPosition(int32 Index, FVector& OutPosition) const;
	int32 GetRenderedRegionPressurePointCount(EUEGTRegionalPressureTier Tier) const;
	bool GetRenderedRegionPressurePointLocalPosition(
		EUEGTRegionalPressureTier Tier,
		int32 Index,
		FVector& OutPosition) const;
	FVector GetCurrentSunDirection() const { return CurrentSunDirection; }
	FDateTime GetCurrentCampaignTimeUtc() const { return CurrentCampaignTimeUtc; }
	EUEGTColorVisionMode GetColorVisionMode() const { return ColorVisionMode; }
	bool IsHighContrastPaletteEnabled() const { return bUseHighContrast; }

protected:
	virtual void BeginPlay() override;

private:
	void ConfigureMarkerComponent(UInstancedStaticMeshComponent* Component) const;
	void ApplyPalette();
	void BuildReferenceGrid();
	void UpdateDayNight(const FDateTime& CampaignTimeUtc);
	void BuildTerminator();
	void AddRegionPressure(const FStrategicRegionView& Region);
	UInstancedStaticMeshComponent* GetRegionPressureComponent(EUEGTRegionalPressureTier Tier) const;
	UMaterialInstanceDynamic* MakeColorMaterial(FName ObjectName, const FLinearColor& Color);
	void AddMarker(UInstancedStaticMeshComponent* Component, const FStrategicGlobeMarkerView& Marker);
	void AddRoute(const FStrategicGlobeRouteView& Route);
	FVector LongitudeLatitudeToLocal(int32 LongitudeMilliDegrees, int32 LatitudeMilliDegrees, float Altitude) const;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UStaticMeshComponent> GlobeMesh;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UInstancedStaticMeshComponent> BaseMarkers;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UInstancedStaticMeshComponent> CraftMarkers;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UInstancedStaticMeshComponent> ContactMarkers;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UInstancedStaticMeshComponent> SiteMarkers;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UInstancedStaticMeshComponent> PlayerRoutePoints;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UInstancedStaticMeshComponent> AdversaryRoutePoints;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UInstancedStaticMeshComponent> ReferenceGridPoints;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UInstancedStaticMeshComponent> TerminatorPoints;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UInstancedStaticMeshComponent> StableRegionPressurePoints;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UInstancedStaticMeshComponent> ElevatedRegionPressurePoints;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UInstancedStaticMeshComponent> CriticalRegionPressurePoints;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UDirectionalLightComponent> SunLight;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<UDirectionalLightComponent> NightFillLight;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Strategic|Globe")
	TObjectPtr<USkyLightComponent> FillLight;

	UPROPERTY(EditDefaultsOnly, Category = "UEGT|Strategic|Globe")
	float GlobeRadius = 520.0f;

	UPROPERTY(EditDefaultsOnly, Category = "UEGT|Strategic|Globe")
	float MarkerAltitude = 24.0f;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseShapeMaterial;

	TArray<FStrategicGlobeMarkerView> BaseMarkerData;
	TArray<FStrategicGlobeMarkerView> CraftMarkerData;
	TArray<FStrategicGlobeMarkerView> ContactMarkerData;
	TArray<FStrategicGlobeMarkerView> SiteMarkerData;
	FDateTime CurrentCampaignTimeUtc = FDateTime(2035, 1, 1, 12, 0, 0);
	FVector CurrentSunDirection = FVector::ForwardVector;
	EUEGTColorVisionMode ColorVisionMode = EUEGTColorVisionMode::Standard;
	bool bUseHighContrast = true;
};
