// Game core. Plain C++ (no UObjects): units, combat, mobs, quests, inventory, simulated players, chat.
// AHLWorldActor owns one FHLGame, ticks it, and mirrors units into gray-box actors. SHLGameUI reads/writes it directly.
// Implementation is split: HLGame.cpp (setup, tick, movement, stats), HLCombat.cpp, HLMobs.cpp, HLPlayer.cpp, HLSim.cpp, HLChat.cpp.
// Units are metres (x east, y south) in area-local coordinates; rendering adds the area's offset.
#pragma once
#include "CoreMinimal.h"
#include "HLData.h"
#include "HLMusic.h"

enum class EHLKind : uint8 { Player, Sim, Mob, Npc };
enum class EHLAura : uint8 { Dot, Hot, Shield, Stun, Slow, Taunt, Buff };
enum class EHLRes : uint8 { None, Power, Energy, Focus, Adrenaline };
enum class EHLMobState : uint8 { Idle, Combat, Evade, Flee };

struct FHLAura
{
	EHLAura Type = EHLAura::Buff;
	FString Name, Stat;
	int32 SourceId = 0;
	float Value = 0, Remaining = 0, Period = 0, TickTimer = 0;
	bool bHarmful = false;
};

/** 2 m occupancy grid with A* (the web build has no nav mesh). */
struct FHLNav
{
	FVector2D Min = FVector2D::ZeroVector;
	float Cell = 2;
	int32 W = 0, H = 0;
	TArray<uint8> Blocked;
	void Init(const FHLAreaDef& Def);
	bool BlockedCell(int32 X, int32 Y) const { return X < 0 || Y < 0 || X >= W || Y >= H || Blocked[Y * W + X] != 0; }
	bool BlockedAt(const FVector2D& P) const { return BlockedCell(FMath::FloorToInt32((P.X - Min.X) / Cell), FMath::FloorToInt32((P.Y - Min.Y) / Cell)); }
	bool LineClear(const FVector2D& A, const FVector2D& B) const;
	bool FindPath(const FVector2D& A, const FVector2D& B, TArray<FVector2D>& Out) const;
	FVector2D NearestFree(const FVector2D& P) const;
	// Exact collision against the solid boxes (the grid above is only for A*): a unit is a circle of radius R; moving
	// into a wall pushes it out along the wall's normal, so it slides along buildings instead of stopping a cell early.
	FVector2D Slide(const FVector2D& From, const FVector2D& Delta, float R = 0.45f) const;
	bool Hits(const FVector2D& P, float R) const;
	bool SegClear(const FVector2D& A, const FVector2D& B, float MinH = 1.0f) const; // line of sight (anything 1 m or taller blocks)
	float Ray3(const FVector& O, const FVector& Dir, float MaxD) const;               // first box hit along a 3D ray (camera)
	struct FRect { float X0, Y0, X1, Y1, H; };
	TArray<FRect> Rects;
	TArray<TArray<int32>> Buckets;
	int32 BW = 0, BH = 0;
	static constexpr float BucketSize = 16.f;
	template <typename F> void ForRects(float X0, float Y0, float X1, float Y1, F&& Fn) const;
private:
	mutable TArray<uint32> RectStamp;
	mutable uint32 RectCur = 0;
	mutable TArray<float> G;
	mutable TArray<int32> Parent;
	mutable TArray<uint32> Stamp;
	mutable uint32 CurStamp = 0;
};

struct FHLLoot { const FHLItem* Item = nullptr; int32 Count = 1; };

