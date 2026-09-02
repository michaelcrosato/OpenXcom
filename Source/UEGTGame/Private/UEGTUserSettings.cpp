// Copyright 2026 UEGT contributors. MIT License.

#include "UEGTUserSettings.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "Internationalization/Internationalization.h"
#include "Localization/UEGTLocalizationService.h"
#include "Misc/App.h"

namespace UEGTUserSettingsPrivate
{
	const TArray<EUEGTInputCommand>& RemappableCommands()
	{
		static const TArray<EUEGTInputCommand> Commands = {
			EUEGTInputCommand::Confirm,
			EUEGTInputCommand::EndTurn,
			EUEGTInputCommand::ToggleStance,
			EUEGTInputCommand::Reload,
			EUEGTInputCommand::Objective,
			EUEGTInputCommand::Extract,
			EUEGTInputCommand::Door,
			EUEGTInputCommand::UseDevice,
			EUEGTInputCommand::TerrainAttack,
			EUEGTInputCommand::ProjectSignal,
			EUEGTInputCommand::NextUnit,
			EUEGTInputCommand::LevelUp,
			EUEGTInputCommand::LevelDown,
			EUEGTInputCommand::CycleWeapon,
			EUEGTInputCommand::ToggleFireMode,
			EUEGTInputCommand::CycleDevice
		};
		return Commands;
	}
}

UUEGTUserSettings::UUEGTUserSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UUEGTUserSettings* UUEGTUserSettings::Get()
{
	return GEngine != nullptr ? Cast<UUEGTUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
}

void UUEGTUserSettings::ApplySettings(const bool bCheckForCommandLineOverrides)
{
	NormalizeCustomSettings();
	Super::ApplySettings(bCheckForCommandLineOverrides);
	ApplyAccessibilitySettings();
	ApplyAudioSettings();
	ApplyLanguageSettings();
}

void UUEGTUserSettings::LoadSettings(const bool bForceReload)
{
	Super::LoadSettings(bForceReload);
	NormalizeCustomSettings();
	ApplyAccessibilitySettings();
	ApplyAudioSettings();
	ApplyLanguageSettings();
}

void UUEGTUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	UIScalePercent = 100;
	bReduceMotion = true;
	bHighContrast = true;
	ColorVisionMode = EUEGTColorVisionMode::Standard;
	CameraSpeedPercent = 100;
	EndTurnSafetyMode = EUEGTEndTurnSafetyMode::Smart;
	bAutoSelectReadyAgent = true;
	bCenterCameraOnSelection = true;
	MasterVolumePercent = 75;
	bMuteWhenUnfocused = true;
	InterfaceCulture = TEXT("en");
	ResetInputBindingsToDefaults();
	SetVSyncEnabled(true);
	SetFrameRateLimit(60.0f);
	SetOverallScalabilityLevel(2);
}

bool UUEGTUserSettings::ApplyAccessibilityPreset(
	const EUEGTAccessibilityPreset Preset)
{
	switch (Preset)
	{
	case EUEGTAccessibilityPreset::Standard:
		UIScalePercent = 100;
		bReduceMotion = false;
		bHighContrast = false;
		CameraSpeedPercent = 100;
		EndTurnSafetyMode = EUEGTEndTurnSafetyMode::Smart;
		bAutoSelectReadyAgent = true;
		bCenterCameraOnSelection = true;
		break;
	case EUEGTAccessibilityPreset::Comfort:
		UIScalePercent = 100;
		bReduceMotion = true;
		bHighContrast = true;
		CameraSpeedPercent = 100;
		EndTurnSafetyMode = EUEGTEndTurnSafetyMode::Smart;
		bAutoSelectReadyAgent = true;
		bCenterCameraOnSelection = true;
		break;
	case EUEGTAccessibilityPreset::MaximumClarity:
		UIScalePercent = 130;
		bReduceMotion = true;
		bHighContrast = true;
		CameraSpeedPercent = 75;
		EndTurnSafetyMode = EUEGTEndTurnSafetyMode::Always;
		bAutoSelectReadyAgent = true;
		bCenterCameraOnSelection = false;
		break;
	default:
		return false;
	}
	NormalizeCustomSettings();
	return true;
}

