// The human player's systems: movement, targeting, interaction, inventory, vendors, trainers, quests, auction house, saves.
#include "HLGame.h"
#include "HLBridge.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

static const TCHAR* SAVE_KEY = TEXT("hollywoodland_save_v1");
static const TCHAR* ARMOUR_ORDER[] = { TEXT("cloth"), TEXT("leather"), TEXT("padded"), TEXT("steel") };
static int32 ArmourRank(const FString& T) { for (int32 i = 0; i < 4; ++i) if (T == ARMOUR_ORDER[i]) return i; return 0; }

// ------------------------------------------------------------------------------------------------------------- character
void FHLGame::NewCharacter(const FString& Name, const FHLClass* Cls)
{
	const FString KeepId = Player.PlayerId;
	Player = FHLPlayer();
	Player.PlayerId = KeepId.IsEmpty() ? FString::Printf(TEXT("p_%08x%08x"), FMath::Rand32(), (uint32)(FPlatformTime::Cycles64() & 0xffffffff)) : KeepId;
	Player.bCreated = true; Player.Name = Name; Player.Cls = Cls; Player.Cents = 25;
	Player.Bag.SetNum(24); Player.Bank.SetNum(24); Player.Bar.Init(nullptr, 12);
	const FHLData& D = FHLData::Get();
	for (const FString& Id : Cls->StartItems) if (const FHLItem* It = D.Item(Id)) Player.Equip.Add(It->Slot, It);
	for (const FHLAbility* Ab : Cls->Abilities) if (Ab->Level <= 1) LearnAbility(Ab, true);
	if (const FHLItem* Food = D.ItemByName(TEXT("Hot Dog"))) AddItem(Food, 5, false);
	if (Cls->Resource == TEXT("power")) if (const FHLItem* Drink = D.ItemByName(TEXT("Coffee"))) AddItem(Drink, 5, false);
	if (const FHLItem* Bd = D.ItemByName(TEXT("Linen Bandage"))) AddItem(Bd, 3, false);
	GiveStarterAmmo();
}

void FHLGame::GiveStarterAmmo()
{
	const FHLItem* const* Gun = Player.Equip.Find(TEXT("ranged"));
	if (!Player.Cls || !Player.Cls->bRanged || !Gun || !*Gun) return;
	for (const FHLItem& It : FHLData::Get().Items)
		if (It.Kind == TEXT("ammo") && It.Req <= 1 && It.Ammo == (*Gun)->Calibre()) { AddItem(&It, 200, false); return; }
}

void FHLGame::LearnAbility(const FHLAbility* Ab, bool bFree)
{
	if (!Ab || Player.Known.Contains(Ab)) return;
	if (!bFree)
	{
		const int32 Cost = 10 * Ab->Level;
		if (Player.Cents < Cost) { Error(TEXT("You can't afford that.")); return; }
		Player.Cents -= Cost;
		System(FString::Printf(TEXT("You have learned %s."), *Ab->Name));
	}
	Player.Known.Add(Ab);
	for (int32 i = 0; i < Player.Bar.Num(); ++i) if (!Player.Bar[i]) { Player.Bar[i] = Ab; break; }
}

// ------------------------------------------------------------------------------------------------------------- save / load
FString FHLGame::SaveJson() const
{
	const FHLUnit* U = PlayerUnit();
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetNumberField(TEXT("v"), 2);
	O->SetStringField(TEXT("player_id"), Player.PlayerId);
	O->SetStringField(TEXT("name"), Player.Name);
	O->SetStringField(TEXT("class"), Player.Cls ? Player.Cls->Id : FString());
	O->SetNumberField(TEXT("level"), U ? U->Level : 1);
	{
		TSharedPtr<FJsonObject> Tal = MakeShared<FJsonObject>();
		for (const auto& KV : Player.Talents) Tal->SetNumberField(KV.Key, KV.Value);
		O->SetObjectField(TEXT("talents"), Tal); O->SetNumberField(TEXT("respecs"), Player.Respecs);
		TSharedPtr<FJsonObject> Dur = MakeShared<FJsonObject>();
		for (const auto& KV : Player.Durability) Dur->SetNumberField(KV.Key, KV.Value);
		O->SetObjectField(TEXT("durability"), Dur);
	}
	O->SetNumberField(TEXT("xp"), Player.Xp); O->SetNumberField(TEXT("cents"), Player.Cents); O->SetNumberField(TEXT("salvage"), Player.Salvage); O->SetNumberField(TEXT("guns"), Player.Guns);
	O->SetNumberField(TEXT("played"), Player.PlayedS); O->SetNumberField(TEXT("deaths"), Player.Deaths); O->SetNumberField(TEXT("kills"), Player.Kills);
	O->SetBoolField(TEXT("show_sim"), Player.bShowSim);
	if (U && U->Area == 0) { O->SetNumberField(TEXT("x"), U->Pos.X); O->SetNumberField(TEXT("y"), U->Pos.Y); }
	auto Inv = [](const TArray<FHLInvSlot>& Slots)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FHLInvSlot& S : Slots)
		{
			TSharedRef<FJsonObject> E = MakeShared<FJsonObject>();
			if (S.Item) { E->SetStringField(TEXT("i"), S.Item->Id); E->SetNumberField(TEXT("n"), S.Count); }
			Arr.Add(MakeShared<FJsonValueObject>(E));
		}
		return Arr;
	};
	O->SetArrayField(TEXT("bag"), Inv(Player.Bag)); O->SetArrayField(TEXT("bank"), Inv(Player.Bank));
	TSharedRef<FJsonObject> Eq = MakeShared<FJsonObject>();
	for (const auto& KV : Player.Equip) if (KV.Value) Eq->SetStringField(KV.Key, KV.Value->Id);
	O->SetObjectField(TEXT("equip"), Eq);
	auto Strs = [](const TArray<FString>& In) { TArray<TSharedPtr<FJsonValue>> Arr; for (const FString& S : In) Arr.Add(MakeShared<FJsonValueString>(S)); return Arr; };
	TArray<FString> Known, Bar;
	for (const FHLAbility* Ab : Player.Known) Known.Add(Ab->Id);
	for (const FHLAbility* Ab : Player.Bar) Bar.Add(Ab ? Ab->Id : FString());
	O->SetArrayField(TEXT("known"), Strs(Known)); O->SetArrayField(TEXT("bar"), Strs(Bar));
	O->SetArrayField(TEXT("done"), Strs(Player.Done.Array())); O->SetArrayField(TEXT("discovered"), Strs(Player.Discovered.Array()));
	TArray<TSharedPtr<FJsonValue>> Qs;
	for (const FHLQuestState& QS : Player.Quests)
	{
		TSharedRef<FJsonObject> E = MakeShared<FJsonObject>();
		E->SetStringField(TEXT("id"), QS.Q->Id);
		TArray<TSharedPtr<FJsonValue>> Pr; for (int32 V : QS.Progress) Pr.Add(MakeShared<FJsonValueNumber>(V));
		E->SetArrayField(TEXT("p"), Pr);
		Qs.Add(MakeShared<FJsonValueObject>(E));
	}
	O->SetArrayField(TEXT("quests"), Qs);
	TSharedRef<FJsonObject> Rep = MakeShared<FJsonObject>();
	for (const auto& KV : Player.Rep) Rep->SetNumberField(KV.Key, KV.Value);
	O->SetObjectField(TEXT("rep"), Rep);
	TSharedRef<FJsonObject> Rel = MakeShared<FJsonObject>();
	for (const auto& KV : Player.Relations)
	{
		if (KV.Value.Familiarity == 0 && KV.Value.Liking == 0 && KV.Value.Facts.Num() == 0 && KV.Value.Bond.IsEmpty()) continue;
		TSharedRef<FJsonObject> E = MakeShared<FJsonObject>();
		E->SetNumberField(TEXT("f"), KV.Value.Familiarity); E->SetNumberField(TEXT("l"), KV.Value.Liking); E->SetArrayField(TEXT("facts"), Strs(KV.Value.Facts));
		if (!KV.Value.Bond.IsEmpty()) E->SetStringField(TEXT("bond"), KV.Value.Bond);
		Rel->SetObjectField(KV.Key, E);
	}
	O->SetObjectField(TEXT("relations"), Rel);
	{
		TArray<TSharedPtr<FJsonValue>> Ml;
		for (const FHLMail& M : Player.Mail)
		{
			TSharedRef<FJsonObject> E = MakeShared<FJsonObject>();
			E->SetStringField(TEXT("f"), M.From); E->SetStringField(TEXT("s"), M.Subject); E->SetStringField(TEXT("b"), M.Body);
			E->SetNumberField(TEXT("c"), M.Cents); E->SetNumberField(TEXT("cod"), M.Cod); E->SetBoolField(TEXT("r"), M.bRead);
			if (M.Item) { E->SetStringField(TEXT("i"), M.Item->Id); E->SetNumberField(TEXT("n"), M.Count); }
			Ml.Add(MakeShared<FJsonValueObject>(E));
		}
		O->SetArrayField(TEXT("mail"), Ml);
		O->SetArrayField(TEXT("friends"), Strs(Player.Friends));
		TArray<TSharedPtr<FJsonValue>> Au;
		for (const FHLAuction& A : Auctions)
		{
			if (!A.bMine && !A.bPlayerBid) continue;
			TSharedRef<FJsonObject> E = MakeShared<FJsonObject>();
			E->SetStringField(TEXT("i"), A.Item->Id); E->SetNumberField(TEXT("n"), A.Count); E->SetNumberField(TEXT("buy"), A.Buyout); E->SetNumberField(TEXT("min"), A.MinBid);
			E->SetNumberField(TEXT("bid"), A.Bid); E->SetNumberField(TEXT("left"), FMath::Max(60.f, A.ExpiresAt - (float)Now)); E->SetBoolField(TEXT("mine"), A.bMine);
			E->SetNumberField(TEXT("seller"), A.SellerIdx);
			Au.Add(MakeShared<FJsonValueObject>(E));
		}
		O->SetArrayField(TEXT("auctions"), Au);
	}
	FString Out;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
	FJsonSerializer::Serialize(O, Writer);
	return Out;
}

