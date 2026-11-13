#include "HLCharacter.h"
#include "HLGame.h"
#include "HLBridge.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

const TCHAR* FHLChars::SlotName[FHLChars::NumSlots] = { TEXT("shirt"), TEXT("trousers"), TEXT("shoes"), TEXT("coat"), TEXT("wrist"), TEXT("gloves"), TEXT("belt") };
static const TCHAR* SexName[2] = { TEXT("Male"), TEXT("Female") };
static const FString CharBase = TEXT("/Game/Characters");

static uint32 Mix(uint32 H, uint32 V) { return HashCombineFast(H, V); }

FHLAppearance FHLAppearance::FromSeed(uint32 Seed, int32 InSex)
{
	FRandomStream R(int32(Seed ^ 0x61c88647u));
	FHLAppearance A;
	A.Sex = InSex >= 0 ? InSex : (R.FRand() < 0.42f ? 1 : 0);
	A.Skin = R.RandRange(0, 7);
	A.Face = R.RandRange(0, 11);
	A.Hair = R.RandRange(0, 11);
	// hair colours weighted toward the common browns
	static const int32 Weights[8] = { 14, 22, 18, 8, 5, 12, 3, 6 };
	int32 Pick = R.RandRange(0, 87), Acc = 0;
	for (int32 i = 0; i < 8; ++i) { Acc += Weights[i]; if (Pick < Acc) { A.HairColor = i; break; } }
	A.Outfit = R.RandRange(0, 7);
	return A;
}

uint32 FHLCharLook::TexKey() const
{
	uint32 H = Mix(Mix(Mix(App.Sex, App.Skin), Mix(App.Face, App.Hair)), Mix(App.HairColor, App.Outfit));
	for (const FString& S : Slots) H = Mix(H, GetTypeHash(S));
	return H;
}

uint32 FHLCharLook::MeshKey() const
{
	uint32 H = Mix(Mix(App.Sex, App.Hair), GetTypeHash(Hat));
		for (const FString& S : Slots) H = Mix(H, GetTypeHash(S)); // geosets come from the slot looks
		return H;
	}

	uint32 FHLCharLook::HeldKey() const
	{
		return Mix(Mix(GetTypeHash(Main), GetTypeHash(Off)), Mix(GetTypeHash(Ranged), (uint32)bRangedOut));
	}

// ------------------------------------------------------------------ data

FHLChars& FHLChars::Get() { static FHLChars Inst; return Inst; }

const TArray<FLinearColor>& FHLChars::HairColors()
{
	static const TArray<FLinearColor> C = {
		FLinearColor::FromSRGBColor(FColor(0x1c, 0x16, 0x12)), FLinearColor::FromSRGBColor(FColor(0x3b, 0x2a, 0x1e)), FLinearColor::FromSRGBColor(FColor(0x6a, 0x3f, 0x22)),
		FLinearColor::FromSRGBColor(FColor(0x8a, 0x3a, 0x1c)), FLinearColor::FromSRGBColor(FColor(0xb0, 0x54, 0x2a)), FLinearColor::FromSRGBColor(FColor(0xb8, 0x93, 0x5a)),
		FLinearColor::FromSRGBColor(FColor(0xd8, 0xcf, 0xb6)), FLinearColor::FromSRGBColor(FColor(0x8c, 0x87, 0x80)) };
	return C;
}
const TArray<FString>& FHLChars::HairColorNames()
{
	static const TArray<FString> N = { TEXT("Black"), TEXT("Dark brown"), TEXT("Chestnut"), TEXT("Auburn"), TEXT("Copper"), TEXT("Honey blonde"), TEXT("Platinum"), TEXT("Grey") };
	return N;
}
const TArray<FString>& FHLChars::SkinNames()
{
	static const TArray<FString> N = { TEXT("Porcelain"), TEXT("Fair"), TEXT("Light"), TEXT("Olive"), TEXT("Tan"), TEXT("Bronze"), TEXT("Brown"), TEXT("Deep") };
	return N;
}
FLinearColor FHLChars::HairTint(int32 Index)
{
	// the hair paint is grayscale with mean ~0.55 sRGB (0.26 linear): scale so the average lands on the chosen colour
	const FLinearColor C = HairColors()[FMath::Clamp(Index, 0, HairColors().Num() - 1)];
	const float K = 1.f / 0.26f;
	return FLinearColor(FMath::Min(C.R * K, 4.f), FMath::Min(C.G * K, 4.f), FMath::Min(C.B * K, 4.f), 1);
}

static FHLChars::FLayer ParseLayer(const TSharedPtr<FJsonObject>& O)
{
	FHLChars::FLayer L;
	if (!O.IsValid()) return L;
	O->TryGetStringField(TEXT("tex"), L.Tex);
	O->TryGetStringField(TEXT("mask"), L.Mask);
	const TArray<TSharedPtr<FJsonValue>>* R;
	if (O->TryGetArrayField(TEXT("rect"), R) && R->Num() == 4) L.Rect = FVector4f((*R)[0]->AsNumber(), (*R)[1]->AsNumber(), (*R)[2]->AsNumber(), (*R)[3]->AsNumber());
	const TArray<TSharedPtr<FJsonValue>>* G;
	if (O->TryGetArrayField(TEXT("geosets"), G)) for (const TSharedPtr<FJsonValue>& V : *G) L.Geosets.Add(V->AsString());
	L.bValid = !L.Tex.IsEmpty() || L.Geosets.Num() > 0;
	return L;
}

