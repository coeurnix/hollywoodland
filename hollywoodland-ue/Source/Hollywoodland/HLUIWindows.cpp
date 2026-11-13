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
	DrawMenu();
}

void SHLGameUI::DrawCharacter() const
{
	FHLGame& Gm = G();
	if (!Win(TEXT("char"), FString::Printf(TEXT("%s  -  Level %d %s"), *Gm.Player.Name, Gm.PlayerUnit()->Level, *Gm.Player.Cls->Name), 330, 470, bChar, 260, 130)) return;
	const FHLUnit& U = *Gm.PlayerUnit();
	for (int32 i = 0; i < 12; ++i)
	{
		const float Y = WY + i * 28;
		const FHLItem* It = Gm.Player.Equip.FindRef(EquipSlots[i]);
		if (ItemIcon(WX, Y, 26, It) == 2) { Gm.Unequip(EquipSlots[i]); }
		Tx(WX + 32, Y + 5, It ? It->Name : FString(EquipNames[i]), 10, It ? HLQualityColor(It->Q) : ColDim);
	}
	float Y = WY + 12 * 28 + 6;
	const float Cx = WX + 160;
	for (int32 i = 0; i < HL_NUM_STATS; ++i) Tx(i < 3 ? WX : Cx, Y + (i % 3) * 15, FString::Printf(TEXT("%s: %d"), HLStatNames[i], U.Stats[i]), 10, ColWhite);
	Tx(Cx, Y + 30, FString::Printf(TEXT("Armour: %d"), (int32)U.Armor), 10, ColWhite);
	Y += 50;
	Tx(WX, Y, FString::Printf(TEXT("Damage: %d - %d  (%.1f s)"), (int32)(U.WeaponMin + U.AttackPower / 14 * U.WeaponSpeed), (int32)(U.WeaponMax + U.AttackPower / 14 * U.WeaponSpeed), U.WeaponSpeed), 10, ColWhite);
	Tx(Cx, Y, FString::Printf(TEXT("Crit: %.1f%%  Dodge: %.1f%%"), U.Crit * 100, U.Dodge * 100), 10, ColWhite);
	Tx(WX, Y + 16, FString::Printf(TEXT("Kills: %d   Deaths: %d   Played: %d min"), Gm.Player.Kills, Gm.Player.Deaths, (int32)(Gm.Player.PlayedS / 60)), 10, ColDim);
	if (Gm.Player.Salvage > 0) Tx(WX, Y + 32, FString::Printf(TEXT("Salvaging %d / 75 (Apprentice)"), Gm.Player.Salvage), 10, ColDim);
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
		if (R == 2 || R == 1)
		{
			if (Gm.OpenNpcMode == TEXT("auction")) { if (S.Item) Gm.ListAuction(i, Gm.MarketPrice(*S.Item) * S.Count); }
			else if (R == 2) Gm.UseBag(i);
		}
	}
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
	if (!Win(TEXT("who"), FString::Printf(TEXT("Who  -  %d players online"), Gm.OnlineCount()), 640, 470, bWho, 380, 140)) { Gm.bWhoOpen = false; return; }
	const FHLData& D = FHLData::Get();
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
	if (!Q.IsEmpty() && Btn(WX + 540, WY - 2, 70, 18, TEXT("Clear"), true, 9)) Gm.WhoQuery.Reset();
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
		const FHLItem* It = S.P->Equipped.FindRef(EquipSlots[i]);
		ItemIcon(WX, Y, 26, It);
		Tx(WX + 32, Y + 5, It ? It->Name : FString(EquipNames[i]), 10, It ? HLQualityColor(It->Q) : ColDim);
	}
	if (!S.P->Mount.IsEmpty()) Tx(WX, WY + 20 + 12 * 28, FString::Printf(TEXT("Mount: %s"), *S.P->Mount), 10, ColDim);
}

