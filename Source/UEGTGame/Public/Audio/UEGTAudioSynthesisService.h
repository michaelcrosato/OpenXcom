#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"

#include "UEGTAudioSynthesisService.generated.h"

/** Original, nonverbal signal and ambience voices generated entirely at runtime. */
UENUM(BlueprintType)
enum class EUEGTAudioCue : uint8
{
	InterfaceConfirm,
	CommandAccepted,
	CommandRejected,
	StrategicAlert,
	InterceptionReady,
	TacticalDeployment,
	TacticalPlayerTurn,
	TacticalAdversaryTurn,
	DebriefSuccess,
	DebriefFailure,
	StrategicAmbience,
	TacticalAmbience
};

/** Immutable synthesis parameters for one presentation-only voice. */
USTRUCT(BlueprintType)
struct UEGTGAME_API FUEGTAudioCueDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	EUEGTAudioCue Cue = EUEGTAudioCue::InterfaceConfirm;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	FName DebugName;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	int32 SampleRate = 24000;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	int32 DurationMilliseconds = 180;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	float BaseFrequencyHz = 440.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	float SecondaryFrequencyHz = 660.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	float Gain = 0.15f;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	float AttackMilliseconds = 8.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	float ReleaseMilliseconds = 70.0f;

	/** Linear pitch travel from the beginning to the end of the voice. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	float PitchSweepRatio = 0.0f;

	/** Gentle amplitude movement; zero disables it. */
	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	float PulseRateHz = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	bool bAmbient = false;

	int32 GetExpectedFrameCount() const;
	bool IsValid() const;
};

/** Signed 16-bit mono PCM plus stable diagnostics used by playback and automation. */
USTRUCT(BlueprintType)
struct UEGTGAME_API FUEGTGeneratedAudio
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	EUEGTAudioCue Cue = EUEGTAudioCue::InterfaceConfirm;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	int32 SampleRate = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	int32 NumChannels = 1;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	int32 NumFrames = 0;

	TArray<int16> Samples;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	int32 PeakAbsoluteSample = 0;

	uint32 Fingerprint = 0;

	bool IsValid() const;
};

/** Deterministic PCM authoring for UEGT's original asset-independent audio palette. */
class UEGTGAME_API FUEGTAudioSynthesisService
{
public:
	static TArray<EUEGTAudioCue> GetCueTypes();
	static FUEGTAudioCueDefinition GetDefinition(EUEGTAudioCue Cue);
	static FUEGTGeneratedAudio Generate(EUEGTAudioCue Cue);
	static FName GetCueName(EUEGTAudioCue Cue);
};