bool FHLGame::LoadJson(const FString& Json)
{
	TSharedPtr<FJsonObject> O;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, O) || !O.IsValid()) return false;
	const FHLData& D = FHLData::Get();
	const FHLClass* Cls = D.Class(O->GetStringField(TEXT("class")));
	if (!Cls) return false;
	Player = FHLPlayer();
	Player.bCreated = true; Player.Cls = Cls;
	O->TryGetStringField(TEXT("player_id"), Player.PlayerId); O->TryGetStringField(TEXT("name"), Player.Name);
	Player.Xp = O->GetIntegerField(TEXT("xp")); Player.Cents = O->GetIntegerField(TEXT("cents"));
	double Num = 0;
	if (O->TryGetNumberField(TEXT("salvage"), Num)) Player.Salvage = Num;
	if (O->TryGetNumberField(TEXT("guns"), Num)) Player.Guns = Num;
	if (O->TryGetNumberField(TEXT("played"), Num)) Player.PlayedS = Num;
	if (O->TryGetNumberField(TEXT("deaths"), Num)) Player.Deaths = Num;
	if (O->TryGetNumberField(TEXT("kills"), Num)) Player.Kills = Num;
	O->TryGetBoolField(TEXT("show_sim"), Player.bShowSim);
	if (O->TryGetNumberField(TEXT("respecs"), Num)) Player.Respecs = Num;
	const TSharedPtr<FJsonObject>* Dur = nullptr;
	if (O->TryGetObjectField(TEXT("durability"), Dur)) for (const auto& KV : (*Dur)->Values) Player.Durability.Add(FString(KV.Key), FMath::Clamp((float)KV.Value->AsNumber(), 0.f, 1.f));
	const TSharedPtr<FJsonObject>* Tal = nullptr;
	if (O->TryGetObjectField(TEXT("talents"), Tal))
		for (const auto& KV : (*Tal)->Values) if (const FHLTalent* T = D.Talent(FString(KV.Key))) if (T->Class == Cls->Id) Player.Talents.Add(FString(KV.Key), FMath::Clamp((int32)KV.Value->AsNumber(), 1, T->Ranks));
	auto Inv = [&](const TCHAR* Key, TArray<FHLInvSlot>& Slots)
	{
		Slots.SetNum(24);
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!O->TryGetArrayField(Key, Arr)) return;
		for (int32 i = 0; i < Arr->Num() && i < 24; ++i)
		{
			const TSharedPtr<FJsonObject> E = (*Arr)[i]->AsObject();
			FString Id;
			if (E.IsValid() && E->TryGetStringField(TEXT("i"), Id)) { Slots[i].Item = D.Item(Id); Slots[i].Count = Slots[i].Item ? FMath::Max(1, (int32)E->GetNumberField(TEXT("n"))) : 0; }
		}
	};
	Inv(TEXT("bag"), Player.Bag); Inv(TEXT("bank"), Player.Bank);
	const TSharedPtr<FJsonObject>* Sub = nullptr;
	if (O->TryGetObjectField(TEXT("equip"), Sub)) for (auto& KV : (*Sub)->Values) if (const FHLItem* It = D.Item(KV.Value->AsString())) Player.Equip.Add(FString(KV.Key), It);
	auto Strs = [&](const TCHAR* Key) { TArray<FString> Out; const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr; if (O->TryGetArrayField(Key, Arr)) for (auto& V : *Arr) Out.Add(V->AsString()); return Out; };
	for (const FString& Id : Strs(TEXT("known"))) if (const FHLAbility* Ab = D.Ability(Id)) Player.Known.Add(Ab);
	Player.Bar.Init(nullptr, 12);
	{ const TArray<FString> Bar = Strs(TEXT("bar")); for (int32 i = 0; i < Bar.Num() && i < 12; ++i) Player.Bar[i] = D.Ability(Bar[i]); }
	for (const FString& S : Strs(TEXT("done"))) Player.Done.Add(S);
	for (const FString& S : Strs(TEXT("discovered"))) Player.Discovered.Add(S);
	const TArray<TSharedPtr<FJsonValue>>* Qs = nullptr;
	if (O->TryGetArrayField(TEXT("quests"), Qs)) for (auto& V : *Qs)
	{
		const TSharedPtr<FJsonObject> E = V->AsObject();
		const FHLQuest* Q = E.IsValid() ? D.Quest(E->GetStringField(TEXT("id"))) : nullptr;
		if (!Q) continue;
		FHLQuestState QS; QS.Q = Q; QS.Progress.Init(0, Q->Objectives.Num());
		const TArray<TSharedPtr<FJsonValue>>* Pr = nullptr;
		if (E->TryGetArrayField(TEXT("p"), Pr)) for (int32 i = 0; i < Pr->Num() && i < QS.Progress.Num(); ++i) QS.Progress[i] = (*Pr)[i]->AsNumber();
		QS.bComplete = true;
		for (int32 i = 0; i < QS.Progress.Num(); ++i) if (QS.Progress[i] < Q->Objectives[i].Count) QS.bComplete = false;
		Player.Quests.Add(QS);
	}
	if (O->TryGetObjectField(TEXT("rep"), Sub)) for (auto& KV : (*Sub)->Values) Player.Rep.Add(FString(KV.Key), (int32)KV.Value->AsNumber());
	if (O->TryGetObjectField(TEXT("relations"), Sub)) for (auto& KV : (*Sub)->Values)
	{
		const TSharedPtr<FJsonObject> E = KV.Value->AsObject();
		if (!E.IsValid()) continue;
		FHLRelation R; R.Familiarity = E->GetIntegerField(TEXT("f")); R.Liking = E->GetIntegerField(TEXT("l")); E->TryGetStringField(TEXT("bond"), R.Bond);
		const TArray<TSharedPtr<FJsonValue>>* Facts = nullptr;
		if (E->TryGetArrayField(TEXT("facts"), Facts)) for (auto& F : *Facts) R.Facts.Add(F->AsString());
		Player.Relations.Add(FString(KV.Key), R);
	}
	const TArray<TSharedPtr<FJsonValue>>* Ml = nullptr;
	if (O->TryGetArrayField(TEXT("mail"), Ml)) for (auto& V : *Ml)
	{
		const TSharedPtr<FJsonObject> E = V->AsObject();
		if (!E.IsValid()) continue;
		FHLMail M; E->TryGetStringField(TEXT("f"), M.From); E->TryGetStringField(TEXT("s"), M.Subject); E->TryGetStringField(TEXT("b"), M.Body);
		M.Cents = E->GetIntegerField(TEXT("c")); M.Cod = E->GetIntegerField(TEXT("cod")); E->TryGetBoolField(TEXT("r"), M.bRead);
		FString Id; if (E->TryGetStringField(TEXT("i"), Id)) { M.Item = D.Item(Id); M.Count = M.Item ? FMath::Max(1, (int32)E->GetIntegerField(TEXT("n"))) : 0; }
		Player.Mail.Add(M);
	}
	Player.Friends = Strs(TEXT("friends"));
	Auctions.Reset(); bAuctionSeeded = false;
	const TArray<TSharedPtr<FJsonValue>>* Au = nullptr;
	if (O->TryGetArrayField(TEXT("auctions"), Au)) for (auto& V : *Au)
	{
		const TSharedPtr<FJsonObject> E = V->AsObject();
		const FHLItem* It = E.IsValid() ? D.Item(E->GetStringField(TEXT("i"))) : nullptr;
		if (!It) continue;
		FHLAuction A; A.Item = It; A.Count = FMath::Max(1, (int32)E->GetIntegerField(TEXT("n"))); A.Buyout = E->GetIntegerField(TEXT("buy")); A.MinBid = E->GetIntegerField(TEXT("min"));
		A.Bid = E->GetIntegerField(TEXT("bid")); A.ExpiresAt = Now + E->GetNumberField(TEXT("left")); A.bMine = E->GetBoolField(TEXT("mine")); A.bPlayerBid = !A.bMine;
		A.SellerIdx = E->GetIntegerField(TEXT("seller"));
		Auctions.Add(A);
	}
	// the unit
	if (!PlayerUnit()) PlayerUnitId = AddUnit(EHLKind::Player, 0, Areas[0].Def->Spawn)->Id;
	FHLUnit& U = *PlayerUnit();
	U.Level = FMath::Clamp((int32)O->GetIntegerField(TEXT("level")), 1, 60);
	double X = 0, Y = 0;
	if (O->TryGetNumberField(TEXT("x"), X) && O->TryGetNumberField(TEXT("y"), Y)) U.Pos = Areas[0].Nav.NearestFree(FVector2D(X, Y));
	if (O->GetIntegerField(TEXT("v")) < 2) GiveStarterAmmo(); // saves from before ammunition existed
	return true;
}

