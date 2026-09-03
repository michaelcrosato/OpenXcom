// Copyright 2026 UEGT contributors. MIT License.

#include "Audio/UEGTAudioSynthesisService.h"

#include "Misc/Crc.h"

namespace UEGTAudioSynthesisPrivate
{
	constexpr int32 SampleRate = 24000;
	constexpr double TwoPi = 6.283185307179586476925286766559;

	FUEGTAudioCueDefinition MakeDefinition(
		const EUEGTAudioCue Cue,
		const TCHAR* Name,
		const int32 DurationMilliseconds,
		const float BaseFrequencyHz,
		const float SecondaryFrequencyHz,
		const float Gain,
		const float AttackMilliseconds,
		const float ReleaseMilliseconds,
		const float PitchSweepRatio,
		const float PulseRateHz,
		const bool bAmbient = false)
	{
		FUEGTAudioCueDefinition Definition;
		Definition.Cue = Cue;
		Definition.DebugName = FName(Name);
		Definition.SampleRate = SampleRate;
		Definition.DurationMilliseconds = DurationMilliseconds;
		Definition.BaseFrequencyHz = BaseFrequencyHz;
		Definition.SecondaryFrequencyHz = SecondaryFrequencyHz;
		Definition.Gain = Gain;
		Definition.AttackMilliseconds = AttackMilliseconds;
		Definition.ReleaseMilliseconds = ReleaseMilliseconds;
		Definition.PitchSweepRatio = PitchSweepRatio;
		Definition.PulseRateHz = PulseRateHz;
		Definition.bAmbient = bAmbient;
		return Definition;
	}

	double SmoothRamp(const double Value)
	{
		const double Clamped = FMath::Clamp(Value, 0.0, 1.0);
		return Clamped * Clamped * (3.0 - 2.0 * Clamped);
	}
}

int32 FUEGTAudioCueDefinition::GetExpectedFrameCount() const
{
	if (SampleRate <= 0 || DurationMilliseconds <= 0)
	{
		return 0;
	}

	const int64 FrameCount =
		(static_cast<int64>(SampleRate) * DurationMilliseconds) / 1000;
	return static_cast<int32>(FMath::Clamp<int64>(FrameCount, 1, MAX_int32));
}

bool FUEGTAudioCueDefinition::IsValid() const
{
	return !DebugName.IsNone()
		&& SampleRate >= 8000 && SampleRate <= 96000
		&& DurationMilliseconds >= 80 && DurationMilliseconds <= 10000
		&& BaseFrequencyHz >= 20.0f && BaseFrequencyHz < SampleRate * 0.45f
		&& SecondaryFrequencyHz >= 20.0f && SecondaryFrequencyHz < SampleRate * 0.45f
		&& Gain > 0.0f && Gain <= 0.5f
		&& AttackMilliseconds >= 0.0f && ReleaseMilliseconds >= 0.0f
		&& GetExpectedFrameCount() > 0;
}

bool FUEGTGeneratedAudio::IsValid() const
{
	return SampleRate > 0 && NumChannels == 1 && NumFrames > 0
		&& Samples.Num() == NumFrames * NumChannels
		&& PeakAbsoluteSample > 0 && PeakAbsoluteSample <= 32767
		&& Fingerprint != 0;
}

TArray<EUEGTAudioCue> FUEGTAudioSynthesisService::GetCueTypes()
{
	return {
		EUEGTAudioCue::InterfaceConfirm,
		EUEGTAudioCue::TacticalSelection,
		EUEGTAudioCue::TacticalImpact,
		EUEGTAudioCue::TacticalSignal,
		EUEGTAudioCue::CommandAccepted,
		EUEGTAudioCue::CommandRejected,
		EUEGTAudioCue::StrategicAlert,
		EUEGTAudioCue::InterceptionReady,
		EUEGTAudioCue::TacticalDeployment,
		EUEGTAudioCue::TacticalPlayerTurn,
		EUEGTAudioCue::TacticalAdversaryTurn,
		EUEGTAudioCue::DebriefSuccess,
		EUEGTAudioCue::DebriefFailure,
		EUEGTAudioCue::StrategicAmbience,
		EUEGTAudioCue::TacticalAmbience
	};
}

