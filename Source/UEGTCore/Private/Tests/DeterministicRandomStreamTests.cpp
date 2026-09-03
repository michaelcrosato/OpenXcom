// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Determinism/DeterministicRandomStream.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDeterministicRandomGoldenSequenceTest,
	"UEGT.Core.Determinism.GoldenSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeterministicRandomGoldenSequenceTest::RunTest(const FString& Parameters)
{
	const uint64 Expected[] = {
		0x08328D7F03BCEC1AULL,
		0x077E7279E17AB6CDULL,
		0x0C4E098F541BB09EULL,
		0xD861FCF47B8B124EULL,
		0xCE980BC1DBB30CBBULL
	};

	FDeterministicRandomStream Stream(42);
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Expected)); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Golden draw %d remains save-compatible"), Index), Stream.NextUInt64(), Expected[Index]);
	}
	TestEqual(TEXT("Draw count records every consumed value"), Stream.DrawCount, int64(UE_ARRAY_COUNT(Expected)));

	Stream.Initialize(42);
	TestEqual(TEXT("Reinitializing reproduces the sequence"), Stream.NextUInt64(), Expected[0]);
	TestEqual(TEXT("Reinitializing resets draw count"), Stream.DrawCount, int64(1));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDeterministicRandomRangeTest,
	"UEGT.Core.Determinism.InclusiveRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeterministicRandomRangeTest::RunTest(const FString& Parameters)
{
	FDeterministicRandomStream First(8675309);
	FDeterministicRandomStream Second(8675309);
	TSet<int32> Observed;

	for (int32 DrawIndex = 0; DrawIndex < 10000; ++DrawIndex)
	{
		const int32 FirstValue = First.NextIntInclusive(-7, 13);
		const int32 SecondValue = Second.NextIntInclusive(-7, 13);
		TestTrue(TEXT("Inclusive range never underflows"), FirstValue >= -7);
		TestTrue(TEXT("Inclusive range never overflows"), FirstValue <= 13);
		TestEqual(TEXT("Equal seeds remain deterministic"), FirstValue, SecondValue);
		Observed.Add(FirstValue);
	}

	TestEqual(TEXT("Representative sample reaches every value"), Observed.Num(), 21);
	const int32 ReversedRangeValue = First.NextIntInclusive(10, -10);
	TestTrue(TEXT("Reversed bounds are normalized"), ReversedRangeValue >= -10 && ReversedRangeValue <= 10);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDeterministicRandomStateTest,
	"UEGT.Core.Determinism.SaveAndFork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeterministicRandomStateTest::RunTest(const FString& Parameters)
{
	FDeterministicRandomStream Parent(123456);
	Parent.NextUInt64();
	const int64 ParentDrawCount = Parent.DrawCount;

	FDeterministicRandomStream SavedCopy = Parent;
	TestEqual(TEXT("Copied save state resumes the exact sequence"), Parent.NextUInt64(), SavedCopy.NextUInt64());

	Parent = SavedCopy;
	const FDeterministicRandomStream FirstChild = Parent.Fork(1001);
	const FDeterministicRandomStream MatchingChild = Parent.Fork(1001);
	const FDeterministicRandomStream DifferentChild = Parent.Fork(1002);
	TestEqual(TEXT("Fork does not consume the parent"), Parent.DrawCount, ParentDrawCount + 1);

	FDeterministicRandomStream MutableFirstChild = FirstChild;
	FDeterministicRandomStream MutableMatchingChild = MatchingChild;
	FDeterministicRandomStream MutableDifferentChild = DifferentChild;
	TestEqual(TEXT("Same salt creates the same child"), MutableFirstChild.NextUInt64(), MutableMatchingChild.NextUInt64());
	TestNotEqual(TEXT("Different salt creates a distinct child"), MutableFirstChild.NextUInt64(), MutableDifferentChild.NextUInt64());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDeterministicRandomBoundaryTest,
	"UEGT.Core.Determinism.Boundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeterministicRandomBoundaryTest::RunTest(const FString& Parameters)
{
	FDeterministicRandomStream Stream(7);
	const uint64 ValidState = Stream.GetStateForSave();
	Stream.DrawCount = MAX_int64;
	TestFalse(TEXT("A stream at the terminal draw count is invalid"), Stream.IsValid());

	FDeterministicRandomStream Restored;
	TestFalse(TEXT("A save at the terminal draw count cannot be restored"),
		Restored.RestoreFromSave(7, MAX_int64, ValidState));
	TestTrue(TEXT("A save with one draw remaining can be restored"),
		Restored.RestoreFromSave(7, MAX_int64 - 1, ValidState));
	TestTrue(TEXT("A restored stream with one draw remaining is valid"), Restored.IsValid());

	Stream.NextUInt64();
	Stream.NextUInt64();
	TestEqual(TEXT("Terminal draws saturate instead of wrapping the save counter"), Stream.DrawCount, MAX_int64);
	TestFalse(TEXT("A saturated stream remains invalid for save validation"), Stream.IsValid());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDeterministicRandomProbabilityTest,
	"UEGT.Core.Determinism.Probability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDeterministicRandomProbabilityTest::RunTest(const FString& Parameters)
{
	FDeterministicRandomStream Stream(99);
	for (int32 Index = 0; Index < 256; ++Index)
	{
		const double Unit = Stream.NextUnitDouble();
		TestTrue(TEXT("Unit draw is at least zero"), Unit >= 0.0);
		TestTrue(TEXT("Unit draw is below one"), Unit < 1.0);
	}

	TestFalse(TEXT("Zero probability never succeeds"), Stream.Chance(0.0));
	TestTrue(TEXT("Full probability always succeeds"), Stream.Chance(1.0));
	TestFalse(TEXT("Negative probability clamps to zero"), Stream.Chance(-5.0));
	TestTrue(TEXT("Probability above one clamps to one"), Stream.Chance(5.0));

	return true;
}

#endif
