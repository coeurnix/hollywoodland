// The mind of a simulated player (AI-IMPROVEMENTS-PLAN.md 4.2, 4.4, 4.6, 4.7):
//   goals      what they are doing and why; the next one is chosen by local utility over options the game computes, and
//              for players near the human the Worker's careful model judges between the same options (kind "plan")
//   quests     each local has a quest of their own, so "need 2 more plates" is true
//   notice     things the player does in view become events; people react with a look, a mirrored emote, a remark
//              (Worker kind "observe"), or pointedly not at all
//   memory     dated episodes about the player that fade, a gist the Worker rewrites (kind "consolidate"), greetings
//   party      nobody reads minds: members act on what they saw and what was said, mishear, lose patience
// Everything here keeps working with the Worker unreachable.
#include "HLGame.h"
#include "HLBridge.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

int64 FHLGame::RealNow()
{
	// -HLMemDays=N moves the calendar forward (memory tests)
	static const int64 Shift = [] { int32 D = 0; FParse::Value(FCommandLine::Get(), TEXT("HLMemDays="), D); return (int64)D * 86400; }();
	return FDateTime::UtcNow().ToUnixTimestamp() + Shift;
}
static int32 Today() { return (int32)(FHLGame::RealNow() / 86400); }
static TArray<TSharedPtr<FJsonValue>> Strs(const TArray<FString>& In) { TArray<TSharedPtr<FJsonValue>> Arr; for (const FString& S : In) Arr.Add(MakeShared<FJsonValueString>(S)); return Arr; }

// ------------------------------------------------------------------------------------------------------------- quests
void FHLGame::SimQuestPick(FHLSim& S)
{
	const FHLData& D = FHLData::Get();
	TArray<const FHLQuest*> Fit;
	for (const FHLQuest& Q : D.Quests)
	{
		if (Q.bDungeon || Q.bGroup || Q.MinLevel > S.Level || Q.Level < S.Level - 3 || Q.Level > S.Level + 1 || Q.Id == S.QuestId) continue;
		if (!Q.Cls.IsEmpty() && Q.Cls != S.P->Class) continue;
		if (Q.Objectives.Num() && Q.Objectives[0].Type == TEXT("kill") && Q.Objectives[0].Mobs.Num()) Fit.Add(&Q);
	}
	if (!Fit.Num()) { S.QuestId.Reset(); S.QuestNeed = S.QuestHave = 0; return; }
	const FHLQuest& Q = *Fit[RandInt(0, Fit.Num() - 1)];
	S.QuestId = Q.Id; S.QuestNeed = Q.Objectives[0].Count; S.QuestHave = RandInt(0, FMath::Max(0, S.QuestNeed - 2));
}

void FHLGame::SimQuestKill(FHLSim& S, const FHLUnit& Mob)
{
	if (S.QuestId.IsEmpty() || S.QuestHave >= S.QuestNeed || !Mob.Mob) return;
	const FHLQuest* Q = FHLData::Get().Quest(S.QuestId);
	if (!Q || !Q->Objectives.Num() || !Q->Objectives[0].Mobs.Contains(Mob.Mob->Id)) return;
	if (++S.QuestHave >= S.QuestNeed) UE_LOG(LogTemp, Display, TEXT("HL_MIND %s finished the kills for %s"), *S.P->Handle, *Q->Name);
}

FString FHLGame::SimDoing(const FHLSim& S) const
{
	FString Out;
	if (const FHLQuest* Q = S.QuestId.IsEmpty() ? nullptr : FHLData::Get().Quest(S.QuestId))
		Out = S.QuestHave >= S.QuestNeed ? FString::Printf(TEXT("has finished the quest \"%s\" and needs to hand it in"), *Q->Name) : FString::Printf(TEXT("on the quest \"%s\" (%d/%d %s)"), *Q->Name, S.QuestHave, S.QuestNeed, *Q->Objectives[0].Label);
	if (!S.GoalKind.IsEmpty() && !S.bInParty) Out += FString::Printf(TEXT("%s%s%s"), Out.IsEmpty() ? TEXT("") : TEXT("; "), *S.GoalKind, S.GoalWhy.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *S.GoalWhy));
	return Out;
}

// ------------------------------------------------------------------------------------------------------------- goals
void FHLGame::MindSetGoal(FHLSim& S, FHLUnit& U, const FString& Kind, const FString& Arg, const FString& Why)
{
	const FHLAreaDef& Def = *Areas[0].Def;
	S.Route.Reset(); S.RouteIdx = 0; S.CampIdx = -1; S.HelpUnit = 0; U.bSitting = false;
	S.State = EHLSimState::Idle; S.StateUntil = Now;
	if (Kind == TEXT("grind") || Kind == TEXT("quest"))
	{
		S.CampIdx = FMath::Clamp(FCString::Atoi(*Arg), 0, Def.Camps.Num() - 1); S.Kills = 0;
		S.KillGoal = Kind == TEXT("quest") ? FMath::Clamp(S.QuestNeed - S.QuestHave, 2, 14) : RandInt(5, 14);
		S.Dest = Def.Camps[S.CampIdx].Pos + FVector2D(Rand() * 10 - 5, Rand() * 10 - 5); S.State = EHLSimState::Travel;
	}
	else if (Kind == TEXT("errand"))
	{
		if (const FVector2D* Poi = Def.Pois.Find(Arg)) S.Route.Add(ErrandSpot(Arg, S, *Poi));
		if (Arg == TEXT("trainers")) S.TrainedLevel = S.Level;
		if (Arg == TEXT("vendor") || Arg == TEXT("auction")) S.LastTown = Now;
	}
	else if (Kind == TEXT("handin"))
	{
		const FHLQuest* Q = FHLData::Get().Quest(S.QuestId);
		const FHLNpcDef* N = nullptr;
		if (Q) for (const FHLNpcDef& Nd : Def.Npcs) if (Nd.Id == Q->Turnin) N = &Nd;
		if (N) { const float R = FMath::DegreesToRadians(N->Yaw); S.Route.Add(Areas[0].Nav.NearestFree(N->Pos + FVector2D(FMath::Cos(R), FMath::Sin(R)) * (2.2f + Rand()) + FVector2D(Rand() * 2 - 1, Rand() * 2 - 1))); }
		S.Xp += Q ? Q->Xp * 0.6f : 0; SimQuestPick(S); // (they pick up the next one while they are there)
	}
	else if (Kind == TEXT("explore") || Kind == TEXT("social"))
	{
		FVector2D Where = Def.IdleSpots[RandInt(0, Def.IdleSpots.Num() - 1)];
		if (Kind == TEXT("explore")) { if (const FVector2D* Poi = Def.Pois.Find(Arg)) Where = *Poi; else if (Def.Portals.Num()) Where = Def.Portals[0].Pos + FVector2D(0, -8); }
		S.Route.Add(Areas[0].Nav.NearestFree(Where + FVector2D(Rand() * 10 - 5, Rand() * 8 - 4)));
	}
	else if (Kind == TEXT("afk")) { S.bAfk = true; S.AwayUntil = Now + 40 + Rand() * 110; S.StateUntil = S.AwayUntil; StopMove(U); if (Chance(0.4f)) U.bSitting = true; }
	else if (Kind == TEXT("help")) { S.HelpUnit = FCString::Atoi(*Arg); S.HelpUntil = Now + 45; }
	else if (Kind == TEXT("leave")) { S.State = EHLSimState::Leaving; S.Dest = Def.Exits[RandInt(0, Def.Exits.Num() - 1)].Pos; }
	S.GoalKind = Kind == TEXT("errand") ? FString::Printf(TEXT("heading to the %s"), *Arg) : Kind == TEXT("quest") ? TEXT("killing things for a quest") : Kind == TEXT("grind") ? TEXT("grinding") :
		Kind == TEXT("handin") ? TEXT("handing in a quest") : Kind == TEXT("explore") ? TEXT("looking around") : Kind == TEXT("social") ? TEXT("hanging out") : Kind == TEXT("afk") ? TEXT("away from the keyboard") :
		Kind == TEXT("help") ? TEXT("helping the player in a fight") : TEXT("leaving");
	S.GoalWhy = Why; S.GoalSince = Now;
	S.GoalHold = Now + 20 + S.P->Tr.Pati * 70; // priorities rarely change in an instant
	UE_LOG(LogTemp, Verbose, TEXT("HL_MIND %s -> %s %s (%s)"), *S.P->Handle, *Kind, *Arg, *Why);
}

