// Human characters from the art package (art/characters, imported by Scripts/import_characters.py into /Game/Characters).
// FHLChars reads Content/Data/characters.json (tools/charprep.py) and maps game state to a look: body (sex, skin, face,
// hair), the art "look" worn in each clothing slot (from the equipped items), a hat and the held items.
// FHLCharVisuals turns a look into components: modular skinned geosets that follow one leader mesh, rigid hair, a hat on
// the head bone, held items in the hands, and one body atlas per distinct look, composed on the GPU from the cropped
// layers (skin, underwear, face, scalp, then the clothing slots in the package's order) into a render target.
// Animation is single-node playback chosen in code (no AnimBP): see FHLCharVisuals::Drive.
#pragma once
#include "CoreMinimal.h"
#include "UObject/GCObject.h"

class FHLGame;
struct FHLUnit;
struct FHLItem;
class UObject;
class UWorld;
class AActor;
class USceneComponent;
class USkeletalMesh;
class UStaticMesh;
class UTexture2D;
class UAnimSequence;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;
class USkeletalMeshComponent;
class UStaticMeshComponent;

enum class EHLStance : uint8 { Unarmed, OneHand, TwoHand, Pistol, Rifle };

// the body a player picks in the creator (personas and NPCs derive theirs from data or a seed)
struct FHLAppearance
{
	int32 Sex = 0, Skin = 0, Face = 0, Hair = 0, HairColor = 0, Outfit = 0;
	static FHLAppearance FromSeed(uint32 Seed, int32 Sex = -1);
	bool operator==(const FHLAppearance& O) const { return Sex == O.Sex && Skin == O.Skin && Face == O.Face && Hair == O.Hair && HairColor == O.HairColor && Outfit == O.Outfit; }
};

struct FHLCharLook
{
	FHLAppearance App;
	FString Slots[7];          // art look id per slot (shirt trousers shoes coat wrist gloves belt); "" = bare
	FString Hat;               // art look id whose hat is worn
	FString Main, Off, Ranged; // held item ids (art package)
	EHLStance Stance = EHLStance::Unarmed;
	bool bRangedOut = false;   // the ranged weapon is in hand instead of the melee ones
	uint32 TexKey() const;     // what the body atlas depends on
	uint32 MeshKey() const;    // what the body components depend on
	uint32 HeldKey() const;    // what the held items depend on (drawing a gun swaps these alone)
};

class FHLChars : public FGCObject
{
public:
	static FHLChars& Get();
	bool Ready();
	static constexpr int32 NumSlots = 7;
	static const TCHAR* SlotName[NumSlots];
	struct FLayer { FString Tex, Mask; FVector4f Rect = FVector4f(0, 0, 1, 1); TArray<FString> Geosets; bool bValid = false; };
	struct FLook { FString Name, Type; int32 Tier = 0; FLayer Slot[2][NumSlots]; FString HatMesh[2], HatTex[2]; bool bHidesTop[2] = { false, false }; };
	struct FHeld { FString Id, Name, Type, Mesh, Tex; bool bLeft = false, bGripL = false; FVector GripL = FVector::ZeroVector; float Roll = 0, Grip = 0, Pitch = 0; }; // Grip: cm along +X from the mesh origin to the hand
	struct FHair { FString Name, Mesh, Top; FLayer Scalp; };
	struct FSex { TArray<FString> Skins; FLayer Underwear; TArray<FLayer> Faces; TArray<FString> Defaults; FString HairTex; TArray<FHair> Hair; FVector HatAnchor = FVector::ZeroVector; float Height = 180; };
	FSex Sexes[2];
	TMap<FString, FLook> Looks;
	TMap<FString, FHeld> Held;
	TArray<FLinearColor> OutfitA, OutfitB; // starter dye swatches (linear)
	static const TArray<FLinearColor>& HairColors();   // sRGB targets
	static const TArray<FString>& HairColorNames();
	static const TArray<FString>& SkinNames();
	static FLinearColor HairTint(int32 Index);        // linear tint for the grayscale hair paint

	FString LookForItem(const FHLItem* It) const;
	FString HeldForItem(const FHLItem* It) const;
	FHLCharLook LookFor(const FHLGame& G, const FHLUnit& U) const;
	FHLCharLook LookFromGear(const FHLAppearance& App, const TMap<FString, const FHLItem*>& Gear, bool bRangedOut) const;

