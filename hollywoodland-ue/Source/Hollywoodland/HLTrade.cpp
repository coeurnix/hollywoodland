// Face-to-face trades with simulated players. Each side puts up goods and money; the simulated player values everything
// from their own point of view (an upgrade they can wear is worth more to them than its market price, gear their class can't
// use only its resale value), wants a margin that depends on their greed and on how they feel about you, and accepts,
// counters with the gap, or loses patience. Now and then a passer-by who could use something in your bags offers to buy it.
#include "HLGame.h"

namespace
{
	// per persona, stable: 0.95 (easy-going) .. 1.15 (haggler)
	float Greed(const FHLPersona& P) { return 0.95f + (GetTypeHash(P.Id) % 21) * 0.01f; }
	int32 ArmourRank(const FString& T) { static const TCHAR* Order[] = { TEXT("cloth"), TEXT("leather"), TEXT("padded"), TEXT("steel") }; for (int32 i = 0; i < 4; ++i) if (T == Order[i]) return i; return 0; }
}

bool FHLGame::SimCanUse(const FHLSim& S, const FHLItem& It) const
{
	const FHLClass* C = S.P ? S.P->Cls : nullptr;
	if (!C || !It.IsEquip() || It.Req > S.Level) return false;
	if (It.Kind == TEXT("armor") && It.Type != TEXT("jewel") && ArmourRank(It.Type) > ArmourRank(C->Armour)) return false;
	if (It.Kind == TEXT("weapon") && !C->Weapons.Contains(It.Type)) return false;
	return true;
}

// What one of It is worth to the simulated player: receiving it (bGetting) or handing it over.
int32 FHLGame::SimWorth(int32 SimIdx, const FHLItem& It, bool bGetting) const
{
	const FHLSim& S = Sims[SimIdx];
	const float Base = (float)MarketPrice(It);
	if (!bGetting) return FMath::RoundToInt32(Base * 1.05f * Greed(*S.P));
	if (It.Kind == TEXT("junk") || It.Kind == TEXT("quest")) return It.Kind == TEXT("quest") ? 0 : It.Price;
	if (It.IsEquip())
	{
		if (!SimCanUse(S, It)) return FMath::RoundToInt32(Base * 0.7f); // to sell on
		const FHLItem* Worn = S.Gear.FindRef(It.Slot);
		if (!Worn && S.P) Worn = S.P->Equipped.FindRef(It.Slot);
		return FMath::RoundToInt32(Base * (!Worn || It.ILvl > Worn->ILvl + 1 ? 1.4f : 0.85f));
	}
	if (It.Kind == TEXT("consumable") || It.Kind == TEXT("ammo")) return FMath::RoundToInt32(Base * (It.Req <= S.Level ? 1.f : 0.7f));
	return FMath::RoundToInt32(Base * 0.9f); // trade goods: crafting materials
}

// The margin they want: what they get must reach (what they give) x this.
static float TradeRatio(const FHLPersona& P, const FHLRelation* R)
{
	float K = Greed(P);
	if (R)
	{
		if (R->Bond == TEXT("rival")) K *= 1.25f;
		else if (R->Bond == TEXT("pal") || R->Bond == TEXT("flirt") || R->Bond == TEXT("mentor")) K *= 0.88f;
		K -= FMath::Clamp(R->Liking, -5, 10) * 0.02f;
	}
	return FMath::Clamp(K, 0.7f, 1.45f);
}

// Goods a player would bring to a trade: their trade-chat item, gear their class can't use and some stackables.
void FHLGame::TradeStockFor(int32 SimIdx)
{
	TradeStock.Reset(); TradeTake.Reset();
	const FHLSim& S = Sims[SimIdx];
	if (const FHLTradeOffer* O = SimWts.Find(SimIdx)) if (O->Item) { TradeStock.Add(*O); TradeTake.Add(0); }
	const TArray<FHLItem>& Items = FHLData::Get().Items;
	FRandomStream R(GetTypeHash(S.P->Id) ^ (uint32)(Now / 1800)); // the same goods for half an hour
	for (int32 Tries = 0; Tries < 400 && TradeStock.Num() < 4; ++Tries)
	{
		const FHLItem& It = Items[R.RandRange(0, Items.Num() - 1)];
		if (It.Kind == TEXT("quest") || It.Q > 3 || It.Price <= 0 || It.Req > S.Level + 2 || It.Req < S.Level - 10) continue;
		if (It.IsEquip() && (SimCanUse(S, It) || It.Q < 2)) continue; // they keep what they can wear, and don't carry grey gear
		if (TradeStock.ContainsByPredicate([&](const FHLTradeOffer& O) { return O.Item == &It; })) continue;
		FHLTradeOffer O; O.Item = &It; O.Count = It.Stack > 1 ? R.RandRange(3, FMath::Min(It.Stack, 20)) : 1; O.At = Now;
		TradeStock.Add(O);
	}
}

