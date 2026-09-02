#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "UEGTTacticalCameraPawn.generated.h"

class UCameraComponent;
class USceneComponent;
class USpringArmComponent;

/** Isometric tactical camera with frame-rate-independent pan, orbit, and zoom primitives. */
UCLASS(BlueprintType)
class UEGTGAME_API AUEGTTacticalCameraPawn final : public APawn
{
	GENERATED_BODY()

public:
	AUEGTTacticalCameraPawn();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|Camera")
	void FocusBoard(int32 Width, int32 Height, int32 ViewedLevel, float CellSize, float LevelHeight);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|Camera")
	void FocusCell(int32 X, int32 Y, int32 Z, float CellSize, float LevelHeight);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Camera")
	void FocusGlobe(float GlobeRadius);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|Camera")
	void Pan(float ForwardAxis, float RightAxis, float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|Camera")
	void Orbit(float YawDeltaDegrees);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Strategic|Camera")
	void OrbitGlobe(float YawDeltaDegrees, float PitchDeltaDegrees);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Accessibility")
	void ApplyAccessibilitySettings(bool bReduceMotion, float CameraSpeedMultiplier);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Tactical|Camera")
	void Zoom(float Direction);

	UFUNCTION(BlueprintPure, Category = "UEGT|Tactical|Camera")
	float GetZoomDistance() const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Strategic|Camera")
	float GetCameraPitch() const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Accessibility")
	bool IsCameraLagEnabled() const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Controls")
	float GetCameraPanSpeed() const { return PanSpeed; }

private:
	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Camera")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "UEGT|Tactical|Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditDefaultsOnly, Category = "UEGT|Tactical|Camera")
	float PanSpeed = 900.0f;

	UPROPERTY(EditDefaultsOnly, Category = "UEGT|Tactical|Camera")
	float MinimumZoom = 650.0f;

	UPROPERTY(EditDefaultsOnly, Category = "UEGT|Tactical|Camera")
	float MaximumZoom = 3600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "UEGT|Tactical|Camera")
	float ZoomStep = 220.0f;
};