bool FHLChars::Ready()
{
	if (bLoaded) return bOk;
	bLoaded = true;
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/characters.json")))) { HLBridge::Log(TEXT("HL characters.json missing: gray-box people")); return false; }
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) return false;
	const TSharedPtr<FJsonObject>* Sx;
	if (Root->TryGetObjectField(TEXT("sexes"), Sx))
		for (int32 s = 0; s < 2; ++s)
		{
			const TSharedPtr<FJsonObject>* O;
			if (!(*Sx)->TryGetObjectField(SexName[s], O)) continue;
			FSex& X = Sexes[s];
			for (const TSharedPtr<FJsonValue>& V : (*O)->GetArrayField(TEXT("skins"))) X.Skins.Add(V->AsString());
			X.Underwear = ParseLayer((*O)->GetObjectField(TEXT("underwear")));
			for (const TSharedPtr<FJsonValue>& V : (*O)->GetArrayField(TEXT("faces"))) X.Faces.Add(ParseLayer(V->AsObject()));
			for (const TSharedPtr<FJsonValue>& V : (*O)->GetArrayField(TEXT("defaults"))) X.Defaults.Add(V->AsString());
			X.HairTex = (*O)->GetStringField(TEXT("hair_tex"));
			for (const TSharedPtr<FJsonValue>& V : (*O)->GetArrayField(TEXT("hair")))
			{
				const TSharedPtr<FJsonObject> H = V->AsObject();
				FHair Hr; H->TryGetStringField(TEXT("name"), Hr.Name); H->TryGetStringField(TEXT("mesh"), Hr.Mesh); H->TryGetStringField(TEXT("top"), Hr.Top);
				Hr.Scalp = ParseLayer(H->GetObjectField(TEXT("scalp")));
				X.Hair.Add(Hr);
			}
		}
	const TSharedPtr<FJsonObject>* Anchor;
	if (Root->TryGetObjectField(TEXT("hat_anchor"), Anchor))
		for (int32 s = 0; s < 2; ++s)
		{
			const TArray<TSharedPtr<FJsonValue>>* A;
			if ((*Anchor)->TryGetArrayField(SexName[s], A) && A->Num() == 3) Sexes[s].HatAnchor = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		}
	const TSharedPtr<FJsonObject>* Ht;
	if (Root->TryGetObjectField(TEXT("height"), Ht)) for (int32 s = 0; s < 2; ++s) (*Ht)->TryGetNumberField(SexName[s], Sexes[s].Height);
	const TSharedPtr<FJsonObject>* Lk;
	if (Root->TryGetObjectField(TEXT("looks"), Lk))
		for (const auto& KV : (*Lk)->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
			FLook& L = Looks.Add(FString(KV.Key));
			O->TryGetStringField(TEXT("name"), L.Name); O->TryGetStringField(TEXT("type"), L.Type); O->TryGetNumberField(TEXT("tier"), L.Tier);
			const TSharedPtr<FJsonObject>* Slots;
			if (O->TryGetObjectField(TEXT("slots"), Slots))
				for (int32 i = 0; i < NumSlots; ++i)
				{
					const TSharedPtr<FJsonObject>* Per;
					if (!(*Slots)->TryGetObjectField(SlotName[i], Per)) continue;
					for (int32 s = 0; s < 2; ++s) { const TSharedPtr<FJsonObject>* E; if ((*Per)->TryGetObjectField(SexName[s], E)) L.Slot[s][i] = ParseLayer(*E); }
				}
			const TSharedPtr<FJsonObject>* Hat;
			if (O->TryGetObjectField(TEXT("hat"), Hat))
				for (int32 s = 0; s < 2; ++s)
				{
					const TSharedPtr<FJsonObject>* E;
					if (!(*Hat)->TryGetObjectField(SexName[s], E)) continue;
					(*E)->TryGetStringField(TEXT("mesh"), L.HatMesh[s]); (*E)->TryGetStringField(TEXT("tex"), L.HatTex[s]); (*E)->TryGetBoolField(TEXT("hides_top"), L.bHidesTop[s]);
				}
		}
	const TSharedPtr<FJsonObject>* Hd;
	if (Root->TryGetObjectField(TEXT("held"), Hd))
		for (const auto& KV : (*Hd)->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
			FHeld& H = Held.Add(FString(KV.Key));
			H.Id = FString(KV.Key); O->TryGetStringField(TEXT("name"), H.Name); O->TryGetStringField(TEXT("type"), H.Type); O->TryGetStringField(TEXT("mesh"), H.Mesh); O->TryGetStringField(TEXT("tex"), H.Tex);
			H.bLeft = O->GetStringField(TEXT("hand")) == TEXT("left");
			const TArray<TSharedPtr<FJsonValue>>* G = nullptr;
			if (O->TryGetArrayField(TEXT("grip_l"), G) && G->Num() == 3) { H.GripL = FVector((*G)[0]->AsNumber(), (*G)[1]->AsNumber(), (*G)[2]->AsNumber()); H.bGripL = !H.GripL.IsNearlyZero(); }
			double Roll = 0; if (O->TryGetNumberField(TEXT("roll"), Roll)) H.Roll = Roll;
		}
	const TArray<TSharedPtr<FJsonValue>>* Sw;
	if (Root->TryGetArrayField(TEXT("starter_colours"), Sw))
		for (const TSharedPtr<FJsonValue>& V : *Sw)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			OutfitA.Add(FLinearColor::FromSRGBColor(FColor::FromHex(O->GetStringField(TEXT("primary")))));
			OutfitB.Add(FLinearColor::FromSRGBColor(FColor::FromHex(O->GetStringField(TEXT("secondary")))));
		}
	bOk = Sexes[0].Skins.Num() > 0 && Sexes[1].Skins.Num() > 0 && Looks.Contains(TEXT("Starter")) && Mat(TEXT("M_HLChar")) != nullptr;
	HLBridge::Log(FString::Printf(TEXT("HL characters: %s (%d looks, %d held items)"), bOk ? TEXT("ready") : TEXT("assets missing"), Looks.Num(), Held.Num()));
	return bOk;
}

// item -> look: armour type letter + tier from the item's level band; rares one tier up; epics to the eight epic looks
FString FHLChars::LookForItem(const FHLItem* It) const
{
	if (!It || It->Kind != TEXT("armor")) return FString();
	const TCHAR* Letter = It->Type == TEXT("cloth") ? TEXT("C") : It->Type == TEXT("leather") ? TEXT("L") : It->Type == TEXT("padded") ? TEXT("P") : It->Type == TEXT("steel") ? TEXT("S") : nullptr;
	if (!Letter) return FString();
	const uint32 H = GetTypeHash(It->Id);
	if (It->Q >= 4)
	{
		if (It->Type == TEXT("steel")) return TEXT("E1");
		if (It->Type == TEXT("leather")) return H % 2 ? TEXT("E2") : TEXT("E3");
		if (It->Type == TEXT("padded")) { static const TCHAR* P[3] = { TEXT("E4"), TEXT("E5"), TEXT("E8") }; return P[H % 3]; }
		return H % 2 ? TEXT("E6") : TEXT("E7");
	}
	const int32 Band = It->Req <= 1 ? 0 : FMath::Min(12, It->Req / 5);
	int32 Tier = 1 + Band * 6 / 13;
	if (It->Q == 3) Tier = FMath::Min(6, Tier + 1);
	return FString::Printf(TEXT("%s%d"), Letter, Tier);
}

FString FHLChars::HeldForItem(const FHLItem* It) const
{
	if (!It || It->Kind != TEXT("weapon")) return FString();
	FString Best;
	int32 BestLen = 0;
	for (const auto& KV : Held) if (KV.Value.Name.Len() > BestLen && It->Name.Contains(KV.Value.Name)) { Best = KV.Key; BestLen = KV.Value.Name.Len(); }
	if (!Best.IsEmpty()) return Best;
	// named items: a model of the same kind, stable per item
	TArray<FString> Same;
	for (const auto& KV : Held) if (KV.Value.Type == It->Type) Same.Add(KV.Key);
	Same.Sort();
	return Same.Num() ? Same[GetTypeHash(It->Id) % Same.Num()] : FString();
}