void FHLGame::SaveNow()
{
	if (!Player.bCreated || !bInWorld) return;
	const FString Json = SaveJson();
	HLBridge::StorageSet(SAVE_KEY, Json);
	static double LastCloud = -1e9;
	if (bWorkerOk && !Session.IsEmpty() && Now - LastCloud > 120)
	{
		LastCloud = Now;
		HLBridge::Fetch(TEXT("PUT"), FString::Printf(TEXT("%s/v1/save/%s?t=%s"), *ApiBase, *Player.PlayerId, *Session), Json, [](const FHLBridgeResult&) {});
	}
}

// ------------------------------------------------------------------------------------------------------------- inventory
bool FHLGame::AddItem(const FHLItem* Item, int32 Count, bool bAnnounce)
{
	if (!Item) return false;
	int32 Left = Count;
	if (Item->Stack > 1) for (FHLInvSlot& S : Player.Bag) if (S.Item == Item && S.Count < Item->Stack && Left > 0) { const int32 N = FMath::Min(Left, Item->Stack - S.Count); S.Count += N; Left -= N; }
	for (FHLInvSlot& S : Player.Bag) if (!S.Item && Left > 0) { const int32 N = FMath::Min(Left, FMath::Max(1, Item->Stack)); S.Item = Item; S.Count = N; Left -= N; }
	if (Left == Count) { Error(TEXT("Your bags are full.")); return false; }
	if (bAnnounce) AddChat(TEXT("loot"), FString(), FString::Printf(TEXT("You receive: [%s]%s"), *Item->Name, Count - Left > 1 ? *FString::Printf(TEXT(" x%d"), Count - Left) : TEXT("")));
	return true;
}
int32 FHLGame::CountItem(const FHLItem* Item) const { int32 N = 0; for (const FHLInvSlot& S : Player.Bag) if (S.Item == Item) N += S.Count; return N; }
void FHLGame::RemoveItem(const FHLItem* Item, int32 Count)
{
	for (FHLInvSlot& S : Player.Bag) if (S.Item == Item && Count > 0) { const int32 N = FMath::Min(Count, S.Count); S.Count -= N; Count -= N; if (S.Count <= 0) S = FHLInvSlot(); }
}

bool FHLGame::CanEquip(const FHLItem& It, FString* Why) const
{
	const FHLUnit* U = PlayerUnit();
	auto Fail = [&](const TCHAR* W) { if (Why) *Why = W; return false; };
	if (!It.IsEquip() || !Player.Cls) return Fail(TEXT("That can't be equipped."));
	if (U && U->Level < It.Req) return Fail(TEXT("Your level is too low."));
	if (It.Kind == TEXT("armor") && It.Type != TEXT("jewel") && ArmourRank(It.Type) > ArmourRank(Player.Cls->Armour)) return Fail(TEXT("Your class can't wear that armour type."));
	if (It.Kind == TEXT("weapon") && !Player.Cls->Weapons.Contains(It.Type)) return Fail(TEXT("Your class can't use that weapon."));
	return true;
}

void FHLGame::EquipBag(int32 BagIdx)
{
	if (!Player.Bag.IsValidIndex(BagIdx) || !Player.Bag[BagIdx].Item) return;
	const FHLItem* It = Player.Bag[BagIdx].Item;
	FString Why;
	if (!CanEquip(*It, &Why)) { Error(Why); return; }
	if (PlayerUnit() && InCombat(*PlayerUnit()) && It->Kind == TEXT("armor")) { Error(TEXT("You can't change armour in combat.")); return; }
	const FHLItem* Old = Player.Equip.FindRef(It->Slot);
	Player.Bag[BagIdx] = FHLInvSlot();
	if (Old) { Player.Bag[BagIdx].Item = Old; Player.Bag[BagIdx].Count = 1; }
	Player.Equip.Add(It->Slot, It);
	if (It->Slot == TEXT("main") && It->Hands == 2) if (const FHLItem* Off = Player.Equip.FindRef(TEXT("off"))) { if (AddItem(Off, 1, false)) Player.Equip.Remove(TEXT("off")); }
	if (It->Slot == TEXT("off")) if (const FHLItem* Main = Player.Equip.FindRef(TEXT("main"))) if (Main->Hands == 2) { if (AddItem(Main, 1, false)) Player.Equip.Remove(TEXT("main")); }
	RecalcPlayerStats();
}

