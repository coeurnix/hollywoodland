#include "HLGame.h"
#include "HLBridge.h"
#include "Misc/ConfigCacheIni.h"
#include "Algo/Reverse.h"

FHLGame* FHLGame::Instance = nullptr;

// ------------------------------------------------------------------------------------------------------------- nav grid
void FHLNav::Init(const FHLAreaDef& Def)
{
	Min = Def.Min; Cell = Def.Cell;
	W = FMath::CeilToInt32(Def.Size.X / Cell); H = FMath::CeilToInt32(Def.Size.Y / Cell);
	Blocked.Init(0, W * H);
	const float Pad = 0.35f;
	for (const FHLBox& B : Def.Boxes)
	{
		if (!B.bSolid || B.H < 1.0f) continue;
		const int32 X0 = FMath::FloorToInt32((B.X - Pad - Min.X) / Cell), X1 = FMath::FloorToInt32((B.X + B.W + Pad - Min.X) / Cell);
		const int32 Y0 = FMath::FloorToInt32((B.Y - Pad - Min.Y) / Cell), Y1 = FMath::FloorToInt32((B.Y + B.D + Pad - Min.Y) / Cell);
		for (int32 Y = FMath::Max(0, Y0); Y <= FMath::Min(H - 1, Y1); ++Y)
			for (int32 X = FMath::Max(0, X0); X <= FMath::Min(W - 1, X1); ++X) Blocked[Y * W + X] = 1;
	}
	G.SetNumZeroed(W * H); Parent.SetNumZeroed(W * H); Stamp.SetNumZeroed(W * H);
}

bool FHLNav::LineClear(const FVector2D& A, const FVector2D& B) const
{
	const float Len = FVector2D::Distance(A, B);
	const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Len / (Cell * 0.45f)));
	for (int32 i = 0; i <= Steps; ++i) if (BlockedAt(FMath::Lerp(A, B, (float)i / Steps))) return false;
	return true;
}

FVector2D FHLNav::NearestFree(const FVector2D& P) const
{
	if (!BlockedAt(P)) return P;
	const int32 CX = FMath::FloorToInt32((P.X - Min.X) / Cell), CY = FMath::FloorToInt32((P.Y - Min.Y) / Cell);
	for (int32 R = 1; R < 40; ++R)
		for (int32 DY = -R; DY <= R; ++DY)
			for (int32 DX = -R; DX <= R; ++DX)
			{
				if (FMath::Abs(DX) != R && FMath::Abs(DY) != R) continue;
				if (!BlockedCell(CX + DX, CY + DY)) return FVector2D(Min.X + (CX + DX + 0.5f) * Cell, Min.Y + (CY + DY + 0.5f) * Cell);
			}
	return P;
}

FVector2D FHLNav::Slide(const FVector2D& From, const FVector2D& Delta) const
{
	FVector2D To = From + Delta;
	if (!BlockedAt(To)) return To;
	To = From + FVector2D(Delta.X, 0);
	if (!BlockedAt(To)) return To;
	To = From + FVector2D(0, Delta.Y);
	if (!BlockedAt(To)) return To;
	return From;
}

