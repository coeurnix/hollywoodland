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
		if (O.Def->bUse) Gesture(U, TEXT("Loot")); else U.bSitting = true; // kneel to pick it up
		return;
	}
	if (Now < S.GatherEnd) return;
	O.bActive = false; O.RespawnAt = Now + O.Def->Respawn;
	if (O.Def->Spread > 0) HiveEggBroken(O.Def->Spread, false);
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
		if ((Player.PlayedS < 200 && !bLifeTest) || Now < NextHappening || P->Area != 0 || P->bDead || SurgeHub >= 0 || Fight.Stage != 0 || Fight.bAnnounced) return;
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
		for (int32 SI : Happen.Sims) { FHLUnit* U = nullptr; if (Arrived(SI, U)) { if (U->Gesture != TEXT("Dance") || Chance(0.04f)) Gesture(*U, Chance(0.85f) ? TEXT("Dance") : TEXT("Cheer")); if (Chance(0.15f)) U->Yaw += Rand() * 180 - 90; } }
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

// ------------------------------------------------------------------------------------------------------------- dungeon groups
// Five simulated players form a group in LFG, meet at the Egyptian's door, go in, and come back a quarter of an hour later
// with boss loot equipped (or a wipe story). While inside they are online but not on the street (/who: in the dungeon).
const FHLItem* FHLGame::SimDungeonLoot(const FHLSim& S) const
{
	const FHLData& D = FHLData::Get();
	static const TCHAR* Bosses[] = { TEXT("mob_boss_brood_keeper"), TEXT("mob_boss_hive_warden"), TEXT("mob_boss_matriarch"), TEXT("mob_boss_hive_mother") };
	static const TCHAR* Order[] = { TEXT("cloth"), TEXT("leather"), TEXT("padded"), TEXT("steel") };
	auto Rank = [](const FString& T) { for (int32 i = 0; i < 4; ++i) if (T == Order[i]) return i; return 0; };
	TArray<const FHLItem*> Fit;
	for (const TCHAR* B : Bosses)
		if (const FHLMobDef* M = D.Mob(B))
			for (const FHLItem* It : M->BossLoot)
			{
				if (!It || !It->IsEquip() || It->Req > S.Level) continue;
				if (It->Kind == TEXT("armor") && It->Type != TEXT("jewel") && Rank(It->Type) > Rank(S.P->Cls->Armour)) continue;
				if (It->Kind == TEXT("weapon") && !S.P->Cls->Weapons.Contains(It->Type)) continue;
				const FHLItem* Cur = S.Gear.FindRef(It->Slot);
				if (Cur && Cur->ILvl >= It->ILvl) continue;
				Fit.AddUnique(It);
			}
	return Fit.Num() ? Fit[FMath::RandRange(0, Fit.Num() - 1)] : nullptr;
}