void FHLGame::Unequip(const FString& Slot)
{
	const FHLItem* It = Player.Equip.FindRef(Slot);
	if (!It) return;
	if (AddItem(It, 1, false)) { Player.Equip.Remove(Slot); RecalcPlayerStats(); }
}

void FHLGame::UseBag(int32 BagIdx)
{
	FHLUnit* U = PlayerUnit();
	if (!U || U->bDead || !Player.Bag.IsValidIndex(BagIdx) || !Player.Bag[BagIdx].Item) return;
	const FHLItem* It = Player.Bag[BagIdx].Item;
	if (OpenNpcMode == TEXT("vendor")) { SellBag(BagIdx); return; }
	if (OpenNpcMode == TEXT("bank"))
	{
		for (FHLInvSlot& S : Player.Bank) if (!S.Item) { S = Player.Bag[BagIdx]; Player.Bag[BagIdx] = FHLInvSlot(); return; }
		Error(TEXT("Your bank is full.")); return;
	}
	if (It->IsEquip()) { EquipBag(BagIdx); return; }
	if (It->Kind != TEXT("consumable")) return;
	if (U->Level < It->Req) { Error(TEXT("Your level is too low.")); return; }
	FHLAura A; A.SourceId = U->Id;
	if (It->UseType == TEXT("food") || It->UseType == TEXT("drink"))
	{
		if (InCombat(*U)) { Error(TEXT("You can't do that while in combat.")); return; }
		const bool bFood = It->UseType == TEXT("food");
		if (!bFood && U->ResType != EHLRes::Power) { Error(TEXT("You don't need that.")); return; }
		A.Type = EHLAura::Hot; A.Name = bFood ? TEXT("Eating") : TEXT("Drinking"); A.Stat = bFood ? TEXT("") : TEXT("res");
		A.Value = It->UseAmount / 9.f; A.Period = 2; A.TickTimer = 2; A.Remaining = 18.05f;
		U->Auras.RemoveAll([&](const FHLAura& O) { return O.Name == A.Name; });
		U->Auras.Add(A); U->bSitting = true; StopMove(*U);
	}
	else if (It->UseType == TEXT("bandage"))
	{
		if (U->HasAuraNamed(TEXT("Recently Bandaged"))) { Error(TEXT("You were bandaged recently.")); return; }
		A.Type = EHLAura::Hot; A.Name = TEXT("First Aid"); A.Value = It->UseAmount / 4.f; A.Period = 1.5f; A.TickTimer = 1.5f; A.Remaining = 6.05f;
		U->Auras.Add(A);
		FHLAura Cd; Cd.Type = EHLAura::Buff; Cd.Name = TEXT("Recently Bandaged"); Cd.Stat = TEXT("none"); Cd.Remaining = 45; Cd.bHarmful = true;
		U->Auras.Add(Cd);
	}
	else if (It->UseType == TEXT("buff"))
	{
		A.Type = EHLAura::Buff; A.Name = It->Name; A.Stat = It->UseStat; A.Value = It->UseValue; A.Remaining = It->UseDur;
		U->Auras.RemoveAll([&](const FHLAura& O) { return O.Name == A.Name; });
		U->Auras.Add(A);
	}
	else return;
	if (--Player.Bag[BagIdx].Count <= 0) Player.Bag[BagIdx] = FHLInvSlot();
}

void FHLGame::SellBag(int32 BagIdx)
{
	if (!Player.Bag.IsValidIndex(BagIdx) || !Player.Bag[BagIdx].Item) return;
	const FHLInvSlot S = Player.Bag[BagIdx];
	if (S.Item->Price <= 0) { Error(TEXT("The merchant doesn't want that.")); return; }
	Player.Cents += S.Item->Price * S.Count / S.Item->Buy;
	Player.Bag[BagIdx] = FHLInvSlot();
}

const FHLItem* FHLGame::BestAmmo() const
{
	const FHLItem* const* Gun = Player.Equip.Find(TEXT("ranged"));
	const FHLUnit* U = PlayerUnit();
	if (!Gun || !*Gun || !U || SlotDurability(TEXT("ranged")) <= 0) return nullptr;
	const FString Cal = (*Gun)->Calibre();
	const FHLItem* Best = nullptr;
	for (const FHLInvSlot& S : Player.Bag)
		if (S.Item && S.Count > 0 && S.Item->Kind == TEXT("ammo") && S.Item->Ammo == Cal && S.Item->Req <= U->Level && (!Best || S.Item->AmmoDps > Best->AmmoDps)) Best = S.Item;
	return Best;
}

bool FHLGame::BuyItem(const FHLItem* Item)
{
	if (!Item) return false;
	const int32 Cost = FMath::Max(1, Item->Price * 4);
	if (Player.Cents < Cost) { Error(TEXT("You can't afford that.")); return false; }
	if (!AddItem(Item, Item->Buy, false)) return false;
	Player.Cents -= Cost;
	return true;
}

// ------------------------------------------------------------------------------------------------------------- targeting / interaction
void FHLGame::TargetUnit(int32 Id)
{
	FHLUnit* U = PlayerUnit();
	if (!U) return;
	if (U->TargetId != Id) U->bAutoAttack = false;
	U->TargetId = Id;
}

void FHLGame::TabTarget()
{
	FHLUnit* U = PlayerUnit();
	if (!U) return;
	TArray<FHLUnit*> C;
	for (FHLUnit* O : UnitList) if (O->Kind == EHLKind::Mob && !O->bDead && O->Area == U->Area && Dist(*U, *O) < 40 && Areas[U->Area].Nav.SegClear(U->Pos, O->Pos)) C.Add(O);
	if (!C.Num()) return;
	C.Sort([&](const FHLUnit& A, const FHLUnit& B) { return FVector2D::DistSquared(A.Pos, U->Pos) < FVector2D::DistSquared(B.Pos, U->Pos); });
	int32 Cur = C.IndexOfByPredicate([&](const FHLUnit* O) { return O->Id == U->TargetId; });
	TargetUnit(C[(Cur + 1) % C.Num()]->Id);
}

void FHLGame::TargetNearestFriend()
{
	FHLUnit* U = PlayerUnit();
	if (!U) return;
	FHLUnit* Best = nullptr; float BestD = 30;
	for (FHLUnit* O : UnitList) if (O->Kind == EHLKind::Sim && !O->bDead && O->Area == U->Area && O->Id != U->TargetId && Dist(*U, *O) < BestD) { Best = O; BestD = Dist(*U, *O); }
	if (Best) TargetUnit(Best->Id);
}