FHLCharLook FHLChars::LookFromGear(const FHLAppearance& App, const TMap<FString, const FHLItem*>& Gear, bool bRangedOut) const
{
	FHLCharLook L;
	L.App = App;
	auto Item = [&Gear](const TCHAR* S) { const FHLItem* const* P = Gear.Find(S); return P ? *P : nullptr; };
	const FString Coat = LookForItem(Item(TEXT("coat")));
	L.Slots[0] = Coat.IsEmpty() ? TEXT("Starter") : Coat; // shirt
	L.Slots[3] = Coat;                                     // coat
	const FString Tr = LookForItem(Item(TEXT("trousers"))), Sh = LookForItem(Item(TEXT("shoes")));
	L.Slots[1] = Tr.IsEmpty() ? TEXT("Starter") : Tr;
	L.Slots[2] = Sh.IsEmpty() ? TEXT("Starter") : Sh;
	L.Slots[4] = LookForItem(Item(TEXT("wrist")));
	L.Slots[5] = LookForItem(Item(TEXT("gloves")));
	L.Slots[6] = LookForItem(Item(TEXT("belt")));
	L.Hat = LookForItem(Item(TEXT("hat")));
	const FHLItem* Main = Item(TEXT("main"));
	const FHLItem* Ranged = Item(TEXT("ranged"));
	L.Main = HeldForItem(Main); L.Off = HeldForItem(Item(TEXT("off"))); L.Ranged = HeldForItem(Ranged);
	L.bRangedOut = bRangedOut && Ranged != nullptr;
	if (L.bRangedOut) L.Stance = (Ranged->Type == TEXT("revolver") || Ranged->Type == TEXT("pistol")) ? EHLStance::Pistol : EHLStance::Rifle;
	else if (!Main || Main->Type == TEXT("fist")) L.Stance = EHLStance::Unarmed;
	else L.Stance = Main->Hands >= 2 ? EHLStance::TwoHand : EHLStance::OneHand;
	return L;
}

static FHLCharLook NpcLook(const FHLChars& C, const FHLNpcDef& N)
{
	static const TMap<FString, FString> Outfit = {
		{ TEXT("npc_sid_grauman"), TEXT("C4") }, { TEXT("npc_max_factor"), TEXT("C3") }, { TEXT("npc_trainer_veteran"), TEXT("S1") }, { TEXT("npc_trainer_stuntman"), TEXT("L4") },
		{ TEXT("npc_trainer_gunman"), TEXT("L3") }, { TEXT("npc_trainer_electrician"), TEXT("P2") }, { TEXT("npc_trainer_powderman"), TEXT("P5") }, { TEXT("npc_trainer_nurse"), TEXT("E6") },
		{ TEXT("npc_trainer_preacher"), TEXT("E7") }, { TEXT("npc_trainer_mechanic"), TEXT("P1") }, { TEXT("npc_newsie"), TEXT("C1") }, { TEXT("npc_roosevelt_clerk"), TEXT("C2") },
		{ TEXT("npc_teller"), TEXT("C3") }, { TEXT("npc_conductor"), TEXT("C2") }, { TEXT("npc_gunsmith"), TEXT("L2") }, { TEXT("npc_haberdasher"), TEXT("C3") },
		{ TEXT("npc_auctioneer"), TEXT("C4") }, { TEXT("npc_auctioneer2"), TEXT("C3") }, { TEXT("npc_salvage_trainer"), TEXT("P1") }, { TEXT("npc_derby_cook"), TEXT("C1") },
		{ TEXT("npc_druggist"), TEXT("C1") }, { TEXT("npc_pantages_manager"), TEXT("C3") }, { TEXT("npc_taft_doorman"), TEXT("C5") }, { TEXT("npc_stagehand"), TEXT("E8") },
		{ TEXT("npc_chief_davis"), TEXT("C3") }, { TEXT("npc_capt_hynes"), TEXT("S2") }, { TEXT("npc_kearney"), TEXT("S1") } };
	const bool bWoman = N.Name.StartsWith(TEXT("Miss ")) || N.Name.StartsWith(TEXT("Mrs. ")) || N.Name.StartsWith(TEXT("Annie ")) || N.Name.StartsWith(TEXT("Edith ")) || N.Name.StartsWith(TEXT("Mary ")) || N.Name.StartsWith(TEXT("Sister "));
	FHLCharLook L;
	L.App = FHLAppearance::FromSeed(GetTypeHash(N.Id), bWoman ? 1 : 0);
	const uint32 H = GetTypeHash(N.Id);
	FString Look = Outfit.FindRef(N.Id);
	if (Look.IsEmpty()) Look = N.HasRole(TEXT("guard")) ? TEXT("S1") : FString::Printf(TEXT("C%d"), 1 + H % 4);
	for (int32 i = 0; i < FHLChars::NumSlots; ++i) L.Slots[i] = Look;
	if (C.Looks.Contains(Look) && (Look.StartsWith(TEXT("S")) || H % 3 != 0)) L.Hat = Look;
	if (N.HasRole(TEXT("guard"))) { L.Main = TEXT("PoliceNightstick"); L.Stance = EHLStance::OneHand; }
	return L;
}

FHLCharLook FHLChars::LookFor(const FHLGame& G, const FHLUnit& U) const
{
	if (U.Id == G.PlayerUnitId) return LookFromGear(G.Player.Look, G.Player.Equip, U.bRangedAuto);
	if (U.SimIdx >= 0 && G.Sims.IsValidIndex(U.SimIdx))
	{
		const FHLSim& S = G.Sims[U.SimIdx];
		FHLAppearance A = FHLAppearance::FromSeed(GetTypeHash(S.P ? S.P->Id : U.Name), S.P ? S.P->Body : -1);
		if (S.P) { A.Skin = S.P->Skin; A.Hair = S.P->HairStyle; }
		return LookFromGear(A, S.Gear, U.bRangedAuto);
	}
	if (U.Npc) return NpcLook(*this, *U.Npc);
	// human mobs: looters in work clothes, enforcers in suits
	FHLCharLook L;
	const uint32 H = GetTypeHash(U.Mob ? U.Mob->Id : U.Name) ^ (uint32)U.Id;
	L.App = FHLAppearance::FromSeed(H, (H % 7 == 0) ? 1 : 0);
	const bool bSuit = U.Mob && (U.Mob->Id.Contains(TEXT("dragna")) || U.Mob->Name.Contains(TEXT("Enforcer")));
	static const TCHAR* Rough[4] = { TEXT("P1"), TEXT("L1"), TEXT("P2"), TEXT("L2") };
	const FString Look = bSuit ? FString(H % 2 ? TEXT("C2") : TEXT("C3")) : FString(Rough[H % 4]);
	for (int32 i = 0; i < NumSlots; ++i) L.Slots[i] = Look;
	if (H % 2) L.Hat = Look;
	L.Main = bSuit ? FString(TEXT("Blackjack")) : FString(H % 3 ? TEXT("Crowbar") : TEXT("PipeWrench"));
	L.Stance = EHLStance::OneHand;
	return L;
}

// ------------------------------------------------------------------ assets