struct FHLUnit
{
	int32 Id = 0;
	EHLKind Kind = EHLKind::Mob;
	FString Name, Title, Shape = TEXT("human");
	int32 Level = 1, Area = 0;
	FVector2D Pos = FVector2D::ZeroVector;
	float Z = 0, VelZ = 0, Yaw = 0, Scale = 1, MoveMult = 1;
	FLinearColor Color = FLinearColor::Gray;
	const FHLClass* Cls = nullptr;
	// stats
	int32 Stats[HL_NUM_STATS] = { 0, 0, 0, 0, 0 };
	float MaxHp = 1, Hp = 1, MaxRes = 0, Res = 0;
	EHLRes ResType = EHLRes::None;
	float Armor = 0, AttackPower = 0, WeaponMin = 1, WeaponMax = 2, WeaponSpeed = 2, Crit = 0.05f, Dodge = 0.05f, Parry = 0, Block = 0, BlockValue = 0, AbilityBonus = 0, AutoRange = 4.5f, Speed = 7;
	bool bRangedAuto = false;
	// combat
	int32 TargetId = 0;
	bool bAutoAttack = false, bDead = false, bSitting = false, bMounted = false, bGhost = false; // ghost: released spirit running back
	float SwingTimer = 0, GcdUntil = 0, CastStart = 0, CastEnd = 0, CombatUntil = 0, DeadAt = 0, RegenTimer = 0, HopT = 0;
	const FHLAbility* Casting = nullptr;
	int32 CastTargetId = 0;
	TMap<const FHLAbility*, float> CdUntil;
	TMap<FString, float> Mods; // talent modifiers (summed over ranks)
	float Mod(const FString& K) const { const float* V = Mods.Find(K); return V ? *V : 0.f; }
	TArray<FHLAura> Auras;
	// movement
	TArray<FVector2D> Path;
	int32 PathIdx = 0;
	FVector2D PathGoal = FVector2D::ZeroVector;
	float RepathAt = 0;
	bool bMovedThisTick = false;
	// mob
	const FHLMobDef* Mob = nullptr;
	int32 CampIdx = -1, CampSlot = -1, TappedBy = 0, ScriptPhase = 0, SummonerId = 0;
	FVector2D Home = FVector2D::ZeroVector;
	float Leash = 40, NextThink = 0, WanderAt = 0, FleeUntil = 0, CorpseUntil = 0, ScriptT[3] = { 0, 0, 0 };
	EHLMobState MobState = EHLMobState::Idle;
	TMap<int32, float> Threat;
	bool bEvent = false, bLootable = false;
	TArray<FHLLoot> Loot;
	int32 LootCents = 0;
	// npc / sim
	const FHLNpcDef* Npc = nullptr;
	int32 SimIdx = -1;

	bool IsFriendly() const { return Kind != EHLKind::Mob; }
	bool Stunned() const { for (const FHLAura& A : Auras) if (A.Type == EHLAura::Stun) return true; return false; }
	float HpPct() const { return MaxHp > 0 ? Hp / MaxHp : 0; }
	float BuffSum(const TCHAR* Stat) const { float V = 0; for (const FHLAura& A : Auras) if (A.Type == EHLAura::Buff && A.Stat == Stat) V += A.Value; return V; }
	bool HasAuraNamed(const FString& N) const { for (const FHLAura& A : Auras) if (A.Name == N) return true; return false; }
};

struct FHLCampState { TArray<int32> UnitIds; TArray<float> RespawnAt; };
struct FHLObjectState { const FHLObjectDef* Def = nullptr; bool bActive = true; float RespawnAt = 0; };
struct FHLAreaState
{
	const FHLAreaDef* Def = nullptr;
	FHLNav Nav;
	TArray<FHLCampState> Camps;
	TArray<FHLObjectState> Objects;
};

struct FHLInvSlot { const FHLItem* Item = nullptr; int32 Count = 0; };
struct FHLQuestState { const FHLQuest* Q = nullptr; TArray<int32> Progress; bool bComplete = false; };
struct FHLRelation { int32 Familiarity = 0, Liking = 0; TArray<FString> Facts; FString Bond; }; // Bond: pal rival flirt mentor gossip (seeded per save)
struct FHLMail { FString From, Subject, Body; int32 Cents = 0, Count = 0, Cod = 0; const FHLItem* Item = nullptr; bool bRead = false; };

