// Copyright 2026 UEGT contributors. MIT License.

#include "Audio/UEGTAudioDirector.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWaveProcedural.h"

namespace UEGTAudioDirectorPrivate
{
	constexpr float AmbientDuckLevel = 0.28f;
	constexpr float AmbientDuckAttackSeconds = 0.04f;
	constexpr float AmbientDuckReleaseSeconds = 0.16f;

	bool IsTacticalLocationEvent(const EStrategicEventType Type)
	{
		switch (Type)
		{
		case EStrategicEventType::TacticalDoorStateChanged:
		case EStrategicEventType::TacticalUnitMoved:
		case EStrategicEventType::TacticalAttackResolved:
		case EStrategicEventType::TacticalSignalProjected:
		case EStrategicEventType::TacticalBlastResolved:
		case EStrategicEventType::TacticalTerrainDamaged:
		case EStrategicEventType::TacticalTerrainDestroyed:
		case EStrategicEventType::TacticalDeviceDeployed:
		case EStrategicEventType::TacticalDeviceIntercepted:
		case EStrategicEventType::TacticalEnvironmentSuppressed:
		case EStrategicEventType::TacticalObjectiveProgressed:
		case EStrategicEventType::TacticalObjectiveContested:
		case EStrategicEventType::TacticalObjectiveCompleted:
		case EStrategicEventType::TacticalObjectiveFailed:
		case EStrategicEventType::TacticalUnitExtracted:
		case EStrategicEventType::TacticalAiDecisionMade:
			return true;
		default:
			return false;
		}
	}
}

void UUEGTAudioDirector::Initialize(UWorld* InWorld)
{
	Shutdown();
	PlaybackWorld = InWorld;
	Diagnostics = FUEGTAudioDirectorDiagnostics();
}

void UUEGTAudioDirector::Shutdown()
{
	StopAmbient();
	for (UAudioComponent* Component : ForegroundComponents)
	{
		if (IsValid(Component))
		{
			Component->Stop();
		}
	}
	ForegroundComponents.Reset();
	ForegroundWaves.Reset();
	TacticalPositionalAttenuation = nullptr;
	PlaybackWorld.Reset();
	Diagnostics.Mode = EUEGTAudioPresentationMode::None;
}

bool UUEGTAudioDirector::PlayCue(const EUEGTAudioCue Cue)
{
	return PlayGeneratedCue(Cue, false);
}

bool UUEGTAudioDirector::PlayCueAtLocation(
	const EUEGTAudioCue Cue,
	const FVector WorldLocation)
{
	return PlayGeneratedCue(Cue, false, &WorldLocation);
}

void UUEGTAudioDirector::SetPresentationMode(const EUEGTAudioPresentationMode Mode)
{
	if (Diagnostics.Mode == Mode)
	{
		return;
	}
	StopAmbient();
	Diagnostics.Mode = Mode;

	EUEGTAudioCue AmbientCue = EUEGTAudioCue::StrategicAmbience;
	if (!TryGetAmbientCue(Mode, AmbientCue))
	{
		return;
	}
	PlayGeneratedCue(AmbientCue, true);
	if (UWorld* World = PlaybackWorld.Get())
	{
		const FUEGTAudioCueDefinition Definition =
			FUEGTAudioSynthesisService::GetDefinition(AmbientCue);
		const float PulseInterval = FMath::Max(
			0.25f, static_cast<float>(Definition.DurationMilliseconds) / 1000.0f - 0.15f);
		World->GetTimerManager().SetTimer(
			AmbientTimerHandle,
			this,
			&UUEGTAudioDirector::PlayAmbientPulse,
			PulseInterval,
			true);
		Diagnostics.bAmbientScheduled = true;
	}
}

