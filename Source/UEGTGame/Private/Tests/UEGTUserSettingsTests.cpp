// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Campaign/CampaignSave.h"
#include "UEGTGameInstance.h"
#include "UEGTUserSettings.h"
#include "Tactical/UEGTTacticalPlayerController.h"

#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTUserSettingsDefaultsTest,
	"UEGT.Core.Game.UserSettings.DefaultsAndBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTUserSettingsDefaultsTest::RunTest(const FString& Parameters)
{
	UUEGTUserSettings* Settings = NewObject<UUEGTUserSettings>();
	TestNotNull(TEXT("Standalone settings object can be constructed"), Settings);
	if (Settings == nullptr)
	{
		return false;
	}
	Settings->SetToDefaults();
	TestEqual(TEXT("Accessible UI scale is the default"), Settings->GetUIScalePercent(), 100);
	TestTrue(TEXT("Reduced motion is enabled by default"), Settings->IsReducedMotionEnabled());
	TestTrue(TEXT("High-contrast markers are enabled by default"), Settings->IsHighContrastEnabled());
	TestEqual(TEXT("Standard color vision palette is the default"),
		Settings->GetColorVisionMode(), EUEGTColorVisionMode::Standard);
	TestEqual(TEXT("Camera speed is neutral by default"), Settings->GetCameraSpeedPercent(), 100);
	TestEqual(TEXT("Smart end-turn safety is the default"),
		Settings->GetEndTurnSafetyMode(), EUEGTEndTurnSafetyMode::Smart);
	TestTrue(TEXT("Ready-agent handoff is enabled by default"), Settings->ShouldAutoSelectReadyAgent());
	TestTrue(TEXT("Selection camera follow is enabled by default"), Settings->ShouldCenterCameraOnSelection());
	TestEqual(TEXT("Master audio defaults to a comfortable level"), Settings->GetMasterVolumePercent(), 75);
	TestTrue(TEXT("Background audio is muted by default"), Settings->ShouldMuteWhenUnfocused());
	TestEqual(TEXT("English is the source interface culture"), Settings->GetInterfaceCulture(), FString(TEXT("en")));
	TestEqual(TEXT("End-turn keeps its familiar default"),
		Settings->GetInputKey(EUEGTInputCommand::EndTurn), EKeys::SpaceBar);
	TestEqual(TEXT("Signal pressure receives a dedicated default key"),
		Settings->GetInputKey(EUEGTInputCommand::ProjectSignal), EKeys::V);
	TestEqual(TEXT("Every current keyboard command is remappable"),
		Settings->GetRemappableInputCommands().Num(), 16);
	TestTrue(TEXT("VSync is enabled by the accessible defaults"), Settings->IsVSyncEnabled());
	TestEqual(TEXT("Accessible defaults cap rendering at 60 FPS"), Settings->GetFrameRateLimit(), 60.0f);
	TestEqual(TEXT("Accessible defaults select the balanced High render preset"),
		Settings->GetOverallScalabilityLevel(), 2);
	TestEqual(TEXT("Accessible defaults map exactly to the recommended Comfort preset"),
		Settings->DetectAccessibilityPreset(), EUEGTAccessibilityPreset::Comfort);

	Settings->SetUIScalePercent(1);
	Settings->SetCameraSpeedPercent(1000);
	Settings->SetMasterVolumePercent(1000);
	Settings->SetInterfaceCulture(TEXT("unsupported-culture"));
	Settings->SetColorVisionMode(static_cast<EUEGTColorVisionMode>(255));
	Settings->SetEndTurnSafetyMode(static_cast<EUEGTEndTurnSafetyMode>(255));
	Settings->NormalizeCustomSettings();
	TestEqual(TEXT("UI scaling is clamped to a usable minimum"), Settings->GetUIScalePercent(), 85);
	TestEqual(TEXT("Camera speed is clamped to a usable maximum"), Settings->GetCameraSpeedPercent(), 200);
	TestEqual(TEXT("Master volume is clamped to its valid maximum"), Settings->GetMasterVolumePercent(), 100);
	TestEqual(TEXT("Unknown cultures normalize to the source culture"),
		Settings->GetInterfaceCulture(), FString(TEXT("en")));
	TestEqual(TEXT("Unknown palette values normalize safely"),
		Settings->GetColorVisionMode(), EUEGTColorVisionMode::Standard);
	TestEqual(TEXT("Unknown end-turn safety values normalize safely"),
		Settings->GetEndTurnSafetyMode(), EUEGTEndTurnSafetyMode::Smart);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTAccessibilityPresetPolicyTest,
	"UEGT.Core.Game.UserSettings.AccessibilityPresetPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTAccessibilityPresetPolicyTest::RunTest(const FString& Parameters)
{
	UUEGTUserSettings* Settings = NewObject<UUEGTUserSettings>();
	Settings->SetToDefaults();
	Settings->SetColorVisionMode(EUEGTColorVisionMode::Tritanopia);
	Settings->SetMasterVolumePercent(25);
	Settings->SetMuteWhenUnfocused(false);
	Settings->SetInterfaceCulture(TEXT("ja"));
	Settings->SetVSyncEnabled(false);
	Settings->SetFrameRateLimit(120.0f);
	Settings->SetOverallScalabilityLevel(1);
	FString BindingDiagnostic;
	TestTrue(TEXT("Preset preservation fixture accepts a custom keyboard binding"),
		Settings->TrySetInputKey(EUEGTInputCommand::Reload, EKeys::J, BindingDiagnostic));

	TestTrue(TEXT("Standard is a selectable preset"),
		Settings->ApplyAccessibilityPreset(EUEGTAccessibilityPreset::Standard));
	TestEqual(TEXT("Standard uses a neutral UI scale"), Settings->GetUIScalePercent(), 100);
	TestFalse(TEXT("Standard enables authored camera motion"), Settings->IsReducedMotionEnabled());
	TestFalse(TEXT("Standard uses softer non-critical markers"), Settings->IsHighContrastEnabled());
	TestEqual(TEXT("Standard retains smart end-turn safety"),
		Settings->GetEndTurnSafetyMode(), EUEGTEndTurnSafetyMode::Smart);
	TestTrue(TEXT("Standard retains ready-agent handoff"), Settings->ShouldAutoSelectReadyAgent());
	TestTrue(TEXT("Standard retains selection camera follow"), Settings->ShouldCenterCameraOnSelection());
	TestEqual(TEXT("Exact Standard settings are detected without a stored preset flag"),
		Settings->DetectAccessibilityPreset(), EUEGTAccessibilityPreset::Standard);

	TestEqual(TEXT("Presets preserve the selected color-vision palette"),
		Settings->GetColorVisionMode(), EUEGTColorVisionMode::Tritanopia);
	TestEqual(TEXT("Presets preserve master audio"), Settings->GetMasterVolumePercent(), 25);
	TestFalse(TEXT("Presets preserve unfocused-audio behavior"), Settings->ShouldMuteWhenUnfocused());
	TestEqual(TEXT("Presets preserve interface culture"),
		Settings->GetInterfaceCulture(), FString(TEXT("ja")));
	TestEqual(TEXT("Presets preserve custom keyboard bindings"),
		Settings->GetInputKey(EUEGTInputCommand::Reload), EKeys::J);
	TestFalse(TEXT("Presets preserve VSync"), Settings->IsVSyncEnabled());
	TestEqual(TEXT("Presets preserve the frame limit"), Settings->GetFrameRateLimit(), 120.0f);
	TestEqual(TEXT("Presets preserve render quality"), Settings->GetOverallScalabilityLevel(), 1);

	TestTrue(TEXT("Comfort is a selectable preset"),
		Settings->ApplyAccessibilityPreset(EUEGTAccessibilityPreset::Comfort));
	TestTrue(TEXT("Comfort enables reduced motion"), Settings->IsReducedMotionEnabled());
	TestTrue(TEXT("Comfort enables high-contrast markers"), Settings->IsHighContrastEnabled());
	TestEqual(TEXT("Exact Comfort settings are detected"),
		Settings->DetectAccessibilityPreset(), EUEGTAccessibilityPreset::Comfort);

	TestTrue(TEXT("Maximum Clarity is a selectable preset"),
		Settings->ApplyAccessibilityPreset(EUEGTAccessibilityPreset::MaximumClarity));
	TestEqual(TEXT("Maximum Clarity uses a 130 percent UI scale"), Settings->GetUIScalePercent(), 130);
	TestEqual(TEXT("Maximum Clarity slows camera navigation"), Settings->GetCameraSpeedPercent(), 75);
	TestEqual(TEXT("Maximum Clarity always guards end turn"),
		Settings->GetEndTurnSafetyMode(), EUEGTEndTurnSafetyMode::Always);
	TestTrue(TEXT("Maximum Clarity retains ready-agent handoff"), Settings->ShouldAutoSelectReadyAgent());
	TestFalse(TEXT("Maximum Clarity prevents automatic selection camera jumps"),
		Settings->ShouldCenterCameraOnSelection());
	TestEqual(TEXT("Exact Maximum Clarity settings are detected"),
		Settings->DetectAccessibilityPreset(), EUEGTAccessibilityPreset::MaximumClarity);

	const int32 ScaleBeforeCustomRequest = Settings->GetUIScalePercent();
	TestFalse(TEXT("Derived Custom state cannot be applied as a destructive preset"),
		Settings->ApplyAccessibilityPreset(EUEGTAccessibilityPreset::Custom));
	TestEqual(TEXT("Rejected Custom application leaves settings unchanged"),
		Settings->GetUIScalePercent(), ScaleBeforeCustomRequest);
	Settings->SetUIScalePercent(129);
	TestEqual(TEXT("Any manual divergence is reported as Custom"),
		Settings->DetectAccessibilityPreset(), EUEGTAccessibilityPreset::Custom);

	const TArray<EUEGTAccessibilityPreset> Presets =
		UUEGTUserSettings::GetSelectableAccessibilityPresets();
	TestEqual(TEXT("New campaign exposes exactly three intentional preset choices"), Presets.Num(), 3);
	TestTrue(TEXT("Preset choice ordering remains stable"),
		Presets[0] == EUEGTAccessibilityPreset::Standard
		&& Presets[1] == EUEGTAccessibilityPreset::Comfort
		&& Presets[2] == EUEGTAccessibilityPreset::MaximumClarity);

	const FString Directory = FPaths::Combine(
		FPaths::AutomationTransientDir(), TEXT("UEGTAccessibilityPreset"));
	const FString Filename = FPaths::Combine(Directory, TEXT("GameUserSettings.ini"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	IFileManager::Get().Delete(*Filename, false, true);
	Settings->ApplyAccessibilityPreset(EUEGTAccessibilityPreset::MaximumClarity);
	Settings->SaveConfig(CPF_Config, *Filename);
	UUEGTUserSettings* Loaded = NewObject<UUEGTUserSettings>();
	Loaded->LoadConfig(Loaded->GetClass(), *Filename);
	Loaded->NormalizeCustomSettings();
	TestEqual(TEXT("Preset fields persist and reconstitute Maximum Clarity without campaign data"),
		Loaded->DetectAccessibilityPreset(), EUEGTAccessibilityPreset::MaximumClarity);
	TestEqual(TEXT("Unrelated palette also survives the preset config round trip"),
		Loaded->GetColorVisionMode(), EUEGTColorVisionMode::Tritanopia);
	IFileManager::Get().Delete(*Filename, false, true);
	IFileManager::Get().DeleteDirectory(*Directory, false, true);

	UUEGTUserSettings* RuntimeSettings = UUEGTUserSettings::Get();
	TestNotNull(TEXT("Campaign-isolation fixture can access the configured runtime settings"), RuntimeSettings);
	if (RuntimeSettings != nullptr)
	{
		RuntimeSettings->LoadSettings(true);
		RuntimeSettings->ApplyAccessibilityPreset(EUEGTAccessibilityPreset::Standard);
		UUEGTGameInstance* StandardCampaign = NewObject<UUEGTGameInstance>();
		const bool bStandardStarted = StandardCampaign != nullptr
			&& StandardCampaign->ReloadContent()
			&& StandardCampaign->StartNewCampaign(
				ECampaignDifficulty::Veteran, 424242, EUEGTFundingModel::BalancedMandate);

		RuntimeSettings->ApplyAccessibilityPreset(EUEGTAccessibilityPreset::MaximumClarity);
		UUEGTGameInstance* ClarityCampaign = NewObject<UUEGTGameInstance>();
		const bool bClarityStarted = ClarityCampaign != nullptr
			&& ClarityCampaign->ReloadContent()
			&& ClarityCampaign->StartNewCampaign(
				ECampaignDifficulty::Veteran, 424242, EUEGTFundingModel::BalancedMandate);
		RuntimeSettings->LoadSettings(true);
		TestTrue(TEXT("Both accessibility profiles start the same deterministic campaign fixture"),
			bStandardStarted && bClarityStarted);
		if (bStandardStarted && bClarityStarted)
		{
			const FDateTime FixedWallClock(2040, 1, 2, 3, 4, 5);
			const FGuid FixedCampaignId(91, 92, 93, 94);
			const FCampaignSaveEnvelope StandardEnvelope = FCampaignSaveCodec::CreateNew(
				StandardCampaign->GetCampaignState(),
				StandardCampaign->GetLoadedContentVersions(),
				TEXT("preset-isolation"),
				FixedWallClock,
				FixedCampaignId);
			const FCampaignSaveEnvelope ClarityEnvelope = FCampaignSaveCodec::CreateNew(
				ClarityCampaign->GetCampaignState(),
				ClarityCampaign->GetLoadedContentVersions(),
				TEXT("preset-isolation"),
				FixedWallClock,
				FixedCampaignId);
			TestEqual(TEXT("Local accessibility presets cannot alter any canonical campaign field"),
				FCampaignSaveCodec::ComputeEnvelopeChecksum(StandardEnvelope),
				FCampaignSaveCodec::ComputeEnvelopeChecksum(ClarityEnvelope));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTUserSettingsInputPolicyTest,
	"UEGT.Core.Game.UserSettings.InputBindingPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTUserSettingsInputPolicyTest::RunTest(const FString& Parameters)
{
	UUEGTUserSettings* Settings = NewObject<UUEGTUserSettings>();
	Settings->SetToDefaults();
	FString Diagnostic;
	FName DiagnosticCode;
	TestFalse(TEXT("Fixed camera keys cannot displace command bindings"),
		Settings->TrySetInputKey(
			EUEGTInputCommand::Reload, EKeys::W, Diagnostic, DiagnosticCode));
	TestTrue(TEXT("Reserved-key rejection explains the fixed navigation policy"),
		Diagnostic.Contains(TEXT("reserved"))
		&& DiagnosticCode == FName(TEXT("input_key_reserved")));
	TestEqual(TEXT("Rejected reassignment is transactional"),
		Settings->GetInputKey(EUEGTInputCommand::Reload), EKeys::R);

	TestFalse(TEXT("Gamepad keys remain on the fixed controller layout"),
		Settings->TrySetInputKey(
			EUEGTInputCommand::Reload, EKeys::Gamepad_FaceButton_Bottom, Diagnostic));
	TestTrue(TEXT("An occupied key can be intentionally reassigned"),
		Settings->TrySetInputKey(
			EUEGTInputCommand::Reload, EKeys::F, Diagnostic, DiagnosticCode));
	TestEqual(TEXT("Requested command receives the occupied key"),
		Settings->GetInputKey(EUEGTInputCommand::Reload), EKeys::F);
	TestEqual(TEXT("Conflicting command receives the displaced key"),
		Settings->GetInputKey(EUEGTInputCommand::Objective), EKeys::R);
	TestTrue(TEXT("Conflict swap is explicit to the player"),
		Diagnostic.Contains(TEXT("avoid a conflict"))
		&& DiagnosticCode == FName(TEXT("input_binding_swapped")));

	TSet<FName> UniqueKeys;
	for (const EUEGTInputCommand Command : UUEGTUserSettings::GetRemappableInputCommands())
	{
		const FKey Key = Settings->GetInputKey(Command);
		TestTrue(TEXT("Every published command retains a supported key"),
			UUEGTUserSettings::IsSupportedInputKey(Key));
		TestFalse(TEXT("Published command keys remain unique"), UniqueKeys.Contains(Key.GetFName()));
		UniqueKeys.Add(Key.GetFName());
	}
	Settings->ResetInputBindingsToDefaults();
	TestEqual(TEXT("Binding reset restores the complete default map"),
		Settings->GetInputKey(EUEGTInputCommand::Reload), EKeys::R);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTUserSettingsPersistenceTest,
	"UEGT.Core.Game.UserSettings.ConfigRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTUserSettingsPersistenceTest::RunTest(const FString& Parameters)
{
	const FString Directory = FPaths::Combine(FPaths::AutomationTransientDir(), TEXT("UEGTUserSettings"));
	const FString Filename = FPaths::Combine(Directory, TEXT("GameUserSettings.ini"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	IFileManager::Get().Delete(*Filename, false, true);

	UUEGTUserSettings* Source = NewObject<UUEGTUserSettings>();
	Source->SetToDefaults();
	Source->SetUIScalePercent(130);
	Source->SetReducedMotionEnabled(false);
	Source->SetHighContrastEnabled(false);
	Source->SetColorVisionMode(EUEGTColorVisionMode::Tritanopia);
	Source->SetCameraSpeedPercent(150);
	Source->SetEndTurnSafetyMode(EUEGTEndTurnSafetyMode::Always);
	Source->SetAutoSelectReadyAgent(false);
	Source->SetCenterCameraOnSelection(false);
	Source->SetMasterVolumePercent(25);
	Source->SetMuteWhenUnfocused(false);
	Source->SetInterfaceCulture(TEXT("ja"));
	FString BindingDiagnostic;
	TestTrue(TEXT("A custom keyboard binding is accepted before persistence"),
		Source->TrySetInputKey(EUEGTInputCommand::Reload, EKeys::J, BindingDiagnostic));
	Source->SaveConfig(CPF_Config, *Filename);

	UUEGTUserSettings* Loaded = NewObject<UUEGTUserSettings>();
	Loaded->LoadConfig(Loaded->GetClass(), *Filename);
	Loaded->NormalizeCustomSettings();
	TestEqual(TEXT("UI scale persists through config"), Loaded->GetUIScalePercent(), 130);
	TestFalse(TEXT("Reduced-motion preference persists through config"), Loaded->IsReducedMotionEnabled());
	TestFalse(TEXT("High-contrast preference persists through config"), Loaded->IsHighContrastEnabled());
	TestEqual(TEXT("Color-vision palette persists through config"),
		Loaded->GetColorVisionMode(), EUEGTColorVisionMode::Tritanopia);
	TestEqual(TEXT("Camera-speed preference persists through config"), Loaded->GetCameraSpeedPercent(), 150);
	TestEqual(TEXT("End-turn safety preference persists through config"),
		Loaded->GetEndTurnSafetyMode(), EUEGTEndTurnSafetyMode::Always);
	TestFalse(TEXT("Ready-agent handoff preference persists through config"),
		Loaded->ShouldAutoSelectReadyAgent());
	TestFalse(TEXT("Selection camera-follow preference persists through config"),
		Loaded->ShouldCenterCameraOnSelection());
	TestEqual(TEXT("Master-volume preference persists through config"), Loaded->GetMasterVolumePercent(), 25);
	TestFalse(TEXT("Unfocused-audio preference persists through config"), Loaded->ShouldMuteWhenUnfocused());
	TestEqual(TEXT("Interface culture persists through config"), Loaded->GetInterfaceCulture(), FString(TEXT("ja")));
	TestEqual(TEXT("Keyboard command assignments persist through config"),
		Loaded->GetInputKey(EUEGTInputCommand::Reload), EKeys::J);
	TestEqual(TEXT("Arbitrary individual settings load as Custom rather than a misleading preset"),
		Loaded->DetectAccessibilityPreset(), EUEGTAccessibilityPreset::Custom);

	const UGameUserSettings* EngineSettings = GEngine != nullptr ? GEngine->GetGameUserSettings() : nullptr;
	TestTrue(TEXT("DefaultEngine selects the UEGT user-settings subclass"),
		EngineSettings != nullptr && EngineSettings->IsA<UUEGTUserSettings>());
	IFileManager::Get().Delete(*Filename, false, true);
	IFileManager::Get().DeleteDirectory(*Directory, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTGameplayPreferencePolicyTest,
	"UEGT.Core.Game.UserSettings.GameplayPreferencePolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTGameplayPreferencePolicyTest::RunTest(const FString& Parameters)
{
	UUEGTUserSettings* Settings = NewObject<UUEGTUserSettings>();
	Settings->SetToDefaults();
	TestTrue(TEXT("Smart safety confirms while a ready agent retains AP"),
		Settings->ShouldConfirmEndTurn(2, 17));
	TestFalse(TEXT("Smart safety permits a fully spent turn immediately"),
		Settings->ShouldConfirmEndTurn(0, 0));
	Settings->SetEndTurnSafetyMode(EUEGTEndTurnSafetyMode::Always);
	TestTrue(TEXT("Always safety confirms even after all AP is spent"),
		Settings->ShouldConfirmEndTurn(0, 0));
	Settings->SetEndTurnSafetyMode(EUEGTEndTurnSafetyMode::Off);
	TestFalse(TEXT("Disabled safety never requests a confirmation"),
		Settings->ShouldConfirmEndTurn(3, 24));
	FUEGTEndTurnConfirmationState Confirmation;
	TestTrue(TEXT("First guarded activation is deferred"), Confirmation.ShouldDefer(true, 42));
	TestTrue(TEXT("Guard arms against the current command sequence"),
		Confirmation.IsArmed() && Confirmation.GetArmedSequence() == 42);
	TestFalse(TEXT("Second activation at the same sequence is accepted"),
		Confirmation.ShouldDefer(true, 42));
	TestFalse(TEXT("Accepted confirmation disarms atomically"), Confirmation.IsArmed());
	TestTrue(TEXT("A changed command sequence cannot reuse stale confirmation"),
		Confirmation.ShouldDefer(true, 43) && Confirmation.ShouldDefer(true, 44));
	TestFalse(TEXT("Disabling confirmation clears an armed guard"),
		Confirmation.ShouldDefer(false, 44));
	TestFalse(TEXT("Disabled guard remains disarmed"), Confirmation.IsArmed());

	FTacticalHudSnapshot Snapshot;
	FTacticalHudUnitView Spent;
	Spent.UnitId = FGuid(1, 0, 0, 0);
	Spent.Team = ETacticalTeam::Player;
	Spent.RemainingActionPoints = 0;
	Snapshot.Units.Add(Spent);
	FTacticalHudUnitView Hostile;
	Hostile.UnitId = FGuid(2, 0, 0, 0);
	Hostile.Team = ETacticalTeam::Adversary;
	Hostile.RemainingActionPoints = 99;
	Snapshot.Units.Add(Hostile);
	FTacticalHudUnitView Ready;
	Ready.UnitId = FGuid(3, 0, 0, 0);
	Ready.Team = ETacticalTeam::Player;
	Ready.RemainingActionPoints = 7;
	Snapshot.Units.Add(Ready);
	FTacticalHudUnitView Extracted;
	Extracted.UnitId = FGuid(4, 0, 0, 0);
	Extracted.Team = ETacticalTeam::Player;
	Extracted.RemainingActionPoints = 11;
	Extracted.bExtracted = true;
	Snapshot.Units.Add(Extracted);

	int32 ReadyAgentCount = 0;
	TestEqual(TEXT("Readiness totals exclude hostiles, spent agents, and extracted agents"),
		AUEGTTacticalPlayerController::CountRemainingPlayerActionPoints(Snapshot, ReadyAgentCount), 7);
	TestEqual(TEXT("Readiness reports the exact number of actionable agents"), ReadyAgentCount, 1);
	TestEqual(TEXT("Ready-agent handoff follows stable HUD order"),
		AUEGTTacticalPlayerController::FindNextReadyPlayerUnit(Snapshot, Spent.UnitId), Ready.UnitId);
	Ready.bIncapacitated = true;
	Snapshot.Units[2] = Ready;
	TestFalse(TEXT("Handoff returns no id when every player agent is unavailable"),
		AUEGTTacticalPlayerController::FindNextReadyPlayerUnit(Snapshot, Spent.UnitId).IsValid());
	TestEqual(TEXT("Manufacturing delta funds preserve ordinary signed cost magnitude"),
		AUEGTTacticalPlayerController::CalculateManufacturingDeltaFunds(125, -3), int64(375));
	TestEqual(TEXT("Manufacturing delta funds saturate positive cost overflow"),
		AUEGTTacticalPlayerController::CalculateManufacturingDeltaFunds(MAX_int64, 2), MAX_int64);
	TestEqual(TEXT("Manufacturing delta funds saturate negative cost overflow"),
		AUEGTTacticalPlayerController::CalculateManufacturingDeltaFunds(MIN_int64, 2), MIN_int64);
	TestEqual(TEXT("Manufacturing delta funds handle the minimum int32 delta magnitude"),
		AUEGTTacticalPlayerController::CalculateManufacturingDeltaFunds(MAX_int64, MIN_int32), MAX_int64);
	return true;
}

#endif
