#include "HLGameMode.h"
#include "HLWorldActor.h"
#include "Engine/World.h"

AHLGameMode::AHLGameMode()
{
	DefaultPawnClass = nullptr;
}

void AHLGameMode::StartPlay()
{
	Super::StartPlay();
	GetWorld()->SpawnActor<AHLWorldActor>(AHLWorldActor::StaticClass(), FTransform::Identity);
}
