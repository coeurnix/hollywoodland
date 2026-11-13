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
#include "Fonts/CompositeFont.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static const FLinearColor ColGold(1.f, 0.82f, 0.25f), ColDim(0.62f, 0.62f, 0.62f), ColWhite(1, 1, 1), ColRed(1, 0.25f, 0.2f), ColGreen(0.3f, 1, 0.3f);

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
	TSharedPtr<FSlateBrush> B;
	if (T) { B = MakeShared<FSlateBrush>(); B->SetResourceObject(T); B->ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY()); B->DrawAs = ESlateBrushDrawType::Image; }
	Art.Add(Name, B);
	return B.Get();
}
void SHLGameUI::Img(float X, float Y, float W, float H, const FSlateBrush* B, const FLinearColor& Tint) const
{
	if (!B) return;
	++Layer; LastKind = 1;
	FSlateDrawElement::MakeBox(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(Sc, FVector2f(X * Sc, Y * Sc))), B, ESlateDrawEffect::None, Tint);
}

// ------------------------------------------------------------------------------------------------------------- toolkit
void SHLGameUI::Rc(float X, float Y, float W, float H, const FLinearColor& C) const
{
	if (LastKind != 1) { ++Layer; LastKind = 1; }
	FSlateDrawElement::MakeBox(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(Sc, FVector2f(X * Sc, Y * Sc))), FCoreStyle::Get().GetBrush("WhiteBrush"), ESlateDrawEffect::None, C);
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
	if (bShadow) FSlateDrawElement::MakeText(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(2000, 40), FSlateLayoutTransform(Sc, FVector2f((X + 1) * Sc, (Y + 1) * Sc))), S, Font, ESlateDrawEffect::None, FLinearColor(0, 0, 0, C.A * 0.9f));
	FSlateDrawElement::MakeText(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(2000, 40), FSlateLayoutTransform(Sc, FVector2f(X * Sc, Y * Sc))), S, Font, ESlateDrawEffect::None, C);
}
float SHLGameUI::TW(const FString& S, int32 Size, bool bBold) const
{
	const TSharedRef<FSlateFontMeasure> FM = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	return FM->Measure(S, HLFont(Size, bBold)).X;
}
void SHLGameUI::TxC(float CX, float Y, const FString& S, int32 Size, const FLinearColor& C, bool bBold) const { Tx(CX - TW(S, Size, bBold) * 0.5f, Y, S, Size, C, bBold); }
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
	FSlateDrawElement::MakeLines(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(VS.X, VS.Y), FSlateLayoutTransform(Sc, FVector2f(0, 0))), Pts, ESlateDrawEffect::None, C, true, Thick);
}
bool SHLGameUI::Btn(float X, float Y, float W, float H, const FString& Label, bool bEnabled, int32 Size) const
{
	const bool bHot = bEnabled && Hover(X, Y, W, H);
	Rc(X - 1, Y - 1, W + 2, H + 2, bHot ? FLinearColor(0.95f, 0.78f, 0.38f) : FLinearColor(0.55f, 0.45f, 0.25f));
	++Layer; LastKind = 1;
	Rc(X, Y, W, H, !bEnabled ? FLinearColor(0.1f, 0.1f, 0.1f) : bHot ? (bLDown ? FLinearColor(0.42f, 0.09f, 0.08f) : FLinearColor(0.34f, 0.1f, 0.09f)) : FLinearColor(0.2f, 0.06f, 0.06f));
	if (H >= 18) { ++Layer; LastKind = 1; Rc(X, Y, W, 1, FLinearColor(1, 0.85f, 0.5f, bEnabled ? 0.25f : 0.08f)); }
	TxC(X + W * 0.5f, Y + (H - Size * 1.45f) * 0.5f, Label, Size, bEnabled ? ColGold : ColDim);
	if (bHot && bLClick) { bLClick = false; G().Sfx.Ui(TEXT("ui_click")); return true; }
	return false;
}
void SHLGameUI::Bar(float X, float Y, float W, float H, float Pct, const FLinearColor& C, const FString& Label, int32 Size) const
{
	Rc(X - 1, Y - 1, W + 2, H + 2, FLinearColor(0, 0, 0, 1));
	++Layer; LastKind = 1;
	Rc(X, Y, W, H, FLinearColor(0.08f, 0.08f, 0.08f));
	++Layer; LastKind = 1;
	Rc(X, Y, W * FMath::Clamp(Pct, 0.f, 1.f), H, C);
	if (!Label.IsEmpty()) TxC(X + W * 0.5f, Y + (H - Size * 1.5f) * 0.5f, Label, Size, ColWhite);
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
	Panel(P.X, P.Y, W, H);
	if (const FSlateBrush* Bg = HLArtBrush(TEXT("ui_panel"))) Img(P.X, P.Y, W, H, Bg, FLinearColor(0.5f, 0.5f, 0.5f, 0.92f));
	++Layer; LastKind = 1;
	Rc(P.X, P.Y, W, 26, FLinearColor(0.16f, 0.05f, 0.05f));

	++Layer; LastKind = 1;
	Rc(P.X, P.Y + 26, W, 1, FLinearColor(0.75f, 0.6f, 0.3f)); Rc(P.X, P.Y + 28, W, 1, FLinearColor(0.75f, 0.6f, 0.3f, 0.4f));
	TxC(P.X + W * 0.5f, P.Y + 3, Title, 14, ColGold, true);
	if (Btn(P.X + W - 22, P.Y + 3, 18, 18, TEXT("x"))) { bOpen = false; G().Sfx.Ui(TEXT("ui_close")); return false; }
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
		FSlateDrawElement::MakeBox(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(S - 4, S - 4), FSlateLayoutTransform(Sc, FVector2f((X + 2) * Sc, (Y + 2) * Sc))), IB, ESlateDrawEffect::None, FLinearColor::White);
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
	Down.Add(K);
	static const FKey BarKeys[12] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero, EKeys::Hyphen, EKeys::Equals };
	for (int32 i = 0; i < 12; ++i) if (K == BarKeys[i]) { Gm.UseBar(i); return FReply::Handled(); }
	if (bSearchFocus)
	{
		if (K == EKeys::BackSpace && AuctionSearch.Len()) AuctionSearch.LeftChopInline(1);
		else if (K == EKeys::Enter || K == EKeys::Escape) bSearchFocus = false;
		return FReply::Handled();
	}
	if (K == EKeys::Enter) { bTyping = true; Down.Reset(); }
	else if (K == EKeys::Slash) { bTyping = true; ChatEntry.Reset(); Down.Reset(); }
	else if (K == EKeys::R && !Gm.LastWhisperFrom.IsEmpty()) { bTyping = true; bSwallowChar = true; ChatEntry = FString::Printf(TEXT("/w %s "), *Gm.LastWhisperFrom); Down.Reset(); }
	else if (K == EKeys::Tab) Gm.TabTarget();
	else if (K == EKeys::SpaceBar) Gm.Input.bJump = true;
	else if (K == EKeys::F) Interact();
	else if (K == EKeys::X) { if (FHLUnit* U = Gm.PlayerUnit()) U->bSitting = !U->bSitting; }
	else if (K == EKeys::T) { if (FHLUnit* U = Gm.PlayerUnit()) { const FHLUnit* T = Gm.Unit(U->TargetId); if (T && T->Kind == EHLKind::Mob && !T->bDead) U->bAutoAttack = !U->bAutoAttack; } }
	else if (K == EKeys::C) bChar = !bChar;
	else if (K == EKeys::B) bBags = !bBags;
	else if (K == EKeys::L) bQuests = !bQuests;
	else if (K == EKeys::P) bSpells = !bSpells;
	else if (K == EKeys::M) bMap = !bMap;
	else if (K == EKeys::O) { bWho = !bWho; Gm.bWhoOpen = bWho; }
	else if (K == EKeys::N) bTalents = !bTalents;
	else if (K == EKeys::K) bCraft = !bCraft;
	else if (K == EKeys::H) bHelp = !bHelp;
	else if (K == EKeys::Escape)
	{
		if (Gm.OpenNpcUnitId || !Gm.OpenNpcMode.IsEmpty()) { Gm.OpenNpcUnitId = 0; Gm.OpenNpcMode.Reset(); }
		else if (bChar || bBags || bQuests || bSpells || bMap || bWho || bInspect || bTalents || bHelp || bCraft) { bChar = bBags = bQuests = bSpells = bMap = bWho = bInspect = bTalents = bHelp = bCraft = false; Gm.bWhoOpen = false; }
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
	auto K = [&](const FKey& Key) { return Down.Contains(Key) ? 1.f : 0.f; };
	const bool bSteer = bRDown && bDragging;
	In.Forward = FMath::Clamp(K(EKeys::W) + K(EKeys::Up) + ((bLDown && bRDown && bDragging) ? 1.f : 0.f), 0.f, 1.f) - FMath::Max(K(EKeys::S), K(EKeys::Down));
	In.Strafe = K(EKeys::E) - K(EKeys::Q);
	const float AD = FMath::Max(K(EKeys::D), K(EKeys::Right)) - FMath::Max(K(EKeys::A), K(EKeys::Left));
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
		if (D < Best && D < 4.5f) { Best = D; BestObj = o; BestUnit = 0; }
	}
	if (BestObj >= 0) Gm.ObjectInteract(P->Area, BestObj);
	else if (BestUnit) Gm.InteractUnit(BestUnit);
}