	UTexture2D* Tex(const FString& Path);
	USkeletalMesh* SkelMesh(const FString& Folder, const FString& Name);
	UStaticMesh* StaticMesh(const FString& Folder, const FString& Name);
	UAnimSequence* Anim(int32 Sex, const FString& Clip);
	UMaterialInterface* Mat(const TCHAR* Name);

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FHLChars"); }
private:
	bool bLoaded = false, bOk = false;
	TMap<FString, TObjectPtr<UObject>> Cache;
	TSet<FString> Missing;
	UObject* Load(const FString& Path);
};

// components of one character on an actor
struct FHLCharRig
{
	TWeakObjectPtr<USceneComponent> Root;               // carries the mesh-facing correction
	TWeakObjectPtr<USkeletalMeshComponent> Leader;      // plays the animation
	TArray<TWeakObjectPtr<USceneComponent>> Parts;      // followers, hair, hat
	TArray<TWeakObjectPtr<USceneComponent>> HeldParts;  // weapons and off-hand items on the hand bones
	FVector Fwd = FVector::ForwardVector;               // the mesh's forward in component space
	uint32 MeshKey = 0, TexKey = 0, HeldKey = 0;
	int32 Sex = -1, AtlasSize = 0;
	TWeakObjectPtr<class UMaterialInstanceDynamic> BodyMat; // the atlas material on the body (atlas slots get recycled)
	// animation state
	FString Base, Shot;
	double ShotUntil = 0;
	float Rate = 1;
	FVector2D LastPos = FVector2D::ZeroVector;
	float Speed = 0, LastSwing = 0;
	bool bWasCasting = false, bHasLast = false, bWasSitting = false, bDancing = false;
	int32 LastAttack = 0, GestureSeq = 0;
};

class FHLCharVisuals : public FGCObject
{
public:
	// (re)builds the rig when the look changed; Parent is the component the character stands on (feet at its origin,
	// facing its +X)
	void Apply(UWorld* World, AActor* Owner, USceneComponent* Parent, FHLCharRig& Rig, const FHLCharLook& L, int32 AtlasSize);
	void Clear(FHLCharRig& Rig);
	// picks and plays the clip for this frame from the unit's state
	void Drive(FHLCharRig& Rig, const FHLUnit& U, const FHLCharLook& L, double Now, float Dt);
	// plain clip control (creator / paper doll)
	void Play(FHLCharRig& Rig, const FString& Clip, bool bLoop, float Rate = 1.f);
	void PlayOnce(FHLCharRig& Rig, const FString& Clip, double Now, float Rate = 1.f);
	UTextureRenderTarget2D* Atlas(UWorld* World, const FHLCharLook& L, int32 Size);
	void Tick(double Now) { Clock = Now; }
	int32 NumAtlases() const { return Atlases.Num(); }

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FHLCharVisuals"); }
private:
	struct FAtlas { TObjectPtr<UTextureRenderTarget2D> RT; TObjectPtr<UMaterialInstanceDynamic> Mid; uint32 Key = 0; int32 Size = 0; double Used = 0, Born = 0; int32 Redo = 0; bool bDirty = false; };
	TArray<FAtlas> Atlases;
	TMap<uint64, int32> AtlasIndex;
	TArray<TObjectPtr<UMaterialInstanceDynamic>> LayerMids;
	TMap<FString, TObjectPtr<UMaterialInstanceDynamic>> PropMids;
	double Clock = 0;
	UMaterialInstanceDynamic* BodyMid(UWorld* World, const FHLCharLook& L, int32 Size);
	UMaterialInstanceDynamic* PropMid(const FString& Tex, const FLinearColor& Tint, const TCHAR* Material);
	// false when a texture was not uploaded yet (the web loads them after the first frames): draw again later
	bool Compose(UWorld* World, UTextureRenderTarget2D* RT, const FHLCharLook& L);
	FString ClipFor(int32 Sex, std::initializer_list<const TCHAR*> Names) const;
	void AttachHeld(AActor* Owner, FHLCharRig& Rig, const FHLCharLook& L);
};
