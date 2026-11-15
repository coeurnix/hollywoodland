#include "HLUI.h"
#include "HLWorldActor.h"
#include "HLBridge.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/ScopeExit.h"

#ifndef HL_UI_COLORS // shared by HLUI.cpp and HLUIWindows.cpp (one unity file may hold both)
#define HL_UI_COLORS
static const FLinearColor ColGold(1.f, 0.82f, 0.25f), ColDim(0.62f, 0.62f, 0.62f), ColWhite(1, 1, 1), ColRed(1, 0.25f, 0.2f), ColGreen(0.3f, 1, 0.3f);
#endif

void SHLGameUI::Construct(const FArguments& Args) { World = Args._World; SetCanTick(false); }
FHLGame& SHLGameUI::G() const { return World->Game; }

// ------------------------------------------------------------------------------------------------------------- fonts
// Alegreya Sans (body) and Limelight (display, Art Deco), both SIL OFL, loaded at runtime from Content/Data/Fonts.
// Falls back to the engine font when the files are missing.
static FSlateFontInfo HLFont(int32 Size, bool bBold)
{
	static TSharedPtr<const FCompositeFont> Body, Display;
	static bool bInit = false;
	if (!bInit)
	{
		bInit = true;
		const FString Dir = FPaths::ProjectContentDir() / TEXT("Data/Fonts/");
		if (FPaths::FileExists(Dir + TEXT("AlegreyaSans-Regular.ttf")) && FPaths::FileExists(Dir + TEXT("AlegreyaSans-Bold.ttf")))
		{
			TSharedRef<FStandaloneCompositeFont> F = MakeShared<FStandaloneCompositeFont>(TEXT("Regular"), Dir + TEXT("AlegreyaSans-Regular.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			F->DefaultTypeface.AppendFont(TEXT("Bold"), Dir + TEXT("AlegreyaSans-Bold.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			Body = F;
		}
		if (FPaths::FileExists(Dir + TEXT("Limelight-Regular.ttf")))
			Display = MakeShared<FStandaloneCompositeFont>(TEXT("Regular"), Dir + TEXT("Limelight-Regular.ttf"), EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
	}
	// headings (16 and up) use the display face; Alegreya runs small, so body sizes get a nudge
	if ((Size >= 16 || (bBold && Size >= 14)) && Display.IsValid()) return FSlateFontInfo(Display, Size, TEXT("Regular"));
	if (Body.IsValid()) return FSlateFontInfo(Body, FMath::RoundToInt(Size * 1.15f), bBold ? TEXT("Bold") : TEXT("Regular"));
	return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}

// ------------------------------------------------------------------------------------------------------------- icons and art
// Atlas sheets from tools/itemgen (icons.json) and tools/artgen (skills.json) in Content/Data/Icons, interface art in
// Content/Data/UI. One heap brush per icon with a UV window into its sheet (heap so pointers stay put).
static UTexture2D* HLLoadTexture(const FString& Path)
{
	TArray<uint8> Bytes;
	UTexture2D* T = FFileHelper::LoadFileToArray(Bytes, *Path) ? FImageUtils::ImportBufferAsTexture2D(Bytes) : nullptr;
	if (T) { T->AddToRoot(); T->Filter = TF_Bilinear; T->LODGroup = TEXTUREGROUP_UI; T->UpdateResource(); }
	return T;
}
struct FHLAtlas
{
	int32 Tile = 0, Cols = 0, PerSheet = 0, SheetSize = 0;
	TArray<UTexture2D*> Sheets;
	TMap<int32, TSharedPtr<FSlateBrush>> Brushes;
	TMap<FString, int32> Named;
	explicit FHLAtlas(const TCHAR* Layout)
	{
		const FString Dir = FPaths::ProjectContentDir() / TEXT("Data/Icons/");
		FString Text;
		TSharedPtr<FJsonObject> J;
		if (!FFileHelper::LoadFileToString(Text, *(Dir + Layout)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()) return;
		Tile = J->GetIntegerField(TEXT("tile")); Cols = J->GetIntegerField(TEXT("columns")); PerSheet = J->GetIntegerField(TEXT("per_sheet")); SheetSize = J->GetIntegerField(TEXT("sheet_size"));
		for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("sheets"))) Sheets.Add(HLLoadTexture(Dir + V->AsString()));
		const TSharedPtr<FJsonObject>* N = nullptr;
		if (J->TryGetObjectField(TEXT("named"), N)) for (const auto& KV : (*N)->Values) Named.Add(FString(KV.Key), (int32)KV.Value->AsNumber());
	}
	const FSlateBrush* Get(int32 Icon)
	{
		if (Icon < 0 || PerSheet <= 0 || Cols <= 0) return nullptr;
		if (const TSharedPtr<FSlateBrush>* B = Brushes.Find(Icon)) return B->IsValid() ? B->Get() : nullptr;
		const int32 Sh = Icon / PerSheet, Cell = Icon % PerSheet;
		if (!Sheets.IsValidIndex(Sh) || !Sheets[Sh]) { Brushes.Add(Icon, nullptr); return nullptr; }
		TSharedPtr<FSlateBrush> B = MakeShared<FSlateBrush>();
		B->SetResourceObject(Sheets[Sh]);
		B->ImageSize = FVector2D(Tile, Tile);
		B->DrawAs = ESlateBrushDrawType::Image;
		const float Inv = 1.f / SheetSize;
		const float U0 = (Cell % Cols) * Tile * Inv, V0 = (Cell / Cols) * Tile * Inv;
		B->SetUVRegion(FBox2f(FVector2f(U0, V0), FVector2f(U0 + Tile * Inv, V0 + Tile * Inv)));
		Brushes.Add(Icon, B);
		return B.Get();
	}
};
static const FSlateBrush* HLIconBrush(int32 Icon) { static FHLAtlas At(TEXT("icons.json")); return At.Get(Icon); }
const FSlateBrush* HLSkillBrush(int32 Icon) { static FHLAtlas At(TEXT("skills.json")); return At.Get(Icon); }
const FSlateBrush* HLMenuBrush(const FString& Name) { static FHLAtlas At(TEXT("skills.json")); const int32* I = At.Named.Find(Name); return I ? At.Get(*I) : nullptr; }
const FSlateBrush* HLArtBrush(const FString& Name)
{
	static TMap<FString, TSharedPtr<FSlateBrush>> Art;
	if (const TSharedPtr<FSlateBrush>* B = Art.Find(Name)) return B->IsValid() ? B->Get() : nullptr;
	UTexture2D* T = HLLoadTexture(FPaths::ProjectContentDir() / TEXT("Data/UI/") + Name + TEXT(".jpg"));
	if (!T) T = HLLoadTexture(FPaths::ProjectContentDir() / TEXT("Data/UI/") + Name + TEXT(".png")); // art with an alpha (the iris)
	TSharedPtr<FSlateBrush> B;
	if (T) { B = MakeShared<FSlateBrush>(); B->SetResourceObject(T); B->ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY()); B->DrawAs = ESlateBrushDrawType::Image; }
	Art.Add(Name, B);
	return B.Get();
}
FSlateBrush* HLMapBrush(const FString& Name, FBox2f& Region)
{
	struct FMap { TSharedPtr<FSlateBrush> B; FBox2f UV; };
	static TMap<FString, FMap> Maps;
	if (FMap* M = Maps.Find(Name)) { Region = M->UV; return M->B.Get(); }
	FMap M;
	M.UV = FBox2f(FVector2f(0, 0), FVector2f(1, 1));
	FString Text;
	TSharedPtr<FJsonObject> J;
	const FString Dir = FPaths::ProjectContentDir() / TEXT("Data/UI/");
	const TSharedPtr<FJsonObject>* E = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* UV = nullptr;
	if (FFileHelper::LoadFileToString(Text, *(Dir + TEXT("maps.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J.IsValid()
		&& J->TryGetObjectField(Name, E) && (*E)->TryGetArrayField(TEXT("uv"), UV) && UV->Num() == 4)
	{
		M.UV = FBox2f(FVector2f((*UV)[0]->AsNumber(), (*UV)[1]->AsNumber()), FVector2f((*UV)[2]->AsNumber(), (*UV)[3]->AsNumber()));
		if (UTexture2D* T = HLLoadTexture(Dir + Name + TEXT(".jpg")))
		{
			M.B = MakeShared<FSlateBrush>(); M.B->SetResourceObject(T); M.B->ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY()); M.B->DrawAs = ESlateBrushDrawType::Image;
		}
	}
	Region = M.UV;
	return Maps.Add(Name, M).B.Get();
}
void SHLGameUI::Img(float X, float Y, float W, float H, const FSlateBrush* B, const FLinearColor& Tint) const
{
	if (!B) return;
	++Layer; LastKind = 1;
	if (!bMuteUI) FSlateDrawElement::MakeBox(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(Sc, FVector2f(X * Sc, Y * Sc))), B, ESlateDrawEffect::None, Tint);
}

// ------------------------------------------------------------------------------------------------------------- toolkit
void SHLGameUI::Rc(float X, float Y, float W, float H, const FLinearColor& C) const
{
	if (LastKind != 1) { ++Layer; LastKind = 1; }
	if (!bMuteUI) FSlateDrawElement::MakeBox(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(Sc, FVector2f(X * Sc, Y * Sc))), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, C);
}
void SHLGameUI::RRc(float X, float Y, float W, float H, const FLinearColor& C, float Radius) const
{
	// one white rounded brush per radius step, tinted per draw
	static TMap<int32, TSharedPtr<FSlateRoundedBoxBrush>> Brushes;
	const int32 R = FMath::Clamp(FMath::RoundToInt(Radius), 1, 32);
	TSharedPtr<FSlateRoundedBoxBrush>& B = Brushes.FindOrAdd(R);
	if (!B.IsValid()) B = MakeShared<FSlateRoundedBoxBrush>(FLinearColor::White, (float)R);
	++Layer; LastKind = 1;
	if (!bMuteUI) FSlateDrawElement::MakeBox(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(Sc, FVector2f(X * Sc, Y * Sc))), B.Get(), ESlateDrawEffect::None, C);
}
void SHLGameUI::Shadow(float X, float Y, float W, float H, float Spread, float Alpha) const
{
	// stacked rounded boxes, widest and faintest first, offset a little down: reads as a soft shadow
	for (int32 i = 3; i >= 1; --i)
	{
		const float E = Spread * i / 3.f;
		RRc(X - E + 2, Y - E + 4, W + E * 2, H + E * 2, FLinearColor(0, 0, 0, Alpha / 3.f), 4 + E);
	}
}
float SHLGameUI::Section(float X, float Y, float W, float H, const FString& Title) const
{
	RRc(X, Y, W, H, FLinearColor(0.f, 0.f, 0.f, 0.38f), 5);
	RRc(X + 1, Y + 1, W - 2, H - 2, FLinearColor(0.11f, 0.09f, 0.07f, 0.7f), 4);
	if (Title.IsEmpty()) return Y + 6;
	++Layer; LastKind = 1;
	Rc(X + 8, Y + 19, W - 16, 1, FLinearColor(0.75f, 0.6f, 0.3f, 0.35f));
	Tx(X + 8, Y + 4, Title.ToUpper(), 9, FLinearColor(0.85f, 0.7f, 0.35f), true);
	return Y + 25;
}
void SHLGameUI::StatRow(float X, float Y, float W, const FString& Label, const FString& Value, int32 Row, const FLinearColor& ValueColor) const
{
	if (Row % 2 == 0) { ++Layer; LastKind = 1; Rc(X, Y - 1, W, 16, FLinearColor(1.f, 0.9f, 0.7f, 0.045f)); }
	Tx(X + 4, Y, Label, 10, FLinearColor(0.7f, 0.68f, 0.62f));
	Tx(X + W - 4 - TW(Value, 10, true), Y, Value, 10, ValueColor, true);
}

void SHLGameUI::Panel(float X, float Y, float W, float H, const FLinearColor& Bg, const FLinearColor& Border) const
{
	// Art Deco frame: gold rule, dark field, a hairline inset, stepped corner blocks
	Rc(X - 2, Y - 2, W + 4, H + 4, FLinearColor(0, 0, 0, 0.85f));
	++Layer; LastKind = 1;
	Rc(X - 1, Y - 1, W + 2, H + 2, Border);
	++Layer; LastKind = 1;
	Rc(X, Y, W, H, FLinearColor(Bg.R, Bg.G, Bg.B, FMath::Max(Bg.A, 0.94f)));
	if (W < 60 || H < 40) return;
	++Layer; LastKind = 1;
	const FLinearColor Hair(Border.R, Border.G, Border.B, 0.45f);
	Rc(X + 4, Y + 4, W - 8, 1, Hair); Rc(X + 4, Y + H - 5, W - 8, 1, Hair); Rc(X + 4, Y + 4, 1, H - 8, Hair); Rc(X + W - 5, Y + 4, 1, H - 8, Hair);
	for (int32 c = 0; c < 4; ++c)
	{
		const float CX = (c & 1) ? X + W - 9 : X + 1, CY = (c & 2) ? Y + H - 9 : Y + 1;
		Rc(CX + 2, CY + 2, 4, 4, Border);
	}
}
void SHLGameUI::Tx(float X, float Y, const FString& S, int32 Size, const FLinearColor& C, bool bBold, bool bShadow) const
{
	if (S.IsEmpty()) return;
	if (LastKind != 2) { ++Layer; LastKind = 2; }
	const FSlateFontInfo Font = HLFont(Size, bBold);
	if (bShadow) if (!bMuteUI) FSlateDrawElement::MakeText(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(2000, 40), FSlateLayoutTransform(Sc, FVector2f((X + 1) * Sc, (Y + 1) * Sc))), S, Font, ESlateDrawEffect::None, FLinearColor(0, 0, 0, C.A * 0.9f));
	if (!bMuteUI) FSlateDrawElement::MakeText(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(2000, 40), FSlateLayoutTransform(Sc, FVector2f(X * Sc, Y * Sc))), S, Font, ESlateDrawEffect::None, C);
}
float SHLGameUI::TW(const FString& S, int32 Size, bool bBold) const
{
	const TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return FM->Measure(S, HLFont(Size, bBold)).X;
}
void SHLGameUI::TxC(float CX, float Y, const FString& S, int32 Size, const FLinearColor& C, bool bBold) const { Tx(CX - TW(S, Size, bBold) * 0.5f, Y, S, Size, C, bBold); }
float SHLGameUI::TH(int32 Size, bool bBold) const
{
	// the drawn height of a line (cap height plus descender as the font lays it out), cached per size
	static TMap<int32, float> Cache;
	const int32 Key = Size * 2 + (bBold ? 1 : 0);
	if (const float* F = Cache.Find(Key)) return *F;
	const TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return Cache.Add(Key, FM->Measure(TEXT("Hg"), HLFont(Size, bBold)).Y);
}
void SHLGameUI::TxMid(float X, float Y, float W, float H, const FString& S, int32 Size, const FLinearColor& C, bool bBold) const
{
	// centred in the box both ways; the font's line box sits a little low, so the measured height is trimmed a touch
	TxC(X + W * 0.5f, Y + (H - TH(Size, bBold)) * 0.5f + Size * 0.04f, S, Size, C, bBold);
}
void SHLGameUI::Wrap(const FString& S, int32 Size, float MaxW, TArray<FString>& Out) const
{
	TArray<FString> Paras;
	S.ParseIntoArray(Paras, TEXT("\n"), false);
	for (const FString& Para : Paras)
	{
		TArray<FString> Words;
		Para.ParseIntoArray(Words, TEXT(" "), true);
		FString Cur;
		for (const FString& Wd : Words)
		{
			const FString Try = Cur.IsEmpty() ? Wd : Cur + TEXT(" ") + Wd;
			if (!Cur.IsEmpty() && TW(Try, Size) > MaxW) { Out.Add(Cur); Cur = Wd; }
			else Cur = Try;
		}
		Out.Add(Cur);
	}
}
float SHLGameUI::TxWrap(float X, float Y, float W, const FString& S, int32 Size, const FLinearColor& C) const
{
	TArray<FString> Lines;
	Wrap(S, Size, W, Lines);
	const float LH = Size * 1.55f;
	for (int32 i = 0; i < Lines.Num(); ++i) Tx(X, Y + i * LH, Lines[i], Size, C);
	return Lines.Num() * LH;
}
void SHLGameUI::Line(const FVector2D& A, const FVector2D& B, const FLinearColor& C, float Thick) const
{
	++Layer; LastKind = 3;
	TArray<FVector2f> Pts = { FVector2f(A.X, A.Y), FVector2f(B.X, B.Y) };
	if (!bMuteUI) FSlateDrawElement::MakeLines(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(VS.X, VS.Y), FSlateLayoutTransform(Sc, FVector2f(0, 0))), Pts, ESlateDrawEffect::None, C, true, Thick);
}
bool SHLGameUI::Btn(float X, float Y, float W, float H, const FString& Label, bool bEnabled, int32 Size) const
{
	const bool bHot = bEnabled && Hover(X, Y, W, H);
	// the border is at least one screen pixel wide on every side (a 1-unit rule vanished at fractional UI scales)
	const float Bd = FMath::Max(1.f, 1.25f / FMath::Max(Sc, 0.01f));
	Rc(X - Bd, Y - Bd, W + 2 * Bd, H + 2 * Bd, bHot ? FLinearColor(0.95f, 0.78f, 0.38f) : FLinearColor(0.62f, 0.5f, 0.28f));
	++Layer; LastKind = 1;
	Rc(X, Y, W, H, !bEnabled ? FLinearColor(0.1f, 0.1f, 0.1f) : bHot ? (bLDown ? FLinearColor(0.42f, 0.09f, 0.08f) : FLinearColor(0.34f, 0.1f, 0.09f)) : FLinearColor(0.2f, 0.06f, 0.06f));
	if (H >= 18) { ++Layer; LastKind = 1; Rc(X, Y, W, 1, FLinearColor(1, 0.85f, 0.5f, bEnabled ? 0.25f : 0.08f)); }
	TxMid(X, Y, W, H, Label, Size, bEnabled ? ColGold : ColDim);
	if (bHot && bLClick) { bLClick = false; G().Sfx.Ui(TEXT("ui_click")); return true; }
	return false;
}
void SHLGameUI::ScrollBar(float X, float Y, float H, int32 First, int32 Visible, int32 Total) const
{
	// a slim track with a thumb for where the list stands; nothing when it all fits
	if (Total <= Visible || H < 20) return;
	Rc(X, Y, 5, H, FLinearColor(0, 0, 0, 0.45f));
	++Layer; LastKind = 1;
	const float Th = FMath::Max(14.f, H * Visible / Total);
	const float Ty = Y + (H - Th) * FMath::Clamp((float)First / FMath::Max(1, Total - Visible), 0.f, 1.f);
	RRc(X, Ty, 5, Th, FLinearColor(0.85f, 0.66f, 0.3f, 0.9f), 2);
}
void SHLGameUI::Glow(float X, float Y, float W, float H) const
{
	const float P = 0.5f + 0.5f * FMath::Sin(FPlatformTime::Seconds() * 4.0);
	for (int32 r = 3; r >= 1; --r)
	{
		const float G = r * 2.5f;
		RRc(X - G, Y - G, W + 2 * G, H + 2 * G, FLinearColor(1, 0.78f, 0.25f, (0.10f + 0.16f * P) * (4 - r) / 3.f), 6 + G);
		++Layer; LastKind = 1;
	}
}
bool SHLGameUI::CloseBtn(float X, float Y, float S) const
{
	// a drawn cross centred in its square (the letter x sat off-centre in the font's line box)
	const bool bClick = Btn(X, Y, S, S, FString());
	const bool bHot = Hover(X, Y, S, S);
	const float M = S * 0.3f;
	const FLinearColor C = bHot ? FLinearColor(1, 0.92f, 0.65f) : ColGold;
	Line(FVector2D(X + M, Y + M), FVector2D(X + S - M, Y + S - M), C, 1.6f);
	Line(FVector2D(X + S - M, Y + M), FVector2D(X + M, Y + S - M), C, 1.6f);
	return bClick;
}
void SHLGameUI::Bar(float X, float Y, float W, float H, float Pct, const FLinearColor& C, const FString& Label, int32 Size) const
{
	Rc(X - 1, Y - 1, W + 2, H + 2, FLinearColor(0, 0, 0, 1));
	++Layer; LastKind = 1;
	Rc(X, Y, W, H, FLinearColor(0.08f, 0.08f, 0.08f));
	++Layer; LastKind = 1;
	Rc(X, Y, W * FMath::Clamp(Pct, 0.f, 1.f), H, C);
	if (!Label.IsEmpty()) TxMid(X, Y, W, H, Label, Size, ColWhite);
}
bool SHLGameUI::Win(const TCHAR* Id, const FString& Title, float W, float H, bool& bOpen, float DefX, float DefY) const
{
	if (!bOpen) return false;
	const FName Key(Id);
	{
		const double T = FPlatformTime::Seconds();
		double& Seen = WinSeen.FindOrAdd(Key, 0);
		if (T - Seen > 0.25) G().Sfx.Ui(Key == FName(TEXT("bags")) || Key == FName(TEXT("bag")) ? TEXT("ui_bag") : TEXT("ui_open"));
		Seen = T;
	}
	FVector2D& P = WinPos.FindOrAdd(Key, FVector2D(DefX, DefY));
	if (DragWin == Key) { if (bLDown) P = Mouse - DragOff; else DragWin = NAME_None; }
	P.X = FMath::Clamp(P.X, 0.0, FMath::Max(0.0, VS.X - W)); P.Y = FMath::Clamp(P.Y, 0.0, FMath::Max(0.0, VS.Y - H));
	Block(P.X, P.Y, W, H);
	++Layer; LastKind = 0;
	Shadow(P.X, P.Y, W, H, 14, 0.5f);
	Panel(P.X, P.Y, W, H);
	if (const FSlateBrush* Bg = HLArtBrush(TEXT("ui_panel"))) Img(P.X, P.Y, W, H, Bg, FLinearColor(0.5f, 0.5f, 0.5f, 0.92f));
	++Layer; LastKind = 1;
	Rc(P.X, P.Y, W, 26, FLinearColor(0.16f, 0.05f, 0.05f));

	++Layer; LastKind = 1;
	Rc(P.X, P.Y + 26, W, 1, FLinearColor(0.75f, 0.6f, 0.3f)); Rc(P.X, P.Y + 28, W, 1, FLinearColor(0.75f, 0.6f, 0.3f, 0.4f));
	TxC(P.X + W * 0.5f, P.Y + 3, Title, 14, ColGold, true);
	if (CloseBtn(P.X + W - 23, P.Y + 4, 18)) { bOpen = false; G().Sfx.Ui(TEXT("ui_close")); return false; }
	if (bLDown && DragWin.IsNone() && !bDragging && Hover(P.X, P.Y, W - 26, 24) && FVector2D::Distance(DownPos, Mouse) < 2 && Hover(P.X, P.Y, W, 24)) { DragWin = Key; DragOff = Mouse - P; }
	WX = P.X + 10; WY = P.Y + 32;
	return true;
}
int32 SHLGameUI::ItemIcon(float X, float Y, float S, const FHLItem* It, int32 Count) const
{
	Rc(X, Y, S, S, FLinearColor(0.25f, 0.22f, 0.16f));
	++Layer; LastKind = 1;
	Rc(X + 1, Y + 1, S - 2, S - 2, FLinearColor(0.07f, 0.07f, 0.08f));
	if (!It) return 0;
	const FLinearColor QC = HLQualityColor(It->Q);
	++Layer; LastKind = 1;
	Rc(X + 3, Y + 3, S - 6, S - 6, FLinearColor(QC.R * 0.28f, QC.G * 0.28f, QC.B * 0.28f));
	if (const FSlateBrush* IB = HLIconBrush(It->Icon))
	{
		++Layer; LastKind = 1;
		if (!bMuteUI) FSlateDrawElement::MakeBox(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(S - 4, S - 4), FSlateLayoutTransform(Sc, FVector2f((X + 2) * Sc, (Y + 2) * Sc))), IB, ESlateDrawEffect::None, FLinearColor::White);
		// quality frame
		++Layer; LastKind = 1;
		Rc(X, Y, S, 1.5f, QC); Rc(X, Y + S - 1.5f, S, 1.5f, QC); Rc(X, Y, 1.5f, S, QC); Rc(X + S - 1.5f, Y, 1.5f, S, QC);
	}
	else
	{
		FString Glyph = It->Kind == TEXT("weapon") ? It->Type.Left(2).ToUpper() : It->Kind == TEXT("armor") ? It->Slot.Left(2).ToUpper() : It->Name.Left(2);
		TxC(X + S * 0.5f, Y + S * 0.5f - 9, Glyph, 10, QC, true);
	}
	if (Count > 1) Tx(X + S - TW(FString::FromInt(Count), 9) - 3, Y + S - 15, FString::FromInt(Count), 9, ColWhite);
	if (Hover(X, Y, S, S))
	{
		TipItem = It; TipPos = Mouse;
		if (bLClick) { bLClick = false; return 1; }
		if (bRClick) { bRClick = false; return 2; }
	}
	return 0;
}
bool SHLGameUI::Project(const FVector& WorldPos, FVector2D& Out) const
{
	FVector2D Px;
	if (!PC || !PC->ProjectWorldLocationToScreen(WorldPos, Px, false)) return false;
	Out = FVector2D(Px.X / ViewPx.X * VS.X, Px.Y / ViewPx.Y * VS.Y);
	return Out.X > -200 && Out.Y > -200 && Out.X < VS.X + 200 && Out.Y < VS.Y + 200;
}