void FHLGame::MindNextGoal(FHLSim& S, FHLUnit& U)
{
	const FHLAreaDef& Def = *Areas[0].Def;
	const FHLData& D = FHLData::Get();
	const int32 SimIdx = int32(&S - Sims.GetData());
	const FHLTraits& T = S.P->Tr;
	const FString& Arch = S.P->Archetype;
	FHLUnit* P = PlayerUnit();
	if (S.QuestId.IsEmpty()) SimQuestPick(S);
	struct FOpt { FString Kind, Arg, Label, Gain, Risk; float Cost = 60, Util = 0; };
	TArray<FOpt> Opts;
	auto Add = [&](const TCHAR* Kind, const FString& Arg, const FString& Label, float Util, float Cost, const FString& Gain = FString(), const FString& Risk = FString())
	{ FOpt O; O.Kind = Kind; O.Arg = Arg; O.Label = Label; O.Util = Util + (Rand() - 0.5f) * 0.24f; O.Cost = Cost; O.Gain = Gain; O.Risk = Risk; Opts.Add(MoveTemp(O)); };
	auto WalkS = [&](const FVector2D& To) { return FVector2D::Distance(U.Pos, To) / 6.f; };
	auto Crowd = [&](int32 Camp) { int32 N = 0; for (const FHLSim& O : Sims) if (O.UnitId && O.CampIdx == Camp && &O != &S) ++N; return N; };

	const FHLQuest* Q = S.QuestId.IsEmpty() ? nullptr : D.Quest(S.QuestId);
	if (Q && S.QuestHave >= S.QuestNeed) Add(TEXT("handin"), FString(), FString::Printf(TEXT("hand in the quest \"%s\""), *Q->Name), 0.75f + T.Know * 0.1f, 90, FString::Printf(TEXT("%d xp and the next quest"), Q->Xp));
	else if (Q)
	{
		int32 Camp = -1; float BestD = 1e9f;
		for (int32 c = 0; c < Def.Camps.Num(); ++c) if (Q->Objectives[0].Mobs.Contains(Def.Camps[c].Mob->Id)) { const float Dd = FVector2D::Distance(Def.Camps[c].Pos, U.Pos) + Crowd(c) * 25.f; if (Dd < BestD) { BestD = Dd; Camp = c; } }
		if (Camp >= 0) Add(TEXT("quest"), FString::FromInt(Camp), FString::Printf(TEXT("kill %s for the quest \"%s\" (%d/%d done)"), *Def.Camps[Camp].Mob->Name, *Q->Name, S.QuestHave, S.QuestNeed),
			0.55f + T.Know * 0.15f + (Arch == TEXT("speedrunner") ? 0.2f : 0.f), WalkS(Def.Camps[Camp].Pos) + (S.QuestNeed - S.QuestHave) * 22.f, FString::Printf(TEXT("quest xp %d"), Q->Xp),
			Crowd(Camp) ? FString::Printf(TEXT("%d other players already at that camp"), Crowd(Camp)) : FString());
	}
	{
		const int32 Camp = PickCampFor(S.Level);
		Add(TEXT("grind"), FString::FromInt(Camp), FString::Printf(TEXT("grind %ss"), *Def.Camps[Camp].Mob->Name), 0.3f + T.Aggr * 0.3f - T.Cur * 0.15f, WalkS(Def.Camps[Camp].Pos) + 200, TEXT("steady xp"), U.HpPct() < 0.6f ? TEXT("health is low") : FString());
	}
	const float Bags = FMath::Clamp(float(Now - S.LastTown) / 700.f, 0.f, 1.f);
	if (const FVector2D* V = Def.Pois.Find(TEXT("vendor"))) Add(TEXT("errand"), TEXT("vendor"), TEXT("go to town and sell"), Bags * (0.55f + T.Caut * 0.25f), WalkS(*V) + 20, TEXT("bags emptied, some money"));
	if (S.Level / 2 > S.TrainedLevel / 2) if (const FVector2D* V = Def.Pois.Find(TEXT("trainers"))) Add(TEXT("errand"), TEXT("trainers"), TEXT("visit the class trainer"), 0.5f + T.Know * 0.35f - (Arch == TEXT("noob") ? 0.35f : 0.f), WalkS(*V) + 15, TEXT("new abilities for this level"));
	if (const FVector2D* V = Def.Pois.Find(TEXT("auction"))) Add(TEXT("errand"), TEXT("auction"), TEXT("check the auction house"), (Arch == TEXT("trader") ? 0.75f : 0.12f + Bags * 0.15f), WalkS(*V) + 40, TEXT("sell greens, look for upgrades"));
	if (const FVector2D* V = Def.Pois.Find(TEXT("inn"))) Add(TEXT("errand"), TEXT("inn"), TEXT("sit down at the Roosevelt for a bit"), 0.08f + (1.f - U.HpPct()) * 0.3f + T.Soc * 0.1f, WalkS(*V) + 30, TEXT("a rest, rested xp"));
	{
		static const TCHAR* Sights[] = { TEXT("forecourt"), TEXT("vine"), TEXT("police"), TEXT("bank"), TEXT("egyptian") };
		const FString Sight = Sights[RandInt(0, 4)];
		Add(TEXT("explore"), Sight, FString::Printf(TEXT("go and look at %s"), Sight == TEXT("egyptian") ? TEXT("the Egyptian Theatre") : *PoiName(Sight)), T.Cur * 0.5f + (Arch == TEXT("tourist") || Arch == TEXT("lore") || Arch == TEXT("jamdev") ? 0.3f : 0.f) - T.Aggr * 0.1f, 120, TEXT("seeing more of the game"));
	}
	Add(TEXT("social"), FString(), TEXT("hang out where the other players are standing"), T.Soc * 0.38f + (Arch == TEXT("flirt") ? 0.25f : 0.f), 90, TEXT("company"));
	if (Chance(0.5f)) Add(TEXT("afk"), FString(), TEXT("go AFK for a minute"), (1.f - T.Attn) * 0.3f + (Arch == TEXT("distracted") ? 0.3f : 0.f), 80);
	if (P && P->Area == U.Area && !P->bDead && InCombat(*P) && Dist(U, *P) < 70 && FMath::Abs(S.Level - P->Level) <= 6)
	{
		FHLUnit* Att = nullptr; int32 N = 0;
		for (FHLUnit* O : UnitList) if (O->Kind == EHLKind::Mob && !O->bDead && O->TargetId == P->Id && O->MobState == EHLMobState::Combat) { Att = O; ++N; }
		if (Att && (P->HpPct() < 0.6f || N >= 2))
			Add(TEXT("help"), FString::FromInt(Att->Id), FString::Printf(TEXT("help the player %s, who is fighting %d enem%s at %d%% health nearby"), *Player.Name, N, N == 1 ? TEXT("y") : TEXT("ies"), FMath::RoundToInt32(P->HpPct() * 100)),
				T.Help * 0.75f + Relation(S).Liking * 0.04f + (Arch == TEXT("leeroy") ? 0.2f : 0.f), 50, TEXT("they may group up after"), N >= 3 ? TEXT("a lot of enemies") : FString());
	}
	if (Now > 1500 && Chance(0.25f)) Add(TEXT("leave"), FString(), TEXT("log off for now"), 0.1f + (S.P->Jam == TEXT("in_progress") ? 0.12f : 0.f), 30, TEXT("back to real life"));
	Opts.Sort([](const FOpt& A, const FOpt& B) { return A.Util > B.Util; });
	MindSetGoal(S, U, Opts[0].Kind, Opts[0].Arg, FString());

	// Near the player, when the choice is close, the careful model judges between the same options in character. The
	// local choice stands meanwhile (a person hesitating a moment), and is replaced only if the answer comes in time.
	static double NextPlanAt = 0;
	const bool bNear = P && P->Area == U.Area && Dist(U, *P) < 55;
	if (!bNear || !bWorkerOk || S.bPlanPending || Now < NextPlanAt || Now < S.NextPlan || Opts.Num() < 3 || Opts[0].Util - Opts[1].Util > 0.22f) return;
	NextPlanAt = Now + 14; S.NextPlan = Now + 150; S.bPlanPending = true; ++PlansAsked;
	TSharedRef<FJsonObject> Req = MakeShared<FJsonObject>();
	Req->SetArrayField(TEXT("speakers"), { MakeShared<FJsonValueObject>(SpeakerJson(SimIdx)) });
	TSharedRef<FJsonObject> Rep = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> Me = MakeShared<FJsonObject>();
	Me->SetNumberField(TEXT("level"), S.Level); Me->SetStringField(TEXT("class"), S.P->Class); Me->SetStringField(TEXT("health"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(U.HpPct() * 100)));
	Me->SetStringField(TEXT("bags"), FString::Printf(TEXT("%d%% full"), FMath::RoundToInt32(Bags * 100))); Me->SetNumberField(TEXT("session_min"), FMath::RoundToInt32(Now / 60));
	if (!S.Mood.IsEmpty()) Me->SetStringField(TEXT("mood"), S.Mood);
	Rep->SetObjectField(TEXT("me"), Me);
	Rep->SetStringField(TEXT("just_finished"), S.GoalKind);
	TArray<TSharedPtr<FJsonValue>> Os;
	const int32 NOpt = FMath::Min(6, Opts.Num());
	for (int32 i = 0; i < NOpt; ++i)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), FString::Printf(TEXT("o%d"), i + 1)); O->SetStringField(TEXT("goal"), Opts[i].Label); O->SetNumberField(TEXT("cost_s"), FMath::RoundToInt32(Opts[i].Cost / 10) * 10);
		if (!Opts[i].Gain.IsEmpty()) O->SetStringField(TEXT("gain"), Opts[i].Gain);
		if (!Opts[i].Risk.IsEmpty()) O->SetStringField(TEXT("risk"), Opts[i].Risk);
		Os.Add(MakeShared<FJsonValueObject>(O));
	}
	Rep->SetArrayField(TEXT("options"), Os);
	TArray<FString> Near;
	if (P) Near.Add(FString::Printf(TEXT("the player %s (level %d %s) %.0f m away, %s"), *Player.Name, P->Level, Player.Cls ? *Player.Cls->Name : TEXT(""), Dist(U, *P), InCombat(*P) ? TEXT("fighting") : P->bSitting ? TEXT("sitting") : TEXT("idle")));
	for (const FHLSim& O : Sims) if (O.UnitId && &O != &S && Near.Num() < 4) if (const FHLUnit* OU = Unit(O.UnitId)) if (Dist(U, *OU) < 30) Near.Add(FString::Printf(TEXT("%s (level %d), %s"), *O.P->Handle, O.Level, InCombat(*OU) ? TEXT("fighting") : TEXT("nearby")));
	Rep->SetArrayField(TEXT("nearby"), Strs(Near));
	Req->SetObjectField(TEXT("report"), Rep);
	struct FKept { FString Kind, Arg; };
	TArray<FKept> Kept; for (int32 i = 0; i < NOpt; ++i) Kept.Add({ Opts[i].Kind, Opts[i].Arg });
	const double Stamp = S.GoalSince;
	WorkerPost(TEXT("plan"), Req, [this, SimIdx, Kept, Stamp](bool bOk, const TSharedPtr<FJsonObject>& Resp)
	{
		FHLSim& S2 = Sims[SimIdx];
		S2.bPlanPending = false;
		FHLUnit* U2 = Unit(S2.UnitId);
		FString Choice;
		if (!bOk || !Resp.IsValid() || !U2 || U2->bDead || !Resp->TryGetStringField(TEXT("choice"), Choice)) return;
		const int32 Idx = FCString::Atoi(*Choice.Mid(1)) - 1;
		FString Why, Say, Mood;
		Resp->TryGetStringField(TEXT("why"), Why); Resp->TryGetStringField(TEXT("say"), Say); Resp->TryGetStringField(TEXT("mood"), Mood);
		if (!Mood.IsEmpty()) S2.Mood = Mood;
		// too late (they are well into something else, or in a fight, a party, a scripted event): keep going, but remember the reason
		const bool bFree = S2.GoalSince == Stamp && Now - Stamp < 25 && !S2.bInParty && S2.State != EHLSimState::Script && !InCombat(*U2) && Kept.IsValidIndex(Idx);
		UE_LOG(LogTemp, Display, TEXT("HL_MIND plan for %s: %s%s, because \"%s\"%s"), *S2.P->Handle, Kept.IsValidIndex(Idx) ? *Kept[Idx].Kind : TEXT("?"), Kept.IsValidIndex(Idx) ? *(TEXT(" ") + Kept[Idx].Arg) : TEXT(""), *Why, bFree ? TEXT("") : TEXT(" (too late, kept the local choice)"));
		if (!bFree) return;
		++PlansUsed;
		if (Idx != 0) MindSetGoal(S2, *U2, Kept[Idx].Kind, Kept[Idx].Arg, Why); else S2.GoalWhy = Why;
		const FHLUnit* P2 = PlayerUnit();
		if (!Say.IsEmpty() && P2 && Dist(*U2, *P2) < 25 && Chance(0.35f + S2.P->Tr.Soc * 0.5f)) AddChat(TEXT("say"), S2.P->Handle, Say, SimIdx, 0.6f + Rand());
	}, 14.f);
}

