#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "Sound/SoundWaveProcedural.h"

#include "UEGTGeneratedSoundWave.generated.h"

struct FUEGTGeneratedAudio;

/** A finite PCM voice with an independent render cursor for each playback. */
UCLASS(Transient)
class UUEGTGeneratedSoundWave final : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	UUEGTGeneratedSoundWave(const FObjectInitializer& ObjectInitializer);
	static UUEGTGeneratedSoundWave* Create(UObject* Outer, FUEGTGeneratedAudio&& Audio);
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;

private:
	TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> Samples;
};
