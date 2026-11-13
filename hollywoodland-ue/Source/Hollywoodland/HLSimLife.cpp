// Sim street life: conversations between simulated players (General chat and out loud), errands at real NPCs (facing the
// auctioneer, learning at their trainer, sitting at the inn), sims gathering quest objects and salvage before the player
// gets to them, auction undercutting, and random street happenings (a duel outside the Roosevelt, a dance on the bank
// steps, an AFK row by the mailbox, a queue at the auctioneer).
#include "HLGame.h"

// ------------------------------------------------------------------------------------------------------------- conversations
bool FHLGame::StartConvo(const FHLConvo& C, const TArray<int32>& Speakers, float Lead)
{
	if (!Speakers.Num()) return false;
	RecentConvos.Add(&C); if (RecentConvos.Num() > 12) RecentConvos.RemoveAt(0);
	const FHLSim& A = Sims[Speakers[0]];
	// fill every line in one pass from speaker a, so the tokens agree from line to line
	FString Joined;
	for (int32 i = 0; i < C.Lines.Num(); ++i) { if (i) Joined += TEXT("||"); Joined += C.Lines[i].Value; }
	LastLinked = nullptr; LastPrice = 0;
	TArray<FString> Parts;
	FillTemplate(A, Joined).ParseIntoArray(Parts, TEXT("||"), false);
	if (Parts.Num() != C.Lines.Num()) return false;
	float T = Lead;
	for (int32 i = 0; i < C.Lines.Num(); ++i)
	{
		const int32 Who = Speakers[FMath::Clamp(C.Lines[i].Key, 0, Speakers.Num() - 1)];
		const FHLSim& S = Sims[Who];
		const FString Text = StyleText(*S.P, Parts[i]);
		// reading the last line, then typing this one at a persona's pace (~6 characters a second), plus a beat
		if (i > 0) T += S.P->ReactionS * 1.5f + 0.8f + Rand() * 1.8f + Text.Len() * 0.16f;
		AddChat(C.Ch, S.P->Handle, Text, Who, T);
	}
	(C.Ch == TEXT("say") ? SayEndsAt : ConvoEndsAt) = Now + T + 2;
	UE_LOG(LogTemp, Display, TEXT("HL_LIFE convo %s (%d lines, %.0f s): %s"), *C.Ch, C.Lines.Num(), T, *Parts[0]);
	return true;
}

void FHLGame::TickConvos()
{
	const FHLData& D = FHLData::Get();
	if (!D.Convos.Num() || !Sims.Num()) return;
	// General: two or three players talk it out, about one every minute and a half
	if (Now >= NextZoneConvo && Now >= ConvoEndsAt)
	{
		NextZoneConvo = Now + 60 + Rand() * 70;
		TArray<const FHLConvo*> Fit;
		for (const FHLConvo& C : D.Convos) if (C.Ch == TEXT("zone") && !RecentConvos.Contains(&C)) Fit.Add(&C);
		if (Fit.Num())
		{
			const FHLConvo& C = *Fit[RandInt(0, Fit.Num() - 1)];
			TArray<int32> Sp;
			for (int32 Try = 0; Try < 80 && Sp.Num() < C.Speakers; ++Try)
			{
				const int32 SI = RandInt(0, Sims.Num() - 1);
				const FHLSim& S = Sims[SI];
				if (!S.bOnline || S.bInParty || S.bAfk || Sp.Contains(SI) || !(S.UnitId || S.Level <= 14)) continue;
				if (Sp.Num() == 0 && (S.Level < C.Min || S.Level > C.Max)) continue;
				Sp.Add(SI);
			}
			if (Sp.Num() == C.Speakers) StartConvo(C, Sp, 0);
		}
	}
	// Out loud: two players standing around near the player chat with speech bubbles
	const FHLUnit* P = PlayerUnit();
	if (Now >= NextSayConvo && Now >= SayEndsAt && P && P->Area == 0 && !P->bDead)
	{
		NextSayConvo = Now + 50 + Rand() * 60;
		auto Free = [&](const FHLSim& S, const FHLUnit*& U) -> bool
		{
			if (!S.UnitId || S.bInParty || S.bAfk || S.EventRole || S.State == EHLSimState::Script || S.State == EHLSimState::Grind || S.State == EHLSimState::Gather || S.State == EHLSimState::Leaving || S.ChatUntil > Now) return false;
			U = Unit(S.UnitId);
			return U && !U->bDead && U->Area == 0 && !InCombat(*U);
		};
		int32 A = -1, B = -1; float BestD = 55;
		for (int32 i = 0; i < Sims.Num(); ++i) { const FHLUnit* U = nullptr; if (Free(Sims[i], U) && Dist(*U, *P) < BestD && Dist(*U, *P) > 4) { A = i; BestD = Dist(*U, *P); } }
		if (A < 0) { NextSayConvo = Now + 12; return; }
		const FHLUnit* UA = Unit(Sims[A].UnitId);
		BestD = 45;
		for (int32 i = 0; i < Sims.Num(); ++i) { const FHLUnit* U = nullptr; if (i != A && Free(Sims[i], U) && Dist(*U, *UA) < BestD) { B = i; BestD = Dist(*U, *UA); } }
		if (B < 0) { NextSayConvo = Now + 12; return; }
		TArray<const FHLConvo*> Fit;
		for (const FHLConvo& C : D.Convos) if (C.Ch == TEXT("say") && C.Speakers <= 2 && !RecentConvos.Contains(&C)) Fit.Add(&C);
		if (!Fit.Num()) return;
		const float Walk = BestD / 5.f + 1.f;
		if (StartConvo(*Fit[RandInt(0, Fit.Num() - 1)], { A, B }, Walk))
		{
			Sims[A].ChatWith = B; Sims[B].ChatWith = A;
			Sims[A].ChatUntil = Sims[B].ChatUntil = SayEndsAt + 3;
			Sims[A].Route.Reset(); Sims[B].Route.Reset(); Sims[A].RouteIdx = Sims[B].RouteIdx = 0;
			Sims[B].bChatWalker = true; Sims[A].bChatWalker = false;
		}
	}
}

