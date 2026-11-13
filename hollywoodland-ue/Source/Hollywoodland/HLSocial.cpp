// Social systems: mail, friends list, persistent bonds with a few simulated players (DESIGN-BIBLE §11 relationships),
// duels, face-to-face trades and cash-on-delivery sales from trade-chat adverts, and auction bids.
#include "HLGame.h"

// ------------------------------------------------------------------------------------------------------------- mail
void FHLGame::SendMail(const FString& From, const FString& Subject, const FString& Body, int32 Cents, const FHLItem* Item, int32 Count, int32 Cod)
{
	FHLMail M; M.From = From; M.Subject = Subject; M.Body = Body; M.Cents = Cents; M.Item = Item; M.Count = Item ? FMath::Max(1, Count) : 0; M.Cod = Cod;
	Player.Mail.Insert(M, 0);
	if (Player.Mail.Num() > 50) Player.Mail.SetNum(50);
	System(TEXT("You have new mail."));
}

int32 FHLGame::UnreadMail() const { int32 N = 0; for (const FHLMail& M : Player.Mail) if (!M.bRead) ++N; return N; }

void FHLGame::TakeMail(int32 Idx)
{
	if (!Player.Mail.IsValidIndex(Idx)) return;
	FHLMail& M = Player.Mail[Idx];
	M.bRead = true;
	if (M.Item)
	{
		if (M.Cod > 0 && Player.Cents < M.Cod) { Error(TEXT("You can't afford the C.O.D. charge.")); return; }
		if (!AddItem(M.Item, M.Count, true)) return;
		if (M.Cod > 0)
		{
			Player.Cents -= M.Cod;
			const int32 SI = FindSimByHandle(M.From);
			if (SI >= 0) { Relation(Sims[SI]).Familiarity++; if (Sims[SI].bOnline) { ChatCtx = M.Item->Name; SimSay(SI, TEXT("whisper"), TEXT("trade_done"), 4 + Rand() * 10, TEXT("ty, enjoy")); ChatCtx.Reset(); } }
		}
		M.Item = nullptr; M.Count = 0; M.Cod = 0;
	}
	if (M.Cents > 0) { Player.Cents += M.Cents; System(FString::Printf(TEXT("You take %s from the letter."), *HLMoney(M.Cents))); M.Cents = 0; }
}

void FHLGame::DeleteMail(int32 Idx)
{
	if (!Player.Mail.IsValidIndex(Idx)) return;
	const FHLMail& M = Player.Mail[Idx];
	if (M.Cod > 0 && M.Item) { const int32 SI = FindSimByHandle(M.From); if (SI >= 0 && Sims[SI].bOnline) SimSay(SI, TEXT("whisper"), TEXT("trade_refused"), 6, TEXT("no worries, ill relist it")); }
	else if (M.Item || M.Cents > 0) { Error(TEXT("Take the contents first.")); return; }
	Player.Mail.RemoveAt(Idx);
}

// ------------------------------------------------------------------------------------------------------------- friends
bool FHLGame::IsFriend(const FHLSim& S) const { return Player.Friends.Contains(S.P->Id); }

void FHLGame::AddFriend(int32 SimIdx, bool bQuiet)
{
	if (!Sims.IsValidIndex(SimIdx) || IsFriend(Sims[SimIdx])) return;
	if (Player.Friends.Num() >= 50) { Error(TEXT("Your friends list is full.")); return; }
	Player.Friends.Add(Sims[SimIdx].P->Id);
	Relation(Sims[SimIdx]).Familiarity++;
	if (!bQuiet) System(FString::Printf(TEXT("%s added to friends."), *Sims[SimIdx].P->Handle));
}

void FHLGame::RemoveFriend(int32 SimIdx)
{
	if (!Sims.IsValidIndex(SimIdx)) return;
	if (Player.Friends.Remove(Sims[SimIdx].P->Id)) System(FString::Printf(TEXT("%s removed from friends list."), *Sims[SimIdx].P->Handle));
}

