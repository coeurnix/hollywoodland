#include "HLEnemies.h"
#include "HLGame.h"
#include "HLBridge.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

FHLEnemyArt& FHLEnemyArt::Get()
{
	static FHLEnemyArt A;
	return A;
}

bool FHLEnemyArt::Load()
{
	if (bTried) return bReady;
	bTried = true;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/enemies.json")))) { HLBridge::Log(TEXT("HL enemies.json missing: gray-box enemies")); return false; }
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return false;
	auto StrMap = [](const TSharedPtr<FJsonObject>& O, const TCHAR* Field, TMap<FString, FString>& Out)
	{
		const TSharedPtr<FJsonObject>* M;
		if (O->TryGetObjectField(Field, M)) for (const auto& KV : (*M)->Values) Out.Add(FString(KV.Key), KV.Value->AsString());
	};
	const TSharedPtr<FJsonObject>* Ms;
	if (Root->TryGetObjectField(TEXT("models"), Ms))
		for (const auto& KV : (*Ms)->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
			FHLEnemyModel& M = Models.Add(FString(KV.Key));
			M.Id = FString(KV.Key);
			O->TryGetStringField(TEXT("mesh"), M.Mesh); O->TryGetStringField(TEXT("kind"), M.Kind);
			StrMap(O, TEXT("roles"), M.Roles);
			const TSharedPtr<FJsonObject>* Cs;
			if (O->TryGetObjectField(TEXT("clips"), Cs))
				for (const auto& C : (*Cs)->Values)
				{
					const TSharedPtr<FJsonObject> CO = C.Value->AsObject();
					FHLEnemyClip& Cl = M.Clips.Add(FString(C.Key));
					CO->TryGetStringField(TEXT("anim"), Cl.Anim); CO->TryGetNumberField(TEXT("len"), Cl.Len);
					CO->TryGetNumberField(TEXT("ev"), Cl.Ev); CO->TryGetBoolField(TEXT("loop"), Cl.bLoop);
				}
		}
	const TSharedPtr<FJsonObject>* Mb;
	if (Root->TryGetObjectField(TEXT("mobs"), Mb))
		for (const auto& KV : (*Mb)->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
			FHLEnemyMob& M = Mobs.Add(FString(KV.Key));
			O->TryGetStringField(TEXT("model"), M.Model); O->TryGetStringField(TEXT("mat"), M.Mat);
			StrMap(O, TEXT("specials"), M.Specials);
		}
	const TSharedPtr<FJsonObject>* St;
	if (Root->TryGetObjectField(TEXT("structures"), St))
		for (const auto& KV : (*St)->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
			FHLEnemyStructure& S = Structures.Add(FString(KV.Key));
			O->TryGetStringField(TEXT("destroyed"), S.Destroyed); O->TryGetStringField(TEXT("mat"), S.Mat); O->TryGetStringField(TEXT("live"), S.Live);
		}
	bReady = Models.Num() > 0;
	HLBridge::Log(FString::Printf(TEXT("HL enemies: %d models, %d mobs, %d structures"), Models.Num(), Mobs.Num(), Structures.Num()));
	return bReady;
}

UObject* FHLEnemyArt::LoadPath(const FString& Path, UClass* Class)
{
	if (Path.IsEmpty()) return nullptr;
	if (const TObjectPtr<UObject>* Hit = Cache.Find(Path)) return Hit->Get();
	UObject* O = StaticLoadObject(Class, nullptr, *Path, nullptr, LOAD_NoWarn);
	if (!O) HLBridge::Log(FString::Printf(TEXT("HL enemy asset missing: %s"), *Path));
	Cache.Add(Path, O);
	return O;
}

USkeletalMesh* FHLEnemyArt::Mesh(const FHLEnemyModel& M) { return Cast<USkeletalMesh>(LoadPath(M.Mesh, USkeletalMesh::StaticClass())); }
UMaterialInterface* FHLEnemyArt::Material(const FString& Path) { return Cast<UMaterialInterface>(LoadPath(Path, UMaterialInterface::StaticClass())); }
UStaticMesh* FHLEnemyArt::StaticMesh(const FString& Path) { return Cast<UStaticMesh>(LoadPath(Path, UStaticMesh::StaticClass())); }
UAnimSequence* FHLEnemyArt::Anim(const FHLEnemyModel& M, const FString& Clip)
{
	const FHLEnemyClip* C = M.Clips.Find(Clip);
	return C ? Cast<UAnimSequence>(LoadPath(C->Anim, UAnimSequence::StaticClass())) : nullptr;
}