bool FHLGame::SimChatBrain(FHLSim& S, FHLUnit& U)
{
	if (S.ChatWith < 0 || Now >= S.ChatUntil) { S.ChatWith = -1; return false; }
	if (InCombat(U) || !Sims.IsValidIndex(S.ChatWith)) { S.ChatUntil = 0; return false; }
	const FHLUnit* O = Unit(Sims[S.ChatWith].UnitId);
	if (!O || O->bDead) { S.ChatUntil = 0; return false; }
	U.bSitting = false;
	if (S.bChatWalker && Dist(U, *O) > 2.6f) { U.MoveMult = 1; MoveTo(U, O->Pos + (U.Pos - O->Pos).GetSafeNormal() * 2.f, 0.6f); }
	else { StopMove(U); FaceTo(U, O->Pos); }
	return true;
}

// ------------------------------------------------------------------------------------------------------------- errands
FVector2D FHLGame::ErrandSpot(const FString& Poi, const FHLSim& S, const FVector2D& PoiPos)
{
	const FHLAreaDef& Def = *Areas[0].Def;
	const FHLNpcDef* Best = nullptr; float BestD = 70;
	for (const FHLNpcDef& N : Def.Npcs)
	{
		bool bFit = false;
		if (Poi == TEXT("vendor")) bFit = N.HasRole(TEXT("vendor"));
		else if (Poi == TEXT("trainers")) bFit = N.HasRole(TEXT("trainer")) && S.P->Cls && N.Trains == S.P->Cls->Id;
		else if (Poi == TEXT("inn")) bFit = N.HasRole(TEXT("innkeeper"));
		else if (Poi == TEXT("bank")) bFit = N.HasRole(TEXT("banker"));
		else if (Poi == TEXT("auction")) bFit = N.HasRole(TEXT("auctioneer"));
		const float Dd = FVector2D::Distance(N.Pos, PoiPos);
		if (bFit && (Poi == TEXT("trainers") || Dd < BestD)) { Best = &N; BestD = Dd; }
	}
	if (Poi == TEXT("mailbox"))
		for (const FHLObjectDef& O : Def.Objects) if (O.Kind == TEXT("mailbox") && FVector2D::Distance(O.Pos, PoiPos) < 60) return Areas[0].Nav.NearestFree(O.Pos + FVector2D(Rand() * 2 - 1, 1.6f));
	if (!Best) return PoiPos + FVector2D(Rand() * 6 - 3, Rand() * 4 - 1);
	// stand in front of the counter, a little to one side so a queue looks like a queue
	const float R = FMath::DegreesToRadians(Best->Yaw);
	const FVector2D Fwd(FMath::Cos(R), FMath::Sin(R)), Side(-Fwd.Y, Fwd.X);
	return Areas[0].Nav.NearestFree(Best->Pos + Fwd * (2.2f + Rand() * 0.8f) + Side * (Rand() * 2.4f - 1.2f));
}