EUEGTAccessibilityPreset UUEGTUserSettings::DetectAccessibilityPreset() const
{
	if (UIScalePercent == 100
		&& !bReduceMotion
		&& !bHighContrast
		&& CameraSpeedPercent == 100
		&& EndTurnSafetyMode == EUEGTEndTurnSafetyMode::Smart
		&& bAutoSelectReadyAgent
		&& bCenterCameraOnSelection)
	{
		return EUEGTAccessibilityPreset::Standard;
	}
	if (UIScalePercent == 100
		&& bReduceMotion
		&& bHighContrast
		&& CameraSpeedPercent == 100
		&& EndTurnSafetyMode == EUEGTEndTurnSafetyMode::Smart
		&& bAutoSelectReadyAgent
		&& bCenterCameraOnSelection)
	{
		return EUEGTAccessibilityPreset::Comfort;
	}
	if (UIScalePercent == 130
		&& bReduceMotion
		&& bHighContrast
		&& CameraSpeedPercent == 75
		&& EndTurnSafetyMode == EUEGTEndTurnSafetyMode::Always
		&& bAutoSelectReadyAgent
		&& !bCenterCameraOnSelection)
	{
		return EUEGTAccessibilityPreset::MaximumClarity;
	}
	return EUEGTAccessibilityPreset::Custom;
}

TArray<EUEGTAccessibilityPreset> UUEGTUserSettings::GetSelectableAccessibilityPresets()
{
	return {
		EUEGTAccessibilityPreset::Standard,
		EUEGTAccessibilityPreset::Comfort,
		EUEGTAccessibilityPreset::MaximumClarity
	};
}

void UUEGTUserSettings::ApplyAudioSettings() const
{
	if (GEngine != nullptr)
	{
		if (FAudioDevice* AudioDevice = GEngine->GetMainAudioDeviceRaw())
		{
			AudioDevice->SetTransientPrimaryVolume(
				static_cast<float>(MasterVolumePercent) / 100.0f);
		}
	}
	FApp::SetUnfocusedVolumeMultiplier(bMuteWhenUnfocused ? 0.0f : 1.0f);
}

void UUEGTUserSettings::ApplyLanguageSettings() const
{
	FInternationalization::Get().SetCurrentLanguageAndLocale(InterfaceCulture);
}

void UUEGTUserSettings::ApplyAccessibilitySettings() const
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetApplicationScale(
			static_cast<float>(UIScalePercent) / 100.0f);
	}
}

void UUEGTUserSettings::NormalizeCustomSettings()
{
	UIScalePercent = FMath::Clamp(UIScalePercent, 85, 150);
	CameraSpeedPercent = FMath::Clamp(CameraSpeedPercent, 50, 200);
	MasterVolumePercent = FMath::Clamp(MasterVolumePercent, 0, 100);
	if (static_cast<uint8>(ColorVisionMode) > static_cast<uint8>(EUEGTColorVisionMode::Tritanopia))
	{
		ColorVisionMode = EUEGTColorVisionMode::Standard;
	}
	if (static_cast<uint8>(EndTurnSafetyMode) > static_cast<uint8>(EUEGTEndTurnSafetyMode::Always))
	{
		EndTurnSafetyMode = EUEGTEndTurnSafetyMode::Smart;
	}
	InterfaceCulture.TrimStartAndEndInline();
	InterfaceCulture.ToLowerInline();
	if (!GetSupportedInterfaceCultures().Contains(InterfaceCulture))
	{
		InterfaceCulture = TEXT("en");
	}
	NormalizeInputBindings();
}

void UUEGTUserSettings::SetUIScalePercent(const int32 Value)
{
	UIScalePercent = FMath::Clamp(Value, 85, 150);
}

void UUEGTUserSettings::SetCameraSpeedPercent(const int32 Value)
{
	CameraSpeedPercent = FMath::Clamp(Value, 50, 200);
}

