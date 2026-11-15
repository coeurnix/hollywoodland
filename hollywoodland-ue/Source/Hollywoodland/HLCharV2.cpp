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
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static const TCHAR* V2Base = TEXT("/Game/CharactersV2");
static const TCHAR* V2Sex[2] = { TEXT("Male"), TEXT("Female") };

template <typename T>
static T* V2Load(const FString& Folder, const FString& Name)
{
	if (Name.IsEmpty()) return nullptr;
	return LoadObject<T>(nullptr, *FString::Printf(TEXT("%s/%s/%s.%s"), V2Base, *Folder, *Name, *Name), nullptr, LOAD_NoWarn);
}

static UMaterialInstanceDynamic* V2Mat(UObject* Outer, bool bMasked, const FString& Tex, const FString& Mask, const FLinearColor& A, const FLinearColor& B, uint32 Hide,
	const FLinearColor& Mul = FLinearColor::White)
{
	if (FParse::Param(FCommandLine::Get(), TEXT("HLV2NoMat"))) return nullptr;
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, bMasked ? TEXT("/Game/CharactersV2/M_HLCharV2Masked.M_HLCharV2Masked") : TEXT("/Game/CharactersV2/M_HLCharV2.M_HLCharV2"), nullptr, LOAD_NoWarn);
	if (!Base) { UE_LOG(LogTemp, Warning, TEXT("HL_V2 base material missing")); return nullptr; }
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Base, Outer);
	if (UTexture2D* T = Tex.IsEmpty() ? nullptr : V2Load<UTexture2D>(TEXT("Tex"), Tex.StartsWith(TEXT("T_")) ? Tex : TEXT("T_") + Tex)) M->SetTextureParameterValue(TEXT("Tex"), T);
	else if (!Tex.IsEmpty()) UE_LOG(LogTemp, Warning, TEXT("HL_V2 missing texture T_%s"), *Tex);
	if (UTexture2D* T = Mask.IsEmpty() ? nullptr : V2Load<UTexture2D>(TEXT("Tex"), Mask.StartsWith(TEXT("T_")) ? Mask : TEXT("T_") + Mask)) M->SetTextureParameterValue(TEXT("Mask"), T);
	M->SetVectorParameterValue(TEXT("TintA"), A);
	M->SetVectorParameterValue(TEXT("TintB"), B);
	M->SetScalarParameterValue(TEXT("HideMask"), (float)Hide);
	M->SetVectorParameterValue(TEXT("Mul"), Mul);
	return M;
}

UAnimSequence* FHLCharV2::Anim(int32 Sex, const FString& Clip)
{
	return V2Load<UAnimSequence>(TEXT("Anim"), FString::Printf(TEXT("A_%s_%s"), V2Sex[Sex & 1], *Clip));
}