void FHLGame::OpenTrade(int32 SimIdx)
{
	FHLUnit* P = PlayerUnit();
	if (!P || !Sims.IsValidIndex(SimIdx)) return;
	const FHLUnit* U = Unit(Sims[SimIdx].UnitId);
	if (!U || Dist(*U, *P) > 11) { Error(TEXT("You are too far away to trade.")); return; }
	if (InCombat(*P) && !bTradeTest) { Error(TEXT("You can't trade while in combat.")); return; }
	TradeSim = SimIdx; TradeBag.Reset(); TradeCents = 0; TradeTheirCents = 0; TradeTries = 0; TradeMood.Reset(); TradeMoodKind = 0;
	TradeStockFor(SimIdx);
	if (const FHLTradeOffer* O = SimWts.Find(SimIdx)) TradeCents = FMath::Min(Player.Cents, O->Price);
}

// What they still need on top of the current offer (0: they'd take it).
int32 FHLGame::TradeGap() const
{
	if (!Sims.IsValidIndex(TradeSim)) return 0;
	const FHLSim& S = Sims[TradeSim];
	int32 Get = TradeCents, Give = TradeTheirCents;
	for (int32 B : TradeBag) if (Player.Bag.IsValidIndex(B) && Player.Bag[B].Item) Get += SimWorth(TradeSim, *Player.Bag[B].Item, true) * Player.Bag[B].Count;
	for (int32 K : TradeTake) if (TradeStock.IsValidIndex(K))
	{
		const FHLTradeOffer& O = TradeStock[K];
		Give += O.Price > 0 ? O.Price : SimWorth(TradeSim, *O.Item, false) * O.Count;
	}
	if (Give <= 0) return 0;
	return FMath::Max(0, FMath::CeilToInt32(Give * TradeRatio(*S.P, Player.Relations.Find(S.P->Id))) - Get);
}