void FHLGame::TickSimDungeon()
{
	const FHLUnit* P = PlayerUnit();
	if (!P || !Sims.Num()) return;
	const FHLAreaDef& Def = *Areas[0].Def;
	const FHLPortal* Door = nullptr;
	for (const FHLPortal& Po : Def.Portals) if (Po.ToArea == TEXT("dng_egyptian")) Door = &Po;
	if (!Door) return;
	static const bool bLifeTest = FParse::Param(FCommandLine::Get(), TEXT("HLLifeTest"));
	FHLSimRun& R = SimRun;
	auto Release = [&]() { for (int32 m : R.Members) if (Sims.IsValidIndex(m) && Sims[m].State == EHLSimState::Script) { Sims[m].State = EHLSimState::Idle; Sims[m].StateUntil = Now + 2; } };
	const FVector2D Outside = Areas[0].Nav.NearestFree(Door->Pos + FVector2D(0, -9));
	if (R.Stage == 0)
	{
		if (R.NextAt == 0) R.NextAt = Now + (bLifeTest ? 20 : 360 + Rand() * 240);
		if (Now < R.NextAt) return;
		R.NextAt = Now + (bLifeTest ? 400 : 480 + Rand() * 360);
		TArray<int32> On, Off;
		for (int32 i = 0; i < Sims.Num(); ++i)
		{
			const FHLSim& S = Sims[i];
			if (!S.bOnline || S.bInParty || S.DungeonUntil > Now || S.Level < 8 || S.Level > 14 || !S.P->Cls || i == DuelSim || i == TradeSim) continue;
			if (!S.UnitId) { Off.Add(i); continue; }
			const FHLUnit* U = Unit(S.UnitId);
			if (U && !U->bDead && U->Area == 0 && !InCombat(*U) && S.Role == EHLSimRole::Local && S.State != EHLSimState::Script && !S.EventRole && S.ChatUntil <= Now) On.Add(i);
		}
		if (On.Num() < 2 || On.Num() + Off.Num() < 5) return;
		On.Sort([&](int32 A, int32 B) { return FVector2D::Distance(Unit(Sims[A].UnitId)->Pos, Door->Pos) < FVector2D::Distance(Unit(Sims[B].UnitId)->Pos, Door->Pos); });
		R = FHLSimRun(); R.NextAt = Now + (bLifeTest ? 400 : 480 + Rand() * 360);
		for (int32 k = 0; k < FMath::Min(3, On.Num()); ++k) R.Members.Add(On[k]);
		while (R.Members.Num() < 5 && Off.Num()) { R.Members.Add(Off[0]); Off.RemoveAt(0); }
		const FHLExit& Ex = Def.Exits[RandInt(0, Def.Exits.Num() - 1)];
		for (int32 k = 0; k < R.Members.Num(); ++k)
		{
			const int32 m = R.Members[k];
			FHLSim& S = Sims[m];
			if (!S.UnitId) SpawnSim(m, EHLSimRole::Local, Ex.Pos + FVector2D(Rand() * 4 - 2, Rand() * 4 - 2));
			S.State = EHLSimState::Script; S.StateUntil = Now + 400; S.Route.Reset(); S.RouteIdx = 0; S.CampIdx = -1;
			const float A = 2 * PI * k / R.Members.Num();
			S.Dest = Areas[0].Nav.NearestFree(Outside + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 2.6f);
		}
		const FHLSim& Lead = Sims[R.Members[0]];
		static const TCHAR* Need[] = { TEXT("heals"), TEXT("a tank"), TEXT("1 dps"), TEXT("heals and dps") };
		AddChat(TEXT("lfg"), Lead.P->Handle, StyleText(*Lead.P, FString::Printf(TEXT("lfm egyptian theatre need %s, meet at the door"), Need[RandInt(0, 3)])), R.Members[0]);
		AddChat(TEXT("lfg"), Lead.P->Handle, StyleText(*Lead.P, TEXT("egyptian full, ty all")), R.Members[0], 25 + Rand() * 20);
		R.Stage = 1; R.At = Now + 240;
		UE_LOG(LogTemp, Display, TEXT("HL_LIFE dungeon group of %d forming (%s)"), R.Members.Num(), *Lead.P->Handle);
		return;
	}
	if (R.Stage == 1)
	{
		int32 Here = 0, Alive = 0;
		for (int32 m : R.Members)
		{
			FHLSim& S = Sims[m];
			FHLUnit* U = S.UnitId ? Unit(S.UnitId) : nullptr;
			if (!U || U->bDead || S.State != EHLSimState::Script) continue;
			++Alive;
			if (FVector2D::Distance(U->Pos, S.Dest) < 3) { ++Here; FaceTo(*U, Outside); }
		}
		if (Alive < 4 || Now > R.At)
		{
			if (Sims.IsValidIndex(R.Members[0]) && Sims[R.Members[0]].bOnline) AddChat(TEXT("lfg"), Sims[R.Members[0]].P->Handle, StyleText(*Sims[R.Members[0]].P, TEXT("nvm egyptian group fell apart")), R.Members[0], 1);
			Release(); R.Stage = 0; return;
		}
		if (Here < Alive) return;
		if (R.GoAt == 0)
		{
			R.GoAt = Now + 7;
			const int32 L = R.Members[0];
			AddChat(TEXT("say"), Sims[L].P->Handle, StyleText(*Sims[L].P, Chance(0.5f) ? TEXT("everyone here? ok lets go, dont pull the spitters") : TEXT("all here, buff up and go")), L, 1);
			if (R.Members.Num() > 1 && Chance(0.6f)) SimSay(R.Members[1], TEXT("say"), TEXT("ready"), 3, TEXT("rdy"));
			return;
		}
		if (Now < R.GoAt) return;
		R.bWin = Chance(0.7f);
		R.At = Now + (bLifeTest ? 50 : (R.bWin ? 900 + Rand() * 600 : 420 + Rand() * 300));
		for (int32 m : R.Members) { Sims[m].Dest = Door->Pos; Sims[m].DungeonUntil = R.At; }
		R.Stage = 2;
		return;
	}
	if (R.Stage == 2)
	{
		// walk in at the door one by one, then nothing until they come out
		for (int32 m : R.Members)
		{
			FHLSim& S = Sims[m];
			if (!S.UnitId) continue;
			const FHLUnit* U = Unit(S.UnitId);
			if (!U || FVector2D::Distance(U->Pos, Door->Pos) < 2.5f || U->bDead) { DespawnSim(m); S.Activity = TEXT("in The Egyptian Theatre"); }
		}
		if (Now < R.At) return;
		TArray<FString> Loots;
		for (int32 k = 0; k < R.Members.Num(); ++k)
		{
			const int32 m = R.Members[k];
			FHLSim& S = Sims[m];
			S.DungeonUntil = 0; S.Activity.Reset();
			if (!S.bOnline || S.UnitId) continue;
			FHLUnit* U = SpawnSim(m, EHLSimRole::Local, Areas[0].Nav.NearestFree(Door->Pos + FVector2D(Rand() * 4 - 2, -4 - Rand() * 3)));
			if (!U) continue;
			if (R.bWin && Chance(0.75f))
				if (const FHLItem* It = SimDungeonLoot(S))
				{
					S.Gear.Add(It->Slot, It);
					RecalcCharStats(*U, *S.P->Cls, S.Level, S.Gear, true);
					Loots.Add(FString::Printf(TEXT("[%s]"), *It->Name));
					if (S.Level < 14 && Chance(0.5f)) { S.Level++; RecalcCharStats(*U, *S.P->Cls, S.Level, S.Gear, true); }
				}
			S.StateUntil = Now + 8 + k * 3 + Rand() * 6; // stand about outside for a moment, then back to it
		}
		const int32 L = R.Members[0];
		if (Sims.IsValidIndex(L) && Sims[L].bOnline)
		{
			const FHLSim& Lead = Sims[L];
			if (R.bWin)
			{
				AddChat(TEXT("zone"), Lead.P->Handle, StyleText(*Lead.P, Chance(0.5f) ? TEXT("egyptian cleared, gg all") : TEXT("hive mother down. gg group")), L, 2);
				if (Loots.Num()) { const int32 W = R.Members[RandInt(0, R.Members.Num() - 1)]; if (Sims[W].bOnline) AddChat(TEXT("zone"), Sims[W].P->Handle, StyleText(*Sims[W].P, FString::Printf(TEXT("%s finally"), *Loots[0])), W, 7 + Rand() * 4); }
				for (int32 k = 1; k < R.Members.Num(); ++k) if (Chance(0.4f)) SimSay(R.Members[k], TEXT("say"), TEXT("duel_watch"), 3 + k * 1.5f, TEXT("gg"));
				if (CensusEvents.Num() >= 16) CensusEvents.RemoveAt(0);
				CensusEvents.Add({ FString::Printf(TEXT("%s's group cleared The Egyptian Theatre"), *Lead.P->Handle), Now });
			}
			else
			{
				AddChat(TEXT("zone"), Lead.P->Handle, StyleText(*Lead.P, Chance(0.5f) ? TEXT("wiped on the hive mother 3 times, calling it") : TEXT("egyptian is cursed. we wiped on the matriarch twice")), L, 2);
				if (R.Members.Num() > 1) SimSay(R.Members[1], TEXT("say"), TEXT("death"), 5, TEXT("rip"));
			}
		}
		UE_LOG(LogTemp, Display, TEXT("HL_LIFE dungeon group back (%s), loot: %s"), R.bWin ? TEXT("cleared") : TEXT("wiped"), *FString::Join(Loots, TEXT(", ")));
		R.Stage = 0;
		return;
	}
}