void FHLEnemyArt::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& KV : Cache) Collector.AddReferencedObject(KV.Value);
}

USkeletalMeshComponent* FHLEnemyArt::Make(AActor* Owner, USceneComponent* Parent, FHLEnemyRig& Rig, const FString& MobId)
{
	if (!Load()) return nullptr;
	const FHLEnemyMob* Mb = Mobs.Find(MobId);
	const FHLEnemyModel* Md = Mb ? Models.Find(Mb->Model) : nullptr;
	USkeletalMesh* SM = Md ? Mesh(*Md) : nullptr;
	if (!SM) return nullptr;
	USkeletalMeshComponent* C = NewObject<USkeletalMeshComponent>(Owner);
	C->SetMobility(EComponentMobility::Movable);
	C->SetSkeletalMesh(SM);
	if (UMaterialInterface* Mat = Material(Mb->Mat)) C->SetMaterial(0, Mat);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	// units off screen are hidden actors: no pose work for them
	C->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	C->SetupAttachment(Parent);
	C->RegisterComponent();
	Rig.Comp = C; Rig.Model = Md; Rig.Mob = Mb;
	return C;
}

void FHLEnemyArt::Play(FHLEnemyRig& Rig, const FString& Clip, bool bLoop, float Rate)
{
	USkeletalMeshComponent* C = Rig.Comp.Get();
	if (!C || !Rig.Model || Clip.IsEmpty()) return;
	if (Rig.Base == Clip && Rig.Shot.IsEmpty()) { if (!FMath::IsNearlyEqual(C->GetPlayRate(), Rate, 0.05f)) C->SetPlayRate(Rate); return; }
	UAnimSequence* Seq = Anim(*Rig.Model, Clip);
	if (!Seq) return;
	C->PlayAnimation(Seq, bLoop);
	C->SetPlayRate(Rate);
	C->TickAnimation(0.f, false); C->RefreshBoneTransforms();
	Rig.Base = Clip; Rig.Shot.Reset();
}

void FHLEnemyArt::PlayOnce(FHLEnemyRig& Rig, const FString& Clip, double Now, float Rate)
{
	USkeletalMeshComponent* C = Rig.Comp.Get();
	UAnimSequence* Seq = C && Rig.Model && !Clip.IsEmpty() ? Anim(*Rig.Model, Clip) : nullptr;
	if (!Seq) return;
	const FHLEnemyClip& Cl = Rig.Model->Clips[Clip];
	// a looping special (the Warden's head-down charge) runs for about a second
	C->PlayAnimation(Seq, Cl.bLoop);
	C->SetPlayRate(Rate);
	C->TickAnimation(0.f, false); C->RefreshBoneTransforms();
	Rig.Shot = Clip; Rig.Base.Reset();
	Rig.ShotUntil = Now + (Cl.bLoop ? FMath::Max(1.f, Cl.Len) : Seq->GetPlayLength() / FMath::Max(0.1f, Rate)) - 0.05;
}