void FHLGame::AcceptTrade()
{
	if (!Sims.IsValidIndex(TradeSim)) { TradeSim = -1; return; }
	FHLSim& S = Sims[TradeSim];
	if (TradeCents > Player.Cents) { Error(TEXT("You don't have that much money.")); return; }
	bool bAnything = TradeCents > 0 || TradeTheirCents > 0 || TradeTake.Num() > 0;
	for (int32 B : TradeBag) if (Player.Bag.IsValidIndex(B) && Player.Bag[B].Item) bAnything = true;
	if (!bAnything) { TradeSim = -1; return; }
	auto Say = [&](const FString& Msg, int32 Kind) { TradeMood = Msg; TradeMoodKind = Kind; AddChat(TEXT("say"), S.P->Handle, StyleText(*S.P, Msg), TradeSim, 0.6f + Rand() * 0.8f); };
	const int32 Gap = TradeGap();
	if (Gap > 0)
	{
		++TradeTries;
		if (TradeTries >= 4)
		{
			Say(TEXT("nah forget it, not worth my time"), 3);
			System(FString::Printf(TEXT("%s cancelled the trade."), *S.P->Handle));
			if (FHLRelation* R = Player.Relations.Find(S.P->Id)) R->Liking = FMath::Max(-5, R->Liking - 1);
			TradeSim = -1; TradeBag.Reset(); TradeTake.Reset();
			return;
		}
		static const TCHAR* Counter[] = { TEXT("add %s and its a deal"), TEXT("bit low. %s more?"), TEXT("cant do that, need like %s more"), TEXT("meet me halfway, %s more") };
		static const TCHAR* Lowball[] = { TEXT("lol no"), TEXT("thats a lowball and you know it"), TEXT("try again") };
		int32 Give = TradeTheirCents;
		for (int32 K : TradeTake) if (TradeStock.IsValidIndex(K)) Give += SimWorth(TradeSim, *TradeStock[K].Item, false) * TradeStock[K].Count;
		if (Gap > Give * 0.7f && TradeTries == 1) Say(Lowball[RandInt(0, 2)], 2);
		else Say(FString(Counter[RandInt(0, 3)]).Replace(TEXT("%s"), *HLMoney(Gap)), 2);
		return; // the window stays open so the player can change the offer
	}
	// both sides hand over
	Player.Cents += TradeTheirCents - TradeCents;
	int32 Given = 0;
	for (int32 B : TradeBag) if (Player.Bag.IsValidIndex(B) && Player.Bag[B].Item)
	{
		// they put on an upgrade straight away (it shows on them, and they won't want another for that slot)
		const FHLItem* It = Player.Bag[B].Item;
		if (SimCanUse(S, *It))
		{
			const FHLItem* Worn = S.Gear.FindRef(It->Slot);
			if (!Worn && S.P) Worn = S.P->Equipped.FindRef(It->Slot);
			if (!Worn || It->ILvl > Worn->ILvl) { S.Gear.Add(It->Slot, It); if (FHLUnit* U = Unit(S.UnitId)) RecalcCharStats(*U, *S.P->Cls, S.Level, S.Gear, false); }
		}
		++Given; Player.Bag[B] = FHLInvSlot();
	}
	int32 Got = 0;
	for (int32 K : TradeTake) if (TradeStock.IsValidIndex(K)) { AddItem(TradeStock[K].Item, TradeStock[K].Count, true); ++Got; }
	if (TradeTake.Contains(0) && TradeStock.IsValidIndex(0) && SimWts.Contains(TradeSim) && SimWts[TradeSim].Item == TradeStock[0].Item) SimWts.Remove(TradeSim);
	FHLRelation& R = Relation(S);
	R.Familiarity++;
	if (!Got && TradeTheirCents == 0)
	{
		System(FString::Printf(TEXT("You gave %s a gift."), *S.P->Handle));
		SimSay(TradeSim, TEXT("say"), TEXT("trade_gift"), 1, TEXT("oh wow ty"));
		R.Liking = FMath::Min(10, R.Liking + (TradeCents >= 50 || Given > 1 ? 2 : 1));
		R.Facts.AddUnique(TEXT("gave me a gift"));
	}
	else
	{
		System(FString::Printf(TEXT("Trade complete with %s."), *S.P->Handle));
		ChatCtx = Got && TradeStock.IsValidIndex(TradeTake[0]) ? TradeStock[TradeTake[0]].Item->Name : FString();
		SimSay(TradeSim, TEXT("say"), TEXT("trade_done"), 1, TEXT("ty"));
		ChatCtx.Reset();
		if (TradeTries == 0) R.Liking = FMath::Min(10, R.Liking + 1); // a fair first offer
		R.Facts.AddUnique(TEXT("traded with me"));
	}
	Sound2D(TEXT("ui_sold"));
	TradeSim = -1; TradeBag.Reset(); TradeTake.Reset(); TradeCents = TradeTheirCents = 0;
}