EUEGTAudioCue UUEGTAudioDirector::SelectCommandCue(
	const FStrategicCommandResult& Result,
	const bool bTacticalContext)
{
	if (!Result.bAccepted)
	{
		return EUEGTAudioCue::CommandRejected;
	}

	EUEGTAudioCue Selected = EUEGTAudioCue::CommandAccepted;
	int32 SelectedPriority = 0;
	const auto Select = [&Selected, &SelectedPriority](const EUEGTAudioCue Cue, const int32 Priority)
	{
		if (Priority > SelectedPriority)
		{
			Selected = Cue;
			SelectedPriority = Priority;
		}
	};
	for (const FStrategicEvent& Event : Result.Events)
	{
		switch (Event.Type)
		{
		case EStrategicEventType::CampaignLost:
		case EStrategicEventType::InterceptionDefeated:
		case EStrategicEventType::BaseAssaultBreached:
			Select(EUEGTAudioCue::DebriefFailure, 100);
			break;
		case EStrategicEventType::CampaignWon:
		case EStrategicEventType::BaseAssaultRepelled:
		case EStrategicEventType::InterceptionWon:
			Select(EUEGTAudioCue::DebriefSuccess, 95);
			break;
		case EStrategicEventType::TacticalBattleResolved:
		case EStrategicEventType::TacticalOperationResolved:
			Select(Event.bSuccessful
				? EUEGTAudioCue::DebriefSuccess
				: EUEGTAudioCue::DebriefFailure, 90);
			break;
		case EStrategicEventType::BaseAssaultStarted:
		case EStrategicEventType::StrategicContactDetected:
		case EStrategicEventType::InterceptionLost:
			Select(EUEGTAudioCue::StrategicAlert, 80);
			break;
		case EStrategicEventType::TacticalObjectiveFailed:
			Select(bTacticalContext
				? EUEGTAudioCue::CommandRejected
				: EUEGTAudioCue::StrategicAlert, 80);
			break;
		case EStrategicEventType::InterceptionReady:
			Select(EUEGTAudioCue::InterceptionReady, 75);
			break;
		case EStrategicEventType::TacticalBattleGenerated:
		case EStrategicEventType::TacticalDeploymentConfirmed:
			Select(EUEGTAudioCue::TacticalDeployment, 70);
			break;
		case EStrategicEventType::TacticalAiTurnCompleted:
			Select(EUEGTAudioCue::TacticalPlayerTurn, 65);
			break;
		case EStrategicEventType::TacticalTurnEnded:
			Select(EUEGTAudioCue::TacticalAdversaryTurn, 65);
			break;
		case EStrategicEventType::TacticalAttackResolved:
		case EStrategicEventType::TacticalBlastResolved:
			Select(EUEGTAudioCue::TacticalImpact, 72);
			break;
		case EStrategicEventType::TacticalSignalProjected:
			Select(EUEGTAudioCue::TacticalSignal, 73);
			break;
		default:
			break;
		}
	}
	return Selected;
}

FName UUEGTAudioDirector::GetModeName(const EUEGTAudioPresentationMode Mode)
{
	switch (Mode)
	{
	case EUEGTAudioPresentationMode::None: return TEXT("none");
	case EUEGTAudioPresentationMode::MainMenu: return TEXT("main-menu");
	case EUEGTAudioPresentationMode::Strategic: return TEXT("strategic");
	case EUEGTAudioPresentationMode::Tactical: return TEXT("tactical");
	case EUEGTAudioPresentationMode::Debrief: return TEXT("debrief");
	default: return TEXT("unknown");
	}
}

float UUEGTAudioDirector::GetAmbientDuckLevel()
{
	return UEGTAudioDirectorPrivate::AmbientDuckLevel;
}

float UUEGTAudioDirector::GetAmbientDuckAttackSeconds()
{
	return UEGTAudioDirectorPrivate::AmbientDuckAttackSeconds;
}

float UUEGTAudioDirector::GetAmbientDuckReleaseSeconds()
{
	return UEGTAudioDirectorPrivate::AmbientDuckReleaseSeconds;
}

bool UUEGTAudioDirector::TryGetLatestTacticalEventCell(
	const FStrategicCommandResult& Result,
	FIntVector& OutCell)
{
	OutCell = FIntVector(0, 0, 0);
	bool bFound = false;
	for (const FStrategicEvent& Event : Result.Events)
	{
		if (UEGTAudioDirectorPrivate::IsTacticalLocationEvent(Event.Type))
		{
			OutCell = FIntVector(Event.ToX, Event.ToY, Event.ToZ);
			bFound = true;
		}
	}
	return bFound;
}