// ------------------------------------------------------------------------------------------------------------- noticing
void FHLGame::Notice(const TCHAR* Kind, const FString& Text, int32 AimedUnitId, float Salience, const TCHAR* Mirror)
{
	const FHLUnit* P = PlayerUnit();
	if (!P || P->Area < 0) return;
	TArray<int32> Saw;
	for (int32 i = 0; i < Sims.Num(); ++i)
	{
		FHLSim& S = Sims[i];
		FHLUnit* U = S.UnitId ? Unit(S.UnitId) : nullptr;
		if (!U || U->bDead || U->Area != P->Area || S.bAfk || S.State == EHLSimState::Script || S.State == EHLSimState::Leaving || SimTyping(S)) continue;
		const bool bAimed = U->Id == AimedUnitId;
		const float Dd = Dist(*U, *P);
		if (Dd > (bAimed ? 40.f : 26.f) || Now < S.NextNotice || !Areas[U->Area].Nav.SegClear(U->Pos, P->Pos)) continue;
		// looking the other way: only close things register
		const float Rad = FMath::DegreesToRadians(U->Yaw);
		const bool bFacing = FVector2D::DotProduct(FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)), (P->Pos - U->Pos).GetSafeNormal()) > -0.2f;
		if (!bFacing && Dd > 9 && !bAimed) continue;
		const FHLTraits& T = S.P->Tr;
		float Pr = Salience * (0.2f + T.Soc * 0.5f + T.Cur * 0.35f) * (bAimed ? 2.2f : 1.f) * (InCombat(*U) ? 0.25f : S.State == EHLSimState::Grind ? 0.6f : 1.f);
		if (S.P->Archetype == TEXT("lurker")) Pr *= 0.12f;
		const FHLRelation* R = Player.Relations.Find(S.P->Id);
		if (R && (R->Liking <= -3 || R->Frustration >= 4)) Pr *= 0.3f; // they have had enough of this player
		if (!Chance(FMath::Min(0.95f, Pr)))
		{
			// seen and deliberately ignored: that is a choice too
			if (bAimed) { S.NextNotice = Now + 10; if (Chance(0.5f)) Remember(i, TEXT("ignored"), FString::Printf(TEXT("ignored the player when they %s"), Kind), 0.2f); UE_LOG(LogTemp, Display, TEXT("HL_MIND %s ignores the player (%s)"), *S.P->Handle, Kind); }
			continue;
		}
		S.NextNotice = Now + 22 + Rand() * 50; ++NoticeCount;
		if (!InCombat(*U))
		{
			// body language first: stop, look, perhaps do it too
			S.LookAt = P->Id; S.LookUntil = Now + 2.5f + Rand() * 4.f + (bAimed ? 2.f : 0.f);
			if (Mirror && Chance(0.2f + T.Hum * 0.35f + T.Soc * 0.25f)) { S.ReactClip = Mirror; S.ReactAt = Now + 0.7f + S.P->ReactionS + Rand() * 1.2f; }
		}
		if (bAimed) Saw.Insert(i, 0); else Saw.Add(i);
		UE_LOG(LogTemp, Display, TEXT("HL_MIND %s notices the player (%s)%s"), *S.P->Handle, Kind, S.ReactAt > Now ? TEXT(", joins in") : TEXT(""));
	}
	if (!Saw.Num()) return;
	Saw.SetNum(FMath::Min(2, Saw.Num()));
	if (bWorkerOk && Now >= NextObserveCall)
	{
		NextObserveCall = Now + 20 + Rand() * 16;
		const FString What = Text.Left(200);
		WorkerSim(TEXT("observe"), Saw, TEXT("observe"), TEXT("say"), What, {}, [this, Saw, What](bool bOk, const TSharedPtr<FJsonObject>& Resp)
		{
			if (!bOk || !Resp.IsValid()) return;
			ApplyWorkerActions(Resp, TEXT("say"), Saw);
			for (int32 SI : Saw) Remember(SI, TEXT("saw"), FString::Printf(TEXT("saw this: %s"), *What.Left(110)), 0.25f);
		});
	}
	else if (!bWorkerOk && AimedUnitId && Chance(0.5f)) SimSay(Saw[0], TEXT("say"), TEXT("obs_react"), 1.5f + Rand() * 2, TEXT("lol"));
}

