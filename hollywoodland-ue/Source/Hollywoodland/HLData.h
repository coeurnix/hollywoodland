// Content registry: loads Content/Data/*.json (written by tools/gen-data.mjs). Plain structs, no UObjects.
// Rows are never reallocated after Load(), so raw pointers into the arrays are stable.
#pragma once
#include "CoreMinimal.h"

enum { HL_STR, HL_AGI, HL_STA, HL_INT, HL_SPI, HL_NUM_STATS };
int32 HLStatIndex(const FString& S);
extern const TCHAR* HLStatNames[HL_NUM_STATS];
FLinearColor HLQualityColor(int32 Q);
FString HLMoney(int32 Cents);

struct FHLEffect
{
	FString Type, Stat;
	float Weapon = 0, Base = 0, Coef = 0, Dur = 0, Period = 0, Pct = 0, Value = 0;
	int32 Ticks = 0;
};
struct FHLAbility
{
	FString Id, Name, Class, Target, School, Desc, Flavor;
	int32 Level = 1, Icon = -1;
	float Cost = 0, Range = 5, Cast = 0, Cd = 0, Radius = 0;
	bool bGcd = true;
	TArray<FHLEffect> Effects;
	bool HasEffect(const TCHAR* Type) const { for (const FHLEffect& E : Effects) if (E.Type == Type) return true; return false; }
	bool IsHeal() const { return School == TEXT("heal"); }
};
struct FHLTalent
{
	FString Id, Name, Class, Desc;
	int32 Spec = 0, Tier = 1, Col = 0, Ranks = 1, Icon = -1;
	TMap<FString, float> Mods; // per rank (tools/gen/talents.mjs)
};
struct FHLClass
{
	FString Id, Name, Resource, Primary, Secondary, Armour, Role;
	float HpPerLevel = 15;
	bool bRanged = false, bCaster = false;
	FLinearColor Color = FLinearColor::White;
	TArray<FString> Weapons, StartItems, Specs;
	TArray<const FHLAbility*> Abilities;
	FString Desc; int32 Icon = -1;
	TArray<FString> SpecDesc; TArray<int32> SpecIcons;
	TArray<const FHLTalent*> Talents;
};
struct FHLItem
{
	FString Id, Name, Kind, Slot, Type, Blurb, UseType, UseStat, Desc;
	int32 Icon = -1; // global slot in Content/Data/Icons (tools/itemgen)
	int32 Q = 1, Req = 0, ILvl = 1, Armor = 0, DMin = 0, DMax = 0, Price = 0, Stack = 1, Hands = 1;
	float Speed = 0, UseAmount = 0, UseValue = 0, UseDur = 0;
	int32 Stats[HL_NUM_STATS] = { 0, 0, 0, 0, 0 };
	bool bNamed = false;
	bool IsEquip() const { return Kind == TEXT("armor") || Kind == TEXT("weapon"); }
};
struct FHLMobDef
{
	FString Id, Name, Family, Rank, Shape, Title, Script;
	int32 LevelMin = 1, LevelMax = 1;
	float Hp = 1, Dmg = 1, Armor = 1, Social = 0, Speed = 6.5f, Swing = 2, Scale = 1, Respawn = 90;
	bool bAggressive = true, bFlees = false, bRanged = false, bMoney = true, bGuard = false;
	FLinearColor Color = FLinearColor::Green;
	TArray<const FHLAbility*> Abilities;
	TArray<const FHLItem*> Junk, BossLoot;
	TArray<TPair<const FHLItem*, float>> Trade;
};
struct FHLBox { float X, Y, W, D, H; FLinearColor Color; FString Label; bool bSolid = true; };
struct FHLNpcDef
{
	FString Id, Name, Title, Trains, Hub;
	FVector2D Pos; float Yaw = 0;
	TArray<FString> Roles;
	TArray<const FHLItem*> Stock;
	FLinearColor Color;
	bool HasRole(const TCHAR* R) const { return Roles.Contains(R); }
};
struct FHLCamp { FString Id; const FHLMobDef* Mob = nullptr; int32 Count = 1; FVector2D Pos; float R = 10, Leash = 40; TArray<FVector2D> Patrol; };
struct FHLObjectDef { FString Id, Kind, Name, Quest, Boss; FVector2D Pos; float Respawn = 60; const FHLItem* Gives = nullptr; bool bUse = false, bSalvage = false, bTall = false, bBig = false; };
struct FHLPortal { FString Id, ToArea, Label; FVector2D Pos, To; float R = 3; };
struct FHLSubzone { FString Name; float X, Y, W, D; FString Levels; bool bHostile = false; };
struct FHLExit { FString Id, Name, ToZone; FVector2D Pos; bool bRedcar = false; };
struct FHLAidStation { FString Name; FVector2D Pos; };
struct FHLAreaDef
{
	FString Id, Name;
	FVector2D Min, Size, Offset, Spawn;
	float Cell = 2;
	bool bDungeon = false;
	FLinearColor Sky, Floor;
	TArray<FHLBox> Boxes, Ground;
	TArray<FHLSubzone> Subzones;
	TArray<FHLNpcDef> Npcs;
	TArray<FHLCamp> Camps;
	TArray<FHLObjectDef> Objects;
	TArray<FHLPortal> Portals;
	TArray<FHLExit> Exits;
	TArray<FHLAidStation> Aid;
	TArray<FVector2D> IdleSpots;
	TMap<FString, FVector2D> Pois;
};
struct FHLObjective { FString Type, Item, Kind, Npc, Label; TArray<FString> Mobs, From; int32 Count = 1; float Rate = 1; };
struct FHLQuest
{
	FString Id, Name, Giver, Turnin, Prereq, Next, Text, CompleteText, Hub, Breadcrumb, Type;
	int32 Level = 1, MinLevel = 1, Xp = 0, Cents = 0;
	bool bGroup = false, bDungeon = false;
	TArray<FHLObjective> Objectives;
	TArray<const FHLItem*> Choice;
	TMap<FString, int32> Rep;
};
struct FHLPersona
{
	FString Id, Handle, Class, HomeZone, Temperament, Guild, Goal, Mount;
	int32 Level = 1, Index = 0;
	float ReactionS = 0.6f, MistakeRate = 0.08f, TypoRate = 0.02f, StartHour = 0, Hours = 3;
	bool bLower = true, bPunct = false, bCaps = false;
	TArray<FString> Abbrev, Friends;
	TMap<FString, const FHLItem*> Equipped;
	const FHLClass* Cls = nullptr;
};
struct FHLZoneInfo { FString Id, Name, Blurb; int32 LevelMin = 1, LevelMax = 60; bool bBuilt = false; TArray<FString> Hubs, Mobs, QuestTitles, Neighbours; FVector4f MapRect; };
struct FHLDungeonInfo { FString Id, Name, Zone, Blurb; int32 LevelMin = 1, LevelMax = 60, Size = 5; bool bBuilt = false; TArray<FString> Bosses; };
struct FHLGuild { FString Id, Name, Kind, Blurb; };
struct FHLRedcarLine { FString Name; TArray<FString> Stops; };
struct FHLChatTemplate { FString Ch, S, Text; int32 Min = 1, Max = 60; bool bGuild = false; };
struct FHLCorpusLine { FString S, T, Text; };

