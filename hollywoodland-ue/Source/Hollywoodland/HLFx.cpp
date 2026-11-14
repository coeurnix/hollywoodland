// Visual effects: the game's FX events (FHLGame::Fx) become short-lived glows drawn with engine basic shapes and one
// additive material (M_HLAdd): tracers and muzzle flashes for guns, glowing bolts for spells, arcing globs for acid and
// thrown bottles, bursts on impact, rings for area effects, columns for heals and levelling. Lasting states (casting,
// shields, stuns, slows, heals and poisons over time) are drawn from the units every frame. Components come from one
// pool that is re-dealt every frame, so nothing is created during play once the pool has grown.
#include "HLWorldActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HLSettings.h"

static const float M = 100.f;

static FLinearColor SchoolColor(const FString& S)
{
	if (S == TEXT("fire")) return FLinearColor(1.f, 0.42f, 0.08f);
	if (S == TEXT("electric")) return FLinearColor(0.45f, 0.75f, 1.f);
	if (S == TEXT("acid")) return FLinearColor(0.5f, 1.f, 0.15f);
	if (S == TEXT("holy")) return FLinearColor(1.f, 0.9f, 0.5f);
	if (S == TEXT("heal")) return FLinearColor(0.35f, 1.f, 0.45f);
	return FLinearColor(1.f, 0.85f, 0.55f); // physical: sparks and muzzle fire
}

FVector AHLWorldActor::FxUnitPos(int32 UnitId, float HeightFrac, bool* bOk) const
{
	const FHLUnit* U = Game.Unit(UnitId);
	if (bOk) *bOk = U != nullptr;
	if (!U) return FVector::ZeroVector;
	const float H = (U->Kind == EHLKind::Mob && U->Shape != TEXT("human") ? 1.2f : 1.75f) * U->Scale;
	return ToWorld(U->Area, U->Pos, U->Z + H * HeightFrac);
}

void AHLWorldActor::FxPiece(UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, const FVector& SizeM, const FLinearColor& C, float Opacity, float Rim)
{
	if (!MatAdd || !Mesh || Opacity <= 0.01f || FxUsed >= 240) return;
	if (!FxActor)
	{
		FxActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
		USceneComponent* Root = NewObject<USceneComponent>(FxActor);
		FxActor->SetRootComponent(Root); Root->RegisterComponent();
	}
	if (FxUsed >= FxPool.Num())
	{
		UStaticMeshComponent* NC = NewObject<UStaticMeshComponent>(FxActor);
		NC->SetMobility(EComponentMobility::Movable);
		NC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		NC->SetCastShadow(false);
		NC->ComponentTags.Add(TEXT("NoShadow"));
		NC->SetUsingAbsoluteLocation(true); NC->SetUsingAbsoluteRotation(true); NC->SetUsingAbsoluteScale(true);
		NC->SetupAttachment(FxActor->GetRootComponent());
		NC->RegisterComponent();
		NC->SetMaterial(0, UMaterialInstanceDynamic::Create(MatAdd, this));
		FxPool.Add(NC);
	}
	UStaticMeshComponent* Comp = FxPool[FxUsed++];
	if (Comp->GetStaticMesh() != Mesh) Comp->SetStaticMesh(Mesh);
	Comp->SetWorldLocationAndRotation(Loc, Rot);
	Comp->SetWorldScale3D(SizeM); // basic shapes are 1 m across
	if (UMaterialInstanceDynamic* D = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0)))
	{
		D->SetVectorParameterValue(TEXT("Color"), C);
		D->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		D->SetScalarParameterValue(TEXT("Rim"), Rim);
	}
	if (!Comp->IsVisible()) Comp->SetVisibility(true);
}

