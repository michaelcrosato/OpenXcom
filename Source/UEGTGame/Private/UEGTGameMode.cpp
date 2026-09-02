// Copyright 2026 UEGT contributors. MIT License.

#include "UEGTGameMode.h"

#include "Tactical/UEGTTacticalCameraPawn.h"
#include "Tactical/UEGTTacticalPlayerController.h"

AUEGTGameMode::AUEGTGameMode()
{
	DefaultPawnClass = AUEGTTacticalCameraPawn::StaticClass();
	PlayerControllerClass = AUEGTTacticalPlayerController::StaticClass();
	bStartPlayersAsSpectators = false;
}