void FHLGame::TickMind()
{
	FHLUnit* P = PlayerUnit();
	if (!P) return;
	// ---- what the player is visibly doing (edges and streaks)
	static bool bWasAir = false, bWasDead = false; static int32 LastLevel = 0, StareSim = -1; static double StareSince = 0;
	const bool bAir = P->Z > 0.05f;
	if (bAir && !bWasAir)
	{
		if (Now > JumpWindow) { JumpWindow = Now + 7; JumpCount = 0; }
		if (++JumpCount >= 6) { JumpCount = 0; JumpWindow = 0; Notice(TEXT("kept jumping"), FString::Printf(TEXT("%s keeps jumping up and down on the spot"), *Player.Name), 0, 0.45f); }
	}
	bWasAir = bAir;
	if (P->bDead && !bWasDead) Notice(TEXT("died"), FString::Printf(TEXT("%s has just died right here"), *Player.Name), 0, 0.6f);
	bWasDead = P->bDead;
	if (LastLevel && P->Level > LastLevel) Notice(TEXT("levelled up"), FString::Printf(TEXT("%s has just reached level %d"), *Player.Name, P->Level), 0, 0.65f, TEXT("Cheer"));
	LastLevel = P->Level;
	if (Now >= NextNoticeScan)
	{
		NextNoticeScan = Now + 1;
		// standing right in front of someone, looking at them
		int32 Close = -1;
		const float Rad = FMath::DegreesToRadians(P->Yaw);
		if (!InCombat(*P) && !Party.Num() && OpenNpcUnitId == 0 && TradeSim < 0)
			for (int32 i = 0; i < Sims.Num(); ++i) if (Sims[i].UnitId) if (const FHLUnit* U = Unit(Sims[i].UnitId))
				if (U->Area == P->Area && Dist(*U, *P) < 2.6f && FVector2D::DotProduct(FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)), (U->Pos - P->Pos).GetSafeNormal()) > 0.7f) Close = i;
		if (Close != StareSim) { StareSim = Close; StareSince = Now; }
		else if (Close >= 0 && Now - StareSince > 7) { StareSince = Now + 40; Notice(TEXT("stood staring at them"), FString::Printf(TEXT("%s has walked right up to %s and is standing there staring at them without a word"), *Player.Name, *Sims[Close].P->Handle), Sims[Close].UnitId, 0.55f); }
		for (FHLSim& S : Sims) if (S.bAfk && S.AwayUntil > 0 && Now >= S.AwayUntil && S.Role == EHLSimRole::Local) { S.bAfk = false; S.AwayUntil = 0; if (FHLUnit* U = Unit(S.UnitId)) U->bSitting = false; }
		// moods and tempers cool off
		for (FHLSim& S : Sims) if (S.Frust > 0) S.Frust = FMath::Max(0.f, S.Frust - 0.012f);
	}
	TickMemory();

	// ---- self-play drivers (-HLAiTest=observe|party|memory): the test player does the things a person would
	static const FString AiTest = [] { FString V; FParse::Value(FCommandLine::Get(), TEXT("HLAiTest="), V); return V; }();
	static double NextTest = 20; static int32 TestStep = 0;
	if (AiTest.IsEmpty() || Now < NextTest || P->bDead) return;
	if (AiTest == TEXT("observe"))
	{
		// wait for an audience, as a person showing off would
		int32 Audience = 0;
		for (const FHLSim& S : Sims) if (S.UnitId) if (const FHLUnit* U = Unit(S.UnitId)) if (U->Area == P->Area && !U->bDead && Dist(*U, *P) < 18) ++Audience;
		if (!Audience || InCombat(*P)) { NextTest = Now + 1; return; }
		NextTest = Now + 12;
		static const TCHAR* Emotes[] = { TEXT("/wave"), TEXT("/dance"), TEXT("/rude"), TEXT("/flirt"), TEXT("/cheer"), TEXT("/dance"), TEXT("/bow") };
		int32 Near = 0; float BestD = 22;
		for (const FHLSim& S : Sims) if (S.UnitId) if (const FHLUnit* U = Unit(S.UnitId)) if (U->Area == P->Area && !U->bDead && Dist(*U, *P) < BestD) { BestD = Dist(*U, *P); Near = U->Id; }
		if (TestStep % 2 == 0 && Near) P->TargetId = Near; else if (P->TargetId && Unit(P->TargetId) && Unit(P->TargetId)->Kind == EHLKind::Sim) P->TargetId = 0;
		UE_LOG(LogTemp, Display, TEXT("HL_MIND test: player does %s%s (%d people within 18 m)"), Emotes[TestStep % 7], P->TargetId == Near && Near ? TEXT(" at someone") : TEXT(""), Audience);
		PlayerChat(Emotes[TestStep % 7]);
		++TestStep;
	}
	else if (AiTest == TEXT("party") && Party.Num())
	{
		NextTest = Now + 18;
		static const TCHAR* Lines[] = { TEXT("/p wait a sec, dont pull"), TEXT("/p ok go"), TEXT("/p kill the one on the left first"), TEXT("/p nice one, ty"), TEXT("/p follow me"), TEXT("/p heal me pls"), TEXT("/p brb drinking"), TEXT("/p ready?") };
		UE_LOG(LogTemp, Display, TEXT("HL_MIND test: player says %s"), Lines[TestStep % 8]);
		PlayerChat(Lines[TestStep++ % 8]);
		ChatChannel = TEXT("say");
	}
	else if (AiTest == TEXT("memory"))
	{
		NextTest = Now + 100000;
		// three acquaintances met five days ago, then the calendar catches up: fading, a save round trip, a greeting, a gist
		int32 Made = 0;
		for (int32 i = 0; i < Sims.Num() && Made < 3; ++i)
		{
			if (!Sims[i].bOnline || !Sims[i].UnitId || Sims[i].P->Tr.Soc < 0.5f) continue;
			Remember(i, TEXT("party"), TEXT("grouped with the player on Franklin Slopes for half an hour"), 0.5f);
			Remember(i, TEXT("kind"), TEXT("the player healed them through a bad pull"), 0.8f);
			Remember(i, TEXT("saw"), TEXT("saw the player dancing on the bank steps"), 0.2f);
			Remember(i, TEXT("talk"), TEXT("the player said they are also in the jam, making a racing game"), 0.5f);
			NoteTalk(i, Player.Name, TEXT("egyptian tomorrow at 8?")); NoteTalk(i, Sims[i].P->Handle, TEXT("sure if i finish my build"));
			FHLRelation& R = Relation(Sims[i]);
			R.Liking = 3; R.DecayDay -= 5; R.LastSeen -= 5 * 86400; for (FHLEpisode& E : R.Episodes) E.Day -= 5;
			++Made;
		}
		DecayMemories();
		const FString Json = SaveJson();
		int32 Before = 0, After = 0;
		for (const auto& KV : Player.Relations) Before += KV.Value.Episodes.Num();
		const TMap<FString, FHLRelation> Keep = Player.Relations;
		Player.Relations.Reset();
		TSharedPtr<FJsonObject> O;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (FJsonSerializer::Deserialize(Reader, O) && O.IsValid())
		{
			const TSharedPtr<FJsonObject>* Rel = nullptr;
			if (O->TryGetObjectField(TEXT("relations"), Rel)) for (const auto& KV : (*Rel)->Values) { const TArray<TSharedPtr<FJsonValue>>* Eps = nullptr; if (KV.Value->AsObject()->TryGetArrayField(TEXT("eps"), Eps)) After += Eps->Num(); }
		}
		Player.Relations = Keep;
		UE_LOG(LogTemp, Display, TEXT("HL_MIND test: memory for %d acquaintances, %d episodes after five days, %d in the save"), Made, Before, After);
		GreetAt = Now + 4; NextMemoryTick = 0;
	}
}