// ------------------------------------------------------------------------------------------------------------- input
FReply SHLGameUI::OnKeyDown(const FGeometry& Geo, const FKeyEvent& E)
{
	const FKey K = E.GetKey();
	FHLGame& Gm = G();
	if (!Gm.bInWorld)
	{
		FString& Box = bRestoreFocus ? RestoreEntry : NameEntry;
		if (K == EKeys::BackSpace && Box.Len()) Box.LeftChopInline(1);
		return FReply::Handled();
	}
	if (bTyping)
	{
		if (K == EKeys::Enter) { if (!ChatEntry.IsEmpty()) Gm.PlayerChat(ChatEntry); ChatEntry.Reset(); bTyping = false; ChatScroll = 0; }
		else if (K == EKeys::Escape) { ChatEntry.Reset(); bTyping = false; }
		else if (K == EKeys::BackSpace && ChatEntry.Len()) ChatEntry.LeftChopInline(1);
		return FReply::Handled();
	}
	if (E.IsRepeat()) return FReply::Handled();
	FHLSettings& St = FHLSettings::Get();
	// settings window waiting for a key to bind: this key is the answer (Esc cancels, Backspace clears)
	if (BindWait >= 0)
	{
		if (K == EKeys::BackSpace) St.Bind((EHLAct)BindWait, EKeys::Invalid, bBindAlt);
		else if (K != EKeys::Escape && K != EKeys::Enter && K != EKeys::Slash) St.Bind((EHLAct)BindWait, K, bBindAlt);
		BindWait = -1;
		return FReply::Handled();
	}
	Down.Add(K);
	for (int32 i = 0; i < 12; ++i) if (St.Is(K, EHLAct((int32)EHLAct::Bar1 + i))) { Gm.UseBar(i); return FReply::Handled(); }
	if (bSearchFocus)
	{
		if (K == EKeys::BackSpace && AuctionSearch.Len()) AuctionSearch.LeftChopInline(1);
		else if (K == EKeys::Enter || K == EKeys::Escape) bSearchFocus = false;
		return FReply::Handled();
	}
	if (K == EKeys::Enter) { bTyping = true; Down.Reset(); }
	else if (K == EKeys::Slash) { bTyping = true; ChatEntry.Reset(); Down.Reset(); }
	else if (St.Is(K, EHLAct::Reply) && !Gm.LastWhisperFrom.IsEmpty()) { bTyping = true; bSwallowChar = true; ChatEntry = FString::Printf(TEXT("/w %s "), *Gm.LastWhisperFrom); Down.Reset(); }
	else if (St.Is(K, EHLAct::Target)) Gm.TabTarget();
	else if (St.Is(K, EHLAct::Jump)) Gm.Input.bJump = true;
	else if (St.Is(K, EHLAct::Interact)) Interact();
	else if (St.Is(K, EHLAct::Sit)) { if (FHLUnit* U = Gm.PlayerUnit()) U->bSitting = !U->bSitting; }
	else if (St.Is(K, EHLAct::AutoAttack)) { if (FHLUnit* U = Gm.PlayerUnit()) { const FHLUnit* T = Gm.Unit(U->TargetId); if (T && T->Kind == EHLKind::Mob && !T->bDead) U->bAutoAttack = !U->bAutoAttack; } }
	else if (St.Is(K, EHLAct::Character)) { bChar = !bChar; bMenu = false; }
	else if (St.Is(K, EHLAct::Bags)) { bBags = !bBags; bMenu = false; }
	else if (St.Is(K, EHLAct::Quests)) { bQuests = !bQuests; bMenu = false; }
	else if (St.Is(K, EHLAct::Spellbook)) { bSpells = !bSpells; bMenu = false; }
	else if (St.Is(K, EHLAct::Map)) { bMap = !bMap; bMenu = false; }
	else if (St.Is(K, EHLAct::Social)) { bWho = !bWho; Gm.bWhoOpen = bWho; bMenu = false; }
	else if (St.Is(K, EHLAct::Talents)) { bTalents = !bTalents; bMenu = false; }
	else if (St.Is(K, EHLAct::Workbench)) { bCraft = !bCraft; bMenu = false; }
	else if (St.Is(K, EHLAct::Help)) bHelp = !bHelp;
	else if (K == EKeys::Escape)
	{
		if (Gm.OpenNpcUnitId || !Gm.OpenNpcMode.IsEmpty()) { Gm.OpenNpcUnitId = 0; Gm.OpenNpcMode.Reset(); }
		else if (bChar || bBags || bQuests || bSpells || bMap || bWho || bInspect || bTalents || bHelp || bCraft || bSettings) { bChar = bBags = bQuests = bSpells = bMap = bWho = bInspect = bTalents = bHelp = bCraft = bSettings = false; Gm.bWhoOpen = false; }
		else if (Gm.PlayerUnit() && Gm.PlayerUnit()->TargetId) Gm.TargetUnit(0);
		else bMenu = !bMenu;
	}
	return FReply::Handled();
}
FReply SHLGameUI::OnKeyUp(const FGeometry& Geo, const FKeyEvent& E) { Down.Remove(E.GetKey()); return FReply::Handled(); }
FReply SHLGameUI::OnKeyChar(const FGeometry& Geo, const FCharacterEvent& E)
{
	const TCHAR C = E.GetCharacter();
	if (C < 32 || C == 127) return FReply::Handled();
	if (!G().bInWorld)
	{
		if (bRestoreFocus) { if (RestoreEntry.Len() < 40 && (FChar::IsAlnum(C) || C == '_')) RestoreEntry.AppendChar(FChar::ToLower(C)); }
		else if (NameEntry.Len() < 12 && (FChar::IsAlpha(C))) NameEntry.AppendChar(C);
		return FReply::Handled();
	}
	if (bSwallowChar) { bSwallowChar = false; return FReply::Handled(); }
	if (bTyping && ChatEntry.Len() < 240) { if (!(ChatEntry.IsEmpty() && C == '\r')) ChatEntry.AppendChar(C); }
	else if (bSearchFocus && C >= 32 && AuctionSearch.Len() < 40) AuctionSearch.AppendChar(C);
	return FReply::Handled();
}
FReply SHLGameUI::OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E)
{
	Mouse = Geo.AbsoluteToLocal(E.GetScreenSpacePosition()) / ScaleFor(Geo);
	const bool bLeft = E.GetEffectingButton() == EKeys::LeftMouseButton, bRight = E.GetEffectingButton() == EKeys::RightMouseButton;
	if (!bLeft && !bRight) return FReply::Handled();
	if (bLeft) bLDown = true; else bRDown = true;
	DownPos = Mouse; DragDist = 0;
	bDownOverUi = !G().bInWorld || OverUi(Mouse);
	bDragging = !bDownOverUi;
	FReply R = FReply::Handled().SetUserFocus(AsShared(), EFocusCause::Mouse);
	if (bDragging) R = R.CaptureMouse(AsShared()).UseHighPrecisionMouseMovement(AsShared());
	return R;
}
FReply SHLGameUI::OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& E)
{
	const bool bLeft = E.GetEffectingButton() == EKeys::LeftMouseButton, bRight = E.GetEffectingButton() == EKeys::RightMouseButton;
	if (!bDragging) Mouse = Geo.AbsoluteToLocal(E.GetScreenSpacePosition()) / ScaleFor(Geo);
	if (bLeft) { bLDown = false; if (DragDist < 6) bLClick = true; }
	if (bRight) { bRDown = false; if (DragDist < 6) bRClick = true; }
	if (!bLDown && !bRDown && bDragging) { bDragging = false; Mouse = DownPos; return FReply::Handled().ReleaseMouseCapture(); }
	return FReply::Handled();
}
FReply SHLGameUI::OnMouseMove(const FGeometry& Geo, const FPointerEvent& E)
{
	if (bDragging && (bLDown || bRDown))
	{
		FVector2D D = E.GetCursorDelta();
		if (D.SizeSquared() > 250.0 * 250.0) D = FVector2D::ZeroVector; // pointer-lock warp
		DragDist += D.Size();
		World->CamYaw += D.X * 0.25f;
		World->CamPitch = FMath::Clamp(World->CamPitch + D.Y * 0.2f, -5.f, 65.f);
		return FReply::Handled();
	}
	Mouse = Geo.AbsoluteToLocal(E.GetScreenSpacePosition()) / ScaleFor(Geo);
	return FReply::Handled();
}
FReply SHLGameUI::OnMouseWheel(const FGeometry& Geo, const FPointerEvent& E)
{
	if (OverUi(Mouse)) Wheel += E.GetWheelDelta();
	else World->CamDist = FMath::Clamp(World->CamDist - E.GetWheelDelta() * 1.5f, 3.f, 40.f);
	return FReply::Handled();
}

void SHLGameUI::PreTick(float Dt)
{
	FHLGame& Gm = G();
	FHLInput& In = Gm.Input;
	const bool bJump = In.bJump;
	In = FHLInput(); In.bJump = bJump;
	if (!Gm.bInWorld || bTyping) return;
	const FHLSettings& St = FHLSettings::Get();
	auto K = [&](EHLAct A) { return St.Held(Down, A) ? 1.f : 0.f; };
	const bool bSteer = bDragging && (bRDown || (bLDown && St.bFaceCamera && DragDist >= 6));
	In.Forward = FMath::Clamp(K(EHLAct::Forward) + ((bLDown && bRDown && bDragging) ? 1.f : 0.f), 0.f, 1.f) - K(EHLAct::Back);
	In.Strafe = K(EHLAct::StrafeRight) - K(EHLAct::StrafeLeft);
	const float AD = K(EHLAct::TurnRight) - K(EHLAct::TurnLeft);
	if (bSteer) In.Strafe += AD; else In.Turn = AD;
	In.bMouseSteer = bSteer; In.CamYaw = World->CamYaw;
	if (!bSteer && In.Turn != 0) World->CamYaw += In.Turn * 200.f * Dt;
	// camera drifts back behind the character while running without the mouse held
	if (!bDragging && In.Forward > 0) if (const FHLUnit* U = Gm.PlayerUnit()) World->CamYaw += FMath::FindDeltaAngleDegrees(World->CamYaw, U->Yaw) * FMath::Min(1.f, Dt * 2.5f);
}

void SHLGameUI::Interact() const
{
	FHLGame& Gm = G();
	FHLUnit* P = Gm.PlayerUnit();
	if (!P || P->bDead) return;
	// nearest lootable corpse, NPC or object
	float Best = 7; int32 BestUnit = 0, BestObj = -1;
	for (FHLUnit* U : Gm.UnitList)
	{
		if (U->Area != P->Area) continue;
		const bool bCorpse = U->Kind == EHLKind::Mob && U->bDead && U->bLootable && U->TappedBy == Gm.PlayerUnitId;
		const bool bNpc = U->Kind == EHLKind::Npc && U->Npc && !(U->Npc->HasRole(TEXT("guard")) && U->Npc->Roles.Num() == 1);
		if (!bCorpse && !bNpc) continue;
		const float D = Gm.Dist(*P, *U) - (bCorpse ? 2.f : 0.f);
		if (D < Best) { Best = D; BestUnit = U->Id; BestObj = -1; }
	}
	const FHLAreaState& A = Gm.Areas[P->Area];
	for (int32 o = 0; o < A.Objects.Num(); ++o)
	{
		if (!A.Objects[o].bActive) continue;
		const float D = FVector2D::Distance(P->Pos, A.Objects[o].Def->Pos) - (A.Objects[o].Def->bBig ? 5.f : 0.f);
		if (D < Best && D < 6.f) { Best = D; BestObj = o; BestUnit = 0; }
	}
	if (BestObj >= 0) Gm.ObjectInteract(P->Area, BestObj);
	else if (BestUnit) { Gm.bLootAll = FSlateApplication::Get().GetModifierKeys().IsShiftDown(); Gm.InteractUnit(BestUnit); }
}

// ------------------------------------------------------------------------------------------------------------- paint
int32 SHLGameUI::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	DL = &Out; PGeo = Geo; Layer = LayerId; LastKind = 0;
	{ static const bool bHide = FParse::Param(FCommandLine::Get(), TEXT("HLHideUI")); bMuteUI = bHide; } // trailer capture: the logic runs, nothing is drawn
	Sc = ScaleFor(Geo);
	VS = FVector2D(Geo.GetLocalSize()) / Sc;
	PC = World && World->GetWorld() ? World->GetWorld()->GetFirstPlayerController() : nullptr;
	if (GEngine && GEngine->GameViewport) { FVector2D V; GEngine->GameViewport->GetViewportSize(V); if (V.X > 0 && V.Y > 0) ViewPx = V; }
	UiRectsNext.Reset(); Picks.Reset(); TipItem = nullptr; TipText.Reset(); TipExtra.Reset();
	FHLGame& Gm = G();
	{ static bool bOnce = false; if (!bOnce && Gm.bInWorld) { bOnce = true; if (Gm.InOpening()) bHelp = false; /* the tour card teaches instead */ if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenBags"))) { bBags = true; bChar = true; } if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenTalents"))) bTalents = true; if (FParse::Param(FCommandLine::Get(), TEXT("HLCraftTest"))) bCraft = true; if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenSpells"))) bSpells = true; if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenFriends"))) { bWho = true; WhoTab = 1; } if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenNews"))) { bWho = true; WhoTab = 3; }
		if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenAuction"))) for (const FHLUnit* N : Gm.UnitList) if (N->Npc && N->Npc->HasRole(TEXT("auctioneer"))) { Gm.OpenNpcUnitId = N->Id; Gm.OpenNpcMode = TEXT("auction"); AuctionSearch = FParse::Param(FCommandLine::Get(), TEXT("HLAuctionFlip")) ? FString() : FString(TEXT("boots")); break; }
		if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenWorldMap"))) { bMap = true; bWorldMap = true; }
		if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenChar"))) bChar = true;
		if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenTrainer"))) for (const FHLUnit* N : Gm.UnitList) if (N->Npc && N->Npc->HasRole(TEXT("trainer")) && FHLData::Get().Class(N->Npc->Trains) == Gm.Player.Cls) { Gm.OpenNpcUnitId = N->Id; Gm.OpenNpcMode = TEXT("trainer"); break; }
		{ int32 Tab = -1; if (FParse::Value(FCommandLine::Get(), TEXT("HLOpenSettings="), Tab)) { bSettings = true; SettingsTab = FMath::Clamp(Tab, 0, 3); bHelp = false; } }
		if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenMap"))) { bMap = true; if (const FVector2D* B = Gm.Areas[0].Def->Pois.Find(TEXT("bank"))) Gm.SetMark(0, *B, TEXT("First National Bank")); }
		if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenGuard"))) for (const FHLUnit* N : Gm.UnitList) if (N->Npc && N->Npc->HasRole(TEXT("guard"))) { Gm.OpenNpcUnitId = N->Id; Gm.OpenNpcMode = TEXT("gossip"); break; } } } // screenshot checks
	if (!Gm.Areas.Num()) { Tx(40, 40, TEXT("Hollywoodland failed to load its data: ") + World->LoadError, 14, ColRed); return Layer; }
	if (!Gm.bInWorld) DrawTitle();
	else
	{
		DrawWorldOverlay();
		DrawHud();
		DrawWindows();
		DrawPopups();
		// the ride: an iris closes on the street (silent-picture style), a title card on black, an iris opens on the new stop
		auto Iris = [&](float Open)
		{
			// Open: 1 = the whole screen, 0 = black. ui_iris.png is clear inside a circle of 0.4 of its width
			const float Rmax = FMath::Sqrt(VS.X * VS.X + VS.Y * VS.Y) * 0.5f + 4, R = Rmax * FMath::Clamp(Open, 0.f, 1.f);
			const FVector2D C(VS.X * 0.5f, VS.Y * 0.47f);
			const float Wd = R / 0.4f;
			const FLinearColor K(0.01f, 0.008f, 0.006f, 1);
			++Layer; LastKind = 1;
			if (R < 1) { Rc(0, 0, VS.X, VS.Y, K); return; }
			if (const FSlateBrush* IB = HLArtBrush(TEXT("ui_iris"))) Img(C.X - Wd * 0.5f, C.Y - Wd * 0.5f, Wd, Wd, IB, FLinearColor::White);
			Rc(0, 0, VS.X, FMath::Max(0.f, C.Y - Wd * 0.5f) + 1, K); Rc(0, C.Y + Wd * 0.5f - 1, VS.X, VS.Y, K);
			Rc(0, 0, FMath::Max(0.f, C.X - Wd * 0.5f) + 1, VS.Y, K); Rc(C.X + Wd * 0.5f - 1, 0, VS.X, VS.Y, K);
		};
		const float Since = float(Gm.Now - Gm.RideUntil);
		if (Gm.RideStop < 0 && Gm.RideUntil > 0 && Since >= 0 && Since < 0.9f) Iris(FMath::InterpEaseOut(0.f, 1.f, Since / 0.9f, 2.f));
		if (Gm.RideStop >= 0 && Gm.Areas[0].Def->Stops.IsValidIndex(Gm.RideStop))
		{
			const float T = float(Gm.Now - Gm.RideStart);
			Iris(1.f - FMath::InterpEaseIn(0.f, 1.f, FMath::Clamp(T / 0.8f, 0.f, 1.f), 2.f));
			const float A = FMath::Clamp((T - 0.8f) / 0.4f, 0.f, 1.f) * FMath::Clamp(float(Gm.RideUntil - Gm.Now) / 0.4f, 0.f, 1.f);
			++Layer; LastKind = 1;
			const FHLRedcarStop& St = Gm.Areas[0].Def->Stops[Gm.RideStop];
			TxC(VS.X * 0.5f, VS.Y * 0.42f, TEXT("PACIFIC ELECTRIC"), 30, FLinearColor(0.85f, 0.2f, 0.15f, A), true);
			TxC(VS.X * 0.5f, VS.Y * 0.42f + 46, FString::Printf(TEXT("Next stop: %s"), *St.Name), 16, FLinearColor(1, 0.92f, 0.75f, A));
			TxC(VS.X * 0.5f, VS.Y * 0.42f + 72, St.Note, 11, FLinearColor(0.75f, 0.7f, 0.6f, A));
		}
		{
			// -HLClickTest (with -HLAuto=<class>): right-click the ground below the screen centre with click-to-move on, log where the player went
			static const bool bClickTest = FParse::Param(FCommandLine::Get(), TEXT("HLClickTest"));
			static int32 Step = 0; static double At = 0;
			if (bClickTest && Gm.PlayerUnit())
			{
				const FHLUnit& P = *Gm.PlayerUnit();
				if (Step == 0 && At == 0) At = Gm.Now + 4;
				if (Step == 0 && Gm.Now > At)
				{
					FHLSettings::Get().bClickMove = true;
					DownPos = FVector2D(VS.X * 0.5f, VS.Y * 0.8f); bRClick = true; bDownOverUi = false; Picks.Reset();
					UE_LOG(LogTemp, Display, TEXT("HL_CLICK start at %.1f,%.1f yaw=%.0f"), P.Pos.X, P.Pos.Y, P.Yaw);
					Step = 1; At = Gm.Now + 8;
				}
				else if (Step == 1 && Gm.Now > At)
				{
					UE_LOG(LogTemp, Display, TEXT("HL_CLICK goal %.1f,%.1f (set=%d) now at %.1f,%.1f d=%.1f"), Gm.ClickGoal.X, Gm.ClickGoal.Y, Gm.bClickGoal ? 1 : 0, P.Pos.X, P.Pos.Y, FVector2D::Distance(P.Pos, Gm.ClickGoal));
					FHLSettings::Get().bClickMove = false;
					Step = 2;
				}
			}
		}
		HandleWorldClick();
		// -HLTipTest: pin an item card (an equippable auction item) for screenshots
		static const bool bTipTest = FParse::Param(FCommandLine::Get(), TEXT("HLTipTest"));
		if (bTipTest && !TipItem) for (const FHLAuction& A : Gm.Auctions) if (A.Item && A.Item->IsEquip() && A.Item->Q >= 2) { TipItem = A.Item; TipPos = FVector2D(700, 620); TipExtra = TEXT("Last 6 sales: average 2.40, low 1.90, high 3.10."); break; }
	}
	DrawTooltip();
	UiRects = UiRectsNext;
	bLClick = bRClick = false; Wheel = 0;
	return Layer + 1;
}