// ------------------------------------------------------------------------------------------------ trades they start
// Rarely, an online player standing near you who could use something in your bags asks to buy it.
void FHLGame::TickTradeOffers()
{
	FHLUnit* P = PlayerUnit();
	if (TradeFromSim >= 0 && (Now >= TradeInviteUntil || !Player.Bag.IsValidIndex(TradeWantBag) || !Player.Bag[TradeWantBag].Item)) TradeFromSim = -1;
	// the asker walks over (TickSimBrain); the request pops up once they are beside you
	if (P && TradeFromSim >= 0 && !bTradeAskShown)
	{
		const FHLUnit* U = Unit(Sims[TradeFromSim].UnitId);
		if (U && Dist(*U, *P) <= 6) { bTradeAskShown = true; TradeInviteUntil = Now + 40; System(FString::Printf(TEXT("%s wants to trade with you."), *Sims[TradeFromSim].P->Handle)); }
	}
	if (!P || TradeFromSim >= 0 || TradeSim >= 0 || DuelFromSim >= 0 || (InCombat(*P) && !bTradeTest) || P->bDead || Now < NextSimTrade) return;
	NextSimTrade = Now + (bTradeTest ? 8 : 20);
	if (!bTradeTest && (Player.PlayedS < 300 || !Chance(0.025f))) return; // about one ask in 15 minutes of standing near people
	TArray<TPair<int32, int32>> Wants; // sim, bag slot
	for (int32 i = 0; i < Sims.Num(); ++i)
	{
		const FHLSim& S = Sims[i];
		const FHLUnit* U = Unit(S.UnitId);
		if (!S.bOnline || S.bInParty || S.bAfk || !U || U->bDead || U->Area != P->Area || Dist(*U, *P) > (bTradeTest ? 40 : 10) || InCombat(*U)) continue;
		for (int32 b = 0; b < Player.Bag.Num(); ++b)
		{
			const FHLItem* It = Player.Bag[b].Item;
			if (!It || !It->IsEquip() || !SimCanUse(S, *It)) continue;
			const FHLItem* Worn = S.Gear.FindRef(It->Slot);
			if (!Worn && S.P) Worn = S.P->Equipped.FindRef(It->Slot);
			if (!Worn || It->ILvl > Worn->ILvl + 1) Wants.Add({ i, b });
		}
	}
	if (bTradeTest)
	{
		int32 Near = 0, Gear = 0;
		for (const FHLSim& S : Sims) { const FHLUnit* U = Unit(S.UnitId); if (S.bOnline && U && U->Area == P->Area && Dist(*U, *P) <= 40) ++Near; }
		for (const FHLInvSlot& B : Player.Bag) if (B.Item && B.Item->IsEquip()) ++Gear;
		UE_LOG(LogTemp, Display, TEXT("HL_TRADE scan: %d players within 40 m, %d gear in bags, %d wants"), Near, Gear, Wants.Num());
	}
	if (!Wants.Num()) return;
	const TPair<int32, int32> W = Wants[RandInt(0, Wants.Num() - 1)];
	const FHLSim& S = Sims[W.Key];
	const FHLItem& It = *Player.Bag[W.Value].Item;
	TradeFromSim = W.Key; TradeWantBag = W.Value; TradeInviteUntil = Now + 60; bTradeAskShown = false;
	// their opening offer: a little under what it's worth to them, rounded like a person would
	const int32 Worth = SimWorth(W.Key, It, true);
	int32 Offer = FMath::RoundToInt32(Worth * (0.7f + Rand() * 0.2f));
	Offer = Offer >= 500 ? Offer / 50 * 50 : Offer >= 100 ? Offer / 10 * 10 : FMath::Max(1, Offer);
	TradeWantOffer = Offer;
	LastWhisperFrom = S.P->Handle;
	AddChat(TEXT("whisper"), S.P->Handle, StyleText(*S.P, FString::Printf(TEXT("hey would you sell that [%s]? %s"), *It.Name, *HLMoney(Offer))), W.Key, 0.5f);
}

void FHLGame::AnswerTradeRequest(bool bAccept)
{
	const int32 SimIdx = TradeFromSim;
	TradeFromSim = -1;
	if (!Sims.IsValidIndex(SimIdx)) return;
	const FHLSim& S = Sims[SimIdx];
	if (!bAccept) { AddChat(TEXT("whisper"), S.P->Handle, StyleText(*S.P, Chance(0.5f) ? TEXT("np") : TEXT("ok no worries")), SimIdx, 1.f); return; }
	OpenTrade(SimIdx);
	if (TradeSim != SimIdx) return;
	TradeTake.Reset(); // they came to buy, not to sell
	if (Player.Bag.IsValidIndex(TradeWantBag) && Player.Bag[TradeWantBag].Item) TradeBag.Add(TradeWantBag);
	TradeTheirCents = TradeWantOffer;
	TradeMood = TEXT("They put up the money for your item.");
	TradeMoodKind = 1;
}
