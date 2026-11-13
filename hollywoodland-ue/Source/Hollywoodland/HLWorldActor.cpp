#include "HLWorldActor.h"
#include "HLBridge.h"
#include "HLUI.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
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
#include "Kismet/KismetRenderingLibrary.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"

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

UStaticMeshComponent* AHLWorldActor::AddProp(AActor* InOwner, int32 Area, const FHLPropPlace& P)
{
	if (!P.IsSet()) return nullptr;
	TObjectPtr<UStaticMesh>* Found = PropMeshes.Find(P.Mesh);
	UStaticMesh* Mesh = Found ? Found->Get() : nullptr;
	if (!Found)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, *P.Mesh, nullptr, LOAD_NoWarn);
		if (!Mesh) HLBridge::Log(FString::Printf(TEXT("HL prop mesh missing: %s"), *P.Mesh));
		PropMeshes.Add(P.Mesh, Mesh);
	}
	if (!Mesh) return nullptr;
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(InOwner);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	if (InOwner->GetRootComponent()) C->SetupAttachment(InOwner->GetRootComponent()); else InOwner->SetRootComponent(C);
	C->SetRelativeLocation(ToWorld(Area, P.Pos, P.Z));
	C->SetRelativeRotation(FRotator(0, P.Yaw, 0));
	C->SetRelativeScale3D(P.Scale);
	// small things vanish early; lamp posts and cars carry further; the Red Car and the hive spires carry across the zone
	const float R = Mesh->GetBounds().SphereRadius * P.Scale.GetMax();
	C->SetCullDistance(FMath::Clamp(R * 90.f, 5000.f, 60000.f));
	C->RegisterComponent();
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
	Game.Sfx.Init(!PLATFORM_WASM && !AutoClass.IsEmpty() && !FParse::Param(FCommandLine::Get(), TEXT("HLSfx")));
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
		for (const FHLBox& B : Def.Boxes) if (!AddProp(Geo, a, B.Prop)) AddMesh(Geo, Cube, ToWorld(a, FVector2D(B.X + B.W * 0.5f, B.Y + B.D * 0.5f), B.H * 0.5f), FVector(B.W, B.D, B.H), B.Color);
		// building-kit modules (facade bays, pilasters, parapets: hundreds of each) draw as one instanced component per mesh
		TMap<FString, UInstancedStaticMeshComponent*> Kit;
		for (const FHLPropPlace& P : Def.Props)
		{
			if (!P.Mesh.StartsWith(TEXT("/Game/Buildings/BRK_")) && !P.Mesh.StartsWith(TEXT("/Game/Buildings/SM_BRK_"))) { AddProp(Geo, a, P); continue; }
			UInstancedStaticMeshComponent*& I = Kit.FindOrAdd(P.Mesh);
			if (!I)
			{
				UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *P.Mesh, nullptr, LOAD_NoWarn);
				if (!Mesh) { HLBridge::Log(FString::Printf(TEXT("HL kit mesh missing: %s"), *P.Mesh)); Kit.Remove(P.Mesh); continue; }
				I = NewObject<UInstancedStaticMeshComponent>(Geo);
				I->SetStaticMesh(Mesh);
				I->SetMobility(EComponentMobility::Movable);
				I->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				I->SetCastShadow(false);
				I->SetupAttachment(Geo->GetRootComponent());
				I->RegisterComponent();
			}
			I->AddInstance(FTransform(FRotator(0, P.Yaw, 0), ToWorld(a, P.Pos, P.Z), P.Scale), /*bWorldSpace*/ true);
		}
		for (const FHLPortal& Po : Def.Portals) AddMesh(Geo, Cylinder, ToWorld(a, Po.Pos, 1.6f), FVector(Po.R * 1.2f, Po.R * 1.2f, 3.2f), FLinearColor(0.2f, 0.9f, 0.8f), true);
		for (const FHLAidStation& Aid : Def.Aid) AddMesh(Geo, Cube, ToWorld(a, Aid.Pos, 0.05f), FVector(3, 3, 0.1f), FLinearColor(0.9f, 0.1f, 0.1f), true);
		// interactable objects
		for (const FHLObjectDef& O : Def.Objects)
		{
			if (UStaticMeshComponent* PC = AddProp(Geo, a, O.Prop))
			{
				ObjectMeshes[a].Add(PC);
				if (O.Kind == TEXT("arc_lamp"))
				{
					// the lamp's light, shown while it is lit (SyncUnits)
					UPointLightComponent* L = NewObject<UPointLightComponent>(Geo);
					L->SetupAttachment(Geo->GetRootComponent());
					L->SetMobility(EComponentMobility::Movable);
					L->SetRelativeLocation(ToWorld(a, O.Pos, 2.4f));
					L->SetIntensity(6000.f); L->SetAttenuationRadius(1400.f); L->SetLightColor(FLinearColor(1.f, 0.92f, 0.75f)); L->SetCastShadows(false);
					L->SetVisibility(false);
					L->RegisterComponent();
					LampLights.Add(PC, L);
				}
				continue;
			}
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
			if (U.Mob && U.Mob->Script == TEXT("flier")) V.Extra = AddMesh(A, Cube, FVector(0, 0, 75 * S), FVector(0.5f, 3.2f, 0.04f) * S, U.Color * 0.7f); // wings
		}
	else if (U.Shape == TEXT("quad"))
	{
		V.Body = AddMesh(A, Cube, FVector(0, 0, 50 * S), FVector(1.1f, 0.42f, 0.5f) * S, U.Color);
		V.Head = AddMesh(A, Cube, FVector(65 * S, 0, 70 * S), FVector(0.4f, 0.32f, 0.32f) * S, U.Color * 0.8f);
	}
	else if (FHLChars::Get().Ready())
	{
		V.bHuman = true;
		V.Look = FHLChars::Get().LookFor(Game, U);
		V.NextLook = Game.Now + 0.5;
		A->SetActorScale3D(FVector(S));
		CharVis.Apply(GetWorld(), A, Root, V.Rig, V.Look, U.Id == Game.PlayerUnitId ? 1024 : 512);
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

UTextureRenderTarget2D* AHLWorldActor::Preview(const FHLCharLook& L, float Yaw, const FString& Clip)
{
	if (!GetWorld() || !FHLChars::Get().Ready()) return nullptr;
	if (!Stage)
	{
		// a kilometre up: nothing above to shadow it, out of sight of the street camera
		Stage = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(0, 0, 100000)));
		USceneComponent* Root = NewObject<USceneComponent>(Stage);
		Root->SetMobility(EComponentMobility::Movable);
		Stage->SetRootComponent(Root); Root->RegisterComponent();
		StageChar = NewObject<USceneComponent>(Stage);
		StageChar->SetMobility(EComponentMobility::Movable);
		StageChar->SetupAttachment(Root); StageChar->RegisterComponent();
		// a backdrop and a round floor, so the figure stands somewhere
		AddMesh(Stage, Cube, FVector(-160, 0, 150), FVector(0.1f, 9.f, 6.f), FLinearColor(0.07f, 0.06f, 0.05f), true);
		AddMesh(Stage, Cylinder, FVector(0, 0, -2), FVector(1.6f, 1.6f, 0.04f), FLinearColor(0.3f, 0.26f, 0.2f));
		UPointLightComponent* Key = NewObject<UPointLightComponent>(Stage);
		Key->SetupAttachment(Root); Key->SetMobility(EComponentMobility::Movable);
		Key->SetRelativeLocation(FVector(260, 140, 230));
		Key->SetIntensityUnits(ELightUnits::Unitless); Key->SetIntensity(2500.f); Key->SetAttenuationRadius(900.f);
		Key->SetLightColor(FLinearColor(1.f, 0.97f, 0.92f)); Key->SetCastShadows(false);
		Key->RegisterComponent();
		StageRT = NewObject<UTextureRenderTarget2D>(this);
		StageRT->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8_SRGB;
		StageRT->ClearColor = FLinearColor(0.16f, 0.13f, 0.1f, 1);
		StageRT->InitAutoFormat(512, 768);
		StageRT->UpdateResourceImmediate(true);
		StageCap = NewObject<USceneCaptureComponent2D>(Stage);
		StageCap->SetupAttachment(Root);
		StageCap->SetMobility(EComponentMobility::Movable);
		// 26 degrees across a 2:3 frame is about 38 up and down: a 2.1 m tall view at 3 m
		StageCap->FOVAngle = 26.f;
		StageCap->SetRelativeLocation(FVector(305, 0, 100));
		StageCap->SetRelativeRotation(FRotator(-1.5f, 180, 0));
		StageCap->TextureTarget = StageRT;
		StageCap->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		StageCap->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		StageCap->ShowOnlyActors.Add(Stage);
		StageCap->bCaptureEveryFrame = false; StageCap->bCaptureOnMovement = false;
		StageCap->PostProcessSettings = Camera->PostProcessSettings;
		StageCap->PostProcessSettings.bOverride_VignetteIntensity = true; StageCap->PostProcessSettings.VignetteIntensity = 0.4f;
		StageCap->PostProcessBlendWeight = 1.f;
		StageCap->ShowFlags.SetFog(false);
		StageCap->ShowFlags.SetAtmosphere(false);
		StageCap->RegisterComponent();
		Spawned.Add(Stage);
	}
	StageWantUntil = FPlatformTime::Seconds() + 0.3;
	Stage->SetActorHiddenInGame(false);
	StageCap->bCaptureEveryFrame = true;
	CharVis.Apply(GetWorld(), Stage, StageChar, StageRig, L, 1024);
	if (USkeletalMeshComponent* Ld = StageRig.Leader.Get()) Ld->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	CharVis.Play(StageRig, Clip.IsEmpty() ? FString(TEXT("Idle")) : Clip, true);
	StageChar->SetRelativeRotation(FRotator(0, Yaw, 0));
	return StageRT;
}

