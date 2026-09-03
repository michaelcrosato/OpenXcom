// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Strategic/StrategicClock.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStrategicTimestampTest,
	"UEGT.Core.StrategicClock.Timestamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStrategicTimestampTest::RunTest(const FString& Parameters)
{
	const FStrategicTimestamp DefaultTimestamp;
	TestEqual(TEXT("Original campaign starts in 2035"), DefaultTimestamp.Utc.GetYear(), 2035);
	TestEqual(TEXT("Original campaign starts at noon UTC"), DefaultTimestamp.Utc.GetHour(), 12);
	TestTrue(TEXT("Default timestamp is usable"), DefaultTimestamp.IsUsable());
	TestEqual(TEXT("Noon is halfway through the UTC day"), DefaultTimestamp.GetDayFraction(), 0.5);

	const FStrategicTimestamp EveningTimestamp(FDateTime(2035, 4, 12, 18, 0, 0));
	TestEqual(TEXT("18:00 is three quarters through the UTC day"), EveningTimestamp.GetDayFraction(), 0.75);
	TestFalse(TEXT("Minimum sentinel is rejected"), FStrategicTimestamp(FDateTime::MinValue()).IsUsable());
	TestFalse(TEXT("Maximum sentinel is rejected"), FStrategicTimestamp(FDateTime::MaxValue()).IsUsable());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStrategicClockRateTest,
	"UEGT.Core.StrategicClock.Rates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStrategicClockRateTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Paused rate has no duration"), FStrategicClock::GetAdvanceForRate(EStrategicTimeRate::Paused).GetTicks(), int64(0));
	TestEqual(TEXT("Five-second rate duration"), FStrategicClock::GetAdvanceForRate(EStrategicTimeRate::FiveSeconds).GetTicks(), 5 * ETimespan::TicksPerSecond);
	TestEqual(TEXT("One-minute rate duration"), FStrategicClock::GetAdvanceForRate(EStrategicTimeRate::OneMinute).GetTicks(), ETimespan::TicksPerMinute);
	TestEqual(TEXT("Five-minute rate duration"), FStrategicClock::GetAdvanceForRate(EStrategicTimeRate::FiveMinutes).GetTicks(), 5 * ETimespan::TicksPerMinute);
	TestEqual(TEXT("Thirty-minute rate duration"), FStrategicClock::GetAdvanceForRate(EStrategicTimeRate::ThirtyMinutes).GetTicks(), 30 * ETimespan::TicksPerMinute);
	TestEqual(TEXT("One-hour rate duration"), FStrategicClock::GetAdvanceForRate(EStrategicTimeRate::OneHour).GetTicks(), ETimespan::TicksPerHour);
	TestEqual(TEXT("One-day rate duration"), FStrategicClock::GetAdvanceForRate(EStrategicTimeRate::OneDay).GetTicks(), ETimespan::TicksPerDay);

	FStrategicTimestamp Timestamp(FDateTime(2035, 1, 1, 12, 0, 0));
	int32 ObservedSlices = 0;
	const int32 ExecutedSlices = FStrategicClock::AdvanceRate(
		Timestamp,
		EStrategicTimeRate::OneMinute,
		[&ObservedSlices](const FStrategicTimeSlice&) { ++ObservedSlices; });
	TestEqual(TEXT("A minute is divided into twelve deterministic slices"), ExecutedSlices, 12);
	TestEqual(TEXT("Every executed slice is observable"), ObservedSlices, 12);
	TestEqual(TEXT("Minute rate advances the timestamp"), Timestamp.Utc, FDateTime(2035, 1, 1, 12, 1, 0));

	const int32 PausedSlices = FStrategicClock::AdvanceRate(
		Timestamp,
		EStrategicTimeRate::Paused,
		[](const FStrategicTimeSlice&) {});
	TestEqual(TEXT("Paused rate executes no slices"), PausedSlices, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStrategicClockMarkersTest,
	"UEGT.Core.StrategicClock.Markers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStrategicClockMarkersTest::RunTest(const FString& Parameters)
{
	FStrategicTimeSlice ObservedSlice;
	FStrategicTimestamp Timestamp(FDateTime(2035, 1, 31, 23, 59, 55));
	FStrategicClock::AdvanceRate(
		Timestamp,
		EStrategicTimeRate::FiveSeconds,
		[&ObservedSlice](const FStrategicTimeSlice& Slice) { ObservedSlice = Slice; });

	const EStrategicTimeMarker ExpectedMarkers[] = {
		EStrategicTimeMarker::SimulationSlice,
		EStrategicTimeMarker::TenMinutes,
		EStrategicTimeMarker::HalfHour,
		EStrategicTimeMarker::Hour,
		EStrategicTimeMarker::Day,
		EStrategicTimeMarker::Month
	};
	const int32 ExpectedMarkerCount = static_cast<int32>(UE_ARRAY_COUNT(ExpectedMarkers));

	TestEqual(TEXT("Month transition exposes every crossed milestone"), ObservedSlice.Markers.Num(), ExpectedMarkerCount);
	for (int32 Index = 0; Index < ObservedSlice.Markers.Num() && Index < ExpectedMarkerCount; ++Index)
	{
		TestTrue(FString::Printf(TEXT("Marker %d uses deterministic fine-to-coarse ordering"), Index), ObservedSlice.Markers[Index] == ExpectedMarkers[Index]);
	}
	TestEqual(TEXT("January advances to February"), Timestamp.Utc, FDateTime(2035, 2, 1, 0, 0, 0));

	Timestamp = FStrategicTimestamp(FDateTime(2035, 12, 31, 23, 59, 55));
	FStrategicClock::AdvanceRate(
		Timestamp,
		EStrategicTimeRate::FiveSeconds,
		[&ObservedSlice](const FStrategicTimeSlice& Slice) { ObservedSlice = Slice; });
	TestTrue(TEXT("Year transition reports the year marker"), ObservedSlice.Contains(EStrategicTimeMarker::Year));
	TestEqual(TEXT("Year transition advances to 2036"), Timestamp.Utc, FDateTime(2036, 1, 1, 0, 0, 0));

	Timestamp = FStrategicTimestamp(FDateTime(2036, 2, 28, 23, 59, 55));
	FStrategicClock::AdvanceRate(
		Timestamp,
		EStrategicTimeRate::FiveSeconds,
		[](const FStrategicTimeSlice&) {});
	TestEqual(TEXT("Engine calendar retains the 2036 leap day"), Timestamp.Utc, FDateTime(2036, 2, 29, 0, 0, 0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStrategicClockStopTest,
	"UEGT.Core.StrategicClock.StopAtDecision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStrategicClockStopTest::RunTest(const FString& Parameters)
{
	FStrategicTimestamp Timestamp(FDateTime(2035, 1, 1, 12, 0, 0));
	int32 ObservedSlices = 0;
	bool bStopRequested = false;

	const int32 ExecutedSlices = FStrategicClock::AdvanceRate(
		Timestamp,
		EStrategicTimeRate::OneMinute,
		[&ObservedSlices, &bStopRequested](const FStrategicTimeSlice&)
		{
			++ObservedSlices;
			bStopRequested = ObservedSlices == 3;
		},
		[&bStopRequested] { return bStopRequested; });

	TestEqual(TEXT("Decision stops before the fourth slice"), ExecutedSlices, 3);
	TestEqual(TEXT("Stopped clock preserves completed simulation time"), Timestamp.Utc, FDateTime(2035, 1, 1, 12, 0, 15));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStrategicClockSliceCountBoundaryTest,
	"UEGT.Core.StrategicClock.SliceCountBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStrategicClockSliceCountBoundaryTest::RunTest(const FString& Parameters)
{
	FStrategicTimestamp Timestamp(FDateTime(2035, 1, 1, 12, 0, 0));
	const FDateTime InitialTimestamp = Timestamp.Utc;
	const int64 QuantumTicks = FStrategicClock::GetSimulationQuantum().GetTicks();
	const FTimespan TooManySlices(static_cast<int64>(MAX_int32) * QuantumTicks + 1);
	int32 ObservedSlices = 0;

	const int32 ExecutedSlices = FStrategicClock::AdvanceBy(
		Timestamp,
		TooManySlices,
		[&ObservedSlices](const FStrategicTimeSlice&) { ++ObservedSlices; });

	TestEqual(TEXT("An unrepresentable slice count is rejected before simulation"), ExecutedSlices, 0);
	TestEqual(TEXT("An oversized clock request preserves the timestamp"), Timestamp.Utc, InitialTimestamp);
	TestEqual(TEXT("An oversized clock request does not invoke the slice handler"), ObservedSlices, 0);

	return true;
}

#endif