// ------------------------------------------------------------------------------------------------------------- bonds
// On a new save a handful of personas become this player's regulars, chosen from the player id so every save differs:
// pals near the player's level, a rival, a flirt, a high-level mentor and a gossip. They whisper, mail, challenge and
// befriend over time; other personas stay strangers unless the player makes friends with them.
void FHLGame::SeedBonds()
{
	for (const auto& KV : Player.Relations) if (!KV.Value.Bond.IsEmpty()) return;
	FHLUnit* P = PlayerUnit();
	if (!P || !Sims.Num()) return;
	FRandomStream R(GetTypeHash(Player.PlayerId));
	TSet<int32> Used;
	auto Pick = [&](int32 MinL, int32 MaxL, const TCHAR* Bond)
	{
		for (int32 Try = 0; Try < 400; ++Try)
		{
			const int32 i = R.RandRange(0, Sims.Num() - 1);
			const FHLSim& S = Sims[i];
			if (Used.Contains(i) || !S.P->Cls || S.Level < MinL || S.Level > MaxL || S.P->Temperament == TEXT("quiet")) continue;
			Used.Add(i); Relation(S).Bond = Bond; return;
		}
	};
	const int32 L = P->Level;
	for (int32 k = 0; k < 3; ++k) Pick(FMath::Max(1, L - 2), L + 4, TEXT("pal"));
	Pick(FMath::Max(1, L - 1), L + 3, TEXT("rival"));
	Pick(FMath::Max(1, L - 2), L + 8, TEXT("flirt"));
	Pick(50, 60, TEXT("mentor"));
	Pick(15, 60, TEXT("gossip"));
}

int32 FHLGame::BondedAwaySim(int32 MinLevel, int32 MaxLevel)
{
	TArray<int32> C;
	for (int32 i = 0; i < Sims.Num(); ++i)
	{
		const FHLSim& S = Sims[i];
		const FHLRelation* R = Player.Relations.Find(S.P->Id);
		if (R && !R->Bond.IsEmpty() && S.bOnline && !S.UnitId && S.Level >= MinLevel && S.Level <= MaxLevel && S.P->Cls) C.Add(i);
	}
	return C.Num() ? C[RandInt(0, C.Num() - 1)] : -1;
}

void FHLGame::TickSocial(float Dt)
{
	if (Now < NextSocial) return;
	NextSocial = Now + 5;
	FHLUnit* P = PlayerUnit();
	if (!P || !Sims.Num()) return;
	SeedBonds();
	if (DuelFromSim >= 0 && Now >= DuelInviteUntil) { DuelFromSim = -1; }
	// expire stale trade adverts
	for (auto It = SimWts.CreateIterator(); It; ++It) if (Now - It.Value().At > 900) It.RemoveCurrent();
	if (Player.PlayedS < (bEventsSoon ? 15 : 90)) return; // let the first minute belong to the Director
	for (int32 i = 0; i < Sims.Num(); ++i)
	{
		FHLSim& S = Sims[i];
		FHLRelation* R = Player.Relations.Find(S.P->Id);
		if (!R || R->Bond.IsEmpty() || !S.bOnline || S.bInParty || S.bAfk) continue;
		double& Next = BondNext.FindOrAdd(S.P->Id);
		if (Next == 0) { Next = Now + (bEventsSoon ? 5 + Rand() * 40 : 60 + Rand() * 400); continue; }
		if (Now < Next) continue;
		Next = Now + (bEventsSoon ? 60 + Rand() * 60 : 600 + Rand() * 600);
		const FString& B = R->Bond;
		auto Whisper = [&](const TCHAR* Situation, float Delay = 1) { SimSay(i, TEXT("whisper"), Situation, Delay); LastWhisperFrom = S.P->Handle; R->Familiarity++; };
		if (B == TEXT("pal"))
		{
			if (R->Familiarity >= 3 && !IsFriend(S)) { Whisper(TEXT("bond_befriend")); AddFriend(i, true); System(FString::Printf(TEXT("%s added you as a friend. (Added to your friends list.)"), *S.P->Handle)); }
			else Whisper(TEXT("bond_pal"));
		}
		else if (B == TEXT("rival"))
		{
			const FHLUnit* U = Unit(S.UnitId);
			if (U && Dist(*U, *P) < 45 && DuelState == 0 && DuelFromSim < 0 && !InCombat(*P) && CanDuelHere() && Chance(0.6f)) SimChallengesDuel(i);
			else if (Chance(0.5f)) Whisper(TEXT("bond_rival"));
			else { ChatCtx = Player.Name; SimSay(i, TEXT("zone"), TEXT("bond_rival_zone"), 1); ChatCtx.Reset(); }
		}
		else if (B == TEXT("flirt")) Whisper(TEXT("bond_flirt"));
		else if (B == TEXT("gossip")) Whisper(TEXT("bond_gossip"));
		else if (B == TEXT("mentor"))
		{
			if (!R->Facts.Contains(TEXT("sent_gift")) && R->Familiarity >= 1)
			{
				const TArray<const FHLItem*>& Pool = FHLData::Get().EquipPool(FMath::Clamp(P->Level + 1, 2, 12), 2);
				TArray<const FHLItem*> Fit;
				for (const FHLItem* It : Pool) if (CanEquip(*It) && It->Req <= P->Level + 1) Fit.Add(It);
				const FHLItem* Gift = Fit.Num() ? Fit[RandInt(0, Fit.Num() - 1)] : nullptr;
				SendMail(S.P->Handle, TEXT("Something for the road"), TEXT("Saw this on the AH and thought of you. Don't spend the rest on gin.\n\nYou'll be fine. Everyone dies to the Hive Mother the first time."), 100 + 25 * P->Level, Gift, 1);
				R->Facts.Add(TEXT("sent_gift"));
				Whisper(TEXT("bond_mentor_gift"), 2);
			}
			else Whisper(TEXT("bond_mentor"));
		}
	}
}

