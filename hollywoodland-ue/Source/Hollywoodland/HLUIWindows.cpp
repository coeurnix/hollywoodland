// Windows: character, bags, quest log, abilities, map, who, NPC dialogs (quests, vendor, trainer, bank, auction, red car),
// inspect, talents, menu. Data-only content (other zones, later abilities, high-level gear) is surfaced here.
#include "HLUI.h"
#include "HLWorldActor.h"

static const FLinearColor ColGold(1.f, 0.82f, 0.25f), ColDim(0.62f, 0.62f, 0.62f), ColWhite(1, 1, 1), ColRed(1, 0.25f, 0.2f), ColGreen(0.3f, 1, 0.3f);
static const TCHAR* EquipSlots[] = { TEXT("hat"), TEXT("neck"), TEXT("coat"), TEXT("wrist"), TEXT("gloves"), TEXT("belt"), TEXT("trousers"), TEXT("shoes"), TEXT("ring"), TEXT("main"), TEXT("off"), TEXT("ranged") };
static const TCHAR* EquipNames[] = { TEXT("Hat"), TEXT("Neck"), TEXT("Coat"), TEXT("Wrist"), TEXT("Gloves"), TEXT("Belt"), TEXT("Trousers"), TEXT("Shoes"), TEXT("Ring"), TEXT("Main hand"), TEXT("Off hand"), TEXT("Ranged") };

void SHLGameUI::DrawWindows() const
{
	DrawNpc();
	DrawCharacter();
	DrawBags();
	DrawQuestLog();
	DrawSpellbook();
	DrawMap();
	DrawWho();
	DrawInspect();
	DrawTalents();
	DrawCraft();
	DrawMenu();
}

void SHLGameUI::DrawCharacter() const
{
	FHLGame& Gm = G();
	const bool bDoll = FHLChars::Get().Ready();
	if (!Win(TEXT("char"), FString::Printf(TEXT("%s  -  Level %d %s"), *Gm.Player.Name, Gm.PlayerUnit()->Level, *Gm.Player.Cls->Name), bDoll ? 420 : 330, bDoll ? 500 : 470, bChar, 260, 110)) return;
	const FHLUnit& U = *Gm.PlayerUnit();
	auto Slot = [&](int32 i, float X, float Y, float S)
	{
		const FHLItem* It = Gm.Player.Equip.FindRef(EquipSlots[i]);
		if (ItemIcon(X, Y, S, It) == 2) Gm.Unequip(EquipSlots[i]);
		if (!It) { if (Hover(X, Y, S, S)) { TipText = EquipNames[i]; TipPos = Mouse; } return It; }
		const float Dur = Gm.SlotDurability(EquipSlots[i]);
		if (Dur < 0.995f)
		{
			// wear shows as a strip along the bottom of the icon (red when broken)
			++Layer; LastKind = 1;
			Rc(X + 2, Y + S - 5, (S - 4), 3, FLinearColor(0, 0, 0, 0.7f));
			Rc(X + 2, Y + S - 5, (S - 4) * FMath::Max(Dur, 0.f), 3, Dur <= 0 ? ColRed : Dur < 0.3f ? FLinearColor(1, 0.6f, 0.2f) : FLinearColor(0.85f, 0.85f, 0.6f));
			if (Dur <= 0) TxC(X + S * 0.5f, Y + 2, TEXT("broken"), 8, ColRed);
		}
		return It;
	};
	float Y;
	if (bDoll)
	{
		// paper doll: clothes down the sides, the figure in the middle, weapons under it
		const float S = 40, Step = 46, Cw = 400, Pw = 196, Ph = 6 * Step - 6;
		for (int32 i = 0; i < 6; ++i) Slot(i, WX, WY + i * Step, S);
		for (int32 i = 6; i < 9; ++i) Slot(i, WX + Cw - S, WY + (i - 6) * Step, S);
		const FHLCharLook L = FHLChars::Get().LookFor(Gm, U);
		DrawPreview(WX + (Cw - Pw) * 0.5f, WY, Pw, Ph, L, TEXT("Idle"));
		for (int32 i = 9; i < 12; ++i) Slot(i, WX + Cw * 0.5f - S * 1.5f - 6 + (i - 9) * Step, WY + Ph + 8, S);
		Y = WY + Ph + 8 + S + 12;
	}
	else
	{
		for (int32 i = 0; i < 12; ++i)
		{
			const float Ry = WY + i * 28;
			const FHLItem* It = Slot(i, WX, Ry, 26);
			Tx(WX + 32, Ry + 5, It ? It->Name : FString(EquipNames[i]), 10, It ? HLQualityColor(It->Q) : ColDim);
		}
		Y = WY + 12 * 28 + 6;
	}
	const float Cx = WX + (bDoll ? 205 : 160);
	for (int32 i = 0; i < HL_NUM_STATS; ++i) Tx(i < 3 ? WX : Cx, Y + (i % 3) * 15, FString::Printf(TEXT("%s: %d"), HLStatNames[i], U.Stats[i]), 10, ColWhite);
	Tx(Cx, Y + 30, FString::Printf(TEXT("Armour: %d"), (int32)U.Armor), 10, ColWhite);
	Y += 50;
	Tx(WX, Y, FString::Printf(TEXT("Damage: %d - %d  (%.1f s)"), (int32)(U.WeaponMin + U.AttackPower / 14 * U.WeaponSpeed), (int32)(U.WeaponMax + U.AttackPower / 14 * U.WeaponSpeed), U.WeaponSpeed), 10, ColWhite);
	Tx(Cx, Y, FString::Printf(TEXT("Crit: %.1f%%  Dodge: %.1f%%"), U.Crit * 100, U.Dodge * 100), 10, ColWhite);
	Tx(WX, Y + 16, FString::Printf(TEXT("Kills: %d   Deaths: %d   Played: %d min"), Gm.Player.Kills, Gm.Player.Deaths, (int32)(Gm.Player.PlayedS / 60)), 10, ColDim);
	if (Gm.Player.Salvage > 0) Tx(WX, Y + 32, FString::Printf(TEXT("Salvaging %d / 75 (Apprentice)"), Gm.Player.Salvage), 10, ColDim);
	if (Gm.Player.Guns > 0) Tx(WX, Y + 48, FString::Printf(TEXT("Gunsmithing %d / 75 (Apprentice)"), Gm.Player.Guns), 10, ColDim);
	if (const int32* Rep = Gm.Player.Rep.Find(TEXT("fac_lapd"))) Tx(Cx, Y + 32, FString::Printf(TEXT("LAPD: %d / 3000 Neutral"), *Rep), 10, ColDim);
}

void SHLGameUI::DrawBags() const
{
	FHLGame& Gm = G();
	if (!Win(TEXT("bags"), FString::Printf(TEXT("Bags    %s"), *HLMoney(Gm.Player.Cents)), 270, 228, bBags, VS.X - 300, VS.Y - 560)) return;
	for (int32 i = 0; i < Gm.Player.Bag.Num(); ++i)
	{
		const FHLInvSlot& S = Gm.Player.Bag[i];
		const int32 R = ItemIcon(WX + (i % 6) * 42, WY + (i / 6) * 42, 40, S.Item, S.Count);
		if (Gm.TradeSim >= 0 && Gm.TradeBag.Contains(i)) { ++Layer; LastKind = 1; Rc(WX + (i % 6) * 42, WY + (i / 6) * 42, 40, 40, FLinearColor(0.2f, 0.8f, 0.3f, 0.35f)); }
		if (R == 2 || R == 1)
		{
			if (Gm.TradeSim >= 0) { if (S.Item && S.Item->Kind != TEXT("quest")) { if (Gm.TradeBag.Contains(i)) Gm.TradeBag.Remove(i); else if (Gm.TradeBag.Num() < 6) Gm.TradeBag.Add(i); } }
			else if (Gm.OpenNpcMode == TEXT("auction")) { if (S.Item) { int32 Avg = 0, Lo, Hi, Ns; const int32 Each = Gm.PriceStats(*S.Item, Avg, Lo, Hi, Ns) ? FMath::Max(2, (Avg + Gm.MarketPrice(*S.Item)) / 2) : Gm.MarketPrice(*S.Item); Gm.ListAuction(i, Each * S.Count, (float)AuctionHours); } }
			else if (R == 2) Gm.UseBag(i);
		}
	}
	if (Gm.TradeSim >= 0) { Tx(WX, WY + 172, TEXT("Click items to add them to the trade."), 9, ColDim); return; }
	Tx(WX, WY + 172, Gm.OpenNpcMode == TEXT("vendor") ? TEXT("Right-click an item to sell it.") : Gm.OpenNpcMode == TEXT("auction") ? TEXT("Click an item to auction it at market price.") : Gm.OpenNpcMode == TEXT("bank") ? TEXT("Right-click to deposit.") : TEXT("Right-click to use or equip."), 9, ColDim);
}

void SHLGameUI::DrawQuestLog() const
{
	FHLGame& Gm = G();
	if (!Win(TEXT("quests"), FString::Printf(TEXT("Quest Log (%d/20)"), Gm.Player.Quests.Num()), 560, 430, bQuests, 420, 150)) return;
	if (!Gm.Player.Quests.Num()) { Tx(WX, WY, TEXT("No quests. Look for a gold ! over someone's head."), 11, ColDim); return; }
	SelQuest = FMath::Clamp(SelQuest, 0, Gm.Player.Quests.Num() - 1);
	const FHLUnit& U = *Gm.PlayerUnit();
	for (int32 i = 0; i < Gm.Player.Quests.Num(); ++i)
	{
		const FHLQuestState& QS = Gm.Player.Quests[i];
		const float Y = WY + i * 18;
		if (i == SelQuest) Rc(WX - 4, Y - 1, 200, 17, FLinearColor(0.3f, 0.25f, 0.1f));
		const int32 Diff = QS.Q->Level - U.Level;
		Tx(WX, Y, FString::Printf(TEXT("[%d%s] %s"), QS.Q->Level, QS.Q->bDungeon ? TEXT("D") : QS.Q->bGroup ? TEXT("+") : TEXT(""), *QS.Q->Name), 10, QS.bComplete ? ColGreen : Diff >= 3 ? ColRed : Diff >= -2 ? FLinearColor(1, 1, 0.2f) : ColDim);
		if (Hover(WX - 4, Y - 1, 200, 17) && bLClick) { bLClick = false; SelQuest = i; }
	}
	const FHLQuestState& QS = Gm.Player.Quests[SelQuest];
	const float X = WX + 212;
	float Y = WY;
	Tx(X, Y, QS.Q->Name, 13, ColGold, true); Y += 24;
	Y += TxWrap(X, Y, 320, QS.Q->Text, 10, ColWhite) + 8;
	for (int32 i = 0; i < QS.Q->Objectives.Num(); ++i) { const FHLObjective& O = QS.Q->Objectives[i]; Tx(X, Y, FString::Printf(TEXT("- %s: %d/%d"), *O.Label, QS.Progress[i], O.Count), 10, QS.Progress[i] >= O.Count ? ColDim : ColWhite); Y += 15; }
	Y += 6;
	Tx(X, Y, FString::Printf(TEXT("Rewards: %d XP, %s"), QS.Q->Xp, *HLMoney(QS.Q->Cents)), 10, ColGold); Y += 18;
	for (int32 i = 0; i < QS.Q->Choice.Num(); ++i) ItemIcon(X + i * 40, Y, 36, QS.Q->Choice[i]);
	if (Btn(X + 220, WY + 360, 100, 24, TEXT("Abandon"), true, 10)) { Gm.AbandonQuest(QS.Q->Id); SelQuest = 0; }
}

