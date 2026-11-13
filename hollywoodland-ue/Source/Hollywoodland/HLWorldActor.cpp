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
	// native self-play runs are silent unless -HLMusic; the browser probe keeps music on to exercise it
	Game.Music.Init(!PLATFORM_WASM && !AutoClass.IsEmpty() && !FParse::Param(FCommandLine::Get(), TEXT("HLMusic")));
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
		if (U->bDead != V->bDeadLook || U->bGhost != V->bGhostLook)
		{
			V->bDeadLook = U->bDead; V->bGhostLook = U->bGhost;
			if (V->Body.IsValid()) V->Body->SetMaterial(0, Mid(U->bDead ? U->Color * 0.45f : U->bGhost ? FLinearColor(0.55f, 0.75f, 1.f) : U->Color));
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
	Game.Music.Tick(Game, Dt);
	SyncUnits();
	// camera: orbit the player (or the forecourt on the title screen)
	const FHLUnit* P = Game.PlayerUnit();
	FVector Focus = P ? ToWorld(P->Area, P->Pos, P->Z + 1.7f) : ToWorld(0, Game.Areas[0].Def->Spawn + FVector2D(0, 8), 2.f);
	float Yaw = CamYaw, Pitch = CamPitch, D = CamDist;
	if (!Game.bInWorld) { Yaw = -90 + GetWorld()->GetTimeSeconds() * 3.f; Pitch = 18; D = 46; }
	const FRotator Rot(-Pitch, Yaw, 0);
	// camera collision: a 3D ray from the head back to the camera against the building boxes (roofs included, so the
	// camera may look over a low wall); pulled in a little in front of the hit and eased back out
	if (P && Game.bInWorld)
	{
		const FHLNav& Nav = Game.Areas[P->Area].Nav;
		const FVector Dir = -Rot.Vector();
		const FVector O(P->Pos.X, P->Pos.Y, P->Z + 1.7f);
		const float Hit = Nav.Ray3(O, Dir, D + 0.4f);
		const float Want = Hit < D + 0.4f ? FMath::Max(1.2f, Hit - 0.4f) : D;
		CamCollideDist = Want < CamCollideDist ? Want : FMath::FInterpTo(CamCollideDist, Want, Dt, 4.f);
		D = FMath::Min(D, CamCollideDist);
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
		#if !PLATFORM_WASM
		if (FParse::Param(FCommandLine::Get(), TEXT("HLAutoTitleShot")) && AutoShots == 0)
		{
			static bool bShot = false;
			if (T < 3) return;
			if (!bShot) { bShot = true; FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/hl_title.png"), true, false); }
			if (T < 5) return;
		}
		#endif
		const FHLClass* C = FHLData::Get().Class(AutoClass);
		if (!C) C = &FHLData::Get().Classes[0];
		Game.NewCharacter(TEXT("Tester"), C);
		Game.EnterWorld();
		for (const FHLQuest& Q : FHLData::Get().Quests) if (Q.Level <= 2 && Game.QuestAvailable(Q)) Game.AcceptQuest(Q);
		int32 StartLevel = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("HLAutoLevel="), StartLevel) && StartLevel > 1)
		{
			// start at a given level (talent and balance checks): abilities trained, talents spent by the bot below
			FHLUnit* P = Game.PlayerUnit();
			P->Level = FMath::Clamp(StartLevel, 1, 60); Game.RecalcPlayerStats(); P->Hp = P->MaxHp;
			for (const FHLAbility* Ab : C->Abilities) if (Ab->Level <= P->Level) Game.LearnAbility(Ab, true);
		}
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
	if (P && P->bGhost && FVector2D::Distance(P->Pos, Game.Player.CorpsePos) > 8) Game.MoveTo(*P, Game.Player.CorpsePos, 4); // every frame
	else if (P && bAutoGoal && !P->bDead && Game.MoveTo(*P, AutoGoal, 5)) bAutoGoal = false;
		if (!P || Game.Now < AutoNextAct) { if (P && !P->bDead && !P->bGhost) if (FHLUnit* Tg = Game.Unit(P->TargetId)) if (!Tg->bDead && Tg->Kind == EHLKind::Mob) Game.MoveTo(*P, Tg->Pos, P->AutoRange - 1); return; }
	AutoNextAct = Game.Now + 0.4f;
	{
		// -HLAutoDieAt=<s>: die once (ghost-run test)
		static float DieAt = -1; if (DieAt < 0) { DieAt = 0; FParse::Value(FCommandLine::Get(), TEXT("HLAutoDieAt="), DieAt); }
		if (DieAt > 0 && T > DieAt && !P->bDead && !P->bGhost) { DieAt = 0; Game.KillUnit(*P, nullptr); AutoNextAct = Game.Now + 2; return; }
	}
	if (P->bDead) { Game.ReleaseSpirit(); return; }
	if (P->bGhost)
	{
		if (FVector2D::Distance(P->Pos, Game.Player.CorpsePos) <= 8) Game.ResurrectAtCorpse();
		else Game.MoveTo(*P, Game.Player.CorpsePos, 4);
		return;
	}
	if (Game.InviteFromSim >= 0) Game.AnswerInvite(true);
	// spend talent points: main tree first (tier by tier), then the next
	if (Game.TalentPoints() > 0)
		for (int32 Spec = 0; Spec < 3 && Game.TalentPoints() > 0; ++Spec)
			for (int32 Tier = 1; Tier <= 5 && Game.TalentPoints() > 0; ++Tier)
				for (const FHLTalent* Tl : Game.Player.Cls->Talents)
					if (Tl->Spec == Spec && Tl->Tier == Tier) while (Game.CanLearnTalent(*Tl).IsEmpty()) Game.LearnTalent(*Tl);
	// world events: defend a surging hub, sit in on picture night until the buff lands
	// -HLSocialTest: exercise trade-chat C.O.D., auction bid/listing, a duel and the mailbox
	static int32 SocialStep = FParse::Param(FCommandLine::Get(), TEXT("HLSocialTest")) ? 0 : 99;
	if (SocialStep < 99 && Game.DuelState == 0 && !Game.InCombat(*P))
	{
		if (SocialStep == 0 && T > 25 && Game.SimWts.Num())
		{
			for (const auto& KV : Game.SimWts) { if (Game.Sims[KV.Key].bOnline) { Game.PlayerChat(FString::Printf(TEXT("/w %s wtb your %s"), *Game.Sims[KV.Key].P->Handle, *KV.Value.Item->Name)); break; } }
			SocialStep = 1;
		}
		else if (SocialStep == 1 && T > 35)
		{
			Game.Player.Cents += 2000; // test money
			int32 Best = -1;
			for (int32 i = 0; i < Game.Auctions.Num(); ++i) if (!Game.Auctions[i].bMine && Game.Auctions[i].Bid > 0 && (Best < 0 || Game.Auctions[i].Bid < Game.Auctions[Best].Bid)) Best = i;
			if (Best >= 0) Game.BidAuction(Best);
			for (int32 i = 0; i < Game.Player.Bag.Num(); ++i) if (Game.Player.Bag[i].Item && Game.Player.Bag[i].Item->Kind == TEXT("consumable")) { Game.ListAuction(i, 2, 1); break; }
			SocialStep = 2;
		}
		else if (SocialStep == 2 && T > 45 && Game.Now >= AutoSocialAt)
		{
			AutoSocialAt = Game.Now + 12;
			int32 Pick = -1;
			for (int32 i = 0; i < Game.Sims.Num(); ++i) if (const FHLUnit* SU = Game.Unit(Game.Sims[i].UnitId)) if (!SU->bDead && !Game.Sims[i].bInParty && Game.Dist(*SU, *P) < 25 && !Game.InCombat(*SU)) { Pick = i; break; }
			if (Pick >= 0 && Game.CanDuelHere()) { Game.ChallengeDuel(Pick); if (Game.DuelState) SocialStep = 3; }
			if (T > 140) SocialStep = 3;
		}
		else if (SocialStep == 3 && T > 160)
		{
			for (const FHLObjectState& O : Game.Areas[0].Objects) if (O.Def->Kind == TEXT("mailbox")) { AutoGoal = O.Def->Pos + FVector2D(1.5f, 1.5f); bAutoGoal = true; if (FVector2D::Distance(P->Pos, O.Def->Pos) < 4) { Game.ObjectInteract(0, int32(&O - Game.Areas[0].Objects.GetData())); for (int32 m = 1; m < Game.Player.Mail.Num(); ++m) Game.TakeMail(m); SocialStep = 4; AutoSocialAt = Game.Now + 30; } break; }
			return;
		}
	}
	if (SocialStep == 4) { if (Game.Now < AutoSocialAt) { P->bSitting = true; return; } SocialStep = 99; }
	if (Game.DuelState == 2 && Game.Sims.IsValidIndex(Game.DuelSim)) { P->TargetId = Game.Sims[Game.DuelSim].UnitId; }
	bAutoGoal = false;
	const FHLData& HD = FHLData::Get();
	// -HLCraftTest: learn Gunsmithing, take Rounds for the Line, load .38s at the bench, hand them in to Earl
	static int32 CraftStep = FParse::Param(FCommandLine::Get(), TEXT("HLCraftTest")) ? 0 : 99;
	if (CraftStep < 99 && P->Area == 0 && !Game.InCombat(*P))
	{
		const FHLQuest* Q = HD.Quest(TEXT("q_hb_29"));
		if (CraftStep == 0 && T > 8 && Q)
		{
			Game.Player.Guns = 1;
			Game.AddItem(HD.ItemByName(TEXT("Scrap Metal")), 40, false); Game.AddItem(HD.ItemByName(TEXT("Brass Casings")), 40, false);
			Game.Player.Done.Add(TEXT("q_hb_28")); Game.AcceptQuest(*Q);
			Game.StartCraft(0, 30);
			CraftStep = 1; return;
		}
		if (CraftStep == 1) { if (Game.CraftIdx >= 0) return; CraftStep = 2; }
		if (CraftStep == 2 && Q)
		{
			for (const FHLNpcDef& N : Game.Areas[0].Def->Npcs) if (N.Id == TEXT("npc_gunsmith")) { AutoGoal = N.Pos; bAutoGoal = true; if (FVector2D::Distance(P->Pos, N.Pos) < 7.5f) { Game.CompleteQuest(*Q, 0); CraftStep = 3; AutoSocialAt = Game.Now + 4; } }
			return;
		}
		if (CraftStep == 3) { if (Game.Now < AutoSocialAt) return; CraftStep = 99; }
		if (CraftStep == 0) return;
	}
	// -HLEscortTest: take Whitey Comes Home and bring him back to the Pantages, fighting off the ambushes
	static int32 EscStep = FParse::Param(FCommandLine::Get(), TEXT("HLEscortTest")) ? 0 : 99;
	if (EscStep < 99 && P->Area == 0)
	{
		const FHLQuest* Q = HD.Quest(TEXT("q_hb_14b"));
		if (EscStep == 0)
		{
			if (T < 6 || !Q) return;
			Game.Player.Done.Add(TEXT("q_hb_14"));
			for (FHLUnit* U : Game.UnitList) if (U->Kind == EHLKind::Npc && U->Npc && U->Npc->Id == TEXT("npc_stagehand")) Game.TeleportUnit(*P, 0, U->Pos + FVector2D(2, 0));
			Game.AcceptQuest(*Q);
			EscStep = 1; return;
		}
		if (EscStep == 1)
		{
			if (!Game.bEscorting)
			{
				const FHLQuestState* QS = Game.QuestState(TEXT("q_hb_14b"));
				UE_LOG(LogTemp, Display, TEXT("HL_ESCORT %s"), QS && QS->bComplete ? TEXT("arrived") : TEXT("failed"));
				EscStep = QS && QS->bComplete ? 2 : 99;
				return;
			}
			const FHLUnit* W = Game.Unit(Game.EscortUnitId);
			FHLUnit* Threat = nullptr; float BestD = 35;
			for (FHLUnit* U : Game.UnitList) if (U->Kind == EHLKind::Mob && !U->bDead && U->Area == 0 && W && (U->TargetId == W->Id || U->TargetId == P->Id) && Game.Dist(*P, *U) < BestD) { Threat = U; BestD = Game.Dist(*P, *U); }
			if (Threat) { if (P->TargetId != Threat->Id) Game.TargetUnit(Threat->Id); }
			else
			{
				P->TargetId = 0; P->bAutoAttack = false;
				for (const FHLObjective& O : Q->Objectives) if (O.Type == TEXT("escort")) { AutoGoal = O.To; bAutoGoal = true; }
				return;
			}
		}
		else if (EscStep == 2)
		{
			for (const FHLNpcDef& N : Game.Areas[0].Def->Npcs) if (N.Id == TEXT("npc_pantages_manager")) { AutoGoal = N.Pos; bAutoGoal = true; if (FVector2D::Distance(P->Pos, N.Pos) < 7.5f) { Game.CompleteQuest(*Q, 0); EscStep = 99; } }
			return;
		}
	}
	const FHLUnit* Fighting = Game.Unit(P->TargetId);
	const bool bBusy = Fighting && !Fighting->bDead && Game.InCombat(*P);
	if (P->Area == 0 && Game.SurgeHub >= 0 && !bBusy && Game.DuelState == 0)
	{
		const FHLUnit* Cur = Game.Unit(P->TargetId);
		if (!Cur || Cur->bDead || !Game.SurgeMobs.Contains(Cur->Id))
		{
			FHLUnit* Best = nullptr; float BestD = 1e9f;
			for (int32 Id : Game.SurgeMobs) if (FHLUnit* SM = Game.Unit(Id)) if (!SM->bDead && Game.Dist(*P, *SM) < BestD) { Best = SM; BestD = Game.Dist(*P, *SM); }
			if (Best && BestD < 60) Game.TargetUnit(Best->Id);
			else { AutoGoal = Game.Hubs[Game.SurgeHub].Pos; bAutoGoal = true; return; }
		}
	}
	else if (P->Area == 0 && Game.FilmStart > 0 && !Game.bFilmBuffGiven && !Game.InCombat(*P) && Game.DuelState == 0)
	{
		if (const FVector2D* Scr = Game.Areas[0].Def->Pois.Find(TEXT("forecourt"))) { P->TargetId = 0; P->bAutoAttack = false; if (FVector2D::Distance(P->Pos, *Scr) < 12) P->bSitting = true; else { AutoGoal = *Scr + FVector2D(0, -6); bAutoGoal = true; } return; }
	}
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