void UUEGTUserSettings::SetMasterVolumePercent(const int32 Value)
{
	MasterVolumePercent = FMath::Clamp(Value, 0, 100);
}

bool UUEGTUserSettings::ShouldConfirmEndTurn(
	const int32 ReadyAgentCount,
	const int32 RemainingActionPoints) const
{
	return EndTurnSafetyMode == EUEGTEndTurnSafetyMode::Always
		|| (EndTurnSafetyMode == EUEGTEndTurnSafetyMode::Smart
			&& ReadyAgentCount > 0 && RemainingActionPoints > 0);
}

void UUEGTUserSettings::SetInterfaceCulture(const FString& CultureName)
{
	InterfaceCulture = CultureName;
	InterfaceCulture.TrimStartAndEndInline();
	InterfaceCulture.ToLowerInline();
	if (!GetSupportedInterfaceCultures().Contains(InterfaceCulture))
	{
		InterfaceCulture = TEXT("en");
	}
}

TArray<FString> UUEGTUserSettings::GetSupportedInterfaceCultures()
{
	return FUEGTLocalizationService::GetSupportedCultures();
}

FString UUEGTUserSettings::GetInterfaceCultureDisplayName(const FString& CultureName)
{
	if (CultureName == TEXT("fr"))
	{
		return TEXT("FRANÇAIS");
	}
	if (CultureName == TEXT("de"))
	{
		return TEXT("DEUTSCH");
	}
	if (CultureName == TEXT("es"))
	{
		return TEXT("ESPAÑOL");
	}
	if (CultureName == TEXT("ja"))
	{
		return TEXT("日本語");
	}
	return TEXT("ENGLISH");
}

TArray<EUEGTInputCommand> UUEGTUserSettings::GetRemappableInputCommands()
{
	return UEGTUserSettingsPrivate::RemappableCommands();
}

FString UUEGTUserSettings::GetInputCommandDisplayName(const EUEGTInputCommand Command)
{
	switch (Command)
	{
	case EUEGTInputCommand::Confirm: return TEXT("CONFIRM / CONTINUE");
	case EUEGTInputCommand::EndTurn: return TEXT("END TURN");
	case EUEGTInputCommand::ToggleStance: return TEXT("TOGGLE STANCE");
	case EUEGTInputCommand::Reload: return TEXT("RELOAD");
	case EUEGTInputCommand::Objective: return TEXT("OBJECTIVE ACTION");
	case EUEGTInputCommand::Extract: return TEXT("EXTRACT");
	case EUEGTInputCommand::Door: return TEXT("DOOR ACTION");
	case EUEGTInputCommand::UseDevice: return TEXT("USE DEVICE");
	case EUEGTInputCommand::TerrainAttack: return TEXT("TERRAIN ATTACK");
	case EUEGTInputCommand::ProjectSignal: return TEXT("SIGNAL PRESSURE");
	case EUEGTInputCommand::NextUnit: return TEXT("NEXT UNIT");
	case EUEGTInputCommand::LevelUp: return TEXT("VIEW LEVEL UP");
	case EUEGTInputCommand::LevelDown: return TEXT("VIEW LEVEL DOWN");
	case EUEGTInputCommand::CycleWeapon: return TEXT("CYCLE WEAPON");
	case EUEGTInputCommand::ToggleFireMode: return TEXT("TOGGLE FIRE MODE");
	case EUEGTInputCommand::CycleDevice: return TEXT("CYCLE DEVICE");
	default: return TEXT("UNKNOWN COMMAND");
	}
}