void SHLGameUI::DrawPreview(float X, float Y, float W, float H, const FHLCharLook& L, const FString& Clip) const
{
	// a drag that started on the figure turns it
	const bool bFromHere = DownPos.X >= X && DownPos.X < X + W && DownPos.Y >= Y && DownPos.Y < Y + H;
	if (bLDown && bFromHere)
	{
		if (bPreviewDrag) PreviewYaw += (Mouse.X - PreviewLastX) * 1.2f;
		bPreviewDrag = true; PreviewLastX = Mouse.X;
	}
	else bPreviewDrag = false;
	Rc(X - 1, Y - 1, W + 2, H + 2, FLinearColor(0.55f, 0.45f, 0.25f));
	Rc(X, Y, W, H, AHLWorldActor::StageBackColor(L.App.Skin));
	UTextureRenderTarget2D* RT = World ? World->Preview(L, PreviewYaw, Clip) : nullptr;
	if (!RT) { TxC(X + W * 0.5f, Y + H * 0.5f - 8, TEXT("(character art not loaded)"), 10, ColDim); return; }
	if (!PreviewBrush.IsValid() || PreviewBrush->GetResourceObject() != RT)
	{
		PreviewBrush = MakeShared<FSlateBrush>();
		PreviewBrush->SetResourceObject(RT);
		PreviewBrush->ImageSize = FVector2D(RT->SizeX, RT->SizeY);
		PreviewBrush->DrawAs = ESlateBrushDrawType::Image;
	}
	++Layer; LastKind = 1;
	// the capture's alpha is not coverage: draw it opaque
	if (!bMuteUI) FSlateDrawElement::MakeBox(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(Sc, FVector2f(X * Sc, Y * Sc))), PreviewBrush.Get(), ESlateDrawEffect::IgnoreTextureAlpha, FLinearColor::White);
	if (Hover(X, Y, W, H) && !bPreviewDrag) TxC(X + W * 0.5f, Y + H - 18, TEXT("drag to turn"), 9, FLinearColor(1, 0.9f, 0.7f, 0.7f));
}

// a new character starts on the Nurse: heals, fights, and shows off the most of the game
static int32 NurseClass()
{
	const FHLData& D = FHLData::Get();
	for (int32 i = 0; i < D.Classes.Num(); ++i) if (D.Classes[i].Id == TEXT("nurse")) return i;
	return 0;
}

void SHLGameUI::DrawCreator(float X, float Y, float W, bool bPreview) const
{
	FHLChars& C = FHLChars::Get();
	if (!C.Ready()) return;
	if (!bNewLookSet) { NewLook = FHLAppearance::FromSeed(FMath::Rand()); bNewLookSet = true; }
	FHLAppearance& A = NewLook;
	const FHLChars::FSex& Sx = C.Sexes[A.Sex & 1];
	const float Pw = bPreview ? 210 : 0, Ph = bPreview ? 315 : 0;
	Tx(X + 4, Y, TEXT("Appearance"), 14, ColGold, true);
	const TMap<FString, const FHLItem*> NoGear;
	if (bPreview) DrawPreview(X + (W - Pw) * 0.5f, Y + 24, Pw, Ph, C.LookFromGear(A, NoGear, false), TEXT("Idle"));
	float Ry = Y + 28 + (bPreview ? Ph + 6 : 0);
	const float Bx = X + 96;
	Tx(X + 6, Ry + 4, TEXT("Body"), 11, ColDim);
	for (int32 s = 0; s < 2; ++s)
	{
		const float Sx0 = Bx + s * 108;
		if (s == A.Sex) Rc(Sx0 - 2, Ry - 2, 104, 26, ColGold);
		if (Btn(Sx0, Ry, 100, 22, s ? TEXT("Woman") : TEXT("Man"), true, 11) && A.Sex != s) A.Sex = s;
	}
	Ry += 28;
	auto Step = [&](const TCHAR* Label, int32& V, int32 N, const FString& Shown, const FLinearColor* SwA = nullptr, const FLinearColor* SwB = nullptr)
	{
		if (N <= 0) return;
		V = FMath::Clamp(V, 0, N - 1);
		Tx(X + 6, Ry + 4, Label, 11, ColDim);
		if (Btn(Bx, Ry, 24, 22, TEXT("<"), true, 12)) V = (V + N - 1) % N;
		if (SwA && SwB) { Rc(Bx + 40, Ry + 3, 70, 16, *SwA); Rc(Bx + 114, Ry + 3, 70, 16, *SwB); }
		else TxC(Bx + 106, Ry + 4, Shown, 11, ColWhite);
		if (Btn(Bx + 188, Ry, 24, 22, TEXT(">"), true, 12)) V = (V + 1) % N;
		Ry += 28;
	};
	Step(TEXT("Skin"), A.Skin, Sx.Skins.Num(), FHLChars::SkinNames().IsValidIndex(A.Skin) ? FHLChars::SkinNames()[A.Skin] : FString::Printf(TEXT("%d"), A.Skin + 1));
	Step(TEXT("Hair"), A.Hair, Sx.Hair.Num(), Sx.Hair.IsValidIndex(A.Hair) ? Sx.Hair[A.Hair].Name : FString());
	Step(TEXT("Hair colour"), A.HairColor, FHLChars::HairColors().Num(), FHLChars::HairColorNames().IsValidIndex(A.HairColor) ? FHLChars::HairColorNames()[A.HairColor] : FString());
	// (no face or clothes-colour rows: the v2 faces read as one, and the v2 clothes are painted per look, not dyed)
	if (Btn(Bx, Ry + 2, 212, 24, TEXT("Surprise me"), true, 11)) A = FHLAppearance::FromSeed(FMath::Rand(), A.Sex);
}

// what the character-select cards show for a saved slot, read from the save JSON (cached per slot)
struct FHLSlotInfo { bool bValid = false; FString Name, ClsId; int32 Level = 1; double Played = 0; FHLAppearance Look; TMap<FString, const FHLItem*> Equip; };
static const FHLSlotInfo& HLSlotInfo(int32 Slot, const FString& Json)
{
	static FHLSlotInfo Cache[3]; static uint32 Hash[3] = { 1, 1, 1 };
	FHLSlotInfo& I = Cache[FMath::Clamp(Slot, 0, 2)];
	const uint32 H = Json.IsEmpty() ? 0 : GetTypeHash(Json);
	if (Hash[Slot] == H) return I;
	Hash[Slot] = H; I = FHLSlotInfo();
	TSharedPtr<FJsonObject> O;
	if (Json.IsEmpty() || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), O) || !O.IsValid()) return I;
	I.bValid = O->TryGetStringField(TEXT("name"), I.Name) && !I.Name.IsEmpty();
	O->TryGetStringField(TEXT("class"), I.ClsId);
	double N = 1; if (O->TryGetNumberField(TEXT("level"), N)) I.Level = (int32)N;
	O->TryGetNumberField(TEXT("played"), I.Played);
	const TSharedPtr<FJsonObject>* Lk = nullptr;
	if (O->TryGetObjectField(TEXT("look"), Lk))
	{
		auto Get = [&](const TCHAR* K, int32 Max) { double V = 0; (*Lk)->TryGetNumberField(K, V); return FMath::Clamp((int32)V, 0, Max); };
		I.Look.Sex = Get(TEXT("sex"), 1); I.Look.Skin = Get(TEXT("skin"), 7); I.Look.Face = Get(TEXT("face"), 11);
		I.Look.Hair = Get(TEXT("hair"), 11); I.Look.HairColor = Get(TEXT("hairc"), 7); I.Look.Outfit = Get(TEXT("outfit"), 7);
	}
	else { FString Pid; O->TryGetStringField(TEXT("player_id"), Pid); I.Look = FHLAppearance::FromSeed(GetTypeHash(Pid)); }
	const TSharedPtr<FJsonObject>* Eq = nullptr;
	if (O->TryGetObjectField(TEXT("equip"), Eq)) for (auto& KV : (*Eq)->Values) if (const FHLItem* It = FHLData::Get().Item(KV.Value->AsString())) I.Equip.Add(FString(KV.Key), It);
	return I;
}

void SHLGameUI::DrawTitle() const
{
	FHLGame& Gm = G();
	const FHLData& D = FHLData::Get();
	FHLChars& Chars = FHLChars::Get();
	static bool bArgs = false;
	if (!bArgs) { bArgs = true; if (FParse::Param(FCommandLine::Get(), TEXT("HLCreateShot"))) { TitleMode = 1; SelClass = NurseClass(); NameEntry = TEXT("Ruby"); } }
	const bool b3D = Chars.Ready() && World && World->bSaveChecked;
	// -HLSpin (trailer capture): the figure turns slowly on its stand
	{ static const bool bSpin = FParse::Param(FCommandLine::Get(), TEXT("HLSpin")); if (bSpin && !bPreviewDrag) PreviewYaw = FMath::Fmod(PreviewYaw + FApp::GetDeltaTime() * 28.f, 360.f); }
	// until the character art and the saves are in: the poster
	if (!b3D)
	{
		if (const FSlateBrush* Art = HLArtBrush(TEXT("ui_title")))
		{
			const float Ar = 16.f / 9.f, Cw = FMath::Max(VS.X, VS.Y * Ar), Ch = Cw / Ar;
			Img((VS.X - Cw) * 0.5f, (VS.Y - Ch) * 0.5f, Cw, Ch, Art);
		}
		TxC(VS.X * 0.5f + 3, 27, TEXT("HOLLYWOODLAND"), 64, FLinearColor(0, 0, 0, 0.8f), true);
		TxC(VS.X * 0.5f, 24, TEXT("HOLLYWOODLAND"), 64, ColGold, true);
		const float Dots = FMath::Fmod(FPlatformTime::Seconds(), 1.5) / 0.5;
		TxC(VS.X * 0.5f, VS.Y - 80, FString(TEXT("Raising the curtain")) + FString::ChrN((int32)Dots + 1, '.'), 14, FLinearColor(1, 0.92f, 0.75f));
		return;
	}
	// the title in the top-left corner (the sign has the top of the frame)
	Tx(24 + 2, 14 + 2, TEXT("HOLLYWOODLAND"), 34, FLinearColor(0, 0, 0, 0.75f), true);
	Tx(24, 14, TEXT("HOLLYWOODLAND"), 34, ColGold, true);
	Tx(26, 64, TEXT("Hollywood, California 1930."), 11, FLinearColor(1, 0.95f, 0.85f), true);
	if (SelSlot < 0) { SelSlot = 0; for (int32 s = 0; s < 3; ++s) if (!World->SavedSlots[s].IsEmpty()) { SelSlot = s; break; } }
	// the very first visit (no characters at all): straight into making one; Back returns to an empty select with the button glowing
	static bool bFirstVisit = true;
	const bool bNoChars = World->SavedSlots[0].IsEmpty() && World->SavedSlots[1].IsEmpty() && World->SavedSlots[2].IsEmpty();
	if (bFirstVisit) { bFirstVisit = false; if (bNoChars && TitleMode == 0) { TitleMode = 1; SelClass = NurseClass(); bNewLookSet = false; NameEntry.Reset(); PreviewYaw = -18.f; } }
	// drag on the open scene turns the figure
	{
		const bool bFree = !Hover(VS.X - 380, 0, 380, VS.Y) && !Hover(0, 0, 440, VS.Y) && Mouse.Y > 100 && Mouse.Y < VS.Y - 110;
		if (bLDown && (bPreviewDrag || bFree)) { if (bPreviewDrag) PreviewYaw += (Mouse.X - PreviewLastX) * 0.8f; bPreviewDrag = true; PreviewLastX = Mouse.X; }
		else bPreviewDrag = false;
	}
	const bool bBlink = FMath::Fmod(FPlatformTime::Seconds(), 1.0) < 0.5;
	if (TitleMode == 0)
	{
		// ---------------------------------------------------------------- character select
		const FHLSlotInfo& Sel = HLSlotInfo(SelSlot, World->SavedSlots[SelSlot]);
		if (Sel.bValid) { const FHLCharLook L = Chars.LookFromGear(Sel.Look, Sel.Equip, false); World->Showcase(&L, PreviewYaw, TEXT("Idle")); }
		else World->Showcase(nullptr, 0, FString());
		const float W = 350, X = VS.X - W - 16, Y = 110, H = 3 * 78 + 190;
		Block(X, Y, W, H); Panel(X, Y, W, H, FLinearColor(0.04f, 0.035f, 0.03f, 0.9f), FLinearColor(0.72f, 0.56f, 0.28f));
		TxC(X + W * 0.5f, Y + 10, TEXT("Your Characters"), 15, ColGold, true);
		for (int32 s = 0; s < 3; ++s)
		{
			const FHLSlotInfo& I = HLSlotInfo(s, World->SavedSlots[s]);
			const float Cy = Y + 40 + s * 78;
			const bool bSel = s == SelSlot, bH = Hover(X + 12, Cy, W - 24, 70);
			Rc(X + 12, Cy, W - 24, 70, bSel ? FLinearColor(0.85f, 0.66f, 0.3f) : bH ? FLinearColor(0.5f, 0.4f, 0.22f) : FLinearColor(0.25f, 0.2f, 0.12f));
			++Layer; LastKind = 1;
			Rc(X + 14, Cy + 2, W - 28, 66, bSel ? FLinearColor(0.16f, 0.12f, 0.07f) : FLinearColor(0.07f, 0.06f, 0.05f));
			if (I.bValid)
			{
				const FHLClass* C = D.Class(I.ClsId);
				Tx(X + 26, Cy + 9, I.Name, 15, ColGold, true);
				Tx(X + 26, Cy + 31, FString::Printf(TEXT("Level %d %s"), I.Level, C ? *C->Name : TEXT("")), 11, C ? C->Color : ColWhite);
				Tx(X + 26, Cy + 48, FString::Printf(TEXT("Hollywood Boulevard    %dh %02dm played"), (int32)(I.Played / 3600), ((int32)I.Played / 60) % 60), 9, ColDim);
			}
			else
			{
				Tx(X + 26, Cy + 14, TEXT("Empty slot"), 13, FLinearColor(0.7f, 0.65f, 0.55f), true);
				Tx(X + 26, Cy + 38, TEXT("New character"), 10, ColDim);
			}
			if (bH && bLClick)
			{
				bLClick = false;
				static double LastClick = 0; static int32 LastSlot = -1;
				const double Now = FPlatformTime::Seconds();
				const bool bDouble = LastSlot == s && Now - LastClick < 0.4;
				LastClick = Now; LastSlot = s;
				if (SelSlot != s) { SelSlot = s; bDelArmed = false; PreviewYaw = -18.f; }
				if (bDouble && !I.bValid) { TitleMode = 1; SelClass = NurseClass(); bNewLookSet = false; NameEntry.Reset(); PreviewYaw = -18.f; return; }
			}
		}
		float By = Y + 40 + 3 * 78 + 8;
		if (Sel.bValid)
		{
			if (Btn(X + 12, By, W - 24, 40, TEXT("Enter Hollywood"), true, 16))
			{
				Gm.SaveSlot = SelSlot;
				if (Gm.LoadJson(World->SavedSlots[SelSlot])) { Gm.EnterWorld(); return; }
				World->SavedSlots[SelSlot].Reset();
			}
			By += 48;
			if (Btn(X + 12, By, 150, 24, bDelArmed ? TEXT("Click again to delete") : TEXT("Delete character"), true, 9))
			{
				if (bDelArmed) { HLBridge::StorageSet(FHLGame::SaveKey(SelSlot), FString()); World->SavedSlots[SelSlot].Reset(); bDelArmed = false; }
				else bDelArmed = true;
			}
			if (bDelArmed) Tx(X + 172, By + 5, TEXT("This cannot be undone."), 9, ColRed);
		}
		else
		{
			if (bNoChars) Glow(X + 12, By, W - 24, 40);
			if (Btn(X + 12, By, W - 24, 40, TEXT("Create New Character"), true, 15)) { TitleMode = 1; SelClass = NurseClass(); bNewLookSet = false; NameEntry.Reset(); PreviewYaw = -18.f; }
			By += 48;
			// restore from the cloud copy on another browser (the code is shown in the game menu) into this empty slot
			const float Bw = 220;
			if (Hover(X + 12, By, Bw, 24) && bLClick) { bLClick = false; bRestoreFocus = true; }
			Rc(X + 12, By, Bw, 24, bRestoreFocus ? FLinearColor(0.85f, 0.7f, 0.35f) : FLinearColor(0.35f, 0.3f, 0.2f));
			++Layer; LastKind = 1;
			Rc(X + 13, By + 1, Bw - 2, 22, FLinearColor(0.02f, 0.02f, 0.02f));
			Tx(X + 18, By + 4, RestoreEntry.IsEmpty() && !bRestoreFocus ? FString(TEXT("restore: type your cloud code")) : RestoreEntry + (bBlink && bRestoreFocus ? TEXT("_") : TEXT("")), 10, RestoreEntry.IsEmpty() && !bRestoreFocus ? ColDim : ColWhite);
			if (Btn(X + 20 + Bw, By, W - Bw - 32, 24, bRestoreBusy ? TEXT("...") : TEXT("Restore"), RestoreEntry.Len() >= 8 && !bRestoreBusy, 10))
			{
				bRestoreBusy = true; RestoreMsg = TEXT("Looking for that save...");
				TWeakPtr<const SWidget> Weak = AsShared();
				const SHLGameUI* Self = this;
				AHLWorldActor* WA = World;
				const int32 Slot = SelSlot;
				HLBridge::Fetch(TEXT("GET"), FString::Printf(TEXT("%s/v1/save/%s"), *Gm.ApiBase, *RestoreEntry), FString(), [Weak, Self, WA, Slot](const FHLBridgeResult& R)
				{
					if (!Weak.IsValid()) return;
					Self->bRestoreBusy = false;
					if (R.Ok() && R.Body.Contains(TEXT("\"name\""))) { WA->SavedSlots[Slot] = R.Body; HLBridge::StorageSet(FHLGame::SaveKey(Slot), R.Body); Self->RestoreMsg = TEXT("Save found."); }
					else Self->RestoreMsg = R.Status == 404 ? TEXT("No save with that code.") : TEXT("Could not reach the server. Try again.");
				});
			}
			if (!RestoreMsg.IsEmpty()) Tx(X + 12, By + 30, RestoreMsg, 9, RestoreMsg.StartsWith(TEXT("Save found")) ? ColGreen : ColDim);
		}
		return;
	}
	// ---------------------------------------------------------------- create: the class on the left, the looks on the right
	if (!bNewLookSet) { NewLook = FHLAppearance::FromSeed(FMath::Rand()); bNewLookSet = true; }
	const FHLClass& C = D.Classes[FMath::Clamp(SelClass, 0, D.Classes.Num() - 1)];
	{
		TMap<FString, const FHLItem*> Gear;
		for (const FString& Id : C.StartItems) if (const FHLItem* It = D.Item(Id)) if (It->IsEquip()) Gear.Add(It->Slot, It);
		const FHLCharLook L = Chars.LookFromGear(NewLook, Gear, false);
		World->Showcase(&L, PreviewYaw, TEXT("Idle"));
	}
	{
		const float W = 440, X = 16, Y = 110, H = VS.Y - Y - 112;
		Block(X, Y, W, H); Panel(X, Y, W, H, FLinearColor(0.04f, 0.035f, 0.03f, 0.92f), FLinearColor(0.72f, 0.56f, 0.28f));
		Tx(X + 14, Y + 10, TEXT("Class"), 14, ColGold, true);
		for (int32 i = 0; i < D.Classes.Num(); ++i)
		{
			const FHLClass& K = D.Classes[i];
			const float Bx = X + 14 + (i % 2) * 198, Byy = Y + 36 + (i / 2) * 34;
			if (i == SelClass) Rc(Bx - 3, Byy - 3, 194, 32, K.Color);
			if (Btn(Bx, Byy, 188, 26, K.Name, true, 11)) SelClass = i;
			if (i == SelClass)
			{
				++Layer; LastKind = 1;
				const FVector2D Ck(Bx + 170, Byy + 13);
				Line(Ck + FVector2D(-5, 0), Ck + FVector2D(-1.5f, 4), K.Color, 2.5f); Line(Ck + FVector2D(-1.5f, 4), Ck + FVector2D(6, -5), K.Color, 2.5f);
			}
		}
		float Cy = Y + 36 + ((D.Classes.Num() + 1) / 2) * 34 + 8;
		Rc(X + 14, Cy, W - 28, 1, FLinearColor(0.55f, 0.45f, 0.25f, 0.6f)); Cy += 10;
		Tx(X + 14, Cy, C.Name, 18, C.Color, true);
		// the role in a line of its own, bold and in the class colour, apart from the story paragraph
		const TCHAR* RoleName = C.Role == TEXT("tank") ? TEXT("Tank") : C.Role == TEXT("healer") ? TEXT("Healer") : C.Role == TEXT("caster") ? TEXT("Ranged caster") : C.Role == TEXT("ranged") ? TEXT("Ranged damage") : TEXT("Melee damage");
		const TCHAR* RoleLine = C.Role == TEXT("tank") ? TEXT("holds the enemy's attention up front") : C.Role == TEXT("healer") ? TEXT("keeps the group alive, fights from range") : C.Role == TEXT("caster") ? TEXT("spells from a distance") : C.Role == TEXT("ranged") ? TEXT("guns from a distance") : TEXT("up close and quick");
		const float Rw = TW(RoleName, 12, true);
		RRc(X + 12, Cy + 24, Rw + 14, 22, C.Color * FLinearColor(0.7f, 0.7f, 0.7f, 1), 5);
		++Layer; LastKind = 1;
		RRc(X + 13.5f, Cy + 25.5f, Rw + 11, 19, FLinearColor(0.05f, 0.04f, 0.03f, 1), 4);
		Tx(X + 19, Cy + 27, RoleName, 12, C.Color * 1.25f, true);
		Tx(X + 34 + Rw, Cy + 28, RoleLine, 11, FLinearColor(1, 0.95f, 0.85f), true);
		Cy += 56;
		if (!C.Desc.IsEmpty()) Cy += TxWrap(X + 14, Cy, W - 44, C.Desc, 10, FLinearColor(0.78f, 0.75f, 0.68f)) + 8;
		auto Row = [&](const TCHAR* K, const FString& V) { Tx(X + 14, Cy, K, 10, ColDim); Tx(X + 110, Cy, V, 10, ColWhite); Cy += 17; };
		FString Res = C.Resource; if (Res.Len()) Res[0] = FChar::ToUpper(Res[0]);
		FString Arm = C.Armour; if (Arm.Len()) Arm[0] = FChar::ToUpper(Arm[0]);
		Row(TEXT("Resource"), Res); Row(TEXT("Armour"), Arm); Row(TEXT("Talent trees"), FString::Join(C.Specs, TEXT(", ")));
		Cy += 8;
		Tx(X + 14, Cy, TEXT("Abilities"), 12, ColGold, true); Cy += 22;
		for (const FHLAbility* Ab : C.Abilities)
		{
			if (Cy > Y + H - 30) break;
			Rc(X + 14, Cy - 2, 24, 24, FLinearColor(0.45f, 0.36f, 0.18f));
			if (const FSlateBrush* IB = Ab->Icon >= 0 ? HLSkillBrush(Ab->Icon) : nullptr) Img(X + 15, Cy - 1, 22, 22, IB);
			Tx(X + 46, Cy + 3, Ab->Name, 10, ColWhite, true);
			const FString Lv = FString::Printf(TEXT("Level %d"), Ab->Level);
			Tx(X + W - 30 - TW(Lv, 9), Cy + 4, Lv, 9, Ab->Level <= 1 ? ColGreen : ColDim);
			if (Hover(X + 14, Cy - 2, W - 28, 24)) AbilityTip(*Ab);
			Cy += 27;
		}
	}
	{
		const float W = 340, X = VS.X - W - 16, Y = 110, H = 284;
		Block(X, Y, W, H); Panel(X, Y, W, H, FLinearColor(0.04f, 0.035f, 0.03f, 0.92f), FLinearColor(0.72f, 0.56f, 0.28f));
		Tx(X + 14, Y + 10, TEXT("Name"), 14, ColGold, true);
		Rc(X + 14, Y + 34, W - 28, 30, FLinearColor(0.4f, 0.35f, 0.2f));
		++Layer; LastKind = 1;
		Rc(X + 15, Y + 35, W - 30, 28, FLinearColor(0.02f, 0.02f, 0.02f));
		if (Hover(X + 14, Y + 34, W - 28, 30) && bLClick) { bLClick = false; bRestoreFocus = false; }
		Tx(X + 22, Y + 39, NameEntry.IsEmpty() && !bBlink ? FString(TEXT("Name")) : NameEntry + (bBlink ? TEXT("_") : TEXT("")), 13, NameEntry.IsEmpty() ? ColDim : ColWhite);
		Tx(X + 16, Y + 68, TEXT("Letters only, at least 2"), 9, NameEntry.Len() == 1 ? FLinearColor(1, 0.66f, 0.3f) : ColDim);
		Rc(X + 14, Y + 86, W - 28, 1, FLinearColor(0.55f, 0.45f, 0.25f, 0.6f));
		DrawCreator(X + 10, Y + 94, W - 20, false);
	}
	{
		const bool bOk = NameEntry.Len() >= 2;
		// under the looks panel, clear of the figure
		const float Bw = 340, Bx = VS.X - Bw - 16, By = 110 + 294;
		if (Btn(Bx, By + 52, 120, 28, TEXT("Back"), true, 11)) { TitleMode = 0; return; }
		if (Btn(Bx, By, Bw, 44, bOk ? TEXT("Enter Hollywood") : TEXT("Name your character"), bOk, 16))
		{
			FString Name = NameEntry.ToLower(); Name[0] = FChar::ToUpper(Name[0]);
			Gm.SaveSlot = FMath::Clamp(SelSlot, 0, 2);
			Gm.NewCharacter(Name, &C);
			Gm.Player.Look = NewLook;
			if (FHLUnit* U = Gm.PlayerUnit()) { U->Level = 1; U->Pos = Gm.Areas[0].Def->Spawn; U->Area = 0; U->bDead = false; }
			Gm.EnterWorld();
			Gm.SaveNow();
		}
	}
}

