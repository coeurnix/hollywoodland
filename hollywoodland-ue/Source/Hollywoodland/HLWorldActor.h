// The one UE actor of the game: owns FHLGame, builds the gray-box world from data, mirrors units into simple mesh
// actors, drives the camera, and hosts the Slate UI. Everything visual is engine basic shapes + /Game/HL materials.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HLGame.h"
#include "HLEnemies.h"
#include "HLTerrain.h"
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
	TWeakObjectPtr<UStaticMeshComponent> Body, Head, Extra, Mount;
		bool bDeadLook = false, bMountLook = false, bGhostLook = false;
		// people from the character art (HLCharacter.h); gray-box shapes otherwise
		bool bHuman = false;
		// invaders and wildlife from the rigged enemy art (HLEnemies.h)
		bool bEnemy = false;
		FHLEnemyRig Enemy;
		FHLCharRig Rig;
		FHLCharLook Look;
		double NextLook = 0;
	};

// one running effect (HLFx.cpp): a projectile in flight, a burst, a ring or a column
struct FHLFxLive
{
	EHLFx Kind = EHLFx::Hit;
	FLinearColor Color = FLinearColor::White;
	int32 SrcId = 0, DstId = 0, Area = 0;
	FVector From = FVector::ZeroVector, To = FVector::ZeroVector; // world, cm
	float Age = 0, Life = 0.3f, Radius = 0;
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
	FVector ToWorld(int32 Area, const FVector2D& P, float Z = 0) const; // Z above the ground (terrain included)
		float GroundZ(int32 Area, const FVector2D& P) const { return Terrain.IsValidIndex(Area) ? Terrain[Area].Height(P) : 0.f; }
		TArray<FHLTerrain> Terrain; // per area (HLTerrain.h); empty grids indoors
		static const FVector MountSeat[4];
		// each area's static geometry: only the one the player is in is drawn (the dungeon's dome sat on the hills' horizon)
		TArray<TWeakObjectPtr<AActor>> AreaGeo;
		int32 GeoShownArea = -1;
		// grass, flowers, shrubs, rocks and trees over the open ground (HLTerrain wildness), instanced per mesh
		void ScatterFoliage(AActor* Geo, int32 Area);
		TArray<TWeakObjectPtr<class UInstancedStaticMeshComponent>> FoliageGrass; // grass and flower cards: cull distance by quality
		// a height-mapped mesh over [Lo, Hi] (area-local metres), ZOff above the terrain
		class UProceduralMeshComponent* TerrainMesh(AActor* InOwner, int32 Area, const FVector2D& Lo, const FVector2D& Hi, float ZOff, UMaterialInterface* Mat);

	UPROPERTY() TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TObjectPtr<UStaticMesh> Cube;
	UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
	UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
	UPROPERTY() TObjectPtr<UStaticMesh> Cone;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatLit;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatUnlit;
	UPROPERTY() TObjectPtr<UMaterialInterface> MatGround;  // painted ground tiles (M_HLGround)
	UPROPERTY() TObjectPtr<UMaterialInterface> MatSkyDome; // day/night panoramas (M_HLSkyDome)
	UPROPERTY() TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> GroundMids;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> SkyMids;
	UPROPERTY() TObjectPtr<class UDirectionalLightComponent> SunLight;
		UPROPERTY() TObjectPtr<class UMaterialParameterCollection> Ambient; // MPC_HLAmbient: hemisphere ambient in the materials
		UPROPERTY() TObjectPtr<class UExponentialHeightFogComponent> Fog;    // distance haze outdoors
	UPROPERTY() TObjectPtr<UMaterialInterface> MatAdd; // additive glow (M_HLAdd)
	UPROPERTY() TObjectPtr<AActor> FxActor;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FxPool;
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
		UPROPERTY() TObjectPtr<UStaticMeshComponent> StageBack; // the backdrop lightens behind darker skin
		int32 StageSkin = -1;
		static FLinearColor StageBackColor(int32 Skin); // also the UI frame behind the capture

private:
	FHLCharRig StageRig;
	double StageWantUntil = 0;
	void TickStage();
	UMaterialInstanceDynamic* Mid(const FLinearColor& C, bool bUnlit = false, float Glow = 0.22f);
	UStaticMeshComponent* AddMesh(AActor* InOwner, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FLinearColor& Color, bool bUnlit = false, bool bShadow = false);
	void BuildWorld();
	UMaterialInstanceDynamic* GroundMid(const FString& Kind, const FLinearColor& C); // null: no such tile (flat colour)
	void TickSky(); // day/night: sun and moon, sky blend, grading, lamps
	// effects (HLFx.cpp): game FX events become live effects; unit states (casting, shields, stuns, slows) draw every frame
	void TickFx(float Dt);
	void FxPiece(UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, const FVector& SizeM, const FLinearColor& C, float Opacity, float Rim);
	FVector FxUnitPos(int32 UnitId, float HeightFrac, bool* bOk = nullptr) const;
	TArray<FHLFxLive> FxLive;
	int32 FxUsed = 0;
	float BaseExposure = 0;
	int32 GfxVersion = -1;
	double NextShadowSweep = 0;
	void ApplyGraphics();
	void ShadowActor(AActor* A, bool bCast);
	// art/props static mesh at a placement (null when the mesh is missing: the caller falls back to a primitive)
	UStaticMeshComponent* AddProp(AActor* InOwner, int32 Area, const struct FHLPropPlace& P);
	UPROPERTY() TMap<FString, TObjectPtr<UStaticMesh>> PropMeshes;
	void AutoTest(float Dt); // -HLAuto=<class>: self-playing smoke test with screenshots (native and web)
	FString AutoClass; float AutoQuit = 0, AutoNextShot = 4, AutoNextAct = 0; int32 AutoShots = 0;
	FVector2D AutoGoal = FVector2D::ZeroVector; bool bAutoGoal = false; // bot walks here every frame
	double AutoSocialAt = 0;
	void SyncUnits();
	void BuildUnit(const FHLUnit& U, FHLUnitVis& V);
	TMap<int32, FHLUnitVis> Vis;
	TArray<TArray<TWeakObjectPtr<UStaticMeshComponent>>> ObjectMeshes;
	// hive structures: the wrecked mesh shown while the object is down, and the egg sac's living skeletal mesh
	TArray<TArray<TWeakObjectPtr<UStaticMeshComponent>>> ObjectDead;
	TArray<TArray<TWeakObjectPtr<class USkeletalMeshComponent>>> ObjectLive;
	TMap<TWeakObjectPtr<UStaticMeshComponent>, TWeakObjectPtr<class UPointLightComponent>> LampLights;
	TSharedPtr<class SHLGameUI> UI;
	TSharedPtr<class SWidget> UIContainer;
};
