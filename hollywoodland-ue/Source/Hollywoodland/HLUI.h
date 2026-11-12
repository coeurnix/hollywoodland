// The whole interface as one immediate-mode Slate leaf widget: it paints every frame from FHLGame state and handles all
// mouse/keyboard input itself (no UMG, no assets, no UE input bindings). Coordinates are "virtual" pixels: the viewport is
// scaled so that it is always 900 units tall. Implementation: HLUI.cpp (toolkit, input, HUD), HLUIWindows.cpp (windows).
#pragma once
#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "HLGame.h"

class AHLWorldActor;

class SHLGameUI : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SHLGameUI) {}
		SLATE_ARGUMENT(AHLWorldActor*, World)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	void PreTick(float Dt);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geo, const FSlateRect& Cull, FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1280, 720); }
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geo, const FKeyEvent& E) override;
	virtual FReply OnKeyUp(const FGeometry& Geo, const FKeyEvent& E) override;
	virtual FReply OnKeyChar(const FGeometry& Geo, const FCharacterEvent& E) override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geo, const FPointerEvent& E) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geo, const FPointerEvent& E) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geo, const FPointerEvent& E) override { return OnMouseButtonDown(Geo, E); }
	virtual FReply OnMouseMove(const FGeometry& Geo, const FPointerEvent& E) override;
	virtual FReply OnMouseWheel(const FGeometry& Geo, const FPointerEvent& E) override;
	virtual void OnFocusLost(const FFocusEvent& E) override { Down.Reset(); bLDown = bRDown = false; }

private:
	AHLWorldActor* World = nullptr;
	FHLGame& G() const;

	// ---- paint context
	mutable FSlateWindowElementList* DL = nullptr;
	mutable FGeometry PGeo;
	mutable int32 Layer = 0, LastKind = 0;
	mutable float Sc = 1;
	mutable FVector2D VS = FVector2D(1600, 900); // virtual size
	mutable FVector2D ViewPx = FVector2D(1600, 900);
	mutable class APlayerController* PC = nullptr;
	static float ScaleFor(const FGeometry& Geo) { return FMath::Clamp(Geo.GetLocalSize().Y / 900.f, 0.5f, 3.f); }

	// ---- toolkit
	void Rc(float X, float Y, float W, float H, const FLinearColor& C) const;
	void Panel(float X, float Y, float W, float H, const FLinearColor& Bg = FLinearColor(0.03f, 0.03f, 0.04f, 0.88f), const FLinearColor& Border = FLinearColor(0.45f, 0.38f, 0.22f, 1)) const;
	void Tx(float X, float Y, const FString& S, int32 Size, const FLinearColor& C, bool bBold = false, bool bShadow = true) const;
	void TxC(float CX, float Y, const FString& S, int32 Size, const FLinearColor& C, bool bBold = false) const;
	float TW(const FString& S, int32 Size, bool bBold = false) const;
	void Wrap(const FString& S, int32 Size, float MaxW, TArray<FString>& Out) const;
	float TxWrap(float X, float Y, float W, const FString& S, int32 Size, const FLinearColor& C) const; // returns height
	void Line(const FVector2D& A, const FVector2D& B, const FLinearColor& C, float Thick = 1) const;
	bool Hover(float X, float Y, float W, float H) const { return Mouse.X >= X && Mouse.X < X + W && Mouse.Y >= Y && Mouse.Y < Y + H; }
	bool Btn(float X, float Y, float W, float H, const FString& Label, bool bEnabled = true, int32 Size = 11) const;
	void Bar(float X, float Y, float W, float H, float Pct, const FLinearColor& C, const FString& Label = FString(), int32 Size = 10) const;
	bool Win(const TCHAR* Id, const FString& Title, float W, float H, bool& bOpen, float DefX, float DefY) const; // content origin in WX, WY
	int32 ItemIcon(float X, float Y, float S, const FHLItem* It, int32 Count = 1) const; // 0 none, 1 left click, 2 right click
	void AbilityTip(const FHLAbility& Ab) const;
	bool Project(const FVector& WorldPos, FVector2D& Out) const;
	void Block(float X, float Y, float W, float H) const { UiRectsNext.Add(FSlateRect(X, Y, X + W, Y + H)); }
	bool OverUi(const FVector2D& P) const { for (const FSlateRect& R : UiRects) if (P.X >= R.Left && P.X < R.Right && P.Y >= R.Top && P.Y < R.Bottom) return true; return false; }

	// ---- screens
	void DrawTitle() const;
	void DrawWorldOverlay() const;
	void DrawHud() const;
	void DrawChat() const;
	void DrawMinimap() const;
	void DrawTracker() const;
	void DrawWindows() const;
	void DrawCharacter() const;
	void DrawBags() const;
	void DrawQuestLog() const;
	void DrawSpellbook() const;
	void DrawMap() const;
	void DrawWho() const;
	void DrawNpc() const;
	void DrawInspect() const;
	void DrawTalents() const;
	void DrawMenu() const;
	void DrawPopups() const;
	void DrawTooltip() const;
	void HandleWorldClick() const;
	void ChatColor(const FHLChatLine& L, FLinearColor& C, FString& Text) const;
	void Interact() const;

	// ---- input state
	TSet<FKey> Down;
	mutable FVector2D Mouse = FVector2D::ZeroVector, DownPos = FVector2D::ZeroVector;
	mutable bool bLDown = false, bRDown = false, bLClick = false, bRClick = false, bDownOverUi = false, bDragging = false;
	mutable float DragDist = 0, Wheel = 0;
	mutable TArray<FSlateRect> UiRects, UiRectsNext;
	// chat entry / text fields
	mutable bool bTyping = false, bNameFocus = true, bSwallowChar = false;
	mutable FString ChatEntry, NameEntry;
	mutable int32 ChatScroll = 0;
	mutable bool bCombatLog = false;
	// windows
	mutable bool bChar = false, bBags = false, bQuests = false, bSpells = false, bMap = false, bWho = false, bInspect = false, bTalents = false, bMenu = false, bWorldMap = false, bHelp = true;
	mutable TMap<FName, FVector2D> WinPos;
	mutable FName DragWin; mutable FVector2D DragOff;
	mutable float WX = 0, WY = 0;
	mutable int32 SelClass = 0, SelQuest = 0, RewardChoice = -1, AuctionPage = 0, AuctionFilter = 0, WhoScroll = 0, VendorScroll = 0, InspectSim = -1;
	mutable FString SelQuestId;
	// tooltip (set during paint, drawn last)
	mutable const FHLItem* TipItem = nullptr; mutable FString TipText; mutable FVector2D TipPos;
	// world picks collected during the overlay pass
	struct FPick { int32 UnitId = 0, Area = -1, Obj = -1; FVector2D Pos; float Radius = 0, Depth = 0; };
	mutable TArray<FPick> Picks;
};