// ------------------------------------------------------------------------------------------------------------- duels
bool FHLGame::CanDuelHere() const
{
	const FHLUnit* P = PlayerUnit();
	if (!P || P->Area != 0) return false;
	for (const FHLHub& H : Hubs) if (FVector2D::Distance(H.Pos, P->Pos) < 35) return false;
	return true;
}

void FHLGame::ChallengeDuel(int32 SimIdx)
{
	FHLUnit* P = PlayerUnit();
	if (!P || !Sims.IsValidIndex(SimIdx)) return;
	FHLSim& S = Sims[SimIdx];
	FHLUnit* U = Unit(S.UnitId);
	if (DuelState != 0) { Error(TEXT("You are already dueling.")); return; }
	if (!U || U->bDead || Dist(*U, *P) > 30) { Error(TEXT("You are too far away.")); return; }
	if (S.bInParty) { Error(TEXT("You can't duel a party member.")); return; }
	if (!CanDuelHere()) { Error(TEXT("You can't duel here. Try away from the hubs.")); return; }
	if (InCombat(*P) || InCombat(*U)) { Error(TEXT("You can't duel while in combat.")); return; }
	System(FString::Printf(TEXT("You have challenged %s to a duel."), *S.P->Handle));
	// higher sims usually humour it; the rival always says yes; busy grinders mostly decline
	const FHLRelation* R = Player.Relations.Find(S.P->Id);
	float Yes = S.State == EHLSimState::Grind ? 0.35f : 0.65f;
	if (R && R->Bond == TEXT("rival")) Yes = 1;
	if (S.Level > P->Level + 8) Yes *= 0.6f;
	if (!Chance(Yes))
	{
		SimSay(SimIdx, TEXT("say"), TEXT("duel_decline"), 1.5f, TEXT("nah"));
		System(FString::Printf(TEXT("%s declines your challenge to a duel."), *S.P->Handle));
		return;
	}
	SimSay(SimIdx, TEXT("say"), TEXT("duel_accept"), 1.2f, TEXT("ok"));
	StartDuel(SimIdx);
}

void FHLGame::SimChallengesDuel(int32 SimIdx)
{
	DuelFromSim = SimIdx; DuelInviteUntil = Now + 30;
	ChatCtx = Player.Name;
	SimSay(SimIdx, TEXT("say"), TEXT("duel_challenge"), 0.5f, TEXT("duel?"));
	ChatCtx.Reset();
	System(FString::Printf(TEXT("%s has challenged you to a duel."), *Sims[SimIdx].P->Handle));
}

void FHLGame::AnswerDuel(bool bAccept)
{
	const int32 SimIdx = DuelFromSim;
	DuelFromSim = -1;
	if (!Sims.IsValidIndex(SimIdx)) return;
	FHLUnit* U = Unit(Sims[SimIdx].UnitId);
	FHLUnit* P = PlayerUnit();
	if (!bAccept || !U || !P || Dist(*U, *P) > 40 || DuelState != 0 || InCombat(*P))
	{
		if (!bAccept) SimSay(SimIdx, TEXT("say"), TEXT("duel_chicken"), 1.5f, TEXT("chicken"));
		return;
	}
	StartDuel(SimIdx);
}