void FHLGame::InteractUnit(int32 Id)
{
	FHLUnit* U = PlayerUnit();
	FHLUnit* T = Unit(Id);
	if (!U || !T || U->bDead) return;
	TargetUnit(Id);
	if (T->Kind == EHLKind::Mob)
	{
		if (T->bDead) { if (T->bLootable && T->TappedBy == PlayerUnitId) { if (Dist(*U, *T) > 6) Error(TEXT("You are too far away.")); else LootCorpse(*T); } return; }
		U->bAutoAttack = true;
		if (!U->bRangedAuto && Dist(*U, *T) > U->AutoRange + 2) Error(TEXT("You are too far away."));
		return;
	}
	if (T->Kind == EHLKind::Npc && T->Npc)
	{
		if (T->bDead) { Error(FString::Printf(TEXT("%s is dead."), *T->Name)); return; }
		if (Dist(*U, *T) > 6) { Error(TEXT("You are too far away.")); return; }
		if (T->Npc->HasRole(TEXT("guard")) && T->Npc->Roles.Num() == 1) { AddChat(TEXT("npc"), T->Name, TEXT("Keep to the boulevard after dark. The Egyptian is theirs, and so is everything past the wall.")); return; }
		QuestEvent(TEXT("talk"), T->Npc->Id);
		OpenNpcUnitId = Id; OpenNpcMode = TEXT("gossip");
	}
}

void FHLGame::LootCorpse(FHLUnit& Corpse)
{
	if (Corpse.LootCents > 0)
	{
		const int32 Share = Corpse.LootCents / (1 + Party.Num());
		Player.Cents += FMath::Max(1, Share);
		AddChat(TEXT("loot"), FString(), FString::Printf(TEXT("You loot %s%s"), *HLMoney(FMath::Max(1, Share)), Party.Num() ? TEXT(" (your share)") : TEXT("")));
		Corpse.LootCents = 0;
	}
	for (int32 i = Corpse.Loot.Num() - 1; i >= 0; --i)
	{
		const FHLLoot L = Corpse.Loot[i];
		if (Party.Num() && L.Item->Q >= 2 && L.Item->IsEquip())
		{
			if (Roll.bActive) continue; // one roll at a time; the rest stays on the corpse
			StartRoll(L.Item);
			Corpse.Loot.RemoveAt(i);
		}
		else if (AddItem(L.Item, L.Count)) Corpse.Loot.RemoveAt(i);
	}
	if (Corpse.Loot.Num() == 0) { Corpse.bLootable = false; Corpse.CorpseUntil = FMath::Min(Corpse.CorpseUntil, (float)Now + 6.f); }
}

void FHLGame::UseBar(int32 Slot)
{
	FHLUnit* U = PlayerUnit();
	if (!U || !Player.Bar.IsValidIndex(Slot) || !Player.Bar[Slot]) return;
	const FHLAbility& Ab = *Player.Bar[Slot];
	if (Ab.Target == TEXT("enemy"))
	{
		FHLUnit* T = Unit(U->TargetId);
		if (!T || T->bDead || !IsHostile(*U, *T)) { TabTarget(); T = Unit(U->TargetId); }
		if (T && !T->bDead && IsHostile(*U, *T)) U->bAutoAttack = true;
	}
	TryCast(*U, Ab, U->TargetId);
}

void FHLGame::ReleaseSpirit()
{
	FHLUnit* U = PlayerUnit();
	if (!U || !U->bDead) return;
	const FHLAreaDef& Def = *Areas[U->Area].Def;
	FVector2D Where = Def.Spawn;
	FString Name = TEXT("the entrance");
	float Best = 1e9f;
	for (const FHLAidStation& A : Def.Aid) { const float D = FVector2D::Distance(A.Pos, U->Pos); if (D < Best) { Best = D; Where = A.Pos; Name = A.Name; } }
	U->bDead = false; U->Hp = U->MaxHp * 0.5f; if (U->ResType == EHLRes::Power) U->Res = U->MaxRes * 0.5f;
	U->CombatUntil = 0;
	for (FHLUnit* M : UnitList) if (M->Kind == EHLKind::Mob) M->Threat.Remove(U->Id);
	TeleportUnit(*U, U->Area, Where);
	if (Def.bDungeon) System(FString::Printf(TEXT("You wake at %s."), *Name));
	else
	{
		// classic ghost run: back to the body to revive, or revive here for durability and sickness
		U->bGhost = true; U->Hp = U->MaxHp; U->bAutoAttack = false; U->TargetId = 0;
		System(FString::Printf(TEXT("Your spirit stands at %s. Return to your body to revive, or speak to the staff here."), *Name));
	}
	// dungeon: fallen party members get up with the player
	for (int32 SI : Party) if (FHLUnit* M = Unit(Sims[SI].UnitId)) { if (M->bDead || Def.bDungeon) { M->bDead = false; M->Hp = M->MaxHp * 0.6f; M->Res = M->ResType == EHLRes::Adrenaline ? 0 : M->MaxRes * 0.6f; M->CombatUntil = 0; TeleportUnit(*M, U->Area, Where + FVector2D(Rand() * 4 - 2, Rand() * 4 - 2)); Sims[SI].State = EHLSimState::Party; } }
}

void FHLGame::ResurrectAtCorpse()
{
	FHLUnit* U = PlayerUnit();
	if (!U || !U->bGhost) return;
	if (U->Area != Player.CorpseArea || FVector2D::Distance(U->Pos, Player.CorpsePos) > 10) { Error(TEXT("You are too far from your body.")); return; }
	U->bGhost = false; U->Hp = U->MaxHp * 0.5f; U->Res = U->ResType == EHLRes::Adrenaline ? 0 : U->MaxRes * 0.5f;
	U->Pos = Player.CorpsePos;
	System(TEXT("You return to your body."));
}

void FHLGame::ResurrectAtStation()
{
	FHLUnit* U = PlayerUnit();
	if (!U || !U->bGhost) return;
	for (const auto& KV : Player.Equip) { float& D = Player.Durability.FindOrAdd(KV.Key, 1.f); D = FMath::Max(0.f, D - 0.25f); }
	Player.SicknessUntil = Now + 600;
	U->bGhost = false;
	RecalcPlayerStats();
	U->Hp = U->MaxHp * 0.5f; U->Res = U->ResType == EHLRes::Adrenaline ? 0 : U->MaxRes * 0.5f;
	FHLAura A; A.Type = EHLAura::Buff; A.Name = TEXT("Resurrection Sickness"); A.Stat = TEXT("none"); A.bHarmful = true; A.Remaining = 600;
	U->Auras.Add(A);
	System(TEXT("You are revived. Resurrection Sickness: attributes reduced by 25% for 10 minutes."));
}

int32 FHLGame::RepairCost() const
{
	// 1 cent x item level per 10% durability (bible §6)
	float C = 0;
	for (const auto& KV : Player.Equip) if (KV.Value) C += KV.Value->ILvl * (1.f - SlotDurability(KV.Key)) * 10.f;
	return FMath::CeilToInt32(C - 0.01f);
}

void FHLGame::RepairAll()
{
	const int32 Cost = RepairCost();
	if (Cost <= 0) { System(TEXT("Your equipment is in good repair.")); return; }
	if (Player.Cents < Cost) { Error(TEXT("You can't afford the repairs.")); return; }
	Player.Cents -= Cost;
	Player.Durability.Reset();
	RecalcPlayerStats();
	System(FString::Printf(TEXT("Repaired for %s."), *HLMoney(Cost)));
}