void SHLGameUI::DrawSpellbook() const
{
	FHLGame& Gm = G();
	if (!Win(TEXT("spells"), TEXT("Abilities"), 470, 440, bSpells, 300, 160)) return;
	const FHLUnit& U = *Gm.PlayerUnit();
	float Y = WY;
	for (const FHLAbility* Ab : Gm.Player.Cls->Abilities)
	{
		const bool bKnown = Gm.Player.Known.Contains(Ab);
		Rc(WX, Y, 30, 30, bKnown ? FLinearColor(0.4f, 0.2f, 0.12f) : FLinearColor(0.12f, 0.12f, 0.12f));
		Img(WX + 1, Y + 1, 28, 28, HLSkillBrush(Ab->Icon), bKnown ? FLinearColor::White : FLinearColor(0.35f, 0.35f, 0.35f, 1));
		Tx(WX + 38, Y, Ab->Name, 11, bKnown ? ColGold : ColDim, true);
		Tx(WX + 38, Y + 15, bKnown ? Ab->Desc : (U.Level >= Ab->Level ? TEXT("Available at your trainer.") : FString::Printf(TEXT("Trainer: level %d"), Ab->Level)), 9, bKnown ? ColWhite : ColDim);
		if (Hover(WX, Y, 440, 30)) AbilityTip(*Ab);
		if (bKnown)
		{
			const int32 Slot = Gm.Player.Bar.IndexOfByKey(Ab);
			if (Slot == INDEX_NONE) { if (Btn(WX + 370, Y + 4, 70, 20, TEXT("Add to bar"), true, 9)) for (int32 i = 0; i < Gm.Player.Bar.Num(); ++i) if (!Gm.Player.Bar[i]) { Gm.Player.Bar[i] = Ab; break; } }
			else
			{
				Tx(WX + 352, Y + 8, FString::Printf(TEXT("key %s"), Slot == 9 ? TEXT("0") : Slot == 10 ? TEXT("-") : Slot == 11 ? TEXT("=") : *FString::FromInt(Slot + 1)), 9, ColDim);
				if (Slot > 0 && Btn(WX + 394, Y + 4, 20, 20, TEXT("<"), true, 9)) Gm.Player.Bar.Swap(Slot, Slot - 1);
				if (Slot < 11 && Btn(WX + 418, Y + 4, 20, 20, TEXT(">"), true, 9)) Gm.Player.Bar.Swap(Slot, Slot + 1);
			}
		}
		Y += 36;
	}
	Tx(WX, Y + 4, TEXT("11 further abilities and ranks are trained from level 14 to 60."), 9, ColDim);
}

void SHLGameUI::DrawMap() const
{
	FHLGame& Gm = G();
	const FHLData& D = FHLData::Get();
	if (!Win(TEXT("map"), bWorldMap ? TEXT("World Map: Hollywood under the Dome") : TEXT("Map: ") + Gm.Areas[Gm.PlayerUnit()->Area].Def->Name, 900, 640, bMap, (VS.X - 900) * 0.5f, 90)) return;
	const FHLUnit& P = *Gm.PlayerUnit();
	if (Btn(WX, WY - 4, 90, 20, TEXT("Zone"), true, 10)) bWorldMap = false;
	if (Btn(WX + 96, WY - 4, 90, 20, TEXT("World"), true, 10)) bWorldMap = true;
	const float Mx = WX, My = WY + 24, Mw = 880, Mh = 570;
	Rc(Mx, My, Mw, Mh, FLinearColor(0.13f, 0.12f, 0.1f));
	if (bWorldMap)
	{
		for (const FHLZoneInfo& Z : D.Zones)
		{
			const float X = Mx + Z.MapRect.X * Mw, Y = My + Z.MapRect.Y * Mh, W = Z.MapRect.Z * Mw, H = Z.MapRect.W * Mh;
			const int32 Diff = Z.LevelMin - P.Level;
			const FLinearColor C = Z.bBuilt ? FLinearColor(0.35f, 0.3f, 0.12f) : Diff > 8 ? FLinearColor(0.3f, 0.1f, 0.1f) : Diff > 2 ? FLinearColor(0.32f, 0.2f, 0.08f) : FLinearColor(0.15f, 0.25f, 0.12f);
			Rc(X, Y, W, H, FLinearColor(0.5f, 0.45f, 0.3f));
			++Layer; LastKind = 1;
			Rc(X + 1, Y + 1, W - 2, H - 2, C);
			TxC(X + W * 0.5f, Y + H * 0.5f - 14, Z.Name, 10, ColWhite, true);
			TxC(X + W * 0.5f, Y + H * 0.5f + 2, FString::Printf(TEXT("%d-%d"), Z.LevelMin, Z.LevelMax), 10, Diff > 8 ? ColRed : ColGold);
			if (Hover(X, Y, W, H))
			{
				FString Dn;
				for (const FHLDungeonInfo& Dg : D.Dungeons) if (Dg.Zone == Z.Id) Dn += FString::Printf(TEXT("\n%s (%d-%d, %d players)"), *Dg.Name, Dg.LevelMin, Dg.LevelMax, Dg.Size);
				TipText = FString::Printf(TEXT("%s (levels %d-%d)\n%s\nHubs: %s%s"), *Z.Name, Z.LevelMin, Z.LevelMax, *Z.Blurb, *FString::Join(Z.Hubs, TEXT(", ")), *Dn); TipPos = Mouse;
			}
			if (Z.bBuilt) TxC(X + W * 0.5f, Y + H - 16, TEXT("You are here"), 9, ColGreen);
		}
		Tx(Mx + 8, My + Mh - 20, TEXT("The dome's edge is the edge of the map. Pacific Electric red cars link the zones once you have discovered their stops."), 9, ColDim);
		return;
	}
	const FHLAreaDef& Def = *Gm.Areas[P.Area].Def;
	const float K = FMath::Min(Mw / Def.Size.X, Mh / Def.Size.Y);
	const float Ox = Mx + (Mw - Def.Size.X * K) * 0.5f, Oy = My + (Mh - Def.Size.Y * K) * 0.5f;
	auto MX = [&](float X) { return Ox + (X - Def.Min.X) * K; };
	auto MY = [&](float Y) { return Oy + (Y - Def.Min.Y) * K; };
	for (const FHLBox& B : Def.Ground) if (B.W > 3 && B.D > 3) Rc(MX(B.X), MY(B.Y), B.W * K, B.D * K, FLinearColor(0.22f, 0.22f, 0.22f));
	++Layer; LastKind = 1;
	for (const FHLBox& B : Def.Boxes) if (B.bSolid && B.H >= 1) Rc(MX(B.X), MY(B.Y), FMath::Max(1.f, B.W * K), FMath::Max(1.f, B.D * K), FLinearColor(B.Color.R * 0.85f, B.Color.G * 0.85f, B.Color.B * 0.85f));
	for (const FHLSubzone& S : Def.Subzones) if (Gm.Player.Discovered.Contains(S.Name) && S.Name != TEXT("The Hive Wall")) TxC(MX(S.X + S.W * 0.5f), MY(S.Y + S.D * 0.5f) - 6, S.Name, 9, FLinearColor(1, 0.95f, 0.75f, 0.9f));
	for (const FHLAidStation& A : Def.Aid) { Rc(MX(A.Pos.X) - 3, MY(A.Pos.Y) - 3, 7, 7, FLinearColor(0.9f, 0.1f, 0.1f)); }
	for (const FHLExit& E : Def.Exits) if (!E.bRedcar) { const FHLZoneInfo* Z = D.Zone(E.ToZone); TxC(FMath::Clamp(MX(E.Pos.X), Mx + 70.f, Mx + Mw - 70.f), FMath::Clamp(MY(E.Pos.Y), My + 4.f, My + Mh - 18.f), FString::Printf(TEXT("to %s (%d-%d)"), Z ? *Z->Name : TEXT("?"), Z ? Z->LevelMin : 0, Z ? Z->LevelMax : 0), 9, ColRed); }
	for (const FHLUnit* U : Gm.UnitList)
	{
		if (U->Area != P.Area || U->Kind != EHLKind::Npc || !U->Npc) continue;
		const int32 Mk = Gm.NpcMarker(*U->Npc);
		if (Mk == 1 || Mk == 2) TxC(MX(U->Pos.X), MY(U->Pos.Y) - 10, Mk == 1 ? TEXT("!") : TEXT("?"), 13, ColGold, true);
	}
	for (int32 SI : Gm.Party) if (const FHLUnit* U = Gm.Unit(Gm.Sims[SI].UnitId)) Rc(MX(U->Pos.X) - 2, MY(U->Pos.Y) - 2, 5, 5, FLinearColor(0.3f, 1, 1));
	Rc(MX(P.Pos.X) - 4, MY(P.Pos.Y) - 4, 8, 8, ColWhite);
	const float Rad = FMath::DegreesToRadians(P.Yaw);
	Line(FVector2D(MX(P.Pos.X), MY(P.Pos.Y)), FVector2D(MX(P.Pos.X), MY(P.Pos.Y)) + FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)) * 14, ColWhite, 2);
}

