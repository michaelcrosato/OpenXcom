#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "Audio/UEGTAudioSynthesisService.h"
#include "Strategic/StrategicCommandService.h"
#include "TimerManager.h"

#include "UEGTAudioDirector.generated.h"

class UAudioComponent;
class USoundWaveProcedural;
class UWorld;

UENUM(BlueprintType)
enum class EUEGTAudioPresentationMode : uint8
{
	None,
	MainMenu,
	Strategic,
	Tactical,
	Debrief
};

USTRUCT(BlueprintType)
struct UEGTGAME_API FUEGTAudioDirectorDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	EUEGTAudioPresentationMode Mode = EUEGTAudioPresentationMode::None;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	EUEGTAudioCue LastCue = EUEGTAudioCue::InterfaceConfirm;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	int32 PlayRequests = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	int32 PlaybackComponentsCreated = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	int32 LastFrameCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	int32 LastQueuedBytes = 0;

	uint32 LastFingerprint = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	bool bLastPlaybackComponentCreated = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	bool bLastPlaybackStarted = false;

	UPROPERTY(BlueprintReadOnly, Category = "UEGT|Audio")
	bool bAmbientScheduled = false;
};

/** Owns transient procedural waves and maps presentation state to a restrained runtime mix. */
UCLASS(BlueprintType)
class UEGTGAME_API UUEGTAudioDirector final : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UWorld* InWorld);
	void Shutdown();

	UFUNCTION(BlueprintCallable, Category = "UEGT|Audio")
	bool PlayCue(EUEGTAudioCue Cue);

	UFUNCTION(BlueprintCallable, Category = "UEGT|Audio")
	void SetPresentationMode(EUEGTAudioPresentationMode Mode);

	UFUNCTION(BlueprintPure, Category = "UEGT|Audio")
	FUEGTAudioDirectorDiagnostics GetDiagnostics() const { return Diagnostics; }

	static EUEGTAudioCue SelectCommandCue(
		const FStrategicCommandResult& Result,
		bool bTacticalContext);
	static FName GetModeName(EUEGTAudioPresentationMode Mode);

protected:
	virtual void BeginDestroy() override;

private:
	bool PlayGeneratedCue(EUEGTAudioCue Cue, bool bAmbient);
	void PlayAmbientPulse();
	void StopAmbient();
	void PruneFinishedAudio();
	static bool TryGetAmbientCue(EUEGTAudioPresentationMode Mode, EUEGTAudioCue& OutCue);

	TWeakObjectPtr<UWorld> PlaybackWorld;
	FTimerHandle AmbientTimerHandle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> ForegroundComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundWaveProcedural>> ForegroundWaves;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> AmbientComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundWaveProcedural>> AmbientWaves;

	UPROPERTY(Transient)
	FUEGTAudioDirectorDiagnostics Diagnostics;
};