void FHLGame::SimArrived(FHLSim& S, FHLUnit& U)
{
	const int32 SimIdx = int32(&S - Sims.GetData());
	FHLUnit* Npc = nullptr; float BestD = 4.5f;
	for (FHLUnit* N : UnitList) if (N->Kind == EHLKind::Npc && N->Npc && N->Area == U.Area && !N->bDead && Dist(*N, U) < BestD) { Npc = N; BestD = Dist(*N, U); }
	if (!Npc)
	{
		for (const FHLObjectDef& O : Areas[0].Def->Objects) if (O.Kind == TEXT("mailbox") && FVector2D::Distance(O.Pos, U.Pos) < 4) { FaceTo(U, O.Pos); S.StateUntil = Now + 5 + Rand() * 10; }
		return;
	}
	FaceTo(U, Npc->Pos);
	if (Npc->Id != OpenNpcUnitId) FaceTo(*Npc, U.Pos); // the shopkeeper looks up
	const FHLUnit* P = PlayerUnit();
	const bool bSeen = P && Dist(*P, U) < 45;
	const FHLNpcDef& N = *Npc->Npc;
	S.StateUntil = Now + 6 + Rand() * 12;
	UE_LOG(LogTemp, Verbose, TEXT("HL_LIFE %s at %s"), *S.P->Handle, *N.Name);
	if (N.HasRole(TEXT("auctioneer"))) { if (Chance(0.6f)) SimPostAuction(SimIdx); S.StateUntil += 6; }
	else if (N.HasRole(TEXT("trainer")) && S.P->Cls && N.Trains == S.P->Cls->Id) { if (bSeen && Chance(0.6f)) AddFloat(U.Id, TEXT("New ability learned"), FLinearColor(1, 0.85f, 0.3f)); }
	else if (N.HasRole(TEXT("innkeeper")) && Chance(0.5f)) { U.bSitting = true; S.StateUntil += 10; }
}

void FHLGame::SimPostAuction(int32 SimIdx)
{
	const FHLData& D = FHLData::Get();
	const FHLSim& S = Sims[SimIdx];
	// half the time, someone has seen the player's listing and undercuts it
	for (const FHLAuction& Mine : Auctions)
	{
		if (!Mine.bMine || !Mine.Item || !Chance(0.5f)) continue;
		bool bAlready = false;
		for (const FHLAuction& O : Auctions) if (!O.bMine && O.Item == Mine.Item && O.SellerIdx == SimIdx) bAlready = true;
		if (bAlready) continue;
		FHLAuction A; A.Item = Mine.Item; A.SellerIdx = SimIdx; A.Count = Mine.Count;
		const int32 Per = Mine.Buyout / FMath::Max(1, Mine.Count);
		A.Buyout = FMath::Max(1, Per - FMath::Max(1, FMath::RoundToInt32(Per * (0.01f + Rand() * 0.04f)))) * A.Count;
		A.MinBid = FMath::Max(1, FMath::RoundToInt32(A.Buyout * 0.6f));
		A.ExpiresAt = Now + 1800 + Rand() * 1800;
		Auctions.Add(A);
		return;
	}
	const TArray<const FHLItem*>& Pool = D.EquipPool(S.Level, Chance(0.85f) ? 2 : 3);
	if (!Pool.Num()) return;
	FHLAuction A; A.Item = Pool[RandInt(0, Pool.Num() - 1)]; A.SellerIdx = SimIdx;
	A.Buyout = FMath::Max(2, FMath::RoundToInt32(MarketPrice(*A.Item) * (0.8f + Rand() * 0.7f)));
	A.MinBid = FMath::Max(1, FMath::RoundToInt32(A.Buyout * 0.6f));
	A.ExpiresAt = Now + 1800 + Rand() * 3000;
	Auctions.Add(A);
}