// ------------------------------------------------------------------------------------------------------------- paint
int32 SHLGameUI::OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	DL = &Out; PGeo = Geo; Layer = LayerId; LastKind = 0;
	Sc = ScaleFor(Geo);
	VS = FVector2D(Geo.GetLocalSize()) / Sc;
	PC = World && World->GetWorld() ? World->GetWorld()->GetFirstPlayerController() : nullptr;
	if (GEngine && GEngine->GameViewport) { FVector2D V; GEngine->GameViewport->GetViewportSize(V); if (V.X > 0 && V.Y > 0) ViewPx = V; }
	UiRectsNext.Reset(); Picks.Reset(); TipItem = nullptr; TipText.Reset();
	FHLGame& Gm = G();
	{ static bool bOnce = false; if (!bOnce && Gm.bInWorld) { bOnce = true; if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenBags"))) { bBags = true; bChar = true; } if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenTalents"))) bTalents = true; if (FParse::Param(FCommandLine::Get(), TEXT("HLCraftTest"))) bCraft = true; if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenSpells"))) bSpells = true; if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenFriends"))) { bWho = true; WhoTab = 1; } if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenNews"))) { bWho = true; WhoTab = 3; }
		if (FParse::Param(FCommandLine::Get(), TEXT("HLOpenAuction"))) for (const FHLUnit* N : Gm.UnitList) if (N->Npc && N->Npc->HasRole(TEXT("auctioneer"))) { Gm.OpenNpcUnitId = N->Id; Gm.OpenNpcMode = TEXT("auction"); AuctionSearch = TEXT("boots"); break; } } } // screenshot checks
	if (!Gm.Areas.Num()) { Tx(40, 40, TEXT("Hollywoodland failed to load its data: ") + World->LoadError, 14, ColRed); return Layer; }
	if (!Gm.bInWorld) DrawTitle();
	else
	{
		DrawWorldOverlay();
		DrawHud();
		DrawWindows();
		DrawPopups();
		HandleWorldClick();
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
	Rc(X, Y, W, H, FLinearColor(0.16f, 0.13f, 0.1f));
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
	FSlateDrawElement::MakeBox(*DL, Layer, PGeo.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(Sc, FVector2f(X * Sc, Y * Sc))), PreviewBrush.Get(), ESlateDrawEffect::IgnoreTextureAlpha, FLinearColor::White);
	if (Hover(X, Y, W, H) && !bPreviewDrag) TxC(X + W * 0.5f, Y + H - 18, TEXT("drag to turn"), 9, FLinearColor(1, 0.9f, 0.7f, 0.7f));
}