struct FHLPlayer
{
	bool bCreated = false;
	FString Name, PlayerId;
	const FHLClass* Cls = nullptr;
	int32 Xp = 0, Cents = 0, Salvage = 0, Deaths = 0, Kills = 0, Guns = 0;
	TArray<FHLInvSlot> Bag, Bank;
	TMap<FString, const FHLItem*> Equip; // hat neck coat wrist gloves belt trousers shoes ring main off ranged
	TArray<const FHLAbility*> Known, Bar;
	TArray<FHLQuestState> Quests;
	TSet<FString> Done, Discovered;
	TMap<FString, int32> Rep;
	TMap<FString, FHLRelation> Relations; // persona id -> memory of the player
	double PlayedS = 0;
	bool bShowSim = false;
	TMap<FString, int32> Talents; // talent id -> rank
	int32 Respecs = 0;
	TMap<FString, float> Durability; // equip slot -> 0..1 (missing = 1); a broken item (0) gives nothing
	int32 CorpseArea = 0; FVector2D CorpsePos = FVector2D::ZeroVector;
	double SicknessUntil = 0; // resurrection sickness (-25% attributes)
	TArray<FHLMail> Mail;
	TArray<FString> Friends; // persona ids
};

enum class EHLSimRole : uint8 { Away, Local, Transient, Idler };
enum class EHLSimState : uint8 { Idle, Travel, Grind, Rest, Dead, Party, Leaving, Script, Gather };
struct FHLSim
{
	const FHLPersona* P = nullptr;
	EHLSimRole Role = EHLSimRole::Away;
	EHLSimState State = EHLSimState::Idle;
	bool bOnline = true, bInParty = false, bAfk = false;
	uint8 EventRole = 0; // 1 defending a hive surge, 2 watching picture night
	int32 UnitId = 0, Level = 1, CampIdx = -1, Kills = 0, KillGoal = 10, RouteIdx = 0, ScriptId = 0;
	float Xp = 0, StateUntil = 0, NextAct = 0, HealBiasUntil = 0, NextEmote = 0, AwayUntil = 0;
	FString Zone, Activity;
		TArray<FVector2D> Route;
		FVector2D Dest = FVector2D::ZeroVector;
		// street life (HLSimLife.cpp)
		int32 ChatWith = -1, GatherObj = -1;
		double ChatUntil = 0, GatherEnd = 0, NextGather = 0, DungeonUntil = 0;
		TMap<FString, const FHLItem*> Gear; // equipped items (the persona's, plus dungeon loot)
		bool bChatWalker = false;
	};

struct FHLChatLine
{
	FString Ch, From, Text; // Ch: say yell zone trade lfg party whisper whisper_to system loot combat emote npc
	int32 SimIdx = -1;
	double Time = 0;
};
struct FHLFloatText { int32 UnitId = 0; FString Text; FLinearColor Color; float Age = 0; bool bCrit = false; };
struct FHLAuction { const FHLItem* Item = nullptr; int32 Count = 1, Buyout = 0, SellerIdx = -1, MinBid = 0, Bid = 0; float ExpiresAt = 0; bool bMine = false, bPlayerBid = false; };
struct FHLRoll { const FHLItem* Item = nullptr; float EndsAt = 0; int32 PlayerChoice = -1; bool bActive = false; };
struct FHLCensusEvent { FString Text; double Time = 0; };

struct FHLInput
{
	float Forward = 0, Strafe = 0, Turn = 0;
	bool bJump = false;
	float CamYaw = 0;       // degrees; movement is relative to this when bMouseSteer
	bool bMouseSteer = false;
};

class FHLGame
{
public:
	FHLGame();
	static FHLGame* Get() { return Instance; }
	static FHLGame* Instance;