FUEGTAudioCueDefinition FUEGTAudioSynthesisService::GetDefinition(const EUEGTAudioCue Cue)
{
	using namespace UEGTAudioSynthesisPrivate;
	switch (Cue)
	{
	case EUEGTAudioCue::InterfaceConfirm:
		return MakeDefinition(Cue, TEXT("interface-confirm"), 180, 720.0f, 1080.0f,
			0.16f, 5.0f, 55.0f, 0.05f, 0.0f);
	case EUEGTAudioCue::TacticalSelection:
		return MakeDefinition(Cue, TEXT("tactical-selection"), 140, 610.0f, 915.0f,
			0.12f, 4.0f, 45.0f, 0.08f, 0.0f);
	case EUEGTAudioCue::TacticalImpact:
		return MakeDefinition(Cue, TEXT("tactical-impact"), 220, 145.0f, 220.0f,
			0.14f, 3.0f, 95.0f, -0.12f, 0.0f);
	case EUEGTAudioCue::TacticalSignal:
		return MakeDefinition(Cue, TEXT("tactical-signal"), 260, 780.0f, 1170.0f,
			0.11f, 4.0f, 110.0f, 0.24f, 0.0f);
	case EUEGTAudioCue::CommandAccepted:
		return MakeDefinition(Cue, TEXT("command-accepted"), 240, 520.0f, 780.0f,
			0.18f, 6.0f, 80.0f, 0.10f, 0.0f);
	case EUEGTAudioCue::CommandRejected:
		return MakeDefinition(Cue, TEXT("command-rejected"), 300, 210.0f, 148.0f,
			0.20f, 6.0f, 110.0f, -0.22f, 3.5f);
	case EUEGTAudioCue::StrategicAlert:
		return MakeDefinition(Cue, TEXT("strategic-alert"), 650, 330.0f, 495.0f,
			0.17f, 8.0f, 170.0f, 0.18f, 2.5f);
	case EUEGTAudioCue::InterceptionReady:
		return MakeDefinition(Cue, TEXT("interception-ready"), 520, 440.0f, 660.0f,
			0.18f, 8.0f, 150.0f, 0.28f, 4.0f);
	case EUEGTAudioCue::TacticalDeployment:
		return MakeDefinition(Cue, TEXT("tactical-deployment"), 900, 110.0f, 220.0f,
			0.19f, 22.0f, 250.0f, 0.32f, 1.5f);
	case EUEGTAudioCue::TacticalPlayerTurn:
		return MakeDefinition(Cue, TEXT("tactical-player-turn"), 420, 360.0f, 540.0f,
			0.15f, 8.0f, 140.0f, 0.18f, 0.0f);
	case EUEGTAudioCue::TacticalAdversaryTurn:
		return MakeDefinition(Cue, TEXT("tactical-adversary-turn"), 480, 180.0f, 270.0f,
			0.17f, 8.0f, 160.0f, -0.12f, 2.0f);
	case EUEGTAudioCue::DebriefSuccess:
		return MakeDefinition(Cue, TEXT("debrief-success"), 1000, 330.0f, 495.0f,
			0.18f, 12.0f, 300.0f, 0.35f, 0.0f);
	case EUEGTAudioCue::DebriefFailure:
		return MakeDefinition(Cue, TEXT("debrief-failure"), 1000, 196.0f, 147.0f,
			0.17f, 12.0f, 350.0f, -0.25f, 0.0f);
	case EUEGTAudioCue::StrategicAmbience:
		return MakeDefinition(Cue, TEXT("strategic-ambience"), 5000, 55.0f, 82.5f,
			0.050f, 700.0f, 700.0f, 0.03f, 0.10f, true);
	case EUEGTAudioCue::TacticalAmbience:
		return MakeDefinition(Cue, TEXT("tactical-ambience"), 5000, 62.0f, 93.0f,
			0.058f, 550.0f, 650.0f, -0.02f, 0.17f, true);
	default:
		return FUEGTAudioCueDefinition();
	}
}

