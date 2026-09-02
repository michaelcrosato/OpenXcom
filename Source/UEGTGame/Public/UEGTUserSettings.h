#pragma once

// Copyright 2026 UEGT contributors. MIT License.

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"

#include "UEGTUserSettings.generated.h"

UENUM(BlueprintType)
enum class EUEGTColorVisionMode : uint8
{
	Standard,
	Deuteranopia,
	Protanopia,
	Tritanopia
};

/** Local input-safety policy for ending a player tactical turn. */
UENUM(BlueprintType)
enum class EUEGTEndTurnSafetyMode : uint8
{
	Off,
	Smart,
	Always
};

/** Coherent local presentation/input-safety bundles offered before campaign creation. */
UENUM(BlueprintType)
enum class EUEGTAccessibilityPreset : uint8
{
	Standard,
	Comfort,
	MaximumClarity,
	Custom
};

/** Keyboard commands that can be reassigned without changing fixed pointer, camera, or gamepad navigation. */
UENUM(BlueprintType)
enum class EUEGTInputCommand : uint8
{
	Confirm,
	EndTurn,
	ToggleStance,
	Reload,
	Objective,
	Extract,
	Door,
	UseDevice,
	TerrainAttack,
	ProjectSignal,
	NextUnit,
	LevelUp,
	LevelDown,
	CycleWeapon,
	ToggleFireMode,
	CycleDevice
};

