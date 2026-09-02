// Copyright 2026 UEGT contributors. MIT License.

#include "Tactical/UEGTTacticalHudWidget.h"

#include "Localization/UEGTLocalizationService.h"
#include "Tactical/UEGTTacticalPlayerController.h"

#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace UEGTTacticalHudPrivate
{
	const FLinearColor PanelColor(0.015f, 0.025f, 0.055f, 0.91f);
	const FLinearColor PanelSoftColor(0.025f, 0.045f, 0.085f, 0.88f);
	const FLinearColor PrimaryText(0.83f, 0.91f, 1.0f, 1.0f);
	const FLinearColor SecondaryText(0.48f, 0.67f, 0.82f, 1.0f);
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

	FString LocalizedContentName(const FName RuleId)
	{
		return FUEGTLocalizationService::ContentName(RuleId, RuleId.ToString());
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
		const FString DoctrineName = LocalizedContentName(Relay.DoctrineId).ToUpper();
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

	FString PhaseLabel(const ETacticalBattlePhase Phase)
	{
		switch (Phase)
		{
		case ETacticalBattlePhase::Deployment:
			return Localized(TEXT("tactical.phase-deployment"), TEXT("DEPLOYMENT"));
		case ETacticalBattlePhase::PlayerTurn:
			return Localized(TEXT("tactical.phase-player"), TEXT("PLAYER TURN"));
		case ETacticalBattlePhase::AdversaryTurn:
			return Localized(TEXT("tactical.phase-adversary"), TEXT("ADVERSARY TURN"));
		case ETacticalBattlePhase::Resolved:
			return Localized(TEXT("tactical.phase-resolved"), TEXT("RESOLVED"));
		default:
			return Localized(TEXT("tactical.phase-unknown"), TEXT("UNKNOWN PHASE"));
		}
	}

	FString WindLabel(const ETacticalWindDirection Direction)
	{
		switch (Direction)
		{
		case ETacticalWindDirection::Calm: return Localized(TEXT("tactical.wind-calm"), TEXT("CALM"));
		case ETacticalWindDirection::North: return Localized(TEXT("tactical.wind-north"), TEXT("NORTH"));
		case ETacticalWindDirection::East: return Localized(TEXT("tactical.wind-east"), TEXT("EAST"));
		case ETacticalWindDirection::South: return Localized(TEXT("tactical.wind-south"), TEXT("SOUTH"));
		case ETacticalWindDirection::West: return Localized(TEXT("tactical.wind-west"), TEXT("WEST"));
		default: return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	FString ObjectiveTypeLabel(const ETacticalObjectiveType Type)
	{
		switch (Type)
		{
		case ETacticalObjectiveType::Disrupt: return Localized(TEXT("tactical.objective-disrupt"), TEXT("DISRUPT"));
		case ETacticalObjectiveType::Recover: return Localized(TEXT("tactical.objective-recover"), TEXT("RECOVER"));
		case ETacticalObjectiveType::Control: return Localized(TEXT("tactical.objective-control"), TEXT("CONTROL"));
		default: return Localized(TEXT("tactical.objective-generic"), TEXT("OBJECTIVE"));
		}
	}

	FString ObjectiveStatusLabel(const ETacticalObjectiveStatus Status)
	{
		switch (Status)
		{
		case ETacticalObjectiveStatus::Active: return Localized(TEXT("tactical.status-active"), TEXT("ACTIVE"));
		case ETacticalObjectiveStatus::Completed: return Localized(TEXT("tactical.status-complete"), TEXT("COMPLETE"));
		case ETacticalObjectiveStatus::Failed: return Localized(TEXT("tactical.status-failed"), TEXT("FAILED"));
		default: return Localized(TEXT("common.unknown"), TEXT("UNKNOWN"));
		}
	}

	TSharedRef<STextBlock> MakeText(
		const FString& Text,
		const int32 Size,
		const FLinearColor& Color = PrimaryText)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(Text))
			.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size))
			.ColorAndOpacity(Color)
			.AutoWrapText(true);
	}
}

