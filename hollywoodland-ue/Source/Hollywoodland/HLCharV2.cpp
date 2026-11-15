#include "HLCharV2.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "Misc/CommandLine.h"

static const TCHAR* V2Base = TEXT("/Game/CharactersV2");
static const TCHAR* V2Sex[2] = { TEXT("Male"), TEXT("Female") };

template <typename T>
static T* V2Load(const FString& Folder, const FString& Name)
{
	if (Name.IsEmpty()) return nullptr;
	return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s/%s/%s.%s"), V2Base, *Folder, *Name, *Name), nullptr, LOAD_NoWarn);
}

static UMaterialInstanceDynamic* V2Mat(UObject* Outer, bool bMasked, const FString& Tex, const FString& Mask, const FLinearColor& A, const FLinearColor& B, uint32 Hide)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("HLV2NoMat"))) return nullptr;
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, bMasked ? TEXT("/Game/CharactersV2/M_HLCharV2Masked.M_HLCharV2Masked") : TEXT("/Game/CharactersV2/M_HLCharV2.M_HLCharV2"), nullptr, LOAD_NoWarn);
	if (!Base) { UE_LOG(LogTemp, Warning, TEXT("HL_V2 base material missing")); return nullptr; }
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
	if (UTexture2D* T = Tex.IsEmpty() ? nullptr : V2Load<UTexture2D>(TEXT("Tex"), TEXT("T_") + Tex)) M->SetTextureParameterValue(TEXT("Tex"), T);
	else if (!Tex.IsEmpty()) UE_LOG(LogTemp, Warning, TEXT("HL_V2 missing texture T_%s"), *Tex);
	if (UTexture2D* T = Mask.IsEmpty() ? nullptr : V2Load<UTexture2D>(TEXT("Tex"), TEXT("T_") + Mask)) M->SetTextureParameterValue(TEXT("Mask"), T);
	M->SetVectorParameterValue(TEXT("TintA"), A);
	M->SetVectorParameterValue(TEXT("TintB"), B);
	M->SetScalarParameterValue(TEXT("HideMask"), (float)Hide);
	{
		UTexture* Got = nullptr; M->GetTextureParameterValue(FHashedMaterialParameterInfo(TEXT("Tex")), Got);
		UE_LOG(LogTemp, Display, TEXT("HL_V2 mat %s tex=%s got=%s hide=%u"), *Base->GetName(), *Tex, Got ? *Got->GetName() : TEXT("none"), Hide);
	}
	return M;
}

UAnimSequence* FHLCharV2::Anim(int32 Sex, const FString& Clip)
{
	return V2Load<UAnimSequence>(TEXT("Anim"), FString::Printf(TEXT("A_%s_%s"), V2Sex[Sex & 1], *Clip));
}

USkeletalMeshComponent* FHLCharV2::Build(AActor* Owner, USceneComponent* Parent, int32 Sex, const TArray<FHLV2Piece>& Pieces, FHLV2Rig& Rig)
{
	Sex &= 1;
	const FString Prefix = FString::Printf(TEXT("SKV2_%s"), V2Sex[Sex]);
	USkeletalMesh* BodyMesh = V2Load<USkeletalMesh>(TEXT("Body"), Prefix + TEXT("_Body"));
	if (!BodyMesh) return nullptr;
	auto Skel = [&](USkeletalMesh* Mesh, USceneComponent* Attach) -> USkeletalMeshComponent*
	{
		USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(Owner);
		C->SetSkeletalMeshAsset(Mesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		C->SetupAttachment(Attach);
		C->RegisterComponent();
		return C;
	};
	USkeletalMeshComponent* Body = Skel(BodyMesh, Parent);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Rig.Body = Body;
	Rig.Sex = Sex;
	// what each layer hides: everything worn over it
	uint32 BodyHide = 0;
	for (const FHLV2Piece& P : Pieces) BodyHide |= P.Covers;
	// body sections by material slot name: skin, eyes, brows, lashes
	const TArray<FSkeletalMaterial>& Slots = BodyMesh->GetMaterials();
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FString N = Slots[i].MaterialSlotName.ToString().ToLower();
		UE_LOG(LogTemp, Display, TEXT("HL_V2 body slot %d %s"), i, *N);
		FString Tex = Prefix + TEXT("_Body_D");
		bool bMasked = false;
		if (N.Contains(TEXT("eyebrow"))) { Tex = Prefix + TEXT("_Body_") + N.RightChop(N.Find(TEXT("eyebrow"))) + TEXT("_D"); bMasked = true; }
		else if (N.Contains(TEXT("eyelash"))) { Tex = Prefix + TEXT("_Body_") + N.RightChop(N.Find(TEXT("eyelash"))) + TEXT("_D"); bMasked = true; }
		else if (N.Contains(TEXT("high-poly")) || N.Contains(TEXT("low-poly")) || N.Contains(TEXT("eye"))) Tex = Prefix + TEXT("_Body_high-poly_D");
		Tex = Tex.Replace(TEXT("."), TEXT("_")).Replace(TEXT("-"), TEXT("_"));
		if (UMaterialInstanceDynamic* M = V2Mat(Owner, bMasked, Tex, FString(), FLinearColor::White, FLinearColor::White, bMasked ? 0u : BodyHide))
			Body->SetMaterial(i, M);
	}
	for (int32 k = 0; k < Pieces.Num(); ++k)
	{
		const FHLV2Piece& P = Pieces[k];
		uint32 Hide = 0;
		for (const FHLV2Piece& Q : Pieces) if (Q.Layer > P.Layer) Hide |= Q.Covers;
		if (!P.Socket.IsNone())
		{
			UStaticMesh* SM = V2Load<UStaticMesh>(TEXT("Hats"), P.Mesh);
			if (!SM) continue;
			UStaticMeshComponent* S = NewObject<UStaticMeshComponent>(Owner);
			S->SetStaticMesh(SM);
			S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// the hat was exported in body space: undo the head bone's bind transform so it sits where it was fitted
			const FTransform HeadRef = Body->GetBoneTransform(Body->GetBoneIndex(P.Socket), FTransform::Identity);
			S->SetupAttachment(Body, P.Socket);
			S->RegisterComponent();
			S->SetRelativeTransform(HeadRef.Inverse());
			if (UMaterialInstanceDynamic* M = V2Mat(Owner, false, P.Tex, P.Mask, P.TintA, P.TintB, 0)) S->SetMaterial(0, M);
			Rig.Pieces.Add(S);
			continue;
		}
		USkeletalMesh* GM = V2Load<USkeletalMesh>(TEXT("Garments"), P.Mesh);
		if (!GM) continue;
		USkeletalMeshComponent* G = Skel(GM, Body);
		G->SetLeaderPoseComponent(Body);
		for (int32 i = 0; i < GM->GetMaterials().Num(); ++i)
			if (UMaterialInstanceDynamic* M = V2Mat(Owner, false, P.Tex, P.Mask, P.TintA, P.TintB, Hide)) G->SetMaterial(i, M);
		Rig.Pieces.Add(G);
	}
	return Body;
}

void FHLCharV2::Play(FHLV2Rig& Rig, const FString& Clip, bool bLoop, float Rate)
{
	USkeletalMeshComponent* B = Rig.Body.Get();
	UAnimSequence* S = B ? Anim(Rig.Sex, Clip) : nullptr;
	if (!S) return;
	B->PlayAnimation(S, bLoop);
	B->SetPlayRate(Rate);
	Rig.Clip = Clip;
}