// ------------------------------------------------------------------------------------------------------------- memory
void FHLGame::Remember(int32 SimIdx, const TCHAR* Kind, const FString& Text, float W)
{
	if (!Sims.IsValidIndex(SimIdx) || Text.IsEmpty()) return;
	FHLRelation& R = Relation(Sims[SimIdx]);
	const int64 T = RealNow();
	if (!R.FirstMet) { R.FirstMet = T; R.DecayDay = Today(); }
	R.LastSeen = T;
	for (FHLEpisode& E : R.Episodes) if (E.Kind == Kind && E.Text == Text) { E.W = FMath::Min(1.f, E.W + W * 0.5f); E.Day = Today(); return; }
	FHLEpisode E; E.Day = Today(); E.Kind = Kind; E.Text = Text.Left(160); E.W = FMath::Clamp(W, 0.05f, 1.f);
	R.Episodes.Add(MoveTemp(E)); ++R.Fresh;
	// a person holds only so much about one acquaintance: the faintest goes
	while (R.Episodes.Num() > 12) { int32 Low = 0; for (int32 i = 1; i < R.Episodes.Num(); ++i) if (R.Episodes[i].W < R.Episodes[Low].W) Low = i; R.Episodes.RemoveAt(Low); }
	UE_LOG(LogTemp, Verbose, TEXT("HL_MIND %s remembers [%s %.2f] %s"), *Sims[SimIdx].P->Handle, Kind, W, *Text);
}