void UUEGTAudioDirector::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

bool UUEGTAudioDirector::PlayGeneratedCue(
	const EUEGTAudioCue Cue,
	const bool bAmbient,
	const FVector* WorldLocation)
{
	PruneFinishedAudio();
	const FUEGTGeneratedAudio Generated = FUEGTAudioSynthesisService::Generate(Cue);
	++Diagnostics.PlayRequests;
	Diagnostics.LastCue = Cue;
	Diagnostics.LastFrameCount = Generated.NumFrames;
	Diagnostics.LastQueuedBytes = Generated.Samples.Num() * sizeof(int16);
	Diagnostics.LastFingerprint = Generated.Fingerprint;
	Diagnostics.bLastPlaybackComponentCreated = false;
	Diagnostics.bLastPlaybackStarted = false;
	Diagnostics.bLastPlaybackPositional = WorldLocation != nullptr;
	Diagnostics.LastPlaybackLocation = WorldLocation != nullptr
		? *WorldLocation
		: FVector::ZeroVector;
	UWorld* World = PlaybackWorld.Get();
	if (!Generated.IsValid() || World == nullptr
		|| (WorldLocation != nullptr
			&& (!FMath::IsFinite(WorldLocation->X)
				|| !FMath::IsFinite(WorldLocation->Y)
				|| !FMath::IsFinite(WorldLocation->Z))))
	{
		return false;
	}

	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	if (Wave == nullptr)
	{
		return false;
	}
	Wave->NumChannels = Generated.NumChannels;
	Wave->SetSampleRate(Generated.SampleRate);
	Wave->SetNumFrames(Generated.NumFrames);
	Wave->Duration = static_cast<float>(Generated.NumFrames) / Generated.SampleRate;
	Wave->SoundGroup = SOUNDGROUP_UI;
	Wave->bLooping = false;
	Wave->QueueAudio(
		reinterpret_cast<const uint8*>(Generated.Samples.GetData()),
		Diagnostics.LastQueuedBytes);

	UAudioComponent* Component = nullptr;
	if (WorldLocation != nullptr)
	{
		USoundAttenuation* Attenuation = GetOrCreateTacticalPositionalAttenuation();
		if (Attenuation == nullptr)
		{
			return false;
		}
		Component = UGameplayStatics::SpawnSoundAtLocation(
			World,
			Wave,
			*WorldLocation,
			FRotator::ZeroRotator,
			1.0f,
			1.0f,
			0.0f,
			Attenuation,
			nullptr,
			true);
	}
	else
	{
		Component = UGameplayStatics::SpawnSound2D(
			World, Wave, 1.0f, 1.0f, 0.0f, nullptr, false, true);
	}
	Diagnostics.bLastPlaybackComponentCreated = Component != nullptr;
	Diagnostics.bLastPlaybackStarted = Component != nullptr && Component->IsPlaying();
	if (Component == nullptr)
	{
		return false;
	}
	++Diagnostics.PlaybackComponentsCreated;
	if (bAmbient)
	{
		if (Diagnostics.bAmbientDucked)
		{
			Component->SetVolumeMultiplier(GetAmbientDuckLevel());
		}
		AmbientComponents.Add(Component);
		AmbientWaves.Add(Wave);
	}
	else
	{
		ForegroundComponents.Add(Component);
		ForegroundWaves.Add(Wave);
		DuckAmbient(static_cast<float>(Generated.NumFrames) / static_cast<float>(Generated.SampleRate));
	}
	return true;
}

USoundAttenuation* UUEGTAudioDirector::GetOrCreateTacticalPositionalAttenuation()
{
	if (IsValid(TacticalPositionalAttenuation))
	{
		return TacticalPositionalAttenuation;
	}

	TacticalPositionalAttenuation = NewObject<USoundAttenuation>(this);
	if (!IsValid(TacticalPositionalAttenuation))
	{
		return nullptr;
	}

	FSoundAttenuationSettings& Settings = TacticalPositionalAttenuation->Attenuation;
	Settings.bAttenuate = true;
	Settings.bSpatialize = true;
	Settings.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
	Settings.AttenuationShape = EAttenuationShape::Sphere;
	Settings.AttenuationShapeExtents = FVector::ZeroVector;
	Settings.FalloffDistance = 1800.0f;
	return TacticalPositionalAttenuation;
}

