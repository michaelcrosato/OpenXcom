// Copyright 2026 UEGT contributors. MIT License.

#include "Strategic/UEGTStrategicHudWidget.h"

#include "Localization/UEGTLocalizationService.h"
#include "Tactical/UEGTTacticalPlayerController.h"
#include "UEGTGameInstance.h"
#include "UEGTUserSettings.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace UEGTStrategicHudPrivate
{
	const FLinearColor PanelColor(0.01f, 0.02f, 0.055f, 0.93f);
	const FLinearColor PanelSoftColor(0.018f, 0.04f, 0.082f, 0.89f);
	const FLinearColor PrimaryText(0.84f, 0.92f, 1.0f, 1.0f);
	const FLinearColor SecondaryText(0.46f, 0.66f, 0.83f, 1.0f);
	const FLinearColor Accent(0.0f, 0.88f, 1.0f, 1.0f);
	const FLinearColor Warning(1.0f, 0.34f, 0.2f, 1.0f);
	const FLinearColor Success(0.2f, 0.95f, 0.62f, 1.0f);

	FString Localized(const TCHAR* Key, const TCHAR* EnglishFallback)
	{
		return FUEGTLocalizationService::Text(Key, EnglishFallback);
	}

	FString LocalizedFormat(
		const TCHAR* Key,
		const TCHAR* EnglishFallback,
		const FStringFormatOrderedArguments& Arguments)
	{
		const FString Pattern = Localized(Key, EnglishFallback);
		return FString::Format(*Pattern, Arguments);
	}

	FString LocalizedContentName(const FName RuleId, const FString& EnglishFallback)
	{
		return FUEGTLocalizationService::ContentName(RuleId, EnglishFallback);
	}

	FString RegionalSupportTierLabel(const ERegionalSupportTier Tier)
	{
		switch (Tier)
		{
		case ERegionalSupportTier::Suspended:
			return Localized(TEXT("strategic.region-support-suspended"), TEXT("SUSPENDED"));
		case ERegionalSupportTier::Strained:
			return Localized(TEXT("strategic.region-support-strained"), TEXT("STRAINED"));
		case ERegionalSupportTier::Committed:
			return Localized(TEXT("strategic.region-support-committed"), TEXT("COMMITTED"));
		case ERegionalSupportTier::Allied:
			return Localized(TEXT("strategic.region-support-allied"), TEXT("ALLIED"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString RegionalActionName(const ERegionalDiplomacyActionType ActionType)
	{
		switch (ActionType)
		{
		case ERegionalDiplomacyActionType::CivicRelief:
			return Localized(TEXT("strategic.region-action-civic-relief"), TEXT("CIVIC RELIEF"));
		case ERegionalDiplomacyActionType::SecurityAccord:
			return Localized(TEXT("strategic.region-action-security-accord"), TEXT("SECURITY ACCORD"));
		case ERegionalDiplomacyActionType::CrisisMobilization:
			return Localized(TEXT("strategic.region-action-crisis-mobilization"), TEXT("CRISIS MOBILIZATION"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString InterceptionPostureName(const EInterceptionPosture Posture)
	{
		switch (Posture)
		{
		case EInterceptionPosture::StandOffScreen:
			return Localized(TEXT("strategic.interception-posture-stand-off"), TEXT("STAND-OFF SCREEN"));
		case EInterceptionPosture::BalancedVector:
			return Localized(TEXT("strategic.interception-posture-balanced"), TEXT("BALANCED VECTOR"));
		case EInterceptionPosture::CloseAssault:
			return Localized(TEXT("strategic.interception-posture-close"), TEXT("CLOSE ASSAULT"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString InterceptionPostureTooltip(
		const EInterceptionPosture Posture,
		const FString& EnglishFallback)
	{
		switch (Posture)
		{
		case EInterceptionPosture::StandOffScreen:
			return Localized(TEXT("strategic.interception-posture-stand-off-tooltip"), *EnglishFallback);
		case EInterceptionPosture::BalancedVector:
			return Localized(TEXT("strategic.interception-posture-balanced-tooltip"), *EnglishFallback);
		case EInterceptionPosture::CloseAssault:
			return Localized(TEXT("strategic.interception-posture-close-tooltip"), *EnglishFallback);
		default:
			return EnglishFallback;
		}
	}

	FString InterceptionCoordinationName(const bool bActive)
	{
		return bActive
			? Localized(TEXT("strategic.interception-coordination-linked"), TEXT("LINKED WING"))
			: Localized(TEXT("strategic.interception-coordination-solo"), TEXT("SOLO VECTOR"));
	}

	FString InterceptionContactManeuverName(const EInterceptionContactManeuver Maneuver)
	{
		switch (Maneuver)
		{
		case EInterceptionContactManeuver::VectorSurvey:
			return Localized(
				TEXT("strategic.interception-contact-maneuver-survey"), TEXT("VECTOR SURVEY"));
		case EInterceptionContactManeuver::SignalShear:
			return Localized(
				TEXT("strategic.interception-contact-maneuver-shear"), TEXT("SIGNAL SHEAR"));
		case EInterceptionContactManeuver::BreaklineCounter:
			return Localized(
				TEXT("strategic.interception-contact-maneuver-breakline"), TEXT("BREAKLINE COUNTER"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString BaseDefenseDoctrineName(const EBaseDefenseFireDoctrine Doctrine)
	{
		switch (Doctrine)
		{
		case EBaseDefenseFireDoctrine::CoordinatedLine:
			return Localized(TEXT("strategic.base-defense-doctrine-coordinated"), TEXT("COORDINATED LINE"));
		case EBaseDefenseFireDoctrine::PrecisionScreen:
			return Localized(TEXT("strategic.base-defense-doctrine-precision"), TEXT("PRECISION SCREEN"));
		case EBaseDefenseFireDoctrine::BreachBreaker:
			return Localized(TEXT("strategic.base-defense-doctrine-breach"), TEXT("BREACH BREAKER"));
		case EBaseDefenseFireDoctrine::GridOvercharge:
			return Localized(TEXT("strategic.base-defense-doctrine-overcharge"), TEXT("GRID OVERCHARGE"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString BaseDefenseDoctrineTooltip(
		const EBaseDefenseFireDoctrine Doctrine,
		const FString& EnglishFallback,
		const int32 AccuracyBonus,
		const int32 DamagePercent)
	{
		switch (Doctrine)
		{
		case EBaseDefenseFireDoctrine::CoordinatedLine:
			return Localized(TEXT("strategic.base-defense-doctrine-coordinated-tooltip"), *EnglishFallback);
		case EBaseDefenseFireDoctrine::PrecisionScreen:
			return Localized(TEXT("strategic.base-defense-doctrine-precision-tooltip"), *EnglishFallback);
		case EBaseDefenseFireDoctrine::BreachBreaker:
			return Localized(TEXT("strategic.base-defense-doctrine-breach-tooltip"), *EnglishFallback);
		case EBaseDefenseFireDoctrine::GridOvercharge:
			return LocalizedFormat(
				TEXT("strategic.base-defense-doctrine-overcharge-tooltip-format"),
				TEXT("Prioritize higher-damage batteries, add {0} accuracy, scale damage to {1}%, and commit the displayed threat-indexed emergency-grid cost."),
				{ FString::FromInt(AccuracyBonus), FString::FromInt(DamagePercent) });
		default:
			return EnglishFallback;
		}
	}

	FString SignedAccuracyModifier(const int32 Modifier)
	{
		return FString::Printf(TEXT("%+d"), Modifier);
	}

	FString CompactMinutesSeconds(const int64 Seconds)
	{
		const int64 ClampedSeconds = FMath::Max<int64>(0, Seconds);
		return FString::Printf(
			TEXT("%lld:%02lld"),
			ClampedSeconds / 60,
			ClampedSeconds % 60);
	}

	FString LocalizedDiagnostic(const FName Code, const FString& EnglishFallback)
	{
		return FUEGTLocalizationService::DiagnosticText(Code, EnglishFallback);
	}

	FString LocalizedContentField(
		const FName RuleId,
		const FName FieldId,
		const FString& EnglishFallback)
	{
		return FUEGTLocalizationService::ContentField(RuleId, FieldId, EnglishFallback);
	}

	FString LocalizedDuration(const int64 Seconds)
	{
		if (Seconds <= 0)
		{
			return Localized(TEXT("strategic.duration-ready"), TEXT("Ready"));
		}
		const int64 Hours = (Seconds + 3599) / 3600;
		if (Hours < 48)
		{
			return LocalizedFormat(
				TEXT("strategic.duration-hours-remaining-format"),
				TEXT("{0} h remaining"),
				{ LexToString(Hours) });
		}
		return LocalizedFormat(
			TEXT("strategic.duration-days-hours-remaining-format"),
			TEXT("{0} d {1} h remaining"),
			{ LexToString(Hours / 24), LexToString(Hours % 24) });
	}

	FString LocalizedFacilityList(
		const TArray<FName>& FacilityIds,
		const TArray<FString>& EnglishNames,
		const TCHAR* Separator)
	{
		TArray<FString> Names;
		const int32 Count = FMath::Max(FacilityIds.Num(), EnglishNames.Num());
		Names.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FName FacilityId = FacilityIds.IsValidIndex(Index) ? FacilityIds[Index] : NAME_None;
			const FString EnglishName = EnglishNames.IsValidIndex(Index)
				? EnglishNames[Index]
				: FacilityId.IsNone() ? FString(TEXT("Unknown Facility")) : FacilityId.ToString();
			Names.Add(LocalizedContentName(FacilityId, EnglishName));
		}
		return FString::Join(Names, Separator);
	}

	FString LocalizedFacilitySummary(const FStrategicFacilityView& Facility)
	{
		const FString Name = LocalizedContentName(Facility.FacilityId, Facility.DisplayName);
		if (Facility.bConstructing)
		{
			return Name;
		}
		if (Facility.Damage <= 0)
		{
			return Name;
		}
		const FString State = Facility.bOperational
			? Localized(TEXT("strategic.facility-degraded"), TEXT("DEGRADED"))
			: Localized(TEXT("strategic.facility-offline"), TEXT("OFFLINE"));
		return FString::Printf(TEXT("%s / %s %d/%d"),
			*Name, *State, Facility.CurrentIntegrity, Facility.MaxIntegrity);
	}

	FString BaseSpecializationName(const FName SpecializationId)
	{
		if (SpecializationId == FName(TEXT("base.specialization.signal-relay")))
		{
			return Localized(TEXT("strategic.base-specialization-name-signal"), TEXT("SIGNAL RELAY"));
		}
		if (SpecializationId == FName(TEXT("base.specialization.research-enclave")))
		{
			return Localized(TEXT("strategic.base-specialization-name-research"), TEXT("RESEARCH ENCLAVE"));
		}
		if (SpecializationId == FName(TEXT("base.specialization.fabrication-works")))
		{
			return Localized(TEXT("strategic.base-specialization-name-fabrication"), TEXT("FABRICATION WORKS"));
		}
		if (SpecializationId == FName(TEXT("base.specialization.flight-operations")))
		{
			return Localized(TEXT("strategic.base-specialization-name-flight"), TEXT("FLIGHT OPERATIONS"));
		}
		if (SpecializationId == FName(TEXT("base.specialization.logistics-depot")))
		{
			return Localized(TEXT("strategic.base-specialization-name-logistics"), TEXT("LOGISTICS DEPOT"));
		}
		return Localized(
			TEXT("strategic.base-specialization-name-integrated"), TEXT("INTEGRATED COMMAND"));
	}

	FString BaseSpecializationMetric(const FName BenefitMetricId)
	{
		if (BenefitMetricId == FName(TEXT("base.specialization.detection-strength")))
		{
			return Localized(TEXT("strategic.base-specialization-metric-detection"), TEXT("DETECTION"));
		}
		if (BenefitMetricId == FName(TEXT("base.specialization.scientist-capacity")))
		{
			return Localized(TEXT("strategic.base-specialization-metric-scientist"), TEXT("SCI CAP"));
		}
		if (BenefitMetricId == FName(TEXT("base.specialization.engineer-capacity")))
		{
			return Localized(TEXT("strategic.base-specialization-metric-engineer"), TEXT("ENG CAP"));
		}
		if (BenefitMetricId == FName(TEXT("base.specialization.craft-berths")))
		{
			return Localized(TEXT("strategic.base-specialization-metric-craft"), TEXT("BERTHS"));
		}
		if (BenefitMetricId == FName(TEXT("base.specialization.storage-capacity")))
		{
			return Localized(TEXT("strategic.base-specialization-metric-storage"), TEXT("STORAGE"));
		}
		return Localized(
			TEXT("strategic.base-specialization-metric-balanced"), TEXT("BALANCED CAPABILITIES"));
	}

	FString LocalizedResearchProjectDetail(const FStrategicProjectView& Project)
	{
		if (Project.bPaused)
		{
			return LocalizedFormat(
				TEXT("strategic.research-paused-format"),
				TEXT("LAB OFFLINE • {0} • {1} scientists"),
				{
					LocalizedFacilityList(Project.MissingFacilityIds, Project.MissingFacilityNames, TEXT(" + ")),
					LexToString(Project.AssignedStaff)
				});
		}
		const FString RequiredFacilities = LocalizedFacilityList(
			Project.RequiredFacilityIds, Project.RequiredFacilityNames, TEXT(" + "));
		if (Project.AssignedStaff <= 0)
		{
			return RequiredFacilities.IsEmpty()
				? Localized(TEXT("strategic.research-unstaffed"), TEXT("Unstaffed"))
				: LocalizedFormat(
					TEXT("strategic.research-unstaffed-format"),
					TEXT("Unstaffed • lab {0}"),
					{ RequiredFacilities });
		}
		const FString Duration = LocalizedDuration(Project.RemainingSeconds);
		return RequiredFacilities.IsEmpty()
			? LocalizedFormat(
				TEXT("strategic.research-active-no-lab-format"),
				TEXT("{0} scientists • {1}"),
				{ LexToString(Project.AssignedStaff), Duration })
			: LocalizedFormat(
				TEXT("strategic.research-active-format"),
				TEXT("{0} scientists • {1} • lab {2}"),
				{ LexToString(Project.AssignedStaff), Duration, RequiredFacilities });
	}

	FString LocalizedResearchOptionDetail(const FStrategicActionOptionView& Option)
	{
		const FString RequiredFacilities = LocalizedFacilityList(
			Option.RequiredFacilityIds, Option.RequiredFacilityNames, TEXT(" + "));
		return RequiredFacilities.IsEmpty()
			? LocalizedFormat(
				TEXT("strategic.research-option-detail-no-lab-format"),
				TEXT("{0} scientist-hours"),
				{ LexToString(Option.DurationHours) })
			: LocalizedFormat(
				TEXT("strategic.research-option-detail-format"),
				TEXT("{0} scientist-hours • lab {1}"),
				{ LexToString(Option.DurationHours), RequiredFacilities });
	}

	FString LocalizedResearchUnavailableReason(const FStrategicActionOptionView& Option)
	{
		if (Option.UnavailableReasonCode == FName(TEXT("research_facility_missing")))
		{
			return LocalizedFormat(
				TEXT("strategic.research-facility-missing-format"),
				TEXT("The primary base requires operational facilities: {0}."),
				{ LocalizedFacilityList(
					Option.MissingFacilityIds, Option.MissingFacilityNames, TEXT(", ")) });
		}
		if (Option.UnavailableReasonCode == FName(TEXT("research_already_known")))
		{
			return Localized(TEXT("strategic.research-already-known"),
				TEXT("This topic is complete or already active."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("research_required")))
		{
			return Localized(TEXT("strategic.research-required"), TEXT("Required research is incomplete."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("base_required")))
		{
			return Localized(TEXT("strategic.base-required"),
				TEXT("Establish a base before ordering this program."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("campaign_concluded")))
		{
			return Localized(TEXT("strategic.campaign-concluded"), TEXT("The campaign has concluded."));
		}
		return LocalizedDiagnostic(Option.UnavailableReasonCode, Option.UnavailableReason);
	}

	FString ToggleLabel(const bool bEnabled)
	{
		return bEnabled
			? Localized(TEXT("common.on"), TEXT("ON"))
			: Localized(TEXT("common.off"), TEXT("OFF"));
	}

	FString EndTurnSafetyLabel(const EUEGTEndTurnSafetyMode Mode)
	{
		switch (Mode)
		{
		case EUEGTEndTurnSafetyMode::Off:
			return Localized(TEXT("gameplay.end-turn-off"), TEXT("OFF"));
		case EUEGTEndTurnSafetyMode::Always:
			return Localized(TEXT("gameplay.end-turn-always"), TEXT("ALWAYS"));
		default:
			return Localized(TEXT("gameplay.end-turn-smart"), TEXT("SMART"));
		}
	}

	FString AccessibilityPresetLabel(const EUEGTAccessibilityPreset Preset)
	{
		switch (Preset)
		{
		case EUEGTAccessibilityPreset::Standard:
			return Localized(TEXT("menu.preset-standard"), TEXT("STANDARD"));
		case EUEGTAccessibilityPreset::Comfort:
			return Localized(TEXT("menu.preset-comfort"), TEXT("COMFORT (RECOMMENDED)"));
		case EUEGTAccessibilityPreset::MaximumClarity:
			return Localized(TEXT("menu.preset-clarity"), TEXT("MAXIMUM CLARITY"));
		default:
			return Localized(TEXT("menu.preset-custom"), TEXT("CUSTOM SETTINGS ACTIVE"));
		}
	}

	FString AccessibilityPresetDetail(const EUEGTAccessibilityPreset Preset)
	{
		switch (Preset)
		{
		case EUEGTAccessibilityPreset::Standard:
			return Localized(TEXT("menu.preset-standard-detail"),
				TEXT("MOTION ON  •  SOFTER MARKERS"));
		case EUEGTAccessibilityPreset::Comfort:
			return Localized(TEXT("menu.preset-comfort-detail"),
				TEXT("REDUCED MOTION  •  HIGH CONTRAST"));
		case EUEGTAccessibilityPreset::MaximumClarity:
			return Localized(TEXT("menu.preset-clarity-detail"),
				TEXT("130% UI  •  SLOW CAMERA  •  ALWAYS CONFIRM"));
		default:
			return FString();
		}
	}

	TSharedRef<STextBlock> MakeText(
		const FString& Text,
		const int32 Size,
		const FLinearColor& Color = PrimaryText,
		const bool bBold = false)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Text))
			.Font(FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size))
			.ColorAndOpacity(Color)
			.AutoWrapText(true);
	}

	FString DifficultyLabel(const ECampaignDifficulty Difficulty)
	{
		switch (Difficulty)
		{
		case ECampaignDifficulty::Cadet: return Localized(TEXT("difficulty.cadet"), TEXT("CADET"));
		case ECampaignDifficulty::Standard: return Localized(TEXT("difficulty.standard"), TEXT("STANDARD"));
		case ECampaignDifficulty::Veteran: return Localized(TEXT("difficulty.veteran"), TEXT("VETERAN"));
		case ECampaignDifficulty::Apex: return Localized(TEXT("difficulty.apex"), TEXT("APEX"));
		default: return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString FundingModelLabel(const EUEGTFundingModel FundingModel)
	{
		switch (FundingModel)
		{
		case EUEGTFundingModel::RapidMobilization:
			return Localized(TEXT("menu.funding-mobilization"), TEXT("RAPID MOBILIZATION"));
		case EUEGTFundingModel::SustainedCharter:
			return Localized(TEXT("menu.funding-sustained"), TEXT("SUSTAINED CHARTER"));
		default:
			return Localized(TEXT("menu.funding-balanced"), TEXT("BALANCED MANDATE"));
		}
	}

	FString OutcomeLabel(const ECampaignOutcome Outcome)
	{
		switch (Outcome)
		{
		case ECampaignOutcome::Ongoing:
			return Localized(TEXT("strategic.outcome-active"), TEXT("OPERATIONS ACTIVE"));
		case ECampaignOutcome::Victory:
			return Localized(TEXT("strategic.outcome-victory"), TEXT("CAMPAIGN VICTORY"));
		case ECampaignOutcome::Failure:
			return Localized(TEXT("strategic.outcome-failure"), TEXT("CAMPAIGN FAILURE"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString OptionVerb(const EStrategicActionOptionType Type)
	{
		switch (Type)
		{
		case EStrategicActionOptionType::Research:
			return Localized(TEXT("strategic.option-research"), TEXT("RESEARCH"));
		case EStrategicActionOptionType::Facility: return TEXT("BUILD");
		case EStrategicActionOptionType::Personnel: return TEXT("RECRUIT");
		case EStrategicActionOptionType::Craft:
			return Localized(TEXT("strategic.option-craft"), TEXT("ACQUIRE"));
		case EStrategicActionOptionType::Manufacturing:
			return Localized(TEXT("strategic.option-manufacturing"), TEXT("MAKE"));
		default: return TEXT("ORDER");
		}
	}

	FString ProjectTypeLabel(const EStrategicProjectType Type)
	{
		switch (Type)
		{
		case EStrategicProjectType::Research:
			return Localized(TEXT("strategic.project-research"), TEXT("RESEARCH"));
		case EStrategicProjectType::Manufacturing:
			return Localized(TEXT("strategic.project-production"), TEXT("PRODUCTION"));
		case EStrategicProjectType::Construction: return TEXT("CONSTRUCTION");
		case EStrategicProjectType::Recruitment: return TEXT("INBOUND PERSONNEL");
		case EStrategicProjectType::CraftAcquisition:
			return Localized(TEXT("strategic.project-craft-inbound"), TEXT("INBOUND CRAFT"));
		default: return TEXT("PROJECT");
		}
	}

	FString TrainingFocusLabel(const EPersonnelTrainingFocus Focus)
	{
		switch (Focus)
		{
		case EPersonnelTrainingFocus::Accuracy:
			return Localized(TEXT("strategic.training-focus-accuracy"), TEXT("ACCURACY"));
		case EPersonnelTrainingFocus::Resolve:
			return Localized(TEXT("strategic.training-focus-resolve"), TEXT("RESOLVE"));
		case EPersonnelTrainingFocus::Mobility:
			return Localized(TEXT("strategic.training-focus-mobility"), TEXT("MOBILITY"));
		case EPersonnelTrainingFocus::Strength:
			return Localized(TEXT("strategic.training-focus-strength"), TEXT("STRENGTH"));
		default:
			return Localized(TEXT("strategic.personnel-status-unknown"), TEXT("UNKNOWN"));
		}
	}

	FString PersonnelStatusLabel(const EPersonnelStatus Status)
	{
		switch (Status)
		{
		case EPersonnelStatus::Available:
			return Localized(TEXT("strategic.personnel-status-available"), TEXT("AVAILABLE"));
		case EPersonnelStatus::Recovering:
			return Localized(TEXT("strategic.personnel-status-recovering"), TEXT("RECOVERING"));
		case EPersonnelStatus::Training:
			return Localized(TEXT("strategic.personnel-status-training"), TEXT("TRAINING"));
		case EPersonnelStatus::Deployed:
			return Localized(TEXT("strategic.personnel-status-deployed"), TEXT("DEPLOYED"));
		case EPersonnelStatus::Stewarding:
			return Localized(TEXT("strategic.personnel-status-stewarding"), TEXT("STEWARDING"));
		default:
			return Localized(TEXT("strategic.personnel-status-unknown"), TEXT("UNKNOWN"));
		}
	}

	FString StewardshipFocusLabel(const EPersonnelStewardshipFocus Focus)
	{
		switch (Focus)
		{
		case EPersonnelStewardshipFocus::RecoveryAdvocacy:
			return Localized(TEXT("strategic.stewardship-focus-recovery"), TEXT("RECOVERY ADVOCACY"));
		case EPersonnelStewardshipFocus::TrainingCadre:
			return Localized(TEXT("strategic.stewardship-focus-training"), TEXT("TRAINING CADRE"));
		case EPersonnelStewardshipFocus::RecruitmentLiaison:
			return Localized(TEXT("strategic.stewardship-focus-recruitment"), TEXT("RECRUITMENT LIAISON"));
		default:
			return Localized(TEXT("strategic.personnel-status-unknown"), TEXT("UNKNOWN"));
		}
	}

	FString PersonnelServiceBandLabel(const EPersonnelServiceBand Band)
	{
		switch (Band)
		{
		case EPersonnelServiceBand::FirstWatch:
			return Localized(TEXT("personnel.service-band-first-watch"), TEXT("FIRST WATCH"));
		case EPersonnelServiceBand::FieldProven:
			return Localized(TEXT("personnel.service-band-field-proven"), TEXT("FIELD PROVEN"));
		case EPersonnelServiceBand::LongWatch:
			return Localized(TEXT("personnel.service-band-long-watch"), TEXT("LONG WATCH"));
		case EPersonnelServiceBand::LegacyAnchor:
			return Localized(TEXT("personnel.service-band-legacy-anchor"), TEXT("LEGACY ANCHOR"));
		case EPersonnelServiceBand::EnduringBeacon:
			return Localized(TEXT("personnel.service-band-enduring-beacon"), TEXT("ENDURING BEACON"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString PersonnelServiceHistoryLabel(
		const FPersonnelServiceHistoryView& ServiceHistory,
		const int32 Missions)
	{
		if (ServiceHistory.bMaximumBand)
		{
			return LocalizedFormat(
				TEXT("personnel.service-maximum-format"),
				TEXT("SERVICE  {0}\nHIGHEST BAND  •  {1} MISSIONS"),
				{ PersonnelServiceBandLabel(ServiceHistory.Band), FString::FromInt(Missions) });
		}
		return LocalizedFormat(
			TEXT("personnel.service-progress-format"),
			TEXT("SERVICE  {0}\nNEXT {1} AT {2} MISSIONS  •  {3} TO GO"),
			{
				PersonnelServiceBandLabel(ServiceHistory.Band),
				PersonnelServiceBandLabel(ServiceHistory.NextBand),
				FString::FromInt(ServiceHistory.NextBandMissions),
				FString::FromInt(ServiceHistory.MissionsUntilNextBand)
			});
	}

	FString PersonnelMentorshipLabel(const FPersonnelMentorshipView& Mentorship)
	{
		const FString Title = Localized(
			TEXT("personnel.mentorship-watchkeeper-name"), TEXT("WATCHKEEPER GUIDANCE"));
		if (!Mentorship.bActive)
		{
			return LocalizedFormat(
				TEXT("personnel.mentorship-inactive-format"),
				TEXT("{0}  //  {1}  •  {2}\nDORMANT  •  NO LOWER-BAND TEAMMATES"),
				{
					Title,
					Mentorship.MentorDisplayName.ToUpper(),
					PersonnelServiceBandLabel(Mentorship.MentorServiceHistory.Band)
				});
		}
		return LocalizedFormat(
			TEXT("personnel.mentorship-active-format"),
			TEXT("{0}  //  {1}  •  {2}\nSTARTING MORALE +{3}  •  LOWER-BAND RECIPIENTS {4}"),
			{
				Title,
				Mentorship.MentorDisplayName.ToUpper(),
				PersonnelServiceBandLabel(Mentorship.MentorServiceHistory.Band),
				FString::FromInt(Mentorship.MoraleBonus),
				FString::FromInt(Mentorship.RecipientCount)
			});
	}

	FString PersonnelMentorshipGuidance()
	{
		return Localized(
			TEXT("personnel.mentorship-guidance"),
			TEXT("Long Watch mentors grant +5 starting morale; Legacy Anchors grant +10; Enduring Beacons grant +15. Only lower-band teammates receive guidance, capped at 100; selection is stable and uses no random draw."));
	}

	FString PersonnelLegacyRelayLabel(const FPersonnelLegacyRelayView& Relay)
	{
		const FString Title = Localized(
			TEXT("personnel.legacy-relay-name"), TEXT("LEGACY RELAY"));
		const FString DoctrineName = LocalizedContentName(
			Relay.DoctrineId, Relay.DoctrineDisplayName).ToUpper();
		if (!Relay.bActive)
		{
			return LocalizedFormat(
				TEXT("personnel.legacy-relay-inactive-format"),
				TEXT("{0}  //  {1}  •  {2}\nDORMANT  •  NO TEAMMATE TO RECEIVE THE RELAY"),
				{ Title, Relay.SpecialistDisplayName.ToUpper(), DoctrineName });
		}
		return LocalizedFormat(
			TEXT("personnel.legacy-relay-active-format"),
			TEXT("{0}  //  {1}  •  {2}\nFIELD RELAY  •  ACC +{3}  RES +{4}  MOB +{5}  STR +{6}  •  RECIPIENTS {7}"),
			{
				Title,
				Relay.SpecialistDisplayName.ToUpper(),
				DoctrineName,
				FString::FromInt(Relay.AccuracyBonus),
				FString::FromInt(Relay.ResolveBonus),
				FString::FromInt(Relay.MobilityBonus),
				FString::FromInt(Relay.StrengthBonus),
				FString::FromInt(Relay.RecipientCount)
			});
	}

	FString PersonnelLegacyRelayGuidance()
	{
		return Localized(
			TEXT("personnel.legacy-relay-guidance"),
			TEXT("A Legacy Anchor or higher with a maxed doctrine relays half its authored ACC/RES/MOB/STR bonuses, rounded up. Missions then IDs select the specialist; total bonus then doctrine ID select the doctrine. No random draw."));
	}

	FString PersonnelSquadBondTierLabel(const EPersonnelSquadBondTier Tier)
	{
		switch (Tier)
		{
		case EPersonnelSquadBondTier::Aligned:
			return Localized(TEXT("personnel.squad-bond-tier-aligned"), TEXT("ALIGNED"));
		case EPersonnelSquadBondTier::Interlocked:
			return Localized(TEXT("personnel.squad-bond-tier-interlocked"), TEXT("INTERLOCKED"));
		case EPersonnelSquadBondTier::Unbroken:
			return Localized(TEXT("personnel.squad-bond-tier-unbroken"), TEXT("UNBROKEN"));
		case EPersonnelSquadBondTier::None:
		default:
			return Localized(TEXT("personnel.squad-bond-tier-none"), TEXT("DEVELOPING"));
		}
	}

	FString PersonnelSquadBondActiveLabel(const FPersonnelSquadBondPairView& Pair)
	{
		return LocalizedFormat(
			TEXT("personnel.squad-bond-active-format"),
			TEXT("{0} + {1}  //  {2}  •  SHARED WINS {3}\nAP +{4}  •  MORALE +{5}"),
			{
				Pair.FirstDisplayName.ToUpper(),
				Pair.SecondDisplayName.ToUpper(),
				PersonnelSquadBondTierLabel(Pair.Tier),
				FString::FromInt(Pair.SharedVictories),
				FString::FromInt(Pair.ActionPointBonus),
				FString::FromInt(Pair.MoraleBonus)
			});
	}

	FString PersonnelSquadBondDevelopingLabel(const FPersonnelSquadBondPairView& Pair)
	{
		return LocalizedFormat(
			TEXT("personnel.squad-bond-developing-format"),
			TEXT("{0} + {1}  //  DEVELOPING {2}/{3}"),
			{
				Pair.FirstDisplayName.ToUpper(),
				Pair.SecondDisplayName.ToUpper(),
				FString::FromInt(Pair.SharedVictories),
				FString::FromInt(Pair.NextTierVictories)
			});
	}

	FString PersonnelSquadBondInactiveLabel(const FPersonnelSquadBondView& SquadBonds)
	{
		return LocalizedFormat(
			TEXT("personnel.squad-bond-inactive-format"),
			TEXT("NO ACTIVE CADENCE  •  DEVELOPING PAIRS {0}"),
			{ FString::FromInt(SquadBonds.DevelopingPairs.Num()) });
	}

	FString PersonnelSquadBondGuidance()
	{
		return Localized(
			TEXT("personnel.squad-bond-guidance"),
			TEXT("Successful operations advance every surviving pair. Deployment selects the strongest non-overlapping pairs by tier, shared wins, then stable identity; no random draw is used."));
	}

	FString CraftStatusLabel(const ECraftStatus Status)
	{
		switch (Status)
		{
		case ECraftStatus::Grounded:
			return Localized(TEXT("strategic.craft-status-grounded"), TEXT("GROUNDED"));
		case ECraftStatus::Servicing:
			return Localized(TEXT("strategic.craft-status-servicing"), TEXT("SERVICING"));
		case ECraftStatus::Airborne:
			return Localized(TEXT("strategic.craft-status-airborne"), TEXT("AIRBORNE"));
		case ECraftStatus::Intercepting:
			return Localized(TEXT("strategic.craft-status-intercepting"), TEXT("INTERCEPTING"));
		case ECraftStatus::Returning:
			return Localized(TEXT("strategic.craft-status-returning"), TEXT("RETURNING"));
		case ECraftStatus::Deploying:
			return Localized(TEXT("strategic.craft-status-deploying"), TEXT("DEPLOYING"));
		case ECraftStatus::OnSite:
			return Localized(TEXT("strategic.craft-status-on-site"), TEXT("ON SITE"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString MutualAidRouteLabel(const EMutualAidRoutePolicy Policy)
	{
		switch (Policy)
		{
		case EMutualAidRoutePolicy::OpenRelay:
			return Localized(TEXT("strategic.mutual-aid-route-open-relay"), TEXT("OPEN RELAY"));
		case EMutualAidRoutePolicy::RapidThread:
			return Localized(TEXT("strategic.mutual-aid-route-rapid-thread"), TEXT("RAPID THREAD"));
		case EMutualAidRoutePolicy::VeiledChain:
			return Localized(TEXT("strategic.mutual-aid-route-veiled-chain"), TEXT("VEILED CHAIN"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString WorksCadreCharterLabel(const EWorksCadreCharter Charter)
	{
		switch (Charter)
		{
		case EWorksCadreCharter::CommonCadence:
			return Localized(
				TEXT("strategic.works-charter-common-cadence"),
				TEXT("COMMON CADENCE"));
		case EWorksCadreCharter::AssemblyCadence:
			return Localized(
				TEXT("strategic.works-charter-assembly-cadence"),
				TEXT("ASSEMBLY CADENCE"));
		case EWorksCadreCharter::RestorationCadence:
			return Localized(
				TEXT("strategic.works-charter-restoration-cadence"),
				TEXT("RESTORATION CADENCE"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString ContactStatusLabel(const FStrategicContactView& Contact)
	{
		if (Contact.bAssaultPending)
		{
			return Localized(
				TEXT("strategic.contact-status-perimeter"), TEXT("AT BASE PERIMETER"));
		}
		switch (Contact.StatusType)
		{
		case EStrategicContactStatus::Detected:
			return Localized(TEXT("strategic.contact-status-detected"), TEXT("DETECTED"));
		case EStrategicContactStatus::Engaged:
			return Localized(TEXT("strategic.contact-status-engaged"), TEXT("ENGAGED"));
		default:
			return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString LocalizedCraftProjectDetail(const FStrategicProjectView& Project)
	{
		return LocalizedFormat(
			TEXT("strategic.craft-inbound-detail-format"),
			TEXT("IN TRANSIT • {0}"),
			{ LocalizedDuration(Project.RemainingSeconds) });
	}

	FString LocalizedCraftOptionDetail(const FStrategicActionOptionView& Option)
	{
		return LocalizedFormat(
			TEXT("strategic.craft-option-detail-format"),
			TEXT("Acquire • {0} h • hull {1} • crew {2}"),
			{
				FString::FromInt(Option.DurationHours),
				FString::FromInt(Option.CraftMaxHull),
				FString::FromInt(Option.CraftAgentCapacity)
			});
	}

	FString LocalizedCraftUnavailableReason(const FStrategicActionOptionView& Option)
	{
		if (Option.UnavailableReasonCode == FName(TEXT("craft_capacity_full")))
		{
			return Localized(
				TEXT("strategic.craft-berth-full"),
				TEXT("The primary base has no open craft berth."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("insufficient_funds")))
		{
			return Localized(
				TEXT("strategic.insufficient-funds"),
				TEXT("Current funds do not cover this order."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("research_required")))
		{
			return Localized(TEXT("strategic.research-required"), TEXT("Required research is incomplete."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("base_required")))
		{
			return Localized(
				TEXT("strategic.base-required"),
				TEXT("Establish a base before ordering this program."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("campaign_concluded")))
		{
			return Localized(TEXT("strategic.campaign-concluded"), TEXT("The campaign has concluded."));
		}
		return LocalizedDiagnostic(Option.UnavailableReasonCode, Option.UnavailableReason);
	}

	FString LocalizedContactName(const FStrategicContactView& Contact)
	{
		return LocalizedContentName(Contact.ContactRuleId, Contact.DisplayName);
	}

	FString LocalizedSiteName(const FStrategicSiteView& Site)
	{
		FString ContactFallback = Site.DisplayName;
		ContactFallback.RemoveFromEnd(Site.Type == EStrategicSiteType::Landing
			? TEXT(" Landing Site") : TEXT(" Wreckage"));
		if (ContactFallback.IsEmpty())
		{
			ContactFallback = Site.SourceContactRuleId.IsNone()
				? Site.DisplayName
				: Site.SourceContactRuleId.ToString();
		}
		return LocalizedFormat(
			Site.Type == EStrategicSiteType::Landing
				? TEXT("strategic.landing-site-name-format") : TEXT("strategic.site-name-format"),
			Site.Type == EStrategicSiteType::Landing ? TEXT("{0} Landing Site") : TEXT("{0} Wreckage"),
			{ LocalizedContentName(Site.SourceContactRuleId, ContactFallback) });
	}

	FString FacilityAbbreviation(const FString& DisplayName)
	{
		TArray<FString> Words;
		DisplayName.ParseIntoArrayWS(Words);
		FString Result;
		for (const FString& Word : Words)
		{
			if (!Word.IsEmpty())
			{
				Result.AppendChar(FChar::ToUpper(Word[0]));
				if (Result.Len() == 3)
				{
					break;
				}
			}
		}
		return Result.IsEmpty() ? TEXT("?") : Result;
	}

	FString MarkerTypeLabel(const EStrategicGlobeMarkerType Type)
	{
		switch (Type)
		{
		case EStrategicGlobeMarkerType::Base:
			return Localized(TEXT("strategic.marker-base"), TEXT("BASE"));
		case EStrategicGlobeMarkerType::Craft:
			return Localized(TEXT("strategic.marker-craft"), TEXT("CRAFT"));
		case EStrategicGlobeMarkerType::Contact:
			return Localized(TEXT("strategic.marker-contact"), TEXT("CONTACT"));
		case EStrategicGlobeMarkerType::Site:
			return Localized(TEXT("strategic.marker-site"), TEXT("SITE"));
		default:
			return Localized(TEXT("strategic.marker-generic"), TEXT("MARKER"));
		}
	}

	FString ColorVisionLabel(const EUEGTColorVisionMode Mode)
	{
		switch (Mode)
		{
		case EUEGTColorVisionMode::Deuteranopia:
			return Localized(TEXT("palette.deuteranopia"), TEXT("DEUTERANOPIA SAFE"));
		case EUEGTColorVisionMode::Protanopia:
			return Localized(TEXT("palette.protanopia"), TEXT("PROTANOPIA SAFE"));
		case EUEGTColorVisionMode::Tritanopia:
			return Localized(TEXT("palette.tritanopia"), TEXT("TRITANOPIA SAFE"));
		default:
			return Localized(TEXT("palette.standard"), TEXT("STANDARD"));
		}
	}

	FString QualityLabel(const int32 Quality)
	{
		switch (Quality)
		{
		case 0: return Localized(TEXT("quality.low"), TEXT("LOW"));
		case 1: return Localized(TEXT("quality.medium"), TEXT("MEDIUM"));
		case 2: return Localized(TEXT("quality.high"), TEXT("HIGH"));
		case 3: return Localized(TEXT("quality.epic"), TEXT("EPIC"));
		case 4: return Localized(TEXT("quality.cinematic"), TEXT("CINEMATIC"));
		default: return Localized(TEXT("quality.custom"), TEXT("CUSTOM"));
		}
	}

	FString LocalizedInputCommandLabel(const EUEGTInputCommand Command)
	{
		switch (Command)
		{
		case EUEGTInputCommand::Confirm:
			return Localized(TEXT("controls.command.confirm"), TEXT("CONFIRM / CONTINUE"));
		case EUEGTInputCommand::EndTurn:
			return Localized(TEXT("controls.command.end-turn"), TEXT("END TURN"));
		case EUEGTInputCommand::ToggleStance:
			return Localized(TEXT("controls.command.toggle-stance"), TEXT("TOGGLE STANCE"));
		case EUEGTInputCommand::Reload:
			return Localized(TEXT("controls.command.reload"), TEXT("RELOAD"));
		case EUEGTInputCommand::Objective:
			return Localized(TEXT("controls.command.objective"), TEXT("OBJECTIVE ACTION"));
		case EUEGTInputCommand::Extract:
			return Localized(TEXT("controls.command.extract"), TEXT("EXTRACT"));
		case EUEGTInputCommand::Door:
			return Localized(TEXT("controls.command.door"), TEXT("DOOR ACTION"));
		case EUEGTInputCommand::UseDevice:
			return Localized(TEXT("controls.command.use-device"), TEXT("USE DEVICE"));
		case EUEGTInputCommand::TerrainAttack:
			return Localized(TEXT("controls.command.terrain-attack"), TEXT("TERRAIN ATTACK"));
		case EUEGTInputCommand::ProjectSignal:
			return Localized(TEXT("controls.command.project-signal"), TEXT("SIGNAL PRESSURE"));
		case EUEGTInputCommand::NextUnit:
			return Localized(TEXT("controls.command.next-unit"), TEXT("NEXT UNIT"));
		case EUEGTInputCommand::LevelUp:
			return Localized(TEXT("controls.command.level-up"), TEXT("VIEW LEVEL UP"));
		case EUEGTInputCommand::LevelDown:
			return Localized(TEXT("controls.command.level-down"), TEXT("VIEW LEVEL DOWN"));
		case EUEGTInputCommand::CycleWeapon:
			return Localized(TEXT("controls.command.cycle-weapon"), TEXT("CYCLE WEAPON"));
		case EUEGTInputCommand::ToggleFireMode:
			return Localized(TEXT("controls.command.toggle-fire-mode"), TEXT("TOGGLE FIRE MODE"));
		case EUEGTInputCommand::CycleDevice:
			return Localized(TEXT("controls.command.cycle-device"), TEXT("CYCLE DEVICE"));
		default:
			return UUEGTUserSettings::GetInputCommandDisplayName(Command);
		}
	}

	const TArray<FKey>& InputKeyCycleOptions()
	{
		static const TArray<FKey> Options = {
			EKeys::Enter, EKeys::B, EKeys::SpaceBar, EKeys::V,
			EKeys::C, EKeys::H, EKeys::R, EKeys::J,
			EKeys::F, EKeys::K, EKeys::X, EKeys::L,
			EKeys::O, EKeys::P, EKeys::G, EKeys::U,
			EKeys::T, EKeys::Y, EKeys::Tab, EKeys::I,
			EKeys::PageUp, EKeys::Home, EKeys::PageDown, EKeys::End,
			EKeys::One, EKeys::Z, EKeys::Two, EKeys::N, EKeys::Three, EKeys::M
		};
		return Options;
	}

	int64 RequiredMaterialQuantity(const FStrategicMaterialRequirementView& Requirement, const int32 Units)
	{
		return Requirement.PerUnitQuantity <= 0 || Units <= 0 ? 0
			: static_cast<int64>(Requirement.PerUnitQuantity) > MAX_int64 / Units ? MAX_int64
			: static_cast<int64>(Requirement.PerUnitQuantity) * Units;
	}

	int64 StorageDeltaForUnits(const int64 PerUnitDelta, const int32 Units)
	{
		if (PerUnitDelta == 0 || Units <= 0)
		{
			return 0;
		}
		if (PerUnitDelta > 0)
		{
			return PerUnitDelta > MAX_int64 / Units ? MAX_int64 : PerUnitDelta * Units;
		}
		return PerUnitDelta < MIN_int64 / Units ? MIN_int64 : PerUnitDelta * Units;
	}

	int64 AdditionalStorageOverflow(const FStrategicBaseView* Base, const int64 Delta)
	{
		if (Base == nullptr || !Base->bStorageEnforced)
		{
			return 0;
		}
		if ((Delta > 0 && Base->StorageCommitted > MAX_int64 - Delta)
			|| (Delta < 0 && Base->StorageCommitted < MIN_int64 - Delta))
		{
			return MAX_int64;
		}
		const int64 ProjectedCommitted = FMath::Max<int64>(0, Base->StorageCommitted + Delta);
		const int64 ProjectedOverflow = FMath::Max<int64>(0, ProjectedCommitted - Base->StorageCapacity);
		return FMath::Max<int64>(0, ProjectedOverflow - Base->StorageOverflow);
	}

	bool HasStorageForChange(const FStrategicBaseView* Base, const int64 Delta)
	{
		return AdditionalStorageOverflow(Base, Delta) == 0;
	}

	FString StorageChangeUnavailableReason(const FStrategicBaseView* Base, const int64 Delta)
	{
		const int64 Additional = AdditionalStorageOverflow(Base, Delta);
		return Additional == MAX_int64
			? Localized(
				TEXT("strategic.storage-change-range"),
				TEXT("This storage change exceeds the supported numeric range."))
			: LocalizedFormat(
				TEXT("strategic.storage-free-required-format"),
				TEXT("Free {0} storage units before making this change."),
				{ LexToString(Additional) });
	}

	bool HasManufacturingMaterials(
		const TArray<FStrategicMaterialRequirementView>& Requirements,
		const int32 Units)
	{
		return !Requirements.ContainsByPredicate(
			[Units](const FStrategicMaterialRequirementView& Requirement)
			{
				return RequiredMaterialQuantity(Requirement, Units) > Requirement.AvailableQuantity;
			});
	}

	FString ManufacturingMaterialSummary(
		const TArray<FStrategicMaterialRequirementView>& Requirements,
		const int32 Units)
	{
		TArray<FString> Parts;
		Parts.Reserve(Requirements.Num());
		for (const FStrategicMaterialRequirementView& Requirement : Requirements)
		{
			const FString DisplayName = LocalizedContentName(Requirement.ItemId, Requirement.DisplayName);
			Parts.Add(LocalizedFormat(
				TEXT("strategic.manufacturing-material-stock-format"),
				TEXT("{0} {1} ({2} stock)"),
				{
					LexToString(RequiredMaterialQuantity(Requirement, Units)),
					DisplayName,
					FString::FromInt(Requirement.AvailableQuantity)
				}));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	FString ManufacturingMaterialUnitSummary(
		const TArray<FStrategicMaterialRequirementView>& Requirements)
	{
		TArray<FString> Parts;
		Parts.Reserve(Requirements.Num());
		for (const FStrategicMaterialRequirementView& Requirement : Requirements)
		{
			const FString DisplayName = LocalizedContentName(Requirement.ItemId, Requirement.DisplayName);
			Parts.Add(LocalizedFormat(
				TEXT("strategic.manufacturing-material-quantity-format"),
				TEXT("{0} {1}"),
				{ FString::FromInt(Requirement.PerUnitQuantity), DisplayName }));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	FString ManufacturingMaterialRefundSummary(
		const TArray<FStrategicMaterialRequirementView>& Requirements)
	{
		TArray<FString> Parts;
		for (const FStrategicMaterialRequirementView& Requirement : Requirements)
		{
			if (Requirement.RefundableQuantity > 0)
			{
				const FString DisplayName = LocalizedContentName(Requirement.ItemId, Requirement.DisplayName);
				Parts.Add(LocalizedFormat(
					TEXT("strategic.manufacturing-material-quantity-format"),
					TEXT("{0} {1}"),
					{ LexToString(Requirement.RefundableQuantity), DisplayName }));
			}
		}
		return FString::Join(Parts, TEXT(", "));
	}

	FString SignedStorageDelta(const int64 Delta)
	{
		return FString::Printf(TEXT("%s%lld"), Delta >= 0 ? TEXT("+") : TEXT(""), Delta);
	}

	FString LocalizedManufacturingProjectDetail(const FStrategicProjectView& Project)
	{
		const FString WorkState = Project.AssignedStaff > 0
			? LocalizedDuration(Project.RemainingSeconds)
			: Localized(TEXT("strategic.manufacturing-unstaffed"), TEXT("Unstaffed"));
		FString Detail = LocalizedFormat(
			TEXT("strategic.manufacturing-project-detail-format"),
			TEXT("{0} units • {1} engineers • {2} • Storage {3}/unit"),
			{
				FString::FromInt(Project.UnitsRemaining),
				FString::FromInt(Project.AssignedStaff),
				WorkState,
				SignedStorageDelta(Project.StorageDeltaPerUnit)
			});
		if (!Project.MaterialRequirements.IsEmpty())
		{
			Detail += LocalizedFormat(
				TEXT("strategic.manufacturing-inputs-per-unit-format"),
				TEXT(" • Inputs/unit: {0}"),
				{ ManufacturingMaterialUnitSummary(Project.MaterialRequirements) });
			const FString Refundable = ManufacturingMaterialRefundSummary(Project.MaterialRequirements);
			if (!Refundable.IsEmpty())
			{
				Detail += LocalizedFormat(
					TEXT("strategic.manufacturing-cancel-returns-format"),
					TEXT(" • Cancel returns: {0}"),
					{ Refundable });
			}
		}
		return Detail;
	}

	FString LocalizedManufacturingOptionDetail(
		const FStrategicActionOptionView& Option,
		const bool bIncludeUnitInputs)
	{
		FString Detail = LocalizedFormat(
			TEXT("strategic.manufacturing-option-detail-format"),
			TEXT("Manufacture 1 • {0} engineer-hours • Storage {1}/unit"),
			{ FString::FromInt(Option.DurationHours), SignedStorageDelta(Option.StorageDeltaPerUnit) });
		if (bIncludeUnitInputs && !Option.MaterialRequirements.IsEmpty())
		{
			Detail += LocalizedFormat(
				TEXT("strategic.manufacturing-option-inputs-format"),
				TEXT(" • Inputs: {0}"),
				{ ManufacturingMaterialSummary(Option.MaterialRequirements, 1) });
		}
		return Detail;
	}

	FString LocalizedManufacturingUnavailableReason(
		const FStrategicActionOptionView& Option,
		const FStrategicBaseView* Base)
	{
		if (Option.UnavailableReasonCode == FName(TEXT("manufacturing_facility_missing")))
		{
			return Localized(
				TEXT("strategic.manufacturing-facility-missing"),
				TEXT("An operational fabrication facility is required."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("manufacturing_materials_missing")))
		{
			return LocalizedFormat(
				TEXT("strategic.manufacturing-materials-missing-format"),
				TEXT("Required production materials are unavailable: {0}."),
				{ ManufacturingMaterialSummary(Option.MaterialRequirements, 1) });
		}
		if (Option.UnavailableReasonCode == FName(TEXT("storage_capacity_exceeded")))
		{
			return StorageChangeUnavailableReason(Base, Option.StorageDeltaPerUnit);
		}
		if (Option.UnavailableReasonCode == FName(TEXT("insufficient_funds")))
		{
			return Localized(
				TEXT("strategic.insufficient-funds"),
				TEXT("Current funds do not cover this order."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("research_required")))
		{
			return Localized(TEXT("strategic.research-required"), TEXT("Required research is incomplete."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("base_required")))
		{
			return Localized(TEXT("strategic.base-required"),
				TEXT("Establish a base before ordering this program."));
		}
		if (Option.UnavailableReasonCode == FName(TEXT("campaign_concluded")))
		{
			return Localized(TEXT("strategic.campaign-concluded"), TEXT("The campaign has concluded."));
		}
		return LocalizedDiagnostic(Option.UnavailableReasonCode, Option.UnavailableReason);
	}
}

TSharedRef<SWidget> UUEGTStrategicHudWidget::RebuildWidget()
{
	using namespace UEGTStrategicHudPrivate;

	TSharedRef<SWidget> Root =
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")))
			.BorderBackgroundColor(PanelColor)
			.Padding(FMargin(22.0f, 12.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(TitleText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24))
					.ColorAndOpacity(Accent)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SAssignNew(SubtitleText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
					.ColorAndOpacity(SecondaryText)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SAssignNew(StatusText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13))
					.AutoWrapText(true)
				]
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(18.0f, 118.0f, 0.0f, 120.0f))
		[
			SNew(SBox)
			.WidthOverride(370.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")))
				.BorderBackgroundColor(PanelSoftColor)
				.Padding(12.0f)
				[
					SAssignNew(LeftScrollBox, SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(LeftBox, SVerticalBox)
					]
				]
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(0.0f, 118.0f, 18.0f, 120.0f))
		[
			SNew(SBox)
			.WidthOverride(400.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")))
				.BorderBackgroundColor(PanelSoftColor)
				.Padding(12.0f)
				[
					SAssignNew(RightScrollBox, SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(RightBox, SVerticalBox)
					]
				]
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(405.0f, 0.0f, 435.0f, 112.0f))
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")))
			.BorderBackgroundColor(PanelSoftColor)
			.Padding(FMargin(12.0f, 7.0f))
			[
				SAssignNew(MarkerText, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13))
				.ColorAndOpacity(PrimaryText)
				.Justification(ETextJustify::Center)
				.AutoWrapText(true)
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")))
			.BorderBackgroundColor(PanelColor)
			.Padding(FMargin(18.0f, 12.0f))
			[
				SAssignNew(ActionBox, SWrapBox)
				.UseAllottedSize(true)
				.InnerSlotPadding(FVector2D(8.0f, 6.0f))
			]
		];

	RefreshSlate();
	return Root;
}

void UUEGTStrategicHudWidget::ShowMainMenu(
	const bool bInContentReady,
	const FString& InContentStatus)
{
	bMainMenu = true;
	bSettingsMenu = false;
	bSettingsControlsPage = false;
	bSettingsGameplayPage = false;
	bSaveBrowser = false;
	bKnowledgeArchive = false;
	bContentReady = bInContentReady;
	ContentStatus = InContentStatus;
	CurrentSnapshot = FStrategicDashboardSnapshot();
	bHasSelectedMarker = false;
	PendingDismissPersonnelId.Invalidate();
	PendingDismantleBaseId.Invalidate();
	PendingDismantleFacilityInstanceId.Invalidate();
	PendingFacilityRuleId = NAME_None;
	RefreshSlate();
}

FString UUEGTStrategicHudWidget::GetRenderedTitleText() const
{
	return TitleText.IsValid() ? TitleText->GetText().ToString() : FString();
}

FString UUEGTStrategicHudWidget::GetRenderedSubtitleText() const
{
	return SubtitleText.IsValid() ? SubtitleText->GetText().ToString() : FString();
}

FString UUEGTStrategicHudWidget::GetRenderedStatusText() const
{
	return StatusText.IsValid() ? StatusText->GetText().ToString() : FString();
}

void UUEGTStrategicHudWidget::ApplySnapshot(const FStrategicDashboardSnapshot& Snapshot)
{
	bMainMenu = false;
	bSettingsMenu = false;
	bSettingsControlsPage = false;
	bSettingsGameplayPage = false;
	bSaveBrowser = false;
	bKnowledgeArchive = false;
	CurrentSnapshot = Snapshot;
	bHasSelectedMarker = false;
	PendingDismissPersonnelId.Invalidate();
	PendingDismantleBaseId.Invalidate();
	PendingDismantleFacilityInstanceId.Invalidate();
	PendingFacilityRuleId = NAME_None;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::ShowStatusMessage(const FString& Message, const bool bIsError)
{
	StatusMessage = Message;
	bStatusIsError = bIsError;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::SelectGlobeMarker(const FStrategicGlobeMarkerView& Marker)
{
	SelectedMarker = Marker;
	bHasSelectedMarker = true;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::FocusFleetPanel()
{
	if (LeftScrollBox.IsValid() && FleetPanelAnchor.IsValid())
	{
		LeftScrollBox->ScrollDescendantIntoView(
			FleetPanelAnchor, false, EDescendantScrollDestination::TopOrLeft);
	}
}

void UUEGTStrategicHudWidget::FocusContactPanel()
{
	if (RightScrollBox.IsValid() && ContactPanelAnchor.IsValid())
	{
		RightScrollBox->ScrollDescendantIntoView(
			ContactPanelAnchor, false, EDescendantScrollDestination::TopOrLeft);
	}
}

void UUEGTStrategicHudWidget::FocusBaseDefensePanel()
{
	if (RightScrollBox.IsValid() && BaseDefensePanelAnchor.IsValid())
	{
		RightScrollBox->ScrollDescendantIntoView(
			BaseDefensePanelAnchor, false, EDescendantScrollDestination::TopOrLeft);
	}
}

void UUEGTStrategicHudWidget::FocusCoalitionEmergencyVotePanel()
{
	if (RightScrollBox.IsValid() && CoalitionEmergencyVotePanelAnchor.IsValid())
	{
		RightScrollBox->ScrollDescendantIntoView(
			CoalitionEmergencyVotePanelAnchor, false, EDescendantScrollDestination::TopOrLeft);
	}
}

void UUEGTStrategicHudWidget::FocusPersonnelPanel()
{
	if (LeftScrollBox.IsValid() && PersonnelPanelAnchor.IsValid())
	{
		LeftScrollBox->ScrollDescendantIntoView(
			PersonnelPanelAnchor, false, EDescendantScrollDestination::TopOrLeft);
	}
}

void UUEGTStrategicHudWidget::FocusMutualAidPanel()
{
	if (LeftScrollBox.IsValid() && MutualAidPanelAnchor.IsValid())
	{
		LeftScrollBox->ScrollDescendantIntoView(
			MutualAidPanelAnchor, false, EDescendantScrollDestination::TopOrLeft);
	}
}

void UUEGTStrategicHudWidget::ShowSettings()
{
	if (SeedTextBox.IsValid())
	{
		SeedText = SeedTextBox->GetText().ToString();
	}
	bSettingsMenu = true;
	bSettingsControlsPage = false;
	bSettingsGameplayPage = false;
	bKnowledgeArchive = false;
	StatusMessage.Empty();
	bStatusIsError = false;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::CloseSettings()
{
	bSettingsMenu = false;
	bSettingsControlsPage = false;
	bSettingsGameplayPage = false;
	StatusMessage.Empty();
	bStatusIsError = false;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::ShowControlSettings()
{
	bSettingsMenu = true;
	bSettingsControlsPage = true;
	bSettingsGameplayPage = false;
	StatusMessage.Empty();
	bStatusIsError = false;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::ShowGameplaySettings()
{
	bSettingsMenu = true;
	bSettingsControlsPage = false;
	bSettingsGameplayPage = true;
	StatusMessage.Empty();
	bStatusIsError = false;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::ShowSaveBrowser(const bool bForSaving)
{
	if (SeedTextBox.IsValid())
	{
		SeedText = SeedTextBox->GetText().ToString();
	}
	bSaveBrowser = true;
	bSaveBrowserForSaving = bForSaving;
	bKnowledgeArchive = false;
	PendingOverwriteSlot.Empty();
	StatusMessage.Empty();
	bStatusIsError = false;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::CloseSaveBrowser()
{
	bSaveBrowser = false;
	PendingOverwriteSlot.Empty();
	StatusMessage.Empty();
	bStatusIsError = false;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::ShowKnowledgeArchive()
{
	bSettingsMenu = false;
	bSettingsControlsPage = false;
	bSettingsGameplayPage = false;
	bSaveBrowser = false;
	bKnowledgeArchive = true;
	StatusMessage.Empty();
	bStatusIsError = false;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::CloseKnowledgeArchive()
{
	bKnowledgeArchive = false;
	StatusMessage.Empty();
	bStatusIsError = false;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::SetKnowledgeArchiveCategoryFilter(const FName CategoryId)
{
	SelectedArchiveCategoryId = CategoryId;
	SelectedArchiveEntryId = NAME_None;
	if (bKnowledgeArchive)
	{
		RefreshSlate();
	}
}

void UUEGTStrategicHudWidget::SetKnowledgeArchiveSearchText(const FString& SearchText)
{
	ArchiveSearchText = SearchText.TrimStartAndEnd().Left(128);
	SelectedArchiveEntryId = NAME_None;
	if (bKnowledgeArchive)
	{
		RefreshSlate();
	}
}

void UUEGTStrategicHudWidget::SelectKnowledgeArchiveRecord(const FName EntryId)
{
	if (!CurrentSnapshot.ArchiveEntries.ContainsByPredicate(
		[EntryId](const FStrategicArchiveEntryView& Entry) { return Entry.EntryId == EntryId; }))
	{
		return;
	}
	SelectedArchiveCategoryId = NAME_None;
	SelectedArchiveEntryId = EntryId;
	ArchiveSearchText.Empty();
	if (bKnowledgeArchive)
	{
		RefreshSlate();
	}
}

void UUEGTStrategicHudWidget::SelectFacilityForPlacement(const FName FacilityId)
{
	const FStrategicActionOptionView* Option = CurrentSnapshot.ActionOptions.FindByPredicate(
		[FacilityId](const FStrategicActionOptionView& View)
		{
			return View.Type == EStrategicActionOptionType::Facility && View.RuleId == FacilityId;
		});
	if (Option == nullptr || !Option->bAvailable || Option->ValidFacilityPlacements.IsEmpty())
	{
		const FString Fallback = UEGTStrategicHudPrivate::Localized(
			TEXT("strategic.facility-no-valid-placement"),
			TEXT("The selected facility has no valid placement at the primary base."));
		ShowStatusMessage(Option != nullptr
			? UEGTStrategicHudPrivate::LocalizedDiagnostic(
				Option->UnavailableReasonCode,
				Option->UnavailableReason.IsEmpty() ? Fallback : Option->UnavailableReason)
			: Fallback, true);
		return;
	}
	PendingFacilityRuleId = FacilityId;
	PendingDismissPersonnelId.Invalidate();
	PendingDismantleBaseId.Invalidate();
	PendingDismantleFacilityInstanceId.Invalidate();
	const FString FacilityName = UEGTStrategicHudPrivate::LocalizedContentName(
		Option->RuleId, Option->DisplayName);
	StatusMessage = UEGTStrategicHudPrivate::LocalizedFormat(
		TEXT("strategic.facility-placement-started-format"),
		TEXT("PLACING {0} ({1}×{2}): select one of {3} highlighted anchor cells on the primary-base grid."),
		{
			FText::FromString(FacilityName).ToUpper().ToString(),
			FString::FromInt(Option->FacilityGridWidth),
			FString::FromInt(Option->FacilityGridHeight),
			FString::FromInt(Option->ValidFacilityPlacements.Num())
		});
	bStatusIsError = false;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::CancelFacilityPlacement()
{
	PendingFacilityRuleId = NAME_None;
	StatusMessage = UEGTStrategicHudPrivate::Localized(
		TEXT("strategic.facility-placement-cancelled"), TEXT("Facility placement cancelled."));
	bStatusIsError = false;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::SelectFacilityForDismantle(
	const FGuid BaseId,
	const FGuid FacilityInstanceId)
{
	const FStrategicBaseView* Base = CurrentSnapshot.Bases.FindByPredicate(
		[BaseId](const FStrategicBaseView& View) { return View.BaseId == BaseId; });
	const FStrategicFacilityView* Facility = Base != nullptr
		? Base->FacilityLayout.FindByPredicate(
			[FacilityInstanceId](const FStrategicFacilityView& View)
			{
				return !View.bConstructing && View.FacilityInstanceId == FacilityInstanceId;
			})
		: nullptr;
	if (Facility == nullptr || !Facility->bCanDismantle)
	{
		const FString Fallback = UEGTStrategicHudPrivate::Localized(
			TEXT("strategic.facility-dismantle-unavailable"),
			TEXT("The selected installed facility cannot currently be dismantled."));
		ShowStatusMessage(Facility != nullptr
			? UEGTStrategicHudPrivate::LocalizedDiagnostic(
				Facility->DismantleUnavailableReasonCode,
				Facility->DismantleUnavailableReason.IsEmpty()
					? Fallback : Facility->DismantleUnavailableReason)
			: Fallback, true);
		return;
	}
	if (PendingDismantleBaseId == BaseId
		&& PendingDismantleFacilityInstanceId == FacilityInstanceId)
	{
		CancelFacilityDismantle();
		return;
	}
	PendingFacilityRuleId = NAME_None;
	PendingDismissPersonnelId.Invalidate();
	PendingDismantleBaseId = BaseId;
	PendingDismantleFacilityInstanceId = FacilityInstanceId;
	const FString FacilityName = UEGTStrategicHudPrivate::LocalizedContentName(
		Facility->FacilityId, Facility->DisplayName);
	StatusMessage = UEGTStrategicHudPrivate::LocalizedFormat(
		TEXT("strategic.facility-dismantle-review-format"),
		TEXT("DISMANTLE REVIEW: {0} will return {1} salvage. Select CONFIRM DISMANTLE to proceed."),
		{
			FText::FromString(FacilityName).ToUpper().ToString(),
			LexToString(Facility->DismantleRefund)
		});
	bStatusIsError = false;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::CancelFacilityDismantle()
{
	PendingDismantleBaseId.Invalidate();
	PendingDismantleFacilityInstanceId.Invalidate();
	StatusMessage = UEGTStrategicHudPrivate::Localized(
		TEXT("strategic.facility-dismantle-cancelled"), TEXT("Facility dismantling cancelled."));
	bStatusIsError = false;
	RefreshSlate();
}

void UUEGTStrategicHudWidget::RefreshSlate()
{
	using namespace UEGTStrategicHudPrivate;
	if (!TitleText.IsValid() || !SubtitleText.IsValid() || !StatusText.IsValid()
		|| !MarkerText.IsValid() || !LeftBox.IsValid() || !RightBox.IsValid() || !ActionBox.IsValid())
	{
		return;
	}
	LeftBox->ClearChildren();
	RightBox->ClearChildren();
	ActionBox->ClearChildren();
	RenderedCommandActionLabels.Reset();
	RenderedDynamicLabels.Reset();
	RenderedSaveBrowserLabels.Reset();
	RenderedArchiveLabels.Reset();
	RenderedAdversaryPlanIntelligenceCount = 0;
	RenderedCoalitionCounterplayCount = 0;
	RenderedContentReloadControlCount = 0;
	RenderedCraftRearmControlCount = 0;
	RenderedEnabledCraftRearmControlCount = 0;
	RenderedCraftServiceControlCount = 0;
	RenderedEnabledCraftServiceControlCount = 0;
	ArchiveSearchTextBox.Reset();
	PersonnelPanelAnchor.Reset();
	MutualAidPanelAnchor.Reset();
	FleetPanelAnchor.Reset();
	ContactPanelAnchor.Reset();
	BaseDefensePanelAnchor.Reset();
	CoalitionEmergencyVotePanelAnchor.Reset();
	MarkerText->SetText(FText::GetEmpty());
	StatusText->SetColorAndOpacity(bStatusIsError ? Warning : SecondaryText);

	if (bSettingsMenu)
	{
		BuildSettings();
	}
	else if (bSaveBrowser)
	{
		BuildSaveBrowser();
	}
	else if (bKnowledgeArchive)
	{
		BuildKnowledgeArchive();
	}
	else if (bMainMenu)
	{
		BuildMainMenu();
	}
	else if (!CurrentSnapshot.bSucceeded)
	{
		TitleText->SetText(FText::FromString(Localized(
			TEXT("strategic.command-title"), TEXT("UEGT  //  STRATEGIC COMMAND"))));
		SubtitleText->SetText(FText::FromString(Localized(
			TEXT("strategic.presentation-unavailable"), TEXT("CAMPAIGN PRESENTATION UNAVAILABLE"))));
		StatusText->SetText(FText::FromString(CurrentSnapshot.Diagnostics.IsEmpty()
			? Localized(TEXT("strategic.dashboard-unavailable"),
				TEXT("The campaign dashboard could not be built."))
			: CurrentSnapshot.Diagnostics[0]));
		LeftBox->AddSlot().AutoHeight()[MakeText(Localized(
			TEXT("strategic.return-or-diagnostics"),
			TEXT("Return to the main menu or inspect content diagnostics.")), 14, SecondaryText)];
	}
	else if (CurrentSnapshot.bRequiresBase)
	{
		BuildBasePlacement();
	}
	else
	{
		BuildDashboard();
	}
}

void UUEGTStrategicHudWidget::BuildMainMenu()
{
	using namespace UEGTStrategicHudPrivate;
	RenderedDifficultyProfileCount = 0;
	RenderedFundingModelOptionCount = 0;
	RenderedAccessibilityPresetOptionCount = 0;
	TitleText->SetText(FText::FromString(Localized(
		TEXT("menu.title"), TEXT("UEGT  //  GLOBAL RESPONSE COMMAND"))));
	SubtitleText->SetText(FText::FromString(Localized(
		TEXT("menu.subtitle"), TEXT("ORIGINAL DETERMINISTIC STRATEGY + TACTICAL DEFENSE"))));
	StatusText->SetText(FText::FromString(StatusMessage.IsEmpty()
		? (bContentReady
			? Localized(TEXT("menu.ready-status"),
				TEXT("Content verified. Establish a new command or recover the default campaign slot."))
			: ContentStatus)
		: StatusMessage));

	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
	[
		MakeText(Localized(TEXT("menu.hero-title"), TEXT("THE SIGNAL FRONT")), 20, Accent, true)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 18.0f)
	[
		MakeText(Localized(TEXT("menu.hero-body"),
			TEXT("Build a global response network, detect anomalous incursions, intercept hostile craft, recover remote sites, and command field teams through deterministic tactical operations.")), 14)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 7.0f)
	[
		MakeText(Localized(TEXT("menu.difficulty"), TEXT("DIFFICULTY")), 15, Accent, true)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		MakeText(Localized(TEXT("menu.difficulty-body"),
			TEXT("Relative to Standard: longer mission gaps provide more response time; escape impact scales score, funding, and pressure losses.")),
			10, SecondaryText)
	];
	const UUEGTGameInstance* GameInstance = GetWorld() != nullptr
		? GetWorld()->GetGameInstance<UUEGTGameInstance>()
		: nullptr;
	const FStrategicSimulationConfig DefaultSimulationConfig;
	for (const ECampaignDifficulty Difficulty : {
		ECampaignDifficulty::Cadet, ECampaignDifficulty::Standard,
		ECampaignDifficulty::Veteran, ECampaignDifficulty::Apex })
	{
		FAdversaryDifficultyTuning Tuning;
		const bool bHasTuning = GameInstance != nullptr
			? GameInstance->GetAdversaryDifficultyTuning(Difficulty, Tuning)
			: FStrategicCommandService::GetAdversaryDifficultyTuning(
				Difficulty, DefaultSimulationConfig, Tuning);
		if (!bHasTuning)
		{
			continue;
		}
		++RenderedDifficultyProfileCount;
		const bool bSelected = Difficulty == SelectedDifficulty;
		const FString ProfileLabel = LocalizedFormat(
			TEXT("menu.difficulty-profile-format"),
			TEXT("{0}\nMISSION GAP {1}% • ESCAPE IMPACT {2}%"),
			{
				DifficultyLabel(Difficulty),
				FString::FromInt(Tuning.MissionIntervalPercent),
				FString::FromInt(Tuning.EscapeConsequencePercent)
			});
		RenderedDynamicLabels.Add(ProfileLabel);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
		[
			SNew(SButton)
			.IsFocusable(true)
			.ButtonColorAndOpacity(bSelected ? FLinearColor(0.0f, 0.42f, 0.54f, 1.0f) : FLinearColor(0.03f, 0.11f, 0.2f, 1.0f))
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleDifficultyClicked, Difficulty)
			[
				MakeText(FString::Printf(
					TEXT("%s%s"), bSelected ? TEXT("▶  ") : TEXT("    "), *ProfileLabel), 12)
			]
		];
	}
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)
	[
		MakeText(Localized(TEXT("menu.funding-mandate"), TEXT("FUNDING MANDATE")),
			14, Accent, true)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		MakeText(Localized(TEXT("menu.funding-body"),
			TEXT("Trade opening reserves against recurring support. Difficulty sets the threat tempo and economy baseline.")),
			10, SecondaryText)
	];
	for (const EUEGTFundingModel FundingModel : {
		EUEGTFundingModel::BalancedMandate,
		EUEGTFundingModel::RapidMobilization,
		EUEGTFundingModel::SustainedCharter })
	{
		FUEGTFundingProjection Projection;
		if (!UUEGTGameInstance::CalculateFundingProjection(
			SelectedDifficulty, FundingModel, Projection))
		{
			continue;
		}
		++RenderedFundingModelOptionCount;
		const bool bSelected = FundingModel == SelectedFundingModel;
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SButton)
			.IsFocusable(true)
			.ButtonColorAndOpacity(bSelected
				? FLinearColor(0.0f, 0.42f, 0.54f, 1.0f)
				: FLinearColor(0.03f, 0.11f, 0.2f, 1.0f))
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleFundingModelClicked, FundingModel)
			[
				MakeText(FString::Printf(TEXT("%s%s\n%s %lld  •  %s %+lld"),
					bSelected ? TEXT("▶  ") : TEXT("    "),
					*FundingModelLabel(FundingModel),
					*Localized(TEXT("menu.funding-reserve"), TEXT("RESERVE")),
					Projection.StartingFunds,
					*Localized(TEXT("menu.funding-monthly"), TEXT("MONTHLY")),
					Projection.MonthlyFunding), 10)
			]
		];
	}

	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		MakeText(Localized(TEXT("menu.campaign-seed"), TEXT("CAMPAIGN SEED")), 15, Accent, true)
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
	[
		SAssignNew(SeedTextBox, SEditableTextBox)
		.Text(FText::FromString(SeedText))
		.HintText(FText::FromString(Localized(
			TEXT("menu.seed-hint"), TEXT("Signed 64-bit deterministic seed"))))
		.SelectAllTextWhenFocused(true)
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 4.0f)
	[
		MakeText(Localized(TEXT("menu.accessibility-preset"),
			TEXT("ACCESSIBILITY PRESET")), 14, Accent, true)
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		MakeText(Localized(TEXT("menu.accessibility-preset-body"),
			TEXT("Local presentation and safeguards only; palette, language, audio, bindings, video, and campaign rules stay unchanged.")),
			10, SecondaryText)
	];
	const UUEGTUserSettings* Settings = UUEGTUserSettings::Get();
	const EUEGTAccessibilityPreset ActivePreset = Settings != nullptr
		? Settings->DetectAccessibilityPreset()
		: EUEGTAccessibilityPreset::Custom;
	for (const EUEGTAccessibilityPreset Preset : UUEGTUserSettings::GetSelectableAccessibilityPresets())
	{
		++RenderedAccessibilityPresetOptionCount;
		const bool bSelected = Preset == ActivePreset;
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SButton)
			.IsFocusable(true)
			.ButtonColorAndOpacity(bSelected
				? FLinearColor(0.0f, 0.42f, 0.54f, 1.0f)
				: FLinearColor(0.03f, 0.11f, 0.2f, 1.0f))
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleAccessibilityPresetClicked, Preset)
			[
				MakeText(FString::Printf(TEXT("%s%s\n%s"),
					bSelected ? TEXT("▶  ") : TEXT("    "),
					*AccessibilityPresetLabel(Preset),
					*AccessibilityPresetDetail(Preset)), 11)
			]
		];
	}
	if (ActivePreset == EUEGTAccessibilityPreset::Custom)
	{
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 3.0f)
		[
			MakeText(AccessibilityPresetLabel(ActivePreset), 10, Warning, true)
		];
	}
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsEnabled(bContentReady)
		.IsFocusable(true)
		.ButtonColorAndOpacity(FLinearColor(0.0f, 0.42f, 0.48f, 1.0f))
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleStartCampaignClicked)
		[
			MakeText(Localized(TEXT("menu.begin-campaign"), TEXT("BEGIN NEW CAMPAIGN")),
				14, PrimaryText, true)
		]
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsEnabled(bContentReady)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleLoadClicked)
		[
			MakeText(Localized(TEXT("menu.load-campaign"), TEXT("LOAD CAMPAIGN")), 14)
		]
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 18.0f, 0.0f, 6.0f)
	[
		MakeText(Localized(TEXT("menu.content-status"), TEXT("CONTENT STATUS")), 15, Accent, true)
	];
	RightBox->AddSlot().AutoHeight()
	[
		MakeText(ContentStatus.IsEmpty()
			? (bContentReady
				? Localized(TEXT("menu.catalog-ready"), TEXT("Rule catalog ready"))
				: Localized(TEXT("menu.catalog-unavailable"), TEXT("Rule catalog unavailable")))
			: ContentStatus,
			13,
			bContentReady ? Success : Warning)
	];
	const FString ModsHelp = Localized(
		TEXT("menu.mods-help"),
		TEXT("User packages load before a campaign starts (from Saved/Mods by default). Reloading is disabled during an active campaign."));
	RenderedDynamicLabels.Add(ModsHelp);
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 3.0f)
	[
		MakeText(ModsHelp, 10, SecondaryText)
	];
	const FString ReloadContentLabel = Localized(
		TEXT("menu.reload-content"), TEXT("RELOAD CONTENT + MODS"));
	RenderedDynamicLabels.Add(ReloadContentLabel);
	++RenderedContentReloadControlCount;
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.ButtonColorAndOpacity(FLinearColor(0.03f, 0.16f, 0.24f, 1.0f))
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleReloadContentClicked)
		[
			MakeText(ReloadContentLabel, 12, PrimaryText, true)
		]
	];
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleSettingsClicked)
		[
			MakeText(Localized(TEXT("menu.settings"), TEXT("SETTINGS + ACCESSIBILITY")), 12)
		]
	];
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleQuitClicked)
		[
			MakeText(Localized(TEXT("menu.quit"), TEXT("QUIT")), 12)
		]
	];
}

void UUEGTStrategicHudWidget::BuildBasePlacement()
{
	using namespace UEGTStrategicHudPrivate;
	TitleText->SetText(FText::FromString(Localized(
		TEXT("strategic.founding-title"), TEXT("UEGT  //  ESTABLISH FIRST COMMAND"))));
	const FStringFormatOrderedArguments FoundingSubtitleArguments = {
		CurrentSnapshot.CampaignTimeUtc.ToString(TEXT("%Y-%m-%d %H:%M UTC")),
		FString::Printf(TEXT("%lld"), CurrentSnapshot.Funds),
		DifficultyLabel(CurrentSnapshot.Difficulty)
	};
	SubtitleText->SetText(FText::FromString(LocalizedFormat(
		TEXT("strategic.founding-subtitle-format"),
		TEXT("{0}  •  FUNDS {1}  •  SEED LOCKED  •  {2}"),
		FoundingSubtitleArguments)));
	StatusText->SetText(FText::FromString(StatusMessage.IsEmpty()
		? Localized(TEXT("strategic.founding-default-status"),
			TEXT("Choose a regional command site. The starter complex includes an operations hub; expand its grid to unlock air operations."))
		: StatusMessage));

	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
	[
		MakeText(Localized(TEXT("strategic.founding-directive"),
			TEXT("FOUNDING DIRECTIVE")), 17, Accent, true)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
	[
		MakeText(Localized(TEXT("strategic.founding-body"),
			TEXT("Regional placement establishes your sensor origin, flight routes, and initial pressure frontier. All choices use the same deterministic command rules.")), 14)
	];
	LeftBox->AddSlot().AutoHeight()
	[
		MakeText(LocalizedFormat(
			TEXT("strategic.founding-summary-format"),
			TEXT("CAMPAIGN SCORE  {0}\nMONTHLY FUNDING  {1}\nADVERSARY ARRIVAL  {2} h"),
			{
				FString::Printf(TEXT("%lld"), CurrentSnapshot.CampaignScore),
				FString::Printf(TEXT("%+lld"), CurrentSnapshot.MonthlyFunding),
				FString::Printf(TEXT("%lld"),
					(CurrentSnapshot.NextAdversaryMissionSeconds + 3599) / 3600)
			}), 14, SecondaryText)
	];

	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		MakeText(Localized(TEXT("strategic.available-regions"),
			TEXT("AVAILABLE REGIONS")), 17, Accent, true)
	];
	for (const FStrategicRegionView& Region : CurrentSnapshot.Regions)
	{
		const FString RegionDisplayName = LocalizedContentName(Region.RegionId, Region.DisplayName);
		const FStringFormatOrderedArguments RegionSummaryArguments = {
			RegionDisplayName.ToUpper(),
			FString::FromInt(Region.Pressure),
			FString::Printf(TEXT("%+.3f°"),
				static_cast<double>(Region.LongitudeMilliDegrees) / 1000.0),
			FString::Printf(TEXT("%+.3f°"),
				static_cast<double>(Region.LatitudeMilliDegrees) / 1000.0)
		};
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
		[
			SNew(SButton)
			.IsEnabled(CurrentSnapshot.Outcome == ECampaignOutcome::Ongoing)
			.IsFocusable(true)
			.ButtonColorAndOpacity(FLinearColor(0.0f, 0.25f, 0.38f, 1.0f))
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleRegionClicked, Region.RegionId)
			[
				MakeText(LocalizedFormat(
					TEXT("strategic.region-summary-format"),
					TEXT("{0}\nPRESSURE {1}   •   {2}, {3}"),
					RegionSummaryArguments), 14)
			]
		];
	}
	const FString SaveDefaultSlotLabel = Localized(
		TEXT("strategic.save-default-slot"), TEXT("SAVE  CAMPAIGN1"));
	RenderedCommandActionLabels.Add(SaveDefaultSlotLabel);
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleSaveClicked)
		[
			MakeText(SaveDefaultSlotLabel, 12)
		]
	];
	const FString SettingsLabel = Localized(TEXT("strategic.settings"), TEXT("SETTINGS"));
	RenderedCommandActionLabels.Add(SettingsLabel);
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleSettingsClicked)
		[
			MakeText(SettingsLabel, 12)
		]
	];
}

void UUEGTStrategicHudWidget::BuildCraftServicePanel(const FStrategicCraftView& Craft)
{
	using namespace UEGTStrategicHudPrivate;

	const auto CeilServiceHours = [](const int64 Seconds)
	{
		return Seconds <= 0 ? int64(0) : int64(1) + (Seconds - 1) / 3600;
	};
	const int64 ReadySeconds = Craft.ServiceQueue.bValid
		? Craft.ServiceQueue.EstimatedReadySeconds
		: Craft.RemainingServiceSeconds;
	const FString ServiceTitle = Localized(
		TEXT("strategic.craft-service-title"), TEXT("TURNAROUND SERVICE"));
	const FString ServiceSummary = LocalizedFormat(
		TEXT("strategic.craft-service-summary-format"),
		TEXT("REPAIR {0} h • REFUEL {1} h\nREADY {2} h • REFUND {3}"),
		{
			LexToString(CeilServiceHours(Craft.RemainingRepairSeconds)),
			LexToString(CeilServiceHours(Craft.RemainingRefuelSeconds)),
			LexToString(CeilServiceHours(ReadySeconds)),
			LexToString(Craft.ServiceCancellationRefund)
		});
	const FString ServiceRotationName = Localized(
		TEXT("strategic.craft-service-rotation-name"), TEXT("RAPID TURNAROUND"));
	const FString ServiceRotationState = !Craft.ServiceQueue.bValid
		? FString()
		: Craft.ServiceQueue.bInServiceLane
			? LocalizedFormat(
				TEXT("strategic.craft-service-rotation-active-format"),
				TEXT("SERVICE LANE {0}/{1} • READY IN {2} h"),
				{
					LexToString(Craft.ServiceQueue.ServiceLaneNumber),
					LexToString(Craft.ServiceQueue.ServiceLaneCount),
					LexToString(CeilServiceHours(Craft.ServiceQueue.EstimatedReadySeconds))
				})
			: LocalizedFormat(
				TEXT("strategic.craft-service-rotation-queued-format"),
				TEXT("QUEUE {0} • WAIT {1} h • READY IN {2} h"),
				{
					LexToString(Craft.ServiceQueue.WaitingPosition),
					LexToString(CeilServiceHours(Craft.ServiceQueue.EstimatedWaitSeconds)),
					LexToString(CeilServiceHours(Craft.ServiceQueue.EstimatedReadySeconds))
				});
	const FString ServiceRotationGuidance = Localized(
		TEXT("strategic.craft-service-rotation-guidance"),
		TEXT("Each operational craft facility supplies one lane. Shortest remaining turnaround goes first; stable craft identity breaks ties. Repair and refuel run together without a random draw."));
	const FString CancelServiceLabel = LocalizedFormat(
		TEXT("strategic.craft-service-cancel-format"),
		TEXT("CANCEL SERVICE • +{0}"),
		{ LexToString(Craft.ServiceCancellationRefund) });
	const FString CancelServiceTooltip = Craft.bCanCancelService
		? LocalizedFormat(
			TEXT("strategic.craft-service-cancel-tooltip-format"),
			TEXT("Cancel unfinished repair and refuel work. Completed components remain applied; {0} funds return."),
			{ LexToString(Craft.ServiceCancellationRefund) })
		: Localized(
			TEXT("strategic.craft-service-cancel-unavailable"),
			TEXT("The selected craft has no active service to cancel."));
	RenderedDynamicLabels.Add(ServiceTitle);
	if (Craft.ServiceQueue.bValid)
	{
		RenderedDynamicLabels.Add(ServiceRotationName);
		RenderedDynamicLabels.Add(ServiceRotationState);
		RenderedDynamicLabels.Add(ServiceRotationGuidance);
	}
	RenderedDynamicLabels.Add(ServiceSummary);
	RenderedDynamicLabels.Add(CancelServiceLabel);
	++RenderedCraftServiceControlCount;
	RenderedEnabledCraftServiceControlCount += Craft.bCanCancelService ? 1 : 0;
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 1.0f)
	[
		MakeText(ServiceTitle, 11, Warning, true)
	];
	if (Craft.ServiceQueue.bValid)
	{
		LeftBox->AddSlot().AutoHeight().Padding(10.0f, 1.0f, 0.0f, 0.0f)
		[
			MakeText(ServiceRotationName, 9,
				Craft.ServiceQueue.bInServiceLane ? Success : Warning, true)
		];
		LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 1.0f)
		[
			MakeText(ServiceRotationState, 9, SecondaryText, true)
		];
		LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 2.0f)
		[
			MakeText(ServiceRotationGuidance, 8, SecondaryText)
		];
	}
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
	[
		MakeText(ServiceSummary, 9, SecondaryText)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		SNew(SButton)
			.IsEnabled(Craft.bCanCancelService)
			.IsFocusable(true)
			.ButtonColorAndOpacity(Craft.bCanCancelService
				? FLinearColor(0.38f, 0.15f, 0.04f, 1.0f)
				: FLinearColor(0.07f, 0.08f, 0.10f, 1.0f))
			.ToolTipText(FText::FromString(CancelServiceTooltip))
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCancelCraftServiceClicked,
				Craft.CraftId)
			[
				MakeText(CancelServiceLabel, 9,
					Craft.bCanCancelService ? PrimaryText : SecondaryText)
			]
	];
}

void UUEGTStrategicHudWidget::BuildPersonnelRecoveryPlanPanel(
	const FStrategicPersonnelView& Person)
{
	using namespace UEGTStrategicHudPrivate;

	const FString RecoveryHeader = Localized(
		TEXT("strategic.recovery-plan-title"), TEXT("RETURN PATH"));
	RenderedDynamicLabels.Add(RecoveryHeader);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 2.0f)
	[
		MakeText(RecoveryHeader, 10,
			Person.RecoveryPlan.bDecisionRequired ? Warning : Accent, true)
	];
	if (Person.RecoveryPlan.bDecisionRequired)
	{
		const FString RecoveryGuidance = Localized(
			TEXT("strategic.recovery-plan-guidance"),
			TEXT("Choose readiness, funded speed, or a longer reflective recovery. Strategic time waits for this decision."));
		RenderedDynamicLabels.Add(RecoveryGuidance);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
		[
			MakeText(RecoveryGuidance, 9, SecondaryText)
		];
		TSharedRef<SWrapBox> RecoveryOptions = SNew(SWrapBox);
		for (const FPersonnelRecoveryPlanOptionView& Option : Person.RecoveryPlan.Options)
		{
			const FString PlanName = Option.Plan == EPersonnelRecoveryPlan::MeasuredReturn
				? Localized(TEXT("strategic.recovery-plan-measured"), TEXT("MEASURED RETURN"))
				: Option.Plan == EPersonnelRecoveryPlan::SurgeCare
					? Localized(TEXT("strategic.recovery-plan-surge"), TEXT("SURGE CARE"))
					: Localized(TEXT("strategic.recovery-plan-reflection"), TEXT("REFLECTION CYCLE"));
			const FString Hours = LexToString((Option.DurationSeconds + 3599) / 3600);
			const FString OptionLabel = Option.Plan == EPersonnelRecoveryPlan::MeasuredReturn
				? LocalizedFormat(
					TEXT("strategic.recovery-plan-measured-format"),
					TEXT("{0}\n{1} H • NO COST"), { PlanName, Hours })
				: Option.Plan == EPersonnelRecoveryPlan::SurgeCare
					? LocalizedFormat(
						TEXT("strategic.recovery-plan-surge-format"),
						TEXT("{0}\n{1} H • {2} FUNDS"),
						{ PlanName, Hours, LexToString(Option.FundingCost) })
					: LocalizedFormat(
						TEXT("strategic.recovery-plan-reflection-format"),
						TEXT("{0}\n{1} H • RES +{2}"),
						{ PlanName, Hours, FString::FromInt(Option.ResolveBonus) });
			const FString UnavailableReason = Option.bAvailable
				? FString()
				: LocalizedDiagnostic(Option.UnavailableReasonCode, Option.UnavailableReason);
			RenderedDynamicLabels.Add(OptionLabel);
			RecoveryOptions->AddSlot().Padding(FMargin(0.0f, 0.0f, 3.0f, 3.0f))
			[
				SNew(SButton)
				.IsEnabled(Option.bAvailable)
				.IsFocusable(true)
				.ToolTipText(FText::FromString(UnavailableReason.IsEmpty()
					? RecoveryGuidance : UnavailableReason))
				.ButtonColorAndOpacity(Option.bAvailable
					? FLinearColor(0.0f, 0.25f, 0.38f, 1.0f)
					: FLinearColor(0.035f, 0.075f, 0.12f, 1.0f))
				.OnClicked_UObject(this,
					&UUEGTStrategicHudWidget::HandlePersonnelRecoveryPlanClicked,
					Person.PersonnelId, Option.Plan)
				[
					MakeText(OptionLabel, 9,
						Option.bAvailable ? PrimaryText : SecondaryText)
				]
			];
		}
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
		[
			RecoveryOptions
		];
		return;
	}

	const EPersonnelRecoveryPlan Selected = Person.RecoveryPlan.SelectedPlan;
	const FString PlanName = Selected == EPersonnelRecoveryPlan::SurgeCare
		? Localized(TEXT("strategic.recovery-plan-surge"), TEXT("SURGE CARE"))
		: Selected == EPersonnelRecoveryPlan::ReflectionCycle
			? Localized(TEXT("strategic.recovery-plan-reflection"), TEXT("REFLECTION CYCLE"))
			: Localized(TEXT("strategic.recovery-plan-measured"), TEXT("MEASURED RETURN"));
	const FString SelectedLabel = LocalizedFormat(
		TEXT("strategic.recovery-plan-active-format"),
		TEXT("{0} • {1} H REMAINING"),
		{ PlanName, LexToString((Person.RemainingRecoverySeconds + 3599) / 3600) });
	RenderedDynamicLabels.Add(SelectedLabel);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
	[
		MakeText(SelectedLabel, 9, Success, true)
	];
}

void UUEGTStrategicHudWidget::BuildPersonnelStewardshipPanel(
	const FStrategicPersonnelView& Person)
{
	using namespace UEGTStrategicHudPrivate;

	const FPersonnelStewardshipView& Stewardship = Person.Stewardship;
	const FString Title = Localized(
		TEXT("strategic.stewardship-title"), TEXT("STEWARDSHIP ROTATION"));
	RenderedDynamicLabels.Add(Title);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 2.0f)
	[
		MakeText(Title, 10, Stewardship.bSelectedPersonnelIsSteward ? Success : Accent, true)
	];

	if (Stewardship.bSelectedPersonnelIsSteward)
	{
		const FString ActiveLabel = LocalizedFormat(
			TEXT("strategic.stewardship-active-format"),
			TEXT("{0} • {1} D REMAINING • {2}% BENEFIT"),
			{
				StewardshipFocusLabel(Stewardship.ActiveFocus),
				LexToString((Stewardship.RemainingSeconds + 86399) / 86400),
				FString::FromInt(Stewardship.ReductionPercent)
			});
		const FString EffectLabel = Stewardship.ActiveFocus == EPersonnelStewardshipFocus::RecoveryAdvocacy
			? LocalizedFormat(TEXT("strategic.stewardship-effect-recovery-format"),
				TEXT("Surge Care started at this base costs {0}% less."),
				{ FString::FromInt(Stewardship.ReductionPercent) })
			: Stewardship.ActiveFocus == EPersonnelStewardshipFocus::TrainingCadre
				? LocalizedFormat(TEXT("strategic.stewardship-effect-training-format"),
					TEXT("Personnel training started at this base takes {0}% less time."),
					{ FString::FromInt(Stewardship.ReductionPercent) })
				: LocalizedFormat(TEXT("strategic.stewardship-effect-recruitment-format"),
					TEXT("Personnel recruited to this base arrive {0}% sooner."),
					{ FString::FromInt(Stewardship.ReductionPercent) });
		const FString TourLabel = LocalizedFormat(
			TEXT("strategic.stewardship-tour-format"),
			TEXT("COMPLETED TOURS {0} • COMPLETION RESOLVE +{1} • REWARD TOURS {2}"),
			{
				FString::FromInt(Stewardship.ToursCompleted),
				FString::FromInt(Stewardship.ResolveBonusOnCompletion),
				FString::FromInt(Stewardship.ResolveAwardTourCap)
			});
		RenderedDynamicLabels.Add(ActiveLabel);
		RenderedDynamicLabels.Add(EffectLabel);
		RenderedDynamicLabels.Add(TourLabel);
		LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 1.0f)
		[
			SNew(SBox)
			.MaxDesiredWidth(320.0f)
			[
				MakeText(ActiveLabel, 9, Success, true)
			]
		];
		LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 1.0f)
		[
			SNew(SBox)
			.MaxDesiredWidth(320.0f)
			[
				MakeText(EffectLabel, 9, SecondaryText)
			]
		];
		LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 3.0f)
		[
			SNew(SBox)
			.MaxDesiredWidth(320.0f)
			[
				MakeText(TourLabel, 8, SecondaryText)
			]
		];
		return;
	}

	const FString Guidance = LocalizedFormat(
		TEXT("strategic.stewardship-guidance-format"),
		TEXT("Commit this veteran for {0} days. They cannot deploy, defend, train, or transfer during the rotation."),
		{ LexToString((Stewardship.DurationSeconds + 86399) / 86400) });
	const FString TourLabel = LocalizedFormat(
		TEXT("strategic.stewardship-ready-format"),
		TEXT("MISSIONS {0}/{1} • COMPLETED TOURS {2} • NEXT COMPLETION RESOLVE +{3}"),
		{
			FString::FromInt(Person.Missions),
			FString::FromInt(Stewardship.MinimumMissions),
			FString::FromInt(Stewardship.ToursCompleted),
			FString::FromInt(Stewardship.ResolveBonusOnCompletion)
		});
	RenderedDynamicLabels.Add(Guidance);
	RenderedDynamicLabels.Add(TourLabel);
	LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 1.0f)
	[
		SNew(SBox)
		.MaxDesiredWidth(320.0f)
		[
			MakeText(Guidance, 9, SecondaryText)
		]
	];
	LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 2.0f)
	[
		SNew(SBox)
		.MaxDesiredWidth(320.0f)
		[
			MakeText(TourLabel, 8, Accent, true)
		]
	];

	TSharedRef<SWrapBox> Options = SNew(SWrapBox);
	for (const FPersonnelStewardshipOptionView& Option : Stewardship.Options)
	{
		const FString OptionLabel = Option.Focus == EPersonnelStewardshipFocus::RecoveryAdvocacy
			? LocalizedFormat(TEXT("strategic.stewardship-option-recovery-format"),
				TEXT("{0}\n-{1}% SURGE FUNDING"),
				{ StewardshipFocusLabel(Option.Focus), FString::FromInt(Option.ReductionPercent) })
			: Option.Focus == EPersonnelStewardshipFocus::TrainingCadre
				? LocalizedFormat(TEXT("strategic.stewardship-option-training-format"),
					TEXT("{0}\n-{1}% TRAINING TIME"),
					{ StewardshipFocusLabel(Option.Focus), FString::FromInt(Option.ReductionPercent) })
				: LocalizedFormat(TEXT("strategic.stewardship-option-recruitment-format"),
					TEXT("{0}\n-{1}% RECRUITMENT TRANSIT"),
					{ StewardshipFocusLabel(Option.Focus), FString::FromInt(Option.ReductionPercent) });
		const FString UnavailableReason = Option.bAvailable
			? FString()
			: LocalizedDiagnostic(Option.UnavailableReasonCode, Option.UnavailableReason);
		RenderedDynamicLabels.Add(OptionLabel);
		Options->AddSlot().Padding(FMargin(0.0f, 0.0f, 3.0f, 3.0f))
		[
			SNew(SButton)
			.IsEnabled(Option.bAvailable)
			.IsFocusable(true)
			.ToolTipText(FText::FromString(UnavailableReason.IsEmpty() ? Guidance : UnavailableReason))
			.ButtonColorAndOpacity(Option.bAvailable
				? FLinearColor(0.0f, 0.25f, 0.38f, 1.0f)
				: FLinearColor(0.035f, 0.075f, 0.12f, 1.0f))
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandlePersonnelStewardshipClicked,
				Person.PersonnelId, Option.Focus)
			[
				MakeText(OptionLabel, 9, Option.bAvailable ? PrimaryText : SecondaryText)
			]
		];
	}
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
	[
		Options
	];
}

void UUEGTStrategicHudWidget::AppendBaseSpecialization(
	const FStrategicBaseView& Base)
{
	using namespace UEGTStrategicHudPrivate;
	const FString Name = BaseSpecializationName(Base.Specialization.SpecializationId);
	const FString Summary = Base.Specialization.bSpecialized
		? LocalizedFormat(
			TEXT("strategic.base-specialization-format"),
			TEXT("BASE SPECIALIZATION  •  {0}  •  INDEX {1}/100  •  {2} {3}"),
			{
				Name,
				FString::FromInt(Base.Specialization.Score),
				BaseSpecializationMetric(Base.Specialization.BenefitMetricId),
				LexToString(Base.Specialization.BenefitValue)
			})
		: LocalizedFormat(
			TEXT("strategic.base-specialization-integrated-format"),
			TEXT("BASE SPECIALIZATION  •  {0}  •  INDEX {1}/100"),
			{
				Name,
				FString::FromInt(Base.Specialization.Score)
			});
	const FString Guidance = Localized(
		TEXT("strategic.base-specialization-guidance"),
		TEXT("Derived from operational facility output; this read-only profile updates as infrastructure is repaired or lost and grants no separate bonus."));
	RenderedDynamicLabels.Add(Summary);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		SNew(SBox)
		.ToolTipText(FText::FromString(Guidance))
		[
			MakeText(Summary, 8, Base.Specialization.bSpecialized ? Success : SecondaryText, true)
		]
	];
}

void UUEGTStrategicHudWidget::AppendRelayQueuePressure(
	const FStrategicBaseView& Base)
{
	using namespace UEGTStrategicHudPrivate;
	if (Base.RelayQueueTotalConvoyCount <= 0 && Base.RelayChannelCount <= 0)
	{
		return;
	}

	const int64 TailHours = Base.RelayQueueTailArrivalSeconds <= 0
		? int64(0)
		: 1 + (Base.RelayQueueTailArrivalSeconds - 1) / 3600;
	const bool bOffline = Base.RelayQueueTotalConvoyCount > 0
		&& Base.RelayChannelCount <= 0;
	const FString Summary = bOffline
		? LocalizedFormat(
			TEXT("strategic.relay-queue-offline-format"),
			TEXT("RELAY LINE OFFLINE  •  {0} HOLDING  •  PRESSURE 100%"),
			{ FString::FromInt(Base.RelayQueueWaitingConvoyCount) })
		: Base.RelayQueueTotalConvoyCount <= 0
			? LocalizedFormat(
				TEXT("strategic.relay-queue-clear-format"),
				TEXT("RELAY LINE CLEAR  •  {0} CHANNELS READY"),
				{ FString::FromInt(Base.RelayChannelCount) })
			: LocalizedFormat(
				TEXT("strategic.relay-queue-pressure-format"),
				TEXT("RELAY LINE {0}/{1} ACTIVE  •  {2} WAITING  •  PRESSURE {3}%  •  TAIL {4} h"),
				{
					FString::FromInt(Base.RelayQueueActiveConvoyCount),
					FString::FromInt(Base.RelayQueueTotalConvoyCount),
					FString::FromInt(Base.RelayQueueWaitingConvoyCount),
					FString::FromInt(Base.RelayQueuePressurePercent),
					LexToString(TailHours)
				});
	RenderedDynamicLabels.Add(Summary);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		MakeText(Summary, 8,
			bOffline || Base.RelayQueuePressurePercent >= 50 ? Warning
				: Base.RelayQueuePressurePercent > 0 ? Accent : Success,
			true)
	];
}

void UUEGTStrategicHudWidget::AppendSignalWatchControls(
	const FStrategicBaseView& Base)
{
	using namespace UEGTStrategicHudPrivate;
	AppendRelayQueuePressure(Base);
	if (Base.FacilityRelayChannelCount <= 0 && Base.SignalWatchScientists <= 0)
	{
		AppendWorksCadreControls(Base);
		return;
	}

	const FString Summary = LocalizedFormat(
		TEXT("strategic.signal-watch-summary-format"),
		TEXT("SIGNAL WATCH  •  SCI {0}/{1}  •  RELAY CH {2}+{3}={4}"),
		{
			FString::FromInt(Base.SignalWatchScientists),
			FString::FromInt(Base.SignalWatchMaximumScientists),
			FString::FromInt(Base.FacilityRelayChannelCount),
			FString::FromInt(Base.SignalWatchBonusChannelCount),
			FString::FromInt(Base.RelayChannelCount)
		});
	const FString Guidance = Localized(
		TEXT("strategic.signal-watch-guidance"),
		TEXT("Each assigned scientist activates one surge channel, capped by operational signal infrastructure, and remains unavailable to research."));
	const FString IncreaseTooltip = Base.bCanIncreaseSignalWatch
		? Localized(
			TEXT("strategic.signal-watch-assign-tooltip"),
			TEXT("Move one available scientist from research capacity to Signal Watch."))
		: LocalizedDiagnostic(
			Base.SignalWatchIncreaseUnavailableReasonCode,
			Base.SignalWatchIncreaseUnavailableReason);
	const FString ReleaseTooltip = Localized(
		TEXT("strategic.signal-watch-release-tooltip"),
		TEXT("Release one Signal Watch scientist back to research capacity."));
	RenderedDynamicLabels.Add(Summary);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.MaxDesiredWidth(250.0f)
			.ToolTipText(FText::FromString(Guidance))
			[
				MakeText(Summary, 8,
					Base.SignalWatchBonusChannelCount > 0 ? Success : SecondaryText, true)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f, 2.0f, 0.0f)
		[
			SNew(SButton)
			.IsEnabled(Base.SignalWatchScientists > 0)
			.IsFocusable(true)
			.ToolTipText(FText::FromString(ReleaseTooltip))
			.OnClicked_UObject(this,
				&UUEGTStrategicHudWidget::HandleSignalWatchStaffClicked,
				Base.BaseId, -1)
			[
				MakeText(TEXT("−"), 11, PrimaryText, true)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.IsEnabled(Base.bCanIncreaseSignalWatch)
			.IsFocusable(true)
			.ToolTipText(FText::FromString(IncreaseTooltip))
			.OnClicked_UObject(this,
				&UUEGTStrategicHudWidget::HandleSignalWatchStaffClicked,
				Base.BaseId, 1)
			[
				MakeText(TEXT("+"), 11, PrimaryText, true)
			]
		]
	];
	AppendWorksCadreControls(Base);
}

void UUEGTStrategicHudWidget::AppendWorksCadreControls(
	const FStrategicBaseView& Base)
{
	using namespace UEGTStrategicHudPrivate;

	const FString Summary = LocalizedFormat(
		TEXT("strategic.works-cadre-summary-format"),
		TEXT("WORKS CADRE  •  ENG {0}/{1}  •  BUILD {2}%  •  REPAIR {3}%"),
		{
			FString::FromInt(Base.WorksCadreEngineers),
			FString::FromInt(Base.WorksCadreMaximumEngineers),
			FString::FromInt(Base.WorksCadreConstructionFrontloadPercent),
			FString::FromInt(Base.WorksCadreRepairFrontloadPercent)
		});
	const FString Guidance = Localized(
		TEXT("strategic.works-cadre-guidance"),
		TEXT("Works Cadre engineers remain unavailable to manufacturing. The selected Works Charter sets separate construction and repair front-loads for future commitments; existing clocks never change."));
	const FString IncreaseTooltip = Base.bCanIncreaseWorksCadre
		? Localized(
			TEXT("strategic.works-cadre-assign-tooltip"),
			TEXT("Move one available engineer from manufacturing capacity to Works Cadre."))
		: LocalizedDiagnostic(
			Base.WorksCadreIncreaseUnavailableReasonCode,
			Base.WorksCadreIncreaseUnavailableReason);
	const FString ReleaseTooltip = Localized(
		TEXT("strategic.works-cadre-release-tooltip"),
		TEXT("Release one Works Cadre engineer back to manufacturing capacity."));
	RenderedDynamicLabels.Add(Summary);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.MaxDesiredWidth(250.0f)
			.ToolTipText(FText::FromString(Guidance))
			[
				MakeText(Summary, 8,
					Base.WorksCadreEngineers > 0 ? Success : SecondaryText, true)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f, 2.0f, 0.0f)
		[
			SNew(SButton)
			.IsEnabled(Base.WorksCadreEngineers > 0)
			.IsFocusable(true)
			.ToolTipText(FText::FromString(ReleaseTooltip))
			.OnClicked_UObject(this,
				&UUEGTStrategicHudWidget::HandleWorksCadreStaffClicked,
				Base.BaseId, -1)
			[
				MakeText(TEXT("−"), 11, PrimaryText, true)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.IsEnabled(Base.bCanIncreaseWorksCadre)
			.IsFocusable(true)
			.ToolTipText(FText::FromString(IncreaseTooltip))
			.OnClicked_UObject(this,
				&UUEGTStrategicHudWidget::HandleWorksCadreStaffClicked,
				Base.BaseId, 1)
			[
				MakeText(TEXT("+"), 11, PrimaryText, true)
			]
		]
	];

	const FString CharterHeading = Localized(
		TEXT("strategic.works-charter-heading"),
		TEXT("WORKS CHARTER  •  FUTURE CLOCKS ONLY"));
	const FString CharterGuidance = Localized(
		TEXT("strategic.works-charter-guidance"),
		TEXT("Choose an even cadence or trade repair mobilization for construction speed, or construction mobilization for repair speed. Selection spends no funds and consumes no random draw."));
	RenderedDynamicLabels.Add(CharterHeading);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
	[
		MakeText(CharterHeading, 8, Accent, true)
	];
	TSharedRef<SWrapBox> CharterOptions = SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(FVector2D(3.0f, 3.0f));
	for (const FStrategicWorksCadreCharterOptionView& Option :
		Base.WorksCadreCharterOptions)
	{
		const FString OptionLabel = LocalizedFormat(
			TEXT("strategic.works-charter-option-format"),
			TEXT("{0}\nBUILD {1}%  •  REPAIR {2}%"),
			{
				WorksCadreCharterLabel(Option.Charter),
				FString::FromInt(Option.ConstructionFrontloadPercent),
				FString::FromInt(Option.RepairFrontloadPercent)
			});
		const FString UnavailableReason = !Option.bSelected && !Option.bEnabled
			? LocalizedDiagnostic(
				Option.UnavailableReasonCode, Option.UnavailableReason)
			: FString();
		RenderedDynamicLabels.Add(OptionLabel);
		CharterOptions->AddSlot().Padding(FMargin(0.0f, 0.0f, 3.0f, 3.0f))
		[
			SNew(SButton)
			.IsEnabled(Option.bEnabled)
			.IsFocusable(true)
			.ToolTipText(FText::FromString(UnavailableReason.IsEmpty()
				? CharterGuidance : UnavailableReason))
			.ButtonColorAndOpacity(Option.bSelected
				? FLinearColor(0.0f, 0.42f, 0.34f, 1.0f)
				: Option.bEnabled
					? FLinearColor(0.0f, 0.25f, 0.38f, 1.0f)
					: FLinearColor(0.035f, 0.075f, 0.12f, 1.0f))
			.OnClicked_UObject(this,
				&UUEGTStrategicHudWidget::HandleWorksCadreCharterClicked,
				Base.BaseId, Option.Charter)
			[
				MakeText(OptionLabel, 8,
					Option.bSelected ? Success
						: Option.bEnabled ? PrimaryText : SecondaryText, true)
			]
		];
	}
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		CharterOptions
	];
}

void UUEGTStrategicHudWidget::AppendMutualAidDispatchControls(
	const FStrategicBaseView& Base,
	const FStrategicInventoryView& Item)
{
	using namespace UEGTStrategicHudPrivate;
	for (const FStrategicMutualAidDispatchOptionView& Option : Item.MutualAidOptions)
	{
		const FStrategicMutualAidRouteOptionView* SelectedRoute =
			Option.Routes.FindByPredicate(
				[this](const FStrategicMutualAidRouteOptionView& Route)
				{
					return Route.Policy == SelectedMutualAidRoutePolicy;
				});
		if (SelectedRoute == nullptr && !Option.Routes.IsEmpty())
		{
			SelectedRoute = &Option.Routes[0];
		}
		const FString DestinationLabel = LocalizedFormat(
			TEXT("strategic.mutual-aid-destination-format"),
			TEXT("TO {0}"), { Option.DestinationBaseName.ToUpper() });
		const FString RouteHeading = Localized(
			TEXT("strategic.mutual-aid-route-heading"), TEXT("THREADLINE ROUTE"));
		const FString SendOneLabel = Localized(
			TEXT("strategic.mutual-aid-send-one-format"),
			TEXT("SEND 1"));
		const FString SendMaximumLabel = LocalizedFormat(
			TEXT("strategic.mutual-aid-send-maximum-format"),
			TEXT("SEND MAX ({0})"), { FString::FromInt(Option.MaximumQuantity) });
		const FString SelectedRouteName = SelectedRoute != nullptr
			? MutualAidRouteLabel(SelectedRoute->Policy)
			: Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		const FString RouteSummary = SelectedRoute == nullptr
			? LocalizedDiagnostic(Option.UnavailableReasonCode, Option.UnavailableReason)
			: SelectedRoute->bInterdictionExpected
				? LocalizedFormat(
					TEXT("strategic.mutual-aid-route-risk-format"),
					TEXT("{0}  •  {1} h  •  EXPOSURE {2}/100  •  DELAY +{3} h WITHOUT ESCORT"),
					{
						SelectedRouteName,
						LexToString((SelectedRoute->TransitSeconds + 3599) / 3600),
						FString::FromInt(SelectedRoute->RoutePressure),
						LexToString((SelectedRoute->InterdictionDelaySeconds + 3599) / 3600)
					})
				: LocalizedFormat(
					TEXT("strategic.mutual-aid-route-clear-format"),
					TEXT("{0}  •  {1} h  •  EXPOSURE {2}/100  •  ROUTE CLEAR"),
					{
						SelectedRouteName,
						LexToString((SelectedRoute->TransitSeconds + 3599) / 3600),
						FString::FromInt(SelectedRoute->RoutePressure)
					});
		const auto CeilHours = [](const int64 Seconds)
		{
			return Seconds <= 0 ? int64(0) : 1 + (Seconds - 1) / 3600;
		};
		const FMutualAidRelayQueueView* Relay = SelectedRoute != nullptr
			? &SelectedRoute->RelayQueue
			: nullptr;
		const int64 EstimatedArrivalSeconds = SelectedRoute == nullptr
			? 0
			: bSelectedMutualAidSignalEscort
				? SelectedRoute->EscortedEstimatedArrivalSeconds
				: SelectedRoute->RelayQueue.EstimatedArrivalSeconds;
		const FString RelaySummary = Relay == nullptr || !Relay->bValid
			|| !Relay->bRelayAvailable
			? Localized(
				TEXT("strategic.mutual-aid-relay-offline"),
				TEXT("RELAY WEAVE  •  SOURCE SIGNAL INFRASTRUCTURE OFFLINE"))
			: Relay->bInTransit
				? LocalizedFormat(
					TEXT("strategic.mutual-aid-relay-active-format"),
					TEXT("RELAY WEAVE  •  CH {0}/{1}  •  ACTIVE  •  ETA {2} h"),
					{
						FString::FromInt(Relay->RelayChannelNumber),
						FString::FromInt(Relay->RelayChannelCount),
						LexToString(CeilHours(EstimatedArrivalSeconds))
					})
				: LocalizedFormat(
					TEXT("strategic.mutual-aid-relay-held-format"),
					TEXT("RELAY WEAVE  •  QUEUE {0}  •  WAIT {1} h  •  ARRIVAL {2} h"),
					{
						FString::FromInt(Relay->WaitingPosition),
						LexToString(CeilHours(Relay->EstimatedWaitSeconds)),
						LexToString(CeilHours(EstimatedArrivalSeconds))
					});
		const int64 SignalEscortCost = SelectedRoute != nullptr
			? SelectedRoute->SignalEscortCost
			: 0;
		const FString EscortLabel = bSelectedMutualAidSignalEscort
			? LocalizedFormat(
				TEXT("strategic.mutual-aid-signal-escort-on-format"),
				TEXT("SIGNAL ESCORT ON  •  {0} FUNDS"),
				{ LexToString(SignalEscortCost) })
			: LocalizedFormat(
				TEXT("strategic.mutual-aid-signal-escort-off-format"),
				TEXT("SIGNAL ESCORT OFF  •  {0} FUNDS"),
				{ LexToString(SignalEscortCost) });
		const bool bEscortAffordable = SelectedRoute != nullptr
			&& SelectedRoute->bSignalEscortAffordable;
		const bool bCanDispatch = Option.bEnabled && SelectedRoute != nullptr
			&& (!bSelectedMutualAidSignalEscort || bEscortAffordable);
		const FString Tooltip = bCanDispatch
			? LocalizedFormat(
				TEXT("strategic.mutual-aid-tooltip-format"),
				TEXT("Reserve destination storage; {0} h route at exposure {1}/100. Cargo remains lossless and no random draw is used."),
				{
					LexToString((SelectedRoute->TransitSeconds + 3599) / 3600),
					FString::FromInt(SelectedRoute->RoutePressure)
				})
			: bSelectedMutualAidSignalEscort && !bEscortAffordable
				? LocalizedDiagnostic(
					TEXT("mutual_aid_signal_escort_funds"),
					TEXT("Campaign funds cannot cover this convoy's Signal Escort."))
			: LocalizedDiagnostic(Option.UnavailableReasonCode, Option.UnavailableReason);
		RenderedDynamicLabels.Add(DestinationLabel);
		RenderedDynamicLabels.Add(RouteHeading);
		RenderedDynamicLabels.Add(RouteSummary);
		RenderedDynamicLabels.Add(RelaySummary);
		RenderedDynamicLabels.Add(EscortLabel);
		RenderedDynamicLabels.Add(SendOneLabel);
		RenderedDynamicLabels.Add(SendMaximumLabel);
		const TSharedRef<STextBlock> DestinationAnchor =
			MakeText(DestinationLabel, 9, SecondaryText, true);
		if (!MutualAidPanelAnchor.IsValid())
		{
			MutualAidPanelAnchor = DestinationAnchor;
		}
		TSharedRef<SHorizontalBox> RouteButtons = SNew(SHorizontalBox);
		for (const FStrategicMutualAidRouteOptionView& Route : Option.Routes)
		{
			const FString PolicyLabel = MutualAidRouteLabel(Route.Policy);
			RenderedDynamicLabels.Add(PolicyLabel);
			RouteButtons->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)
			[
				SNew(SButton)
				.IsFocusable(true)
				.ButtonColorAndOpacity(Route.Policy == SelectedMutualAidRoutePolicy
					? FLinearColor(0.0f, 0.34f, 0.42f, 1.0f)
					: FLinearColor(0.04f, 0.12f, 0.18f, 1.0f))
				.OnClicked_UObject(
					this, &UUEGTStrategicHudWidget::HandleMutualAidRoutePolicyClicked,
					Route.Policy)
				[
					MakeText(PolicyLabel, 8, PrimaryText, true)
				]
			];
		}
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				DestinationAnchor
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 1.0f)
			[
				MakeText(RouteHeading, 8, SecondaryText, true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				RouteButtons
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				MakeText(RouteSummary, 8,
					SelectedRoute != nullptr && SelectedRoute->bInterdictionExpected
						? Warning : SecondaryText, true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				MakeText(RelaySummary, 8,
					Relay != nullptr && Relay->bInTransit ? Success : Warning, true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(SButton)
				.IsFocusable(true)
				.IsEnabled(bSelectedMutualAidSignalEscort || bEscortAffordable)
				.ButtonColorAndOpacity(bSelectedMutualAidSignalEscort
					? FLinearColor(0.34f, 0.20f, 0.03f, 1.0f)
					: FLinearColor(0.06f, 0.12f, 0.16f, 1.0f))
				.ToolTipText(FText::FromString(Localized(
					TEXT("strategic.mutual-aid-signal-escort-guidance"),
					TEXT("Commit the displayed funds at dispatch to prevent one forecast midpoint delay. Cargo is never lost."))))
				.OnClicked_UObject(
					this, &UUEGTStrategicHudWidget::HandleMutualAidSignalEscortClicked)
				[
					MakeText(EscortLabel, 8,
						bSelectedMutualAidSignalEscort ? Warning : SecondaryText, true)
				]
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.IsFocusable(true)
					.IsEnabled(bCanDispatch)
					.ToolTipText(FText::FromString(Tooltip))
					.OnClicked_UObject(
						this, &UUEGTStrategicHudWidget::HandleMutualAidConvoyClicked,
						Base.BaseId, Option.DestinationBaseId, Item.ItemId, 1,
						SelectedRoute != nullptr ? SelectedRoute->Policy
							: EMutualAidRoutePolicy::OpenRelay,
						bSelectedMutualAidSignalEscort)
					[
						MakeText(SendOneLabel, 9)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(SButton)
					.IsFocusable(true)
					.IsEnabled(bCanDispatch && Option.MaximumQuantity > 1)
					.ToolTipText(FText::FromString(Tooltip))
					.OnClicked_UObject(
						this, &UUEGTStrategicHudWidget::HandleMutualAidConvoyClicked,
						Base.BaseId, Option.DestinationBaseId,
						Item.ItemId, Option.MaximumQuantity,
						SelectedRoute != nullptr ? SelectedRoute->Policy
							: EMutualAidRoutePolicy::OpenRelay,
						bSelectedMutualAidSignalEscort)
					[
						MakeText(SendMaximumLabel, 9)
					]
				]
			]
		];
	}
}

void UUEGTStrategicHudWidget::AppendMutualAidConvoySummary()
{
	using namespace UEGTStrategicHudPrivate;
	if (CurrentSnapshot.MutualAidConvoys.IsEmpty())
	{
		return;
	}
	const FString ConvoyCountLabel = LocalizedFormat(
		TEXT("strategic.mutual-aid-in-transit-format"),
		TEXT("MUTUAL AID COMMITMENTS  {0}"),
		{ FString::FromInt(CurrentSnapshot.MutualAidConvoys.Num()) });
	RenderedDynamicLabels.Add(ConvoyCountLabel);
	const TSharedRef<STextBlock> ConvoyAnchor = MakeText(ConvoyCountLabel, 13, Accent, true);
	if (!MutualAidPanelAnchor.IsValid())
	{
		MutualAidPanelAnchor = ConvoyAnchor;
	}
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 4.0f)
	[
		ConvoyAnchor
	];
	for (const FStrategicMutualAidConvoyView& Convoy : CurrentSnapshot.MutualAidConvoys)
	{
		const FString RouteLabel = MutualAidRouteLabel(Convoy.RoutePolicy);
		const FString RouteStatus = Convoy.bSignalEscort
			? Localized(
				TEXT("strategic.mutual-aid-status-signal-escort"),
				TEXT("SIGNAL ESCORT ACTIVE"))
			: Convoy.InterdictionDelaySeconds > 0
				? LocalizedFormat(
					TEXT("strategic.mutual-aid-status-delayed-format"),
					TEXT("INTERDICTED  +{0} h"),
					{ LexToString((Convoy.InterdictionDelaySeconds + 3599) / 3600) })
				: !Convoy.bInterdictionResolved
					? Localized(
						TEXT("strategic.mutual-aid-status-forecast"),
						TEXT("INTERDICTION FORECAST"))
					: Localized(
						TEXT("strategic.mutual-aid-status-clear"),
						TEXT("ROUTE CLEAR"));
		const FString ConvoyLabel = LocalizedFormat(
			TEXT("strategic.mutual-aid-card-format"),
			TEXT("{0}  ×{1}\n{2}  →  {3}  •  {4} STORAGE  •  {5} h\n{6}  •  EXPOSURE {7}/100  •  {8}"),
			{
				LocalizedContentName(Convoy.ItemId, Convoy.ItemDisplayName),
				FString::FromInt(Convoy.Quantity), Convoy.SourceBaseName,
				Convoy.DestinationBaseName, LexToString(Convoy.TotalStorage),
				LexToString((Convoy.RemainingTransitSeconds + 3599) / 3600),
				RouteLabel, FString::FromInt(Convoy.RoutePressure), RouteStatus
			});
		const auto CeilHours = [](const int64 Seconds)
		{
			return Seconds <= 0 ? int64(0) : 1 + (Seconds - 1) / 3600;
		};
		const auto SignedCeilHours = [](const int64 Seconds)
		{
			const int64 Magnitude = Seconds < 0 ? -Seconds : Seconds;
			const int64 Hours = Magnitude == 0
				? 0
				: 1 + (Magnitude - 1) / 3600;
			return Seconds < 0
				? FString::Printf(TEXT("-%lld"), Hours)
				: Seconds > 0
					? FString::Printf(TEXT("+%lld"), Hours)
					: FString(TEXT("0"));
		};
		const FString RelayStatus = !Convoy.RelayQueue.bValid
			|| !Convoy.RelayQueue.bRelayAvailable
			? Localized(
				TEXT("strategic.mutual-aid-relay-offline"),
				TEXT("RELAY WEAVE  •  SOURCE SIGNAL INFRASTRUCTURE OFFLINE"))
			: Convoy.RelayQueue.bInTransit
				? LocalizedFormat(
					TEXT("strategic.mutual-aid-relay-active-format"),
					TEXT("RELAY WEAVE  •  CH {0}/{1}  •  ACTIVE  •  ETA {2} h"),
					{
						FString::FromInt(Convoy.RelayQueue.RelayChannelNumber),
						FString::FromInt(Convoy.RelayQueue.RelayChannelCount),
						LexToString(CeilHours(
							Convoy.RelayQueue.EstimatedArrivalSeconds))
					})
				: LocalizedFormat(
					TEXT("strategic.mutual-aid-relay-held-format"),
					TEXT("RELAY WEAVE  •  QUEUE {0}  •  WAIT {1} h  •  ARRIVAL {2} h"),
					{
						FString::FromInt(Convoy.RelayQueue.WaitingPosition),
						LexToString(CeilHours(
							Convoy.RelayQueue.EstimatedWaitSeconds)),
						LexToString(CeilHours(
							Convoy.RelayQueue.EstimatedArrivalSeconds))
					});
		RenderedDynamicLabels.Add(ConvoyLabel);
		RenderedDynamicLabels.Add(RelayStatus);
		TSharedRef<SVerticalBox> ConvoyPanel = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeText(ConvoyLabel, 10, PrimaryText, true)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(2.0f, 0.0f, 0.0f, 1.0f)
			[
				MakeText(RelayStatus, 8,
					Convoy.RelayQueue.bInTransit ? Success : Warning, true)
			];
		if (Convoy.RelayWaypointBaseId.IsValid())
		{
			const FString WaypointStatus = LocalizedFormat(
				TEXT("strategic.mutual-aid-relay-waypoint-current-format"),
				TEXT("RELAY WAYPOINT  •  VIA {0}  •  THEN {1}  •  EXPOSURE {2}/100  •  SOURCE CHANNEL HELD END-TO-END"),
				{
					Convoy.RelayWaypointBaseName,
					MutualAidRouteLabel(Convoy.OnwardRoutePolicy),
					FString::FromInt(Convoy.OnwardRoutePressure)
				});
			RenderedDynamicLabels.Add(WaypointStatus);
			ConvoyPanel->AddSlot().AutoHeight().Padding(2.0f, 0.0f, 0.0f, 1.0f)
			[
				MakeText(WaypointStatus, 8, Accent, true)
			];
			if (Convoy.BalancedHandoffQuantity > 0)
			{
				const FString HandoffStatus = LocalizedFormat(
					TEXT("strategic.mutual-aid-balanced-handoff-current-format"),
					TEXT("BALANCED HANDOFF  •  {0} TO {1}  •  {2} TO {3}  •  {4} STORAGE RESERVED"),
					{
						FString::FromInt(Convoy.BalancedHandoffQuantity),
						Convoy.RelayWaypointBaseName,
						FString::FromInt(Convoy.FinalDeliveryQuantity),
						Convoy.DestinationBaseName,
						LexToString(Convoy.BalancedHandoffStorage)
					});
				RenderedDynamicLabels.Add(HandoffStatus);
				ConvoyPanel->AddSlot().AutoHeight().Padding(2.0f, 0.0f, 0.0f, 1.0f)
				[
					MakeText(HandoffStatus, 8, Success, true)
				];
			}
		}
		else if (Convoy.CurrentLegOriginBaseId != Convoy.SourceBaseId)
		{
			const FString OnwardStatus = LocalizedFormat(
				TEXT("strategic.mutual-aid-relay-waypoint-onward-format"),
				TEXT("WAYPOINT REACHED  •  ONWARD FROM {0}  •  SOURCE CHANNEL STILL HELD"),
				{ Convoy.CurrentLegOriginBaseName });
			RenderedDynamicLabels.Add(OnwardStatus);
			ConvoyPanel->AddSlot().AutoHeight().Padding(2.0f, 0.0f, 0.0f, 1.0f)
			[
				MakeText(OnwardStatus, 8, Success, true)
			];
		}
		if (Convoy.bCanPrioritizeRelief)
		{
			const FString PriorityHeading = LocalizedFormat(
				TEXT("strategic.mutual-aid-relief-priority-heading-format"),
				TEXT("RELIEF PRIORITY  •  HELD LINE +{0}"),
				{ FString::FromInt(Convoy.ReliefPriorityBypassedConvoyCount) });
			const FString PriorityGuidance = Localized(
				TEXT("strategic.mutual-aid-relief-priority-guidance"),
				TEXT("Move this never-departed convoy to the front of the held relay line. Earlier held commitments move back in stable order; active work and every cargo contract stay fixed."));
			const FString PriorityAction = LocalizedFormat(
				TEXT("strategic.mutual-aid-relief-priority-action-format"),
				TEXT("ELEVATE RELIEF PRIORITY\nARRIVAL {0} h • RECOVER {1} h • PASS {2}"),
				{
					LexToString(CeilHours(
						Convoy.ReliefPriorityProjectedRelayQueue.EstimatedArrivalSeconds)),
					LexToString(CeilHours(
						Convoy.ReliefPriorityRecoveredWaitSeconds)),
					FString::FromInt(Convoy.ReliefPriorityBypassedConvoyCount)
				});
			RenderedDynamicLabels.Add(PriorityHeading);
			RenderedDynamicLabels.Add(PriorityAction);
			ConvoyPanel->AddSlot().AutoHeight().Padding(2.0f, 2.0f, 0.0f, 1.0f)
			[
				MakeText(PriorityHeading, 8, Accent, true)
			];
			ConvoyPanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(SButton)
				.IsFocusable(true)
				.IsEnabled(true)
				.ButtonColorAndOpacity(FLinearColor(0.04f, 0.12f, 0.18f, 1.0f))
				.ToolTipText(FText::FromString(PriorityGuidance))
				.OnClicked_UObject(
					this,
					&UUEGTStrategicHudWidget::HandleMutualAidReliefPriorityClicked,
					Convoy.ConvoyId)
				[
					MakeText(PriorityAction, 8, PrimaryText, true)
				]
			];
		}
		if (Convoy.bCanRetune)
		{
			const FString RetuneHeading = Localized(
				TEXT("strategic.mutual-aid-retune-heading"),
				TEXT("THREADLINE RETUNE  •  HELD BEFORE DEPARTURE"));
			const FString RetuneGuidance = Localized(
				TEXT("strategic.mutual-aid-retune-guidance"),
				TEXT("A held convoy may change route before departure. Cargo, storage, escort, funds, identity, and FIFO order remain committed."));
			RenderedDynamicLabels.Add(RetuneHeading);
			TSharedRef<SHorizontalBox> RetuneButtons = SNew(SHorizontalBox);
			for (const FStrategicMutualAidRouteOptionView& Route : Convoy.RetuneRoutes)
			{
				const FString OptionLabel = LocalizedFormat(
					TEXT("strategic.mutual-aid-retune-option-format"),
					TEXT("{0}\n{1} h • EXP {2}"),
					{
						MutualAidRouteLabel(Route.Policy),
						LexToString(CeilHours(Route.TransitSeconds)),
						FString::FromInt(Route.RoutePressure)
					});
				const FString Tooltip = Route.bEnabled || Route.UnavailableReason.IsEmpty()
					? RetuneGuidance
					: LocalizedDiagnostic(
						Route.UnavailableReasonCode, Route.UnavailableReason);
				RenderedDynamicLabels.Add(OptionLabel);
				RetuneButtons->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)
				[
					SNew(SButton)
					.IsFocusable(true)
					.IsEnabled(Route.bEnabled)
					.ButtonColorAndOpacity(Route.Policy == Convoy.RoutePolicy
						? FLinearColor(0.0f, 0.34f, 0.42f, 1.0f)
						: FLinearColor(0.04f, 0.12f, 0.18f, 1.0f))
					.ToolTipText(FText::FromString(Tooltip))
					.OnClicked_UObject(
						this, &UUEGTStrategicHudWidget::HandleMutualAidRetuneClicked,
						Convoy.ConvoyId, Route.Policy)
					[
						MakeText(OptionLabel, 7,
							Route.Policy == Convoy.RoutePolicy ? Accent : PrimaryText, true)
					]
				];
			}
			ConvoyPanel->AddSlot().AutoHeight().Padding(2.0f, 2.0f, 0.0f, 1.0f)
			[
				MakeText(RetuneHeading, 8, Accent, true)
			];
			ConvoyPanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				RetuneButtons
			];
		}
		if (Convoy.bCanCommissionSignalEscort)
		{
			const FString SuretyHeading = Localized(
				TEXT("strategic.mutual-aid-signal-surety-heading"),
				TEXT("SIGNAL SURETY  •  HELD BEFORE DEPARTURE"));
			const FString SuretyGuidance = Localized(
				TEXT("strategic.mutual-aid-signal-surety-guidance"),
				TEXT("Commission a Signal Escort before departure. It removes the forecast delay and shortens this and later queue projections; cargo, storage, route, identity, and FIFO order stay fixed."));
			const FString SuretyAction = LocalizedFormat(
				TEXT("strategic.mutual-aid-signal-surety-action-format"),
				TEXT("COMMISSION SIGNAL SURETY\nFUNDS {0} • ARRIVAL {1} h • RECOVER {2} h"),
				{
					LexToString(Convoy.SignalEscortCommissionCost),
					LexToString(CeilHours(
						Convoy.SignalEscortProjectedRelayQueue.EstimatedArrivalSeconds)),
					LexToString(CeilHours(
						Convoy.SignalEscortPreventedDelaySeconds))
				});
			RenderedDynamicLabels.Add(SuretyHeading);
			RenderedDynamicLabels.Add(SuretyAction);
			ConvoyPanel->AddSlot().AutoHeight().Padding(2.0f, 2.0f, 0.0f, 1.0f)
			[
				MakeText(SuretyHeading, 8, Accent, true)
			];
			ConvoyPanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(SButton)
				.IsFocusable(true)
				.IsEnabled(true)
				.ButtonColorAndOpacity(FLinearColor(0.04f, 0.12f, 0.18f, 1.0f))
				.ToolTipText(FText::FromString(SuretyGuidance))
				.OnClicked_UObject(
					this,
					&UUEGTStrategicHudWidget::HandleMutualAidSignalEscortCommissionClicked,
					Convoy.ConvoyId)
				[
					MakeText(SuretyAction, 8, PrimaryText, true)
				]
			];
		}
		if (Convoy.bCanDivertRelief)
		{
			const FString DiversionHeading = Localized(
				TEXT("strategic.mutual-aid-relief-diversion-heading"),
				TEXT("RELIEF DIVERSION  •  HELD BEFORE DEPARTURE"));
			const FString DiversionGuidance = Localized(
				TEXT("strategic.mutual-aid-relief-diversion-guidance"),
				TEXT("Redirect this never-departed held convoy. Its exact destination reservation moves atomically, route exposure and every affected arrival are recomputed, and any paid Signal Escort remains attached."));
			RenderedDynamicLabels.Add(DiversionHeading);
			TSharedRef<SVerticalBox> DiversionButtons = SNew(SVerticalBox);
			for (const FStrategicMutualAidDiversionOptionView& Option :
				Convoy.ReliefDiversionOptions)
			{
				const FString OptionLabel = LocalizedFormat(
					TEXT("strategic.mutual-aid-relief-diversion-option-format"),
					TEXT("DIVERT TO {0}\nRESERVE {1} • ARRIVAL {2} h • SHIFT {3} h • FOLLOW-ON {4}"),
					{
						Option.DestinationBaseName,
						LexToString(Option.DivertedStorage),
						LexToString(CeilHours(
							Option.ProjectedRelayQueue.EstimatedArrivalSeconds)),
						SignedCeilHours(Option.ArrivalShiftSeconds),
						FString::FromInt(Option.AffectedConvoyCount)
					});
				const FString Tooltip = Option.bEnabled
					|| Option.UnavailableReason.IsEmpty()
					? DiversionGuidance
					: LocalizedDiagnostic(
						Option.UnavailableReasonCode, Option.UnavailableReason);
				RenderedDynamicLabels.Add(OptionLabel);
				DiversionButtons->AddSlot().AutoHeight().Padding(
					0.0f, 0.0f, 0.0f, 2.0f)
				[
					SNew(SButton)
					.IsFocusable(true)
					.IsEnabled(Option.bEnabled)
					.ButtonColorAndOpacity(Option.bEnabled
						? FLinearColor(0.05f, 0.20f, 0.19f, 1.0f)
						: FLinearColor(0.04f, 0.08f, 0.10f, 1.0f))
					.ToolTipText(FText::FromString(Tooltip))
					.OnClicked_UObject(
						this,
						&UUEGTStrategicHudWidget::HandleMutualAidReliefDiversionClicked,
						Convoy.ConvoyId, Option.DestinationBaseId)
					[
						MakeText(OptionLabel, 8, PrimaryText, true)
					]
				];
			}
			ConvoyPanel->AddSlot().AutoHeight().Padding(2.0f, 2.0f, 0.0f, 1.0f)
			[
				MakeText(DiversionHeading, 8, Accent, true)
			];
			ConvoyPanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				DiversionButtons
			];
		}
		if (Convoy.bCanConfigureRelayWaypoint)
		{
			const FString WaypointHeading = Localized(
				TEXT("strategic.mutual-aid-relay-waypoint-heading"),
				TEXT("RELAY WAYPOINT  •  TWO-LEG ROUTE"));
			const FString WaypointGuidance = Localized(
				TEXT("strategic.mutual-aid-relay-waypoint-guidance"),
				TEXT("Stage this never-departed convoy through another established base. The source channel remains reserved end-to-end, final storage stays committed, and each leg resolves its own transparent exposure checkpoint."));
			RenderedDynamicLabels.Add(WaypointHeading);
			TSharedRef<SVerticalBox> WaypointButtons = SNew(SVerticalBox);
			for (const FStrategicMutualAidWaypointOptionView& Option :
				Convoy.RelayWaypointOptions)
			{
				const FString OptionLabel = Option.bDirectRoute
					? LocalizedFormat(
						TEXT("strategic.mutual-aid-relay-waypoint-direct-option-format"),
						TEXT("RESTORE DIRECT ROUTE\nFINAL {0} h • SHIFT {1} h • FOLLOW-ON {2}"),
						{
							LexToString(CeilHours(
								Option.ProjectedRelayQueue.EstimatedArrivalSeconds)),
							SignedCeilHours(Option.ArrivalShiftSeconds),
							FString::FromInt(Option.AffectedConvoyCount)
						})
					: LocalizedFormat(
						TEXT("strategic.mutual-aid-relay-waypoint-option-format"),
						TEXT("VIA {0} • THEN {1}\nWAYPOINT {2} h • FINAL {3} h • SHIFT {4} h • EXP {5}/{6} • FOLLOW-ON {7}"),
						{
							Option.WaypointBaseName,
							MutualAidRouteLabel(Option.OnwardRoutePolicy),
							LexToString(CeilHours(Option.WaypointArrivalSeconds)),
							LexToString(CeilHours(
								Option.ProjectedRelayQueue.EstimatedArrivalSeconds)),
							SignedCeilHours(Option.ArrivalShiftSeconds),
							FString::FromInt(Option.FirstLegRoutePressure),
							FString::FromInt(Option.OnwardRoutePressure),
							FString::FromInt(Option.AffectedConvoyCount)
						});
				const FString Tooltip = Option.bEnabled
					|| Option.UnavailableReason.IsEmpty()
						? WaypointGuidance
						: LocalizedDiagnostic(
							Option.UnavailableReasonCode, Option.UnavailableReason);
				const bool bSelected = Option.bDirectRoute
					? !Convoy.RelayWaypointBaseId.IsValid()
					: Convoy.RelayWaypointBaseId == Option.WaypointBaseId
						&& Convoy.OnwardRoutePolicy == Option.OnwardRoutePolicy;
				RenderedDynamicLabels.Add(OptionLabel);
				WaypointButtons->AddSlot().AutoHeight().Padding(
					0.0f, 0.0f, 0.0f, 2.0f)
				[
					SNew(SButton)
					.IsFocusable(true)
					.IsEnabled(Option.bEnabled)
					.ButtonColorAndOpacity(bSelected
						? FLinearColor(0.0f, 0.34f, 0.42f, 1.0f)
						: Option.bEnabled
							? FLinearColor(0.05f, 0.20f, 0.19f, 1.0f)
							: FLinearColor(0.04f, 0.08f, 0.10f, 1.0f))
					.ToolTipText(FText::FromString(Tooltip))
					.OnClicked_UObject(
						this,
						&UUEGTStrategicHudWidget::HandleMutualAidRelayWaypointClicked,
						Convoy.ConvoyId, Option.WaypointBaseId,
						Option.OnwardRoutePolicy)
					[
						MakeText(OptionLabel, 8,
							bSelected ? Accent : PrimaryText, true)
					]
				];
			}
			ConvoyPanel->AddSlot().AutoHeight().Padding(2.0f, 2.0f, 0.0f, 1.0f)
			[
				MakeText(WaypointHeading, 8, Accent, true)
			];
			ConvoyPanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				WaypointButtons
			];
		}
		if (Convoy.bCanConfigureBalancedHandoff)
		{
			const FString HandoffHeading = Localized(
				TEXT("strategic.mutual-aid-balanced-handoff-heading"),
				TEXT("BALANCED HANDOFF  •  WAYPOINT CARGO"));
			const FString HandoffGuidance = Localized(
				TEXT("strategic.mutual-aid-balanced-handoff-guidance"),
				TEXT("Before departure, reserve an even share for the waypoint while the remainder stays committed to the final destination. Relay timing, source capacity, escort, and random state do not change."));
			RenderedDynamicLabels.Add(HandoffHeading);
			TSharedRef<SHorizontalBox> HandoffButtons = SNew(SHorizontalBox);
			for (const FStrategicMutualAidBalancedHandoffOptionView& Option :
				Convoy.BalancedHandoffOptions)
			{
				const FString PlanLabel = Localized(
					Option.bEnabledChoice
						? TEXT("strategic.mutual-aid-balanced-handoff-balanced")
						: TEXT("strategic.mutual-aid-balanced-handoff-through"),
					Option.bEnabledChoice
						? TEXT("BALANCED HANDOFF")
						: TEXT("THROUGH CARGO"));
				const FString OptionLabel = LocalizedFormat(
					TEXT("strategic.mutual-aid-balanced-handoff-option-format"),
					TEXT("{0}\nWAYPOINT {1} • FINAL {2} • HANDOFF STORAGE {3}"),
					{
						PlanLabel,
						FString::FromInt(Option.WaypointQuantity),
						FString::FromInt(Option.FinalQuantity),
						LexToString(Option.HandoffStorage)
					});
				const FString Tooltip = Option.bEnabled
					|| Option.UnavailableReason.IsEmpty()
						? HandoffGuidance
						: LocalizedDiagnostic(
							Option.UnavailableReasonCode, Option.UnavailableReason);
				const bool bSelected = (Convoy.BalancedHandoffQuantity > 0)
					== Option.bEnabledChoice;
				RenderedDynamicLabels.Add(OptionLabel);
				HandoffButtons->AddSlot().FillWidth(1.0f).Padding(
					0.0f, 0.0f, 3.0f, 0.0f)
				[
					SNew(SButton)
					.IsFocusable(true)
					.IsEnabled(Option.bEnabled)
					.ButtonColorAndOpacity(bSelected
						? FLinearColor(0.0f, 0.34f, 0.42f, 1.0f)
						: Option.bEnabled
							? FLinearColor(0.05f, 0.20f, 0.19f, 1.0f)
							: FLinearColor(0.04f, 0.08f, 0.10f, 1.0f))
					.ToolTipText(FText::FromString(Tooltip))
					.OnClicked_UObject(
						this,
						&UUEGTStrategicHudWidget::HandleMutualAidBalancedHandoffClicked,
						Convoy.ConvoyId, Option.bEnabledChoice)
					[
						MakeText(OptionLabel, 7,
							bSelected ? Accent : PrimaryText, true)
					]
				];
			}
			ConvoyPanel->AddSlot().AutoHeight().Padding(2.0f, 2.0f, 0.0f, 1.0f)
			[
				MakeText(HandoffHeading, 8, Accent, true)
			];
			ConvoyPanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				HandoffButtons
			];
		}
		if (Convoy.bCanStandDownRelief)
		{
			const FString StandDownHeading = LocalizedFormat(
				TEXT("strategic.mutual-aid-relief-stand-down-heading-format"),
				TEXT("RELIEF STAND-DOWN  •  RELEASE {0} STORAGE"),
				{ LexToString(Convoy.ReliefStandDownReleasedStorage) });
			const FString StandDownGuidance = Localized(
				TEXT("strategic.mutual-aid-relief-stand-down-guidance"),
				TEXT("Withdraw this never-departed held convoy. Cargo returns to its source, destination storage is released, and later held commitments advance. Already-spent Signal Escort funds are not refunded."));
			const FString StandDownAction = LocalizedFormat(
				TEXT("strategic.mutual-aid-relief-stand-down-action-format"),
				TEXT("STAND DOWN RELIEF CONVOY\nRETURN {0} • RELEASE {1} • ADVANCE {2}"),
				{
					FString::FromInt(Convoy.Quantity),
					LexToString(Convoy.ReliefStandDownReleasedStorage),
					FString::FromInt(Convoy.ReliefStandDownAdvancedConvoyCount)
				});
			RenderedDynamicLabels.Add(StandDownHeading);
			RenderedDynamicLabels.Add(StandDownAction);
			ConvoyPanel->AddSlot().AutoHeight().Padding(2.0f, 2.0f, 0.0f, 1.0f)
			[
				MakeText(StandDownHeading, 8, Warning, true)
			];
			ConvoyPanel->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(SButton)
				.IsFocusable(true)
				.IsEnabled(true)
				.ButtonColorAndOpacity(FLinearColor(0.28f, 0.08f, 0.05f, 1.0f))
				.ToolTipText(FText::FromString(StandDownGuidance))
				.OnClicked_UObject(
					this,
					&UUEGTStrategicHudWidget::HandleMutualAidReliefStandDownClicked,
					Convoy.ConvoyId)
				[
					MakeText(StandDownAction, 8, PrimaryText, true)
				]
			];
		}
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
		[
			ConvoyPanel
		];
	}
}

void UUEGTStrategicHudWidget::BuildDashboard()
{
	using namespace UEGTStrategicHudPrivate;
	TitleText->SetText(FText::FromString(Localized(
		TEXT("strategic.command-title"), TEXT("UEGT  //  STRATEGIC COMMAND"))));
	const FStringFormatOrderedArguments DashboardSubtitleArguments = {
		CurrentSnapshot.CampaignTimeUtc.ToString(TEXT("%Y-%m-%d %H:%M:%S UTC")),
		FString::Printf(TEXT("%lld"), CurrentSnapshot.Funds),
		FString::Printf(TEXT("%lld"), CurrentSnapshot.CampaignScore),
		FString::FromInt(CurrentSnapshot.AdversaryEscalationLevel),
		OutcomeLabel(CurrentSnapshot.Outcome)
	};
	SubtitleText->SetText(FText::FromString(LocalizedFormat(
		TEXT("strategic.dashboard-subtitle-format"),
		TEXT("{0}  •  FUNDS {1}  •  SCORE {2}  •  ESCALATION {3}  •  {4}"),
		DashboardSubtitleArguments)));
	const bool bRecoveryDecisionRequired = CurrentSnapshot.Personnel.ContainsByPredicate(
		[](const FStrategicPersonnelView& Person)
		{
			return Person.StatusType == EPersonnelStatus::Recovering
				&& Person.RecoveryPlan.bDecisionRequired;
		});
	StatusText->SetText(FText::FromString(StatusMessage.IsEmpty()
		? (CurrentSnapshot.bDecisionRequired
			? (bRecoveryDecisionRequired
				? Localized(TEXT("strategic.recovery-decision-pause"),
					TEXT("DECISION PAUSE: choose a Return Path for each newly injured person."))
				: Localized(TEXT("strategic.decision-pause"),
					TEXT("DECISION PAUSE: a field operation is ready for tactical deployment.")))
			: Localized(TEXT("strategic.navigation-hint"),
				TEXT("LMB selects globe markers  •  WASD / sticks orbit  •  wheel / triggers zoom  •  time controls advance simulation")))
		: StatusMessage));
	StatusText->SetColorAndOpacity(CurrentSnapshot.bDecisionRequired || bStatusIsError ? Warning : SecondaryText);
	const FStrategicActionOptionView* PlacementOption = PendingFacilityRuleId.IsNone()
		? nullptr
		: CurrentSnapshot.ActionOptions.FindByPredicate(
			[this](const FStrategicActionOptionView& Option)
			{
				return Option.Type == EStrategicActionOptionType::Facility
					&& Option.RuleId == PendingFacilityRuleId;
			});
	const FString PlacementDisplayName = PlacementOption != nullptr
		? LocalizedContentName(PlacementOption->RuleId, PlacementOption->DisplayName)
		: FString();

	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		MakeText(Localized(TEXT("strategic.command-network"),
			TEXT("COMMAND NETWORK")), 16, Accent, true)
	];
	for (const FStrategicBaseView& Base : CurrentSnapshot.Bases)
	{
		const bool bPlacementBase = PlacementOption != nullptr && Base.BaseId == CurrentSnapshot.PrimaryBaseId;
		const FStrategicFacilityView* SelectedDismantle = Base.BaseId == PendingDismantleBaseId
			? Base.FacilityLayout.FindByPredicate(
				[this](const FStrategicFacilityView& Facility)
				{
					return !Facility.bConstructing
						&& Facility.FacilityInstanceId == PendingDismantleFacilityInstanceId;
				})
			: nullptr;
		const FString ScientistOverflow = Base.ScientistOverCapacity > 0
			? LocalizedFormat(
				TEXT("strategic.capacity-over-suffix-format"), TEXT(" • OVER {0}"),
				{ FString::FromInt(Base.ScientistOverCapacity) })
			: FString();
		const FString EngineerOverflow = Base.EngineerOverCapacity > 0
			? LocalizedFormat(
				TEXT("strategic.capacity-over-suffix-format"), TEXT(" • OVER {0}"),
				{ FString::FromInt(Base.EngineerOverCapacity) })
			: FString();
		TArray<FString> FacilitySummaries;
		FacilitySummaries.Reserve(Base.FacilityLayout.Num());
		for (const FStrategicFacilityView& Facility : Base.FacilityLayout)
		{
			FacilitySummaries.Add(LocalizedFacilitySummary(Facility));
		}
		if (FacilitySummaries.IsEmpty())
		{
			FacilitySummaries = Base.Facilities;
		}
		const FString BaseSummary = LocalizedFormat(
			TEXT("strategic.base-command-summary-format"),
			TEXT("{0}  //  {1}\nSCI WORK {2}/{3} • ROSTER {4}/{5} • FAC +{6}{7}\nENG WORK {8}/{9} • ROSTER {10}/{11} • FAC +{12}{13}\nBERTHS {14}/{15}   SENSOR {16} km / {17}%   DEFENSE {18} BAT • MAX {19} • EXP ~{20}\n{21}"),
			{
				FText::FromString(Base.Name).ToUpper().ToString(),
				FText::FromString(Base.RegionDisplayName).ToUpper().ToString(),
				FString::FromInt(Base.AssignedScientists),
				FString::FromInt(Base.ScientistCapacity),
				FString::FromInt(Base.ScientistPersonnel),
				FString::FromInt(Base.ScientistCapacity),
				FString::FromInt(Base.FacilityScientistCapacity),
				ScientistOverflow,
				FString::FromInt(Base.AssignedEngineers),
				FString::FromInt(Base.EngineerCapacity),
				FString::FromInt(Base.EngineerPersonnel),
				FString::FromInt(Base.EngineerCapacity),
				FString::FromInt(Base.FacilityEngineerCapacity),
				EngineerOverflow,
				FString::FromInt(Base.CraftOccupied),
				FString::FromInt(Base.CraftCapacity),
				FString::FromInt(Base.SensorRangeKilometers),
				FString::FromInt(Base.DetectionStrength),
				FString::FromInt(Base.DefenseBatteryCount),
				FString::FromInt(Base.MaximumDefenseDamage),
				FString::FromInt(Base.ExpectedDefenseDamage),
				FString::Join(FacilitySummaries, TEXT(" • "))
			});
		RenderedDynamicLabels.Add(BaseSummary);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 10.0f)
		[
			MakeText(BaseSummary, 13)
		];
		AppendBaseSpecialization(Base);
		AppendSignalWatchControls(Base);

		const int32 RenderWidth = FMath::Clamp(Base.GridWidth, 1, 12);
		const int32 RenderHeight = FMath::Clamp(Base.GridHeight, 1, 12);
		TSharedRef<SGridPanel> FacilityGrid = SNew(SGridPanel);
		for (int32 GridY = 0; GridY < RenderHeight; ++GridY)
		{
			for (int32 GridX = 0; GridX < RenderWidth; ++GridX)
			{
				const FStrategicFacilityView* Facility = Base.FacilityLayout.FindByPredicate(
					[GridX, GridY](const FStrategicFacilityView& Entry)
					{
						return GridX >= Entry.GridX && GridX < Entry.GridX + Entry.GridWidth
							&& GridY >= Entry.GridY && GridY < Entry.GridY + Entry.GridHeight;
					});
				const bool bAnchor = Facility != nullptr
					&& GridX == Facility->GridX && GridY == Facility->GridY;
				const bool bSelectedForDismantle = Facility != nullptr && !Facility->bConstructing
					&& Base.BaseId == PendingDismantleBaseId
					&& Facility->FacilityInstanceId == PendingDismantleFacilityInstanceId;
				const bool bValidPlacementAnchor = bPlacementBase
					&& PlacementOption->ValidFacilityPlacements.Contains(FIntPoint(GridX, GridY));
				const FString FacilityDisplayName = Facility != nullptr
					? LocalizedContentName(Facility->FacilityId, Facility->DisplayName)
					: FString();
				const FString CellLabel = bValidPlacementAnchor ? TEXT("+") : Facility == nullptr ? FString()
					: bAnchor ? FacilityAbbreviation(FacilityDisplayName) : TEXT("·");
				FString BaseTooltip;
				if (Facility == nullptr)
				{
					BaseTooltip = LocalizedFormat(
						TEXT("strategic.facility-empty-cell-format"), TEXT("Empty grid cell {0},{1}"),
						{ FString::FromInt(GridX), FString::FromInt(GridY) });
				}
				else if (Facility->bConstructing)
				{
					TArray<FString> PlannedEffects;
					if (Facility->MaximumScientistCapacity > 0)
					{
						PlannedEffects.Add(LocalizedFormat(
							TEXT("strategic.facility-planned-scientist-capacity-format"),
							TEXT("+{0} scientist capacity"),
							{ FString::FromInt(Facility->MaximumScientistCapacity) }));
					}
					if (Facility->MaximumEngineerCapacity > 0)
					{
						PlannedEffects.Add(LocalizedFormat(
							TEXT("strategic.facility-planned-engineer-capacity-format"),
							TEXT("+{0} engineer capacity"),
							{ FString::FromInt(Facility->MaximumEngineerCapacity) }));
					}
					const FString PlannedSuffix = PlannedEffects.IsEmpty()
						? FString()
						: LocalizedFormat(
							TEXT("strategic.facility-planned-suffix-format"), TEXT("\nPlanned: {0}"),
							{ FString::Join(PlannedEffects, TEXT(" • ")) });
					BaseTooltip = LocalizedFormat(
						TEXT("strategic.facility-constructing-tooltip-format"),
						TEXT("{0} • constructing • {1} h remaining • grid {2},{3} • {4}×{5}{6}"),
						{
							FacilityDisplayName,
							LexToString((Facility->RemainingBuildSeconds + 3599) / 3600),
							FString::FromInt(Facility->GridX),
							FString::FromInt(Facility->GridY),
							FString::FromInt(Facility->GridWidth),
							FString::FromInt(Facility->GridHeight),
							PlannedSuffix
						});
				}
				else
				{
					TArray<FString> EffectParts;
					if (Facility->MaximumStorageCapacity > 0)
					{
						EffectParts.Add(LocalizedFormat(
							TEXT("strategic.facility-contribution-storage-format"), TEXT("storage {0}/{1}"),
							{ FString::FromInt(Facility->StorageCapacity),
								FString::FromInt(Facility->MaximumStorageCapacity) }));
					}
					if (Facility->MaximumScientistCapacity > 0)
					{
						EffectParts.Add(LocalizedFormat(
							TEXT("strategic.facility-contribution-scientist-format"),
							TEXT("scientist capacity {0}/{1}"),
							{ FString::FromInt(Facility->ScientistCapacity),
								FString::FromInt(Facility->MaximumScientistCapacity) }));
					}
					if (Facility->MaximumEngineerCapacity > 0)
					{
						EffectParts.Add(LocalizedFormat(
							TEXT("strategic.facility-contribution-engineer-format"),
							TEXT("engineer capacity {0}/{1}"),
							{ FString::FromInt(Facility->EngineerCapacity),
								FString::FromInt(Facility->MaximumEngineerCapacity) }));
					}
					if (Facility->MaximumCraftCapacity > 0)
					{
						EffectParts.Add(LocalizedFormat(
							TEXT("strategic.facility-contribution-berths-format"), TEXT("berths {0}/{1}"),
							{ FString::FromInt(Facility->CraftCapacity),
								FString::FromInt(Facility->MaximumCraftCapacity) }));
					}
					if (Facility->MaximumSensorRangeKilometers > 0 || Facility->MaximumDetectionStrength > 0)
					{
						EffectParts.Add(LocalizedFormat(
							TEXT("strategic.facility-contribution-sensor-format"),
							TEXT("sensor {0}/{1} km at {2}/{3}%"),
							{
								FString::FromInt(Facility->SensorRangeKilometers),
								FString::FromInt(Facility->MaximumSensorRangeKilometers),
								FString::FromInt(Facility->DetectionStrength),
								FString::FromInt(Facility->MaximumDetectionStrength)
							}));
					}
					if (Facility->MaximumBaseDefenseDamage > 0)
					{
						const int32 CurrentExpectedDamage = static_cast<int32>(
							(static_cast<int64>(Facility->BaseDefenseAccuracy) * Facility->BaseDefenseDamage + 50) / 100);
						const int32 MaximumExpectedDamage = static_cast<int32>(
							(static_cast<int64>(Facility->MaximumBaseDefenseAccuracy)
								* Facility->MaximumBaseDefenseDamage + 50) / 100);
						EffectParts.Add(LocalizedFormat(
							TEXT("strategic.facility-contribution-battery-format"),
							TEXT("battery {0}/{1}% accuracy, {2}/{3} damage, ~{4}/~{5} expected"),
							{
								FString::FromInt(Facility->BaseDefenseAccuracy),
								FString::FromInt(Facility->MaximumBaseDefenseAccuracy),
								FString::FromInt(Facility->BaseDefenseDamage),
								FString::FromInt(Facility->MaximumBaseDefenseDamage),
								FString::FromInt(CurrentExpectedDamage),
								FString::FromInt(MaximumExpectedDamage)
							}));
						if (!Facility->BaseDefenseSupplyItemId.IsNone()
							&& Facility->BaseDefenseSupplyPerShot > 0)
						{
							EffectParts.Add(LocalizedFormat(
								TEXT("strategic.facility-contribution-battery-supply-format"),
								TEXT("supply {0} {1}/shot"),
								{
									FString::FromInt(Facility->BaseDefenseSupplyPerShot),
									LocalizedContentName(
										Facility->BaseDefenseSupplyItemId,
										Facility->BaseDefenseSupplyDisplayName)
								}));
						}
					}
					const FString EffectStatus = EffectParts.IsEmpty()
						? Localized(
							TEXT("strategic.facility-contribution-none"),
							TEXT("no storage, personnel, berth, sensor, or battery contribution"))
						: FString::Join(EffectParts, TEXT(" • "));
					const FString RepairStatus = Facility->bRepairing
						? LocalizedFormat(
							TEXT("strategic.facility-repair-active-tooltip-format"),
							TEXT("repair active • {0} h remaining • {1} refundable"),
							{ LexToString((Facility->RemainingRepairSeconds + 3599) / 3600),
								LexToString(Facility->RepairCancellationRefund) })
						: Facility->Damage > 0
							? (Facility->bCanRepair
								? LocalizedFormat(
									TEXT("strategic.facility-repair-available-tooltip-format"),
									TEXT("repair available • {0} funds • {1} h"),
									{ LexToString(Facility->RepairCost),
										LexToString((Facility->RepairDurationSeconds + 3599) / 3600) })
								: LocalizedDiagnostic(
									Facility->RepairUnavailableReasonCode,
									Facility->RepairUnavailableReason))
							: Localized(TEXT("strategic.facility-full-integrity"), TEXT("full integrity"));
					const FString DismantleStatus = Facility->bCanDismantle
						? LocalizedFormat(
							TEXT("strategic.facility-dismantle-tooltip-format"),
							TEXT("Click to review dismantling • salvage {0}."),
							{ LexToString(Facility->DismantleRefund) })
						: LocalizedDiagnostic(
							Facility->DismantleUnavailableReasonCode,
							Facility->DismantleUnavailableReason);
					BaseTooltip = LocalizedFormat(
						TEXT("strategic.facility-tooltip-format"),
						TEXT("{0} • {1} • integrity {2}/{3} • output {4}% • grid {5},{6} • {7}×{8}\n{9}\n{10}\n{11}"),
						{
							FacilityDisplayName,
							Facility->bOperational
								? Localized(TEXT("strategic.facility-operational"), TEXT("operational"))
								: Localized(TEXT("strategic.facility-offline"), TEXT("OFFLINE")),
							FString::FromInt(Facility->CurrentIntegrity),
							FString::FromInt(Facility->MaxIntegrity),
							FString::FromInt(Facility->EffectivenessPercent),
							FString::FromInt(Facility->GridX),
							FString::FromInt(Facility->GridY),
							FString::FromInt(Facility->GridWidth),
							FString::FromInt(Facility->GridHeight),
							EffectStatus,
							RepairStatus,
							DismantleStatus
						});
				}
				const FLinearColor CellColor = Facility == nullptr
					? FLinearColor(0.018f, 0.035f, 0.065f, 1.0f)
					: bSelectedForDismantle
						? FLinearColor(0.42f, 0.065f, 0.035f, 1.0f)
					: Facility->bConstructing
						? FLinearColor(0.48f, 0.23f, 0.035f, 1.0f)
					: Facility->bRepairing
						? FLinearColor(0.28f, 0.12f, 0.42f, 1.0f)
					: Facility->bOperational && Facility->Damage == 0
						? FLinearColor(0.0f, 0.24f, 0.36f, 1.0f)
					: Facility->bOperational
						? FLinearColor(0.48f, 0.23f, 0.035f, 1.0f)
						: FLinearColor(0.44f, 0.045f, 0.035f, 1.0f);
				TSharedRef<SWidget> CellContent = SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")))
					.BorderBackgroundColor(CellColor)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						MakeText(CellLabel, 9, Facility != nullptr && !Facility->bOperational ? Warning : PrimaryText, bAnchor)
					];
				FString Tooltip = BaseTooltip;
				if (bPlacementBase)
				{
					Tooltip = bValidPlacementAnchor
						? LocalizedFormat(
							TEXT("strategic.facility-placement-valid-tooltip-format"),
							TEXT("Place {0} ({1}×{2}) with its anchor at grid {3},{4}."),
							{
								PlacementDisplayName,
								FString::FromInt(PlacementOption->FacilityGridWidth),
								FString::FromInt(PlacementOption->FacilityGridHeight),
								FString::FromInt(GridX), FString::FromInt(GridY)
							})
						: LocalizedFormat(
							TEXT("strategic.facility-placement-invalid-tooltip-format"),
							TEXT("{0} cannot anchor at grid {1},{2}; its full footprint must fit, remain clear, and touch the base complex."),
							{ PlacementDisplayName, FString::FromInt(GridX), FString::FromInt(GridY) });
					CellContent = SNew(SButton)
						.IsEnabled(bValidPlacementAnchor)
						.IsFocusable(true)
						.ContentPadding(0.0f)
						.ButtonColorAndOpacity(bValidPlacementAnchor
							? FLinearColor(0.0f, 0.38f, 0.30f, 1.0f)
							: CellColor)
						.ToolTipText(FText::FromString(Tooltip))
						.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleFacilityPlacementClicked,
							PlacementOption->RuleId, Base.BaseId, GridX, GridY)
						[
							MakeText(CellLabel, 9, bValidPlacementAnchor ? PrimaryText
								: Facility != nullptr && !Facility->bOperational ? Warning : SecondaryText,
								bValidPlacementAnchor || bAnchor)
						];
				}
				else if (Facility != nullptr && !Facility->bConstructing)
				{
					Tooltip = BaseTooltip;
					CellContent = SNew(SButton)
						.IsEnabled(Facility->bCanDismantle)
						.IsFocusable(true)
						.ContentPadding(0.0f)
						.ButtonColorAndOpacity(CellColor)
						.ToolTipText(FText::FromString(Tooltip))
						.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleFacilityDismantleReviewClicked,
							Base.BaseId, Facility->FacilityInstanceId)
						[
							MakeText(CellLabel, 9, bSelectedForDismantle ? Warning : PrimaryText, bAnchor)
						];
				}
				FacilityGrid->AddSlot(GridX, GridY)
				.Padding(1.0f)
				[
					SNew(SBox)
					.WidthOverride(36.0f)
					.HeightOverride(23.0f)
					.ToolTipText(FText::FromString(Tooltip))
					[
						CellContent
					]
				];
			}
		}
		const FString BaseGridLabel = LocalizedFormat(
			TEXT("strategic.base-grid-format"), TEXT("BASE GRID  {0} × {1}"),
			{ FString::FromInt(Base.GridWidth), FString::FromInt(Base.GridHeight) });
		RenderedDynamicLabels.Add(BaseGridLabel);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
		[
			MakeText(BaseGridLabel, 11, SecondaryText, true)
		];
		LeftBox->AddSlot().AutoHeight()
		[
			FacilityGrid
		];
		if (bPlacementBase)
		{
			const FString PlacementLegend = Localized(
				TEXT("strategic.facility-placement-legend"),
				TEXT("GREEN + valid anchor  •  disabled cells cannot fit or connect the selected footprint"));
			const FString CancelPlacementLabel = Localized(
				TEXT("strategic.facility-placement-cancel-action"), TEXT("CANCEL FACILITY PLACEMENT"));
			RenderedDynamicLabels.Add(PlacementLegend);
			RenderedDynamicLabels.Add(CancelPlacementLabel);
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 3.0f)
			[
				MakeText(PlacementLegend, 9, SecondaryText)
			];
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SButton)
				.IsFocusable(true)
				.ButtonColorAndOpacity(FLinearColor(0.28f, 0.10f, 0.05f, 1.0f))
				.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCancelFacilityPlacementClicked)
				[
					MakeText(CancelPlacementLabel, 9, Warning, true)
				]
			];
		}
		else if (SelectedDismantle != nullptr)
		{
			const FString SelectedDismantleName = LocalizedContentName(
				SelectedDismantle->FacilityId, SelectedDismantle->DisplayName);
			const FString DismantleSelectionLabel = LocalizedFormat(
				TEXT("strategic.facility-dismantle-selection-format"),
				TEXT("DISMANTLE {0}  •  SALVAGE {1}"),
				{ SelectedDismantleName, LexToString(SelectedDismantle->DismantleRefund) });
			const FString ConfirmDismantleLabel = Localized(
				TEXT("strategic.facility-dismantle-confirm-action"), TEXT("CONFIRM DISMANTLE"));
			const FString KeepFacilityLabel = Localized(
				TEXT("strategic.facility-dismantle-keep-action"), TEXT("KEEP FACILITY"));
			RenderedDynamicLabels.Add(DismantleSelectionLabel);
			RenderedDynamicLabels.Add(ConfirmDismantleLabel);
			RenderedDynamicLabels.Add(KeepFacilityLabel);
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 3.0f)
			[
				MakeText(DismantleSelectionLabel, 9, Warning, true)
			];
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
						.IsFocusable(true)
						.ButtonColorAndOpacity(FLinearColor(0.42f, 0.065f, 0.035f, 1.0f))
					.ToolTipText(FText::FromString(Localized(
						TEXT("strategic.facility-dismantle-confirm-tooltip"),
						TEXT("Permanently remove this facility and recover the displayed salvage."))))
						.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleConfirmFacilityDismantleClicked)
					[
						MakeText(ConfirmDismantleLabel, 9, Warning, true)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
						.IsFocusable(true)
						.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCancelFacilityDismantleClicked)
					[
						MakeText(KeepFacilityLabel, 9, PrimaryText, true)
					]
				]
			];
		}
		else
		{
			const FString FacilityGridLegend = Localized(
				TEXT("strategic.facility-grid-legend"),
				TEXT("CYAN ready  •  AMBER damaged/building  •  RED offline  •  PURPLE repair  •  click installed facilities for dismantling"));
			RenderedDynamicLabels.Add(FacilityGridLegend);
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 6.0f)
			[
				MakeText(FacilityGridLegend, 9, SecondaryText)
			];
		}
		for (const FStrategicFacilityView& Facility : Base.FacilityLayout)
		{
			if (Facility.bConstructing || Facility.Damage <= 0)
			{
				continue;
			}
			const FString IntegrityState = Facility.bOperational
				? Localized(TEXT("strategic.facility-degraded"), TEXT("DEGRADED"))
				: Localized(TEXT("strategic.facility-offline"), TEXT("OFFLINE"));
			const FString FacilityDisplayName = LocalizedContentName(Facility.FacilityId, Facility.DisplayName);
			const FString IntegrityLabel = LocalizedFormat(
				TEXT("strategic.facility-integrity-format"),
				TEXT("{0}  //  {1}  //  INTEGRITY {2}/{3}  //  OUTPUT {4}%"),
				{
					FacilityDisplayName,
					IntegrityState,
					FString::FromInt(Facility.CurrentIntegrity),
					FString::FromInt(Facility.MaxIntegrity),
					FString::FromInt(Facility.EffectivenessPercent)
				});
			RenderedDynamicLabels.Add(IntegrityLabel);
			const FLinearColor IntegrityColor = Facility.bOperational ? Warning : FLinearColor(1.0f, 0.22f, 0.16f, 1.0f);
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 2.0f)
			[
				MakeText(IntegrityLabel, 10, IntegrityColor, true)
			];
			if (Facility.bRepairing)
			{
				const FString CancelRepairTooltip = LocalizedFormat(
					TEXT("strategic.facility-repair-cancel-tooltip-format"),
					TEXT("Cancel this all-or-nothing repair and return the full {0}-fund reservation. Existing damage remains."),
					{ LexToString(Facility.RepairCancellationRefund) });
				const FString RepairingLabel = LocalizedFormat(
					TEXT("strategic.facility-repairing-cancel-format"),
					TEXT("REPAIRING  {0} h  •  CANCEL +{1}"),
					{ LexToString((Facility.RemainingRepairSeconds + 3599) / 3600),
						LexToString(Facility.RepairCancellationRefund) });
				RenderedDynamicLabels.Add(RepairingLabel);
				LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SButton)
					.IsFocusable(true)
					.ButtonColorAndOpacity(FLinearColor(0.28f, 0.12f, 0.42f, 1.0f))
					.ToolTipText(FText::FromString(CancelRepairTooltip))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCancelFacilityRepairClicked,
						Base.BaseId, Facility.FacilityInstanceId)
					[
						MakeText(RepairingLabel, 9, PrimaryText, true)
					]
				];
			}
			else
			{
				const FString RepairLabel = LocalizedFormat(
					TEXT("strategic.restore-facility-format"),
					TEXT("RESTORE {0} INTEGRITY  •  {1} FUNDS  •  {2} h"),
					{
						FString::FromInt(Facility.Damage),
						LexToString(Facility.RepairCost),
						LexToString((Facility.RepairDurationSeconds + 3599) / 3600)
					});
				RenderedDynamicLabels.Add(RepairLabel);
				const FString RepairTooltip = Facility.bCanRepair
					? LocalizedFormat(
						TEXT("strategic.facility-repair-start-tooltip-format"),
						TEXT("Reserve {0} funds now; restore all current damage after {1} strategic hours."),
						{ LexToString(Facility.RepairCost),
							LexToString((Facility.RepairDurationSeconds + 3599) / 3600) })
					: LocalizedDiagnostic(
						Facility.RepairUnavailableReasonCode,
						Facility.RepairUnavailableReason);
				LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SButton)
					.IsEnabled(Facility.bCanRepair)
					.IsFocusable(true)
					.ButtonColorAndOpacity(Facility.bCanRepair
						? FLinearColor(0.0f, 0.34f, 0.28f, 1.0f)
						: FLinearColor(0.12f, 0.12f, 0.14f, 1.0f))
					.ToolTipText(FText::FromString(RepairTooltip))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleFacilityRepairClicked,
						Base.BaseId, Facility.FacilityInstanceId)
					[
						MakeText(RepairLabel, 9, Facility.bCanRepair ? PrimaryText : SecondaryText, true)
					]
				];
			}
		}
		if (Base.bStorageEnforced || !Base.Inventory.IsEmpty())
		{
			const FLinearColor StorageColor = Base.StorageOverflow > 0 ? Warning : Accent;
			const FString StorageLabel = Base.bStorageEnforced
				? Base.StorageOverflow > 0
					? LocalizedFormat(
						TEXT("strategic.storage-over-capacity-format"),
						TEXT("STORAGE OVER CAPACITY  {0} / {1}  •  {2} OVER"),
						{ LexToString(Base.StorageCommitted), LexToString(Base.StorageCapacity),
							LexToString(Base.StorageOverflow) })
					: LocalizedFormat(
						TEXT("strategic.storage-available-format"),
						TEXT("STORAGE  {0} / {1}  •  {2} FREE"),
						{ LexToString(Base.StorageCommitted), LexToString(Base.StorageCapacity),
							LexToString(Base.StorageAvailable) })
				: LocalizedFormat(
					TEXT("strategic.storage-legacy-format"),
					TEXT("STORAGE  {0} TYPES  •  UNLIMITED LEGACY RULESET"),
					{ FString::FromInt(Base.Inventory.Num()) });
			RenderedDynamicLabels.Add(StorageLabel);
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 3.0f)
			[
				MakeText(StorageLabel, 11, StorageColor, true)
			];
			if (Base.bStorageEnforced && Base.StorageReserved > 0)
			{
				const FString ReservedStorageLabel = LocalizedFormat(
					TEXT("strategic.storage-reserved-format"),
					TEXT("{0} STORED  •  {1} PRODUCTION  •  {2} INBOUND AID"),
					{
						LexToString(Base.StorageUsed),
						LexToString(Base.StorageProductionReserved),
						LexToString(Base.StorageMutualAidReserved)
					});
				RenderedDynamicLabels.Add(ReservedStorageLabel);
				LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
				[
					MakeText(ReservedStorageLabel, 9,
						Base.StorageOverflow > 0 ? Warning : SecondaryText, true)
				];
			}
			if (Base.Inventory.IsEmpty())
			{
				const FString EmptyInventoryLabel = Localized(
					TEXT("strategic.inventory-empty"), TEXT("No unassigned inventory"));
				RenderedDynamicLabels.Add(EmptyInventoryLabel);
				LeftBox->AddSlot().AutoHeight()[MakeText(EmptyInventoryLabel, 10, SecondaryText)];
			}
			for (const FStrategicInventoryView& Item : Base.Inventory)
			{
				const FString ItemDisplayName = LocalizedContentName(Item.ItemId, Item.DisplayName);
				const FString InventoryLabel = Item.UnitSellValue > 0
					? LocalizedFormat(
						TEXT("strategic.inventory-sellable-format"),
						TEXT("{0}  ×{1}  •  {2} STORAGE  •  {3} EACH"),
						{ ItemDisplayName, FString::FromInt(Item.Quantity), LexToString(Item.TotalStorage),
							FString::FromInt(Item.UnitSellValue) })
					: LocalizedFormat(
						TEXT("strategic.inventory-retained-format"),
						TEXT("{0}  ×{1}  •  {2} STORAGE  •  RETAIN"),
						{ ItemDisplayName, FString::FromInt(Item.Quantity), LexToString(Item.TotalStorage) });
				RenderedDynamicLabels.Add(InventoryLabel);
				LeftBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
				[
					MakeText(InventoryLabel, 10)
				];
				if (Item.UnitSellValue > 0 && Item.Quantity > 0)
				{
					const FString SellOneLabel = Localized(
						TEXT("strategic.inventory-sell-one"), TEXT("SELL 1"));
					const FString SellAllLabel = Localized(
						TEXT("strategic.inventory-sell-all"), TEXT("SELL ALL"));
					RenderedDynamicLabels.Add(SellOneLabel);
					RenderedDynamicLabels.Add(SellAllLabel);
					LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SButton)
							.IsFocusable(true)
							.ToolTipText(FText::FromString(LocalizedFormat(
								TEXT("strategic.inventory-sell-one-tooltip-format"),
								TEXT("Sell one for {0}."), { FString::FromInt(Item.UnitSellValue) })))
							.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleSellInventoryClicked,
								Base.BaseId, Item.ItemId, 1)
							[
								MakeText(SellOneLabel, 9)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.IsFocusable(true)
							.ToolTipText(FText::FromString(LocalizedFormat(
								TEXT("strategic.inventory-sell-all-tooltip-format"),
								TEXT("Sell all {0} for {1}."),
								{ FString::FromInt(Item.Quantity),
									LexToString(static_cast<int64>(Item.Quantity) * Item.UnitSellValue) })))
							.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleSellInventoryClicked,
								Base.BaseId, Item.ItemId, Item.Quantity)
							[
								MakeText(SellAllLabel, 9)
							]
						]
					];
				}
				AppendMutualAidDispatchControls(Base, Item);
			}
		}
	}

	AppendMutualAidConvoySummary();

	const FString PersonnelCountLabel = LocalizedFormat(
		TEXT("strategic.personnel-count-format"), TEXT("PERSONNEL  {0}"),
		{ FString::FromInt(CurrentSnapshot.Personnel.Num()) });
	RenderedDynamicLabels.Add(PersonnelCountLabel);
	PersonnelPanelAnchor = MakeText(PersonnelCountLabel, 16, Accent, true);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 7.0f)
	[
		PersonnelPanelAnchor.ToSharedRef()
	];
	if (CurrentSnapshot.Personnel.IsEmpty())
	{
		const FString NoPersonnelLabel = Localized(
			TEXT("strategic.personnel-empty"), TEXT("No personnel on station"));
		RenderedDynamicLabels.Add(NoPersonnelLabel);
		LeftBox->AddSlot().AutoHeight()[MakeText(NoPersonnelLabel, 13, SecondaryText)];
	}
	for (const FStrategicPersonnelView& Person : CurrentSnapshot.Personnel)
	{
		const FStrategicBaseView* PersonnelBase = CurrentSnapshot.Bases.FindByPredicate(
			[&Person](const FStrategicBaseView& View) { return View.BaseId == Person.BaseId; });
		const FString PersonnelBaseName = PersonnelBase != nullptr
			? PersonnelBase->Name
			: Localized(TEXT("strategic.personnel-unknown-base"), TEXT("Unknown base"));
		FString DutyDetail;
		if (Person.StatusType == EPersonnelStatus::Training)
		{
			DutyDetail = LocalizedFormat(
				TEXT("strategic.personnel-training-duty-format"), TEXT(" • {0} {1} h"),
				{ TrainingFocusLabel(Person.TrainingFocus),
					LexToString((Person.RemainingTrainingSeconds + 3599) / 3600) });
		}
		else if (Person.StatusType == EPersonnelStatus::Recovering)
		{
			DutyDetail = LocalizedFormat(
				TEXT("strategic.personnel-recovery-duty-format"), TEXT(" • {0} h recovery"),
				{ LexToString((Person.RemainingRecoverySeconds + 3599) / 3600) });
		}
		else if (Person.StatusType == EPersonnelStatus::Stewarding)
		{
			DutyDetail = LocalizedFormat(
				TEXT("strategic.personnel-stewardship-duty-format"), TEXT(" • {0} {1} d"),
				{
					StewardshipFocusLabel(Person.Stewardship.ActiveFocus),
					LexToString((Person.Stewardship.RemainingSeconds + 86399) / 86400)
				});
		}
		if (Person.bAssignedToCraft)
		{
			DutyDetail += Localized(
				TEXT("strategic.personnel-craft-assigned"), TEXT(" • CRAFT ASSIGNED"));
		}
		const FString RoleDisplayName = LocalizedContentName(Person.RoleId, Person.RoleDisplayName);
		const FString PersonnelLabel = LocalizedFormat(
			TEXT("strategic.personnel-card-format"),
			TEXT("{0}  •  {1}\n{2}  •  BASE {3}{4}   RANK {5}   HP {6}/{7}\nACC {8}   RES {9}   MOB {10}   STR {11}\nMISSIONS {12}   KILLS {13}   XP {14}"),
			{
				Person.DisplayName,
				RoleDisplayName,
				PersonnelStatusLabel(Person.StatusType),
				PersonnelBaseName.ToUpper(),
				DutyDetail,
				FString::FromInt(Person.Rank),
				FString::FromInt(Person.CurrentHealth),
				FString::FromInt(Person.MaxHealth),
				FString::FromInt(Person.Accuracy),
				FString::FromInt(Person.Resolve),
				FString::FromInt(Person.Mobility),
				FString::FromInt(Person.Strength),
				FString::FromInt(Person.Missions),
				FString::FromInt(Person.Kills),
				FString::FromInt(Person.Experience)
			});
		RenderedDynamicLabels.Add(PersonnelLabel);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SBox)
			.MaxDesiredWidth(320.0f)
			[
				MakeText(PersonnelLabel, 11)
			]
		];
		const FString ServiceHistoryLabel = PersonnelServiceHistoryLabel(
			Person.ServiceHistory, Person.Missions);
		RenderedDynamicLabels.Add(ServiceHistoryLabel);
		LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 2.0f)
		[
			MakeText(ServiceHistoryLabel, 9, Accent, true)
		];
		if (Person.StewardshipToursCompleted > 0)
		{
			const FString StewardshipHistory = LocalizedFormat(
				TEXT("strategic.stewardship-history-format"), TEXT("STEWARDSHIP TOURS {0}"),
				{ FString::FromInt(Person.StewardshipToursCompleted) });
			RenderedDynamicLabels.Add(StewardshipHistory);
			LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 2.0f)
			[
				MakeText(StewardshipHistory, 9, Success, true)
			];
		}
		if (!Person.Commendations.IsEmpty())
		{
			const FString CommendationHeader = LocalizedFormat(
				TEXT("strategic.personnel-commendations-format"), TEXT("CITATIONS  {0}"),
				{ FString::FromInt(Person.Commendations.Num()) });
			RenderedDynamicLabels.Add(CommendationHeader);
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 1.0f)
			[
				MakeText(CommendationHeader, 10, Success, true)
			];
			for (const FStrategicPersonnelCommendationView& Commendation : Person.Commendations)
			{
				const FString CommendationName = LocalizedContentName(
					Commendation.CommendationId, Commendation.DisplayName);
				const FString CommendationSummary = LocalizedContentField(
					Commendation.CommendationId, TEXT("summary"), Commendation.Summary);
				const FString CommendationLabel = LocalizedFormat(
					TEXT("strategic.personnel-commendation-format"), TEXT("◆ {0} — {1}"),
					{ CommendationName, CommendationSummary });
				RenderedDynamicLabels.Add(CommendationLabel);
				LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 1.0f)
				[
					MakeText(CommendationLabel, 9, SecondaryText)
				];
			}
		}
		if (!Person.DoctrineOptions.IsEmpty())
		{
			const FString DoctrineHeader = Person.PendingDoctrineChoices > 0
				? LocalizedFormat(
					TEXT("strategic.personnel-doctrine-ready-format"),
					TEXT("FIELD DOCTRINE  •  OPEN CHOICES: {0}"),
					{ FString::FromInt(Person.PendingDoctrineChoices) })
				: Localized(TEXT("strategic.personnel-doctrines"), TEXT("FIELD DOCTRINES"));
			RenderedDynamicLabels.Add(DoctrineHeader);
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 2.0f)
			[
				MakeText(DoctrineHeader, 10,
					Person.PendingDoctrineChoices > 0 ? Accent : SecondaryText, true)
			];
			TSharedRef<SWrapBox> DoctrineBox = SNew(SWrapBox);
			for (const FStrategicPersonnelDoctrineView& Doctrine : Person.DoctrineOptions)
			{
				const FString DoctrineName = LocalizedContentName(Doctrine.DoctrineId, Doctrine.DisplayName);
				const FString DoctrineSummary = LocalizedContentField(
					Doctrine.DoctrineId, TEXT("summary"), Doctrine.Summary);
				const FString BonusLabel = LocalizedFormat(
					TEXT("strategic.personnel-doctrine-bonus-format"),
					TEXT("HP +{0}  ACC +{1}  RES +{2}  MOB +{3}  STR +{4}"),
					{
						FString::FromInt(Doctrine.MaxHealthBonus),
						FString::FromInt(Doctrine.AccuracyBonus),
						FString::FromInt(Doctrine.ResolveBonus),
						FString::FromInt(Doctrine.MobilityBonus),
						FString::FromInt(Doctrine.StrengthBonus)
					});
				const FString DoctrineLabel = LocalizedFormat(
					TEXT("strategic.personnel-doctrine-option-format"),
					TEXT("{0}  •  LV {1}/{2}\n{3}"),
					{
						DoctrineName,
						FString::FromInt(Doctrine.CurrentSelections),
						FString::FromInt(Doctrine.MaximumSelections),
						BonusLabel
					});
				const FString UnavailableReason = Doctrine.bEnabled
					? FString()
					: LocalizedDiagnostic(Doctrine.UnavailableReasonCode, Doctrine.UnavailableReason);
				const FString DoctrineTooltip = UnavailableReason.IsEmpty()
					? DoctrineSummary
					: LocalizedFormat(
						TEXT("strategic.personnel-doctrine-tooltip-format"), TEXT("{0}\n{1}"),
						{ DoctrineSummary, UnavailableReason });
				RenderedDynamicLabels.Add(DoctrineLabel);
				DoctrineBox->AddSlot().Padding(FMargin(0.0f, 0.0f, 3.0f, 3.0f))
				[
					SNew(SButton)
					.IsEnabled(Doctrine.bEnabled)
					.IsFocusable(true)
					.ToolTipText(FText::FromString(DoctrineTooltip))
					.ButtonColorAndOpacity(Doctrine.bEnabled
						? FLinearColor(0.0f, 0.25f, 0.38f, 1.0f)
						: FLinearColor(0.035f, 0.075f, 0.12f, 1.0f))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandlePersonnelDoctrineClicked,
						Person.PersonnelId, Doctrine.DoctrineId)
					[
						MakeText(DoctrineLabel, 9, Doctrine.bEnabled ? PrimaryText : SecondaryText)
					]
				];
			}
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
			[
				DoctrineBox
			];
		}
		if (Person.StatusType == EPersonnelStatus::Recovering && Person.RecoveryPlan.bRecovering)
		{
			BuildPersonnelRecoveryPlanPanel(Person);
		}
		if (Person.Stewardship.bSelectedPersonnelIsSteward
			|| (Person.RoleCategory == EPersonnelRoleCategory::FieldAgent && Person.Stewardship.bEligible))
		{
			BuildPersonnelStewardshipPanel(Person);
		}
		if (Person.StatusType == EPersonnelStatus::Available)
		{
			if (Person.RoleCategory == EPersonnelRoleCategory::FieldAgent && PersonnelBase != nullptr)
			{
				const FString FieldLoadoutLabel = LocalizedFormat(
					TEXT("strategic.field-loadout-format"), TEXT("FIELD LOADOUT  {0}/16"),
					{ FString::FromInt(Person.EquippedItemIds.Num()) });
				RenderedDynamicLabels.Add(FieldLoadoutLabel);
				LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 2.0f)
				[
					MakeText(FieldLoadoutLabel, 10, SecondaryText, true)
				];
				TArray<FName> LoadoutItemIds = Person.EquippedItemIds;
				for (const FStrategicInventoryView& StoredItem : PersonnelBase->Inventory)
				{
					if (StoredItem.bPersonnelEquippable)
					{
						LoadoutItemIds.AddUnique(StoredItem.ItemId);
					}
				}
				LoadoutItemIds.Sort(FNameLexicalLess());
				if (LoadoutItemIds.IsEmpty())
				{
					const FString EmptyLoadoutLabel = Localized(
						TEXT("strategic.field-loadout-empty"),
						TEXT("No field equipment is stored or assigned"));
					RenderedDynamicLabels.Add(EmptyLoadoutLabel);
					LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
					[
						MakeText(EmptyLoadoutLabel, 10, SecondaryText)
					];
				}
				for (const FName ItemId : LoadoutItemIds)
				{
					const FStrategicInventoryView* StoredItem = PersonnelBase->Inventory.FindByPredicate(
						[ItemId](const FStrategicInventoryView& Item) { return Item.ItemId == ItemId; });
					int32 EquippedCount = 0;
					for (const FName EquippedId : Person.EquippedItemIds)
					{
						EquippedCount += EquippedId == ItemId ? 1 : 0;
					}
					const int32 StoredCount = StoredItem != nullptr && StoredItem->bPersonnelEquippable ? StoredItem->Quantity : 0;
					const int32 EquippedIndex = Person.EquippedItemIds.IndexOfByKey(ItemId);
					const FString ItemEnglishName = StoredItem != nullptr
						? StoredItem->DisplayName
						: Person.EquippedItemNames.IsValidIndex(EquippedIndex)
							? Person.EquippedItemNames[EquippedIndex]
							: ItemId.ToString();
					const FString ItemName = LocalizedContentName(ItemId, ItemEnglishName);
					const FString LoadoutLabel = LocalizedFormat(
						TEXT("strategic.loadout-item-format"),
						TEXT("{0}  •  EQUIPPED {1}  •  STORED {2}"),
						{ ItemName, FString::FromInt(EquippedCount), FString::FromInt(StoredCount) });
					RenderedDynamicLabels.Add(LoadoutLabel);
					LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							MakeText(LoadoutLabel, 9, PrimaryText)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(3.0f, 0.0f)
						[
							SNew(SButton)
							.IsEnabled(EquippedCount > 0)
							.IsFocusable(true)
							.ToolTipText(FText::FromString(Localized(
								TEXT("strategic.loadout-return-tooltip"),
								TEXT("Return one equipped unit to base stores."))))
							.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandlePersonnelEquipmentClicked,
								Person.PersonnelId, ItemId, -1)
							[
								MakeText(TEXT("−"), 10, EquippedCount > 0 ? PrimaryText : SecondaryText, true)
							]
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							SNew(SButton)
							.IsEnabled(StoredCount > 0 && Person.EquippedItemIds.Num() < 16)
							.IsFocusable(true)
							.ToolTipText(FText::FromString(Person.EquippedItemIds.Num() >= 16
								? Localized(TEXT("strategic.loadout-limit-tooltip"),
									TEXT("Personnel loadout is at its 16-unit limit."))
								: Localized(TEXT("strategic.loadout-equip-tooltip"),
									TEXT("Equip one stored unit."))))
							.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandlePersonnelEquipmentClicked,
								Person.PersonnelId, ItemId, 1)
							[
								MakeText(TEXT("+"), 10,
									StoredCount > 0 && Person.EquippedItemIds.Num() < 16 ? PrimaryText : SecondaryText, true)
							]
						]
					];
				}
			}

			struct FTrainingButton
			{
				EPersonnelTrainingFocus Focus;
				const TCHAR* LocalizationKey;
				const TCHAR* EnglishLabel;
				int32 Attribute;
			};
			const FTrainingButton TrainingButtons[] = {
				{ EPersonnelTrainingFocus::Accuracy, TEXT("strategic.train-accuracy"), TEXT("TRAIN ACC"), Person.Accuracy },
				{ EPersonnelTrainingFocus::Resolve, TEXT("strategic.train-resolve"), TEXT("TRAIN RES"), Person.Resolve },
				{ EPersonnelTrainingFocus::Mobility, TEXT("strategic.train-mobility"), TEXT("TRAIN MOB"), Person.Mobility },
				{ EPersonnelTrainingFocus::Strength, TEXT("strategic.train-strength"), TEXT("TRAIN STR"), Person.Strength }
			};
			TSharedRef<SWrapBox> TrainingBox = SNew(SWrapBox);
			for (const FTrainingButton& Entry : TrainingButtons)
			{
				const bool bCanTrain = !Person.bAssignedToCraft && Entry.Attribute < 100;
				const FString TrainingLabel = Localized(Entry.LocalizationKey, Entry.EnglishLabel);
				RenderedDynamicLabels.Add(TrainingLabel);
				TrainingBox->AddSlot()
				.Padding(FMargin(0.0f, 0.0f, 3.0f, 3.0f))
				[
					SNew(SButton)
					.IsEnabled(bCanTrain)
					.IsFocusable(true)
					.ToolTipText(FText::FromString(Person.bAssignedToCraft
						? Localized(TEXT("strategic.train-craft-tooltip"),
							TEXT("Remove this person from their craft roster before training."))
						: Entry.Attribute >= 100
							? Localized(TEXT("strategic.train-max-tooltip"),
								TEXT("This attribute is already at its maximum."))
							: Localized(TEXT("strategic.train-start-tooltip"),
								TEXT("Begin one deterministic training cycle for this attribute."))))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleTrainPersonnelClicked,
						Person.PersonnelId, Entry.Focus)
					[
						MakeText(TrainingLabel, 9, bCanTrain ? PrimaryText : SecondaryText)
					]
				];
			}
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
			[
				TrainingBox
			];

			TSharedRef<SWrapBox> PersonnelActions = SNew(SWrapBox);
			for (const FStrategicBaseView& Destination : CurrentSnapshot.Bases)
			{
				if (Destination.BaseId == Person.BaseId)
				{
					continue;
				}
				const FString TransferLabel = LocalizedFormat(
					TEXT("strategic.transfer-label-format"), TEXT("MOVE TO {0}"),
					{ Destination.Name.ToUpper() });
				RenderedDynamicLabels.Add(TransferLabel);
				PersonnelActions->AddSlot().Padding(FMargin(0.0f, 0.0f, 3.0f, 3.0f))
				[
					SNew(SButton)
					.IsEnabled(!Person.bAssignedToCraft)
					.IsFocusable(true)
					.ToolTipText(FText::FromString(Person.bAssignedToCraft
						? Localized(TEXT("strategic.transfer-craft-tooltip"),
							TEXT("Remove this person from their craft roster before transfer."))
						: LocalizedFormat(
							TEXT("strategic.transfer-tooltip-format"),
							TEXT("Transfer this person and their equipped items to {0}, subject to destination capacity."),
							{ Destination.Name })))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleTransferPersonnelClicked,
						Person.PersonnelId, Destination.BaseId)
					[
						MakeText(TransferLabel, 9,
							Person.bAssignedToCraft ? SecondaryText : PrimaryText)
					]
				];
			}
			const bool bConfirmDismiss = PendingDismissPersonnelId == Person.PersonnelId;
			const FString DismissLabel = bConfirmDismiss
				? Localized(TEXT("strategic.dismiss-confirm"), TEXT("CONFIRM DISMISS"))
				: Localized(TEXT("strategic.dismiss"), TEXT("DISMISS"));
			RenderedDynamicLabels.Add(DismissLabel);
			PersonnelActions->AddSlot().Padding(FMargin(0.0f, 0.0f, 3.0f, 3.0f))
			[
				SNew(SButton)
				.IsEnabled(!Person.bAssignedToCraft)
				.IsFocusable(true)
				.ButtonColorAndOpacity(bConfirmDismiss
					? FLinearColor(0.52f, 0.12f, 0.06f, 1.0f)
					: FLinearColor(0.20f, 0.07f, 0.07f, 1.0f))
				.ToolTipText(FText::FromString(Person.bAssignedToCraft
					? Localized(TEXT("strategic.dismiss-craft-tooltip"),
						TEXT("Remove this person from their craft roster before dismissal."))
					: Localized(TEXT("strategic.dismiss-tooltip"),
						TEXT("Dismiss this person permanently. Equipped items return to base stores; confirmation is required."))))
				.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleDismissPersonnelClicked, Person.PersonnelId)
				[
					MakeText(DismissLabel, 9,
						Person.bAssignedToCraft ? SecondaryText : PrimaryText, bConfirmDismiss)
				]
			];
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
			[
				PersonnelActions
			];
		}
	}
	if (!CurrentSnapshot.Personnel.IsEmpty())
	{
		const FString AutoEquipLabel = Localized(
			TEXT("strategic.auto-equip-field-team"), TEXT("AUTO-EQUIP AVAILABLE FIELD TEAM"));
		RenderedDynamicLabels.Add(AutoEquipLabel);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 7.0f, 0.0f, 2.0f)
		[
			SNew(SButton)
			.IsFocusable(true)
			.ButtonColorAndOpacity(FLinearColor(0.0f, 0.25f, 0.38f, 1.0f))
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleEquipFieldTeamClicked)
			[
				MakeText(AutoEquipLabel, 11)
			]
		];
	}
	if (!CurrentSnapshot.Memorial.IsEmpty())
	{
		const FString MemorialHeader = LocalizedFormat(
			TEXT("strategic.memorial-count-format"), TEXT("MEMORIAL  {0}"),
			{ FString::FromInt(CurrentSnapshot.Memorial.Num()) });
		RenderedDynamicLabels.Add(MemorialHeader);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 7.0f)
		[
			MakeText(MemorialHeader, 16, Warning, true)
		];
		for (const FStrategicMemorialView& Memorial : CurrentSnapshot.Memorial)
		{
			const FString MemorialCard = LocalizedFormat(
				TEXT("strategic.memorial-card-format"),
				TEXT("{0}  •  {1}\nRANK {2}   MISSIONS {3}   KILLS {4}   DOCTRINE LEVELS {5}   CITATIONS {6}\nLAST DUTY {7} UTC  •  {8}"),
				{
					Memorial.DisplayName,
					LocalizedContentName(Memorial.RoleId, Memorial.RoleDisplayName),
					FString::FromInt(Memorial.Rank),
					FString::FromInt(Memorial.Missions),
					FString::FromInt(Memorial.Kills),
					FString::FromInt(Memorial.DoctrineSelections.Num()),
					FString::FromInt(Memorial.Commendations.Num()),
					Memorial.DeathUtc.ToString(TEXT("%Y-%m-%d")),
					LocalizedContentName(Memorial.CauseId, Memorial.CauseDisplayName)
				});
			RenderedDynamicLabels.Add(MemorialCard);
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				MakeText(MemorialCard, 10, PrimaryText)
			];
			const FString MemorialServiceLabel = PersonnelServiceHistoryLabel(
				Memorial.ServiceHistory, Memorial.Missions);
			RenderedDynamicLabels.Add(MemorialServiceLabel);
			LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 2.0f)
			[
				MakeText(MemorialServiceLabel, 9, Warning, true)
			];
			if (Memorial.StewardshipToursCompleted > 0)
			{
				const FString StewardshipHistory = LocalizedFormat(
					TEXT("strategic.stewardship-memorial-format"), TEXT("STEWARDSHIP TOURS {0}"),
					{ FString::FromInt(Memorial.StewardshipToursCompleted) });
				RenderedDynamicLabels.Add(StewardshipHistory);
				LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 2.0f)
				[
					MakeText(StewardshipHistory, 9, Warning, true)
				];
			}
			for (const FStrategicPersonnelCommendationView& Commendation : Memorial.Commendations)
			{
				const FString CommendationLabel = LocalizedFormat(
					TEXT("strategic.personnel-commendation-format"), TEXT("◆ {0} — {1}"),
					{
						LocalizedContentName(Commendation.CommendationId, Commendation.DisplayName),
						LocalizedContentField(
							Commendation.CommendationId, TEXT("summary"), Commendation.Summary)
					});
				RenderedDynamicLabels.Add(CommendationLabel);
				LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 1.0f)
				[
					MakeText(CommendationLabel, 9, SecondaryText)
				];
			}
		}
	}

	const FString FleetLabel = LocalizedFormat(
		TEXT("strategic.fleet-count-format"), TEXT("FLEET {0}"),
		{ FString::FromInt(CurrentSnapshot.Craft.Num()) });
	RenderedDynamicLabels.Add(FleetLabel);
	FleetPanelAnchor = MakeText(FleetLabel, 16, Accent, true);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 7.0f)
	[
		FleetPanelAnchor.ToSharedRef()
	];
	if (CurrentSnapshot.Craft.IsEmpty())
	{
		const FString NoCraftLabel = Localized(
			TEXT("strategic.fleet-empty"), TEXT("No operational craft"));
		RenderedDynamicLabels.Add(NoCraftLabel);
		LeftBox->AddSlot().AutoHeight()[MakeText(NoCraftLabel, 13, SecondaryText)];
	}
	for (const FStrategicCraftView& Craft : CurrentSnapshot.Craft)
	{
		const FStrategicPersonnelView* AssignedPilot = Craft.AssignedPilotId.IsValid()
			? CurrentSnapshot.Personnel.FindByPredicate(
				[&Craft](const FStrategicPersonnelView& Person) { return Person.PersonnelId == Craft.AssignedPilotId; })
			: nullptr;
		const FString PilotLabel = AssignedPilot != nullptr
			? LocalizedFormat(
				TEXT("strategic.craft-pilot-format"), TEXT("PILOT {0}"),
				{ AssignedPilot->DisplayName.ToUpper() })
			: Localized(TEXT("strategic.craft-no-pilot"), TEXT("NO PILOT"));
		const FString CraftCard = LocalizedFormat(
			TEXT("strategic.craft-card-format"),
			TEXT("{0} • {1}\n{2}   HULL {3}/{4}   FUEL {5}/{6}   TEAM {7}/{8}   {9}"),
			{
				Craft.DisplayName,
				LocalizedContentName(Craft.CraftRuleId, Craft.TypeDisplayName),
				CraftStatusLabel(Craft.StatusType),
				FString::FromInt(Craft.CurrentHull),
				FString::FromInt(Craft.MaxHull),
				FString::FromInt(Craft.CurrentFuel),
				FString::FromInt(Craft.FuelCapacity),
				FString::FromInt(Craft.AssignedAgents),
				FString::FromInt(Craft.AgentCapacity),
				PilotLabel
			});
		RenderedDynamicLabels.Add(CraftCard);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			MakeText(CraftCard, 12)
		];
		if (Craft.Mentorship.bHasMentor)
		{
			const FString MentorshipLabel = PersonnelMentorshipLabel(Craft.Mentorship);
			const FString MentorshipGuidance = PersonnelMentorshipGuidance();
			RenderedDynamicLabels.Add(MentorshipLabel);
			RenderedDynamicLabels.Add(MentorshipGuidance);
			LeftBox->AddSlot().AutoHeight().Padding(10.0f, 3.0f, 0.0f, 1.0f)
			[
				MakeText(MentorshipLabel, 10,
					Craft.Mentorship.bActive ? Success : SecondaryText, true)
			];
			LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 3.0f)
			[
				MakeText(MentorshipGuidance, 8, SecondaryText)
			];
		}
		if (Craft.LegacyRelay.bHasSpecialist)
		{
			const FString RelayLabel = PersonnelLegacyRelayLabel(Craft.LegacyRelay);
			const FString RelayGuidance = PersonnelLegacyRelayGuidance();
			RenderedDynamicLabels.Add(RelayLabel);
			RenderedDynamicLabels.Add(RelayGuidance);
			LeftBox->AddSlot().AutoHeight().Padding(10.0f, 3.0f, 0.0f, 1.0f)
			[
				MakeText(RelayLabel, 10,
					Craft.LegacyRelay.bActive ? Success : SecondaryText, true)
			];
			LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 3.0f)
			[
				MakeText(RelayGuidance, 8, SecondaryText)
			];
		}
		if (Craft.SquadBonds.ResolvedPersonnelCount >= 2)
		{
			const FString SquadBondTitle = Localized(
				TEXT("personnel.squad-bond-name"), TEXT("FIELD CADENCE"));
			RenderedDynamicLabels.Add(SquadBondTitle);
			LeftBox->AddSlot().AutoHeight().Padding(10.0f, 3.0f, 0.0f, 1.0f)
			[
				MakeText(SquadBondTitle, 10, Accent, true)
			];
			if (Craft.SquadBonds.ActivePairs.IsEmpty())
			{
				const FString InactiveLabel = PersonnelSquadBondInactiveLabel(Craft.SquadBonds);
				RenderedDynamicLabels.Add(InactiveLabel);
				LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 1.0f)
				[
					MakeText(InactiveLabel, 10, SecondaryText, true)
				];
			}
			for (const FPersonnelSquadBondPairView& Pair : Craft.SquadBonds.ActivePairs)
			{
				const FString PairLabel = PersonnelSquadBondActiveLabel(Pair);
				RenderedDynamicLabels.Add(PairLabel);
				LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 1.0f)
				[
					MakeText(PairLabel, 10, Success, true)
				];
			}
			for (const FPersonnelSquadBondPairView& Pair : Craft.SquadBonds.DevelopingPairs)
			{
				const FString PairLabel = PersonnelSquadBondDevelopingLabel(Pair);
				RenderedDynamicLabels.Add(PairLabel);
				LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 1.0f)
				[
					MakeText(PairLabel, 10, SecondaryText, true)
				];
			}
			const FString Guidance = PersonnelSquadBondGuidance();
			RenderedDynamicLabels.Add(Guidance);
			LeftBox->AddSlot().AutoHeight().Padding(10.0f, 0.0f, 0.0f, 3.0f)
			[
				MakeText(Guidance, 8, SecondaryText)
			];
		}
		if (Craft.StatusType == ECraftStatus::Servicing)
		{
			BuildCraftServicePanel(Craft);
		}
		if (!Craft.PendingSalvage.IsEmpty())
		{
			const FStrategicBaseView* CraftBase = CurrentSnapshot.Bases.FindByPredicate(
				[&Craft](const FStrategicBaseView& Base) { return Base.BaseId == Craft.BaseId; });
			const FString SalvageTitle = Localized(
				TEXT("strategic.salvage-title"), TEXT("SALVAGE DISPOSITION"));
			const FString SalvageState = Craft.bSalvageDispositionAvailable
				? Localized(TEXT("strategic.salvage-ready"),
					TEXT("Recovered cargo is ready. Retain it in base storage or sell it directly."))
				: Localized(TEXT("strategic.salvage-returning"),
					TEXT("Recovered cargo is secured aboard. Disposition controls unlock after landing."));
			RenderedDynamicLabels.Add(SalvageTitle);
			RenderedDynamicLabels.Add(SalvageState);
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 1.0f)
			[
				MakeText(SalvageTitle, 11, Warning, true)
			];
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
			[
				MakeText(SalvageState, 9, SecondaryText)
			];
			for (const FStrategicCraftSalvageView& Salvage : Craft.PendingSalvage)
			{
				const FString ItemName = LocalizedContentName(Salvage.ItemId, Salvage.DisplayName);
				const FString SalvageLabel = Salvage.UnitSellValue > 0
					? LocalizedFormat(
						TEXT("strategic.salvage-item-format"),
						TEXT("{0}  ×{1}  •  {2} STORAGE  •  SALE {3}"),
						{
							ItemName,
							FString::FromInt(Salvage.Quantity),
							LexToString(Salvage.TotalStorage),
							LexToString(Salvage.TotalSellValue)
						})
					: LocalizedFormat(
						TEXT("strategic.salvage-retain-only-format"),
						TEXT("{0}  ×{1}  •  {2} STORAGE  •  NO SALE VALUE"),
						{
							ItemName,
							FString::FromInt(Salvage.Quantity),
							LexToString(Salvage.TotalStorage)
						});
				RenderedDynamicLabels.Add(SalvageLabel);
				LeftBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
				[
					MakeText(SalvageLabel, 10)
				];
				const FString RetainLabel = Localized(
					TEXT("strategic.salvage-retain-all"), TEXT("RETAIN ALL"));
				const FString SellLabel = Salvage.UnitSellValue > 0
					? LocalizedFormat(TEXT("strategic.salvage-sell-all-format"),
						TEXT("SELL ALL  •  {0}"), { LexToString(Salvage.TotalSellValue) })
					: Localized(TEXT("strategic.salvage-no-sale-value"), TEXT("NO SALE VALUE"));
				const FString RetainTooltip = !Craft.bSalvageDispositionAvailable
					? Localized(TEXT("strategic.salvage-await-landing-tooltip"),
						TEXT("This craft must land before recovered cargo can be retained."))
					: Salvage.bCanRetainAtBase
						? LocalizedFormat(TEXT("strategic.salvage-retain-tooltip-format"),
							TEXT("Move all recovered units into {0} inventory, using {1} storage."),
							{ CraftBase != nullptr ? CraftBase->Name : FString(), LexToString(Salvage.TotalStorage) })
						: LocalizedFormat(TEXT("strategic.salvage-storage-blocked-tooltip-format"),
							TEXT("Retaining all requires {0} storage; free capacity or sell this salvage."),
							{ LexToString(Salvage.TotalStorage) });
				const FString SellTooltip = !Craft.bSalvageDispositionAvailable
					? Localized(TEXT("strategic.salvage-await-sale-tooltip"),
						TEXT("This craft must land before recovered cargo can be sold."))
					: Salvage.bCanSell
						? LocalizedFormat(TEXT("strategic.salvage-sell-tooltip-format"),
							TEXT("Sell all recovered units directly for {0}; base storage is not used."),
							{ LexToString(Salvage.TotalSellValue) })
						: Localized(TEXT("strategic.salvage-unsellable-tooltip"),
							TEXT("This recovered item has no positive sale value."));
				RenderedDynamicLabels.Add(RetainLabel);
				RenderedDynamicLabels.Add(SellLabel);
				LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SButton)
						.IsEnabled(Salvage.bCanRetainAtBase)
						.IsFocusable(true)
						.ToolTipText(FText::FromString(RetainTooltip))
						.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCraftSalvageClicked,
							Craft.CraftId, Salvage.ItemId, Salvage.Quantity,
							ECraftSalvageDisposition::RetainAtBase)
						[
							MakeText(RetainLabel, 9, Salvage.bCanRetainAtBase ? PrimaryText : SecondaryText)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.IsEnabled(Salvage.bCanSell)
						.IsFocusable(true)
						.ToolTipText(FText::FromString(SellTooltip))
						.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCraftSalvageClicked,
							Craft.CraftId, Salvage.ItemId, Salvage.Quantity,
							ECraftSalvageDisposition::Sell)
						[
							MakeText(SellLabel, 9, Salvage.bCanSell ? PrimaryText : SecondaryText)
						]
					]
				];
			}
		}
		if (!Craft.Weapons.IsEmpty())
		{
			const FString AmmunitionTitle = LocalizedFormat(
				TEXT("strategic.craft-ammunition-title-format"),
				TEXT("CRAFT AMMUNITION  {0}/{1}"),
				{
					LexToString(Craft.TotalAmmunitionLoaded),
					LexToString(Craft.TotalAmmunitionCapacity)
				});
			RenderedDynamicLabels.Add(AmmunitionTitle);
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 2.0f)
			[
				MakeText(AmmunitionTitle, 11,
					Craft.TotalAmmunitionMissing > 0 ? Warning : Success, true)
			];
			for (const FStrategicCraftWeaponView& Weapon : Craft.Weapons)
			{
				const FString WeaponLabel = LocalizedFormat(
					TEXT("strategic.craft-weapon-ammunition-format"),
					TEXT("{0} ×{1}  •  {2} {3}/{4}  •  BASE {5}  •  LOADABLE {6}"),
					{
						LocalizedContentName(Weapon.WeaponItemId, Weapon.WeaponDisplayName),
						FString::FromInt(Weapon.MountCount),
						LocalizedContentName(Weapon.AmmunitionItemId, Weapon.AmmunitionDisplayName),
						LexToString(Weapon.LoadedAmmunition),
						LexToString(Weapon.Capacity),
						LexToString(Weapon.BaseAvailableAmmunition),
						LexToString(Weapon.LoadableAmmunition)
					});
				RenderedDynamicLabels.Add(WeaponLabel);
				LeftBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
				[
					MakeText(WeaponLabel, 9, SecondaryText)
				];
			}

			const FString FullRearmLabel = LocalizedFormat(
				TEXT("strategic.craft-rearm-full-format"),
				TEXT("FULL REARM  •  {0}"),
				{ LexToString(Craft.TotalAmmunitionMissing) });
			const FString LoadAvailableLabel = LocalizedFormat(
				TEXT("strategic.craft-rearm-available-format"),
				TEXT("LOAD AVAILABLE  •  {0}/{1}"),
				{
					LexToString(Craft.TotalAmmunitionLoadable),
					LexToString(Craft.TotalAmmunitionMissing)
				});
			const FString FullRearmTooltip = !Craft.PendingSalvage.IsEmpty()
				? Localized(TEXT("strategic.craft-rearm-salvage-tooltip"),
					TEXT("Retain or sell all recovered salvage before rearming this craft."))
				: Craft.StatusType != ECraftStatus::Grounded
					? Localized(TEXT("strategic.craft-rearm-grounded-tooltip"),
						TEXT("Only a grounded craft can rearm."))
					: Craft.TotalAmmunitionMissing <= 0
						? Localized(TEXT("strategic.craft-rearm-not-needed-tooltip"),
							TEXT("Every mounted weapon is already fully armed."))
						: Craft.bCanRearmFully
							? LocalizedFormat(TEXT("strategic.craft-rearm-full-tooltip-format"),
								TEXT("Load all {0} missing rounds in one transaction."),
								{ LexToString(Craft.TotalAmmunitionMissing) })
							: LocalizedFormat(TEXT("strategic.craft-rearm-full-insufficient-tooltip-format"),
								TEXT("A full load needs {0} rounds; base stores can supply {1}."),
								{
									LexToString(Craft.TotalAmmunitionMissing),
									LexToString(Craft.TotalAmmunitionLoadable)
								});
			const FString LoadAvailableTooltip = !Craft.PendingSalvage.IsEmpty()
				? Localized(TEXT("strategic.craft-rearm-salvage-tooltip"),
					TEXT("Retain or sell all recovered salvage before rearming this craft."))
				: Craft.StatusType != ECraftStatus::Grounded
					? Localized(TEXT("strategic.craft-rearm-grounded-tooltip"),
						TEXT("Only a grounded craft can rearm."))
					: Craft.TotalAmmunitionMissing <= 0
						? Localized(TEXT("strategic.craft-rearm-not-needed-tooltip"),
							TEXT("Every mounted weapon is already fully armed."))
						: Craft.bCanLoadAvailableAmmunition
							? LocalizedFormat(TEXT("strategic.craft-rearm-available-tooltip-format"),
								TEXT("Load {0} available rounds now; {1} will remain missing."),
								{
									LexToString(Craft.TotalAmmunitionLoadable),
									LexToString(FMath::Max<int64>(0,
										Craft.TotalAmmunitionMissing - Craft.TotalAmmunitionLoadable))
								})
							: Localized(TEXT("strategic.craft-rearm-no-stock-tooltip"),
								TEXT("No compatible ammunition is available at this craft's base."));
			RenderedDynamicLabels.Add(FullRearmLabel);
			RenderedDynamicLabels.Add(LoadAvailableLabel);
			RenderedCraftRearmControlCount += 2;
			RenderedEnabledCraftRearmControlCount += Craft.bCanRearmFully ? 1 : 0;
			RenderedEnabledCraftRearmControlCount += Craft.bCanLoadAvailableAmmunition ? 1 : 0;
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 4.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
				[
					SNew(SButton)
					.IsEnabled(Craft.bCanRearmFully)
					.IsFocusable(true)
					.ButtonColorAndOpacity(Craft.bCanRearmFully
						? FLinearColor(0.0f, 0.30f, 0.38f, 1.0f)
						: FLinearColor(0.07f, 0.08f, 0.10f, 1.0f))
					.ToolTipText(FText::FromString(FullRearmTooltip))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCraftRearmClicked,
						Craft.CraftId, ECraftRearmPolicy::FullLoad)
					[
						MakeText(FullRearmLabel, 9,
							Craft.bCanRearmFully ? PrimaryText : SecondaryText)
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SButton)
					.IsEnabled(Craft.bCanLoadAvailableAmmunition)
					.IsFocusable(true)
					.ButtonColorAndOpacity(Craft.bCanLoadAvailableAmmunition
						? FLinearColor(0.0f, 0.30f, 0.38f, 1.0f)
						: FLinearColor(0.07f, 0.08f, 0.10f, 1.0f))
					.ToolTipText(FText::FromString(LoadAvailableTooltip))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCraftRearmClicked,
						Craft.CraftId, ECraftRearmPolicy::LoadAvailable)
					[
						MakeText(LoadAvailableLabel, 9,
							Craft.bCanLoadAvailableAmmunition ? PrimaryText : SecondaryText)
					]
				]
			];
		}
		if (Craft.StatusType == ECraftStatus::Grounded)
		{
			const FString PilotAssignmentLabel = Localized(
				TEXT("strategic.craft-pilot-assignment"), TEXT("PILOT ASSIGNMENT"));
			RenderedDynamicLabels.Add(PilotAssignmentLabel);
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 2.0f)
			[
				MakeText(PilotAssignmentLabel, 10, SecondaryText, true)
			];
			TSharedRef<SWrapBox> PilotControls = SNew(SWrapBox);
			bool bHasPilotCandidate = false;
			for (const FStrategicPersonnelView& Person : CurrentSnapshot.Personnel)
			{
				if (Person.BaseId != Craft.BaseId || Person.RoleCategory != EPersonnelRoleCategory::Pilot)
				{
					continue;
				}
				bHasPilotCandidate = true;
				const bool bSelected = Craft.AssignedPilotId == Person.PersonnelId;
				const bool bEligible = bSelected
					|| (Person.StatusType == EPersonnelStatus::Available && !Person.bAssignedToCraft);
				PilotControls->AddSlot().Padding(FMargin(0.0f, 0.0f, 3.0f, 3.0f))
				[
					SNew(SButton)
					.IsEnabled(bEligible)
					.IsFocusable(true)
					.ButtonColorAndOpacity(bSelected
						? FLinearColor(0.0f, 0.40f, 0.46f, 1.0f)
						: FLinearColor(0.03f, 0.11f, 0.2f, 1.0f))
					.ToolTipText(FText::FromString(bSelected
						? Localized(TEXT("strategic.craft-pilot-clear-tooltip"),
							TEXT("Clear this craft's pilot assignment."))
						: bEligible
							? Localized(TEXT("strategic.craft-pilot-assign-tooltip"),
								TEXT("Assign this available pilot to the craft."))
							: Localized(TEXT("strategic.craft-pilot-unavailable-tooltip"),
								TEXT("Pilot is unavailable or assigned to another craft."))))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCraftPilotClicked,
						Craft.CraftId, bSelected ? FGuid() : Person.PersonnelId)
					[
						MakeText(FString::Printf(TEXT("%s%s"), bSelected ? TEXT("✓  ") : TEXT(""),
							*Person.DisplayName.ToUpper()), 9, bEligible ? PrimaryText : SecondaryText)
					]
				];
			}
			if (bHasPilotCandidate)
			{
				LeftBox->AddSlot().AutoHeight()[PilotControls];
			}
			else
			{
				const FString NoPilotsLabel = Localized(
					TEXT("strategic.craft-no-pilots-at-base"),
					TEXT("No pilots stationed at this base"));
				RenderedDynamicLabels.Add(NoPilotsLabel);
				LeftBox->AddSlot().AutoHeight()[MakeText(NoPilotsLabel, 10, SecondaryText)];
			}

			if (Craft.AgentCapacity > 0)
			{
				const FString FieldTeamLabel = LocalizedFormat(
					TEXT("strategic.craft-field-team-format"), TEXT("FIELD TEAM {0}/{1}"),
					{ FString::FromInt(Craft.AssignedAgents), FString::FromInt(Craft.AgentCapacity) });
				RenderedDynamicLabels.Add(FieldTeamLabel);
				LeftBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 2.0f)
				[
					MakeText(FieldTeamLabel, 10, SecondaryText, true)
				];
				TSharedRef<SWrapBox> AgentControls = SNew(SWrapBox);
				bool bHasAgentCandidate = false;
				for (const FStrategicPersonnelView& Person : CurrentSnapshot.Personnel)
				{
					if (Person.BaseId != Craft.BaseId || Person.RoleCategory != EPersonnelRoleCategory::FieldAgent)
					{
						continue;
					}
					bHasAgentCandidate = true;
					const bool bSelected = Craft.AssignedAgentIds.Contains(Person.PersonnelId);
					const bool bEligible = bSelected || (Craft.AssignedAgents < Craft.AgentCapacity
						&& Person.StatusType == EPersonnelStatus::Available && !Person.bAssignedToCraft);
					AgentControls->AddSlot().Padding(FMargin(0.0f, 0.0f, 3.0f, 3.0f))
					[
						SNew(SButton)
						.IsEnabled(bEligible)
						.IsFocusable(true)
						.ButtonColorAndOpacity(bSelected
							? FLinearColor(0.0f, 0.34f, 0.40f, 1.0f)
							: FLinearColor(0.03f, 0.11f, 0.2f, 1.0f))
						.ToolTipText(FText::FromString(bSelected
							? Localized(TEXT("strategic.craft-agent-remove-tooltip"),
								TEXT("Remove this agent from the craft's field team."))
							: bEligible
								? Localized(TEXT("strategic.craft-agent-add-tooltip"),
									TEXT("Add this available agent to the craft's field team."))
								: Localized(TEXT("strategic.craft-agent-unavailable-tooltip"),
									TEXT("Agent is unavailable, assigned elsewhere, or the craft is full."))))
						.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCraftAgentClicked,
							Craft.CraftId, Person.PersonnelId)
						[
							MakeText(FString::Printf(TEXT("%s%s"), bSelected ? TEXT("✓  ") : TEXT(""),
								*Person.DisplayName.ToUpper()), 9, bEligible ? PrimaryText : SecondaryText)
						]
					];
				}
				if (bHasAgentCandidate)
				{
					LeftBox->AddSlot().AutoHeight()[AgentControls];
				}
				else
				{
					const FString NoAgentsLabel = Localized(
						TEXT("strategic.craft-no-agents-at-base"),
						TEXT("No field agents stationed at this base"));
					RenderedDynamicLabels.Add(NoAgentsLabel);
					LeftBox->AddSlot().AutoHeight()[MakeText(NoAgentsLabel, 10, SecondaryText)];
				}
			}

			if (CurrentSnapshot.Bases.Num() > 1)
			{
				const FString CraftRebasingLabel = Localized(
					TEXT("strategic.craft-rebasing"), TEXT("CRAFT REBASING"));
				RenderedDynamicLabels.Add(CraftRebasingLabel);
				LeftBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 2.0f)
				[
					MakeText(CraftRebasingLabel, 10, SecondaryText, true)
				];
				const bool bRosterClear = !Craft.AssignedPilotId.IsValid() && Craft.AssignedAgentIds.IsEmpty();
				TSharedRef<SWrapBox> TransferControls = SNew(SWrapBox);
				for (const FStrategicBaseView& Destination : CurrentSnapshot.Bases)
				{
					if (Destination.BaseId == Craft.BaseId)
					{
						continue;
					}
					const bool bHasBerth = Destination.CraftOccupied < Destination.CraftCapacity;
					const bool bSalvageClear = Craft.PendingSalvage.IsEmpty();
					const bool bCanTransfer = bRosterClear && bHasBerth && bSalvageClear;
					const FString ToolTip = !bSalvageClear
						? Localized(TEXT("strategic.craft-rebase-salvage-tooltip"),
							TEXT("Retain or sell all recovered salvage before rebasing this craft."))
						: !bRosterClear
						? Localized(TEXT("strategic.craft-rebase-roster-tooltip"),
							TEXT("Release the assigned pilot and field team before rebasing this craft."))
						: !bHasBerth
							? Localized(TEXT("strategic.craft-rebase-berth-tooltip"),
								TEXT("The destination has no free operational craft berth; incoming orders also reserve capacity."))
							: LocalizedFormat(TEXT("strategic.craft-rebase-tooltip-format"),
								TEXT("Rebase immediately to {0}. Mounted equipment and cargo remain aboard."),
								{ Destination.Name });
					const FString RebaseLabel = LocalizedFormat(
						TEXT("strategic.craft-rebase-format"),
						TEXT("REBASE TO {0} • BERTHS {1}/{2}"),
						{
							Destination.Name.ToUpper(),
							FString::FromInt(Destination.CraftOccupied),
							FString::FromInt(Destination.CraftCapacity)
						});
					RenderedDynamicLabels.Add(RebaseLabel);
					TransferControls->AddSlot().Padding(FMargin(0.0f, 0.0f, 3.0f, 3.0f))
					[
						SNew(SButton)
						.IsEnabled(bCanTransfer)
						.IsFocusable(true)
						.ButtonColorAndOpacity(FLinearColor(0.03f, 0.11f, 0.2f, 1.0f))
						.ToolTipText(FText::FromString(ToolTip))
						.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleTransferCraftClicked,
							Craft.CraftId, Destination.BaseId)
						[
							MakeText(RebaseLabel, 9, bCanTransfer ? PrimaryText : SecondaryText)
						]
					];
				}
				LeftBox->AddSlot().AutoHeight()[TransferControls];
			}
		}
		const FString AutoPrepareLabel = Localized(
			TEXT("strategic.craft-auto-prepare"), TEXT("AUTO-PREPARE CRAFT"));
		RenderedDynamicLabels.Add(AutoPrepareLabel);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 5.0f)
		[
			SNew(SButton)
			.IsEnabled(Craft.StatusType == ECraftStatus::Grounded && Craft.PendingSalvage.IsEmpty())
			.IsFocusable(true)
			.ToolTipText(FText::FromString(!Craft.PendingSalvage.IsEmpty()
				? Localized(TEXT("strategic.craft-auto-prepare-salvage-tooltip"),
					TEXT("Retain or sell all recovered salvage before preparing this craft."))
				: Craft.StatusType == ECraftStatus::Grounded
				? Localized(TEXT("strategic.craft-auto-prepare-tooltip"),
					TEXT("Assign available crew, install compatible inventory, rearm, and begin service as needed."))
				: Localized(TEXT("strategic.craft-auto-prepare-grounded-tooltip"),
					TEXT("Craft preparation requires a grounded craft."))))
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandlePrepareCraftClicked, Craft.CraftId)
			[
				MakeText(AutoPrepareLabel, 11)
			]
		];
	}

	const FString GlobalSituationLabel = Localized(
		TEXT("strategic.global-situation"), TEXT("GLOBAL SITUATION"));
	RenderedDynamicLabels.Add(GlobalSituationLabel);
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 7.0f)
	[
		MakeText(GlobalSituationLabel, 16, Accent, true)
	];
	const FString GlobalSummary = LocalizedFormat(
		TEXT("strategic.global-summary-format"),
		TEXT("CONTACTS {0}   SITES {1}   BASE ALERTS {2}\nMISSIONS {3} LAUNCHED / {4} THWARTED / {5} ESCAPED\nNEXT WINDOW {6} h\nFUNDING {7}  •  OUTGOINGS -{8}  •  NET {9}"),
		{
			FString::FromInt(CurrentSnapshot.Contacts.Num()),
			FString::FromInt(CurrentSnapshot.Sites.Num()),
			FString::FromInt(CurrentSnapshot.BaseAssaults.Num()),
			FString::FromInt(CurrentSnapshot.AdversaryMissionsLaunched),
			FString::FromInt(CurrentSnapshot.AdversaryMissionsThwarted),
			FString::FromInt(CurrentSnapshot.AdversaryMissionsEscaped),
			LexToString((CurrentSnapshot.NextAdversaryMissionSeconds + 3599) / 3600),
			FString::Printf(TEXT("%+lld"), CurrentSnapshot.MonthlyFunding),
			LexToString(CurrentSnapshot.MonthlyOutgoings),
			FString::Printf(TEXT("%+lld"), CurrentSnapshot.NetMonthlyFunding)
		});
	RenderedDynamicLabels.Add(GlobalSummary);
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		MakeText(GlobalSummary, 12, SecondaryText)
	];
	FString AdaptationStatus;
	if (CurrentSnapshot.bAtMaximumAdversaryEscalation)
	{
		AdaptationStatus = Localized(TEXT("strategic.adaptation-maximum"), TEXT("ADAPTATION CEILING REACHED"));
	}
	else if (CurrentSnapshot.ResolvedMissionsUntilNextEscalation == 1)
	{
		AdaptationStatus = Localized(TEXT("strategic.next-adaptation-one"), TEXT("NEXT ADAPTATION IN 1 RESOLUTION"));
	}
	else
	{
		AdaptationStatus = LocalizedFormat(
			TEXT("strategic.next-adaptation-format"),
			TEXT("NEXT ADAPTATION IN {0} RESOLUTIONS"),
			{ FString::Printf(TEXT("%lld"), CurrentSnapshot.ResolvedMissionsUntilNextEscalation) });
	}
	const FString CampaignObjectives = LocalizedFormat(
		TEXT("strategic.campaign-objectives-format"),
		TEXT("CONTAINMENT {0}/{1} THWARTED  •  ESCALATION {2}/{3}\nREGIONAL STRAIN {4}/{5}  •  {6}"),
		{
			FString::FromInt(CurrentSnapshot.AdversaryMissionsThwarted),
			FString::FromInt(CurrentSnapshot.VictoryThwartedMissionTarget),
			FString::FromInt(CurrentSnapshot.AdversaryEscalationLevel),
			FString::FromInt(CurrentSnapshot.VictoryEscalationTarget),
			FString::FromInt(CurrentSnapshot.HighestRegionalPressure),
			FString::FromInt(CurrentSnapshot.RegionalCollapsePressureThreshold),
			AdaptationStatus
		});
	RenderedDynamicLabels.Add(CampaignObjectives);
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		MakeText(CampaignObjectives, 12, Accent)
	];
	if (CurrentSnapshot.HorizonCompact.RequiredCharters > 0)
	{
		const FStrategicHorizonCompactView& Compact = CurrentSnapshot.HorizonCompact;
		const FString CompactName = Localized(
			TEXT("strategic.coalition-horizon-compact"), TEXT("HORIZON COMPACT"));
		const FString CompactDetail = Compact.bRatified
			? LocalizedFormat(
				TEXT("strategic.coalition-compact-cohesion-format"),
				TEXT("{0}  •  ACTIVE {1} • WITHDRAWN {2}\nWITHDRAW BELOW {3} SUPPORT • RESTORE AT {4}\nFUNDING {5}% • REDIRECT {6}% • TOTAL {7}/MONTH"),
				{
					CompactName,
					FString::FromInt(Compact.ActiveMemberRegionIds.Num()),
					FString::FromInt(Compact.WithdrawnMemberRegionIds.Num()),
					FString::FromInt(Compact.WithdrawalSupportThreshold),
					FString::FromInt(Compact.RestorationMinimumSupport),
					FString::FromInt(Compact.FundingPercent),
					FString::FromInt(Compact.SharedEscapePressurePercent),
					LexToString(Compact.CurrentMonthlyFunding)
				})
			: LocalizedFormat(
				TEXT("strategic.coalition-compact-detail-format"),
				TEXT("{0}  •  FUNDS {1} • SIGNED {2}/{3} • MIN SUPPORT {4}\nEACH MEMBER -{5} SUPPORT • FUNDING {6}% • REDIRECT {7}%\nTOTAL FUNDING {8} → {9}"),
				{
					CompactName,
					LexToString(Compact.Cost),
					FString::FromInt(Compact.SignedCharters),
					FString::FromInt(Compact.RequiredCharters),
					FString::FromInt(Compact.MinimumMemberSupport),
					FString::FromInt(Compact.MemberSupportCost),
					FString::FromInt(Compact.FundingPercent),
					FString::FromInt(Compact.SharedEscapePressurePercent),
					LexToString(Compact.CurrentMonthlyFunding),
					LexToString(Compact.ProjectedMonthlyFunding)
				});
		const FString CompactDisabledReason = Compact.bRatified
			|| Compact.UnavailableReasonCode.IsNone()
			? FString()
			: LocalizedDiagnostic(Compact.UnavailableReasonCode, Compact.UnavailableReason);
		const FString CompactButtonLabel = CompactDisabledReason.IsEmpty()
			? CompactDetail
			: LocalizedFormat(
				TEXT("strategic.region-action-unavailable-format"), TEXT("{0}\n{1}"),
				{ CompactDetail, CompactDisabledReason });
		RenderedCommandActionLabels.Add(CompactButtonLabel);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 7.0f)
		[
			SNew(SButton)
				.IsEnabled(Compact.bEnabled)
				.IsFocusable(true)
				.ButtonColorAndOpacity(Compact.bRatified
					? FLinearColor(0.05f, 0.40f, 0.27f, 1.0f)
					: FLinearColor(0.24f, 0.15f, 0.42f, 1.0f))
				.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleHorizonCompactClicked)
			[
				MakeText(CompactButtonLabel, 10,
					Compact.bRatified ? Success : (Compact.bEnabled ? PrimaryText : SecondaryText))
			]
		];
		if (Compact.bRatified)
		{
			const FString AidName = Localized(
				TEXT("strategic.coalition-reciprocal-aid"), TEXT("RECIPROCAL AID"));
			auto RegionName = [this](const FName RegionId)
			{
				const FStrategicRegionView* Region = CurrentSnapshot.Regions.FindByPredicate(
					[RegionId](const FStrategicRegionView& View)
					{
						return View.RegionId == RegionId;
					});
				return LocalizedContentName(
					RegionId,
					Region != nullptr ? Region->DisplayName : RegionId.ToString()).ToUpper();
			};
			for (const FStrategicCoalitionAidView& Aid : Compact.AidOptions)
			{
				const FString TargetName = RegionName(Aid.TargetRegionId);
				FString AidDetail = Aid.bEnabled
					? LocalizedFormat(
						TEXT("strategic.coalition-aid-ready-format"),
						TEXT("{0}  •  RELIEVE {1}\nPRESSURE {2} → {3} • {4} ACCEPTS +{5}\nFUNDS {6} • SUPPORT +{7}/-{8} • FUNDING {9}"),
						{
							AidName,
							TargetName,
							FString::FromInt(Aid.TargetCurrentPressure),
							FString::FromInt(Aid.TargetProjectedPressure),
							RegionName(Aid.DonorRegionId),
							FString::FromInt(Aid.PressureTransfer),
							LexToString(Aid.Cost),
							FString::FromInt(Aid.TargetSupportGain),
							FString::FromInt(Aid.DonorSupportCost),
							FString::Printf(TEXT("%+lld"), Aid.MonthlyFundingDelta)
						})
					: LocalizedFormat(
						TEXT("strategic.coalition-aid-policy-format"),
						TEXT("{0}  •  RELIEVE {1}\nFUNDS {2} • CRISIS {3}+ • MOVE UP TO {4} PRESSURE • SUPPORT +{5}/-{6}"),
						{
							AidName,
							TargetName,
							LexToString(Aid.Cost),
							FString::FromInt(Aid.MinimumTargetPressure),
							FString::FromInt(Aid.MaximumPressureTransfer),
							FString::FromInt(Aid.DonorSupportCost),
							FString::FromInt(Aid.DonorSupportCost)
						});
				if (Aid.bDonorWouldWithdraw)
				{
					AidDetail += TEXT("\n") + Localized(
						TEXT("strategic.coalition-withdrawal-warning"),
						TEXT("WARNING: THIS SUPPORT LOSS WILL WITHDRAW A COMPACT MEMBER"));
				}
				const FString DisabledReason = Aid.bEnabled || Aid.UnavailableReasonCode.IsNone()
					? FString()
					: LocalizedDiagnostic(Aid.UnavailableReasonCode, Aid.UnavailableReason);
				const FString AidButtonLabel = DisabledReason.IsEmpty()
					? AidDetail
					: LocalizedFormat(
						TEXT("strategic.region-action-unavailable-format"), TEXT("{0}\n{1}"),
						{ AidDetail, DisabledReason });
				RenderedCommandActionLabels.Add(AidButtonLabel);
				RightBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 4.0f)
				[
					SNew(SButton)
						.IsEnabled(Aid.bEnabled)
						.IsFocusable(true)
						.ButtonColorAndOpacity(Aid.bEnabled
							? FLinearColor(0.08f, 0.34f, 0.38f, 1.0f)
							: FLinearColor(0.12f, 0.16f, 0.20f, 1.0f))
						.OnClicked_UObject(
							this,
							&UUEGTStrategicHudWidget::HandleReciprocalAidClicked,
							Aid.TargetRegionId)
					[
						MakeText(AidButtonLabel, 9, Aid.bEnabled ? PrimaryText : SecondaryText)
					]
				];
			}
		}
	}
	for (const FStrategicRegionView& Region : CurrentSnapshot.Regions)
	{
		const FString RegionDisplayName = LocalizedContentName(Region.RegionId, Region.DisplayName).ToUpper();
		const FString RegionSummary = Region.bHasMandate
			? LocalizedFormat(
				TEXT("strategic.region-mandate-format"),
				TEXT("{0}  •  PRESSURE {1}  •  SUPPORT {2} {3}\nFUNDING {4} → {5} / MONTH"),
				{
					RegionDisplayName,
					FString::FromInt(Region.Pressure),
					FString::FromInt(Region.Support),
					RegionalSupportTierLabel(Region.SupportTier),
					FString::Printf(TEXT("%+lld"), Region.CurrentMonthlyFunding),
					FString::Printf(TEXT("%+lld"), Region.ProjectedMonthlyFunding)
				})
			: LocalizedFormat(
				TEXT("strategic.region-pressure-format"), TEXT("{0}  •  PRESSURE {1}"),
				{ RegionDisplayName, FString::FromInt(Region.Pressure) });
		RenderedDynamicLabels.Add(RegionSummary);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
		[
			MakeText(RegionSummary, 12,
				Region.Pressure >= 70 || (Region.bHasMandate && Region.Support < 40) ? Warning : PrimaryText)
		];
		for (const FStrategicRegionalActionView& Option : Region.ActionOptions)
		{
			const FString ActionName = RegionalActionName(Option.ActionType);
			const FString SignedSupportDelta = FString::Printf(TEXT("%+d"), Option.SupportDelta);
			FString ActionDetail = Option.ActionType == ERegionalDiplomacyActionType::CrisisMobilization
				? LocalizedFormat(
					TEXT("strategic.region-action-crisis-detail-format"),
					TEXT("{0}  •  FUNDS {1}\nSUPPORT {2} • PRESSURE -{3} • THRESHOLD {4}"),
					{
						ActionName,
						LexToString(Option.Cost),
						SignedSupportDelta,
						FString::FromInt(Option.PressureReduction),
						FString::FromInt(Option.MinimumPressure)
					})
				: LocalizedFormat(
					TEXT("strategic.region-action-detail-format"),
					TEXT("{0}  •  FUNDS {1} • SUPPORT {2} • PRESSURE -{3}"),
					{
						ActionName,
						LexToString(Option.Cost),
						SignedSupportDelta,
						FString::FromInt(Option.PressureReduction)
					});
			if (Option.bWouldWithdrawCompactMember)
			{
				ActionDetail += TEXT("\n") + Localized(
					TEXT("strategic.coalition-withdrawal-warning"),
					TEXT("WARNING: THIS SUPPORT LOSS WILL WITHDRAW A COMPACT MEMBER"));
			}
			const FString DisabledReason = Option.UnavailableReasonCode.IsNone()
				? FString()
				: LocalizedDiagnostic(Option.UnavailableReasonCode, Option.UnavailableReason);
			const FString ButtonLabel = Option.bEnabled || DisabledReason.IsEmpty()
				? ActionDetail
				: LocalizedFormat(
					TEXT("strategic.region-action-unavailable-format"), TEXT("{0}\n{1}"),
					{ ActionDetail, DisabledReason });
			RenderedCommandActionLabels.Add(ButtonLabel);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SNew(SButton)
				.IsEnabled(Option.bEnabled)
				.IsFocusable(true)
				.ButtonColorAndOpacity(Option.ActionType == ERegionalDiplomacyActionType::CivicRelief
					? FLinearColor(0.0f, 0.30f, 0.34f, 1.0f)
					: Option.ActionType == ERegionalDiplomacyActionType::SecurityAccord
						? FLinearColor(0.03f, 0.20f, 0.42f, 1.0f)
						: FLinearColor(0.42f, 0.14f, 0.04f, 1.0f))
				.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleRegionalDiplomacyClicked,
					Region.RegionId, Option.ActionType)
				[
					MakeText(ButtonLabel, 10, Option.bEnabled ? PrimaryText : SecondaryText)
				]
			];
		}
		if (Region.bHasMandate)
		{
		const FStrategicRegionalCharterView& Charter = Region.ResilienceCharter;
		const FString CharterName = Localized(
			TEXT("strategic.region-charter-resilience"), TEXT("RESILIENCE CHARTER"));
		const FString CharterDetail = Charter.bSigned
			? LocalizedFormat(
				TEXT("strategic.region-charter-active-format"),
				TEXT("{0}  •  ACTIVE\nFUNDING {1}% • MISSION WEIGHT {2}% • ESCAPE PRESSURE {3}%"),
				{
					CharterName,
					FString::FromInt(Charter.FundingPercent),
					FString::FromInt(Charter.MissionWeightPercent),
					FString::FromInt(Charter.EscapePressurePercent)
				})
			: LocalizedFormat(
				TEXT("strategic.region-charter-detail-format"),
				TEXT("{0}  •  FUNDS {1} • SUPPORT -{2} • MIN {3}\nFUNDING {4} → {5} • MISSION WEIGHT {6}% • ESCAPE PRESSURE {7}%"),
				{
					CharterName,
					LexToString(Charter.Cost),
					FString::FromInt(Charter.SupportCost),
					FString::FromInt(Charter.MinimumSupport),
					LexToString(Region.CurrentMonthlyFunding),
					LexToString(Charter.ProjectedMonthlyFunding),
					FString::FromInt(Charter.MissionWeightPercent),
					FString::FromInt(Charter.EscapePressurePercent)
				});
		const FString CharterDisabledReason = Charter.bSigned || Charter.UnavailableReasonCode.IsNone()
			? FString()
			: LocalizedDiagnostic(Charter.UnavailableReasonCode, Charter.UnavailableReason);
		const FString CharterButtonLabel = CharterDisabledReason.IsEmpty()
			? CharterDetail
			: LocalizedFormat(
				TEXT("strategic.region-action-unavailable-format"), TEXT("{0}\n{1}"),
				{ CharterDetail, CharterDisabledReason });
		RenderedCommandActionLabels.Add(CharterButtonLabel);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 5.0f)
		[
			SNew(SButton)
				.IsEnabled(Charter.bEnabled)
				.IsFocusable(true)
				.ButtonColorAndOpacity(Charter.bSigned
					? FLinearColor(0.05f, 0.40f, 0.27f, 1.0f)
					: FLinearColor(0.18f, 0.22f, 0.40f, 1.0f))
				.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleRegionalCharterClicked,
					Region.RegionId)
			[
				MakeText(CharterButtonLabel, 10,
					Charter.bSigned ? Success : (Charter.bEnabled ? PrimaryText : SecondaryText))
			]
		];
		const FStrategicCompactRestorationView& Restoration =
			Region.HorizonCompactRestoration;
		if (Restoration.bWithdrawn)
		{
			const FStrategicCompactEmergencyVoteView& Vote =
				Region.HorizonCompactEmergencyVote;
			auto BallotNames = [this](const TArray<FName>& RegionIds)
			{
				TArray<FString> Names;
				for (const FName RegionId : RegionIds)
				{
					const FStrategicRegionView* BallotRegion =
						CurrentSnapshot.Regions.FindByPredicate(
							[RegionId](const FStrategicRegionView& Candidate)
							{
								return Candidate.RegionId == RegionId;
							});
					Names.Add(LocalizedContentName(
						RegionId,
						BallotRegion != nullptr
							? BallotRegion->DisplayName
							: RegionId.ToString()).ToUpper());
				}
				return Names.IsEmpty() ? FString(TEXT("—")) : FString::Join(Names, TEXT(", "));
			};
			const FString VoteName = Localized(
				TEXT("strategic.coalition-emergency-vote"),
				TEXT("EMERGENCY SOLIDARITY VOTE"));
			const FString Ballots = LocalizedFormat(
				TEXT("strategic.coalition-emergency-vote-ballots-format"),
				TEXT("FOR {0} • AGAINST {1}"),
				{
					BallotNames(Vote.SupportingMemberRegionIds),
					BallotNames(Vote.OpposingMemberRegionIds)
				});
			const FString VoteDetail = LocalizedFormat(
				TEXT("strategic.coalition-emergency-vote-detail-format"),
				TEXT("{0}  •  FUNDS {1} • YES {2}/{3}\nSUPPORT {4} → {5} • PRESSURE {6} → {7}\n{8}\nYES MEMBERS -{9} SUPPORT • LIMIT {10} PRESSURE • FUNDING {11}"),
				{
					VoteName,
					LexToString(Vote.Cost),
					FString::FromInt(Vote.SupportingMemberRegionIds.Num()),
					FString::FromInt(Vote.RequiredVotes),
					FString::FromInt(Vote.TargetCurrentSupport),
					FString::FromInt(Vote.TargetProjectedSupport),
					FString::FromInt(Vote.TargetCurrentPressure),
					FString::FromInt(Vote.TargetProjectedPressure),
					Ballots,
					FString::FromInt(Vote.VoterSupportCost),
					FString::FromInt(Vote.MaximumVoterPressure),
					FString::Printf(TEXT("%+lld"), Vote.MonthlyFundingDelta)
				});
			const FString VoteDisabledReason = Vote.bEnabled
				|| Vote.UnavailableReasonCode.IsNone()
				? FString()
				: LocalizedDiagnostic(
					Vote.UnavailableReasonCode,
					Vote.UnavailableReason);
			const FString VoteButtonLabel = VoteDisabledReason.IsEmpty()
				? VoteDetail
				: LocalizedFormat(
					TEXT("strategic.region-action-unavailable-format"), TEXT("{0}\n{1}"),
					{ VoteDetail, VoteDisabledReason });
			RenderedCommandActionLabels.Add(VoteButtonLabel);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 5.0f)
			[
				SAssignNew(CoalitionEmergencyVotePanelAnchor, SButton)
					.IsEnabled(Vote.bEnabled)
					.IsFocusable(true)
					.ButtonColorAndOpacity(Vote.bEnabled
						? FLinearColor(0.34f, 0.20f, 0.48f, 1.0f)
						: FLinearColor(0.20f, 0.14f, 0.24f, 1.0f))
					.OnClicked_UObject(
						this,
						&UUEGTStrategicHudWidget::HandleCompactEmergencyVoteClicked,
						Region.RegionId)
				[
					MakeText(VoteButtonLabel, 8, Vote.bEnabled ? PrimaryText : SecondaryText)
				]
			];

			const FString RestorationName = Localized(
				TEXT("strategic.coalition-restoration"),
				TEXT("RESTORE COMPACT MEMBERSHIP"));
			const FString RestorationDetail = LocalizedFormat(
				TEXT("strategic.coalition-restoration-detail-format"),
				TEXT("{0}  •  FUNDS {1} • SUPPORT {2}/{3}\nTOTAL FUNDING {4} → {5}"),
				{
					RestorationName,
					LexToString(Restoration.Cost),
					FString::FromInt(Restoration.CurrentSupport),
					FString::FromInt(Restoration.MinimumSupport),
					LexToString(Restoration.CurrentMonthlyFunding),
					LexToString(Restoration.ProjectedMonthlyFunding)
				});
			const FString RestorationDisabledReason = Restoration.bEnabled
				|| Restoration.UnavailableReasonCode.IsNone()
				? FString()
				: LocalizedDiagnostic(
					Restoration.UnavailableReasonCode,
					Restoration.UnavailableReason);
			const FString RestorationButtonLabel = RestorationDisabledReason.IsEmpty()
				? RestorationDetail
				: LocalizedFormat(
					TEXT("strategic.region-action-unavailable-format"), TEXT("{0}\n{1}"),
					{ RestorationDetail, RestorationDisabledReason });
			RenderedCommandActionLabels.Add(RestorationButtonLabel);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 5.0f)
			[
				SNew(SButton)
					.IsEnabled(Restoration.bEnabled)
					.IsFocusable(true)
					.ButtonColorAndOpacity(Restoration.bEnabled
						? FLinearColor(0.20f, 0.36f, 0.12f, 1.0f)
						: FLinearColor(0.34f, 0.12f, 0.08f, 1.0f))
					.OnClicked_UObject(
						this,
						&UUEGTStrategicHudWidget::HandleCompactRestorationClicked,
						Region.RegionId)
				[
					MakeText(
						RestorationButtonLabel, 9,
						Restoration.bEnabled ? PrimaryText : Warning)
				]
			];
		}
		}
	}
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
	[
		MakeText(Localized(TEXT("strategic.globe-rings-legend"),
			TEXT("GLOBE RINGS  DOT <30  •  TILE 30-69  •  BAR 70+")), 10, SecondaryText)
	];

	if (!CurrentSnapshot.BaseAssaults.IsEmpty())
	{
		const FString BaseDefenseAlertLabel = Localized(
			TEXT("strategic.base-defense-alert"), TEXT("BASE DEFENSE ALERT"));
		RenderedDynamicLabels.Add(BaseDefenseAlertLabel);
		BaseDefensePanelAnchor = MakeText(
			BaseDefenseAlertLabel, 16, FLinearColor(1.0f, 0.22f, 0.16f, 1.0f), true);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 6.0f)
		[
			BaseDefensePanelAnchor.ToSharedRef()
		];
		for (const FStrategicBaseAssaultView& Assault : CurrentSnapshot.BaseAssaults)
		{
			const FString MissionName = LocalizedContentName(Assault.MissionRuleId, Assault.MissionName);
			const FString ContactName = LocalizedContentName(Assault.ContactRuleId, Assault.ContactName);
			TArray<FString> DefenseSupplyLines;
			for (const FStrategicBaseDefenseSupplyView& Supply : Assault.DefenseSupplies)
			{
				DefenseSupplyLines.Add(LocalizedFormat(
					TEXT("strategic.defense-supply-stock-format"),
					TEXT("{0}  •  STOCK {1}/{2}  •  ALLOCATED {3}"),
					{
						LocalizedContentName(Supply.ItemId, Supply.DisplayName).ToUpper(),
						FString::FromInt(Supply.AvailableQuantity),
						FString::FromInt(Supply.RequiredQuantity),
						FString::FromInt(Supply.AllocatedQuantity)
					}));
			}
			const FString DefenseReadiness = Assault.DefenseBatteryCount > 0
				? (Assault.ReadyDefenseBatteryCount == Assault.DefenseBatteryCount
					? LocalizedFormat(
						Assault.DefenseBatteryCount == 1
							? TEXT("strategic.defense-battery-readiness-one-format")
							: TEXT("strategic.defense-battery-readiness-many-format"),
						Assault.DefenseBatteryCount == 1
							? TEXT("{0} BATTERY  •  UP TO {1} DAMAGE  •  ~{2} EXPECTED")
							: TEXT("{0} BATTERIES  •  UP TO {1} DAMAGE  •  ~{2} EXPECTED"),
						{
							FString::FromInt(Assault.DefenseBatteryCount),
							FString::FromInt(Assault.MaximumDefenseDamage),
							FString::FromInt(Assault.ExpectedDefenseDamage)
						})
					: LocalizedFormat(
						Assault.DefenseBatteryCount == 1
							? TEXT("strategic.defense-battery-supply-readiness-one-format")
							: TEXT("strategic.defense-battery-supply-readiness-many-format"),
						Assault.DefenseBatteryCount == 1
							? TEXT("{0}/{1} BATTERY SUPPLIED  •  UP TO {2} DAMAGE  •  ~{3} EXPECTED")
							: TEXT("{0}/{1} BATTERIES SUPPLIED  •  UP TO {2} DAMAGE  •  ~{3} EXPECTED"),
						{
							FString::FromInt(Assault.ReadyDefenseBatteryCount),
							FString::FromInt(Assault.DefenseBatteryCount),
							FString::FromInt(Assault.MaximumDefenseDamage),
							FString::FromInt(Assault.ExpectedDefenseDamage)
						}))
				: Localized(
					TEXT("strategic.no-operational-defense-batteries"),
					TEXT("NO OPERATIONAL DEFENSE BATTERIES"));
			const FString DefenseReadinessWithSupply = DefenseSupplyLines.IsEmpty()
				? DefenseReadiness
				: DefenseReadiness + TEXT("\n") + FString::Join(DefenseSupplyLines, TEXT("\n"));
			const FString GroundReadiness = Assault.bTacticalDefensePrepared
				? LocalizedFormat(
					Assault.DefenderCount == 1
						? TEXT("strategic.ground-team-committed-one-format")
						: TEXT("strategic.ground-team-committed-many-format"),
					Assault.DefenderCount == 1
						? TEXT("GROUND TEAM COMMITTED  •  {0} DEFENDER")
						: TEXT("GROUND TEAM COMMITTED  •  {0} DEFENDERS"),
					{ FString::FromInt(Assault.DefenderCount) })
				: (Assault.bCanDeployTacticalDefense
					? LocalizedFormat(
						Assault.DefenderCount == 1
							? TEXT("strategic.ground-team-ready-one-format")
							: TEXT("strategic.ground-team-ready-many-format"),
						Assault.DefenderCount == 1
							? TEXT("GROUND TEAM READY  •  {0} DEFENDER")
							: TEXT("GROUND TEAM READY  •  {0} DEFENDERS"),
						{ FString::FromInt(Assault.DefenderCount) })
					: Localized(
						TEXT("strategic.ground-team-unavailable"),
						TEXT("GROUND TEAM UNAVAILABLE")));
			const FString BaseAssaultCard = LocalizedFormat(
				TEXT("strategic.base-assault-card-format"),
				TEXT("{0}  //  {1}\n{2}  •  THREAT {3}  •  HULL {4}\n{5}\n{6}\nBREACH RISK  {7} DAMAGE × UP TO {8} FACILITIES"),
				{
					Assault.BaseName.ToUpper(), MissionName.ToUpper(), ContactName.ToUpper(),
					FString::FromInt(Assault.ThreatRating), FString::FromInt(Assault.ContactHull),
					DefenseReadinessWithSupply, GroundReadiness,
					FString::FromInt(Assault.BreachDamagePerFacility),
					FString::FromInt(Assault.MaximumFacilitiesHit)
				});
			RenderedDynamicLabels.Add(BaseAssaultCard);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				MakeText(BaseAssaultCard, 11, Warning, true)
			];
			const FString GroundDefenseTooltip = Assault.bTacticalDefensePrepared
				? Localized(
					TEXT("strategic.enter-prepared-ground-defense-tooltip"),
					TEXT("Enter the prepared base-defense battlefield."))
				: (Assault.bCanDeployTacticalDefense
					? Localized(
						TEXT("strategic.deploy-ground-defense-tooltip"),
						TEXT("Commit all available unassigned field agents at this base and enter tactical command."))
					: LocalizedDiagnostic(
						Assault.TacticalUnavailableReasonCode,
						Assault.TacticalUnavailableReason));
			const FString GroundDefenseLabel = Assault.bTacticalDefensePrepared
				? Localized(TEXT("strategic.enter-ground-defense"), TEXT("ENTER GROUND DEFENSE"))
				: Localized(TEXT("strategic.deploy-ground-defense"), TEXT("DEPLOY GROUND DEFENSE"));
			RenderedDynamicLabels.Add(GroundDefenseTooltip);
			RenderedDynamicLabels.Add(GroundDefenseLabel);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SNew(SButton)
				.IsEnabled(Assault.bTacticalDefensePrepared
					? Assault.TacticalOperationId.IsValid()
					: Assault.bCanDeployTacticalDefense)
				.IsFocusable(true)
				.ButtonColorAndOpacity(FLinearColor(0.04f, 0.32f, 0.38f, 1.0f))
				.ToolTipText(FText::FromString(GroundDefenseTooltip))
				.OnClicked_UObject(
					this,
					Assault.bTacticalDefensePrepared
						? &UUEGTStrategicHudWidget::HandleOperationClicked
						: &UUEGTStrategicHudWidget::HandleDeployBaseDefenseClicked,
					Assault.bTacticalDefensePrepared ? Assault.TacticalOperationId : Assault.AssaultId)
				[
					MakeText(GroundDefenseLabel, 12, PrimaryText, true)
				]
			];
			if (Assault.FireDoctrines.IsEmpty())
			{
				const FString BatteryDefenseTooltip = Assault.bCanResolve
					? Localized(
						TEXT("strategic.fire-defense-batteries-tooltip"),
						TEXT("Fire every supplied defense battery once, consuming its authored load. A surviving contact breaches the base and damages distinct facilities."))
					: LocalizedDiagnostic(Assault.UnavailableReasonCode, Assault.UnavailableReason);
				const FString BatteryDefenseLabel = Localized(
					TEXT("strategic.fire-defense-batteries"), TEXT("FIRE DEFENSE BATTERIES"));
				RenderedDynamicLabels.Add(BatteryDefenseTooltip);
				RenderedDynamicLabels.Add(BatteryDefenseLabel);
				RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 5.0f)
				[
					SNew(SButton)
					.IsEnabled(Assault.bCanResolve)
					.IsFocusable(true)
					.ButtonColorAndOpacity(FLinearColor(0.62f, 0.08f, 0.04f, 1.0f))
					.ToolTipText(FText::FromString(BatteryDefenseTooltip))
					.OnClicked_UObject(
						this,
						&UUEGTStrategicHudWidget::HandleResolveBaseAssaultClicked,
						Assault.AssaultId,
						EBaseDefenseFireDoctrine::CoordinatedLine)
					[
						MakeText(BatteryDefenseLabel, 12, PrimaryText, true)
					]
				];
			}
			else
			{
				const FString DoctrineHeading = Localized(
					TEXT("strategic.base-defense-doctrine-heading"),
					TEXT("FIRE DOCTRINE  //  SELECT ONE VOLLEY"));
				const FString DoctrineGuidance = Localized(
					TEXT("strategic.base-defense-doctrine-guidance"),
					TEXT("Supply is allocated in doctrine order; batteries cease fire when the contact is destroyed."));
				RenderedDynamicLabels.Add(DoctrineHeading);
				RenderedDynamicLabels.Add(DoctrineGuidance);
				RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 1.0f)
				[
					MakeText(DoctrineHeading, 10, Accent, true)
				];
				RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
				[
					MakeText(DoctrineGuidance, 9, SecondaryText)
				];

				TSharedRef<SGridPanel> DoctrineGrid = SNew(SGridPanel);
				int32 DoctrineIndex = 0;
				for (const FStrategicBaseDefenseDoctrineView& Option : Assault.FireDoctrines)
				{
					FString Detail = LocalizedFormat(
						TEXT("strategic.base-defense-doctrine-detail-format"),
						TEXT("{0}/{1} READY  •  MAX {2}  •  ~{3}"),
						{
							FString::FromInt(Option.ReadyDefenseBatteryCount),
							FString::FromInt(Option.DefenseBatteryCount),
							FString::FromInt(Option.MaximumDefenseDamage),
							FString::FromInt(Option.ExpectedDefenseDamage)
						});
					if (Option.FundingCost > 0)
					{
						Detail += TEXT("\n") + LocalizedFormat(
							TEXT("strategic.base-defense-doctrine-cost-format"),
							TEXT("EMERGENCY GRID COST {0}"),
							{ FString::Printf(TEXT("%lld"), Option.FundingCost) });
					}
					const FString ButtonLabel = BaseDefenseDoctrineName(Option.Doctrine) + TEXT("\n") + Detail;
					FString Tooltip = Option.bCanResolve
						? BaseDefenseDoctrineTooltip(
							Option.Doctrine,
							Option.Summary,
							Option.AccuracyBonus,
							Option.DamagePercent)
						: LocalizedDiagnostic(Option.UnavailableReasonCode, Option.UnavailableReason);
					for (const FStrategicBaseDefenseSupplyView& Supply : Option.DefenseSupplies)
					{
						Tooltip += TEXT("\n") + LocalizedFormat(
							TEXT("strategic.defense-supply-stock-format"),
							TEXT("{0}  •  STOCK {1}/{2}  •  ALLOCATED {3}"),
							{
								LocalizedContentName(Supply.ItemId, Supply.DisplayName).ToUpper(),
								FString::FromInt(Supply.AvailableQuantity),
								FString::FromInt(Supply.RequiredQuantity),
								FString::FromInt(Supply.AllocatedQuantity)
							});
					}
					RenderedDynamicLabels.Add(ButtonLabel);
					RenderedDynamicLabels.Add(Tooltip);
					const FLinearColor ButtonColor = Option.Doctrine == EBaseDefenseFireDoctrine::GridOvercharge
						? FLinearColor(0.42f, 0.08f, 0.52f, 1.0f)
						: (Option.Doctrine == EBaseDefenseFireDoctrine::PrecisionScreen
							? FLinearColor(0.04f, 0.32f, 0.48f, 1.0f)
							: (Option.Doctrine == EBaseDefenseFireDoctrine::BreachBreaker
								? FLinearColor(0.62f, 0.08f, 0.04f, 1.0f)
								: FLinearColor(0.38f, 0.22f, 0.04f, 1.0f)));
					DoctrineGrid->AddSlot(DoctrineIndex % 2, DoctrineIndex / 2).Padding(0.0f, 0.0f, 3.0f, 3.0f)
					[
						SNew(SButton)
						.IsEnabled(Option.bCanResolve)
						.IsFocusable(true)
						.ButtonColorAndOpacity(ButtonColor)
						.ToolTipText(FText::FromString(Tooltip))
						.OnClicked_UObject(
							this,
							&UUEGTStrategicHudWidget::HandleResolveBaseAssaultClicked,
							Assault.AssaultId,
							Option.Doctrine)
						[
							MakeText(ButtonLabel, 9, PrimaryText, true)
						]
					];
					++DoctrineIndex;
				}
				RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
				[
					DoctrineGrid
				];
			}
		}
	}

	if (!CurrentSnapshot.Contacts.IsEmpty())
	{
		const FString ContactsLabel = Localized(
			TEXT("strategic.detected-contacts"), TEXT("DETECTED CONTACTS"));
		RenderedDynamicLabels.Add(ContactsLabel);
		ContactPanelAnchor = MakeText(ContactsLabel, 16, Warning, true);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 6.0f)
		[
			ContactPanelAnchor.ToSharedRef()
		];
		for (const FStrategicContactView& Contact : CurrentSnapshot.Contacts)
		{
			const FString TargetSuffix = Contact.bTargetsBase
				? LocalizedFormat(
					TEXT("strategic.contact-target-format"), TEXT("\nTARGET  {0}"),
					{ Contact.TargetBaseName.ToUpper() })
				: FString();
			const FString ContactCard = LocalizedFormat(
				TEXT("strategic.contact-card-format"),
				TEXT("{0}  •  {1}\nTHREAT {2}   HULL {3}/{4}   ROUTE {5}%{6}"),
				{
					LocalizedContactName(Contact).ToUpper(),
					ContactStatusLabel(Contact),
					FString::FromInt(Contact.ThreatRating),
					FString::FromInt(Contact.CurrentHull),
					FString::FromInt(Contact.MaxHull),
					FString::FromInt(FMath::RoundToInt(Contact.RouteProgress * 100.0f)),
					TargetSuffix
				});
			RenderedDynamicLabels.Add(ContactCard);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				MakeText(ContactCard, 12, Warning)
			];
			if (!Contact.PlanId.IsNone())
			{
				const FString PlanEnds = Localized(
					TEXT("strategic.adversary-plan-ends"), TEXT("PATTERN ENDS"));
				const FString EscapeOutcome = Contact.EscapeBranchMissionRuleId.IsNone()
					? PlanEnds
					: FText::FromString(LocalizedContentName(
						Contact.EscapeBranchMissionRuleId, Contact.EscapeBranchMissionName)).ToUpper().ToString();
				const FString ThwartOutcome = Contact.ThwartBranchMissionRuleId.IsNone()
					? PlanEnds
					: FText::FromString(LocalizedContentName(
						Contact.ThwartBranchMissionRuleId, Contact.ThwartBranchMissionName)).ToUpper().ToString();
				const FString PlanCard = LocalizedFormat(
					TEXT("strategic.adversary-plan-format"),
					TEXT("ADVERSARY PLAN  //  {0}  •  STAGE {1}\nIF ESCAPED → {2}\nIF THWARTED → {3}"),
					{
						FText::FromString(LocalizedContentName(
							Contact.PlanId, Contact.PlanDisplayName)).ToUpper().ToString(),
						FString::FromInt(Contact.PlanStage),
						EscapeOutcome,
						ThwartOutcome
					});
				++RenderedAdversaryPlanIntelligenceCount;
				RenderedDynamicLabels.Add(PlanCard);
				RightBox->AddSlot().AutoHeight().Padding(8.0f, 1.0f, 0.0f, 5.0f)
				[
					MakeText(PlanCard, 10, SecondaryText)
				];
			}
			if (Contact.bHasCoalitionCounterplay)
			{
				auto RegionName = [this](const FName RegionId)
				{
					const FStrategicRegionView* Region = CurrentSnapshot.Regions.FindByPredicate(
						[RegionId](const FStrategicRegionView& View)
						{
							return View.RegionId == RegionId;
						});
					return LocalizedContentName(
						RegionId,
						Region != nullptr ? Region->DisplayName : RegionId.ToString()).ToUpper();
				};
				auto FormatCounterplayMembers = [this, &RegionName](
					const TArray<FStrategicCoalitionCounterplayMemberView>& Members,
					const bool bEscape)
				{
					if (Members.IsEmpty())
					{
						return Localized(
							TEXT("strategic.coalition-counterplay-none"),
							TEXT("NO ELIGIBLE MEMBERS"));
					}
					TArray<FString> Rows;
					for (const FStrategicCoalitionCounterplayMemberView& Member : Members)
					{
						const FString Suffix = bEscape && Member.bWouldWithdraw
							? Localized(
								TEXT("strategic.coalition-counterplay-withdrawal"),
								TEXT(" • WITHDRAWS"))
							: !bEscape && Member.bRemainsWithdrawn
								? Localized(
									TEXT("strategic.coalition-counterplay-remains-withdrawn"),
									TEXT(" • REMAINS WITHDRAWN"))
								: FString();
						Rows.Add(LocalizedFormat(
							TEXT("strategic.coalition-counterplay-member-format"),
							TEXT("{0} {1}→{2}{3}"),
							{
								RegionName(Member.RegionId),
								FString::FromInt(Member.CurrentSupport),
								FString::FromInt(Member.ProjectedSupport),
								Suffix
							}));
					}
					return FString::Join(Rows, TEXT("  /  "));
				};
				const FString CounterplayCard = LocalizedFormat(
					TEXT("strategic.coalition-counterplay-format"),
					TEXT("COALITION COUNTERPLAY\nESCAPE → {0}\nTHWART → {1}"),
					{
						FormatCounterplayMembers(Contact.EscapeStrainMembers, true),
						FormatCounterplayMembers(Contact.ThwartRecoveryMembers, false)
					});
				++RenderedCoalitionCounterplayCount;
				RenderedDynamicLabels.Add(CounterplayCard);
				RightBox->AddSlot().AutoHeight().Padding(8.0f, 1.0f, 0.0f, 5.0f)
				[
					MakeText(CounterplayCard, 10, Warning)
				];
			}
			if (Contact.bCanShadowToLanding)
			{
				const FString LandingChoice = LocalizedFormat(
					TEXT("strategic.contact-landing-choice-format"),
					TEXT("OUTCOME INTELLIGENCE\nDESTROY → WRECKAGE • THREAT {0} • {1}\nTRACK TO ARRIVAL → INTACT LANDING • THREAT {2} • {3}\nARRIVAL APPLIES MISSION CONSEQUENCES"),
					{
						FString::FromInt(Contact.ThreatRating),
						LocalizedDuration(Contact.WreckageSiteLifetimeSeconds),
						FString::FromInt(Contact.LandingSiteThreatRating),
						LocalizedDuration(Contact.LandingSiteLifetimeSeconds)
					});
				RenderedDynamicLabels.Add(LandingChoice);
				RightBox->AddSlot().AutoHeight().Padding(8.0f, 1.0f, 0.0f, 5.0f)
				[
					MakeText(LandingChoice, 10, Warning)
				];
			}
			if (Contact.bAssaultPending)
			{
				const FString PerimeterLabel = Localized(
					TEXT("strategic.contact-perimeter-decision"),
					TEXT("PERIMETER DEFENSE DECISION REQUIRED ABOVE"));
				RenderedDynamicLabels.Add(PerimeterLabel);
				RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 5.0f)
				[
					MakeText(PerimeterLabel, 10,
						FLinearColor(1.0f, 0.22f, 0.16f, 1.0f), true)
				];
			}
			else if (Contact.StatusType == EStrategicContactStatus::Engaged)
			{
				if (Contact.InterceptionCoordination.bValid)
				{
					const FString CoordinationHeading = Localized(
						TEXT("strategic.interception-coordination-heading"),
						TEXT("FORMATION LINK  //  AUTOMATIC"));
					const FString CoordinationDetail = LocalizedFormat(
						TEXT("strategic.interception-coordination-detail-format"),
						TEXT("{0}  •  SUPPORT {1}  •  FIRE {2}  •  RETURN {3}"),
						{
							InterceptionCoordinationName(Contact.InterceptionCoordination.bActive),
							FString::FromInt(Contact.InterceptionCoordination.SupportingCraftCount),
							SignedAccuracyModifier(Contact.InterceptionCoordination.OutgoingAccuracyModifier),
							SignedAccuracyModifier(Contact.InterceptionCoordination.IncomingAccuracyModifier)
						});
					const FString CoordinationTooltip = Localized(
						TEXT("strategic.interception-coordination-tooltip"),
						TEXT("Each craft beyond the lead adds +5 fire accuracy and -5 return-fire accuracy, capped at 15. Coordination consumes no extra random draw."));
					RenderedDynamicLabels.Add(CoordinationHeading);
					RenderedDynamicLabels.Add(CoordinationDetail);
					TSharedRef<STextBlock> CoordinationText = MakeText(
						CoordinationHeading + TEXT("\n") + CoordinationDetail,
						9,
						Contact.InterceptionCoordination.bActive ? Warning : SecondaryText,
						Contact.InterceptionCoordination.bActive);
					CoordinationText->SetToolTipText(FText::FromString(CoordinationTooltip));
					RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 4.0f)
					[
						CoordinationText
					];
				}
				if (Contact.InterceptionContactManeuver.bValid)
				{
					const FString ManeuverHeading = Localized(
						TEXT("strategic.interception-contact-maneuver-heading"),
						TEXT("CONTACT MANEUVER  //  AUTOMATIC"));
					const FString ManeuverDetail = LocalizedFormat(
						TEXT("strategic.interception-contact-maneuver-detail-format"),
						TEXT("{0}  •  ROUNDS {1}  •  FIRE {2}  •  RETURN {3}"),
						{
							InterceptionContactManeuverName(
								Contact.InterceptionContactManeuver.Maneuver),
							FString::FromInt(
								Contact.InterceptionContactManeuver.CompletedCombatRounds),
							SignedAccuracyModifier(
								Contact.InterceptionContactManeuver.OutgoingAccuracyModifier),
							SignedAccuracyModifier(
								Contact.InterceptionContactManeuver.IncomingAccuracyModifier)
						});
					const FString ManeuverTooltip = Localized(
						TEXT("strategic.interception-contact-maneuver-tooltip"),
						TEXT("The contact begins with Vector Survey, enters Signal Shear after two completed rounds while above 35% hull, and switches to Breakline Counter at 35% hull or lower. Maneuver selection consumes no extra random draw."));
					RenderedDynamicLabels.Add(ManeuverHeading);
					RenderedDynamicLabels.Add(ManeuverDetail);
					const FLinearColor ManeuverColor =
						Contact.InterceptionContactManeuver.Maneuver
							== EInterceptionContactManeuver::BreaklineCounter
						? FLinearColor(1.0f, 0.28f, 0.14f, 1.0f)
						: (Contact.InterceptionContactManeuver.Maneuver
								== EInterceptionContactManeuver::SignalShear
							? Accent
							: SecondaryText);
					TSharedRef<STextBlock> ManeuverText = MakeText(
						ManeuverHeading + TEXT("\n") + ManeuverDetail,
						9,
						ManeuverColor,
						Contact.InterceptionContactManeuver.Maneuver
							!= EInterceptionContactManeuver::VectorSurvey);
					ManeuverText->SetToolTipText(FText::FromString(ManeuverTooltip));
					RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						ManeuverText
					];
				}

				const FString PostureHeading = Localized(
					TEXT("strategic.interception-posture-heading"),
					TEXT("ENGAGEMENT GEOMETRY  //  SELECT ONE ROUND"));
				const FString PostureGuidance = Localized(
					TEXT("strategic.interception-posture-guidance"),
					TEXT("Accuracy modifiers apply before the 5–100% safety clamp."));
				RenderedDynamicLabels.Add(PostureHeading);
				RenderedDynamicLabels.Add(PostureGuidance);
				RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 1.0f)
				[
					MakeText(PostureHeading, 10, Accent, true)
				];
				RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
				[
					MakeText(PostureGuidance, 9, SecondaryText)
				];

				TSharedRef<SHorizontalBox> PostureRow = SNew(SHorizontalBox);
				for (const FStrategicInterceptionPostureView& Option : Contact.InterceptionPostures)
				{
					const FString Detail = LocalizedFormat(
						TEXT("strategic.interception-posture-detail-format"),
						TEXT("OUTGOING {0}  •  INCOMING {1}"),
						{
							SignedAccuracyModifier(Option.OutgoingAccuracyModifier),
							SignedAccuracyModifier(Option.IncomingAccuracyModifier)
						});
					const FString ButtonLabel = InterceptionPostureName(Option.Posture) + TEXT("\n") + Detail;
					const FString Tooltip = InterceptionPostureTooltip(Option.Posture, Option.Summary);
					RenderedDynamicLabels.Add(ButtonLabel);
					const FLinearColor ButtonColor = Option.Posture == EInterceptionPosture::StandOffScreen
						? FLinearColor(0.04f, 0.24f, 0.42f, 1.0f)
						: (Option.Posture == EInterceptionPosture::CloseAssault
							? FLinearColor(0.55f, 0.12f, 0.08f, 1.0f)
							: FLinearColor(0.04f, 0.36f, 0.34f, 1.0f));
					PostureRow->AddSlot().FillWidth(1.0f).Padding(0.0f, 0.0f, 3.0f, 0.0f)
					[
						SNew(SButton)
						.IsFocusable(true)
						.ButtonColorAndOpacity(ButtonColor)
						.ToolTipText(FText::FromString(Tooltip))
						.OnClicked_UObject(
							this,
							&UUEGTStrategicHudWidget::HandleResolveInterceptionClicked,
							Contact.ContactId,
							Option.Posture)
						[
							MakeText(ButtonLabel, 9, PrimaryText, true)
						]
					];
				}
				RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
				[
					PostureRow
				];

				const FString WithdrawalHeading = Localized(
					TEXT("strategic.interception-withdrawal-heading"),
					TEXT("WITHDRAWAL DOCTRINE  //  SELECT ONE COMMAND"));
				RenderedDynamicLabels.Add(WithdrawalHeading);
				RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
				[
					MakeText(WithdrawalHeading, 10, Accent, true)
				];
				TSharedRef<SWrapBox> WithdrawalRow = SNew(SWrapBox)
					.UseAllottedSize(true)
					.InnerSlotPadding(FVector2D(3.0f, 3.0f));
				for (const FStrategicInterceptionWithdrawalView& Option : Contact.InterceptionWithdrawals)
				{
					FString ButtonLabel;
					FString Tooltip;
					switch (Option.Doctrine)
					{
					case EInterceptionWithdrawalDoctrine::FormationBreak:
						ButtonLabel = LocalizedFormat(
							TEXT("strategic.interception-withdraw-format"),
							TEXT("BREAK CONTACT  •  FORMATION {0}"),
							{ FString::FromInt(Option.OnStationCraftCount) });
						Tooltip = Localized(
							TEXT("strategic.interception-withdraw-tooltip"),
							TEXT("Return every on-station craft without firing or consuming a combat random draw. The contact remains detected."));
						break;
					case EInterceptionWithdrawalDoctrine::EvasiveRelay:
						ButtonLabel = LocalizedFormat(
							TEXT("strategic.interception-relay-format"),
							TEXT("EVASIVE RELAY  •  {0}  •  HULL {1}/{2}"),
							{
								Option.PriorityCraftDisplayName.IsEmpty()
									? Option.DisplayName
									: Option.PriorityCraftDisplayName,
								FString::FromInt(Option.PriorityCraftCurrentHull),
								FString::FromInt(Option.PriorityCraftMaximumHull)
							});
						Tooltip = Localized(
							TEXT("strategic.interception-relay-tooltip"),
							TEXT("Return only the lowest-integrity craft without a combat round or random draw. Other craft hold the engagement."));
						break;
					case EInterceptionWithdrawalDoctrine::WakeSnare:
						ButtonLabel = LocalizedFormat(
							TEXT("strategic.interception-wake-snare-format"),
							TEXT("WAKE SNARE  •  ROUNDS {0}/{1}  •  DELAY {2}"),
							{
								FString::FromInt(Option.CompletedCombatRounds),
								FString::FromInt(Option.RequiredCombatRounds),
								CompactMinutesSeconds(Option.ContactRouteDelaySeconds)
							});
						Tooltip = Localized(
							TEXT("strategic.interception-wake-snare-tooltip"),
							TEXT("After two completed combat rounds, return the entire formation and rewind up to 30 minutes of the contact's route progress. No combat round or random draw is consumed."));
						break;
					default:
						ButtonLabel = Option.DisplayName;
						Tooltip = Option.Summary;
						break;
					}
					if (!Option.bEnabled && !Option.UnavailableReasonCode.IsNone())
					{
						Tooltip += TEXT("\n") + LocalizedDiagnostic(
							Option.UnavailableReasonCode, Option.UnavailableReason);
					}
					RenderedDynamicLabels.Add(ButtonLabel);
					const FLinearColor ButtonColor =
						Option.Doctrine == EInterceptionWithdrawalDoctrine::EvasiveRelay
							? FLinearColor(0.18f, 0.22f, 0.48f, 1.0f)
							: Option.Doctrine == EInterceptionWithdrawalDoctrine::WakeSnare
								? FLinearColor(0.03f, 0.32f, 0.25f, 1.0f)
								: FLinearColor(0.42f, 0.26f, 0.03f, 1.0f);
					WithdrawalRow->AddSlot()
					[
						SNew(SButton)
						.IsFocusable(true)
						.IsEnabled(Option.bEnabled)
						.ButtonColorAndOpacity(ButtonColor)
						.ToolTipText(FText::FromString(Tooltip))
						.OnClicked_UObject(
							this,
							&UUEGTStrategicHudWidget::HandleWithdrawInterceptionClicked,
							Contact.ContactId,
							Option.Doctrine)
						[
							MakeText(ButtonLabel, 9, PrimaryText, true)
						]
					];
				}
				RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
				[
					WithdrawalRow
				];
			}
			else
			{
				const FString DispatchLabel = Localized(
					TEXT("strategic.contact-dispatch-interceptor"),
					TEXT("DISPATCH READY INTERCEPTOR"));
				RenderedDynamicLabels.Add(DispatchLabel);
				RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 5.0f)
				[
					SNew(SButton)
					.IsFocusable(true)
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleDispatchContactClicked, Contact.ContactId)
					[
						MakeText(DispatchLabel, 11)
					]
				];
			}
		}
	}

	if (!CurrentSnapshot.Sites.IsEmpty())
	{
		const FString SitesLabel = Localized(
			TEXT("strategic.tactical-sites"), TEXT("TACTICAL SITES"));
		RenderedDynamicLabels.Add(SitesLabel);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 6.0f)
		[
			MakeText(SitesLabel, 16, Accent, true)
		];
		for (const FStrategicSiteView& Site : CurrentSnapshot.Sites)
		{
			const FString SiteCard = LocalizedFormat(
				TEXT("strategic.site-card-format"), TEXT("{0}\nTHREAT {1}   LIFETIME {2}"),
				{
					LocalizedSiteName(Site).ToUpper(),
					FString::FromInt(Site.ThreatRating),
					LocalizedDuration(Site.RemainingLifetimeSeconds)
				});
			RenderedDynamicLabels.Add(SiteCard);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				MakeText(SiteCard, 12)
			];
			const FString DeployLabel = Localized(
				TEXT("strategic.site-deploy-transport"), TEXT("DEPLOY READY TRANSPORT"));
			RenderedDynamicLabels.Add(DeployLabel);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 5.0f)
			[
				SNew(SButton)
				.IsFocusable(true)
				.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleDeploySiteClicked, Site.SiteId)
				[
					MakeText(DeployLabel, 11)
				]
			];
		}
	}

	if (!CurrentSnapshot.PendingOperationIds.IsEmpty())
	{
		const FString TacticalDecisionLabel = Localized(
			TEXT("strategic.tactical-decision"), TEXT("TACTICAL DECISION"));
		RenderedDynamicLabels.Add(TacticalDecisionLabel);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 6.0f)
		[
			MakeText(TacticalDecisionLabel, 16, Warning, true)
		];
		for (const FGuid OperationId : CurrentSnapshot.PendingOperationIds)
		{
			const FString BeginDeploymentLabel = Localized(
				TEXT("strategic.tactical-begin-deployment"), TEXT("BEGIN TACTICAL DEPLOYMENT"));
			RenderedDynamicLabels.Add(BeginDeploymentLabel);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
			[
				SNew(SButton)
				.IsEnabled(OperationId.IsValid())
				.IsFocusable(true)
				.ButtonColorAndOpacity(FLinearColor(0.55f, 0.12f, 0.08f, 1.0f))
				.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleOperationClicked, OperationId)
				[
					MakeText(BeginDeploymentLabel, 13, PrimaryText, true)
				]
			];
		}
	}

	const FString ActiveProgramsLabel = LocalizedFormat(
		TEXT("strategic.active-programs-format"),
		TEXT("ACTIVE PROGRAMS {0}"),
		{ FString::FromInt(CurrentSnapshot.Projects.Num()) });
	RenderedDynamicLabels.Add(ActiveProgramsLabel);
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 6.0f)
	[
		MakeText(ActiveProgramsLabel, 16, Accent, true)
	];
	if (CurrentSnapshot.Projects.IsEmpty())
	{
		const FString NoActiveProgramsLabel = Localized(
			TEXT("strategic.no-active-programs"), TEXT("No active programs"));
		RenderedDynamicLabels.Add(NoActiveProgramsLabel);
		RightBox->AddSlot().AutoHeight()[MakeText(NoActiveProgramsLabel, 12, SecondaryText)];
	}
	for (const FStrategicProjectView& Project : CurrentSnapshot.Projects)
	{
		const FString ProjectDisplayName = Project.Type == EStrategicProjectType::Recruitment
			|| Project.Type == EStrategicProjectType::CraftAcquisition
			? Project.DisplayName
			: LocalizedContentName(Project.RuleId, Project.DisplayName);
		const FString ProjectDetail = Project.Type == EStrategicProjectType::Research
			? LocalizedResearchProjectDetail(Project)
			: Project.Type == EStrategicProjectType::Manufacturing
				? LocalizedManufacturingProjectDetail(Project)
				: Project.Type == EStrategicProjectType::CraftAcquisition
					? LocalizedCraftProjectDetail(Project)
					: Project.Detail;
		const FString ProjectLabel = FString::Printf(TEXT("%s  //  %s\n%s   %d%%"),
			*ProjectTypeLabel(Project.Type), *ProjectDisplayName, *ProjectDetail,
			FMath::RoundToInt(Project.Progress * 100.0f));
		RenderedDynamicLabels.Add(ProjectLabel);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			MakeText(ProjectLabel, 12,
				Project.bPaused ? Warning : PrimaryText)
		];
		if (Project.Type == EStrategicProjectType::Research
			|| Project.Type == EStrategicProjectType::Manufacturing)
		{
			const FStrategicBaseView* Base = CurrentSnapshot.Bases.FindByPredicate(
				[&Project](const FStrategicBaseView& Entry) { return Entry.BaseId == Project.BaseId; });
			const bool bResearch = Project.Type == EStrategicProjectType::Research;
			const bool bCanAdd = Base != nullptr && (bResearch
				? Base->AssignedScientists < Base->ScientistCapacity
				: Base->AssignedEngineers < Base->EngineerCapacity);
			const FString StaffLabel = bResearch
				? Localized(TEXT("strategic.staff-scientists"), TEXT("SCIENTISTS"))
				: Localized(TEXT("strategic.staff-engineers"), TEXT("ENGINEERS"));
			const FString StaffCountLabel = FString::Printf(
				TEXT("%d %s"), Project.AssignedStaff, *StaffLabel);
			RenderedDynamicLabels.Add(StaffCountLabel);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 5.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.IsEnabled(Project.AssignedStaff > 0)
					.IsFocusable(true)
					.ToolTipText(FText::FromString(Localized(
						TEXT("strategic.staff-release-tooltip"),
						TEXT("Release one staff member from this program."))))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleProjectStaffClicked,
						Project.Type, Project.ProjectId, Project.RuleId, -1)
					[
						MakeText(TEXT("−"), 11, PrimaryText, true)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					MakeText(StaffCountLabel, 10, SecondaryText, true)
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.IsEnabled(bCanAdd)
					.IsFocusable(true)
					.ToolTipText(FText::FromString(bCanAdd
						? Localized(TEXT("strategic.staff-assign-tooltip"),
							TEXT("Assign one available staff member to this program."))
						: Localized(TEXT("strategic.staff-capacity-tooltip"),
							TEXT("No matching staff capacity is currently available at this base."))))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleProjectStaffClicked,
						Project.Type, Project.ProjectId, Project.RuleId, 1)
					[
						MakeText(TEXT("+"), 11, PrimaryText, true)
					]
				]
			];
			if (!bResearch)
			{
				const bool bCanAffordUnit = Project.UnitCost <= 0 || CurrentSnapshot.Funds >= Project.UnitCost;
				const bool bHasUnitMaterials = HasManufacturingMaterials(Project.MaterialRequirements, 1);
				const bool bHasUnitStorage = HasStorageForChange(Base, Project.StorageDeltaPerUnit);
				const bool bCanAddUnit = bCanAffordUnit && bHasUnitMaterials && bHasUnitStorage;
				const int64 RemoveUnitStorageDelta = Project.StorageDeltaPerUnit == MIN_int64
					? MAX_int64 : -Project.StorageDeltaPerUnit;
				const FString AddUnitTooltip = !bCanAffordUnit
					? Localized(
						TEXT("strategic.manufacturing-add-funds-tooltip"),
						TEXT("Current funds cannot reserve another unit."))
					: !bHasUnitMaterials
						? LocalizedFormat(
							TEXT("strategic.manufacturing-add-materials-tooltip-format"),
							TEXT("Another unit requires {0}."),
							{ ManufacturingMaterialSummary(Project.MaterialRequirements, 1) })
						: !bHasUnitStorage
							? StorageChangeUnavailableReason(Base, Project.StorageDeltaPerUnit)
						: Project.MaterialRequirements.IsEmpty()
							? Localized(
								TEXT("strategic.manufacturing-add-tooltip"),
								TEXT("Reserve one additional unit at the end of this production run."))
							: LocalizedFormat(
								TEXT("strategic.manufacturing-add-with-inputs-tooltip-format"),
								TEXT("Reserve one additional unit and {0}."),
								{ ManufacturingMaterialSummary(Project.MaterialRequirements, 1) });
				const FString RunLabel = LocalizedFormat(
					TEXT("strategic.manufacturing-run-format"),
					TEXT("RUN {0}  •  {1} EACH"),
					{ FString::FromInt(Project.UnitsRemaining), FString::FromInt(Project.UnitCost) });
				RenderedDynamicLabels.Add(RunLabel);
				RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SButton)
						.IsEnabled(Project.UnitsRemaining > 1 && Project.bCanRemoveManufacturingUnit)
						.IsFocusable(true)
						.ToolTipText(FText::FromString(!Project.bCanRemoveManufacturingUnit
							? StorageChangeUnavailableReason(Base, RemoveUnitStorageDelta)
							: Project.MaterialRequirements.IsEmpty()
							? Localized(
								TEXT("strategic.manufacturing-remove-tooltip"),
								TEXT("Remove one untouched tail unit and refund its full reservation. Use cancel to remove the final unit."))
							: LocalizedFormat(
								TEXT("strategic.manufacturing-remove-with-inputs-tooltip-format"),
								TEXT("Remove one untouched tail unit; refund its funds and {0}. Use cancel to remove the final unit."),
								{ ManufacturingMaterialUnitSummary(Project.MaterialRequirements) })))
						.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleManufacturingProjectQuantityClicked,
							Project.ProjectId, -1)
						[
							MakeText(TEXT("−"), 11, PrimaryText, true)
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						MakeText(RunLabel, 10, SecondaryText, true)
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SButton)
						.IsEnabled(bCanAddUnit)
						.IsFocusable(true)
						.ToolTipText(FText::FromString(AddUnitTooltip))
						.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleManufacturingProjectQuantityClicked,
							Project.ProjectId, 1)
						[
							MakeText(TEXT("+"), 11, PrimaryText, true)
						]
					]
				];
			}
			const FString MaterialRefund = ManufacturingMaterialRefundSummary(Project.MaterialRequirements);
			const FString ProductionCancelTooltip = !Project.bCanCancel
				? StorageChangeUnavailableReason(Base, Project.CancellationStorageDelta)
				: MaterialRefund.IsEmpty()
				? LocalizedFormat(
					TEXT("strategic.manufacturing-cancel-tooltip-format"),
					TEXT("Cancel production. Untouched units refund {0}; the current partial unit is sunk."),
					{ LexToString(Project.CancellationRefund) })
				: LocalizedFormat(
					TEXT("strategic.manufacturing-cancel-inputs-tooltip-format"),
					TEXT("Cancel production. Untouched units refund {0} and return {1}; inputs for the current partial unit are consumed."),
					{ LexToString(Project.CancellationRefund), MaterialRefund });
			const FString CancelProjectLabel = bResearch
				? Localized(TEXT("strategic.cancel-research"), TEXT("CANCEL RESEARCH"))
				: LocalizedFormat(
					TEXT("strategic.manufacturing-cancel-format"),
					TEXT("CANCEL PRODUCTION  •  REFUND {0}"),
					{ LexToString(Project.CancellationRefund) });
			RenderedDynamicLabels.Add(CancelProjectLabel);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SButton)
				.IsEnabled(bResearch || Project.bCanCancel)
				.IsFocusable(true)
				.ButtonColorAndOpacity(FLinearColor(0.30f, 0.08f, 0.06f, 1.0f))
				.ToolTipText(FText::FromString(bResearch
					? Localized(TEXT("strategic.cancel-research-tooltip"),
						TEXT("Cancel this research program and immediately release its scientists."))
					: ProductionCancelTooltip))
				.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCancelProjectClicked,
					Project.Type, Project.ProjectId, Project.RuleId)
				[
					MakeText(CancelProjectLabel, 9, Warning, true)
				]
			];
		}
		else if (Project.Type == EStrategicProjectType::Construction)
		{
			const FString CancelConstructionTooltip = LocalizedFormat(
				TEXT("strategic.facility-construction-cancel-tooltip-format"),
				TEXT("Cancel construction, free its grid footprint, and refund {0} based on unspent build time."),
				{ LexToString(Project.CancellationRefund) });
			const FString CancelConstructionLabel = LocalizedFormat(
				TEXT("strategic.facility-construction-cancel-action-format"),
				TEXT("CANCEL CONSTRUCTION  •  REFUND {0}"),
				{ LexToString(Project.CancellationRefund) });
			RenderedDynamicLabels.Add(CancelConstructionLabel);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(SButton)
				.IsFocusable(true)
				.ButtonColorAndOpacity(FLinearColor(0.30f, 0.08f, 0.06f, 1.0f))
				.ToolTipText(FText::FromString(CancelConstructionTooltip))
				.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCancelProjectClicked,
					Project.Type, Project.ProjectId, Project.RuleId)
				[
					MakeText(CancelConstructionLabel, 9, Warning, true)
				]
			];
		}
	}

	const FString ProgramsProcurementLabel = Localized(
		TEXT("strategic.programs-procurement"), TEXT("PROGRAMS + PROCUREMENT"));
	RenderedDynamicLabels.Add(ProgramsProcurementLabel);
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 6.0f)
	[
		MakeText(ProgramsProcurementLabel, 16, Accent, true)
	];
	for (const FStrategicActionOptionView& Option : CurrentSnapshot.ActionOptions)
	{
		if (Option.Type == EStrategicActionOptionType::Manufacturing)
		{
			const FString OptionDisplayName = LocalizedContentName(Option.RuleId, Option.DisplayName);
			int32& Quantity = ManufacturingQuantities.FindOrAdd(Option.RuleId);
			Quantity = FMath::Clamp(Quantity <= 0 ? 1 : Quantity, 1, 99);
			const int64 TotalCost = Option.Cost * Quantity;
			const bool bBatchAffordable = Option.Cost <= 0 || TotalCost <= CurrentSnapshot.Funds;
			const bool bBatchHasMaterials = HasManufacturingMaterials(Option.MaterialRequirements, Quantity);
			const FStrategicBaseView* Base = CurrentSnapshot.Bases.FindByPredicate(
				[this](const FStrategicBaseView& Entry) { return Entry.BaseId == CurrentSnapshot.PrimaryBaseId; });
			const int64 BatchStorageDelta = StorageDeltaForUnits(Option.StorageDeltaPerUnit, Quantity);
			const bool bBatchHasStorage = HasStorageForChange(Base, BatchStorageDelta);
			const bool bCanOrder = Option.bAvailable && bBatchAffordable && bBatchHasMaterials && bBatchHasStorage;
			const FString OptionDetail = LocalizedManufacturingOptionDetail(Option, Quantity > 1);
			const FString BatchDetail = !Option.bAvailable
				? LocalizedManufacturingUnavailableReason(Option, Base)
				: !bBatchAffordable
					? LocalizedFormat(
						TEXT("strategic.manufacturing-batch-cost-format"),
						TEXT("Batch costs {0}; only {1} funds are available."),
						{ LexToString(TotalCost), LexToString(CurrentSnapshot.Funds) })
					: !bBatchHasMaterials
						? LocalizedFormat(
							TEXT("strategic.manufacturing-batch-materials-format"),
							TEXT("Batch requires {0}."),
							{ ManufacturingMaterialSummary(Option.MaterialRequirements, Quantity) })
						: !bBatchHasStorage
							? StorageChangeUnavailableReason(Base, BatchStorageDelta)
						: Option.MaterialRequirements.IsEmpty()
							? OptionDetail
							: LocalizedFormat(
								TEXT("strategic.manufacturing-batch-inputs-format"),
								TEXT("{0} • Batch inputs: {1}"),
								{ OptionDetail,
									ManufacturingMaterialSummary(Option.MaterialRequirements, Quantity) });
			const FString ManufacturingOptionLabel = LocalizedFormat(
				TEXT("strategic.manufacturing-option-format"),
				TEXT("MAKE ×{0}  {1}  •  {2}\n{3}"),
				{ FString::FromInt(Quantity), OptionDisplayName, LexToString(TotalCost), BatchDetail });
			RenderedDynamicLabels.Add(ManufacturingOptionLabel);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.IsEnabled(Quantity > 1)
					.IsFocusable(true)
					.ToolTipText(FText::FromString(Localized(
						TEXT("strategic.manufacturing-reduce-tooltip"),
						TEXT("Reduce this production batch by one unit."))))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleManufacturingQuantityClicked,
						Option.RuleId, -1)
					[
						MakeText(TEXT("−"), 11, PrimaryText, true)
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.IsEnabled(bCanOrder)
					.IsFocusable(true)
					.ToolTipText(FText::FromString(BatchDetail))
					.ButtonColorAndOpacity(bCanOrder
						? FLinearColor(0.0f, 0.25f, 0.38f, 1.0f)
						: FLinearColor(0.04f, 0.055f, 0.075f, 1.0f))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleManufacturingOrderClicked, Option.RuleId)
					[
						MakeText(ManufacturingOptionLabel, 10, bCanOrder ? PrimaryText : SecondaryText)
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.IsEnabled(Quantity < 99)
					.IsFocusable(true)
					.ToolTipText(FText::FromString(Localized(
						TEXT("strategic.manufacturing-increase-tooltip"),
						TEXT("Increase this production batch by one unit."))))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleManufacturingQuantityClicked,
						Option.RuleId, 1)
					[
						MakeText(TEXT("+"), 11, PrimaryText, true)
					]
				]
			];
			continue;
		}
		const FString Cost = Option.Cost > 0 ? FString::Printf(TEXT("  •  %lld"), Option.Cost) : FString();
		const FString OptionDisplayName = LocalizedContentName(Option.RuleId, Option.DisplayName);
		FString OptionDetail = Option.Type == EStrategicActionOptionType::Research
			? LocalizedResearchOptionDetail(Option)
			: Option.Type == EStrategicActionOptionType::Craft
				? LocalizedCraftOptionDetail(Option)
				: Option.Detail;
		if (Option.Type == EStrategicActionOptionType::Facility
			&& !Option.BaseDefenseSupplyItemId.IsNone() && Option.BaseDefenseSupplyPerShot > 0)
		{
			OptionDetail += LocalizedFormat(
				TEXT("strategic.facility-option-defense-supply-format"),
				TEXT(" • supply {0} {1}/shot"),
				{
					FString::FromInt(Option.BaseDefenseSupplyPerShot),
					LocalizedContentName(
						Option.BaseDefenseSupplyItemId, Option.BaseDefenseSupplyDisplayName)
				});
		}
		const FString OptionUnavailableReason = Option.Type == EStrategicActionOptionType::Research
			? LocalizedResearchUnavailableReason(Option)
			: Option.Type == EStrategicActionOptionType::Craft
				? LocalizedCraftUnavailableReason(Option)
				: LocalizedDiagnostic(Option.UnavailableReasonCode, Option.UnavailableReason);
		const bool bSelectedPlacement = Option.Type == EStrategicActionOptionType::Facility
			&& PendingFacilityRuleId == Option.RuleId;
		const FString OptionTooltip = bSelectedPlacement
			? Localized(
				TEXT("strategic.facility-placement-cancel-tooltip"),
				TEXT("Cancel manual facility placement and return the base grid to inspection mode."))
			: Option.bAvailable ? OptionDetail : OptionUnavailableReason;
		const FString OptionLabel = bSelectedPlacement
			? LocalizedFormat(
				TEXT("strategic.facility-placement-cancel-card-format"),
				TEXT("CANCEL PLACEMENT  {0}\nSELECT A GREEN + CELL ON THE BASE GRID"),
				{ OptionDisplayName })
			: FString::Printf(TEXT("%s  %s%s\n%s"),
				*OptionVerb(Option.Type), *OptionDisplayName, *Cost,
				Option.bAvailable ? *OptionDetail : *OptionUnavailableReason);
		RenderedDynamicLabels.Add(OptionLabel);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SButton)
			.IsEnabled(Option.bAvailable || bSelectedPlacement)
			.IsFocusable(true)
			.ToolTipText(FText::FromString(OptionTooltip))
			.ButtonColorAndOpacity(bSelectedPlacement
				? FLinearColor(0.48f, 0.23f, 0.035f, 1.0f)
				: Option.bAvailable ? FLinearColor(0.0f, 0.25f, 0.38f, 1.0f)
				: FLinearColor(0.04f, 0.055f, 0.075f, 1.0f))
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleOptionClicked, Option.Type, Option.RuleId)
			[
				MakeText(OptionLabel, 11,
					Option.bAvailable || bSelectedPlacement ? PrimaryText : SecondaryText)
			]
		];
	}

	if (bHasSelectedMarker)
	{
		FString MarkerName = SelectedMarker.DisplayName;
		FString MarkerDetail = SelectedMarker.Detail;
		if (SelectedMarker.Type == EStrategicGlobeMarkerType::Craft)
		{
			if (const FStrategicCraftView* Craft = CurrentSnapshot.Craft.FindByPredicate(
				[this](const FStrategicCraftView& Entry) { return Entry.CraftId == SelectedMarker.EntityId; }))
			{
				MarkerName = Craft->DisplayName;
				const FString CraftType = LocalizedContentName(Craft->CraftRuleId, Craft->TypeDisplayName);
				MarkerDetail = Craft->RemainingRouteSeconds > 0
					? LocalizedFormat(
						TEXT("strategic.craft-marker-route-format"), TEXT("{0} • {1} • {2}"),
						{ CraftType, CraftStatusLabel(Craft->StatusType), LocalizedDuration(Craft->RemainingRouteSeconds) })
					: LocalizedFormat(
						TEXT("strategic.craft-marker-format"), TEXT("{0} • {1}"),
						{ CraftType, CraftStatusLabel(Craft->StatusType) });
			}
		}
		else if (SelectedMarker.Type == EStrategicGlobeMarkerType::Contact)
		{
			if (const FStrategicContactView* Contact = CurrentSnapshot.Contacts.FindByPredicate(
				[this](const FStrategicContactView& Entry) { return Entry.ContactId == SelectedMarker.EntityId; }))
			{
				MarkerName = LocalizedContactName(*Contact);
				MarkerDetail = Contact->bTargetsBase
					? LocalizedFormat(
						TEXT("strategic.contact-marker-target-format"),
						TEXT("{0} • THREAT {1} • HULL {2}/{3} • ROUTE {4}% • TARGET {5}"),
						{
							ContactStatusLabel(*Contact), FString::FromInt(Contact->ThreatRating),
							FString::FromInt(Contact->CurrentHull), FString::FromInt(Contact->MaxHull),
							FString::FromInt(FMath::RoundToInt(Contact->RouteProgress * 100.0f)),
							Contact->TargetBaseName.ToUpper()
						})
					: LocalizedFormat(
						TEXT("strategic.contact-marker-format"),
						TEXT("{0} • THREAT {1} • HULL {2}/{3} • ROUTE {4}%"),
						{
							ContactStatusLabel(*Contact), FString::FromInt(Contact->ThreatRating),
							FString::FromInt(Contact->CurrentHull), FString::FromInt(Contact->MaxHull),
							FString::FromInt(FMath::RoundToInt(Contact->RouteProgress * 100.0f))
						});
			}
		}
		else if (SelectedMarker.Type == EStrategicGlobeMarkerType::Site)
		{
			if (const FStrategicSiteView* Site = CurrentSnapshot.Sites.FindByPredicate(
				[this](const FStrategicSiteView& Entry) { return Entry.SiteId == SelectedMarker.EntityId; }))
			{
				MarkerName = LocalizedSiteName(*Site);
				MarkerDetail = LocalizedFormat(
					TEXT("strategic.site-marker-format"), TEXT("THREAT {0} • {1}"),
					{ FString::FromInt(Site->ThreatRating), LocalizedDuration(Site->RemainingLifetimeSeconds) });
			}
		}
		const FString SelectedMarkerLabel = LocalizedFormat(
			TEXT("strategic.marker-selected-format"), TEXT("{0}  //  {1}  •  {2}  •  {3}°, {4}°"),
			{
				MarkerTypeLabel(SelectedMarker.Type), MarkerName, MarkerDetail,
				FString::Printf(TEXT("%+.3f"), static_cast<double>(SelectedMarker.LongitudeMilliDegrees) / 1000.0),
				FString::Printf(TEXT("%+.3f"), static_cast<double>(SelectedMarker.LatitudeMilliDegrees) / 1000.0)
			});
		RenderedDynamicLabels.Add(SelectedMarkerLabel);
		MarkerText->SetText(FText::FromString(SelectedMarkerLabel));
	}

	struct FTimeButton
	{
		EStrategicTimeRate Rate;
		FString Label;
	};
	const FTimeButton TimeButtons[] = {
		{ EStrategicTimeRate::FiveSeconds,
			Localized(TEXT("strategic.time-five-seconds"), TEXT("5 SEC")) },
		{ EStrategicTimeRate::OneMinute,
			Localized(TEXT("strategic.time-one-minute"), TEXT("1 MIN")) },
		{ EStrategicTimeRate::FiveMinutes,
			Localized(TEXT("strategic.time-five-minutes"), TEXT("5 MIN")) },
		{ EStrategicTimeRate::ThirtyMinutes,
			Localized(TEXT("strategic.time-thirty-minutes"), TEXT("30 MIN")) },
		{ EStrategicTimeRate::OneHour,
			Localized(TEXT("strategic.time-one-hour"), TEXT("1 HOUR")) },
		{ EStrategicTimeRate::OneDay,
			Localized(TEXT("strategic.time-one-day"), TEXT("1 DAY")) }
	};
	for (const FTimeButton& Entry : TimeButtons)
	{
		RenderedCommandActionLabels.Add(Entry.Label);
		ActionBox->AddSlot()
		[
			SNew(SButton)
			.IsEnabled(CurrentSnapshot.bCanAdvanceTime)
			.IsFocusable(true)
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleTimeClicked, Entry.Rate)
			[
				MakeText(Entry.Label, 11)
			]
		];
	}
	const FString ArchiveLabel = Localized(TEXT("archive.command-action"), TEXT("ARCHIVE"));
	RenderedCommandActionLabels.Add(ArchiveLabel);
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleKnowledgeArchiveClicked)
		[
			MakeText(ArchiveLabel, 11)
		]
	];
	const FString SaveLabel = Localized(TEXT("strategic.save"), TEXT("SAVE"));
	RenderedCommandActionLabels.Add(SaveLabel);
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleSaveClicked)
		[
			MakeText(SaveLabel, 11)
		]
	];
	const FString LoadLabel = Localized(TEXT("strategic.load"), TEXT("LOAD"));
	RenderedCommandActionLabels.Add(LoadLabel);
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleLoadClicked)
		[
			MakeText(LoadLabel, 11)
		]
	];
	const FString SettingsLabel = Localized(TEXT("strategic.settings"), TEXT("SETTINGS"));
	RenderedCommandActionLabels.Add(SettingsLabel);
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleSettingsClicked)
		[
			MakeText(SettingsLabel, 11)
		]
	];
}

void UUEGTStrategicHudWidget::BuildKnowledgeArchive()
{
	using namespace UEGTStrategicHudPrivate;
	TitleText->SetText(FText::FromString(Localized(
		TEXT("archive.title"), TEXT("UEGT  //  SIGNAL ARCHIVE"))));
	SubtitleText->SetText(FText::FromString(LocalizedFormat(
		TEXT("archive.subtitle-format"),
		TEXT("{0} UNLOCKED RECORDS  •  {1} CLASSIFIED"),
		{
			FString::FromInt(CurrentSnapshot.ArchiveEntries.Num()),
			FString::FromInt(CurrentSnapshot.ArchiveLockedCount)
		})));
	StatusText->SetText(FText::FromString(StatusMessage.IsEmpty()
		? Localized(TEXT("archive.instructions"),
			TEXT("Browse research-authorized records. Classified record names and contents remain undisclosed."))
		: StatusMessage));
	StatusText->SetColorAndOpacity(bStatusIsError ? Warning : SecondaryText);
	MarkerText->SetText(FText::GetEmpty());

	TArray<FName> CategoryIds;
	TMap<FName, int32> CategoryCounts;
	for (const FStrategicArchiveEntryView& Entry : CurrentSnapshot.ArchiveEntries)
	{
		CategoryCounts.FindOrAdd(Entry.CategoryId) += 1;
		CategoryIds.AddUnique(Entry.CategoryId);
	}
	CategoryIds.Sort(FNameLexicalLess());
	if (!SelectedArchiveCategoryId.IsNone() && !CategoryIds.Contains(SelectedArchiveCategoryId))
	{
		SelectedArchiveCategoryId = NAME_None;
	}

	const FString CategoriesLabel = Localized(TEXT("archive.categories"), TEXT("CATEGORIES"));
	RenderedArchiveLabels.Add(CategoriesLabel);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 7.0f)
	[
		MakeText(CategoriesLabel, 17, Accent, true)
	];
	const FString AllRecordsLabel = Localized(TEXT("archive.all-records"), TEXT("ALL RECORDS"));
	const FString AllRecordsCountLabel = LocalizedFormat(
		TEXT("archive.category-count-format"), TEXT("{0}  {1}"),
		{ AllRecordsLabel, FString::FromInt(CurrentSnapshot.ArchiveEntries.Num()) });
	RenderedArchiveLabels.Add(AllRecordsLabel);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.ButtonColorAndOpacity(SelectedArchiveCategoryId.IsNone()
			? FLinearColor(0.0f, 0.35f, 0.42f, 1.0f)
			: FLinearColor(0.03f, 0.11f, 0.2f, 1.0f))
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleArchiveCategoryClicked, FName())
		[
			MakeText(AllRecordsCountLabel, 12, PrimaryText, true)
		]
	];
	for (const FName CategoryId : CategoryIds)
	{
		const FStrategicArchiveEntryView* FirstEntry = CurrentSnapshot.ArchiveEntries.FindByPredicate(
			[CategoryId](const FStrategicArchiveEntryView& Entry)
			{
				return Entry.CategoryId == CategoryId;
			});
		const FString CategoryFallback = FirstEntry != nullptr
			? FirstEntry->CategoryDisplayName
			: CategoryId.ToString();
		const FString CategoryDisplayLabel = FText::FromString(LocalizedContentName(
			CategoryId, CategoryFallback)).ToUpper().ToString();
		const FString CategoryLabel = LocalizedFormat(
			TEXT("archive.category-count-format"), TEXT("{0}  {1}"),
			{ CategoryDisplayLabel, FString::FromInt(CategoryCounts.FindRef(CategoryId)) });
		RenderedArchiveLabels.Add(CategoryDisplayLabel);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(SButton)
			.IsFocusable(true)
			.ButtonColorAndOpacity(SelectedArchiveCategoryId == CategoryId
				? FLinearColor(0.0f, 0.35f, 0.42f, 1.0f)
				: FLinearColor(0.03f, 0.11f, 0.2f, 1.0f))
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleArchiveCategoryClicked, CategoryId)
			[
				MakeText(CategoryLabel, 12, PrimaryText, SelectedArchiveCategoryId == CategoryId)
			]
		];
	}

	const FString SearchLabel = Localized(TEXT("archive.search"), TEXT("SEARCH"));
	RenderedArchiveLabels.Add(SearchLabel);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 7.0f)
	[
		MakeText(SearchLabel, 15, Accent, true)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 5.0f)
	[
		SAssignNew(ArchiveSearchTextBox, SEditableTextBox)
		.Text(FText::FromString(ArchiveSearchText))
		.HintText(FText::FromString(Localized(
			TEXT("archive.search-placeholder"), TEXT("Search unlocked records"))))
		.SelectAllTextWhenFocused(false)
	];
	const FString SearchActionLabel = Localized(TEXT("archive.search-action"), TEXT("SEARCH"));
	const FString ClearSearchLabel = Localized(TEXT("archive.clear-search"), TEXT("CLEAR SEARCH"));
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 12.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(SButton)
			.IsFocusable(true)
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleArchiveSearchClicked)
			[
				MakeText(SearchActionLabel, 11, PrimaryText, true)
			]
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.IsEnabled(!ArchiveSearchText.IsEmpty())
			.IsFocusable(true)
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleArchiveClearSearchClicked)
			[
				MakeText(ClearSearchLabel, 11, PrimaryText)
			]
		]
	];

	TArray<const FStrategicArchiveEntryView*> CategoryEntries;
	TArray<const FStrategicArchiveEntryView*> FilteredEntries;
	const FString NormalizedSearch = ArchiveSearchText.TrimStartAndEnd();
	for (const FStrategicArchiveEntryView& Entry : CurrentSnapshot.ArchiveEntries)
	{
		if (!SelectedArchiveCategoryId.IsNone() && Entry.CategoryId != SelectedArchiveCategoryId)
		{
			continue;
		}
		CategoryEntries.Add(&Entry);
		const FString EntryDisplayName = LocalizedContentName(Entry.EntryId, Entry.DisplayName);
		const FString EntrySummary = LocalizedContentField(Entry.EntryId, TEXT("summary"), Entry.Summary);
		const FString EntryBody = LocalizedContentField(Entry.EntryId, TEXT("body"), Entry.Body);
		const FString CategoryDisplayName = LocalizedContentName(
			Entry.CategoryId, Entry.CategoryDisplayName);
		const bool bMatchesSearch = NormalizedSearch.IsEmpty()
			|| EntryDisplayName.Contains(NormalizedSearch, ESearchCase::IgnoreCase)
			|| EntrySummary.Contains(NormalizedSearch, ESearchCase::IgnoreCase)
			|| EntryBody.Contains(NormalizedSearch, ESearchCase::IgnoreCase)
			|| CategoryDisplayName.Contains(NormalizedSearch, ESearchCase::IgnoreCase);
		if (bMatchesSearch)
		{
			FilteredEntries.Add(&Entry);
		}
	}
	const FString RecordIndexLabel = Localized(TEXT("archive.record-index"), TEXT("RECORD INDEX"));
	RenderedArchiveLabels.Add(RecordIndexLabel);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
	[
		MakeText(RecordIndexLabel, 15, Accent, true)
	];
	const FString ResultsLabel = LocalizedFormat(
		TEXT("archive.results-format"), TEXT("{0} OF {1} RECORDS"),
		{ FString::FromInt(FilteredEntries.Num()), FString::FromInt(CategoryEntries.Num()) });
	RenderedArchiveLabels.Add(ResultsLabel);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
	[
		MakeText(ResultsLabel, 11, SecondaryText)
	];
	if (CurrentSnapshot.ArchiveEntries.IsEmpty())
	{
		const FString EmptyLabel = Localized(TEXT("archive.empty"),
			TEXT("No archive records have been authorized. Complete research to release new material."));
		RenderedArchiveLabels.Add(EmptyLabel);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 5.0f)
		[
			MakeText(EmptyLabel, 13, SecondaryText)
		];
	}
	else if (FilteredEntries.IsEmpty())
	{
		const FString NoResultsLabel = Localized(TEXT("archive.no-results"),
			TEXT("No authorized records match the current category and search."));
		RenderedArchiveLabels.Add(NoResultsLabel);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 5.0f)
		[
			MakeText(NoResultsLabel, 13, SecondaryText)
		];
	}
	else
	{
		const bool bSelectedRecordVisible = FilteredEntries.ContainsByPredicate(
			[this](const FStrategicArchiveEntryView* Entry)
			{
				return Entry != nullptr && Entry->EntryId == SelectedArchiveEntryId;
			});
		if (!bSelectedRecordVisible)
		{
			SelectedArchiveEntryId = FilteredEntries[0]->EntryId;
		}
		for (const FStrategicArchiveEntryView* Entry : FilteredEntries)
		{
			if (Entry == nullptr)
			{
				continue;
			}
			const FString EntryDisplayName = LocalizedContentName(Entry->EntryId, Entry->DisplayName);
			const FString EntrySummary = LocalizedContentField(
				Entry->EntryId, TEXT("summary"), Entry->Summary);
			RenderedArchiveLabels.Add(EntryDisplayName);
			RenderedArchiveLabels.Add(EntrySummary);
			const bool bSelected = Entry->EntryId == SelectedArchiveEntryId;
			LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
			[
				SNew(SButton)
				.IsFocusable(true)
				.ButtonColorAndOpacity(bSelected
					? FLinearColor(0.0f, 0.35f, 0.42f, 1.0f)
					: FLinearColor(0.03f, 0.11f, 0.2f, 1.0f))
				.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleArchiveEntryClicked, Entry->EntryId)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						MakeText(EntryDisplayName, 13, PrimaryText, true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						MakeText(EntrySummary, 10, SecondaryText)
					]
				]
			];
		}
	}

	const FStrategicArchiveEntryView* SelectedEntry = CurrentSnapshot.ArchiveEntries.FindByPredicate(
		[this](const FStrategicArchiveEntryView& Entry)
		{
			return Entry.EntryId == SelectedArchiveEntryId;
		});
	if (SelectedEntry == nullptr || FilteredEntries.IsEmpty())
	{
		SelectedArchiveEntryId = NAME_None;
		const FString SelectRecordLabel = Localized(TEXT("archive.select-record"),
			TEXT("Select an authorized record to inspect its complete entry."));
		RenderedArchiveLabels.Add(SelectRecordLabel);
		RightBox->AddSlot().AutoHeight()
		[
			MakeText(SelectRecordLabel, 14, SecondaryText)
		];
	}
	else
	{
		const FString CategoryDisplayName = FText::FromString(LocalizedContentName(
			SelectedEntry->CategoryId, SelectedEntry->CategoryDisplayName)).ToUpper().ToString();
		const FString EntryDisplayName = LocalizedContentName(
			SelectedEntry->EntryId, SelectedEntry->DisplayName);
		const FString EntrySummary = LocalizedContentField(
			SelectedEntry->EntryId, TEXT("summary"), SelectedEntry->Summary);
		const FString EntryBody = LocalizedContentField(
			SelectedEntry->EntryId, TEXT("body"), SelectedEntry->Body);
		RenderedArchiveLabels.Add(CategoryDisplayName);
		RenderedArchiveLabels.Add(EntryDisplayName);
		RenderedArchiveLabels.Add(EntrySummary);
		RenderedArchiveLabels.Add(EntryBody);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			MakeText(CategoryDisplayName, 12, Accent, true)
		];
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			MakeText(EntryDisplayName, 20, PrimaryText, true)
		];
		const FString AuthorizationLabel = Localized(TEXT("archive.authorization"),
			TEXT("RESEARCH-AUTHORIZED RECORD"));
		RenderedArchiveLabels.Add(AuthorizationLabel);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)
		[
			MakeText(AuthorizationLabel, 11, Success, true)
		];
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			MakeText(EntrySummary, 14, SecondaryText, true)
		];
		RightBox->AddSlot().AutoHeight()
		[
			MakeText(EntryBody, 13, PrimaryText)
		];

		const FString RelatedLabel = Localized(TEXT("archive.related-records"), TEXT("RELATED RECORDS"));
		RenderedArchiveLabels.Add(RelatedLabel);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 20.0f, 0.0f, 7.0f)
		[
			MakeText(RelatedLabel, 15, Accent, true)
		];
		bool bAddedRelatedRecord = false;
		for (const FName RelatedEntryId : SelectedEntry->RelatedEntryIds)
		{
			const FStrategicArchiveEntryView* Related = CurrentSnapshot.ArchiveEntries.FindByPredicate(
				[RelatedEntryId](const FStrategicArchiveEntryView& Entry)
				{
					return Entry.EntryId == RelatedEntryId;
				});
			if (Related == nullptr)
			{
				continue;
			}
			bAddedRelatedRecord = true;
			const FString RelatedDisplayName = LocalizedContentName(
				Related->EntryId, Related->DisplayName);
			RenderedArchiveLabels.Add(RelatedDisplayName);
			RightBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				SNew(SButton)
				.IsFocusable(true)
				.ButtonColorAndOpacity(FLinearColor(0.03f, 0.11f, 0.2f, 1.0f))
				.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleArchiveRelatedEntryClicked, RelatedEntryId)
				[
					MakeText(RelatedDisplayName, 12, PrimaryText, true)
				]
			];
		}
		if (!bAddedRelatedRecord)
		{
			const FString NoRelatedLabel = Localized(TEXT("archive.no-related-records"),
				TEXT("No related records are currently unlocked."));
			RenderedArchiveLabels.Add(NoRelatedLabel);
			RightBox->AddSlot().AutoHeight()
			[
				MakeText(NoRelatedLabel, 12, SecondaryText)
			];
		}
	}

	if (!ArchiveSearchText.IsEmpty())
	{
		RenderedCommandActionLabels.Add(ClearSearchLabel);
		ActionBox->AddSlot()
		[
			SNew(SButton)
			.IsFocusable(true)
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleArchiveClearSearchClicked)
			[
				MakeText(ClearSearchLabel, 12, PrimaryText)
			]
		];
	}
	const FString BackLabel = Localized(TEXT("archive.back"), TEXT("BACK TO COMMAND"));
	RenderedCommandActionLabels.Add(BackLabel);
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.ButtonColorAndOpacity(FLinearColor(0.0f, 0.35f, 0.42f, 1.0f))
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleArchiveBackClicked)
		[
			MakeText(BackLabel, 12, PrimaryText, true)
		]
	];
}

void UUEGTStrategicHudWidget::BuildSaveBrowser()
{
	using namespace UEGTStrategicHudPrivate;
	TitleText->SetText(FText::FromString(bSaveBrowserForSaving
		? Localized(TEXT("save.title-save"), TEXT("UEGT  //  SAVE CAMPAIGN"))
		: Localized(TEXT("save.title-load"), TEXT("UEGT  //  LOAD CAMPAIGN"))));
	SubtitleText->SetText(FText::FromString(Localized(
		TEXT("save.subtitle"),
		TEXT("VERIFIED LOCAL SLOTS  •  AUTOMATIC BACKUP RECOVERY"))));
	StatusText->SetText(FText::FromString(StatusMessage.IsEmpty()
		? (bSaveBrowserForSaving
			? Localized(TEXT("save.instructions-save"),
				TEXT("Choose a new slot name or select an existing slot twice to confirm overwrite."))
			: Localized(TEXT("save.instructions-load"),
				TEXT("Choose a verified campaign slot. Damaged or incompatible saves remain visible for diagnosis.")))
		: StatusMessage));
	StatusText->SetColorAndOpacity(bStatusIsError ? Warning : SecondaryText);
	MarkerText->SetText(FText::GetEmpty());

	UUEGTGameInstance* Instance = GetWorld() != nullptr
		? GetWorld()->GetGameInstance<UUEGTGameInstance>()
		: nullptr;
	const FCampaignSaveSlotListResult Listing = Instance != nullptr
		? Instance->ListCampaignSaves()
		: FCampaignSaveSlotListResult();

	const FString CampaignSlotsLabel = Localized(
		TEXT("save.campaign-slots"), TEXT("CAMPAIGN SLOTS"));
	RenderedSaveBrowserLabels.Add(CampaignSlotsLabel);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		MakeText(CampaignSlotsLabel, 17, Accent, true)
	];
	if (Instance == nullptr || !Listing.bSucceeded)
	{
		const FString Diagnostic = Listing.Diagnostics.IsEmpty()
			? Localized(TEXT("save.storage-unavailable"),
				TEXT("Save storage is unavailable in the current world."))
			: FUEGTLocalizationService::DiagnosticText(
				Listing.Diagnostics[0].Code, Listing.Diagnostics[0].Message);
		RenderedSaveBrowserLabels.Add(Diagnostic);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 6.0f)
		[
			MakeText(Diagnostic, 13, Warning, true)
		];
	}
	else if (Listing.Slots.IsEmpty())
	{
		const FString EmptyLabel = bSaveBrowserForSaving
			? Localized(TEXT("save.none-save"),
				TEXT("No existing campaigns. Create the first named slot."))
			: Localized(TEXT("save.none-load"),
				TEXT("No campaign saves were found."));
		RenderedSaveBrowserLabels.Add(EmptyLabel);
		LeftBox->AddSlot().AutoHeight().Padding(0.0f, 6.0f)
		[
			MakeText(EmptyLabel, 13, SecondaryText)
		];
	}
	else
	{
		for (const FCampaignSaveSlotSummary& SaveSummary : Listing.Slots)
		{
			const FString IntegrityLabel = !SaveSummary.bLoadable
				? Localized(TEXT("save.integrity-unavailable"), TEXT("UNAVAILABLE"))
				: (SaveSummary.bRecovered
					? Localized(TEXT("save.integrity-recovered"), TEXT("RECOVERED FALLBACK"))
					: Localized(TEXT("save.integrity-verified"), TEXT("VERIFIED")));
			const FLinearColor IntegrityColor = !SaveSummary.bLoadable
				? Warning
				: (SaveSummary.bRecovered ? FLinearColor(1.0f, 0.72f, 0.18f, 1.0f) : Success);
			FString Detail;
			if (SaveSummary.bLoadable)
			{
				Detail = LocalizedFormat(
					TEXT("save.detail-format"),
					TEXT("{0}  •  {1}\nCAMPAIGN  {2}  •  FUNDS  ${3}  •  SCORE  {4}\nSAVED  {5}  •  BUILD  {6}"),
					{
						IntegrityLabel,
						DifficultyLabel(SaveSummary.Difficulty),
						SaveSummary.CampaignTimeUtc.ToString(TEXT("%Y-%m-%d %H:%M UTC")),
						FString::Printf(TEXT("%lld"), SaveSummary.Funds),
						FString::Printf(TEXT("%lld"), SaveSummary.CampaignScore),
						SaveSummary.LastSavedUtc.ToString(TEXT("%Y-%m-%d %H:%M UTC")),
						SaveSummary.BuildVersion.IsEmpty()
							? Localized(TEXT("common.unknown"), TEXT("UNKNOWN"))
							: SaveSummary.BuildVersion
					});
			}
			else
			{
				Detail = SaveSummary.Diagnostics.IsEmpty()
					? Localized(TEXT("save.unverified-detail"),
						TEXT("UNAVAILABLE  •  No verified primary, temporary, or backup save could be loaded."))
					: LocalizedFormat(
						TEXT("save.unavailable-diagnostic-format"),
						TEXT("UNAVAILABLE  •  {0}"),
						{ FUEGTLocalizationService::DiagnosticText(
							SaveSummary.Diagnostics[0].Code,
							SaveSummary.Diagnostics[0].Message) });
			}
			RenderedSaveBrowserLabels.Add(Detail);

			const bool bConfirmOverwrite = PendingOverwriteSlot.Equals(
				SaveSummary.SlotName, ESearchCase::IgnoreCase);
			const FString CardTitle = bConfirmOverwrite
				? LocalizedFormat(
					TEXT("save.confirm-overwrite-card-format"),
					TEXT("CONFIRM OVERWRITE  //  {0}"),
					{ SaveSummary.SlotName })
				: SaveSummary.SlotName;
			TSharedRef<SVerticalBox> CardContent = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeText(CardTitle, 15, PrimaryText, true)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					MakeText(Detail, 11, IntegrityColor)
				];

			if (bSaveBrowserForSaving)
			{
				LeftBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
				[
					SNew(SButton)
					.IsFocusable(true)
					.ButtonColorAndOpacity(bConfirmOverwrite
						? FLinearColor(0.48f, 0.18f, 0.06f, 1.0f)
						: FLinearColor(0.03f, 0.11f, 0.2f, 1.0f))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleSaveSlotClicked, SaveSummary.SlotName)
					[
						CardContent
					]
				];
			}
			else
			{
				LeftBox->AddSlot().AutoHeight().Padding(0.0f, 4.0f)
				[
					SNew(SButton)
					.IsEnabled(SaveSummary.bLoadable)
					.IsFocusable(true)
					.ButtonColorAndOpacity(SaveSummary.bLoadable
						? FLinearColor(0.03f, 0.11f, 0.2f, 1.0f)
						: FLinearColor(0.14f, 0.04f, 0.04f, 1.0f))
					.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleLoadSlotClicked, SaveSummary.SlotName)
					[
						CardContent
					]
				];
			}
		}
	}

	const FString RightTitle = bSaveBrowserForSaving
		? Localized(TEXT("save.create-named-slot"), TEXT("CREATE NAMED SLOT"))
		: Localized(TEXT("save.integrity-title"), TEXT("SAVE INTEGRITY"));
	RenderedSaveBrowserLabels.Add(RightTitle);
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		MakeText(RightTitle, 17, Accent, true)
	];
	if (bSaveBrowserForSaving)
	{
		const FString NamingHelp = Localized(TEXT("save.naming-help"),
			TEXT("Use 1–48 letters, digits, underscores, or hyphens. Existing names require a second selection before replacement."));
		RenderedSaveBrowserLabels.Add(NamingHelp);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			MakeText(NamingHelp, 13, SecondaryText)
		];
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 9.0f)
		[
			SAssignNew(SaveSlotTextBox, SEditableTextBox)
			.Text(FText::FromString(SaveSlotText))
			.HintText(FText::FromString(TEXT("Campaign1")))
			.SelectAllTextWhenFocused(true)
		];
		const FString SaveToNamedSlotLabel = Localized(
			TEXT("save.to-named-slot"), TEXT("SAVE TO NAMED SLOT"));
		RenderedSaveBrowserLabels.Add(SaveToNamedSlotLabel);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
		[
			SNew(SButton)
			.IsEnabled(Instance != nullptr)
			.IsFocusable(true)
			.ButtonColorAndOpacity(FLinearColor(0.0f, 0.42f, 0.48f, 1.0f))
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCreateSaveSlotClicked)
			[
				MakeText(SaveToNamedSlotLabel, 14, PrimaryText, true)
			]
		];
	}
	else
	{
		const FString IntegrityHelp = Localized(TEXT("save.integrity-help"),
			TEXT("Every slot is checksum-verified before it is listed as loadable. Content-package versions are checked against the active catalog."));
		RenderedSaveBrowserLabels.Add(IntegrityHelp);
		RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
		[
			MakeText(IntegrityHelp, 13, SecondaryText)
		];
	}
	const FString GuaranteesTitle = Localized(
		TEXT("save.integrity-guarantees"), TEXT("INTEGRITY GUARANTEES"));
	RenderedSaveBrowserLabels.Add(GuaranteesTitle);
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 20.0f, 0.0f, 7.0f)
	[
		MakeText(GuaranteesTitle, 15, Accent, true)
	];
	const FString GuaranteesBody = Localized(TEXT("save.guarantees-body"),
		TEXT("• Verified temporary writes\n• Automatic primary/backup rotation\n• Fallback recovery from interrupted or damaged writes\n• Corrupt and incompatible slots stay visible but cannot be loaded"));
	RenderedSaveBrowserLabels.Add(GuaranteesBody);
	RightBox->AddSlot().AutoHeight()
	[
		MakeText(GuaranteesBody, 13, SecondaryText)
	];

	const FString BackLabel = Localized(TEXT("settings.back"), TEXT("BACK TO COMMAND"));
	RenderedCommandActionLabels.Add(BackLabel);
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.ButtonColorAndOpacity(FLinearColor(0.0f, 0.35f, 0.42f, 1.0f))
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleSaveBrowserBackClicked)
		[
			MakeText(BackLabel, 12, PrimaryText, true)
		]
	];
}

void UUEGTStrategicHudWidget::BuildSettings()
{
	if (bSettingsControlsPage)
	{
		BuildControlSettings();
		return;
	}
	if (bSettingsGameplayPage)
	{
		BuildGameplaySettings();
		return;
	}
	using namespace UEGTStrategicHudPrivate;
	TitleText->SetText(FText::FromString(Localized(
		TEXT("settings.title"), TEXT("UEGT  //  SETTINGS + ACCESSIBILITY"))));
	SubtitleText->SetText(FText::FromString(Localized(
		TEXT("settings.subtitle"), TEXT("PERSISTENT LOCAL PREFERENCES  •  CHANGES APPLY IMMEDIATELY"))));
	StatusText->SetText(FText::FromString(StatusMessage.IsEmpty()
		? Localized(TEXT("settings.default-status"),
			TEXT("Every critical state also uses labels or geometry; color is never the only signal."))
		: StatusMessage));
	StatusText->SetColorAndOpacity(bStatusIsError ? Warning : SecondaryText);
	MarkerText->SetText(FText::GetEmpty());

	UUEGTUserSettings* Settings = UUEGTUserSettings::Get();
	if (Settings == nullptr)
	{
		LeftBox->AddSlot().AutoHeight()[MakeText(Localized(
			TEXT("settings.unavailable"), TEXT("Persistent user settings are unavailable.")),
			14, Warning, true)];
		return;
	}

	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		MakeText(Localized(TEXT("settings.accessibility"), TEXT("ACCESSIBILITY")), 17, Accent, true)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
	[
		MakeText(Localized(TEXT("settings.accessibility-help"),
			TEXT("Scale the entire command interface, remove camera easing, and select palettes designed to separate friendly, hostile, objective, and warning states.")),
			13, SecondaryText)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleUIScaleClicked)
		[
			MakeText(FString::Printf(TEXT("%s  //  %d%%"),
				*Localized(TEXT("settings.ui-scale"), TEXT("UI SCALE")), Settings->GetUIScalePercent()), 14)
		]
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleReducedMotionClicked)
		[
			MakeText(FString::Printf(TEXT("%s  //  %s"),
				*Localized(TEXT("settings.reduced-motion"), TEXT("REDUCED MOTION")),
				*ToggleLabel(Settings->IsReducedMotionEnabled())), 14)
		]
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleHighContrastClicked)
		[
			MakeText(FString::Printf(TEXT("%s  //  %s"),
				*Localized(TEXT("settings.high-contrast"), TEXT("HIGH CONTRAST MARKERS")),
				*ToggleLabel(Settings->IsHighContrastEnabled())), 14)
		]
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleColorVisionClicked)
		[
			MakeText(FString::Printf(TEXT("%s  //  %s"),
				*Localized(TEXT("settings.color-palette"), TEXT("COLOR-VISION PALETTE")),
				*ColorVisionLabel(Settings->GetColorVisionMode())), 14)
		]
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCameraSpeedClicked)
		[
			MakeText(FString::Printf(TEXT("%s  //  %d%%"),
				*Localized(TEXT("settings.camera-speed"), TEXT("CAMERA SPEED")),
				Settings->GetCameraSpeedPercent()), 14)
		]
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 8.0f)
	[
		MakeText(Localized(TEXT("settings.audio"), TEXT("AUDIO")), 17, Accent, true)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
	[
		MakeText(Localized(TEXT("settings.audio-help"),
			TEXT("Original signals and ambience are synthesized at runtime. Master output uses Unreal's primary audio device; unfocused mute prevents background audio from competing with other applications.")),
			13, SecondaryText)
	];
	const FString AudioProfileLabel = Localized(
		TEXT("settings.audio-profile"), TEXT("ORIGINAL PROCEDURAL MIX"));
	RenderedDynamicLabels.Add(AudioProfileLabel);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
	[
		MakeText(AudioProfileLabel, 11, Accent, true)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleMasterVolumeClicked)
		[
			MakeText(FString::Printf(TEXT("%s  //  %d%%"),
				*Localized(TEXT("settings.master-volume"), TEXT("MASTER VOLUME")),
				Settings->GetMasterVolumePercent()), 14)
		]
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleMuteWhenUnfocusedClicked)
		[
			MakeText(FString::Printf(TEXT("%s  //  %s"),
				*Localized(TEXT("settings.mute-unfocused"), TEXT("MUTE WHEN UNFOCUSED")),
				*ToggleLabel(Settings->ShouldMuteWhenUnfocused())), 14)
		]
	];
	const FString AudioPreviewLabel = Localized(
		TEXT("settings.audio-preview"), TEXT("PREVIEW AUDIO CUE"));
	RenderedDynamicLabels.Add(AudioPreviewLabel);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleAudioPreviewClicked)
		[
			MakeText(AudioPreviewLabel, 14)
		]
	];

	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		MakeText(Localized(TEXT("settings.display"), TEXT("DISPLAY + PERFORMANCE")), 17, Accent, true)
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
	[
		MakeText(Localized(TEXT("settings.display-help"),
			TEXT("These settings use Unreal's native scalability and presentation paths and persist in the platform user-settings file.")),
			13, SecondaryText)
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleVSyncClicked)
		[
			MakeText(FString::Printf(TEXT("%s  //  %s"),
				*Localized(TEXT("settings.vertical-sync"), TEXT("VERTICAL SYNC")),
				*ToggleLabel(Settings->IsVSyncEnabled())), 14)
		]
	];
	const float FrameLimit = Settings->GetFrameRateLimit();
	const FString FrameLimitLabel = Localized(TEXT("settings.frame-limit"), TEXT("FRAME LIMIT"));
	const FString FrameLimitText = FrameLimit <= 0.5f
		? FString::Printf(TEXT("%s  //  %s"), *FrameLimitLabel,
			*Localized(TEXT("common.unlimited"), TEXT("UNLIMITED")))
		: FString::Printf(TEXT("%s  //  %d FPS"), *FrameLimitLabel, FMath::RoundToInt(FrameLimit));
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleFrameLimitClicked)
		[
			MakeText(FrameLimitText, 14)
		]
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleQualityClicked)
		[
			MakeText(FString::Printf(TEXT("%s  //  %s"),
				*Localized(TEXT("settings.render-quality"), TEXT("RENDER QUALITY")),
				*QualityLabel(Settings->GetOverallScalabilityLevel())), 14)
		]
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 8.0f)
	[
		MakeText(Localized(TEXT("settings.language-controls"), TEXT("LANGUAGE + CONTROLS")),
			17, Accent, true)
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
	[
		MakeText(Localized(TEXT("settings.language-help"),
			TEXT("Menus, settings, the campaign founding flow, and strategic command chrome translate immediately. Other campaign and tactical text without an authored entry falls back to English.")),
			13, SecondaryText)
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleInterfaceCultureClicked)
		[
			MakeText(FString::Printf(TEXT("%s  //  %s"),
				*Localized(TEXT("settings.interface-culture"), TEXT("INTERFACE CULTURE")),
				*UUEGTUserSettings::GetInterfaceCultureDisplayName(Settings->GetInterfaceCulture())), 14)
		]
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.ButtonColorAndOpacity(FLinearColor(0.0f, 0.28f, 0.36f, 1.0f))
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleControlSettingsClicked)
		[
			MakeText(Localized(TEXT("settings.keyboard-remapping"), TEXT("KEYBOARD COMMAND REMAPPING")),
				14, PrimaryText, true)
		]
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.ButtonColorAndOpacity(FLinearColor(0.0f, 0.28f, 0.36f, 1.0f))
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleGameplaySettingsClicked)
		[
			MakeText(Localized(TEXT("settings.gameplay-options"), TEXT("GAMEPLAY OPTIONS")),
				14, PrimaryText, true)
		]
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.ButtonColorAndOpacity(FLinearColor(0.35f, 0.12f, 0.08f, 1.0f))
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleResetSettingsClicked)
		[
			MakeText(Localized(TEXT("settings.restore-defaults"), TEXT("RESTORE ACCESSIBLE DEFAULTS")), 13)
		]
	];

	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.ButtonColorAndOpacity(FLinearColor(0.0f, 0.35f, 0.42f, 1.0f))
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleSettingsBackClicked)
		[
			MakeText(Localized(TEXT("settings.back"), TEXT("BACK TO COMMAND")), 12, PrimaryText, true)
		]
	];
}

void UUEGTStrategicHudWidget::BuildControlSettings()
{
	using namespace UEGTStrategicHudPrivate;
	const FString ControlTitle = Localized(
		TEXT("controls.title"), TEXT("UEGT  //  KEYBOARD COMMANDS"));
	const FString ControlSubtitle = Localized(
		TEXT("controls.subtitle"),
		TEXT("PERSISTENT CONFLICT-SAFE REMAPPING  •  CHANGES APPLY IMMEDIATELY"));
	const FString ControlStatus = StatusMessage.IsEmpty()
		? Localized(
			TEXT("controls.default-status"),
			TEXT("Select an action to cycle its key. If a key is already assigned, the two commands exchange keys so neither becomes unbound."))
		: StatusMessage;
	TitleText->SetText(FText::FromString(ControlTitle));
	SubtitleText->SetText(FText::FromString(ControlSubtitle));
	StatusText->SetText(FText::FromString(ControlStatus));
	StatusText->SetColorAndOpacity(bStatusIsError ? Warning : SecondaryText);
	MarkerText->SetText(FText::GetEmpty());
	RenderedDynamicLabels.Add(ControlTitle);
	RenderedDynamicLabels.Add(ControlSubtitle);
	RenderedDynamicLabels.Add(ControlStatus);

	UUEGTUserSettings* Settings = UUEGTUserSettings::Get();
	if (Settings == nullptr)
	{
		const FString UnavailableLabel = Localized(
			TEXT("settings.unavailable"), TEXT("Persistent user settings are unavailable."));
		RenderedDynamicLabels.Add(UnavailableLabel);
		LeftBox->AddSlot().AutoHeight()[MakeText(UnavailableLabel, 14, Warning, true)];
		return;
	}

	const TArray<EUEGTInputCommand> Commands = UUEGTUserSettings::GetRemappableInputCommands();
	const int32 SplitIndex = (Commands.Num() + 1) / 2;
	const auto AddCommandButton = [this, Settings](
		const TSharedPtr<SVerticalBox>& Column,
		const EUEGTInputCommand Command)
	{
		const FKey Key = Settings->GetInputKey(Command);
		const FString CommandLabel = UEGTStrategicHudPrivate::LocalizedFormat(
			TEXT("controls.binding-format"), TEXT("{0}  //  {1}"),
			{
				UEGTStrategicHudPrivate::LocalizedInputCommandLabel(Command),
				Key.GetDisplayName(false).ToString()
			});
		RenderedDynamicLabels.Add(CommandLabel);
		Column->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
		[
			SNew(SButton)
			.IsFocusable(true)
			.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleInputBindingClicked, Command)
			[
				UEGTStrategicHudPrivate::MakeText(CommandLabel, 13)
			]
		];
	};

	const FString TacticalActionsLabel = Localized(
		TEXT("controls.tactical-actions"), TEXT("TACTICAL ACTIONS"));
	const FString TacticalActionsHelp = Localized(
		TEXT("controls.tactical-actions-help"),
		TEXT("These keyboard bindings drive the same validated controller commands as pointer and gamepad input."));
	RenderedDynamicLabels.Add(TacticalActionsLabel);
	RenderedDynamicLabels.Add(TacticalActionsHelp);
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		MakeText(TacticalActionsLabel, 17, Accent, true)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
	[
		MakeText(TacticalActionsHelp, 13, SecondaryText)
	];
	for (int32 Index = 0; Index < SplitIndex; ++Index)
	{
		AddCommandButton(LeftBox, Commands[Index]);
	}

	const FString TargetingEquipmentLabel = Localized(
		TEXT("controls.targeting-equipment"), TEXT("TARGETING + EQUIPMENT"));
	const FString FixedControlsHelp = Localized(
		TEXT("controls.fixed-controls-help"),
		TEXT("WASD/QE camera movement, arrow-key targeting, mouse controls, Escape, strategic time keys 4–6, and all gamepad mappings stay fixed and cannot be displaced."));
	RenderedDynamicLabels.Add(TargetingEquipmentLabel);
	RenderedDynamicLabels.Add(FixedControlsHelp);
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		MakeText(TargetingEquipmentLabel, 17, Accent, true)
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
	[
		MakeText(FixedControlsHelp, 13, SecondaryText)
	];
	for (int32 Index = SplitIndex; Index < Commands.Num(); ++Index)
	{
		AddCommandButton(RightBox, Commands[Index]);
	}

	const FString RestoreKeyDefaultsLabel = Localized(
		TEXT("controls.restore-key-defaults"), TEXT("RESTORE KEY DEFAULTS"));
	const FString GameplayOptionsLabel = Localized(
		TEXT("settings.gameplay-options"), TEXT("GAMEPLAY OPTIONS"));
	const FString GeneralSettingsLabel = Localized(
		TEXT("settings.general-settings"), TEXT("GENERAL SETTINGS"));
	const FString BackLabel = Localized(TEXT("settings.back"), TEXT("BACK TO COMMAND"));
	RenderedDynamicLabels.Add(RestoreKeyDefaultsLabel);
	RenderedDynamicLabels.Add(GameplayOptionsLabel);
	RenderedDynamicLabels.Add(GeneralSettingsLabel);
	RenderedDynamicLabels.Add(BackLabel);
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.ButtonColorAndOpacity(FLinearColor(0.35f, 0.12f, 0.08f, 1.0f))
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleResetInputBindingsClicked)
		[
			MakeText(RestoreKeyDefaultsLabel, 12, PrimaryText, true)
		]
	];
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleGameplaySettingsClicked)
		[
			MakeText(GameplayOptionsLabel, 12, PrimaryText, true)
		]
	];
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleGeneralSettingsClicked)
		[
			MakeText(GeneralSettingsLabel, 12, PrimaryText, true)
		]
	];
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.ButtonColorAndOpacity(FLinearColor(0.0f, 0.35f, 0.42f, 1.0f))
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleSettingsBackClicked)
		[
			MakeText(BackLabel, 12, PrimaryText, true)
		]
	];
}

void UUEGTStrategicHudWidget::BuildGameplaySettings()
{
	using namespace UEGTStrategicHudPrivate;
	TitleText->SetText(FText::FromString(Localized(
		TEXT("gameplay.title"), TEXT("UEGT  //  GAMEPLAY OPTIONS"))));
	SubtitleText->SetText(FText::FromString(Localized(
		TEXT("gameplay.subtitle"), TEXT("LOCAL TACTICAL FLOW PREFERENCES  •  CAMPAIGN RULES STAY AUTHORITATIVE"))));
	StatusText->SetText(FText::FromString(StatusMessage.IsEmpty()
		? Localized(TEXT("gameplay.default-status"),
			TEXT("These preferences change input safeguards and presentation only; they never change simulation outcomes."))
		: StatusMessage));
	StatusText->SetColorAndOpacity(bStatusIsError ? Warning : SecondaryText);
	MarkerText->SetText(FText::GetEmpty());

	UUEGTUserSettings* Settings = UUEGTUserSettings::Get();
	if (Settings == nullptr)
	{
		LeftBox->AddSlot().AutoHeight()[MakeText(Localized(
			TEXT("settings.unavailable"), TEXT("Persistent user settings are unavailable.")),
			14, Warning, true)];
		return;
	}

	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		MakeText(Localized(TEXT("gameplay.flow"), TEXT("TACTICAL FLOW")), 17, Accent, true)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
	[
		MakeText(Localized(TEXT("gameplay.flow-help"),
			TEXT("Choose how the command layer protects unfinished turns, hands off between ready agents, and follows selections.")),
			13, SecondaryText)
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleEndTurnSafetyClicked)
		[
			MakeText(FString::Printf(TEXT("%s  //  %s"),
				*Localized(TEXT("gameplay.end-turn-safety"), TEXT("END-TURN SAFETY")),
				*EndTurnSafetyLabel(Settings->GetEndTurnSafetyMode())), 14)
		]
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleAutoSelectReadyAgentClicked)
		[
			MakeText(FString::Printf(TEXT("%s  //  %s"),
				*Localized(TEXT("gameplay.auto-select"), TEXT("AUTO-SELECT READY AGENT")),
				*ToggleLabel(Settings->ShouldAutoSelectReadyAgent())), 14)
		]
	];
	LeftBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleCenterCameraOnSelectionClicked)
		[
			MakeText(FString::Printf(TEXT("%s  //  %s"),
				*Localized(TEXT("gameplay.camera-follow"), TEXT("CAMERA FOLLOWS SELECTION")),
				*ToggleLabel(Settings->ShouldCenterCameraOnSelection())), 14)
		]
	];

	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		MakeText(Localized(TEXT("gameplay.guardrails"), TEXT("AUTHORITATIVE GUARDRAILS")), 17, Accent, true)
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 14.0f)
	[
		MakeText(Localized(TEXT("gameplay.guardrails-help"),
			TEXT("End-turn safety uses the current fog-safe HUD snapshot. Auto-selection and camera follow never issue a domain command or consume a random draw.")),
			13, SecondaryText)
	];
	RightBox->AddSlot().AutoHeight().Padding(0.0f, 6.0f)
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")))
		.BorderBackgroundColor(FLinearColor(0.04f, 0.13f, 0.17f, 1.0f))
		.Padding(12.0f)
		[
			MakeText(Localized(TEXT("gameplay.guardrails-mandatory"),
				TEXT("Mandatory strategic decisions, base assaults, tactical legality, AI behavior, damage, and deterministic replay are never bypassed by local gameplay preferences.")),
				13, Success, true)
		]
	];

	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleGeneralSettingsClicked)
		[
			MakeText(Localized(TEXT("settings.general-settings"), TEXT("GENERAL SETTINGS")),
				12, PrimaryText, true)
		]
	];
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleControlSettingsClicked)
		[
			MakeText(Localized(TEXT("settings.keyboard-remapping"), TEXT("KEYBOARD COMMAND REMAPPING")),
				12, PrimaryText, true)
		]
	];
	ActionBox->AddSlot()
	[
		SNew(SButton)
		.IsFocusable(true)
		.ButtonColorAndOpacity(FLinearColor(0.0f, 0.35f, 0.42f, 1.0f))
		.OnClicked_UObject(this, &UUEGTStrategicHudWidget::HandleSettingsBackClicked)
		[
			MakeText(Localized(TEXT("settings.back"), TEXT("BACK TO COMMAND")),
				12, PrimaryText, true)
		]
	];
}

FReply UUEGTStrategicHudWidget::HandleDifficultyClicked(const ECampaignDifficulty Difficulty)
{
	if (SeedTextBox.IsValid())
	{
		SeedText = SeedTextBox->GetText().ToString();
	}
	SelectedDifficulty = Difficulty;
	RefreshSlate();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleFundingModelClicked(
	const EUEGTFundingModel FundingModel)
{
	if (SeedTextBox.IsValid())
	{
		SeedText = SeedTextBox->GetText().ToString();
	}
	SelectedFundingModel = FundingModel;
	RefreshSlate();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleAccessibilityPresetClicked(
	const EUEGTAccessibilityPreset Preset)
{
	if (SeedTextBox.IsValid())
	{
		SeedText = SeedTextBox->GetText().ToString();
	}
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		if (Settings->ApplyAccessibilityPreset(Preset))
		{
			ApplySettingsAndRefresh(UEGTStrategicHudPrivate::Localized(
				TEXT("status.accessibility-preset-applied"),
				TEXT("Accessibility preset saved and applied.")));
		}
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleStartCampaignClicked()
{
	if (SeedTextBox.IsValid())
	{
		SeedText = SeedTextBox->GetText().ToString();
	}
	int64 Seed = 0;
	if (!LexTryParseString(Seed, *SeedText.TrimStartAndEnd()))
	{
		ShowStatusMessage(UEGTStrategicHudPrivate::Localized(
			TEXT("menu.campaign-seed-invalid"),
			TEXT("Campaign seed must be a signed 64-bit integer.")), true);
		return FReply::Handled();
	}
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->StartStrategicCampaign(SelectedDifficulty, Seed, SelectedFundingModel);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleReloadContentClicked()
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->ReloadContentCatalog();
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleLoadClicked()
{
	ShowSaveBrowser(false);
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleSaveClicked()
{
	ShowSaveBrowser(true);
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleSaveSlotClicked(FString SlotName)
{
	SlotName = SlotName.TrimStartAndEnd();
	if (!FCampaignSaveStore::IsValidSlotName(SlotName))
	{
		ShowStatusMessage(UEGTStrategicHudPrivate::Localized(
			TEXT("save.invalid-name"),
			TEXT("Slot name must contain 1–48 ASCII letters, digits, underscores, or hyphens.")), true);
		return FReply::Handled();
	}
	SaveSlotText = SlotName;
	if (!PendingOverwriteSlot.Equals(SlotName, ESearchCase::IgnoreCase))
	{
		PendingOverwriteSlot = SlotName;
		StatusMessage = UEGTStrategicHudPrivate::LocalizedFormat(
			TEXT("save.confirm-overwrite-status-format"),
			TEXT("Select {0} again to confirm overwrite. Its verified backup will rotate automatically."),
			{ SlotName });
		bStatusIsError = false;
		RefreshSlate();
		return FReply::Handled();
	}
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->SaveCampaignSlot(SlotName);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleLoadSlotClicked(FString SlotName)
{
	SlotName = SlotName.TrimStartAndEnd();
	if (!FCampaignSaveStore::IsValidSlotName(SlotName))
	{
		ShowStatusMessage(UEGTStrategicHudPrivate::Localized(
			TEXT("save.invalid-selected"),
			TEXT("The selected campaign slot name is invalid.")), true);
		return FReply::Handled();
	}
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->LoadCampaignSlot(SlotName);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCreateSaveSlotClicked()
{
	if (SaveSlotTextBox.IsValid())
	{
		SaveSlotText = SaveSlotTextBox->GetText().ToString().TrimStartAndEnd();
	}
	if (!FCampaignSaveStore::IsValidSlotName(SaveSlotText))
	{
		ShowStatusMessage(UEGTStrategicHudPrivate::Localized(
			TEXT("save.invalid-name"),
			TEXT("Slot name must contain 1–48 ASCII letters, digits, underscores, or hyphens.")), true);
		return FReply::Handled();
	}
	UUEGTGameInstance* Instance = GetWorld() != nullptr
		? GetWorld()->GetGameInstance<UUEGTGameInstance>()
		: nullptr;
	if (Instance == nullptr)
	{
		ShowStatusMessage(UEGTStrategicHudPrivate::Localized(
			TEXT("save.storage-unavailable"),
			TEXT("Save storage is unavailable in the current world.")), true);
		return FReply::Handled();
	}
	const FCampaignSaveSlotListResult Listing = Instance->ListCampaignSaves();
	if (!Listing.bSucceeded)
	{
		ShowStatusMessage(Listing.Diagnostics.IsEmpty()
			? UEGTStrategicHudPrivate::Localized(
				TEXT("save.enumeration-failed"),
				TEXT("Campaign slots could not be enumerated."))
			: FUEGTLocalizationService::DiagnosticText(
				Listing.Diagnostics[0].Code, Listing.Diagnostics[0].Message), true);
		return FReply::Handled();
	}
	if (const FCampaignSaveSlotSummary* Existing = Listing.Slots.FindByPredicate(
		[this](const FCampaignSaveSlotSummary& SaveSummary)
		{
			return SaveSummary.SlotName.Equals(SaveSlotText, ESearchCase::IgnoreCase);
		}))
	{
		return HandleSaveSlotClicked(Existing->SlotName);
	}
	PendingOverwriteSlot.Empty();
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->SaveCampaignSlot(SaveSlotText);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleSaveBrowserBackClicked()
{
	if (SaveSlotTextBox.IsValid())
	{
		SaveSlotText = SaveSlotTextBox->GetText().ToString();
	}
	CloseSaveBrowser();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleKnowledgeArchiveClicked()
{
	ShowKnowledgeArchive();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleArchiveBackClicked()
{
	CloseKnowledgeArchive();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleArchiveCategoryClicked(const FName CategoryId)
{
	SetKnowledgeArchiveCategoryFilter(CategoryId);
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleArchiveEntryClicked(const FName EntryId)
{
	SelectedArchiveEntryId = EntryId;
	RefreshSlate();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleArchiveRelatedEntryClicked(const FName EntryId)
{
	SelectKnowledgeArchiveRecord(EntryId);
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleArchiveSearchClicked()
{
	const FString SearchText = ArchiveSearchTextBox.IsValid()
		? ArchiveSearchTextBox->GetText().ToString().TrimStartAndEnd()
		: ArchiveSearchText.TrimStartAndEnd();
	SetKnowledgeArchiveSearchText(SearchText);
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleArchiveClearSearchClicked()
{
	SetKnowledgeArchiveSearchText(FString());
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleQuitClicked()
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->QuitGame();
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleRegionClicked(const FName RegionId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->EstablishStarterBase(RegionId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleRegionalDiplomacyClicked(
	const FName RegionId,
	const ERegionalDiplomacyActionType ActionType)
{
	if (AUEGTTacticalPlayerController* Controller = GetOwningPlayer<AUEGTTacticalPlayerController>())
	{
		Controller->ExecuteRegionalDiplomacy(RegionId, ActionType);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleRegionalCharterClicked(const FName RegionId)
{
	if (AUEGTTacticalPlayerController* Controller = GetOwningPlayer<AUEGTTacticalPlayerController>())
	{
		Controller->SignRegionalCharter(RegionId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleHorizonCompactClicked()
{
	if (AUEGTTacticalPlayerController* Controller = GetOwningPlayer<AUEGTTacticalPlayerController>())
	{
		Controller->RatifyHorizonCompact();
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleReciprocalAidClicked(const FName TargetRegionId)
{
	if (AUEGTTacticalPlayerController* Controller = GetOwningPlayer<AUEGTTacticalPlayerController>())
	{
		Controller->DeployReciprocalAid(TargetRegionId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCompactRestorationClicked(const FName RegionId)
{
	if (AUEGTTacticalPlayerController* Controller = GetOwningPlayer<AUEGTTacticalPlayerController>())
	{
		Controller->RestoreHorizonCompactMember(RegionId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCompactEmergencyVoteClicked(
	const FName TargetRegionId)
{
	if (AUEGTTacticalPlayerController* Controller = GetOwningPlayer<AUEGTTacticalPlayerController>())
	{
		Controller->CallHorizonCompactEmergencyVote(TargetRegionId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleTimeClicked(const EStrategicTimeRate Rate)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->AdvanceStrategicClock(Rate);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleOptionClicked(
	const EStrategicActionOptionType Type,
	const FName RuleId)
{
	if (Type == EStrategicActionOptionType::Facility)
	{
		if (PendingFacilityRuleId == RuleId)
		{
			CancelFacilityPlacement();
		}
		else
		{
			SelectFacilityForPlacement(RuleId);
		}
		return FReply::Handled();
	}
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->ExecuteStrategicOption(Type, RuleId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleFacilityPlacementClicked(
	const FName FacilityId,
	const FGuid BaseId,
	const int32 GridX,
	const int32 GridY)
{
	PendingFacilityRuleId = NAME_None;
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->StartStrategicFacilityConstruction(FacilityId, BaseId, GridX, GridY);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCancelFacilityPlacementClicked()
{
	CancelFacilityPlacement();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleFacilityDismantleReviewClicked(
	const FGuid BaseId,
	const FGuid FacilityInstanceId)
{
	SelectFacilityForDismantle(BaseId, FacilityInstanceId);
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleConfirmFacilityDismantleClicked()
{
	const FGuid BaseId = PendingDismantleBaseId;
	const FGuid FacilityInstanceId = PendingDismantleFacilityInstanceId;
	PendingDismantleBaseId.Invalidate();
	PendingDismantleFacilityInstanceId.Invalidate();
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->DismantleStrategicFacility(BaseId, FacilityInstanceId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCancelFacilityDismantleClicked()
{
	CancelFacilityDismantle();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleFacilityRepairClicked(
	const FGuid BaseId,
	const FGuid FacilityInstanceId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->RepairStrategicFacility(BaseId, FacilityInstanceId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCancelFacilityRepairClicked(
	const FGuid BaseId,
	const FGuid FacilityInstanceId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->CancelStrategicFacilityRepair(BaseId, FacilityInstanceId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleManufacturingQuantityClicked(
	const FName RuleId,
	const int32 Delta)
{
	int32& Quantity = ManufacturingQuantities.FindOrAdd(RuleId);
	Quantity = FMath::Clamp((Quantity <= 0 ? 1 : Quantity) + Delta, 1, 99);
	RefreshSlate();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleManufacturingOrderClicked(const FName RuleId)
{
	const int32 Quantity = FMath::Clamp(ManufacturingQuantities.FindRef(RuleId), 1, 99);
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->StartStrategicManufacturing(RuleId, Quantity);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleManufacturingProjectQuantityClicked(
	const FGuid ProjectId,
	const int32 Delta)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->AdjustStrategicManufacturingUnits(ProjectId, Delta);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleProjectStaffClicked(
	const EStrategicProjectType Type,
	const FGuid ProjectId,
	const FName RuleId,
	const int32 Delta)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->AdjustStrategicProjectStaff(Type, ProjectId, RuleId, Delta);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCancelProjectClicked(
	const EStrategicProjectType Type,
	const FGuid ProjectId,
	const FName RuleId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->CancelStrategicProject(Type, ProjectId, RuleId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleSellInventoryClicked(
	const FGuid BaseId,
	const FName ItemId,
	const int32 Quantity)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->SellStrategicInventory(BaseId, ItemId, Quantity);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleMutualAidConvoyClicked(
	const FGuid SourceBaseId,
	const FGuid DestinationBaseId,
	const FName ItemId,
	const int32 Quantity,
	const EMutualAidRoutePolicy RoutePolicy,
	const bool bSignalEscort)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->DispatchStrategicMutualAidConvoy(
			SourceBaseId, DestinationBaseId, ItemId, Quantity,
			RoutePolicy, bSignalEscort);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleMutualAidRoutePolicyClicked(
	const EMutualAidRoutePolicy RoutePolicy)
{
	SelectedMutualAidRoutePolicy = RoutePolicy;
	RefreshSlate();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleMutualAidRetuneClicked(
	const FGuid ConvoyId,
	const EMutualAidRoutePolicy RoutePolicy)
{
	if (AUEGTTacticalPlayerController* Controller =
		Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->RetuneStrategicMutualAidConvoy(ConvoyId, RoutePolicy);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleMutualAidSignalEscortCommissionClicked(
	const FGuid ConvoyId)
{
	if (AUEGTTacticalPlayerController* Controller =
		Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->CommissionStrategicMutualAidSignalEscort(ConvoyId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleMutualAidReliefPriorityClicked(
	const FGuid ConvoyId)
{
	if (AUEGTTacticalPlayerController* Controller =
		Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->PrioritizeStrategicMutualAidConvoy(ConvoyId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleMutualAidReliefStandDownClicked(
	const FGuid ConvoyId)
{
	if (AUEGTTacticalPlayerController* Controller =
		Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->StandDownStrategicMutualAidConvoy(ConvoyId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleMutualAidReliefDiversionClicked(
	const FGuid ConvoyId,
	const FGuid DestinationBaseId)
{
	if (AUEGTTacticalPlayerController* Controller =
		Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->DivertStrategicMutualAidConvoy(ConvoyId, DestinationBaseId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleMutualAidRelayWaypointClicked(
	const FGuid ConvoyId,
	const FGuid WaypointBaseId,
	const EMutualAidRoutePolicy OnwardRoutePolicy)
{
	if (AUEGTTacticalPlayerController* Controller =
		Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->ConfigureStrategicMutualAidRelayWaypoint(
			ConvoyId, WaypointBaseId, OnwardRoutePolicy);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleMutualAidBalancedHandoffClicked(
	const FGuid ConvoyId,
	const bool bEnabled)
{
	if (AUEGTTacticalPlayerController* Controller =
		Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->ConfigureStrategicMutualAidBalancedHandoff(
			ConvoyId, bEnabled);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleMutualAidSignalEscortClicked()
{
	bSelectedMutualAidSignalEscort = !bSelectedMutualAidSignalEscort;
	RefreshSlate();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleSignalWatchStaffClicked(
	const FGuid BaseId,
	const int32 DeltaScientists)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->AdjustStrategicSignalWatch(BaseId, DeltaScientists);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleWorksCadreStaffClicked(
	const FGuid BaseId,
	const int32 DeltaEngineers)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->AdjustStrategicWorksCadre(BaseId, DeltaEngineers);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleWorksCadreCharterClicked(
	const FGuid BaseId,
	const EWorksCadreCharter Charter)
{
	if (AUEGTTacticalPlayerController* Controller =
		Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->SetStrategicWorksCadreCharter(BaseId, Charter);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleTrainPersonnelClicked(
	const FGuid PersonnelId,
	const EPersonnelTrainingFocus Focus)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->BeginStrategicPersonnelTraining(PersonnelId, Focus);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandlePersonnelDoctrineClicked(
	const FGuid PersonnelId,
	const FName DoctrineId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->SelectStrategicPersonnelDoctrine(PersonnelId, DoctrineId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandlePersonnelRecoveryPlanClicked(
	const FGuid PersonnelId,
	const EPersonnelRecoveryPlan Plan)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->SelectStrategicPersonnelRecoveryPlan(PersonnelId, Plan);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandlePersonnelStewardshipClicked(
	const FGuid PersonnelId,
	const EPersonnelStewardshipFocus Focus)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->BeginStrategicPersonnelStewardship(PersonnelId, Focus);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandlePersonnelEquipmentClicked(
	const FGuid PersonnelId,
	const FName ItemId,
	const int32 Delta)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->AdjustStrategicPersonnelEquipment(PersonnelId, ItemId, Delta);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleTransferPersonnelClicked(
	const FGuid PersonnelId,
	const FGuid DestinationBaseId)
{
	PendingDismissPersonnelId.Invalidate();
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->TransferStrategicPersonnel(PersonnelId, DestinationBaseId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleDismissPersonnelClicked(const FGuid PersonnelId)
{
	using namespace UEGTStrategicHudPrivate;
	const FStrategicPersonnelView* Person = CurrentSnapshot.Personnel.FindByPredicate(
		[PersonnelId](const FStrategicPersonnelView& View) { return View.PersonnelId == PersonnelId; });
	if (Person == nullptr)
	{
		ShowStatusMessage(Localized(
			TEXT("strategic.personnel-unavailable"),
			TEXT("The selected personnel member is no longer available.")), true);
		return FReply::Handled();
	}
	if (PendingDismissPersonnelId != PersonnelId)
	{
		PendingDismissPersonnelId = PersonnelId;
		ShowStatusMessage(LocalizedFormat(
			TEXT("strategic.dismiss-confirmation-format"),
			TEXT("Select CONFIRM DISMISS for {0} to permanently remove them from the roster."),
			{ Person->DisplayName }), false);
		return FReply::Handled();
	}
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->DismissStrategicPersonnel(PersonnelId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleOperationClicked(const FGuid OperationId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->BeginPendingTacticalOperation(OperationId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandlePrepareCraftClicked(const FGuid CraftId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->AutoPrepareCraft(CraftId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCancelCraftServiceClicked(const FGuid CraftId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->CancelStrategicCraftService(CraftId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCraftRearmClicked(
	const FGuid CraftId,
	const ECraftRearmPolicy Policy)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->RearmStrategicCraft(CraftId, Policy);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleTransferCraftClicked(
	const FGuid CraftId,
	const FGuid DestinationBaseId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->TransferStrategicCraft(CraftId, DestinationBaseId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCraftSalvageClicked(
	const FGuid CraftId,
	const FName ItemId,
	const int32 Quantity,
	const ECraftSalvageDisposition Disposition)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->ResolveStrategicCraftSalvage(CraftId, ItemId, Quantity, Disposition);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCraftPilotClicked(
	const FGuid CraftId,
	const FGuid PersonnelId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->AssignStrategicCraftPilot(CraftId, PersonnelId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCraftAgentClicked(
	const FGuid CraftId,
	const FGuid PersonnelId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->ToggleStrategicCraftAgent(CraftId, PersonnelId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleEquipFieldTeamClicked()
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->AutoEquipFieldTeam();
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleDispatchContactClicked(const FGuid ContactId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->DispatchReadyCraftToContact(ContactId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleResolveInterceptionClicked(
	const FGuid ContactId,
	const EInterceptionPosture Posture)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->ResolveContactInterception(ContactId, Posture);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleWithdrawInterceptionClicked(
	const FGuid ContactId,
	const EInterceptionWithdrawalDoctrine Doctrine)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->WithdrawContactInterceptionWithDoctrine(ContactId, Doctrine);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleResolveBaseAssaultClicked(
	const FGuid AssaultId,
	const EBaseDefenseFireDoctrine FireDoctrine)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->ResolveBaseAssault(AssaultId, FireDoctrine);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleDeployBaseDefenseClicked(const FGuid AssaultId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->DeployBaseDefense(AssaultId);
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleDeploySiteClicked(const FGuid SiteId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->DeployReadyCraftToSite(SiteId);
	}
	return FReply::Handled();
}

void UUEGTStrategicHudWidget::ApplySettingsAndRefresh(const FString& Message)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->ApplyUserSettings();
	}
	else if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		Settings->ApplySettings(false);
	}
	StatusMessage = Message;
	bStatusIsError = false;
	RefreshSlate();
}

FReply UUEGTStrategicHudWidget::HandleSettingsClicked()
{
	ShowSettings();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleSettingsBackClicked()
{
	CloseSettings();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleUIScaleClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		const int32 Options[] = { 90, 100, 115, 130 };
		int32 Next = Options[0];
		for (const int32 Option : Options)
		{
			if (Option > Settings->GetUIScalePercent())
			{
				Next = Option;
				break;
			}
		}
		Settings->SetUIScalePercent(Next);
		ApplySettingsAndRefresh(UEGTStrategicHudPrivate::LocalizedFormat(
			TEXT("status.ui-scale-set-format"), TEXT("Interface scale set to {0}%."),
			{ FString::FromInt(Next) }));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleReducedMotionClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		Settings->SetReducedMotionEnabled(!Settings->IsReducedMotionEnabled());
		ApplySettingsAndRefresh(Settings->IsReducedMotionEnabled()
			? UEGTStrategicHudPrivate::Localized(
				TEXT("status.reduced-motion-enabled"),
				TEXT("Reduced motion enabled; camera easing is disabled."))
			: UEGTStrategicHudPrivate::Localized(
				TEXT("status.reduced-motion-disabled"),
				TEXT("Reduced motion disabled; camera easing is enabled.")));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleHighContrastClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		Settings->SetHighContrastEnabled(!Settings->IsHighContrastEnabled());
		ApplySettingsAndRefresh(Settings->IsHighContrastEnabled()
			? UEGTStrategicHudPrivate::Localized(
				TEXT("status.high-contrast-enabled"),
				TEXT("High-contrast strategic and tactical markers enabled."))
			: UEGTStrategicHudPrivate::Localized(
				TEXT("status.high-contrast-disabled"),
				TEXT("High-contrast strategic and tactical markers disabled.")));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleColorVisionClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		const uint8 Next = (static_cast<uint8>(Settings->GetColorVisionMode()) + 1)
			% (static_cast<uint8>(EUEGTColorVisionMode::Tritanopia) + 1);
		Settings->SetColorVisionMode(static_cast<EUEGTColorVisionMode>(Next));
		ApplySettingsAndRefresh(UEGTStrategicHudPrivate::LocalizedFormat(
			TEXT("status.color-vision-set-format"), TEXT("Color-vision palette set to {0}."),
			{ UEGTStrategicHudPrivate::ColorVisionLabel(Settings->GetColorVisionMode()) }));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCameraSpeedClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		const int32 Options[] = { 75, 100, 125, 150 };
		int32 Next = Options[0];
		for (const int32 Option : Options)
		{
			if (Option > Settings->GetCameraSpeedPercent())
			{
				Next = Option;
				break;
			}
		}
		Settings->SetCameraSpeedPercent(Next);
		ApplySettingsAndRefresh(UEGTStrategicHudPrivate::LocalizedFormat(
			TEXT("status.camera-speed-set-format"), TEXT("Camera speed set to {0}%."),
			{ FString::FromInt(Next) }));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleMasterVolumeClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		const int32 Options[] = { 0, 25, 50, 75, 100 };
		int32 Next = Options[0];
		for (const int32 Option : Options)
		{
			if (Option > Settings->GetMasterVolumePercent())
			{
				Next = Option;
				break;
			}
		}
		Settings->SetMasterVolumePercent(Next);
		ApplySettingsAndRefresh(UEGTStrategicHudPrivate::LocalizedFormat(
			TEXT("status.master-volume-set-format"), TEXT("Master volume set to {0}%."),
			{ FString::FromInt(Next) }));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleMuteWhenUnfocusedClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		Settings->SetMuteWhenUnfocused(!Settings->ShouldMuteWhenUnfocused());
		ApplySettingsAndRefresh(Settings->ShouldMuteWhenUnfocused()
			? UEGTStrategicHudPrivate::Localized(
				TEXT("status.unfocused-audio-muted"),
				TEXT("Audio will mute while the game is not focused."))
			: UEGTStrategicHudPrivate::Localized(
				TEXT("status.unfocused-audio-continues"),
				TEXT("Audio will continue while the game is not focused.")));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleAudioPreviewClicked()
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->PreviewAudioCue();
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleInterfaceCultureClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		const TArray<FString> Cultures = UUEGTUserSettings::GetSupportedInterfaceCultures();
		const int32 CurrentIndex = Cultures.IndexOfByKey(Settings->GetInterfaceCulture());
		const FString Next = Cultures[(CurrentIndex + 1) % Cultures.Num()];
		Settings->SetInterfaceCulture(Next);
		const FString FeedbackFormat = FUEGTLocalizationService::TextForCulture(
			TEXT("status.culture-set"),
			TEXT("Interface culture set to %s. Untranslated campaign and tactical text uses the English fallback."),
			Next);
		ApplySettingsAndRefresh(FeedbackFormat.Replace(
			TEXT("%s"),
			*UUEGTUserSettings::GetInterfaceCultureDisplayName(Next),
			ESearchCase::CaseSensitive));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleControlSettingsClicked()
{
	ShowControlSettings();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleGameplaySettingsClicked()
{
	ShowGameplaySettings();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleGeneralSettingsClicked()
{
	bSettingsControlsPage = false;
	bSettingsGameplayPage = false;
	StatusMessage.Empty();
	bStatusIsError = false;
	RefreshSlate();
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleEndTurnSafetyClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		const EUEGTEndTurnSafetyMode Next = Settings->GetEndTurnSafetyMode() == EUEGTEndTurnSafetyMode::Smart
			? EUEGTEndTurnSafetyMode::Always
			: Settings->GetEndTurnSafetyMode() == EUEGTEndTurnSafetyMode::Always
				? EUEGTEndTurnSafetyMode::Off
				: EUEGTEndTurnSafetyMode::Smart;
		Settings->SetEndTurnSafetyMode(Next);
		ApplySettingsAndRefresh(UEGTStrategicHudPrivate::Localized(
			TEXT("gameplay.status-updated"), TEXT("Gameplay preference saved and applied.")));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleAutoSelectReadyAgentClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		Settings->SetAutoSelectReadyAgent(!Settings->ShouldAutoSelectReadyAgent());
		ApplySettingsAndRefresh(UEGTStrategicHudPrivate::Localized(
			TEXT("gameplay.status-updated"), TEXT("Gameplay preference saved and applied.")));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleCenterCameraOnSelectionClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		Settings->SetCenterCameraOnSelection(!Settings->ShouldCenterCameraOnSelection());
		ApplySettingsAndRefresh(UEGTStrategicHudPrivate::Localized(
			TEXT("gameplay.status-updated"), TEXT("Gameplay preference saved and applied.")));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleInputBindingClicked(const EUEGTInputCommand Command)
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		const TArray<FKey>& Options = UEGTStrategicHudPrivate::InputKeyCycleOptions();
		const int32 CurrentIndex = Options.IndexOfByKey(Settings->GetInputKey(Command));
		const FKey NextKey = Options[(CurrentIndex + 1) % Options.Num()];
		const FKey PreviousKey = Settings->GetInputKey(Command);
		FString Diagnostic;
		FName DiagnosticCode;
		if (Settings->TrySetInputKey(Command, NextKey, Diagnostic, DiagnosticCode))
		{
			FString Feedback;
			if (DiagnosticCode == FName(TEXT("input_binding_unchanged")))
			{
				Feedback = UEGTStrategicHudPrivate::LocalizedFormat(
					TEXT("status.input-binding-unchanged-format"),
					TEXT("{0} remains assigned to {1}."),
					{
						UEGTStrategicHudPrivate::LocalizedInputCommandLabel(Command),
						NextKey.GetDisplayName(false).ToString()
					});
			}
			else if (DiagnosticCode == FName(TEXT("input_binding_swapped")))
			{
				const TArray<EUEGTInputCommand> RemappableCommands =
					UUEGTUserSettings::GetRemappableInputCommands();
				const EUEGTInputCommand* SwappedCommand =
					RemappableCommands.FindByPredicate(
						[Settings, Command, PreviousKey](const EUEGTInputCommand Candidate)
						{
							return Candidate != Command && Settings->GetInputKey(Candidate) == PreviousKey;
						});
				Feedback = SwappedCommand != nullptr
					? UEGTStrategicHudPrivate::LocalizedFormat(
						TEXT("status.input-binding-swapped-format"),
						TEXT("{0} assigned to {1}; {2} moved to {3} to avoid a conflict."),
						{
							UEGTStrategicHudPrivate::LocalizedInputCommandLabel(Command),
							NextKey.GetDisplayName(false).ToString(),
							UEGTStrategicHudPrivate::LocalizedInputCommandLabel(*SwappedCommand),
							PreviousKey.GetDisplayName(false).ToString()
						})
					: Diagnostic;
			}
			else
			{
				Feedback = UEGTStrategicHudPrivate::LocalizedFormat(
					TEXT("status.input-binding-assigned-format"),
					TEXT("{0} assigned to {1}."),
					{
						UEGTStrategicHudPrivate::LocalizedInputCommandLabel(Command),
						NextKey.GetDisplayName(false).ToString()
					});
			}
			ApplySettingsAndRefresh(Feedback);
		}
		else
		{
			StatusMessage = UEGTStrategicHudPrivate::LocalizedDiagnostic(DiagnosticCode, Diagnostic);
			bStatusIsError = true;
			RefreshSlate();
		}
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleResetInputBindingsClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		Settings->ResetInputBindingsToDefaults();
		ApplySettingsAndRefresh(UEGTStrategicHudPrivate::Localized(
			TEXT("status.input-bindings-restored"),
			TEXT("Keyboard command bindings restored to accessible defaults.")));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleVSyncClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		Settings->SetVSyncEnabled(!Settings->IsVSyncEnabled());
		ApplySettingsAndRefresh(Settings->IsVSyncEnabled()
			? UEGTStrategicHudPrivate::Localized(
				TEXT("status.vertical-sync-enabled"), TEXT("Vertical synchronization enabled."))
			: UEGTStrategicHudPrivate::Localized(
				TEXT("status.vertical-sync-disabled"), TEXT("Vertical synchronization disabled.")));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleFrameLimitClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		const float Current = Settings->GetFrameRateLimit();
		const float Next = Current <= 0.5f ? 30.0f
			: Current <= 30.5f ? 60.0f
			: Current <= 60.5f ? 120.0f
			: 0.0f;
		Settings->SetFrameRateLimit(Next);
		ApplySettingsAndRefresh(Next <= 0.5f
			? UEGTStrategicHudPrivate::Localized(
				TEXT("status.frame-limit-disabled"), TEXT("Frame limit disabled."))
			: UEGTStrategicHudPrivate::LocalizedFormat(
				TEXT("status.frame-limit-set-format"), TEXT("Frame limit set to {0} FPS."),
				{ FString::FromInt(FMath::RoundToInt(Next)) }));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleQualityClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		const int32 Current = Settings->GetOverallScalabilityLevel();
		const int32 Next = Current < 0 || Current >= 3 ? 0 : Current + 1;
		Settings->SetOverallScalabilityLevel(Next);
		ApplySettingsAndRefresh(UEGTStrategicHudPrivate::LocalizedFormat(
			TEXT("status.render-quality-set-format"), TEXT("Render quality set to {0}."),
			{ UEGTStrategicHudPrivate::QualityLabel(Next) }));
	}
	return FReply::Handled();
}

FReply UUEGTStrategicHudWidget::HandleResetSettingsClicked()
{
	if (UUEGTUserSettings* Settings = UUEGTUserSettings::Get())
	{
		Settings->SetToDefaults();
		ApplySettingsAndRefresh(UEGTStrategicHudPrivate::Localized(
			TEXT("status.accessible-defaults-restored"),
			TEXT("Accessible defaults restored and saved.")));
	}
	return FReply::Handled();
}
