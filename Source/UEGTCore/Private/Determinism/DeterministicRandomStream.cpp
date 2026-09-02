// Copyright 2026 UEGT contributors. MIT License.

#include "Determinism/DeterministicRandomStream.h"

namespace DeterministicRandomPrivate
{
	constexpr uint64 SeedScrambler = 0x9E3779B97F4A7C15ULL;
	constexpr uint64 NonZeroFallback = 0xD1B54A32D192ED03ULL;
	constexpr uint64 OutputMultiplier = 0x2545F4914F6CDD1DULL;
	constexpr double Unit53Scale = 1.0 / 9007199254740992.0;
}

FDeterministicRandomStream::FDeterministicRandomStream()
{
	Initialize(0);
}

FDeterministicRandomStream::FDeterministicRandomStream(const int64 Seed)
{
	Initialize(Seed);
}

void FDeterministicRandomStream::Initialize(const int64 Seed)
{
	InitialSeed = Seed;
	DrawCount = 0;
	State = static_cast<uint64>(Seed) ^ DeterministicRandomPrivate::SeedScrambler;
	if (State == 0)
	{
		State = DeterministicRandomPrivate::NonZeroFallback;
	}
}

bool FDeterministicRandomStream::IsValid() const
{
	return State != 0 && DrawCount >= 0 && DrawCount < MAX_int64;
}

uint64 FDeterministicRandomStream::GetStateForSave() const
{
	return State;
}

bool FDeterministicRandomStream::RestoreFromSave(
	const int64 SavedInitialSeed,
	const int64 SavedDrawCount,
	const uint64 SavedState)
{
	if (SavedDrawCount < 0 || SavedDrawCount >= MAX_int64 || SavedState == 0)
	{
		return false;
	}

	InitialSeed = SavedInitialSeed;
	DrawCount = SavedDrawCount;
	State = SavedState;
	return true;
}

uint64 FDeterministicRandomStream::NextUInt64()
{
	uint64 Value = State;
	Value ^= Value >> 12;
	Value ^= Value << 25;
	Value ^= Value >> 27;
	State = Value;
	++DrawCount;
	return Value * DeterministicRandomPrivate::OutputMultiplier;
}

uint32 FDeterministicRandomStream::NextUInt32()
{
	return static_cast<uint32>(NextUInt64() >> 32);
}

double FDeterministicRandomStream::NextUnitDouble()
{
	return static_cast<double>(NextUInt64() >> 11) * DeterministicRandomPrivate::Unit53Scale;
}

int32 FDeterministicRandomStream::NextIntInclusive(int32 Minimum, int32 Maximum)
{
	if (Minimum > Maximum)
	{
		Swap(Minimum, Maximum);
	}

	const uint64 Width = static_cast<uint64>(static_cast<int64>(Maximum) - static_cast<int64>(Minimum)) + 1ULL;
	const uint64 RejectionThreshold = (0ULL - Width) % Width;
	uint64 Draw;
	do
	{
		Draw = NextUInt64();
	}
	while (Draw < RejectionThreshold);

	const int64 Result = static_cast<int64>(Minimum) + static_cast<int64>(Draw % Width);
	return static_cast<int32>(Result);
}

bool FDeterministicRandomStream::Chance(const double Probability)
{
	const double Draw = NextUnitDouble();
	return Draw < FMath::Clamp(Probability, 0.0, 1.0);
}

FDeterministicRandomStream FDeterministicRandomStream::Fork(const uint64 Salt) const
{
	uint64 ChildSeed = State ^ Salt ^ DeterministicRandomPrivate::NonZeroFallback;
	ChildSeed ^= ChildSeed >> 30;
	ChildSeed *= 0xBF58476D1CE4E5B9ULL;
	ChildSeed ^= ChildSeed >> 27;
	ChildSeed *= 0x94D049BB133111EBULL;
	ChildSeed ^= ChildSeed >> 31;
	return FDeterministicRandomStream(static_cast<int64>(ChildSeed));
}
