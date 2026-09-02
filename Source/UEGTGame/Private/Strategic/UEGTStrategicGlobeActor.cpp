// Copyright 2026 UEGT contributors. MIT License.

#include "Strategic/UEGTStrategicGlobeActor.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AUEGTStrategicGlobeActor::AUEGTStrategicGlobeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GlobeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlobeMesh"));
	GlobeMesh->SetupAttachment(SceneRoot);
	GlobeMesh->SetMobility(EComponentMobility::Movable);
	GlobeMesh->SetCastShadow(true);

	auto MakeInstances = [this](const FName Name)
	{
		UInstancedStaticMeshComponent* Component = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Component->SetupAttachment(SceneRoot);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCastShadow(true);
		return Component;
	};
	BaseMarkers = MakeInstances(TEXT("BaseMarkers"));
	CraftMarkers = MakeInstances(TEXT("CraftMarkers"));
	ContactMarkers = MakeInstances(TEXT("ContactMarkers"));
	SiteMarkers = MakeInstances(TEXT("SiteMarkers"));
	PlayerRoutePoints = MakeInstances(TEXT("PlayerRoutePoints"));
	AdversaryRoutePoints = MakeInstances(TEXT("AdversaryRoutePoints"));
	ReferenceGridPoints = MakeInstances(TEXT("ReferenceGridPoints"));
	TerminatorPoints = MakeInstances(TEXT("TerminatorPoints"));
	StableRegionPressurePoints = MakeInstances(TEXT("StableRegionPressurePoints"));
	ElevatedRegionPressurePoints = MakeInstances(TEXT("ElevatedRegionPressurePoints"));
	CriticalRegionPressurePoints = MakeInstances(TEXT("CriticalRegionPressurePoints"));
	ReferenceGridPoints->SetCastShadow(false);
	TerminatorPoints->SetCastShadow(false);
	StableRegionPressurePoints->SetCastShadow(false);
	ElevatedRegionPressurePoints->SetCastShadow(false);
	CriticalRegionPressurePoints->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SphereMesh.Succeeded())
	{
		GlobeMesh->SetStaticMesh(SphereMesh.Object);
		CraftMarkers->SetStaticMesh(SphereMesh.Object);
		PlayerRoutePoints->SetStaticMesh(SphereMesh.Object);
		AdversaryRoutePoints->SetStaticMesh(SphereMesh.Object);
		ReferenceGridPoints->SetStaticMesh(SphereMesh.Object);
		TerminatorPoints->SetStaticMesh(SphereMesh.Object);
		StableRegionPressurePoints->SetStaticMesh(SphereMesh.Object);
	}
	if (CylinderMesh.Succeeded())
	{
		BaseMarkers->SetStaticMesh(CylinderMesh.Object);
		ContactMarkers->SetStaticMesh(CylinderMesh.Object);
		CriticalRegionPressurePoints->SetStaticMesh(CylinderMesh.Object);
	}
	if (CubeMesh.Succeeded())
	{
		SiteMarkers->SetStaticMesh(CubeMesh.Object);
		ElevatedRegionPressurePoints->SetStaticMesh(CubeMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		BaseShapeMaterial = ShapeMaterial.Object;
	}

	SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
	SunLight->SetupAttachment(SceneRoot);
	SunLight->SetRelativeRotation(FRotator::ZeroRotator);
	SunLight->SetIntensity(2.0f);
	SunLight->SetLightColor(FLinearColor(1.0f, 0.88f, 0.68f));
	SunLight->SetCastShadows(true);
	SunLight->SetForwardShadingPriority(1);

	NightFillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("NightFillLight"));
	NightFillLight->SetupAttachment(SceneRoot);
	NightFillLight->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	NightFillLight->SetIntensity(0.9f);
	NightFillLight->SetLightColor(FLinearColor(0.16f, 0.42f, 1.0f));
	NightFillLight->SetCastShadows(false);

	FillLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(SceneRoot);
	FillLight->SetIntensity(0.16f);
	FillLight->SetLightColor(FLinearColor(0.08f, 0.18f, 0.34f));
}