UObject* FHLChars::Load(const FString& Path)
{
	if (TObjectPtr<UObject>* Found = Cache.Find(Path)) return *Found;
	if (Missing.Contains(Path)) return nullptr;
	UObject* O = StaticLoadObject(UObject::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (O) Cache.Add(Path, O);
	else { Missing.Add(Path); HLBridge::Log(FString::Printf(TEXT("HL character asset missing: %s"), *Path)); }
	return O;
}

static FString ObjPath(const FString& Folder, const FString& Name) { return FString::Printf(TEXT("%s/%s/%s.%s"), *CharBase, *Folder, *Name, *Name); }

UTexture2D* FHLChars::Tex(const FString& Path)
{
	if (Path.IsEmpty()) return nullptr;
	FString Folder, Name;
	if (!Path.Split(TEXT("/"), &Folder, &Name)) return nullptr;
	UTexture2D* T = Cast<UTexture2D>(Load(ObjPath(TEXT("Tex/") + Folder, Name)));
#if WITH_EDITOR
	// editor runs build textures asynchronously; an atlas composed meanwhile would bake the white placeholder in for good
	if (T && T->IsCompiling()) FTextureCompilingManager::Get().FinishCompilation({ T });
#endif
	return T;
}
USkeletalMesh* FHLChars::SkelMesh(const FString& Folder, const FString& Name) { return Name.IsEmpty() ? nullptr : Cast<USkeletalMesh>(Load(ObjPath(Folder, Name))); }
UStaticMesh* FHLChars::StaticMesh(const FString& Folder, const FString& Name) { return Name.IsEmpty() ? nullptr : Cast<UStaticMesh>(Load(ObjPath(Folder, Name))); }
UAnimSequence* FHLChars::Anim(int32 Sex, const FString& Clip)
{
	const FString Name = FString::Printf(TEXT("A_%s_%s"), SexName[Sex & 1], *Clip);
	const FString Path = ObjPath(TEXT("Anim"), Name);
	if (Missing.Contains(Path)) return nullptr;
	if (TObjectPtr<UObject>* Found = Cache.Find(Path)) return Cast<UAnimSequence>(*Found);
	UObject* O = StaticLoadObject(UObject::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (O) Cache.Add(Path, O); else Missing.Add(Path); // optional clips fall back silently
	return Cast<UAnimSequence>(O);
}
UMaterialInterface* FHLChars::Mat(const TCHAR* Name) { return Cast<UMaterialInterface>(Load(FString::Printf(TEXT("%s/%s.%s"), *CharBase, Name, Name))); }

void FHLChars::AddReferencedObjects(FReferenceCollector& Collector) { Collector.AddReferencedObjects(Cache); }

// ------------------------------------------------------------------ visuals

void FHLCharVisuals::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FAtlas& A : Atlases) { Collector.AddReferencedObject(A.RT); Collector.AddReferencedObject(A.Mid); }
	Collector.AddReferencedObjects(LayerMids);
	Collector.AddReferencedObjects(PropMids);
}

UMaterialInstanceDynamic* FHLCharVisuals::PropMid(const FString& Tex, const FLinearColor& Tint, const TCHAR* Material)
{
	const FString Key = FString::Printf(TEXT("%s|%s|%s"), Material, *Tex, *Tint.ToString());
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = PropMids.Find(Key)) return *Found;
	FHLChars& C = FHLChars::Get();
	UMaterialInterface* Base = C.Mat(Material);
	if (!Base) return nullptr;
	UMaterialInstanceDynamic* D = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
	if (UTexture2D* T = C.Tex(Tex)) D->SetTextureParameterValue(TEXT("Tex"), T);
	D->SetVectorParameterValue(TEXT("Tint"), Tint);
	PropMids.Add(Key, D);
	return D;
}

bool FHLCharVisuals::Compose(UWorld* World, UTextureRenderTarget2D* RT, const FHLCharLook& L)
{
	FHLChars& C = FHLChars::Get();
	const FHLChars::FSex& Sx = C.Sexes[L.App.Sex & 1];
	UCanvas* Canvas = nullptr;
	FVector2D Size;
	FDrawToRenderTargetContext Ctx;
	UKismetRenderingLibrary::ClearRenderTarget2D(World, RT, FLinearColor(0.5f, 0.4f, 0.35f, 1));
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, RT, Canvas, Size, Ctx);
	if (!Canvas) return false;
	// a texture whose GPU resource is not up yet draws as black and would stay baked into the atlas
	bool bReady = true;
	auto Ok = [&](UTexture2D* T) { if (T && (!T->GetResource() || T->HasPendingInitOrStreaming())) bReady = false; return T; };
	int32 NextMid = 0;
	auto Draw = [&](const FHLChars::FLayer& Ly, const FLinearColor& Mul, const FLinearColor* TintA, const FLinearColor* TintB)
	{
		UTexture2D* T = Ok(C.Tex(Ly.Tex));
		if (!T) return;
		const FVector2D Pos(Ly.Rect.X * Size.X, Ly.Rect.Y * Size.Y), Sz((Ly.Rect.Z - Ly.Rect.X) * Size.X, (Ly.Rect.W - Ly.Rect.Y) * Size.Y);
		UTexture2D* M = TintA ? Ok(C.Tex(Ly.Mask)) : nullptr;
		if (M)
		{
			// dyed layer (the starter outfit): the compositing material applies the mask-weighted swatches
			if (!LayerMids.IsValidIndex(NextMid))
			{
				UMaterialInterface* Base = C.Mat(TEXT("M_HLCharLayer"));
				if (!Base) return;
				LayerMids.Add(UMaterialInstanceDynamic::Create(Base, GetTransientPackage()));
			}
			UMaterialInstanceDynamic* D = LayerMids[NextMid++];
			D->SetTextureParameterValue(TEXT("Layer"), T);
			D->SetTextureParameterValue(TEXT("Mask"), M);
			D->SetVectorParameterValue(TEXT("TintA"), *TintA);
			D->SetVectorParameterValue(TEXT("TintB"), TintB ? *TintB : FLinearColor::White);
			D->SetVectorParameterValue(TEXT("Mul"), Mul);
			Canvas->K2_DrawMaterial(D, Pos, Sz, FVector2D::ZeroVector, FVector2D::UnitVector);
		}
		else Canvas->K2_DrawTexture(T, Pos, Sz, FVector2D::ZeroVector, FVector2D::UnitVector, Mul, BLEND_Translucent);
	};
	if (UTexture2D* Skin = Ok(C.Tex(Sx.Skins.IsValidIndex(L.App.Skin) ? Sx.Skins[L.App.Skin] : Sx.Skins[0])))
		Canvas->K2_DrawTexture(Skin, FVector2D::ZeroVector, Size, FVector2D::ZeroVector, FVector2D::UnitVector, FLinearColor::White, BLEND_Opaque);
	Draw(Sx.Underwear, FLinearColor::White, nullptr, nullptr);
	if (Sx.Faces.IsValidIndex(L.App.Face)) Draw(Sx.Faces[L.App.Face], FLinearColor::White, nullptr, nullptr);
	if (Sx.Hair.IsValidIndex(L.App.Hair)) Draw(Sx.Hair[L.App.Hair].Scalp, FHLChars::HairTint(L.App.HairColor), nullptr, nullptr);
	const int32 Dye = C.OutfitA.Num() ? FMath::Clamp(L.App.Outfit, 0, C.OutfitA.Num() - 1) : -1;
	for (int32 i = 0; i < FHLChars::NumSlots; ++i)
	{
		const FHLChars::FLook* Lk = C.Looks.Find(L.Slots[i]);
		if (!Lk) continue;
		const FHLChars::FLayer& Ly = Lk->Slot[L.App.Sex & 1][i];
		if (!Ly.bValid || Ly.Tex.IsEmpty()) continue;
		if (!Ly.Mask.IsEmpty() && Dye >= 0) Draw(Ly, FLinearColor::White, &C.OutfitA[Dye], &C.OutfitB[Dye]);
		else Draw(Ly, FLinearColor::White, nullptr, nullptr);
	}
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Ctx);
	return bReady;
}