void FHLGame::NoteTalk(int32 SimIdx, const FString& From, const FString& Text)
{
	if (!Sims.IsValidIndex(SimIdx) || Text.IsEmpty()) return;
	FHLRelation& R = Relation(Sims[SimIdx]);
	R.Lines.Add(FString::Printf(TEXT("%s: %s"), *From, *Text.Left(110)));
	while (R.Lines.Num() > 6) R.Lines.RemoveAt(0);
	if (!R.FirstMet) { R.FirstMet = RealNow(); R.DecayDay = Today(); }
	R.LastSeen = RealNow();
}

void FHLGame::DecayMemories()
{
	const FHLData& D = FHLData::Get();
	const int32 Day = Today();
	int32 Faded = 0, Kept = 0;
	for (auto& KV : Player.Relations)
	{
		FHLRelation& R = KV.Value;
		if (!R.DecayDay) { R.DecayDay = Day; continue; }
		const int32 Days = Day - R.DecayDay;
		if (Days <= 0) continue;
		R.DecayDay = Day;
		float Attn = 0.5f;
		for (const FHLPersona& Pe : D.Personas) if (Pe.Id == KV.Key) { Attn = Pe.Tr.Attn; break; }
		for (int32 i = R.Episodes.Num() - 1; i >= 0; --i)
		{
			FHLEpisode& E = R.Episodes[i];
			// half-life in days: 2 for a passing thing seen by someone inattentive, about two weeks for something that mattered
			const bool bMattered = E.Kind == TEXT("slight") || E.Kind == TEXT("kind") || E.Kind == TEXT("gift") || E.Kind == TEXT("party") || E.Kind == TEXT("wipe") || E.Kind == TEXT("duel");
			const float Half = (2.f + 6.f * Attn) * (bMattered ? 2.f : 1.f);
			E.W *= FMath::Pow(0.5f, Days / Half);
			if (E.W < 0.07f) { R.Episodes.RemoveAt(i); ++Faded; } else ++Kept;
		}
		// feelings settle too
		if (Days >= 2) { R.Frustration = FMath::Max(0, R.Frustration - Days / 2); if (Days >= 7 && FMath::Abs(R.Liking) > 0 && R.Bond.IsEmpty()) R.Liking -= FMath::Sign(R.Liking); }
		if (Days >= 14 && !R.Episodes.Num()) R.Lines.Reset();
	}
	// the save holds memories for about sixty acquaintances: the faintest strangers go first
	TArray<TPair<float, FString>> Weights;
	for (const auto& KV : Player.Relations) { if (!KV.Value.Bond.IsEmpty() || (!KV.Value.Episodes.Num() && KV.Value.Gist.IsEmpty())) continue; float W = FMath::Abs(KV.Value.Liking) * 0.2f; for (const FHLEpisode& E : KV.Value.Episodes) W += E.W; Weights.Add({ W, KV.Key }); }
	Weights.Sort([](const TPair<float, FString>& A, const TPair<float, FString>& B) { return A.Key < B.Key; });
	for (int32 i = 0; i < Weights.Num() - 60; ++i) { FHLRelation& R = Player.Relations[Weights[i].Value]; R.Episodes.Reset(); R.Gist.Reset(); R.Lines.Reset(); }
	if (Faded || Kept) UE_LOG(LogTemp, Display, TEXT("HL_MIND memories: %d kept, %d faded away"), Kept, Faded);
}