void SHLGameUI::DrawWho() const
{
	FHLGame& Gm = G();
	if (Gm.bWhoOpen && !bWho) bWho = true;
	const FHLGuild* MyG = Gm.PlayerGuild();
	const FString WhoTitle = WhoTab == 0 ? FString::Printf(TEXT("Who  -  %d players online"), Gm.OnlineCount()) : WhoTab == 1 ? FString::Printf(TEXT("Friends  -  %d"), Gm.Player.Friends.Num()) : WhoTab == 3 ? FString(TEXT("The Dome Herald  -  server news")) : MyG ? MyG->Name : FString(TEXT("Guild"));
	if (!Win(TEXT("who"), WhoTitle, 640, 470, bWho, 380, 140)) { Gm.bWhoOpen = false; return; }
	const FHLData& D = FHLData::Get();
	for (int32 t = 0; t < 4; ++t) { static const TCHAR* Tabs[] = { TEXT("Who"), TEXT("Friends"), TEXT("Guild"), TEXT("News") }; if (WhoTab == t) Rc(WX + 244 + t * 94 - 2, WY - 4, 90, 22, ColGold); if (Btn(WX + 244 + t * 94, WY - 2, 86, 18, Tabs[t], true, 9)) WhoTab = t; }
	if (WhoTab == 3)
	{
		// the server's history as a newspaper: headline, a line of copy, how long ago
		Tx(WX, WY + 26, TEXT("Thirty days of the dome, and more. Click a story to bring it up in General."), 9, ColDim);
		if (Wheel != 0 && Hover(WX, WY, 620, 440)) WhoScroll -= (int32)(Wheel * 2);
		const int32 PerPage = 9;
		WhoScroll = FMath::Clamp(WhoScroll, 0, FMath::Max(0, Gm.History.Num() - PerPage));
		for (int32 r = 0; r < PerPage && r + WhoScroll < Gm.History.Num(); ++r)
		{
			const FHLGame::FHLHistoryEvent& E = Gm.History[r + WhoScroll];
			const float Y = WY + 46 + r * 42;
			const bool bHot = Hover(WX - 4, Y - 2, 620, 40);
			if (bHot) Rc(WX - 4, Y - 2, 620, 40, FLinearColor(0.25f, 0.2f, 0.1f));
			Tx(WX, Y, E.Headline, 11, ColGold, true);
			Tx(WX + 610 - TW(E.Days == 1 ? TEXT("yesterday") : *FString::Printf(TEXT("%d days ago"), E.Days), 9), Y + 2, E.Days == 1 ? FString(TEXT("yesterday")) : FString::Printf(TEXT("%d days ago"), E.Days), 9, ColDim);
			FString Copy = E.Body;
			if (TW(Copy, 9) > 600) { while (Copy.Len() > 10 && TW(Copy + TEXT("..."), 9) > 600) Copy.LeftChopInline(1); Copy += TEXT("..."); }
			Tx(WX, Y + 18, Copy, 9, ColWhite);
			if (bHot) { TipText = E.Body; TipPos = Mouse; }
			if (bHot && bLClick) { bLClick = false; bTyping = true; ChatEntry = FString::Printf(TEXT("/1 anyone remember %s?"), *E.Ref); }
		}
		return;
	}
	if (WhoTab == 2)
	{
		if (!MyG) { TxWrap(WX, WY + 30, 600, TEXT("You are not in a guild. Guilds recruit in General chat, and someone may invite you once you have a few levels. Accepting puts their name over your head and opens guild chat (/g)."), 11, ColDim); return; }
		TArray<int32> M = Gm.GuildMembers(Gm.Player.Guild);
		int32 On = 0; for (int32 i : M) if (Gm.Sims[i].bOnline) ++On;
		M.Sort([&](int32 A, int32 B) { const FHLSim& Sa = Gm.Sims[A]; const FHLSim& Sb = Gm.Sims[B]; return Sa.bOnline != Sb.bOnline ? Sa.bOnline : Sa.Level != Sb.Level ? Sa.Level > Sb.Level : A < B; });
		Tx(WX, WY, FString::Printf(TEXT("%d members, %d online"), M.Num() + 1, On + 1), 10, ColDim);
		TxWrap(WX, WY + 18, 610, FString::Printf(TEXT("Message of the Day: %s"), *MyG->Motd), 10, FLinearColor(0.35f, 1, 0.35f));
		const float Top = WY + 52;
		Tx(WX, Top, TEXT("Name"), 10, ColGold); Tx(WX + 130, Top, TEXT("Rank"), 10, ColGold); Tx(WX + 230, Top, TEXT("Lvl"), 10, ColGold); Tx(WX + 270, Top, TEXT("Class"), 10, ColGold); Tx(WX + 360, Top, TEXT("Where"), 10, ColGold);
		const int32 PerPage = 19;
		if (Wheel != 0 && Hover(WX, WY, 620, 440)) WhoScroll -= (int32)(Wheel * 3);
		WhoScroll = FMath::Clamp(WhoScroll, 0, FMath::Max(0, M.Num() + 1 - PerPage));
		for (int32 r = 0; r < PerPage && r + WhoScroll < M.Num() + 1; ++r)
		{
			const int32 Row = r + WhoScroll;
			const float Y = Top + 18 + r * 17;
			if (Row == 0)
			{
				const FHLUnit* PU = Gm.PlayerUnit();
				Tx(WX, Y + 1, Gm.Player.Name, 10, Gm.Player.Cls ? Gm.Player.Cls->Color * 1.4f : ColWhite); Tx(WX + 130, Y + 1, TEXT("Initiate"), 10, ColDim);
				Tx(WX + 230, Y + 1, FString::FromInt(PU ? PU->Level : 1), 10, ColWhite); Tx(WX + 270, Y + 1, Gm.Player.Cls ? Gm.Player.Cls->Name : FString(), 10, ColWhite);
				Tx(WX + 360, Y + 1, Gm.CurSubzone, 10, ColDim);
				continue;
			}
			const int32 SI = M[Row - 1];
			const FHLSim& S = Gm.Sims[SI];
			const bool bHot = S.bOnline && Hover(WX - 4, Y, 620, 17);
			if (bHot) { Rc(WX - 4, Y, 620, 17, FLinearColor(0.25f, 0.2f, 0.1f)); TipText = FString::Printf(TEXT("%s\n%s\nClick to whisper. Right-click to invite."), *S.P->Handle, *Gm.SimActivity(S)); TipPos = Mouse; }
			const FLinearColor Cc = S.bOnline ? (S.P->Cls ? S.P->Cls->Color * 1.4f : ColWhite) : ColDim;
			Tx(WX, Y + 1, S.P->Handle, 10, Cc); Tx(WX + 130, Y + 1, Gm.GuildRank(SI), 10, S.bOnline ? ColWhite : ColDim);
			Tx(WX + 230, Y + 1, FString::FromInt(S.Level), 10, S.bOnline ? ColWhite : ColDim); Tx(WX + 270, Y + 1, S.P->Cls ? S.P->Cls->Name : FString(), 10, S.bOnline ? ColWhite : ColDim);
			const FHLZoneInfo* Z = D.Zone(S.UnitId ? TEXT("zone_hollywood_blvd") : S.Zone);
			Tx(WX + 360, Y + 1, S.bOnline ? (S.DungeonUntil > Gm.Now ? FString(TEXT("The Egyptian Theatre")) : Z ? Z->Name : FString()) : FString(TEXT("Offline")), 10, ColDim);
			if (bHot && bLClick) { bLClick = false; bTyping = true; ChatEntry = FString::Printf(TEXT("/w %s "), *S.P->Handle); }
			if (bHot && bRClick) { bRClick = false; Gm.InviteSim(SI); }
		}
		if (Btn(WX + 500, WY + 418, 110, 22, TEXT("Leave guild"), true, 9)) Gm.LeaveGuild();
		if (Btn(WX + 380, WY + 418, 110, 22, TEXT("Guild chat"), true, 9)) { bTyping = true; ChatEntry = TEXT("/g "); }
		return;
	}
	if (WhoTab == 1)
	{
		TArray<int32> Fr;
		for (int32 i = 0; i < Gm.Sims.Num(); ++i) if (Gm.IsFriend(Gm.Sims[i])) Fr.Add(i);
		Fr.Sort([&](int32 A, int32 B) { return Gm.Sims[A].bOnline != Gm.Sims[B].bOnline ? Gm.Sims[A].bOnline : Gm.Sims[A].P->Handle < Gm.Sims[B].P->Handle; });
		Tx(WX, WY, Fr.Num() ? TEXT("Target someone and press Add friend, or type /friend <name>.") : TEXT("No friends yet. Group with people, and target them to add them."), 10, ColDim);
		for (int32 r = 0; r < Fr.Num() && r < 20; ++r)
		{
			const FHLSim& S = Gm.Sims[Fr[r]];
			const float Y = WY + 24 + r * 21;
			const FHLRelation* Rl = Gm.Player.Relations.Find(S.P->Id);
			Tx(WX, Y + 2, S.P->Handle, 11, S.bOnline ? (S.P->Cls ? S.P->Cls->Color * 1.4f : ColWhite) : ColDim, true);
			Tx(WX + 130, Y + 3, S.bOnline ? FString::Printf(TEXT("Level %d %s"), S.Level, S.P->Cls ? *S.P->Cls->Name : TEXT("")) : FString(TEXT("Offline")), 10, S.bOnline ? ColWhite : ColDim);
			if (S.bOnline) Tx(WX + 270, Y + 3, Gm.SimActivity(S).Left(34), 9, ColDim);
			if (Rl && !Rl->Bond.IsEmpty() && Rl->Bond != TEXT("pal") && Rl->Bond != TEXT("gossip")) Tx(WX + 270 + (S.bOnline ? 0 : 0), Y + 12, Rl->Bond == TEXT("rival") ? TEXT("rival") : Rl->Bond == TEXT("mentor") ? TEXT("mentor") : TEXT(""), 8, ColGold);
			if (S.bOnline && Btn(WX + 470, Y, 50, 18, TEXT("/w"), true, 9)) { bTyping = true; ChatEntry = FString::Printf(TEXT("/w %s "), *S.P->Handle); }
			if (S.bOnline && Btn(WX + 524, Y, 50, 18, TEXT("Invite"), !S.bInParty, 9)) Gm.InviteSim(Fr[r]);
			if (Btn(WX + 578, Y, 40, 18, TEXT("x"), true, 9)) { Gm.RemoveFriend(Fr[r]); break; }
		}
		return;
	}
	TArray<int32> Rows;
	const FString Q = Gm.WhoQuery.ToLower();
	for (int32 i = 0; i < Gm.Sims.Num(); ++i)
	{
		const FHLSim& S = Gm.Sims[i];
		if (!S.bOnline) continue;
		if (!Q.IsEmpty())
		{
			const FHLZoneInfo* Z = D.Zone(S.Zone);
			const FString Hay = (S.P->Handle + TEXT(" ") + S.P->Class + TEXT(" ") + (Z ? Z->Name : FString()) + TEXT(" ") + FString::FromInt(S.Level)).ToLower();
			if (!Hay.Contains(Q)) continue;
		}
		Rows.Add(i);
	}
	Rows.Sort([&](int32 A, int32 B) { return Gm.Sims[A].Level != Gm.Sims[B].Level ? Gm.Sims[A].Level > Gm.Sims[B].Level : A < B; });
	Tx(WX, WY, Q.IsEmpty() ? FString::Printf(TEXT("%d shown. Filter with /who <text>  (name, class, zone or level)"), Rows.Num()) : FString::Printf(TEXT("%d match \"%s\""), Rows.Num(), *Gm.WhoQuery), 10, ColDim);
	if (!Q.IsEmpty() && Btn(WX + 330, WY - 2, 70, 18, TEXT("Clear"), true, 9)) Gm.WhoQuery.Reset();
	const int32 PerPage = 22;
	if (Wheel != 0 && Hover(WX, WY, 620, 440)) WhoScroll -= (int32)(Wheel * 3);
	WhoScroll = FMath::Clamp(WhoScroll, 0, FMath::Max(0, Rows.Num() - PerPage));
	Tx(WX, WY + 18, TEXT("Name"), 10, ColGold); Tx(WX + 130, WY + 18, TEXT("Lvl"), 10, ColGold); Tx(WX + 170, WY + 18, TEXT("Class"), 10, ColGold); Tx(WX + 260, WY + 18, TEXT("Guild"), 10, ColGold); Tx(WX + 420, WY + 18, TEXT("Zone"), 10, ColGold);
	for (int32 r = 0; r < PerPage && r + WhoScroll < Rows.Num(); ++r)
	{
		const int32 SI = Rows[r + WhoScroll];
		const FHLSim& S = Gm.Sims[SI];
		const float Y = WY + 36 + r * 17;
		const bool bHot = Hover(WX - 4, Y, 620, 17);
		if (bHot) { Rc(WX - 4, Y, 620, 17, FLinearColor(0.25f, 0.2f, 0.1f)); TipText = FString::Printf(TEXT("%s\n%s\nClick to whisper. Right-click to invite."), *S.P->Handle, *Gm.SimActivity(S)); TipPos = Mouse; }
		const FHLGuild* Gd = D.Guild(S.P->Guild);
		const FHLZoneInfo* Z = D.Zone(S.UnitId ? TEXT("zone_hollywood_blvd") : S.Zone);
		Tx(WX, Y + 1, S.P->Handle + (Gm.Player.bShowSim ? TEXT(" <AI>") : TEXT("")), 10, S.P->Cls ? S.P->Cls->Color * 1.4f : ColWhite);
		Tx(WX + 130, Y + 1, FString::FromInt(S.Level), 10, ColWhite); Tx(WX + 170, Y + 1, S.P->Cls ? S.P->Cls->Name : FString(), 10, ColWhite);
		Tx(WX + 260, Y + 1, Gd ? Gd->Name : FString(), 10, ColDim); Tx(WX + 420, Y + 1, Z ? Z->Name : FString(), 10, ColDim);
		if (bHot && bLClick) { bLClick = false; bTyping = true; ChatEntry = FString::Printf(TEXT("/w %s "), *S.P->Handle); }
		if (bHot && bRClick) { bRClick = false; Gm.InviteSim(SI); }
	}
}