void SHLGameUI::DrawWorldOverlay() const
{
	FHLGame& Gm = G();
	const FHLUnit* P = Gm.PlayerUnit();
	if (!P) return;
	const FHLAreaState& Area = Gm.Areas[P->Area];
	// corpses with loot for you twinkle: a few gold stars over the body, each on its own beat
	for (const FHLUnit* U : Gm.UnitList)
	{
		if (U->Kind != EHLKind::Mob || !U->bDead || !U->bLootable || U->TappedBy != Gm.PlayerUnitId || U->Area != P->Area) continue;
		const float Dm = FVector2D::Distance(U->Pos, P->Pos);
		if (Dm > 45) continue;
		const float K = FMath::Clamp(30.f / FMath::Max(Dm, 4.f), 0.6f, 2.2f);
		const double T = FPlatformTime::Seconds();
		for (int32 s = 0; s < 6; ++s)
		{
			const float Ph = (float)FMath::Fmod(T * 0.9 + s * 0.37 + U->Id * 0.13, 1.0);
			const float A = FMath::Sin(Ph * PI);
			const float Ang = s * 1.9f + U->Id, Rr = 0.5f + 0.35f * (s % 3);
			const FVector2D At = U->Pos + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * Rr;
			FVector2D S;
			if (!Project(World->ToWorld(P->Area, At, 0.25f + 0.5f * Ph), S)) continue;
			const float R = (2.f + 3.5f * A) * K;
			const FLinearColor C(1, 0.9f, 0.45f, A * 0.95f);
			Line(S - FVector2D(R, 0), S + FVector2D(R, 0), C, 1.5f);
			Line(S - FVector2D(0, R), S + FVector2D(0, R), C, 1.5f);
			Line(S - FVector2D(R, R) * 0.45f, S + FVector2D(R, R) * 0.45f, FLinearColor(1, 1, 0.8f, A * 0.6f), 1.f);
			Line(S - FVector2D(R, -R) * 0.45f, S + FVector2D(R, -R) * 0.45f, FLinearColor(1, 1, 0.8f, A * 0.6f), 1.f);
		}
	}
	// landmark labels
	for (const FHLBox& B : Area.Def->Boxes)
	{
		if (B.Label.IsEmpty()) continue;
		const FVector2D C(B.X + B.W * 0.5f, B.Y + B.D * 0.5f);
		const float D = FVector2D::Distance(C, P->Pos);
		if (D > 190) continue;
		FVector2D S;
		if (Project(World->ToWorld(P->Area, C, B.H + 2), S)) TxC(S.X, S.Y, B.Label, D < 60 ? 13 : 11, FLinearColor(1, 0.92f, 0.7f, D < 120 ? 1.f : 0.6f), true);
	}
	// portals, objects
	for (const FHLPortal& Po : Area.Def->Portals)
	{
		FVector2D S;
		if (FVector2D::Distance(Po.Pos, P->Pos) < 60 && Project(World->ToWorld(P->Area, Po.Pos, 4), S)) TxC(S.X, S.Y, Po.Label, 11, FLinearColor(0.4f, 1, 0.9f), true);
	}
	for (int32 o = 0; o < Area.Objects.Num(); ++o)
	{
		const FHLObjectState& O = Area.Objects[o];
		if (!O.bActive) continue;
		const float D = FVector2D::Distance(O.Def->Pos, P->Pos);
		if (D > 45) continue;
		bool bWanted = O.Def->Quest.IsEmpty() || !O.Def->Boss.IsEmpty();
		if (!O.Def->Quest.IsEmpty()) if (const FHLQuestState* QS = Gm.QuestState(O.Def->Quest)) for (int32 i = 0; i < QS->Q->Objectives.Num(); ++i) if (QS->Q->Objectives[i].Kind == O.Def->Kind && QS->Progress[i] < QS->Q->Objectives[i].Count) bWanted = true;
		FVector2D S;
		if (!Project(World->ToWorld(P->Area, O.Def->Pos, O.Def->bTall ? 7.5f : O.Def->bBig ? 4 : 1.3f), S)) continue;
		if (bWanted || D < 12)
		{
			const FString Lb = (bWanted && !O.Def->Quest.IsEmpty() ? TEXT("* ") : TEXT("")) + O.Def->Name;
			const float Lw = TW(Lb, 10) * 0.5f + 3;
			const FBox2D R(FVector2D(S.X - Lw, S.Y), FVector2D(S.X + Lw, S.Y + 15));
			bool bClash = false;
			for (const FBox2D& Bx : HudReserve) if (Bx.Intersect(R)) { bClash = true; break; }
			if (!bClash) TxC(S.X, S.Y, Lb, 10, bWanted ? FLinearColor(1, 0.9f, 0.3f) : ColDim);
			// why it can't be worked yet, under its name
			if (!bClash && O.Def->bSalvage && Gm.Player.Salvage <= 0) TxC(S.X, S.Y + 14, TEXT("Requires Salvaging"), 9, FLinearColor(1, 0.35f, 0.3f));
		}
		// the click lands on the thing itself (a film can lies on the ground, well under its name), the name counts too
		FVector2D G2 = S + FVector2D(0, 24);
		Project(World->ToWorld(P->Area, O.Def->Pos, O.Def->bTall ? 3.f : O.Def->bBig ? 1.5f : 0.35f), G2);
		FPick Pk; Pk.Area = P->Area; Pk.Obj = o; Pk.Pos = G2; Pk.Radius = FMath::Clamp(700.f / FMath::Max(4.f, D), 24.f, 90.f); Pk.Depth = D;
		Picks.Add(Pk);
		Pk.Pos = S + FVector2D(0, 8); Pk.Radius = FMath::Max(TW(O.Def->Name, 10) * 0.5f + 6, 22.f);
		Picks.Add(Pk);
	}
	// nameplates: the target first, then nearest first; a label that would overlap one already drawn (or the HUD) waits
	TArray<FHLUnit*> Order;
	for (FHLUnit* U : Gm.UnitList) if (U->Area == P->Area) Order.Add(U);
	Order.Sort([&](const FHLUnit& A, const FHLUnit& B) { auto K = [&](const FHLUnit& U) { return U.Id == P->TargetId ? -1.f : (float)FVector2D::DistSquared(U.Pos, P->Pos); }; return K(A) < K(B); });
	TArray<FBox2D> Used = HudReserve;
	for (FHLUnit* U : Order)
	{
		const float D = FVector2D::Distance(U->Pos, P->Pos);
		const bool bTarget = U->Id == P->TargetId;
		if (D > 80 && !bTarget) continue;
		// rigged creatures are about as tall as their art (the drone stands 2.3 m at scale 1); fliers hang at their lift
		const float Body = (U->Shape == TEXT("human") ? 1.9f : U->Shape == TEXT("bug") ? 1.8f : 1.3f) * U->Scale;
		const float Base = U->Z + U->Lift + (U->bMounted ? 0.9f : 0);
		const float Height = Body + Base;
		FVector2D S, Mid, Low;
		if (!Project(World->ToWorld(U->Area, U->Pos, U->bDead ? 0.8f + U->Lift : Height + 0.35f), S)) continue;
		if (Project(World->ToWorld(U->Area, U->Pos, Base + Body * 0.5f), Mid) && Project(World->ToWorld(U->Area, U->Pos, Base), Low))
		{
			// a generous circle around the body: at least half its on-screen height, never under 26 px
			const float Half = FVector2D::Distance(Mid, Low);
			FPick Pk; Pk.UnitId = U->Id; Pk.Pos = Mid; Pk.Radius = FMath::Clamp(FMath::Max(Half * 1.25f, 900.f * FMath::Max(1.f, U->Scale) / FMath::Max(3.f, D + World->CamDist * 0.5f)), 26.f, 160.f); Pk.Depth = D;
			if (U->bDead) Pk.Radius *= 0.8f;
			if (U->Id != Gm.PlayerUnitId) Picks.Add(Pk);
		}
		if (U->Id == Gm.PlayerUnitId) continue;
		if (U->bDead) { if (U->Kind == EHLKind::Mob && U->bLootable && U->TappedBy == Gm.PlayerUnitId && D < 40) TxC(S.X, S.Y, TEXT("* loot *"), 10, ColGold); continue; }
		if (D > 55 && !bTarget && U->Kind != EHLKind::Npc) continue;
		if (!FHLSettings::Get().bNameplates && !bTarget && U->Kind != EHLKind::Npc) continue;
		FLinearColor C = ColWhite; FString Name = U->Name;
		if (U->Kind == EHLKind::Mob)
		{
			C = !U->Mob->bAggressive ? FLinearColor(1, 0.9f, 0.2f) : FLinearColor(1, 0.3f, 0.25f);
			if (U->TappedBy && U->TappedBy != Gm.PlayerUnitId) C = FLinearColor(0.6f, 0.6f, 0.6f);
		}
		else if (U->Kind == EHLKind::Npc) C = FLinearColor(0.4f, 1, 0.4f);
		else { C = U->SimIdx >= 0 && Gm.Sims[U->SimIdx].bInParty ? FLinearColor(0.5f, 0.8f, 1) : FLinearColor(0.55f, 0.65f, 1); if (Gm.Player.bShowSim) Name += TEXT(" <AI>"); if (U->SimIdx >= 0 && Gm.Sims[U->SimIdx].bAfk) Name = TEXT("<AFK> ") + Name; }
		const int32 Sz = bTarget ? 12 : D < 25 ? 11 : 9;
		{
			const bool bMarked = U->Kind == EHLKind::Npc && U->Npc && Gm.NpcMarker(*U->Npc) != 0;
			const float Lw = TW(Name, Sz, bTarget) + 6;
			const FBox2D R(FVector2D(S.X - Lw * 0.5f, S.Y - 11), FVector2D(S.X + Lw * 0.5f, S.Y + Sz * 1.5f));
			bool bClash = false;
			for (const FBox2D& Bx : Used) if (Bx.Intersect(R)) { bClash = true; break; }
			if (bClash && !bTarget && !bMarked) continue;
			Used.Add(R);
		}
		float Yy = S.Y;
		TxC(S.X, Yy, Name, Sz, C, bTarget);
		if (!U->Title.IsEmpty() && D < 30) { Yy -= Sz * 1.4f; TxC(S.X, Yy + Sz * 2.8f, U->Title, 9, FLinearColor(C.R, C.G, C.B, 0.8f)); }
		if (U->Kind != EHLKind::Npc && (bTarget || U->Hp < U->MaxHp || (U->Kind == EHLKind::Mob && D < 30)))
		{
			const float Bw = 60, Bx = S.X - Bw * 0.5f, By = S.Y - 9;
			Bar(Bx, By, Bw, 5, U->HpPct(), U->Kind == EHLKind::Mob ? FLinearColor(0.8f, 0.1f, 0.1f) : FLinearColor(0.1f, 0.75f, 0.1f));
			if (U->Kind == EHLKind::Mob) { const int32 Diff = U->Level - P->Level; Tx(Bx - 18, By - 6, U->Level - P->Level >= 10 ? TEXT("??") : FString::FromInt(U->Level), 9, Diff >= 5 ? ColRed : Diff >= 3 ? FLinearColor(1, 0.5f, 0.1f) : Diff >= -2 ? FLinearColor(1, 1, 0.2f) : Diff >= -5 ? ColGreen : ColDim); }
			if (U->Casting) Bar(Bx, By + 8, Bw, 4, (Gm.Now - U->CastStart) / FMath::Max(0.1f, U->CastEnd - U->CastStart), FLinearColor(0.9f, 0.7f, 0.1f));
		}
		if (U->Kind == EHLKind::Npc && U->Npc)
		{
			const int32 Mk = Gm.NpcMarker(*U->Npc);
			if (Mk) TxC(S.X, S.Y - 34, Mk == 1 ? TEXT("!") : TEXT("?"), 22, Mk == 3 ? ColDim : ColGold, true);
		}
	}
	// floating combat text
	TMap<int32, int32> FloatN;
	for (const FHLFloatText& F : Gm.Floats)
	{
		const int32 Nth = FloatN.FindOrAdd(F.UnitId)++;
		if (Nth >= 5) continue; // a unit shows its five newest
		const FHLUnit* U = Gm.Unit(F.UnitId);
		if (!U || U->Area != P->Area || F.Text.IsEmpty() || !FHLSettings::Get().bFloatText) continue;
		FVector2D S;
		if (!Project(World->ToWorld(U->Area, U->Pos, 2.3f * FMath::Max(1.f, U->Scale * 0.8f) + U->Lift + F.Age * 1.2f), S)) continue;
		const float A = FMath::Clamp(1.6f - F.Age, 0.f, 1.f);
		TxC(S.X + ((Nth + F.UnitId) % 3 - 1) * 22, S.Y - (Nth % 2) * 8, F.Text, F.bCrit ? 17 : 12, FLinearColor(F.Color.R, F.Color.G, F.Color.B, A), true);
	}
	// speech balloons: cream paper, dark ink, a tail down to the speaker; nearer speakers draw on top
	{
		TArray<const FHLSpeech*> Live;
		for (const FHLSpeech& Sp : Gm.Speech) if (Gm.Now < Sp.Until) Live.Add(&Sp);
		Live.Sort([&](const FHLSpeech& A, const FHLSpeech& B)
		{
			const FHLUnit* UA = Gm.Unit(A.UnitId); const FHLUnit* UB = Gm.Unit(B.UnitId);
			return (UA ? FVector2D::DistSquared(UA->Pos, P->Pos) : 0) > (UB ? FVector2D::DistSquared(UB->Pos, P->Pos) : 0);
		});
		for (const FHLSpeech* Sp : Live)
		{
			const FHLUnit* U = Gm.Unit(Sp->UnitId);
			if (!U || U->Area != P->Area || U->bDead) continue;
			const float D = FVector2D::Distance(U->Pos, P->Pos);
			if (D > 45) continue;
			const float Body = (U->Shape == TEXT("human") ? 1.9f : 1.4f) * U->Scale + U->Z + U->Lift + (U->bMounted ? 0.9f : 0);
			FVector2D S;
			if (!Project(World->ToWorld(U->Area, U->Pos, Body + 0.75f), S)) continue;
			const float A = FMath::Clamp((float)(Sp->Until - Gm.Now) * 2.f, 0.f, 1.f) * FMath::Clamp((float)(Gm.Now - Sp->Start) * 6.f, 0.f, 1.f) * (D > 30 ? 0.75f : 1.f);
			const int32 Fs = D < 18 ? 10 : 9;
			TArray<FString> Lines;
			Wrap(Sp->Text, Fs, 210, Lines);
			if (Lines.Num() > 5) { Lines.SetNum(5); Lines[4] += TEXT("..."); }
			float Wd = 0; for (const FString& L : Lines) Wd = FMath::Max(Wd, TW(L, Fs));
			const float LH = Fs * 1.5f, Bw = Wd + 18, Bh = Lines.Num() * LH + 10;
			const float Bx = FMath::Clamp(S.X - Bw * 0.5f, 4.f, VS.X - Bw - 4), By = S.Y - Bh - 10;
			const FLinearColor Paper = Sp->bYell ? FLinearColor(1.f, 0.93f, 0.86f, 0.95f * A) : FLinearColor(0.98f, 0.96f, 0.9f, 0.93f * A);
			const FLinearColor Edge(0.25f, 0.18f, 0.1f, 0.85f * A);
			Shadow(Bx, By, Bw, Bh, 5, 0.35f * A);
			RRc(Bx - 1, By - 1, Bw + 2, Bh + 2, Sp->bYell ? FLinearColor(0.75f, 0.15f, 0.1f, 0.9f * A) : Edge, 8);
			RRc(Bx, By, Bw, Bh, Paper, 7);
			// tail: a little stepped wedge pointing at the speaker
			const float Tx0 = FMath::Clamp(S.X, Bx + 12, Bx + Bw - 12);
			for (int32 i = 0; i < 4; ++i) { const float Tw = 10 - i * 2.5f; ++Layer; LastKind = 1; Rc(Tx0 - Tw * 0.5f, By + Bh + i * 2.5f - 1, Tw, 3, Paper); }
			for (int32 i = 0; i < Lines.Num(); ++i) Tx(Bx + 9, By + 4 + i * LH, Lines[i], Fs, FLinearColor(0.12f, 0.09f, 0.06f, A), Sp->bYell, false);
		}
	}
}

void SHLGameUI::HandleWorldClick() const
{
	if (!(bLClick || bRClick) || bDownOverUi) return;
	FHLGame& Gm = G();
	const FPick* Best = nullptr; float BestScore = 1e9f;
	for (const FPick& Pk : Picks)
	{
		const float D = FVector2D::Distance(Pk.Pos, DownPos);
		if (D > Pk.Radius) continue;
		const float Score = D / Pk.Radius + Pk.Depth * 0.01f + (Pk.Obj >= 0 ? 0.3f : 0.f);
		if (Score < BestScore) { BestScore = Score; Best = &Pk; }
	}
	if (bLClick)
	{
		if (Best && Best->UnitId) Gm.TargetUnit(Best->UnitId);
		else if (!Best) Gm.TargetUnit(0);
	}
		else if (bRClick && Best)
		{
			Gm.bLootAll = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
			if (Best->UnitId) Gm.InteractUnit(Best->UnitId);
			else Gm.ObjectInteract(Best->Area, Best->Obj);
		}
		else if (bRClick && FHLSettings::Get().bClickMove)
		{
			// ray from the click to the ground plane at the player's feet
			FHLUnit* P = Gm.PlayerUnit();
			FVector O, D;
			if (P && !P->bDead && PC && ViewPx.X > 0 && PC->DeprojectScreenPositionToWorld(DownPos.X / VS.X * ViewPx.X, DownPos.Y / VS.Y * ViewPx.Y, O, D) && D.Z < -0.01f)
			{
				const FVector Feet = World->ToWorld(P->Area, P->Pos);
				const FVector Hit = O + D * ((Feet.Z - O.Z) / D.Z);
				const FVector2D Off = Gm.Areas[P->Area].Def->Offset;
				const FVector2D Goal(Hit.X / 100.f - Off.X, Hit.Y / 100.f - Off.Y);
				if (FVector2D::Distance(Goal, P->Pos) < 120) { Gm.ClickGoal = Goal; Gm.bClickGoal = true; Gm.bClickGhost = P->bGhost; Gm.StopMove(*P); }
			}
		}
	}

// ------------------------------------------------------------------------------------------------------------- HUD
static FLinearColor ResColor(EHLRes R) { return R == EHLRes::Power ? FLinearColor(0.15f, 0.35f, 0.95f) : R == EHLRes::Energy ? FLinearColor(0.95f, 0.85f, 0.1f) : R == EHLRes::Focus ? FLinearColor(0.9f, 0.5f, 0.15f) : FLinearColor(0.8f, 0.1f, 0.1f); }