FKey UUEGTUserSettings::GetDefaultInputKey(const EUEGTInputCommand Command)
{
	switch (Command)
	{
	case EUEGTInputCommand::Confirm: return EKeys::Enter;
	case EUEGTInputCommand::EndTurn: return EKeys::SpaceBar;
	case EUEGTInputCommand::ToggleStance: return EKeys::C;
	case EUEGTInputCommand::Reload: return EKeys::R;
	case EUEGTInputCommand::Objective: return EKeys::F;
	case EUEGTInputCommand::Extract: return EKeys::X;
	case EUEGTInputCommand::Door: return EKeys::O;
	case EUEGTInputCommand::UseDevice: return EKeys::G;
	case EUEGTInputCommand::TerrainAttack: return EKeys::T;
	case EUEGTInputCommand::ProjectSignal: return EKeys::V;
	case EUEGTInputCommand::NextUnit: return EKeys::Tab;
	case EUEGTInputCommand::LevelUp: return EKeys::PageUp;
	case EUEGTInputCommand::LevelDown: return EKeys::PageDown;
	case EUEGTInputCommand::CycleWeapon: return EKeys::One;
	case EUEGTInputCommand::ToggleFireMode: return EKeys::Two;
	case EUEGTInputCommand::CycleDevice: return EKeys::Three;
	default: return EKeys::Invalid;
	}
}

FName UUEGTUserSettings::GetInputKeyName(const EUEGTInputCommand Command) const
{
	switch (Command)
	{
	case EUEGTInputCommand::Confirm: return ConfirmKeyName;
	case EUEGTInputCommand::EndTurn: return EndTurnKeyName;
	case EUEGTInputCommand::ToggleStance: return StanceKeyName;
	case EUEGTInputCommand::Reload: return ReloadKeyName;
	case EUEGTInputCommand::Objective: return ObjectiveKeyName;
	case EUEGTInputCommand::Extract: return ExtractKeyName;
	case EUEGTInputCommand::Door: return DoorKeyName;
	case EUEGTInputCommand::UseDevice: return UseDeviceKeyName;
	case EUEGTInputCommand::TerrainAttack: return TerrainAttackKeyName;
	case EUEGTInputCommand::ProjectSignal: return ProjectSignalKeyName;
	case EUEGTInputCommand::NextUnit: return NextUnitKeyName;
	case EUEGTInputCommand::LevelUp: return LevelUpKeyName;
	case EUEGTInputCommand::LevelDown: return LevelDownKeyName;
	case EUEGTInputCommand::CycleWeapon: return CycleWeaponKeyName;
	case EUEGTInputCommand::ToggleFireMode: return ToggleFireModeKeyName;
	case EUEGTInputCommand::CycleDevice: return CycleDeviceKeyName;
	default: return NAME_None;
	}
}

void UUEGTUserSettings::SetInputKeyName(const EUEGTInputCommand Command, const FName KeyName)
{
	switch (Command)
	{
	case EUEGTInputCommand::Confirm: ConfirmKeyName = KeyName; break;
	case EUEGTInputCommand::EndTurn: EndTurnKeyName = KeyName; break;
	case EUEGTInputCommand::ToggleStance: StanceKeyName = KeyName; break;
	case EUEGTInputCommand::Reload: ReloadKeyName = KeyName; break;
	case EUEGTInputCommand::Objective: ObjectiveKeyName = KeyName; break;
	case EUEGTInputCommand::Extract: ExtractKeyName = KeyName; break;
	case EUEGTInputCommand::Door: DoorKeyName = KeyName; break;
	case EUEGTInputCommand::UseDevice: UseDeviceKeyName = KeyName; break;
	case EUEGTInputCommand::TerrainAttack: TerrainAttackKeyName = KeyName; break;
	case EUEGTInputCommand::ProjectSignal: ProjectSignalKeyName = KeyName; break;
	case EUEGTInputCommand::NextUnit: NextUnitKeyName = KeyName; break;
	case EUEGTInputCommand::LevelUp: LevelUpKeyName = KeyName; break;
	case EUEGTInputCommand::LevelDown: LevelDownKeyName = KeyName; break;
	case EUEGTInputCommand::CycleWeapon: CycleWeaponKeyName = KeyName; break;
	case EUEGTInputCommand::ToggleFireMode: ToggleFireModeKeyName = KeyName; break;
	case EUEGTInputCommand::CycleDevice: CycleDeviceKeyName = KeyName; break;
	default: break;
	}
}

FKey UUEGTUserSettings::GetInputKey(const EUEGTInputCommand Command) const
{
	return FKey(GetInputKeyName(Command));
}