TSharedRef<SWidget> UUEGTTacticalHudWidget::RebuildWidget()
{
	using namespace UEGTTacticalHudPrivate;

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
					SAssignNew(MissionText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24))
					.ColorAndOpacity(Accent)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 0.0f)
				[
					SAssignNew(PhaseText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 14))
					.ColorAndOpacity(SecondaryText)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
				[
					SAssignNew(FogText, STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 11))
					.ColorAndOpacity(FLinearColor(0.32f, 0.68f, 0.82f, 1.0f))
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
		.Padding(FMargin(18.0f, 118.0f, 0.0f, 132.0f))
		[
			SNew(SBox)
			.WidthOverride(330.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")))
				.BorderBackgroundColor(PanelSoftColor)
				.Padding(12.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(RosterBox, SVerticalBox)
					]
				]
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(0.0f, 118.0f, 18.0f, 132.0f))
		[
			SNew(SBox)
			.WidthOverride(350.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")))
				.BorderBackgroundColor(PanelSoftColor)
				.Padding(12.0f)
				[
					SAssignNew(ObjectiveBox, SVerticalBox)
				]
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(370.0f, 0.0f, 390.0f, 120.0f))
		[
			SAssignNew(HoverPanel, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush(TEXT("GenericWhiteBox")))
			.BorderBackgroundColor(PanelSoftColor)
			.Padding(FMargin(12.0f, 7.0f))
			[
				SAssignNew(HoverText, STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13))
				.ColorAndOpacity(PrimaryText)
				.Justification(ETextJustify::Center)
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

void UUEGTTacticalHudWidget::ApplySnapshot(const FTacticalHudSnapshot& Snapshot)
{
	CurrentSnapshot = Snapshot;
	CurrentDebrief = FTacticalDebriefView();
	RefreshSlate();
}

void UUEGTTacticalHudWidget::ApplyDebrief(const FTacticalDebriefView& Debrief)
{
	CurrentSnapshot = FTacticalHudSnapshot();
	CurrentDebrief = Debrief;
	RefreshSlate();
}

void UUEGTTacticalHudWidget::ShowStatusMessage(const FString& Message, const bool bIsError)
{
	StatusMessage = Message;
	bStatusIsError = bIsError;
	RefreshSlate();
}

FString UUEGTTacticalHudWidget::GetRenderedMissionText() const
{
	return MissionText.IsValid() ? MissionText->GetText().ToString() : FString();
}

FString UUEGTTacticalHudWidget::GetRenderedPhaseText() const
{
	return PhaseText.IsValid() ? PhaseText->GetText().ToString() : FString();
}

FString UUEGTTacticalHudWidget::GetRenderedFogText() const
{
	return FogText.IsValid() ? FogText->GetText().ToString() : FString();
}

FString UUEGTTacticalHudWidget::GetRenderedStatusText() const
{
	return StatusText.IsValid() ? StatusText->GetText().ToString() : FString();
}

FString UUEGTTacticalHudWidget::GetRenderedHoverText() const
{
	return HoverText.IsValid() ? HoverText->GetText().ToString() : FString();
}

FString UUEGTTacticalHudWidget::ActionLabel(const FTacticalHudActionAvailability& Action)
{
	using namespace UEGTTacticalHudPrivate;
	FString Label;
	switch (Action.ActionType)
	{
	case ETacticalHudActionType::ConfirmDeployment: Label = Localized(TEXT("tactical.action-confirm"), TEXT("CONFIRM")); break;
	case ETacticalHudActionType::Move: Label = Localized(TEXT("tactical.action-move"), TEXT("MOVE")); break;
	case ETacticalHudActionType::AttackUnit: Label = Localized(TEXT("tactical.action-attack"), TEXT("ATTACK")); break;
	case ETacticalHudActionType::ProjectSignal: Label = Localized(TEXT("tactical.action-signal"), TEXT("SIGNAL PRESSURE")); break;
	case ETacticalHudActionType::AttackTerrain: Label = Localized(TEXT("tactical.action-fire-cell"), TEXT("FIRE AT CELL")); break;
	case ETacticalHudActionType::Reload: Label = Localized(TEXT("tactical.action-reload"), TEXT("RELOAD")); break;
	case ETacticalHudActionType::EjectMagazine: Label = Localized(TEXT("tactical.action-eject-magazine"), TEXT("EJECT MAGAZINE")); break;
	case ETacticalHudActionType::ChangeStance:
		Label = Action.RequestedStance == ETacticalStance::Crouched
			? Localized(TEXT("tactical.action-crouch"), TEXT("CROUCH"))
			: Localized(TEXT("tactical.action-stand"), TEXT("STAND"));
		break;
	case ETacticalHudActionType::OperateDoor:
		Label = Action.bRequestedDoorOpen
			? Localized(TEXT("tactical.action-open-door"), TEXT("OPEN DOOR"))
			: Localized(TEXT("tactical.action-close-door"), TEXT("CLOSE DOOR"));
		break;
	case ETacticalHudActionType::DeployDevice: Label = Localized(TEXT("tactical.action-device"), TEXT("DEPLOY DEVICE")); break;
	case ETacticalHudActionType::InteractObjective: Label = Localized(TEXT("tactical.action-objective"), TEXT("OBJECTIVE")); break;
	case ETacticalHudActionType::Extract: Label = Localized(TEXT("tactical.action-extract"), TEXT("EXTRACT")); break;
	case ETacticalHudActionType::EndTurn: Label = Localized(TEXT("tactical.action-end-turn"), TEXT("END TURN")); break;
	default: Label = Localized(TEXT("tactical.action-generic"), TEXT("ACTION")); break;
	}
	if (Action.ActionPointCost > 0)
	{
		Label = LocalizedFormat(
			TEXT("tactical.action-cost-format"), TEXT("{0}  {1} AP"),
			{ Label, FString::FromInt(Action.ActionPointCost) });
	}
	return Label;
}

void UUEGTTacticalHudWidget::RefreshSlate()
{
	using namespace UEGTTacticalHudPrivate;

	if (!MissionText.IsValid() || !PhaseText.IsValid() || !FogText.IsValid() || !StatusText.IsValid()
		|| !HoverText.IsValid() || !HoverPanel.IsValid() || !RosterBox.IsValid()
		|| !ObjectiveBox.IsValid() || !ActionBox.IsValid())
	{
		return;
	}
	RosterBox->ClearChildren();
	ObjectiveBox->ClearChildren();
	ActionBox->ClearChildren();
	RenderedSectionLabels.Reset();
	RenderedActionLabels.Reset();
	RenderedActionTooltips.Reset();
	RenderedUnitSummaries.Reset();
	HoverPanel->SetVisibility(CurrentSnapshot.bSucceeded && CurrentSnapshot.Hover.bHasCell
		? EVisibility::SelfHitTestInvisible
		: EVisibility::Collapsed);
	FogText->SetVisibility(CurrentSnapshot.bSucceeded
		? EVisibility::SelfHitTestInvisible
		: EVisibility::Collapsed);

	if (CurrentDebrief.bAvailable)
	{
		const bool bBaseDefense = CurrentDebrief.OperationType == ETacticalOperationType::BaseDefense;
		FString DebriefTitle = CurrentDebrief.MissionDisplayName.IsEmpty()
			? Localized(TEXT("tactical.debrief-title"), TEXT("TACTICAL DEBRIEF"))
			: CurrentDebrief.MissionDisplayName;
		if (bBaseDefense && !CurrentDebrief.BaseDisplayName.IsEmpty())
		{
			DebriefTitle += FString::Printf(TEXT("  //  %s"), *CurrentDebrief.BaseDisplayName.ToUpper());
		}
		MissionText->SetText(FText::FromString(DebriefTitle));
		const FString OutcomeLabel = CurrentDebrief.bMissionSucceeded
			? Localized(TEXT("tactical.mission-complete"), TEXT("MISSION COMPLETE"))
			: Localized(TEXT("tactical.mission-failed"), TEXT("MISSION FAILED"));
		PhaseText->SetText(FText::FromString(LocalizedFormat(
			TEXT("tactical.debrief-summary-format"),
			TEXT("{0}  •  SCORE {1}  •  CAMPAIGN {2}"),
			{
				OutcomeLabel,
				FString::Printf(TEXT("%+lld"), CurrentDebrief.ScoreAwarded),
				FString::Printf(TEXT("%lld"), CurrentDebrief.CampaignScore)
			})));
		StatusText->SetColorAndOpacity(CurrentDebrief.bMissionSucceeded ? Success : Warning);
		StatusText->SetText(FText::FromString(StatusMessage.IsEmpty()
			? (bBaseDefense
				? (CurrentDebrief.bMissionSucceeded
					? Localized(TEXT("tactical.debrief-defense-success"),
						TEXT("The command relay held. Surviving defenders have returned to base duty."))
					: Localized(TEXT("tactical.debrief-defense-failure"),
						TEXT("The command relay fell. Review casualties and facility damage before returning.")))
				: Localized(TEXT("tactical.debrief-field-status"),
					TEXT("Debrief retained after tactical state cleanup. Continue from the strategic layer.")))
			: StatusMessage));
		const FString PersonnelLabel = Localized(TEXT("tactical.personnel"), TEXT("PERSONNEL"));
		RenderedSectionLabels.Add(PersonnelLabel);
		RosterBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[MakeText(PersonnelLabel, 16, Accent)];
		for (const FTacticalDebriefPersonnelView& Person : CurrentDebrief.Personnel)
		{
			const FString State = Person.bKilled
				? Localized(TEXT("tactical.personnel-kia"), TEXT("KIA"))
				: (Person.bInjured
					? Localized(TEXT("tactical.personnel-wounded"), TEXT("WOUNDED"))
					: Localized(TEXT("tactical.personnel-ready"), TEXT("READY")));
			RosterBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
			[
				MakeText(LocalizedFormat(
					TEXT("tactical.debrief-personnel-format"),
					TEXT("{0}  •  {1}\nHP {2} → {3}   XP +{4}   RANK {5}{6}"),
					{
						Person.DisplayName,
						State,
						FString::FromInt(Person.StartingHealth),
						FString::FromInt(Person.EndingHealth),
						FString::FromInt(Person.ExperienceGained),
						FString::FromInt(Person.NewRank),
						Person.bPromoted ? FString(TEXT(" ↑")) : FString()
					}),
					13,
					Person.bKilled ? Warning : PrimaryText)
			];
			const FString ServiceHistoryLabel = PersonnelServiceHistoryLabel(
				Person.ServiceHistory, Person.Missions);
			RenderedSectionLabels.Add(ServiceHistoryLabel);
			RosterBox->AddSlot().AutoHeight().Padding(12.0f, 0.0f, 0.0f, 2.0f)
			[
				MakeText(ServiceHistoryLabel, 10, Person.bKilled ? Warning : Accent)
			];
			if (Person.bServiceBandAdvanced)
			{
				const FString MilestoneLabel = LocalizedFormat(
					TEXT("personnel.service-milestone-format"),
					TEXT("SERVICE MILESTONE  •  {0}"),
					{ PersonnelServiceBandLabel(Person.ServiceHistory.Band) });
				RenderedSectionLabels.Add(MilestoneLabel);
				RosterBox->AddSlot().AutoHeight().Padding(12.0f, 0.0f, 0.0f, 2.0f)
				[
					MakeText(MilestoneLabel, 10, Person.bKilled ? Warning : Success)
				];
			}
			for (const FName CommendationId : Person.AwardedCommendationIds)
			{
				const FString AwardLabel = LocalizedFormat(
					TEXT("tactical.debrief-commendation-format"),
					TEXT("CITATION AWARDED  •  {0}"),
					{ LocalizedContentName(CommendationId) });
				RenderedSectionLabels.Add(AwardLabel);
				RosterBox->AddSlot().AutoHeight().Padding(12.0f, 0.0f, 0.0f, 2.0f)
				[
					MakeText(AwardLabel, 10, Success)
				];
			}
		}
		const FString RecoveryLabel = bBaseDefense
			? Localized(TEXT("tactical.base-status"), TEXT("BASE STATUS"))
			: Localized(TEXT("tactical.recovery"), TEXT("RECOVERY"));
		RenderedSectionLabels.Add(RecoveryLabel);
		ObjectiveBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			MakeText(RecoveryLabel, 16, Accent)
		];
		if (bBaseDefense)
		{
			ObjectiveBox->AddSlot().AutoHeight()
			[
				MakeText(CurrentDebrief.bMissionSucceeded
					? Localized(TEXT("tactical.perimeter-secured"),
						TEXT("PERIMETER SECURED\nCOMMAND RELAY OPERATIONAL"))
					: Localized(TEXT("tactical.perimeter-breached"),
						TEXT("PERIMETER BREACHED\nFACILITY DAMAGE RECORDED")),
					13, CurrentDebrief.bMissionSucceeded ? Success : Warning)
			];
		}
		else if (CurrentDebrief.RecoveredCargo.IsEmpty())
		{
			ObjectiveBox->AddSlot().AutoHeight()[MakeText(Localized(
				TEXT("tactical.no-recovered-cargo"), TEXT("No recovered cargo")), 13, SecondaryText)];
		}
		for (const FTacticalHudItemView& Item : CurrentDebrief.RecoveredCargo)
		{
			if (bBaseDefense)
			{
				break;
			}
			const FString ItemName = FUEGTLocalizationService::ContentName(Item.ItemId, Item.DisplayName);
			const int64 TotalStorage = static_cast<int64>(Item.Quantity) * Item.UnitMass;
			const int64 TotalSale = static_cast<int64>(Item.Quantity) * Item.UnitSellValue;
			ObjectiveBox->AddSlot().AutoHeight().Padding(0.0f, 2.0f)
			[
				MakeText(Item.UnitSellValue > 0
					? LocalizedFormat(
						TEXT("tactical.recovered-item-format"),
						TEXT("{0}  ×{1}  •  {2} STORAGE  •  SALE {3}"),
						{
							ItemName,
							FString::FromInt(Item.Quantity),
							LexToString(TotalStorage),
							LexToString(TotalSale)
						})
					: LocalizedFormat(
						TEXT("tactical.recovered-item-retain-format"),
						TEXT("{0}  ×{1}  •  {2} STORAGE  •  RETAIN ONLY"),
						{
							ItemName,
							FString::FromInt(Item.Quantity),
							LexToString(TotalStorage)
						}), 13)
			];
		}
		if (!bBaseDefense && !CurrentDebrief.RecoveredCargo.IsEmpty())
		{
			const FString DispositionNote = LocalizedFormat(
				TEXT("tactical.salvage-disposition-note-format"),
				TEXT("Salvage remains aboard {0}. Retain or sell it after landing in Strategic Command."),
				{ CurrentDebrief.CraftDisplayName });
			RenderedSectionLabels.Add(DispositionNote);
			ObjectiveBox->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				MakeText(DispositionNote, 10, SecondaryText)
			];
		}
		HoverText->SetText(FText::GetEmpty());
		const FString ReturnLabel = Localized(
			TEXT("tactical.return-strategic"), TEXT("RETURN TO STRATEGIC COMMAND"));
		RenderedActionLabels.Add(ReturnLabel);
		ActionBox->AddSlot()
		[
			SNew(SButton)
			.IsFocusable(true)
			.ButtonColorAndOpacity(FLinearColor(0.0f, 0.5f, 0.42f, 1.0f))
			.OnClicked_UObject(this, &UUEGTTacticalHudWidget::HandleStrategicReturnClicked)
			[
				MakeText(ReturnLabel, 12, PrimaryText)
			]
		];
		return;
	}

	if (!CurrentSnapshot.bSucceeded)
	{
		MissionText->SetText(FText::FromString(Localized(
			TEXT("tactical.command-title"), TEXT("UEGT  //  TACTICAL COMMAND"))));
		PhaseText->SetText(FText::FromString(Localized(
			TEXT("tactical.no-active-battle"), TEXT("NO ACTIVE TACTICAL BATTLE"))));
		StatusText->SetColorAndOpacity(bStatusIsError ? Warning : SecondaryText);
		FString Message = StatusMessage;
		if (Message.IsEmpty() && !CurrentSnapshot.Diagnostics.IsEmpty())
		{
			Message = FUEGTLocalizationService::DiagnosticText(
				CurrentSnapshot.Diagnostics[0].Code,
				CurrentSnapshot.Diagnostics[0].Message);
		}
		if (Message.IsEmpty())
		{
			Message = Localized(TEXT("tactical.no-active-guidance"),
				TEXT("Begin or load a campaign, then deploy a transport to a tactical site."));
		}
		StatusText->SetText(FText::FromString(Message));
		RosterBox->AddSlot().AutoHeight()[MakeText(Localized(
			TEXT("tactical.control-hints"),
			TEXT("WASD / LEFT STICK  PAN\nQ · E / RIGHT STICK  ORBIT\nWHEEL / TRIGGERS  ZOOM\nTAB / DPAD  SELECT UNIT")), 13, SecondaryText)];
		ObjectiveBox->AddSlot().AutoHeight()[MakeText(Localized(
			TEXT("tactical.activation-help"),
			TEXT("The command HUD activates when a generated battle is present in the campaign.")), 13, SecondaryText)];
		HoverText->SetText(FText::GetEmpty());
		return;
	}

	const bool bBaseDefense = CurrentSnapshot.OperationType == ETacticalOperationType::BaseDefense;
	const FString MissionTheater = bBaseDefense
		? (CurrentSnapshot.BaseDisplayName.IsEmpty()
			? Localized(TEXT("tactical.base-defense"), TEXT("BASE DEFENSE"))
			: CurrentSnapshot.BaseDisplayName.ToUpper())
		: Localized(TEXT("tactical.field-operation"), TEXT("FIELD OPERATION"));
	MissionText->SetText(FText::FromString(LocalizedFormat(
		TEXT("tactical.mission-heading-format"),
		TEXT("{0}  //  {1}  //  LEVEL {2} OF {3}"),
		{
			CurrentSnapshot.MissionDisplayName.ToUpper(),
			MissionTheater,
			FString::FromInt(CurrentSnapshot.ViewedLevel + 1),
			FString::FromInt(CurrentSnapshot.Levels)
		})));
	PhaseText->SetText(FText::FromString(bBaseDefense
		? LocalizedFormat(
			TEXT("tactical.phase-defense-format"),
			TEXT("TURN {0} / {1}  •  {2}  •  WIND {3} {4}  •  HOLD THE RELAY"),
			{
				FString::FromInt(CurrentSnapshot.TurnNumber),
				FString::FromInt(CurrentSnapshot.TurnLimit),
				PhaseLabel(CurrentSnapshot.Phase),
				WindLabel(CurrentSnapshot.WindDirection),
				FString::FromInt(CurrentSnapshot.WindStrength)
			})
		: LocalizedFormat(
			TEXT("tactical.phase-field-format"),
			TEXT("TURN {0} / {1}  •  {2}  •  WIND {3} {4}  •  CARGO {5} / {6}"),
			{
				FString::FromInt(CurrentSnapshot.TurnNumber),
				FString::FromInt(CurrentSnapshot.TurnLimit),
				PhaseLabel(CurrentSnapshot.Phase),
				WindLabel(CurrentSnapshot.WindDirection),
				FString::FromInt(CurrentSnapshot.WindStrength),
				FString::Printf(TEXT("%lld"), CurrentSnapshot.CargoMass),
				FString::FromInt(CurrentSnapshot.CargoCapacity)
			})));
	FogText->SetText(FText::FromString(LocalizedFormat(
		TEXT("tactical.fog-summary-format"),
		TEXT("CURRENT SIGHT {0}  •  SIGNAL MEMORY {1}"),
		{
			FString::FromInt(CurrentSnapshot.VisibleCellCount),
			FString::FromInt(FMath::Max(0, CurrentSnapshot.KnownCellCount - CurrentSnapshot.VisibleCellCount))
		})));
	StatusText->SetColorAndOpacity(bStatusIsError ? Warning : SecondaryText);
	StatusText->SetText(FText::FromString(StatusMessage.IsEmpty()
		? Localized(TEXT("tactical.navigation-hint"),
			TEXT("LMB select / target  •  RMB context action  •  Space end turn  •  Page Up/Down change level"))
		: StatusMessage));

	const FString RosterLabel = Localized(
		TEXT("tactical.roster-title"), TEXT("FIELD TEAM / CONTACTS"));
	RenderedSectionLabels.Add(RosterLabel);
	RosterBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[MakeText(RosterLabel, 16, Accent)];
	if (CurrentSnapshot.Mentorship.bHasMentor)
	{
		const FString MentorshipLabel = PersonnelMentorshipLabel(CurrentSnapshot.Mentorship);
		const FString MentorshipGuidance = PersonnelMentorshipGuidance();
		RenderedSectionLabels.Add(MentorshipLabel);
		RenderedSectionLabels.Add(MentorshipGuidance);
		RosterBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
		[
			MakeText(MentorshipLabel, 10,
				CurrentSnapshot.Mentorship.bActive ? Success : SecondaryText)
		];
		RosterBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			MakeText(MentorshipGuidance, 8, SecondaryText)
		];
	}
	if (CurrentSnapshot.LegacyRelay.bHasSpecialist)
	{
		const FString RelayLabel = PersonnelLegacyRelayLabel(CurrentSnapshot.LegacyRelay);
		const FString RelayGuidance = PersonnelLegacyRelayGuidance();
		RenderedSectionLabels.Add(RelayLabel);
		RenderedSectionLabels.Add(RelayGuidance);
		RosterBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
		[
			MakeText(RelayLabel, 10,
				CurrentSnapshot.LegacyRelay.bActive ? Success : SecondaryText)
		];
		RosterBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			MakeText(RelayGuidance, 8, SecondaryText)
		];
	}
	if (CurrentSnapshot.SquadBonds.ResolvedPersonnelCount >= 2)
	{
		const FString SquadBondTitle = Localized(
			TEXT("personnel.squad-bond-name"), TEXT("FIELD CADENCE"));
		RenderedSectionLabels.Add(SquadBondTitle);
		RosterBox->AddSlot().AutoHeight().Padding(0.0f, 1.0f, 0.0f, 2.0f)
		[
			MakeText(SquadBondTitle, 10, Accent)
		];
		if (CurrentSnapshot.SquadBonds.ActivePairs.IsEmpty())
		{
			const FString InactiveLabel = PersonnelSquadBondInactiveLabel(CurrentSnapshot.SquadBonds);
			RenderedSectionLabels.Add(InactiveLabel);
			RosterBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				MakeText(InactiveLabel, 10, SecondaryText)
			];
		}
		for (const FPersonnelSquadBondPairView& Pair : CurrentSnapshot.SquadBonds.ActivePairs)
		{
			const FString PairLabel = PersonnelSquadBondActiveLabel(Pair);
			RenderedSectionLabels.Add(PairLabel);
			RosterBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				MakeText(PairLabel, 10, Success)
			];
		}
		for (const FPersonnelSquadBondPairView& Pair : CurrentSnapshot.SquadBonds.DevelopingPairs)
		{
			const FString PairLabel = PersonnelSquadBondDevelopingLabel(Pair);
			RenderedSectionLabels.Add(PairLabel);
			RosterBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				MakeText(PairLabel, 10, SecondaryText)
			];
		}
		const FString Guidance = PersonnelSquadBondGuidance();
		RenderedSectionLabels.Add(Guidance);
		RosterBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			MakeText(Guidance, 8, SecondaryText)
		];
	}
	for (const FTacticalHudUnitView& Unit : CurrentSnapshot.Units)
	{
		const bool bPlayer = Unit.Team == ETacticalTeam::Player;
		const FString Prefix = Unit.bLastKnown ? TEXT("◇") : (bPlayer ? (Unit.bSelected ? TEXT("▶") : TEXT(" ")) : TEXT("◆"));
		FString Line;
		if (Unit.bLastKnown)
		{
			Line = LocalizedFormat(
				TEXT("tactical.last-known-unit-format"),
				TEXT("{0} {1}\nLAST KNOWN CONTACT  •  TURN {2}\nHP {3}/{4}   MORALE {5}   SUP {6}   L{7}"),
				{
					Prefix, Unit.DisplayName,
					FString::FromInt(Unit.LastSeenTurnNumber),
					FString::FromInt(Unit.CurrentHealth), FString::FromInt(Unit.MaxHealth),
					FString::FromInt(Unit.CurrentMorale), FString::FromInt(Unit.Suppression),
					FString::FromInt(Unit.Z + 1)
				});
		}
		else
		{
			Line = LocalizedFormat(
				TEXT("tactical.unit-summary-format"),
				TEXT("{0} {1}\nHP {2}/{3}   AP {4}/{5}   MORALE {6}   SUP {7}   L{8}"),
				{
					Prefix, Unit.DisplayName,
					FString::FromInt(Unit.CurrentHealth), FString::FromInt(Unit.MaxHealth),
					FString::FromInt(Unit.RemainingActionPoints), FString::FromInt(Unit.MaxActionPoints),
					FString::FromInt(Unit.CurrentMorale), FString::FromInt(Unit.Suppression),
					FString::FromInt(Unit.Z + 1)
				});
		}
		if (Unit.bSelected && bPlayer)
		{
			const FTacticalHudWeaponView* Weapon = Unit.Weapons.FindByPredicate(
				[this](const FTacticalHudWeaponView& Entry)
				{
					return Entry.ItemId == CurrentSnapshot.EffectiveWeaponItemId;
				});
			if (Weapon != nullptr && Weapon->MagazineCapacity > 0)
			{
				Line += TEXT("\n") + LocalizedFormat(
					TEXT("tactical.weapon-ammunition-format"),
					TEXT("{0}  •  MAG {1}/{2}  •  RESERVE {3} ({4} RDS)  •  PARTIAL {5}  •  NEXT {6}"),
					{
						FUEGTLocalizationService::ContentName(Weapon->ItemId, Weapon->DisplayName),
						FString::FromInt(Weapon->LoadedAmmunition),
						FString::FromInt(Weapon->MagazineCapacity),
						FString::FromInt(Weapon->ReserveMagazines),
						FString::FromInt(Weapon->ReserveAmmunition),
						FString::FromInt(Weapon->PartialReserveMagazines),
						FString::FromInt(Weapon->NextReloadAmmunition)
					});
			}
		}
		RenderedUnitSummaries.Add(Line);
		if (Unit.bLastKnown)
		{
			RosterBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
			[
				MakeText(Line, 13, FLinearColor(0.72f, 0.38f, 0.42f))
			];
		}
		else
		{
			RosterBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
			[
				SNew(SButton)
				.IsFocusable(true)
				.ButtonColorAndOpacity(Unit.bSelected ? FLinearColor(0.0f, 0.36f, 0.48f, 1.0f)
					: (bPlayer ? FLinearColor(0.03f, 0.12f, 0.22f, 1.0f) : FLinearColor(0.28f, 0.035f, 0.06f, 1.0f)))
				.OnClicked_UObject(this, &UUEGTTacticalHudWidget::HandleUnitClicked, Unit.UnitId)
				[
					MakeText(Line, 13, bPlayer ? PrimaryText : FLinearColor(1.0f, 0.55f, 0.58f))
				]
			];
		}
	}

	const FString ObjectiveLabel = Localized(
		TEXT("tactical.mission-objective"), TEXT("MISSION OBJECTIVE"));
	RenderedSectionLabels.Add(ObjectiveLabel);
	ObjectiveBox->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[MakeText(ObjectiveLabel, 16, Accent)];
	for (const FTacticalHudObjectiveView& Objective : CurrentSnapshot.Objectives)
	{
		FString Detail = LocalizedFormat(
			TEXT("tactical.objective-summary-format"),
			TEXT("{0}  •  {1}\nPLAYER {2} / {3}"),
			{
				ObjectiveTypeLabel(Objective.Type), ObjectiveStatusLabel(Objective.Status),
				FString::FromInt(Objective.PlayerInteractions),
				FString::FromInt(Objective.RequiredInteractions)
			});
		if (Objective.Type == ETacticalObjectiveType::Control)
		{
			Detail += LocalizedFormat(
				TEXT("tactical.objective-adversary-format"),
				TEXT("   ADVERSARY {0} / {1}"),
				{ FString::FromInt(Objective.AdversaryInteractions), FString::FromInt(Objective.RequiredInteractions) });
		}
		if (!Objective.RewardItemId.IsNone())
		{
			Detail += LocalizedFormat(
				TEXT("tactical.objective-recovery-format"),
				TEXT("\nRECOVERY {0} ×{1}"),
				{ Objective.RewardDisplayName, FString::FromInt(Objective.RewardQuantity) });
		}
		Detail += LocalizedFormat(
			TEXT("tactical.cell-format"), TEXT("\nCELL {0} · {1} · L{2}"),
			{ FString::FromInt(Objective.X), FString::FromInt(Objective.Y), FString::FromInt(Objective.Z + 1) });
		ObjectiveBox->AddSlot().AutoHeight().Padding(0.0f, 3.0f)
		[
			SNew(SButton)
			.IsFocusable(true)
			.OnClicked_UObject(this, &UUEGTTacticalHudWidget::HandleObjectiveClicked, Objective.ObjectiveId)
			[
				MakeText(Detail, 13, Objective.Status == ETacticalObjectiveStatus::Failed ? Warning : PrimaryText)
			]
		];
	}
	const FString ProtocolLabel = bBaseDefense
		? Localized(TEXT("tactical.defense-protocol"), TEXT("DEFENSE PROTOCOL"))
		: Localized(TEXT("tactical.transport-manifest"), TEXT("TRANSPORT MANIFEST"));
	RenderedSectionLabels.Add(ProtocolLabel);
	ObjectiveBox->AddSlot().AutoHeight().Padding(0.0f, 14.0f, 0.0f, 6.0f)
	[
		MakeText(ProtocolLabel, 16, Accent)
	];
	if (bBaseDefense)
	{
		ObjectiveBox->AddSlot().AutoHeight()
		[
			MakeText(Localized(TEXT("tactical.no-extraction"),
				TEXT("No extraction. Secure the command relay or eliminate the assault force.")), 13, SecondaryText)
		];
	}
	else if (CurrentSnapshot.Cargo.IsEmpty())
	{
		ObjectiveBox->AddSlot().AutoHeight()[MakeText(Localized(
			TEXT("tactical.manifest-empty"), TEXT("Manifest empty")), 13, SecondaryText)];
	}
	for (const FTacticalHudItemView& Item : CurrentSnapshot.Cargo)
	{
		if (bBaseDefense)
		{
			break;
		}
		ObjectiveBox->AddSlot().AutoHeight()[MakeText(FString::Printf(TEXT("%s  ×%d"), *Item.DisplayName, Item.Quantity), 13)];
	}

	FString HoverLine;
	if (CurrentSnapshot.Hover.bHasCell)
	{
		HoverLine = LocalizedFormat(
			TEXT("tactical.hover-cell-format"), TEXT("CELL {0} · {1} · L{2}"),
			{
				FString::FromInt(CurrentSnapshot.Hover.X),
				FString::FromInt(CurrentSnapshot.Hover.Y),
				FString::FromInt(CurrentSnapshot.Hover.Z + 1)
			});
		if (!CurrentSnapshot.Hover.bCellVisible)
		{
			HoverLine += Localized(TEXT("tactical.hover-unseen"), TEXT("  •  UNSEEN"));
		}
		else
		{
			if (CurrentSnapshot.Hover.bHasPathPreview && CurrentSnapshot.Hover.Path.bSucceeded)
			{
				HoverLine += LocalizedFormat(
					TEXT("tactical.hover-path-format"), TEXT("  •  PATH {0} AP"),
					{ FString::FromInt(CurrentSnapshot.Hover.Path.TotalCost) });
			}
			if (CurrentSnapshot.Hover.bHasUnitAttackPreview && CurrentSnapshot.Hover.UnitAttack.bSucceeded)
			{
				HoverLine += LocalizedFormat(
					TEXT("tactical.hover-hit-format"), TEXT("  •  HIT {0}%"),
					{ FString::FromInt(CurrentSnapshot.Hover.UnitAttack.HitChance) });
			}
			if (CurrentSnapshot.Hover.bHasSignalPreview && CurrentSnapshot.Hover.Signal.bSucceeded)
			{
				HoverLine += LocalizedFormat(
					TEXT("tactical.hover-signal-format"), TEXT("  •  SIGNAL {0}%  −{1} MORALE  +{2} SUP"),
					{
						FString::FromInt(CurrentSnapshot.Hover.Signal.HitChance),
						FString::FromInt(CurrentSnapshot.Hover.Signal.MoraleDamage),
						FString::FromInt(CurrentSnapshot.Hover.Signal.SuppressionGain)
					});
			}
			if (CurrentSnapshot.Hover.bHasDeviceTrajectory && CurrentSnapshot.Hover.DeviceTrajectory.bSucceeded)
			{
				HoverLine += LocalizedFormat(
					TEXT("tactical.hover-land-format"), TEXT("  •  LAND {0} · {1} · L{2}"),
					{
						FString::FromInt(CurrentSnapshot.Hover.DeviceTrajectory.LandingX),
						FString::FromInt(CurrentSnapshot.Hover.DeviceTrajectory.LandingY),
						FString::FromInt(CurrentSnapshot.Hover.DeviceTrajectory.LandingZ + 1)
					});
			}
		}
	}
	HoverText->SetText(FText::FromString(HoverLine));

	for (const FTacticalHudActionAvailability& Action : CurrentSnapshot.Actions)
	{
		const FString RenderedLabel = ActionLabel(Action);
		RenderedActionLabels.Add(RenderedLabel);
		const FString Tooltip = Action.bAvailable
			? RenderedLabel
			: FUEGTLocalizationService::DiagnosticText(
				Action.UnavailableReasonCode, Action.UnavailableReason);
		RenderedActionTooltips.Add(Tooltip);
		ActionBox->AddSlot()
		[
			SNew(SButton)
			.IsEnabled(Action.bAvailable)
			.IsFocusable(true)
			.ToolTipText(FText::FromString(Tooltip))
			.ButtonColorAndOpacity(Action.bAvailable
				? FLinearColor(0.0f, 0.28f, 0.4f, 1.0f)
				: FLinearColor(0.045f, 0.055f, 0.075f, 1.0f))
			.OnClicked_UObject(this, &UUEGTTacticalHudWidget::HandleActionClicked, Action.ActionType)
			[
				MakeText(RenderedLabel, 12, Action.bAvailable ? PrimaryText : SecondaryText)
			]
		];
	}
	if (CurrentSnapshot.Phase == ETacticalBattlePhase::Resolved)
	{
		const FString DebriefLabel = Localized(
			TEXT("tactical.open-debrief"), TEXT("OPEN DEBRIEF"));
		RenderedActionLabels.Add(DebriefLabel);
		ActionBox->AddSlot()
		[
			SNew(SButton)
			.IsFocusable(true)
			.ButtonColorAndOpacity(FLinearColor(0.0f, 0.5f, 0.42f, 1.0f))
			.OnClicked_UObject(this, &UUEGTTacticalHudWidget::HandleDebriefClicked)
			[
				MakeText(DebriefLabel, 12, PrimaryText)
			]
		];
	}
}

FReply UUEGTTacticalHudWidget::HandleActionClicked(const ETacticalHudActionType ActionType)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->ExecuteHudAction(ActionType);
	}
	return FReply::Handled();
}

FReply UUEGTTacticalHudWidget::HandleDebriefClicked()
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->ResolveTacticalDebrief();
	}
	return FReply::Handled();
}

FReply UUEGTTacticalHudWidget::HandleStrategicReturnClicked()
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->ReturnToStrategicCommand();
	}
	return FReply::Handled();
}

FReply UUEGTTacticalHudWidget::HandleUnitClicked(const FGuid UnitId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->SelectOrTargetTacticalUnit(UnitId);
	}
	return FReply::Handled();
}

FReply UUEGTTacticalHudWidget::HandleObjectiveClicked(const FName ObjectiveId)
{
	if (AUEGTTacticalPlayerController* Controller = Cast<AUEGTTacticalPlayerController>(GetOwningPlayer()))
	{
		Controller->TargetTacticalObjective(ObjectiveId);
	}
	return FReply::Handled();
}