UTextureRenderTarget2D* FHLCharVisuals::Atlas(UWorld* World, const FHLCharLook& L, int32 Size)
{
	const uint64 Key = (uint64(L.TexKey()) << 16) | uint64(Size);
	if (const int32* Idx = AtlasIndex.Find(Key))
	{
		FAtlas& A = Atlases[*Idx];
		A.Used = Clock;
		if (A.bDirty) A.bDirty = !Compose(World, A.RT, L);
		// compose again a few times while the atlas is young: on a slow connection (itch's CDN) a layer can pass the
		// resource check before its pixels reach the GPU, and the atlas would keep the black it drew
		else
		{
			static const double Redo[] = { 0.5, 1.5, 4, 10, 25 };
			if (A.Redo < (int32)UE_ARRAY_COUNT(Redo) && Clock - A.Born >= Redo[A.Redo]) { ++A.Redo; A.bDirty = !Compose(World, A.RT, L); }
		}
		return A.RT;
	}
	// reuse the longest-unused atlas of this size once the cache is full (none used in the last few seconds)
	const int32 Cap = Size >= 1024 ? 6 : 72;
	int32 Count = 0, Oldest = INDEX_NONE;
	for (int32 i = 0; i < Atlases.Num(); ++i)
		if (Atlases[i].Size == Size)
		{
			++Count;
			if (Atlases[i].Used < Clock - 3 && (Oldest == INDEX_NONE || Atlases[i].Used < Atlases[Oldest].Used)) Oldest = i;
		}
	int32 Slot;
	if (Count >= Cap && Oldest != INDEX_NONE)
	{
		Slot = Oldest;
		AtlasIndex.Remove((uint64(Atlases[Slot].Key) << 16) | uint64(Size));
	}
	else
	{
		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
		RT->RenderTargetFormat = RTF_RGBA8_SRGB;
		RT->ClearColor = FLinearColor::Black;
		RT->bAutoGenerateMips = false;
		RT->InitAutoFormat(Size, Size);
		RT->UpdateResourceImmediate(true);
		FAtlas A; A.RT = RT; A.Size = Size;
		if (UMaterialInterface* Base = FHLChars::Get().Mat(TEXT("M_HLChar")))
		{
			A.Mid = UMaterialInstanceDynamic::Create(Base, GetTransientPackage());
			A.Mid->SetTextureParameterValue(TEXT("Atlas"), RT);
		}
		Slot = Atlases.Add(A);
	}
	Atlases[Slot].Key = L.TexKey();
	Atlases[Slot].Used = Atlases[Slot].Born = Clock;
	Atlases[Slot].Redo = 0;
	AtlasIndex.Add(Key, Slot);
	Atlases[Slot].bDirty = !Compose(World, Atlases[Slot].RT, L);
	if (Atlases[Slot].bDirty) UE_LOG(LogTemp, Log, TEXT("HL atlas %08x: textures not uploaded yet, composing again later"), L.TexKey());

	return Atlases[Slot].RT;
}

UMaterialInstanceDynamic* FHLCharVisuals::BodyMid(UWorld* World, const FHLCharLook& L, int32 Size)
{
	Atlas(World, L, Size);
	const int32* Idx = AtlasIndex.Find((uint64(L.TexKey()) << 16) | uint64(Size));
	return Idx ? Atlases[*Idx].Mid.Get() : nullptr;
}

void FHLCharVisuals::Clear(FHLCharRig& Rig)
{
	for (TWeakObjectPtr<USceneComponent>& P : Rig.Parts) if (P.IsValid()) P->DestroyComponent();
	for (TWeakObjectPtr<USceneComponent>& P : Rig.HeldParts) if (P.IsValid()) P->DestroyComponent();
	Rig.Parts.Reset(); Rig.HeldParts.Reset();
	if (Rig.Leader.IsValid()) Rig.Leader->DestroyComponent();
	if (Rig.Root.IsValid()) Rig.Root->DestroyComponent();
	Rig.Leader.Reset(); Rig.Root.Reset();
	Rig.MeshKey = Rig.TexKey = Rig.HeldKey = 0; Rig.Sex = -1;
	Rig.Base.Reset(); Rig.Shot.Reset();
}

// component-space reference pose of a bone
static FTransform RefCS(const USkeletalMesh* Mesh, FName Bone)
{
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	int32 I = Ref.FindBoneIndex(Bone);
	FTransform T = FTransform::Identity;
		while (I != INDEX_NONE) { T = T * Ref.GetRefBonePose()[I]; I = Ref.GetParentIndex(I); }
		return T;
	}

	// component-space pose of a bone in a clip at a time: the clip's rotations on the mesh's own bone lengths
	// (bones without a track keep the reference pose)
	static bool PoseCS(const USkeletalMesh* Mesh, const UAnimSequence* Seq, FName Bone, double Time, FTransform& Out)
	{
		USkeleton* Sk = Seq ? const_cast<USkeleton*>(Seq->GetSkeleton()) : nullptr; // the bone lookup is not const
		if (!Mesh || !Sk) return false;
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		int32 I = Ref.FindBoneIndex(Bone);
		if (I == INDEX_NONE) return false;
		const FAnimExtractContext Ctx(Time);
		Out = FTransform::Identity;
		while (I != INDEX_NONE)
		{
			FTransform Local = Ref.GetRefBonePose()[I];
			const int32 SkI = Sk->GetSkeletonBoneIndexFromMeshBoneIndex(Mesh, I);
			if (SkI != INDEX_NONE)
			{
				FTransform A = Local;
				Seq->GetBoneTransform(A, FSkeletonPoseBoneIndex(SkI), Ctx, false);
				Local.SetRotation(A.GetRotation());
			}
			Out = Out * Local;
			I = Ref.GetParentIndex(I);
		}
		return true;
	}