void FHLGame::TickMemory()
{
	if (Now < NextMemoryTick) return;
	NextMemoryTick = Now + 20;
	static bool bDecayed = false;
	if (!bDecayed) { bDecayed = true; DecayMemories(); }
	const FHLUnit* P = PlayerUnit();
	// ---- someone who remembers the player says hello after a login
	if (GreetAt > 0 && Now >= GreetAt && P && !P->bDead)
	{
		GreetAt = 0;
		int32 Best = -1; float BestW = 0.4f; int64 Away = 0;
		for (int32 i = 0; i < Sims.Num(); ++i)
		{
			const FHLSim& S = Sims[i];
			const FHLRelation* R = Player.Relations.Find(S.P->Id);
			if (!R || !S.bOnline || S.bAfk || R->Liking < 0 || R->Frustration >= 3 || (R->Episodes.Num() == 0 && R->Gist.IsEmpty()) || RealNow() - R->LastSeen < 1800) continue;
			float W = R->Liking * 0.3f + S.P->Tr.Soc + (R->Bond.IsEmpty() ? 0 : 1.f) + Rand() * 0.5f; for (const FHLEpisode& E : R->Episodes) W += E.W * 0.5f;
			if (S.P->Archetype == TEXT("lurker") || S.P->Archetype == TEXT("silentpro")) W -= 1.5f;
			if (W > BestW) { BestW = W; Best = i; Away = RealNow() - R->LastSeen; }
		}
		if (Best >= 0)
		{
			const FString Gap = Away > 172800 ? FString::Printf(TEXT("%d days"), (int32)(Away / 86400)) : Away > 5400 ? FString::Printf(TEXT("%d hours"), (int32)(Away / 3600)) : TEXT("a little while");
			UE_LOG(LogTemp, Display, TEXT("HL_MIND %s greets the returning player (away %s)"), *Sims[Best].P->Handle, *Gap);
			TArray<int32> Sp = { Best };
			WorkerSim(TEXT("reply"), Sp, TEXT("greet"), TEXT("whisper"), FString::Printf(TEXT("The player has just logged in again after %s away."), *Gap), {}, [this, Sp](bool bOk, const TSharedPtr<FJsonObject>& Resp)
			{
				if (bOk && Resp.IsValid()) ApplyWorkerActions(Resp, TEXT("whisper"), Sp);
				else SimSay(Sp[0], TEXT("whisper"), TEXT("greet_back"), 2 + Rand() * 3, TEXT("wb {player}"));
			});
			Relation(Sims[Best]).LastSeen = RealNow();
		}
	}
	// ---- fold fresh episodes into the gist, one acquaintance at a time
	static double NextFold = 0; static bool bFolding = false;
	if (!bWorkerOk || bFolding || Now < NextFold) return;
	for (int32 i = 0; i < Sims.Num(); ++i)
	{
		FHLRelation* R = Player.Relations.Find(Sims[i].P->Id);
		if (!R || R->Fresh < 4 || R->Episodes.Num() < 3) continue;
		NextFold = Now + 45; bFolding = true;
		TSharedRef<FJsonObject> Req = MakeShared<FJsonObject>();
		Req->SetArrayField(TEXT("speakers"), { MakeShared<FJsonValueObject>(SpeakerJson(i)) });
		TSharedRef<FJsonObject> M = MakeShared<FJsonObject>();
		M->SetStringField(TEXT("gist"), R->Gist);
		TArray<TSharedPtr<FJsonValue>> Eps;
		for (const FHLEpisode& E : R->Episodes) { TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetNumberField(TEXT("days"), FMath::Max(0, Today() - E.Day)); O->SetNumberField(TEXT("w"), E.W); O->SetStringField(TEXT("kind"), E.Kind); O->SetStringField(TEXT("text"), E.Text); Eps.Add(MakeShared<FJsonValueObject>(O)); }
		M->SetArrayField(TEXT("episodes"), Eps); M->SetArrayField(TEXT("lines"), Strs(R->Lines));
		Req->SetObjectField(TEXT("memory"), M);
		const int32 Count = R->Episodes.Num();
		WorkerPost(TEXT("consolidate"), Req, [this, i, Count](bool bOk, const TSharedPtr<FJsonObject>& Resp)
		{
			bFolding = false;
			FHLRelation& R2 = Relation(Sims[i]);
			R2.Fresh = 0;
			FString Gist;
			if (!bOk || !Resp.IsValid() || !Resp->TryGetStringField(TEXT("gist"), Gist) || Gist.IsEmpty() || R2.Episodes.Num() != Count) return;
			R2.Gist = Gist;
			TSet<int32> Keep;
			const TArray<TSharedPtr<FJsonValue>>* K = nullptr;
			if (Resp->TryGetArrayField(TEXT("keep"), K)) for (auto& V : *K) Keep.Add((int32)V->AsNumber());
			for (int32 e = R2.Episodes.Num() - 1; e >= 0; --e) if (!Keep.Contains(e) && R2.Episodes[e].W < 0.75f) R2.Episodes.RemoveAt(e);
			FString Str;
			if (Resp->TryGetStringField(TEXT("promise"), Str)) R2.Promise = Str;
			if (Resp->TryGetStringField(TEXT("nickname"), Str) && !Str.IsEmpty()) R2.Nick = Str;
			UE_LOG(LogTemp, Display, TEXT("HL_MIND %s now remembers the player as: %s"), *Sims[i].P->Handle, *Gist);
		}, 20.f);
		break;
	}
}

// ------------------------------------------------------------------------------------------------------------- party
void FHLGame::PartyFrustrate(int32 SimIdx, float Amount, const TCHAR* Why)
{
	if (!Sims.IsValidIndex(SimIdx)) return;
	FHLSim& S = Sims[SimIdx];
	const FHLTraits& T = S.P->Tr;
	S.Frust += Amount * (1.3f - T.Pati) * (S.P->Archetype == TEXT("drama") || S.P->Archetype == TEXT("grumbler") ? 1.5f : 1.f);
	if (!S.bInParty || S.Frust < 2.5f || Now < S.NextGripe) return;
	S.NextGripe = Now + 40 + Rand() * 40;
	const bool bLeave = S.Frust > 5.5f && (T.Pati < 0.45f || S.Frust > 8.f);
	S.Mood = bLeave ? TEXT("fed up") : TEXT("annoyed");
	FHLRelation& R = Relation(S);
	R.Frustration = FMath::Min(10, R.Frustration + 1); if (bLeave) R.Liking = FMath::Max(-10, R.Liking - 1);
	Remember(SimIdx, TEXT("slight"), FString::Printf(TEXT("got annoyed in a group with the player: %s"), Why), bLeave ? 0.7f : 0.45f);
	UE_LOG(LogTemp, Display, TEXT("HL_MIND %s is %s: %s (%.1f)"), *S.P->Handle, bLeave ? TEXT("leaving, fed up") : TEXT("annoyed"), Why, S.Frust);
	TArray<int32> Sp = { SimIdx };
	const FString What = FString::Printf(TEXT("%s is %s with the group because: %s.%s"), *S.P->Handle, bLeave ? TEXT("fed up") : TEXT("annoyed"), Why, bLeave ? TEXT(" They say so and are about to leave the party.") : TEXT(" They say so, their way."));
	WorkerSim(TEXT("reply"), Sp, TEXT("event"), TEXT("party"), What, {}, [this, Sp, bLeave](bool bOk, const TSharedPtr<FJsonObject>& Resp)
	{
		if (bOk && Resp.IsValid()) ApplyWorkerActions(Resp, TEXT("party"), Sp);
		else SimSay(Sp[0], TEXT("party"), bLeave ? TEXT("gripe_leave") : TEXT("gripe"), 1 + Rand() * 2, bLeave ? TEXT("ok im done, gl") : TEXT("come on"));
	});
	if (bLeave) { S.StateUntil = Now + 9; S.Frust = 2; } // the brain's normal "time to go" takes it from here
}

/** A party line with a meaning the others can act on. Each listener hears it (or not) and takes it right (or not). */
void FHLGame::SimPartySay(int32 SimIdx, const TCHAR* Intent, const TCHAR* Situation, const TCHAR* Fallback, int32 TargetUnit)
{
	if (!Sims.IsValidIndex(SimIdx) || !Sims[SimIdx].bInParty) return;
	SimSay(SimIdx, TEXT("party"), Situation, 0.2f + Rand() * 0.6f, Fallback);
	PartyHeard(SimIdx, Intent, TargetUnit);
}

