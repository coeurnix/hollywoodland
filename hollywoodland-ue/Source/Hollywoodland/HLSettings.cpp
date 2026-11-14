#include "HLSettings.h"
#include "HLBridge.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

static const TCHAR* StoreKey = TEXT("hl_settings");

FHLSettings& FHLSettings::Get()
{
	static FHLSettings S;
	return S;
}

FHLSettings::FHLSettings()
{
	ResetKeys();
#if PLATFORM_WASM
	ApplyPreset(1);
#else
	ApplyPreset(2);
#endif
}

void FHLSettings::ResetKeys()
{
	auto B = [&](EHLAct A, const TCHAR* L, const FKey& K, const FKey& Alt = EKeys::Invalid) { Binds[(int32)A] = { L, K, Alt }; };
	B(EHLAct::Forward, TEXT("Move forward"), EKeys::W, EKeys::Up);
	B(EHLAct::Back, TEXT("Move back"), EKeys::S, EKeys::Down);
	B(EHLAct::TurnLeft, TEXT("Turn left"), EKeys::A, EKeys::Left);
	B(EHLAct::TurnRight, TEXT("Turn right"), EKeys::D, EKeys::Right);
	B(EHLAct::StrafeLeft, TEXT("Strafe left"), EKeys::Q);
	B(EHLAct::StrafeRight, TEXT("Strafe right"), EKeys::E);
	B(EHLAct::Jump, TEXT("Jump"), EKeys::SpaceBar);
	static const FKey Bar[12] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero, EKeys::Hyphen, EKeys::Equals };
	static const TCHAR* BarL[12] = { TEXT("Action button 1"), TEXT("Action button 2"), TEXT("Action button 3"), TEXT("Action button 4"), TEXT("Action button 5"), TEXT("Action button 6"),
		TEXT("Action button 7"), TEXT("Action button 8"), TEXT("Action button 9"), TEXT("Action button 10"), TEXT("Action button 11"), TEXT("Action button 12") };
	for (int32 i = 0; i < 12; ++i) B(EHLAct((int32)EHLAct::Bar1 + i), BarL[i], Bar[i]);
	B(EHLAct::Target, TEXT("Target nearest enemy"), EKeys::Tab);
	B(EHLAct::Interact, TEXT("Interact"), EKeys::F);
	B(EHLAct::Sit, TEXT("Sit / stand"), EKeys::X);
	B(EHLAct::AutoAttack, TEXT("Toggle attack"), EKeys::T);
	B(EHLAct::Reply, TEXT("Reply to whisper"), EKeys::R);
	B(EHLAct::Character, TEXT("Character"), EKeys::C);
	B(EHLAct::Bags, TEXT("Bags"), EKeys::B);
	B(EHLAct::Quests, TEXT("Quest log"), EKeys::L);
	B(EHLAct::Spellbook, TEXT("Abilities"), EKeys::P);
	B(EHLAct::Map, TEXT("Map"), EKeys::M);
	B(EHLAct::Social, TEXT("Who / friends"), EKeys::O);
	B(EHLAct::Talents, TEXT("Talents"), EKeys::N);
	B(EHLAct::Workbench, TEXT("Workbench"), EKeys::K);
	B(EHLAct::Help, TEXT("How to play"), EKeys::H);
}

void FHLSettings::ApplyPreset(int32 Q)
{
	Quality = FMath::Clamp(Q, 0, 3);
	static const int32 Sh[4] = { 0, 1, 2, 2 };
	static const float Vd[4] = { 0.7f, 1.f, 1.2f, 1.5f };
	static const int32 Rs[4] = { 70, 100, 100, 100 };
	Shadows = Sh[Quality]; ViewDist = Vd[Quality]; ResScale = Rs[Quality]; bEffects = true;
}

FString FHLSettings::KeyName(EHLAct A) const
{
	const FKey& K = Binds[(int32)A].Key;
	if (!K.IsValid()) return FString();
	if (K == EKeys::SpaceBar) return TEXT("Space");
	if (K == EKeys::Hyphen) return TEXT("-");
	if (K == EKeys::Equals) return TEXT("=");
	FString S = K.GetDisplayName(false).ToString();
	return S.Len() > 6 ? S.Left(6) : S;
}