void FHLCharVisuals::Apply(UWorld* World, AActor* Owner, USceneComponent* Parent, FHLCharRig& Rig, const FHLCharLook& L, int32 AtlasSize)
{
	FHLChars& C = FHLChars::Get();
	if (!C.Ready() || !World || !Owner || !Parent) return;
	const int32 Sex = L.App.Sex & 1;
	const uint32 MKey = L.MeshKey(), TKey = L.TexKey(), HKey = L.HeldKey();
	// a weapon drawn or put away: swap the held items only. Rebuilding the skeleton would show one frame of the
	// reference (A) pose before the new leader evaluates its clip.
	if (Rig.Leader.IsValid() && Rig.MeshKey == MKey && Rig.HeldKey != HKey) AttachHeld(Owner, Rig, L);
	UMaterialInstanceDynamic* Body = BodyMid(World, L, AtlasSize);
	// same look on the same atlas: nothing to do. (When the unit was out of view its atlas slot may have gone to someone
	// else, and the look then sits in a new slot with its own material.)
	if (Rig.Leader.IsValid() && Rig.MeshKey == MKey && Rig.TexKey == TKey && Rig.AtlasSize == AtlasSize && Rig.BodyMat.Get() == Body) return;
	if (Rig.Leader.IsValid() && Rig.MeshKey == MKey)
	{
		// only the paint changed (dye, skin...): swap the body material
		for (TWeakObjectPtr<USceneComponent>& P : Rig.Parts)
			if (USkeletalMeshComponent* S = Cast<USkeletalMeshComponent>(P.Get())) if (S->ComponentTags.Contains(TEXT("body"))) for (int32 m = 0; m < S->GetNumMaterials(); ++m) S->SetMaterial(m, Body);
		for (int32 m = 0; m < Rig.Leader->GetNumMaterials(); ++m) Rig.Leader->SetMaterial(m, Body);
		Rig.TexKey = TKey; Rig.AtlasSize = AtlasSize; Rig.BodyMat = Body;
		return;
	}
	Clear(Rig);
	const FHLChars::FSex& Sx = C.Sexes[Sex];
	// geosets: defaults, then each slot's look in the package's slot order (Torso/Arms/Hands/Legs/Feet/Head replace their
	// group; Coat_Tails, Coat_Long, Belt_Heavy and Collar_Up add)
	TMap<FString, FString> Groups;
	TArray<FString> Extra;
	for (const FString& G : Sx.Defaults) { FString Grp, Rest; G.Split(TEXT("_"), &Grp, &Rest); Groups.Add(Grp, G); }
	for (int32 i = 0; i < FHLChars::NumSlots; ++i)
		if (const FHLChars::FLook* Lk = C.Looks.Find(L.Slots[i]))
			for (const FString& G : Lk->Slot[Sex][i].Geosets)
			{
				FString Grp, Rest; G.Split(TEXT("_"), &Grp, &Rest);
				if (Grp == TEXT("Head") || Grp == TEXT("Torso") || Grp == TEXT("Arms") || Grp == TEXT("Hands") || Grp == TEXT("Legs") || Grp == TEXT("Feet")) Groups.Add(Grp, G);
				else Extra.AddUnique(G);
			}
	USkeletalMesh* HeadMesh = C.SkelMesh(TEXT("Body"), FString::Printf(TEXT("SK_%s_%s"), SexName[Sex], *Groups.FindRef(TEXT("Head"))));
	if (!HeadMesh || !Body) return;
	// facing: the package's characters face +Y in their source; find the imported forward from the hands
	const FTransform HandR = RefCS(HeadMesh, TEXT("hand_r")), HandL = RefCS(HeadMesh, TEXT("hand_l"));
	FVector Right = HandR.GetLocation() - HandL.GetLocation(); Right.Z = 0;
	FVector Fwd = FVector::CrossProduct(Right.GetSafeNormal(), FVector::UpVector).GetSafeNormal();
	if (Fwd.IsNearlyZero()) Fwd = FVector::ForwardVector;
	const float MeshYaw = -FMath::RadiansToDegrees(FMath::Atan2(Fwd.Y, Fwd.X));
	USceneComponent* Root = NewObject<USceneComponent>(Owner);
	Root->SetupAttachment(Parent);
	Root->RegisterComponent();
	Root->SetRelativeRotation(FRotator(0, MeshYaw, 0));
	Rig.Root = Root;
	auto MakeSkel = [&](USkeletalMesh* Mesh, UMaterialInterface* Mat, bool bBody) -> USkeletalMeshComponent*
	{
		USkeletalMeshComponent* S = NewObject<USkeletalMeshComponent>(Owner);
		S->SetSkeletalMeshAsset(Mesh);
		S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		S->SetCastShadow(false);
		S->SetupAttachment(Root);
		if (bBody) S->ComponentTags.Add(TEXT("body"));
		S->RegisterComponent();
		for (int32 m = 0; m < S->GetNumMaterials(); ++m) S->SetMaterial(m, Mat);
		return S;
	};
	USkeletalMeshComponent* Leader = MakeSkel(HeadMesh, Body, true);
	Leader->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Leader->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Rig.Leader = Leader;
	auto Follow = [&](USkeletalMesh* Mesh, UMaterialInterface* Mat, bool bBody)
	{
		if (!Mesh) return;
		USkeletalMeshComponent* S = MakeSkel(Mesh, Mat, bBody);
		S->SetLeaderPoseComponent(Leader);
		S->bUseBoundsFromLeaderPoseComponent = true;
		Rig.Parts.Add(S);
	};
	for (const auto& KV : Groups) if (KV.Key != TEXT("Head")) Follow(C.SkelMesh(TEXT("Body"), FString::Printf(TEXT("SK_%s_%s"), SexName[Sex], *KV.Value)), Body, true);
	for (const FString& G : Extra) Follow(C.SkelMesh(TEXT("Body"), FString::Printf(TEXT("SK_%s_%s"), SexName[Sex], *G)), Body, true);
	// hair (rigid on the head) and hat
	const FHLChars::FLook* HatLook = C.Looks.Find(L.Hat);
	const bool bHat = HatLook && !HatLook->HatMesh[Sex].IsEmpty();
	if (Sx.Hair.IsValidIndex(L.App.Hair))
	{
		UMaterialInstanceDynamic* HairMat = PropMid(Sx.HairTex, FHLChars::HairTint(L.App.HairColor), TEXT("M_HLHair"));
		const FHLChars::FHair& Hr = Sx.Hair[L.App.Hair];
		Follow(C.SkelMesh(TEXT("Hair"), Hr.Mesh), HairMat, false);
		if (!(bHat && HatLook->bHidesTop[Sex])) Follow(C.SkelMesh(TEXT("Hair"), Hr.Top), HairMat, false);
	}
	auto Attach = [&](UStaticMesh* Mesh, UMaterialInterface* Mat, FName Bone, const FTransform& WantCS)
	{
		if (!Mesh) return;
		UStaticMeshComponent* S = NewObject<UStaticMeshComponent>(Owner);
		S->SetStaticMesh(Mesh);
		S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		S->SetCastShadow(false);
		S->SetupAttachment(Leader, Bone);
		S->RegisterComponent();
		if (Mat) for (int32 m = 0; m < S->GetNumMaterials(); ++m) S->SetMaterial(m, Mat);
		S->SetRelativeTransform(WantCS.GetRelativeTransform(RefCS(HeadMesh, Bone)));
		Rig.Parts.Add(S);
	};
	if (bHat)
	{
		// the hat's origin is the crown; the package gives the crown in the source frame (y = forward, z = up)
		const FVector Crown = Fwd * 0.f + FVector(0, 0, Sx.Height);
		const FVector Ahead = Fwd * (Sex ? 5.83f : 7.70f);
		Attach(C.StaticMesh(TEXT("Hats"), HatLook->HatMesh[Sex]), PropMid(HatLook->HatTex[Sex], FLinearColor::White, TEXT("M_HLProp")), TEXT("head"), FTransform(Crown + Ahead));
	}
		Rig.Fwd = Fwd;
		Rig.MeshKey = MKey; Rig.TexKey = TKey; Rig.Sex = Sex; Rig.AtlasSize = AtlasSize; Rig.BodyMat = Body;
		AttachHeld(Owner, Rig, L);
	}

	void FHLCharVisuals::AttachHeld(AActor* Owner, FHLCharRig& Rig, const FHLCharLook& L)
	{
		for (TWeakObjectPtr<USceneComponent>& P : Rig.HeldParts) if (P.IsValid()) P->DestroyComponent();
		Rig.HeldParts.Reset();
		Rig.HeldKey = L.HeldKey();
		USkeletalMeshComponent* Leader = Rig.Leader.Get();
		const USkeletalMesh* HeadMesh = Leader ? Leader->GetSkeletalMeshAsset() : nullptr;
		if (!HeadMesh) return;
		FHLChars& C = FHLChars::Get();
		// held items by their grip origin: blade/barrel (+X) forward, top (+Z) up, in the palm of the bind pose
		auto Hold = [&](const FString& Id, bool bForceLeft)
		{
			const FHLChars::FHeld* H = C.Held.Find(Id);
			UStaticMesh* Mesh = H ? C.StaticMesh(TEXT("Held"), H->Mesh) : nullptr;
			if (!Mesh) return;
			const bool bLeft = H->bLeft || bForceLeft;
			const FName Bone = bLeft ? TEXT("hand_l") : TEXT("hand_r");
			const FTransform Hand = RefCS(HeadMesh, Bone);
			const FTransform Finger = RefCS(HeadMesh, bLeft ? TEXT("middle_01_l") : TEXT("middle_01_r"));
			FVector Palm = Hand.GetLocation();
			if (!Finger.GetLocation().IsNearlyZero()) Palm = FMath::Lerp(Hand.GetLocation(), Finger.GetLocation(), 0.75f) - FVector(0, 0, 2.5f);
			// assets modelled against the convention are rolled upright first
			const FQuat Fix(FVector::ForwardVector, FMath::DegreesToRadians(H->Roll));
			FQuat Q = FRotationMatrix::MakeFromXZ(Rig.Fwd, FVector::UpVector).ToQuat();
			FTransform BoneCS = RefCS(HeadMesh, Bone);
			// firearms: the aim pose decides the grip. Fixed to the bind (A) pose hand, a revolver pointed up and back and a
			// rifle hung barrel-up once the arms came up to aim. Fit the gun to the hand of the stance's combat clip instead:
			// barrel along the facing (a long gun from the trigger hand towards the hand under the barrel), top up.
			if (L.bRangedOut && !bLeft)
			{
				const bool bPistol = L.Stance == EHLStance::Pistol;
				const UAnimSequence* Aim = C.Anim(Rig.Sex, bPistol ? TEXT("Combat_Ranged1H") : TEXT("Combat_Rifle"));
				FTransform HandA, FingerA, OtherA;
				if (PoseCS(HeadMesh, Aim, Bone, 0.0, HandA))
				{
					// the gun's support line (grip origin to its GRIP_L point, which sits above the bore line on long guns)
					// goes from the trigger hand to the support hand; a pistol's barrel goes along the facing
					FVector Line = Rig.Fwd, GunLine = FVector::ForwardVector;
					if (!bPistol && H->bGripL && PoseCS(HeadMesh, Aim, TEXT("hand_l"), 0.0, OtherA))
					{
						const FVector Along = OtherA.GetLocation() - HandA.GetLocation();
						if (Along.Size() > 8.f) { Line = Along.GetSafeNormal(); GunLine = Fix.RotateVector(H->GripL).GetSafeNormal(); }
					}
					Palm = HandA.GetLocation();
					if (PoseCS(HeadMesh, Aim, TEXT("middle_01_r"), 0.0, FingerA)) Palm = FMath::Lerp(HandA.GetLocation(), FingerA.GetLocation(), 0.6f);
					const FQuat Gun = FRotationMatrix::MakeFromXZ(GunLine, FVector::UpVector).ToQuat();
					Q = FRotationMatrix::MakeFromXZ(Line, FVector::UpVector).ToQuat() * Gun.Inverse();
					BoneCS = HandA;
				}
			}
			UStaticMeshComponent* S = NewObject<UStaticMeshComponent>(Owner);
			S->SetStaticMesh(Mesh);
			S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			S->SetCastShadow(false);
			S->SetupAttachment(Leader, Bone);
			S->RegisterComponent();
			UMaterialInterface* Mat = PropMid(H->Tex, FLinearColor::White, TEXT("M_HLProp"));
			if (Mat) for (int32 m = 0; m < S->GetNumMaterials(); ++m) S->SetMaterial(m, Mat);
			S->SetRelativeTransform(FTransform(Q * Fix, Palm).GetRelativeTransform(BoneCS));
			Rig.HeldParts.Add(S);
		};
		if (L.bRangedOut) Hold(L.Ranged, false);
		else { Hold(L.Main, false); Hold(L.Off, true); }
	}