	// ---- lifecycle (HLGame.cpp)
	bool Init(FString& OutError);
	void Tick(float Dt);
	void EnterWorld();
	double Now = 0;
	float FrameDt = 0;
	bool bInWorld = false;
	FHLMusic Music; // ticked by AHLWorldActor (also on the title screen)
	FRandomStream Rng;
	float Rand() { return Rng.FRand(); }
	int32 RandInt(int32 A, int32 B) { return Rng.RandRange(A, B); }
	bool Chance(float P) { return Rng.FRand() < P; }

	// ---- world
	TArray<FHLAreaState> Areas;
	TMap<int32, TUniquePtr<FHLUnit>> Units;
	TArray<FHLUnit*> UnitList;
	int32 NextUnitId = 1;
	int32 PlayerUnitId = 0;
	FHLUnit* Unit(int32 Id) const { const TUniquePtr<FHLUnit>* P = Units.Find(Id); return P ? P->Get() : nullptr; }
	FHLUnit* PlayerUnit() const { return Unit(PlayerUnitId); }
	FHLUnit* AddUnit(EHLKind Kind, int32 Area, const FVector2D& Pos);
	void RemoveUnit(int32 Id);
	FHLUnit* SpawnMob(const FHLMobDef* Def, int32 Area, const FVector2D& Pos, int32 CampIdx = -1, int32 Slot = -1);
	void SpawnArea(int32 AreaIdx, bool bResetBosses);
	void TeleportUnit(FHLUnit& U, int32 Area, const FVector2D& Pos);
	FString SubzoneAt(int32 Area, const FVector2D& P) const;
	float Dist(const FHLUnit& A, const FHLUnit& B) const { return A.Area == B.Area ? FVector2D::Distance(A.Pos, B.Pos) : 1e9f; }

	// ---- movement (HLGame.cpp)
	FHLInput Input;
	bool MoveTo(FHLUnit& U, const FVector2D& Goal, float StopDist = 0.5f); // returns true when arrived
	void StopMove(FHLUnit& U) { U.Path.Reset(); U.PathIdx = 0; }
	void FaceTo(FHLUnit& U, const FVector2D& P);
	void SeparateSims(float Dt);

	// ---- stats (HLGame.cpp)
	void RecalcPlayerStats();
	void RecalcCharStats(FHLUnit& U, const FHLClass& C, int32 Level, const TMap<FString, const FHLItem*>& Equip, bool bFull, const TMap<FString, int32>* Talents = nullptr);
	// talents (DESIGN-BIBLE §5): a point per level from 10, a tier opens every 5 points spent in that tree
	int32 TalentPoints() const;                       // unspent
	int32 TalentSpent(int32 Spec = -1) const;
	FString CanLearnTalent(const FHLTalent& T) const; // empty when allowed
	void LearnTalent(const FHLTalent& T);
	int32 RespecCost() const { return FMath::Min(5000, 100 << FMath::Min(Player.Respecs, 6)); }
	void ResetTalents();
	float AbCost(const FHLUnit& U, const FHLAbility& Ab) const { return Ab.Cost * FMath::Max(0.f, 1.f - U.Mod(TEXT("cost:") + Ab.Id)); }
	void InitMobStats(FHLUnit& U);
	static int32 XpToNext(int32 L) { return 40 * L * L + 360 * L; }

