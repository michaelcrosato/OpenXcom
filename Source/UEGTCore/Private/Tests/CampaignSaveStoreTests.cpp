// Copyright 2026 UEGT contributors. MIT License.

#if WITH_DEV_AUTOMATION_TESTS

#include "Campaign/CampaignSaveStore.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace CampaignSaveStoreTests
{
	TArray<FCampaignContentVersion> MakePackages()
	{
		FCampaignContentVersion Base;
		Base.PackageId = TEXT("uegt.base");
		Base.Version = TEXT("1.0.0");
		return { Base };
	}

	FCampaignSaveEnvelope MakeEnvelope(const int64 Funds)
	{
		FCampaignState State;
		State.Funds = Funds;
		State.SimulationRandom.Initialize(1907);
		return FCampaignSaveCodec::CreateNew(
			State,
			MakePackages(),
			TEXT("0.2.0-test"),
			FDateTime(2026, 8, 29, 20, 0, 0),
			FGuid(0x10203040, 0x50607080, 0x90abcdef, 0x11223344));
	}

	FString ResetTestDirectory(const FString& TestName)
	{
		const FString Directory = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CampaignSaveStore"), TestName));
		IFileManager::Get().DeleteDirectory(*Directory, false, true);
		return Directory;
	}

	void CleanupTestDirectory(const FString& Directory)
	{
		IFileManager::Get().DeleteDirectory(*Directory, false, true);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveStoreBackupRecoveryTest,
	"UEGT.Core.CampaignSaveStore.BackupRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveStoreBackupRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveStoreTests;

	const FString Directory = ResetTestDirectory(TEXT("BackupRecovery"));
	const FString Slot = TEXT("campaign-01");
	const FCampaignSaveStoreResult FirstSave = FCampaignSaveStore::Save(
		Directory,
		Slot,
		MakeEnvelope(100),
		FDateTime(2026, 8, 29, 20, 5, 0));
	TestTrue(TEXT("First slot write succeeds"), FirstSave.bSucceeded);
	TestTrue(TEXT("Primary file exists after first write"), IFileManager::Get().FileExists(*FCampaignSaveStore::GetPrimaryPath(Directory, Slot)));

	const FCampaignSaveStoreResult SecondSave = FCampaignSaveStore::Save(
		Directory,
		Slot,
		MakeEnvelope(200),
		FDateTime(2026, 8, 29, 20, 10, 0));
	TestTrue(TEXT("Second slot write succeeds"), SecondSave.bSucceeded);
	TestTrue(TEXT("Prior primary is retained as backup"), IFileManager::Get().FileExists(*FCampaignSaveStore::GetBackupPath(Directory, Slot)));

	const FCampaignSaveStoreResult Current = FCampaignSaveStore::Load(Directory, Slot, MakePackages());
	TestTrue(TEXT("Newest primary loads"), Current.bSucceeded);
	TestFalse(TEXT("Healthy primary is not recovery"), Current.bRecovered);
	TestTrue(TEXT("Healthy load reports primary source"), Current.Source == ECampaignSaveSource::Primary);
	TestEqual(TEXT("Newest state is selected"), Current.Envelope.State.Funds, int64(200));

	TestTrue(
		TEXT("Corrupt primary fixture writes"),
		FFileHelper::SaveStringToFile(TEXT("{corrupt"), *FCampaignSaveStore::GetPrimaryPath(Directory, Slot), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	const FCampaignSaveStoreResult Recovered = FCampaignSaveStore::Load(Directory, Slot, MakePackages());
	TestTrue(TEXT("Valid backup recovers a corrupt primary"), Recovered.bSucceeded);
	TestTrue(TEXT("Backup load reports recovery"), Recovered.bRecovered);
	TestTrue(TEXT("Backup source is explicit"), Recovered.Source == ECampaignSaveSource::Backup);
	TestTrue(TEXT("Recovery emits a stable diagnostic"), Recovered.HasDiagnostic(TEXT("save_recovered")));
	TestEqual(TEXT("Backup preserves the prior committed state"), Recovered.Envelope.State.Funds, int64(100));

	FString BackupBeforeResave;
	TestTrue(TEXT("Verified recovery backup can be read"), FFileHelper::LoadFileToString(
		BackupBeforeResave, *FCampaignSaveStore::GetBackupPath(Directory, Slot)));
	const FCampaignSaveStoreResult Resaved = FCampaignSaveStore::Save(
		Directory, Slot, MakeEnvelope(300), FDateTime(2026, 8, 29, 20, 15, 0));
	TestTrue(TEXT("Saving after backup recovery succeeds"), Resaved.bSucceeded);
	FString BackupAfterResave;
	TestTrue(TEXT("Recovery backup still exists after resaving"), FFileHelper::LoadFileToString(
		BackupAfterResave, *FCampaignSaveStore::GetBackupPath(Directory, Slot)));
	TestEqual(TEXT("Corrupt primary does not overwrite the verified backup"),
		BackupAfterResave, BackupBeforeResave);
	const FCampaignSaveStoreResult ResavedLoad = FCampaignSaveStore::Load(Directory, Slot, MakePackages());
	TestTrue(TEXT("New committed state loads from the primary"), ResavedLoad.bSucceeded
		&& ResavedLoad.Source == ECampaignSaveSource::Primary && ResavedLoad.Envelope.State.Funds == 300);

	TestTrue(TEXT("Second corrupt primary fixture writes"), FFileHelper::SaveStringToFile(
		TEXT("{corrupt-again"), *FCampaignSaveStore::GetPrimaryPath(Directory, Slot),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	const FCampaignSaveStoreResult RecoveredAgain = FCampaignSaveStore::Load(Directory, Slot, MakePackages());
	TestTrue(TEXT("Backup remains recoverable after another primary corruption"), RecoveredAgain.bSucceeded
		&& RecoveredAgain.Source == ECampaignSaveSource::Backup && RecoveredAgain.Envelope.State.Funds == 100);

	CleanupTestDirectory(Directory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveStoreInterruptedWriteTest,
	"UEGT.Core.CampaignSaveStore.InterruptedWriteRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveStoreInterruptedWriteTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveStoreTests;

	const FString Directory = ResetTestDirectory(TEXT("InterruptedWrite"));
	const FString Slot = TEXT("autosave");
	const FCampaignSaveStoreResult PrimarySave = FCampaignSaveStore::Save(
		Directory,
		Slot,
		MakeEnvelope(300),
		FDateTime(2026, 8, 29, 20, 5, 0));
	TestTrue(TEXT("Primary fixture saves"), PrimarySave.bSucceeded);

	FCampaignSaveEnvelope NewerEnvelope = MakeEnvelope(400);
	NewerEnvelope.Header.LastSavedUtc = FDateTime(2026, 8, 29, 20, 20, 0);
	const FCampaignSaveWriteResult NewerWrite = FCampaignSaveCodec::Serialize(NewerEnvelope);
	TestTrue(TEXT("Interrupted temporary fixture serializes"), NewerWrite.bSucceeded);
	TestTrue(
		TEXT("Verified temporary candidate fixture writes"),
		FFileHelper::SaveStringToFile(NewerWrite.Json, *FCampaignSaveStore::GetTemporaryPath(Directory, Slot), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

	const FCampaignSaveStoreResult Recovered = FCampaignSaveStore::Load(Directory, Slot, MakePackages());
	TestTrue(TEXT("Newer valid temporary save is recovered"), Recovered.bSucceeded);
	TestTrue(TEXT("Temporary selection reports recovery"), Recovered.bRecovered);
	TestTrue(TEXT("Temporary source is explicit"), Recovered.Source == ECampaignSaveSource::Temporary);
	TestEqual(TEXT("Newest verified state wins"), Recovered.Envelope.State.Funds, int64(400));

	FCampaignSaveEnvelope StaleEnvelope = MakeEnvelope(250);
	StaleEnvelope.Header.LastSavedUtc = FDateTime(2026, 8, 29, 20, 1, 0);
	const FCampaignSaveWriteResult StaleWrite = FCampaignSaveCodec::Serialize(StaleEnvelope);
	TestTrue(
		TEXT("Stale temporary fixture writes"),
		FFileHelper::SaveStringToFile(StaleWrite.Json, *FCampaignSaveStore::GetTemporaryPath(Directory, Slot), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	const FCampaignSaveStoreResult PrimaryWins = FCampaignSaveStore::Load(Directory, Slot, MakePackages());
	TestTrue(TEXT("Healthy newer primary still loads"), PrimaryWins.bSucceeded);
	TestTrue(TEXT("Primary beats stale temporary candidate"), PrimaryWins.Source == ECampaignSaveSource::Primary);
	TestEqual(TEXT("Stale temporary does not roll state back"), PrimaryWins.Envelope.State.Funds, int64(300));

	CleanupTestDirectory(Directory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveStoreListingTest,
	"UEGT.Core.CampaignSaveStore.ValidatedSlotListing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveStoreListingTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveStoreTests;

	const FString Directory = ResetTestDirectory(TEXT("ValidatedSlotListing"));
	TestTrue(TEXT("First alpha save writes"), FCampaignSaveStore::Save(
		Directory, TEXT("Alpha"), MakeEnvelope(100), FDateTime(2026, 8, 29, 20, 5, 0)).bSucceeded);
	TestTrue(TEXT("Second alpha save creates recovery history"), FCampaignSaveStore::Save(
		Directory, TEXT("Alpha"), MakeEnvelope(150), FDateTime(2026, 8, 29, 20, 10, 0)).bSucceeded);
	TestTrue(TEXT("Newer beta save writes"), FCampaignSaveStore::Save(
		Directory, TEXT("Beta"), MakeEnvelope(250), FDateTime(2026, 8, 29, 20, 20, 0)).bSucceeded);
	TestTrue(TEXT("Corrupt alpha primary fixture writes"), FFileHelper::SaveStringToFile(
		TEXT("{corrupt"), *FCampaignSaveStore::GetPrimaryPath(Directory, TEXT("Alpha")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	TestTrue(TEXT("Unrecoverable slot fixture writes"), FFileHelper::SaveStringToFile(
		TEXT("{broken"), *FCampaignSaveStore::GetPrimaryPath(Directory, TEXT("Broken")),
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

	const FCampaignSaveSlotListResult Listed = FCampaignSaveStore::List(Directory, MakePackages());
	TestTrue(TEXT("Existing save directory enumerates successfully"), Listed.bSucceeded);
	TestEqual(TEXT("Primary, recovered, and corrupt slots are all visible"), Listed.Slots.Num(), 3);
	if (Listed.Slots.Num() == 3)
	{
		TestTrue(TEXT("Loadable slots sort newest first"),
			Listed.Slots[0].SlotName == TEXT("Beta")
			&& Listed.Slots[0].bLoadable
			&& Listed.Slots[0].Funds == 250);
		TestTrue(TEXT("Backup-only recovery is explicit in slot summary"),
			Listed.Slots[1].SlotName == TEXT("Alpha")
			&& Listed.Slots[1].bLoadable
			&& Listed.Slots[1].bRecovered
			&& Listed.Slots[1].Source == ECampaignSaveSource::Backup
			&& Listed.Slots[1].Funds == 100);
		TestTrue(TEXT("Unrecoverable slot remains visible but disabled"),
			Listed.Slots[2].SlotName == TEXT("Broken")
			&& !Listed.Slots[2].bLoadable
			&& !Listed.Slots[2].Diagnostics.IsEmpty());
	}

	const FCampaignSaveSlotListResult MissingDirectory = FCampaignSaveStore::List(
		FPaths::Combine(Directory, TEXT("NotCreated")), MakePackages());
	TestTrue(TEXT("Missing save directory is a successful empty browser"),
		MissingDirectory.bSucceeded && MissingDirectory.Slots.IsEmpty());
	const FCampaignSaveSlotListResult InvalidDirectory = FCampaignSaveStore::List(FString());
	TestFalse(TEXT("Empty listing path fails safely"), InvalidDirectory.bSucceeded);
	TestTrue(TEXT("Invalid listing path has a stable diagnostic"),
		InvalidDirectory.HasDiagnostic(TEXT("invalid_save_directory")));

	CleanupTestDirectory(Directory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCampaignSaveStorePathSafetyTest,
	"UEGT.Core.CampaignSaveStore.PathSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCampaignSaveStorePathSafetyTest::RunTest(const FString& Parameters)
{
	using namespace CampaignSaveStoreTests;

	const FString Directory = ResetTestDirectory(TEXT("PathSafety"));
	TestTrue(TEXT("Letters, digits, underscore, and hyphen are accepted"), FCampaignSaveStore::IsValidSlotName(TEXT("Campaign_01-auto")));
	TestFalse(TEXT("Directory traversal is rejected"), FCampaignSaveStore::IsValidSlotName(TEXT("../escape")));
	TestFalse(TEXT("Path separators are rejected"), FCampaignSaveStore::IsValidSlotName(TEXT("folder/slot")));
	TestFalse(TEXT("Empty slot name is rejected"), FCampaignSaveStore::IsValidSlotName(FString()));

	const FCampaignSaveStoreResult InvalidSave = FCampaignSaveStore::Save(
		Directory,
		TEXT("../escape"),
		MakeEnvelope(500),
		FDateTime(2026, 8, 29, 20, 30, 0));
	TestFalse(TEXT("Unsafe slot cannot be written"), InvalidSave.bSucceeded);
	TestTrue(TEXT("Unsafe slot has a stable diagnostic"), InvalidSave.HasDiagnostic(TEXT("invalid_slot_name")));

	const FCampaignSaveStoreResult Missing = FCampaignSaveStore::Load(Directory, TEXT("not-created"));
	TestFalse(TEXT("Missing slot fails cleanly"), Missing.bSucceeded);
	TestTrue(TEXT("Missing slot has a stable diagnostic"), Missing.HasDiagnostic(TEXT("save_not_found")));

	CleanupTestDirectory(Directory);
	return true;
}

#endif