FUEGTGeneratedAudio FUEGTAudioSynthesisService::Generate(const EUEGTAudioCue Cue)
{
	using namespace UEGTAudioSynthesisPrivate;
	const FUEGTAudioCueDefinition Definition = GetDefinition(Cue);
	FUEGTGeneratedAudio Generated;
	Generated.Cue = Cue;
	if (!Definition.IsValid())
	{
		return Generated;
	}

	Generated.SampleRate = Definition.SampleRate;
	Generated.NumChannels = 1;
	Generated.NumFrames = Definition.GetExpectedFrameCount();
	Generated.Samples.SetNumUninitialized(Generated.NumFrames);

	const double DurationSeconds = static_cast<double>(Definition.DurationMilliseconds) / 1000.0;
	const double AttackSeconds = FMath::Max(
		static_cast<double>(Definition.AttackMilliseconds) / 1000.0, 1.0 / Definition.SampleRate);
	const double ReleaseSeconds = FMath::Max(
		static_cast<double>(Definition.ReleaseMilliseconds) / 1000.0, 1.0 / Definition.SampleRate);
	for (int32 Frame = 0; Frame < Generated.NumFrames; ++Frame)
	{
		const double Time = static_cast<double>(Frame) / Definition.SampleRate;
		const double Normalized = Generated.NumFrames > 1
			? static_cast<double>(Frame) / (Generated.NumFrames - 1)
			: 0.0;
		const double Attack = SmoothRamp(Time / AttackSeconds);
		const double Release = SmoothRamp((DurationSeconds - Time) / ReleaseSeconds);
		const double Envelope = Attack * Release;
		const double Sweep = Definition.PitchSweepRatio;
		const double BaseCycles = Definition.BaseFrequencyHz
			* (Time + Sweep * ((Time * Time / DurationSeconds) - Time));
		const double SecondaryDrift = 0.035 * FMath::Sin(TwoPi * 0.23 * Time);
		const double Primary = FMath::Sin(TwoPi * BaseCycles);
		const double Secondary = FMath::Sin(
			TwoPi * Definition.SecondaryFrequencyHz * Time + SecondaryDrift);
		const double Foundation = Definition.bAmbient
			? FMath::Sin(TwoPi * Definition.BaseFrequencyHz * 0.5 * Time)
			: 0.0;
		const double Voice = Definition.bAmbient
			? 0.62 * Primary + 0.27 * Secondary + 0.11 * Foundation
			: 0.72 * Primary + 0.28 * Secondary;
		const double Pulse = Definition.PulseRateHz > 0.0f
			? 0.76 + 0.24 * FMath::Sin(TwoPi * Definition.PulseRateHz * Time - 0.5 * PI)
			: 1.0;
		const double Scaled = FMath::Clamp(
			Voice * Envelope * Pulse * Definition.Gain, -0.999, 0.999);
		const int32 IntegerSample = FMath::Clamp(
			FMath::RoundToInt(Scaled * 32767.0), -32767, 32767);
		Generated.Samples[Frame] = static_cast<int16>(IntegerSample);
		Generated.PeakAbsoluteSample = FMath::Max(
			Generated.PeakAbsoluteSample, FMath::Abs(IntegerSample));
	}
	Generated.Fingerprint = FCrc::MemCrc32(
		Generated.Samples.GetData(), Generated.Samples.Num() * sizeof(int16));
	return Generated;
}

FName FUEGTAudioSynthesisService::GetCueName(const EUEGTAudioCue Cue)
{
	return GetDefinition(Cue).DebugName;
}