bool FHLGame::Undercut(const FHLAuction& Mine) const
{
	const int32 Per = Mine.Buyout / FMath::Max(1, Mine.Count);
	for (const FHLAuction& O : Auctions) if (!O.bMine && O.Item == Mine.Item && O.Buyout / FMath::Max(1, O.Count) < Per) return true;
	return false;
}

// ------------------------------------------------------------------------------------------------------------- gathering
bool FHLGame::TryStartGather(FHLSim& S, FHLUnit& U)
{
	if (Now < S.NextGather || S.CampIdx < 0) return false;
	S.NextGather = Now + 25 + Rand() * 40;
	if (!Chance(0.4f)) return false;
	const FHLAreaDef& Def = *Areas[0].Def;
	const FHLCamp& Camp = Def.Camps[S.CampIdx];
	const FHLUnit* P = PlayerUnit();
	int32 Best = -1; float BestD = Camp.R + 30;
	for (int32 o = 0; o < Areas[0].Objects.Num(); ++o)
	{
		const FHLObjectState& O = Areas[0].Objects[o];
		if (!O.bActive || O.Def->Kind == TEXT("mailbox")) continue;
		if (P && P->Area == 0 && FVector2D::Distance(P->Pos, O.Def->Pos) < 10) continue; // not from under the player's nose
		bool bTaken = false;
		for (const FHLSim& Other : Sims) if (Other.State == EHLSimState::Gather && Other.GatherObj == o) bTaken = true;
		if (bTaken) continue;
		const float Dd = FVector2D::Distance(O.Def->Pos, Camp.Pos);
		if (Dd < BestD) { Best = o; BestD = Dd; }
	}
	if (Best < 0) return false;
	S.GatherObj = Best; S.GatherEnd = 0; S.State = EHLSimState::Gather;
	U.TargetId = 0; U.bAutoAttack = false;
	return true;
}

void FHLGame::SimGatherBrain(FHLSim& S, FHLUnit& U)
{
	if (!Areas[0].Objects.IsValidIndex(S.GatherObj)) { S.State = EHLSimState::Grind; return; }
	FHLObjectState& O = Areas[0].Objects[S.GatherObj];
	if (!O.bActive) { U.bSitting = false; S.State = EHLSimState::Grind; S.GatherObj = -1; return; } // someone beat them to it
	if (S.GatherEnd == 0)
	{
		if (!MoveTo(U, O.Def->Pos, 1.8f)) return;
		StopMove(U); FaceTo(U, O.Def->Pos);
		S.GatherEnd = Now + (O.Def->bUse ? 2.5f : 1.5f) + Rand() * 0.6f;
		if (O.Def->bUse) U.HopT = 0.3f; else U.bSitting = true; // kneel to pick it up
		return;
	}
	if (Now < S.GatherEnd) return;
	O.bActive = false; O.RespawnAt = Now + O.Def->Respawn;
	UE_LOG(LogTemp, Verbose, TEXT("HL_LIFE %s gathered %s"), *S.P->Handle, *O.Def->Kind);
	U.bSitting = false; S.GatherObj = -1; S.GatherEnd = 0; S.State = EHLSimState::Grind;
	S.StateUntil = Now + 0.5f;
}

// ------------------------------------------------------------------------------------------------------------- npc facing
void FHLGame::TickNpcFacing()
{
	if (Now < NextNpcFacing) return;
	NextNpcFacing = Now + 0.5f;
	const FHLUnit* P = PlayerUnit();
	for (FHLUnit* N : UnitList)
	{
		if (N->Kind != EHLKind::Npc || !N->Npc || N->bDead || (bEscorting && N->Id == EscortUnitId)) continue;
		if (P && N->Id == OpenNpcUnitId) { FaceTo(*N, P->Pos); continue; }
		bool bCustomer = false;
		for (const FHLSim& S : Sims)
			if (S.UnitId && S.State == EHLSimState::Idle && Now < S.StateUntil)
				if (const FHLUnit* U = Unit(S.UnitId)) if (U->Area == N->Area && Dist(*U, *N) < 4.5f) { bCustomer = true; break; }
		if (!bCustomer) N->Yaw = N->Npc->Yaw;
	}
}