void SHLGameUI::DrawInspect() const
{
	FHLGame& Gm = G();
	if (!Gm.Sims.IsValidIndex(InspectSim)) { bInspect = false; return; }
	const FHLSim& S = Gm.Sims[InspectSim];
	const FHLData& D = FHLData::Get();
	if (!Win(TEXT("inspect"), FString::Printf(TEXT("%s  -  Level %d %s"), *S.P->Handle, S.Level, S.P->Cls ? *S.P->Cls->Name : TEXT("")), 320, 420, bInspect, 520, 130)) return;
	if (const FHLGuild* Gd = D.Guild(S.P->Guild)) Tx(WX, WY, FString::Printf(TEXT("<%s>"), *Gd->Name), 10, ColGold);
	for (int32 i = 0; i < 12; ++i)
	{
		const float Y = WY + 20 + i * 28;
		const FHLItem* It = S.Gear.FindRef(EquipSlots[i]);
		ItemIcon(WX, Y, 26, It);
		Tx(WX + 32, Y + 5, It ? It->Name : FString(EquipNames[i]), 10, It ? HLQualityColor(It->Q) : ColDim);
	}
	if (!S.P->Mount.IsEmpty()) Tx(WX, WY + 20 + 12 * 28, FString::Printf(TEXT("Mount: %s"), *S.P->Mount), 10, ColDim);
}

// One line per modifier, for the given number of ranks.
static FString TalentEffect(const FHLTalent& T, int32 Rank)
{
	const FHLData& D = FHLData::Get();
	TArray<FString> Out;
	for (const auto& M : T.Mods)
	{
		const float V = M.Value * Rank;
		const FString P = FString::Printf(TEXT("%g%%"), FMath::RoundToFloat(FMath::Abs(V) * 1000.f) / 10.f);
		const FString& K = M.Key;
		auto AbName = [&](const FString& Id) { const FHLAbility* Ab = D.Ability(Id); return Ab ? Ab->Name : Id; };
		if (K.StartsWith(TEXT("ab:")))
		{
			const FHLAbility* Ab = D.Ability(K.Mid(3));
			const TCHAR* What = !Ab ? TEXT("effect") : Ab->HasEffect(TEXT("shield")) ? TEXT("absorption") : (Ab->HasEffect(TEXT("heal")) || Ab->HasEffect(TEXT("hot"))) ? TEXT("healing") : TEXT("damage");
			Out.Add(FString::Printf(TEXT("%s %s +%s."), *AbName(K.Mid(3)), What, *P));
		}
		else if (K.StartsWith(TEXT("cost:"))) Out.Add(FString::Printf(TEXT("%s costs %s less."), *AbName(K.Mid(5)), *P));
		else if (K.StartsWith(TEXT("cd:"))) Out.Add(FString::Printf(TEXT("%s cooldown %g s shorter."), *AbName(K.Mid(3)), FMath::RoundToFloat(V * 10.f) / 10.f));
		else
		{
			static const TMap<FString, FString> Names = {
				{ TEXT("str_pct"), TEXT("Strength") }, { TEXT("agi_pct"), TEXT("Agility") }, { TEXT("sta_pct"), TEXT("Stamina") }, { TEXT("int_pct"), TEXT("Intellect") },
				{ TEXT("spi_pct"), TEXT("Spirit") }, { TEXT("hp_pct"), TEXT("Maximum health") }, { TEXT("res_pct"), TEXT("Maximum Power") }, { TEXT("armor_pct"), TEXT("Armour") },
				{ TEXT("crit"), TEXT("Critical strike chance") }, { TEXT("dodge"), TEXT("Dodge chance") }, { TEXT("parry"), TEXT("Parry chance") }, { TEXT("block"), TEXT("Block chance") },
				{ TEXT("dmg_pct"), TEXT("All damage done") }, { TEXT("heal_pct"), TEXT("Healing done") }, { TEXT("threat_pct"), TEXT("Threat caused") },
				{ TEXT("speed_pct"), TEXT("Run speed") }, { TEXT("regen_pct"), TEXT("Resource regeneration") } };
			if (K == TEXT("taken_pct")) Out.Add(FString::Printf(TEXT("Damage taken -%s."), *P));
			else if (const FString* N = Names.Find(K)) Out.Add(FString::Printf(TEXT("%s +%s."), **N, *P));
		}
	}
	return FString::Join(Out, TEXT(" "));
}