// ------------------------------------------------------------------------------------------------------------- fight night
// Announced two minutes ahead. Up to four simulated players near the player's level box in bouts outside the Roosevelt
// (semi-finals, then a final) while others watch; the champion then calls the player out. Beat the champion for a purse.
bool FHLGame::FightPair(int32& A, int32& B) const
{
	const TArray<int32>& Fi = Fight.Fighters;
	const TArray<int32>& W = Fight.Winners;
	A = B = -1;
	if (Fight.Bout == 0 && Fi.Num() >= 2) { A = Fi[0]; B = Fi[1]; }
	else if (Fi.Num() >= 4 && Fight.Bout == 1) { A = Fi[2]; B = Fi[3]; }
	else if (Fi.Num() >= 4 && Fight.Bout == 2 && W.Num() >= 2) { A = W[0]; B = W[1]; }
	else if (Fi.Num() == 3 && Fight.Bout == 1 && W.Num() >= 1) { A = W[0]; B = Fi[2]; }
	return A >= 0 && B >= 0;
}

void FHLGame::PlaceFightCrowd(const FVector2D& Ring)
{
	int32 A, B;
	const bool bPair = FightPair(A, B);
	TArray<int32> Ringside;
	for (int32 SI : Fight.Fighters) if (SI != A && SI != B) Ringside.Add(SI);
	Ringside.Append(Fight.Watchers);
	const FHLNav& Nav = Areas[0].Nav;
	if (bPair) { Sims[A].Dest = Nav.NearestFree(Ring + FVector2D(-2.8f, 0)); Sims[B].Dest = Nav.NearestFree(Ring + FVector2D(2.8f, 0)); }
	for (int32 k = 0; k < Ringside.Num(); ++k)
	{
		const float Ang = 2 * PI * (k + 0.5f) / FMath::Max(1, Ringside.Num()) + PI * 0.5f;
		Sims[Ringside[k]].Dest = Nav.NearestFree(Ring + FVector2D(FMath::Cos(Ang) * 7.f, FMath::Sin(Ang) * 6.f));
	}
}