	// ---- combat (HLCombat.cpp)
	bool IsHostile(const FHLUnit& A, const FHLUnit& B) const { return !A.bGhost && !B.bGhost && (A.IsFriendly() != B.IsFriendly() || InDuel(A, B)); }
	bool InDuel(const FHLUnit& A, const FHLUnit& B) const
	{
		if (DuelState != 2 || !Sims.IsValidIndex(DuelSim)) return false;
		const int32 S = Sims[DuelSim].UnitId;
		return (A.Id == PlayerUnitId && B.Id == S) || (B.Id == PlayerUnitId && A.Id == S);
	}
	bool InCombat(const FHLUnit& U) const { return Now < U.CombatUntil; }
	FString CanCast(const FHLUnit& U, const FHLAbility& Ab, const FHLUnit* Target) const; // empty = ok
	bool TryCast(FHLUnit& U, const FHLAbility& Ab, int32 TargetId);
	void ExecuteAbility(FHLUnit& U, const FHLAbility& Ab, FHLUnit* Target);
	void DealDamage(FHLUnit* Src, FHLUnit& Dst, float Amount, const FString& School, bool bCrit, const FString& What);
	void HealUnit(FHLUnit* Src, FHLUnit& Dst, float Amount, const FString& What);
	void AddThreat(FHLUnit& Mob, int32 SrcId, float Amount);
	void KillUnit(FHLUnit& U, FHLUnit* Killer);
	void TickCombat(FHLUnit& U, float Dt);
	float AbilityPower(const FHLUnit& U, const FHLEffect& E) const;
	void GrantXp(int32 Amount, bool bQuest);
	int32 OwnerOf(const FHLUnit& U) const; // tap owner: the player for party sims
	void AddFloat(int32 UnitId, const FString& Text, const FLinearColor& Color, bool bCrit = false);

	// ---- mobs & objects (HLMobs.cpp)
	void TickMob(FHLUnit& U, float Dt);
	void TickCamps(float Dt);
	void MobEnterCombat(FHLUnit& U, int32 TargetId);
	void MobEvade(FHLUnit& U);
	void GenerateLoot(FHLUnit& Mob);
	void BossScript(FHLUnit& U, float Dt);
	bool ObjectInteract(int32 AreaIdx, int32 ObjIdx); // by the player
	float DungeonLeftAt = -1e9;
	bool bDungeonCleared = false;

	// ---- player (HLPlayer.cpp)
	FHLPlayer Player;
	void NewCharacter(const FString& Name, const FHLClass* Cls);
	FString SaveJson() const;
	bool LoadJson(const FString& Json);
	void SaveNow();
	float NextSaveAt = 30;
	bool AddItem(const FHLItem* Item, int32 Count = 1, bool bAnnounce = true);
	int32 CountItem(const FHLItem* Item) const;
	const FHLItem* BestAmmo() const;
	void GiveStarterAmmo(); // best usable ammunition in the bags for the equipped ranged weapon
	void RemoveItem(const FHLItem* Item, int32 Count);
	bool CanEquip(const FHLItem& It, FString* Why = nullptr) const;
	void EquipBag(int32 BagIdx);
	void Unequip(const FString& Slot);
	void UseBag(int32 BagIdx);
	void SellBag(int32 BagIdx);
	bool BuyItem(const FHLItem* Item);
	void LearnAbility(const FHLAbility* Ab, bool bFree);
	void UseBar(int32 Slot);
	void TargetUnit(int32 Id);
	void TabTarget();
	void TargetNearestFriend();
	void InteractUnit(int32 Id);
	void LootCorpse(FHLUnit& Corpse);
	void ReleaseSpirit();
	void ResurrectAtCorpse();
	void ResurrectAtStation();
	int32 RepairCost() const;
	void RepairAll();
	float SlotDurability(const FString& Slot) const { const float* D = Player.Durability.Find(Slot); return D ? *D : 1.f; }
	void TickPlayer(float Dt);
	// quests
	const FHLQuestState* QuestState(const FString& Id) const;
	bool QuestAvailable(const FHLQuest& Q) const;
	bool QuestReadyAt(const FHLQuest& Q, const FString& NpcId) const;
	void AcceptQuest(const FHLQuest& Q);
	void AbandonQuest(const FString& Id);
	void CompleteQuest(const FHLQuest& Q, int32 Choice);
	void QuestEvent(const TCHAR* Type, const FString& Key, int32 Count = 1, const FHLUnit* Mob = nullptr);
	int32 NpcMarker(const FHLNpcDef& N) const; // 0 none, 1 available (!), 2 ready (?), 3 in progress (grey ?)
	// UI-facing state
	int32 OpenNpcUnitId = 0;       // NPC dialog open
	FString OpenNpcMode;           // "", quest:<id>, vendor, trainer, bank, auction, redcar, inn, prof
	FString ErrorText; float ErrorUntil = 0;
	FString BannerText, BannerSub; float BannerUntil = 0; bool bBannerHostile = false;
	void Error(const FString& T) { ErrorText = T; ErrorUntil = Now + 2.5; }
	void Banner(const FString& T, const FString& Sub = FString(), float Dur = 4) { BannerText = T; BannerSub = Sub; BannerUntil = Now + Dur; bBannerHostile = false; }
	TArray<FHLFloatText> Floats;
	FString CurSubzone;
	// casting an object interaction
	int32 InteractArea = -1, InteractObj = -1; float InteractEnd = 0, InteractStart = 0; FString InteractLabel;
	// auction house
	TArray<FHLAuction> Auctions;
	float NextAuctionChurn = 0;
	void TickAuctions();
	bool BuyAuction(int32 Idx);
	bool ListAuction(int32 BagIdx, int32 Buyout, float Hours = 1); // hours of play time
	int32 MarketPrice(const FHLItem& It) const;
	// loot roll
	FHLRoll Roll;
	void StartRoll(const FHLItem* Item);
	void FinishRoll();