USkeletalMeshComponent* FHLCharV2::Build(AActor* Owner, USceneComponent* Parent, int32 Sex, const TArray<FHLV2Piece>& InPieces, FHLV2Rig& Rig,
	const FLinearColor& SkinMul)
{
	// only pieces whose mesh exists count (a missing garment must not hide the body zones it would have covered)
	TArray<FHLV2Piece> Pieces;
	for (const FHLV2Piece& P : InPieces)
		if (P.Socket.IsNone() ? V2Load<USkeletalMesh>(TEXT("Garments"), P.Mesh) != nullptr : V2Load<UStaticMesh>(TEXT("Hats"), P.Mesh) != nullptr) Pieces.Add(P);
		else UE_LOG(LogTemp, Warning, TEXT("HL_V2 missing mesh %s"), *P.Mesh);
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
		FString Tex = Prefix + TEXT("_Body_D");
		bool bMasked = false;
		if (N.Contains(TEXT("eyebrow"))) { Tex = Prefix + TEXT("_Body_") + N.RightChop(N.Find(TEXT("eyebrow"))) + TEXT("_D"); bMasked = true; }
		else if (N.Contains(TEXT("eyelash"))) { Tex = Prefix + TEXT("_Body_") + N.RightChop(N.Find(TEXT("eyelash"))) + TEXT("_D"); bMasked = true; }
		else if (N.Contains(TEXT("high-poly")) || N.Contains(TEXT("low-poly")) || N.Contains(TEXT("eye")))
		{
			Tex = Prefix + TEXT("_Body_high-poly_D");
			bMasked = true; // the cornea shell maps to a transparent patch: clipped, or it hides the iris in white
		}
		Tex = Tex.Replace(TEXT("."), TEXT("_"));
		const bool bSkin = !bMasked && !N.Contains(TEXT("high-poly")) && !N.Contains(TEXT("low-poly"));
		if (UMaterialInstanceDynamic* M = V2Mat(Owner, bMasked, Tex, FString(), FLinearColor::White, FLinearColor::White, bMasked ? 0u : BodyHide, bSkin ? SkinMul : FLinearColor::White))
			Body->SetMaterial(i, M);
	}
	for (int32 k = 0; k < Pieces.Num(); ++k)
	{
		const FHLV2Piece& P = Pieces[k];
		uint32 Hide = 0;
		// (trousers do not cut away the tail of a shirt: their front waist sits lower than the hips zone reaches)
		for (const FHLV2Piece& Q : Pieces) if (Q.Layer > P.Layer) Hide |= Q.Layer == 2 ? (Q.Covers & ~(uint32)HLZone::Hips) : Q.Covers;
		if (!P.Socket.IsNone())
		{
			UStaticMesh* SM = V2Load<UStaticMesh>(TEXT("Hats"), P.Mesh);
			if (!SM) continue;
			UStaticMeshComponent* S = NewObject<UStaticMeshComponent>(Owner);
			S->SetStaticMesh(SM);
			S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			// the hat was exported in body space: undo the head bone's bind transform so it sits where it was fitted
			FTransform HeadRef = FTransform::Identity;
			{
				const FReferenceSkeleton& RS = BodyMesh->GetRefSkeleton();
				for (int32 b = RS.FindBoneIndex(P.Socket); b != INDEX_NONE; b = RS.GetParentIndex(b)) HeadRef = HeadRef * RS.GetRefBonePose()[b];
			}
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
			if (UMaterialInstanceDynamic* M = V2Mat(Owner, P.bMasked, P.Tex, P.Mask, P.TintA, P.TintB, Hide, P.Mul)) G->SetMaterial(i, M);
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

FHLCharsV2& FHLCharsV2::Get() { static FHLCharsV2 Inst; return Inst; }

bool FHLCharsV2::Ready()
{
	if (bLoaded) return bOk;
	bLoaded = true;
	if (FParse::Param(FCommandLine::Get(), TEXT("HLV1Chars"))) return false;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/characters_v2.json")))) return false;
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return false;
	if (!V2Load<USkeletalMesh>(TEXT("Body"), TEXT("SKV2_Male_Body")) || !V2Load<USkeletalMesh>(TEXT("Body"), TEXT("SKV2_Female_Body"))) return false;
	const TSharedPtr<FJsonObject>* Looks;
	if (Root->TryGetObjectField(TEXT("looks"), Looks))
		for (const auto& L : (*Looks)->Values)
			for (const auto& Sx : L.Value->AsObject()->Values)
				for (const auto& Sl : Sx.Value->AsObject()->Values)
				{
					const TSharedPtr<FJsonObject> O = Sl.Value->AsObject();
					FHLV2Piece P;
					P.Mesh = O->GetStringField(TEXT("mesh")); P.Tex = O->GetStringField(TEXT("tex"));
					O->TryGetStringField(TEXT("mask"), P.Mask);
					P.Layer = (int32)O->GetNumberField(TEXT("layer")); P.Covers = (uint32)O->GetNumberField(TEXT("covers"));
					Pieces.Add(FString::Printf(TEXT("%s|%s|%s"), *L.Key, *Sx.Key, *Sl.Key), P);
				}
	const TSharedPtr<FJsonObject>* HatsO;
	if (Root->TryGetObjectField(TEXT("hats"), HatsO))
		for (const auto& L : (*HatsO)->Values)
			for (const auto& Sx : L.Value->AsObject()->Values)
			{
				const TSharedPtr<FJsonObject> O = Sx.Value->AsObject();
				FHLV2Piece P;
				P.Mesh = O->GetStringField(TEXT("mesh")); P.Tex = O->GetStringField(TEXT("tex"));
				P.Socket = TEXT("head"); P.Layer = 9; P.bHidesHair = O->GetBoolField(TEXT("hides"));
				Pieces.Add(FString::Printf(TEXT("%s|%s|hat"), *L.Key, *Sx.Key), P);
			}
	const TSharedPtr<FJsonObject>* HairO;
	if (Root->TryGetObjectField(TEXT("hair"), HairO))
		for (int32 Sex = 0; Sex < 2; ++Sex)
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr;
			if (!(*HairO)->TryGetArrayField(V2Sex[Sex], Arr)) continue;
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FHLV2Piece P;
				P.Mesh = V->AsObject()->GetStringField(TEXT("mesh")); P.Tex = V->AsObject()->GetStringField(TEXT("tex"));
				P.Layer = 8; P.bMasked = true;
				Hair[Sex].Add(P);
			}
		}
	const TArray<TSharedPtr<FJsonValue>>* SkinA;
	if (Root->TryGetArrayField(TEXT("skin"), SkinA))
		for (const TSharedPtr<FJsonValue>& V : *SkinA)
		{
			const TArray<TSharedPtr<FJsonValue>>& C = V->AsArray();
			if (C.Num() >= 3) Skin.Add(FLinearColor(FColor((uint8)(C[0]->AsNumber() * 255), (uint8)(C[1]->AsNumber() * 255), (uint8)(C[2]->AsNumber() * 255))));
		}
	bOk = Pieces.Num() > 0;
	UE_LOG(LogTemp, Display, TEXT("HL_V2 table: %d pieces, hair %d/%d, %d skins"), Pieces.Num(), Hair[0].Num(), Hair[1].Num(), Skin.Num());
	return bOk;
}

const FHLV2Piece* FHLCharsV2::Piece(const FString& Look, int32 Sex, const FString& Slot) const
{
	return Pieces.Find(FString::Printf(TEXT("%s|%s|%s"), *Look, V2Sex[Sex & 1], *Slot));
}