void AUEGTStrategicGlobeActor::BeginPlay()
{
	Super::BeginPlay();
	GlobeMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GlobeMesh->SetCollisionObjectType(ECC_WorldStatic);
	GlobeMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	GlobeMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GlobeMesh->SetRelativeScale3D(FVector(GlobeRadius / 50.0f));
	for (UInstancedStaticMeshComponent* Component : {
		BaseMarkers.Get(), CraftMarkers.Get(), ContactMarkers.Get(), SiteMarkers.Get() })
	{
		ConfigureMarkerComponent(Component);
	}
	for (UInstancedStaticMeshComponent* Component : {
		PlayerRoutePoints.Get(), AdversaryRoutePoints.Get(), ReferenceGridPoints.Get(), TerminatorPoints.Get(),
		StableRegionPressurePoints.Get(), ElevatedRegionPressurePoints.Get(), CriticalRegionPressurePoints.Get() })
	{
		if (Component != nullptr)
		{
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	ApplyPalette();
	BuildReferenceGrid();
	UpdateDayNight(CurrentCampaignTimeUtc);
}

void AUEGTStrategicGlobeActor::ConfigureMarkerComponent(UInstancedStaticMeshComponent* Component) const
{
	if (Component == nullptr)
	{
		return;
	}
	Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Component->SetCollisionObjectType(ECC_WorldStatic);
	Component->SetCollisionResponseToAllChannels(ECR_Ignore);
	Component->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

UMaterialInstanceDynamic* AUEGTStrategicGlobeActor::MakeColorMaterial(
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

void AUEGTStrategicGlobeActor::ApplyPalette()
{
	FLinearColor BaseColor(0.0f, 0.92f, 1.0f);
	FLinearColor CraftColor(0.25f, 0.95f, 0.62f);
	FLinearColor ContactColor(1.0f, 0.18f, 0.28f);
	FLinearColor SiteColor(1.0f, 0.67f, 0.08f);
	FLinearColor StableRegionColor(0.12f, 0.88f, 1.0f);
	FLinearColor ElevatedRegionColor(1.0f, 0.68f, 0.08f);
	FLinearColor CriticalRegionColor(1.0f, 0.16f, 0.34f);
	switch (ColorVisionMode)
	{
	case EUEGTColorVisionMode::Deuteranopia:
		ContactColor = FLinearColor(0.95f, 0.28f, 0.78f);
		SiteColor = FLinearColor(1.0f, 0.86f, 0.08f);
		ElevatedRegionColor = FLinearColor(1.0f, 0.86f, 0.08f);
		CriticalRegionColor = FLinearColor(0.95f, 0.28f, 0.78f);
		break;
	case EUEGTColorVisionMode::Protanopia:
		CraftColor = FLinearColor(0.0f, 0.78f, 0.95f);
		ContactColor = FLinearColor(1.0f, 0.55f, 0.05f);
		SiteColor = FLinearColor(0.68f, 0.4f, 1.0f);
		ElevatedRegionColor = FLinearColor(0.68f, 0.4f, 1.0f);
		CriticalRegionColor = FLinearColor(1.0f, 0.55f, 0.05f);
		break;
	case EUEGTColorVisionMode::Tritanopia:
		BaseColor = FLinearColor(0.18f, 0.9f, 0.4f);
		CraftColor = FLinearColor(0.72f, 0.88f, 0.16f);
		ContactColor = FLinearColor(1.0f, 0.18f, 0.35f);
		SiteColor = FLinearColor(0.0f, 0.78f, 1.0f);
		StableRegionColor = FLinearColor(0.18f, 0.9f, 0.4f);
		ElevatedRegionColor = FLinearColor(0.72f, 0.88f, 0.16f);
		CriticalRegionColor = FLinearColor(1.0f, 0.18f, 0.35f);
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
		UPrimitiveComponent* Component;
		FName Name;
		FLinearColor Color;
	};
	const FPaletteEntry Palette[] = {
		{ GlobeMesh, TEXT("GlobeMaterial"), FLinearColor(0.018f, 0.065f, 0.14f) },
		{ BaseMarkers, TEXT("BaseMarkerMaterial"), Tone(BaseColor) },
		{ CraftMarkers, TEXT("CraftMarkerMaterial"), Tone(CraftColor) },
		{ ContactMarkers, TEXT("ContactMarkerMaterial"), Tone(ContactColor) },
		{ SiteMarkers, TEXT("SiteMarkerMaterial"), Tone(SiteColor) },
		{ PlayerRoutePoints, TEXT("PlayerRouteMaterial"), FLinearColor(0.0f, 0.82f, 1.0f) },
		{ AdversaryRoutePoints, TEXT("AdversaryRouteMaterial"), FLinearColor(0.95f, 0.12f, 0.3f) },
		{ ReferenceGridPoints, TEXT("ReferenceGridMaterial"), FLinearColor(0.08f, 0.9f, 1.0f) },
		{ TerminatorPoints, TEXT("TerminatorMaterial"),
			Tone(FLinearColor(1.0f, 0.68f, 0.16f)) },
		{ StableRegionPressurePoints, TEXT("StableRegionPressureMaterial"), Tone(StableRegionColor) },
		{ ElevatedRegionPressurePoints, TEXT("ElevatedRegionPressureMaterial"), Tone(ElevatedRegionColor) },
		{ CriticalRegionPressurePoints, TEXT("CriticalRegionPressureMaterial"), Tone(CriticalRegionColor) }
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

void AUEGTStrategicGlobeActor::ApplyAccessibilityPalette(
	const EUEGTColorVisionMode Mode,
	const bool bHighContrast)
{
	ColorVisionMode = Mode;
	bUseHighContrast = bHighContrast;
	ApplyPalette();
}

void AUEGTStrategicGlobeActor::BuildReferenceGrid()
{
	if (ReferenceGridPoints == nullptr)
	{
		return;
	}
	ReferenceGridPoints->ClearInstances();
	const auto AddPoint = [this](const int32 LongitudeDegrees, const int32 LatitudeDegrees)
	{
		const FVector Location = LongitudeLatitudeToLocal(
			LongitudeDegrees * 1000, LatitudeDegrees * 1000, 3.0f);
		ReferenceGridPoints->AddInstance(FTransform(
			FRotator::ZeroRotator, Location, FVector(0.04f)));
	};
	for (int32 Longitude = -150; Longitude <= 180; Longitude += 30)
	{
		for (int32 Latitude = -80; Latitude <= 80; Latitude += 5)
		{
			AddPoint(Longitude, Latitude);
		}
	}
	for (int32 Latitude = -60; Latitude <= 60; Latitude += 30)
	{
		for (int32 Longitude = -175; Longitude <= 180; Longitude += 5)
		{
			AddPoint(Longitude, Latitude);
		}
	}
}

double AUEGTStrategicGlobeActor::CalculateSolarDeclinationDegrees(
	const FDateTime& CampaignTimeUtc)
{
	const double DaysInYear = FDateTime::IsLeapYear(CampaignTimeUtc.GetYear()) ? 366.0 : 365.0;
	const double SeasonalAngle = 2.0 * PI
		* (static_cast<double>(CampaignTimeUtc.GetDayOfYear()) - 80.0) / DaysInYear;
	return 23.44 * FMath::Sin(SeasonalAngle);
}

double AUEGTStrategicGlobeActor::CalculateSubsolarLongitudeDegrees(
	const FDateTime& CampaignTimeUtc)
{
	const double HoursUtc = CampaignTimeUtc.GetHour()
		+ CampaignTimeUtc.GetMinute() / 60.0
		+ CampaignTimeUtc.GetSecond() / 3600.0
		+ CampaignTimeUtc.GetMillisecond() / 3600000.0;
	double Longitude = 180.0 - HoursUtc * 15.0;
	while (Longitude > 180.0)
	{
		Longitude -= 360.0;
	}
	while (Longitude <= -180.0)
	{
		Longitude += 360.0;
	}
	return Longitude;
}

FVector AUEGTStrategicGlobeActor::CalculateSunDirection(
	const FDateTime& CampaignTimeUtc)
{
	const double Longitude = FMath::DegreesToRadians(
		CalculateSubsolarLongitudeDegrees(CampaignTimeUtc));
	const double Declination = FMath::DegreesToRadians(
		CalculateSolarDeclinationDegrees(CampaignTimeUtc));
	const double CosDeclination = FMath::Cos(Declination);
	return FVector(
		CosDeclination * FMath::Cos(Longitude),
		CosDeclination * FMath::Sin(Longitude),
		FMath::Sin(Declination)).GetSafeNormal();
}

EUEGTRegionalPressureTier AUEGTStrategicGlobeActor::ClassifyRegionalPressure(
	const int32 Pressure)
{
	if (Pressure >= 70)
	{
		return EUEGTRegionalPressureTier::Critical;
	}
	if (Pressure >= 30)
	{
		return EUEGTRegionalPressureTier::Elevated;
	}
	return EUEGTRegionalPressureTier::Stable;
}

int32 AUEGTStrategicGlobeActor::GetRegionalPressureSampleCount(
	const EUEGTRegionalPressureTier Tier)
{
	switch (Tier)
	{
	case EUEGTRegionalPressureTier::Elevated:
		return 12;
	case EUEGTRegionalPressureTier::Critical:
		return 16;
	default:
		return 8;
	}
}

double AUEGTStrategicGlobeActor::CalculateRegionPressureRingRadiusDegrees(
	const int32 Pressure)
{
	return 4.5 + static_cast<double>(FMath::Clamp(Pressure, 0, 100)) * 0.035;
}

void AUEGTStrategicGlobeActor::UpdateDayNight(const FDateTime& CampaignTimeUtc)
{
	CurrentCampaignTimeUtc = CampaignTimeUtc;
	CurrentSunDirection = CalculateSunDirection(CampaignTimeUtc);
	if (SunLight != nullptr)
	{
		SunLight->SetRelativeRotation((-CurrentSunDirection).Rotation());
	}
	if (NightFillLight != nullptr)
	{
		NightFillLight->SetRelativeRotation(CurrentSunDirection.Rotation());
	}
	BuildTerminator();
}

void AUEGTStrategicGlobeActor::BuildTerminator()
{
	if (TerminatorPoints == nullptr)
	{
		return;
	}
	TerminatorPoints->ClearInstances();
	FVector BasisX = FVector::CrossProduct(CurrentSunDirection, FVector::UpVector);
	if (!BasisX.Normalize())
	{
		BasisX = FVector::CrossProduct(CurrentSunDirection, FVector::RightVector).GetSafeNormal();
	}
	const FVector BasisY = FVector::CrossProduct(CurrentSunDirection, BasisX).GetSafeNormal();
	constexpr int32 PointCount = 96;
	for (int32 Index = 0; Index < PointCount; ++Index)
	{
		const double Angle = 2.0 * PI * static_cast<double>(Index) / PointCount;
		const FVector Direction = (BasisX * FMath::Cos(Angle) + BasisY * FMath::Sin(Angle)).GetSafeNormal();
		TerminatorPoints->AddInstance(FTransform(
			FRotator::ZeroRotator,
			Direction * (GlobeRadius + 8.0f),
			FVector(0.055f)));
	}
}

UInstancedStaticMeshComponent* AUEGTStrategicGlobeActor::GetRegionPressureComponent(
	const EUEGTRegionalPressureTier Tier) const
{
	switch (Tier)
	{
	case EUEGTRegionalPressureTier::Elevated:
		return ElevatedRegionPressurePoints.Get();
	case EUEGTRegionalPressureTier::Critical:
		return CriticalRegionPressurePoints.Get();
	default:
		return StableRegionPressurePoints.Get();
	}
}

void AUEGTStrategicGlobeActor::AddRegionPressure(const FStrategicRegionView& Region)
{
	const int32 Pressure = FMath::Clamp(Region.Pressure, 0, 100);
	const EUEGTRegionalPressureTier Tier = ClassifyRegionalPressure(Pressure);
	UInstancedStaticMeshComponent* Component = GetRegionPressureComponent(Tier);
	if (Component == nullptr)
	{
		return;
	}

	const FVector CenterDirection = LongitudeLatitudeToLocal(
		Region.LongitudeMilliDegrees,
		Region.LatitudeMilliDegrees,
		0.0f).GetSafeNormal();
	FVector TangentX = FVector::CrossProduct(CenterDirection, FVector::UpVector);
	if (!TangentX.Normalize())
	{
		TangentX = FVector::CrossProduct(CenterDirection, FVector::RightVector).GetSafeNormal();
	}
	const FVector TangentY = FVector::CrossProduct(CenterDirection, TangentX).GetSafeNormal();
	const double AngularRadius = FMath::DegreesToRadians(
		CalculateRegionPressureRingRadiusDegrees(Pressure));
	const double RingCos = FMath::Cos(AngularRadius);
	const double RingSin = FMath::Sin(AngularRadius);
	const int32 SampleCount = GetRegionalPressureSampleCount(Tier);
	for (int32 Index = 0; Index < SampleCount; ++Index)
	{
		const double Angle = 2.0 * PI * static_cast<double>(Index) / SampleCount;
		const FVector TangentDirection =
			TangentX * FMath::Cos(Angle) + TangentY * FMath::Sin(Angle);
		const FVector Direction =
			(CenterDirection * RingCos + TangentDirection * RingSin).GetSafeNormal();
		FVector Scale(0.06f);
		if (Tier == EUEGTRegionalPressureTier::Elevated)
		{
			Scale = FVector(0.052f, 0.052f, 0.035f);
		}
		else if (Tier == EUEGTRegionalPressureTier::Critical)
		{
			Scale = FVector(0.04f, 0.04f, 0.1f);
		}
		Component->AddInstance(FTransform(
			FRotationMatrix::MakeFromZ(Direction).Rotator(),
			Direction * (GlobeRadius + 14.0f),
			Scale));
	}
}

bool AUEGTStrategicGlobeActor::IsLocationInDaylight(
	const int32 LongitudeMilliDegrees,
	const int32 LatitudeMilliDegrees) const
{
	const FVector LocationNormal = LongitudeLatitudeToLocal(
		LongitudeMilliDegrees,
		LatitudeMilliDegrees,
		0.0f).GetSafeNormal();
	return FVector::DotProduct(LocationNormal, CurrentSunDirection) >= 0.0f;
}

FVector AUEGTStrategicGlobeActor::LongitudeLatitudeToLocal(
	const int32 LongitudeMilliDegrees,
	const int32 LatitudeMilliDegrees,
	const float Altitude) const
{
	const double Longitude = FMath::DegreesToRadians(
		static_cast<double>(FMath::Clamp(LongitudeMilliDegrees, -180000, 180000)) / 1000.0);
	const double Latitude = FMath::DegreesToRadians(
		static_cast<double>(FMath::Clamp(LatitudeMilliDegrees, -90000, 90000)) / 1000.0);
	const double Radius = static_cast<double>(GlobeRadius + Altitude);
	const double CosLatitude = FMath::Cos(Latitude);
	return FVector(
		Radius * CosLatitude * FMath::Cos(Longitude),
		Radius * CosLatitude * FMath::Sin(Longitude),
		Radius * FMath::Sin(Latitude));
}

FVector AUEGTStrategicGlobeActor::LongitudeLatitudeToWorld(
	const int32 LongitudeMilliDegrees,
	const int32 LatitudeMilliDegrees,
	const float Altitude) const
{
	return GetActorTransform().TransformPosition(
		LongitudeLatitudeToLocal(LongitudeMilliDegrees, LatitudeMilliDegrees, Altitude));
}

void AUEGTStrategicGlobeActor::AddMarker(
	UInstancedStaticMeshComponent* Component,
	const FStrategicGlobeMarkerView& Marker)
{
	if (Component == nullptr)
	{
		return;
	}
	const FVector Location = LongitudeLatitudeToLocal(
		Marker.LongitudeMilliDegrees, Marker.LatitudeMilliDegrees, MarkerAltitude);
	const FVector Normal = Location.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FRotator Rotation = FRotationMatrix::MakeFromZ(Normal).Rotator();
	FVector Scale(0.14f, 0.14f, 0.22f);
	if (Marker.Type == EStrategicGlobeMarkerType::Craft)
	{
		Scale = FVector(0.13f);
	}
	else if (Marker.Type == EStrategicGlobeMarkerType::Site)
	{
		Scale = FVector(0.12f);
	}
	else if (Marker.bUrgent)
	{
		Scale = FVector(0.17f, 0.17f, 0.28f);
	}
	Component->AddInstance(FTransform(Rotation, Location, Scale));
}

void AUEGTStrategicGlobeActor::AddRoute(const FStrategicGlobeRouteView& Route)
{
	UInstancedStaticMeshComponent* Component = Route.bPlayerControlled ? PlayerRoutePoints : AdversaryRoutePoints;
	if (Component == nullptr)
	{
		return;
	}
	const FVector Start = LongitudeLatitudeToLocal(
		Route.OriginLongitudeMilliDegrees, Route.OriginLatitudeMilliDegrees, 0.0f).GetSafeNormal();
	const FVector End = LongitudeLatitudeToLocal(
		Route.DestinationLongitudeMilliDegrees, Route.DestinationLatitudeMilliDegrees, 0.0f).GetSafeNormal();
	constexpr int32 Samples = 18;
	for (int32 Index = 1; Index < Samples; ++Index)
	{
		const float Alpha = static_cast<float>(Index) / static_cast<float>(Samples);
		FVector Direction = FMath::Lerp(Start, End, Alpha).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			Direction = Start;
		}
		const float Altitude = 14.0f + FMath::Sin(Alpha * PI) * 42.0f;
		const FVector Location = Direction * (GlobeRadius + Altitude);
		const float Pulse = FMath::Abs(Alpha - Route.Progress) < 0.045f ? 0.09f : 0.045f;
		Component->AddInstance(FTransform(FRotator::ZeroRotator, Location, FVector(Pulse)));
	}
}

void AUEGTStrategicGlobeActor::ClearGlobe()
{
	for (UInstancedStaticMeshComponent* Component : {
		BaseMarkers.Get(), CraftMarkers.Get(), ContactMarkers.Get(), SiteMarkers.Get(),
		PlayerRoutePoints.Get(), AdversaryRoutePoints.Get(), StableRegionPressurePoints.Get(),
		ElevatedRegionPressurePoints.Get(), CriticalRegionPressurePoints.Get() })
	{
		if (Component != nullptr)
		{
			Component->ClearInstances();
		}
	}
	BaseMarkerData.Reset();
	CraftMarkerData.Reset();
	ContactMarkerData.Reset();
	SiteMarkerData.Reset();
}

void AUEGTStrategicGlobeActor::ApplySnapshot(const FStrategicDashboardSnapshot& Snapshot)
{
	if (ReferenceGridPoints != nullptr && ReferenceGridPoints->GetInstanceCount() == 0)
	{
		BuildReferenceGrid();
	}
	ClearGlobe();
	if (!Snapshot.bSucceeded)
	{
		return;
	}
	UpdateDayNight(Snapshot.CampaignTimeUtc);
	for (const FStrategicRegionView& Region : Snapshot.Regions)
	{
		AddRegionPressure(Region);
	}
	for (const FStrategicGlobeMarkerView& Marker : Snapshot.GlobeMarkers)
	{
		switch (Marker.Type)
		{
		case EStrategicGlobeMarkerType::Base:
			BaseMarkerData.Add(Marker);
			AddMarker(BaseMarkers, Marker);
			break;
		case EStrategicGlobeMarkerType::Craft:
			CraftMarkerData.Add(Marker);
			AddMarker(CraftMarkers, Marker);
			break;
		case EStrategicGlobeMarkerType::Contact:
			ContactMarkerData.Add(Marker);
			AddMarker(ContactMarkers, Marker);
			break;
		case EStrategicGlobeMarkerType::Site:
			SiteMarkerData.Add(Marker);
			AddMarker(SiteMarkers, Marker);
			break;
		default:
			break;
		}
	}
	for (const FStrategicGlobeRouteView& Route : Snapshot.GlobeRoutes)
	{
		AddRoute(Route);
	}
}

void AUEGTStrategicGlobeActor::SetPresentationEnabled(const bool bEnabled)
{
	SetActorHiddenInGame(!bEnabled);
	SetActorEnableCollision(bEnabled);
}

bool AUEGTStrategicGlobeActor::ResolveHit(const FHitResult& Hit, FUEGTStrategicGlobeHit& OutHit) const
{
	OutHit = FUEGTStrategicGlobeHit();
	const UPrimitiveComponent* Component = Hit.GetComponent();
	const int32 Index = Hit.Item;
	const TArray<FStrategicGlobeMarkerView>* Data = nullptr;
	if (Component == BaseMarkers)
	{
		Data = &BaseMarkerData;
	}
	else if (Component == CraftMarkers)
	{
		Data = &CraftMarkerData;
	}
	else if (Component == ContactMarkers)
	{
		Data = &ContactMarkerData;
	}
	else if (Component == SiteMarkers)
	{
		Data = &SiteMarkerData;
	}
	if (Data == nullptr || !Data->IsValidIndex(Index))
	{
		return false;
	}
	OutHit.bHit = true;
	OutHit.Marker = (*Data)[Index];
	return true;
}

int32 AUEGTStrategicGlobeActor::GetRenderedBaseCount() const
{
	return BaseMarkers != nullptr ? BaseMarkers->GetInstanceCount() : 0;
}

int32 AUEGTStrategicGlobeActor::GetRenderedCraftCount() const
{
	return CraftMarkers != nullptr ? CraftMarkers->GetInstanceCount() : 0;
}

int32 AUEGTStrategicGlobeActor::GetRenderedContactCount() const
{
	return ContactMarkers != nullptr ? ContactMarkers->GetInstanceCount() : 0;
}

int32 AUEGTStrategicGlobeActor::GetRenderedSiteCount() const
{
	return SiteMarkers != nullptr ? SiteMarkers->GetInstanceCount() : 0;
}

int32 AUEGTStrategicGlobeActor::GetRenderedRoutePointCount() const
{
	return (PlayerRoutePoints != nullptr ? PlayerRoutePoints->GetInstanceCount() : 0)
		+ (AdversaryRoutePoints != nullptr ? AdversaryRoutePoints->GetInstanceCount() : 0);
}

int32 AUEGTStrategicGlobeActor::GetRenderedReferencePointCount() const
{
	return ReferenceGridPoints != nullptr ? ReferenceGridPoints->GetInstanceCount() : 0;
}

int32 AUEGTStrategicGlobeActor::GetRenderedTerminatorPointCount() const
{
	return TerminatorPoints != nullptr ? TerminatorPoints->GetInstanceCount() : 0;
}

bool AUEGTStrategicGlobeActor::GetRenderedTerminatorPointLocalPosition(
	const int32 Index,
	FVector& OutPosition) const
{
	FTransform InstanceTransform;
	if (TerminatorPoints == nullptr
		|| !TerminatorPoints->GetInstanceTransform(Index, InstanceTransform, false))
	{
		OutPosition = FVector::ZeroVector;
		return false;
	}
	OutPosition = InstanceTransform.GetLocation();
	return true;
}

int32 AUEGTStrategicGlobeActor::GetRenderedRegionPressurePointCount(
	const EUEGTRegionalPressureTier Tier) const
{
	const UInstancedStaticMeshComponent* Component = GetRegionPressureComponent(Tier);
	return Component != nullptr ? Component->GetInstanceCount() : 0;
}

bool AUEGTStrategicGlobeActor::GetRenderedRegionPressurePointLocalPosition(
	const EUEGTRegionalPressureTier Tier,
	const int32 Index,
	FVector& OutPosition) const
{
	const UInstancedStaticMeshComponent* Component = GetRegionPressureComponent(Tier);
	FTransform InstanceTransform;
	if (Component == nullptr || !Component->GetInstanceTransform(Index, InstanceTransform, false))
	{
		OutPosition = FVector::ZeroVector;
		return false;
	}
	OutPosition = InstanceTransform.GetLocation();
	return true;
}
