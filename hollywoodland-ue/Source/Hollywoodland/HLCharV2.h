// v2 characters (CHARACTERS-V2.md): a full MakeHuman body on the UE-Mannequin-named skeleton, with real garment meshes
// that follow its pose. Every vertex carries a zone (vertex colour R): a worn piece hides the zones it covers on the
// layers under it (M_HLCharV2's HideMask), so skin and inner garments never poke through.
#pragma once
#include "CoreMinimal.h"

class AActor;
class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UAnimSequence;

namespace HLZone
{
	enum : uint32 { Head = 1u << 0, Neck = 1u << 1, Chest = 1u << 2, Belly = 1u << 3, Hips = 1u << 4, UpperArm = 1u << 5,
		Forearm = 1u << 6, Hand = 1u << 7, Thigh = 1u << 8, Calf = 1u << 9, Foot = 1u << 10 };
}

struct FHLV2Piece
{
	FString Mesh;                 // asset name under /Game/CharactersV2/Garments (skeletal) or Hats (static, Socket set)
	FString Tex, Mask;            // asset names under /Game/CharactersV2/Tex
	FLinearColor TintA = FLinearColor::White, TintB = FLinearColor::White;
	uint32 Covers = 0;            // zones this piece hides on the body and on earlier pieces
	int32 Layer = 0;              // 0 body .. higher is worn over lower
	FName Socket;                 // static piece: the bone it rides (hats: head)
};

struct FHLV2Rig
{
	TWeakObjectPtr<USkeletalMeshComponent> Body;
	TArray<TWeakObjectPtr<USceneComponent>> Pieces;
	int32 Sex = 0;
	FString Clip;
};

class FHLCharV2
{
public:
	// builds the body (Sex 0 male, 1 female) under Parent with the given pieces; returns the body (leader) component
	static USkeletalMeshComponent* Build(AActor* Owner, USceneComponent* Parent, int32 Sex, const TArray<FHLV2Piece>& Pieces, FHLV2Rig& Rig);
	static void Play(FHLV2Rig& Rig, const FString& Clip, bool bLoop = true, float Rate = 1.f);
	static UAnimSequence* Anim(int32 Sex, const FString& Clip);
};