void SHLGameUI::DrawTalents() const
{
	FHLGame& Gm = G();
	const float TW3 = 228, WW = TW3 * 3 + 26, WH = 446;
	if (!Win(TEXT("talents"), TEXT("Talents"), WW, WH, bTalents, 380, 120)) return;
	const FHLUnit& U = *Gm.PlayerUnit();
	const FHLClass& C = *Gm.Player.Cls;
	const int32 Points = Gm.TalentPoints();
	Tx(WX, WY, U.Level < 10 ? TEXT("Talents open at level 10: one point per level, 51 by level 60.") : FString::Printf(TEXT("Unspent talent points: %d"), Points), 11, U.Level < 10 || Points == 0 ? ColDim : ColGreen, true);
	const float S = 40, GX = 52, GY = 60;
	const FHLTalent* Hot = nullptr;
	for (int32 t = 0; t < C.Specs.Num() && t < 3; ++t)
	{
		const float X0 = WX + t * TW3, Y0 = WY + 24;
		const int32 Spent = Gm.TalentSpent(t);
		// tree backdrop: the spec's icon, dimmed and stretched
		Rc(X0, Y0, TW3 - 8, WH - 92, FLinearColor(0.02f, 0.02f, 0.025f));
		if (C.SpecIcons.IsValidIndex(t)) Img(X0, Y0 + 40, TW3 - 8, TW3 - 8, HLSkillBrush(C.SpecIcons[t]), FLinearColor(0.22f, 0.2f, 0.18f, 0.55f));
		if (C.SpecIcons.IsValidIndex(t)) Img(X0 + 6, Y0 + 6, 26, 26, HLSkillBrush(C.SpecIcons[t]));
		Tx(X0 + 38, Y0 + 4, C.Specs[t], 16, ColGold, true);
		Tx(X0 + 40, Y0 + 24, FString::Printf(TEXT("%d point%s"), Spent, Spent == 1 ? TEXT("") : TEXT("s")), 9, ColDim);
		if (Hover(X0, Y0, TW3 - 8, 36) && C.SpecDesc.IsValidIndex(t) && !C.SpecDesc[t].IsEmpty()) { TipText = C.Specs[t] + TEXT("\n") + C.SpecDesc[t]; TipPos = Mouse; }
		for (const FHLTalent* T : C.Talents)
		{
			if (T->Spec != t) continue;
			const float X = X0 + 14 + T->Col * GX, Y = Y0 + 46 + (T->Tier - 1) * GY;
			const int32 Rank = Gm.Player.Talents.FindRef(T->Id);
			const bool bOpen = Spent >= (T->Tier - 1) * 5 && U.Level >= 10;
			const bool bCan = Gm.CanLearnTalent(*T).IsEmpty();
			const FLinearColor Edge = Rank >= T->Ranks ? FLinearColor(1, 0.8f, 0.3f) : Rank > 0 || bCan ? FLinearColor(0.35f, 0.9f, 0.35f) : FLinearColor(0.3f, 0.3f, 0.3f);
			Rc(X - 2, Y - 2, S + 4, S + 4, Edge);
			++Layer; LastKind = 1;
			Rc(X, Y, S, S, FLinearColor(0.05f, 0.05f, 0.05f));
			const float Lum = Rank > 0 || bCan ? 1.f : bOpen ? 0.55f : 0.3f;
			Img(X, Y, S, S, HLSkillBrush(T->Icon), FLinearColor(Lum, Lum, Lum, 1));
			const FString R = FString::Printf(TEXT("%d/%d"), Rank, T->Ranks);
			++Layer; LastKind = 1;
			Rc(X + S - TW(R, 8) - 5, Y + S - 12, TW(R, 8) + 6, 13, FLinearColor(0, 0, 0, 0.85f));
			Tx(X + S - TW(R, 8) - 2, Y + S - 13, R, 8, Rank >= T->Ranks ? ColGold : Rank > 0 || bCan ? ColGreen : ColDim);
			if (Hover(X, Y, S, S))
			{
				Hot = T;
				if (bLClick && bCan) { bLClick = false; Gm.LearnTalent(*T); }
			}
		}
	}
	if (Hot)
	{
		const int32 Rank = Gm.Player.Talents.FindRef(Hot->Id);
		FString Tip = Hot->Name + TEXT("\n") + FString::Printf(TEXT("Rank %d/%d"), Rank, Hot->Ranks);
		if (Rank > 0) Tip += TEXT("\n") + TalentEffect(*Hot, Rank);
		if (Rank < Hot->Ranks) Tip += (Rank > 0 ? TEXT("\nNext rank: ") : TEXT("\n")) + TalentEffect(*Hot, Rank + 1);
		const int32 Need = (Hot->Tier - 1) * 5;
		if (Gm.TalentSpent(Hot->Spec) < Need) Tip += FString::Printf(TEXT("\nRequires %d points in %s"), Need, *C.Specs[Hot->Spec]);
		else if (Rank < Hot->Ranks && Gm.CanLearnTalent(*Hot).IsEmpty()) Tip += TEXT("\nClick to learn");
		if (!Hot->Desc.IsEmpty()) Tip += TEXT("\n") + Hot->Desc;
		TipText = Tip; TipPos = Mouse;
	}
	Tx(WX, WY + WH - 60, FString::Printf(TEXT("Your class trainer can unlearn all talents for %s (doubling each time, to $50)."), *HLMoney(Gm.RespecCost())), 9, ColDim);
}

void SHLGameUI::DrawMenu() const
{
	FHLGame& Gm = G();
	if (!Win(TEXT("menu"), TEXT("Game Menu"), 330, 350, bMenu, (VS.X - 330) * 0.5f, 240)) return;
	if (Btn(WX, WY, 300, 26, Gm.Player.bShowSim ? TEXT("Show simulated players: ON (tags every AI player)") : TEXT("Show simulated players: OFF"), true, 10)) Gm.Player.bShowSim = !Gm.Player.bShowSim;
	if (Btn(WX, WY + 34, 300, 26, TEXT("Save now"), true, 10)) { Gm.SaveNow(); Gm.System(TEXT("Game saved.")); }
	if (Btn(WX, WY + 68, 300, 26, TEXT("How to play (H)"), true, 10)) bHelp = true;
	if (Btn(WX, WY + 102, 300, 26, TEXT("Unstuck: return to the Chinese forecourt"), true, 10)) { if (FHLUnit* U = Gm.PlayerUnit()) if (!Gm.InCombat(*U)) Gm.TeleportUnit(*U, 0, Gm.Areas[0].Def->Spawn); }
	{
		const int32 Pct = FMath::RoundToInt(Gm.Music.GetVolume() * 100);
		if (Btn(WX, WY + 136, 300, 26, Pct ? FString::Printf(TEXT("Music volume: %d%%"), Pct) : FString(TEXT("Music: off")), true, 10))
			Gm.Music.SetVolume(Pct >= 100 ? 0.f : Pct < 25 ? 0.25f : FMath::Min(1.f, (Pct / 25 + 1) * 0.25f));
		const FString Now = Gm.Music.NowPlaying();
		if (!Now.IsEmpty()) Tx(WX, WY + 166, FString::Printf(TEXT("Now playing: %s"), *Now), 9, ColDim);
	}
	TxWrap(WX, WY + 186, 300, TEXT("Single-player. Every other player is simulated by AI; the server only writes their chat. Your character is saved in this browser and backed up to the server."), 9, ColDim);
	Tx(WX, WY + 238, TEXT("Cloud code (restore on another browser):"), 9, ColDim);
	Tx(WX, WY + 254, Gm.Player.PlayerId, 12, ColGold, true);
}