void AHLWorldActor::TickStage()
{
	if (!Stage || !StageCap || FPlatformTime::Seconds() < StageWantUntil || Stage->IsHidden()) return;
	// nobody looked for a moment: stop filming and hide the stage
	StageCap->bCaptureEveryFrame = false;
	Stage->SetActorHiddenInGame(true);
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
		float Z = U->Z + (U->HopT > 0 && !V->bHuman ? FMath::Sin(U->HopT * PI * 2) * 0.5f : 0.f);
		if (U->bSitting && !U->bDead && !V->bHuman) Z -= 0.45f;
		if (U->bMounted) Z += 0.9f;
		Z += U->Lift;
		if (U->bBurrowed) Z -= 0.85f * U->Scale; // only the back shows, a mound in the street
		A->SetActorLocationAndRotation(ToWorld(U->Area, U->Pos, Z), FRotator(0, U->Yaw, U->bDead && !V->bHuman ? 90 : 0));
		if (V->bHuman)
		{
			// equipment changes show up within half a second
			if (Game.Now >= V->NextLook)
			{
				V->NextLook = Game.Now + 0.5;
				V->Look = FHLChars::Get().LookFor(Game, *U);
				CharVis.Apply(GetWorld(), A, A->GetRootComponent(), V->Rig, V->Look, U->Id == Game.PlayerUnitId ? 1024 : 512);
			}
			static const bool bNoAnim = FParse::Param(FCommandLine::Get(), TEXT("HLBindPose"));
			if (!bNoAnim) CharVis.Drive(V->Rig, *U, V->Look, Game.Now, Game.FrameDt);
		}
		if ((U->bDead != V->bDeadLook || U->bGhost != V->bGhostLook) && !V->bHuman)
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
			if (UStaticMeshComponent* C = ObjectMeshes[a][o].Get())
			{
				const FHLObjectState& St = Game.Areas[a].Objects[o];
				// a lit arc lamp stays where it is and shines until it gutters out (respawns); everything else is taken or destroyed
				const bool bLamp = St.Def && St.Def->Kind == TEXT("arc_lamp");
				const bool bShow = bLamp || St.bActive;
				if (C->IsVisible() != bShow) C->SetVisibility(bShow);
				if (bLamp) if (UPointLightComponent* L = LampLights.FindRef(C).Get()) { const bool bLit = !St.bActive; if (L->IsVisible() != bLit) L->SetVisibility(bLit); }
			}
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
	CharVis.Tick(Game.Now);
	SyncUnits();
	TickStage();
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
	// sound: the listener is the player's head, facing the camera
	if (P && Game.bInWorld) Game.Sfx.Tick(Game, Dt, P->Area, P->Pos, P->Z + 1.7f, Yaw);
	else Game.Sfx.Tick(Game, Dt, -1, FVector2D::ZeroVector, 0, Yaw);
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
		{
			// -HLGoto=x,y: start somewhere else (mob behaviour checks)
			FString Goto;
			if (FParse::Value(FCommandLine::Get(), TEXT("HLGoto="), Goto, false))
			{
				FString Xs, Ys;
				if (Goto.Split(TEXT(","), &Xs, &Ys)) Game.TeleportUnit(*Game.PlayerUnit(), 0, Game.Areas[0].Nav.NearestFree(FVector2D(FCString::Atof(*Xs), FCString::Atof(*Ys))));
			}
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
					S.Level = 10; Game.RecalcCharStats(*Game.Unit(S.UnitId), *S.P->Cls, 10, S.Gear, true);
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
	if (P && FParse::Param(FCommandLine::Get(), TEXT("HLViews")))
	{
		// building art check: a fixed tour of the boulevard fronts, one screenshot per stop (hl_view_NN.png)
		struct FView { float X, Y, Yaw, Pitch, Dist; };
		static const FView Views[] = { { 470, 204, -90, 8, 24 }, { 385, 204, -90, 8, 22 }, { 580, 204, -90, 14, 30 }, { 640, 196, 90, 8, 24 }, { 230, 196, 90, 8, 26 }, { 781, 200, 90, 6, 12 }, { 120, 205, -120, 18, 40 } };
		static int32 Step = -1; static double NextStep = 0;
		AutoNextShot = 1e9;
		P->CombatUntil = 0; P->bAutoAttack = false; P->TargetId = 0;
		if (T >= NextStep)
		{
			if (++Step >= (int32)UE_ARRAY_COUNT(Views)) { UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false); return; }
			const FView& V = Views[Step];
			Game.TeleportUnit(*P, 0, FVector2D(V.X, V.Y));
			NextStep = T + 2.5;
		}
		const FView& V = Views[FMath::Max(0, Step)];
		CamYaw = V.Yaw; CamPitch = V.Pitch; CamDist = V.Dist; P->Yaw = V.Yaw; P->Pos = FVector2D(V.X, V.Y); // pinned: the self-play bot keeps walking
#if !PLATFORM_WASM
		static int32 Shot = -1;
		if (Shot != Step && T > NextStep - 0.6) { Shot = Step; FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/hl_view_%02d.png"), Step), true, false); }
#endif
	}
	if (P && FParse::Param(FCommandLine::Get(), TEXT("HLLineup")))
	{
		// character art check: camera in front of the player, a new body and outfit every 3 s, a screenshot of each
		static int32 Step = -1;
		static double NextStep = 0, ShotAt = 0;
		CamYaw = P->Yaw + (FParse::Param(FCommandLine::Get(), TEXT("HLLineupSide")) ? 90 : 180); CamPitch = 6; CamDist = FParse::Param(FCommandLine::Get(), TEXT("HLLineupFar")) ? 7.f : 3.4f;
		AutoNextShot = 1e9;
		if (T >= NextStep)
		{
			NextStep = T + 3; ShotAt = T + 2.2; ++Step;
			const int32 k = Step;
			FHLAppearance& A = Game.Player.Look;
			A.Sex = k % 2; A.Skin = (k / 2) % 8; A.Face = (k * 5) % 12; A.Hair = (k * 7 + k / 2) % 12; A.HairColor = (k * 3) % 8; A.Outfit = k % 8;
			static const TCHAR* Types[4] = { TEXT("cloth"), TEXT("leather"), TEXT("padded"), TEXT("steel") };
			static const int32 Bands[] = { 1, 5, 10, 20, 30, 40, 50, 60 };
			const FString Ty = Types[(k / 2) % 4];
			const int32 Want = Bands[(k / 8) % 8];
			Game.Player.Equip.Reset();
			if (k % 8 != 0) // every eighth step shows the starter outfit
				for (const TCHAR* Sl : { TEXT("coat"), TEXT("trousers"), TEXT("shoes"), TEXT("gloves"), TEXT("belt"), TEXT("wrist"), TEXT("hat") })
				{
					const FHLItem* Best = nullptr;
					for (const FHLItem& It : FHLData::Get().Items)
						if (It.Kind == TEXT("armor") && It.Slot == Sl && It.Type == Ty && It.Q == (k % 5 == 4 ? 4 : 2) && (!Best || FMath::Abs(It.Req - Want) < FMath::Abs(Best->Req - Want))) Best = &It;
					if (!Best) for (const FHLItem& It : FHLData::Get().Items) if (It.Kind == TEXT("armor") && It.Slot == Sl && It.Type == Ty && (!Best || FMath::Abs(It.Req - Want) < FMath::Abs(Best->Req - Want))) Best = &It;
					if (Best) Game.Player.Equip.Add(Sl, Best);
				}
			static const TCHAR* Weap[] = { TEXT("knife"), TEXT("club"), TEXT("sabre"), TEXT("heavy"), TEXT("revolver"), TEXT("rifle"), TEXT("tool"), TEXT("smg") };
			static const TCHAR* Guns[] = { TEXT("revolver"), TEXT("rifle"), TEXT("smg") };
			// -HLLineupGuns: only firearms, drawn and aimed (the hand grip check)
			const bool bGuns = FParse::Param(FCommandLine::Get(), TEXT("HLLineupGuns"));
			const FString Wt = bGuns ? Guns[k % 3] : Weap[k % 8];
			for (const FHLItem& It : FHLData::Get().Items)
				if (It.Kind == TEXT("weapon") && It.Type == Wt) { Game.Player.Equip.Add(It.Slot, &It); break; }
			for (const FHLItem& It : FHLData::Get().Items) if (It.Kind == TEXT("weapon") && It.Type == (k % 3 ? TEXT("lamp") : TEXT("shield"))) { if (!Game.Player.Equip.Contains(TEXT("off"))) Game.Player.Equip.Add(TEXT("off"), &It); break; }
			P->bRangedAuto = Wt == TEXT("revolver") || Wt == TEXT("rifle") || Wt == TEXT("smg");
			P->bAutoAttack = false; P->CombatUntil = (bGuns || k % 3 == 1) ? Game.Now + 3 : 0; P->bSitting = false;
			if (FParse::Param(FCommandLine::Get(), TEXT("HLLineupGestures")))
			{
				// one gesture per step (and a sit every sixth), the screenshot lands mid-clip
				static const TCHAR* Gs[] = { TEXT("Wave"), TEXT("Dance"), TEXT("Cheer"), TEXT("Laugh"), TEXT("Bow"), TEXT("Point"), TEXT("Shrug"), TEXT("Talk"), TEXT("Loot") };
				P->CombatUntil = 0;
				if (k % 6 == 5) P->bSitting = true; else Game.Gesture(*P, Gs[k % 9]);
			}
			// -HLLineupDance: the dance loop only, shots spread over the clip
			if (FParse::Param(FCommandLine::Get(), TEXT("HLLineupDance")))
			{
				P->CombatUntil = 0; P->bSitting = false;
				Game.Player.Equip.Remove(TEXT("main")); Game.Player.Equip.Remove(TEXT("off"));
				Game.Gesture(*P, TEXT("Dance"));
				ShotAt = T + 0.3 + (k % 5) * 0.6;
			}
			if (FHLUnitVis* V = Vis.Find(P->Id)) V->NextLook = 0;
			FString Desc;
			for (const auto& KV : Game.Player.Equip) Desc += KV.Key + TEXT("=") + KV.Value->Name + TEXT("; ");
			HLBridge::Log(FString::Printf(TEXT("HL_LINEUP %d sex=%d skin=%d face=%d hair=%d hairc=%d outfit=%d %s"), k, A.Sex, A.Skin, A.Face, A.Hair, A.HairColor, A.Outfit, *Desc));
		}
	#if !PLATFORM_WASM
		if (ShotAt > 0 && T >= ShotAt)
		{
			ShotAt = 0;
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/hl_lineup_%02d.png"), Step), false, false);
			if (FParse::Param(FCommandLine::Get(), TEXT("HLDumpAtlas")))
				if (FHLUnitVis* V = Vis.Find(P->Id))
					UKismetRenderingLibrary::ExportRenderTarget(this, CharVis.Atlas(GetWorld(), V->Look, 1024), FPaths::ProjectSavedDir() / TEXT("Screenshots"), FString::Printf(TEXT("atlas_%02d.png"), Step));
		}
	#endif
		return;
	}
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
	// -HLAuctionTest: walk to the auctioneer and keep the auction house open
	static const bool bAuctionTest = FParse::Param(FCommandLine::Get(), TEXT("HLAuctionTest"));
	if (bAuctionTest && P->Area == 0 && T > 8)
	{
		for (const FHLNpcDef& N : Game.Areas[0].Def->Npcs) if (N.HasRole(TEXT("auctioneer")))
		{
			if (FVector2D::Distance(P->Pos, N.Pos) > 7.5f) Game.TeleportUnit(*P, 0, Game.Areas[0].Nav.NearestFree(N.Pos + FVector2D(0, 4)));
			P->TargetId = 0; P->bAutoAttack = false;
			if (FVector2D::Distance(P->Pos, N.Pos) < 7.5f) { for (const auto& KV : Game.Units) if (KV.Value->Npc == &N) Game.OpenNpcUnitId = KV.Key; Game.OpenNpcMode = TEXT("auction"); bAutoGoal = false; }
		}
		return;
	}
	// -HLGotoMob=<mob id>: jump next to the first living unit of that template once (mob behaviour checks)
	{
		static bool bGotoDone = false; static FString GotoMob;
		if (!bGotoDone && T > 4 && FParse::Value(FCommandLine::Get(), TEXT("HLGotoMob="), GotoMob))
			for (FHLUnit* U : Game.UnitList) if (U->Kind == EHLKind::Mob && !U->bDead && U->Mob && U->Mob->Id == GotoMob && U->Area == 0)
			{
				bGotoDone = true;
				Game.TeleportUnit(*P, 0, Game.Areas[0].Nav.NearestFree(U->Pos + FVector2D(6, 6))); Game.MobEnterCombat(*U, P->Id); P->TargetId = U->Id; P->bAutoAttack = true;
				UE_LOG(LogTemp, Display, TEXT("HL_LIFE goto %s at %.0f,%.0f"), *GotoMob, U->Pos.X, U->Pos.Y);
				break;
			}
		static int32 GotoId = 0; if (bGotoDone && !GotoId) GotoId = P->TargetId;
		static double NextGotoLog = 0; if (bGotoDone && Game.Now > NextGotoLog) { NextGotoLog = Game.Now + 2; const FHLUnit* Gu = Game.Unit(GotoId); if (Gu) { UE_LOG(LogTemp, Display, TEXT("HL_LIFE goto mob state=%d dead=%d d=%.1f lift=%.1f hp=%.0f/%.0f ptarget=%d"), (int32)Gu->MobState, Gu->bDead, FVector2D::Distance(Gu->Pos, P->Pos), Gu->Lift, Gu->Hp, Gu->MaxHp, P->TargetId); } else { UE_LOG(LogTemp, Display, TEXT("HL_LIFE goto mob gone")); } }
		static bool bPocket = false;
		if (bGotoDone && !bPocket && FParse::Param(FCommandLine::Get(), TEXT("HLPocketTest")) && Game.Unit(P->TargetId) && Game.InCombat(*P))
		{
			bPocket = true;
			const FHLData& Dd = FHLData::Get();
			for (const FHLItem& It : Dd.Items) if (It.Type == TEXT("pocket")) Game.AddItem(&It, 2, false);
			auto UseNamed = [&](const TCHAR* N) { for (int32 i = 0; i < Game.Player.Bag.Num(); ++i) if (Game.Player.Bag[i].Item && Game.Player.Bag[i].Item->Name == N) { Game.UseBag(i); UE_LOG(LogTemp, Display, TEXT("HL_LIFE pocket used %s, target hp %.0f"), N, Game.Unit(P->TargetId) ? Game.Unit(P->TargetId)->Hp : -1.f); return; } };
			UseNamed(TEXT("Mk II Fragmentation Grenade"));
			UseNamed(TEXT("Patent Tonic")); // refused: shared cooldown
		}
		if (bGotoDone) { if (FHLUnit* Gm = Game.Unit(P->TargetId)) { if (!Gm->bDead) { P->bAutoAttack = true; Game.FaceTo(*P, Gm->Pos); if (FVector2D::Distance(P->Pos, Gm->Pos) > 4) Game.MoveTo(*P, Gm->Pos, 3.5f); } } return; } // stand and fight
	}
	// -HLInnTest: rest at the inn (rested XP), whisper someone (typing indicator), then rent a bank box
	static const bool bInnTest = FParse::Param(FCommandLine::Get(), TEXT("HLInnTest"));
	if (bInnTest && P->Area == 0 && T > 8)
	{
		static int32 InnStep = 0;
		const TCHAR* InnRole = T < 70 ? TEXT("innkeeper") : TEXT("banker");
		for (const FHLNpcDef& N : Game.Areas[0].Def->Npcs) if (N.HasRole(InnRole))
		{
			if (FVector2D::Distance(P->Pos, N.Pos) > 7.5f) Game.TeleportUnit(*P, 0, Game.Areas[0].Nav.NearestFree(N.Pos + FVector2D(0, 4)));
			P->TargetId = 0; P->bAutoAttack = false; bAutoGoal = false;
			break;
		}
		if (InnStep == 0 && T > 20)
		{
			InnStep = 1;
			for (const FHLSim& S : Game.Sims) if (S.bOnline && !S.bAfk) { Game.PlayerChat(FString::Printf(TEXT("/w %s hey, which way to the egyptian?"), *S.P->Handle)); break; }
		}
		if (InnStep == 1 && T > 75)
		{
			InnStep = 2;
			Game.Player.Cents += 2000;
			for (const FHLNpcDef& N : Game.Areas[0].Def->Npcs) if (N.HasRole(TEXT("banker"))) for (const auto& KV : Game.Units) if (KV.Value->Npc == &N) Game.OpenNpcUnitId = KV.Key;
			Game.OpenNpcMode = TEXT("bank");
			Game.BuyBankBox(); Game.BuyBankBox();
			UE_LOG(LogTemp, Display, TEXT("HL_LIFE inn test: rested %.0f, bank %d slots, cents %d"), Game.Player.Rested, Game.Player.Bank.Num(), Game.Player.Cents);
		}
		if (InnStep == 2 && T > 95) { InnStep = 3; P->TargetId = 0; Game.OpenNpcUnitId = 0; UE_LOG(LogTemp, Display, TEXT("HL_LIFE inn test done")); }
		if (InnStep < 3) return;
	}
	// -HLLifeTest: stand near whatever is happening on the street and watch
	static const bool bLifeTest = FParse::Param(FCommandLine::Get(), TEXT("HLLifeTest"));
	if (bLifeTest && Game.DuelFromSim >= 0) Game.AnswerDuel(true);
	if (bLifeTest && Game.GuildFromSim >= 0 && Game.Now >= Game.GuildInviteAt + 2) { Game.AnswerGuildInvite(true); Game.PlayerChat(TEXT("/g hi all, thanks for the invite")); }
	if (bLifeTest && Game.DuelState == 1) return;
	if (bLifeTest && Game.DuelState == 2 && Game.Sims.IsValidIndex(Game.DuelSim))
	{
		if (FHLUnit* Du = Game.Unit(Game.Sims[Game.DuelSim].UnitId))
		{
			P->TargetId = Du->Id; P->bAutoAttack = true; Game.FaceTo(*P, Du->Pos); CamYaw = P->Yaw;
			for (int32 i = 0; i < Game.Player.Bar.Num(); ++i) if (Game.Player.Bar[i] && Game.Player.Bar[i]->Target == TEXT("enemy") && Game.CanCast(*P, *Game.Player.Bar[i], Du).IsEmpty()) { Game.TryCast(*P, *Game.Player.Bar[i], Du->Id); break; }
		}
		return;
	}
	if (bLifeTest && P->Area == 0 && Game.DuelState == 0)
	{
		P->TargetId = 0; P->bAutoAttack = false;
		const FVector2D* Inn = Game.Areas[0].Def->Pois.Find(TEXT("inn"));
		FVector2D Want = Game.Happen.Kind ? Game.Happen.Pos + FVector2D(0, Game.Happen.Kind == 4 ? -9.f : 9.f) : (Inn ? *Inn + FVector2D(0, -6) : P->Pos);
		if ((Game.Fight.Stage || Game.Fight.bAnnounced) && Inn) Want = *Inn + FVector2D(0, -20);
		else if (Game.SimRun.Stage == 1 || Game.SimRun.Stage == 2) for (const FHLPortal& Po : Game.Areas[0].Def->Portals) if (Po.ToArea == TEXT("dng_egyptian")) Want = Po.Pos + FVector2D(0, -18);
		if (FVector2D::Distance(P->Pos, Want) > 6) { AutoGoal = Want; bAutoGoal = true; }
		else { Game.FaceTo(*P, (Game.Fight.Stage && Inn) ? *Inn + FVector2D(0, -10) : Game.SimRun.Stage ? P->Pos + FVector2D(0, 10) : Game.Happen.Kind ? Game.Happen.Pos : P->Pos + FVector2D(0, -5)); CamYaw = P->Yaw; }
		return;
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
