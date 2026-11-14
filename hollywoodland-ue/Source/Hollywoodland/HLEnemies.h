// Rigged enemies (art/enemies -> /Game/Enemies, tools/enemyprep.py -> Content/Data/enemies.json): which skeletal mesh,
// material and clips each mob uses, and the destroyed hive structures. Playback is single-node, chosen in code like the
// people (HLCharacter.h): FHLEnemyRig::Drive picks idle / walk / run / attack / hit / death and the specials each frame.
#pragma once
#include "CoreMinimal.h"
#include "UObject/GCObject.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UAnimSequence;
class UMaterialInterface;
struct FHLUnit;

struct FHLEnemyClip { FString Anim; float Len = 1, Ev = -1; bool bLoop = false; };
struct FHLEnemyModel
{
	FString Id, Mesh, Kind;
	TMap<FString, FString> Roles;       // idle, walk, run, attack, hit, death -> clip
	TMap<FString, FHLEnemyClip> Clips;  // clip -> asset
	bool Has(const FString& Clip) const { return Clips.Contains(Clip); }
	FString Role(const TCHAR* R) const { const FString* C = Roles.Find(R); return C ? *C : FString(); }
};
struct FHLEnemyMob { FString Model, Mat; TMap<FString, FString> Specials; }; // ability id / script cue -> clip
struct FHLEnemyStructure { FString Destroyed, Mat, Live; };

// per-unit playback state (lives in FHLUnitVis)
struct FHLEnemyRig
{
	TWeakObjectPtr<USkeletalMeshComponent> Comp;
	const FHLEnemyModel* Model = nullptr;
	const FHLEnemyMob* Mob = nullptr;
	FString Base, Shot;
	double ShotUntil = 0;
	float LastSwing = 0, LastHp = -1, Speed = 0;
	FVector2D LastPos = FVector2D::ZeroVector;
	int32 CueSeq = 0;
	bool bHasLast = false, bWasBurrowed = false, bWasDead = false;
};

class FHLEnemyArt : public FGCObject
{
public:
	static FHLEnemyArt& Get();
	bool Load(); // once; false when enemies.json is missing (gray-box shapes stay)
	bool Ready() const { return bReady; }
	const FHLEnemyMob* Mob(const FString& MobId) const { return Mobs.Find(MobId); }
	const FHLEnemyModel* Model(const FString& Id) const { return Models.Find(Id); }
	const FHLEnemyStructure* Structure(const FString& Id) const { return Structures.Find(Id); }
	USkeletalMesh* Mesh(const FHLEnemyModel& M);
	UMaterialInterface* Material(const FString& Path);
	UStaticMesh* StaticMesh(const FString& Path);
	UAnimSequence* Anim(const FHLEnemyModel& M, const FString& Clip);
	// one rig: make the component (null when the art is missing), then drive it every frame
	USkeletalMeshComponent* Make(AActor* Owner, USceneComponent* Parent, FHLEnemyRig& Rig, const FString& MobId);
	void Drive(FHLEnemyRig& Rig, const FHLUnit& U, double Now, float Dt);
	void Play(FHLEnemyRig& Rig, const FString& Clip, bool bLoop, float Rate = 1.f);
	void PlayOnce(FHLEnemyRig& Rig, const FString& Clip, double Now, float Rate = 1.f);

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FHLEnemyArt"); }

private:
	bool bTried = false, bReady = false;
	TMap<FString, FHLEnemyModel> Models;
	TMap<FString, FHLEnemyMob> Mobs;
	TMap<FString, FHLEnemyStructure> Structures;
	TMap<FString, TObjectPtr<UObject>> Cache; // asset path -> loaded object (null entries remember misses)
	UObject* LoadPath(const FString& Path, UClass* Class);
};