void SHLGameUI::DrawCreator(float X, float Y, float W) const
{
	FHLChars& C = FHLChars::Get();
	if (!C.Ready()) return;
	if (!bNewLookSet) { NewLook = FHLAppearance::FromSeed(FMath::Rand()); bNewLookSet = true; }
	FHLAppearance& A = NewLook;
	const FHLChars::FSex& Sx = C.Sexes[A.Sex & 1];
	const float Pw = 210, Ph = 315;
	TxC(X + W * 0.5f, Y, TEXT("Appearance"), 13, ColGold, true);
	const TMap<FString, const FHLItem*> NoGear;
	DrawPreview(X + (W - Pw) * 0.5f, Y + 24, Pw, Ph, C.LookFromGear(A, NoGear, false), TEXT("Idle"));
	float Ry = Y + 24 + Ph + 10;
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
	Step(TEXT("Face"), A.Face, Sx.Faces.Num(), FString::Printf(TEXT("%d of %d"), A.Face + 1, Sx.Faces.Num()));
	Step(TEXT("Hair"), A.Hair, Sx.Hair.Num(), Sx.Hair.IsValidIndex(A.Hair) ? Sx.Hair[A.Hair].Name : FString());
	Step(TEXT("Hair colour"), A.HairColor, FHLChars::HairColors().Num(), FHLChars::HairColorNames().IsValidIndex(A.HairColor) ? FHLChars::HairColorNames()[A.HairColor] : FString());
	const int32 Dye = FMath::Clamp(A.Outfit, 0, FMath::Max(0, C.OutfitA.Num() - 1));
	if (C.OutfitA.Num() && C.OutfitB.Num() == C.OutfitA.Num()) Step(TEXT("Clothes"), A.Outfit, C.OutfitA.Num(), FString(), &C.OutfitB[Dye], &C.OutfitA[Dye]); // shirt, then trousers
	if (Btn(Bx, Ry + 2, 212, 24, TEXT("Surprise me"), true, 11)) A = FHLAppearance::FromSeed(FMath::Rand(), A.Sex);
}