void SHLGameUI::DrawHud() const
{
	FHLGame& Gm = G();
	FHLUnit* P = Gm.PlayerUnit();
	if (!P) return;
	auto UnitFrame = [&](float X, float Y, float W, const FHLUnit& U, bool bBig)
	{
		const float H = bBig ? 58 : 40;
		Block(X, Y, W, H);
		Panel(X, Y, W, H);
		FLinearColor NC = U.Kind == EHLKind::Mob ? FLinearColor(1, 0.4f, 0.3f) : U.Cls ? U.Cls->Color * 1.3f : ColWhite;
		Tx(X + 6, Y + 3, U.Name, bBig ? 11 : 10, NC, true);
		const FString Lv = U.Kind == EHLKind::Mob && U.Level - P->Level >= 10 ? TEXT("??") : FString::FromInt(U.Level);
		FString Rank = U.Mob ? (U.Mob->Rank == TEXT("elite") ? TEXT(" Elite") : U.Mob->Rank == TEXT("rare") ? TEXT(" Rare") : U.Mob->Rank == TEXT("boss") ? TEXT(" Boss") : TEXT("")) : TEXT("");
		Tx(X + W - TW(Lv + Rank, 10) - 6, Y + 4, Lv + Rank, 10, ColGold);
		Bar(X + 6, Y + (bBig ? 22 : 19), W - 12, bBig ? 14 : 9, U.HpPct(), U.bDead ? FLinearColor(0.3f, 0.3f, 0.3f) : FLinearColor(0.1f, 0.7f, 0.1f), bBig ? FString::Printf(TEXT("%d / %d"), (int32)U.Hp, (int32)U.MaxHp) : FString(), 9);
		if (U.ResType != EHLRes::None) Bar(X + 6, Y + (bBig ? 40 : 31), W - 12, bBig ? 11 : 5, U.MaxRes > 0 ? U.Res / U.MaxRes : 0, ResColor(U.ResType), bBig ? FString::Printf(TEXT("%d / %d"), (int32)U.Res, (int32)U.MaxRes) : FString(), 8);
	};
		// in combat: the player frame glows and pulses red (tanks watch this), with a small tag under the name
		const bool bFighting = Gm.InCombat(*P) && !P->bDead;
		if (bFighting)
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin((float)Gm.Now * 5.f);
			for (int32 i = 3; i >= 1; --i) RRc(16 - i * 3, 16 - i * 3, 230 + i * 6, 58 + i * 6, FLinearColor(1.f, 0.08f, 0.04f, (0.12f + 0.16f * Pulse) * (4 - i) / 3.f), 6 + i * 3);
		}
		UnitFrame(16, 16, 230, *P, true);
		if (bFighting)
		{
			const float Pulse = 0.5f + 0.5f * FMath::Sin((float)Gm.Now * 5.f);
			++Layer; LastKind = 1;
			Rc(15, 15, 232, 2, FLinearColor(1, 0.15f, 0.1f, 0.6f + 0.4f * Pulse)); Rc(15, 73, 232, 2, FLinearColor(1, 0.15f, 0.1f, 0.6f + 0.4f * Pulse));
			Rc(15, 15, 2, 60, FLinearColor(1, 0.15f, 0.1f, 0.6f + 0.4f * Pulse)); Rc(245, 15, 2, 60, FLinearColor(1, 0.15f, 0.1f, 0.6f + 0.4f * Pulse));
			const float Tw = TW(TEXT("IN COMBAT"), 8, true) + 12;
			RRc(131 - Tw * 0.5f, 68, Tw, 14, FLinearColor(0.45f, 0.04f, 0.03f, 0.95f), 4);
			TxC(131, 68, TEXT("IN COMBAT"), 8, FLinearColor(1, 0.85f, 0.75f), true);
		}
		float AuraBottom = 80;
		// auras: buffs in a row of icons under the frame, debuffs in a bigger red-edged row below them (and named, so a daze
		// or a sickness is unmissable); resurrection sickness counts as a debuff
		{
			struct FShown { FString Name; float Left; bool bBad; int32 Icon; EHLAura Type; };
			TArray<FShown> Buffs, Debuffs;
			static TMap<FString, int32> IconByName;
			if (!IconByName.Num()) for (const FHLAbility& Ab : FHLData::Get().Abilities) if (Ab.Icon >= 0) IconByName.Add(Ab.Name, Ab.Icon);
			for (const FHLAura& A : P->Auras)
			{
				if (A.Stat == TEXT("none") && !A.bHarmful && A.Name != TEXT("Recently Bandaged")) continue;
				const int32* Ic = IconByName.Find(A.Name);
				(A.bHarmful ? Debuffs : Buffs).Add({ A.Name, A.Remaining, A.bHarmful, Ic ? *Ic : -1, A.Type });
			}
			if (Gm.Player.SicknessUntil > Gm.Now) Debuffs.Add({ TEXT("Resurrection Sickness"), (float)(Gm.Player.SicknessUntil - Gm.Now), true, -1, EHLAura::Buff });
			// newly arrived debuffs get a centred notice
			{
				TSet<FString> CurDebuffs;
				for (const FShown& D : Debuffs) CurDebuffs.Add(D.Name);
				for (const FShown& D : Debuffs)
					if (!SeenDebuffs.Contains(D.Name))
					{
						const bool bStun = D.Type == EHLAura::Stun;
						Gm.Notice(bStun ? FString::Printf(TEXT("%s!"), *D.Name) : D.Name, bStun ? FLinearColor(1, 0.75f, 0.2f) : FLinearColor(1, 0.35f, 0.3f));
					}
				SeenDebuffs = MoveTemp(CurDebuffs);
			}
			auto Glyph = [](EHLAura T, bool bBad) { return T == EHLAura::Stun ? TEXT("!") : T == EHLAura::Slow ? TEXT("~") : T == EHLAura::Dot ? TEXT("x") : T == EHLAura::Hot ? TEXT("+") : T == EHLAura::Shield ? TEXT("o") : bBad ? TEXT("-") : TEXT("^"); };
			auto Icon = [&](float X, float Y, float Sz, const FShown& A)
			{
				const FLinearColor Edge = A.bBad ? FLinearColor(0.9f, 0.12f, 0.08f) : FLinearColor(0.75f, 0.62f, 0.3f);
				Rc(X - 1, Y - 1, Sz + 2, Sz + 2, Edge);
				++Layer; LastKind = 1;
				Rc(X, Y, Sz, Sz, A.bBad ? FLinearColor(0.28f, 0.05f, 0.05f) : FLinearColor(0.07f, 0.16f, 0.28f));
				if (const FSlateBrush* IB = A.Icon >= 0 ? HLSkillBrush(A.Icon) : nullptr) Img(X + 1, Y + 1, Sz - 2, Sz - 2, IB);
				else TxC(X + Sz * 0.5f, Y + Sz * 0.5f - Sz * 0.36f, Glyph(A.Type, A.bBad), FMath::RoundToInt(Sz * 0.5f), A.bBad ? FLinearColor(1, 0.6f, 0.5f) : FLinearColor(0.7f, 0.85f, 1), true);
				const int32 S = FMath::CeilToInt32(A.Left);
				const FString T = S >= 60 ? FString::Printf(TEXT("%dm"), (S + 59) / 60) : FString::Printf(TEXT("%ds"), S);
				TxC(X + Sz * 0.5f, Y + Sz + 1, T, 8, A.bBad && A.Left < 3 ? FLinearColor(1, 0.5f, 0.4f) : ColWhite);
				if (Hover(X, Y, Sz, Sz)) { TipText = FString::Printf(TEXT("%s\n%s, %s left"), *A.Name, A.bBad ? TEXT("Harmful") : TEXT("Helpful"), *T); TipPos = Mouse; }
			};
			float Ax = 16;
			// riding: the mount leads the buff row (its portrait); right-click it to climb down
			const bool bRiding = P->bMounted;
			if (bRiding)
			{
				static const TCHAR* Art[] = { TEXT("mount_horse"), TEXT("mount_scout"), TEXT("mount_modela"), TEXT("mount_modela") };
				static const TCHAR* Names[] = { TEXT("Bay Mare"), TEXT("Indian Scout"), TEXT("Ford Model A"), TEXT("Police Car") };
				const int32 K = FMath::Min<int32>(P->MountKind, 3);
				Rc(Ax - 1, 83, 24, 24, FLinearColor(0.95f, 0.78f, 0.38f));
				++Layer; LastKind = 1;
				if (const FSlateBrush* IB = HLArtBrush(Art[K])) Img(Ax, 84, 22, 22, IB);
				const float Left = Gm.TestDriveUntil > 0 ? (float)(Gm.TestDriveUntil - Gm.Now) : -1.f;
				if (Left >= 0) TxC(Ax + 11, 107, FString::Printf(TEXT("%ds"), FMath::CeilToInt32(Left)), 8, ColWhite);
				if (Hover(Ax - 1, 83, 24, 24))
				{
					TipText = FString::Printf(TEXT("%s\nMounted: 60%% faster\nRight-click to dismount"), Names[K]); TipPos = Mouse;
					if (bRClick) { bRClick = false; Gm.EndTestDrive(TEXT("You climb down.")); }
				}
				Ax += 28;
			}
			for (const FShown& A : Buffs) { Icon(Ax, 84, 22, A); Ax += 26; }
			Ax = 16;
			const float Dy = Buffs.Num() || bRiding ? 120 : 84;
			for (const FShown& A : Debuffs)
			{
				Icon(Ax, Dy, 30, A);
				Tx(Ax + 36, Dy + 7, A.Name, 9, FLinearColor(1, 0.55f, 0.45f), true);
						Ax += 42 + TW(A.Name, 9, true) + 8;
					}
					AuraBottom = Debuffs.Num() ? Dy + 44 : Buffs.Num() || bRiding ? 120 : 80;
				}
	// target
	if (FHLUnit* T = Gm.Unit(P->TargetId))
	{
		UnitFrame(262, 16, 230, *T, true);
		if (T->Casting) Bar(262, 78, 230, 12, (Gm.Now - T->CastStart) / FMath::Max(0.1f, T->CastEnd - T->CastStart), FLinearColor(0.9f, 0.7f, 0.1f), T->Casting->Name, 8);
		if (T->Kind == EHLKind::Sim && T->SimIdx >= 0)
		{
			Block(262, 78, 230, 22);
			if (Btn(262, 78, 72, 20, TEXT("Inspect"), true, 9)) { bInspect = true; InspectSim = T->SimIdx; }
			if (Btn(340, 78, 72, 20, Gm.Sims[T->SimIdx].bInParty ? TEXT("In party") : TEXT("Invite"), !Gm.Sims[T->SimIdx].bInParty, 9)) Gm.InviteSim(T->SimIdx);
			if (Btn(418, 78, 74, 20, TEXT("Whisper"), true, 9)) { bTyping = true; ChatEntry = FString::Printf(TEXT("/w %s "), *T->Name); }
			Block(262, 100, 230, 22);
			const bool bFr = Gm.IsFriend(Gm.Sims[T->SimIdx]);
			if (Btn(262, 100, 72, 20, TEXT("Trade"), Gm.Dist(*P, *T) <= 11, 9)) Gm.OpenTrade(T->SimIdx);
			if (Btn(340, 100, 72, 20, TEXT("Duel"), Gm.DuelState == 0, 9)) Gm.ChallengeDuel(T->SimIdx);
			if (Btn(418, 100, 74, 20, bFr ? TEXT("Unfriend") : TEXT("Add friend"), true, 9)) { if (bFr) Gm.RemoveFriend(T->SimIdx); else Gm.AddFriend(T->SimIdx); }
		}
		else if (T->Kind == EHLKind::Mob && !T->Title.IsEmpty()) Tx(266, 94, T->Title, 9, ColDim);
	}
	// party
	for (int32 i = 0; i < Gm.Party.Num(); ++i)
	{
		const FHLSim& S = Gm.Sims[Gm.Party[i]];
		if (const FHLUnit* U = Gm.Unit(S.UnitId))
		{
			const float Y = FMath::Max(110.f, AuraBottom + 6) + i * 48;
			UnitFrame(16, Y, 170, *U, false);
			if (Hover(16, Y, 170, 40) && bLClick) { bLClick = false; Gm.TargetUnit(U->Id); }
		}
	}
	if (Gm.Party.Num()) { const float Ly = FMath::Max(110.f, AuraBottom + 6) + Gm.Party.Num() * 48; Block(16, Ly, 90, 18); if (Btn(16, Ly, 90, 18, TEXT("Leave party"), true, 9)) Gm.LeaveParty(); }

	// action bar
	const float Bs = 42, Bw = 12 * (Bs + 4), Bx = FMath::Max(456.f, (VS.X - Bw) * 0.5f - 20), By = VS.Y - 66;
	Block(Bx - 6, By - 6, Bw + 8, Bs + 12);
	Panel(Bx - 6, By - 6, Bw + 8, Bs + 12);
	static const TCHAR* KeyNames[12] = { TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("0"), TEXT("-"), TEXT("=") };
	const FHLUnit* T = Gm.Unit(P->TargetId);
	for (int32 i = 0; i < 12; ++i)
	{
		const float X = Bx + i * (Bs + 4);
		const FHLAbility* Ab = Gm.Player.Bar.IsValidIndex(i) ? Gm.Player.Bar[i] : nullptr;
		Rc(X, By, Bs, Bs, FLinearColor(0.3f, 0.26f, 0.16f));
		++Layer; LastKind = 1;
		Rc(X + 1, By + 1, Bs - 2, Bs - 2, FLinearColor(0.06f, 0.06f, 0.07f));
		if (Ab)
		{
			const bool bUsable = P->Res >= Gm.AbCost(*P, *Ab) || P->ResType == EHLRes::None;
			const bool bRange = Ab->Target != TEXT("enemy") || (T && Gm.Dist(*P, *T) <= Ab->Range + T->Scale * 0.5f + 0.5f);
			const FLinearColor SC = Ab->IsHeal() ? FLinearColor(0.15f, 0.5f, 0.2f) : Ab->School == TEXT("electric") ? FLinearColor(0.15f, 0.35f, 0.7f) : Ab->School == TEXT("fire") ? FLinearColor(0.7f, 0.3f, 0.08f) : Ab->School == TEXT("acid") ? FLinearColor(0.4f, 0.55f, 0.1f) : Ab->School == TEXT("holy") ? FLinearColor(0.65f, 0.6f, 0.3f) : FLinearColor(0.45f, 0.2f, 0.15f);
			if (const FSlateBrush* IB = HLSkillBrush(Ab->Icon))
			{
				// out of range: red tint; not enough resource: blue-grey
				const FLinearColor Tint = (P->bDead || P->bGhost) ? FLinearColor(0.32f, 0.32f, 0.32f, 1) : !bUsable ? FLinearColor(0.35f, 0.4f, 0.6f, 1) : !bRange ? FLinearColor(1, 0.35f, 0.3f, 1) : FLinearColor::White;
				Img(X + 2, By + 2, Bs - 4, Bs - 4, IB, Tint);
			}
			else
			{
				++Layer; LastKind = 1;
				Rc(X + 3, By + 3, Bs - 6, Bs - 6, SC * (bUsable ? (bRange ? 1.f : 0.6f) : 0.35f));
				TArray<FString> Wd; Ab->Name.ParseIntoArray(Wd, TEXT(" "));
				for (int32 w = 0; w < Wd.Num() && w < 2; ++w) TxC(X + Bs * 0.5f, By + 8 + w * 12, Wd[w].Left(7), 8, ColWhite);
			}
			const float* Cd = P->CdUntil.Find(Ab);
			float Rem = Cd ? *Cd - Gm.Now : 0;
			const float Gcd = Ab->bGcd ? P->GcdUntil - Gm.Now : 0;
			if (Rem > 0 || Gcd > 0)
			{
				const float Frac = Rem > 0 ? FMath::Clamp(Rem / FMath::Max(1.f, Ab->Cd), 0.f, 1.f) : FMath::Clamp(Gcd / 1.5f, 0.f, 1.f);
				++Layer; LastKind = 1;
				Rc(X + 1, By + 1 + (Bs - 2) * (1 - Frac), Bs - 2, (Bs - 2) * Frac, FLinearColor(0, 0, 0, 0.65f));
				if (Rem > 1.5f) TxC(X + Bs * 0.5f, By + 14, FString::FromInt(FMath::CeilToInt32(Rem)), 13, ColGold, true);
			}
			if (Hover(X, By, Bs, Bs)) { AbilityTip(*Ab); if (bLClick) { bLClick = false; Gm.UseBar(i); } }
		}
		Tx(X + 3, By + Bs - 14, KeyNames[i], 8, ColDim);
	}
	// xp bar
	{
		const int32 Need = FHLGame::XpToNext(P->Level);
		Block(Bx - 6, VS.Y - 18, Bw + 8, 12);
		Bar(Bx - 6, VS.Y - 16, Bw + 8, 9, (float)Gm.Player.Xp / Need, Gm.Player.Rested >= 1 ? FLinearColor(0.2f, 0.35f, 0.85f) : FLinearColor(0.5f, 0.15f, 0.7f), Gm.Player.Rested >= 1 ? FString::Printf(TEXT("Level %d   XP %d / %d   Rested +%d"), P->Level, Gm.Player.Xp, Need, (int32)Gm.Player.Rested) : FString::Printf(TEXT("Level %d   XP %d / %d"), P->Level, Gm.Player.Xp, Need), 7);
		if (Gm.Player.Rested >= 1)
		{
			const float Rx = Bx - 6 + (Bw + 8) * FMath::Min(1.f, (Gm.Player.Xp + Gm.Player.Rested) / (float)Need);
			Rc(Rx - 1, VS.Y - 18, 3, 13, FLinearColor(0.45f, 0.7f, 1.f));
			if (Hover(Bx - 6, VS.Y - 16, Bw + 8, 9)) { TipText = FString::Printf(TEXT("Rested\nThe next %d experience from kills is doubled."), (int32)Gm.Player.Rested); TipPos = Mouse; }
		}
	}
	// cast / interaction bar
	if (P->Casting) Bar(VS.X * 0.5f - 130, VS.Y - 150, 260, 18, (Gm.Now - P->CastStart) / FMath::Max(0.1f, P->CastEnd - P->CastStart), FLinearColor(0.9f, 0.7f, 0.1f), P->Casting->Name, 10);
	else if (Gm.InteractObj >= 0) Bar(VS.X * 0.5f - 130, VS.Y - 150, 260, 18, (Gm.Now - Gm.InteractStart) / FMath::Max(0.1f, Gm.InteractEnd - Gm.InteractStart), FLinearColor(0.3f, 0.7f, 0.9f), Gm.InteractLabel, 10);
	else if (Gm.CraftIdx >= 0 && FHLGame::GunRecipes().IsValidIndex(Gm.CraftIdx))
	{
		const FHLGame::FHLRecipe& R = FHLGame::GunRecipes()[Gm.CraftIdx];
		Bar(VS.X * 0.5f - 130, VS.Y - 150, 260, 18, (float)((Gm.Now - Gm.CraftStart) / FMath::Max(0.1, Gm.CraftEnd - Gm.CraftStart)), FLinearColor(0.85f, 0.6f, 0.25f), Gm.CraftLeft > 1 ? FString::Printf(TEXT("%s (%d)"), *R.Product, Gm.CraftLeft) : R.Product, 10);
	}
	if (P->bAutoAttack && !P->bDead && !P->bGhost && T && !T->bDead) TxC(VS.X * 0.5f, VS.Y - 96, P->bRangedAuto ? TEXT("Auto-shot") : TEXT("Attacking"), 9, FLinearColor(1, 0.5f, 0.4f));
	// error and banner text
	if (Gm.Now < Gm.ErrorUntil && !(P->bDead && Gm.ErrorText == TEXT("You are dead."))) TxC(VS.X * 0.5f, 170, Gm.ErrorText, 14, FLinearColor(1, 0.2f, 0.15f, FMath::Min(1.f, Gm.ErrorUntil - (float)Gm.Now)), true);
	if (Gm.Now < Gm.NoticeUntil && !Gm.NoticeText.IsEmpty())
	{
		// pops in a size larger, settles, fades at the end
		const float Age = (float)(Gm.Now - Gm.NoticeStart), A = FMath::Clamp((float)(Gm.NoticeUntil - Gm.Now) * 1.5f, 0.f, 1.f);
		const int32 Sz = Age < 0.15f ? 26 : 22;
		const float Y = VS.Y * 0.30f, Wd = TW(Gm.NoticeText, Sz, true) + 40;
		RRc(VS.X * 0.5f - Wd * 0.5f, Y - 6, Wd, Sz * 1.6f + 8, FLinearColor(0, 0, 0, 0.45f * A), 10);
		TxC(VS.X * 0.5f, Y, Gm.NoticeText, Sz, FLinearColor(Gm.NoticeColor.R, Gm.NoticeColor.G, Gm.NoticeColor.B, A), true);
	}
	{
		const double Life = 4.0 * FHLSettings::Get().TextHold;
		Gm.Toasts.RemoveAll([&](const FHLGame::FToast& Ts) { return Gm.Now - Ts.At > Life; });
		float Ty = VS.Y - 128;
		for (int32 i = Gm.Toasts.Num() - 1; i >= 0; --i)
		{
			const FHLGame::FToast& Ts = Gm.Toasts[i];
			const float Age = (float)(Gm.Now - Ts.At), A = FMath::Clamp((float)(Life - Age) * 2.f, 0.f, 1.f) * FMath::Clamp(Age * 8.f, 0.f, 1.f);
			const float Wd = TW(Ts.Text, 11, true) + 28, Rise = (1.f - FMath::Clamp(Age * 8.f, 0.f, 1.f)) * 8.f;
			RRc(VS.X * 0.5f - Wd * 0.5f, Ty - 3 + Rise, Wd, 22, FLinearColor(0.04f, 0.035f, 0.03f, 0.75f * A), 8);
			TxC(VS.X * 0.5f, Ty + Rise, Ts.Text, 11, FLinearColor(Ts.Color.R, Ts.Color.G, Ts.Color.B, A), true);
			Ty -= 26;
		}
	}
	float CardBottom = 0; // the quest card's lower edge, so the zone banner can drop under it
	if (Gm.Now < Gm.QuestCardUntil)
	{
		// the quest card: pops in, holds, fades; gold-framed so it reads over any scene
		const float Age = (float)(Gm.Now - Gm.QuestCardStart), A = FMath::Clamp((float)(Gm.QuestCardUntil - Gm.Now) * 1.2f, 0.f, 1.f) * FMath::Clamp(Age * 6.f, 0.f, 1.f);
		const float Pop = 1.f + 0.12f * FMath::Clamp(1.f - Age * 5.f, 0.f, 1.f);
		const int32 Ts = FMath::RoundToInt(24 * Pop);
		const float Wd = FMath::Max(TW(Gm.QuestCardTitle, Ts, true), TW(Gm.QuestCardSub, 13, true)) + 80, Ht = 78, Cy = FMath::Max(VS.Y * 0.125f, TourBottom > 0 ? TourBottom + 14 : 0.f); // clear of the opening-night card
		const FLinearColor C = Gm.QuestCardColor;
		CardBottom = Cy + Ht + 2;
		RRc(VS.X * 0.5f - Wd * 0.5f - 2, Cy - 2, Wd + 4, Ht + 4, FLinearColor(C.R, C.G, C.B, 0.85f * A), 10);
		++Layer; LastKind = 1;
		RRc(VS.X * 0.5f - Wd * 0.5f, Cy, Wd, Ht, FLinearColor(0.05f, 0.035f, 0.025f, 0.9f * A), 9);
		++Layer; LastKind = 1;
		Rc(VS.X * 0.5f - Wd * 0.5f + 18, Cy + 44, Wd - 36, 1, FLinearColor(C.R, C.G, C.B, 0.5f * A));
		TxC(VS.X * 0.5f, Cy + 10, Gm.QuestCardTitle, Ts, FLinearColor(C.R, C.G, C.B, A), true);
		TxC(VS.X * 0.5f, Cy + 51, Gm.QuestCardSub, 13, FLinearColor(1, 0.97f, 0.9f, A), true);
	}
	if (Gm.Now < Gm.BannerUntil)
	{
		const float A = FMath::Min(1.f, Gm.BannerUntil - (float)Gm.Now);
		const float BnY = FMath::Max(210.f, CardBottom + 16);
		TxC(VS.X * 0.5f, BnY, Gm.BannerText, 26, FLinearColor(1, 0.85f, 0.4f, A), true);
		TxC(VS.X * 0.5f, BnY + 42, Gm.BannerSub, 13, Gm.bBannerHostile ? FLinearColor(1, 0.3f, 0.2f, A) : FLinearColor(1, 1, 1, A), Gm.bBannerHostile);
	}
	// micro menu
	{
		static const TCHAR* Lbl[] = { TEXT("Character"), TEXT("Bags"), TEXT("Quests"), TEXT("Abilities"), TEXT("Map"), TEXT("Who"), TEXT("Talents"), TEXT("Menu") };
		bool* Flags[] = { &bChar, &bBags, &bQuests, &bSpells, &bMap, &bWho, &bTalents, &bMenu };
		// one framed strip of adjacent bevelled buttons (classic micro menu): 8 x 38 px, 6 px frame
		const float Bsz = 36, Gap = 2, Pad = 5, Mw = 8 * Bsz + 7 * Gap + Pad * 2, Mh = Bsz + Pad * 2;
		const float Mx = VS.X - Mw - 12, Mtop = VS.Y - Mh - 8;
		Block(Mx - 2, Mtop - 2, Mw + 4, Mh + 4);
		Panel(Mx, Mtop, Mw, Mh, FLinearColor(0.05f, 0.045f, 0.04f), FLinearColor(0.72f, 0.56f, 0.28f));
		static const TCHAR* Icons[] = { TEXT("menu_char"), TEXT("menu_bags"), TEXT("menu_quests"), TEXT("menu_skills"), TEXT("menu_map"), TEXT("menu_who"), TEXT("menu_talents"), TEXT("menu_menu") };
		for (int32 i = 0; i < 8; ++i)
		{
			const float X = Mx + Pad + i * (Bsz + Gap), Y = Mtop + Pad;
			const bool bH = Hover(X, Y, Bsz, Bsz), bOn = *Flags[i];
			++Layer; LastKind = 1;
			// bevel: black outline, light top-left edge, dark bottom-right edge (inverted while the window is open)
			Rc(X, Y, Bsz, Bsz, FLinearColor(0, 0, 0, 1));
			++Layer; LastKind = 1;
			const FLinearColor Lite = bH ? FLinearColor(1, 0.86f, 0.5f) : FLinearColor(0.62f, 0.5f, 0.3f), Dark(0.16f, 0.12f, 0.07f);
			Rc(X + 1, Y + 1, Bsz - 2, 2, bOn ? Dark : Lite); Rc(X + 1, Y + 1, 2, Bsz - 2, bOn ? Dark : Lite);
			Rc(X + 1, Y + Bsz - 3, Bsz - 2, 2, bOn ? Lite : Dark); Rc(X + Bsz - 3, Y + 1, 2, Bsz - 2, bOn ? Lite : Dark);
			const float O = bOn ? 1.f : 0.f; // pressed: the face sinks a pixel
			if (const FSlateBrush* IB = HLMenuBrush(Icons[i]))
				Img(X + 3 + O, Y + 3 + O, Bsz - 6, Bsz - 6, IB, bH || bOn ? FLinearColor::White : FLinearColor(0.78f, 0.78f, 0.78f, 1));
			else TxC(X + Bsz * 0.5f, Y + 11, FString(Lbl[i]).Left(4), 8, ColWhite);
			if (bOn) { ++Layer; LastKind = 1; Rc(X + 3, Y + 3, Bsz - 6, 1, FLinearColor(1, 0.8f, 0.35f, 0.9f)); Rc(X + 3, Y + Bsz - 4, Bsz - 6, 1, FLinearColor(1, 0.8f, 0.35f, 0.9f)); }
			// the key that opens it, in the bottom-right corner like an action-bar keybind
			{
				static const EHLAct Acts[] = { EHLAct::Character, EHLAct::Bags, EHLAct::Quests, EHLAct::Spellbook, EHLAct::Map, EHLAct::Social, EHLAct::Talents, EHLAct::Count };
				const FString KeyL = Acts[i] == EHLAct::Count ? FString(TEXT("Esc")) : FHLSettings::Get().KeyName(Acts[i]);
				const float KW = TW(KeyL, 8, true);
				++Layer; LastKind = 1;
				Rc(X + Bsz - KW - 6 + O, Y + Bsz - 14 + O, KW + 4, 11, FLinearColor(0, 0, 0, 0.6f));
				Tx(X + Bsz - KW - 4 + O, Y + Bsz - 15 + O, KeyL, 8, FLinearColor(1, 0.92f, 0.7f), true);
			}
			if (bH) { TipText = Lbl[i]; TipPos = Mouse; if (bLClick) { bLClick = false; *Flags[i] = !*Flags[i]; if (i == 5) Gm.bWhoOpen = bWho; } }
		}
		{
			const FString Info = HLMoney(Gm.Player.Cents);
			Tx(VS.X - 14 - TW(Info.TrimEnd(), 10), Mtop - 18, Info.TrimEnd(), 10, ColGold);
		}
	}
	DrawQuestMarkers();
	DrawMinimap();
	DrawTracker();
	DrawChat();
	DrawTour();
}