bool FHLNav::FindPath(const FVector2D& AIn, const FVector2D& BIn, TArray<FVector2D>& Out) const
{
	Out.Reset();
	const FVector2D A = NearestFree(AIn), B = NearestFree(BIn);
	auto CellOf = [this](const FVector2D& P) { return FIntPoint(FMath::Clamp(FMath::FloorToInt32((P.X - Min.X) / Cell), 0, W - 1), FMath::Clamp(FMath::FloorToInt32((P.Y - Min.Y) / Cell), 0, H - 1)); };
	auto Center = [this](int32 Idx) { return FVector2D(Min.X + (Idx % W + 0.5f) * Cell, Min.Y + (Idx / W + 0.5f) * Cell); };
	const FIntPoint SC = CellOf(A), GC = CellOf(B);
	const int32 Start = SC.Y * W + SC.X, Goal = GC.Y * W + GC.X;
	if (Start == Goal) { Out.Add(B); return true; }
	if (++CurStamp == 0) { FMemory::Memzero(Stamp.GetData(), Stamp.Num() * sizeof(uint32)); CurStamp = 1; }
	struct FNode { float F; int32 Idx; };
	auto Less = [](const FNode& L, const FNode& R) { return L.F < R.F; };
	auto Heur = [&](int32 Idx) { const int32 DX = FMath::Abs(Idx % W - GC.X), DY = FMath::Abs(Idx / W - GC.Y); return (float)(FMath::Max(DX, DY) + 0.414f * FMath::Min(DX, DY)); };
	TArray<FNode> Open;
	Open.HeapPush({ Heur(Start), Start }, Less);
	G[Start] = 0; Parent[Start] = -1; Stamp[Start] = CurStamp;
	static const int32 DXs[8] = { 1, -1, 0, 0, 1, 1, -1, -1 }, DYs[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
	int32 Expanded = 0;
	bool bFound = false;
	while (Open.Num() && Expanded < 40000)
	{
		FNode Nd;
		Open.HeapPop(Nd, Less, EAllowShrinking::No);
		if (Nd.Idx == Goal) { bFound = true; break; }
		const float GHere = G[Nd.Idx];
		if (Nd.F - Heur(Nd.Idx) > GHere + 0.001f) continue; // stale
		++Expanded;
		const int32 X = Nd.Idx % W, Y = Nd.Idx / W;
		for (int32 k = 0; k < 8; ++k)
		{
			const int32 NX = X + DXs[k], NY = Y + DYs[k];
			if (BlockedCell(NX, NY)) continue;
			if (k >= 4 && (BlockedCell(X + DXs[k], Y) || BlockedCell(X, Y + DYs[k]))) continue; // no corner cutting
			const int32 NI = NY * W + NX;
			const float NG = GHere + (k >= 4 ? 1.414f : 1.f);
			if (Stamp[NI] == CurStamp && G[NI] <= NG) continue;
			Stamp[NI] = CurStamp; G[NI] = NG; Parent[NI] = Nd.Idx;
			Open.HeapPush({ NG + Heur(NI), NI }, Less);
		}
	}
	if (!bFound) return false;
	TArray<FVector2D> Raw;
	for (int32 I = Goal; I != -1; I = Parent[I]) Raw.Add(Center(I));
	Algo::Reverse(Raw);
	Raw[0] = A; Raw.Last() = B;
	// string pulling
	int32 Cur = 0;
	while (Cur < Raw.Num() - 1)
	{
		int32 Next = Raw.Num() - 1;
		while (Next > Cur + 1 && !LineClear(Raw[Cur], Raw[Next])) --Next;
		Out.Add(Raw[Next]);
		Cur = Next;
	}
	return true;
}

// ------------------------------------------------------------------------------------------------------------- lifecycle
FHLGame::FHLGame() : Rng((int32)(FPlatformTime::Cycles64() & 0x7fffffff)) { Instance = this; }

bool FHLGame::Init(FString& OutError)
{
	FHLData& D = FHLData::Get();
	if (!D.Load(OutError)) return false;
	ApiBase = TEXT("https://hollywoodland-server.coeurnix.workers.dev");
	FString Cfg;
	if (GConfig && GConfig->GetString(TEXT("Hollywoodland"), TEXT("ApiBase"), Cfg, GEngineIni) && Cfg.Contains(TEXT("://"))) ApiBase = Cfg;
	Areas.SetNum(D.Areas.Num());
	for (int32 i = 0; i < D.Areas.Num(); ++i)
	{
		FHLAreaState& A = Areas[i];
		A.Def = &D.Areas[i];
		A.Nav.Init(*A.Def);
		A.Camps.SetNum(A.Def->Camps.Num());
		for (int32 c = 0; c < A.Camps.Num(); ++c) { A.Camps[c].UnitIds.Init(0, A.Def->Camps[c].Count); A.Camps[c].RespawnAt.Init(0, A.Def->Camps[c].Count); }
		A.Objects.SetNum(A.Def->Objects.Num());
		for (int32 o = 0; o < A.Objects.Num(); ++o) A.Objects[o].Def = &A.Def->Objects[o];
		for (const FHLNpcDef& N : A.Def->Npcs)
		{
			FHLUnit* U = AddUnit(EHLKind::Npc, i, N.Pos);
			U->Name = N.Name; U->Title = N.Title; U->Npc = &N; U->Yaw = N.Yaw; U->Color = N.Color; U->Level = 30; U->MaxHp = U->Hp = 5000;
		}
		SpawnArea(i, true);
	}
	return true;
}

void FHLGame::EnterWorld()
{
	const FHLAreaDef& Zone = *Areas[0].Def;
	if (!PlayerUnit())
	{
		FHLUnit* U = AddUnit(EHLKind::Player, 0, Zone.Spawn);
		PlayerUnitId = U->Id;
	}
	FHLUnit& U = *PlayerUnit();
	U.Name = Player.Name; U.Cls = Player.Cls; U.Color = Player.Cls->Color; U.Yaw = -90;
	RecalcPlayerStats();
	U.Hp = U.MaxHp; U.Res = U.ResType == EHLRes::Adrenaline ? 0 : U.MaxRes;
	bInWorld = true;
	InitSims();
	WorkerSession();
	CurSubzone = SubzoneAt(U.Area, U.Pos);
	Banner(Zone.Name, CurSubzone, 5);
	System(TEXT("Welcome to Hollywoodland. This is a single-player game: every other player you meet is simulated by AI."));
	System(FString::Printf(TEXT("There are %d players online. Type /help for chat commands."), OnlineCount()));
	TickAuctions();
}

FHLUnit* FHLGame::AddUnit(EHLKind Kind, int32 Area, const FVector2D& Pos)
{
	TUniquePtr<FHLUnit> U = MakeUnique<FHLUnit>();
	U->Id = NextUnitId++; U->Kind = Kind; U->Area = Area; U->Pos = Pos;
	FHLUnit* Raw = U.Get();
	Units.Add(Raw->Id, MoveTemp(U));
	UnitList.Add(Raw);
	return Raw;
}

void FHLGame::RemoveUnit(int32 Id)
{
	if (FHLUnit* U = Unit(Id))
	{
		if (U->CampIdx >= 0 && U->CampSlot >= 0) Areas[U->Area].Camps[U->CampIdx].UnitIds[U->CampSlot] = 0;
		UnitList.Remove(U);
		Units.Remove(Id);
	}
}

void FHLGame::TeleportUnit(FHLUnit& U, int32 Area, const FVector2D& Pos)
{
	U.Area = Area; U.Pos = Areas[Area].Nav.NearestFree(Pos); StopMove(U); U.Z = 0;
}

FString FHLGame::SubzoneAt(int32 Area, const FVector2D& P) const
{
	for (const FHLSubzone& S : Areas[Area].Def->Subzones) if (P.X >= S.X && P.X <= S.X + S.W && P.Y >= S.Y && P.Y <= S.Y + S.D) return S.Name;
	return Areas[Area].Def->Name;
}

void FHLGame::Tick(float Dt)
{
	if (!bInWorld) return;
	Dt = FMath::Min(Dt, 0.1f);
	FrameDt = Dt;
	Now += Dt;
	Player.PlayedS += Dt;
	for (FHLUnit* U : UnitList) U->bMovedThisTick = false;
	TickPlayer(Dt);
	// Units may be added during the loop (summons); iterate by index over a snapshot count and never remove here.
	const int32 Count = UnitList.Num();
	for (int32 i = 0; i < Count && i < UnitList.Num(); ++i)
	{
		FHLUnit& U = *UnitList[i];
		if (U.Kind == EHLKind::Npc) continue;
		TickCombat(U, Dt);
		if (U.Kind == EHLKind::Mob) TickMob(U, Dt);
		if (U.HopT > 0) U.HopT = FMath::Max(0.f, U.HopT - Dt);
	}
	TickSims(Dt);
	TickCamps(Dt);
	TickCensus();
	TickDirector(Dt);
	TickChat(Dt);
	if (Now >= NextAuctionChurn) TickAuctions();
	if (Roll.bActive && Now >= Roll.EndsAt) FinishRoll();
	for (int32 i = Floats.Num() - 1; i >= 0; --i) { Floats[i].Age += Dt; if (Floats[i].Age > 1.6f) Floats.RemoveAt(i); }
	if (Now >= NextSaveAt) { NextSaveAt = Now + 30; SaveNow(); }
}

// ------------------------------------------------------------------------------------------------------------- movement
void FHLGame::FaceTo(FHLUnit& U, const FVector2D& P)
{
	const FVector2D D = P - U.Pos;
	if (D.SizeSquared() > 0.0001f) U.Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
}

bool FHLGame::MoveTo(FHLUnit& U, const FVector2D& Goal, float StopDist)
{
	const float D = FVector2D::Distance(U.Pos, Goal);
	if (D <= StopDist) { StopMove(U); return true; }
	if (U.bDead || U.Stunned() || U.Casting) return false;
	const FHLNav& Nav = Areas[U.Area].Nav;
	const bool bGoalMoved = FVector2D::DistSquared(U.PathGoal, Goal) > 4.f;
	if (U.Path.Num() == 0 || U.PathIdx >= U.Path.Num() || (bGoalMoved && Now >= U.RepathAt))
	{
		U.Path.Reset(); U.PathIdx = 0;
		if (Nav.LineClear(U.Pos, Goal)) U.Path.Add(Goal);
		else if (!Nav.FindPath(U.Pos, Goal, U.Path)) U.Path.Add(Goal);
		U.PathGoal = Goal;
		U.RepathAt = Now + 0.6f + Rand() * 0.6f;
	}
	if (U.bMovedThisTick) return false;
	U.bMovedThisTick = true;
	float Speed = U.Speed * U.MoveMult * (U.bMounted ? 1.6f : 1.f);
	for (const FHLAura& A : U.Auras) if (A.Type == EHLAura::Slow) Speed *= (1.f - A.Value);
	float Step = Speed * FrameDt;
	while (Step > 0 && U.PathIdx < U.Path.Num())
	{
		const FVector2D T = U.Path[U.PathIdx];
		const float Seg = FVector2D::Distance(U.Pos, T);
		if (Seg <= Step) { U.Pos = T; Step -= Seg; ++U.PathIdx; }
		else
		{
			const FVector2D Dir = (T - U.Pos) / Seg;
			U.Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
			U.Pos = Nav.Slide(U.Pos, Dir * Step);
			Step = 0;
		}
	}
	U.bSitting = false;
	return FVector2D::Distance(U.Pos, Goal) <= StopDist;
}

// ------------------------------------------------------------------------------------------------------------- stats
void FHLGame::RecalcCharStats(FHLUnit& U, const FHLClass& C, int32 Level, const TMap<FString, const FHLItem*>& Equip, bool bFull)
{
	U.Cls = &C; U.Level = Level;
	const int32 Prim = HLStatIndex(C.Primary), Sec = HLStatIndex(C.Secondary);
	for (int32 i = 0; i < HL_NUM_STATS; ++i)
	{
		const float Base = i == Prim ? 14 : i == Sec ? 12 : 10;
		const float Gain = i == Prim ? 2 : i == Sec ? 1 : 0.5f;
		U.Stats[i] = FMath::RoundToInt32(Base + Gain * (Level - 1));
	}
	float Armor = 0;
	const FHLItem* Main = nullptr; const FHLItem* Ranged = nullptr;
	for (const auto& KV : Equip)
	{
		const FHLItem* It = KV.Value;
		if (!It) continue;
		for (int32 i = 0; i < HL_NUM_STATS; ++i) U.Stats[i] += It->Stats[i];
		Armor += It->Armor;
		if (KV.Key == TEXT("main")) Main = It;
		if (KV.Key == TEXT("ranged")) Ranged = It;
	}
	const float OldMaxHp = U.MaxHp, OldMaxRes = U.MaxRes;
	U.MaxHp = 20 + C.HpPerLevel * Level + 10 * U.Stats[HL_STA];
	U.Armor = Armor;
	if (C.Resource == TEXT("power")) { U.ResType = EHLRes::Power; U.MaxRes = 18 * Level + 15 * U.Stats[HL_INT]; }
	else if (C.Resource == TEXT("energy")) { U.ResType = EHLRes::Energy; U.MaxRes = 100; }
	else if (C.Resource == TEXT("focus")) { U.ResType = EHLRes::Focus; U.MaxRes = 100; }
	else { U.ResType = EHLRes::Adrenaline; U.MaxRes = 100; }
	const FHLItem* Weapon = C.bRanged && Ranged ? Ranged : Main;
	U.bRangedAuto = C.bRanged && Ranged != nullptr;
	U.AutoRange = U.bRangedAuto ? 30.f : 4.5f;
	if (U.bRangedAuto) U.AttackPower = 2 * U.Stats[HL_AGI];
	else if (C.Id == TEXT("stuntman")) U.AttackPower = U.Stats[HL_STR] + U.Stats[HL_AGI];
	else if (C.Id == TEXT("veteran") || C.Id == TEXT("mechanic")) U.AttackPower = 2 * U.Stats[HL_STR];
	else U.AttackPower = U.Stats[HL_STR];
	if (Weapon && Weapon->Speed > 0) { U.WeaponMin = Weapon->DMin; U.WeaponMax = Weapon->DMax; U.WeaponSpeed = Weapon->Speed; }
	else { U.WeaponMin = 1; U.WeaponMax = 2; U.WeaponSpeed = 2; }
	U.AbilityBonus = 0.5f * (C.Id == TEXT("preacher") ? FMath::Max(U.Stats[HL_INT], U.Stats[HL_SPI]) : U.Stats[HL_INT]);
	U.Crit = C.bCaster ? 0.05f + U.Stats[HL_INT] / 6000.f : 0.05f + U.Stats[HL_AGI] / 2000.f;
	U.Dodge = 0.05f + U.Stats[HL_AGI] / 2000.f;
	U.Speed = 7;
	if (bFull || OldMaxHp <= 1) { U.Hp = U.MaxHp; U.Res = U.ResType == EHLRes::Adrenaline ? 0 : U.MaxRes; }
	else { U.Hp = FMath::Min(U.Hp, U.MaxHp); U.Res = FMath::Min(U.Res, U.MaxRes); }
}

void FHLGame::RecalcPlayerStats()
{
	if (FHLUnit* U = PlayerUnit()) RecalcCharStats(*U, *Player.Cls, FMath::Max(1, U->Level), Player.Equip, false);
}

void FHLGame::InitMobStats(FHLUnit& U)
{
	const FHLMobDef& M = *U.Mob;
	const float L = U.Level;
	float HpMult = 1, DmgMult = 1;
	if (M.Rank == TEXT("elite")) { HpMult = 3; DmgMult = 1.5f; }
	else if (M.Rank == TEXT("rare")) { HpMult = 4; DmgMult = 1.6f; }
	else if (M.Rank == TEXT("boss")) { HpMult = 12; DmgMult = 2.5f; }
	U.MaxHp = U.Hp = FMath::RoundToFloat((42 + 15 * L + 0.8f * L * L) * M.Hp * HpMult);
	const float Hit = (3 + 1.1f * L + 0.025f * L * L) * M.Dmg * DmgMult * (M.Swing / 2.f);
	U.WeaponMin = Hit * 0.85f; U.WeaponMax = Hit * 1.15f; U.WeaponSpeed = M.Swing; U.AttackPower = 0;
	U.Armor = 20 * L * M.Armor;
	U.Crit = 0.05f; U.Dodge = 0.05f; U.Speed = M.Speed; U.AutoRange = 3.5f + M.Scale * 0.8f;
	U.ResType = EHLRes::None; U.MaxRes = U.Res = 0;
}

FHLUnit* FHLGame::SpawnMob(const FHLMobDef* Def, int32 Area, const FVector2D& Pos, int32 CampIdx, int32 Slot)
{
	FHLUnit* U = AddUnit(EHLKind::Mob, Area, Areas[Area].Nav.NearestFree(Pos));
	U->Mob = Def; U->Name = Def->Name; U->Title = Def->Title; U->Shape = Def->Shape; U->Color = Def->Color; U->Scale = Def->Scale;
	U->Level = RandInt(Def->LevelMin, Def->LevelMax);
	U->Home = U->Pos; U->CampIdx = CampIdx; U->CampSlot = Slot; U->Yaw = Rand() * 360;
	U->Leash = CampIdx >= 0 ? Areas[Area].Def->Camps[CampIdx].Leash : 40;
	U->WanderAt = Now + Rand() * 10;
	InitMobStats(*U);
	return U;
}

void FHLGame::SpawnArea(int32 AreaIdx, bool bResetBosses)
{
	FHLAreaState& A = Areas[AreaIdx];
	TArray<int32> ToRemove;
	for (FHLUnit* U : UnitList) if (U->Kind == EHLKind::Mob && U->Area == AreaIdx && (bResetBosses || !U->bDead || U->Mob->Rank != TEXT("boss"))) ToRemove.Add(U->Id);
	for (int32 Id : ToRemove) RemoveUnit(Id);
	for (int32 c = 0; c < A.Camps.Num(); ++c)
	{
		const FHLCamp& Camp = A.Def->Camps[c];
		for (int32 s = 0; s < Camp.Count; ++s)
		{
			if (A.Camps[c].UnitIds[s] != 0) continue;
			const float Ang = Rand() * 2 * PI, R = Camp.R * FMath::Sqrt(Rand());
			FHLUnit* U = SpawnMob(Camp.Mob, AreaIdx, Camp.Pos + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * R, c, s);
			A.Camps[c].UnitIds[s] = U->Id;
			A.Camps[c].RespawnAt[s] = 0;
		}
	}
	for (FHLObjectState& O : A.Objects) { O.bActive = true; O.RespawnAt = 0; }
}