	// ---- simulated players (HLSim.cpp)
	TArray<FHLSim> Sims;
	TArray<int32> Party; // sim indices in the player's party
	TArray<FHLCensusEvent> CensusEvents;
	int32 InviteFromSim = -1; float InviteUntil = 0; // pending invite to the player
	int32 CensusCursor = 0; float NextCensus = 0, NextPopCheck = 0, NextDirector = 0;
	int32 DirectorStep = 0; int32 BuddySim = -1;
	void InitSims();
	void TickSims(float Dt);
	void TickSimBrain(FHLSim& S, FHLUnit& U, float Dt);
	void SimCombat(FHLSim& S, FHLUnit& U);
	FHLUnit* SpawnSim(int32 SimIdx, EHLSimRole Role, const FVector2D& Pos);
	void DespawnSim(int32 SimIdx);
	void TickCensus();
	void TickDirector(float Dt);
	void InviteSim(int32 SimIdx);
	void SimJoinParty(int32 SimIdx);
	void SimLeaveParty(int32 SimIdx, bool bSay);
	void AnswerInvite(bool bAccept);
	void LeaveParty();
	void PartyEvent(const FString& What);
	int32 FindSimByHandle(const FString& Handle) const;
	FString SimActivity(const FHLSim& S) const;
	int32 OnlineCount() const;
	FHLRelation& Relation(const FHLSim& S) { return Player.Relations.FindOrAdd(S.P->Id); }
	int32 PickCampFor(int32 Level);
	const FHLAbility* SimPickAbility(FHLSim& S, FHLUnit& U, FHLUnit*& OutTarget);

	// ---- chat & worker (HLChat.cpp)
	TArray<FHLChatLine> Chat;
	struct FPendingChat { double At; FHLChatLine Line; };
	TArray<FPendingChat> PendingChat;
	float NextAmbient = 3, NextGossip = 150;
	FString Session; bool bWorkerOk = false; float NextSessionTry = 0; int32 WorkerInFlight = 0, NextReqId = 1;
	int32 WorkerCalls = 0, WorkerFails = 0;
	FString ApiBase;
	FString LastWhisperFrom;
	const FHLItem* LastLinked = nullptr;
	void AddChat(const FString& Ch, const FString& From, const FString& Text, int32 SimIdx = -1, float Delay = 0);
	void System(const FString& Text) { AddChat(TEXT("system"), FString(), Text); UE_LOG(LogTemp, Verbose, TEXT("HL_SYS %s"), *Text); }
	void SimSay(int32 SimIdx, const FString& Ch, const FString& Situation, float Delay = 0, const FString& Fallback = FString());
	FString CorpusLine(const FHLSim& S, const FString& Situation);
	FString StyleText(const FHLPersona& P, const FString& Text);
	FString FillTemplate(const FHLSim& S, const FString& Text);
	void TickChat(float Dt);
	void PlayerChat(const FString& Text); // raw input incl. slash commands
	FString ChatChannel = TEXT("say"); FString WhisperTarget;
	void WorkerSession();
	void WorkerSim(const FString& Kind, const TArray<int32>& Speakers, const FString& TrigKind, const FString& Channel, const FString& Text,
		const TArray<FString>& Events, TFunction<void(bool bOk, const TSharedPtr<class FJsonObject>& Resp)> Done);
	void ApplyWorkerActions(const TSharedPtr<class FJsonObject>& Resp, const FString& DefaultChannel, const TArray<int32>& Speakers);
	FString WhoQuery; bool bWhoOpen = false;
	FString ChatCtx; // fills {ctx} in corpus lines (event names)