void SHLGameUI::AbilityTip(const FHLAbility& Ab) const
{
	const FHLUnit* P = G().PlayerUnit();
	FString Res = P && P->Cls ? P->Cls->Resource : TEXT("power");
	Res[0] = FChar::ToUpper(Res[0]);
	const float Cost = P ? G().AbCost(*P, Ab) : Ab.Cost;
	TipText = FString::Printf(TEXT("%s\n%s%s%s\n%s"), *Ab.Name,
		Cost > 0 ? *FString::Printf(TEXT("%d %s   "), FMath::RoundToInt32(Cost), *Res) : TEXT(""),
		Ab.Range > 6 ? *FString::Printf(TEXT("%d m range   "), (int32)Ab.Range) : (Ab.Target == TEXT("enemy") ? TEXT("Melee range   ") : TEXT("")),
		*FString::Printf(TEXT("%s%s"), Ab.Cast > 0 ? *FString::Printf(TEXT("%.1f s cast"), Ab.Cast) : TEXT("Instant"), Ab.Cd > 0 ? *FString::Printf(TEXT("   %d s cooldown"), (int32)Ab.Cd) : TEXT("")),
		*Ab.Desc);
	if (!Ab.Flavor.IsEmpty()) TipText += TEXT("\n") + Ab.Flavor;
	TipPos = Mouse;
}

// Where the quest wants the player next: the turn-in NPC when complete, else the nearest camp, object or NPC for the
// first unfinished objective. Only targets in the player's current area.
static bool HLQuestGoal(const FHLGame& Gm, const FHLQuestState& QS, const FHLUnit& P, FVector2D& Out)
{
	const FHLAreaDef& Def = *Gm.Areas[P.Area].Def;
	float Best = 1e9f;
	auto Try = [&](const FVector2D& W) { const float D = FVector2D::DistSquared(W, P.Pos); if (D < Best) { Best = D; Out = W; } };
	auto TryNpc = [&](const FString& Id) { for (const FHLNpcDef& N : Def.Npcs) if (N.Id == Id) Try(N.Pos); };
	if (QS.bComplete) { TryNpc(QS.Q->Turnin); return Best < 1e9f; }
	for (int32 i = 0; i < QS.Q->Objectives.Num(); ++i)
	{
		const FHLObjective& O = QS.Q->Objectives[i];
		if (!QS.Progress.IsValidIndex(i) || QS.Progress[i] >= O.Count) continue;
		if (O.Type == TEXT("talk")) TryNpc(O.Npc);
		else if (O.Type == TEXT("escort")) Try(O.To);
		else if (O.Type == TEXT("craft")) {}
		else if (O.Type == TEXT("do")) { if (!O.Npc.IsEmpty()) TryNpc(O.Npc.Replace(TEXT("{cls}"), Gm.Player.Cls ? *Gm.Player.Cls->Id : TEXT(""))); }
		else if (O.Type == TEXT("kill") || O.Type == TEXT("item"))
		{
			const TArray<FString>& Ids = O.Type == TEXT("kill") ? O.Mobs : O.From;
			for (const FHLCamp& C : Def.Camps) if (C.Mob && Ids.Contains(C.Mob->Id)) Try(C.Pos);
		}
		else
		{
			// only objects that can be used now: a lamp just lit (or a can just taken) is out until it respawns,
			// so the marker moves on to the next one instead of sitting on the spent one
			const TArray<FHLObjectState>& States = Gm.Areas[P.Area].Objects;
			for (int32 k = 0; k < Def.Objects.Num(); ++k)
			{
				const FHLObjectDef& Ob = Def.Objects[k];
				if (States.IsValidIndex(k) && !States[k].bActive) continue;
				if (Ob.Quest == QS.Q->Id || (!O.Kind.IsEmpty() && Ob.Kind == O.Kind)) Try(Ob.Pos);
			}
		}
		if (Best < 1e9f) return true;
	}
	return false;
}
static const TCHAR* HLCompass(const FVector2D& D) // x east, y south
{
	static const TCHAR* Names[8] = { TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW"), TEXT("N"), TEXT("NE") };
	const float A = FMath::Atan2(D.Y, D.X);
	return Names[((int32)FMath::RoundToInt(A / (PI / 4)) + 8) % 8];
}

void SHLGameUI::DrawQuestMarkers() const
{
	FHLGame& Gm = G();
	const FHLUnit* P = Gm.PlayerUnit();
	if (!P || P->bDead || !PC || !PC->PlayerCameraManager) return;
	const FVector CamLoc = PC->PlayerCameraManager->GetCameraLocation();
	const FRotator CamRot = PC->PlayerCameraManager->GetCameraRotation();
	int32 Shown = 0;
	TArray<TPair<FVector2D, FLinearColor>> Goals;
	if (P->bGhost && P->Area == Gm.Player.CorpseArea) Goals.Add({ Gm.Player.CorpsePos, FLinearColor(0.6f, 0.85f, 1.f) });
	else if (Gm.SurgeHub >= 0 && P->Area == 0 && Gm.Hubs.IsValidIndex(Gm.SurgeHub)) Goals.Add({ Gm.Hubs[Gm.SurgeHub].Pos, FLinearColor(1, 0.35f, 0.25f) });
	// the map mark (guard directions, the map's service list) leads; it clears on arrival
	int32 MarkIdx = -1;
	if (!P->bGhost && Gm.MarkArea == P->Area)
	{
		if (FVector2D::Distance(Gm.MarkPos, P->Pos) < 8) { Gm.System(FString::Printf(TEXT("You have reached %s."), *Gm.MarkLabel)); Gm.MarkArea = -1; }
		else { MarkIdx = Goals.Num(); Goals.Add({ Gm.MarkPos, FLinearColor(0.45f, 0.9f, 1.f) }); }
	}
	if (!P->bGhost) for (const FHLQuestState& QS : Gm.Player.Quests)
	{
		FVector2D Goal;
		if (Goals.Num() < 3 && HLQuestGoal(Gm, QS, *P, Goal)) Goals.Add({ Goal, QS.bComplete ? FLinearColor(1, 0.85f, 0.2f) : FLinearColor(1, 0.95f, 0.6f) });
	}
	for (int32 gi = 0; gi < Goals.Num(); ++gi)
	{
		const auto& GC = Goals[gi];
		const FVector2D Goal = GC.Key;
		const int32 Meters = (int32)FMath::RoundToInt(FVector2D::Distance(Goal, P->Pos));
		const FString Dist = gi == MarkIdx ? FString::Printf(TEXT("%s  %d m"), *Gm.MarkLabel, Meters) : FString::Printf(TEXT("%d m"), Meters);
		++Shown;
		const float D = FVector2D::Distance(Goal, P->Pos);
		if (D < 6) continue;
		const FLinearColor C = GC.Value;
		const FVector Wp = World->ToWorld(P->Area, Goal, 3);
		FVector2D S;
		const bool bOn = Project(Wp, S) && S.X > 40 && S.X < VS.X - 40 && S.Y > 60 && S.Y < VS.Y - 160;
		if (bOn)
		{
			// diamond over the goal with the distance under it
			const float R = 7;
			Line(S + FVector2D(0, -R), S + FVector2D(R, 0), C, 2); Line(S + FVector2D(R, 0), S + FVector2D(0, R), C, 2);
			Line(S + FVector2D(0, R), S + FVector2D(-R, 0), C, 2); Line(S + FVector2D(-R, 0), S + FVector2D(0, -R), C, 2);
			TxC(S.X, S.Y + 9, Dist, 9, C);
		}
		else
		{
			// arrow on an ellipse around the screen centre, pointing the way relative to the camera (along the route: a
			// path is found once a second and the arrow aims at the farthest point of it in plain sight)
			FVector Aim = Wp;
			if (gi < 4 && D > 20)
			{
				FRouteCache& Rt = Routes[gi];
				if (Rt.Area != P->Area || FVector2D::Distance(Rt.Goal, Goal) > 4 || Gm.Now - Rt.T > 1.0 || FVector2D::Distance(Rt.From, P->Pos) > 6)
				{
					const FHLNav& Nv = Gm.Areas[P->Area].Nav;
					TArray<FVector2D> Path;
					Rt.Way = Goal;
					if (!Nv.LineClear(P->Pos, Goal) && Nv.FindPath(P->Pos, Goal, Path) && Path.Num() > 0)
					{
						Rt.Way = Path[0];
						for (int32 k = 0; k < Path.Num() && k < 60; ++k) { if (Nv.LineClear(P->Pos, Path[k])) Rt.Way = Path[k]; else break; }
					}
					Rt.Goal = Goal; Rt.From = P->Pos; Rt.T = Gm.Now; Rt.Area = P->Area;
				}
				if (FVector2D::Distance(Rt.Way, P->Pos) > 3) Aim = World->ToWorld(P->Area, Rt.Way, 3);
			}
			const FVector L = CamRot.UnrotateVector(Aim - CamLoc);
			const float Ang = FMath::Atan2(L.Y, L.X); // 0 = straight ahead, + = right
			const FVector2D Dir(FMath::Sin(Ang), -FMath::Cos(Ang));
			const FVector2D Ctr(VS.X * 0.5f, VS.Y * 0.45f);
			const FVector2D Tip = Ctr + FVector2D(Dir.X * VS.X * 0.3f, Dir.Y * VS.Y * 0.32f);
			const FVector2D Side(-Dir.Y, Dir.X);
			const FVector2D Base = Tip - Dir * 16;
			Line(Base + Side * 9, Tip, C, 3); Line(Base - Side * 9, Tip, C, 3); Line(Base + Side * 9, Base - Side * 9, C, 2);
			TxC(FMath::Clamp(Base.X - Dir.X * 14, TW(Dist, 9) * 0.5f + 8, VS.X - TW(Dist, 9) * 0.5f - 8), Base.Y - Dir.Y * 14 - 6, Dist, 9, C);
		}
	}
}

void SHLGameUI::DrawMinimap() const
{
	FHLGame& Gm = G();
	HudReserve.Reset();
	HudReserve.Add(FBox2D(FVector2D(VS.X - 236, 0), FVector2D(VS.X, 236)));
	const FHLUnit* P = Gm.PlayerUnit();
	const FHLAreaState& Area = Gm.Areas[P->Area];
	const float S = 170, X = VS.X - S - 16, Y = 16, Range = 80, K = S / (Range * 2);
	Block(X, Y, S, S + 22);
	Panel(X, Y, S, S, FLinearColor(0.1f, 0.1f, 0.09f));
	auto Clip = [&](float Wx, float Wy, float Ww, float Wd, const FLinearColor& C)
	{
		float X0 = X + S * 0.5f + (Wx - P->Pos.X) * K, Y0 = Y + S * 0.5f + (Wy - P->Pos.Y) * K, X1 = X0 + Ww * K, Y1 = Y0 + Wd * K;
		X0 = FMath::Max(X0, X); Y0 = FMath::Max(Y0, Y); X1 = FMath::Min(X1, X + S); Y1 = FMath::Min(Y1, Y + S);
		if (X1 > X0 && Y1 > Y0) Rc(X0, Y0, X1 - X0, Y1 - Y0, C);
	};
	for (const FHLBox& B : Area.Def->Ground) if (B.W > 3 && B.D > 3) Clip(B.X, B.Y, B.W, B.D, FLinearColor(0.2f, 0.2f, 0.2f));
	++Layer; LastKind = 1;
	for (const FHLBox& B : Area.Def->Boxes) if (B.bSolid && B.H >= 1) Clip(B.X, B.Y, B.W, B.D, FLinearColor(B.Color.R * 0.8f, B.Color.G * 0.8f, B.Color.B * 0.8f));
	++Layer; LastKind = 1;
	// hovering a marker names it: the nearest one under the cursor wins
	const bool bOverMap = Hover(X, Y, S, S);
	float TipBest = 8.f; FString TipWhat;
	auto Note = [&](const FVector2D& At, const FString& What) { if (!bOverMap) return; const float Dm = FVector2D::Distance(At, Mouse); if (Dm < TipBest) { TipBest = Dm; TipWhat = What; } };
	auto Dot = [&](const FVector2D& Wp, const FLinearColor& C, float Sz, const FString& What = FString())
	{
		const float Dx = X + S * 0.5f + (Wp.X - P->Pos.X) * K, Dy = Y + S * 0.5f + (Wp.Y - P->Pos.Y) * K;
		if (Dx > X + 2 && Dx < X + S - 2 && Dy > Y + 2 && Dy < Y + S - 2) { Rc(Dx - Sz * 0.5f, Dy - Sz * 0.5f, Sz, Sz, C); if (!What.IsEmpty()) Note(FVector2D(Dx, Dy), What); }
	};
	for (const FHLUnit* U : Gm.UnitList)
	{
		if (U->Area != P->Area || U->bDead || U == P) continue;
		if (U->Kind == EHLKind::Npc)
		{
			if (U->Npc)
			{
				const int32 Mk = Gm.NpcMarker(*U->Npc);
				const FString What = U->Npc->Title.IsEmpty() ? U->Npc->Name : U->Npc->Name + TEXT("\n") + U->Npc->Title + (Mk == 1 ? TEXT("\nHas a quest for you") : Mk == 2 ? TEXT("\nA quest to hand in") : TEXT(""));
				Dot(U->Pos, Mk == 1 || Mk == 2 ? ColGold : FLinearColor(0.3f, 0.8f, 0.3f), Mk ? 6 : 3, What);
			}
		}
		else if (U->Kind == EHLKind::Sim) Dot(U->Pos, U->SimIdx >= 0 && Gm.Sims[U->SimIdx].bInParty ? FLinearColor(0.3f, 1, 1) : FLinearColor(0.45f, 0.55f, 1), 3, FString::Printf(TEXT("%s\nLevel %d"), *U->Name, U->Level));
		else if (FVector2D::Distance(U->Pos, P->Pos) < 45) Dot(U->Pos, FLinearColor(1, 0.25f, 0.2f), 3, FString::Printf(TEXT("%s\nLevel %d"), *U->Name, U->Level));
	}
	++Layer; LastKind = 1;
	for (const FHLQuestState& QS : Gm.Player.Quests)
	{
		FVector2D Goal;
		if (!HLQuestGoal(Gm, QS, *P, Goal)) continue;
		FVector2D M((Goal.X - P->Pos.X) * K, (Goal.Y - P->Pos.Y) * K);
		const float Lim = S * 0.5f - 6;
		const float Over = FMath::Max(FMath::Abs(M.X), FMath::Abs(M.Y)) / Lim;
		if (Over > 1) M /= Over;
		const FVector2D Q(X + S * 0.5f + M.X, Y + S * 0.5f + M.Y);
		const FLinearColor QC = QS.bComplete ? FLinearColor(1, 0.85f, 0.2f) : FLinearColor(1, 0.95f, 0.6f);
		Line(Q + FVector2D(0, -4), Q + FVector2D(4, 0), QC, 2); Line(Q + FVector2D(4, 0), Q + FVector2D(0, 4), QC, 2);
			Line(Q + FVector2D(0, 4), Q + FVector2D(-4, 0), QC, 2); Line(Q + FVector2D(-4, 0), Q + FVector2D(0, -4), QC, 2);
			Note(Q, QS.Q->Name + (QS.bComplete ? TEXT("\nReady to hand in") : TEXT("")));
		}
	if (Gm.MarkArea == P->Area)
	{
		FVector2D M((Gm.MarkPos.X - P->Pos.X) * K, (Gm.MarkPos.Y - P->Pos.Y) * K);
		const float Over = FMath::Max(FMath::Abs(M.X), FMath::Abs(M.Y)) / (S * 0.5f - 6);
		if (Over > 1) M /= Over;
		const FVector2D Q(X + S * 0.5f + M.X, Y + S * 0.5f + M.Y);
			Rc(Q.X - 4, Q.Y - 4, 8, 8, FLinearColor(0.05f, 0.1f, 0.15f)); Rc(Q.X - 3, Q.Y - 3, 6, 6, FLinearColor(0.45f, 0.9f, 1.f));
			Note(Q, Gm.MarkLabel.IsEmpty() ? FString(TEXT("Your marker")) : Gm.MarkLabel);
		}
	const float Rad = FMath::DegreesToRadians(P->Yaw);
	const FVector2D C(X + S * 0.5f, Y + S * 0.5f);
	Rc(C.X - 3, C.Y - 3, 6, 6, ColWhite);
	Line(C, C + FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)) * 12, ColWhite, 2);
	Note(C, TEXT("You"));
	if (!TipWhat.IsEmpty()) { TipText = TipWhat; TipPos = Mouse; }
	TxC(X + S * 0.5f, Y + S + 3, Gm.CurSubzone, 10, ColGold);
	if (const int32 Unread = Gm.UnreadMail())
	{
		const FString M = FString::Printf(TEXT("New mail (%d)"), Unread);
		Rc(X + 2, Y + S - 18, TW(M, 9) + 8, 16, FLinearColor(0, 0, 0, 0.7f));
		Tx(X + 6, Y + S - 17, M, 9, ColGold, true);
	}
	Tx(X + 4, Y + 2, TEXT("N"), 9, ColDim);
	Rc(X + S - 15, Y + 2, 13, 13, FLinearColor(0, 0, 0, 0.6f));
	TxC(X + S - 8.5f, Y + 2, TEXT("?"), 9, ColGold, true);
	if (Hover(X + S - 16, Y + 1, 15, 15))
	{
		TipText = TEXT("Minimap\nWhite arrow: you\nGold dot: a quest to take or hand in\nGreen dot: a townsperson\nBlue dot: another player (cyan: your party)\nRed dot: an enemy nearby\nGold diamond: where your quest goal is\nBlue square: your map marker");
		TipPos = Mouse;
	}
}