void FHLGame::PartyHeard(int32 FromSim, const FString& InIntent, int32 TargetUnit)
{
	FHLUnit* P = PlayerUnit();
	for (int32 SI : Party)
	{
		if (SI == FromSim) continue;
		FHLSim& S = Sims[SI];
		FHLUnit* U = Unit(S.UnitId);
		if (!U || U->bDead) continue;
		const FHLTraits& T = S.P->Tr;
		// busy fighting, or just not reading chat
		if (Chance((InCombat(*U) ? 0.22f : 0.05f) * (1.3f - T.Attn))) { UE_LOG(LogTemp, Display, TEXT("HL_MIND %s misses \"%s\" in party chat"), *S.P->Handle, *InIntent); ++Misreads; continue; }
		FString Intent = InIntent; int32 Target = TargetUnit;
		if (Chance((1.f - T.Attn) * 0.16f + (S.P->English == TEXT("basic") ? 0.06f : 0.f) + FMath::Min(0.1f, S.Load * 0.06f)))
		{
			// took it the wrong way: the neighbouring meaning
			if (Intent == TEXT("wait")) Intent = TEXT("go"); else if (Intent == TEXT("go")) Intent = TEXT("wait"); else if (Intent == TEXT("rest")) Intent = TEXT("go"); else if (Intent == TEXT("follow")) Intent = TEXT("wait");
			else if (Intent == TEXT("attack")) { TArray<int32> Others; for (FHLUnit* O : UnitList) if (O->Kind == EHLKind::Mob && !O->bDead && O->Area == U->Area && O->Id != TargetUnit && Dist(*O, *U) < 35) Others.Add(O->Id); if (Others.Num()) Target = Others[RandInt(0, Others.Num() - 1)]; }
			else if (Intent == TEXT("heal")) Intent = TEXT("banter");
			++Misreads;
			UE_LOG(LogTemp, Display, TEXT("HL_MIND %s misreads \"%s\" as \"%s\""), *S.P->Handle, *InIntent, *Intent);
		}
		if (Intent == TEXT("wait"))
		{
			// the impatient hear it and go anyway, a little later
			if (Chance(0.3f + T.Pati * 0.7f)) S.BelWaitUntil = Now + 22 + T.Pati * 15; else S.ImpatientAt = Now + 6 + Rand() * 8;
		}
		else if (Intent == TEXT("go")) { S.BelWaitUntil = 0; S.BelRestUntil = 0; U->bSitting = false; if (T.Aggr > 0.7f) S.ImpatientAt = Now + 1 + Rand() * 2; }
		else if (Intent == TEXT("attack")) { S.BelTarget = Target; S.BelWaitUntil = 0; }
		else if (Intent == TEXT("heal")) { if (FromSim < 0) S.HealBiasUntil = Now + 20; }
		else if (Intent == TEXT("follow")) { S.TightUntil = Now + 45; S.BelWaitUntil = 0; U->bSitting = false; }
		else if (Intent == TEXT("rest")) { S.BelWaitUntil = Now + 14 + T.Pati * 10; if (U->HpPct() < 0.9f || (U->ResType == EHLRes::Power && U->Res < U->MaxRes * 0.8f)) S.BelRestUntil = Now + 12; }
		else if (Intent == TEXT("run")) { S.BelTarget = 0; S.TightUntil = Now + 15; S.BelWaitUntil = Now + 8; if (P) S.SeenPos = P->Pos; }
		else if (Intent == TEXT("thanks")) { S.Frust = FMath::Max(0.f, S.Frust - 1.5f); if (FromSim < 0 && Chance(0.4f)) { FHLRelation& R = Relation(S); R.Liking = FMath::Min(10, R.Liking + 1); } }
		else if (Intent == TEXT("sorry")) S.Frust = FMath::Max(0.f, S.Frust - 1.f);
		else if (Intent == TEXT("blame")) { if (FromSim < 0) PartyFrustrate(SI, S.P->Archetype == TEXT("drama") ? 2.f : 0.8f, TEXT("the player complained at the group")); }
	}
}

/** What a party member does off their own bat: say what they need, lose patience, pull when nobody else will. */
void FHLGame::TickPartyMind(FHLSim& S, FHLUnit& U)
{
	const int32 SimIdx = int32(&S - Sims.GetData());
	const FHLTraits& T = S.P->Tr;
	FHLUnit* P = PlayerUnit();
	if (!P || InCombat(U)) { S.ImpatientAt = 0; return; }
	if (S.BelTarget) { const FHLUnit* B = Unit(S.BelTarget); if (!B || B->bDead) S.BelTarget = 0; }
	// needs are spoken, not sensed: out of power, hurt
	const bool bLowPower = U.ResType == EHLRes::Power && U.Res < U.MaxRes * 0.3f;
	if ((bLowPower || U.HpPct() < 0.5f) && Now >= S.NextNeedSay && Now >= S.BelRestUntil)
	{
		S.NextNeedSay = Now + 40; S.BelRestUntil = Now + 14 + Rand() * 6;
		SimPartySay(SimIdx, TEXT("rest"), bLowPower ? TEXT("oom") : TEXT("party_rest"), bLowPower ? TEXT("oom, drinking") : TEXT("sec, need to eat"));
	}
	if (Now < S.BelRestUntil)
	{
		U.bSitting = true;
		// someone starts a fight while I am sitting here with no power: noted
		if (P->bAutoAttack && InCombat(*P) && Now >= S.NextGripe - 20) PartyFrustrate(SimIdx, 1.6f, TEXT("the player pulled while they were resting"));
		return;
	}
	// nothing is happening and there are things to kill right there: the impatient pull
	if (T.Aggr < 0.7f || Now < S.BelWaitUntil - (1.f - T.Pati) * 14) { S.ImpatientAt = 0; return; }
	if (S.ImpatientAt == 0) { S.ImpatientAt = Now + 14 + T.Pati * 40 + Rand() * 10; return; }
	if (Now < S.ImpatientAt || InCombat(*P) || Dist(U, *P) > 30) return;
	S.ImpatientAt = 0;
	FHLUnit* Best = nullptr; float BestD = 32;
	for (FHLUnit* O : UnitList)
	{
		if (O->Kind != EHLKind::Mob || O->bDead || O->Area != U.Area || !O->Mob || O->Mob->bGuard || O->MobState != EHLMobState::Idle || O->Level > S.Level + 2) continue;
		if (O->Mob->Rank != TEXT("normal") && S.P->Archetype != TEXT("leeroy")) continue;
		const float Dd = Dist(U, *O);
		if (Dd < BestD && Areas[U.Area].Nav.SegClear(U.Pos, O->Pos)) { Best = O; BestD = Dd; }
	}
	if (!Best) return;
	const bool bToldToWait = Now < S.BelWaitUntil;
	S.BelTarget = Best->Id; S.BelWaitUntil = 0; U.bSitting = false; U.TargetId = Best->Id;
	UE_LOG(LogTemp, Display, TEXT("HL_MIND %s pulls %s on their own%s"), *S.P->Handle, *Best->Name, bToldToWait ? TEXT(" (after being told to wait)") : TEXT(""));
	SimPartySay(SimIdx, TEXT("attack"), TEXT("party_pull"), S.P->Archetype == TEXT("leeroy") ? TEXT("LEEEROY") : TEXT("pulling"), Best->Id);
	// the careful ones did not want that
	for (int32 SI : Party) if (SI != SimIdx) { const FHLSim& O = Sims[SI]; const FHLUnit* OU = Unit(O.UnitId); if (OU && (O.P->Tr.Caut > 0.65f || Now < O.BelRestUntil || OU->HpPct() < 0.6f)) PartyFrustrate(SI, bToldToWait ? 1.5f : 0.8f, *FString::Printf(TEXT("%s pulled before the group was ready"), *S.P->Handle)); }
}
