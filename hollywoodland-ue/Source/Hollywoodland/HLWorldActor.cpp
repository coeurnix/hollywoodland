#include "HLWorldActor.h"
#include "HLBridge.h"
#include "HLUI.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/SWeakWidget.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Kismet/KismetSystemLibrary.h"

static const float M = 100.f; // metres -> UE units

AHLWorldActor::AHLWorldActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->SetUsingAbsoluteLocation(true); Camera->SetUsingAbsoluteRotation(true);
	Camera->FieldOfView = 80;
	// fixed exposure: flat gray-box colours should look the same on every renderer tier
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_AutoExposureMethod = true; PP.AutoExposureMethod = AEM_Manual;
	PP.bOverride_AutoExposureApplyPhysicalCameraExposure = true; PP.AutoExposureApplyPhysicalCameraExposure = false;
	// The web (mobile forward) tier shows scene colour about two stops brighter than the desktop tonemapper path.
#if PLATFORM_WASM
	PP.bOverride_AutoExposureBias = true; PP.AutoExposureBias = 0.f;
#else
	PP.bOverride_AutoExposureBias = true; PP.AutoExposureBias = 2.0f;
#endif
	PP.bOverride_BloomIntensity = true; PP.BloomIntensity = 0.f;
	PP.bOverride_VignetteIntensity = true; PP.VignetteIntensity = 0.25f;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereF(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylF(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeF(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LitF(TEXT("/Game/HL/M_HL.M_HL"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitF(TEXT("/Game/HL/M_HLSky.M_HLSky"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicF(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	Cube = CubeF.Object; Sphere = SphereF.Object; Cylinder = CylF.Object; Cone = ConeF.Object;
	MatLit = LitF.Succeeded() ? LitF.Object : BasicF.Object;
	MatUnlit = UnlitF.Succeeded() ? UnlitF.Object : BasicF.Object;
}

FVector AHLWorldActor::ToWorld(int32 Area, const FVector2D& P, float Z) const
{
	const FVector2D Off = Game.Areas.IsValidIndex(Area) ? Game.Areas[Area].Def->Offset : FVector2D::ZeroVector;
	return FVector((P.X + Off.X) * M, (P.Y + Off.Y) * M, Z * M);
}

UMaterialInstanceDynamic* AHLWorldActor::Mid(const FLinearColor& C, bool bUnlit, float Glow)
{
	const FColor Q = C.ToFColor(false);
	const uint32 Key = (uint32(Q.R >> 2) << 18) | (uint32(Q.G >> 2) << 12) | (uint32(Q.B >> 2) << 6) | (bUnlit ? 32u : 0u) | uint32(FMath::Clamp(Glow * 10.f, 0.f, 31.f));
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = Mids.Find(Key)) return *Found;
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(bUnlit ? MatUnlit : MatLit, this);
	D->SetVectorParameterValue(TEXT("Color"), bUnlit ? C * 0.6f : C * 0.78f);
	if (!bUnlit) D->SetScalarParameterValue(TEXT("Glow"), Glow);
	Mids.Add(Key, D);
	return D;
}

UStaticMeshComponent* AHLWorldActor::AddMesh(AActor* InOwner, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FLinearColor& Color, bool bUnlit, bool bShadow)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(InOwner);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(bShadow);
	C->SetMaterial(0, Mid(Color, bUnlit));
	if (InOwner->GetRootComponent()) C->SetupAttachment(InOwner->GetRootComponent()); else InOwner->SetRootComponent(C);
	C->RegisterComponent();
	C->SetRelativeLocation(Loc);
	C->SetRelativeScale3D(Scale);
	return C;
}

void AHLWorldActor::BeginPlay()
{
	Super::BeginPlay();
	if (!Game.Init(LoadError)) HLBridge::Log(FString::Printf(TEXT("HL data load failed: %s"), *LoadError));
	else { BuildWorld(); HLBridge::Log(FString::Printf(TEXT("HL world built: %d units, %d areas"), Game.UnitList.Num(), Game.Areas.Num())); }
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		PC->SetViewTarget(this);
		PC->bShowMouseCursor = true;
	}
	FParse::Value(FCommandLine::Get(), TEXT("HLAuto="), AutoClass);
	FParse::Value(FCommandLine::Get(), TEXT("HLAutoQuit="), AutoQuit);
	// browser test hook: localStorage hl_auto=<class> starts the self-playing test (tools/probe.py --auto)
	HLBridge::StorageGet(TEXT("hl_auto"), [this](const FHLBridgeResult& R) { if (R.Ok() && !R.Body.IsEmpty() && AutoClass.IsEmpty()) AutoClass = R.Body.TrimStartAndEnd(); });
	HLBridge::StorageGet(TEXT("hollywoodland_save_v1"), [this](const FHLBridgeResult& R) { bSaveChecked = true; if (R.Ok()) SavedJson = R.Body; });
	if (GEngine && GEngine->GameViewport)
	{
		UI = SNew(SHLGameUI).World(this);
		UIContainer = SNew(SWeakWidget).PossiblyNullContent(UI);
		GEngine->GameViewport->AddViewportWidgetContent(UIContainer.ToSharedRef(), 10);
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			FInputModeUIOnly Mode;
			Mode.SetWidgetToFocus(UI);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(Mode);
		}
	}
}

void AHLWorldActor::EndPlay(const EEndPlayReason::Type Reason)
{
	Game.SaveNow();
	if (UIContainer.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(UIContainer.ToSharedRef());
	UI.Reset(); UIContainer.Reset();
	Super::EndPlay(Reason);
}

void AHLWorldActor::BuildWorld()
{
	UWorld* W = GetWorld();
	// one movable sun; the materials carry an emissive ambient term (no sky light on the web tier)
	if (ADirectionalLight* Sun = W->SpawnActor<ADirectionalLight>(FVector(0, 0, 5000), FRotator(-48, 35, 0)))
	{
		Sun->GetRootComponent()->SetMobility(EComponentMobility::Movable);
		if (ULightComponent* L = Sun->GetLightComponent()) { L->SetIntensity(2.0f); L->SetLightColor(FLinearColor(1.f, 0.9f, 0.78f)); L->SetCastShadows(false); }
		Spawned.Add(Sun);
	}
	ObjectMeshes.SetNum(Game.Areas.Num());
	for (int32 a = 0; a < Game.Areas.Num(); ++a)
	{
		const FHLAreaDef& Def = *Game.Areas[a].Def;
		AActor* Geo = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
		Spawned.Add(Geo);
		USceneComponent* Root = NewObject<USceneComponent>(Geo);
		Geo->SetRootComponent(Root); Root->RegisterComponent();
		const FVector2D Ctr = Def.Min + Def.Size * 0.5f;
		// floor (top at z = 0) and sky/dome shell
		AddMesh(Geo, Cube, ToWorld(a, Ctr, -0.5f), FVector(Def.Size.X + 400, Def.Size.Y + 400, 1), Def.Floor);
		const float SkyR = Def.bDungeon ? 400.f : 2600.f;
		AddMesh(Geo, Sphere, ToWorld(a, Ctr, 0), FVector(SkyR * 2), Def.Sky, true);
		int32 gi = 0;
		for (const FHLBox& B : Def.Ground) AddMesh(Geo, Cube, ToWorld(a, FVector2D(B.X + B.W * 0.5f, B.Y + B.D * 0.5f), 0.01f + 0.004f * (gi++ % 5)), FVector(B.W, B.D, 0.03f), B.Color);
		for (const FHLBox& B : Def.Boxes) AddMesh(Geo, Cube, ToWorld(a, FVector2D(B.X + B.W * 0.5f, B.Y + B.D * 0.5f), B.H * 0.5f), FVector(B.W, B.D, B.H), B.Color);
		for (const FHLPortal& Po : Def.Portals) AddMesh(Geo, Cylinder, ToWorld(a, Po.Pos, 1.6f), FVector(Po.R * 1.2f, Po.R * 1.2f, 3.2f), FLinearColor(0.2f, 0.9f, 0.8f), true);
		for (const FHLAidStation& Aid : Def.Aid) AddMesh(Geo, Cube, ToWorld(a, Aid.Pos, 0.05f), FVector(3, 3, 0.1f), FLinearColor(0.9f, 0.1f, 0.1f), true);
		// interactable objects
		for (const FHLObjectDef& O : Def.Objects)
		{
			UStaticMesh* Mesh = Cube; FVector Scale(0.6f); float Z = 0.3f; FLinearColor C(0.8f, 0.7f, 0.2f); bool bUnlit = false;
			if (O.Kind == TEXT("film_can") || O.Kind == TEXT("reel") || O.Kind == TEXT("coffee_tin")) { Mesh = Cylinder; Scale = FVector(0.5f, 0.5f, 0.25f); Z = 0.15f; C = O.Kind == TEXT("coffee_tin") ? FLinearColor(0.7f, 0.2f, 0.1f) : FLinearColor(0.75f, 0.75f, 0.8f); }
			else if (O.Kind == TEXT("arc_lamp")) { Mesh = Cylinder; Scale = FVector(0.3f, 0.3f, 4); Z = 2; C = FLinearColor(0.2f, 0.2f, 0.22f); }
			else if (O.Kind == TEXT("spire")) { Mesh = Cone; Scale = FVector(2.4f, 2.4f, 7); Z = 3.5f; C = FLinearColor(0.35f, 0.15f, 0.4f); }
			else if (O.Kind == TEXT("egg") || O.Kind == TEXT("boss_egg")) { Mesh = Sphere; Scale = FVector(1.3f, 1.3f, 0.9f); Z = 0.4f; C = FLinearColor(0.75f, 0.8f, 0.5f); }
			else if (O.Kind == TEXT("egg_sac")) { Mesh = Sphere; Scale = FVector(3, 3, 2.4f); Z = 1.2f; C = FLinearColor(0.8f, 0.5f, 0.6f); }
			else if (O.Kind == TEXT("callbox")) { Scale = FVector(0.4f, 0.4f, 1.4f); Z = 0.7f; C = FLinearColor(0.1f, 0.2f, 0.6f); }
			else if (O.Kind == TEXT("scrap")) { Scale = FVector(1.2f, 0.9f, 0.5f); Z = 0.25f; C = FLinearColor(0.45f, 0.35f, 0.25f); }
			else if (O.Kind == TEXT("mailbox")) { Scale = FVector(0.5f, 0.5f, 1.2f); Z = 0.6f; C = FLinearColor(0.1f, 0.3f, 0.15f); }
			else if (O.Kind == TEXT("redcar")) { Scale = FVector(13, 2.6f, 3.2f); Z = 1.9f; C = FLinearColor(0.7f, 0.08f, 0.06f); }
			else if (O.Kind == TEXT("usher")) { Mesh = Cylinder; Scale = FVector(0.55f, 0.55f, 1.5f); Z = 0.75f; C = FLinearColor(0.6f, 0.1f, 0.15f); }
			ObjectMeshes[a].Add(AddMesh(Geo, Mesh, ToWorld(a, O.Pos, Z), Scale, C, bUnlit));
		}
	}
	// target ring
	AActor* RingActor = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	Spawned.Add(RingActor);
	Ring = AddMesh(RingActor, Cylinder, FVector::ZeroVector, FVector(1.6f, 1.6f, 0.03f), FLinearColor(1, 0.1f, 0.1f), true);
	Ring->SetVisibility(false);
}

void AHLWorldActor::BuildUnit(const FHLUnit& U, FHLUnitVis& V)
{
	AActor* A = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
	USceneComponent* Root = NewObject<USceneComponent>(A);
	Root->SetMobility(EComponentMobility::Movable);
	A->SetRootComponent(Root); Root->RegisterComponent();
	const float S = U.Scale;
	if (U.Shape == TEXT("bug"))
	{
		V.Body = AddMesh(A, Sphere, FVector(0, 0, 55 * S), FVector(1.5f, 0.95f, 0.75f) * S, U.Color);
		V.Head = AddMesh(A, Cone, FVector(80 * S, 0, 60 * S), FVector(0.5f, 0.5f, 0.7f) * S, U.Color * 0.6f);
		V.Head->SetRelativeRotation(FRotator(-90, 0, 0));
	}
	else if (U.Shape == TEXT("quad"))
	{
		V.Body = AddMesh(A, Cube, FVector(0, 0, 50 * S), FVector(1.1f, 0.42f, 0.5f) * S, U.Color);
		V.Head = AddMesh(A, Cube, FVector(65 * S, 0, 70 * S), FVector(0.4f, 0.32f, 0.32f) * S, U.Color * 0.8f);
	}
	else
	{
		V.Body = AddMesh(A, Cylinder, FVector(0, 0, 70 * S), FVector(0.55f, 0.55f, 1.4f) * S, U.Color);
		static const FLinearColor Skin[4] = { FLinearColor(0.9f, 0.72f, 0.58f), FLinearColor(0.75f, 0.55f, 0.4f), FLinearColor(0.5f, 0.35f, 0.25f), FLinearColor(0.95f, 0.8f, 0.68f) };
		V.Head = AddMesh(A, Sphere, FVector(0, 0, 160 * S), FVector(0.4f) * S, U.Kind == EHLKind::Mob ? U.Color * 1.3f : Skin[U.Id % 4]);
		// a nose so facing is readable
		V.Extra = AddMesh(A, Cube, FVector(22 * S, 0, 160 * S), FVector(0.12f, 0.1f, 0.08f) * S, FLinearColor(0.1f, 0.1f, 0.1f));
	}
	V.Actor = A;
	Spawned.Add(A);
}

void AHLWorldActor::SyncUnits()
{
	const FHLUnit* P = Game.PlayerUnit();
	const int32 ViewArea = P ? P->Area : 0;
	const FVector2D ViewPos = P ? P->Pos : Game.Areas[0].Def->Spawn;
	TSet<int32> Alive;
	for (FHLUnit* U : Game.UnitList)
	{
		const bool bVisible = U->Area == ViewArea && FVector2D::DistSquared(U->Pos, ViewPos) < 170.f * 170.f;
		FHLUnitVis* V = Vis.Find(U->Id);
		if (!bVisible) { if (V && V->Actor.IsValid()) V->Actor->SetActorHiddenInGame(true); if (V) Alive.Add(U->Id); continue; }
		if (!V) { V = &Vis.Add(U->Id); BuildUnit(*U, *V); }
		Alive.Add(U->Id);
		AActor* A = V->Actor.Get();
		if (!A) continue;
		A->SetActorHiddenInGame(false);
		float Z = U->Z + (U->HopT > 0 ? FMath::Sin(U->HopT * PI * 2) * 0.5f : 0.f);
		if (U->bSitting && !U->bDead) Z -= 0.45f;
		if (U->bMounted) Z += 0.9f;
		A->SetActorLocationAndRotation(ToWorld(U->Area, U->Pos, Z), FRotator(0, U->Yaw, U->bDead ? 90 : 0));
		if (U->bDead != V->bDeadLook)
		{
			V->bDeadLook = U->bDead;
			if (V->Body.IsValid()) V->Body->SetMaterial(0, Mid(U->bDead ? U->Color * 0.45f : U->Color));
		}
		if (U->bMounted != V->bMountLook)
		{
			V->bMountLook = U->bMounted;
			if (U->bMounted && !V->Extra.IsValid()) V->Extra = AddMesh(A, Cube, FVector(0, 0, -45), FVector(2.2f, 0.8f, 0.8f), FLinearColor(0.12f, 0.12f, 0.14f));
			else if (U->bMounted && V->Extra.IsValid()) { V->Extra->SetRelativeLocation(FVector(0, 0, -45)); V->Extra->SetRelativeScale3D(FVector(2.2f, 0.8f, 0.8f)); }
		}
	}
	for (auto It = Vis.CreateIterator(); It; ++It)
	{
		if (Alive.Contains(It.Key())) continue;
		if (AActor* A = It.Value().Actor.Get()) { Spawned.Remove(A); A->Destroy(); }
		It.RemoveCurrent();
	}
	// objects
	for (int32 a = 0; a < Game.Areas.Num(); ++a)
		for (int32 o = 0; o < Game.Areas[a].Objects.Num() && o < ObjectMeshes[a].Num(); ++o)
			if (UStaticMeshComponent* C = ObjectMeshes[a][o].Get()) { const bool bShow = Game.Areas[a].Objects[o].bActive; if (C->IsVisible() != bShow) C->SetVisibility(bShow); }
	// target ring
	const FHLUnit* T = P ? Game.Unit(P->TargetId) : nullptr;
	if (Ring)
	{
		Ring->SetVisibility(T != nullptr);
		if (T)
		{
			Ring->SetWorldLocation(ToWorld(T->Area, T->Pos, 0.06f));
			Ring->SetWorldScale3D(FVector(1.5f * FMath::Max(1.f, T->Scale), 1.5f * FMath::Max(1.f, T->Scale), 0.03f));
			Ring->SetMaterial(0, Mid(T->Kind == EHLKind::Mob ? (T->Mob && !T->Mob->bAggressive ? FLinearColor(1, 0.9f, 0.1f) : FLinearColor(1, 0.1f, 0.1f)) : FLinearColor(0.2f, 1, 0.2f), true));
		}
	}
}

void AHLWorldActor::Tick(float Dt)
{
	Super::Tick(Dt);
	HLBridge::Poll();
	if (!Game.Areas.Num()) return;
	if (UI.IsValid()) UI->PreTick(Dt);
	if (!AutoClass.IsEmpty()) AutoTest(Dt);
	Game.Tick(Dt);
	SyncUnits();
	// camera: orbit the player (or the forecourt on the title screen)
	const FHLUnit* P = Game.PlayerUnit();
	FVector Focus = P ? ToWorld(P->Area, P->Pos, P->Z + 1.7f) : ToWorld(0, Game.Areas[0].Def->Spawn + FVector2D(0, 8), 2.f);
	float Yaw = CamYaw, Pitch = CamPitch, D = CamDist;
	if (!Game.bInWorld) { Yaw = -90 + GetWorld()->GetTimeSeconds() * 3.f; Pitch = 18; D = 46; }
	const FRotator Rot(-Pitch, Yaw, 0);
	// camera collision against the nav grid: pull in until the line from the player is clear
	if (P && Game.bInWorld)
	{
		const FHLNav& Nav = Game.Areas[P->Area].Nav;
		const FVector Dir = -Rot.Vector();
		float Clear = D;
		for (float S = 1.f; S <= D; S += 0.75f) { const FVector2D Pt = P->Pos + FVector2D(Dir.X, Dir.Y) * S; if (Nav.BlockedAt(Pt)) { Clear = FMath::Max(1.5f, S - 1.0f); break; } }
		D = Clear;
	}
	FVector Loc = Focus - Rot.Vector() * D * M;
	Loc.Z = FMath::Max(Loc.Z, 40.f);
	Camera->SetWorldLocationAndRotation(Loc, (Focus - Loc).Rotation());
	// periodic status line for automated checks (browser console)
	static double NextLog = 5;
	if (Game.bInWorld && Game.Now >= NextLog)
	{
		NextLog = Game.Now + 20;
		int32 SimsIn = 0; for (const FHLSim& S : Game.Sims) if (S.UnitId) ++SimsIn;
		int32 Bosses = 0, DMobs = 0, PartyAlive = 0;
		for (const FHLUnit* U : Game.UnitList) if (U->Kind == EHLKind::Mob && U->Area == 1 && !U->bDead) { ++DMobs; if (U->Mob->Rank == TEXT("boss")) ++Bosses; }
		for (int32 SI : Game.Party) if (const FHLUnit* U = Game.Unit(Game.Sims[SI].UnitId)) if (!U->bDead) ++PartyAlive;
		HLBridge::Log(FString::Printf(TEXT("HL_STATUS t=%.0f lvl=%d xp=%d hp=%.0f area=%d deaths=%d kills=%d party=%d/%d dmobs=%d bosses=%d units=%d sims=%d chat=%d worker=%d calls=%d fails=%d fps=%.0f"), Game.Now, P ? P->Level : 0, Game.Player.Xp,
			P ? P->HpPct() * 100 : 0.f, P ? P->Area : -1, Game.Player.Deaths, Game.Player.Kills, PartyAlive, Game.Party.Num(), DMobs, Bosses,
			Game.UnitList.Num(), SimsIn, Game.Chat.Num(), Game.bWorkerOk ? 1 : 0, Game.WorkerCalls, Game.WorkerFails, Dt > 0 ? 1.f / Dt : 0.f));
	}
}

void AHLWorldActor::AutoTest(float Dt)
{
	const double T = GetWorld()->GetTimeSeconds();
	if (!Game.bInWorld)
	{
		if (!bSaveChecked || T < 2) return;
		const FHLClass* C = FHLData::Get().Class(AutoClass);
		if (!C) C = &FHLData::Get().Classes[0];
		Game.NewCharacter(TEXT("Tester"), C);
		Game.EnterWorld();
		for (const FHLQuest& Q : FHLData::Get().Quests) if (Q.Level <= 2 && Game.QuestAvailable(Q)) Game.AcceptQuest(Q);
		if (FParse::Param(FCommandLine::Get(), TEXT("HLAutoDungeon")))
		{
			// dungeon soak test: level 10 player + 4 level-10 simulated players (tank, healer, 2 others) inside the Egyptian
			FHLUnit* P = Game.PlayerUnit();
			P->Level = 10; Game.RecalcPlayerStats(); P->Hp = P->MaxHp;
			for (const FHLAbility* Ab : C->Abilities) if (Ab->Level <= 10) Game.LearnAbility(Ab, true);
			bool bTank = C->Role == TEXT("tank"), bHeal = C->Role == TEXT("healer");
			for (int32 Pass = 0; Pass < 3 && Game.Party.Num() < 4; ++Pass)
				for (int32 i = 0; i < Game.Sims.Num() && Game.Party.Num() < 4; ++i)
				{
					FHLSim& S = Game.Sims[i];
					if (!S.UnitId || S.Role != EHLSimRole::Local || S.bInParty) continue;
					const FString& SimRole = S.P->Cls->Role;
					if (Pass == 0 && !(SimRole == TEXT("tank") && !bTank)) continue;
					if (Pass == 1 && !(SimRole == TEXT("healer") && !bHeal)) continue;
					if (Pass == 2 && (SimRole == TEXT("tank") || SimRole == TEXT("healer"))) continue;
					if (SimRole == TEXT("tank")) bTank = true;
					if (SimRole == TEXT("healer")) bHeal = true;
					S.Level = 10; Game.RecalcCharStats(*Game.Unit(S.UnitId), *S.P->Cls, 10, S.P->Equipped, true);
					Game.SimJoinParty(i);
				}
			Game.SpawnArea(1, true);
			Game.TeleportUnit(*P, 1, Game.Areas[1].Def->Spawn);
			for (int32 SI : Game.Party) Game.TeleportUnit(*Game.Unit(Game.Sims[SI].UnitId), 1, Game.Areas[1].Def->Spawn + FVector2D(SI % 5 - 2, 2));
		}
		return;
	}
	if (T >= AutoNextShot)
	{
		AutoNextShot = T + (AutoShots < 3 ? 6 : 25);
#if !PLATFORM_WASM
		FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/hl_auto_%02d.png"), AutoShots++), true, false);
#endif
	}
	if (AutoQuit > 0 && T > AutoQuit) { UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false); return; }
	FHLUnit* P = Game.PlayerUnit();
	if (!P || Game.Now < AutoNextAct) { if (P && !P->bDead) if (FHLUnit* Tg = Game.Unit(P->TargetId)) if (!Tg->bDead && Tg->Kind == EHLKind::Mob) Game.MoveTo(*P, Tg->Pos, P->AutoRange - 1); return; }
	AutoNextAct = Game.Now + 0.4f;
	if (P->bDead) { Game.ReleaseSpirit(); return; }
	if (Game.InviteFromSim >= 0) Game.AnswerInvite(true);
	FHLUnit* Tg = Game.Unit(P->TargetId);
	if (!Tg || Tg->bDead || Tg->Kind != EHLKind::Mob)
	{
		for (FHLUnit* U : Game.UnitList) if (U->Kind == EHLKind::Mob && U->bDead && U->bLootable && U->TappedBy == Game.PlayerUnitId && Game.Dist(*P, *U) < 6) Game.LootCorpse(*U);
		if (P->HpPct() < 0.5f && !Game.InCombat(*P)) { P->bSitting = true; return; }
		FHLUnit* Best = nullptr; float BestD = 1e9f;
		const int32 MaxLvl = P->Area == 0 ? P->Level + 1 : 99;
		if (P->Area != 0) { bool bRest = false; for (int32 SI : Game.Party) if (const FHLUnit* Mb = Game.Unit(Game.Sims[SI].UnitId)) if (Mb->bDead || Mb->HpPct() < 0.6f || (Mb->ResType == EHLRes::Power && Mb->Res < Mb->MaxRes * 0.4f)) bRest = true; if (bRest && !Game.InCombat(*P)) { P->bSitting = true; return; } }
		for (FHLUnit* U : Game.UnitList) if (U->Kind == EHLKind::Mob && !U->bDead && U->Area == P->Area && U->Level <= MaxLvl && !U->Mob->bGuard && (U->TappedBy == 0 || U->TappedBy == Game.PlayerUnitId) && Game.Dist(*P, *U) < BestD) { Best = U; BestD = Game.Dist(*P, *U); }
		if (Best) Game.TargetUnit(Best->Id);
		return;
	}
	P->bAutoAttack = true;
	CamYaw = P->Yaw;
	for (int32 i = 0; i < Game.Player.Bar.Num(); ++i) if (Game.Player.Bar[i] && Game.Player.Bar[i]->Target == TEXT("enemy") && Game.CanCast(*P, *Game.Player.Bar[i], Tg).IsEmpty()) { Game.TryCast(*P, *Game.Player.Bar[i], Tg->Id); break; }
}