void SHLGameUI::DrawTitle() const
{
	FHLGame& Gm = G();
	const FHLData& D = FHLData::Get();
	if (const FSlateBrush* Art = HLArtBrush(TEXT("ui_title")))
	{
		// cover the screen, keep the aspect
		const float Ar = 16.f / 9.f, Cw = FMath::Max(VS.X, VS.Y * Ar), Ch = Cw / Ar;
		Img((VS.X - Cw) * 0.5f, (VS.Y - Ch) * 0.5f, Cw, Ch, Art);
		TxC(VS.X * 0.5f + 3, 24 + 3, TEXT("HOLLYWOODLAND"), 64, FLinearColor(0, 0, 0, 0.8f), true);
		TxC(VS.X * 0.5f, 24, TEXT("HOLLYWOODLAND"), 64, ColGold, true);
		if (const FSlateBrush* Hd = HLArtBrush(TEXT("ui_header"))) { const float Hw = 300, Hh = Hw * Hd->ImageSize.Y / FMath::Max(1.0, Hd->ImageSize.X); Img(VS.X * 0.5f - Hw * 0.5f, 104, Hw, Hh, Hd); }
	}
	const bool bArt = HLArtBrush(TEXT("ui_title")) != nullptr;
	// the new character's looks sit in a column to the right when the character art is in and there is room
	const bool bCreator = FHLChars::Get().Ready() && VS.X >= 1110;
	const float W = 760, H = 640, FullW = W + (bCreator ? 330 : 0), X = (VS.X - FullW) * 0.5f, Y = bArt ? FMath::Max(VS.Y - H - 30, 110.f) : (VS.Y - H) * 0.5f, Top = bArt ? 58 : 0;
	Panel(X, Y + Top, FullW, H - Top, FLinearColor(0.03f, 0.03f, 0.04f, 0.9f));
	if (bCreator && World->bSaveChecked)
	{
		Rc(X + W - 4, Y + Top + 14, 1, H - Top - 28, FLinearColor(0.55f, 0.45f, 0.25f, 0.5f));
		DrawCreator(X + W + 4, Y + Top + 12, 310);
	}
	if (!HLArtBrush(TEXT("ui_title"))) TxC(X + W * 0.5f, Y + 16, TEXT("HOLLYWOODLAND"), 34, ColGold, true);
	TxC(X + W * 0.5f, Y + 70, TEXT("Hollywood, autumn 1930. The dome is down. The studios belong to the hive."), 12, ColWhite);
	Rc(X + 20, Y + 100, W - 40, 62, FLinearColor(0.12f, 0.1f, 0.03f));
	TxWrap(X + 30, Y + 106, W - 60, TEXT("This is a single-player game. Every other \"player\" you meet (in chat, in groups, at the auction house) is simulated by AI. Nothing you type is seen by another human. They play their parts in character, so enjoy the show."), 11, FLinearColor(1, 0.9f, 0.6f));
	if (!World->bSaveChecked) { TxC(X + W * 0.5f, Y + 300, TEXT("Checking for a saved character..."), 13, ColDim); return; }
	float Cy = Y + 176;
	if (!World->SavedJson.IsEmpty())
	{
		if (Btn(X + W * 0.5f - 150, Cy, 300, 36, TEXT("Continue saved character"), true, 13))
		{
			if (Gm.LoadJson(World->SavedJson)) { Gm.EnterWorld(); return; }
			World->SavedJson.Reset();
		}
		Cy += 46;
		TxC(X + W * 0.5f, Cy, TEXT("or create a new character (replaces the save):"), 10, ColDim);
		Cy += 20;
	}
	Tx(X + 30, Cy + 6, TEXT("Name:"), 13, ColWhite);
	Rc(X + 90, Cy, 240, 30, FLinearColor(0.4f, 0.35f, 0.2f));
	++Layer; LastKind = 1;
	Rc(X + 91, Cy + 1, 238, 28, FLinearColor(0.02f, 0.02f, 0.02f));
	const bool bBlink = FMath::Fmod(FPlatformTime::Seconds(), 1.0) < 0.5;
	if (Hover(X + 90, Cy, 240, 30) && bLClick) { bLClick = false; bRestoreFocus = false; }
	Tx(X + 98, Cy + 5, NameEntry + (bBlink && !bRestoreFocus ? TEXT("_") : TEXT("")), 13, ColWhite);
	Tx(X + 340, Cy + 8, TEXT("(type a name, letters only)"), 10, ColDim);
	Cy += 44;
	Tx(X + 30, Cy, TEXT("Class:"), 13, ColWhite);
	Cy += 24;
	for (int32 i = 0; i < D.Classes.Num(); ++i)
	{
		const FHLClass& C = D.Classes[i];
		const float Bx = X + 30 + (i % 4) * 176, By = Cy + (i / 4) * 40;
		if (i == SelClass) Rc(Bx - 3, By - 3, 172, 36, C.Color);
		if (Btn(Bx, By, 166, 30, C.Name, true, 12)) SelClass = i;
	}
	Cy += 92;
	const FHLClass& C = D.Classes[FMath::Clamp(SelClass, 0, D.Classes.Num() - 1)];
	Tx(X + 30, Cy, C.Name, 16, C.Color, true);
	FString Role = C.Role == TEXT("tank") ? TEXT("Tank and melee damage") : C.Role == TEXT("healer") ? TEXT("Healer") : C.Role == TEXT("caster") ? TEXT("Ranged caster") : C.Role == TEXT("ranged") ? TEXT("Ranged damage") : TEXT("Melee damage");
	Tx(X + 30, Cy + 26, FString::Printf(TEXT("%s.  Armour: %s.  Resource: %s.  Specialisations: %s."), *Role, *C.Armour, *C.Resource, *FString::Join(C.Specs, TEXT(", "))), 11, ColWhite);
	FString Abil;
	for (const FHLAbility* Ab : C.Abilities) Abil += FString::Printf(TEXT("%s (%d)   "), *Ab->Name, Ab->Level);
	TxWrap(X + 30, Cy + 48, W - 60, TEXT("Abilities to level 12: ") + Abil, 10, ColDim);
	{
		// restore from the cloud copy on another browser: the code is shown in the game menu
		const float Ry = Y + H - 142;
		Tx(X + 30, Ry + 5, TEXT("Restore a save:"), 10, ColDim);
		const float Bx = X + 140, Bw = 250;
		if (Hover(Bx, Ry, Bw, 24) && bLClick) { bLClick = false; bRestoreFocus = true; }
		Rc(Bx, Ry, Bw, 24, bRestoreFocus ? FLinearColor(0.85f, 0.7f, 0.35f) : FLinearColor(0.35f, 0.3f, 0.2f));
		++Layer; LastKind = 1;
		Rc(Bx + 1, Ry + 1, Bw - 2, 22, FLinearColor(0.02f, 0.02f, 0.02f));
		Tx(Bx + 6, Ry + 4, RestoreEntry.IsEmpty() && !bRestoreFocus ? FString(TEXT("click, then type your cloud code")) : RestoreEntry + (bBlink && bRestoreFocus ? TEXT("_") : TEXT("")), 10, RestoreEntry.IsEmpty() && !bRestoreFocus ? ColDim : ColWhite);
		if (Btn(Bx + Bw + 8, Ry, 80, 24, bRestoreBusy ? TEXT("...") : TEXT("Restore"), RestoreEntry.Len() >= 8 && !bRestoreBusy, 10))
		{
			bRestoreBusy = true; RestoreMsg = TEXT("Looking for that save...");
			TWeakPtr<const SWidget> Weak = AsShared();
			const SHLGameUI* Self = this;
			AHLWorldActor* WA = World;
			HLBridge::Fetch(TEXT("GET"), FString::Printf(TEXT("%s/v1/save/%s"), *Gm.ApiBase, *RestoreEntry), FString(), [Weak, Self, WA](const FHLBridgeResult& R)
			{
				if (!Weak.IsValid()) return;
				Self->bRestoreBusy = false;
				if (R.Ok() && R.Body.Contains(TEXT("\"name\""))) { WA->SavedJson = R.Body; HLBridge::StorageSet(TEXT("hollywoodland_save_v1"), R.Body); Self->RestoreMsg = TEXT("Save found. Press Continue saved character."); }
				else Self->RestoreMsg = R.Status == 404 ? TEXT("No save with that code.") : TEXT("Could not reach the server. Try again.");
			});
		}
		if (!RestoreMsg.IsEmpty()) Tx(Bx + Bw + 96, Ry + 5, RestoreMsg, 10, RestoreMsg.StartsWith(TEXT("Save found")) ? ColGreen : ColDim);
	}
	const bool bOk = NameEntry.Len() >= 2;
	if (Btn(X + W * 0.5f - 130, Y + H - 96, 260, 40, TEXT("Enter Hollywood"), bOk, 15))
	{
		FString Name = NameEntry.ToLower(); Name[0] = FChar::ToUpper(Name[0]);
		Gm.NewCharacter(Name, &C);
		if (bNewLookSet) Gm.Player.Look = NewLook;
		if (FHLUnit* U = Gm.PlayerUnit()) { U->Level = 1; U->Pos = Gm.Areas[0].Def->Spawn; U->Area = 0; U->bDead = false; }
		Gm.EnterWorld();
	}
	TxC(X + W * 0.5f, Y + H - 46, TEXT("WASD move  |  hold right mouse to turn  |  Tab target  |  1-0 abilities  |  right-click or F to interact  |  Enter to chat"), 10, ColDim);
	TxC(X + W * 0.5f, Y + H - 26, TEXT("Proof of concept: gray-box art. Hollywood Boulevard (levels 1-12) and the Egyptian Theatre are playable."), 10, ColDim);
}