void SHLGameUI::DrawNpc() const
{
	FHLGame& Gm = G();
	const FHLData& D = FHLData::Get();
	const FHLUnit& P = *Gm.PlayerUnit();
	bool bOpen = true;
	if (Gm.OpenNpcMode == TEXT("redcar"))
	{
		if (!Win(TEXT("npc"), TEXT("Pacific Electric Railway: Route Map"), 520, 360, bOpen, 300, 140)) { Gm.OpenNpcMode.Reset(); return; }
		float Y = WY;
		for (const FHLRedcarLine& L : D.Redcar) { Tx(WX, Y, L.Name, 11, ColGold, true); Y += 17; Tx(WX + 12, Y, FString::Join(L.Stops, TEXT("  -  ")), 10, ColWhite); Y += 24; }
		Tx(WX, Y + 6, TEXT("Fare: 5c per stop."), 10, ColWhite);
		Tx(WX, Y + 24, TEXT("You have discovered: Hollywood Blvd (Highland)."), 10, ColGreen);
		Tx(WX, Y + 42, TEXT("You can only ride to stops you have already visited on foot."), 10, ColDim);
		return;
	}
	if (Gm.OpenNpcMode == TEXT("mail")) { DrawMail(); return; }
	const FHLUnit* N = Gm.Unit(Gm.OpenNpcUnitId);
	if (!N || !N->Npc) { if (Gm.OpenNpcUnitId) { Gm.OpenNpcUnitId = 0; Gm.OpenNpcMode.Reset(); } return; }
	const FHLNpcDef& Np = *N->Npc;
	const FString Title = Np.Title.IsEmpty() ? Np.Name : FString::Printf(TEXT("%s  -  %s"), *Np.Name, *Np.Title);
	auto Close = [&] { Gm.OpenNpcUnitId = 0; Gm.OpenNpcMode.Reset(); };

	if (Gm.OpenNpcMode == TEXT("gossip"))
	{
		if (!Win(TEXT("npc"), Title, 440, 400, bOpen, 300, 140)) { Close(); return; }
		float Y = WY;
		static const TCHAR* Greetings[] = { TEXT("What can I do for you?"), TEXT("Keep your voice down. They hear better than you'd think."), TEXT("Another day under the glass. What'll it be?"), TEXT("Make it quick, friend.") };
		Tx(WX, Y, Greetings[N->Id % 4], 11, ColWhite); Y += 26;
		for (const FHLQuest& Q : D.Quests)
		{
			const bool bReady = Gm.QuestReadyAt(Q, Np.Id);
			const bool bAvail = Q.Giver == Np.Id && Gm.QuestAvailable(Q);
			const bool bActive = !bReady && Gm.QuestState(Q.Id) && (Q.Giver == Np.Id || Q.Turnin == Np.Id);
			if (!bReady && !bAvail && !bActive) continue;
			const FString Lbl = FString::Printf(TEXT("%s  %s  [%d]"), bReady ? TEXT("?") : bAvail ? TEXT("!") : TEXT("..."), *Q.Name, Q.Level);
			if (Btn(WX, Y, 410, 24, Lbl, true, 10)) { Gm.OpenNpcMode = TEXT("quest:") + Q.Id; RewardChoice = -1; }
			Y += 30;
		}
		Y += 6;
		auto Service = [&](const TCHAR* Role, const TCHAR* Label, const TCHAR* Mode) { if (Np.HasRole(Role)) { if (Btn(WX, Y, 410, 24, Label, true, 10)) { Gm.OpenNpcMode = Mode; if (FCString::Strcmp(Mode, TEXT("vendor")) == 0 || FCString::Strcmp(Mode, TEXT("bank")) == 0 || FCString::Strcmp(Mode, TEXT("auction")) == 0) bBags = true; VendorScroll = 0; AuctionPage = 0; } Y += 30; } };
		if (Np.HasRole(TEXT("vendor")) && Gm.HubClosed(Np.Hub))
		{
			Tx(WX, Y + 4, FString::Printf(TEXT("\"We're boarded up after the surge. Come back in %d minutes.\""), FMath::Max(1, (int32)((Gm.HubClosedUntil[Np.Hub] - Gm.Now) / 60) + 1)), 10, ColDim); Y += 30;
		}
		else Service(TEXT("vendor"), TEXT("Let me browse your goods."), TEXT("vendor"));
		Service(TEXT("trainer"), TEXT("I'd like to train."), TEXT("trainer"));
		Service(TEXT("trainer_prof"), TEXT("Teach me a profession."), TEXT("prof"));
		Service(TEXT("banker"), TEXT("I'd like to check my deposit box."), TEXT("bank"));
		Service(TEXT("auctioneer"), TEXT("Show me the auction listings."), TEXT("auction"));
		Service(TEXT("redcar"), TEXT("Where do the red cars go?"), TEXT("redcar"));
		if (Np.HasRole(TEXT("innkeeper")))
				{
					if (Gm.Player.bInnHome) Tx(WX, Y + 5, TEXT("\"Your room's kept made up. Rest easy.\""), 10, ColDim);
					else if (Btn(WX, Y, 410, 24, TEXT("Make this inn my home."), true, 10)) { Gm.Player.bInnHome = true; Gm.System(TEXT("The Roosevelt Hotel is now your home. Logging out here earns rested experience three times faster.")); Gm.SaveNow(); }
					Y += 30;
				}
		if (Np.HasRole(TEXT("repair")))
		{
			const int32 Cost = Gm.RepairCost();
			if (Btn(WX, Y, 410, 24, Cost > 0 ? FString::Printf(TEXT("Repair my gear (%s)."), *HLMoney(Cost)) : FString(TEXT("Repair my gear.")), true, 10)) Gm.RepairAll();
			Y += 30;
		}
		return;
	}
	if (Gm.OpenNpcMode.StartsWith(TEXT("quest:")))
	{
		const FHLQuest* Q = D.Quest(Gm.OpenNpcMode.Mid(6));
		if (!Q) { Gm.OpenNpcMode = TEXT("gossip"); return; }
		if (!Win(TEXT("npc"), Title, 440, 470, bOpen, 300, 140)) { Close(); return; }
		const bool bReady = Gm.QuestReadyAt(*Q, Np.Id);
		const FHLQuestState* QS = Gm.QuestState(Q->Id);
		float Y = WY;
		Tx(WX, Y, Q->Name, 14, ColGold, true); Y += 26;
		Y += TxWrap(WX, Y, 415, bReady && !Q->CompleteText.IsEmpty() ? Q->CompleteText : Q->Text, 11, ColWhite) + 10;
		if (!bReady) for (int32 i = 0; i < Q->Objectives.Num(); ++i) { const FHLObjective& O = Q->Objectives[i]; Tx(WX, Y, QS ? FString::Printf(TEXT("- %s: %d/%d"), *O.Label, QS->Progress[i], O.Count) : (O.Count > 1 ? FString::Printf(TEXT("- %s: %d"), *O.Label, O.Count) : TEXT("- ") + O.Label), 10, ColDim); Y += 15; }
		Y += 8;
		Tx(WX, Y, FString::Printf(TEXT("Reward: %d experience, %s"), Q->Xp, *HLMoney(Q->Cents)), 10, ColGold); Y += 18;
		if (Q->Choice.Num())
		{
			Tx(WX, Y, bReady ? TEXT("Choose one:") : TEXT("You will be able to choose one of:"), 10, ColWhite); Y += 16;
			for (int32 i = 0; i < Q->Choice.Num(); ++i)
			{
				if (bReady && RewardChoice == i) Rc(WX + i * 46 - 3, Y - 3, 46, 46, ColGold);
				if (ItemIcon(WX + i * 46, Y, 40, Q->Choice[i]) && bReady) RewardChoice = i;
			}
			Y += 48;
		}
		if (Q->bGroup) { Tx(WX, Y, Q->bDungeon ? TEXT("Dungeon quest: bring a party of five.") : TEXT("Group quest: suggested players 3."), 10, ColRed); Y += 16; }
		const float By = WY + 400;
		if (bReady) { if (Btn(WX, By, 150, 28, TEXT("Complete Quest"), !Q->Choice.Num() || RewardChoice >= 0, 11)) { Gm.CompleteQuest(*Q, RewardChoice); Gm.OpenNpcMode = TEXT("gossip"); RewardChoice = -1; } }
		else if (!QS && Gm.QuestAvailable(*Q)) { if (Btn(WX, By, 150, 28, TEXT("Accept"), true, 11)) { Gm.AcceptQuest(*Q); Gm.OpenNpcMode = TEXT("gossip"); } }
		if (Btn(WX + 264, By, 150, 28, TEXT("Back"), true, 11)) Gm.OpenNpcMode = TEXT("gossip");
		return;
	}
	if (Gm.OpenNpcMode == TEXT("vendor"))
	{
		if (!Win(TEXT("npc"), Title, 440, 470, bOpen, 300, 140)) { Close(); return; }
		const int32 PerPage = 12;
		if (Wheel != 0 && Hover(WX, WY, 420, 430)) VendorScroll -= (int32)Wheel * 3;
		VendorScroll = FMath::Clamp(VendorScroll, 0, FMath::Max(0, Np.Stock.Num() - PerPage));
		for (int32 r = 0; r < PerPage && r + VendorScroll < Np.Stock.Num(); ++r)
		{
			const FHLItem* It = Np.Stock[r + VendorScroll];
			const float Y = WY + r * 32;
			const bool bCan = !It->IsEquip() || Gm.CanEquip(*It);
			ItemIcon(WX, Y, 28, It);
			Tx(WX + 34, Y + 1, It->Name, 10, bCan ? HLQualityColor(It->Q) : ColRed);
			Tx(WX + 34, Y + 14, It->IsEquip() ? FString::Printf(TEXT("level %d %s"), It->Req, *It->Type) : It->Buy > 1 ? FString::Printf(TEXT("box of %d"), It->Buy) : It->Type, 8, ColDim);
			const int32 Cost = FMath::Max(1, It->Price * 4);
			if (Btn(WX + 330, Y + 2, 84, 24, HLMoney(Cost), Gm.Player.Cents >= Cost, 10)) Gm.BuyItem(It);
		}
		Tx(WX, WY + 392, FString::Printf(TEXT("%d-%d of %d (mouse wheel scrolls)      You have %s"), VendorScroll + 1, FMath::Min(Np.Stock.Num(), VendorScroll + PerPage), Np.Stock.Num(), *HLMoney(Gm.Player.Cents)), 9, ColDim);
		if (Btn(WX + 264, WY + 408, 150, 22, TEXT("Back"), true, 10)) Gm.OpenNpcMode = TEXT("gossip");
		return;
	}
	if (Gm.OpenNpcMode == TEXT("trainer"))
	{
		if (!Win(TEXT("npc"), Title, 470, 470, bOpen, 300, 140)) { Close(); return; }
		const FHLClass* C = D.Class(Np.Trains);
		if (!C || C != Gm.Player.Cls) { TxWrap(WX, WY, 440, FString::Printf(TEXT("I train %s. You'll want to see your own trainer, along the forecourt here."), C ? *(C->Name + TEXT("s")) : TEXT("others")), 11, ColWhite); return; }
		float Y = WY;
		for (const FHLAbility* Ab : C->Abilities)
		{
			const bool bKnown = Gm.Player.Known.Contains(Ab), bLevel = P.Level >= Ab->Level;
			const int32 Cost = 10 * Ab->Level;
			Tx(WX, Y + 4, Ab->Name, 11, bKnown ? ColDim : bLevel ? ColGreen : ColRed, true);
			Tx(WX + 190, Y + 5, FString::Printf(TEXT("Level %d"), Ab->Level), 10, bLevel ? ColWhite : ColRed);
			if (Hover(WX, Y, 300, 24)) AbilityTip(*Ab);
			if (bKnown) Tx(WX + 340, Y + 5, TEXT("Known"), 10, ColDim);
			else if (Btn(WX + 320, Y + 1, 120, 22, FString::Printf(TEXT("Train  %s"), *HLMoney(Cost)), bLevel && Gm.Player.Cents >= Cost, 10)) Gm.LearnAbility(Ab, false);
			Y += 28;
		}
		static const int32 Later[] = { 14, 16, 18, 20, 24, 28, 32, 36, 40, 50, 60 };
		FString More;
		for (int32 L : Later) More += FString::Printf(TEXT("%d  "), L);
		Tx(WX, Y + 6, TEXT("Further abilities and ranks at levels: ") + More, 9, ColDim);
		if (Gm.TalentSpent() > 0 && Btn(WX, Y + 26, 220, 22, FString::Printf(TEXT("Unlearn talents  %s"), *HLMoney(Gm.RespecCost())), Gm.Player.Cents >= Gm.RespecCost(), 10)) Gm.ResetTalents();
		return;
	}
	if (Gm.OpenNpcMode == TEXT("prof") && Np.Prof == TEXT("guns"))
	{
		if (!Win(TEXT("npc"), Title, 440, 330, bOpen, 300, 140)) { Close(); return; }
		Tx(WX, WY, TEXT("Gunsmithing (crafting)"), 12, ColGold, true);
		TxWrap(WX, WY + 22, 410, TEXT("Load cartridges and build rough guns from Scrap Metal and Brass Casings. Salvage scrap from the wrecks in the back lots; looters carry casings. Earl sells both, at a price."), 10, ColWhite);
		const int32 Cost = 25;
		const bool bLevel = P.Level >= 5;
		if (Gm.Player.Guns > 0)
		{
			Tx(WX, WY + 84, FString::Printf(TEXT("You are an Apprentice Gunsmith (%d / 75)."), Gm.Player.Guns), 10, ColGreen);
			if (Btn(WX + 250, WY + 80, 150, 24, TEXT("Open workbench (K)"), true, 10)) bCraft = true;
		}
		else if (Btn(WX, WY + 80, 220, 26, FString::Printf(TEXT("Learn Gunsmithing  %s"), *HLMoney(Cost)), bLevel && Gm.Player.Cents >= Cost, 10))
		{
			Gm.Player.Cents -= Cost; Gm.Player.Guns = 1; bCraft = true;
			Gm.System(TEXT("You have learned Gunsmithing. Press K to open your workbench."));
		}
		if (!bLevel && Gm.Player.Guns <= 0) Tx(WX + 230, WY + 86, TEXT("Requires level 5"), 10, ColRed);
		float Y = WY + 120;
		Tx(WX, Y, TEXT("Recipes"), 11, ColGold, true); Y += 20;
		for (const FHLGame::FHLRecipe& R : FHLGame::GunRecipes())
		{
			Tx(WX + 8, Y, R.Product, 10, ColWhite);
			Tx(WX + 300, Y, FString::Printf(TEXT("Skill %d"), R.Skill), 10, Gm.Player.Guns >= R.Skill ? ColDim : ColRed);
			Y += 16;
		}
		return;
	}
	if (Gm.OpenNpcMode == TEXT("prof"))
	{
		if (!Win(TEXT("npc"), Title, 440, 300, bOpen, 300, 140)) { Close(); return; }
		Tx(WX, WY, TEXT("Salvaging (gathering)"), 12, ColGold, true);
		TxWrap(WX, WY + 22, 410, TEXT("Strip scrap metal from wrecks, and chitin and glands from dead invaders. Used by Gunsmithing, Metalworking and Pharmacy."), 10, ColWhite);
		if (Gm.Player.Salvage > 0) Tx(WX, WY + 70, FString::Printf(TEXT("You are an Apprentice Salvager (%d / 75)."), Gm.Player.Salvage), 10, ColGreen);
		else if (Btn(WX, WY + 70, 200, 26, TEXT("Learn Salvaging (free)"), true, 10)) { Gm.Player.Salvage = 1; Gm.System(TEXT("You have learned Salvaging. Look for wreckage in the back lots.")); }
		Tx(WX, WY + 120, TEXT("Other professions (trainers elsewhere under the dome):"), 10, ColDim);
		TxWrap(WX, WY + 138, 410, TEXT("Foraging, Skinning, Gunsmithing, Metalworking, Leatherworking, Tailoring, Pharmacy, Electrics, Cooking, First Aid, Fishing."), 10, ColDim);
		return;
	}
	if (Gm.OpenNpcMode == TEXT("bank"))
	{
		const int32 Rows = (Gm.Player.Bank.Num() + 5) / 6;
		if (!Win(TEXT("npc"), Title, 290, 90 + Rows * 42, bOpen, 300, 140)) { Close(); return; }
		for (int32 i = 0; i < Gm.Player.Bank.Num(); ++i)
		{
			FHLInvSlot& S = Gm.Player.Bank[i];
			if (ItemIcon(WX + (i % 6) * 42, WY + (i / 6) * 42, 40, S.Item, S.Count) && S.Item) { if (Gm.AddItem(S.Item, S.Count, false)) S = FHLInvSlot(); }
		}
		const float By = WY + Rows * 42 + 6;
		Tx(WX, By, TEXT("Click to withdraw. Right-click bag items to deposit."), 9, ColDim);
		if (const int32 Cost = FHLGame::BankBoxCost(Gm.Player.BankBoxes))
		{
			if (Btn(WX, By + 16, 270, 22, FString::Printf(TEXT("Rent another box (6 slots)  %s"), *HLMoney(Cost)), Gm.Player.Cents >= Cost, 9)) Gm.BuyBankBox();
		}
		else Tx(WX, By + 20, TEXT("You rent every box the bank has."), 9, ColDim);
		return;
	}
	if (Gm.OpenNpcMode == TEXT("auction"))
	{
		if (!Win(TEXT("npc"), TEXT("Auction House  -  Taft Building"), 700, 500, bOpen, 240, 110)) { Close(); return; }
		static const TCHAR* Filters[] = { TEXT("All"), TEXT("Usable"), TEXT("Weapons"), TEXT("Armour"), TEXT("Goods"), TEXT("Mine") };
		for (int32 f = 0; f < 6; ++f) { if (f == AuctionFilter) Rc(WX + f * 70 - 2, WY - 4, 68, 24, ColGold); if (Btn(WX + f * 70, WY - 2, 64, 20, Filters[f], true, 9)) { AuctionFilter = f; AuctionPage = 0; } }
		// search box
		{
			const float Sx = WX + 430, Sw = 240;
			if (Hover(Sx, WY - 2, Sw, 20) && bLClick) { bLClick = false; bSearchFocus = true; }
			else if (bLClick && bSearchFocus && !Hover(Sx, WY - 2, Sw, 20)) bSearchFocus = false;
			Rc(Sx, WY - 2, Sw, 20, bSearchFocus ? FLinearColor(0.85f, 0.7f, 0.35f) : FLinearColor(0.35f, 0.3f, 0.2f));
			++Layer; LastKind = 1;
			Rc(Sx + 1, WY - 1, Sw - 2, 18, FLinearColor(0.04f, 0.04f, 0.04f));
			const bool bCaret = bSearchFocus && FMath::Fmod(Gm.Now, 1.0) < 0.5;
			Tx(Sx + 6, WY, AuctionSearch.IsEmpty() && !bSearchFocus ? FString(TEXT("Search by name...")) : AuctionSearch + (bCaret ? TEXT("|") : TEXT("")), 10, AuctionSearch.IsEmpty() && !bSearchFocus ? ColDim : ColWhite);
			if (!AuctionSearch.IsEmpty() && Btn(Sx + Sw - 20, WY, 18, 16, TEXT("x"), true, 8)) { AuctionSearch.Reset(); AuctionPage = 0; }
		}
		const FString Needle = AuctionSearch.ToLower();
		TArray<int32> Rows;
		for (int32 i = 0; i < Gm.Auctions.Num(); ++i)
		{
			const FHLAuction& A = Gm.Auctions[i];
			if (AuctionFilter == 1 && !(A.Item->IsEquip() && Gm.CanEquip(*A.Item))) continue;
			if (AuctionFilter == 2 && A.Item->Kind != TEXT("weapon")) continue;
			if (AuctionFilter == 3 && A.Item->Kind != TEXT("armor")) continue;
			if (AuctionFilter == 4 && A.Item->IsEquip()) continue;
			if (AuctionFilter == 5 && !A.bMine && !A.bPlayerBid) continue;
			if (!Needle.IsEmpty() && !A.Item->Name.ToLower().Contains(Needle)) continue;
			Rows.Add(i);
		}
		Rows.Sort([&](int32 A, int32 B) { return Gm.Auctions[A].Item->Req != Gm.Auctions[B].Item->Req ? Gm.Auctions[A].Item->Req < Gm.Auctions[B].Item->Req : A < B; });
		const int32 PerPage = 13, Pages = FMath::Max(1, (Rows.Num() + PerPage - 1) / PerPage);
		if (Wheel != 0 && Hover(WX, WY, 680, 460)) AuctionPage -= (int32)FMath::Sign(Wheel);
		AuctionPage = FMath::Clamp(AuctionPage, 0, Pages - 1);
		Tx(WX + 34, WY + 26, TEXT("Item"), 10, ColGold); Tx(WX + 290, WY + 26, TEXT("Lvl"), 10, ColGold); Tx(WX + 322, WY + 26, TEXT("Seller"), 10, ColGold); Tx(WX + 422, WY + 26, TEXT("Bid"), 10, ColGold); Tx(WX + 498, WY + 26, TEXT("Buyout"), 10, ColGold);
		for (int32 r = 0; r < PerPage && r + AuctionPage * PerPage < Rows.Num(); ++r)
		{
			const int32 Idx = Rows[r + AuctionPage * PerPage];
			const FHLAuction& A = Gm.Auctions[Idx];
			const float Y = WY + 44 + r * 30;
			ItemIcon(WX, Y, 27, A.Item, A.Count);
			Tx(WX + 34, Y + 2, A.Item->Name.Left(34), 10, HLQualityColor(A.Item->Q));
			{
				int32 Avg = 0, Lo = 0, Hi = 0, Ns = 0;
				if (Gm.PriceStats(*A.Item, Avg, Lo, Hi, Ns))
				{
					const int32 Each = A.Buyout / FMath::Max(1, A.Count);
					const FLinearColor Pc = Each <= Avg * 0.85f ? ColGreen : Each >= Avg * 1.3f ? FLinearColor(1, 0.45f, 0.35f) : ColDim;
					Tx(WX + 34, Y + 16, FString::Printf(TEXT("recent sales avg %s%s"), *HLMoney(Avg), A.Count > 1 ? TEXT(" each") : TEXT("")), 8, Pc);
					if (Hover(WX + 34, Y, 250, 28)) { TipText = FString::Printf(TEXT("%s\nLast %d sales: average %s, low %s, high %s%s.\nThis buyout: %s%s."), *A.Item->Name, Ns, *HLMoney(Avg), *HLMoney(Lo), *HLMoney(Hi), A.Count > 1 ? TEXT(" each") : TEXT(""), *HLMoney(Each), A.Count > 1 ? TEXT(" each") : TEXT("")); TipPos = Mouse; }
				}
			}
			Tx(WX + 290, Y + 5, A.Item->Req > 0 ? FString::FromInt(A.Item->Req) : TEXT("-"), 10, A.Item->Req > P.Level ? ColRed : ColWhite);
			Tx(WX + 322, Y + 5, A.bMine ? Gm.Player.Name : Gm.Sims.IsValidIndex(A.SellerIdx) ? Gm.Sims[A.SellerIdx].P->Handle.Left(14) : FString(TEXT("?")), 10, ColDim);
			Tx(WX + 422, Y + 5, A.Bid > 0 ? HLMoney(A.Bid) : FString(TEXT("-")), 9, A.bPlayerBid ? ColGreen : ColDim);
			Tx(WX + 498, Y + 5, HLMoney(A.Buyout), 10, Gm.Player.Cents >= A.Buyout ? ColWhite : ColRed);
			if (A.bMine) { if (Btn(WX + 590, Y + 2, 84, 22, TEXT("Cancel"), A.Bid == 0, 10)) { Gm.BuyAuction(Idx); break; } }
			else
			{
				const int32 Nb = Gm.NextBid(A);
				if (Btn(WX + 572, Y + 2, 48, 22, A.bPlayerBid ? TEXT("High") : TEXT("Bid"), !A.bPlayerBid && Gm.Player.Cents >= Nb, 9)) { Gm.BidAuction(Idx); break; }
				if (Hover(WX + 572, Y + 2, 48, 22) && !A.bPlayerBid) { TipText = FString::Printf(TEXT("Bid %s"), *HLMoney(Nb)); TipPos = Mouse; }
				if (Btn(WX + 624, Y + 2, 52, 22, TEXT("Buy"), Gm.Player.Cents >= A.Buyout, 9)) { Gm.BuyAuction(Idx); break; }
			}
		}
		Tx(WX, WY + 436, FString::Printf(TEXT("Page %d / %d   (%d auctions, mouse wheel pages)    You have %s."), AuctionPage + 1, Pages, Rows.Num(), *HLMoney(Gm.Player.Cents)), 9, ColDim);
		Tx(WX, WY + 454, TEXT("Sell: click a bag item. Duration:"), 9, ColDim);
		static const int32 Hrs[] = { 1, 4, 12 };
		for (int32 h = 0; h < 3; ++h) { if (AuctionHours == Hrs[h]) Rc(WX + 250 + h * 60 - 2, WY + 450, 56, 22, ColGold); if (Btn(WX + 250 + h * 60, WY + 452, 52, 18, FString::Printf(TEXT("%d h"), Hrs[h]), true, 9)) AuctionHours = Hrs[h]; }
		Tx(WX + 440, WY + 454, TEXT("Deposit 5% per hour, 5% cut on sale."), 9, ColDim);
		return;
	}
	Gm.OpenNpcMode = TEXT("gossip");
}

