// The one UE actor of the game: owns FHLGame, builds the gray-box world from data, mirrors units into simple mesh
// actors, drives the camera, and hosts the Slate UI. Everything visual is engine basic shapes + /Game/HL materials.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HLGame.h"
#include "HLWorldActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UCameraComponent;

struct FHLUnitVis
{
	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<UStaticMeshComponent> Body, Head, Extra;
	bool bDeadLook = false, bMountLook = false;
};

UCLASS()
class AHLWorldActor : public AActor
{
	GENERATED_BODY()
public:
	AHLWorldActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float Dt) override;

	FHLGame Game;
	FString LoadError;
	bool bSaveChecked = false;
	FString SavedJson;
	// camera (driven by the UI)
	float CamYaw = 90, CamPitch = 22, CamDist = 14;
	FVector ToWorld(int32 Area, const FVector2D& P, float Z = 0) const;

	UPROPERTY() TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
	UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
	UPROPERTY() TObjectPtr<UStaticMesh> Cone;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatLit;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatUnlit;
	UPROPERTY() TMap<uint32, TObjectPtr<UMaterialInstanceDynamic>> Mids;
	UPROPERTY() TArray<TObjectPtr<AActor>> Spawned;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Ring;

private:
	UMaterialInstanceDynamic* Mid(const FLinearColor& C, bool bUnlit = false, float Glow = 0.22f);
	UStaticMeshComponent* AddMesh(AActor* InOwner, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FLinearColor& Color, bool bUnlit = false, bool bShadow = false);
	void BuildWorld();
	void AutoTest(float Dt); // -HLAuto=<class>: self-playing smoke test with screenshots (native and web)
	FString AutoClass; float AutoQuit = 0, AutoNextShot = 4, AutoNextAct = 0; int32 AutoShots = 0;
	void SyncUnits();
	void BuildUnit(const FHLUnit& U, FHLUnitVis& V);
	TMap<int32, FHLUnitVis> Vis;
	TArray<TArray<TWeakObjectPtr<UStaticMeshComponent>>> ObjectMeshes;
	TSharedPtr<class SHLGameUI> UI;
	TSharedPtr<class SWidget> UIContainer;
};