/** Persistent video, control, and accessibility preferences for the native UEGT shell. */
UCLASS(Config = GameUserSettings)
class UEGTGAME_API UUEGTUserSettings final : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UUEGTUserSettings(const FObjectInitializer& ObjectInitializer);

	static UUEGTUserSettings* Get();

	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;
	virtual void LoadSettings(bool bForceReload = false) override;
	virtual void SetToDefaults() override;

	void ApplyAccessibilitySettings() const;
	void ApplyAudioSettings() const;
	void ApplyLanguageSettings() const;
	void NormalizeCustomSettings();
	bool ApplyAccessibilityPreset(EUEGTAccessibilityPreset Preset);
	EUEGTAccessibilityPreset DetectAccessibilityPreset() const;
	static TArray<EUEGTAccessibilityPreset> GetSelectableAccessibilityPresets();

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Audio")
	int32 GetMasterVolumePercent() const { return MasterVolumePercent; }

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Audio")
	void SetMasterVolumePercent(int32 Value);

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Audio")
	bool ShouldMuteWhenUnfocused() const { return bMuteWhenUnfocused; }

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Audio")
	void SetMuteWhenUnfocused(bool bEnabled) { bMuteWhenUnfocused = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Language")
	FString GetInterfaceCulture() const { return InterfaceCulture; }

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Language")
	void SetInterfaceCulture(const FString& CultureName);

	static TArray<FString> GetSupportedInterfaceCultures();
	static FString GetInterfaceCultureDisplayName(const FString& CultureName);

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Controls")
	FKey GetInputKey(EUEGTInputCommand Command) const;

	bool TrySetInputKey(EUEGTInputCommand Command, const FKey& Key, FString& OutDiagnostic);
	/** Extended result keeps a stable presentation code without coupling settings policy to a locale. */
	bool TrySetInputKey(
		EUEGTInputCommand Command,
		const FKey& Key,
		FString& OutDiagnostic,
		FName& OutDiagnosticCode);
	void ResetInputBindingsToDefaults();
	static TArray<EUEGTInputCommand> GetRemappableInputCommands();
	static FString GetInputCommandDisplayName(EUEGTInputCommand Command);
	static FKey GetDefaultInputKey(EUEGTInputCommand Command);
	static bool IsSupportedInputKey(const FKey& Key);

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Accessibility")
	int32 GetUIScalePercent() const { return UIScalePercent; }

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Accessibility")
	void SetUIScalePercent(int32 Value);

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Accessibility")
	bool IsReducedMotionEnabled() const { return bReduceMotion; }

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Accessibility")
	void SetReducedMotionEnabled(bool bEnabled) { bReduceMotion = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Accessibility")
	bool IsHighContrastEnabled() const { return bHighContrast; }

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Accessibility")
	void SetHighContrastEnabled(bool bEnabled) { bHighContrast = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Accessibility")
	EUEGTColorVisionMode GetColorVisionMode() const { return ColorVisionMode; }

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Accessibility")
	void SetColorVisionMode(EUEGTColorVisionMode Mode) { ColorVisionMode = Mode; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Controls")
	int32 GetCameraSpeedPercent() const { return CameraSpeedPercent; }

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Controls")
	void SetCameraSpeedPercent(int32 Value);

	float GetCameraSpeedMultiplier() const
	{
		return static_cast<float>(CameraSpeedPercent) / 100.0f;
	}

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Gameplay")
	EUEGTEndTurnSafetyMode GetEndTurnSafetyMode() const { return EndTurnSafetyMode; }

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Gameplay")
	void SetEndTurnSafetyMode(EUEGTEndTurnSafetyMode Mode) { EndTurnSafetyMode = Mode; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Gameplay")
	bool ShouldConfirmEndTurn(int32 ReadyAgentCount, int32 RemainingActionPoints) const;

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Gameplay")
	bool ShouldAutoSelectReadyAgent() const { return bAutoSelectReadyAgent; }

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Gameplay")
	void SetAutoSelectReadyAgent(bool bEnabled) { bAutoSelectReadyAgent = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "UEGT|Settings|Gameplay")
	bool ShouldCenterCameraOnSelection() const { return bCenterCameraOnSelection; }

	UFUNCTION(BlueprintCallable, Category = "UEGT|Settings|Gameplay")
	void SetCenterCameraOnSelection(bool bEnabled) { bCenterCameraOnSelection = bEnabled; }

private:
	FName GetInputKeyName(EUEGTInputCommand Command) const;
	void SetInputKeyName(EUEGTInputCommand Command, FName KeyName);
	void NormalizeInputBindings();
	static bool IsReservedInputKey(const FKey& Key);

	UPROPERTY(Config)
	int32 MasterVolumePercent = 75;

	UPROPERTY(Config)
	bool bMuteWhenUnfocused = true;

	UPROPERTY(Config)
	FString InterfaceCulture = TEXT("en");

	UPROPERTY(Config)
	int32 UIScalePercent = 100;

	UPROPERTY(Config)
	bool bReduceMotion = true;

	UPROPERTY(Config)
	bool bHighContrast = true;

	UPROPERTY(Config)
	EUEGTColorVisionMode ColorVisionMode = EUEGTColorVisionMode::Standard;

	UPROPERTY(Config)
	int32 CameraSpeedPercent = 100;

	UPROPERTY(Config)
	EUEGTEndTurnSafetyMode EndTurnSafetyMode = EUEGTEndTurnSafetyMode::Smart;

	UPROPERTY(Config)
	bool bAutoSelectReadyAgent = true;

	UPROPERTY(Config)
	bool bCenterCameraOnSelection = true;

	UPROPERTY(Config)
	FName ConfirmKeyName = TEXT("Enter");

	UPROPERTY(Config)
	FName EndTurnKeyName = TEXT("SpaceBar");

	UPROPERTY(Config)
	FName StanceKeyName = TEXT("C");

	UPROPERTY(Config)
	FName ReloadKeyName = TEXT("R");

	UPROPERTY(Config)
	FName ObjectiveKeyName = TEXT("F");

	UPROPERTY(Config)
	FName ExtractKeyName = TEXT("X");

	UPROPERTY(Config)
	FName DoorKeyName = TEXT("O");

	UPROPERTY(Config)
	FName UseDeviceKeyName = TEXT("G");

	UPROPERTY(Config)
	FName TerrainAttackKeyName = TEXT("T");

	UPROPERTY(Config)
	FName ProjectSignalKeyName = TEXT("V");

	UPROPERTY(Config)
	FName NextUnitKeyName = TEXT("Tab");

	UPROPERTY(Config)
	FName LevelUpKeyName = TEXT("PageUp");

	UPROPERTY(Config)
	FName LevelDownKeyName = TEXT("PageDown");

	UPROPERTY(Config)
	FName CycleWeaponKeyName = TEXT("One");

	UPROPERTY(Config)
	FName ToggleFireModeKeyName = TEXT("Two");

	UPROPERTY(Config)
	FName CycleDeviceKeyName = TEXT("Three");
};