void AHLWorldActor::TickFx(float Dt)
{
	FxUsed = 0;
	const FHLUnit* P = Game.PlayerUnit();
	const double T = Game.Now;
	// 1. new events near the player become live effects
	for (const FHLFxEvent& E : Game.FxQueue)
	{
		if (!P || E.Area != P->Area || FVector2D::Distance(E.Pos, P->Pos) > 110.f) continue;
		FHLFxLive L;
		L.Kind = E.Kind; L.SrcId = E.SrcId; L.DstId = E.DstId; L.Area = E.Area; L.Radius = E.Radius;
		L.Color = SchoolColor(E.School);
		bool bSrc = false, bDst = false;
		const FHLUnit* S = Game.Unit(E.SrcId);
		const float Yaw = S ? FMath::DegreesToRadians(S->Yaw) : 0.f;
		const FVector Fwd(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f);
		L.From = FxUnitPos(E.SrcId, 0.62f, &bSrc) + Fwd * 45.f;
		L.To = E.DstId ? FxUnitPos(E.DstId, 0.55f, &bDst) : ToWorld(E.Area, E.Pos, 0.1f);
		const float Dist = FVector::Dist(L.From, L.To);
		switch (E.Kind)
		{
		case EHLFx::Shot: L.Life = FMath::Max(0.05f, Dist / 9000.f); L.Color = FLinearColor(1.f, 0.82f, 0.45f); break;
		case EHLFx::Bolt: L.Life = FMath::Max(0.12f, Dist / 2600.f); break;
		case EHLFx::Spit: case EHLFx::Thrown: L.Life = FMath::Max(0.15f, Dist / 1700.f); if (E.Kind == EHLFx::Thrown) L.Color = FLinearColor(1.f, 0.55f, 0.15f); break;
		case EHLFx::Hit: L.Life = 0.22f; L.From = L.To; break;
		case EHLFx::Crit: L.Life = 0.34f; L.From = L.To; break;
		case EHLFx::Nova: L.Life = 0.55f; L.From = L.To = ToWorld(E.Area, E.Pos, 0.08f); break;
		case EHLFx::Heal: L.Life = 0.95f; L.To = FxUnitPos(E.DstId, 0.f); break;
		case EHLFx::Buff: L.Life = 0.75f; L.To = FxUnitPos(E.DstId, 0.f); break;
		case EHLFx::Shield: L.Life = 0.5f; L.Color = FLinearColor(0.6f, 0.8f, 1.f); break;
		case EHLFx::LevelUp: L.Life = 2.2f; L.To = FxUnitPos(E.DstId, 0.f); L.Color = FLinearColor(1.f, 0.82f, 0.35f); break;
		}
		FxLive.Add(L);
	}
	Game.FxQueue.Reset();

	// 2. live effects
	const FRotator Flat = FRotator::ZeroRotator;
	TArray<FHLFxLive> Born;
	for (int32 i = FxLive.Num() - 1; i >= 0; --i)
	{
		FHLFxLive& L = FxLive[i];
		L.Age += Dt;
		const bool bProjectile = L.Kind == EHLFx::Shot || L.Kind == EHLFx::Bolt || L.Kind == EHLFx::Spit || L.Kind == EHLFx::Thrown;
		if (bProjectile && L.DstId) { bool bOk = false; const FVector Now = FxUnitPos(L.DstId, 0.55f, &bOk); if (bOk) L.To = Now; } // homes on a moving target
		if (L.Kind == EHLFx::Heal || L.Kind == EHLFx::Buff || L.Kind == EHLFx::LevelUp) { bool bOk = false; const FVector Now = FxUnitPos(L.DstId, 0.f, &bOk); if (bOk) L.To = Now; }
		if (L.Kind == EHLFx::Shield && L.DstId) { bool bOk = false; const FVector Now = FxUnitPos(L.DstId, 0.5f, &bOk); if (bOk) L.To = Now; }
		const float A = FMath::Clamp(L.Age / L.Life, 0.f, 1.f);
		if (L.Age >= L.Life)
		{
			if (bProjectile)
			{
				FHLFxLive H; H.Kind = L.Kind == EHLFx::Shot ? EHLFx::Hit : EHLFx::Crit; H.Life = L.Kind == EHLFx::Shot ? 0.16f : 0.3f;
				H.From = H.To = L.To; H.Color = L.Color; H.DstId = L.DstId; H.Area = L.Area;
				Born.Add(H);
			}
			FxLive.RemoveAtSwap(i);
			continue;
		}
		const FVector Dir = (L.To - L.From).GetSafeNormal();
		const FRotator Along = Dir.IsNearlyZero() ? Flat : FRotationMatrix::MakeFromZ(Dir).Rotator();
		switch (L.Kind)
		{
		case EHLFx::Shot:
		{
			const FVector Pos = FMath::Lerp(L.From, L.To, A);
			FxPiece(Cylinder, Pos, Along, FVector(0.05f, 0.05f, 1.4f), L.Color, 1.6f, 0.f);
			if (L.Age < 0.07f) FxPiece(Sphere, L.From, Flat, FVector(0.38f), FLinearColor(1.f, 0.7f, 0.3f), 1.8f, 0.f); // muzzle flash
			break;
		}
		case EHLFx::Bolt:
		{
			const FVector Pos = FMath::Lerp(L.From, L.To, A);
			const float Pulse = 1.f + 0.15f * FMath::Sin(L.Age * 40.f);
			FxPiece(Sphere, Pos, Flat, FVector(0.36f * Pulse), FLinearColor(1, 1, 1) * 0.5f + L.Color * 0.6f, 1.6f, 0.f);
			FxPiece(Sphere, Pos, Flat, FVector(0.85f * Pulse), L.Color, 0.45f, 0.f);
			for (int32 k = 1; k <= 3; ++k) FxPiece(Sphere, Pos - Dir * (35.f * k), Flat, FVector(0.42f - 0.09f * k), L.Color, 0.7f - 0.17f * k, 0.f);
			break;
		}
		case EHLFx::Spit: case EHLFx::Thrown:
		{
			const float Arc = FVector::Dist(L.From, L.To) * 0.16f;
			auto At = [&](float a) { return FMath::Lerp(L.From, L.To, a) + FVector(0, 0, FMath::Sin(PI * a) * Arc); };
			const FVector Pos = At(A);
			FxPiece(Sphere, Pos, Flat, FVector(L.Kind == EHLFx::Spit ? 0.34f : 0.26f), L.Color, 1.3f, 0.f);
			for (int32 k = 1; k <= 3; ++k) FxPiece(Sphere, At(FMath::Max(0.f, A - 0.06f * k)), Flat, FVector(0.26f - 0.05f * k), L.Color, 0.6f - 0.15f * k, 0.f);
			break;
		}
		case EHLFx::Hit: case EHLFx::Crit:
		{
			const float Big = L.Kind == EHLFx::Crit ? 1.5f : 0.85f;
			const float F = (1.f - A) * (1.f - A);
			FxPiece(Sphere, L.To, Flat, FVector(FMath::Lerp(0.15f, Big, FMath::Sqrt(A))), L.Color, 1.4f * F, 0.25f);
			FxPiece(Sphere, L.To, Flat, FVector(FMath::Lerp(0.1f, Big * 0.45f, A)), FLinearColor(1, 1, 1), 1.2f * F, 0.f);
			break;
		}
		case EHLFx::Nova:
		{
			const float R = L.Radius * 2.f * FMath::Sqrt(A);
			FxPiece(Cylinder, L.To + FVector(0, 0, 25), Flat, FVector(R, R, 0.5f), L.Color, 1.4f * (1.f - A), 1.f);
			FxPiece(Cylinder, L.To, Flat, FVector(R * 0.96f, R * 0.96f, 0.04f), L.Color, 0.35f * (1.f - A), 0.f);
			break;
		}
		case EHLFx::Heal:
		{
			const float F = FMath::Sin(PI * A);
			FxPiece(Cylinder, L.To + FVector(0, 0, 110 + 40 * A), Flat, FVector(1.3f, 1.3f, 2.4f), L.Color, 0.9f * F, 1.f);
			for (int32 k = 0; k < 5; ++k)
			{
				const float Ang = L.Age * 5.f + k * (2 * PI / 5);
				FxPiece(Sphere, L.To + FVector(FMath::Cos(Ang) * 55, FMath::Sin(Ang) * 55, 30 + 190 * FMath::Frac(A + k * 0.2f)), Flat, FVector(0.13f), L.Color, 1.2f * F, 0.f);
			}
			break;
		}
		case EHLFx::Buff:
			FxPiece(Cylinder, L.To + FVector(0, 0, 10 + 190 * A), Flat, FVector(1.5f, 1.5f, 0.15f), L.Color, 1.2f * (1.f - A), 1.f);
			break;
		case EHLFx::Shield:
			FxPiece(Sphere, L.To, Flat, FVector(FMath::Lerp(1.4f, 2.3f, A)), L.Color, 1.f * (1.f - A), 1.f);
			break;
		case EHLFx::LevelUp:
		{
			const float F = A < 0.15f ? A / 0.15f : 1.f - (A - 0.15f) / 0.85f;
			FxPiece(Cylinder, L.To + FVector(0, 0, 400), Flat, FVector(2.f, 2.f, 8.f), L.Color, 1.1f * F, 1.f);
			const float R = 1.f + 9.f * FMath::Sqrt(A);
			FxPiece(Cylinder, L.To + FVector(0, 0, 15), Flat, FVector(R, R, 0.3f), L.Color, 1.2f * (1.f - A), 1.f);
			for (int32 k = 0; k < 8; ++k)
			{
				const float Ang = L.Age * 2.5f + k * (2 * PI / 8);
				FxPiece(Sphere, L.To + FVector(FMath::Cos(Ang) * 90, FMath::Sin(Ang) * 90, 20 + 500 * FMath::Frac(A * 1.5f + k * 0.125f)), Flat, FVector(0.18f), L.Color, 1.5f * F, 0.f);
			}
			break;
		}
		}
	}
	FxLive.Append(Born);

	// 3. lasting states on the units in view (the effects setting turns these off; projectiles and bursts stay)
	if (P && FHLSettings::Get().bEffects)
	{
		for (const FHLUnit* U : Game.UnitList)
		{
			if (U->bDead || U->Area != P->Area || FVector2D::DistSquared(U->Pos, P->Pos) > 70.f * 70.f) continue;
			const FHLUnitVis* V = Vis.Find(U->Id);
			if (!V || !V->Actor.IsValid() || V->Actor->IsHidden()) continue;
			const FVector Feet = FxUnitPos(U->Id, 0.f);
			const float Hgt = FxUnitPos(U->Id, 1.f).Z - Feet.Z;
			const float Ph = float(FMath::Fmod(T + U->Id * 0.37, 1000.0));
			if (U->Casting)
			{
				const FLinearColor C = SchoolColor(U->Casting->IsHeal() ? FString(TEXT("heal")) : U->Casting->School);
				const float Prog = FMath::Clamp(float((T - U->CastStart) / FMath::Max(0.1, U->CastEnd - U->CastStart)), 0.f, 1.f);
				const float Yaw = FMath::DegreesToRadians(U->Yaw);
				const FVector Hands = Feet + FVector(FMath::Cos(Yaw) * 40, FMath::Sin(Yaw) * 40, Hgt * 0.58f);
				const float Pulse = 1.f + 0.12f * FMath::Sin(Ph * 18.f);
				FxPiece(Sphere, Hands, FRotator::ZeroRotator, FVector((0.16f + 0.22f * Prog) * Pulse), C, 1.5f, 0.f);
				FxPiece(Sphere, Hands, FRotator::ZeroRotator, FVector((0.4f + 0.4f * Prog) * Pulse), C, 0.35f, 0.f);
				FxPiece(Cylinder, Feet + FVector(0, 0, 4), FRotator::ZeroRotator, FVector(1.7f, 1.7f, 0.06f), C, 0.3f + 0.2f * Prog, 1.f);
			}
			bool bShield = false, bStun = false, bSlow = false, bHot = false, bDot = false; FString DotSchool;
			for (const FHLAura& Au : U->Auras)
			{
				bShield |= Au.Type == EHLAura::Shield; bStun |= Au.Type == EHLAura::Stun; bSlow |= Au.Type == EHLAura::Slow;
				bHot |= Au.Type == EHLAura::Hot;
				if (Au.Type == EHLAura::Dot) { bDot = true; DotSchool = Au.Stat; }
			}
			if (bShield) FxPiece(Sphere, Feet + FVector(0, 0, Hgt * 0.5f), FRotator::ZeroRotator, FVector(FMath::Max(1.6f, Hgt / M * 1.25f)), FLinearColor(0.55f, 0.75f, 1.f), 0.45f + 0.1f * FMath::Sin(Ph * 4.f), 1.f);
			if (bStun)
				for (int32 k = 0; k < 3; ++k)
				{
					const float Ang = Ph * 5.f + k * (2 * PI / 3);
					FxPiece(Sphere, Feet + FVector(FMath::Cos(Ang) * 35, FMath::Sin(Ang) * 35, Hgt + 25), FRotator::ZeroRotator, FVector(0.13f), FLinearColor(1.f, 0.9f, 0.3f), 1.5f, 0.f);
				}
			if (bSlow) FxPiece(Cylinder, Feet + FVector(0, 0, 5), FRotator::ZeroRotator, FVector(1.4f, 1.4f, 0.1f), FLinearColor(0.4f, 0.6f, 1.f), 0.6f, 1.f);
			if (bHot || bDot)
			{
				const FLinearColor C = bHot ? SchoolColor(TEXT("heal")) : SchoolColor(DotSchool.IsEmpty() ? FString(TEXT("acid")) : DotSchool);
				for (int32 k = 0; k < 3; ++k)
				{
					const float F = FMath::Frac(Ph * 0.6f + k / 3.f);
					const float Ang = k * 2.1f + U->Id;
					FxPiece(Sphere, Feet + FVector(FMath::Cos(Ang) * 40, FMath::Sin(Ang) * 40, Hgt * (bHot ? F : 0.9f - 0.8f * F)), FRotator::ZeroRotator, FVector(0.11f), C, 1.1f * FMath::Sin(PI * F), 0.f);
				}
			}
		}
	}
	// 4. park what nobody used this frame
	for (int32 i = FxUsed; i < FxPool.Num(); ++i) if (FxPool[i] && FxPool[i]->IsVisible()) FxPool[i]->SetVisibility(false);
}