void FHLGame::TickPlayer(float Dt)
{
	FHLUnit* U = PlayerUnit();
	if (!U) return;
	FHLAreaState& Area = Areas[U->Area];
	if (!U->bDead && !U->Stunned())
	{
		if (Input.bMouseSteer) U->Yaw = Input.CamYaw; else U->Yaw += Input.Turn * 200.f * Dt;
		FVector2D Move(Input.Forward, Input.Strafe);
		if (!Move.IsNearlyZero())
		{
			if (Move.SizeSquared() > 1) Move.Normalize();
			const float Rad = FMath::DegreesToRadians(U->Yaw);
			const FVector2D Fwd(FMath::Cos(Rad), FMath::Sin(Rad)), Right(-FMath::Sin(Rad), FMath::Cos(Rad));
			float Speed = U->Speed * (Input.Forward < 0 ? 0.6f : 1.f) * (U->bGhost ? 1.25f : 1.f);
			for (const FHLAura& A : U->Auras) if (A.Type == EHLAura::Slow) Speed *= (1.f - A.Value);
			U->Pos = Area.Nav.Slide(U->Pos, (Fwd * Move.X + Right * Move.Y) * Speed * Dt);
			if (U->Casting) { U->Casting = nullptr; U->GcdUntil = Now; Error(TEXT("Interrupted.")); }
			if (InteractObj >= 0) InteractObj = -1;
			if (CraftIdx >= 0) { CraftIdx = -1; Error(TEXT("Interrupted.")); }
			if (U->bSitting) { U->bSitting = false; U->Auras.RemoveAll([](const FHLAura& A) { return A.Name == TEXT("Eating") || A.Name == TEXT("Drinking"); }); }
			if (OpenNpcUnitId) if (FHLUnit* N = Unit(OpenNpcUnitId)) if (Dist(*U, *N) > 8) { OpenNpcUnitId = 0; OpenNpcMode.Reset(); }
		}
		if (Input.bJump && U->Z <= 0) U->VelZ = 6.5f;
	}
	Input.bJump = false;
	if (U->Z > 0 || U->VelZ > 0) { U->VelZ -= 20 * Dt; U->Z += U->VelZ * Dt; if (U->Z <= 0) { U->Z = 0; U->VelZ = 0; } }

	// object interaction finishes
	if (InteractObj >= 0 && Now >= InteractEnd)
	{
		const int32 A = InteractArea, Oi = InteractObj;
		InteractObj = -1;
		FHLObjectState& O = Areas[A].Objects[Oi];
		if (O.bActive && !U->bDead)
		{
			const FHLObjectDef& Def = *O.Def;
			O.bActive = false; O.RespawnAt = Now + Def.Respawn;
			if (Def.Gives && Def.Gives->Kind != TEXT("quest")) AddItem(Def.Gives, 1);
			else if (Def.Gives) AddChat(TEXT("loot"), FString(), FString::Printf(TEXT("You receive: [%s]"), *Def.Gives->Name));
			if (Def.bSalvage) { if (Player.Salvage < 75 && Chance(0.8f)) { Player.Salvage++; System(FString::Printf(TEXT("Your skill in Salvaging has increased to %d."), Player.Salvage)); } }
			QuestEvent(Def.bUse ? TEXT("use") : TEXT("object"), Def.Kind);
			if (Def.Kind == TEXT("usher")) AddChat(TEXT("npc"), TEXT("Captured Usher"), TEXT("Bless you. I'm going out the front, and I'm not coming back."));
		}
	}
	TickCraft();
	if (Player.SicknessUntil > 0 && Now >= Player.SicknessUntil) { Player.SicknessUntil = 0; RecalcPlayerStats(); System(TEXT("Resurrection Sickness has faded.")); }
	// subzone discovery
	static float NextZoneCheck = 0;
	if (Now >= NextZoneCheck)
	{
		NextZoneCheck = Now + 0.5f;
		const FString Sz = SubzoneAt(U->Area, U->Pos);
		if (Sz != CurSubzone)
		{
			CurSubzone = Sz;
			const FHLSubzone* SzDef = Area.Def->Subzones.FindByPredicate([&Sz](const FHLSubzone& S) { return S.Name == Sz; });
			if (SzDef && SzDef->bHostile)
			{
				Banner(Sz, FString::Printf(TEXT("Hostile territory, levels %s"), *SzDef->Levels), 4);
				bBannerHostile = true;
				if (U->Level + 5 < FCString::Atoi(*SzDef->Levels)) Error(TEXT("The creatures here are far beyond you."));
			}
			else Banner(Sz, SzDef && !SzDef->Levels.IsEmpty() ? FString::Printf(TEXT("Levels %s"), *SzDef->Levels) : FString(), 3);
			if (!Player.Discovered.Contains(Sz)) { Player.Discovered.Add(Sz); System(FString::Printf(TEXT("Discovered: %s"), *Sz)); GrantXp(10 + 5 * U->Level, true); }
		}
		// portals
		static float PortalLock = 0;
		if (!U->bDead && Now >= PortalLock) for (const FHLPortal& Po : Area.Def->Portals)
		{
			if (FVector2D::Distance(U->Pos, Po.Pos) > Po.R) continue;
			const int32 To = FHLData::Get().AreaIndex(Po.ToArea);
			if (To < 0) break;
			if (InCombat(*U)) { Error(TEXT("You can't enter while in combat.")); break; }
			const bool bEnterDungeon = Areas[To].Def->bDungeon;
			if (bEnterDungeon && (bDungeonCleared || Now - DungeonLeftAt > 600)) { SpawnArea(To, true); bDungeonCleared = false; }
			if (!bEnterDungeon) DungeonLeftAt = Now;
			TeleportUnit(*U, To, Po.To);
			for (int32 SI : Party) if (FHLUnit* M = Unit(Sims[SI].UnitId)) { M->bDead = false; if (M->Hp <= 0) M->Hp = M->MaxHp * 0.5f; TeleportUnit(*M, To, Po.To + FVector2D(Rand() * 6 - 3, Rand() * 3 + 1)); Sims[SI].State = EHLSimState::Party; }
			PortalLock = Now + 2;
			CurSubzone = SubzoneAt(To, U->Pos);
			Banner(Areas[To].Def->Name, bEnterDungeon ? TEXT("Dungeon, levels 8-12, 5 players") : CurSubzone, 4);
			if (bEnterDungeon && Party.Num() < 2) System(TEXT("The Egyptian Theatre is tuned for a party of five. Invite players with /invite <name> or answer an LFG shout."));
			break;
		}
		// drop far or vanished targets
		if (U->TargetId) { const FHLUnit* T = Unit(U->TargetId); if (!T || T->Area != U->Area || Dist(*U, *T) > 90) U->TargetId = 0; }
	}
}

// ------------------------------------------------------------------------------------------------------------- quests
const FHLQuestState* FHLGame::QuestState(const FString& Id) const
{
	for (const FHLQuestState& QS : Player.Quests) if (QS.Q->Id == Id) return &QS;
	return nullptr;
}

bool FHLGame::QuestAvailable(const FHLQuest& Q) const
{
	const FHLUnit* U = PlayerUnit();
	if (!U || Player.Done.Contains(Q.Id) || QuestState(Q.Id)) return false;
	if (U->Level < Q.MinLevel) return false;
	if (!Q.Cls.IsEmpty() && (!Player.Cls || Player.Cls->Id != Q.Cls)) return false;
	if (!Q.Prereq.IsEmpty() && !Player.Done.Contains(Q.Prereq)) return false;
	return true;
}

bool FHLGame::QuestReadyAt(const FHLQuest& Q, const FString& NpcId) const
{
	const FHLQuestState* QS = QuestState(Q.Id);
	return QS && QS->bComplete && Q.Turnin == NpcId;
}

void FHLGame::AcceptQuest(const FHLQuest& Q)
{
	if (!QuestAvailable(Q)) return;
	if (Player.Quests.Num() >= 20) { Error(TEXT("Your quest log is full.")); return; }
	if (!CanStartEscort(Q, true)) return;
	FHLQuestState QS; QS.Q = &Q; QS.Progress.Init(0, Q.Objectives.Num());
	Player.Quests.Add(QS);
	System(FString::Printf(TEXT("Quest accepted: %s"), *Q.Name));
	if (!Q.Breadcrumb.IsEmpty()) System(TEXT("That road leads out through the hive wall. The guards at the gap are far above your level."));
	StartEscort(Q);
}

