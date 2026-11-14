#include "HLWorldActor.h"
#include "HLBridge.h"
#include "HLUI.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
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
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/IConsoleManager.h"
#include "HLSettings.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"

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
	BaseExposure = PP.AutoExposureBias;
	PP.bOverride_BloomIntensity = true; PP.BloomIntensity = 0.f;
	PP.bOverride_VignetteIntensity = true; PP.VignetteIntensity = 0.25f;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereF(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylF(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeF(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LitF(TEXT("/Game/HL/M_HL.M_HL"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitF(TEXT("/Game/HL/M_HLSky.M_HLSky"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicF(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GroundF(TEXT("/Game/HL/M_HLGround.M_HLGround"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SkyDomeF(TEXT("/Game/HL/M_HLSkyDome.M_HLSkyDome"));
	MatGround = GroundF.Succeeded() ? GroundF.Object : nullptr;
	MatSkyDome = SkyDomeF.Succeeded() ? SkyDomeF.Object : nullptr;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> AddF(TEXT("/Game/HL/M_HLAdd.M_HLAdd"));
	MatAdd = AddF.Succeeded() ? AddF.Object : nullptr;
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> AmbF(TEXT("/Game/HL/MPC_HLAmbient.MPC_HLAmbient"));
	Ambient = AmbF.Succeeded() ? AmbF.Object : nullptr;
	Cube = CubeF.Object; Sphere = SphereF.Object; Cylinder = CylF.Object; Cone = ConeF.Object;
	MatLit = LitF.Succeeded() ? LitF.Object : BasicF.Object;
	MatUnlit = UnlitF.Succeeded() ? UnlitF.Object : BasicF.Object;
}

FVector AHLWorldActor::ToWorld(int32 Area, const FVector2D& P, float Z) const
{
		const FVector2D Off = Game.Areas.IsValidIndex(Area) ? Game.Areas[Area].Def->Offset : FVector2D::ZeroVector;
		return FVector((P.X + Off.X) * M, (P.Y + Off.Y) * M, (Z + GroundZ(Area, P)) * M);
	}

	UProceduralMeshComponent* AHLWorldActor::TerrainMesh(AActor* InOwner, int32 Area, const FVector2D& Lo, const FVector2D& Hi, float ZOff, UMaterialInterface* Mat)
	{
		if (!Terrain.IsValidIndex(Area) || !Terrain[Area].IsValid()) return nullptr;
		const FHLTerrain& T = Terrain[Area];
		// vertices on the terrain grid inside [Lo, Hi], plus the exact edges so patches line up with their boxes
		TArray<float> Xs, Ys;
		Xs.Add(Lo.X); for (float X = T.Origin.X + FMath::CeilToFloat((Lo.X - T.Origin.X) / T.Cell + 0.01f) * T.Cell; X < Hi.X - 0.01f; X += T.Cell) Xs.Add(X); Xs.Add(Hi.X);
		Ys.Add(Lo.Y); for (float Y = T.Origin.Y + FMath::CeilToFloat((Lo.Y - T.Origin.Y) / T.Cell + 0.01f) * T.Cell; Y < Hi.Y - 0.01f; Y += T.Cell) Ys.Add(Y); Ys.Add(Hi.Y);
		const int32 Nx = Xs.Num(), Ny = Ys.Num();
		TArray<FVector> V; TArray<FVector> Nrm; TArray<FVector2D> UV; TArray<int32> Tri;
		V.Reserve(Nx * Ny); Nrm.Reserve(Nx * Ny); UV.Reserve(Nx * Ny); Tri.Reserve((Nx - 1) * (Ny - 1) * 6);
		const FVector Base = ToWorld(Area, FVector2D::ZeroVector, 0) - FVector(0, 0, GroundZ(Area, FVector2D::ZeroVector) * M);
		for (int32 j = 0; j < Ny; ++j)
			for (int32 i = 0; i < Nx; ++i)
			{
				const FVector2D P(Xs[i], Ys[j]);
				V.Add(FVector(P.X * M, P.Y * M, (T.Height(P) + ZOff) * M) + Base);
				Nrm.Add(T.Normal(P));
				UV.Add(P / 10.f);
			}
		for (int32 j = 0; j + 1 < Ny; ++j)
			for (int32 i = 0; i + 1 < Nx; ++i)
			{
				const int32 A = j * Nx + i, B = A + 1, C = A + Nx, D = C + 1;
				Tri.Append({ A, C, B, B, C, D });
			}
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(InOwner);
		C->SetMobility(EComponentMobility::Movable);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->ComponentTags.Add(TEXT("NoShadow"));
		C->SetupAttachment(InOwner->GetRootComponent());
		C->RegisterComponent();
		C->CreateMeshSection_LinearColor(0, V, Tri, Nrm, UV, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
		if (Mat) C->SetMaterial(0, Mat);
		return C;
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

UMaterialInstanceDynamic* AHLWorldActor::GroundMid(const FString& Kind, const FLinearColor& C)
{
	if (Kind.IsEmpty() || !MatGround) return nullptr;
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = GroundMids.Find(Kind)) return *Found;
	UTexture* Tex = LoadObject<UTexture>(nullptr, *FString::Printf(TEXT("/Game/HL/Tex/T_GND_%s.T_GND_%s"), *Kind, *Kind), nullptr, LOAD_NoWarn);
	if (!Tex) { HLBridge::Log(FString::Printf(TEXT("HL ground tile missing: %s"), *Kind)); GroundMids.Add(Kind, nullptr); return nullptr; }
	// metres per repeat; the tint keeps each surface near its old flat colour's brightness
	struct FTile { const TCHAR* Kind; float Tile; FLinearColor Tint; };
	static const FTile Tiles[] = { { TEXT("Asphalt"), 7.f, FLinearColor(0.8f, 0.8f, 0.82f) }, { TEXT("Sidewalk"), 6.f, FLinearColor(0.85f, 0.83f, 0.8f) },
		{ TEXT("Lot"), 9.f, FLinearColor(0.95f, 0.95f, 0.95f) }, { TEXT("Grass"), 6.f, FLinearColor(0.85f, 0.9f, 0.8f) },
		{ TEXT("Blight"), 10.f, FLinearColor(0.9f, 0.85f, 0.9f) }, { TEXT("Theatre"), 3.f, FLinearColor(0.75f, 0.7f, 0.7f) } };
	float Tile = 6.f; FLinearColor Tint(0.85f, 0.85f, 0.85f);
	for (const FTile& T : Tiles) if (Kind == T.Kind) { Tile = T.Tile; Tint = T.Tint; }
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(MatGround, this);
	D->SetTextureParameterValue(TEXT("Tex"), Tex);
	D->SetScalarParameterValue(TEXT("Tile"), Tile);
	D->SetVectorParameterValue(TEXT("Tint"), Tint);
	D->SetScalarParameterValue(TEXT("Glow"), 0.22f);
	GroundMids.Add(Kind, D);
	return D;
}

UStaticMeshComponent* AHLWorldActor::AddMesh(AActor* InOwner, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale, const FLinearColor& Color, bool bUnlit, bool bShadow)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(InOwner);
	C->SetStaticMesh(Mesh);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(bShadow);
	if (bUnlit) C->ComponentTags.Add(TEXT("NoShadow")); // sky, markers, rings: never cast
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
	C->SetCullDistance(FMath::Clamp(R * 90.f, 5000.f, R > 1500.f ? 400000.f : 60000.f));
	C->RegisterComponent();
	return C;
}

void AHLWorldActor::ScatterFoliage(AActor* Geo, int32 Area)
{
	if (!Terrain.IsValidIndex(Area) || !Terrain[Area].IsValid()) return;
	const FHLTerrain& T = Terrain[Area];
	const FHLAreaDef& Def = *Game.Areas[Area].Def;
	// one hierarchical instanced component per mesh (art/foliage via tools/propprep.py), culled per cluster
	TMap<FString, UHierarchicalInstancedStaticMeshComponent*> Comps;
	auto Comp = [&](const TCHAR* Name, float CullM, bool bGrass) -> UHierarchicalInstancedStaticMeshComponent*
	{
		if (UHierarchicalInstancedStaticMeshComponent** F = Comps.Find(Name)) return *F;
		const FString Path = FString::Printf(TEXT("/Game/Props/SM_%s.SM_%s"), Name, Name);
		UHierarchicalInstancedStaticMeshComponent* C = nullptr;
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path, nullptr, LOAD_NoWarn))
		{
			C = NewObject<UHierarchicalInstancedStaticMeshComponent>(Geo);
			C->SetStaticMesh(Mesh);
			C->SetMobility(EComponentMobility::Movable);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCastShadow(false);
			C->ComponentTags.Add(TEXT("NoShadow"));
			C->SetCullDistances(0, FMath::RoundToInt32(CullM * M));
			C->SetupAttachment(Geo->GetRootComponent());
			C->RegisterComponent();
			if (bGrass) FoliageGrass.Add(C);
		}
		else HLBridge::Log(FString::Printf(TEXT("HL foliage mesh missing: %s"), *Path));
		Comps.Add(Name, C);
		return C;
	};
	FRandomStream R(7919 + Area * 977);
	auto Add = [&](UHierarchicalInstancedStaticMeshComponent* C, const FVector2D& P, float Scale, float Sink)
	{
		if (C) C->AddInstance(FTransform(FRotator(0, R.FRand() * 360.f, 0), ToWorld(Area, P, -Sink), FVector(Scale)), /*bWorldSpace*/ true);
	};
	TArray<const FHLBox*> Blight;
	for (const FHLBox& B : Def.Ground) if (B.Kind == TEXT("Blight")) Blight.Add(&B);
	auto InBlight = [&](const FVector2D& P) { for (const FHLBox* B : Blight) if (P.X >= B->X && P.X <= B->X + B->W && P.Y >= B->Y && P.Y <= B->Y + B->D) return true; return false; };
	const FVector2D Lo = Def.Min, Hi = Def.Min + Def.Size;
	const float GrassCull = 70.f;

	// tufts of grass over the open lots, with drifts of wildflowers; the blight keeps only dry grass
	static const TCHAR* Flowers[] = { TEXT("FlowerPoppy"), TEXT("FlowerLupine"), TEXT("FlowerDaisy"), TEXT("FlowerMustard") };
	int32 NGrass = 0, NBig = 0;
	for (float Y = Lo.Y; Y < Hi.Y; Y += 6.f)
		for (float X = Lo.X; X < Hi.X; X += 6.f)
		{
			const FVector2D C0(X + R.FRand() * 6.f, Y + R.FRand() * 6.f);
			const float W = T.Wildness(C0);
			if (W < 0.1f || R.FRand() > W * 0.7f) continue;
			const bool bBlight = InBlight(C0);
			UHierarchicalInstancedStaticMeshComponent* G = Comp(bBlight || R.FRand() < 0.35f ? TEXT("GrassDry") : TEXT("GrassGreen"), GrassCull, true);
			for (int32 k = R.RandRange(3, 7); k > 0; --k, ++NGrass) Add(G, C0 + FVector2D(R.FRandRange(-1.8f, 1.8f), R.FRandRange(-1.8f, 1.8f)), R.FRandRange(0.7f, 1.35f), 0.05f);
			if (!bBlight && W > 0.4f && R.FRand() < 0.2f)
			{
				UHierarchicalInstancedStaticMeshComponent* F = Comp(Flowers[R.RandRange(0, 3)], GrassCull * 0.8f, true);
				const FVector2D FC = C0 + FVector2D(R.FRandRange(-3.f, 3.f), R.FRandRange(-3.f, 3.f));
				for (int32 k = R.RandRange(3, 8); k > 0; --k, ++NGrass) Add(F, FC + FVector2D(R.FRandRange(-1.5f, 1.5f), R.FRandRange(-1.5f, 1.5f)), R.FRandRange(0.8f, 1.25f), 0.03f);
			}
		}
	// shrubs, rocks and trees: sparse in the lots, a scrubby woodland on the hills past the hive ring
	static const TCHAR* Trees[] = { TEXT("TreeEucalyptus"), TEXT("TreeEucalyptus"), TEXT("TreePepper"), TEXT("TreePepper"), TEXT("PalmCanary") };
	static const TCHAR* Shrubs[] = { TEXT("ShrubChaparral"), TEXT("ShrubChaparral"), TEXT("ShrubChaparral"), TEXT("ShrubOleander"), TEXT("Agave"), TEXT("Yucca"), TEXT("CactusPear"), TEXT("CactusPear") };
	const FVector2D TLo = T.Origin, THi = T.Origin + FVector2D((T.NX - 1) * T.Cell, (T.NY - 1) * T.Cell);
	for (float Y = TLo.Y; Y < THi.Y; Y += 14.f)
		for (float X = TLo.X; X < THi.X; X += 14.f)
		{
			const FVector2D P(X + R.FRand() * 14.f, Y + R.FRand() * 14.f);
			if (T.Wildness(P) < 0.75f) continue;
			const bool bOut = P.X < Lo.X || P.Y < Lo.Y || P.X > Hi.X || P.Y > Hi.Y;
			const float Roll = R.FRand();
			if (!bOut && InBlight(P))
			{
				if (Roll < 0.06f) { Add(Comp(R.FRand() < 0.5f ? TEXT("RockBoulder") : TEXT("RockFlat"), 300.f, false), P, R.FRandRange(0.6f, 1.5f), 0.15f); ++NBig; }
				continue;
			}
			const float PTree = bOut ? 0.05f : 0.02f, PShrub = bOut ? 0.25f : 0.08f, PRock = 0.04f;
			if (Roll < PTree) Add(Comp(Trees[R.RandRange(0, UE_ARRAY_COUNT(Trees) - 1)], 1600.f, false), P, R.FRandRange(0.75f, 1.2f), 0.25f);
			else if (Roll < PTree + PShrub) Add(Comp(Shrubs[R.RandRange(0, UE_ARRAY_COUNT(Shrubs) - 1)], 260.f, false), P, R.FRandRange(0.7f, 1.3f), 0.1f);
			else if (Roll < PTree + PShrub + PRock) Add(Comp(R.FRand() < 0.6f ? TEXT("RockBoulder") : TEXT("RockFlat"), 320.f, false), P, R.FRandRange(0.5f, 1.6f), 0.2f);
			else continue;
			++NBig;
		}
	HLBridge::Log(FString::Printf(TEXT("HL foliage: area %d, %d grass and flowers, %d shrubs, rocks and trees"), Area, NGrass, NBig));
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
	FHLSettings::Get().Load();
	{ int32 Sh = -1; if (FParse::Value(FCommandLine::Get(), TEXT("HLShadows="), Sh)) { FHLSettings::Get().Shadows = FMath::Clamp(Sh, 0, 2); ++FHLSettings::Get().Version; } }
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
		SunLight = Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
		Spawned.Add(Sun);
	}
	// a light distance haze: depth for the streets and the hills; the sky dome (2.6 km out) stays clear of it
	if (AExponentialHeightFog* F = W->SpawnActor<AExponentialHeightFog>(FVector(0, 0, 0), FRotator::ZeroRotator))
	{
		Fog = F->GetComponent();
		Fog->SetMobility(EComponentMobility::Movable);
		Fog->SetFogDensity(0.009f);
		Fog->SetFogHeightFalloff(0.08f);
		Fog->SetStartDistance(3500.f);
		Fog->SetFogMaxOpacity(0.55f);
		Fog->SetFogCutoffDistance(200000.f);
		Spawned.Add(F);
	}
	Terrain.SetNum(Game.Areas.Num());
	for (int32 a = 0; a < Game.Areas.Num(); ++a) Terrain[a].Build(*Game.Areas[a].Def);
	ObjectMeshes.SetNum(Game.Areas.Num());
	ObjectDead.SetNum(Game.Areas.Num());
	ObjectLive.SetNum(Game.Areas.Num());
	FHLEnemyArt& EA = FHLEnemyArt::Get();
	EA.Load();
	for (int32 a = 0; a < Game.Areas.Num(); ++a)
	{
		const FHLAreaDef& Def = *Game.Areas[a].Def;
		AActor* Geo = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
		Spawned.Add(Geo);
		AreaGeo.Add(Geo);
		USceneComponent* Root = NewObject<USceneComponent>(Geo);
		Geo->SetRootComponent(Root); Root->RegisterComponent();
		const FVector2D Ctr = Def.Min + Def.Size * 0.5f;
		// floor (top at z = 0) and sky/dome shell
		// outdoors the floor rolls with the terrain; indoors it is a slab
		const FVector2D FloorLo = Def.Min - FVector2D(FHLTerrain::Margin), FloorHi = Def.Min + Def.Size + FVector2D(FHLTerrain::Margin);
		if (Terrain[a].IsValid())
		{
			UMaterialInterface* FM = GroundMid(Def.FloorKind, Def.Floor);
			TerrainMesh(Geo, a, FloorLo, FloorHi, 0.f, FM ? FM : static_cast<UMaterialInterface*>(Mid(Def.Floor)));
		}
		else
		{
			UStaticMeshComponent* Floor = AddMesh(Geo, Cube, ToWorld(a, Ctr, -0.5f), FVector(Def.Size.X + 400, Def.Size.Y + 400, 1), Def.Floor);
			if (UMaterialInstanceDynamic* G = GroundMid(Def.FloorKind, Def.Floor)) Floor->SetMaterial(0, G);
			Floor->ComponentTags.Add(TEXT("NoShadow"));
		}
		const float SkyR = Def.bDungeon ? 400.f : 2600.f;
		UStaticMeshComponent* SkyC = AddMesh(Geo, Sphere, ToWorld(a, Ctr, 0), FVector(SkyR * 2), Def.Sky, true);
		if (!Def.bDungeon && MatSkyDome)
		{
			UMaterialInstanceDynamic* S = UMaterialInstanceDynamic::Create(MatSkyDome, this);
			SkyC->SetMaterial(0, S); SkyMids.Add(S);
		}
		int32 gi = 0;
		for (const FHLBox& B : Def.Ground)
		{
			// wild ground (blight, grass) follows the hills; made ground stays a flat slab
			if (Terrain[a].IsValid() && (B.Kind == TEXT("Blight") || B.Kind == TEXT("Grass")))
			{
				UMaterialInterface* GM = GroundMid(B.Kind, B.Color);
				TerrainMesh(Geo, a, FVector2D(B.X, B.Y), FVector2D(B.X + B.W, B.Y + B.D), 0.03f + 0.01f * (gi++ % 3), GM ? GM : static_cast<UMaterialInterface*>(Mid(B.Color)));
				continue;
			}
			UStaticMeshComponent* GC = AddMesh(Geo, Cube, ToWorld(a, FVector2D(B.X + B.W * 0.5f, B.Y + B.D * 0.5f), 0.01f + 0.004f * (gi++ % 5)), FVector(B.W, B.D, 0.03f), B.Color);
			if (UMaterialInstanceDynamic* G = GroundMid(B.Kind, B.Color)) GC->SetMaterial(0, G);
			GC->ComponentTags.Add(TEXT("NoShadow"));
		}
		for (const FHLBox& B : Def.Boxes) if (!AddProp(Geo, a, B.Prop)) AddMesh(Geo, Cube, ToWorld(a, FVector2D(B.X + B.W * 0.5f, B.Y + B.D * 0.5f), B.H * 0.5f), FVector(B.W, B.D, B.H), B.Color);
		// building-kit modules (facade bays, pilasters, parapets: hundreds of each) draw as one instanced component per mesh
		TMap<FString, UInstancedStaticMeshComponent*> Kit;
		for (const FHLPropPlace& P : Def.Props)
		{
			// every repeated kit family batches; one-off landmarks (LM_) and signs go through the normal prop path
			static const TCHAR* KitFams[] = { TEXT("SM_BRK_"), TEXT("SM_DECO_"), TEXT("SM_MSN_"), TEXT("SM_SPN_"), TEXT("SM_DR_"), TEXT("SM_RF_"), TEXT("BRK_") };
			bool bKit = false;
			for (const TCHAR* F : KitFams) if (P.Mesh.StartsWith(FString(TEXT("/Game/Buildings/")) + F)) { bKit = true; break; }
			if (!bKit) { AddProp(Geo, a, P); continue; }
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
		ScatterFoliage(Geo, a);
		for (const FHLPortal& Po : Def.Portals) AddMesh(Geo, Cylinder, ToWorld(a, Po.Pos, 1.6f), FVector(Po.R * 1.2f, Po.R * 1.2f, 3.2f), FLinearColor(0.2f, 0.9f, 0.8f), true);
		for (const FHLAidStation& Aid : Def.Aid) AddMesh(Geo, Cube, ToWorld(a, Aid.Pos, 0.05f), FVector(3, 3, 0.1f), FLinearColor(0.9f, 0.1f, 0.1f), true);
		// interactable objects
		for (const FHLObjectDef& O : Def.Objects)
		{
			// hive structures leave a wreck behind instead of vanishing; the egg sac pulses while it lives
			const TCHAR* Struct = O.Kind == TEXT("spire") ? TEXT("HiveSpire") : O.Kind == TEXT("egg_sac") ? TEXT("HiveEggSac")
				: (O.Kind == TEXT("egg") || O.Kind == TEXT("boss_egg") || O.Kind == TEXT("spread_egg")) ? TEXT("HiveEggCluster") : nullptr;
			const FHLEnemyStructure* HS = Struct ? EA.Structure(Struct) : nullptr;
			UStaticMeshComponent* DeadC = nullptr;
			USkeletalMeshComponent* LiveC = nullptr;
			if (HS)
			{
				FHLPropPlace P = O.Prop;
				if (!P.IsSet()) { P.Pos = O.Pos; P.Scale = FVector(1); }
				if (UStaticMesh* DM = EA.StaticMesh(HS->Destroyed))
				{
					DeadC = NewObject<UStaticMeshComponent>(Geo);
					DeadC->SetStaticMesh(DM);
					DeadC->SetMobility(EComponentMobility::Movable);
					DeadC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					if (UMaterialInterface* Mt = EA.Material(HS->Mat)) DeadC->SetMaterial(0, Mt);
					DeadC->SetupAttachment(Geo->GetRootComponent());
					DeadC->SetRelativeLocation(ToWorld(a, P.Pos, P.Z));
					DeadC->SetRelativeRotation(FRotator(0, P.Yaw, 0));
					DeadC->SetRelativeScale3D(P.Scale);
					DeadC->SetVisibility(false);
					DeadC->RegisterComponent();
				}
				if (const FHLEnemyModel* LM = HS->Live.IsEmpty() ? nullptr : EA.Model(HS->Live))
					if (USkeletalMesh* SK = EA.Mesh(*LM))
					{
						LiveC = NewObject<USkeletalMeshComponent>(Geo);
						LiveC->SetMobility(EComponentMobility::Movable);
						LiveC->SetSkeletalMesh(SK);
						if (UMaterialInterface* Mt = EA.Material(FString::Printf(TEXT("/Game/Enemies/%s/MI_%s.MI_%s"), *LM->Id, *LM->Id, *LM->Id))) LiveC->SetMaterial(0, Mt);
						LiveC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						LiveC->SetAnimationMode(EAnimationMode::AnimationSingleNode);
						LiveC->SetupAttachment(Geo->GetRootComponent());
						LiveC->SetRelativeLocation(ToWorld(a, P.Pos, P.Z));
						LiveC->SetRelativeRotation(FRotator(0, P.Yaw, 0));
						LiveC->SetRelativeScale3D(FVector(P.Scale.X));
						LiveC->RegisterComponent();
						if (UAnimSequence* Pulse = EA.Anim(*LM, TEXT("Pulse"))) LiveC->PlayAnimation(Pulse, true);
					}
			}
			ObjectDead[a].Add(DeadC);
			ObjectLive[a].Add(LiveC);
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
	if (U.Kind == EHLKind::Mob && U.Mob && FHLEnemyArt::Get().Make(A, Root, V.Enemy, U.Mob->Id))
	{
		V.bEnemy = true;
		A->SetActorScale3D(FVector(S));
	}
	else if (U.Shape == TEXT("bug"))
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

FLinearColor AHLWorldActor::StageBackColor(int32 Skin)
{
	// skins run Porcelain (0) .. Deep (7): a dark umber curtain for the light ones, a warm parchment for the dark ones
	const float T = FMath::Clamp((Skin - 2.5f) / 4.f, 0.f, 1.f);
	return FMath::Lerp(FLinearColor(0.07f, 0.06f, 0.05f), FLinearColor(0.62f, 0.55f, 0.44f), T);
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
		StageBack = AddMesh(Stage, Cube, FVector(-160, 0, 150), FVector(0.1f, 9.f, 6.f), FLinearColor(0.07f, 0.06f, 0.05f), true);
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
	if (StageSkin != L.App.Skin && StageBack)
	{
		StageSkin = L.App.Skin;
		const FLinearColor Bg = StageBackColor(L.App.Skin);
		StageBack->SetMaterial(0, Mid(Bg, true));
		StageRT->ClearColor = Bg;
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

void AHLWorldActor::ShadowActor(AActor* A, bool bCast)
{
	if (!A) return;
	TInlineComponentArray<UPrimitiveComponent*> Comps(A);
	for (UPrimitiveComponent* P : Comps)
		if (!P->ComponentHasTag(TEXT("NoShadow")) && P->CastShadow != bCast) P->SetCastShadow(bCast);
}

void AHLWorldActor::ApplyGraphics()
{
	// settings window (HLSettings.h): real-time sun/moon shadows, draw distance, render resolution
	const FHLSettings& St = FHLSettings::Get();
	GfxVersion = St.Version;
	auto Set = [](const TCHAR* Name, float V) { if (IConsoleVariable* Cv = IConsoleManager::Get().FindConsoleVariable(Name)) Cv->Set(V, ECVF_SetByGameSetting); };
	Set(TEXT("r.ViewDistanceScale"), St.ViewDist);
	Set(TEXT("r.ScreenPercentage"), (float)St.ResScale);
	if (SunLight)
	{
		SunLight->SetCastShadows(St.Shadows > 0);
		SunLight->SetDynamicShadowDistanceMovableLight(St.Shadows >= 2 ? (St.Quality >= 3 ? 12000.f : 8000.f) : 4500.f);
		SunLight->SetDynamicShadowCascades(St.Shadows >= 2 && St.Quality >= 3 ? 3 : St.Shadows >= 2 ? 2 : 1);
	}
	Set(TEXT("r.Shadow.MaxResolution"), St.Quality >= 2 ? 2048.f : 1024.f);
	// a soft bloom lifts lamps, muzzle flashes and glowing eggs; none on Low
	Camera->PostProcessSettings.BloomIntensity = St.Quality == 0 ? 0.f : St.Quality == 1 ? 0.25f : 0.35f;
	Camera->PostProcessSettings.bOverride_BloomThreshold = true; Camera->PostProcessSettings.BloomThreshold = 1.2f;
	if (Fog) Fog->SetFogMaxOpacity(St.Quality == 0 ? 0.35f : 0.55f);
	{
		// grass and flower cards: the cheapest thing to trim on a slow machine
		const int32 GrassCull = FMath::RoundToInt32((St.Quality == 0 ? 30.f : St.Quality == 1 ? 50.f : St.Quality == 2 ? 70.f : 90.f) * M);
		for (const TWeakObjectPtr<UInstancedStaticMeshComponent>& C : FoliageGrass) if (C.IsValid()) C->SetCullDistances(0, GrassCull);
	}
	for (AActor* A : Spawned) ShadowActor(A, St.Shadows >= 2);
	for (auto& KV : Vis) ShadowActor(KV.Value.Actor.Get(), St.Shadows >= 1);
	HLBridge::Log(FString::Printf(TEXT("HL graphics: quality %d shadows %d view %.2f res %d"), St.Quality, St.Shadows, St.ViewDist, St.ResScale));
}

void AHLWorldActor::TickSky()
{
	// Day/night on the world clock (FHLGame::Hour): one movable sun that becomes a cool moon at night, the sky dome
	// blending its day and night panoramas, a colour grade (golden hour, blue night) and brighter lamps after dark.
	// Dungeons keep a neutral grade.
	const FHLUnit* P = Game.PlayerUnit();
	const bool bIndoors = P && Game.Areas.IsValidIndex(P->Area) && Game.Areas[P->Area].Def->bDungeon;
	const float H = Game.Hour();
	const float D = FHLGame::Daylight(H);
	const float Elev = (H >= 6.f && H <= 19.f) ? FMath::Sin(PI * (H - 6.f) / 13.f) : 0.f;
	const float Gold = D * (1.f - FMath::SmoothStep(0.12f, 0.5f, Elev)); // low sun
	if (SunLight)
	{
		const FRotator SunRot(-FMath::Max(10.f, Elev * 62.f), 180.f - FMath::Clamp((H - 6.f) / 13.f, 0.f, 1.f) * 180.f + 35.f, 0.f);
		const FRotator MoonRot(-42.f, 60.f, 0.f);
		SunLight->SetWorldRotation(D > 0.35f ? SunRot : MoonRot);
		const FLinearColor SunC = FMath::Lerp(FLinearColor(1.f, 0.92f, 0.8f), FLinearColor(1.f, 0.62f, 0.38f), Gold);
		SunLight->SetLightColor(FMath::Lerp(FLinearColor(0.55f, 0.65f, 1.f), SunC, D));
		SunLight->SetIntensity(D > 0.35f ? FMath::Lerp(0.9f, 2.0f, FMath::SmoothStep(0.35f, 1.f, D)) : FMath::Lerp(0.55f, 0.9f, D / 0.35f));
	}
	const FLinearColor SkyTint = FMath::Lerp(FLinearColor(1, 1, 1), FLinearColor(1.08f, 0.78f, 0.66f), Gold);
	for (UMaterialInstanceDynamic* S : SkyMids)
	{
		S->SetScalarParameterValue(TEXT("Night"), 1.f - D);
		S->SetVectorParameterValue(TEXT("Tint"), SkyTint * FMath::Lerp(0.85f, 1.f, D));
	}
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	const float Dg = bIndoors ? 1.f : D, Gg = bIndoors ? 0.f : Gold;
	PP.AutoExposureBias = BaseExposure + FMath::Lerp(-1.25f, 0.f, Dg);
	PP.bOverride_ColorGain = true;
	const FLinearColor Gain = FMath::Lerp(FLinearColor(0.78f, 0.88f, 1.15f), FLinearColor(1, 1, 1), Dg) * FMath::Lerp(FLinearColor(1, 1, 1), FLinearColor(1.06f, 0.97f, 0.88f), Gg);
	PP.ColorGain = FVector4(Gain.R, Gain.G, Gain.B, 1.f);
	PP.bOverride_ColorSaturation = true;
	const float Sat = FMath::Lerp(0.78f, 1.f, Dg) * (1.f + 0.08f * Gg);
	PP.ColorSaturation = FVector4(Sat, Sat, Sat, 1.f);
	// hemisphere ambient (materials, MPC_HLAmbient): a cool sky fill from above and a warm, darker bounce from below,
	// blue and dim at night, warmer at golden hour, neutral indoors
	if (Ambient)
		if (UMaterialParameterCollectionInstance* AI = GetWorld()->GetParameterCollectionInstance(Ambient))
		{
			FLinearColor SkyA = FMath::Lerp(FLinearColor(0.7f, 0.8f, 1.12f), FLinearColor(1.2f, 1.24f, 1.34f), Dg);
			FLinearColor GndA = FMath::Lerp(FLinearColor(0.38f, 0.38f, 0.46f), FLinearColor(0.64f, 0.58f, 0.5f), Dg);
			SkyA = FMath::Lerp(SkyA, FLinearColor(1.3f, 1.12f, 0.98f), Gg * 0.6f);
			if (bIndoors) { SkyA = FLinearColor(1.12f, 1.08f, 1.04f); GndA = FLinearColor(0.7f, 0.65f, 0.6f); }
			AI->SetVectorParameterValue(TEXT("SkyAmb"), SkyA);
			AI->SetVectorParameterValue(TEXT("GroundAmb"), GndA);
		}
	if (Fog)
	{
		Fog->SetVisibility(!bIndoors);
		const FLinearColor Day = FMath::Lerp(FLinearColor(0.55f, 0.62f, 0.74f), FLinearColor(0.85f, 0.6f, 0.42f), Gold);
		Fog->SetFogInscatteringColor(FMath::Lerp(FLinearColor(0.035f, 0.045f, 0.08f), Day, D));
	}
	// lamps carry the night
	static float LastLamp = -1;
	const float Lamp = FMath::Lerp(11000.f, 3500.f, D);
	if (FMath::Abs(Lamp - LastLamp) > 50.f)
	{
		LastLamp = Lamp;
		for (auto& KV : LampLights) if (UPointLightComponent* L = KV.Value.Get()) L->SetIntensity(Lamp);
	}
}

// where the rider sits on each mount kind (HLMountKind), metres from the mount's pivot (bottom centre, front +X):
// the saddle, the motorcycle seat, the driver's seat of the Model A (left-hand drive) and of the police car
const FVector AHLWorldActor::MountSeat[4] = { FVector(-0.1f, 0, 0.85f), FVector(-0.2f, 0, 0.35f), FVector(-0.3f, -0.35f, 0.2f), FVector(-0.3f, -0.35f, 0.2f) };

void AHLWorldActor::SyncUnits()
{
	const FHLUnit* P = Game.PlayerUnit();
	const int32 ViewArea = P ? P->Area : 0;
	const FVector2D ViewPos = P ? P->Pos : Game.Areas[0].Def->Spawn;
	if (GeoShownArea != ViewArea)
	{
		GeoShownArea = ViewArea;
		for (int32 a = 0; a < AreaGeo.Num(); ++a) if (AActor* G = AreaGeo[a].Get()) G->SetActorHiddenInGame(a != ViewArea);
	}
	TSet<int32> Alive;
	for (FHLUnit* U : Game.UnitList)
	{
		const bool bVisible = U->Area == ViewArea && FVector2D::DistSquared(U->Pos, ViewPos) < 170.f * 170.f;
		FHLUnitVis* V = Vis.Find(U->Id);
		if (!bVisible) { if (V && V->Actor.IsValid()) V->Actor->SetActorHiddenInGame(true); if (V) Alive.Add(U->Id); continue; }
		if (!V) { V = &Vis.Add(U->Id); BuildUnit(*U, *V); ShadowActor(V->Actor.Get(), FHLSettings::Get().Shadows >= 1); }
		Alive.Add(U->Id);
		AActor* A = V->Actor.Get();
		if (!A) continue;
		A->SetActorHiddenInGame(false);
		float Z = U->Z + (U->HopT > 0 && !V->bHuman ? FMath::Sin(U->HopT * PI * 2) * 0.5f : 0.f);
		if (U->bSitting && !U->bDead && !V->bHuman && !V->bEnemy) Z -= 0.45f;
		if (U->bMounted) Z += MountSeat[FMath::Min<int32>(U->MountKind, 3)].Z;
		Z += U->Lift;
		// only the back shows, a mound in the street (the rigged burrower's BurrowedIdle sinks itself)
		if (U->bBurrowed && !(V->bEnemy && V->Enemy.Model && V->Enemy.Model->Has(TEXT("BurrowedIdle")))) Z -= 0.85f * U->Scale;
		A->SetActorLocationAndRotation(ToWorld(U->Area, U->Pos, Z), FRotator(0, U->Yaw + U->FaceOfs, U->bDead && !V->bHuman && !V->bEnemy ? 90 : 0));
		if (V->bEnemy) FHLEnemyArt::Get().Drive(V->Enemy, *U, Game.Now, Game.FrameDt);
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
		if ((U->bDead != V->bDeadLook || U->bGhost != V->bGhostLook) && !V->bHuman && !V->bEnemy)
		{
			V->bDeadLook = U->bDead; V->bGhostLook = U->bGhost;
			if (V->Body.IsValid()) V->Body->SetMaterial(0, Mid(U->bDead ? U->Color * 0.45f : U->bGhost ? FLinearColor(0.55f, 0.75f, 1.f) : U->Color));
		}
		if (U->bMounted != V->bMountLook)
		{
			V->bMountLook = U->bMounted;
			// the mount art (art/world mounts) under the seated rider: the actor sits at the seat, the mount hangs below it
			const int32 K = FMath::Min<int32>(U->MountKind, 3);
			if (U->bMounted && !V->Mount.IsValid())
			{
				static const TCHAR* MountMesh[] = { TEXT("/Game/Props/SM_MountHorse.SM_MountHorse"), TEXT("/Game/Props/SM_MountScout.SM_MountScout"),
					TEXT("/Game/Props/SM_MountModelA.SM_MountModelA"), TEXT("/Game/Props/SM_MountPolice.SM_MountPolice") };
				FHLPropPlace MP; MP.Mesh = MountMesh[K]; MP.Scale = FVector(1);
				if (UStaticMeshComponent* C = AddProp(A, U->Area, MP))
				{
					C->SetUsingAbsoluteLocation(false); C->SetUsingAbsoluteRotation(false);
					C->SetRelativeLocation(-MountSeat[K] * M);
					C->SetRelativeRotation(FRotator::ZeroRotator);
					C->SetCastShadow(true);
					V->Mount = C;
				}
				else V->Mount = AddMesh(A, Cube, FVector(0, 0, -45), FVector(2.2f, 0.8f, 0.8f), FLinearColor(0.12f, 0.12f, 0.14f));
			}
			if (V->Mount.IsValid()) V->Mount->SetVisibility(U->bMounted);
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
				USkeletalMeshComponent* Live = o < ObjectLive[a].Num() ? ObjectLive[a][o].Get() : nullptr;
				if (Live) { if (Live->IsVisible() != bShow) Live->SetVisibility(bShow); if (C->IsVisible()) C->SetVisibility(false); }
				else if (C->IsVisible() != bShow) C->SetVisibility(bShow);
				if (UStaticMeshComponent* Dead = o < ObjectDead[a].Num() ? ObjectDead[a][o].Get() : nullptr) if (Dead->IsVisible() == bShow) Dead->SetVisibility(!bShow);
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
	// -HLTimeScale=N (self-play tests only): N bot + simulation steps per frame, for long soak runs (dungeon, pacing)
	static int32 TimeScale = -1;
	if (TimeScale < 0) { TimeScale = 1; if (!AutoClass.IsEmpty()) FParse::Value(FCommandLine::Get(), TEXT("HLTimeScale="), TimeScale); TimeScale = FMath::Clamp(TimeScale, 1, 16); }
	for (int32 Step = 0; Step < TimeScale; ++Step)
	{
		if (!AutoClass.IsEmpty()) AutoTest(Dt);
		Game.Tick(Dt);
	}
	Game.Music.Tick(Game, Dt);
	CharVis.Tick(Game.Now);
	SyncUnits();
	TickStage();
	TickSky();
	if (FHLSettings::Get().Version != GfxVersion) ApplyGraphics();
	// rigs rebuild parts when gear changes (and those start unshadowed): keep the people casting
	if (FHLSettings::Get().Shadows >= 1 && FPlatformTime::Seconds() > NextShadowSweep)
	{
		NextShadowSweep = FPlatformTime::Seconds() + 2.0;
		for (auto& KV : Vis) if (KV.Value.Actor.IsValid() && !KV.Value.Actor->IsHidden()) ShadowActor(KV.Value.Actor.Get(), true);
	}
	TickFx(Dt);
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
	{
		// stay above the hills (the ground under the camera, plus a little)
		const int32 CA = P ? P->Area : 0;
		const FVector2D Off = Game.Areas.IsValidIndex(CA) ? Game.Areas[CA].Def->Offset : FVector2D::ZeroVector;
		Loc.Z = FMath::Max(Loc.Z, (GroundZ(CA, FVector2D(Loc.X / M, Loc.Y / M) - Off) + 0.6f) * M);
	}
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
		// -HLViewSet=nature: the open ground, the hills and the sign, and the Egyptian Theatre's dressing
		struct FView { float X, Y, Yaw, Pitch, Dist; int32 Area = 0; };
		static const FView Fronts[] = { { 470, 204, -90, 8, 24 }, { 385, 204, -90, 8, 22 }, { 580, 204, -90, 14, 30 }, { 640, 196, 90, 8, 24 }, { 230, 196, 90, 8, 26 }, { 781, 200, 90, 6, 12 }, { 120, 205, -120, 18, 40 } };
		static const FView Nature[] = { { 298, 150, -90, 3, 14 }, { 300, -60, -90, 8, 26 }, { 420, -10, 20, 14, 22 }, { 700, 360, 45, 12, 25 }, { -150, 350, 180, 6, 30 },
			{ 60, 12, 90, 12, 18, 1 }, { 60, 150, 90, 10, 26, 1 }, { 60, 222, 90, 6, 22, 1 } };
		static FString Set; static bool bSet = FParse::Value(FCommandLine::Get(), TEXT("HLViewSet="), Set);
		// -HLViewSet=landmarks: the building kit's hub landmarks (art/buildings LM_*)
		static const FView Landmarks[] = { { 93, 200, -90, 12, 42 }, { 228, 200, -90, 14, 48 }, { 318, 204, -70, 10, 34 }, { 718, 200, -90, 14, 42 },
			{ 97, 200, 90, 14, 48 }, { 475, 200, 90, 12, 42 }, { 530, 257, 60, 18, 30 }, { 790, 200, 90, 16, 48 }, { 758, 283, 0, 12, 30 }, { 228, 92, 90, 28, 52 } };
		const bool bNature = bSet && Set == TEXT("nature"), bLandmarks = bSet && Set == TEXT("landmarks");
		const FView* Views = bNature ? Nature : bLandmarks ? Landmarks : Fronts;
		const int32 NViews = bNature ? UE_ARRAY_COUNT(Nature) : bLandmarks ? UE_ARRAY_COUNT(Landmarks) : UE_ARRAY_COUNT(Fronts);
		static int32 Step = -1; static double NextStep = 0;
		AutoNextShot = 1e9;
		P->CombatUntil = 0; P->bAutoAttack = false; P->TargetId = 0;
		if (T >= NextStep)
		{
			if (++Step >= NViews) { UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false); return; }
			const FView& V = Views[Step];
			Game.TeleportUnit(*P, V.Area, FVector2D(V.X, V.Y));
			NextStep = T + 2.5;
		}
		const FView& V = Views[FMath::Max(0, Step)];
		CamYaw = V.Yaw; CamPitch = V.Pitch; CamDist = V.Dist; P->Yaw = V.Yaw; P->Pos = FVector2D(V.X, V.Y); // pinned: the self-play bot keeps walking
#if !PLATFORM_WASM
		static int32 Shot = -1;
		if (Shot != Step && T > NextStep - 0.6) { Shot = Step; FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/hl_view_%02d.png"), Step), true, false); }
#endif
	}
	if (P && FParse::Param(FCommandLine::Get(), TEXT("HLEnemyLineup")))
	{
		// enemy art check: each rigged model in its idle beside the player (both facing +X), seen from the side, one
		// screenshot each (hl_enemy_NN.png); then the walk clip a step later
		static TArray<FString> MobIds; static int32 Step = -1; static double NextStep = 0; static TWeakObjectPtr<AActor> Show; static FHLEnemyRig Rig;
		FHLEnemyArt& EA = FHLEnemyArt::Get();
		if (MobIds.IsEmpty() && EA.Load())
		{
			TSet<FString> Seen;
			for (const FHLMobDef& Md : FHLData::Get().Mobs) if (const FHLEnemyMob* E = EA.Mob(Md.Id)) if (!Seen.Contains(E->Model)) { Seen.Add(E->Model); MobIds.Add(Md.Id); }
		}
		AutoNextShot = 1e9;
		P->CombatUntil = 0; P->bAutoAttack = false; P->TargetId = 0;
		const FVector2D At(250, 300);
		P->Pos = At; P->Yaw = 0;
		CamYaw = 90; CamPitch = 8; CamDist = 9;
		if (T >= NextStep)
		{
			if (++Step >= MobIds.Num() * 2) { UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false); return; }
			NextStep = T + 2;
			if (Show.IsValid()) Show->Destroy();
			const FHLMobDef* LM = FHLData::Get().Mob(MobIds[Step / 2]);
			AActor* A = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
			USceneComponent* R = NewObject<USceneComponent>(A); A->SetRootComponent(R); R->RegisterComponent();
			Rig = FHLEnemyRig();
			EA.Make(A, R, Rig, LM->Id);
			A->SetActorLocationAndRotation(ToWorld(P->Area, At + FVector2D(0, -3.5f), 0), FRotator::ZeroRotator);
			A->SetActorScale3D(FVector(FMath::Min(LM->Scale, 1.6f)));
			if (Rig.Model) EA.Play(Rig, Rig.Model->Role(Step % 2 ? TEXT("walk") : TEXT("idle")), true);
			Show = A;
			HLBridge::Log(FString::Printf(TEXT("HL lineup %d %s (%s)"), Step, *LM->Id, Rig.Model ? *Rig.Model->Id : TEXT("none")));
		}
#if !PLATFORM_WASM
		static int32 Shot = -1;
		if (Shot != Step && T > NextStep - 0.5) { Shot = Step; FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / FString::Printf(TEXT("Screenshots/hl_enemy_%02d.png"), Step), true, false); }
#endif
		return;
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
	// -HLTradeTest: carry low-level gear others want; take the first ask, ask double (expect a counter), then settle
	if (Game.bTradeTest)
	{
		static bool bStocked = false;
		static int32 Step = 0;
		if (!bStocked)
		{
			bStocked = true;
			for (const FHLItem& It : FHLData::Get().Items) if (It.IsEquip() && It.Req >= 1 && It.Req <= 6 && It.Q == 2 && Game.Player.Bag.FilterByPredicate([](const FHLInvSlot& S) { return S.Item != nullptr; }).Num() < 10) Game.AddItem(&It, 1, false);
		}
		if (Game.TradeFromSim >= 0 && Game.bTradeAskShown)
		{
			UE_LOG(LogTemp, Display, TEXT("HL_TRADE ask from %s: %s for %s"), *Game.Sims[Game.TradeFromSim].P->Handle, *Game.Player.Bag[Game.TradeWantBag].Item->Name, *HLMoney(Game.TradeWantOffer));
			Game.AnswerTradeRequest(true); Step = 0;
		}
		if (Game.TradeSim >= 0)
		{
			if (Step == 0) { Game.TradeTheirCents *= 2; Game.AcceptTrade(); UE_LOG(LogTemp, Display, TEXT("HL_TRADE asked double: mood %d \"%s\" gap %s"), Game.TradeMoodKind, *Game.TradeMood, *HLMoney(Game.TradeGap())); Step = 1; }
			else if (Step == 1) { Game.TradeTheirCents /= 2; const int32 Before = Game.Player.Cents; Game.AcceptTrade(); UE_LOG(LogTemp, Display, TEXT("HL_TRADE settled: open %d, money %d -> %d"), Game.TradeSim, Before, Game.Player.Cents); Step = 2; }
		}
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
	{
		// bot state every 20 s (why a soak run stalls)
		static double NextBotLog = 0;
		static FVector2D LastLogPos(1e9, 1e9);
		if (Game.Now > NextBotLog)
		{
			NextBotLog = Game.Now + 20;
			FString Party;
			for (int32 SI : Game.Party) if (const FHLUnit* Mb = Game.Unit(Game.Sims[SI].UnitId)) Party += FString::Printf(TEXT(" %s:%s%.0f/%.0f%s"), *Mb->Name, Mb->bDead ? TEXT("DEAD ") : TEXT(""), Mb->HpPct() * 100, Mb->MaxRes > 0 ? Mb->Res * 100 / Mb->MaxRes : 0.f, Mb->ResType == EHLRes::Power ? TEXT("p") : TEXT(""));
			UE_LOG(LogTemp, Display, TEXT("HL_BOT t=%.0f at %.0f,%.0f ghost=%d sit=%d target=%s d=%.0f goal=%d party:%s"), Game.Now, P->Pos.X, P->Pos.Y, P->bGhost, P->bSitting,
					Tg ? *Tg->Name : TEXT("-"), Tg ? Game.Dist(*P, *Tg) : 0.f, bAutoGoal ? 1 : 0, *Party);
				if (!Tg && FVector2D::Distance(LastLogPos, P->Pos) < 1) // stalled without a target: what is the bot walking to?
				{
					const FHLUnit* Far = nullptr; float FD = 1e9f;
					for (const FHLUnit* U : Game.UnitList) if (U->Kind == EHLKind::Mob && !U->bDead && U->Area == P->Area && !U->Mob->bGuard && (U->TappedBy == 0 || U->TappedBy == Game.PlayerUnitId) && Game.Dist(*P, *U) < FD) { Far = U; FD = Game.Dist(*P, *U); }
					if (Far) UE_LOG(LogTemp, Display, TEXT("HL_BOT   stalled: nearest %s lvl %d d=%.0f at %.0f,%.0f z=%.1f path=%d/%d"), *Far->Name, Far->Level, FD, Far->Pos.X, Far->Pos.Y, Far->Z, P->PathIdx, P->Path.Num());
				}
				LastLogPos = P->Pos;
				if (!Tg) for (const FHLUnit* B : Game.UnitList) if (B->Kind == EHLKind::Mob && !B->bDead && B->Area == P->Area && B->Mob->Rank == TEXT("boss"))
					UE_LOG(LogTemp, Display, TEXT("HL_BOT   boss %s state=%d tapped=%d(me %d) threat=%d hp=%.0f at %.0f,%.0f"), *B->Name, (int32)B->MobState, B->TappedBy, Game.PlayerUnitId, B->Threat.Num(), B->HpPct() * 100, B->Pos.X, B->Pos.Y);
		}
	}
	if (!Tg || Tg->bDead || Tg->Kind != EHLKind::Mob)
	{
		for (FHLUnit* U : Game.UnitList) if (U->Kind == EHLKind::Mob && U->bDead && U->bLootable && U->TappedBy == Game.PlayerUnitId && Game.Dist(*P, *U) < 6) Game.LootCorpse(*U);
		if (P->HpPct() < 0.5f && !Game.InCombat(*P)) { P->bSitting = true; return; }
		FHLUnit* Best = nullptr; float BestD = 1e9f;
		const int32 MaxLvl = P->Area == 0 ? P->Level + 1 : 99;
		if (P->Area != 0) { bool bRest = false; for (int32 SI : Game.Party) if (const FHLUnit* Mb = Game.Unit(Game.Sims[SI].UnitId)) if (Mb->bDead || Mb->HpPct() < 0.6f || (Mb->ResType == EHLRes::Power && Mb->Res < Mb->MaxRes * 0.4f)) bRest = true; if (bRest && !Game.InCombat(*P)) { P->bSitting = true; return; } }
		for (FHLUnit* U : Game.UnitList) if (U->Kind == EHLKind::Mob && !U->bDead && U->Area == P->Area && U->Level <= MaxLvl && !U->Mob->bGuard && (U->TappedBy == 0 || U->TappedBy == Game.PlayerUnitId) && Game.Dist(*P, *U) < BestD) { Best = U; BestD = Game.Dist(*P, *U); }
		// targets drop past 90 m (TickPlayer): walk toward a far one first
		if (Best && BestD > 70) { AutoGoal = Best->Pos; bAutoGoal = true; }
		else if (Best) Game.TargetUnit(Best->Id);
		return;
	}
		// boss mechanics a player handles: burn the egg sac / egg clusters tied to the boss being fought
		if (Tg->Mob && Tg->Mob->Rank == TEXT("boss") && Game.InCombat(*Tg))
		{
			FHLAreaState& Ar = Game.Areas[P->Area];
			int32 Best = -1; float BestD = 1e9f;
			for (int32 i = 0; i < Ar.Objects.Num(); ++i) if (Ar.Objects[i].bActive && Ar.Objects[i].Def->Boss == Tg->Mob->Id && FVector2D::Distance(P->Pos, Ar.Objects[i].Def->Pos) < BestD) { Best = i; BestD = FVector2D::Distance(P->Pos, Ar.Objects[i].Def->Pos); }
			if (Best >= 0)
			{
				if (Game.InteractObj == Best && Game.InteractArea == P->Area && Game.Now < Game.InteractEnd) return; // channelling
				const FVector2D At = Ar.Objects[Best].Def->Pos;
				if (BestD > 3.5f) { P->bAutoAttack = false; Game.MoveTo(*P, At, 3); return; }
				Game.ObjectInteract(P->Area, Best);
				return;
			}
		}
		P->bAutoAttack = true;
		CamYaw = P->Yaw;
		for (int32 i = 0; i < Game.Player.Bar.Num(); ++i) if (Game.Player.Bar[i] && Game.Player.Bar[i]->Target == TEXT("enemy") && Game.CanCast(*P, *Game.Player.Bar[i], Tg).IsEmpty()) { Game.TryCast(*P, *Game.Player.Bar[i], Tg->Id); break; }
	}