// ------------------------------------------------------------------------------------------------------------- happenings
void FHLGame::EndHappening()
{
	for (int32 SI : Happen.Sims)
		if (Sims.IsValidIndex(SI))
		{
			FHLSim& S = Sims[SI];
			if (S.State == EHLSimState::Script) { S.State = EHLSimState::Idle; S.StateUntil = Now + 2 + Rand() * 6; }
			if (Happen.Kind == 3 && S.bAfk) { S.bAfk = false; if (Chance(0.5f)) SimSay(SI, TEXT("say"), TEXT("back"), Rand() * 3); }
			if (Happen.Kind == 1 && (SI == Happen.Duel[0] || SI == Happen.Duel[1])) { S.State = EHLSimState::Rest; S.StateUntil = Now + 12; } // patch up after the duel
		}
	Happen = FHLHappening();
}

void FHLGame::TickHappenings()
{
	const FHLUnit* P = PlayerUnit();
	if (!P) return;
	const FHLAreaDef& Def = *Areas[0].Def;
	if (Happen.Kind == 0)
	{
		static const bool bLifeTest = FParse::Param(FCommandLine::Get(), TEXT("HLLifeTest"));
		if (bLifeTest && NextHappening == 0) { NextHappening = Now + 25; NextSayConvo = Now + 15; NextZoneConvo = Now + 10; }
		if ((Player.PlayedS < 200 && !bLifeTest) || Now < NextHappening || P->Area != 0 || P->bDead || SurgeHub >= 0) return;
		NextHappening = Now + (bLifeTest ? 20 + Rand() * 20 : 150 + Rand() * 180);
		// a happening near the player, of a different kind from the last one
		struct FOpt { int32 Kind; FVector2D Pos; };
		TArray<FOpt> Opts;
		if (const FVector2D* V = Def.Pois.Find(TEXT("inn"))) Opts.Add({ 1, Areas[0].Nav.NearestFree(*V + FVector2D(0, -10)) });
		if (const FVector2D* V = Def.Pois.Find(TEXT("bank"))) Opts.Add({ 2, Areas[0].Nav.NearestFree(*V + FVector2D(0, 3)) });
		if (const FVector2D* V = Def.Pois.Find(TEXT("mailbox"))) Opts.Add({ 3, *V });
		for (const FHLNpcDef& N : Def.Npcs) if (N.HasRole(TEXT("auctioneer"))) { Opts.Add({ 4, N.Pos }); break; }
		Opts.RemoveAll([&](const FOpt& O) { return O.Kind == LastHappening; });
		Opts.Sort([&](const FOpt& A, const FOpt& B) { return FVector2D::Distance(A.Pos, P->Pos) < FVector2D::Distance(B.Pos, P->Pos); });
		if (!Opts.Num()) return;
		const FOpt& O = Opts[Chance(0.7f) ? 0 : RandInt(0, Opts.Num() - 1)];
		// recruit sims who are free and not too far away
		TArray<int32> Free;
		for (int32 i = 0; i < Sims.Num(); ++i)
		{
			const FHLSim& S = Sims[i];
			if (!S.UnitId || S.bInParty || S.bAfk || S.Role == EHLSimRole::Idler || S.EventRole || S.State == EHLSimState::Script || S.State == EHLSimState::Gather || S.ChatUntil > Now || i == DuelSim || i == TradeSim) continue;
			const FHLUnit* U = Unit(S.UnitId);
			if (!U || U->bDead || U->Area != 0 || InCombat(*U) || FVector2D::Distance(U->Pos, O.Pos) > 220) continue;
			Free.Add(i);
		}
		Free.Sort([&](int32 A, int32 B) { return FVector2D::Distance(Unit(Sims[A].UnitId)->Pos, O.Pos) < FVector2D::Distance(Unit(Sims[B].UnitId)->Pos, O.Pos); });
		const int32 Want = O.Kind == 1 ? 5 : O.Kind == 2 ? 7 : O.Kind == 3 ? 3 : 5;
		if (Free.Num() < FMath::Min(Want, 3)) return;
		Free.SetNum(FMath::Min(Want, Free.Num()));
		Happen.Kind = O.Kind; Happen.Pos = O.Pos; Happen.Sims = Free; Happen.Until = Now + (O.Kind == 3 ? 150 + Rand() * 90 : O.Kind == 4 ? 75 : 90); Happen.NextBeat = Now + 4;
		LastHappening = O.Kind;
		UE_LOG(LogTemp, Display, TEXT("HL_LIFE happening %d with %d sims at %.0f,%.0f"), O.Kind, Free.Num(), O.Pos.X, O.Pos.Y);
		for (int32 k = 0; k < Free.Num(); ++k)
		{
			FHLSim& S = Sims[Free[k]];
			S.State = EHLSimState::Script; S.StateUntil = Happen.Until + 30; S.Route.Reset(); S.RouteIdx = 0;
			S.Dest = HappeningSpot(k, Free.Num());
		}
		const FHLSim& Lead = Sims[Free[0]];
		if (O.Kind == 1)
		{
			Happen.Duel[0] = Free[0]; Happen.Duel[1] = Free[1];
			AddChat(TEXT("zone"), Lead.P->Handle, StyleText(*Lead.P, FString::Printf(TEXT("duel outside the roosevelt, me vs %s, come watch"), *Sims[Free[1]].P->Handle)), Free[0], 1);
		}
		else if (O.Kind == 2) AddChat(TEXT("zone"), Lead.P->Handle, StyleText(*Lead.P, TEXT("dance party on the bank steps!! everyone come")), Free[0], 1);
		else if (O.Kind == 4 && Chance(0.5f)) AddChat(TEXT("zone"), Lead.P->Handle, StyleText(*Lead.P, TEXT("why is there a line at the auction house")), Free[0], 20);
		return;
	}
	if (Now >= Happen.Until) { EndHappening(); return; }
	if (Now < Happen.NextBeat) return;
	Happen.NextBeat = Now + (Happen.Kind == 1 ? 1.4f + Rand() * 0.6f : 1.f);
	const bool bNear = P->Area == 0 && FVector2D::Distance(P->Pos, Happen.Pos) < 60;
	auto Arrived = [&](int32 SI, FHLUnit*& U) { U = Sims.IsValidIndex(SI) ? Unit(Sims[SI].UnitId) : nullptr; return U && !U->bDead && Sims[SI].State == EHLSimState::Script && FVector2D::Distance(U->Pos, Sims[SI].Dest) < 2.5f; };
	switch (Happen.Kind)
	{
	case 1: // duel: two fight, the rest stand in a ring and watch
	{
		FHLUnit* A = nullptr; FHLUnit* B = nullptr;
		const bool bA = Arrived(Happen.Duel[0], A), bB = Arrived(Happen.Duel[1], B);
		for (int32 k = 2; k < Happen.Sims.Num(); ++k) { FHLUnit* W = nullptr; if (Arrived(Happen.Sims[k], W)) FaceTo(*W, Happen.Pos); }
		if (!bA || !bB) { if (Now > Happen.Until - 30) EndHappening(); return; }
		FaceTo(*A, B->Pos); FaceTo(*B, A->Pos);
		if (Happen.bDone) return;
		if (Happen.DuelStart == 0) { Happen.DuelStart = Now + 3; if (bNear) AddChat(TEXT("system"), FString(), TEXT("A duel is about to begin outside the Roosevelt.")); return; }
		if (Now < Happen.DuelStart) return;
		FHLUnit* Att = Chance(0.5f) ? A : B; FHLUnit* Def2 = Att == A ? B : A;
		Att->HopT = 0.3f;
		const float Hit = Def2->MaxHp * (0.06f + Rand() * 0.07f) * FMath::Clamp((float)Att->Level / FMath::Max(1, Def2->Level), 0.5f, 2.f);
		if (Def2->Hp - Hit <= Def2->MaxHp * 0.08f)
		{
			Def2->Hp = FMath::Max(1.f, Def2->MaxHp * 0.08f);
			const int32 WinSI = Att == A ? Happen.Duel[0] : Happen.Duel[1], LoseSI = Att == A ? Happen.Duel[1] : Happen.Duel[0];
			if (bNear) System(FString::Printf(TEXT("%s has defeated %s in a duel."), *Sims[WinSI].P->Handle, *Sims[LoseSI].P->Handle));
			for (int32 k = 2; k < Happen.Sims.Num(); ++k) if (Chance(0.45f)) SimSay(Happen.Sims[k], TEXT("say"), TEXT("duel_watch"), 1.2f + Rand() * 3);
			SimSay(LoseSI, TEXT("say"), TEXT("duel_sim_lost"), 2.5f + Rand());
			if (Chance(0.6f)) SimSay(WinSI, TEXT("say"), TEXT("duel_sim_won"), 4.5f + Rand() * 2);
			Happen.Until = Now + 8; Happen.bDone = true;
			return;
		}
		DealDamage(Att, *Def2, Hit, TEXT("physical"), Chance(0.15f), TEXT("attack"));
		return;
	}
	case 2: // dance: hop about, the odd emote
	{
		for (int32 SI : Happen.Sims) { FHLUnit* U = nullptr; if (Arrived(SI, U)) { if (Chance(0.5f)) U->HopT = 0.45f; if (Chance(0.15f)) U->Yaw += Rand() * 180 - 90; } }
		if (bNear && Chance(0.08f))
		{
			const int32 SI = Happen.Sims[RandInt(0, Happen.Sims.Num() - 1)], SJ = Happen.Sims[RandInt(0, Happen.Sims.Num() - 1)];
			const FString& A = Sims[SI].P->Handle; const FString& B = Sims[SJ].P->Handle;
			const int32 F = SI == SJ ? 0 : RandInt(0, 3);
			AddChat(TEXT("emote"), FString(), F == 0 ? A + TEXT(" dances.") : F == 1 ? FString::Printf(TEXT("%s dances with %s."), *A, *B) : F == 2 ? A + TEXT(" cheers.") : FString::Printf(TEXT("%s claps excitedly for %s."), *A, *B));
		}
		return;
	}
	case 3: // afk row by the mailbox
	{
		for (int32 SI : Happen.Sims) { FHLUnit* U = nullptr; if (Arrived(SI, U) && !Sims[SI].bAfk) { Sims[SI].bAfk = true; FaceTo(*U, Happen.Pos + FVector2D(0, 6)); if (Chance(0.4f)) SimSay(SI, TEXT("say"), TEXT("afk"), Rand() * 2); } }
		return;
	}
	case 4: // queue at the auctioneer: the front one finishes every 12-18 s and the line shuffles up
	{
		if (Now >= Happen.NextServe && Happen.Sims.Num())
		{
			Happen.NextServe = Now + 12 + Rand() * 6;
			FHLUnit* U = nullptr;
			if (Arrived(Happen.Sims[0], U))
			{
				const int32 SI = Happen.Sims[0];
				if (Chance(0.6f)) SimPostAuction(SI);
				Sims[SI].State = EHLSimState::Idle; Sims[SI].StateUntil = Now + 1;
				Happen.Sims.RemoveAt(0);
				for (int32 k = 0; k < Happen.Sims.Num(); ++k) Sims[Happen.Sims[k]].Dest = HappeningSpot(k, Happen.Sims.Num());
				if (!Happen.Sims.Num()) { EndHappening(); return; }
			}
		}
		for (int32 k = 0; k < Happen.Sims.Num(); ++k) { FHLUnit* U = nullptr; if (Arrived(Happen.Sims[k], U)) FaceTo(*U, Happen.Pos); }
		return;
	}
	default: return;
	}
}

