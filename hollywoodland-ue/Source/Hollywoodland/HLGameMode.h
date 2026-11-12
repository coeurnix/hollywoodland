#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HLGameMode.generated.h"

/** Spawns the single AHLWorldActor that runs the whole game. No pawn: the camera lives on the world actor. */
UCLASS()
class AHLGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AHLGameMode();
	virtual void StartPlay() override;
};
