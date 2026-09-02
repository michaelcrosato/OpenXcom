// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Audio/UEGTAudioDirector.h"
#include "Audio/UEGTAudioSynthesisService.h"
#include "Localization/UEGTLocalizationService.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTAudioSynthesisTest,
	"UEGT.Core.Game.Audio.ProceduralSynthesis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTAudioSynthesisTest::RunTest(const FString& Parameters)
{
	const TArray<EUEGTAudioCue> Cues = FUEGTAudioSynthesisService::GetCueTypes();
	TestEqual(TEXT("The original runtime palette publishes twelve intentional voices"), Cues.Num(), 12);
	TSet<FName> Names;
	TSet<uint32> Fingerprints;
	int32 AmbientCount = 0;
	for (const EUEGTAudioCue Cue : Cues)
	{
		const FUEGTAudioCueDefinition Definition =
			FUEGTAudioSynthesisService::GetDefinition(Cue);
		const FUEGTGeneratedAudio First = FUEGTAudioSynthesisService::Generate(Cue);
		const FUEGTGeneratedAudio Second = FUEGTAudioSynthesisService::Generate(Cue);
		TestTrue(*FString::Printf(TEXT("%s has a safe synthesis contract"), *Definition.DebugName.ToString()),
			Definition.IsValid());
		TestTrue(*FString::Printf(TEXT("%s yields valid mono PCM"), *Definition.DebugName.ToString()),
			First.IsValid());
		TestEqual(*FString::Printf(TEXT("%s emits the contracted frame count"), *Definition.DebugName.ToString()),
			First.NumFrames, Definition.GetExpectedFrameCount());
		TestTrue(*FString::Printf(TEXT("%s remains bit-identical across repeated generation"), *Definition.DebugName.ToString()),
			First.Fingerprint == Second.Fingerprint && First.Samples == Second.Samples);
		TestTrue(*FString::Printf(TEXT("%s stays below the conservative PCM peak ceiling"), *Definition.DebugName.ToString()),
			First.PeakAbsoluteSample > 100 && First.PeakAbsoluteSample < 8192);

		int64 SignedSum = 0;
		for (const int16 Sample : First.Samples)
		{
			SignedSum += Sample;
		}
		const double DcOffset = FMath::Abs(static_cast<double>(SignedSum) / First.Samples.Num());
		TestTrue(*FString::Printf(TEXT("%s has no material DC offset"), *Definition.DebugName.ToString()),
			DcOffset < 200.0);
		TestFalse(TEXT("Cue debug names are unique"), Names.Contains(Definition.DebugName));
		TestFalse(TEXT("Cue waveforms have distinct fingerprints"), Fingerprints.Contains(First.Fingerprint));
		Names.Add(Definition.DebugName);
		Fingerprints.Add(First.Fingerprint);
		if (Definition.bAmbient)
		{
			++AmbientCount;
			TestTrue(TEXT("Ambient beds are long and intentionally quiet"),
				Definition.DurationMilliseconds == 5000 && First.PeakAbsoluteSample < 2500);
		}
	}
	TestEqual(TEXT("Strategic and tactical modes each have a dedicated ambience bed"), AmbientCount, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTAudioEventRoutingTest,
	"UEGT.Core.Game.Audio.EventRoutingPriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTAudioEventRoutingTest::RunTest(const FString& Parameters)
{
	UUEGTAudioDirector* ModeDirector = NewObject<UUEGTAudioDirector>();
	TestNotNull(TEXT("Presentation-mode routing can be exercised without an audio device"), ModeDirector);
	if (ModeDirector != nullptr)
	{
		ModeDirector->Initialize(nullptr);
		ModeDirector->SetPresentationMode(EUEGTAudioPresentationMode::MainMenu);
		const FUEGTAudioDirectorDiagnostics MenuAudio = ModeDirector->GetDiagnostics();
		TestTrue(TEXT("Main menu selects the strategic ambience voice"),
			MenuAudio.Mode == EUEGTAudioPresentationMode::MainMenu
			&& MenuAudio.LastCue == EUEGTAudioCue::StrategicAmbience
			&& MenuAudio.PlayRequests == 1);
		ModeDirector->SetPresentationMode(EUEGTAudioPresentationMode::Tactical);
		const FUEGTAudioDirectorDiagnostics TacticalAudio = ModeDirector->GetDiagnostics();
		TestTrue(TEXT("Tactical presentation switches to its distinct ambience voice"),
			TacticalAudio.Mode == EUEGTAudioPresentationMode::Tactical
			&& TacticalAudio.LastCue == EUEGTAudioCue::TacticalAmbience
			&& TacticalAudio.PlayRequests == 2);
		ModeDirector->SetPresentationMode(EUEGTAudioPresentationMode::Tactical);
		TestEqual(TEXT("Refreshing the same mode cannot restart its ambience"),
			ModeDirector->GetDiagnostics().PlayRequests, 2);
		ModeDirector->SetPresentationMode(EUEGTAudioPresentationMode::Debrief);
		TestTrue(TEXT("Debrief mode stops ambience without adding another bed"),
			ModeDirector->GetDiagnostics().Mode == EUEGTAudioPresentationMode::Debrief
			&& ModeDirector->GetDiagnostics().PlayRequests == 2
			&& !ModeDirector->GetDiagnostics().bAmbientScheduled);
		ModeDirector->Shutdown();
	}

	FStrategicCommandResult Result;
	TestEqual(TEXT("Rejected commands always use the rejection voice"),
		UUEGTAudioDirector::SelectCommandCue(Result, false), EUEGTAudioCue::CommandRejected);
	Result.bAccepted = true;
	TestEqual(TEXT("Accepted commands without a state transition use restrained confirmation"),
		UUEGTAudioDirector::SelectCommandCue(Result, false), EUEGTAudioCue::CommandAccepted);

	FStrategicEvent& Ready = Result.Events.AddDefaulted_GetRef();
	Ready.Type = EStrategicEventType::InterceptionReady;
	TestEqual(TEXT("Interception readiness has a distinct operational signal"),
		UUEGTAudioDirector::SelectCommandCue(Result, false), EUEGTAudioCue::InterceptionReady);
	FStrategicEvent& Alert = Result.Events.AddDefaulted_GetRef();
	Alert.Type = EStrategicEventType::BaseAssaultStarted;
	TestEqual(TEXT("Base-defense alerts supersede routine interception readiness"),
		UUEGTAudioDirector::SelectCommandCue(Result, false), EUEGTAudioCue::StrategicAlert);
	FStrategicEvent& Loss = Result.Events.AddDefaulted_GetRef();
	Loss.Type = EStrategicEventType::CampaignLost;
	TestEqual(TEXT("Campaign outcomes supersede every lower-priority alert"),
		UUEGTAudioDirector::SelectCommandCue(Result, false), EUEGTAudioCue::DebriefFailure);

	Result.Events.Reset();
	FStrategicEvent& Resolved = Result.Events.AddDefaulted_GetRef();
	Resolved.Type = EStrategicEventType::TacticalBattleResolved;
	Resolved.bSuccessful = true;
	TestEqual(TEXT("Successful tactical resolution uses the positive debrief voice"),
		UUEGTAudioDirector::SelectCommandCue(Result, true), EUEGTAudioCue::DebriefSuccess);
	Resolved.bSuccessful = false;
	TestEqual(TEXT("Failed tactical resolution uses the negative debrief voice"),
		UUEGTAudioDirector::SelectCommandCue(Result, true), EUEGTAudioCue::DebriefFailure);
	Resolved.Type = EStrategicEventType::TacticalTurnEnded;
	TestEqual(TEXT("Player turn completion signals the adversary handoff"),
		UUEGTAudioDirector::SelectCommandCue(Result, true), EUEGTAudioCue::TacticalAdversaryTurn);
	Resolved.Type = EStrategicEventType::TacticalAiTurnCompleted;
	TestEqual(TEXT("AI completion signals control returning to the player"),
		UUEGTAudioDirector::SelectCommandCue(Result, true), EUEGTAudioCue::TacticalPlayerTurn);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTAudioLocalizationCoverageTest,
	"UEGT.Core.Game.Audio.FiveCulturePresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTAudioLocalizationCoverageTest::RunTest(const FString& Parameters)
{
	const FUEGTLocalizationLoadResult Loaded = FUEGTLocalizationService::LoadCatalogFile(
		FUEGTLocalizationService::GetDefaultCatalogFilename());
	TestTrue(TEXT("The audio presentation catalog loads under strict validation"), Loaded.bSucceeded);
	const TArray<FString> Keys = {
		TEXT("settings.audio-help"),
		TEXT("settings.audio-profile"),
		TEXT("settings.audio-preview"),
		TEXT("status.audio-preview"),
		TEXT("status.audio-preview-unavailable")
	};
	for (const FString& Key : Keys)
	{
		const FUEGTLocalizedTextEntry* Entry = Loaded.Catalog.Entries.Find(Key);
		TestTrue(*FString::Printf(TEXT("%s is authored"), *Key), Entry != nullptr);
		if (Entry != nullptr)
		{
			TestEqual(*FString::Printf(TEXT("%s covers all five cultures"), *Key),
				Entry->Translations.Num(), 5);
		}
	}
	TestEqual(TEXT("French audio profile copy is exact for runtime screenshot verification"),
		Loaded.Catalog.Resolve(TEXT("settings.audio-profile"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("MIXAGE PROCÉDURAL ORIGINAL")));
	TestEqual(TEXT("Japanese preview action is authored without an English fallback"),
		Loaded.Catalog.Resolve(TEXT("settings.audio-preview"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("オーディオ信号を試聴")));
	return true;
}

#endif