void SHLGameUI::DrawTalents() const
{
	FHLGame& Gm = G();
	if (!Win(TEXT("talents"), TEXT("Talents"), 560, 330, bTalents, 400, 200)) return;
	const FHLUnit& U = *Gm.PlayerUnit();
	const int32 Points = FMath::Max(0, U.Level - 9);
	Tx(WX, WY, U.Level < 10 ? TEXT("Talents unlock at level 10. One point per level, three trees of sixteen talents.") : FString::Printf(TEXT("Unspent talent points: %d"), Points), 11, U.Level < 10 ? ColDim : ColGold);
	for (int32 t = 0; t < Gm.Player.Cls->Specs.Num() && t < 3; ++t)
	{
		const float X = WX + t * 180, Y = WY + 30;
		Tx(X, Y, Gm.Player.Cls->Specs[t], 12, ColGold, true);
		for (int32 i = 0; i < 16; ++i) { Rc(X + (i % 4) * 38, Y + 24 + (i / 4) * 38, 32, 32, FLinearColor(0.14f, 0.14f, 0.14f)); Tx(X + (i % 4) * 38 + 20, Y + 24 + (i / 4) * 38 + 18, TEXT("0"), 8, ColDim); }
	}
	Tx(WX, WY + 220, TEXT("Talent trees are not selectable in this proof of concept."), 10, ColDim);
	Tx(WX, WY + 238, TEXT("Respec at your trainer: $1, doubling each time to $50."), 10, ColDim);
}