// Opening Night: a card at the top of the screen for the reel in progress, and a pulsing frame round the part of the
// interface it is talking about (an action-bar slot, a micro-menu button, the minimap, the bags).
void SHLGameUI::DrawTour() const
{
	FHLGame& Gm = G();
	const FHLUnit* P = Gm.PlayerUnit();
	TourBottom = 0;
	if (!P || P->bDead || bTourHidden) return;
	const FHLQuestState* Cur = nullptr;
	for (const FHLQuestState& QS : Gm.Player.Quests) if (QS.Q->Tour > 0 && (!Cur || QS.Q->Tour < Cur->Q->Tour)) Cur = &QS;
	const FHLQuest* Offer = nullptr;
	if (!Cur) for (const FHLQuest& Q : FHLData::Get().Quests) if (Q.Tour > 0 && Gm.QuestAvailable(Q)) { Offer = &Q; break; }
	if (!Cur && !Offer) return;
	const FHLQuest& Q = Cur ? *Cur->Q : *Offer;
	auto Done = [&](int32 i) { return Cur && Cur->Progress.IsValidIndex(i) && Cur->Progress[i] >= Q.Objectives[i].Count; };
	auto Key = [](EHLAct A) { return FHLSettings::Get().KeyName(A); };
	auto NpcName = [](const FString& Id) { const FHLNpcDef* N = FHLData::Get().Npc(Id); return N ? N->Name : FString(TEXT("them")); };
	const FString& Mode = Gm.OpenNpcMode;
	// highlight targets: 0 none, 1..12 action-bar slot, 20+i micro-menu button i, 40 minimap, 41 the bag window
	int32 Hi = 0;
	FString Line;
	if (!Cur)
	{
		Line = FString::Printf(TEXT("%s has the next reel for you: look for the gold ! over their head and right-click them."), *NpcName(Q.Giver));
		Hi = 40;
	}
	else if (Cur->bComplete)
	{
		Line = FString::Printf(TEXT("Done. Back to %s."), *NpcName(Q.Turnin));
		Hi = 40;
	}
	else switch (Q.Tour)
	{
	case 1:
	{
		const FHLClass* C = Gm.Player.Cls;
		const FString A1 = Gm.Player.Bar.IsValidIndex(0) && Gm.Player.Bar[0] ? Gm.Player.Bar[0]->Name : TEXT("your first ability");
		const FString A2 = Gm.Player.Bar.IsValidIndex(1) && Gm.Player.Bar[1] ? Gm.Player.Bar[1]->Name : FString();
		FString How;
		if (!C) How = TEXT("Press 1 to attack.");
		else if (C->Id == TEXT("veteran")) How = FString::Printf(TEXT("Veterans fight up close. Adrenaline (the red bar) starts empty and fills as you hit and get hit: walk in, let it build, then spend it on %s (1)."), *A1);
		else if (C->Id == TEXT("stuntman")) How = FString::Printf(TEXT("Stuntmen hit hard and fast. Energy (the yellow bar) refills on its own: get close and throw %s (1) whenever it's full enough."), *A1);
		else if (C->Id == TEXT("gunman")) How = FString::Printf(TEXT("Gunmen fight from range. Start with %s (1) from 30 m, then auto-shot fires on its own while you keep your distance."), *A1);
		else if (C->Role == TEXT("healer")) How = FString::Printf(TEXT("%s (1) hurts from range. %s (2) mends: with no friend targeted it heals you. Power (blue) comes back when you stop casting."), *A1, *A2);
		else if (C->bCaster) How = FString::Printf(TEXT("You cast from range: stand still while %s (1) fills its bar. Power (blue) comes back when you stop casting for a moment."), *A1);
		else How = FString::Printf(TEXT("%s (1) is cheap: get close and chain it. Your weapon swings on its own between."), *A1);
		Line = TEXT("W A S D to move. Click a Worker behind the theatre, then press 1. ") + How;
		Hi = 1;
		break;
	}
	case 2:
		if (Mode == TEXT("trainer")) Line = TEXT("Click the new ability to learn it.");
		else Line = FString::Printf(TEXT("Your class trainer, %s, stands on the forecourt with the others. Right-click them and ask to train."), *NpcName(FString(TEXT("npc_trainer_")) + (Gm.Player.Cls ? Gm.Player.Cls->Id : FString())));
		Hi = Mode == TEXT("trainer") ? 0 : 40;
		break;
	case 3:
		if (!bTalents) { Line = FString::Printf(TEXT("Open your talents: press %s, or the Talents button down here."), *Key(EHLAct::Talents)); Hi = 26; }
		else Line = TEXT("Pick a talent in the top row.");
		break;
	case 4:
		if (Mode == TEXT("bank")) { Line = TEXT("This is your vault: it holds what your bags can't. Right-click the Premiere Ticket in your bags to deposit it."); Hi = 41; }
		else Line = TEXT("Walk east along the boulevard to the First National Bank. Miss Frost is at the window: right-click her and check your deposit box.");
		break;
	case 5:
		if (Mode == TEXT("vendor") && !Done(0)) Line = TEXT("Buy a hat, or anything you like the look of.");
		else if (!Done(0)) Line = TEXT("Saul Hirsch's outfitters is two doors east of the bank. Right-click him and browse his goods.");
		else { Line = FString::Printf(TEXT("Right-click what you bought in your bags to wear it. Your Character window (%s) shows everything you have on."), *Key(EHLAct::Character)); Hi = bBags ? 41 : 21; }
		break;
	case 6:
		if (!Done(0)) Line = Mode == TEXT("redcar") ? TEXT("Pick Vine Street and press Ride. It costs 5 cents and takes a few seconds.") : TEXT("Conductor Ruiz is at the Highland red car stop, across the street from the bank. Right-click him.");
		else Line = TEXT("This is Vine Street. Cornelius Vane is in front of the Taft Building, by the auction booth.");
		break;
	case 7:
		if (Mode != TEXT("auction")) Line = TEXT("Right-click Cornelius Vane (or Miss Reed) and ask for the listings.");
		else if (!Done(0)) Line = TEXT("Everything other players are selling.");
		else { Line = TEXT("Now sell: click the Souvenir Postcards in your bags, set a price, and list them. If they sell, the money comes by mail."); Hi = 41; }
		break;
	case 8:
		if (P->bMounted) Line = TEXT("You're mounted: 60% faster. Take a lap of Vine Street before Eddie wants it back.");
		else if (Mode == TEXT("mounts")) Line = TEXT("Pick one and take it for a test drive. The real thing comes at level 40, so enjoy it.");
		else Line = TEXT("Eddie Kellerman's lot is just east of the Taft Building. Right-click him and see what's on the lot.");
		break;
	case 9:
		if (!Done(0)) { Line = FString::Printf(TEXT("%s opens your map."), *Key(EHLAct::Map)); Hi = 24; }
		else Line = TEXT("Head back to Sid at Grauman's Chinese, at the west end. The red car runs back to Highland if you'd rather ride.");
		break;
	}
	// the card
	const float W = 460, X = FMath::Max(506.f, VS.X * 0.5f - W * 0.5f), Y = 12;
	TArray<FString> Wrapped;
	Wrap(Line, 10, W - 24, Wrapped);
	const float TextH = Wrapped.Num() * 10 * 1.55f;
	const float H = 46 + TextH + 8;
	Block(X, Y, W, H);
	HudReserve.Add(FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H)));
	TourBottom = Y + H;
	Panel(X, Y, W, H, FLinearColor(0.06f, 0.05f, 0.04f, 0.96f), FLinearColor(0.85f, 0.66f, 0.3f));
	++Layer; LastKind = 1;
	Tx(X + 12, Y + 7, FString::Printf(TEXT("OPENING NIGHT   Reel %d of 9"), Q.Tour), 9, FLinearColor(0.9f, 0.3f, 0.2f), true);
	Tx(X + 12, Y + 22, Q.Name, 12, ColGold, true);
	TxWrap(X + 12, Y + 42, W - 24, Line, 10, ColWhite);
	if (Btn(X + W - 74, Y + 6, 64, 16, TEXT("Hide tips"), true, 8)) bTourHidden = true;
	// the highlight
	if (Hi == 0) return;
	float Hx = 0, Hy = 0, Hw = 0, Hh = 0;
	if (Hi >= 1 && Hi <= 12)
	{
		const float Bs = 42, Bw = 12 * (Bs + 4), Bx = FMath::Max(456.f, (VS.X - Bw) * 0.5f - 20), By = VS.Y - 66;
		Hx = Bx + (Hi - 1) * (Bs + 4); Hy = By; Hw = Bs; Hh = Bs;
	}
	else if (Hi >= 20 && Hi < 28)
	{
		const float Bsz = 36, Gap = 2, Pad = 5, Mw = 8 * Bsz + 7 * Gap + Pad * 2, Mh = Bsz + Pad * 2;
		Hx = VS.X - Mw - 12 + Pad + (Hi - 20) * (Bsz + Gap); Hy = VS.Y - Mh - 8 + Pad; Hw = Bsz; Hh = Bsz;
	}
	else if (Hi == 40) { Hx = VS.X - 222; Hy = 8; Hw = 214; Hh = 214; }
	else if (Hi == 41)
	{
		if (!bBags) { const float Bsz = 36, Gap = 2, Pad = 5, Mw = 8 * Bsz + 7 * Gap + Pad * 2, Mh = Bsz + Pad * 2; Hx = VS.X - Mw - 12 + Pad + (Bsz + Gap); Hy = VS.Y - Mh - 8 + Pad; Hw = Bsz; Hh = Bsz; }
		else { const FVector2D* R = WinPos.Find(FName(TEXT("bags"))); if (!R) return; Hx = R->X; Hy = R->Y; Hw = 270; Hh = 228; }
	}
	const float Pulse = 0.55f + 0.45f * FMath::Sin((float)Gm.Now * 6.f), G2 = 3 + 3 * Pulse;
	const FLinearColor C(1.f, 0.82f, 0.3f, 0.5f + 0.5f * Pulse);
	++Layer; LastKind = 1;
	Rc(Hx - G2, Hy - G2, Hw + 2 * G2, 3, C); Rc(Hx - G2, Hy + Hh + G2 - 3, Hw + 2 * G2, 3, C);
	Rc(Hx - G2, Hy - G2, 3, Hh + 2 * G2, C); Rc(Hx + Hw + G2 - 3, Hy - G2, 3, Hh + 2 * G2, C);
}

void SHLGameUI::DrawTracker() const
{
	FHLGame& Gm = G();
	float X = VS.X - 250, Y = 222;
	ON_SCOPE_EXIT { if (Y > 224) HudReserve.Add(FBox2D(FVector2D(X - 8, 214), FVector2D(VS.X, Y + 4))); };
	auto Clock = [](double S) { const int32 T = FMath::Max(0, (int32)S); return FString::Printf(TEXT("%d:%02d"), T / 60, T % 60); };
	if (Gm.SurgeHub >= 0 && Gm.Hubs.IsValidIndex(Gm.SurgeHub))
	{
		const FHLGame::FHLHub& H = Gm.Hubs[Gm.SurgeHub];
		Tx(X, Y, TEXT("Hive Surge"), 10, FLinearColor(1, 0.4f, 0.3f), true);
		if (const FHLUnit* PU = Gm.PlayerUnit()) { const FString Hint = FString::Printf(TEXT("%s %d m"), HLCompass(H.Pos - PU->Pos), (int32)FVector2D::Distance(H.Pos, PU->Pos)); Tx(VS.X - 16 - TW(Hint, 9), Y + 1, Hint, 9, ColDim); }
		Y += 15;
		Tx(X + 8, Y, FString::Printf(TEXT("Defend %s"), *H.Name), 9, ColWhite); Y += 14;
		Tx(X + 8, Y, FString::Printf(TEXT("%s left    Invaders slain: %d"), *Clock(Gm.SurgeEnd - Gm.Now), Gm.SurgeKills), 9, ColWhite); Y += 18;
	}
	if (Gm.bFilmAnnounced)
	{
		const bool bOn = Gm.FilmStart > 0;
		Tx(X, Y, TEXT("Picture Night"), 10, FLinearColor(0.75f, 0.85f, 1.f), true); Y += 15;
		Tx(X + 8, Y, FString::Printf(TEXT("%s at Grauman's Chinese"), *Gm.FilmShort), 9, ColWhite); Y += 14;
		Tx(X + 8, Y, bOn ? (Gm.bFilmBuffGiven ? FString::Printf(TEXT("%s left (buff received)"), *Clock(Gm.FilmEnd - Gm.Now)) : FString::Printf(TEXT("%s left. Watch to be inspired"), *Clock(Gm.FilmEnd - Gm.Now))) : FString::Printf(TEXT("Starts in %s"), *Clock(Gm.NextFilmAt - Gm.Now)), 9, ColDim); Y += 18;
	}
	for (int32 s = 1; s < Gm.HiveSites.Num(); ++s) if (Gm.HiveSites[s].bActive)
	{
		Tx(X, Y, TEXT("Hive Spread"), 10, FLinearColor(1.f, 0.45f, 0.35f), true); Y += 15;
		const FHLUnit* PU = Gm.PlayerUnit();
		const FVector2D At = Gm.HiveSitePos(s);
		const float Dm = PU && PU->Area == 0 ? FVector2D::Distance(PU->Pos, At) : -1;
		Tx(X + 8, Y, FString::Printf(TEXT("Destroy the egg clusters near %s: %d left%s"), *Gm.HiveSiteName(s), Gm.HiveEggsLeft(s), Dm >= 0 ? *FString::Printf(TEXT("  (%d m)"), (int32)Dm) : TEXT("")), 9, ColWhite); Y += 18;
	}
	if (Gm.Fight.bAnnounced)
	{
		Tx(X, Y, TEXT("Fight Night"), 10, FLinearColor(1.f, 0.7f, 0.35f), true); Y += 15;
		Tx(X + 8, Y, TEXT("Outside the Roosevelt Hotel"), 9, ColWhite); Y += 14;
		int32 A = -1, B = -1;
		if (Gm.Fight.Stage == 0) Tx(X + 8, Y, FString::Printf(TEXT("Starts in %s"), *Clock(Gm.Fight.NextAt - Gm.Now)), 9, ColDim);
		else if (Gm.Fight.Stage == 1 && Gm.FightPair(A, B)) Tx(X + 8, Y, FString::Printf(TEXT("%s vs %s"), *Gm.Sims[A].P->Handle, *Gm.Sims[B].P->Handle), 9, ColDim);
		else if (Gm.Sims.IsValidIndex(Gm.Fight.Champ)) Tx(X + 8, Y, FString::Printf(TEXT("Champion: %s. Beat them for a purse"), *Gm.Sims[Gm.Fight.Champ].P->Handle), 9, ColDim);
		else Tx(X + 8, Y, TEXT("Final bout"), 9, ColDim);
		Y += 18;
	}
	if (!Gm.Player.Quests.Num())
	{
		if (Gm.PlayerUnit()->Level <= 2 && Gm.Player.Done.Num() == 0 && bTourHidden) { Tx(X, Y, TEXT("Speak to Sid Grauman"), 10, ColGold); Tx(X, Y + 16, TEXT("the gold ! on the forecourt"), 10, ColDim); }
		return;
	}
	for (const FHLQuestState& QS : Gm.Player.Quests)
	{
		if (Y > VS.Y - 320) break;
		const float Y0 = Y;
		Tx(X, Y, QS.Q->Name, 10, QS.bComplete ? ColGreen : ColGold, true);
		{
			FVector2D Goal;
			const FHLUnit* PU = Gm.PlayerUnit();
			if (PU && HLQuestGoal(Gm, QS, *PU, Goal) && FVector2D::Distance(Goal, PU->Pos) >= 6)
			{
				const FString Hint = FString::Printf(TEXT("%s %d m"), HLCompass(Goal - PU->Pos), (int32)FMath::RoundToInt(FVector2D::Distance(Goal, PU->Pos)));
				Tx(VS.X - 16 - TW(Hint, 9), Y + 1, Hint, 9, ColDim);
			}
		}
		Y += 15;
		if (QS.bComplete) { const FHLNpcDef* N = FHLData::Get().Npc(QS.Q->Turnin); Tx(X + 8, Y, FString::Printf(TEXT("Return to %s"), N ? *N->Name : TEXT("the quest giver")), 9, ColWhite); Y += 14; }
		else for (int32 i = 0; i < QS.Q->Objectives.Num(); ++i)
		{
			const FHLObjective& O = QS.Q->Objectives[i];
			Tx(X + 8, Y, O.Count > 1 ? FString::Printf(TEXT("%s: %d/%d"), *O.Label, QS.Progress[i], O.Count) : O.Label, 9, QS.Progress[i] >= O.Count ? ColDim : ColWhite); Y += 14;
			if (O.Type == TEXT("escort") && Gm.bEscorting) if (const FHLUnit* W = Gm.Unit(Gm.EscortUnitId)) { Bar(X + 8, Y + 1, 150, 9, W->HpPct(), W->HpPct() < 0.35f ? ColRed : ColGreen, FString::Printf(TEXT("%s %d%%"), *W->Name, (int32)(W->HpPct() * 100)), 7); Y += 14; }
		}
				Y += 4;
				// click an entry: the quest log opens on it
				const int32 Qi = &QS - Gm.Player.Quests.GetData();
				if (Hover(X - 6, Y0, VS.X - X - 6, Y - Y0))
				{
					RRc(X - 6, Y0 - 2, VS.X - X - 4, Y - Y0, FLinearColor(1, 0.85f, 0.4f, 0.08f), 4);
					if (bLClick) { bLClick = false; SelQuest = Qi; bQuests = true; }
				}
			}
		}

		void SHLGameUI::ChatColor(const FHLChatLine& L, FLinearColor& C, FString& Text) const
{
	const FString& Ch = L.Ch;
	if (Ch == TEXT("say")) { C = ColWhite; Text = FString::Printf(TEXT("[%s] says: %s"), *L.From, *L.Text); }
	else if (Ch == TEXT("yell")) { C = FLinearColor(1, 0.25f, 0.25f); Text = FString::Printf(TEXT("[%s] yells: %s"), *L.From, *L.Text); }
	else if (Ch == TEXT("zone")) { C = FLinearColor(1, 0.75f, 0.75f); Text = FString::Printf(TEXT("[1. General] [%s]: %s"), *L.From, *L.Text); }
	else if (Ch == TEXT("trade")) { C = FLinearColor(1, 0.75f, 0.75f); Text = FString::Printf(TEXT("[2. Trade] [%s]: %s"), *L.From, *L.Text); }
	else if (Ch == TEXT("lfg")) { C = FLinearColor(1, 0.75f, 0.75f); Text = FString::Printf(TEXT("[4. LookingForGroup] [%s]: %s"), *L.From, *L.Text); }
	else if (Ch == TEXT("guild")) { C = FLinearColor(0.25f, 1, 0.25f); Text = L.From.IsEmpty() ? L.Text : FString::Printf(TEXT("[Guild] [%s]: %s"), *L.From, *L.Text); }
	else if (Ch == TEXT("party")) { C = FLinearColor(0.65f, 0.65f, 1); Text = FString::Printf(TEXT("[Party] [%s]: %s"), *L.From, *L.Text); }
	else if (Ch == TEXT("whisper")) { C = FLinearColor(1, 0.5f, 1); Text = FString::Printf(TEXT("[%s] whispers: %s"), *L.From, *L.Text); }
	else if (Ch == TEXT("whisper_to")) { C = FLinearColor(1, 0.5f, 1); Text = FString::Printf(TEXT("To [%s]: %s"), *L.From, *L.Text); }
	else if (Ch == TEXT("system")) { C = FLinearColor(1, 1, 0.2f); Text = L.Text; }
	else if (Ch == TEXT("loot")) { C = FLinearColor(0.1f, 0.75f, 0.1f); Text = L.Text; }
	else if (Ch == TEXT("quest")) { C = FLinearColor(1, 0.7f, 0.2f); Text = L.Text; }
	else if (Ch == TEXT("emote")) { C = FLinearColor(1, 0.55f, 0.25f); Text = L.Text; }
	else if (Ch == TEXT("npc")) { C = FLinearColor(1, 1, 0.65f); Text = FString::Printf(TEXT("%s says: %s"), *L.From, *L.Text); }
	else { C = FLinearColor(0.75f, 0.75f, 0.75f); Text = L.Text; }
}