void SHLGameUI::DrawTrade() const
{
	FHLGame& Gm = G();
	if (!Gm.Sims.IsValidIndex(Gm.TradeSim)) { Gm.TradeSim = -1; return; }
	const FHLSim& S = Gm.Sims[Gm.TradeSim];
	const FHLUnit* U = Gm.Unit(S.UnitId);
	const FHLUnit* P = Gm.PlayerUnit();
	bool bOpen = true;
	if (!U || !P || Gm.Dist(*U, *P) > 14) { Gm.TradeSim = -1; Gm.System(TEXT("Trade cancelled.")); return; }
	if (!Win(TEXT("trade"), FString::Printf(TEXT("Trade  -  %s"), *S.P->Handle), 480, 300, bOpen, 360, 160) || !bOpen) { Gm.TradeSim = -1; Gm.TradeBag.Reset(); return; }
	bBags = true;
	const FHLGame::FHLTradeOffer* O = Gm.SimWts.Find(Gm.TradeSim);
	// their side
	Tx(WX, WY, S.P->Handle, 11, ColGold, true);
	if (O && O->Item)
	{
		ItemIcon(WX, WY + 22, 36, O->Item, O->Count);
		Tx(WX + 42, WY + 24, O->Item->Name, 10, HLQualityColor(O->Item->Q));
		Tx(WX + 42, WY + 40, FString::Printf(TEXT("Asking %s"), *HLMoney(O->Price)), 10, ColWhite);
	}
	else TxWrap(WX, WY + 24, 200, TEXT("Nothing offered. Anything you put in will be a gift."), 10, ColDim);
	// your side
	const float Rx = WX + 240;
	Tx(Rx, WY, Gm.Player.Name, 11, ColGold, true);
	for (int32 k = 0; k < 6; ++k)
	{
		const int32 B = Gm.TradeBag.IsValidIndex(k) ? Gm.TradeBag[k] : -1;
		const FHLInvSlot* Sl = B >= 0 && Gm.Player.Bag.IsValidIndex(B) ? &Gm.Player.Bag[B] : nullptr;
		if (ItemIcon(Rx + (k % 3) * 40, WY + 22 + (k / 3) * 40, 36, Sl ? Sl->Item : nullptr, Sl ? Sl->Count : 0) && B >= 0) { Gm.TradeBag.Remove(B); break; }
	}
	Tx(Rx, WY + 110, FString::Printf(TEXT("Money: %s"), *HLMoney(Gm.TradeCents)), 11, ColWhite);
	static const int32 Steps[] = { 1, 10, 100 };
	for (int32 k = 0; k < 3; ++k)
	{
		if (Btn(Rx + k * 72, WY + 132, 34, 20, FString::Printf(TEXT("+%s"), *HLMoney(Steps[k])), true, 8)) Gm.TradeCents = FMath::Min(Gm.Player.Cents, Gm.TradeCents + Steps[k]);
		if (Btn(Rx + k * 72 + 36, WY + 132, 34, 20, FString::Printf(TEXT("-%s"), *HLMoney(Steps[k])), true, 8)) Gm.TradeCents = FMath::Max(0, Gm.TradeCents - Steps[k]);
	}
	Tx(WX, WY + 170, TEXT("Click items in your bags to offer them."), 9, ColDim);
	if (Btn(WX + 120, WY + 210, 110, 28, TEXT("Trade"), true, 12)) Gm.AcceptTrade();
	if (Btn(WX + 250, WY + 210, 110, 28, TEXT("Cancel"), true, 12)) { Gm.TradeSim = -1; Gm.TradeBag.Reset(); }
}