FString FHLCharVisuals::ClipFor(int32 Sex, std::initializer_list<const TCHAR*> Names) const
{
	for (const TCHAR* N : Names) if (FHLChars::Get().Anim(Sex, N)) return N;
	return FString();
}

void FHLCharVisuals::Play(FHLCharRig& Rig, const FString& Clip, bool bLoop, float InRate)
{
	USkeletalMeshComponent* Leader = Rig.Leader.Get();
	if (!Leader || Clip.IsEmpty()) return;
	if (Rig.Base == Clip && Rig.Shot.IsEmpty()) { if (!FMath::IsNearlyEqual(Rig.Rate, InRate, 0.05f)) { Leader->SetPlayRate(InRate); Rig.Rate = InRate; } return; }
	UAnimSequence* Seq = FHLChars::Get().Anim(Rig.Sex, Clip);
	if (!Seq) return;
	Leader->PlayAnimation(Seq, bLoop);
	Leader->SetPlayRate(InRate);
	// pose now: a leader made this frame would otherwise draw its reference (A) pose once
	Leader->TickAnimation(0.f, false); Leader->RefreshBoneTransforms();
	Rig.Base = Clip; Rig.Rate = InRate; Rig.Shot.Reset();
}

void FHLCharVisuals::PlayOnce(FHLCharRig& Rig, const FString& Clip, double Now, float InRate)
{
	USkeletalMeshComponent* Leader = Rig.Leader.Get();
	UAnimSequence* Seq = Leader && !Clip.IsEmpty() ? FHLChars::Get().Anim(Rig.Sex, Clip) : nullptr;
	if (!Seq) return;
	Leader->PlayAnimation(Seq, false);
	Leader->SetPlayRate(InRate);
	Leader->TickAnimation(0.f, false); Leader->RefreshBoneTransforms();
	Rig.Shot = Clip; Rig.Rate = InRate;
	Rig.ShotUntil = Now + Seq->GetPlayLength() / FMath::Max(0.1f, InRate) - 0.05;
	Rig.Base.Reset(); // the loop restarts when the shot ends
}