void FHLGame::AbandonQuest(const FString& Id)
{
	Player.Quests.RemoveAll([&](const FHLQuestState& QS) { return QS.Q->Id == Id; });
}

void FHLGame::QuestEvent(const TCHAR* Type, const FString& Key, int32 Count, const FHLUnit* Mob)
{
	const FString T(Type);
	for (FHLQuestState& QS : Player.Quests)
	{
		bool bChanged = false;
		for (int32 i = 0; i < QS.Q->Objectives.Num(); ++i)
		{
			const FHLObjective& Oj = QS.Q->Objectives[i];
			if (QS.Progress[i] >= Oj.Count) continue;
			bool bHit = false;
			if (T == TEXT("kill"))
			{
				if (Oj.Type == TEXT("kill") && Oj.Mobs.Contains(Key)) bHit = true;
				else if (Oj.Type == TEXT("item") && Oj.From.Contains(Key) && Chance(Oj.Rate)) { bHit = true; AddChat(TEXT("loot"), FString(), FString::Printf(TEXT("You receive: [%s]"), *Oj.Item)); }
			}
			else if (Oj.Type == T && (Oj.Kind == Key || Oj.Npc == Key || (T == TEXT("craft") && Oj.Item == Key))) bHit = true;
			if (!bHit) continue;
			QS.Progress[i] = FMath::Min(Oj.Count, QS.Progress[i] + Count);
			bChanged = true;
			if (Oj.Type != TEXT("talk")) AddChat(TEXT("quest"), FString(), FString::Printf(TEXT("%s: %d/%d"), *Oj.Label, QS.Progress[i], Oj.Count));
		}
		if (bChanged && !QS.bComplete)
		{
			bool bAll = true;
			for (int32 i = 0; i < QS.Progress.Num(); ++i) if (QS.Progress[i] < QS.Q->Objectives[i].Count) bAll = false;
			if (bAll) { QS.bComplete = true; if (QS.Q->Type != TEXT("talk")) { AddChat(TEXT("quest"), FString(), FString::Printf(TEXT("%s (Complete)"), *QS.Q->Name)); AddFloat(PlayerUnitId, TEXT("Quest complete"), FLinearColor(1, 0.85f, 0.2f)); } }
		}
	}
}

void FHLGame::CompleteQuest(const FHLQuest& Q, int32 Choice)
{
	const FHLQuestState* QS = QuestState(Q.Id);
	if (!QS || !QS->bComplete) return;
	for (const auto& Tk : Q.Takes) if (CountItem(Tk.Key) < Tk.Value) { Error(FString::Printf(TEXT("You need %d x %s."), Tk.Value, *Tk.Key->Name)); return; }
	if (Q.Choice.Num())
	{
		if (!Q.Choice.IsValidIndex(Choice)) { Error(TEXT("Choose a reward.")); return; }
		if (!AddItem(Q.Choice[Choice], 1)) return;
	}
	for (const auto& Tk : Q.Takes) RemoveItem(Tk.Key, Tk.Value);
	Player.Quests.RemoveAll([&](const FHLQuestState& S) { return S.Q == &Q; });
	Player.Done.Add(Q.Id);
	Player.Cents += Q.Cents;
	System(FString::Printf(TEXT("%s completed."), *Q.Name));
	if (Q.Cents) AddChat(TEXT("loot"), FString(), FString::Printf(TEXT("You receive %s."), *HLMoney(Q.Cents)));
	for (const auto& KV : Q.Rep) { Player.Rep.FindOrAdd(KV.Key) += KV.Value; System(FString::Printf(TEXT("Your reputation with the Los Angeles Police Department has increased by %d."), KV.Value)); }
	GrantXp(Q.Xp, true);
	SaveNow();
}

int32 FHLGame::NpcMarker(const FHLNpcDef& N) const
{
	int32 Best = 0;
	for (const FHLQuest& Q : FHLData::Get().Quests)
	{
		if (QuestReadyAt(Q, N.Id)) return 2;
		if (Q.Giver == N.Id && QuestAvailable(Q)) Best = FMath::Max(Best, 1);
		else if (Q.Turnin == N.Id && QuestState(Q.Id) && Best == 0) Best = 3;
	}
	return Best;
}

// ------------------------------------------------------------------------------------------------------------- auction house
int32 FHLGame::MarketPrice(const FHLItem& It) const
{
	// vendor price x demand multiplier (1.5..8): rises with the number of simulated players in the item's level band
	int32 InBand = 0;
	for (const FHLSim& S : Sims) if (S.bOnline && FMath::Abs(S.Level - It.Req) <= 5) ++InBand;
	const float Demand = FMath::Clamp(1.5f + 6.5f * InBand / FMath::Max(1, Sims.Num() / 3), 1.5f, 8.f);
	return FMath::Max(2, FMath::RoundToInt32(It.Price * Demand));
}

