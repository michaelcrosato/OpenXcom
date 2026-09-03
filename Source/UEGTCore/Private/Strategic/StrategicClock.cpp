// Copyright 2026 UEGT contributors. MIT License.

#include "Strategic/StrategicClock.h"

namespace StrategicClockPrivate
{
	constexpr int64 TenMinuteTicks = 10 * ETimespan::TicksPerMinute;
	constexpr int64 HalfHourTicks = 30 * ETimespan::TicksPerMinute;
}

FStrategicTimestamp::FStrategicTimestamp()
	: Utc(2035, 1, 1, 12, 0, 0)
{
}

FStrategicTimestamp::FStrategicTimestamp(const FDateTime& InUtc)
	: Utc(InUtc)
{
}

bool FStrategicTimestamp::IsUsable() const
{
	return Utc > FDateTime::MinValue() && Utc < FDateTime::MaxValue();
}

double FStrategicTimestamp::GetDayFraction() const
{
	const int64 TicksWithinDay = Utc.GetTicks() % ETimespan::TicksPerDay;
	return static_cast<double>(TicksWithinDay) / static_cast<double>(ETimespan::TicksPerDay);
}

bool FStrategicTimeSlice::Contains(const EStrategicTimeMarker Marker) const
{
	return Markers.Contains(Marker);
}

FTimespan FStrategicClock::GetSimulationQuantum()
{
	return FTimespan::FromSeconds(5.0);
}

FTimespan FStrategicClock::GetAdvanceForRate(const EStrategicTimeRate Rate)
{
	switch (Rate)
	{
	case EStrategicTimeRate::FiveSeconds:
		return FTimespan::FromSeconds(5.0);
	case EStrategicTimeRate::OneMinute:
		return FTimespan::FromMinutes(1.0);
	case EStrategicTimeRate::FiveMinutes:
		return FTimespan::FromMinutes(5.0);
	case EStrategicTimeRate::ThirtyMinutes:
		return FTimespan::FromMinutes(30.0);
	case EStrategicTimeRate::OneHour:
		return FTimespan::FromHours(1.0);
	case EStrategicTimeRate::OneDay:
		return FTimespan::FromDays(1.0);
	case EStrategicTimeRate::Paused:
	default:
		return FTimespan::Zero();
	}
}

int32 FStrategicClock::AdvanceRate(
	FStrategicTimestamp& Timestamp,
	const EStrategicTimeRate Rate,
	const TFunctionRef<void(const FStrategicTimeSlice&)> HandleSlice)
{
	return AdvanceBy(Timestamp, GetAdvanceForRate(Rate), HandleSlice);
}

int32 FStrategicClock::AdvanceRate(
	FStrategicTimestamp& Timestamp,
	const EStrategicTimeRate Rate,
	const TFunctionRef<void(const FStrategicTimeSlice&)> HandleSlice,
	const TFunctionRef<bool()> ShouldStop)
{
	return AdvanceBy(Timestamp, GetAdvanceForRate(Rate), HandleSlice, ShouldStop);
}

int32 FStrategicClock::AdvanceBy(
	FStrategicTimestamp& Timestamp,
	const FTimespan& Amount,
	const TFunctionRef<void(const FStrategicTimeSlice&)> HandleSlice)
{
	return AdvanceBy(Timestamp, Amount, HandleSlice, [] { return false; });
}

int32 FStrategicClock::AdvanceBy(
	FStrategicTimestamp& Timestamp,
	const FTimespan& Amount,
	const TFunctionRef<void(const FStrategicTimeSlice&)> HandleSlice,
	const TFunctionRef<bool()> ShouldStop)
{
	if (!Timestamp.IsUsable() || Amount <= FTimespan::Zero())
	{
		return 0;
	}

	const int64 QuantumTicks = GetSimulationQuantum().GetTicks();
	int64 RemainingTicks = Amount.GetTicks();
	const int64 MaximumSlices = RemainingTicks / QuantumTicks
		+ (RemainingTicks % QuantumTicks == 0 ? 0 : 1);
	if (MaximumSlices > MAX_int32)
	{
		return 0;
	}
	int32 ExecutedSlices = 0;

	while (RemainingTicks > 0 && !ShouldStop())
	{
		const int64 SliceTicks = FMath::Min(RemainingTicks, QuantumTicks);
		if (Timestamp.Utc.GetTicks() > FDateTime::MaxValue().GetTicks() - SliceTicks)
		{
			break;
		}

		FStrategicTimeSlice Slice;
		Slice.PreviousUtc = Timestamp.Utc;
		Timestamp.Utc = FDateTime(Timestamp.Utc.GetTicks() + SliceTicks);
		Slice.CurrentUtc = Timestamp.Utc;
		Slice.Markers = FindCrossedMarkers(Slice.PreviousUtc, Slice.CurrentUtc);

		HandleSlice(Slice);
		RemainingTicks -= SliceTicks;
		++ExecutedSlices;
	}

	return ExecutedSlices;
}

TArray<EStrategicTimeMarker> FStrategicClock::FindCrossedMarkers(
	const FDateTime& PreviousUtc,
	const FDateTime& CurrentUtc)
{
	TArray<EStrategicTimeMarker> Markers;
	Markers.Reserve(7);
	Markers.Add(EStrategicTimeMarker::SimulationSlice);

	if (PreviousUtc.GetTicks() / StrategicClockPrivate::TenMinuteTicks != CurrentUtc.GetTicks() / StrategicClockPrivate::TenMinuteTicks)
	{
		Markers.Add(EStrategicTimeMarker::TenMinutes);
	}
	if (PreviousUtc.GetTicks() / StrategicClockPrivate::HalfHourTicks != CurrentUtc.GetTicks() / StrategicClockPrivate::HalfHourTicks)
	{
		Markers.Add(EStrategicTimeMarker::HalfHour);
	}
	if (PreviousUtc.GetTicks() / ETimespan::TicksPerHour != CurrentUtc.GetTicks() / ETimespan::TicksPerHour)
	{
		Markers.Add(EStrategicTimeMarker::Hour);
	}
	if (PreviousUtc.GetYear() != CurrentUtc.GetYear()
		|| PreviousUtc.GetMonth() != CurrentUtc.GetMonth()
		|| PreviousUtc.GetDay() != CurrentUtc.GetDay())
	{
		Markers.Add(EStrategicTimeMarker::Day);
	}
	if (PreviousUtc.GetYear() != CurrentUtc.GetYear() || PreviousUtc.GetMonth() != CurrentUtc.GetMonth())
	{
		Markers.Add(EStrategicTimeMarker::Month);
	}
	if (PreviousUtc.GetYear() != CurrentUtc.GetYear())
	{
		Markers.Add(EStrategicTimeMarker::Year);
	}

	return Markers;
}