void FHLCharVisuals::Drive(FHLCharRig& Rig, const FHLUnit& U, const FHLCharLook& L, double Now, float Dt)
{
	if (!Rig.Leader.IsValid() || Rig.Sex < 0) return;
	const int32 S = Rig.Sex;
	// ground speed from the simulated position (units move in metres)
	if (!Rig.bHasLast) { Rig.LastPos = U.Pos; Rig.bHasLast = true; Rig.GestureSeq = U.GestureSeq; Rig.bWasSitting = U.bSitting; }
	const FVector2D Delta = U.Pos - Rig.LastPos;
	Rig.LastPos = U.Pos;
	const float Inst = Dt > 0 ? (float)Delta.Size() / Dt : 0.f;
	Rig.Speed = Inst > 30 ? 0.f : FMath::FInterpTo(Rig.Speed, Inst, Dt, 10.f); // teleports read as standing
	if (U.bDead)
	{
		if (Rig.Base != TEXT("Death") && Rig.Base != TEXT("Death_B")) { Rig.Shot.Reset(); Play(Rig, ClipFor(S, { TEXT("Death") }), false); }
		return;
	}
	const bool bCasting = U.Casting != nullptr;
	// gestures (emotes, looting, talking) when standing still and not fighting; a dance goes on until something else
	if (U.GestureSeq != Rig.GestureSeq)
	{
		Rig.GestureSeq = U.GestureSeq;
		if (Rig.Speed < 1.f && !U.bAutoAttack && !bCasting && !U.Gesture.IsEmpty() && FHLChars::Get().Anim(S, U.Gesture))
		{
			if (U.Gesture == TEXT("Dance")) { Rig.Shot.Reset(); Play(Rig, U.Gesture, true); Rig.bDancing = true; }
			else { Rig.bDancing = false; PlayOnce(Rig, U.Gesture, Now); }
		}
	}
	// one-shots: melee swings and shots (the swing timer restarts on each auto attack), cast releases
	if (U.bAutoAttack && U.SwingTimer > Rig.LastSwing + 0.25f && Rig.Speed < 1.f)
	{
		static const TCHAR* Unarmed[3] = { TEXT("Attack_Unarmed"), TEXT("Attack_Unarmed_B"), TEXT("Attack_Unarmed_C") };
		FString Clip;
		switch (L.Stance)
		{
		case EHLStance::Pistol: Clip = ClipFor(S, { TEXT("Shoot_Pistol") }); break;
		case EHLStance::Rifle: Clip = ClipFor(S, { TEXT("Shoot_Rifle") }); break;
		case EHLStance::TwoHand: Clip = ClipFor(S, { TEXT("Attack_2H"), TEXT("Attack_1H_A"), TEXT("Attack_Unarmed_C") }); break;
		case EHLStance::OneHand: Clip = ClipFor(S, { (Rig.LastAttack++ % 2) ? TEXT("Attack_1H_B") : TEXT("Attack_1H_A"), TEXT("Attack_1H_A"), TEXT("Attack_Unarmed") }); break;
		default: Clip = ClipFor(S, { Unarmed[Rig.LastAttack++ % 3] }); break;
		}
		PlayOnce(Rig, Clip, Now, 1.15f);
	}
	Rig.LastSwing = U.SwingTimer;
	if (Rig.bWasCasting && !bCasting && Rig.Speed < 1.f) PlayOnce(Rig, ClipFor(S, { TEXT("CastRelease") }), Now);
	Rig.bWasCasting = bCasting;
	const bool bMoving = Rig.Speed > 0.6f;
	if (U.bSitting != Rig.bWasSitting)
	{
		Rig.bWasSitting = U.bSitting;
		Rig.bDancing = false;
		if (!bMoving) PlayOnce(Rig, ClipFor(S, { U.bSitting ? TEXT("SitDown") : TEXT("StandUp") }), Now);
	}
	if (!Rig.Shot.IsEmpty())
	{
		if (Now < Rig.ShotUntil && !bMoving) return;
		Rig.Shot.Reset();
	}
	// base loop
	if (U.Z > 0.15f || U.VelZ > 0.5f) { Play(Rig, ClipFor(S, { TEXT("JumpLoop"), TEXT("Idle") }), true); return; }
	if (bMoving)
	{
		Rig.bDancing = false;
		const float MoveYaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
		const float Rel = FMath::FindDeltaAngleDegrees(U.Yaw, MoveYaw);
		const float Abs = FMath::Abs(Rel);
		FString Clip;
		float Rate;
		if (Rig.Speed < 3.2f) { Clip = ClipFor(S, { Abs > 120 ? TEXT("WalkBack") : TEXT("Walk") }); Rate = FMath::Clamp(Rig.Speed / 1.6f, 0.6f, 1.6f); }
		else
		{
			if (Abs > 120) Clip = ClipFor(S, { TEXT("RunBack") });
			else if (Abs > 50) Clip = ClipFor(S, { Rel > 0 ? TEXT("StrafeR") : TEXT("StrafeL") });
			else if (L.Stance == EHLStance::Pistol) Clip = ClipFor(S, { TEXT("Run_Pistol"), TEXT("Run") });
			else if (L.Stance == EHLStance::Rifle) Clip = ClipFor(S, { TEXT("Run_Rifle"), TEXT("Run") });
			else Clip = ClipFor(S, { TEXT("Run") });
			Rate = FMath::Clamp(Rig.Speed / 4.6f, 0.8f, 1.9f);
		}
		Play(Rig, Clip, true, Rate);
		return;
	}
	if (U.bSitting) { Play(Rig, ClipFor(S, { TEXT("SitLoop"), TEXT("Idle") }), true); return; }
	if (Rig.bDancing && !bCasting && !U.bAutoAttack && U.CombatUntil <= Now) { Play(Rig, TEXT("Dance"), true); return; }
	Rig.bDancing = false;
	if (bCasting) { Play(Rig, ClipFor(S, { TEXT("CastLoop"), TEXT("Idle") }), true); return; }
	const bool bFight = U.bAutoAttack || U.CombatUntil > Now;
	if (bFight)
	{
		FString Clip;
		switch (L.Stance)
		{
		case EHLStance::Pistol: Clip = ClipFor(S, { TEXT("Combat_Ranged1H"), TEXT("Idle") }); break;
		case EHLStance::Rifle: Clip = ClipFor(S, { TEXT("Combat_Rifle"), TEXT("Idle") }); break;
		case EHLStance::TwoHand: Clip = ClipFor(S, { TEXT("Combat_2H"), TEXT("Combat_1H"), TEXT("Idle") }); break;
		case EHLStance::OneHand: Clip = ClipFor(S, { TEXT("Combat_1H"), TEXT("Idle") }); break;
		default: Clip = ClipFor(S, { TEXT("Combat_Unarmed"), TEXT("Combat_1H"), TEXT("Idle") }); break;
		}
		Play(Rig, Clip, true);
		return;
	}
	Play(Rig, ClipFor(S, { TEXT("Idle") }), true);
}