void FHLGame::StartDuel(int32 SimIdx)
{
	FHLUnit* P = PlayerUnit();
	FHLUnit* U = Unit(Sims[SimIdx].UnitId);
	if (!P || !U) return;
	DuelSim = SimIdx; DuelState = 1; DuelAt = Now + 3; DuelFlag = (P->Pos + U->Pos) * 0.5f;
	Banner(TEXT("Duel"), FString::Printf(TEXT("You versus %s. Starting in 3..."), *Sims[SimIdx].P->Handle), 3);
}

void FHLGame::EndDuel(int32 WinnerUnitId, const TCHAR* How)
{
	if (DuelState == 0 || !Sims.IsValidIndex(DuelSim)) { DuelState = 0; DuelSim = -1; return; }
	FHLSim& S = Sims[DuelSim];
	FHLUnit* P = PlayerUnit();
	FHLUnit* U = Unit(S.UnitId);
	const bool bWon = P && WinnerUnitId == P->Id;
	for (FHLUnit* X : { P, U }) if (X)
	{
		X->bAutoAttack = false; X->Casting = nullptr; X->TargetId = 0; X->CombatUntil = 0;
		X->Auras.RemoveAll([&](const FHLAura& A) { return A.bHarmful && (A.SourceId == (P ? P->Id : 0) || A.SourceId == (U ? U->Id : 0)); });
	}
	const FString Winner = bWon ? Player.Name : S.P->Handle, Loser = bWon ? S.P->Handle : Player.Name;
	System(FString::Printf(TEXT("%s has %s %s in a duel."), *Winner, How, *Loser));
	FHLRelation& R = Relation(S);
	R.Familiarity++;
	if (R.Bond == TEXT("rival")) SimSay(DuelSim, TEXT("say"), bWon ? TEXT("duel_lose_rival") : TEXT("duel_win_rival"), 1.5f);
	else SimSay(DuelSim, TEXT("say"), bWon ? TEXT("duel_lose") : TEXT("duel_win"), 1.5f, TEXT("gg"));
	if (bWon) R.Facts.AddUnique(TEXT("lost a duel to them"));
	DuelState = 0; DuelSim = -1;
}

void FHLGame::TickDuel()
{
	if (DuelState == 0) return;
	FHLUnit* P = PlayerUnit();
	FHLUnit* U = Sims.IsValidIndex(DuelSim) ? Unit(Sims[DuelSim].UnitId) : nullptr;
	if (!P || !U || P->bDead || U->bDead || U->Area != P->Area) { DuelState = 0; DuelSim = -1; return; }
	if (DuelState == 1 && Now >= DuelAt) { DuelState = 2; Banner(TEXT("Duel"), TEXT("Fight!"), 1.5f); P->TargetId = U->Id; }
	if (DuelState == 2)
	{
		if (FVector2D::Distance(P->Pos, DuelFlag) > 40) EndDuel(U->Id, TEXT("won by forfeit against"));
		else if (FVector2D::Distance(U->Pos, DuelFlag) > 40) EndDuel(P->Id, TEXT("won by forfeit against"));
	}
}

// ------------------------------------------------------------------------------------------------------------- trade
void FHLGame::NoteWts(int32 SimIdx, const FHLItem* Item, int32 Price)
{
	if (!Item || !Sims.IsValidIndex(SimIdx)) return;
	FHLTradeOffer O; O.Item = Item; O.Count = Item->Stack > 1 ? 20 : 1; O.Price = Price > 0 ? Price : FMath::Max(2, MarketPrice(*Item)); O.At = Now;
	SimWts.Add(SimIdx, O);
}

// A whisper to someone selling in trade chat: "wtb", "how much", "ill take it", or the item's name.
bool FHLGame::HandleWtbWhisper(int32 SimIdx, const FString& Text)
{
	const FHLTradeOffer* O = SimWts.Find(SimIdx);
	if (!O || !O->Item) return false;
	const FString T = Text.ToLower();
	static const TCHAR* Keys[] = { TEXT("wtb"), TEXT("buy"), TEXT("how much"), TEXT("still have"), TEXT("take it"), TEXT("interested"), TEXT("cod"), TEXT("c.o.d"), TEXT("i'll take"), TEXT("ill take"), TEXT("price") };
	bool bHit = T.Contains(O->Item->Name.ToLower());
	for (const TCHAR* K : Keys) if (T.Contains(K)) bHit = true;
	if (!bHit) return false;
	const FHLSim& S = Sims[SimIdx];
	const FString Msg = FString::Printf(TEXT("sure, sending [%s] cod %s. check your mail"), *O->Item->Name, *HLMoney(O->Price));
	AddChat(TEXT("whisper"), S.P->Handle, StyleText(*S.P, Msg), SimIdx, 3 + Rand() * 4);
	LastWhisperFrom = S.P->Handle;
	PendingMail.Add({ Now + 12 + Rand() * 20, S.P->Handle, O->Item, O->Count, O->Price });
	SimWts.Remove(SimIdx);
	return true;
}