void UUEGTAudioDirector::PlayAmbientPulse()
{
	EUEGTAudioCue Cue = EUEGTAudioCue::StrategicAmbience;
	if (TryGetAmbientCue(Diagnostics.Mode, Cue))
	{
		PlayGeneratedCue(Cue, true);
	}
}

void UUEGTAudioDirector::StopAmbient()
{
	if (UWorld* World = PlaybackWorld.Get())
	{
		World->GetTimerManager().ClearTimer(AmbientTimerHandle);
		World->GetTimerManager().ClearTimer(AmbientDuckTimerHandle);
	}
	for (UAudioComponent* Component : AmbientComponents)
	{
		if (IsValid(Component))
		{
			Component->Stop();
		}
	}
	AmbientComponents.Reset();
	AmbientWaves.Reset();
	Diagnostics.bAmbientScheduled = false;
	Diagnostics.bAmbientDucked = false;
}

void UUEGTAudioDirector::DuckAmbient(const float HoldSeconds)
{
	UWorld* World = PlaybackWorld.Get();
	if (World == nullptr)
	{
		return;
	}

	int32 DuckedComponents = 0;
	for (UAudioComponent* Component : AmbientComponents)
	{
		if (IsValid(Component) && Component->IsPlaying())
		{
			Component->AdjustVolume(
				GetAmbientDuckAttackSeconds(),
				GetAmbientDuckLevel(),
				EAudioFaderCurve::Logarithmic);
			++DuckedComponents;
		}
	}
	if (DuckedComponents == 0)
	{
		return;
	}

	Diagnostics.bAmbientDucked = true;
	++Diagnostics.AmbientDuckRequests;
	const float ExistingRelease = World->GetTimerManager().GetTimerRemaining(AmbientDuckTimerHandle);
	const float ReleaseDelay = FMath::Max(
		0.01f,
		FMath::Max(HoldSeconds, ExistingRelease));
	World->GetTimerManager().SetTimer(
		AmbientDuckTimerHandle,
		this,
		&UUEGTAudioDirector::ReleaseAmbientDuck,
		ReleaseDelay,
		false);
}

void UUEGTAudioDirector::ReleaseAmbientDuck()
{
	for (UAudioComponent* Component : AmbientComponents)
	{
		if (IsValid(Component) && Component->IsPlaying())
		{
			Component->AdjustVolume(
				GetAmbientDuckReleaseSeconds(),
				1.0f,
				EAudioFaderCurve::Logarithmic);
		}
	}
	Diagnostics.bAmbientDucked = false;
}

void UUEGTAudioDirector::PruneFinishedAudio()
{
	for (int32 Index = ForegroundComponents.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(ForegroundComponents[Index]) || !ForegroundComponents[Index]->IsPlaying())
		{
			ForegroundComponents.RemoveAtSwap(Index);
			ForegroundWaves.RemoveAtSwap(Index);
		}
	}
	for (int32 Index = AmbientComponents.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(AmbientComponents[Index]) || !AmbientComponents[Index]->IsPlaying())
		{
			AmbientComponents.RemoveAtSwap(Index);
			AmbientWaves.RemoveAtSwap(Index);
		}
	}
}

bool UUEGTAudioDirector::TryGetAmbientCue(
	const EUEGTAudioPresentationMode Mode,
	EUEGTAudioCue& OutCue)
{
	switch (Mode)
	{
	case EUEGTAudioPresentationMode::MainMenu:
	case EUEGTAudioPresentationMode::Strategic:
		OutCue = EUEGTAudioCue::StrategicAmbience;
		return true;
	case EUEGTAudioPresentationMode::Tactical:
		OutCue = EUEGTAudioCue::TacticalAmbience;
		return true;
	default:
		return false;
	}
}