class FHLData
{
public:
	static FHLData& Get();
	bool Load(FString& OutError);
	bool bLoaded = false;

	TArray<FHLAbility> Abilities;
	TArray<FHLClass> Classes;
	TArray<FHLTalent> Talents;
	TArray<FHLItem> Items;
	TArray<FHLMobDef> Mobs;
	TArray<FHLAreaDef> Areas;
	TArray<FHLQuest> Quests;
	TArray<FHLPersona> Personas;
	TArray<FHLZoneInfo> Zones;
	TArray<FHLDungeonInfo> Dungeons;
	TArray<FHLGuild> Guilds;
	TArray<FHLRedcarLine> Redcar;
	TArray<FHLChatTemplate> Templates;
	TArray<FHLCorpusLine> Corpus;

	const FHLAbility* Ability(const FString& Id) const { const FHLAbility* const* P = AbilityById.Find(Id); return P ? *P : nullptr; }
	const FHLTalent* Talent(const FString& Id) const { const FHLTalent* const* P = TalentById.Find(Id); return P ? *P : nullptr; }
	const FHLClass* Class(const FString& Id) const { const FHLClass* const* P = ClassById.Find(Id); return P ? *P : nullptr; }
	const FHLItem* Item(const FString& Id) const { const FHLItem* const* P = ItemById.Find(Id); return P ? *P : nullptr; }
	const FHLItem* ItemByName(const FString& Name) const { const FHLItem* const* P = ItemByNameMap.Find(Name); return P ? *P : nullptr; }
	const FHLMobDef* Mob(const FString& Id) const { const FHLMobDef* const* P = MobById.Find(Id); return P ? *P : nullptr; }
	const FHLQuest* Quest(const FString& Id) const { const FHLQuest* const* P = QuestById.Find(Id); return P ? *P : nullptr; }
	const FHLZoneInfo* Zone(const FString& Id) const { for (const FHLZoneInfo& Z : Zones) if (Z.Id == Id) return &Z; return nullptr; }
	const FHLGuild* Guild(const FString& Id) const { for (const FHLGuild& G : Guilds) if (G.Id == Id) return &G; return nullptr; }
	const FHLNpcDef* Npc(const FString& Id) const;
	int32 AreaIndex(const FString& Id) const { for (int32 i = 0; i < Areas.Num(); ++i) if (Areas[i].Id == Id) return i; return -1; }
	/** Items a mob of this level may drop / the auction house may list: equipment of the given quality whose req band fits. */
	const TArray<const FHLItem*>& EquipPool(int32 Level, int32 Quality) const;

private:
	TMap<FString, const FHLAbility*> AbilityById;
	TMap<FString, const FHLTalent*> TalentById;
	TMap<FString, const FHLClass*> ClassById;
	TMap<FString, const FHLItem*> ItemById, ItemByNameMap;
	TMap<FString, const FHLMobDef*> MobById;
	TMap<FString, const FHLQuest*> QuestById;
	mutable TMap<int32, TArray<const FHLItem*>> Pools;
};