void FHLGame::OpenTrade(int32 SimIdx)
{
	FHLUnit* P = PlayerUnit();
	if (!P || !Sims.IsValidIndex(SimIdx)) return;
	const FHLUnit* U = Unit(Sims[SimIdx].UnitId);
	if (!U || Dist(*U, *P) > 11) { Error(TEXT("You are too far away to trade.")); return; }
	if (InCombat(*P)) { Error(TEXT("You can't trade while in combat.")); return; }
	TradeSim = SimIdx; TradeBag.Reset(); TradeCents = 0;
	const FHLTradeOffer* O = SimWts.Find(SimIdx);
	TradeCents = O ? O->Price : 0;
}

void FHLGame::AcceptTrade()
{
	if (!Sims.IsValidIndex(TradeSim)) { TradeSim = -1; return; }
	FHLSim& S = Sims[TradeSim];
	const FHLTradeOffer* O = SimWts.Find(TradeSim);
	int32 Value = TradeCents;
	for (int32 B : TradeBag) if (Player.Bag.IsValidIndex(B) && Player.Bag[B].Item) Value += MarketPrice(*Player.Bag[B].Item) * Player.Bag[B].Count;
	if (TradeCents > Player.Cents) { Error(TEXT("You don't have that much money.")); return; }
	if (O && Value < O->Price * 0.85f)
	{
		SimSay(TradeSim, TEXT("say"), TEXT("trade_lowball"), 1, TEXT("lowball lol"));
		return; // window stays open so the player can raise the offer
	}
	if (!O && Value <= 0) { TradeSim = -1; return; }
	// hand over the player's side
	Player.Cents -= TradeCents;
	for (int32 B : TradeBag) if (Player.Bag.IsValidIndex(B)) Player.Bag[B] = FHLInvSlot();
	FHLRelation& R = Relation(S);
	if (O)
	{
		AddItem(O->Item, O->Count, true);
		System(FString::Printf(TEXT("Trade complete with %s."), *S.P->Handle));
		ChatCtx = O->Item->Name; SimSay(TradeSim, TEXT("say"), TEXT("trade_done"), 1, TEXT("ty")); ChatCtx.Reset();
		SimWts.Remove(TradeSim);
		R.Familiarity++;
	}
	else
	{
		// a gift
		System(FString::Printf(TEXT("You gave %s a gift."), *S.P->Handle));
		SimSay(TradeSim, TEXT("say"), TEXT("trade_gift"), 1, TEXT("oh wow ty"));
		R.Liking = FMath::Min(10, R.Liking + (Value >= 50 ? 2 : 1)); R.Familiarity++;
		R.Facts.AddUnique(TEXT("gave me a gift"));
	}
	TradeSim = -1; TradeBag.Reset(); TradeCents = 0;
}

// ------------------------------------------------------------------------------------------------------------- auction bids
int32 FHLGame::NextBid(const FHLAuction& A) const { return A.Bid > 0 ? A.Bid + FMath::Max(1, A.Bid / 20) : A.MinBid; }

bool FHLGame::BidAuction(int32 Idx)
{
	if (!Auctions.IsValidIndex(Idx)) return false;
	FHLAuction& A = Auctions[Idx];
	if (A.bMine || A.bPlayerBid) return false;
	const int32 Amt = NextBid(A);
	if (Amt >= A.Buyout) return BuyAuction(Idx);
	if (Player.Cents < Amt) { Error(TEXT("You can't afford that.")); return false; }
	Player.Cents -= Amt;
	A.Bid = Amt; A.bPlayerBid = true;
	System(FString::Printf(TEXT("Bid accepted: %s on [%s]."), *HLMoney(Amt), *A.Item->Name));
	return true;
}

void FHLGame::TickPendingMail()
{
	for (int32 i = PendingMail.Num() - 1; i >= 0; --i)
	{
		const FPendingCod& C = PendingMail[i];
		if (Now < C.At) continue;
		SendMail(C.From, FString::Printf(TEXT("C.O.D.: %s"), *C.Item->Name), TEXT("as promised. pay on delivery"), 0, C.Item, C.Count, C.Price);
		PendingMail.RemoveAt(i);
	}
}
