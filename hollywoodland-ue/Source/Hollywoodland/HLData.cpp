#include "HLData.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

const TCHAR* HLStatNames[HL_NUM_STATS] = { TEXT("Strength"), TEXT("Agility"), TEXT("Stamina"), TEXT("Intellect"), TEXT("Spirit") };

int32 HLStatIndex(const FString& S)
{
	if (S == TEXT("str")) return HL_STR;
	if (S == TEXT("agi")) return HL_AGI;
	if (S == TEXT("sta")) return HL_STA;
	if (S == TEXT("int")) return HL_INT;
	return HL_SPI;
}
FLinearColor HLQualityColor(int32 Q)
{
	switch (Q)
	{
	case 0: return FLinearColor(0.62f, 0.62f, 0.62f);
	case 1: return FLinearColor(1, 1, 1);
	case 2: return FLinearColor(0.12f, 1.f, 0.f);
	case 3: return FLinearColor(0.f, 0.44f, 0.87f);
	default: return FLinearColor(0.64f, 0.21f, 0.93f);
	}
}
FString HLMoney(int32 Cents)
{
	if (Cents >= 100) return FString::Printf(TEXT("$%d.%02d"), Cents / 100, Cents % 100);
	return FString::Printf(TEXT("%dc"), Cents);
}

namespace
{
	typedef TSharedPtr<FJsonObject> JObj;
	FString S(const JObj& O, const TCHAR* K) { FString V; if (O.IsValid()) O->TryGetStringField(K, V); return V; }
	double N(const JObj& O, const TCHAR* K, double Def = 0) { double V = Def; if (O.IsValid()) O->TryGetNumberField(K, V); return V; }
	bool B(const JObj& O, const TCHAR* K, bool Def = false) { bool V = Def; if (O.IsValid()) O->TryGetBoolField(K, V); return V; }
	const TArray<TSharedPtr<FJsonValue>>& A(const JObj& O, const TCHAR* K)
	{
		static const TArray<TSharedPtr<FJsonValue>> Empty;
		const TArray<TSharedPtr<FJsonValue>>* V = nullptr;
		if (O.IsValid() && O->TryGetArrayField(K, V) && V) return *V;
		return Empty;
	}
	JObj Sub(const JObj& O, const TCHAR* K) { const JObj* V = nullptr; if (O.IsValid() && O->TryGetObjectField(K, V) && V) return *V; return nullptr; }
	TArray<FString> SA(const JObj& O, const TCHAR* K) { TArray<FString> Out; for (auto& V : A(O, K)) Out.Add(V->AsString()); return Out; }
	FLinearColor Col(const JObj& O, const TCHAR* K, FLinearColor Def = FLinearColor::Gray)
	{
		const auto& Arr = A(O, K);
		if (Arr.Num() >= 3) return FLinearColor(Arr[0]->AsNumber(), Arr[1]->AsNumber(), Arr[2]->AsNumber());
		return Def;
	}
	FVector2D V2(const JObj& O, const TCHAR* K)
	{
		const auto& Arr = A(O, K);
		return Arr.Num() >= 2 ? FVector2D(Arr[0]->AsNumber(), Arr[1]->AsNumber()) : FVector2D::ZeroVector;
	}
	JObj LoadFile(const TCHAR* Name, FString& Err)
	{
		FString Text;
		const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / Name;
		if (!FFileHelper::LoadFileToString(Text, *Path)) { Err += FString::Printf(TEXT("missing %s; "), Name); return nullptr; }
		JObj Obj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) { Err += FString::Printf(TEXT("bad json %s; "), Name); return nullptr; }
		return Obj;
	}
}

FHLData& FHLData::Get() { static FHLData D; return D; }

const FHLNpcDef* FHLData::Npc(const FString& Id) const
{
	for (const FHLAreaDef& Ar : Areas) for (const FHLNpcDef& Np : Ar.Npcs) if (Np.Id == Id) return &Np;
	return nullptr;
}