void FHLEnemyArt::Drive(FHLEnemyRig& Rig, const FHLUnit& U, double Now, float Dt)
{
	if (!Rig.Comp.IsValid() || !Rig.Model) return;
	const FHLEnemyModel& M = *Rig.Model;
	// ground speed from the simulated position (metres per second); teleports read as standing
	if (!Rig.bHasLast)
	{
		Rig.bHasLast = true; Rig.LastPos = U.Pos; Rig.LastHp = U.Hp; Rig.LastSwing = U.SwingTimer; Rig.bWasBurrowed = U.bBurrowed;
		// only a hatch is worth replaying when the unit first comes into view
		Rig.CueSeq = U.AnimCue == TEXT("hatch") ? U.AnimCueSeq - 1 : U.AnimCueSeq;
		Rig.bWasDead = U.bDead;
		if (U.bDead) { Play(Rig, M.Role(TEXT("death")), false); if (USkeletalMeshComponent* C = Rig.Comp.Get()) { C->SetPosition(M.Clips.FindRef(M.Role(TEXT("death"))).Len, false); C->RefreshBoneTransforms(); } return; }
	}
	const FVector2D Delta = U.Pos - Rig.LastPos;
	Rig.LastPos = U.Pos;
	const float Inst = Dt > 0 ? (float)Delta.Size() / Dt : 0.f;
	Rig.Speed = Inst > 40 ? 0.f : FMath::FInterpTo(Rig.Speed, Inst, Dt, 10.f);
	// death: the clip ends on the corpse pose and holds it
	if (U.bDead)
	{
		if (!Rig.bWasDead) { Rig.bWasDead = true; Rig.Shot.Reset(); Rig.Base.Reset(); Play(Rig, M.Role(TEXT("death")), false); }
		return;
	}
	if (Rig.bWasDead) { Rig.bWasDead = false; Rig.Base.Reset(); Rig.LastHp = U.Hp; }
	// burrowing: under the street, a mound breathing; Burrow going down, Emerge coming up
	if (U.bBurrowed != Rig.bWasBurrowed)
	{
		Rig.bWasBurrowed = U.bBurrowed;
		PlayOnce(Rig, U.bBurrowed ? TEXT("Burrow") : TEXT("Emerge"), Now);
	}
	if (U.bBurrowed && M.Has(TEXT("BurrowedIdle")))
	{
		Rig.LastHp = U.Hp; Rig.LastSwing = U.SwingTimer; Rig.CueSeq = U.AnimCueSeq;
		if (!Rig.Shot.IsEmpty() && Now < Rig.ShotUntil) return;
		Rig.Shot.Reset();
		Play(Rig, TEXT("BurrowedIdle"), true);
		return;
	}
	bool bSpecial = !Rig.Shot.IsEmpty() && Now < Rig.ShotUntil && Rig.Shot != M.Role(TEXT("attack")) && Rig.Shot != M.Role(TEXT("hit"));
	// specials: abilities and script moves name a clip through the mob's specials, or directly
	if (U.AnimCueSeq != Rig.CueSeq)
	{
		Rig.CueSeq = U.AnimCueSeq;
		const FString* S = Rig.Mob ? Rig.Mob->Specials.Find(U.AnimCue) : nullptr;
		const FString Clip = S ? *S : (M.Has(U.AnimCue) ? U.AnimCue : FString());
		if (!Clip.IsEmpty()) { PlayOnce(Rig, Clip, Now); bSpecial = true; }
	}
	// each swing (the swing timer jumps back up when one lands)
	if (U.SwingTimer > Rig.LastSwing + 0.25f && U.bAutoAttack && !bSpecial)
	{
		const FString Clip = U.bRangedAuto && M.Has(TEXT("Spit")) ? FString(TEXT("Spit")) : M.Role(TEXT("attack"));
		PlayOnce(Rig, Clip, Now, 1.1f);
	}
	Rig.LastSwing = U.SwingTimer;
	// a flinch on a real hit, unless it's mid-swing or mid-special
	if (U.Hp < Rig.LastHp - FMath::Max(1.f, U.MaxHp * 0.02f) && Rig.Shot.IsEmpty()) PlayOnce(Rig, M.Role(TEXT("hit")), Now);
	Rig.LastHp = U.Hp;
	const bool bMoving = Rig.Speed > 0.6f;
	if (!Rig.Shot.IsEmpty())
	{
		// specials play out; a swing or a flinch gives way to running
		if (Now < Rig.ShotUntil && (bSpecial || !bMoving)) return;
		Rig.Shot.Reset();
	}
	// base loop, the rate matched to the ground speed (the clips are authored at scale 1)
	const float S = FMath::Max(0.3f, U.Scale);
	if (!bMoving) { Play(Rig, M.Role(TEXT("idle")), true); return; }
	if (Rig.Speed < 3.6f * S) Play(Rig, M.Role(TEXT("walk")), true, FMath::Clamp(Rig.Speed / (1.8f * S), 0.6f, 1.8f));
	else Play(Rig, M.Role(TEXT("run")), true, FMath::Clamp(Rig.Speed / (6.f * S), 0.7f, 1.8f));
}
