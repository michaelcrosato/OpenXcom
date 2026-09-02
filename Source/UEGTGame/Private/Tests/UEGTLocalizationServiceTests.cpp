// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Content/ContentPackageCatalog.h"
#include "Localization/UEGTLocalizationService.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUEGTLocalizationCatalogTest,
	"UEGT.Core.Game.Localization.CatalogAndLocaleSnapshots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUEGTLocalizationCatalogTest::RunTest(const FString& Parameters)
{
	const FString Filename = FUEGTLocalizationService::GetDefaultCatalogFilename();
	const FUEGTLocalizationLoadResult Loaded = FUEGTLocalizationService::LoadCatalogFile(Filename);
	TestTrue(TEXT("Authored UI localization catalog loads strictly"), Loaded.bSucceeded);
	TestEqual(TEXT("Localization schema is explicit"), Loaded.Catalog.SchemaVersion, 1);
	TestEqual(TEXT("Localization catalog uses the original UEGT namespace"),
		Loaded.Catalog.CatalogId, FName(TEXT("uegt.ui")));
	TestEqual(TEXT("English remains the source culture"), Loaded.Catalog.SourceCulture, FString(TEXT("en")));
	TestEqual(TEXT("All five selectable cultures are authored"), Loaded.Catalog.Cultures.Num(), 5);
	TestEqual(TEXT("The complete shell, strategic and tactical systems, Mutual Aid policies, Signal Watch, Works Cadre, Works Charters, Enduring Beacon, relay pressure, interception safeguards, and base specialization expose one thousand four hundred seventy-six localized keys"),
		Loaded.Catalog.Entries.Num(), 1476);
	if (!Loaded.bSucceeded)
	{
		return false;
	}
	const FContentCatalogLoadResult Content = FContentPackageCatalog::LoadDirectory(
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Rules")));
	TestTrue(TEXT("Archive localization coverage is checked against the active authored rule catalog"),
		Content.bSucceeded && Content.RuleSet.ArchiveEntries.Num() == 9);
	for (const TPair<FName, FKnowledgeArchiveEntryRule>& Pair : Content.RuleSet.ArchiveEntries)
	{
		FString NormalizedId = Pair.Key.ToString().ToLower();
		NormalizedId.ReplaceInline(TEXT("_"), TEXT("-"));
		const FString Prefix = FString::Printf(TEXT("content.%s."), *NormalizedId);
		const FUEGTLocalizedTextEntry* Name = Loaded.Catalog.Entries.Find(Prefix + TEXT("name"));
		const FUEGTLocalizedTextEntry* Summary = Loaded.Catalog.Entries.Find(Prefix + TEXT("summary"));
		const FUEGTLocalizedTextEntry* Body = Loaded.Catalog.Entries.Find(Prefix + TEXT("body"));
		TestTrue(*FString::Printf(TEXT("Archive record %s has complete staged name/summary/body fields"), *Pair.Key.ToString()),
			Name != nullptr && Summary != nullptr && Body != nullptr);
		if (Name != nullptr && Summary != nullptr && Body != nullptr)
		{
			TestEqual(*FString::Printf(TEXT("Archive record %s name fallback stays aligned with the rule"), *Pair.Key.ToString()),
				Name->Source, Pair.Value.DisplayName);
			TestEqual(*FString::Printf(TEXT("Archive record %s summary fallback stays aligned with the rule"), *Pair.Key.ToString()),
				Summary->Source, Pair.Value.Summary);
			TestEqual(*FString::Printf(TEXT("Archive record %s body fallback stays aligned with the rule"), *Pair.Key.ToString()),
				Body->Source, Pair.Value.Body);
		}
	}
	TestEqual(TEXT("All authored contact names are loaded for localization coverage"),
		Content.RuleSet.Contacts.Num(), 3);
	for (const TPair<FName, FContactRule>& Pair : Content.RuleSet.Contacts)
	{
		FString NormalizedId = Pair.Key.ToString().ToLower();
		NormalizedId.ReplaceInline(TEXT("_"), TEXT("-"));
		const FUEGTLocalizedTextEntry* Name = Loaded.Catalog.Entries.Find(
			FString::Printf(TEXT("content.%s.name"), *NormalizedId));
		TestTrue(*FString::Printf(TEXT("Contact %s has a staged localized name"), *Pair.Key.ToString()),
			Name != nullptr);
		if (Name != nullptr)
		{
			TestEqual(*FString::Printf(TEXT("Contact %s name fallback matches its rule"), *Pair.Key.ToString()),
				Name->Source, Pair.Value.DisplayName);
		}
	}
	TestEqual(TEXT("All authored adversary mission names are loaded for localization coverage"),
		Content.RuleSet.AdversaryMissions.Num(), 14);
	for (const TPair<FName, FAdversaryMissionRule>& Pair : Content.RuleSet.AdversaryMissions)
	{
		FString NormalizedId = Pair.Key.ToString().ToLower();
		NormalizedId.ReplaceInline(TEXT("_"), TEXT("-"));
		const FUEGTLocalizedTextEntry* Name = Loaded.Catalog.Entries.Find(
			FString::Printf(TEXT("content.%s.name"), *NormalizedId));
		TestTrue(*FString::Printf(TEXT("Adversary mission %s has a staged localized name"), *Pair.Key.ToString()),
			Name != nullptr);
		if (Name != nullptr)
		{
			TestEqual(*FString::Printf(TEXT("Adversary mission %s name fallback matches its rule"), *Pair.Key.ToString()),
				Name->Source, Pair.Value.DisplayName);
		}
	}
	TestEqual(TEXT("All authored adversary plan names are loaded for localization coverage"),
		Content.RuleSet.AdversaryPlans.Num(), 2);
	for (const TPair<FName, FAdversaryPlanRule>& Pair : Content.RuleSet.AdversaryPlans)
	{
		FString NormalizedId = Pair.Key.ToString().ToLower();
		NormalizedId.ReplaceInline(TEXT("_"), TEXT("-"));
		const FUEGTLocalizedTextEntry* Name = Loaded.Catalog.Entries.Find(
			FString::Printf(TEXT("content.%s.name"), *NormalizedId));
		TestTrue(*FString::Printf(TEXT("Adversary plan %s has a staged localized name"), *Pair.Key.ToString()),
			Name != nullptr);
		if (Name != nullptr)
		{
			TestEqual(*FString::Printf(TEXT("Adversary plan %s name fallback matches its rule"), *Pair.Key.ToString()),
				Name->Source, Pair.Value.DisplayName);
		}
	}
	TestEqual(TEXT("All authored field doctrines are loaded for localization coverage"),
		Content.RuleSet.PersonnelDoctrines.Num(), 4);
	for (const TPair<FName, FPersonnelDoctrineRule>& Pair : Content.RuleSet.PersonnelDoctrines)
	{
		FString NormalizedId = Pair.Key.ToString().ToLower();
		NormalizedId.ReplaceInline(TEXT("_"), TEXT("-"));
		const FString Prefix = FString::Printf(TEXT("content.%s."), *NormalizedId);
		const FUEGTLocalizedTextEntry* Name = Loaded.Catalog.Entries.Find(Prefix + TEXT("name"));
		const FUEGTLocalizedTextEntry* Summary = Loaded.Catalog.Entries.Find(Prefix + TEXT("summary"));
		TestTrue(*FString::Printf(TEXT("Doctrine %s has staged name and summary fields"), *Pair.Key.ToString()),
			Name != nullptr && Summary != nullptr);
		if (Name != nullptr && Summary != nullptr)
		{
			TestEqual(*FString::Printf(TEXT("Doctrine %s name fallback matches its rule"), *Pair.Key.ToString()),
				Name->Source, Pair.Value.DisplayName);
			TestEqual(*FString::Printf(TEXT("Doctrine %s summary fallback matches its rule"), *Pair.Key.ToString()),
				Summary->Source, Pair.Value.Summary);
		}
	}
	TestEqual(TEXT("All authored personnel commendations are loaded for localization coverage"),
		Content.RuleSet.PersonnelCommendations.Num(), 4);
	for (const TPair<FName, FPersonnelCommendationRule>& Pair : Content.RuleSet.PersonnelCommendations)
	{
		FString NormalizedId = Pair.Key.ToString().ToLower();
		NormalizedId.ReplaceInline(TEXT("_"), TEXT("-"));
		const FString Prefix = FString::Printf(TEXT("content.%s."), *NormalizedId);
		const FUEGTLocalizedTextEntry* Name = Loaded.Catalog.Entries.Find(Prefix + TEXT("name"));
		const FUEGTLocalizedTextEntry* Summary = Loaded.Catalog.Entries.Find(Prefix + TEXT("summary"));
		TestTrue(*FString::Printf(TEXT("Commendation %s has staged name and summary fields"), *Pair.Key.ToString()),
			Name != nullptr && Summary != nullptr);
		if (Name != nullptr && Summary != nullptr)
		{
			TestEqual(*FString::Printf(TEXT("Commendation %s name fallback matches its rule"), *Pair.Key.ToString()),
				Name->Source, Pair.Value.DisplayName);
			TestEqual(*FString::Printf(TEXT("Commendation %s summary fallback matches its rule"), *Pair.Key.ToString()),
				Summary->Source, Pair.Value.Summary);
		}
	}
	const TArray<FName> ArchiveCategoryIds = {
		TEXT("category.command"), TEXT("category.operations"), TEXT("category.science"),
		TEXT("category.engineering"), TEXT("category.aeronautics")
	};
	for (const FName CategoryId : ArchiveCategoryIds)
	{
		TestTrue(*FString::Printf(TEXT("Archive category %s has a staged content name"), *CategoryId.ToString()),
			Loaded.Catalog.Entries.Contains(FString::Printf(
				TEXT("content.%s.name"), *CategoryId.ToString())));
	}
	const TArray<FString> DiagnosticFamilyKeys = {
		TEXT("diagnostic.generic-capacity"), TEXT("diagnostic.generic-unknown"),
		TEXT("diagnostic.generic-invalid"), TEXT("diagnostic.generic-insufficient"),
		TEXT("diagnostic.generic-unavailable"), TEXT("diagnostic.generic-overflow"),
		TEXT("diagnostic.generic-missing"), TEXT("diagnostic.generic-already"),
		TEXT("diagnostic.generic-pending"), TEXT("diagnostic.generic-failed"),
		TEXT("diagnostic.generic-rejected")
	};
	for (const FString& Key : DiagnosticFamilyKeys)
	{
		TestTrue(*FString::Printf(TEXT("Diagnostic family %s is staged"), *Key),
			Loaded.Catalog.Entries.Contains(Key));
	}
	const TArray<FName> ExactDiagnosticCodes = {
		TEXT("stale_command"), TEXT("campaign_unavailable"), TEXT("campaign_concluded"),
		TEXT("tactical_operation_pending"), TEXT("base_assault_pending"), TEXT("insufficient_funds"),
		TEXT("personnel_assigned_to_craft"), TEXT("personnel_staffing_committed"),
		TEXT("research_prerequisite_missing"), TEXT("research_facility_missing"),
		TEXT("manufacturing_facility_missing"), TEXT("manufacturing_research_missing"),
		TEXT("tactical_deployment_unconfirmed"), TEXT("insufficient_action_points"),
		TEXT("tactical_target_out_of_range"), TEXT("no_tactical_line_of_sight"),
		TEXT("tactical_weapon_empty"), TEXT("invalid_tactical_ejection"),
		TEXT("tactical_reload_no_improvement"), TEXT("tactical_magazine_inventory_full"),
		TEXT("save_not_found"),
		TEXT("no_valid_save_candidate"), TEXT("invalid_slot_name"),
		TEXT("facility_repair_active"), TEXT("facility_supports_active_production"),
		TEXT("facility_scientist_capacity_required"),
		TEXT("facility_engineer_capacity_required"),
		TEXT("facility_craft_capacity_required"), TEXT("facility_undamaged"),
		TEXT("research_already_active"), TEXT("research_already_completed"),
		TEXT("personnel_capacity_full"), TEXT("personnel_capacity_exceeded"),
		TEXT("scientist_capacity_exceeded"), TEXT("engineer_capacity_exceeded"),
		TEXT("manufacturing_materials_missing"), TEXT("manufacturing_quantity_below_minimum"),
		TEXT("insufficient_inventory"), TEXT("storage_capacity_exceeded"),
		TEXT("invalid_mutual_aid_route_policy"), TEXT("mutual_aid_signal_escort_funds"),
		TEXT("mutual_aid_signal_surety_already_committed"),
		TEXT("mutual_aid_signal_surety_departed"),
		TEXT("mutual_aid_signal_surety_unneeded"),
		TEXT("mutual_aid_relief_priority_already_front"),
		TEXT("mutual_aid_relief_priority_departed"),
		TEXT("mutual_aid_relief_priority_departed_ahead"),
		TEXT("mutual_aid_relief_stand_down_departed"),
		TEXT("mutual_aid_relief_stand_down_inventory_overflow"),
		TEXT("mutual_aid_relief_stand_down_source_storage"),
		TEXT("mutual_aid_relief_stand_down_projection_overflow"),
		TEXT("mutual_aid_relief_diversion_same_destination"),
		TEXT("mutual_aid_relief_diversion_departed"),
		TEXT("mutual_aid_relief_diversion_destination_storage"),
		TEXT("mutual_aid_relief_diversion_projection_overflow"),
		TEXT("mutual_aid_relay_waypoint_same_plan"),
		TEXT("mutual_aid_relay_waypoint_departed"),
		TEXT("mutual_aid_relay_waypoint_base"),
		TEXT("mutual_aid_relay_waypoint_projection_overflow"),
		TEXT("mutual_aid_relay_waypoint_diversion_pending"),
		TEXT("mutual_aid_interdiction_overflow"),
		TEXT("facility_no_space"), TEXT("facility_research_missing"),
		TEXT("facility_disconnects_base"), TEXT("facility_repair_not_active"),
		TEXT("craft_capacity_full"), TEXT("craft_capacity_exceeded"),
		TEXT("craft_research_missing"), TEXT("equipment_research_missing"),
		TEXT("personnel_research_missing"), TEXT("personnel_unavailable"),
		TEXT("personnel_already_at_base"), TEXT("craft_already_at_base"),
		TEXT("craft_roster_assigned"), TEXT("agent_already_assigned"),
		TEXT("pilot_already_assigned"), TEXT("craft_agent_capacity_exceeded"),
		TEXT("craft_pilot_missing"), TEXT("craft_agents_missing"),
		TEXT("contact_unavailable"), TEXT("craft_unavailable"),
		TEXT("craft_has_no_weapons"), TEXT("invalid_craft_rearm_policy"),
		TEXT("craft_rearm_not_needed"),
		TEXT("craft_service_not_needed"), TEXT("craft_service_not_active"),
		TEXT("insufficient_ammunition"),
		TEXT("insufficient_interception_fuel"), TEXT("insufficient_deployment_fuel"),
		TEXT("site_already_targeted"), TEXT("unknown_contact"),
		TEXT("unknown_site"), TEXT("unknown_craft"),
		TEXT("no_active_campaign"), TEXT("invalid_save_directory"),
		TEXT("save_directory_create_failed"), TEXT("temporary_write_failed"),
		TEXT("temporary_verification_failed"), TEXT("backup_rotation_failed"),
		TEXT("save_commit_failed"), TEXT("save_candidate_unreadable"),
		TEXT("save_candidate_invalid"), TEXT("save_checksum_mismatch"),
		TEXT("incompatible_content"), TEXT("content_not_ready"),
		TEXT("content_fingerprint_mismatch"), TEXT("missing_content_packages"),
		TEXT("legacy_save_without_checksum"), TEXT("unsupported_format_version"),
		TEXT("unsupported_write_version"), TEXT("serialization_failed"),
		TEXT("invalid_json"), TEXT("invalid_field_type"),
		TEXT("invalid_field_value"), TEXT("missing_field"), TEXT("unknown_field"),
		TEXT("invalid_campaign_id"), TEXT("invalid_build_version"),
		TEXT("invalid_save_time"), TEXT("invalid_random_state"),
		TEXT("invalid_difficulty"), TEXT("save_migrated"), TEXT("save_recovered"),
		TEXT("personnel_doctrine_choice_unavailable"), TEXT("unknown_personnel_doctrine"),
		TEXT("invalid_personnel_doctrine"), TEXT("personnel_doctrine_maximum"),
		TEXT("personnel_doctrine_no_effect"), TEXT("invalid_personnel_commendation"),
		TEXT("invalid_memorial_doctrine"), TEXT("invalid_memorial_commendation"),
		TEXT("invalid_personnel_progression"), TEXT("invalid_memorial_progression"),
		TEXT("invalid_adversary_config"), TEXT("invalid_base_defense_overcharge_config"),
		TEXT("insufficient_base_defense_overcharge_funds"),
		TEXT("invalid_interception_posture"), TEXT("invalid_interception_withdrawal_doctrine"),
		TEXT("invalid_interception_rules"), TEXT("contact_not_engaged"),
		TEXT("invalid_contact_rule"), TEXT("interception_round_overflow"),
		TEXT("interception_range_overflow"), TEXT("invalid_contact_route"),
		TEXT("invalid_craft_route"),
		TEXT("interception_wake_snare_rounds_required"),
		TEXT("interception_wake_snare_no_route_progress"),
		TEXT("invalid_base_defense_fire_doctrine"),
		TEXT("regional_crisis_not_severe"), TEXT("insufficient_crisis_mobilization_support"),
		TEXT("coalition_aid_target_withdrawn"),
		TEXT("coalition_restoration_compact_required"),
		TEXT("coalition_restoration_target_not_withdrawn"),
		TEXT("coalition_restoration_support_required"),
		TEXT("invalid_coalition_emergency_vote_config"),
		TEXT("invalid_coalition_emergency_vote_state"),
		TEXT("coalition_emergency_vote_compact_required"),
		TEXT("coalition_emergency_vote_already_used"),
		TEXT("coalition_emergency_vote_target_not_withdrawn"),
		TEXT("coalition_emergency_vote_member_required"),
		TEXT("coalition_emergency_vote_rejected"),
		TEXT("coalition_emergency_vote_no_effect")
	};
	const TArray<FName> ContentDiagnosticCodes = {
		TEXT("content_directory_missing"), TEXT("no_content_packages"), TEXT("file_read_failed"),
		TEXT("invalid_package_id"), TEXT("invalid_package_version"),
		TEXT("unsupported_schema_version"), TEXT("duplicate_package_id"),
		TEXT("missing_dependency"), TEXT("dependency_cycle"),
		TEXT("invalid_rule_id"), TEXT("duplicate_rule_in_package"),
		TEXT("unexpected_rule_override"), TEXT("missing_rule_override_target"),
		TEXT("invalid_rule_value"), TEXT("missing_research_reference"),
		TEXT("region_funding_weight_overflow"), TEXT("missing_ammunition_reference"),
		TEXT("missing_tactical_ammunition_reference"),
		TEXT("missing_manufacturing_input_reference"), TEXT("duplicate_manufacturing_input"),
		TEXT("self_archive_reference"),
		TEXT("missing_archive_reference"), TEXT("missing_adversary_plan_opening"),
		TEXT("invalid_adversary_plan_opening"), TEXT("missing_adversary_plan_reference"),
		TEXT("missing_region_reference"), TEXT("missing_adversary_plan_branch"),
		TEXT("invalid_adversary_plan_branch"), TEXT("missing_contact_reference"),
		TEXT("landing_site_threat_overflow"), TEXT("missing_landing_tactical_mapping"),
		TEXT("orphaned_adversary_plan_mission"), TEXT("adversary_weight_overflow"),
		TEXT("missing_opening_adversary_mission"), TEXT("invalid_tactical_floor_reference"),
		TEXT("invalid_tactical_obstacle_reference"), TEXT("invalid_tactical_door_reference"),
		TEXT("invalid_tactical_vertical_connector_reference"),
		TEXT("missing_tactical_unit_reference"), TEXT("missing_tactical_reward_reference"),
		TEXT("duplicate_tactical_contact_mapping"), TEXT("orphaned_landing_tactical_mapping"),
		TEXT("missing_research_facility_reference"),
		TEXT("cyclic_research_facility_requirement"), TEXT("missing_unlock_reference"),
		TEXT("research_cycle")
	};
	for (const FName Code : ExactDiagnosticCodes)
	{
		FString NormalizedCode = Code.ToString().ToLower();
		NormalizedCode.ReplaceInline(TEXT("_"), TEXT("-"));
		TestTrue(*FString::Printf(TEXT("High-value diagnostic %s is staged"), *Code.ToString()),
			Loaded.Catalog.Entries.Contains(FString::Printf(
				TEXT("diagnostic.%s"), *NormalizedCode)));
	}
	for (const FName Code : ContentDiagnosticCodes)
	{
		FString NormalizedCode = Code.ToString().ToLower();
		NormalizedCode.ReplaceInline(TEXT("_"), TEXT("-"));
		TestTrue(*FString::Printf(TEXT("Player-facing content diagnostic %s is staged"), *Code.ToString()),
			Loaded.Catalog.Entries.Contains(FString::Printf(
				TEXT("diagnostic.%s"), *NormalizedCode)));
	}

	TestEqual(TEXT("French menu locale snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("menu.hero-title"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("LE FRONT DU SIGNAL")));
	TestEqual(TEXT("Regional French culture normalizes to the authored language"),
		Loaded.Catalog.Resolve(TEXT("menu.hero-title"), TEXT("fallback"), TEXT("fr-CA")),
		FString(TEXT("LE FRONT DU SIGNAL")));
	TestEqual(TEXT("French user-mod reload control snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("menu.reload-content"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("RECHARGER CONTENU + MODS")));
	TestEqual(TEXT("French user-mod reload success snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("status.content-reloaded"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("Contenu et mods utilisateur rechargés avec succès.")));
	TestEqual(TEXT("German settings locale snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("settings.display"), TEXT("fallback"), TEXT("de")),
		FString(TEXT("ANZEIGE + LEISTUNG")));
	TestEqual(TEXT("Spanish menu locale snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("menu.begin-campaign"), TEXT("fallback"), TEXT("es")),
		FString(TEXT("INICIAR NUEVA CAMPAÑA")));
	TestEqual(TEXT("Japanese settings locale snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("settings.back"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("司令画面へ戻る")));
	TestEqual(TEXT("French stand-off interception posture snapshot is exact"),
		Loaded.Catalog.Resolve(
			TEXT("strategic.interception-posture-stand-off"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("ÉCRAN À DISTANCE")));
	TestEqual(TEXT("French interception tradeoff format preserves signed values"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.interception-posture-detail-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("+20"), TEXT("+25") }),
		FString(TEXT("TIR +20  •  RIPOSTE +25")));
	TestEqual(TEXT("Japanese close-assault guidance snapshot is exact"),
		Loaded.Catalog.Resolve(
			TEXT("strategic.interception-posture-close-tooltip"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("距離を詰めて射撃精度を高めますが、編隊も敵の反撃にさらされます。")));
	TestEqual(TEXT("French linked-wing coordination preserves exact support and signed modifiers"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.interception-coordination-detail-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("ESCADRE RELIÉE"), TEXT("1"), TEXT("+5"), TEXT("-5") }),
		FString(TEXT("ESCADRE RELIÉE  •  APPUI 1  •  TIR +5  •  RIPOSTE -5")));
	TestEqual(TEXT("Japanese linked-wing guidance states the cap and no-extra-draw contract"),
		Loaded.Catalog.Resolve(
			TEXT("strategic.interception-coordination-tooltip"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("先導機以外の各航空機が射撃命中率に+5、反撃命中率に-5を加え、上限は15です。連携による追加の乱数消費はありません。")));
	TestEqual(TEXT("French Signal Shear format preserves completed rounds and signed modifiers"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.interception-contact-maneuver-detail-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("CISAILLEMENT DU SIGNAL"), TEXT("2"), TEXT("-10"), TEXT("-15") }),
		FString(TEXT("CISAILLEMENT DU SIGNAL  •  TOURS 2  •  TIR -10  •  RIPOSTE -15")));
	TestEqual(TEXT("Japanese contact maneuver guidance states both phase boundaries and no-extra-draw contract"),
		Loaded.Catalog.Resolve(
			TEXT("strategic.interception-contact-maneuver-tooltip"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("接触はベクトル観測から始まり、船体が35%を超えた状態で2ラウンド完了すると信号シアへ移行し、船体が35%以下になると離脱線反撃へ切り替えます。機動選択による追加の乱数消費はありません。")));
	TestEqual(TEXT("French precision-screen doctrine snapshot is exact"),
		Loaded.Catalog.Resolve(
			TEXT("strategic.base-defense-doctrine-precision"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("ÉCRAN DE PRÉCISION")));
	TestEqual(TEXT("French doctrine preview preserves supplied and damage values"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.base-defense-doctrine-detail-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("1"), TEXT("3"), TEXT("180"), TEXT("83") }),
		FString(TEXT("PRÊTES 1/3  •  MAX 180  •  ~83")));
	TestEqual(TEXT("Japanese breach-breaker doctrine guidance snapshot is exact"),
		Loaded.Catalog.Resolve(
			TEXT("strategic.base-defense-doctrine-breach-tooltip"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("威力の高い砲台、次に命中率の高い砲台を優先し、同値は安定ID順で決定します。")));
	TestEqual(TEXT("French Grid Overcharge doctrine snapshot is exact"),
		Loaded.Catalog.Resolve(
			TEXT("strategic.base-defense-doctrine-overcharge"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("SURCHARGE DU RÉSEAU")));
	TestEqual(TEXT("Japanese Grid Overcharge guidance preserves the configured boost"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.base-defense-doctrine-overcharge-tooltip-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("15"), TEXT("125") }),
		FString(TEXT("高威力の砲台を優先し、命中率を15加算、ダメージを125%に強化して、表示された脅威連動の緊急グリッド費用を投入します。")));
	TestEqual(TEXT("French Grid Overcharge cost preserves exact campaign funds"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.base-defense-doctrine-cost-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("150000") }),
		FString(TEXT("COÛT D'URGENCE DU RÉSEAU 150000")));
	TestEqual(TEXT("French formation-withdrawal action preserves the authoritative craft count"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.interception-withdraw-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("2") }),
		FString(TEXT("ROMPRE LE CONTACT  •  FORMATION 2")));
	TestEqual(TEXT("Japanese formation-withdrawal guidance states its no-roll contract"),
		Loaded.Catalog.Resolve(
			TEXT("strategic.interception-withdraw-tooltip"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("戦闘射撃や乱数消費を行わず、現場の全航空機を帰投させます。接触は探知状態のままです。")));
	TestEqual(TEXT("German formation-withdrawal result preserves the returning craft count"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.contact-interception-withdrawn-format"), TEXT("fallback"), TEXT("de")),
			{ TEXT("2") }),
		FString(TEXT("Formationsrückzug befohlen • zurückkehrende Fluggeräte: 2.")));
	TestEqual(TEXT("French interception withdrawal heading is exact"),
		Loaded.Catalog.Resolve(
			TEXT("strategic.interception-withdrawal-heading"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("DOCTRINE DE RETRAIT  //  CHOISIR UNE COMMANDE")));
	TestEqual(TEXT("French evasive relay action preserves craft identity and hull"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.interception-relay-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("Aiguille 02"), TEXT("25"), TEXT("100") }),
		FString(TEXT("RELAIS D’ÉVASION  •  Aiguille 02  •  COQUE 25/100")));
	TestEqual(TEXT("Japanese evasive relay guidance states its no-round contract"),
		Loaded.Catalog.Resolve(
			TEXT("strategic.interception-relay-tooltip"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("戦闘ラウンドや乱数消費なしで、耐久率が最も低い航空機だけを帰投させます。残りは交戦を維持します。")));
	TestEqual(TEXT("German evasive relay result preserves craft and remaining count"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.contact-interception-relay-withdrawn-format"),
				TEXT("fallback"), TEXT("de")),
			{ TEXT("Kestrel Zwei"), TEXT("1") }),
		FString(TEXT("Ausweichstaffel befohlen • Kestrel Zwei kehrt zurück • weiter gebundene Fluggeräte: 1.")));
	TestEqual(TEXT("French Wake Snare action preserves exact round gate and digital delay"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.interception-wake-snare-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("2"), TEXT("2"), TEXT("30:00") }),
		FString(TEXT("PIÈGE DE SILLAGE  •  TOURS 2/2  •  RETARD 30:00")));
	TestEqual(TEXT("Japanese Wake Snare guidance states its route and no-draw contract"),
		Loaded.Catalog.Resolve(
			TEXT("strategic.interception-wake-snare-tooltip"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("戦闘を2ラウンド完了した後、編隊全体を帰投させ、接触の航路進行を最大30分巻き戻します。戦闘ラウンドも乱数も消費しません。")));
	TestEqual(TEXT("Spanish invalid withdrawal doctrine diagnostic is exact"),
		Loaded.Catalog.Resolve(
			TEXT("diagnostic.invalid-interception-withdrawal-doctrine"),
			TEXT("fallback"), TEXT("es")),
		FString(TEXT("La doctrina de retirada de intercepción seleccionada no es compatible.")));
	TestEqual(TEXT("French gameplay-option locale snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("gameplay.end-turn-safety"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("SÉCURITÉ DE FIN DE TOUR")));
	TestEqual(TEXT("Japanese gameplay guardrail snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("gameplay.guardrails"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("権威的な安全策")));
	TestEqual(TEXT("French new-campaign accessibility preset snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("menu.accessibility-preset"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("PRÉRÉGLAGE D'ACCESSIBILITÉ")));
	TestEqual(TEXT("Japanese maximum-clarity detail snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("menu.preset-clarity-detail"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("UI 130%  •  低速カメラ  •  常に確認")));
	TestEqual(TEXT("Spanish sustained-funding snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("menu.funding-sustained"), TEXT("fallback"), TEXT("es")),
		FString(TEXT("PACTO SOSTENIDO")));
	TestEqual(TEXT("French campaign-founding title snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("strategic.founding-title"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("UEGT  //  FONDER LE PREMIER QG")));
	TestEqual(TEXT("French campaign-initialization status snapshot is exact"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.campaign-initialized-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("20350101") }),
		FString(TEXT("Campagne initialisée avec la graine déterministe 20350101.")));
	TestEqual(TEXT("German strategic decision-pause snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("strategic.decision-pause"), TEXT("fallback"), TEXT("de")),
		FString(TEXT("ENTSCHEIDUNGSPAUSE: Ein Feldeinsatz ist zur taktischen Entsendung bereit.")));
	TestEqual(TEXT("Spanish strategic time-control snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("strategic.time-one-hour"), TEXT("fallback"), TEXT("es")),
		FString(TEXT("1 HORA")));
	TestEqual(TEXT("Japanese strategic command title snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("strategic.command-title"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("UEGT  //  戦略司令部")));
	TestEqual(TEXT("French save-browser title snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("save.title-load"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("UEGT  //  CHARGER LA CAMPAGNE")));
	TestEqual(TEXT("Spanish recovered-save state snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("save.integrity-recovered"), TEXT("fallback"), TEXT("es")),
		FString(TEXT("COPIA RECUPERADA")));
	TestEqual(TEXT("Japanese save-integrity heading snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("save.integrity-title"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("セーブ整合性")));
	TestEqual(TEXT("French tactical phase snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("tactical.phase-player"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("TOUR DU JOUEUR")));
	TestEqual(TEXT("Japanese tactical action snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("tactical.action-end-turn"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("ターン終了")));
	TestEqual(TEXT("French magazine-ejection action snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("tactical.action-eject-magazine"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("ÉJECTER LE CHARGEUR")));
	TestEqual(TEXT("French tactical ammunition format preserves exact magazine state"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("tactical.weapon-ammunition-format"), TEXT("fallback"), TEXT("fr")),
			{
				TEXT("Fusil de service"), TEXT("4"), TEXT("6"), TEXT("2"),
				TEXT("9"), TEXT("1"), TEXT("6")
			}),
		FString(TEXT("Fusil de service  •  CHARGEUR 4/6  •  RÉSERVE 2 (9 MUN.)  •  PARTIELS 1  •  SUIVANT 6")));
	TestEqual(TEXT("French ejected-magazine feedback preserves retained rounds"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("tactical.magazine-ejected-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("Fusil de service"), TEXT("4") }),
		FString(TEXT("Fusil de service éjecté : 4 munitions conservées.")));
	TestEqual(TEXT("German base-defense heading snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("tactical.base-defense"), TEXT("fallback"), TEXT("de")),
		FString(TEXT("BASISVERTEIDIGUNG")));
	TestEqual(TEXT("French archive title snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("archive.title"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("UEGT  //  ARCHIVES DU SIGNAL")));
	TestEqual(TEXT("Japanese archive authorization snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("archive.authorization"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("研究承認済み記録")));
	TestEqual(TEXT("French authored research name snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("content.research.signal-analysis.name"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("Analyse des signaux anormaux")));
	TestEqual(TEXT("Japanese authored facility name snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("content.facility.operations-hub.name"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("作戦司令部")));
	TestEqual(TEXT("French authored item name snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("content.item.pulse-carbine.name"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("Carabine à impulsions")));
	TestEqual(TEXT("French authored adversary-plan name snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("content.plan.mirror-rain.name"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("Schéma de Pluie miroir")));
	TestEqual(TEXT("Japanese authored coalition-counterplay mission snapshot is exact"),
		Loaded.Catalog.Resolve(
			TEXT("content.mission.ashen-accord-severance.name"),
			TEXT("fallback"), TEXT("ja")),
		FString(TEXT("灰燼協定切断作戦")));
	TestEqual(TEXT("German authored personnel role snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("content.role.engineer.name"), TEXT("fallback"), TEXT("de")),
		FString(TEXT("Systemingenieur")));
	TestEqual(TEXT("French authored field-doctrine name snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("content.doctrine.clear-sight.name"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("Vision nette")));
	TestEqual(TEXT("Japanese authored field-doctrine summary snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("content.doctrine.pathfinder.summary"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("経路規律により、不確かな地形での機動力が向上します。")));
	TestEqual(TEXT("German authored commendation snapshot is exact"),
		Loaded.Catalog.Resolve(TEXT("content.commendation.long-watch.name"), TEXT("fallback"), TEXT("de")),
		FString(TEXT("Stern der langen Wacht")));
	TestEqual(TEXT("French doctrine option format preserves exact level and bonus values"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.personnel-doctrine-option-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("Vision nette"), TEXT("1"), TEXT("3"), TEXT("PV +0  PRÉ +4  VOL +0  MOB +0  FOR +0") }),
		FString(TEXT("Vision nette  •  NIV 1/3\nPV +0  PRÉ +4  VOL +0  MOB +0  FOR +0")));
	TestEqual(TEXT("French inventory format preserves localized item and authoritative values"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.inventory-sellable-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("Fusil de service"), TEXT("2"), TEXT("16"), TEXT("4800") }),
		FString(TEXT("Fusil de service  ×2  •  STOCKAGE 16  •  4800 L'UNITÉ")));
	TestEqual(TEXT("French storage ledger distinguishes production from inbound Mutual Aid"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.storage-reserved-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("18"), TEXT("6"), TEXT("12") }),
		FString(TEXT("18 STOCKÉS  •  6 PRODUCTION  •  12 AIDE EN APPROCHE")));
	TestEqual(TEXT("French Mutual Aid destination header remains concise"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.mutual-aid-destination-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("PATAGONIA CARE ANNEX") }),
		FString(TEXT("VERS PATAGONIA CARE ANNEX")));
	TestEqual(TEXT("French Mutual Aid maximum action remains compact"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.mutual-aid-send-maximum-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("8") }),
		FString(TEXT("MAX. ENVOYER (8)")));
	TestEqual(TEXT("French Rapid Thread summary preserves time, exposure, and forecast delay"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.mutual-aid-route-risk-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("FIL RAPIDE"), TEXT("48"), TEXT("100"), TEXT("24") }),
		FString(TEXT("FIL RAPIDE  •  48 h  •  EXPOSITION 100/100  •  RETARD +24 h SANS ESCORTE")));
	TestEqual(TEXT("French Signal Escort toggle preserves its exact committed funds"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.mutual-aid-signal-escort-on-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("25000") }),
		FString(TEXT("ESCORTE-SIGNAL ACTIVE  •  25000 FONDS")));
	TestEqual(TEXT("French Relay Weave hold preserves exact queue, wait, and arrival"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.mutual-aid-relay-held-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("1"), TEXT("36"), TEXT("108") }),
		FString(TEXT("TRAME RELAIS  •  FILE 1  •  ATTENTE 36 h  •  ARRIVÉE 108 h")));
	TestEqual(TEXT("Japanese Relay Weave activity preserves channel and arrival"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.mutual-aid-relay-active-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("1"), TEXT("2"), TEXT("48") }),
		FString(TEXT("リレー網  •  CH 1/2  •  稼働中  •  到着 48時間")));
	TestEqual(TEXT("French Signal Watch summary preserves staffing and derived relay channels"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.signal-watch-summary-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("1"), TEXT("1"), TEXT("1"), TEXT("1"), TEXT("2") }),
		FString(TEXT("VEILLE SIGNAL  •  SCI 1/1  •  CANAUX RELAIS 1+1=2")));
	TestEqual(TEXT("French Signal Relay consequence preserves the separate specialization channel"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.base-specialization-operational-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("1"), TEXT("CANAL RELAIS") }),
		FString(TEXT("CONSÉQUENCE ACTIVE  •  +1 CANAL RELAIS")));
	TestEqual(TEXT("French Research Enclave consequence preserves its percentage rate"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.base-specialization-rate-operational-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("20"), TEXT("VITESSE DE RECHERCHE") }),
		FString(TEXT("CONSÉQUENCE ACTIVE  •  +20% VITESSE DE RECHERCHE")));
	TestEqual(TEXT("French research project detail preserves the derived rate"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.research-rate-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("20") }),
		FString(TEXT("+20% VITESSE DE RECHERCHE")));
	TestEqual(TEXT("French Fabrication Works consequence preserves its percentage rate"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.base-specialization-rate-operational-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("20"), TEXT("VITESSE DE FABRICATION") }),
		FString(TEXT("CONSÉQUENCE ACTIVE  •  +20% VITESSE DE FABRICATION")));
	TestEqual(TEXT("French manufacturing project detail preserves the derived rate"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.manufacturing-rate-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("20") }),
		FString(TEXT("+20% VITESSE DE FABRICATION")));
	TestEqual(TEXT("French Flight Operations consequence preserves its additional service lane"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.base-specialization-operational-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("1"), TEXT("VOIE D'ENTRETIEN") }),
		FString(TEXT("CONSÉQUENCE ACTIVE  •  +1 VOIE D'ENTRETIEN")));
	TestEqual(TEXT("French Logistics Depot consequence preserves its percentage efficiency"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.base-specialization-rate-operational-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("20"), TEXT("EFFICACITÉ DU STOCKAGE") }),
		FString(TEXT("CONSÉQUENCE ACTIVE  •  +20% EFFICACITÉ DU STOCKAGE")));
	TestEqual(TEXT("French Signal Watch summary preserves specialization and staffed channel components"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.signal-watch-specialization-summary-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("1"), TEXT("2"), TEXT("1"), TEXT("1"), TEXT("1"), TEXT("3") }),
		FString(TEXT("VEILLE SIGNAL  •  SCI 1/2  •  CANAUX RELAIS 1+1+1=3")));
	TestEqual(TEXT("Japanese Signal Watch feedback preserves assignment and total channels"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.signal-watch-updated-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("1"), TEXT("2") }),
		FString(TEXT("信号監視の配置を1名に設定しました。リレーチャンネル2個が稼働中です。")));
	TestEqual(TEXT("French Works Cadre summary preserves staffing and distinct construction and repair front-loads"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.works-cadre-summary-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("2"), TEXT("3"), TEXT("30"), TEXT("10") }),
		FString(TEXT("CADRE TRAVAUX  •  ING 2/3  •  CONSTR. 30 %  •  RÉPAR. 10 %")));
	TestEqual(TEXT("Japanese Works Cadre feedback preserves assignment and both exact front-loads"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.works-cadre-updated-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("3"), TEXT("45"), TEXT("15") }),
		FString(TEXT("作業班の配置を3名に設定しました。今後の建設は45%、修理は15%先行します。")));
	TestEqual(TEXT("French Works Charter option preserves localized policy and asymmetric totals"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.works-charter-option-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("CADENCE D’ASSEMBLAGE"), TEXT("45"), TEXT("15") }),
		FString(TEXT("CADENCE D’ASSEMBLAGE\nCONSTR. 45 %  •  RÉPAR. 15 %")));
	TestEqual(TEXT("Japanese Works Charter feedback preserves policy, exact totals, and immutable clocks"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.works-charter-updated-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("復旧工程"), TEXT("15"), TEXT("45") }),
		FString(TEXT("作業憲章を復旧工程に設定しました。今後の建設は15%、修理は45%先行します。既存の時間は変わりません。")));
	TestEqual(TEXT("French Threadline Retune option preserves route, clock, and exposure"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.mutual-aid-retune-option-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("CHAÎNE VOILÉE"), TEXT("96"), TEXT("50") }),
		FString(TEXT("CHAÎNE VOILÉE\n96 h • EXP 50")));
	TestEqual(TEXT("Japanese Threadline Retune result preserves destination, route, clock, exposure, and arrival"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.mutual-aid-retuned-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("パタゴニア"), TEXT("隠密連鎖"), TEXT("96"), TEXT("50"), TEXT("132") }),
		FString(TEXT("パタゴニア向け支援を隠密連鎖へ再調整しました。96時間、経路危険度50/100、到着予定132時間。貨物、保管枠、護衛、資金は変わりません。")));
	TestEqual(TEXT("French Signal Surety action preserves funding, recovered delay, and arrival"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.mutual-aid-signal-surety-action-format"),
				TEXT("fallback"), TEXT("fr")),
			{ TEXT("25000"), TEXT("84"), TEXT("24") }),
		FString(TEXT("COMMANDER LA GARANTIE SIGNAL\nFONDS 25000 • ARRIVÉE 84 h • GAIN 24 h")));
	TestEqual(TEXT("Japanese Signal Surety result preserves destination, funding, delay, and arrival"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.mutual-aid-signal-surety-commissioned-format"),
				TEXT("fallback"), TEXT("ja")),
			{ TEXT("パタゴニア"), TEXT("25000"), TEXT("24"), TEXT("84") }),
		FString(TEXT("パタゴニア向け支援に信号保証を手配しました。資金25000を投入し、予測遅延24時間を防止、到着予定は84時間です。貨物、保管枠、経路、識別情報、FIFO順は変わりません。")));
	TestEqual(TEXT("Spanish Signal Surety no-effect diagnostic is exact"),
		Loaded.Catalog.Resolve(
			TEXT("diagnostic.mutual-aid-signal-surety-unneeded"),
			TEXT("fallback"), TEXT("es")),
		FString(TEXT("Este convoy no tiene ningún retraso previsto sin resolver que una escolta de señal pueda evitar.")));
	TestEqual(TEXT("French Relief Priority action preserves arrival, recovered wait, and bypass count"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.mutual-aid-relief-priority-action-format"),
				TEXT("fallback"), TEXT("fr")),
			{ TEXT("84"), TEXT("72"), TEXT("1") }),
		FString(TEXT("ÉLEVER LA PRIORITÉ SECOURS\nARRIVÉE 84 h • GAIN 72 h • DEVANCE 1")));
	TestEqual(TEXT("Japanese Relief Priority result preserves destination and exact queue movement"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.mutual-aid-relief-priority-set-format"),
				TEXT("fallback"), TEXT("ja")),
			{ TEXT("パタゴニア"), TEXT("2"), TEXT("168"), TEXT("120") }),
		FString(TEXT("パタゴニア向け支援に救援優先を設定しました。待機契約2件を追い越し、待ち時間を168時間短縮、到着予定は120時間です。貨物、保管枠、経路、護衛、資金、稼働中の中継作業は変わりません。")));
	TestEqual(TEXT("Spanish Relief Priority progressed-ahead safeguard is exact"),
		Loaded.Catalog.Resolve(
			TEXT("diagnostic.mutual-aid-relief-priority-departed-ahead"),
			TEXT("fallback"), TEXT("es")),
		FString(TEXT("La prioridad de auxilio no puede adelantar a un convoy retenido que ya haya avanzado.")));
	TestEqual(TEXT("French Relief Stand-Down action preserves cargo, storage, and advance count"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.mutual-aid-relief-stand-down-action-format"),
				TEXT("fallback"), TEXT("fr")),
			{ TEXT("2"), TEXT("4"), TEXT("1") }),
		FString(TEXT("RETIRER LE CONVOI DE SECOURS\nRETOUR 2 • LIBÈRE 4 • AVANCE 1")));
	TestEqual(TEXT("Japanese Relief Stand-Down result preserves every returned contract value"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.mutual-aid-relief-stand-down-completed-format"),
				TEXT("fallback"), TEXT("ja")),
			{
				TEXT("パタゴニア"), TEXT("2"), TEXT("再生医療ジェル"),
				TEXT("カスケード"), TEXT("4"), TEXT("1"), TEXT("25000")
			}),
		FString(TEXT("パタゴニア向け救援輸送を撤回しました。2 × 再生医療ジェルをカスケードへ返却し、目的地の保管4を解放、後続の待機契約1件が前進しました。支払い済みの信号護衛費（25000）は返金されません。稼働中の中継作業、他の貨物、識別情報、経路、資金、乱数状態は変わりません。")));
	TestEqual(TEXT("Spanish Relief Stand-Down source-storage safeguard is exact"),
		Loaded.Catalog.Resolve(
			TEXT("diagnostic.mutual-aid-relief-stand-down-source-storage"),
			TEXT("fallback"), TEXT("es")),
		FString(TEXT("La base de origen no tiene espacio libre para la carga devuelta de este convoy.")));
	TestEqual(TEXT("French Relief Diversion option preserves destination and every projection"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.mutual-aid-relief-diversion-option-format"),
				TEXT("fallback"), TEXT("fr")),
			{ TEXT("ATLANTIQUE"), TEXT("10"), TEXT("168"), TEXT("-24"), TEXT("2") }),
		FString(TEXT("DÉROUTER VERS ATLANTIQUE\nRÉSERVE 10 • ARRIVÉE 168 h • DÉCALAGE -24 h • SUIVANTS 2")));
	TestEqual(TEXT("Japanese Relief Diversion result preserves every moved contract value"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.mutual-aid-relief-diversion-completed-format"),
				TEXT("fallback"), TEXT("ja")),
			{
				TEXT("パタゴニア"), TEXT("北大西洋"), TEXT("2"), TEXT("再生医療ジェル"),
				TEXT("10"), TEXT("75"), TEXT("40"), TEXT("168"), TEXT("-24"),
				TEXT("2"), TEXT("25000")
			}),
		FString(TEXT("救援輸送をパタゴニアから北大西洋へ転送しました。2 × 再生医療ジェルと保管10を移し、危険度は75/100 → 40/100、到着予定は168時間（変動-24時間）、後続契約2件が変化しました。支払い済みの信号護衛（25000）は維持されます。出発基地の在庫、貨物、識別情報、中継順、資金、稼働中の作業、乱数状態は変わりません。")));
	TestEqual(TEXT("Spanish Relief Diversion storage safeguard is exact"),
		Loaded.Catalog.Resolve(
			TEXT("diagnostic.mutual-aid-relief-diversion-destination-storage"),
			TEXT("fallback"), TEXT("es")),
		FString(TEXT("El destino solicitado no tiene espacio libre para la carga de este convoy.")));
	TestEqual(TEXT("French Relay Waypoint option preserves both legs and every queue projection"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.mutual-aid-relay-waypoint-option-format"),
				TEXT("fallback"), TEXT("fr")),
			{
				TEXT("MERIDIAN RELAY"), TEXT("FIL RAPIDE"), TEXT("144"),
				TEXT("216"), TEXT("+72"), TEXT("50"), TEXT("75"), TEXT("1")
			}),
		FString(TEXT("VIA MERIDIAN RELAY • PUIS FIL RAPIDE\nRELAIS 144 h • FINALE 216 h • DÉCALAGE +72 h • EXP 50/75 • SUIVANTS 1")));
	TestEqual(TEXT("Japanese Relay Waypoint result preserves both exposures and the end-to-end channel contract"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.mutual-aid-relay-waypoint-completed-format"),
				TEXT("fallback"), TEXT("ja")),
			{
				TEXT("北大西洋"), TEXT("メリディアン中継所経由 → 高速連絡網"),
				TEXT("50"), TEXT("75"), TEXT("216"), TEXT("+72"), TEXT("1")
			}),
		FString(TEXT("北大西洋向け支援経路をメリディアン中継所経由 → 高速連絡網に設定しました。第1区間の危険度は50/100、後半は75/100、到着予定は216時間（変動+72時間）、後続契約1件が変化しました。出発側リレーチャンネルは全行程で予約され、貨物、最終保管枠、識別情報、支払い済み護衛、資金、稼働中の作業、乱数状態は変わりません。")));
	TestEqual(TEXT("Spanish Relay Waypoint base safeguard is exact"),
		Loaded.Catalog.Resolve(
			TEXT("diagnostic.mutual-aid-relay-waypoint-base"),
			TEXT("fallback"), TEXT("es")),
		FString(TEXT("Un punto de relevo debe ser distinto del origen y del destino final.")));
	TestEqual(TEXT("French Balanced Handoff status preserves both cargo shares and reserved storage"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.mutual-aid-balanced-handoff-current-format"),
				TEXT("fallback"), TEXT("fr")),
			{
				TEXT("3"), TEXT("MERIDIAN RELAY"), TEXT("3"),
				TEXT("NORTH ATLANTIC"), TEXT("6")
			}),
		FString(TEXT("RELAIS ÉQUILIBRÉ  •  3 VERS MERIDIAN RELAY  •  3 VERS NORTH ATLANTIC  •  6 STOCKAGE RÉSERVÉ")));
	TestEqual(TEXT("French Through Cargo option preserves the exact zero handoff projection"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.mutual-aid-balanced-handoff-option-format"),
				TEXT("fallback"), TEXT("fr")),
			{ TEXT("CARGAISON DIRECTE"), TEXT("0"), TEXT("6"), TEXT("0") }),
		FString(TEXT("CARGAISON DIRECTE\nPOINT RELAIS 0 • FINALE 6 • STOCKAGE RELAIS 0")));
	TestEqual(TEXT("Japanese Balanced Handoff result preserves each share and all unchanged contracts"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.mutual-aid-balanced-handoff-completed-format"),
				TEXT("fallback"), TEXT("ja")),
			{
				TEXT("メリディアン中継所"), TEXT("均等引き渡し"), TEXT("3"),
				TEXT("再生医療ジェル"), TEXT("6"), TEXT("3"), TEXT("北大西洋")
			}),
		FString(TEXT("メリディアン中継所経由の貨物計画を均等引き渡しに設定しました。3 × 再生医療ジェルと保管6を経由点向けに確保し、3個は北大西洋へ続行します。リレー時間、出発チャンネル、識別情報、支払い済み護衛、資金、稼働中の作業、乱数状態は変わりません。")));
	TestEqual(TEXT("Spanish Balanced Handoff final-storage safeguard is exact"),
		Loaded.Catalog.Resolve(
			TEXT("diagnostic.mutual-aid-balanced-handoff-destination-storage"),
			TEXT("fallback"), TEXT("es")),
		FString(TEXT("El destino final no tiene almacén libre para la parte posterior prevista.")));
	TestEqual(TEXT("Japanese Mutual Aid card preserves cargo, route doctrine, exposure, status, storage, and exact clock"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.mutual-aid-card-format"), TEXT("fallback"), TEXT("ja")),
			{
				TEXT("再生医療ジェル"), TEXT("6"), TEXT("カスケード"),
				TEXT("パタゴニア"), TEXT("12"), TEXT("36"),
				TEXT("高速連絡網"), TEXT("100"), TEXT("妨害予測あり")
			}),
		FString(TEXT("再生医療ジェル  ×6\nカスケード  →  パタゴニア  •  保管 12  •  36時間\n高速連絡網  •  経路危険度 100/100  •  妨害予測あり")));
	TestEqual(TEXT("Japanese loadout feedback can reorder names without losing placeholders"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.loadout-added-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("制式小銃"), TEXT("Ari West") }),
		FString(TEXT("Ari Westの装備に制式小銃を追加しました。")));
	TestEqual(TEXT("French personnel card localizes status and stat chrome around exact values"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.personnel-card-format"), TEXT("fallback"), TEXT("fr")),
			{
				TEXT("Ari West"), TEXT("Agent de terrain"), TEXT("DISPONIBLE"), TEXT("CASCADE"), TEXT(""),
				TEXT("1"), TEXT("48"), TEXT("48"), TEXT("61"), TEXT("57"), TEXT("64"), TEXT("53"),
				TEXT("2"), TEXT("3"), TEXT("80")
			}),
		FString(TEXT("Ari West  •  Agent de terrain\nDISPONIBLE  •  BASE CASCADE   RANG 1   PV 48/48\nPRÉC 61   RÉS 57   MOB 64   FOR 53\nMISSIONS 2   ÉLIM. 3   EXP 80")));
	TestEqual(TEXT("French service history exposes the exact current and next mission bands"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("personnel.service-progress-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("AGUERRI"), TEXT("LONGUE VEILLE"), TEXT("10"), TEXT("3") }),
		FString(TEXT("SERVICE  AGUERRI\nPROCHAIN PALIER LONGUE VEILLE À 10 MISSIONS  •  RESTE 3")));
	TestEqual(TEXT("French terminal service history retains the exact lifetime mission count"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("personnel.service-maximum-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("PILIER D’HÉRITAGE"), TEXT("27") }),
		FString(TEXT("SERVICE  PILIER D’HÉRITAGE\nPALIER MAXIMAL  •  27 MISSIONS")));
	TestEqual(TEXT("French Watchkeeper Guidance retains mentor, service band, bonus, and recipient count"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("personnel.mentorship-active-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("CONSEIL DE LA VIGIE"), TEXT("MARA SOL"), TEXT("PILIER D’HÉRITAGE"), TEXT("10"), TEXT("2") }),
		FString(TEXT("CONSEIL DE LA VIGIE  //  MARA SOL  •  PILIER D’HÉRITAGE\nMORAL INITIAL +10  •  BÉNÉFICIAIRES DE PALIER INFÉRIEUR 2")));
	TestEqual(TEXT("French Legacy Relay retains specialist, doctrine, all four bonuses, and recipients"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("personnel.legacy-relay-active-format"), TEXT("fallback"), TEXT("fr")),
			{
				TEXT("RELAIS D’HÉRITAGE"), TEXT("MARA SOL"), TEXT("VISION NETTE"),
				TEXT("2"), TEXT("0"), TEXT("0"), TEXT("0"), TEXT("3")
			}),
		FString(TEXT("RELAIS D’HÉRITAGE  //  MARA SOL  •  VISION NETTE\nRELAIS DE TERRAIN  •  PRÉC +2  RÉS +0  MOB +0  FOR +0  •  BÉNÉFICIAIRES 3")));
	TestEqual(TEXT("French Field Cadence retains pair, tier, shared wins, AP, and morale"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("personnel.squad-bond-active-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("MARA SOL"), TEXT("PAVEL ORIN"), TEXT("IMBRIQUÉS"), TEXT("8"), TEXT("1"), TEXT("5") }),
		FString(TEXT("MARA SOL + PAVEL ORIN  //  IMBRIQUÉS  •  VICTOIRES COMMUNES 8\nPA +1  •  MORAL +5")));
	TestEqual(TEXT("French developing Field Cadence retains its exact next threshold"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("personnel.squad-bond-developing-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("MARA SOL"), TEXT("PAVEL ORIN"), TEXT("2"), TEXT("3") }),
		FString(TEXT("MARA SOL + PAVEL ORIN  //  FORMATION 2/3")));
	TestEqual(TEXT("French Surge Care retains exact duration and funding"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.recovery-plan-surge-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("SOINS INTENSIFS"), TEXT("5"), TEXT("20000") }),
		FString(TEXT("SOINS INTENSIFS\n5 H • 20000 FONDS")));
	TestEqual(TEXT("French Reflection Cycle retains exact duration and Resolve benefit"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.recovery-plan-reflection-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("CYCLE DE RÉFLEXION"), TEXT("15"), TEXT("1") }),
		FString(TEXT("CYCLE DE RÉFLEXION\n15 H • RÉS +1")));
	TestEqual(TEXT("French pending Return Path diagnostic is exact"),
		Loaded.Catalog.Resolve(
			TEXT("diagnostic.personnel-recovery-plan-required"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("Choisissez un parcours de retour pour chaque nouvelle personne blessée avant de faire avancer le temps stratégique.")));
	TestEqual(TEXT("French memorial record localizes role-independent history chrome and cause"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.memorial-card-format"), TEXT("fallback"), TEXT("fr")),
			{
				TEXT("Mara Sol"), TEXT("Agent de terrain"), TEXT("4"), TEXT("10"), TEXT("8"),
				TEXT("1"), TEXT("1"), TEXT("2042-04-06"), TEXT("Perte en opération tactique")
			}),
		FString(TEXT("Mara Sol  •  Agent de terrain\nRANG 4   MISSIONS 10   ÉLIM. 8   NIV. DOCTRINE 1   CITATIONS 1\nDERNIER SERVICE 2042-04-06 UTC  •  Perte en opération tactique")));
	TestEqual(TEXT("French tactical-casualty cause resolves from its stable content identity"),
		Loaded.Catalog.Resolve(TEXT("content.cause.tactical-casualty.name"), TEXT("fallback"), TEXT("fr")),
		FString(TEXT("Perte en opération tactique")));
	TestEqual(TEXT("French active-production format preserves exact quantities, staffing, duration, and storage delta"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.manufacturing-project-detail-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("3"), TEXT("2"), TEXT("8 h restantes"), TEXT("+3") }),
		FString(TEXT("3 unités • 2 ingénieurs • 8 h restantes • Stockage +3/unité")));
	const FString FrenchManufacturingDetail = FString::Format(
		*Loaded.Catalog.Resolve(
			TEXT("strategic.manufacturing-option-detail-format"), TEXT("fallback"), TEXT("fr")),
		{ TEXT("12"), TEXT("+3") });
	const FString FrenchManufacturingInputs = FString::Format(
		*Loaded.Catalog.Resolve(
			TEXT("strategic.manufacturing-material-stock-format"), TEXT("fallback"), TEXT("fr")),
		{ TEXT("2"), TEXT("Éclat de résonance"), TEXT("6") });
	const FString FrenchManufacturingBatch = FString::Format(
		*Loaded.Catalog.Resolve(
			TEXT("strategic.manufacturing-batch-inputs-format"), TEXT("fallback"), TEXT("fr")),
		{ FrenchManufacturingDetail, FrenchManufacturingInputs });
	TestEqual(TEXT("French manufacturing option composes localized recipe, material stock, and authoritative values"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.manufacturing-option-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("1"), TEXT("Scanner de terrain"), TEXT("250"), FrenchManufacturingBatch }),
		FString(TEXT("FABRIQUER ×1  Scanner de terrain  •  250\nFabriquer 1 • 12 heures-ingénieur • Stockage +3/unité • Entrées de la série : 2 Éclat de résonance (stock 6)")));
	TestEqual(TEXT("Japanese production feedback can reorder the localized output while preserving every value"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.production-started-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("フィールドスキャナー"), TEXT("2"), TEXT("5"), TEXT(" 投入資材を予約：4 共鳴片。") }),
		FString(TEXT("フィールドスキャナー ×2の生産を技術者5名で開始しました。 投入資材を予約：4 共鳴片。")));
	TestEqual(TEXT("French craft card localizes type, state, and operational gauges around the personal callsign"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.craft-card-format"), TEXT("fallback"), TEXT("fr")),
			{
				TEXT("Runtime Skiff"), TEXT("Transport Héron"), TEXT("AU SOL"),
				TEXT("100"), TEXT("100"), TEXT("400"), TEXT("500"), TEXT("1"), TEXT("4"),
				TEXT("PILOTE KAI NORTH")
			}),
		FString(TEXT("Runtime Skiff • Transport Héron\nAU SOL   COQUE 100/100   CARBURANT 400/500   ÉQUIPE 1/4   PILOTE KAI NORTH")));
	TestEqual(TEXT("French craft-ammunition row preserves mounted count, readiness, base stock, and loadable rounds"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.craft-weapon-ammunition-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("Canon d'essai"), TEXT("2"), TEXT("Munitions d'essai"),
				TEXT("4"), TEXT("12"), TEXT("5"), TEXT("5") }),
		FString(TEXT("Canon d'essai ×2  •  Munitions d'essai 4/12  •  BASE 5  •  CHARGEABLE 5")));
	TestEqual(TEXT("French partial-rearm control preserves exact loadable and missing values"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.craft-rearm-available-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("5"), TEXT("8") }),
		FString(TEXT("CHARGER LE DISPONIBLE  •  5/8")));
	TestEqual(TEXT("French craft-service summary preserves both clocks, ready time, and refund"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.craft-service-summary-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("2"), TEXT("1"), TEXT("2"), TEXT("300") }),
		FString(TEXT("RÉPARATION 2 h • RAVITAILLEMENT 1 h\nPRÊT DANS 2 h • REMBOURSEMENT 300")));
	TestEqual(TEXT("French craft-service cancellation preserves the exact refundable reservation"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.craft-service-cancel-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("300") }),
		FString(TEXT("ANNULER L'ENTRETIEN • +300")));
	TestEqual(TEXT("French active service rotation preserves exact lane and ready values"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.craft-service-rotation-active-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("1"), TEXT("2"), TEXT("3") }),
		FString(TEXT("POSTE D'ENTRETIEN 1/2 • PRÊT DANS 3 h")));
	TestEqual(TEXT("French queued service rotation preserves position, wait, and ready values"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.craft-service-rotation-queued-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("2"), TEXT("3"), TEXT("7") }),
		FString(TEXT("FILE 2 • ATTENTE 3 h • PRÊT DANS 7 h")));
	TestTrue(TEXT("French service rotation localizes its policy and deterministic guidance"),
		Loaded.Catalog.Resolve(
			TEXT("strategic.craft-service-rotation-name"), TEXT("fallback"), TEXT("fr"))
			== TEXT("ROTATION RAPIDE")
		&& Loaded.Catalog.Resolve(
			TEXT("strategic.craft-service-rotation-guidance"), TEXT("fallback"), TEXT("fr"))
			.Contains(TEXT("sans tirage aléatoire")));
	TestEqual(TEXT("French contact card preserves exact threat, hull, route, and target values"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.contact-card-format"), TEXT("fallback"), TEXT("fr")),
			{
				TEXT("CONTACT RASE-VAGUE"), TEXT("DÉTECTÉ"), TEXT("2"), TEXT("70"), TEXT("80"),
				TEXT("25"), FString::Format(
					*Loaded.Catalog.Resolve(TEXT("strategic.contact-target-format"), TEXT("fallback"), TEXT("fr")),
					{ TEXT("RUNTIME STATION") })
			}),
		FString(TEXT("CONTACT RASE-VAGUE  •  DÉTECTÉ\nMENACE 2   COQUE 70/80   TRAJET 25%\nCIBLE  RUNTIME STATION")));
	TestEqual(TEXT("French adversary-plan intelligence preserves both authored outcome branches"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.adversary-plan-format"), TEXT("fallback"), TEXT("fr")),
			{
				TEXT("SCHÉMA DE PLUIE MIROIR"), TEXT("1"),
				TEXT("RAID DE VERRE NOCTURNE"), TEXT("INCURSION SAFRAN")
			}),
		FString(TEXT("PLAN ADVERSE  //  SCHÉMA DE PLUIE MIROIR  •  PHASE 1\nEN CAS DE FUITE → RAID DE VERRE NOCTURNE\nSI DÉJOUÉ → INCURSION SAFRAN")));
	TestEqual(TEXT("Japanese terminal plan intelligence preserves the branch-end state"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.adversary-plan-format"), TEXT("fallback"), TEXT("ja")),
			{
				TEXT("ミラー・レイン・パターン"), TEXT("2"),
				Loaded.Catalog.Resolve(TEXT("strategic.adversary-plan-ends"), TEXT("fallback"), TEXT("ja")),
				TEXT("サフラン侵攻")
			}),
		FString(TEXT("敵対計画  //  ミラー・レイン・パターン  •  段階 2\n逃走時 → パターン終了\n阻止時 → サフラン侵攻")));
	const FString FrenchCounterplayEscape = FString::Format(
		*Loaded.Catalog.Resolve(
			TEXT("strategic.coalition-counterplay-member-format"),
			TEXT("fallback"), TEXT("fr")),
		{
			TEXT("PACTE CASCADIEN"), TEXT("27"), TEXT("19"),
			Loaded.Catalog.Resolve(
				TEXT("strategic.coalition-counterplay-withdrawal"),
				TEXT("fallback"), TEXT("fr"))
		});
	const FString FrenchCounterplayRecovery = FString::Format(
		*Loaded.Catalog.Resolve(
			TEXT("strategic.coalition-counterplay-member-format"),
			TEXT("fallback"), TEXT("fr")),
		{
			TEXT("ASSEMBLÉE NORD-ATLANTIQUE"), TEXT("30"), TEXT("40"),
			Loaded.Catalog.Resolve(
				TEXT("strategic.coalition-counterplay-remains-withdrawn"),
				TEXT("fallback"), TEXT("fr"))
		});
	TestEqual(TEXT("French coalition counterplay preserves exact support projections and membership states"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.coalition-counterplay-format"),
				TEXT("fallback"), TEXT("fr")),
			{ FrenchCounterplayEscape, FrenchCounterplayRecovery }),
		FString(TEXT("CONTRE-MESURE COALITIONNELLE\nEN CAS DE FUITE → PACTE CASCADIEN SOUTIEN 27→19 • SE RETIRE\nSI DÉJOUÉ → ASSEMBLÉE NORD-ATLANTIQUE SOUTIEN 30→40 • RESTE RETIRÉ")));
	TestEqual(TEXT("French landing-outcome intelligence preserves both exact site profiles"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.contact-landing-choice-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("2"), TEXT("72 h restantes"), TEXT("4"), TEXT("36 h restantes") }),
		FString(TEXT("RENSEIGNEMENT SUR LES ISSUES\nDÉTRUIRE → ÉPAVE • MENACE 2 • 72 h restantes\nSUIVRE JUSQU’À L’ARRIVÉE → ATTERRISSAGE INTACT • MENACE 4 • 36 h restantes\nL’ARRIVÉE APPLIQUE LES CONSÉQUENCES DE LA MISSION")));
	TestEqual(TEXT("Japanese intact-site name composes a localized source contact"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.landing-site-name-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("ハーベスター反応") }),
		FString(TEXT("ハーベスター反応着陸地点")));
	TestEqual(TEXT("French recovery-site card composes the localized source contact and lifetime"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.site-card-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("ÉPAVE : CONTACT MOISSONNEUR"), TEXT("4"), TEXT("36 h restantes") }),
		FString(TEXT("ÉPAVE : CONTACT MOISSONNEUR\nMENACE 4   DURÉE 36 h restantes")));
	TestEqual(TEXT("Japanese contact marker can lead with the target while preserving every authoritative value"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.contact-marker-target-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("探知"), TEXT("2"), TEXT("70"), TEXT("80"), TEXT("25"), TEXT("CASCADE") }),
		FString(TEXT("目標 CASCADE • 探知 • 脅威 2 • 機体 70/80 • 経路 25%")));
	TestEqual(TEXT("French global situation preserves every exact count and funding value"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.global-summary-format"), TEXT("fallback"), TEXT("fr")),
			{
				TEXT("1"), TEXT("1"), TEXT("0"), TEXT("7"), TEXT("3"), TEXT("2"),
				TEXT("72"), TEXT("+1800000"), TEXT("650000"), TEXT("+1150000")
			}),
		FString(TEXT("CONTACTS 1   SITES 1   ALERTES DE BASE 0\nMISSIONS : 7 LANCÉES / 3 DÉJOUÉES / 2 ÉCHAPPÉES\nPROCHAINE FENÊTRE 72 h\nFINANCEMENT +1800000  •  DÉPENSES -650000  •  NET +1150000")));
	const FString FrenchAdaptation = Loaded.Catalog.Resolve(
		TEXT("strategic.next-adaptation-one"), TEXT("fallback"), TEXT("fr"));
	TestEqual(TEXT("French adaptation countdown preserves the exact remaining resolution"),
		FrenchAdaptation, FString(TEXT("PROCHAINE ADAPTATION DANS 1 RÉSOLUTION")));
	TestEqual(TEXT("French campaign objectives preserve both victory gates and the regional failure threshold"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.campaign-objectives-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("3"), TEXT("12"), TEXT("4"), TEXT("5"), TEXT("90"), TEXT("100"), FrenchAdaptation }),
		FString(TEXT("ENDIGUEMENT 3/12 DÉJOUÉES  •  ESCALADE 4/5\nTENSION RÉGIONALE 90/100  •  PROCHAINE ADAPTATION DANS 1 RÉSOLUTION")));
	TestEqual(TEXT("Japanese adaptation ceiling has a dedicated localized terminal state"),
		Loaded.Catalog.Resolve(TEXT("strategic.adaptation-maximum"), TEXT("fallback"), TEXT("ja")),
		FString(TEXT("適応上限に到達")));
	TestEqual(TEXT("Japanese craft acquisition feedback can reorder the persisted craft name"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.craft-acquisition-placed-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("スパロー迎撃機 01") }),
		FString(TEXT("スパロー迎撃機 01の調達を発注しました。")));
	TestEqual(TEXT("French research-start feedback preserves the localized project and exact staffing"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.research-started-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("Analyse des signaux anormaux"), TEXT("5") }),
		FString(TEXT("Analyse des signaux anormaux a démarré avec 5 scientifiques.")));
	TestEqual(TEXT("French research cancellation frames the localized project name"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.research-cancelled-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("Analyse des signaux anormaux") }),
		FString(TEXT("Annulation de Analyse des signaux anormaux ; les scientifiques affectés sont libérés.")));
	TestEqual(TEXT("Japanese construction cancellation can reorder the localized facility and exact refund"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.construction-cancelled-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("研究棟"), TEXT("750000") }),
		FString(TEXT("研究棟を中止 • 払い戻し 750000。")));
	TestEqual(TEXT("Localized outage format preserves exact facility and staffing values"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("strategic.research-paused-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("Centre des opérations"), TEXT("5") }),
		FString(TEXT("LABORATOIRE HORS LIGNE • Centre des opérations • 5 scientifiques")));
	TestEqual(TEXT("Localized archive counts retain exact authorized and classified values"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("archive.subtitle-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("3"), TEXT("5") }),
		FString(TEXT("3 DOSSIERS DÉVERROUILLÉS  •  5 CLASSIFIÉS")));
	TestEqual(TEXT("Localized tactical phase retains exact authoritative values"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("tactical.phase-field-format"), TEXT("fallback"), TEXT("fr")),
			{
				TEXT("2"), TEXT("20"), TEXT("TOUR DU JOUEUR"), TEXT("NORD"),
				TEXT("2"), TEXT("3"), TEXT("10")
			}),
		FString(TEXT("TOUR 2 / 20  •  TOUR DU JOUEUR  •  VENT NORD 2  •  CARGAISON 3 / 10")));
	TestEqual(TEXT("Localized save metadata retains exact authoritative values"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("save.detail-format"), TEXT("fallback"), TEXT("fr")),
			{
				TEXT("VÉRIFIÉ"), TEXT("VÉTÉRAN"), TEXT("2035-02-03 04:05 UTC"),
				TEXT("1250000"), TEXT("420"), TEXT("2035-02-03 04:06 UTC"), TEXT("0.24")
			}),
		FString(TEXT("VÉRIFIÉ  •  VÉTÉRAN\nCAMPAGNE  2035-02-03 04:05 UTC  •  FONDS  $1250000  •  SCORE  420\nSAUVEGARDE  2035-02-03 04:06 UTC  •  VERSION  0.24")));
	TestEqual(TEXT("Localized founding formats retain deterministic numeric projection"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.founding-subtitle-format"), TEXT("fallback"), TEXT("fr")),
			{
				TEXT("2035-01-01 12:00 UTC"),
				TEXT("1500000"),
				TEXT("STANDARD")
			}),
		FString(TEXT("2035-01-01 12:00 UTC  •  FONDS 1500000  •  GRAINE VERROUILLÉE  •  STANDARD")));
	TestEqual(TEXT("French difficulty profile preserves both exact policy percentages"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("menu.difficulty-profile-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("VÉTÉRAN"), TEXT("85"), TEXT("125") }),
		FString(TEXT("VÉTÉRAN\nINTERVALLE DES MISSIONS 85% • IMPACT DES FUITES 125%")));
	TestEqual(TEXT("Japanese difficulty profile preserves both exact policy percentages"),
		FString::Format(
			*Loaded.Catalog.Resolve(TEXT("menu.difficulty-profile-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("APEX"), TEXT("70"), TEXT("150") }),
		FString(TEXT("APEX\nミッション間隔 70% • 逃走時影響 150%")));
	TestEqual(TEXT("Unsupported cultures fall back to authored English"),
		Loaded.Catalog.Resolve(TEXT("menu.quit"), TEXT("fallback"), TEXT("it")),
		FString(TEXT("QUIT")));
	TestEqual(TEXT("Unknown keys preserve their explicit call-site fallback"),
		Loaded.Catalog.Resolve(TEXT("missing.key"), TEXT("SAFE FALLBACK"), TEXT("ja")),
		FString(TEXT("SAFE FALLBACK")));

	const FUEGTLocalizationLoadResult Activated = FUEGTLocalizationService::ReloadDefaultCatalog();
	TestTrue(TEXT("Default catalog activates for runtime lookup"), Activated.bSucceeded
		&& FUEGTLocalizationService::IsCatalogReady()
		&& FUEGTLocalizationService::GetActiveEntryCount() == 1476);
	TestEqual(TEXT("Explicit-culture runtime lookup uses the activated catalog"),
		FUEGTLocalizationService::TextForCulture(
			TEXT("settings.language-controls"), TEXT("fallback"), TEXT("ja-JP")),
		FString(TEXT("言語 + 操作")));
	TestEqual(TEXT("Content-name lookup derives the stable rule key and normalizes regional cultures"),
		FUEGTLocalizationService::ContentNameForCulture(
			TEXT("research.signal-analysis"), TEXT("fallback"), TEXT("fr-CA")),
		FString(TEXT("Analyse des signaux anormaux")));
	TestEqual(TEXT("Content-name lookup resolves staged item rules"),
		FUEGTLocalizationService::ContentNameForCulture(
			TEXT("item.service-rifle"), TEXT("fallback"), TEXT("es-MX")),
		FString(TEXT("Fusil de servicio")));
	TestEqual(TEXT("Content-name lookup resolves staged personnel role rules"),
		FUEGTLocalizationService::ContentNameForCulture(
			TEXT("role.pilot"), TEXT("fallback"), TEXT("ja-JP")),
		FString(TEXT("迎撃機パイロット")));
	TestEqual(TEXT("Content-name lookup resolves staged craft rules"),
		FUEGTLocalizationService::ContentNameForCulture(
			TEXT("craft.heron-transport"), TEXT("fallback"), TEXT("fr-CA")),
		FString(TEXT("Transport Héron")));
	TestEqual(TEXT("Content-name lookup resolves staged contact rules"),
		FUEGTLocalizationService::ContentNameForCulture(
			TEXT("contact.skimmer"), TEXT("fallback"), TEXT("de-DE")),
		FString(TEXT("Gleiterkontakt")));
	TestEqual(TEXT("Content-name lookup resolves staged adversary mission rules"),
		FUEGTLocalizationService::ContentNameForCulture(
			TEXT("mission.nightglass-raid"), TEXT("fallback"), TEXT("es-MX")),
		FString(TEXT("Incursión Cristal Nocturno")));
	TestEqual(TEXT("Content-name lookup resolves the late-game Cinder contact"),
		FUEGTLocalizationService::ContentNameForCulture(
			TEXT("contact.cinder-loom"), TEXT("fallback"), TEXT("fr-CA")),
		FString(TEXT("Contact Métier de braise")));
	TestEqual(TEXT("Content-name lookup resolves a Cinder stage-three mission"),
		FUEGTLocalizationService::ContentNameForCulture(
			TEXT("mission.ashen-crown-raid"), TEXT("fallback"), TEXT("de-DE")),
		FString(TEXT("Aschenkronen-Überfall")));
	TestEqual(TEXT("Content-name lookup resolves the Cinder plan"),
		FUEGTLocalizationService::ContentNameForCulture(
			TEXT("plan.cinder-lattice"), TEXT("fallback"), TEXT("ja-JP")),
		FString(TEXT("シンダー・ラティス・シーケンス")));
	TestEqual(TEXT("Content-name lookup resolves authored regional mandate partners"),
		FUEGTLocalizationService::ContentNameForCulture(
			TEXT("region.north-atlantic"), TEXT("fallback"), TEXT("fr-CA")),
		FString(TEXT("Assemblée nord-atlantique")));
	TestEqual(TEXT("Japanese regional outreach preserves exact numeric tradeoffs"),
		FString::Format(
			*FUEGTLocalizationService::TextForCulture(
				TEXT("strategic.region-action-detail-format"), TEXT("fallback"), TEXT("ja-JP")),
			{ TEXT("民生支援"), TEXT("120000"), TEXT("+12"), TEXT("4") }),
		FString(TEXT("民生支援  •  資金 120000 • 支持 +12 • 圧力 -4")));
	TestEqual(TEXT("French crisis mobilization preserves its exact support-funded threshold tradeoff"),
		FString::Format(
			*FUEGTLocalizationService::TextForCulture(
				TEXT("strategic.region-action-crisis-detail-format"), TEXT("fallback"), TEXT("fr-FR")),
			{ TEXT("MOBILISATION DE CRISE"), TEXT("0"), TEXT("-15"), TEXT("25"), TEXT("60") }),
		FString(TEXT("MOBILISATION DE CRISE  •  FONDS 0\nSOUTIEN -15 • PRESSION -25 • SEUIL 60")));
	TestEqual(TEXT("French crisis threshold diagnostic is exact"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("regional_crisis_not_severe"), TEXT("fallback"), TEXT("fr-FR")),
		FString(TEXT("La pression régionale n'a pas atteint le seuil de crise configuré.")));
	TestEqual(TEXT("French Resilience Charter preserves every exact durable percentage"),
		FString::Format(
			*FUEGTLocalizationService::TextForCulture(
				TEXT("strategic.region-charter-detail-format"), TEXT("fallback"), TEXT("fr-FR")),
			{ TEXT("CHARTE DE RÉSILIENCE"), TEXT("250000"), TEXT("10"), TEXT("60"),
				TEXT("175000"), TEXT("157500"), TEXT("50"), TEXT("75") }),
		FString(TEXT("CHARTE DE RÉSILIENCE  •  FONDS 250000 • SOUTIEN -10 • MIN 60\nFINANCEMENT 175000 → 157500 • POIDS MISSION 50% • PRESSION DE FUITE 75%")));
	TestEqual(TEXT("French charter support diagnostic is exact"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("regional_charter_support_required"), TEXT("fallback"), TEXT("fr-FR")),
		FString(TEXT("Ce partenaire régional n'a pas atteint le soutien requis pour une charte de résilience.")));
	TestEqual(TEXT("French Horizon Compact preserves every exact coalition tradeoff"),
		FString::Format(
			*FUEGTLocalizationService::TextForCulture(
				TEXT("strategic.coalition-compact-detail-format"), TEXT("fallback"), TEXT("fr-FR")),
			{ TEXT("PACTE HORIZON"), TEXT("400000"), TEXT("2"), TEXT("2"), TEXT("50"),
				TEXT("5"), TEXT("95"), TEXT("33"), TEXT("280000"), TEXT("290000") }),
		FString(TEXT("PACTE HORIZON  •  FONDS 400000 • SIGNÉES 2/2 • SOUTIEN MIN 50\nCHAQUE MEMBRE -5 SOUTIEN • FINANCEMENT 95% • REDIRECTION 33%\nFINANCEMENT TOTAL 280000 → 290000")));
	TestEqual(TEXT("French compact charter-count diagnostic is exact"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("coalition_compact_charters_required"), TEXT("fallback"), TEXT("fr-FR")),
		FString(TEXT("Le Pacte Horizon exige davantage de chartes de résilience signées.")));
	TestEqual(TEXT("French Horizon Compact cohesion preserves active and withdrawn counts plus both support thresholds"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.coalition-compact-cohesion-format"), TEXT("fallback"), TEXT("fr-FR")),
			{ TEXT("PACTE HORIZON"), TEXT("1"), TEXT("1"), TEXT("25"),
				TEXT("40"), TEXT("95"), TEXT("33"), TEXT("185000") }),
		FString(TEXT("PACTE HORIZON  •  ACTIFS 1 • RETIRÉS 1\nRETRAIT SOUS 25 SOUTIEN • RETOUR À 40\nFINANCEMENT 95% • REDIRECTION 33% • TOTAL 185000/MOIS")));
	TestEqual(TEXT("French compact withdrawal warning is exact"),
		Loaded.Catalog.Resolve(
			TEXT("strategic.coalition-withdrawal-warning"), TEXT("fallback"), TEXT("fr-FR")),
		FString(TEXT("ATTENTION : CETTE PERTE DE SOUTIEN ENTRAÎNERA LE RETRAIT D'UN MEMBRE")));
	TestEqual(TEXT("French compact restoration preserves cost, support threshold, and exact funding projection"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.coalition-restoration-detail-format"), TEXT("fallback"), TEXT("fr-FR")),
			{ TEXT("RÉTABLIR L'ADHÉSION AU PACTE"), TEXT("100000"), TEXT("40"),
				TEXT("40"), TEXT("185000"), TEXT("190000") }),
		FString(TEXT("RÉTABLIR L'ADHÉSION AU PACTE  •  FONDS 100000 • SOUTIEN 40/40\nFINANCEMENT TOTAL 185000 → 190000")));
	TestEqual(TEXT("French withdrawn-member aid diagnostic is exact"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("coalition_aid_target_withdrawn"), TEXT("fallback"), TEXT("fr-FR")),
		FString(TEXT("L'aide réciproque ne peut pas viser un membre qui s'est retiré du Pacte Horizon.")));
	TestEqual(TEXT("French compact restoration support diagnostic is exact"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("coalition_restoration_support_required"), TEXT("fallback"), TEXT("fr-FR")),
		FString(TEXT("Ce membre retiré n'a pas rétabli assez de soutien pour rejoindre le Pacte Horizon.")));
	const FString FrenchEmergencyBallots = FString::Format(
		*Loaded.Catalog.Resolve(
			TEXT("strategic.coalition-emergency-vote-ballots-format"),
			TEXT("fallback"), TEXT("fr-FR")),
		{ TEXT("PACTE NORD-ATLANTIQUE, PACTE PACIFIQUE OUEST"), TEXT("—") });
	TestEqual(TEXT("French emergency solidarity ballots preserve both deterministic camps"),
		FrenchEmergencyBallots,
		FString(TEXT("POUR PACTE NORD-ATLANTIQUE, PACTE PACIFIQUE OUEST • CONTRE —")));
	TestEqual(TEXT("French emergency solidarity vote preserves exact majority and recovery economics"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.coalition-emergency-vote-detail-format"),
				TEXT("fallback"), TEXT("fr-FR")),
			{ TEXT("VOTE DE SOLIDARITÉ D'URGENCE"), TEXT("200000"), TEXT("2"), TEXT("2"),
				TEXT("30"), TEXT("42"), TEXT("45"), TEXT("30"), FrenchEmergencyBallots,
				TEXT("2"), TEXT("70"), TEXT("+22500") }),
		FString(TEXT("VOTE DE SOLIDARITÉ D'URGENCE  •  FONDS 200000 • OUI 2/2\nSOUTIEN 30 → 42 • PRESSION 45 → 30\nPOUR PACTE NORD-ATLANTIQUE, PACTE PACIFIQUE OUEST • CONTRE —\nMEMBRES FAVORABLES -2 SOUTIEN • LIMITE 70 PRESSION • FINANCEMENT +22500")));
	TestEqual(TEXT("French emergency-vote majority rejection diagnostic is exact"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("coalition_emergency_vote_rejected"), TEXT("fallback"), TEXT("fr-FR")),
		FString(TEXT("Une majorité absolue des membres actifs ne soutiendra pas cette motion d'urgence.")));
	TestEqual(TEXT("French mandate summary preserves support and funding projections"),
		FString::Format(
			*FUEGTLocalizationService::TextForCulture(
				TEXT("strategic.region-mandate-format"), TEXT("fallback"), TEXT("fr-FR")),
			{ TEXT("PACTE CASCADIEN"), TEXT("20"), TEXT("55"), TEXT("ENGAGÉ"), TEXT("+400000"), TEXT("+400000") }),
		FString(TEXT("PACTE CASCADIEN  •  PRESSION 20  •  SOUTIEN 55 ENGAGÉ\nFINANCEMENT +400000 → +400000 / MOIS")));
	TestEqual(TEXT("Content-name lookup resolves archive categories from stable ids"),
		FUEGTLocalizationService::ContentNameForCulture(
			TEXT("category.engineering"), TEXT("fallback"), TEXT("fr-CA")),
		FString(TEXT("Ingénierie")));
	TestEqual(TEXT("Content-name lookup resolves authored archive record titles"),
		FUEGTLocalizationService::ContentNameForCulture(
			TEXT("archive.perimeter-doctrine"), TEXT("fallback"), TEXT("de-DE")),
		FString(TEXT("Doktrin der Perimeterverteidigung")));
	TestEqual(TEXT("Content-field lookup resolves localized archive summaries"),
		FUEGTLocalizationService::ContentFieldForCulture(
			TEXT("archive.command-cycle"), TEXT("summary"), TEXT("fallback"), TEXT("ja-JP")),
		FString(TEXT("戦略時間、判断停止、正式命令がどのように連動するか。")));
	TestTrue(TEXT("Content-field lookup resolves complete localized archive bodies"),
		FUEGTLocalizationService::ContentFieldForCulture(
			TEXT("archive.signal-front-charter"), TEXT("body"), TEXT("fallback"), TEXT("fr-FR"))
			.StartsWith(TEXT("L'UEGT existe pour maintenir le lien")));
	TestTrue(TEXT("Late-game intelligence body resolves in Spanish"),
		FUEGTLocalizationService::ContentFieldForCulture(
			TEXT("archive.cinder-lattice-assessment"), TEXT("body"), TEXT("fallback"), TEXT("es-MX"))
			.StartsWith(TEXT("A diferencia de las salidas aisladas")));
	TestEqual(TEXT("Unknown content names preserve their explicit authored fallback"),
		FUEGTLocalizationService::ContentNameForCulture(
			TEXT("research.mod-topic"), TEXT("Mod Topic"), TEXT("de")),
		FString(TEXT("Mod Topic")));
	TestEqual(TEXT("Unknown content fields preserve their explicit authored fallback"),
		FUEGTLocalizationService::ContentFieldForCulture(
			TEXT("archive.mod-record"), TEXT("body"), TEXT("Mod archive body"), TEXT("es")),
		FString(TEXT("Mod archive body")));
	TestEqual(TEXT("Exact diagnostic codes resolve an authored actionable translation"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("insufficient_funds"), TEXT("Raw dynamic funds diagnostic"), TEXT("fr-CA")),
		FString(TEXT("Les fonds disponibles sont insuffisants pour cette commande.")));
	TestEqual(TEXT("Exact French Signal Escort funding guidance replaces dynamic convoy totals"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("mutual_aid_signal_escort_funds"), TEXT("Raw escort-funds diagnostic"), TEXT("fr-FR")),
		FString(TEXT("Les fonds de la campagne ne permettent pas de financer l’escorte-signal de ce convoi.")));
	TestEqual(TEXT("Exact Spanish Threadline policy diagnostic remains actionable"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("invalid_mutual_aid_route_policy"), TEXT("Raw route-policy diagnostic"), TEXT("es-ES")),
		FString(TEXT("La política de ruta de ayuda mutua seleccionada no es compatible.")));
	TestEqual(TEXT("Exact Japanese Threadline overflow diagnostic identifies the guarded clock"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("mutual_aid_interdiction_overflow"), TEXT("Raw route-clock diagnostic"), TEXT("ja-JP")),
		FString(TEXT("相互支援輸送の妨害遅延が、対応する経路時間を超えています。")));
	TestEqual(TEXT("Exact French Relay Weave outage diagnostic identifies source infrastructure"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("mutual_aid_relay_unavailable"), TEXT("Raw relay diagnostic"), TEXT("fr-FR")),
		FString(TEXT("La base de départ ne dispose d’aucune capacité de signal opérationnelle pour un canal relais d’aide mutuelle.")));
	TestEqual(TEXT("Exact French Signal Watch capacity diagnostic identifies the facility gate"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("signal_watch_channel_capacity_exceeded"), TEXT("Raw Signal Watch diagnostic"), TEXT("fr-FR")),
		FString(TEXT("La veille signal exige un canal d’installation opérationnel par scientifique ; cette base en prend actuellement moins en charge.")));
	TestEqual(TEXT("Exact Spanish Works Cadre diagnostic identifies the three-engineer limit"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("works_cadre_limit_exceeded"), TEXT("Raw Works Cadre diagnostic"), TEXT("es-ES")),
		FString(TEXT("El cuadro de obras admite como máximo tres ingenieros en una base.")));
	TestEqual(TEXT("Exact German Works Charter diagnostic rejects unknown specialization values"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("invalid_works_cadre_charter"), TEXT("Raw Works Charter diagnostic"), TEXT("de-DE")),
		FString(TEXT("Die gewählte Werkcharta wird nicht unterstützt.")));
	TestEqual(TEXT("Exact Spanish Threadline Retune departure diagnostic preserves the no-progress gate"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("mutual_aid_retune_departed"), TEXT("Raw retune diagnostic"), TEXT("es-ES")),
		FString(TEXT("Solo un convoy retenido que nunca haya avanzado puede cambiar su ruta Threadline.")));
	TestEqual(TEXT("Exact content-package diagnostics override dynamic path details in French"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("content_directory_missing"), TEXT("Raw dynamic path diagnostic"), TEXT("fr-CA")),
		FString(TEXT("Le dossier configuré des paquets de contenu n'existe pas.")));
	TestEqual(TEXT("Exact Spanish adversary configuration diagnostics include the adaptation and outcome gates"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("invalid_adversary_config"), TEXT("Raw adversary diagnostic"), TEXT("es-ES")),
		FString(TEXT("La configuración del adversario, la dificultad, la diplomacia regional, la adaptación y el desenlace de campaña debe mantenerse dentro de los límites admitidos.")));
	TestEqual(TEXT("Exact French interception rule diagnostics identify the missing formation data"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("invalid_interception_rules"), TEXT("Raw interception-rules diagnostic"), TEXT("fr-FR")),
		FString(TEXT("Les données de l'appareil, du contact ou de la base d'attache requises pour l'interception sont manquantes.")));
	TestEqual(TEXT("Exact German contact participation diagnostics identify the on-station requirement"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("contact_not_engaged"), TEXT("Raw contact-participation diagnostic"), TEXT("de-DE")),
		FString(TEXT("Abfangkampfrunden erfordern mindestens ein stationiertes Fluggerät an einem engagierten Kontakt.")));
	TestEqual(TEXT("Exact Spanish contact profile diagnostics remain actionable"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("invalid_contact_rule"), TEXT("Raw contact-rule diagnostic"), TEXT("es-ES")),
		FString(TEXT("Falta el perfil de combate del contacto o no es válido.")));
	TestEqual(TEXT("Exact Japanese interception round diagnostics identify the guarded counter"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("interception_round_overflow"), TEXT("Raw interception-round diagnostic"), TEXT("ja-JP")),
		FString(TEXT("迎撃はこれ以上戦闘ラウンドを受け付けられません。")));
	TestEqual(TEXT("Exact English interception range diagnostics preserve the numeric guard"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("interception_range_overflow"), TEXT("Raw interception-range diagnostic"), TEXT("en-US")),
		FString(TEXT("Interception route exceeds the supported numeric range.")));
	TestEqual(TEXT("Exact French contact route diagnostics preserve the coordinate guard"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("invalid_contact_route"), TEXT("Raw contact-route diagnostic"), TEXT("fr-FR")),
		FString(TEXT("La route du contact exige des coordonnées d'origine et de destination distinctes dans les limites de longitude et de latitude.")));
	TestEqual(TEXT("Exact Japanese craft route diagnostics identify the return-path failure"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("invalid_craft_route"), TEXT("Raw craft-route diagnostic"), TEXT("ja-JP")),
		FString(TEXT("航空機に有効な帰還航路がありません。")));
	TestEqual(TEXT("Exact French Grid Overcharge funding guidance replaces dynamic fund totals"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("insufficient_base_defense_overcharge_funds"), TEXT("Raw dynamic overcharge diagnostic"), TEXT("fr-FR")),
		FString(TEXT("Les fonds disponibles ne couvrent pas cette surcharge du réseau indexée sur la menace.")));
	TestEqual(TEXT("Exact adversary-plan validation remains actionable in German"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("invalid_adversary_plan_branch"), TEXT("Raw branch diagnostic"), TEXT("de-DE")),
		FString(TEXT("Ein Zweig eines gegnerischen Plans muss im selben Plan bleiben und genau eine Stufe vorrücken.")));
	TestEqual(TEXT("Exact tactical-recipe validation remains actionable in Japanese"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("missing_landing_tactical_mapping"), TEXT("Raw recipe diagnostic"), TEXT("ja-JP")),
		FString(TEXT("着陸可能なミッションに対応する無傷地点の戦術レシピがありません。")));
	TestEqual(TEXT("Exact French tactical recipe ambiguity replaces dynamic source-contact details"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("ambiguous_tactical_mission"), TEXT("Raw ambiguous tactical diagnostic"), TEXT("fr-FR")),
		FString(TEXT("Plusieurs recettes de mission tactique correspondent au contexte de l'opération et au contact source.")));
	TestEqual(TEXT("Exact German tactical battle validation summarizes every guarded state family"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("invalid_tactical_battle"), TEXT("Raw invalid battle diagnostic"), TEXT("de-DE")),
		FString(TEXT("Identität, Einsatzverknüpfung, Abmessungen, Phase, Wetter, Zufallszustand, Zellen oder Fracht des taktischen Gefechts sind ungültig.")));
	TestEqual(TEXT("Exact Spanish game-layer lookup identifies an unavailable battlefield"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("unknown_tactical_battle"), TEXT("Raw unknown battle diagnostic"), TEXT("es-ES")),
		FString(TEXT("El campo de batalla táctico solicitado ya no está disponible.")));
	TestEqual(TEXT("Exact Japanese throw-trajectory guidance overrides the generic invalid-data family"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("invalid_tactical_throw_trajectory"), TEXT("Raw throw diagnostic"), TEXT("ja-JP")),
		FString(TEXT("有効な投擲軌道では選択したセルまで装置が届きません。")));
	TestEqual(TEXT("Exact French demo-only rejection no longer leaks its English fixture fallback"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("demo_action_unavailable"), TEXT("Raw demo diagnostic"), TEXT("fr-FR")),
		FString(TEXT("Cette démonstration de présentation n'active que les commandes exactes prévues.")));
	TestEqual(TEXT("Exact Japanese debrief safeguard identifies a missing resolution event"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("tactical_resolution_event_missing"), TEXT("Raw debrief diagnostic"), TEXT("ja-JP")),
		FString(TEXT("受理されたコマンドから対応する戦術解決イベントが発行されませんでした。")));
	TestEqual(TEXT("Unstaged diagnostic codes resolve through their localized semantic family"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("unknown_mod_target"), TEXT("Raw mod target diagnostic"), TEXT("ja-JP")),
		FString(TEXT("要求された対象は利用できなくなりました。")));
	TestEqual(TEXT("Semantic families recognize diagnostic keywords at the start of a code"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("already_at_mod_destination"), TEXT("Raw occupied-cell diagnostic"), TEXT("de-DE")),
		FString(TEXT("Der angeforderte Zustand ist bereits aktiv.")));
	TestEqual(TEXT("Exact tactical interaction diagnostics override their broader semantic family"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("tactical_door_out_of_reach"), TEXT("Raw tactical door diagnostic"), TEXT("fr-FR")),
		FString(TEXT("Placez d'abord l'unité sélectionnée à côté de la porte.")));
	TestEqual(TEXT("Exact Japanese tactical capacity guidance remains actionable"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("tactical_recovery_capacity_exceeded"), TEXT("Raw tactical recovery diagnostic"), TEXT("ja-JP")),
		FString(TEXT("輸送機に回収報酬を積む容量がありません。")));
	TestEqual(TEXT("Exact base-defense staffing diagnostics remain actionable in French"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("no_base_defenders"), TEXT("Raw ground-defense staffing diagnostic"), TEXT("fr-FR")),
		FString(TEXT("Aucun agent de terrain disponible et non affecté n'est stationné dans la base menacée.")));
	TestEqual(TEXT("Exact French manufacturing diagnostics replace dynamic raw material text with actionable guidance"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("manufacturing_materials_missing"), TEXT("Raw production-material diagnostic"), TEXT("fr-FR")),
		FString(TEXT("Les matériaux de production requis sont indisponibles.")));
	TestEqual(TEXT("Exact Japanese deployment-fuel diagnostics remain actionable"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("insufficient_deployment_fuel"), TEXT("Raw route-fuel diagnostic"), TEXT("ja-JP")),
		FString(TEXT("この航空機には現地へ展開して帰還するための燃料が不足しています。")));
	TestEqual(TEXT("Exact German duplicate-research diagnostics override the broader already-active family"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("research_already_active"), TEXT("Raw duplicate-research diagnostic"), TEXT("de-DE")),
		FString(TEXT("Für dieses Forschungsthema läuft bereits ein Projekt.")));
	TestEqual(TEXT("Exact French checksum diagnostics explain why a save cannot be trusted"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("save_checksum_mismatch"), TEXT("Raw checksum diagnostic"), TEXT("fr-FR")),
		FString(TEXT("La vérification de l'intégrité de la sauvegarde de campagne a échoué.")));
	TestEqual(TEXT("Exact Japanese backup diagnostics identify the failed recovery stage"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("backup_rotation_failed"), TEXT("Raw backup-rotation diagnostic"), TEXT("ja-JP")),
		FString(TEXT("以前のキャンペーンセーブをバックアップへ移動できませんでした。")));
	TestEqual(TEXT("Exact Spanish content compatibility diagnostics distinguish a mismatched package set"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("incompatible_content"), TEXT("Raw content-compatibility diagnostic"), TEXT("es-ES")),
		FString(TEXT("El conjunto de contenido activo no coincide con esta partida guardada de campaña.")));
	TestEqual(TEXT("Exact French doctrine-choice diagnostics remain actionable"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("personnel_doctrine_choice_unavailable"), TEXT("Raw doctrine-choice diagnostic"), TEXT("fr-FR")),
		FString(TEXT("Cette personne n'a aucun choix de doctrine de promotion en attente.")));
	TestEqual(TEXT("French battery readiness preserves exact authoritative values"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.defense-battery-readiness-many-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("2"), TEXT("155"), TEXT("124") }),
		FString(TEXT("2 BATTERIES  •  JUSQU'À 155 DÉGÂTS  •  ~124 ATTENDUS")));
	TestEqual(TEXT("French supply-limited readiness distinguishes operational and loaded batteries"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.defense-battery-supply-readiness-many-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("2"), TEXT("3"), TEXT("155"), TEXT("124") }),
		FString(TEXT("2/3 BATTERIES APPROVISIONNÉES  •  JUSQU'À 155 DÉGÂTS  •  ~124 ATTENDUS")));
	TestEqual(TEXT("Japanese defense supply stock can reorder a localized item and exact quantities"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.defense-supply-stock-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("周辺防衛コンデンサーバンク"), TEXT("3"), TEXT("7"), TEXT("3") }),
		FString(TEXT("周辺防衛コンデンサーバンク  •  在庫 3/7  •  割当 3")));
	TestEqual(TEXT("Reserved input keys resolve an exact French policy diagnostic"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("input_key_reserved"), TEXT("Raw reserved-key diagnostic"), TEXT("fr-FR")),
		FString(TEXT("Cette touche est réservée à la caméra, au curseur, au temps ou à la navigation dans les paramètres.")));
	TestEqual(TEXT("Japanese binding-swap feedback can reorder localized commands and neutral key names"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("status.input-binding-swapped-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("リロード"), TEXT("F"), TEXT("目標アクション"), TEXT("R") }),
		FString(TEXT("リロードをFに割り当て、競合を避けるため目標アクションをRへ移動しました。")));
	TestEqual(TEXT("Facility presentation diagnostics resolve from the retained stable code"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("facility_craft_capacity_required"), TEXT("Raw berth diagnostic"), TEXT("fr-FR")),
		FString(TEXT("Redéployez ou retirez les appareils avant de supprimer les postes d'amarrage dont ils ont besoin.")));
	TestEqual(TEXT("Localized construction feedback can reorder locale-neutral authoritative values"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.facility-construction-started-format"), TEXT("fallback"), TEXT("ja")),
			{ TEXT("飛行甲板"), TEXT("カスカディア司令部"), TEXT("2"), TEXT("4") }),
		FString(TEXT("カスカディア司令部のグリッド2,4で飛行甲板の建設を開始しました。")));
	TestEqual(TEXT("French base command summary localizes facility capacity chrome around authoritative values"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.base-command-summary-format"), TEXT("fallback"), TEXT("fr")),
			{
				TEXT("CASCADE"), TEXT("CASCADIA"),
				TEXT("5"), TEXT("12"), TEXT("8"), TEXT("12"), TEXT("8"), TEXT(" • DÉPASSEMENT 1"),
				TEXT("3"), TEXT("9"), TEXT("6"), TEXT("9"), TEXT("4"), TEXT(""),
				TEXT("1"), TEXT("2"), TEXT("1800"), TEXT("72"), TEXT("2"), TEXT("90"), TEXT("54"),
				TEXT("Centre des opérations")
			}),
		FString(TEXT("CASCADE  //  CASCADIA\nSCI AFFECTÉS 5/12 • EFFECTIF 8/12 • INST +8 • DÉPASSEMENT 1\nING AFFECTÉS 3/9 • EFFECTIF 6/9 • INST +4\nPOSTES 1/2   CAPTEUR 1800 km / 72%   DÉFENSE 2 BAT • MAX 90 • ATT ~54\nCentre des opérations")));
	TestEqual(TEXT("Japanese facility tooltip reorders spatial chrome without losing any authoritative value"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("strategic.facility-tooltip-format"), TEXT("fallback"), TEXT("ja")),
			{
				TEXT("作戦司令部"), TEXT("稼働中"), TEXT("320"), TEXT("400"), TEXT("80"),
				TEXT("1"), TEXT("2"), TEXT("2"), TEXT("2"),
				TEXT("科学者収容力 8/10"), TEXT("修理可能 • 資金24000 • 16時間"),
				TEXT("クリックして解体を確認 • サルベージ750。")
			}),
		FString(TEXT("作戦司令部 • グリッド 1,2 • 2×2 • 稼働中 • 耐久度 320/400 • 出力 80%\n科学者収容力 8/10\n修理可能 • 資金24000 • 16時間\nクリックして解体を確認 • サルベージ750。")));
	TestEqual(TEXT("Localized tactical confirmation preserves exact readiness values"),
		FString::Format(
			*Loaded.Catalog.Resolve(
				TEXT("tactical.end-turn-confirm-many-format"), TEXT("fallback"), TEXT("fr")),
			{ TEXT("3"), TEXT("17") }),
		FString(TEXT("3 agents prêts disposent encore de 17 PA. Appuyez de nouveau sur FIN DU TOUR pour confirmer.")));
	TestEqual(TEXT("Unrecognized diagnostic families preserve their explicit authored fallback"),
		FUEGTLocalizationService::DiagnosticTextForCulture(
			TEXT("mod_specific_condition"), TEXT("Mod-specific diagnostic"), TEXT("de")),
		FString(TEXT("Mod-specific diagnostic")));
	const FString OriginalCulture = FInternationalization::Get().GetCurrentLanguage()->GetName();
	TestTrue(TEXT("Unreal accepts an authored interface culture"),
		FInternationalization::Get().SetCurrentLanguageAndLocale(TEXT("fr")));
	TestEqual(TEXT("Runtime lookup follows Unreal's active language immediately"),
		FUEGTLocalizationService::Text(TEXT("menu.content-status"), TEXT("fallback")),
		FString(TEXT("ÉTAT DU CONTENU")));
	TestEqual(TEXT("Runtime diagnostic lookup follows Unreal's active language immediately"),
		FUEGTLocalizationService::DiagnosticText(
			TEXT("facility_repair_active"), TEXT("Raw active-repair diagnostic")),
		FString(TEXT("Annulez la réparation active de l'installation avant de continuer.")));
	TestTrue(TEXT("Locale snapshot restores the original Unreal culture"),
		FInternationalization::Get().SetCurrentLanguageAndLocale(OriginalCulture));

	FString Json;
	TestTrue(TEXT("Localization source remains readable for rejection fixtures"),
		FFileHelper::LoadFileToString(Json, *Filename));
	FString MissingJapanese = Json;
	MissingJapanese.ReplaceInline(TEXT("\"ja\": \"終了\""), TEXT("\"jp\": \"終了\""));
	const FUEGTLocalizationLoadResult MissingJapaneseResult =
		FUEGTLocalizationService::ParseCatalog(MissingJapanese);
	TestTrue(TEXT("Missing supported translation is rejected"),
		!MissingJapaneseResult.bSucceeded
		&& MissingJapaneseResult.Diagnostics.ContainsByPredicate(
			[](const FString& Diagnostic) { return Diagnostic.Contains(TEXT("non-empty 'ja'")); }));

	FString DuplicateKey = Json;
	DuplicateKey.ReplaceInline(TEXT("\"key\": \"menu.subtitle\""), TEXT("\"key\": \"menu.title\""));
	const FUEGTLocalizationLoadResult DuplicateResult =
		FUEGTLocalizationService::ParseCatalog(DuplicateKey);
	TestTrue(TEXT("Duplicate localized keys are rejected"),
		!DuplicateResult.bSucceeded
		&& DuplicateResult.Diagnostics.ContainsByPredicate(
			[](const FString& Diagnostic) { return Diagnostic.Contains(TEXT("duplicated")); }));

	FString EnglishMismatch = Json;
	EnglishMismatch.ReplaceInline(TEXT("\"en\": \"QUIT\""), TEXT("\"en\": \"EXIT\""));
	const FUEGTLocalizationLoadResult EnglishMismatchResult =
		FUEGTLocalizationService::ParseCatalog(EnglishMismatch);
	TestTrue(TEXT("English translation cannot diverge from source text"),
		!EnglishMismatchResult.bSucceeded
		&& EnglishMismatchResult.Diagnostics.ContainsByPredicate(
			[](const FString& Diagnostic) { return Diagnostic.Contains(TEXT("must equal its source")); }));

	FString MissingFormatPlaceholder = Json;
	MissingFormatPlaceholder.ReplaceInline(
		TEXT("\"fr\": \"Impossible de charger {0}.\""),
		TEXT("\"fr\": \"Impossible de charger.\""));
	const FUEGTLocalizationLoadResult MissingFormatPlaceholderResult =
		FUEGTLocalizationService::ParseCatalog(MissingFormatPlaceholder);
	TestTrue(TEXT("Localized runtime formats cannot drop authoritative value placeholders"),
		!MissingFormatPlaceholderResult.bSucceeded
		&& MissingFormatPlaceholderResult.Diagnostics.ContainsByPredicate(
			[](const FString& Diagnostic) { return Diagnostic.Contains(TEXT("indexed format placeholder")); }));

	FString PackagingConfig;
	const FString DefaultGameConfig = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultGame.ini"));
	TestTrue(TEXT("Project packaging configuration remains readable"),
		FFileHelper::LoadFileToString(PackagingConfig, *DefaultGameConfig));
	TestTrue(TEXT("Localization resources are staged for packaged builds"),
		PackagingConfig.Contains(TEXT("DirectoriesToAlwaysStageAsNonUFS=(Path=\"Localization\")")));
	return true;
}

#endif