	// ---- street life (HLSimLife.cpp)
	struct FHLHappening { int32 Kind = 0; FVector2D Pos = FVector2D::ZeroVector; TArray<int32> Sims; double Until = 0, NextBeat = 0, DuelStart = 0, NextServe = 0; int32 Duel[2] = { -1, -1 }; bool bDone = false; };
	FHLHappening Happen;
	int32 LastHappening = 0;
	TArray<const FHLConvo*> RecentConvos;
	double ConvoEndsAt = 0, SayEndsAt = 0, NextZoneConvo = 40, NextSayConvo = 60, NextNpcFacing = 0, NextHappening = 0;
	bool StartConvo(const FHLConvo& C, const TArray<int32>& Speakers, float Lead);
	void TickConvos();
	bool SimChatBrain(FHLSim& S, FHLUnit& U);
	FVector2D ErrandSpot(const FString& Poi, const FHLSim& S, const FVector2D& PoiPos);
	void SimArrived(FHLSim& S, FHLUnit& U);
	void SimPostAuction(int32 SimIdx);
	bool Undercut(const FHLAuction& Mine) const;
	bool TryStartGather(FHLSim& S, FHLUnit& U);
	void SimGatherBrain(FHLSim& S, FHLUnit& U);
	void TickNpcFacing();
	void TickHappenings();
	void EndHappening();
	FVector2D HappeningSpot(int32 K, int32 N) const;
	struct FHLSimRun { int32 Stage = 0; TArray<int32> Members; double NextAt = 0, At = 0, GoAt = 0; bool bWin = false; };
	FHLSimRun SimRun;
	const FHLItem* SimDungeonLoot(const FHLSim& S) const;
	void TickSimDungeon();
	struct FHLFight { int32 Stage = 0, Bout = 0, Champ = -1; TArray<int32> Fighters, Watchers, Winners; double NextAt = 0, BeatAt = 0, BoutStart = 0, NextBout = 0, StageAt = 0, Until = 0; bool bAnnounced = false, bBoutDone = false; };
	FHLFight Fight;
	bool FightPair(int32& A, int32& B) const;
	void PlaceFightCrowd(const FVector2D& Ring);
	void EndFightNight(const TCHAR* Why);
	void TickFightNight();
	void FightNightDuelOver(bool bPlayerWon);
	// ---- gunsmithing and escorts (HLCraft.cpp)
	struct FHLRecipe { FString Product, Mat1, Mat2; int32 Yield = 1, Skill = 1, N1 = 0, N2 = 0; };
	static const TArray<FHLRecipe>& GunRecipes();
	int32 CraftIdx = -1, CraftLeft = 0;
	double CraftStart = 0, CraftEnd = 0;
	int32 RecipeColour(const FHLRecipe& R) const; // -1 too high, 0 grey, 1 green, 2 yellow, 3 orange
	int32 CraftableCount(const FHLRecipe& R) const;
	void StartCraft(int32 Idx, int32 Times);
	void TickCraft();
	int32 EscortUnitId = 0, EscortAmbush = 0;
	const FHLQuest* EscortQuest = nullptr;
	FVector2D EscortStart = FVector2D::ZeroVector;
	double EscortResetAt = 0;
	bool bEscorting = false;
	bool CanStartEscort(const FHLQuest& Q, bool bReport);
	void StartEscort(const FHLQuest& Q);
	void EndEscort(bool bFailed, const FString& Why);
	void EscortDied(FHLUnit& W);
	void TickEscort(float Dt);
	// ---- world events (HLEvents.cpp)
	struct FHLHub { FString Id, Name; FVector2D Pos; };
	TArray<FHLHub> Hubs;
	int32 SurgeHub = -1, SurgeWarnHub = -1, SurgeKills = 0;
	double SurgeStartAt = 0, SurgeEnd = 0, SurgeNextWave = 0, NextSurgeAt = 0;
	TSet<int32> SurgeMobs;
	TMap<FString, double> HubClosedUntil;
	double FilmStart = 0, FilmEnd = 0, NextFilmAt = 0, NextFilmChat = 0; float FilmWatchS = 0;
	bool bFilmAnnounced = false, bFilmBuffGiven = false, bEventsSoon = false;
	FString FilmTitle, FilmShort;
	void InitEvents();
	void TickEvents(float Dt);
	bool SimEventBrain(FHLSim& S, FHLUnit& U);
	void OnEventMobKilled(const FHLUnit& Mob, const FHLUnit* Killer);
	void AnnounceRare(const FHLUnit& Mob);
	bool HubClosed(const FString& HubId) const;
	int32 RandomLocalSim(const FVector2D& Near, float MaxD);
	void EventSay(const FString& Situation, const FString& Ctx, const FString& Ch, const FVector2D& Near, float MaxD, float Delay);

