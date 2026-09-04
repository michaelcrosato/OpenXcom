#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"

#include "DeterministicRandomStream.generated.h"

/**
 * Save-ready deterministic random stream owned by simulation state.
 *
 * The algorithm is deliberately independent of platform CRTs and Unreal's
 * evolving utility implementations. Its sequence is protected by golden
 * tests because changing it invalidates replay and save determinism.
 */
USTRUCT(BlueprintType)
struct UEGTCORE_API FDeterministicRandomStream
{
	GENERATED_BODY()

	FDeterministicRandomStream();
	explicit FDeterministicRandomStream(int64 Seed);

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UEGT|Determinism")
	int64 InitialSeed = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = "UEGT|Determinism")
	int64 DrawCount = 0;

	/** Reinitializes the exact sequence and resets DrawCount. */
	void Initialize(int64 Seed);

	bool IsValid() const;

	/** Exact algorithm state used by the versioned campaign-save codec. */
	uint64 GetStateForSave() const;

	/** Restores a previously validated save snapshot without replaying draws. */
	bool RestoreFromSave(int64 SavedInitialSeed, int64 SavedDrawCount, uint64 SavedState);

	uint64 NextUInt64();
	uint32 NextUInt32();
	double NextUnitDouble();
	/** Attempts an inclusive integer draw without consuming past the save-safe boundary. */
	bool TryNextIntInclusive(int32 Minimum, int32 Maximum, int32& OutValue);
	int32 NextIntInclusive(int32 Minimum, int32 Maximum);
	bool Chance(double Probability);

	/** Creates a deterministic child stream without consuming the parent. */
	FDeterministicRandomStream Fork(uint64 Salt) const;

private:
	UPROPERTY(SaveGame)
	uint64 State = 0;
};
