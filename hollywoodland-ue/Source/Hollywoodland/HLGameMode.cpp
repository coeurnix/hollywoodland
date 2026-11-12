#include "HLGameMode.h"
#include "GameFramework/SpectatorPawn.h"

AHLGameMode::AHLGameMode()
{
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}