void FHLGame::TickAuctions()
{
	NextAuctionChurn = Now + 20;
	const FHLData& D = FHLData::Get();
	if (!Sims.Num()) return;
	auto AddListing = [&]()
	{
		// stock follows the population's level spread: pick a seller, list something their level would drop or craft
		const int32 SI = RandInt(0, Sims.Num() - 1);
		const FHLSim& S = Sims[SI];
		const FHLItem* It = nullptr;
		const float R = Rand();
		if (R < 0.2f)
		{
			static const TCHAR* Goods[] = { TEXT("Scrap Metal"), TEXT("Chitin Shard"), TEXT("Coyote Pelt"), TEXT("Invader Gland"), TEXT("Brass Casings"), TEXT("Wool Cloth"), TEXT("White Sage"), TEXT("Hive Resin"), TEXT("Elder Chitin Plate"), TEXT("Bootleg Gin") };
			It = D.ItemByName(Goods[RandInt(0, 9)]);
		}
		else
		{
			const TArray<const FHLItem*>& Pool = D.EquipPool(S.Level, R < 0.85f ? 2 : (R < 0.97f ? 3 : 4));
			if (Pool.Num()) It = Pool[RandInt(0, Pool.Num() - 1)];
		}
		if (!It) return;
		FHLAuction A; A.Item = It; A.SellerIdx = SI; A.Count = It->Stack > 1 ? RandInt(1, 4) * 5 : 1;
		A.Buyout = FMath::Max(2, FMath::RoundToInt32(MarketPrice(*It) * (0.8f + Rand() * 0.8f))) * A.Count;
		A.MinBid = FMath::Max(1, FMath::RoundToInt32(A.Buyout * (0.55f + Rand() * 0.2f)));
		if (Chance(0.3f)) A.Bid = A.MinBid + RandInt(0, A.MinBid / 10); // someone already bid
		A.ExpiresAt = Now + 600 + Rand() * 3000;
		Auctions.Add(A);
	};
	if (!bAuctionSeeded) { bAuctionSeeded = true; for (int32 i = 0; i < 160; ++i) AddListing(); }
	for (int32 i = Auctions.Num() - 1; i >= 0; --i)
	{
		FHLAuction& A = Auctions[i];
		if (A.bMine)
		{
			// a simulated player buys it if the price is fair
			const int32 Fair = MarketPrice(*A.Item) * A.Count;
			const bool bSold = Chance((A.Buyout <= Fair * 1.3f ? 0.25f : A.Buyout <= Fair * 2 ? 0.05f : 0.f) * (Undercut(A) ? 0.15f : 1.f));
			if (bSold || (Now >= A.ExpiresAt && A.Bid > 0))
			{
				const int32 Price = bSold ? A.Buyout : A.Bid;
				const int32 Net = FMath::RoundToInt32(Price * 0.95f);
				const FString Buyer = Sims[RandInt(0, Sims.Num() - 1)].P->Handle;
				SendMail(TEXT("Hollywood Auction House"), FString::Printf(TEXT("Auction successful: %s"), *A.Item->Name),
					FString::Printf(TEXT("Buyer: %s\nSale price: %s\nAuction house cut (5%%): %s"), *Buyer, *HLMoney(Price), *HLMoney(Price - Net)), Net);
				Auctions.RemoveAt(i);
			}
			else if (Now >= A.ExpiresAt) { SendMail(TEXT("Hollywood Auction House"), FString::Printf(TEXT("Auction expired: %s"), *A.Item->Name), TEXT("Your auction has expired without a buyer. The item is enclosed."), 0, A.Item, A.Count); Auctions.RemoveAt(i); }
			else if (A.Bid == 0 && Chance(0.02f) && A.Buyout <= Fair * 2.5f) A.Bid = FMath::Max(1, FMath::RoundToInt32(Fair * (0.5f + Rand() * 0.3f))); // a sim bids on it
			continue;
		}
		if (A.bPlayerBid)
		{
			// rival bidders: someone may outbid the player before the auction closes
			if (Now >= A.ExpiresAt)
			{
				SendMail(TEXT("Hollywood Auction House"), FString::Printf(TEXT("Auction won: %s"), *A.Item->Name), FString::Printf(TEXT("You won an auction for %s with a bid of %s."), *A.Item->Name, *HLMoney(A.Bid)), 0, A.Item, A.Count);
				Auctions.RemoveAt(i);
			}
			else if (Chance(A.Bid < MarketPrice(*A.Item) * A.Count * 0.9f ? 0.05f : 0.015f))
			{
				SendMail(TEXT("Hollywood Auction House"), FString::Printf(TEXT("Outbid on %s"), *A.Item->Name), FString::Printf(TEXT("You have been outbid on %s. Your bid of %s is returned."), *A.Item->Name, *HLMoney(A.Bid)), A.Bid);
				A.Bid = NextBid(A); A.bPlayerBid = false;
				if (A.Bid >= A.Buyout) Auctions.RemoveAt(i);
			}
			continue;
		}
		if (Now >= A.ExpiresAt || Chance(0.03f)) Auctions.RemoveAt(i);
	}
	for (int32 i = 0; i < 6 && Auctions.Num() < 170; ++i) AddListing();
}

bool FHLGame::BuyAuction(int32 Idx)
{
	if (!Auctions.IsValidIndex(Idx)) return false;
	const FHLAuction A = Auctions[Idx];
	if (A.bMine) { AddItem(A.Item, A.Count, false); Auctions.RemoveAt(Idx); System(TEXT("Auction cancelled.")); return true; }
	const int32 Refund = A.bPlayerBid ? A.Bid : 0; // buying out over your own bid
	if (Player.Cents + Refund < A.Buyout) { Error(TEXT("You can't afford that.")); return false; }
	if (!AddItem(A.Item, A.Count, false)) return false;
	Player.Cents += Refund - A.Buyout;
	System(FString::Printf(TEXT("You won an auction for [%s]."), *A.Item->Name));
	Auctions.RemoveAt(Idx);
	return true;
}

bool FHLGame::ListAuction(int32 BagIdx, int32 Buyout, float Hours)
{
	if (!Player.Bag.IsValidIndex(BagIdx) || !Player.Bag[BagIdx].Item) return false;
	const FHLInvSlot S = Player.Bag[BagIdx];
	if (S.Item->Kind == TEXT("quest") || S.Item->Price <= 0) { Error(TEXT("You can't auction that.")); return false; }
	const int32 Deposit = FMath::Max(1, FMath::RoundToInt32(S.Item->Price * S.Count / S.Item->Buy * 0.05f * Hours));
	if (Player.Cents < Deposit) { Error(TEXT("You can't afford the deposit.")); return false; }
	Player.Cents -= Deposit;
	FHLAuction A; A.Item = S.Item; A.Count = S.Count; A.Buyout = FMath::Max(1, Buyout); A.MinBid = FMath::Max(1, A.Buyout * 7 / 10); A.bMine = true; A.ExpiresAt = Now + Hours * 3600.f;
	Auctions.Insert(A, 0);
	Player.Bag[BagIdx] = FHLInvSlot();
	System(FString::Printf(TEXT("Auction created for [%s] at %s (deposit %s)."), *A.Item->Name, *HLMoney(A.Buyout), *HLMoney(Deposit)));
	return true;
}

// ------------------------------------------------------------------------------------------------------------- loot rolls
void FHLGame::StartRoll(const FHLItem* Item)
{
	Roll = FHLRoll(); Roll.Item = Item; Roll.bActive = true; Roll.EndsAt = Now + 12; Roll.PlayerChoice = -1;
	AddChat(TEXT("loot"), FString(), FString::Printf(TEXT("Rolling for [%s]: Need, Greed or Pass."), *Item->Name));
}

void FHLGame::FinishRoll()
{
	if (!Roll.bActive) return;
	Roll.bActive = false;
	const FHLItem* It = Roll.Item;
	struct FEntry { FString Name; int32 SimIdx; int32 Kind; int32 Value; };
	TArray<FEntry> Rolls;
	if (Roll.PlayerChoice == 0 || Roll.PlayerChoice == 1) Rolls.Add({ Player.Name, -1, Roll.PlayerChoice == 0 ? 2 : 1, RandInt(1, 100) });
	for (int32 SI : Party)
	{
		const FHLSim& S = Sims[SI];
		const FHLClass* C = S.P->Cls;
		const bool bUsable = C && (It->Kind == TEXT("weapon") ? C->Weapons.Contains(It->Type) : (It->Type == TEXT("jewel") || It->Type == C->Armour));
		const bool bWants = bUsable && It->Stats[HLStatIndex(C->Primary)] > 0;
		const int32 Kind = bWants && Chance(0.75f) ? 2 : Chance(0.7f) ? 1 : 0;
		if (Kind) Rolls.Add({ S.P->Handle, SI, Kind, RandInt(1, 100) });
	}
	if (!Rolls.Num()) { AddChat(TEXT("loot"), FString(), FString::Printf(TEXT("Everyone passed on [%s]."), *It->Name)); return; }
	Rolls.Sort([](const FEntry& A, const FEntry& B) { return A.Kind != B.Kind ? A.Kind > B.Kind : A.Value > B.Value; });
	for (const FEntry& E : Rolls) AddChat(TEXT("loot"), FString(), FString::Printf(TEXT("%s rolls %s: %d for [%s]"), *E.Name, E.Kind == 2 ? TEXT("Need") : TEXT("Greed"), E.Value, *It->Name));
	const FEntry& W = Rolls[0];
	AddChat(TEXT("loot"), FString(), FString::Printf(TEXT("%s wins [%s]."), *W.Name, *It->Name));
	if (W.SimIdx < 0) { AddItem(It, 1, false); for (int32 SI : Party) if (Chance(0.4f)) SimSay(SI, TEXT("party"), TEXT("loot_lose"), 1.5f + Rand() * 3); }
	else { SimSay(W.SimIdx, TEXT("party"), TEXT("loot_win"), 1.5f + Rand() * 2); }
}