const TArray<const FHLItem*>& FHLData::EquipPool(int32 Level, int32 Quality) const
{
	static const int32 Bands[] = { 1, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60 };
	int32 Band = 1;
	for (int32 Bd : Bands) if (Bd <= Level) Band = Bd;
	const int32 Key = Band * 10 + Quality;
	if (TArray<const FHLItem*>* P = Pools.Find(Key)) return *P;
	TArray<const FHLItem*>& Pool = Pools.Add(Key);
	for (const FHLItem& It : Items) if (It.IsEquip() && It.Q == Quality && It.Req == Band && !(It.bNamed && Quality == 3 && Band == 10)) Pool.Add(&It);
	return Pool;
}

bool FHLData::Load(FString& Err)
{
	if (bLoaded) return true;
	// classes + abilities
	if (JObj R = LoadFile(TEXT("classes.json"), Err))
	{
		for (auto& V : A(R, TEXT("abilities")))
		{
			JObj O = V->AsObject();
			FHLAbility Ab;
			Ab.Id = S(O, TEXT("id")); Ab.Name = S(O, TEXT("name")); Ab.Class = S(O, TEXT("class")); Ab.Target = S(O, TEXT("target"));
			Ab.School = S(O, TEXT("school")); Ab.Desc = S(O, TEXT("desc")); Ab.Level = N(O, TEXT("level"), 1);
			Ab.Cost = N(O, TEXT("cost")); Ab.Range = N(O, TEXT("range"), 5); Ab.Cast = N(O, TEXT("cast")); Ab.Cd = N(O, TEXT("cd"));
			Ab.Radius = N(O, TEXT("radius")); Ab.bGcd = B(O, TEXT("gcd"), true); Ab.Flavor = S(O, TEXT("flavor")); Ab.Icon = N(O, TEXT("icon"), -1);
			for (auto& EV : A(O, TEXT("effects")))
			{
				JObj E = EV->AsObject();
				FHLEffect Ef;
				Ef.Type = S(E, TEXT("type")); Ef.Stat = S(E, TEXT("stat")); Ef.Weapon = N(E, TEXT("weapon")); Ef.Base = N(E, TEXT("base"));
				Ef.Coef = N(E, TEXT("coef")); Ef.Dur = N(E, TEXT("dur")); Ef.Period = N(E, TEXT("period")); Ef.Pct = N(E, TEXT("pct"));
				Ef.Value = N(E, TEXT("value")); Ef.Ticks = N(E, TEXT("ticks"));
				Ab.Effects.Add(Ef);
			}
			Abilities.Add(MoveTemp(Ab));
		}
		for (const FHLAbility& Ab : Abilities) AbilityById.Add(Ab.Id, &Ab);
		for (auto& V : A(R, TEXT("talents")))
		{
			JObj O = V->AsObject();
			FHLTalent T;
			T.Id = S(O, TEXT("id")); T.Name = S(O, TEXT("name")); T.Class = S(O, TEXT("class")); T.Desc = S(O, TEXT("desc"));
			T.Spec = N(O, TEXT("spec")); T.Tier = N(O, TEXT("tier"), 1); T.Col = N(O, TEXT("col")); T.Ranks = N(O, TEXT("ranks"), 1); T.Icon = N(O, TEXT("icon"), -1);
			if (JObj M = Sub(O, TEXT("mods"))) for (const auto& KV : M->Values) T.Mods.Add(FString(KV.Key), KV.Value->AsNumber());
			Talents.Add(MoveTemp(T));
		}
		for (const FHLTalent& T : Talents) TalentById.Add(T.Id, &T);
		for (auto& V : A(R, TEXT("classes")))
		{
			JObj O = V->AsObject();
			FHLClass C;
			C.Id = S(O, TEXT("id")); C.Name = S(O, TEXT("name")); C.Resource = S(O, TEXT("resource")); C.Primary = S(O, TEXT("primary"));
			C.Secondary = S(O, TEXT("secondary")); C.Armour = S(O, TEXT("armour")); C.Role = S(O, TEXT("role")); C.HpPerLevel = N(O, TEXT("hp_per_level"), 15);
			C.bRanged = B(O, TEXT("ranged")); C.bCaster = B(O, TEXT("caster")); C.Color = Col(O, TEXT("color"));
			C.Weapons = SA(O, TEXT("weapons")); C.StartItems = SA(O, TEXT("start_items")); C.Specs = SA(O, TEXT("specs"));
			for (const FString& Id : SA(O, TEXT("abilities"))) if (const FHLAbility* Ab = Ability(Id)) C.Abilities.Add(Ab);
			C.Desc = S(O, TEXT("desc")); C.Icon = N(O, TEXT("icon"), -1); C.SpecDesc = SA(O, TEXT("spec_desc"));
			for (auto& IV : A(O, TEXT("spec_icons"))) C.SpecIcons.Add((int32)IV->AsNumber());
			for (const FHLTalent& T : Talents) if (T.Class == C.Id) C.Talents.Add(&T);
			Classes.Add(MoveTemp(C));
		}
		for (const FHLClass& C : Classes) ClassById.Add(C.Id, &C);
	}
	// items
	if (JObj R = LoadFile(TEXT("items.json"), Err))
	{
		const auto& Arr = A(R, TEXT("items"));
		Items.Reserve(Arr.Num());
		for (auto& V : Arr)
		{
			JObj O = V->AsObject();
			FHLItem It;
			It.Id = S(O, TEXT("id")); It.Name = S(O, TEXT("name")); It.Kind = S(O, TEXT("kind")); It.Slot = S(O, TEXT("slot")); It.Type = S(O, TEXT("type"));
			It.Blurb = S(O, TEXT("blurb")); It.Q = N(O, TEXT("q"), 1); It.Req = N(O, TEXT("req")); It.ILvl = N(O, TEXT("ilvl"), 1); It.Armor = N(O, TEXT("armor"));
			It.DMin = N(O, TEXT("dmin")); It.DMax = N(O, TEXT("dmax")); It.Speed = N(O, TEXT("speed")); It.Price = N(O, TEXT("price")); It.Stack = N(O, TEXT("stack"), 1);
			It.Hands = N(O, TEXT("hands"), 1); It.bNamed = B(O, TEXT("named")); It.Desc = S(O, TEXT("desc")); It.Icon = N(O, TEXT("icon"), -1);
			It.Ammo = S(O, TEXT("ammo")); It.AmmoDps = N(O, TEXT("ammo_dps")); It.Buy = FMath::Max(1, (int32)N(O, TEXT("buy"), 1));
			if (JObj St = Sub(O, TEXT("stats")))
			{
				It.Stats[HL_STR] = N(St, TEXT("str")); It.Stats[HL_AGI] = N(St, TEXT("agi")); It.Stats[HL_STA] = N(St, TEXT("sta"));
				It.Stats[HL_INT] = N(St, TEXT("int")); It.Stats[HL_SPI] = N(St, TEXT("spi"));
			}
			if (JObj U = Sub(O, TEXT("use")))
			{
				It.UseType = S(U, TEXT("type")); It.UseAmount = N(U, TEXT("amount")); It.UseStat = S(U, TEXT("stat")); It.UseValue = N(U, TEXT("value")); It.UseDur = N(U, TEXT("dur"));
			}
			Items.Add(MoveTemp(It));
		}
		for (const FHLItem& It : Items) { ItemById.Add(It.Id, &It); if (!ItemByNameMap.Contains(It.Name)) ItemByNameMap.Add(It.Name, &It); }
	}
	// mobs
	if (JObj R = LoadFile(TEXT("mobs.json"), Err))
	{
		for (auto& V : A(R, TEXT("mobs")))
		{
			JObj O = V->AsObject();
			FHLMobDef M;
			M.Id = S(O, TEXT("id")); M.Name = S(O, TEXT("name")); M.Family = S(O, TEXT("family")); M.Rank = S(O, TEXT("rank")); M.Shape = S(O, TEXT("shape"));
			M.Title = S(O, TEXT("title")); M.Script = S(O, TEXT("script")); M.LevelMin = N(O, TEXT("level_min"), 1); M.LevelMax = N(O, TEXT("level_max"), 1);
			M.Hp = N(O, TEXT("hp"), 1); M.Dmg = N(O, TEXT("dmg"), 1); M.Armor = N(O, TEXT("armor"), 1); M.Social = N(O, TEXT("social")); M.Speed = N(O, TEXT("speed"), 6.5);
			M.Swing = N(O, TEXT("swing"), 2); M.Scale = N(O, TEXT("scale"), 1); M.Respawn = N(O, TEXT("respawn"), 90);
			M.bAggressive = B(O, TEXT("aggressive"), true); M.bFlees = B(O, TEXT("flees")); M.bRanged = B(O, TEXT("ranged")); M.bMoney = B(O, TEXT("money"), true); M.bGuard = B(O, TEXT("guard"));
			M.Color = Col(O, TEXT("color"));
			for (const FString& Id : SA(O, TEXT("abilities"))) if (const FHLAbility* Ab = Ability(Id)) M.Abilities.Add(Ab);
			for (const FString& Id : SA(O, TEXT("junk"))) if (const FHLItem* It = Item(Id)) M.Junk.Add(It);
			for (const FString& Id : SA(O, TEXT("boss_loot"))) if (const FHLItem* It = Item(Id)) M.BossLoot.Add(It);
			for (auto& TV : A(O, TEXT("trade"))) { JObj T = TV->AsObject(); if (const FHLItem* It = Item(S(T, TEXT("item")))) M.Trade.Add({ It, (float)N(T, TEXT("rate")) }); }
			Mobs.Add(MoveTemp(M));
		}
		for (const FHLMobDef& M : Mobs) MobById.Add(M.Id, &M);
	}
	// areas
	if (JObj R = LoadFile(TEXT("areas.json"), Err))
	{
		for (auto& V : A(R, TEXT("areas")))
		{
			JObj O = V->AsObject();
			FHLAreaDef Ar;
			Ar.Id = S(O, TEXT("id")); Ar.Name = S(O, TEXT("name")); Ar.Min = V2(O, TEXT("min")); Ar.Size = V2(O, TEXT("size")); Ar.Offset = V2(O, TEXT("offset"));
			Ar.Spawn = V2(O, TEXT("spawn")); Ar.Cell = N(O, TEXT("cell"), 2); Ar.bDungeon = B(O, TEXT("dungeon")); Ar.Sky = Col(O, TEXT("sky")); Ar.Floor = Col(O, TEXT("floor"));
			auto ReadBoxes = [](const TArray<TSharedPtr<FJsonValue>>& Arr, TArray<FHLBox>& Out)
			{
				for (auto& BV : Arr)
				{
					JObj Bx = BV->AsObject();
					FHLBox Box;
					Box.X = N(Bx, TEXT("x")); Box.Y = N(Bx, TEXT("y")); Box.W = N(Bx, TEXT("w")); Box.D = N(Bx, TEXT("d")); Box.H = N(Bx, TEXT("h"), 0.05);
					Box.Color = Col(Bx, TEXT("color")); Box.Label = S(Bx, TEXT("label")); Box.bSolid = B(Bx, TEXT("solid"), true);
					Out.Add(Box);
				}
			};
			ReadBoxes(A(O, TEXT("boxes")), Ar.Boxes);
			ReadBoxes(A(O, TEXT("ground")), Ar.Ground);
			for (auto& SV : A(O, TEXT("subzones"))) { JObj Z = SV->AsObject(); Ar.Subzones.Add({ S(Z, TEXT("name")), (float)N(Z, TEXT("x")), (float)N(Z, TEXT("y")), (float)N(Z, TEXT("w")), (float)N(Z, TEXT("d")), S(Z, TEXT("levels")), B(Z, TEXT("hostile")) }); }
			for (auto& NV : A(O, TEXT("npcs")))
			{
				JObj Nn = NV->AsObject();
				FHLNpcDef Np;
				Np.Id = S(Nn, TEXT("id")); Np.Name = S(Nn, TEXT("name")); Np.Title = S(Nn, TEXT("title")); Np.Trains = S(Nn, TEXT("trains")); Np.Hub = S(Nn, TEXT("hub"));
				Np.Pos = FVector2D(N(Nn, TEXT("x")), N(Nn, TEXT("y"))); Np.Yaw = N(Nn, TEXT("yaw")); Np.Roles = SA(Nn, TEXT("roles")); Np.Color = Col(Nn, TEXT("color"));
				for (const FString& Id : SA(Nn, TEXT("stock"))) if (const FHLItem* It = Item(Id)) Np.Stock.Add(It);
				Ar.Npcs.Add(MoveTemp(Np));
			}
			for (auto& CV : A(O, TEXT("camps")))
			{
				JObj Cc = CV->AsObject();
				FHLCamp Cp;
				Cp.Id = S(Cc, TEXT("id")); Cp.Mob = Mob(S(Cc, TEXT("mob"))); Cp.Count = N(Cc, TEXT("count"), 1); Cp.Pos = FVector2D(N(Cc, TEXT("x")), N(Cc, TEXT("y")));
				Cp.R = N(Cc, TEXT("r"), 10); Cp.Leash = N(Cc, TEXT("leash"), 40);
				for (auto& PV : A(Cc, TEXT("patrol"))) { const auto& P = PV->AsArray(); if (P.Num() >= 2) Cp.Patrol.Add(FVector2D(P[0]->AsNumber(), P[1]->AsNumber())); }
				if (Cp.Mob) Ar.Camps.Add(Cp);
			}
			for (auto& OV : A(O, TEXT("objects")))
			{
				JObj Oo = OV->AsObject();
				FHLObjectDef Ob;
				Ob.Id = S(Oo, TEXT("id")); Ob.Kind = S(Oo, TEXT("kind")); Ob.Name = S(Oo, TEXT("name")); Ob.Quest = S(Oo, TEXT("quest")); Ob.Boss = S(Oo, TEXT("boss"));
				Ob.Pos = FVector2D(N(Oo, TEXT("x")), N(Oo, TEXT("y"))); Ob.Respawn = N(Oo, TEXT("respawn"), 60); Ob.Gives = Item(S(Oo, TEXT("gives_id")));
				Ob.bUse = B(Oo, TEXT("use")); Ob.bSalvage = B(Oo, TEXT("salvage")); Ob.bTall = B(Oo, TEXT("tall")); Ob.bBig = B(Oo, TEXT("big"));
				Ar.Objects.Add(MoveTemp(Ob));
			}
			for (auto& PV : A(O, TEXT("portals")))
			{
				JObj Pp = PV->AsObject();
				FHLPortal Po;
				Po.Id = S(Pp, TEXT("id")); Po.ToArea = S(Pp, TEXT("to_area")); Po.Label = S(Pp, TEXT("label")); Po.Pos = FVector2D(N(Pp, TEXT("x")), N(Pp, TEXT("y")));
				Po.To = V2(Pp, TEXT("to")); Po.R = N(Pp, TEXT("r"), 3);
				Ar.Portals.Add(Po);
			}
			for (auto& EV : A(O, TEXT("exits"))) { JObj Ee = EV->AsObject(); Ar.Exits.Add({ S(Ee, TEXT("id")), S(Ee, TEXT("name")), S(Ee, TEXT("to_zone")), FVector2D(N(Ee, TEXT("x")), N(Ee, TEXT("y"))), B(Ee, TEXT("redcar")) }); }
			for (auto& AV : A(O, TEXT("aid"))) { JObj Aa = AV->AsObject(); Ar.Aid.Add({ S(Aa, TEXT("name")), FVector2D(N(Aa, TEXT("x")), N(Aa, TEXT("y"))) }); }
			for (auto& IV : A(O, TEXT("idle_spots"))) { const auto& P = IV->AsArray(); if (P.Num() >= 2) Ar.IdleSpots.Add(FVector2D(P[0]->AsNumber(), P[1]->AsNumber())); }
			if (JObj Pois = Sub(O, TEXT("pois"))) for (auto& KV : Pois->Values) { const auto& P = KV.Value->AsArray(); if (P.Num() >= 2) Ar.Pois.Add(FString(KV.Key), FVector2D(P[0]->AsNumber(), P[1]->AsNumber())); }
			Areas.Add(MoveTemp(Ar));
		}
	}
	// quests
	if (JObj R = LoadFile(TEXT("quests.json"), Err))
	{
		for (auto& V : A(R, TEXT("quests")))
		{
			JObj O = V->AsObject();
			FHLQuest Q;
			Q.Id = S(O, TEXT("id")); Q.Name = S(O, TEXT("name")); Q.Giver = S(O, TEXT("giver")); Q.Turnin = S(O, TEXT("turnin")); Q.Prereq = S(O, TEXT("prereq"));
			Q.Next = S(O, TEXT("next")); Q.Type = S(O, TEXT("type")); Q.Text = S(O, TEXT("text")); Q.CompleteText = S(O, TEXT("complete_text")); Q.Hub = S(O, TEXT("hub")); Q.Breadcrumb = S(O, TEXT("breadcrumb"));
			Q.Level = N(O, TEXT("level"), 1); Q.MinLevel = N(O, TEXT("min_level"), 1); Q.bGroup = B(O, TEXT("group")); Q.bDungeon = B(O, TEXT("dungeon"));
			JObj Rw = Sub(O, TEXT("rewards"));
			Q.Xp = N(Rw, TEXT("xp")); Q.Cents = N(Rw, TEXT("cents"));
			for (const FString& Id : SA(Rw, TEXT("choice"))) if (const FHLItem* It = Item(Id)) Q.Choice.Add(It);
			if (JObj Rep = Sub(Rw, TEXT("rep"))) for (auto& KV : Rep->Values) Q.Rep.Add(FString(KV.Key), (int32)KV.Value->AsNumber());
			for (auto& OV : A(O, TEXT("objectives")))
			{
				JObj Ob = OV->AsObject();
				FHLObjective Oj;
				Oj.Type = S(Ob, TEXT("type")); Oj.Item = S(Ob, TEXT("item")); Oj.Kind = S(Ob, TEXT("kind")); Oj.Npc = S(Ob, TEXT("npc")); Oj.Label = S(Ob, TEXT("label"));
				Oj.Mobs = SA(Ob, TEXT("mobs")); Oj.From = SA(Ob, TEXT("from")); Oj.Count = N(Ob, TEXT("count"), 1); Oj.Rate = N(Ob, TEXT("rate"), 1);
				Q.Objectives.Add(MoveTemp(Oj));
			}
			Quests.Add(MoveTemp(Q));
		}
		for (const FHLQuest& Q : Quests) QuestById.Add(Q.Id, &Q);
	}
	// personas
	if (JObj R = LoadFile(TEXT("personas.json"), Err))
	{
		const auto& Arr = A(R, TEXT("personas"));
		Personas.Reserve(Arr.Num());
		for (auto& V : Arr)
		{
			JObj O = V->AsObject();
			FHLPersona P;
			P.Index = Personas.Num();
			P.Id = S(O, TEXT("id")); P.Handle = S(O, TEXT("handle")); P.Class = S(O, TEXT("class")); P.HomeZone = S(O, TEXT("home_zone")); P.Temperament = S(O, TEXT("temperament"));
			P.Guild = S(O, TEXT("guild")); P.Goal = S(O, TEXT("goal")); P.Mount = S(O, TEXT("mount")); P.Level = N(O, TEXT("level"), 1);
			JObj Sk = Sub(O, TEXT("skill")); P.ReactionS = N(Sk, TEXT("reaction_s"), 0.6); P.MistakeRate = N(Sk, TEXT("mistake_rate"), 0.08);
			JObj Ty = Sub(O, TEXT("typing")); P.bLower = B(Ty, TEXT("lowercase"), true); P.bPunct = B(Ty, TEXT("punctuation")); P.bCaps = B(Ty, TEXT("caps"));
			P.TypoRate = N(Ty, TEXT("typo_rate"), 0.02); P.Abbrev = SA(Ty, TEXT("abbrev"));
			JObj Sc = Sub(O, TEXT("schedule")); P.StartHour = N(Sc, TEXT("start_hour")); P.Hours = N(Sc, TEXT("hours"), 3);
			P.Friends = SA(O, TEXT("friends"));
			if (JObj Eq = Sub(O, TEXT("equipped"))) for (auto& KV : Eq->Values) if (const FHLItem* It = Item(KV.Value->AsString())) P.Equipped.Add(FString(KV.Key), It);
			P.Cls = Class(P.Class);
			Personas.Add(MoveTemp(P));
		}
	}
	// world
	if (JObj R = LoadFile(TEXT("world.json"), Err))
	{
		JObj Map = Sub(R, TEXT("map"));
		for (auto& V : A(R, TEXT("zones")))
		{
			JObj O = V->AsObject();
			FHLZoneInfo Z;
			Z.Id = S(O, TEXT("id")); Z.Name = S(O, TEXT("name")); Z.Blurb = S(O, TEXT("blurb")); Z.LevelMin = N(O, TEXT("level_min"), 1); Z.LevelMax = N(O, TEXT("level_max"), 60);
			Z.bBuilt = B(O, TEXT("built")); Z.Hubs = SA(O, TEXT("hubs")); Z.Mobs = SA(O, TEXT("mobs")); Z.QuestTitles = SA(O, TEXT("quest_titles")); Z.Neighbours = SA(O, TEXT("neighbours"));
			const auto& Rc = A(Map, *Z.Id);
			if (Rc.Num() >= 4) Z.MapRect = FVector4f(Rc[0]->AsNumber(), Rc[1]->AsNumber(), Rc[2]->AsNumber(), Rc[3]->AsNumber());
			Zones.Add(MoveTemp(Z));
		}
		for (auto& V : A(R, TEXT("dungeons")))
		{
			JObj O = V->AsObject();
			FHLDungeonInfo D;
			D.Id = S(O, TEXT("id")); D.Name = S(O, TEXT("name")); D.Zone = S(O, TEXT("zone")); D.Blurb = S(O, TEXT("blurb")); D.LevelMin = N(O, TEXT("level_min"), 1);
			D.LevelMax = N(O, TEXT("level_max"), 60); D.Size = N(O, TEXT("size"), 5); D.bBuilt = B(O, TEXT("built")); D.Bosses = SA(O, TEXT("bosses"));
			Dungeons.Add(MoveTemp(D));
		}
		for (auto& V : A(R, TEXT("guilds"))) { JObj O = V->AsObject(); Guilds.Add({ S(O, TEXT("id")), S(O, TEXT("name")), S(O, TEXT("kind")), S(O, TEXT("blurb")) }); }
		for (auto& V : A(R, TEXT("redcar"))) { JObj O = V->AsObject(); Redcar.Add({ S(O, TEXT("name")), SA(O, TEXT("stops")) }); }
	}
	// chat
	if (JObj R = LoadFile(TEXT("chat.json"), Err))
	{
		for (auto& V : A(R, TEXT("templates")))
		{
			JObj O = V->AsObject();
			FHLChatTemplate T;
			T.Ch = S(O, TEXT("ch")); T.S = S(O, TEXT("s")); T.Text = S(O, TEXT("text")); T.Min = N(O, TEXT("min"), 1); T.Max = N(O, TEXT("max"), 60); T.bGuild = B(O, TEXT("guild"));
			Templates.Add(MoveTemp(T));
		}
		for (auto& V : A(R, TEXT("corpus"))) { JObj O = V->AsObject(); Corpus.Add({ S(O, TEXT("s")), S(O, TEXT("t")), S(O, TEXT("text")) }); }
	}
	bLoaded = Err.IsEmpty() && Classes.Num() > 0 && Areas.Num() > 0;
	return bLoaded;
}