	// ---- social (HLSocial.cpp)
	void SendMail(const FString& From, const FString& Subject, const FString& Body, int32 Cents, const FHLItem* Item = nullptr, int32 Count = 1, int32 Cod = 0);
	int32 UnreadMail() const;
	void TakeMail(int32 Idx);
	void DeleteMail(int32 Idx);
	struct FPendingCod { double At; FString From; const FHLItem* Item; int32 Count, Price; };
	TArray<FPendingCod> PendingMail;
	void TickPendingMail();
	bool IsFriend(const FHLSim& S) const;
	void AddFriend(int32 SimIdx, bool bQuiet = false);
	void RemoveFriend(int32 SimIdx);
	void SeedBonds();
	int32 BondedAwaySim(int32 MinLevel, int32 MaxLevel);
	void TickSocial(float Dt);
	double NextSocial = 0;
	TMap<FString, double> BondNext;
	// duels (outside hubs; nobody dies: the loser stops at 1 health)
	int32 DuelSim = -1, DuelState = 0, DuelFromSim = -1; // state 1 countdown, 2 fighting
	double DuelAt = 0, DuelInviteUntil = 0; FVector2D DuelFlag = FVector2D::ZeroVector;
	bool CanDuelHere() const;
	void ChallengeDuel(int32 SimIdx);
	void SimChallengesDuel(int32 SimIdx);
	void AnswerDuel(bool bAccept);
	void StartDuel(int32 SimIdx);
	void EndDuel(int32 WinnerUnitId, const TCHAR* How);
	void TickDuel();
	// trades: face to face (trade window) or by C.O.D. mail after whispering someone selling in trade chat
	struct FHLTradeOffer { const FHLItem* Item = nullptr; int32 Count = 1, Price = 0; double At = 0; };
	TMap<int32, FHLTradeOffer> SimWts;
	int32 LastPrice = 0;
	void NoteWts(int32 SimIdx, const FHLItem* Item, int32 Price);
	bool HandleWtbWhisper(int32 SimIdx, const FString& Text);
	int32 TradeSim = -1, TradeCents = 0; TArray<int32> TradeBag;
	void OpenTrade(int32 SimIdx);
	void AcceptTrade();
	// auction bids
	bool bAuctionSeeded = false;
	int32 NextBid(const FHLAuction& A) const;
	bool BidAuction(int32 Idx);
};