void FHLGame::EndFightNight(const TCHAR* Why)
{
	for (int32 SI : Fight.Fighters) if (Sims.IsValidIndex(SI) && Sims[SI].State == EHLSimState::Script) { Sims[SI].State = EHLSimState::Rest; Sims[SI].StateUntil = Now + 10; }
	for (int32 SI : Fight.Watchers) if (Sims.IsValidIndex(SI) && Sims[SI].State == EHLSimState::Script) { Sims[SI].State = EHLSimState::Idle; Sims[SI].StateUntil = Now + 2 + Rand() * 5; }
	static const bool bLifeTest = FParse::Param(FCommandLine::Get(), TEXT("HLLifeTest"));
	const double Next = Now + (bLifeTest ? 600 : 2400 + Rand() * 1200);
	UE_LOG(LogTemp, Display, TEXT("HL_LIFE fight night over (%s)"), Why);
	Fight = FHLFight();
	Fight.NextAt = Next;
}

void FHLGame::TickFightNight()
{
	FHLUnit* P = PlayerUnit();
	const FVector2D* Inn = Areas[0].Def->Pois.Find(TEXT("inn"));
	if (!P || !Inn || !Sims.Num()) return;
	static const bool bLifeTest = FParse::Param(FCommandLine::Get(), TEXT("HLLifeTest"));
	FHLFight& F = Fight;
	const FVector2D Ring = Areas[0].Nav.NearestFree(*Inn + FVector2D(0, -10));
	F.RingPos = Ring;
	const bool bNear = P->Area == 0 && FVector2D::Distance(P->Pos, Ring) < 60;
	if (F.Stage == 0)
	{
		if (F.NextAt == 0) F.NextAt = Now + (bLifeTest ? 150 : 1200 + Rand() * 900);
		if (!F.bAnnounced && Now >= F.NextAt - 120 && SurgeHub < 0)
		{
			F.bAnnounced = true;
			System(TEXT("Fight night: bouts outside the Roosevelt in 2 minutes. The champion takes on all comers."));
			for (int32 Try = 0; Try < 30; ++Try) { const int32 SI = RandInt(0, Sims.Num() - 1); if (Sims[SI].bOnline && !Sims[SI].bInParty && Sims[SI].Level <= 20) { AddChat(TEXT("zone"), Sims[SI].P->Handle, StyleText(*Sims[SI].P, Chance(0.5f) ? TEXT("fight night at the roosevelt, who's in") : TEXT("fight night soon, outside the roosevelt. bring money for bets lol")), SI, 3); break; } }
		}
		if (!F.bAnnounced || Now < F.NextAt) return;
		if (SurgeHub >= 0 || Happen.Kind) { F.NextAt = Now + 30; return; }
		// fighters near the player's level, nearest the ring first; then a crowd
		TArray<int32> Free;
		for (int32 i = 0; i < Sims.Num(); ++i)
		{
			const FHLSim& S = Sims[i];
			if (!S.UnitId || S.bInParty || S.bAfk || S.Role == EHLSimRole::Idler || S.EventRole || S.State == EHLSimState::Script || S.State == EHLSimState::Gather || S.DungeonUntil > Now || i == DuelSim || i == TradeSim) continue;
			const FHLUnit* U = Unit(S.UnitId);
			if (U && !U->bDead && U->Area == 0 && !InCombat(*U) && FVector2D::Distance(U->Pos, Ring) < 260) Free.Add(i);
		}
		Free.Sort([&](int32 A, int32 B) { return FMath::Abs(Sims[A].Level - P->Level) * 20 + FVector2D::Distance(Unit(Sims[A].UnitId)->Pos, Ring) < FMath::Abs(Sims[B].Level - P->Level) * 20 + FVector2D::Distance(Unit(Sims[B].UnitId)->Pos, Ring); });
		if (Free.Num() < 2) { EndFightNight(TEXT("nobody came")); return; }
		for (int32 k = 0; k < Free.Num() && F.Fighters.Num() < 4; ++k) if (FMath::Abs(Sims[Free[k]].Level - P->Level) <= 4 || F.Fighters.Num() < 2) F.Fighters.Add(Free[k]);
		for (int32 SI : Free) if (!F.Fighters.Contains(SI) && F.Watchers.Num() < 5) F.Watchers.Add(SI);
		for (int32 SI : F.Fighters) { FHLSim& S = Sims[SI]; S.State = EHLSimState::Script; S.StateUntil = Now + 900; S.Route.Reset(); S.RouteIdx = 0; }
		for (int32 SI : F.Watchers) { FHLSim& S = Sims[SI]; S.State = EHLSimState::Script; S.StateUntil = Now + 900; S.Route.Reset(); S.RouteIdx = 0; }
		F.Stage = 1; F.Bout = 0; F.BoutStart = 0; F.Until = Now + 600;
		PlaceFightCrowd(Ring);
		if (bNear) Banner(TEXT("Fight Night"), TEXT("Outside the Roosevelt Hotel"), 4);
		UE_LOG(LogTemp, Display, TEXT("HL_LIFE fight night: %d fighters, %d watching"), F.Fighters.Num(), F.Watchers.Num());
		return;
	}
	if (Now > F.Until) { EndFightNight(TEXT("timed out")); return; }
	auto Ready = [&](int32 SI, FHLUnit*& U) { U = Sims.IsValidIndex(SI) ? Unit(Sims[SI].UnitId) : nullptr; return U && !U->bDead && Sims[SI].State == EHLSimState::Script && FVector2D::Distance(U->Pos, Sims[SI].Dest) < 2.5f; };
	if (F.Stage == 1)
	{
		if (Now < F.BeatAt) return;
		F.BeatAt = Now + 1.3f + Rand() * 0.6f;
		for (int32 SI : F.Watchers) { FHLUnit* W = nullptr; if (Ready(SI, W)) FaceTo(*W, Ring); }
		int32 A, B;
		if (!FightPair(A, B)) { F.Stage = 2; F.StageAt = Now + 3; return; }
		FHLUnit* UA = nullptr; FHLUnit* UB = nullptr;
		const bool bA = Ready(A, UA), bB = Ready(B, UB);
		for (int32 SI : F.Fighters) if (SI != A && SI != B) { FHLUnit* W = nullptr; if (Ready(SI, W)) FaceTo(*W, Ring); }
		if (!bA || !bB)
		{
			// a fighter who can't make it forfeits
			if ((UA && UA->bDead) || (UB && UB->bDead) || !Sims[A].UnitId || !Sims[B].UnitId) { F.Winners.Add(UA && !UA->bDead && Sims[A].UnitId ? A : B); ++F.Bout; PlaceFightCrowd(Ring); }
			return;
		}
		FaceTo(*UA, UB->Pos); FaceTo(*UB, UA->Pos);
		if (F.BoutStart == 0)
		{
			F.BoutStart = Now + 3.5;
			UA->Hp = UA->MaxHp; UB->Hp = UB->MaxHp;
			SoundAt(TEXT("ev_bell"), 0, Ring);
			const TCHAR* Name = F.Fighters.Num() >= 4 ? (F.Bout == 2 ? TEXT("The final") : TEXT("A semi-final")) : (F.Fighters.Num() == 3 && F.Bout == 1 ? TEXT("The final") : TEXT("The bout"));
			if (bNear) AddChat(TEXT("system"), FString(), FString::Printf(TEXT("%s: %s (level %d) against %s (level %d)."), Name, *Sims[A].P->Handle, Sims[A].Level, *Sims[B].P->Handle, Sims[B].Level));
			return;
		}
		if (Now < F.BoutStart) return;
		if (F.bBoutDone)
		{
			if (Now >= F.NextBout) { F.bBoutDone = false; F.BoutStart = 0; ++F.Bout; PlaceFightCrowd(Ring); }
			return;
		}
		FHLUnit* Att = Chance(0.5f) ? UA : UB; FHLUnit* Def2 = Att == UA ? UB : UA;
		Att->HopT = 0.3f;
		const float Hit = Def2->MaxHp * (0.05f + Rand() * 0.07f) * FMath::Clamp((float)Att->Level / FMath::Max(1, Def2->Level), 0.6f, 1.7f);
		if (Def2->Hp - Hit <= Def2->MaxHp * 0.08f)
		{
			Def2->Hp = FMath::Max(1.f, Def2->MaxHp * 0.08f);
			const int32 WinSI = Att == UA ? A : B, LoseSI = Att == UA ? B : A;
			F.Winners.Add(WinSI); F.bBoutDone = true; F.NextBout = Now + 7;
			SoundAt(TEXT("ev_bell"), 0, Ring); SoundAt(TEXT("ev_cheer"), 0, Ring);
			if (bNear) System(FString::Printf(TEXT("%s has defeated %s."), *Sims[WinSI].P->Handle, *Sims[LoseSI].P->Handle));
			for (int32 SI : F.Watchers) if (Chance(0.3f)) SimSay(SI, TEXT("say"), TEXT("duel_watch"), 1 + Rand() * 3);
			SimSay(LoseSI, TEXT("say"), TEXT("duel_sim_lost"), 2.5f + Rand());
			return;
		}
		Sound(TEXT("hit_fist"), *Def2);
		DealDamage(Att, *Def2, Hit, TEXT("physical"), Chance(0.15f), TEXT("punch"));
		if (bNear && Chance(0.15f) && F.Watchers.Num()) { static const TCHAR* Cheer[] = { TEXT("cheers."), TEXT("whistles."), TEXT("shouts encouragement."), TEXT("winces.") }; { const int32 W = F.Watchers[RandInt(0, F.Watchers.Num() - 1)]; const int32 Ci = RandInt(0, 3); AddChat(TEXT("emote"), FString(), Sims[W].P->Handle + TEXT(" ") + Cheer[Ci]); if (FHLUnit* Wu = Unit(Sims[W].UnitId)) Gesture(*Wu, Ci == 3 ? TEXT("Shrug") : TEXT("Cheer")); } }
		return;
	}
	if (F.Stage == 2)
	{
		// the champion calls the player out
		if (Now < F.StageAt) return;
		F.Champ = F.Winners.Num() ? F.Winners.Last() : -1;
		if (!Sims.IsValidIndex(F.Champ) || !Sims[F.Champ].UnitId) { EndFightNight(TEXT("no champion")); return; }
		const FHLSim& C = Sims[F.Champ];
		FHLUnit* CU = Unit(C.UnitId);
		CU->Hp = CU->MaxHp;
		if (bNear) Banner(TEXT("Fight Night"), FString::Printf(TEXT("%s is the champion"), *C.P->Handle), 4);
		if (bNear && !P->bDead && DuelState == 0 && !InCombat(*P) && FMath::Abs(C.Level - P->Level) <= 6)
		{
			AddChat(TEXT("say"), C.P->Handle, StyleText(*C.P, FString::Printf(TEXT("who's next? %s, you want some?"), *Player.Name)), F.Champ, 1);
			SimChallengesDuel(F.Champ);
			F.Stage = 3; F.Until = Now + 150;
			return;
		}
		AddChat(TEXT("zone"), C.P->Handle, StyleText(*C.P, TEXT("fight night champ, undefeated. who wants it next time")), F.Champ, 3);
		EndFightNight(TEXT("champion crowned"));
		return;
	}
	if (F.Stage == 3)
	{
		// the duel itself runs through the normal duel system; EndDuel pays the purse
		if (DuelFromSim == F.Champ || DuelState != 0) return;
		EndFightNight(TEXT("challenge answered"));
	}
}

void FHLGame::FightNightDuelOver(bool bPlayerWon)
{
	if (Fight.Stage != 3) return;
	FHLUnit* P = PlayerUnit();
	if (bPlayerWon && P)
	{
		const int32 Purse = 50 + 20 * P->Level;
		Player.Cents += Purse;
		Banner(TEXT("Fight Night"), TEXT("You are the champion"), 4);
		System(FString::Printf(TEXT("You win the fight night purse: %s."), *HLMoney(Purse)));
		for (int32 SI : Fight.Watchers) if (Chance(0.4f)) SimSay(SI, TEXT("say"), TEXT("duel_watch"), 1 + Rand() * 3);
	}
	else for (int32 SI : Fight.Watchers) if (Chance(0.3f)) SimSay(SI, TEXT("say"), TEXT("duel_watch"), 1 + Rand() * 3);
}