void SHLGameUI::DrawWorldOverlay() const
{
	FHLGame& Gm = G();
	const FHLUnit* P = Gm.PlayerUnit();
	if (!P) return;
	const FHLAreaState& Area = Gm.Areas[P->Area];
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
		if (bWanted || D < 12) TxC(S.X, S.Y, (bWanted && !O.Def->Quest.IsEmpty() ? TEXT("* ") : TEXT("")) + O.Def->Name, 10, bWanted ? FLinearColor(1, 0.9f, 0.3f) : ColDim);
		FPick Pk; Pk.Area = P->Area; Pk.Obj = o; Pk.Pos = S + FVector2D(0, 24); Pk.Radius = FMath::Clamp(500.f / FMath::Max(4.f, D), 16.f, 70.f); Pk.Depth = D;
		Picks.Add(Pk);
	}
	// nameplates
	for (FHLUnit* U : Gm.UnitList)
	{
		if (U->Area != P->Area) continue;
		const float D = FVector2D::Distance(U->Pos, P->Pos);
		const bool bTarget = U->Id == P->TargetId;
		if (D > 80 && !bTarget) continue;
		const float Height = (U->Shape == TEXT("human") ? 1.9f : 1.3f) * U->Scale + U->Z + (U->bMounted ? 0.9f : 0);
		FVector2D S, Feet;
		if (!Project(World->ToWorld(U->Area, U->Pos, U->bDead ? 0.8f : Height + 0.35f), S)) continue;
		if (Project(World->ToWorld(U->Area, U->Pos, Height * 0.5f), Feet))
		{
			FPick Pk; Pk.UnitId = U->Id; Pk.Pos = Feet; Pk.Radius = FMath::Clamp(700.f * FMath::Max(1.f, U->Scale) / FMath::Max(3.f, D + World->CamDist * 0.5f), 14.f, 90.f); Pk.Depth = D;
			if (U->Id != Gm.PlayerUnitId) Picks.Add(Pk);
		}
		if (U->Id == Gm.PlayerUnitId) continue;
		if (U->bDead) { if (U->Kind == EHLKind::Mob && U->bLootable && U->TappedBy == Gm.PlayerUnitId && D < 40) TxC(S.X, S.Y, TEXT("* loot *"), 10, ColGold); continue; }
		if (D > 55 && !bTarget && U->Kind != EHLKind::Npc) continue;
		FLinearColor C = ColWhite; FString Name = U->Name;
		if (U->Kind == EHLKind::Mob)
		{
			C = !U->Mob->bAggressive ? FLinearColor(1, 0.9f, 0.2f) : FLinearColor(1, 0.3f, 0.25f);
			if (U->TappedBy && U->TappedBy != Gm.PlayerUnitId) C = FLinearColor(0.6f, 0.6f, 0.6f);
		}
		else if (U->Kind == EHLKind::Npc) C = FLinearColor(0.4f, 1, 0.4f);
		else { C = U->SimIdx >= 0 && Gm.Sims[U->SimIdx].bInParty ? FLinearColor(0.5f, 0.8f, 1) : FLinearColor(0.55f, 0.65f, 1); if (Gm.Player.bShowSim) Name += TEXT(" <AI>"); if (U->SimIdx >= 0 && Gm.Sims[U->SimIdx].bAfk) Name = TEXT("<AFK> ") + Name; }
		const int32 Sz = bTarget ? 12 : D < 25 ? 11 : 9;
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
	// floating combat text and speech
	for (const FHLFloatText& F : Gm.Floats)
	{
		const FHLUnit* U = Gm.Unit(F.UnitId);
		if (!U || U->Area != P->Area || F.Text.IsEmpty()) continue;
		FVector2D S;
		const bool bSpeech = F.Text.Len() > 14 && !F.Text.StartsWith(TEXT("+"));
		if (!Project(World->ToWorld(U->Area, U->Pos, 2.3f * FMath::Max(1.f, U->Scale * 0.8f) + (bSpeech ? 0.6f : F.Age * 1.2f)), S)) continue;
		const float A = FMath::Clamp(1.6f - F.Age, 0.f, 1.f);
		if (bSpeech) { const float Wd = TW(F.Text, 10) + 12; Rc(S.X - Wd * 0.5f, S.Y - 4, Wd, 20, FLinearColor(0, 0, 0, 0.55f * A)); TxC(S.X, S.Y - 2, F.Text, 10, FLinearColor(1, 1, 1, A)); }
		else TxC(S.X + (F.UnitId % 3 - 1) * 14, S.Y, F.Text, F.bCrit ? 17 : 12, FLinearColor(F.Color.R, F.Color.G, F.Color.B, A), true);
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
		if (Best->UnitId) Gm.InteractUnit(Best->UnitId);
		else Gm.ObjectInteract(Best->Area, Best->Obj);
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
	UnitFrame(16, 16, 230, *P, true);
	// auras
	{
		float Ax = 16;
		for (const FHLAura& A : P->Auras)
		{
			if (A.Stat == TEXT("none") && A.Name != TEXT("Recently Bandaged")) continue;
			const FLinearColor C = A.bHarmful ? FLinearColor(0.5f, 0.1f, 0.1f) : FLinearColor(0.1f, 0.3f, 0.5f);
			const FString Lbl = FString::Printf(TEXT("%s %ds"), *A.Name, FMath::CeilToInt32(A.Remaining));
			const float Wd = TW(Lbl, 9) + 8;
			Rc(Ax, 80, Wd, 16, C); Tx(Ax + 4, 81, Lbl, 9, ColWhite);
			Ax += Wd + 4;
		}
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
			const float Y = 110 + i * 48;
			UnitFrame(16, Y, 170, *U, false);
			if (Hover(16, Y, 170, 40) && bLClick) { bLClick = false; Gm.TargetUnit(U->Id); }
		}
	}
	if (Gm.Party.Num()) { Block(16, 110 + Gm.Party.Num() * 48, 90, 18); if (Btn(16, 110 + Gm.Party.Num() * 48, 90, 18, TEXT("Leave party"), true, 9)) Gm.LeaveParty(); }

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
				const FLinearColor Tint = !bUsable ? FLinearColor(0.35f, 0.4f, 0.6f, 1) : !bRange ? FLinearColor(1, 0.35f, 0.3f, 1) : FLinearColor::White;
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
			if (Hover(Bx - 6, VS.Y - 16, Bw + 8, 9)) { TipText = FString::Printf(TEXT("Rested\nThe next %d experience from kills is doubled.\nRest at an inn, or log out at your home inn, to earn more."), (int32)Gm.Player.Rested); TipPos = Mouse; }
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
	if (P->bAutoAttack && T && !T->bDead) TxC(VS.X * 0.5f, VS.Y - 96, P->bRangedAuto ? TEXT("Auto-shot") : TEXT("Attacking"), 9, FLinearColor(1, 0.5f, 0.4f));
	// error and banner text
	if (Gm.Now < Gm.ErrorUntil) TxC(VS.X * 0.5f, 170, Gm.ErrorText, 14, FLinearColor(1, 0.2f, 0.15f, FMath::Min(1.f, Gm.ErrorUntil - (float)Gm.Now)), true);
	if (Gm.Now < Gm.BannerUntil)
	{
		const float A = FMath::Min(1.f, Gm.BannerUntil - (float)Gm.Now);
		TxC(VS.X * 0.5f, 210, Gm.BannerText, 26, FLinearColor(1, 0.85f, 0.4f, A), true);
		TxC(VS.X * 0.5f, 252, Gm.BannerSub, 13, Gm.bBannerHostile ? FLinearColor(1, 0.3f, 0.2f, A) : FLinearColor(1, 1, 1, A), Gm.bBannerHostile);
	}
	// micro menu
	{
		static const TCHAR* Lbl[] = { TEXT("Char (C)"), TEXT("Bags (B)"), TEXT("Quests (L)"), TEXT("Skills (P)"), TEXT("Map (M)"), TEXT("Who (O)"), TEXT("Talent (N)"), TEXT("Menu") };
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
			if (bH) { TipText = Lbl[i]; TipPos = Mouse; if (bLClick) { bLClick = false; *Flags[i] = !*Flags[i]; if (i == 5) Gm.bWhoOpen = bWho; } }
		}
		{
			const FString Info = FString::Printf(TEXT("%s%s    Players online: %d    %s"), Gm.bWorkerOk ? TEXT("") : TEXT("(chat AI offline: corpus mode)    "), *HLMoney(Gm.Player.Cents), Gm.OnlineCount(), TEXT(""));
			Tx(VS.X - 14 - TW(Info.TrimEnd(), 10), Mtop - 18, Info.TrimEnd(), 10, ColGold);
		}
	}
	DrawQuestMarkers();
	DrawMinimap();
	DrawTracker();
	DrawChat();
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
	if (!P->bGhost) for (const FHLQuestState& QS : Gm.Player.Quests)
	{
		FVector2D Goal;
		if (Goals.Num() < 3 && HLQuestGoal(Gm, QS, *P, Goal)) Goals.Add({ Goal, QS.bComplete ? FLinearColor(1, 0.85f, 0.2f) : FLinearColor(1, 0.95f, 0.6f) });
	}
	for (const auto& GC : Goals)
	{
		const FVector2D Goal = GC.Key;
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
			TxC(S.X, S.Y + 9, FString::Printf(TEXT("%d m"), (int32)FMath::RoundToInt(D)), 9, C);
		}
		else
		{
			// arrow on an ellipse around the screen centre, pointing the way relative to the camera
			const FVector L = CamRot.UnrotateVector(Wp - CamLoc);
			const float Ang = FMath::Atan2(L.Y, L.X); // 0 = straight ahead, + = right
			const FVector2D Dir(FMath::Sin(Ang), -FMath::Cos(Ang));
			const FVector2D Ctr(VS.X * 0.5f, VS.Y * 0.45f);
			const FVector2D Tip = Ctr + FVector2D(Dir.X * VS.X * 0.3f, Dir.Y * VS.Y * 0.32f);
			const FVector2D Side(-Dir.Y, Dir.X);
			const FVector2D Base = Tip - Dir * 16;
			Line(Base + Side * 9, Tip, C, 3); Line(Base - Side * 9, Tip, C, 3); Line(Base + Side * 9, Base - Side * 9, C, 2);
			TxC(Base.X - Dir.X * 14, Base.Y - Dir.Y * 14 - 6, FString::Printf(TEXT("%d m"), (int32)FMath::RoundToInt(D)), 9, C);
		}
	}
}