void FHLSettings::Bind(EHLAct A, const FKey& K, bool bAlt)
{
	if (K.IsValid())
		for (FHLBind& O : Binds) { if (O.Key == K) O.Key = EKeys::Invalid; if (O.Alt == K) O.Alt = EKeys::Invalid; }
	FHLBind& B = Binds[(int32)A];
	(bAlt ? B.Alt : B.Key) = K;
	Changed();
}

void FHLSettings::Save() const
{
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetNumberField(TEXT("ui"), UiScale);
	O->SetBoolField(TEXT("plates"), bNameplates);
	O->SetBoolField(TEXT("float"), bFloatText);
	O->SetBoolField(TEXT("help"), bHelpOnStart);
	O->SetBoolField(TEXT("facecam"), bFaceCamera);
	O->SetBoolField(TEXT("clickmove"), bClickMove);
	O->SetNumberField(TEXT("hold"), TextHold);
	O->SetNumberField(TEXT("q"), Quality);
	O->SetNumberField(TEXT("shadows"), Shadows);
	O->SetNumberField(TEXT("view"), ViewDist);
	O->SetNumberField(TEXT("res"), ResScale);
	O->SetBoolField(TEXT("fx"), bEffects);
	TSharedRef<FJsonObject> K = MakeShared<FJsonObject>();
	for (int32 i = 0; i < (int32)EHLAct::Count; ++i)
		K->SetStringField(FString::FromInt(i), Binds[i].Key.GetFName().ToString() + TEXT("|") + Binds[i].Alt.GetFName().ToString());
	O->SetObjectField(TEXT("keys"), K);
	FString Out;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(O, W);
	HLBridge::StorageSet(StoreKey, Out);
}

void FHLSettings::Load()
{
	HLBridge::StorageGet(StoreKey, [](const FHLBridgeResult& R)
	{
		if (!R.Ok() || R.Body.IsEmpty()) return;
		TSharedPtr<FJsonObject> O;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(R.Body), O) || !O) return;
		FHLSettings& S = Get();
		double D = 0; bool b = false;
		if (O->TryGetNumberField(TEXT("ui"), D)) S.UiScale = FMath::Clamp((float)D, 0.8f, 1.3f);
		if (O->TryGetBoolField(TEXT("plates"), b)) S.bNameplates = b;
		if (O->TryGetBoolField(TEXT("float"), b)) S.bFloatText = b;
		if (O->TryGetBoolField(TEXT("help"), b)) S.bHelpOnStart = b;
		if (O->TryGetBoolField(TEXT("facecam"), b)) S.bFaceCamera = b;
		if (O->TryGetBoolField(TEXT("clickmove"), b)) S.bClickMove = b;
		if (O->TryGetNumberField(TEXT("hold"), D)) S.TextHold = FMath::Clamp((float)D, 1.f, 3.f);
		if (O->TryGetNumberField(TEXT("q"), D)) S.Quality = FMath::Clamp((int32)D, 0, 3);
		if (O->TryGetNumberField(TEXT("shadows"), D)) S.Shadows = FMath::Clamp((int32)D, 0, 2);
		if (O->TryGetNumberField(TEXT("view"), D)) S.ViewDist = FMath::Clamp((float)D, 0.6f, 1.5f);
		if (O->TryGetNumberField(TEXT("res"), D)) S.ResScale = FMath::Clamp((int32)D, 50, 100);
		if (O->TryGetBoolField(TEXT("fx"), b)) S.bEffects = b;
		const TSharedPtr<FJsonObject>* K = nullptr;
		if (O->TryGetObjectField(TEXT("keys"), K) && K)
			for (int32 i = 0; i < (int32)EHLAct::Count; ++i)
			{
				FString V;
				if (!(*K)->TryGetStringField(FString::FromInt(i), V)) continue;
				FString A, Bk; V.Split(TEXT("|"), &A, &Bk);
				S.Binds[i].Key = FKey(*A); S.Binds[i].Alt = FKey(*Bk);
				if (!S.Binds[i].Key.IsValid()) S.Binds[i].Key = EKeys::Invalid;
				if (!S.Binds[i].Alt.IsValid()) S.Binds[i].Alt = EKeys::Invalid;
			}
		++S.Version;
	});
}
