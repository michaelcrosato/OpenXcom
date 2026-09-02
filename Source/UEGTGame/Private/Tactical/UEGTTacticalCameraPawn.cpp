// Copyright 2026 UEGT contributors. MIT License.

#include "Tactical/UEGTTacticalCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"

AUEGTTacticalCameraPawn::AUEGTTacticalCameraPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(SceneRoot);
	SpringArm->TargetArmLength = 1800.0f;
	SpringArm->SetRelativeRotation(FRotator(-55.0f, -45.0f, 0.0f));
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 12.0f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->FieldOfView = 42.0f;
	Camera->PostProcessBlendWeight = 1.0f;
	Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
	Camera->PostProcessSettings.AutoExposureBias = -1.15f;
	Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
	Camera->PostProcessSettings.MotionBlurAmount = 0.0f;
}

void AUEGTTacticalCameraPawn::FocusBoard(
	const int32 Width,
	const int32 Height,
	const int32 ViewedLevel,
	const float CellSize,
	const float LevelHeight)
{
	const FVector Focus(
		FMath::Max(1, Width) * CellSize * 0.5f,
		FMath::Max(1, Height) * CellSize * 0.5f,
		ViewedLevel * LevelHeight);
	SetActorLocation(Focus);
	const float BoardSpan = FMath::Max(Width, Height) * CellSize;
	SpringArm->TargetArmLength = FMath::Clamp(BoardSpan * 1.2f, MinimumZoom, MaximumZoom);
}

void AUEGTTacticalCameraPawn::FocusCell(
	const int32 X,
	const int32 Y,
	const int32 Z,
	const float CellSize,
	const float LevelHeight)
{
	SetActorLocation(FVector(
		(FMath::Max(0, X) + 0.5f) * FMath::Max(1.0f, CellSize),
		(FMath::Max(0, Y) + 0.5f) * FMath::Max(1.0f, CellSize),
		FMath::Max(0, Z) * FMath::Max(1.0f, LevelHeight)));
}

void AUEGTTacticalCameraPawn::FocusGlobe(const float GlobeRadius)
{
	SetActorLocation(FVector::ZeroVector);
	SpringArm->SetRelativeRotation(FRotator(-18.0f, -45.0f, 0.0f));
	// Unreal's camera FOV is horizontal, so a 16:9 viewport needs substantially more
	// distance than a tactical board to keep the full globe visible behind side panels.
	SpringArm->TargetArmLength = FMath::Clamp(GlobeRadius * 6.25f, MinimumZoom, MaximumZoom);
}

void AUEGTTacticalCameraPawn::Pan(
	const float ForwardAxis,
	const float RightAxis,
	const float DeltaSeconds)
{
	if (FMath::IsNearlyZero(ForwardAxis) && FMath::IsNearlyZero(RightAxis))
	{
		return;
	}
	const FRotator YawRotation(0.0f, SpringArm->GetRelativeRotation().Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	FVector Direction = Forward * ForwardAxis + Right * RightAxis;
	Direction.Z = 0.0f;
	Direction = Direction.GetClampedToMaxSize(1.0f);
	AddActorWorldOffset(Direction * PanSpeed * FMath::Max(0.0f, DeltaSeconds), true);
}

void AUEGTTacticalCameraPawn::Orbit(const float YawDeltaDegrees)
{
	FRotator Rotation = SpringArm->GetRelativeRotation();
	Rotation.Yaw = FRotator::NormalizeAxis(Rotation.Yaw + YawDeltaDegrees);
	SpringArm->SetRelativeRotation(Rotation);
}

void AUEGTTacticalCameraPawn::OrbitGlobe(
	const float YawDeltaDegrees,
	const float PitchDeltaDegrees)
{
	FRotator Rotation = SpringArm->GetRelativeRotation();
	Rotation.Yaw = FRotator::NormalizeAxis(Rotation.Yaw + YawDeltaDegrees);
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch + PitchDeltaDegrees, -78.0f, -4.0f);
	SpringArm->SetRelativeRotation(Rotation);
}

void AUEGTTacticalCameraPawn::ApplyAccessibilitySettings(
	const bool bReduceMotion,
	const float CameraSpeedMultiplier)
{
	const float SpeedMultiplier = FMath::Clamp(CameraSpeedMultiplier, 0.5f, 2.0f);
	SpringArm->bEnableCameraLag = !bReduceMotion;
	PanSpeed = 900.0f * SpeedMultiplier;
	ZoomStep = 220.0f * SpeedMultiplier;
}

void AUEGTTacticalCameraPawn::Zoom(const float Direction)
{
	SpringArm->TargetArmLength = FMath::Clamp(
		SpringArm->TargetArmLength - Direction * ZoomStep,
		MinimumZoom,
		MaximumZoom);
}

float AUEGTTacticalCameraPawn::GetZoomDistance() const
{
	return SpringArm != nullptr ? SpringArm->TargetArmLength : 0.0f;
}

float AUEGTTacticalCameraPawn::GetCameraPitch() const
{
	return SpringArm != nullptr ? SpringArm->GetRelativeRotation().Pitch : 0.0f;
}

bool AUEGTTacticalCameraPawn::IsCameraLagEnabled() const
{
	return SpringArm != nullptr && SpringArm->bEnableCameraLag;
}