void SHLGameUI::DrawCraft() const
{
	if (!bCraft) return;
	FHLGame& Gm = G();
	const FHLData& D = FHLData::Get();
	bool bOpen = true;
	if (!Win(TEXT("craft"), FString::Printf(TEXT("Gunsmithing  %d / 75"), Gm.Player.Guns), 470, 340, bOpen, 420, 150) || !bOpen) { bCraft = false; return; }
	if (Gm.Player.Guns <= 0)
	{
		TxWrap(WX, WY, 440, TEXT("You have not learned a crafting profession. Earl Tanner at the Hollywood Gun Shop (Highland) teaches Gunsmithing from level 5."), 10, ColWhite);
		return;
	}
	static const FLinearColor Cols[] = { FLinearColor(0.55f, 0.55f, 0.55f), FLinearColor(0.3f, 0.85f, 0.3f), FLinearColor(1.f, 0.9f, 0.2f), FLinearColor(1.f, 0.5f, 0.15f) };
	const FHLItem* Scrap = D.ItemByName(TEXT("Scrap Metal"));
	const FHLItem* Brass = D.ItemByName(TEXT("Brass Casings"));
	float Y = WY;
	const TArray<FHLGame::FHLRecipe>& Rs = FHLGame::GunRecipes();
	for (int32 i = 0; i < Rs.Num(); ++i)
	{
		const FHLGame::FHLRecipe& R = Rs[i];
		const FHLItem* Out = D.ItemByName(R.Product);
		const int32 C = Gm.RecipeColour(R);
		ItemIcon(WX, Y, 34, Out, R.Yield);
		Tx(WX + 42, Y + 1, R.Yield > 1 ? FString::Printf(TEXT("%s  x%d"), *R.Product, R.Yield) : R.Product, 11, C < 0 ? ColRed : Cols[C], true);
		Tx(WX + 42, Y + 19, FString::Printf(TEXT("%d Scrap Metal, %d Brass Casings"), R.N1, R.N2), 9, ColDim);
		if (C < 0) Tx(WX + 330, Y + 9, FString::Printf(TEXT("Requires %d"), R.Skill), 10, ColRed);
		else
		{
			const int32 N = Gm.CraftableCount(R);
			if (Btn(WX + 300, Y + 6, 64, 22, N > 0 ? FString::Printf(TEXT("Make (%d)"), N) : FString(TEXT("Make")), N > 0 && Gm.CraftIdx < 0, 9)) Gm.StartCraft(i, 1);
			if (Btn(WX + 370, Y + 6, 64, 22, TEXT("Make all"), N > 1 && Gm.CraftIdx < 0, 9)) Gm.StartCraft(i, N);
		}
		Y += 42;
	}
	Tx(WX, Y + 6, FString::Printf(TEXT("You have: %d Scrap Metal, %d Brass Casings"), Scrap ? Gm.CountItem(Scrap) : 0, Brass ? Gm.CountItem(Brass) : 0), 10, ColWhite);
	Tx(WX, Y + 24, TEXT("Orange always teaches you something, yellow usually, green rarely, grey never."), 9, ColDim);
}

void SHLGameUI::DrawMail() const
{
	FHLGame& Gm = G();
	bool bOpen = true;
	const FHLUnit* P = Gm.PlayerUnit();
	// close when the player walks away from the mailbox
	bool bNear = false;
	if (P) for (const FHLObjectState& O : Gm.Areas[P->Area].Objects) if (O.Def->Kind == TEXT("mailbox") && FVector2D::Distance(O.Def->Pos, P->Pos) < 8) bNear = true;
	if (!bNear || !Win(TEXT("npc"), FString::Printf(TEXT("Mailbox  -  %d letters"), Gm.Player.Mail.Num()), 620, 420, bOpen, 300, 140) || !bOpen) { Gm.OpenNpcMode.Reset(); return; }
	TArray<FHLMail>& Mail = Gm.Player.Mail;
	if (!Mail.Num()) { Tx(WX, WY, TEXT("You have no mail."), 11, ColDim); return; }
	SelMail = FMath::Clamp(SelMail, 0, Mail.Num() - 1);
	for (int32 i = 0; i < Mail.Num() && i < 12; ++i)
	{
		const FHLMail& M = Mail[i];
		const float Y = WY + i * 32;
		const bool bH = Hover(WX - 2, Y, 236, 28);
		if (i == SelMail || bH) Rc(WX - 2, Y, 236, 28, i == SelMail ? FLinearColor(0.3f, 0.24f, 0.12f) : FLinearColor(0.2f, 0.16f, 0.08f));
		ItemIcon(WX, Y + 1, 26, M.Item, M.Count);
		Tx(WX + 32, Y + 1, M.From.Left(26), 10, M.bRead ? ColDim : ColGold, !M.bRead);
		Tx(WX + 32, Y + 16, M.Subject.Left(32), 9, M.bRead ? ColDim : ColWhite);
		if (bH && bLClick) { bLClick = false; SelMail = i; Mail[i].bRead = true; }
	}
	FHLMail& M = Mail[SelMail];
	M.bRead = true;
	const float Rx = WX + 250;
	Tx(Rx, WY, M.Subject, 12, ColGold, true);
	Tx(Rx, WY + 20, FString::Printf(TEXT("From: %s"), *M.From), 10, ColDim);
	float Y = WY + 44;
	TArray<FString> Paras; M.Body.ParseIntoArray(Paras, TEXT("\n"), false);
	for (const FString& Pa : Paras) { Y += TxWrap(Rx, Y, 340, Pa, 10, ColWhite) + 4; }
	Y = FMath::Max(Y + 10, WY + 220);
	if (M.Item) { ItemIcon(Rx, Y, 36, M.Item, M.Count); Tx(Rx + 44, Y + 2, M.Item->Name, 10, HLQualityColor(M.Item->Q)); if (M.Cod > 0) Tx(Rx + 44, Y + 18, FString::Printf(TEXT("C.O.D. %s"), *HLMoney(M.Cod)), 10, ColRed); Y += 44; }
	if (M.Cents > 0) { Tx(Rx, Y, FString::Printf(TEXT("Enclosed: %s"), *HLMoney(M.Cents)), 11, ColGreen); Y += 22; }
	if ((M.Item || M.Cents > 0) && Btn(Rx, WY + 340, 150, 26, M.Cod > 0 && M.Item ? FString::Printf(TEXT("Pay %s"), *HLMoney(M.Cod)) : FString(TEXT("Take")), true, 11)) Gm.TakeMail(SelMail);
	if (Btn(Rx + 170, WY + 340, 150, 26, M.Cod > 0 && M.Item ? TEXT("Return to sender") : TEXT("Delete"), !(M.Cod == 0 && (M.Item || M.Cents > 0)), 11)) Gm.DeleteMail(SelMail);
}