FVector2D FHLGame::HappeningSpot(int32 K, int32 N) const
{
	const FHLNav& Nav = Areas[0].Nav;
	switch (Happen.Kind)
	{
	case 1:
		if (K < 2) return Nav.NearestFree(Happen.Pos + FVector2D(K == 0 ? -3.f : 3.f, 0));
		{ const float A = PI * (0.15f + 0.7f * (K - 2) / FMath::Max(1, N - 3)); return Nav.NearestFree(Happen.Pos + FVector2D(FMath::Cos(A) * 6.5f, FMath::Sin(A) * 6.5f)); }
	case 2: { const float A = 2 * PI * K / FMath::Max(1, N); return Nav.NearestFree(Happen.Pos + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (2.f + (K % 2) * 1.6f)); }
	case 3: return Nav.NearestFree(Happen.Pos + FVector2D(-4.f + K * 2.4f, 3.f + (K % 2) * 0.8f));
	case 4:
	{
		// a single file in front of the auctioneer's counter
		const FHLNpcDef* N0 = nullptr;
		for (const FHLNpcDef& Nd : Areas[0].Def->Npcs) if (Nd.HasRole(TEXT("auctioneer"))) { N0 = &Nd; break; }
		const float R = N0 ? FMath::DegreesToRadians(N0->Yaw) : 0;
		const FVector2D Fwd(FMath::Cos(R), FMath::Sin(R));
		return Nav.NearestFree(Happen.Pos + Fwd * (2.2f + K * 1.3f));
	}
	default: return Happen.Pos;
	}
}
