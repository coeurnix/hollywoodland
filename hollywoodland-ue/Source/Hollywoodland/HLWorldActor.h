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
class UTextureRenderTarget2D;

struct FHLUnitVis
{
	TWeakObjectPtr<AActor> Actor;
	TWeakObjectPtr<UStaticMeshComponent> Body, Head, Extra;
		bool bDeadLook = false, bMountLook = false, bGhostLook = false;
		// people from the character art (HLCharacter.h); gray-box shapes otherwise
		bool bHuman = false;
		FHLCharRig Rig;
		FHLCharLook Look;
		double NextLook = 0;
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
	float CamYaw = -90, CamPitch = 22, CamDist = 14, CamCollideDist = 40;
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
	FHLCharVisuals CharVis; // body atlases and character materials

	// character preview (creator, paper doll): one rig on a little stage far off the map, filmed by a capture that sees
	// only the stage. The UI calls Preview every frame it shows one; the capture stops when nobody asks.
	UTextureRenderTarget2D* Preview(const FHLCharLook& L, float Yaw, const FString& Clip);
	UPROPERTY() TObjectPtr<AActor> Stage;
	UPROPERTY() TObjectPtr<class USceneCaptureComponent2D> StageCap;
	UPROPERTY() TObjectPtr<UTextureRenderTarget2D> StageRT;
	UPROPERTY() TObjectPtr<USceneComponent> StageChar;

private:
	FHLCharRig StageRig;
	double StageWantUntil = 0;
	void TickStage();
	UMaterialInstanceDynamic* Mid(const FLinearColor& C, bool bUnlit = false, float Glow = 0.22f);
	UStaticMeshComponent* AddMesh(AActor* InOwner, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FLinearColor& Color, bool bUnlit = false, bool bShadow = false);
	void BuildWorld();
	void AutoTest(float Dt); // -HLAuto=<class>: self-playing smoke test with screenshots (native and web)
	FString AutoClass; float AutoQuit = 0, AutoNextShot = 4, AutoNextAct = 0; int32 AutoShots = 0;
	FVector2D AutoGoal = FVector2D::ZeroVector; bool bAutoGoal = false; // bot walks here every frame
	double AutoSocialAt = 0;
	void SyncUnits();
	void BuildUnit(const FHLUnit& U, FHLUnitVis& V);
	TMap<int32, FHLUnitVis> Vis;
	TArray<TArray<TWeakObjectPtr<UStaticMeshComponent>>> ObjectMeshes;
	TSharedPtr<class SHLGameUI> UI;
	TSharedPtr<class SWidget> UIContainer;
};