void SHLGameUI::DrawChat() const
{
	FHLGame& Gm = G();
	const float X = 16, W = 430, H = 210, Y = VS.Y - H - 44;
	Block(X, Y - 20, W, H + 48);
	Rc(X, Y, W, H, FLinearColor(0, 0, 0, 0.42f));
	if (Btn(X, Y - 20, 70, 18, TEXT("Chat"), true, 9)) bCombatLog = false;
	if (Btn(X + 74, Y - 20, 90, 18, TEXT("Combat Log"), true, 9)) bCombatLog = true;
	if (Hover(X, Y, W, H) && Wheel != 0) ChatScroll = FMath::Clamp(ChatScroll + (int32)(Wheel * 3), 0, 300);
	{
		int32 Lines = 0;
		for (const FHLChatLine& L : Gm.Chat) if ((L.Ch == TEXT("combat")) == bCombatLog) ++Lines;
		const int32 Vis = (int32)(H / 15);
		ChatScroll = FMath::Min(ChatScroll, FMath::Max(0, Lines - Vis));
		ScrollBar(X + W - 7, Y + 2, H - 4, Lines - Vis - ChatScroll, Vis, Lines);
	}
	const float LH = 15;
	float Cy = Y + H - LH - 2;
	int32 Skip = ChatScroll;
	for (int32 i = Gm.Chat.Num() - 1; i >= 0 && Cy >= Y; --i)
	{
		const FHLChatLine& L = Gm.Chat[i];
		if ((L.Ch == TEXT("combat")) != bCombatLog) continue;
		FLinearColor C; FString Text;
		ChatColor(L, C, Text);
		TArray<FString> Lines;
		Wrap(Text, 10, W - 12, Lines);
		for (int32 k = Lines.Num() - 1; k >= 0 && Cy >= Y; --k)
		{
			if (Skip > 0) { --Skip; continue; }
			// item links ([Name], from anyone) in quality colours; hovering one shows its card
			const FString& Ln = Lines[k];
			float Lx = X + 6;
			int32 From = 0;
			while (From < Ln.Len())
			{
				const int32 Open = Ln.Find(TEXT("["), ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
				const int32 Close = Open == INDEX_NONE ? INDEX_NONE : Ln.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open);
				if (Close == INDEX_NONE) { Tx(Lx, Cy, Ln.Mid(From), 10, C); break; }
				const FString Pre = Ln.Mid(From, Open - From), Link = Ln.Mid(Open, Close - Open + 1);
				const FHLItem* It = FHLData::Get().ItemByName(Link.Mid(1, Link.Len() - 2));
				Tx(Lx, Cy, Pre, 10, C); Lx += TW(Pre, 10);
				const float Lw = TW(Link, 10);
				const bool bHot = It && Hover(Lx, Cy, Lw, LH);
				Tx(Lx, Cy, Link, 10, It ? (bHot ? HLQualityColor(It->Q) * 1.3f : HLQualityColor(It->Q)) : C);
				if (bHot) { TipItem = It; TipPos = Mouse; Rc(Lx, Cy + LH - 3, Lw, 1, HLQualityColor(It->Q)); }
				Lx += Lw;
				From = Close + 1;
			}
			if (k == 0 && L.SimIdx >= 0 && Hover(X, Cy, W, LH) && bLClick && !L.From.IsEmpty()) { bLClick = false; bTyping = true; ChatEntry = FString::Printf(TEXT("/w %s "), *L.From); }
			Cy -= LH;
		}
	}
	// entry line
	const float Ey = Y + H + 2;
	Rc(X, Ey, W, 22, FLinearColor(0, 0, 0, bTyping ? 0.85f : 0.4f));
	if (bTyping)
	{
		const bool bBlink = FMath::Fmod(FPlatformTime::Seconds(), 1.0) < 0.5;
		const FString Prefix = Gm.ChatChannel == TEXT("whisper") ? FString::Printf(TEXT("To %s: "), *Gm.WhisperTarget) : Gm.ChatChannel == TEXT("zone") ? TEXT("General: ") : Gm.ChatChannel == TEXT("party") ? TEXT("Party: ") : Gm.ChatChannel == TEXT("guild") ? TEXT("Guild: ") : Gm.ChatChannel == TEXT("say") ? TEXT("Say: ") : Gm.ChatChannel + TEXT(": ");
		FString Show = ChatEntry;
		while (TW(Prefix + Show, 10) > W - 20 && Show.Len() > 1) Show.RightChopInline(1);
		Tx(X + 6, Ey + 3, Prefix + Show + (bBlink ? TEXT("_") : TEXT("")), 10, ColWhite);
	}
	else
	{
			Tx(X + 6, Ey + 3, TEXT("Enter to chat"), 10, ColDim);
			if (Hover(X, Ey, W, 22) && bLClick) { bLClick = false; bTyping = true; }
		}
		const FString Typer = Gm.WhoIsTyping();
		if (!Typer.IsEmpty())
		{
			const int32 Dots = (int32)FMath::Fmod(FPlatformTime::Seconds() * 2.5, 4.0);
			const FString Msg = Typer + TEXT(" is typing") + FString::ChrN(Dots, '.');
			Tx(X + W - TW(Typer + TEXT(" is typing..."), 9) - 8, Ey - 14, Msg, 9, FLinearColor(1.f, 0.55f, 1.f, 0.85f));
		}
}

void SHLGameUI::DrawPopups() const
{
	FHLGame& Gm = G();
	FHLUnit* P = Gm.PlayerUnit();
	if (P->bDead)
	{
		const float W = 320, H = 110, X = (VS.X - W) * 0.5f, Y = 300;
		Block(X, Y, W, H); Panel(X, Y, W, H);
		TxC(X + W * 0.5f, Y + 14, TEXT("You have died."), 16, ColRed, true);
		Rc(X + 57, Y + 53, 206, 40, FLinearColor(0.95f, 0.78f, 0.38f, 0.55f + 0.25f * FMath::Sin(FPlatformTime::Seconds() * 3.0)));
		++Layer; LastKind = 1;
		Rc(X + 58, Y + 54, 204, 38, FLinearColor(0.04f, 0.035f, 0.03f));
		if (Btn(X + 60, Y + 56, 200, 34, TEXT("Release to aid station"), true, 12)) Gm.ReleaseSpirit();
		}
		else if (P->bGhost)
		{
			const bool bNear = P->Area == Gm.Player.CorpseArea && FVector2D::Distance(P->Pos, Gm.Player.CorpsePos) <= 10;
			const float W = 380, H = bNear ? 96 : 118, X = (VS.X - W) * 0.5f, Y = 96;
			Block(X, Y, W, H); Panel(X, Y, W, H);
			if (bNear)
			{
				TxC(X + W * 0.5f, Y + 12, TEXT("You stand over your body."), 13, FLinearColor(0.7f, 0.85f, 1), true);
				if (Btn(X + 90, Y + 46, 200, 32, TEXT("Resurrect"), true, 12)) Gm.ResurrectAtCorpse();
			}
			else
			{
				TxC(X + W * 0.5f, Y + 10, FString::Printf(TEXT("Your body lies %d m away."), (int32)FVector2D::Distance(P->Pos, Gm.Player.CorpsePos)), 13, FLinearColor(0.7f, 0.85f, 1), true);
				TxC(X + W * 0.5f, Y + 32, TEXT("Follow the arrow back, or revive here (-25% durability, 10 min sickness)."), 9, ColDim);
				if (Btn(X + 90, Y + 62, 200, 32, TEXT("Revive at the aid station"), true, 11)) Gm.ResurrectAtStation();
			}
		}
	if (Gm.InviteFromSim >= 0 && Gm.Sims.IsValidIndex(Gm.InviteFromSim))
	{
		const float W = 340, H = 100, X = (VS.X - W) * 0.5f, Y = 120;
		Block(X, Y, W, H); Panel(X, Y, W, H);
		const FHLSim& S = Gm.Sims[Gm.InviteFromSim];
		TxC(X + W * 0.5f, Y + 12, FString::Printf(TEXT("%s (level %d %s) invites you to a group."), *S.P->Handle, S.Level, S.P->Cls ? *S.P->Cls->Name : TEXT("")), 11, ColWhite);
		if (Btn(X + 40, Y + 52, 110, 30, TEXT("Accept"), true, 12)) Gm.AnswerInvite(true);
		if (Btn(X + 190, Y + 52, 110, 30, TEXT("Decline"), true, 12)) Gm.AnswerInvite(false);
	}
	if (Gm.GuildFromSim >= 0 && Gm.Sims.IsValidIndex(Gm.GuildFromSim) && Gm.Now >= Gm.GuildInviteAt)
	{
		const float W = 360, H = 112, X = (VS.X - W) * 0.5f, Y = 340;
		Block(X, Y, W, H); Panel(X, Y, W, H);
		const FHLSim& S = Gm.Sims[Gm.GuildFromSim];
		const FHLGuild* Gd = FHLData::Get().Guild(S.P->Guild);
		TxC(X + W * 0.5f, Y + 10, FString::Printf(TEXT("%s invites you to join the guild"), *S.P->Handle), 11, ColWhite);
		TxC(X + W * 0.5f, Y + 28, Gd ? FString::Printf(TEXT("<%s>"), *Gd->Name) : FString(), 13, FLinearColor(0.35f, 1, 0.35f));
		if (Gd) TxC(X + W * 0.5f, Y + 48, Gd->Blurb, 9, ColDim);
		if (Btn(X + 50, Y + 70, 110, 28, TEXT("Accept"), true, 12)) Gm.AnswerGuildInvite(true);
		if (Btn(X + 200, Y + 70, 110, 28, TEXT("Decline"), true, 12)) Gm.AnswerGuildInvite(false);
	}
	if (Gm.DuelFromSim >= 0 && Gm.Sims.IsValidIndex(Gm.DuelFromSim))
	{
		const float W = 340, H = 100, X = (VS.X - W) * 0.5f, Y = 230;
		Block(X, Y, W, H); Panel(X, Y, W, H);
		const FHLSim& S = Gm.Sims[Gm.DuelFromSim];
		TxC(X + W * 0.5f, Y + 12, FString::Printf(TEXT("%s (level %d %s) challenges you to a duel."), *S.P->Handle, S.Level, S.P->Cls ? *S.P->Cls->Name : TEXT("")), 11, ColWhite);
		if (Btn(X + 40, Y + 52, 110, 30, TEXT("Accept"), true, 12)) Gm.AnswerDuel(true);
		if (Btn(X + 190, Y + 52, 110, 30, TEXT("Decline"), true, 12)) Gm.AnswerDuel(false);
	}
	if (Gm.TradeFromSim >= 0 && Gm.bTradeAskShown && Gm.Sims.IsValidIndex(Gm.TradeFromSim) && Gm.Player.Bag.IsValidIndex(Gm.TradeWantBag) && Gm.Player.Bag[Gm.TradeWantBag].Item)
	{
		const float W = 380, H = 112, X = (VS.X - W) * 0.5f, Y = 230;
		Block(X, Y, W, H); Panel(X, Y, W, H);
		const FHLSim& S = Gm.Sims[Gm.TradeFromSim];
		const FHLItem* It = Gm.Player.Bag[Gm.TradeWantBag].Item;
		TxC(X + W * 0.5f, Y + 10, FString::Printf(TEXT("%s (level %d %s) wants to trade."), *S.P->Handle, S.Level, S.P->Cls ? *S.P->Cls->Name : TEXT("")), 11, ColWhite);
		ItemIcon(X + 20, Y + 32, 28, It);
		Tx(X + 54, Y + 32, It->Name, 10, HLQualityColor(It->Q));
		Tx(X + 54, Y + 46, FString::Printf(TEXT("They offer %s for it."), *HLMoney(Gm.TradeWantOffer)), 10, ColGold);
		if (Btn(X + 50, Y + 72, 120, 28, TEXT("Open trade"), true, 12)) Gm.AnswerTradeRequest(true);
		if (Btn(X + 210, Y + 72, 120, 28, TEXT("Decline"), true, 12)) Gm.AnswerTradeRequest(false);
	}
	if (Gm.TradeSim >= 0) DrawTrade();
	if (Gm.Roll.bActive && Gm.Roll.Item)
	{
		const float W = 300, H = 96, X = (VS.X - W) * 0.5f, Y = VS.Y - 330;
		Block(X, Y, W, H); Panel(X, Y, W, H);
		ItemIcon(X + 10, Y + 10, 40, Gm.Roll.Item);
		Tx(X + 58, Y + 10, Gm.Roll.Item->Name, 11, HLQualityColor(Gm.Roll.Item->Q), true);
		Bar(X + 58, Y + 32, W - 70, 6, (Gm.Roll.EndsAt - Gm.Now) / 12.f, ColGold);
		if (Gm.Roll.PlayerChoice < 0)
		{
			if (Btn(X + 10, Y + 58, 86, 26, TEXT("Need"))) { Gm.Roll.PlayerChoice = 0; Gm.Roll.EndsAt = FMath::Min(Gm.Roll.EndsAt, (float)Gm.Now + 1.5f); }
			if (Btn(X + 106, Y + 58, 86, 26, TEXT("Greed"))) { Gm.Roll.PlayerChoice = 1; Gm.Roll.EndsAt = FMath::Min(Gm.Roll.EndsAt, (float)Gm.Now + 1.5f); }
			if (Btn(X + 202, Y + 58, 86, 26, TEXT("Pass"))) { Gm.Roll.PlayerChoice = 2; Gm.Roll.EndsAt = FMath::Min(Gm.Roll.EndsAt, (float)Gm.Now + 1.5f); }
		}
		else TxC(X + W * 0.5f, Y + 62, TEXT("Rolling..."), 11, ColDim);
	}
	// the controls card steps aside by itself once you have found your feet (level 3); H brings it back
	{
		static bool bAutoHid = false;
		if (!bAutoHid && G().PlayerUnit() && G().PlayerUnit()->Level >= 3) { bAutoHid = true; bHelp = false; }
	}
	if (bHelp)
	{
		auto K = [](EHLAct A) { return FHLSettings::Get().KeyName(A); };
		const TPair<FString, FString> Rows[] = {
			{ K(EHLAct::Forward) + TEXT(" ") + K(EHLAct::TurnLeft) + TEXT(" ") + K(EHLAct::Back) + TEXT(" ") + K(EHLAct::TurnRight), TEXT("Move") },
			{ K(EHLAct::Jump), TEXT("Jump") },
			{ K(EHLAct::Target) + TEXT(" / left-click"), TEXT("Pick a target") },
			{ TEXT("Right-click / ") + K(EHLAct::Interact), TEXT("Attack, talk, loot, use") },
			{ K(EHLAct::Bar1) + TEXT(" - ") + K(EHLAct::Bar12), TEXT("Abilities on the action bar") },
			{ K(EHLAct::Sit), TEXT("Sit") },
			{ K(EHLAct::Bags) + TEXT("  ") + K(EHLAct::Character) + TEXT("  ") + K(EHLAct::Quests), TEXT("Bags, character, quest log") },
			{ K(EHLAct::Spellbook) + TEXT("  ") + K(EHLAct::Talents) + TEXT("  ") + K(EHLAct::Map), TEXT("Abilities, talents, map") },
			{ K(EHLAct::Social) + TEXT("  ") + K(EHLAct::Workbench), TEXT("Who's online, workbench") },
			{ TEXT("Enter"), TEXT("Chat, click a name to whisper") },
			{ TEXT("Gold !  /  gold ?"), TEXT("A quest to take / to hand in") },
		};
		const float W = 360, H = 40 + UE_ARRAY_COUNT(Rows) * 17 + 8, X = VS.X - W - 16, Y = VS.Y - H - 84;
		Block(X, Y, W, H); Panel(X, Y, W, H);
		Tx(X + 10, Y + 8, TEXT("How to play"), 13, ColGold, true);
		float Ry = Y + 34;
		for (const auto& R : Rows) { Tx(X + 12, Ry, R.Key, 10, ColGold); Tx(X + 140, Ry, R.Value, 10, ColWhite); Ry += 17; }
		if (Btn(X + W - 80, Y + 6, 70, 20, TEXT("Hide (") + K(EHLAct::Help) + TEXT(")"), true, 9)) bHelp = false;
	}
}

void SHLGameUI::DrawTooltip() const
{
	if (!TipItem && TipText.IsEmpty()) return;
	TArray<TPair<FString, FLinearColor>> Lines;
	if (TipItem)
	{
		const FHLItem& It = *TipItem;
		const FHLGame& Gm = G();
		if (It.IsEquip())
		{
			FString Slot = It.Slot; Slot[0] = FChar::ToUpper(Slot[0]);
			FString Type = It.Type; Type[0] = FChar::ToUpper(Type[0]);
			Lines.Add({ FString::Printf(TEXT("%s    %s%s"), *Slot, *Type, It.Hands == 2 ? TEXT(" (two-hand)") : TEXT("")), ColWhite });
			if (It.Speed > 0) Lines.Add({ FString::Printf(TEXT("%d - %d damage    speed %.1f    (%.1f dps)"), It.DMin, It.DMax, It.Speed, (It.DMin + It.DMax) * 0.5f / It.Speed), ColWhite });
			if (It.Armor > 0) Lines.Add({ FString::Printf(TEXT("%d Armour"), It.Armor), ColWhite });
			for (int32 i = 0; i < HL_NUM_STATS; ++i) if (It.Stats[i]) Lines.Add({ FString::Printf(TEXT("+%d %s"), It.Stats[i], HLStatNames[i]), ColGreen });
			if (!It.EquipText.IsEmpty()) Lines.Add({ TEXT("Equip: ") + It.EquipText, ColGreen });
			FString Why;
			const bool bCan = Gm.CanEquip(It, &Why);
			Lines.Add({ FString::Printf(TEXT("Requires level %d"), It.Req), Gm.PlayerUnit() && Gm.PlayerUnit()->Level >= It.Req ? ColWhite : ColRed });
			if (!bCan && !Why.StartsWith(TEXT("Your level"))) Lines.Add({ Why, ColRed });
			Lines.Add({ FString::Printf(TEXT("Item level %d"), It.ILvl), ColDim });
		}
		else if (It.Kind == TEXT("consumable"))
		{
			if (It.UseType == TEXT("food")) Lines.Add({ FString::Printf(TEXT("Use: restores %d health over 18 s. Must remain seated."), (int32)It.UseAmount), ColGreen });
			else if (It.UseType == TEXT("drink")) Lines.Add({ FString::Printf(TEXT("Use: restores %d Power over 18 s. Must remain seated."), (int32)It.UseAmount), ColGreen });
			else if (It.UseType == TEXT("bandage")) Lines.Add({ FString::Printf(TEXT("Use: heals %d over 6 s."), (int32)It.UseAmount), ColGreen });
			else if (It.UseType == TEXT("grenade")) Lines.Add({ FString::Printf(TEXT("Use: about %d fire damage to your target and everything within 6 m. Range 30 m."), (int32)It.UseAmount), ColGreen });
			if (It.Type == TEXT("pocket")) Lines.Add({ TEXT("Pocket item: usable in combat. 1 min cooldown shared by pocket items."), ColDim });
			if (It.Req > 1) Lines.Add({ FString::Printf(TEXT("Requires level %d"), It.Req), ColWhite });
		}
		if (!It.Blurb.IsEmpty()) Lines.Add({ It.Blurb, ColGold });
		if (!It.Desc.IsEmpty())
		{
			TArray<FString> Wr;
			Wrap(It.Desc, 10, 280, Wr);
			for (const FString& D : Wr) Lines.Add({ D, FLinearColor(1, 0.86f, 0.55f) });
		}
		if (It.Kind == TEXT("ammo")) Lines.Add({ FString::Printf(TEXT("Ammunition (%s)    +%.1f dps"), *It.Ammo, It.AmmoDps), ColWhite });
		if (It.Price > 0) Lines.Add({ It.Buy > 1 ? FString::Printf(TEXT("Sell price: %s per %d"), *HLMoney(It.Price), It.Buy) : FString::Printf(TEXT("Sell price: %s"), *HLMoney(It.Price)), ColDim });
		if (!TipExtra.IsEmpty())
		{
			TArray<FString> Raw;
			TipExtra.ParseIntoArray(Raw, TEXT("\n"), true);
			for (const FString& R : Raw) { TArray<FString> Wr; Wrap(R, 10, 280, Wr); for (const FString& S : Wr) Lines.Add({ S, FLinearColor(0.6f, 0.85f, 1) }); }
		}
		// the card: icon and name in a header, quality-coloured frame, the details below a rule
		static const TCHAR* QNames[] = { TEXT("Poor"), TEXT("Common"), TEXT("Uncommon"), TEXT("Rare"), TEXT("Epic"), TEXT("Legendary") };
		const FLinearColor QC = HLQualityColor(It.Q);
		FString What = It.Kind == TEXT("consumable") ? (It.UseType.IsEmpty() ? FString(TEXT("Consumable")) : It.UseType) : It.IsEquip() ? (It.Type.IsEmpty() ? It.Kind : It.Type) : It.Kind == TEXT("ammo") ? FString(TEXT("Ammunition")) : It.Kind;
		if (What.Len()) What[0] = FChar::ToUpper(What[0]);
		const FString Sub = FString::Printf(TEXT("%s %s%s"), QNames[FMath::Clamp(It.Q, 0, 5)], *What, It.bNamed ? TEXT("  -  named") : TEXT(""));
		float W = FMath::Max3(270.f, TW(It.Name, 12, true) + 70, TW(Sub, 9) + 70);
		for (const auto& L : Lines) W = FMath::Max(W, TW(L.Key, 10) + 20);
		const float Head = 52, H = Head + Lines.Num() * 16 + 12;
		float X = TipPos.X + 18, Y = TipPos.Y - H - 8;
		if (X + W > VS.X - 4) X = TipPos.X - W - 14;
		if (Y < 4) Y = FMath::Min(TipPos.Y + 22, VS.Y - H - 4);
		Layer += 3; LastKind = 0;
		Panel(X, Y, W, H, FLinearColor(0.025f, 0.022f, 0.035f, 0.96f), QC * 0.8f);
		++Layer; LastKind = 1;
		Rc(X + 1, Y + 1, W - 2, Head - 4, FLinearColor(QC.R * 0.18f, QC.G * 0.18f, QC.B * 0.18f, 0.9f)); // header wash in the quality colour
		++Layer; LastKind = 1;
		Rc(X + 8, Y + 7, 38, 38, QC); Rc(X + 9, Y + 8, 36, 36, FLinearColor(0, 0, 0, 1));
		if (const FSlateBrush* IB = HLIconBrush(It.Icon)) Img(X + 10, Y + 9, 34, 34, IB);
		Tx(X + 54, Y + 8, It.Name, 12, QC, true);
		Tx(X + 54, Y + 28, Sub, 9, FLinearColor(0.85f, 0.8f, 0.7f));
		Rc(X + 8, Y + Head - 2, W - 16, 1, QC * 0.6f);
		for (int32 i = 0; i < Lines.Num(); ++i) Tx(X + 10, Y + Head + 4 + i * 16, Lines[i].Key, 10, Lines[i].Value);
		return;
	}
	else
	{
		TArray<FString> Raw;
		TipText.ParseIntoArray(Raw, TEXT("\n"), true);
		for (int32 i = 0; i < Raw.Num(); ++i)
		{
			TArray<FString> Wr;
			Wrap(Raw[i], 10, 280, Wr);
			for (const FString& S : Wr) Lines.Add({ S, i == 0 ? ColGold : i == Raw.Num() - 1 ? FLinearColor(1, 0.9f, 0.6f) : ColWhite });
		}
	}
	float W = 120;
	for (const auto& L : Lines) W = FMath::Max(W, TW(L.Key, 10) + 16);
	const float H = Lines.Num() * 16 + 10;
	float X = TipPos.X + 16, Y = TipPos.Y - H - 8;
	if (X + W > VS.X - 4) X = TipPos.X - W - 12;
	if (Y < 4) Y = 4;
	Layer += 3; LastKind = 0;
	Panel(X, Y, W, H, FLinearColor(0.02f, 0.02f, 0.05f));
	for (int32 i = 0; i < Lines.Num(); ++i) Tx(X + 9, Y + 5 + i * 16, Lines[i].Key, 10, Lines[i].Value, i == 0);
}
