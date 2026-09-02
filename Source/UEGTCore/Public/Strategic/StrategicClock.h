#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Templates/Function.h"

#include "StrategicClock.generated.h"

/** Amount of simulated campaign time requested by one player-facing clock pulse. */
UENUM(BlueprintType)
enum class EStrategicTimeRate : uint8
{
	Paused UMETA(DisplayName = "Paused"),
	FiveSeconds UMETA(DisplayName = "5 Seconds"),
	OneMinute UMETA(DisplayName = "1 Minute"),
	FiveMinutes UMETA(DisplayName = "5 Minutes"),
	ThirtyMinutes UMETA(DisplayName = "30 Minutes"),
	OneHour UMETA(DisplayName = "1 Hour"),
	OneDay UMETA(DisplayName = "1 Day")
};

/** Milestones crossed by a deterministic strategic simulation slice. */
UENUM(BlueprintType)
enum class EStrategicTimeMarker : uint8
{
	SimulationSlice UMETA(DisplayName = "Simulation Slice"),
	TenMinutes UMETA(DisplayName = "10 Minutes"),
	HalfHour UMETA(DisplayName = "Half Hour"),
	Hour UMETA(DisplayName = "Hour"),
	Day UMETA(DisplayName = "Day"),
	Month UMETA(DisplayName = "Month"),
	Year UMETA(DisplayName = "Year")
};

/** Save-ready UTC timestamp for the strategic simulation. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicTimestamp
{
	GENERATED_BODY()

	FStrategicTimestamp();
	explicit FStrategicTimestamp(const FDateTime& InUtc);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "UEGT|Strategic Time")
	FDateTime Utc;

	/** The sentinel values are reserved for invalid/uninitialized imported data. */
	bool IsUsable() const;

	/** Fraction of the UTC day in [0, 1), useful for original globe lighting. */
	double GetDayFraction() const;
};

/** Immutable description of one simulation slice and every milestone it crossed. */
USTRUCT(BlueprintType)
struct UEGTCORE_API FStrategicTimeSlice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic Time")
	FDateTime PreviousUtc;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic Time")
	FDateTime CurrentUtc;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Strategic Time")
	TArray<EStrategicTimeMarker> Markers;

	bool Contains(EStrategicTimeMarker Marker) const;
};

/**
 * Original event-oriented campaign clock.
 *
 * Large time-rate requests are divided into deterministic slices so moving
 * entities and scheduled events cannot be skipped. Consumers may stop after
 * any complete slice when an encounter or decision pauses campaign time.
 */
class UEGTCORE_API FStrategicClock final
{
public:
	static FTimespan GetSimulationQuantum();
	static FTimespan GetAdvanceForRate(EStrategicTimeRate Rate);

	static int32 AdvanceRate(
		FStrategicTimestamp& Timestamp,
		EStrategicTimeRate Rate,
		TFunctionRef<void(const FStrategicTimeSlice&)> HandleSlice);

	static int32 AdvanceRate(
		FStrategicTimestamp& Timestamp,
		EStrategicTimeRate Rate,
		TFunctionRef<void(const FStrategicTimeSlice&)> HandleSlice,
		TFunctionRef<bool()> ShouldStop);

	static int32 AdvanceBy(
		FStrategicTimestamp& Timestamp,
		const FTimespan& Amount,
		TFunctionRef<void(const FStrategicTimeSlice&)> HandleSlice);

	static int32 AdvanceBy(
		FStrategicTimestamp& Timestamp,
		const FTimespan& Amount,
		TFunctionRef<void(const FStrategicTimeSlice&)> HandleSlice,
		TFunctionRef<bool()> ShouldStop);

private:
	static TArray<EStrategicTimeMarker> FindCrossedMarkers(const FDateTime& PreviousUtc, const FDateTime& CurrentUtc);
};