void SHLGameUI::DrawMenu() const
{
	FHLGame& Gm = G();
	if (!Win(TEXT("menu"), TEXT("Game Menu"), 330, 300, bMenu, (VS.X - 330) * 0.5f, 240)) return;
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
		Service(TEXT("vendor"), TEXT("Let me browse your goods."), TEXT("vendor"));
		Service(TEXT("trainer"), TEXT("I'd like to train."), TEXT("trainer"));
		Service(TEXT("trainer_prof"), TEXT("Teach me a profession."), TEXT("prof"));
		Service(TEXT("banker"), TEXT("I'd like to check my deposit box."), TEXT("bank"));
		Service(TEXT("auctioneer"), TEXT("Show me the auction listings."), TEXT("auction"));
		Service(TEXT("redcar"), TEXT("Where do the red cars go?"), TEXT("redcar"));
		if (Np.HasRole(TEXT("innkeeper"))) { if (Btn(WX, Y, 410, 24, TEXT("Make this inn my home."), true, 10)) { Gm.System(TEXT("The Roosevelt Hotel is now your home. Resting here earns rested experience.")); } Y += 30; }
		if (Np.HasRole(TEXT("repair"))) { if (Btn(WX, Y, 410, 24, TEXT("Repair my gear."), true, 10)) Gm.System(TEXT("Your equipment is in good repair.")); Y += 30; }
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
			Tx(WX + 34, Y + 14, It->IsEquip() ? FString::Printf(TEXT("level %d %s"), It->Req, *It->Type) : It->Type, 8, ColDim);
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
		if (!Win(TEXT("npc"), Title, 290, 250, bOpen, 300, 140)) { Close(); return; }
		for (int32 i = 0; i < Gm.Player.Bank.Num(); ++i)
		{
			FHLInvSlot& S = Gm.Player.Bank[i];
			if (ItemIcon(WX + (i % 6) * 42, WY + (i / 6) * 42, 40, S.Item, S.Count) && S.Item) { if (Gm.AddItem(S.Item, S.Count, false)) S = FHLInvSlot(); }
		}
		Tx(WX, WY + 174, TEXT("Click to withdraw. Right-click bag items to deposit."), 9, ColDim);
		Tx(WX, WY + 190, TEXT("More boxes: $1, $5, $10, $25, $50, $100."), 9, ColDim);
		return;
	}
	if (Gm.OpenNpcMode == TEXT("auction"))
	{
		if (!Win(TEXT("npc"), TEXT("Auction House  -  Taft Building"), 700, 500, bOpen, 240, 110)) { Close(); return; }
		static const TCHAR* Filters[] = { TEXT("All"), TEXT("Usable"), TEXT("Weapons"), TEXT("Armour"), TEXT("Goods"), TEXT("Mine") };
		for (int32 f = 0; f < 6; ++f) { if (f == AuctionFilter) Rc(WX + f * 84 - 2, WY - 4, 82, 24, ColGold); if (Btn(WX + f * 84, WY - 2, 78, 20, Filters[f], true, 10)) { AuctionFilter = f; AuctionPage = 0; } }
		TArray<int32> Rows;
		for (int32 i = 0; i < Gm.Auctions.Num(); ++i)
		{
			const FHLAuction& A = Gm.Auctions[i];
			if (AuctionFilter == 1 && !(A.Item->IsEquip() && Gm.CanEquip(*A.Item))) continue;
			if (AuctionFilter == 2 && A.Item->Kind != TEXT("weapon")) continue;
			if (AuctionFilter == 3 && A.Item->Kind != TEXT("armor")) continue;
			if (AuctionFilter == 4 && A.Item->IsEquip()) continue;
			if (AuctionFilter == 5 && !A.bMine) continue;
			Rows.Add(i);
		}
		Rows.Sort([&](int32 A, int32 B) { return Gm.Auctions[A].Item->Req != Gm.Auctions[B].Item->Req ? Gm.Auctions[A].Item->Req < Gm.Auctions[B].Item->Req : A < B; });
		const int32 PerPage = 13, Pages = FMath::Max(1, (Rows.Num() + PerPage - 1) / PerPage);
		if (Wheel != 0 && Hover(WX, WY, 680, 460)) AuctionPage -= (int32)FMath::Sign(Wheel);
		AuctionPage = FMath::Clamp(AuctionPage, 0, Pages - 1);
		Tx(WX + 34, WY + 26, TEXT("Item"), 10, ColGold); Tx(WX + 330, WY + 26, TEXT("Lvl"), 10, ColGold); Tx(WX + 380, WY + 26, TEXT("Seller"), 10, ColGold); Tx(WX + 500, WY + 26, TEXT("Buyout"), 10, ColGold);
		for (int32 r = 0; r < PerPage && r + AuctionPage * PerPage < Rows.Num(); ++r)
		{
			const int32 Idx = Rows[r + AuctionPage * PerPage];
			const FHLAuction& A = Gm.Auctions[Idx];
			const float Y = WY + 44 + r * 30;
			ItemIcon(WX, Y, 27, A.Item, A.Count);
			Tx(WX + 34, Y + 5, A.Item->Name, 10, HLQualityColor(A.Item->Q));
			Tx(WX + 330, Y + 5, A.Item->Req > 0 ? FString::FromInt(A.Item->Req) : TEXT("-"), 10, A.Item->Req > P.Level ? ColRed : ColWhite);
			Tx(WX + 380, Y + 5, A.bMine ? Gm.Player.Name : Gm.Sims[A.SellerIdx].P->Handle, 10, ColDim);
			Tx(WX + 500, Y + 5, HLMoney(A.Buyout), 10, Gm.Player.Cents >= A.Buyout ? ColWhite : ColRed);
			if (Btn(WX + 590, Y + 2, 84, 22, A.bMine ? TEXT("Cancel") : TEXT("Buyout"), A.bMine || Gm.Player.Cents >= A.Buyout, 10)) { Gm.BuyAuction(Idx); break; }
		}
		Tx(WX, WY + 440, FString::Printf(TEXT("Page %d / %d   (%d auctions, mouse wheel pages)    You have %s.   To sell: click an item in your bags (5%% deposit, 5%% cut)."), AuctionPage + 1, Pages, Rows.Num(), *HLMoney(Gm.Player.Cents)), 9, ColDim);
		return;
	}
	Gm.OpenNpcMode = TEXT("gossip");
}