void SHLGameUI::DrawMinimap() const
{
	FHLGame& Gm = G();
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
	auto Dot = [&](const FVector2D& Wp, const FLinearColor& C, float Sz) { const float Dx = X + S * 0.5f + (Wp.X - P->Pos.X) * K, Dy = Y + S * 0.5f + (Wp.Y - P->Pos.Y) * K; if (Dx > X + 2 && Dx < X + S - 2 && Dy > Y + 2 && Dy < Y + S - 2) Rc(Dx - Sz * 0.5f, Dy - Sz * 0.5f, Sz, Sz, C); };
	for (const FHLUnit* U : Gm.UnitList)
	{
		if (U->Area != P->Area || U->bDead || U == P) continue;
		if (U->Kind == EHLKind::Npc) { if (U->Npc) { const int32 Mk = Gm.NpcMarker(*U->Npc); Dot(U->Pos, Mk == 1 || Mk == 2 ? ColGold : FLinearColor(0.3f, 0.8f, 0.3f), Mk ? 6 : 3); } }
		else if (U->Kind == EHLKind::Sim) Dot(U->Pos, U->SimIdx >= 0 && Gm.Sims[U->SimIdx].bInParty ? FLinearColor(0.3f, 1, 1) : FLinearColor(0.45f, 0.55f, 1), 3);
		else if (FVector2D::Distance(U->Pos, P->Pos) < 45) Dot(U->Pos, FLinearColor(1, 0.25f, 0.2f), 3);
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
	}
	const float Rad = FMath::DegreesToRadians(P->Yaw);
	const FVector2D C(X + S * 0.5f, Y + S * 0.5f);
	Rc(C.X - 3, C.Y - 3, 6, 6, ColWhite);
	Line(C, C + FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)) * 12, ColWhite, 2);
	TxC(X + S * 0.5f, Y + S + 3, Gm.CurSubzone, 10, ColGold);
	if (const int32 Unread = Gm.UnreadMail())
	{
		const FString M = FString::Printf(TEXT("New mail (%d)"), Unread);
		Rc(X + 2, Y + S - 18, TW(M, 9) + 8, 16, FLinearColor(0, 0, 0, 0.7f));
		Tx(X + 6, Y + S - 17, M, 9, ColGold, true);
	}
	Tx(X + 4, Y + 2, TEXT("N"), 9, ColDim);
}