bool UUEGTUserSettings::IsReservedInputKey(const FKey& Key)
{
	return Key == EKeys::Escape
		|| Key == EKeys::W || Key == EKeys::A || Key == EKeys::S || Key == EKeys::D
		|| Key == EKeys::Q || Key == EKeys::E
		|| Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::Left || Key == EKeys::Right
		|| Key == EKeys::Four || Key == EKeys::Five || Key == EKeys::Six;
}

bool UUEGTUserSettings::IsSupportedInputKey(const FKey& Key)
{
	return Key.IsValid() && Key.IsBindableToActions() && Key.IsDigital()
		&& !Key.IsGamepadKey() && !Key.IsMouseButton() && !Key.IsTouch()
		&& !Key.IsGesture() && !Key.IsVirtual() && !IsReservedInputKey(Key);
}

void UUEGTUserSettings::ResetInputBindingsToDefaults()
{
	for (const EUEGTInputCommand Command : UEGTUserSettingsPrivate::RemappableCommands())
	{
		SetInputKeyName(Command, GetDefaultInputKey(Command).GetFName());
	}
}

void UUEGTUserSettings::NormalizeInputBindings()
{
	TSet<FName> UsedKeys;
	for (const EUEGTInputCommand Command : UEGTUserSettingsPrivate::RemappableCommands())
	{
		const FKey Key = GetInputKey(Command);
		if (!IsSupportedInputKey(Key) || UsedKeys.Contains(Key.GetFName()))
		{
			ResetInputBindingsToDefaults();
			return;
		}
		UsedKeys.Add(Key.GetFName());
	}
}

bool UUEGTUserSettings::TrySetInputKey(
	const EUEGTInputCommand Command,
	const FKey& Key,
	FString& OutDiagnostic)
{
	FName DiagnosticCode;
	return TrySetInputKey(Command, Key, OutDiagnostic, DiagnosticCode);
}

bool UUEGTUserSettings::TrySetInputKey(
	const EUEGTInputCommand Command,
	const FKey& Key,
	FString& OutDiagnostic,
	FName& OutDiagnosticCode)
{
	OutDiagnostic.Empty();
	OutDiagnosticCode = NAME_None;
	if (!UEGTUserSettingsPrivate::RemappableCommands().Contains(Command))
	{
		OutDiagnosticCode = TEXT("input_command_not_remappable");
		OutDiagnostic = TEXT("The requested command is not remappable.");
		return false;
	}
	if (!IsSupportedInputKey(Key))
	{
		OutDiagnosticCode = TEXT("input_key_reserved");
		OutDiagnostic = TEXT("That key is reserved for camera, cursor, time, or settings navigation.");
		return false;
	}
	NormalizeInputBindings();
	const FKey PreviousKey = GetInputKey(Command);
	if (PreviousKey == Key)
	{
		OutDiagnosticCode = TEXT("input_binding_unchanged");
		OutDiagnostic = FString::Printf(TEXT("%s remains assigned to %s."),
			*GetInputCommandDisplayName(Command), *Key.GetDisplayName(false).ToString());
		return true;
	}

	for (const EUEGTInputCommand OtherCommand : UEGTUserSettingsPrivate::RemappableCommands())
	{
		if (OtherCommand != Command && GetInputKey(OtherCommand) == Key)
		{
			SetInputKeyName(OtherCommand, PreviousKey.GetFName());
			SetInputKeyName(Command, Key.GetFName());
			OutDiagnosticCode = TEXT("input_binding_swapped");
			OutDiagnostic = FString::Printf(TEXT("%s assigned to %s; %s moved to %s to avoid a conflict."),
				*GetInputCommandDisplayName(Command), *Key.GetDisplayName(false).ToString(),
				*GetInputCommandDisplayName(OtherCommand), *PreviousKey.GetDisplayName(false).ToString());
			return true;
		}
	}

	SetInputKeyName(Command, Key.GetFName());
	OutDiagnosticCode = TEXT("input_binding_assigned");
	OutDiagnostic = FString::Printf(TEXT("%s assigned to %s."),
		*GetInputCommandDisplayName(Command), *Key.GetDisplayName(false).ToString());
	return true;
}
