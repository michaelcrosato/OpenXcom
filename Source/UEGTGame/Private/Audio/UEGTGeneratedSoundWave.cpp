// Copyright 2026 UEGT contributors. MIT License.

#include "Audio/UEGTGeneratedSoundWave.h"

#include "Audio/UEGTAudioSynthesisService.h"
#include "Sound/SoundGenerator.h"

namespace UEGTGeneratedSoundWavePrivate
{
	class FFinitePcmGenerator final : public ISoundGenerator
	{
	public:
		explicit FFinitePcmGenerator(TSharedRef<const TArray<int16>, ESPMode::ThreadSafe> InSamples)
			: Samples(MoveTemp(InSamples))
		{
		}

		virtual int32 OnGenerateAudio(float* OutAudio, const int32 NumSamples) override
		{
			const int32 Count = FMath::Min(NumSamples, Samples->Num() - NextSample);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				OutAudio[Index] = static_cast<float>((*Samples)[NextSample + Index]) / 32768.0f;
			}
			if (Count < NumSamples)
			{
				FMemory::Memzero(OutAudio + Count, (NumSamples - Count) * sizeof(float));
			}
			NextSample += Count;
			return Count;
		}

		virtual bool IsFinished() const override { return NextSample == Samples->Num(); }
		virtual int32 GetNumChannels() const override { return 1; }

	private:
		TSharedRef<const TArray<int16>, ESPMode::ThreadSafe> Samples;
		int32 NextSample = 0;
	};
}

UUEGTGeneratedSoundWave::UUEGTGeneratedSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UUEGTGeneratedSoundWave* UUEGTGeneratedSoundWave::Create(UObject* Outer, FUEGTGeneratedAudio&& Audio)
{
	if (!Audio.IsValid())
	{
		return nullptr;
	}
	UUEGTGeneratedSoundWave* Wave = NewObject<UUEGTGeneratedSoundWave>(Outer);
	Wave->NumChannels = Audio.NumChannels;
	Wave->SetSampleRate(Audio.SampleRate);
	Wave->SetNumFrames(Audio.NumFrames);
	Wave->Duration = static_cast<float>(Audio.NumFrames) / Audio.SampleRate;
	Wave->SoundGroup = SOUNDGROUP_UI;
	Wave->bLooping = false;
	Wave->Samples = MakeShared<TArray<int16>, ESPMode::ThreadSafe>(MoveTemp(Audio.Samples));
	return Wave;
}

ISoundGeneratorPtr UUEGTGeneratedSoundWave::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	if (!Samples.IsValid())
	{
		return nullptr;
	}
	return MakeShared<UEGTGeneratedSoundWavePrivate::FFinitePcmGenerator, ESPMode::ThreadSafe>(Samples.ToSharedRef());
}