void SHLGameUI::DrawTracker() const
{
	FHLGame& Gm = G();
	float X = VS.X - 250, Y = 222;
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
		if (Gm.PlayerUnit()->Level <= 2 && Gm.Player.Done.Num() == 0) { Tx(X, Y, TEXT("Speak to Sid Grauman (the gold ! nearby)."), 10, ColGold); Tx(X, Y + 16, TEXT("Right-click him, or stand close and press F."), 10, ColDim); }
		return;
	}
	for (const FHLQuestState& QS : Gm.Player.Quests)
	{
		if (Y > VS.Y - 320) break;
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
			// item links in quality colours
			const FString& Ln = Lines[k];
			int32 Open = INDEX_NONE;
			if (L.SimIdx < 0 && L.From.IsEmpty() && Ln.FindChar('[', Open) && Ln.Find(TEXT("]")) > Open)
			{
				const int32 Close = Ln.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open);
				const FString Pre = Ln.Left(Open), Link = Ln.Mid(Open, Close - Open + 1), Post = Ln.Mid(Close + 1);
				const FHLItem* It = FHLData::Get().ItemByName(Link.Mid(1, Link.Len() - 2));
				Tx(X + 6, Cy, Pre, 10, C);
				Tx(X + 6 + TW(Pre, 10), Cy, Link, 10, It ? HLQualityColor(It->Q) : C);
				Tx(X + 6 + TW(Pre + Link, 10), Cy, Post, 10, C);
				if (It && Hover(X + 6 + TW(Pre, 10), Cy, TW(Link, 10), LH)) { TipItem = It; TipPos = Mouse; }
			}
			else Tx(X + 6, Cy, Ln, 10, C);
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
			Tx(X + 6, Ey + 3, TEXT("Press Enter to chat  (/help for commands, click a name to whisper)"), 10, ColDim);
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
	if (bHelp)
	{
		const float W = 430, H = 250, X = VS.X - W - 16, Y = VS.Y - H - 84;
		Block(X, Y, W, H); Panel(X, Y, W, H);
		Tx(X + 10, Y + 8, TEXT("How to play"), 13, ColGold, true);
		TxWrap(X + 10, Y + 32, W - 20,
			TEXT("Move: W A S D (Q/E strafe). Hold the right mouse button and drag to turn; left-drag looks around; wheel zooms. Space jumps.\n")
			TEXT("Fight: Tab or left-click selects a target; right-click it (or press 1) to attack; number keys use abilities.\n")
			TEXT("Interact: right-click people, corpses and objects, or stand close and press F. Gold ! means a quest, gold ? means hand it in.\n")
			TEXT("Rest: use food from your bags (B), or press X to sit. Train new abilities every 2 levels at your class trainer.\n")
			TEXT("People: Enter to chat, click a name to whisper, target someone and Invite them. /who lists everyone online.\n")
			TEXT("Professions: Salvaging from Ike Brandt at Vine, Gunsmithing from Earl Tanner at Highland. K opens your workbench.\n")
			TEXT("Press H to hide or show this panel."), 10, ColWhite);
		if (Btn(X + W - 80, Y + 6, 70, 20, TEXT("Hide (H)"), true, 9)) bHelp = false;
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
		Lines.Add({ It.Name, HLQualityColor(It.Q) });
		if (It.IsEquip())
		{
			FString Slot = It.Slot; Slot[0] = FChar::ToUpper(Slot[0]);
			FString Type = It.Type; Type[0] = FChar::ToUpper(Type[0]);
			Lines.Add({ FString::Printf(TEXT("%s    %s%s"), *Slot, *Type, It.Hands == 2 ? TEXT(" (two-hand)") : TEXT("")), ColWhite });
			if (It.Speed > 0) Lines.Add({ FString::Printf(TEXT("%d - %d damage    speed %.1f    (%.1f dps)"), It.DMin, It.DMax, It.Speed, (It.DMin + It.DMax) * 0.5f / It.Speed), ColWhite });
			if (It.Armor > 0) Lines.Add({ FString::Printf(TEXT("%d Armour"), It.Armor), ColWhite });
			for (int32 i = 0; i < HL_NUM_STATS; ++i) if (It.Stats[i]) Lines.Add({ FString::Printf(TEXT("+%d %s"), It.Stats[i], HLStatNames[i]), ColGreen });
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
