// Simulated players (DESIGN-BIBLE §11, §13): the Census (off-screen personas), in-zone brains (locals, transients, idlers),
// party behaviour, and the Director that paces the first 15 minutes. All decisions here are local; the Worker only supplies
// speech and accept/decline decisions (HLChat.cpp).
#include "HLGame.h"
#include "Dom/JsonObject.h"

static const int32 MAX_LOCALS = 40, MAX_TRANSIENTS = 8, MAX_IDLERS = 6;

int32 FHLGame::OnlineCount() const { int32 N = 1; for (const FHLSim& S : Sims) if (S.bOnline) ++N; return N; }

int32 FHLGame::FindSimByHandle(const FString& Handle) const
{
	for (int32 i = 0; i < Sims.Num(); ++i) if (Sims[i].P->Handle.Equals(Handle, ESearchCase::IgnoreCase)) return i;
	for (int32 i = 0; i < Sims.Num(); ++i) if (Sims[i].P->Handle.StartsWith(Handle, ESearchCase::IgnoreCase) && Handle.Len() >= 3) return i;
	return -1;
}

FString FHLGame::SimActivity(const FHLSim& S) const
{
	if (!S.bOnline) return TEXT("offline");
	if (!S.UnitId && S.DungeonUntil > Now) return TEXT("in The Egyptian Theatre (dungeon)");
	const FHLZoneInfo* Z = FHLData::Get().Zone(S.Zone);
	if (S.UnitId)
	{
		if (S.bInParty)
		{
			// where they really are and what they are doing, so a line never claims to be somewhere else
			const FHLUnit* U = Unit(S.UnitId);
			const FHLUnit* T = U ? Unit(U->TargetId) : nullptr;
			const FString Where = U && Areas.IsValidIndex(U->Area) && U->Area != 0 ? FString::Printf(TEXT("inside %s"), *Areas[U->Area].Def->Name) : FString::Printf(TEXT("on Hollywood Boulevard near %s"), *CurSubzone);
			if (U && U->bDead) return FString::Printf(TEXT("in your party, %s, dead and waiting to be revived"), *Where);
			if (U && InCombat(*U)) return FString::Printf(TEXT("in your party, %s, in a fight right now%s"), *Where, T && T->Kind == EHLKind::Mob ? *FString::Printf(TEXT(" with %s"), *T->Name) : TEXT(""));
			return FString::Printf(TEXT("in your party, %s, a few steps from you"), *Where);
		}
		if (S.Role == EHLSimRole::Idler) return S.bAfk ? TEXT("AFK on Hollywood Boulevard") : TEXT("hanging around Hollywood Boulevard");
		if (S.Role == EHLSimRole::Transient) return TEXT("running errands on Hollywood Boulevard (bank, auction house)");
		if (S.bAfk) return TEXT("AFK on Hollywood Boulevard");
		if (S.State == EHLSimState::Grind || S.State == EHLSimState::Rest) return TEXT("questing on Hollywood Boulevard");
		if (!S.GoalKind.IsEmpty()) return FString::Printf(TEXT("%s on Hollywood Boulevard"), *S.GoalKind);
		return TEXT("on Hollywood Boulevard");
	}
	return S.Activity.IsEmpty() ? FString::Printf(TEXT("questing in %s"), Z ? *Z->Name : TEXT("the world")) : S.Activity;
}

int32 FHLGame::PickCampFor(int32 Level)
{
	const FHLAreaDef& Def = *Areas[0].Def;
	TArray<int32> Fit;
	for (int32 c = 0; c < Def.Camps.Num(); ++c)
	{
		const FHLMobDef& M = *Def.Camps[c].Mob;
		if (M.bGuard || M.Rank != TEXT("normal")) continue;
		if (M.LevelMin >= Level - 2 && M.LevelMin <= Level + 1) Fit.Add(c);
	}
	if (!Fit.Num()) for (int32 c = 0; c < Def.Camps.Num(); ++c) { const FHLMobDef& M = *Def.Camps[c].Mob; if (!M.bGuard && M.Rank == TEXT("normal") && M.LevelMax <= Level) Fit.Add(c); }
	return Fit.Num() ? Fit[RandInt(0, Fit.Num() - 1)] : 0;
}

FHLUnit* FHLGame::SpawnSim(int32 SimIdx, EHLSimRole Role, const FVector2D& Pos)
{
	FHLSim& S = Sims[SimIdx];
	if (S.UnitId) return Unit(S.UnitId);
	FHLUnit* U = AddUnit(EHLKind::Sim, 0, Areas[0].Nav.NearestFree(Pos));
	U->Name = S.P->Handle; U->SimIdx = SimIdx; U->Yaw = Rand() * 360;
	if (const FHLGuild* G = FHLData::Get().Guild(S.P->Guild)) U->Title = FString::Printf(TEXT("<%s>"), *G->Name);
	RecalcCharStats(*U, *S.P->Cls, S.Level, S.Gear, true);
	U->Color = S.P->Cls->Color;
	S.UnitId = U->Id; S.Role = Role; S.State = EHLSimState::Idle; S.StateUntil = Now + Rand() * 6; S.bInParty = false; S.Route.Reset(); S.RouteIdx = 0; S.CampIdx = -1;
	S.Zone = TEXT("zone_hollywood_blvd"); S.bOnline = true;
	S.TrainedLevel = S.Level - (Chance(0.3f) ? 2 : 0); S.LastTown = Now - Rand() * 500; S.FollowDist = 0; S.Frust = 0; S.GoalKind.Reset(); S.GoalWhy.Reset();
	S.TypeFrom = S.TypeUntil = 0; S.BelTarget = 0; S.BelWaitUntil = S.BelRestUntil = 0; S.NextSee = 0; S.MoveGoal = FVector2D::ZeroVector;
	return U;
}

void FHLGame::DespawnSim(int32 SimIdx)
{
	FHLSim& S = Sims[SimIdx];
	if (S.bInParty) { Party.Remove(SimIdx); S.bInParty = false; }
	if (S.UnitId) { if (PlayerUnit() && PlayerUnit()->TargetId == S.UnitId) PlayerUnit()->TargetId = 0; RemoveUnit(S.UnitId); }
	S.UnitId = 0; S.Role = EHLSimRole::Away;
	if (S.Level > 12) S.Zone = S.P->HomeZone;
}

void FHLGame::InitSims()
{
	const FHLData& D = FHLData::Get();
	if (Sims.Num()) return;
	Sims.SetNum(D.Personas.Num());
	const FHLAreaDef& Def = *Areas[0].Def;
	int32 Locals = 0, Idlers = 0;
	for (int32 i = 0; i < Sims.Num(); ++i)
	{
		FHLSim& S = Sims[i];
		S.P = &D.Personas[i]; S.Level = S.P->Level; S.Zone = S.P->HomeZone; S.bOnline = Chance(0.86f); S.Gear = S.P->Equipped;
		S.Xp = Rand() * XpToNext(S.Level) * 0.8f;
	}
	for (int32 i = 0; i < Sims.Num(); ++i)
	{
		FHLSim& S = Sims[i];
		if (!S.bOnline || !S.P->Cls) continue;
		if (S.Level <= 12 && Locals < MAX_LOCALS)
		{
			++Locals;
			if (Chance(0.7f))
			{
				const int32 Camp = PickCampFor(S.Level);
				SpawnSim(i, EHLSimRole::Local, Def.Camps[Camp].Pos + FVector2D(Rand() * 16 - 8, Rand() * 16 - 8));
				S.CampIdx = Camp; S.State = EHLSimState::Grind; S.KillGoal = RandInt(6, 16);
			}
			else SpawnSim(i, EHLSimRole::Local, Def.IdleSpots[RandInt(0, Def.IdleSpots.Num() - 1)] + FVector2D(Rand() * 6 - 3, Rand() * 4 - 2));
		}
		else if (S.Level >= 13 && Idlers < MAX_IDLERS && Chance(S.Level >= 30 ? 0.3f : 0.04f))
		{
			++Idlers;
			FHLUnit* U = SpawnSim(i, EHLSimRole::Idler, Def.IdleSpots[RandInt(0, Def.IdleSpots.Num() - 1)] + FVector2D(Rand() * 5 - 2.5f, Rand() * 4 - 2));
			S.bAfk = Chance(0.4f); S.StateUntil = Now + 200 + Rand() * 900;
			if (U && !S.P->Mount.IsEmpty() && Chance(0.5f)) { U->bMounted = true; U->MountKind = HLMountKind(S.P->Mount); }
		}
	}
		NextCensus = Now + 5; NextPopCheck = Now + 8; NextDirector = Now + 5;
		BuildHistory();
	}

// ------------------------------------------------------------------------------------------------------------- brains
const FHLAbility* FHLGame::SimPickAbility(FHLSim& S, FHLUnit& U, FHLUnit*& OutTarget)
{
	const FHLClass& C = *S.P->Cls;
	FHLUnit* Enemy = Unit(U.TargetId);
	if (Enemy && (Enemy->bDead || !IsHostile(U, *Enemy))) Enemy = nullptr;
	TArray<FHLUnit*> Friends;
	Friends.Add(&U);
	if (S.bInParty)
	{
		if (FHLUnit* P = PlayerUnit()) if (!P->bDead && Dist(U, *P) < 40) Friends.Add(P);
		for (int32 SI : Party) if (FHLUnit* M = Unit(Sims[SI].UnitId)) if (M != &U && !M->bDead && Dist(U, *M) < 40) Friends.Add(M);
	}
	FHLUnit* Lowest = &U; float LowPct = U.HpPct();
	for (FHLUnit* F : Friends)
	{
		float Pct = F->HpPct();
		if (F->Kind == EHLKind::Player && Now < S.HealBiasUntil) Pct -= 0.25f;
		if (Pct < LowPct) { LowPct = Pct; Lowest = F; }
	}
	int32 Near = 0;
	if (Enemy) for (FHLUnit* O : UnitList) if (O->Kind == EHLKind::Mob && !O->bDead && O->Area == U.Area && O->MobState == EHLMobState::Combat && FVector2D::Distance(O->Pos, Enemy->Pos) < 7) ++Near;

	const FHLAbility* Best = nullptr; float BestScore = 0; FHLUnit* BestTarget = nullptr;
	for (const FHLAbility* Ab : C.Abilities)
	{
		if (Ab->Level > S.Level) continue;
		float Score = 0; FHLUnit* T = Enemy;
		const bool bHeal = Ab->IsHeal() && (Ab->HasEffect(TEXT("heal")) || Ab->HasEffect(TEXT("hot")) || Ab->HasEffect(TEXT("shield")));
		if (bHeal)
		{
			T = Ab->Target == TEXT("self") ? &U : Lowest;
			const float Pct = T == &U ? U.HpPct() : LowPct;
			if (Pct > 0.72f - (1.f - S.P->Tr.Skill) * 0.22f) continue; // the slower healers notice late
			if (T != &U && S.Load > 1.1f && S.P->Tr.Attn < 0.5f && U.HpPct() < 0.6f) continue; // tunnel vision: saving themselves
			if ((Ab->HasEffect(TEXT("hot")) || Ab->HasEffect(TEXT("shield"))) && T->HasAuraNamed(Ab->Name)) continue;
			Score = 100 + (1 - Pct) * 100 + (Ab->Cast <= 0 && Pct < 0.4f ? 40 : 0);
			if (Ab->Target == TEXT("party")) { int32 Hurt = 0; for (FHLUnit* F : Friends) if (F->HpPct() < 0.7f) ++Hurt; if (Hurt < 2) continue; Score += 30; }
		}
		else if (Ab->Target == TEXT("self") && Ab->Radius <= 0)
		{
			if (!Enemy || !InCombat(U) || U.HasAuraNamed(Ab->Name)) continue;
			Score = Ab->HasEffect(TEXT("buff")) && Ab->Effects[0].Stat == TEXT("taken_pct") ? (U.HpPct() < 0.5f ? 90 : 0) : 30;
			if (Score <= 0) continue;
			T = &U;
		}
		else
		{
			if (!Enemy) continue;
			for (const FHLEffect& E : Ab->Effects)
			{
				if (E.Type == TEXT("damage")) Score += E.Weapon * 14 + E.Base + E.Coef * 6;
				else if (E.Type == TEXT("dot")) Score += Enemy->HasAuraNamed(Ab->Name) ? -1000 : (Enemy->HpPct() > 0.4f ? 26 : -1000);
				else if (E.Type == TEXT("stun")) Score += (Enemy->Casting || Enemy->HpPct() > 0.5f) ? 22 : 0;
				else if (E.Type == TEXT("slow")) Score += Enemy->HasAuraNamed(Ab->Name) ? -1000 : 4;
				else if (E.Type == TEXT("taunt")) Score += (C.Role == TEXT("tank") && S.bInParty && Enemy->TargetId != U.Id) ? 200 : -1000;
			}
			if (Ab->Radius > 0) Score = Near >= 3 ? Score * 2 : Score * 0.6f;
			if (Score <= 0) continue;
			T = Ab->Target == TEXT("self") ? &U : Enemy;
			if (Ab->Target == TEXT("self") && Ab->Radius > 0 && Dist(U, *Enemy) > Ab->Radius) continue;
		}
		Score *= 1.f + (Rand() - 0.5f) * 1.6f * (1.f - S.P->Tr.Skill) * (Now < S.PanicUntil ? 2.f : 1.f); // good enough, not optimal
		if (Score <= BestScore) continue;
		if (!CanCast(U, *Ab, T).IsEmpty()) continue;
		Best = Ab; BestScore = Score; BestTarget = T;
	}
	OutTarget = BestTarget;
	return Best;
}

void FHLGame::SimCombat(FHLSim& S, FHLUnit& U)
{
	FHLUnit* T = Unit(U.TargetId);
	const bool bEnemy = T && !T->bDead && IsHostile(U, *T) && T->Area == U.Area;
	if (bEnemy)
	{
		U.bAutoAttack = true;
		const bool bRangedStyle = S.P->Cls->bCaster || U.bRangedAuto;
		const float Want = bRangedStyle ? 20.f : U.AutoRange - 0.8f;
		const float D = Dist(U, *T);
		const bool bLos = Areas[U.Area].Nav.SegClear(U.Pos, T->Pos);
		if (!U.Casting)
		{
			if (D > Want + (bRangedStyle ? 6 : 0) || !bLos) MoveTo(U, T->Pos, FMath::Max(1.5f, Want - 1));
			else { StopMove(U); FaceTo(U, T->Pos); }
		}
	}
	// ---- the person at the keyboard (HLMotor.cpp): a new pull takes a moment to read, pressure breeds mistakes
	const FHLTraits& Tr = S.P->Tr;
	const int32 SimIdx = int32(&S - Sims.GetData());
	S.Load = SimLoad(S, U);
	if (bEnemy && U.TargetId != S.EngageId) { S.EngageId = U.TargetId; S.EngageAt = Now + S.P->ReactionS * (0.5f + Rand()) + (1.f - Tr.Skill) * 0.9f; }
	const bool bDecide = Now >= S.NextAct; // (rolls below are per decision, not per frame)
	if (bDecide && bEnemy && S.Load > 1.4f && U.HpPct() < 0.45f && Now >= S.PanicUntil + 30 && Chance((1.f - Tr.Skill) * 0.12f))
	{
		// too much at once: a shout (short enough to type mid-fight) and a few seconds of bad decisions
		S.PanicUntil = Now + 3 + Rand() * 3;
		if (Now >= S.NextNeedSay) { S.NextNeedSay = Now + 80; if (S.bInParty) SimPartySay(SimIdx, TEXT("heal"), TEXT("combat_help"), TEXT("help")); else if (Chance(0.35f)) SimSay(SimIdx, TEXT("say"), TEXT("combat_help"), 0.2f, TEXT("help")); }
		UE_LOG(LogTemp, Display, TEXT("HL_MIND %s is overwhelmed (load %.1f)"), *S.P->Handle, S.Load);
	}
	// backing away from something that is chewing on you: most learn it by accident, then keep doing it
	if (bDecide && bEnemy && S.P->Cls->Role != TEXT("tank") && T->TargetId == U.Id && !T->bRangedAuto && U.HpPct() < 0.4f && Now >= S.KiteUntil + 6 && !U.Casting)
		if (Chance(S.bKnowsKite ? 0.3f : 0.03f) || (Now < S.PanicUntil && Chance(0.2f))) { S.bKiting = true; S.KiteUntil = Now + 2.5f + Rand() * 1.5f; }
	if (S.bKiting)
	{
		if (!bEnemy || Now >= S.KiteUntil) S.bKiting = false;
		else { const FVector2D Away = (U.Pos - T->Pos).GetSafeNormal(); MoveTo(U, Areas[U.Area].Nav.NearestFree(U.Pos + Away * 8.f), 0.5f); }
	}
	if (T && T->bDead && S.KiteUntil > Now - 12 && !S.bKnowsKite && S.KiteUntil > 0 && !U.bDead)
	{
		S.bKnowsKite = true;
		UE_LOG(LogTemp, Display, TEXT("HL_MIND %s has learned to kite"), *S.P->Handle);
		if (const FHLUnit* PU = PlayerUnit()) if (Dist(U, *PU) < 30 && Chance(0.6f)) SimSay(SimIdx, S.bInParty ? TEXT("party") : TEXT("say"), TEXT("trick"), 2 + Rand() * 2, TEXT("oh you can just back up and they cant hit you"));
	}
	if (Now >= S.NextAct && Now >= S.EngageAt && Now >= U.GcdUntil && !U.Casting && !U.Stunned())
	{
		FHLUnit* AT = nullptr;
		if (const FHLAbility* Ab = SimPickAbility(S, U, AT))
		{
			if (Chance(S.P->MistakeRate * (1.f + 1.6f * FMath::Min(1.5f, S.Load)) * (Now < S.PanicUntil ? 3.f : 1.f))) S.NextAct = Now + 1.2f + Rand() * 2; // a mistake: hesitates
			else { TryCast(U, *Ab, AT ? AT->Id : 0); S.NextAct = Now + S.P->ReactionS * (0.6f + Rand() * 0.8f) * (1.f + FMath::Min(0.6f, S.Load * 0.3f)); }
		}
		else S.NextAct = Now + 0.4f;
	}
}

void FHLGame::TickSimBrain(FHLSim& S, FHLUnit& U, float Dt)
{
	const FHLAreaDef& Def = *Areas[0].Def;
	const int32 SimIdx = int32(&S - Sims.GetData());
	FHLUnit* P = PlayerUnit();
	// level up (locals earn it from kills)
	if (S.Level < 60 && S.Xp >= XpToNext(S.Level))
	{
		S.Xp -= XpToNext(S.Level); S.Level++;
		RecalcCharStats(U, *S.P->Cls, S.Level, S.Gear, true);
		if (P && Dist(*P, U) < 40) { AddFloat(U.Id, TEXT("Level up!"), FLinearColor(1, 0.85f, 0.2f)); if (Chance(0.6f)) SimSay(SimIdx, S.bInParty ? TEXT("party") : TEXT("say"), TEXT("ding"), 1 + Rand() * 2); }
	}
	if (U.bDead)
	{
		if (S.State == EHLSimState::Script) return;
		if (S.bInParty)
		{
			// gets up once the fight is over and the player is alive
			if (P && !P->bDead && !InCombat(*P) && Now - U.DeadAt > 8) { U.bDead = false; U.Hp = U.MaxHp * 0.5f; U.Res = U.ResType == EHLRes::Adrenaline ? 0 : U.MaxRes * 0.5f; TeleportUnit(U, P->Area, P->Pos + FVector2D(Rand() * 4 - 2, Rand() * 4 - 2)); }
			return;
		}
		if (Now >= S.StateUntil)
		{
			FVector2D Where = Def.Aid[0].Pos; float Best = 1e9f;
			for (const FHLAidStation& A : Def.Aid) { const float D = FVector2D::Distance(A.Pos, U.Pos); if (D < Best) { Best = D; Where = A.Pos; } }
			U.bDead = false; U.Hp = U.MaxHp * 0.5f; U.Res = U.ResType == EHLRes::Adrenaline ? 0 : U.MaxRes * 0.5f; U.CombatUntil = 0;
			TeleportUnit(U, 0, Where + FVector2D(Rand() * 4 - 2, Rand() * 4 - 2));
			S.State = EHLSimState::Rest; S.StateUntil = Now + 10;
			if (S.CampIdx >= 0 && Chance(0.4f)) S.CampIdx = PickCampFor(FMath::Max(1, S.Level - 1));
		}
		return;
	}

	// ---- hands on the keyboard (HLMotor.cpp): typing a line means not moving and not pressing abilities. In a fight the
	// line waits for a lull (up to ten seconds), then they stop and type it anyway.
	if (SimTyping(S))
	{
		if (InCombat(U) && S.TypeHeld < 10.f)
		{
			S.TypeHeld += Dt; S.TypeFrom += Dt; S.TypeUntil += Dt;
			for (FPendingChat& Pc : PendingChat) if (Pc.Line.SimIdx == SimIdx) Pc.At += Dt;
		}
		else { StopMove(U); if (!InCombat(U)) U.bAutoAttack = false; return; }
	}
	else if (Now >= S.TypeUntil) S.TypeHeld = 0;
	// ---- something caught their eye (HLMind.cpp Notice): stop, look, perhaps join in
	if (Now < S.LookUntil && !InCombat(U) && !S.bInParty)
	{
		if (const FHLUnit* L = Unit(S.LookAt)) { StopMove(U); FaceTo(U, L->Pos); }
		if (S.ReactAt > 0 && Now >= S.ReactAt) { S.ReactAt = 0; U.bSitting = false; Gesture(U, *S.ReactClip); }
		return;
	}
	if (S.ReactAt > 0 && Now >= S.ReactAt) { S.ReactAt = 0; if (!InCombat(U)) Gesture(U, *S.ReactClip); }
	// ---- helping the player out of a fight (a goal the mind chose)
	if (S.HelpUnit && !S.bInParty)
	{
		FHLUnit* Hm = Now < S.HelpUntil ? Unit(S.HelpUnit) : nullptr;
		if (Hm && !Hm->bDead && Hm->Kind == EHLKind::Mob && Hm->Area == U.Area) { U.bSitting = false; U.TargetId = Hm->Id; SimCombat(S, U); return; }
		S.HelpUnit = 0; U.TargetId = 0; U.bAutoAttack = false;
		if (P && !P->bDead && Dist(U, *P) < 25) { if (Chance(0.6f)) SimSay(SimIdx, TEXT("say"), TEXT("heal_passby"), 1 + Rand() * 2, TEXT("np")); Remember(SimIdx, TEXT("kind"), TEXT("helped the player out of a fight"), 0.5f); Relation(S).Familiarity++; }
		S.State = EHLSimState::Idle; S.StateUntil = Now + 3 + Rand() * 4;
	}
	// ---- trading with the player (or waiting on their answer): stand facing them
	if (P && (SimIdx == TradeSim || SimIdx == TradeFromSim) && !InCombat(U))
	{
		U.bSitting = false;
		if (Dist(U, *P) > 4) { U.MoveMult = 1; MoveTo(U, P->Pos + (U.Pos - P->Pos).GetSafeNormal() * 2.5f, 0.6f); }
		else { StopMove(U); FaceTo(U, P->Pos); }
		return;
	}
	// ---- duel with the player
	if (DuelState != 0 && SimIdx == DuelSim && P)
	{
		U.bSitting = false;
		if (DuelState == 1) { StopMove(U); FaceTo(U, P->Pos); U.bAutoAttack = false; return; }
		U.TargetId = P->Id; SimCombat(S, U); return;
	}
	// ---- party member
	if (S.bInParty && P)
	{
		if (U.Area != P->Area && (Now - S.SeenAt > 2.5f || Areas[P->Area].Def->bDungeon != Areas[U.Area].Def->bDungeon)) { TeleportUnit(U, P->Area, P->Pos + FVector2D(Rand() * 4 - 2, Rand() * 4 - 2)); S.SeenPos = P->Pos; S.SeenArea = P->Area; }
		if (Now >= S.StateUntil && !InCombat(U) && !Areas[P->Area].Def->bDungeon) { SimLeaveParty(SimIdx, true); return; }
		// pick an enemy: whatever is attacking the party, preferring the target they believe is the one (what they were
		// told, else what they last saw the player fighting: SimSee refreshes that view at their own pace)
		SimSee(S, U);
		FHLUnit* PT = Unit(S.BelTarget ? S.BelTarget : S.SeenTarget);
		if (PT && S.BelTarget && PT->MobState != EHLMobState::Combat && Now < S.BelWaitUntil) PT = nullptr; // told to wait
		FHLUnit* Enemy = nullptr;
		const bool bTank = S.P->Cls->Role == TEXT("tank");
		if (PT && !PT->bDead && PT->Kind == EHLKind::Mob && (PT->MobState == EHLMobState::Combat || S.bSeenFighting || PT->Id == S.BelTarget)) Enemy = PT;
		float BestD = 1e9f;
		for (FHLUnit* O : UnitList)
		{
			if (O->Kind != EHLKind::Mob || O->bDead || O->Area != U.Area || O->MobState != EHLMobState::Combat) continue;
			const FHLUnit* OT = Unit(O->TargetId);
			if (!OT || OwnerOf(*OT) != PlayerUnitId) continue;
			const float D = Dist(U, *O);
			if (bTank && OT->Id != U.Id) { if (!Enemy || Unit(Enemy->TargetId) == &U || D < BestD) { Enemy = O; BestD = D; } }
			else if (!Enemy) { Enemy = O; BestD = D; }
		}
		if (Enemy && P->bDead && !InCombat(U)) Enemy = nullptr;
		U.TargetId = Enemy ? Enemy->Id : 0;
		if (Enemy || InCombat(U)) { U.bSitting = false; SimCombat(S, U); if (Enemy) return; }
		U.bAutoAttack = false;
		// out of combat: heal up, follow
		if (Now >= S.NextAct && !U.Casting) { FHLUnit* AT = nullptr; if (const FHLAbility* Ab = SimPickAbility(S, U, AT)) { TryCast(U, *Ab, AT ? AT->Id : 0); S.NextAct = Now + 1; } }
		TickPartyMind(S, U);
		if (U.TargetId && Unit(U.TargetId) && !Unit(U.TargetId)->bDead) { SimCombat(S, U); return; } // their own pull
		if (Now < S.BelRestUntil) return; // sitting down, as they said
		SimFollow(S, U);
		return;
	}

	if (S.State == EHLSimState::Script) { MoveTo(U, S.Dest, 2.f); return; } // the Director set it running
	if (S.EventRole && SimEventBrain(S, U)) return;
	if (S.State == EHLSimState::Gather)
	{
		if (InCombat(U)) { U.bSitting = false; S.State = EHLSimState::Grind; S.GatherObj = -1; S.GatherEnd = 0; }
		else { SimGatherBrain(S, U); return; }
	}
	if (SimChatBrain(S, U)) return;

	// anyone attacked defends themselves (transients one-shot in passing)
	if (InCombat(U) && S.State != EHLSimState::Grind)
	{
		FHLUnit* Att = nullptr;
		for (FHLUnit* O : UnitList) if (O->Kind == EHLKind::Mob && !O->bDead && O->TargetId == U.Id && O->MobState == EHLMobState::Combat) { Att = O; break; }
		if (Att)
		{
			if (S.Role != EHLSimRole::Local && S.Level > Att->Level + 10) { Att->TappedBy = U.Id; DealDamage(&U, *Att, Att->MaxHp * 3, TEXT("physical"), true, TEXT("attack")); }
			else { U.TargetId = Att->Id; SimCombat(S, U); return; }
		}
	}

	switch (S.State)
	{
	case EHLSimState::Idle:
	{
		if (S.Role == EHLSimRole::Idler)
		{
			if (!S.bAfk && Now >= S.NextEmote) { S.NextEmote = Now + 8 + Rand() * 30; if (Chance(0.5f)) { static const TCHAR* Idle[] = { TEXT("Talk"), TEXT("Talk"), TEXT("Shrug"), TEXT("Laugh"), TEXT("Wave"), TEXT("Point"), TEXT("Cheer") }; Gesture(U, Idle[RandInt(0, 6)]); } else U.Yaw += Rand() * 120 - 60; }
			if (Now >= S.StateUntil) { S.State = EHLSimState::Leaving; S.Dest = Def.Exits[RandInt(0, Def.Exits.Num() - 1)].Pos; if (Chance(0.3f)) SimSay(SimIdx, TEXT("say"), TEXT("bye"), 0); }
			break;
		}
		if (Now < S.StateUntil) break;
		if (S.Role == EHLSimRole::Transient || S.Route.Num())
		{
			if (S.RouteIdx < S.Route.Num()) { S.Dest = S.Route[S.RouteIdx++]; S.State = EHLSimState::Travel; }
			else if (S.Role == EHLSimRole::Transient) { S.State = EHLSimState::Leaving; S.Dest = Def.Exits[RandInt(0, Def.Exits.Num() - 1)].Pos; }
			else { S.Route.Reset(); S.RouteIdx = 0; }
			break;
		}
		// local: what next? (HLMind.cpp: their own quest, a grind, an errand, a look around, company, a break)
		MindNextGoal(S, U);
		break;
	}
	case EHLSimState::Travel:
	{
		if (SimMove(S, U, S.Dest, 2.f))
		{
			if (S.Role == EHLSimRole::Local && S.CampIdx >= 0 && !S.Route.Num()) S.State = EHLSimState::Grind;
			else { S.State = EHLSimState::Idle; S.StateUntil = Now + 8 + Rand() * 30; if (Chance(0.3f)) U.HopT = 0.4f; SimArrived(S, U); }
		}
		break;
	}
	case EHLSimState::Leaving:
	{
		if (SimMove(S, U, S.Dest, 3.f)) { DespawnSim(SimIdx); return; }
		break;
	}
	case EHLSimState::Rest:
	{
		U.bSitting = true; U.bAutoAttack = false;
		if (InCombat(U)) { U.bSitting = false; S.State = EHLSimState::Grind; break; }
		if (U.HpPct() > 0.85f && (U.ResType != EHLRes::Power || U.Res > U.MaxRes * 0.6f) && Now >= S.StateUntil) { U.bSitting = false; S.State = S.CampIdx >= 0 ? EHLSimState::Travel : EHLSimState::Idle; if (S.CampIdx >= 0) S.Dest = Def.Camps[S.CampIdx].Pos + FVector2D(Rand() * 10 - 5, Rand() * 10 - 5); }
		break;
	}
	case EHLSimState::Grind:
	{
		if (S.CampIdx < 0) { S.State = EHLSimState::Idle; break; }
		const FHLCamp& Camp = Def.Camps[S.CampIdx];
		FHLUnit* T = Unit(U.TargetId);
		if (T && (T->bDead || T->Kind != EHLKind::Mob)) { T = nullptr; U.TargetId = 0; U.bAutoAttack = false; S.StateUntil = Now + 1 + Rand() * 2.5f; /* "looting" */ }
		if (!T)
		{
			if (!InCombat(U) && (U.HpPct() < 0.5f || (U.ResType == EHLRes::Power && U.Res < U.MaxRes * 0.25f))) { S.State = EHLSimState::Rest; S.StateUntil = Now + 6; break; }
			if (S.Kills >= S.KillGoal && !InCombat(U))
			{
				// done here: the mind picks what comes next (after a moment's thought)
				S.Route.Reset(); S.RouteIdx = 0; S.Kills = 0;
				S.State = EHLSimState::Idle; S.StateUntil = Now + 1 + Rand() * 3; S.CampIdx = -1;
				break;
			}
			if (Now < S.StateUntil) break;
			if (!InCombat(U) && TryStartGather(S, U)) break;
			// someone attacking me first
			FHLUnit* Best = nullptr; float BestD = 1e9f;
			for (FHLUnit* O : UnitList)
			{
				if (O->Kind != EHLKind::Mob || O->bDead || O->Area != 0 || O->Mob->bGuard) continue;
				const float D = Dist(U, *O);
				if (O->TargetId == U.Id && O->MobState == EHLMobState::Combat) { Best = O; break; }
				if (O->CampIdx != S.CampIdx && FVector2D::Distance(O->Pos, Camp.Pos) > Camp.R + 12) continue;
				if (O->TappedBy != 0 && O->TappedBy != U.Id) continue;
				if (O->Level > S.Level + 2 || O->Mob->Rank != TEXT("normal") || O->MobState == EHLMobState::Evade) continue;
				if (D < BestD) { Best = O; BestD = D; }
			}
			if (Best) { U.TargetId = Best->Id; T = Best; }
			else { if (SimMove(S, U, S.Dest, 2.f)) { S.Dest = Camp.Pos + FVector2D(Rand() * 2 - 1, Rand() * 2 - 1) * Camp.R; S.StateUntil = Now + 2 + Rand() * 4; } break; }
		}
		SimCombat(S, U);
		break;
	}
	default: break;
	}
}

void FHLGame::TickSims(float Dt)
{
	for (int32 i = 0; i < Sims.Num(); ++i)
	{
		FHLSim& S = Sims[i];
		if (!S.UnitId) continue;
		FHLUnit* U = Unit(S.UnitId);
		if (!U) { S.UnitId = 0; S.Role = EHLSimRole::Away; continue; }
		if (U->Z > 0 || U->VelZ > 0) { U->VelZ -= 20 * Dt; U->Z += U->VelZ * Dt; if (U->Z <= 0) { U->Z = 0; U->VelZ = 0; } } // their jumps (HLMotor.cpp)
		U->MoveMult = 1;
		TickSimBrain(S, *U, Dt);
	}
	if (Now < NextPopCheck) return;
	NextPopCheck = Now + 6 + Rand() * 6;
	const FHLAreaDef& Def = *Areas[0].Def;
	int32 Transients = 0, Locals = 0, Idlers = 0;
	for (const FHLSim& S : Sims) if (S.UnitId) { if (S.Role == EHLSimRole::Transient) ++Transients; else if (S.Role == EHLSimRole::Local) ++Locals; else if (S.Role == EHLSimRole::Idler) ++Idlers; }
	auto RandomAway = [&](int32 MinLevel, int32 MaxLevel) -> int32
	{
		for (int32 Try = 0; Try < 40; ++Try) { const int32 i = RandInt(0, Sims.Num() - 1); const FHLSim& S = Sims[i]; if (S.bOnline && !S.UnitId && S.DungeonUntil <= Now && S.Level >= MinLevel && S.Level <= MaxLevel && S.P->Cls) return i; }
		return -1;
	};
	// someone on a mount every couple of minutes: high levels are rare on a server this young, so the few there are
	// take turns riding through (the bible's "level 60 on a Duesenberg" moment within minutes of starting)
	bool bRider = false;
	for (const FHLSim& S : Sims) if (S.UnitId) if (const FHLUnit* U = Unit(S.UnitId)) if (U->bMounted) bRider = true;
	static double NextRider = 0;
	const bool bWantRider = !bRider && Now >= NextRider;
	if (Transients < MAX_TRANSIENTS && (bWantRider || Chance(0.6f)))
	{
		int32 i = Chance(0.15f) ? BondedAwaySim(13, 60) : -1;
		if (bWantRider) { i = RandomAway(40, 60); NextRider = Now + 100 + Rand() * 70; }
		if (i < 0) i = RandomAway(13, 60);
		if (i >= 0)
		{
			int32 ExIdx = RandInt(0, Def.Exits.Num() - 1);
			if (bWantRider) for (int32 Try = 0; Try < 8 && Def.Exits[ExIdx].bRedcar; ++Try) ExIdx = RandInt(0, Def.Exits.Num() - 1);
			const FHLExit& Ex = Def.Exits[ExIdx];
			FHLUnit* U = SpawnSim(i, EHLSimRole::Transient, Ex.Pos + FVector2D(Rand() * 4 - 2, Rand() * 4 - 2));
			FHLSim& S = Sims[i];
			if (U && !S.P->Mount.IsEmpty() && !Ex.bRedcar) { U->bMounted = true; U->MountKind = HLMountKind(S.P->Mount); }
			static const TCHAR* Errands[] = { TEXT("bank"), TEXT("auction"), TEXT("trainers"), TEXT("vendor"), TEXT("mailbox"), TEXT("inn") };
			TArray<FString> Pick = { TEXT("bank"), TEXT("auction") };
			Pick.Add(Errands[RandInt(2, 5)]);
			if (Chance(0.5f)) Pick.Swap(0, 1);
			for (const FString& K : Pick) if (const FVector2D* Poi = Def.Pois.Find(K)) if (Chance(0.8f)) S.Route.Add(ErrandSpot(K, S, *Poi));
			S.StateUntil = Now;
		}
	}
	if (Locals < MAX_LOCALS - 4 && Chance(0.3f))
	{
		int32 i = Chance(0.35f) ? BondedAwaySim(1, 14) : -1;
		if (i < 0) i = RandomAway(1, 12);
		if (i >= 0) { SpawnSim(i, EHLSimRole::Local, *Def.Pois.Find(TEXT("inn")) + FVector2D(Rand() * 6 - 3, -Rand() * 4)); if (Chance(0.3f)) SimSay(i, TEXT("zone"), TEXT("greet"), 2); }
	}
	if (Idlers < MAX_IDLERS && Chance(0.1f))
	{
		const int32 i = RandomAway(13, 60);
		if (i >= 0) { SpawnSim(i, EHLSimRole::Transient, Def.Exits[RandInt(0, Def.Exits.Num() - 1)].Pos); Sims[i].Route.Add(Def.IdleSpots[RandInt(0, Def.IdleSpots.Num() - 1)] + FVector2D(Rand() * 4 - 2, Rand() * 4 - 2)); Sims[i].Role = EHLSimRole::Transient; }
	}
}

// ------------------------------------------------------------------------------------------------------------- census
void FHLGame::TickCensus()
{
	if (Now < NextCensus || !Sims.Num()) return;
	NextCensus = Now + 5;
	const FHLData& D = FHLData::Get();
	auto Event = [&](const FString& T) { if (CensusEvents.Num() >= 16) CensusEvents.RemoveAt(0); CensusEvents.Add({ T, Now }); };
	for (int32 n = 0; n < 60; ++n)
	{
		CensusCursor = (CensusCursor + 1) % Sims.Num();
		FHLSim& S = Sims[CensusCursor];
		if (S.UnitId || S.DungeonUntil > Now) continue;
		if (!S.bOnline) { if (Chance(0.01f)) { S.bOnline = true; if (IsFriend(S)) System(FString::Printf(TEXT("%s has come online."), *S.P->Handle)); } continue; }
		if (Chance(0.004f)) { S.bOnline = false; if (IsFriend(S)) System(FString::Printf(TEXT("%s has gone offline."), *S.P->Handle)); continue; }
		const FHLGuild* G = D.Guild(S.P->Guild);
		// questing / grinding off-screen
		if (S.Level < 60 && Chance(0.004f))
		{
			S.Level++;
			if (S.Level % 10 == 0) Event(FString::Printf(TEXT("%s reached level %d"), *S.P->Handle, S.Level));
			for (const FHLZoneInfo& Z : D.Zones) if (S.Level >= Z.LevelMin && S.Level <= Z.LevelMax && Chance(0.5f)) { S.Zone = Z.Id; break; }
		}
		// dungeons and raids as dice
		if (Chance(0.012f))
		{
			TArray<const FHLDungeonInfo*> Fit;
			for (const FHLDungeonInfo& Dn : D.Dungeons) if (!Dn.bBuilt && S.Level >= Dn.LevelMin && S.Level <= Dn.LevelMax + 3) Fit.Add(&Dn);
			if (Fit.Num())
			{
				const FHLDungeonInfo& Dn = *Fit[RandInt(0, Fit.Num() - 1)];
				S.Activity = FString::Printf(TEXT("in %s"), *Dn.Name); S.Zone = Dn.Zone;
				const float R = Rand();
				const FString& Boss = Dn.Bosses[RandInt(0, Dn.Bosses.Num() - 1)];
				if (R < 0.3f) { Event(G ? FString::Printf(TEXT("<%s> cleared %s"), *G->Name, *Dn.Name) : FString::Printf(TEXT("%s's group cleared %s"), *S.P->Handle, *Dn.Name)); GuildEvent(TEXT("clear"), FString::Printf(TEXT("%d|%s"), CensusCursor, *Dn.Name)); }
				else if (R < 0.55f) Event(FString::Printf(TEXT("%s's group wiped on %s in %s"), *S.P->Handle, *Boss, *Dn.Name));
				else if (R < 0.8f)
				{
					const TArray<const FHLItem*>& Pool = D.EquipPool(Dn.LevelMin, Dn.Size > 5 ? 4 : 3);
					if (Pool.Num()) Event(FString::Printf(TEXT("%s looted [%s] from %s in %s"), *S.P->Handle, *Pool[RandInt(0, Pool.Num() - 1)]->Name, *Boss, *Dn.Name));
				}
			}
		}
		else if (Chance(0.02f)) { S.Activity.Reset(); }
	}
}

// ------------------------------------------------------------------------------------------------------------- party
void FHLGame::SimJoinParty(int32 SimIdx)
{
	FHLSim& S = Sims[SimIdx];
	if (S.bInParty || Party.Num() >= 4 || !S.UnitId) return;
	Party.Add(SimIdx); S.bInParty = true; S.State = EHLSimState::Party; S.StateUntil = Now + 900 + Rand() * 1500; S.Route.Reset();
	if (FHLUnit* U = Unit(S.UnitId)) { U->TargetId = 0; U->bAutoAttack = false; U->bSitting = false; }
	System(FString::Printf(TEXT("%s joins the party."), *S.P->Handle));
	FHLRelation& R = Relation(S);
	R.Familiarity += 2;
	Remember(SimIdx, TEXT("party"), FString::Printf(TEXT("grouped with the player in %s"), *CurSubzone), 0.5f);
	S.FollowDist = 0; S.BelTarget = 0; S.BelWaitUntil = S.BelRestUntil = 0; S.Frust = 0; S.SeenTarget = 0; S.NextSee = 0; S.TightUntil = Now + 6; S.NextLost = Now + 40;
	SimSay(SimIdx, TEXT("party"), TEXT("party_join"), 1 + Rand() * 2);
}

void FHLGame::SimLeaveParty(int32 SimIdx, bool bSay)
{
	FHLSim& S = Sims[SimIdx];
	if (!S.bInParty) return;
	if (bSay) AddChat(TEXT("party"), S.P->Handle, StyleText(*S.P, CorpusLine(S, TEXT("party_leave"))), SimIdx);
	Party.Remove(SimIdx); S.bInParty = false; S.State = EHLSimState::Idle; S.StateUntil = Now + 3; S.CampIdx = -1;
	System(FString::Printf(TEXT("%s leaves the party."), *S.P->Handle));
	if (FHLUnit* U = Unit(S.UnitId)) { U->TargetId = 0; U->bAutoAttack = false; if (U->Area != 0) TeleportUnit(*U, 0, *Areas[0].Def->Pois.Find(TEXT("forecourt"))); }
}

void FHLGame::LeaveParty()
{
	const TArray<int32> Copy = Party;
	for (int32 SI : Copy) SimLeaveParty(SI, false);
	System(TEXT("You leave the party."));
}

void FHLGame::AnswerInvite(bool bAccept)
{
	const int32 SI = InviteFromSim;
	InviteFromSim = -1;
	if (SI < 0 || !Sims.IsValidIndex(SI)) return;
	if (bAccept) SimJoinParty(SI);
	else System(FString::Printf(TEXT("You decline %s's invitation."), *Sims[SI].P->Handle));
}

void FHLGame::InviteSim(int32 SimIdx)
{
	if (!Sims.IsValidIndex(SimIdx)) return;
	FHLSim& S = Sims[SimIdx];
	FHLUnit* P = PlayerUnit();
	if (!P) return;
	if (S.bInParty) { Error(TEXT("Already in your party.")); return; }
	if (Party.Num() >= 4) { Error(TEXT("Your party is full.")); return; }
	if (!S.bOnline) { System(FString::Printf(TEXT("No player named %s is currently playing."), *S.P->Handle)); return; }
	System(FString::Printf(TEXT("You have invited %s to join your group."), *S.P->Handle));
	Relation(S).Familiarity++;
	// Facade guardrails are enforced locally whatever the model says: only locals near the player's level can join.
	const bool bEligible = S.UnitId && S.Role == EHLSimRole::Local && FMath::Abs(S.Level - P->Level) <= 5 && S.State != EHLSimState::Script;
	auto Resolve = [this, SimIdx](bool bAccept, bool bSpoke)
	{
		FHLSim& S2 = Sims[SimIdx];
		if (bAccept && S2.UnitId && !S2.bInParty && Party.Num() < 4) SimJoinParty(SimIdx);
		else
		{
			System(FString::Printf(TEXT("%s declines your group invitation."), *S2.P->Handle));
			if (!bSpoke && Chance(0.7f)) SimSay(SimIdx, TEXT("whisper"), S2.Level > (PlayerUnit() ? PlayerUnit()->Level : 1) + 5 ? TEXT("decline_low") : TEXT("decline_busy"), 1 + Rand() * 2);
		}
	};
	const float Base = S.P->Temperament == TEXT("helpful") ? 0.92f : S.P->Temperament == TEXT("quiet") ? 0.6f : S.P->Temperament == TEXT("mercenary") ? 0.5f : 0.75f;
	const bool bLocalRule = bEligible && Chance(FMath::Clamp(Base + 0.08f * Relation(S).Liking, 0.1f, 0.98f));
	TArray<int32> Sp = { SimIdx };
	WorkerSim(TEXT("decide"), Sp, TEXT("invite"), TEXT("whisper"), CurSubzone, {}, [this, SimIdx, bEligible, bLocalRule, Resolve, Sp](bool bOk, const TSharedPtr<FJsonObject>& Resp)
	{
		bool bAccept = bLocalRule, bSpoke = false;
		if (bOk && Resp.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Acts = nullptr;
			if (Resp->TryGetArrayField(TEXT("actions"), Acts)) for (auto& V : *Acts)
			{
				const TSharedPtr<FJsonObject> A = V->AsObject();
				FString Dec;
				if (A.IsValid() && A->TryGetStringField(TEXT("decision"), Dec) && !Dec.IsEmpty()) bAccept = bEligible && Dec == TEXT("accept");
				const TSharedPtr<FJsonObject>* ChatObj = nullptr;
				if (A.IsValid() && A->TryGetObjectField(TEXT("chat"), ChatObj)) bSpoke = true;
			}
			// A model line that says yes while the local guardrail says no would contradict the outcome: drop it.
			if (bEligible || !bSpoke) ApplyWorkerActions(Resp, bAccept ? TEXT("party") : TEXT("whisper"), Sp); else bSpoke = false;
		}
		Resolve(bAccept, bSpoke);
	});
}

void FHLGame::PartyEvent(const FString& What)
{
	if (!Party.Num()) return;
	TArray<int32> Sp;
	for (int32 SI : Party) if (Sp.Num() < 3 && Sims[SI].UnitId) Sp.Add(SI);
	const FString Situation = What.Contains(TEXT("died")) ? TEXT("wipe") : TEXT("boss_kill");
	// shared history, and tempers: a wipe wears on everyone, a kill mends it
	for (int32 SI : Party)
	{
		Remember(SI, Situation == TEXT("wipe") ? TEXT("wipe") : TEXT("party"), What.Left(120), 0.55f);
		if (Situation == TEXT("wipe")) PartyFrustrate(SI, 0.9f, TEXT("the group wiped")); else Sims[SI].Frust = FMath::Max(0.f, Sims[SI].Frust - 2.f);
	}
	// a boss kill is a moment worth a written line (the last boss always; others half the time); deaths and wipes are routine
	const bool bMajor = Situation == TEXT("boss_kill") && (What.Contains(TEXT("Hive Mother")) || Chance(0.5f));
	if (!bWorkerOk || !bMajor) { for (int32 SI : Sp) if (Chance(0.55f)) SimSay(SI, TEXT("party"), Situation, 1.5f + Rand() * 4); return; }
	WorkerSim(TEXT("reply"), Sp, TEXT("event"), TEXT("party"), What, {}, [this, Sp, Situation](bool bOk, const TSharedPtr<FJsonObject>& Resp)
	{
		if (bOk && Resp.IsValid()) ApplyWorkerActions(Resp, TEXT("party"), Sp);
		else for (int32 SI : Sp) if (Chance(0.6f)) SimSay(SI, TEXT("party"), Situation, 1.5f + Rand() * 4);
	});
}

// ------------------------------------------------------------------------------------------------------------- director
void FHLGame::TickDirector(float Dt)
{
	FHLUnit* P = PlayerUnit();
	if (!P || Now < NextDirector) return;
	NextDirector = Now + 2;
	const FHLAreaDef& Def = *Areas[0].Def;
	const double T = Player.PlayedS;
	auto NearestLocal = [&](TFunctionRef<bool(const FHLSim&)> Pred, float MaxD) -> int32
	{
		int32 Best = -1; float BestD = MaxD;
		for (int32 i = 0; i < Sims.Num(); ++i)
		{
			const FHLSim& S = Sims[i];
			if (!S.UnitId || S.Role != EHLSimRole::Local || S.bInParty || S.State == EHLSimState::Script || !Pred(S)) continue;
			const FHLUnit* U = Unit(S.UnitId);
			if (!U || U->bDead || U->Area != P->Area) continue;
			const float D = Dist(*P, *U);
			if (D < BestD) { Best = i; BestD = D; }
		}
		return Best;
	};
	// Scripted train: drive the runner
	for (int32 i = 0; i < Sims.Num(); ++i)
	{
		FHLSim& S = Sims[i];
		if (S.State != EHLSimState::Script || !S.UnitId) continue;
		FHLUnit* U = Unit(S.UnitId);
		if (!U) continue;
		if (U->bDead) { if (Now - U->DeadAt > 6) { S.State = EHLSimState::Dead; S.StateUntil = Now + 10; } continue; }
		if (Now >= S.StateUntil) { S.State = EHLSimState::Idle; S.StateUntil = Now + 5; continue; }
	}
	// the lost newbie wanders off once they have their answer (or give up waiting)
	if (LostSim >= 0 && Now >= LostUntil)
	{
		if (Sims.IsValidIndex(LostSim) && Sims[LostSim].UnitId && Sims[LostSim].State == EHLSimState::Script) { SimSay(LostSim, TEXT("say"), TEXT("dir_lost_bye"), 0.5f, TEXT("nvm found it, ty")); Sims[LostSim].State = EHLSimState::Idle; Sims[LostSim].StateUntil = Now + 3; }
		LostSim = -1;
	}
	if (P->Area != 0 || P->bDead) return;
	if (InOpening()) { DirBeatAt = T; return; } // the Director waits for the tour to finish
	if (!DirBeats.Num())
	{
		// a fresh running order for this play: the invite opens and the raiders and the buddy close; the middle is shuffled
		TArray<int32> Mid = { 1, 2, 3 };
		if (Chance(0.4f)) Mid.Remove(Chance(0.5f) ? 2 : 3);
		for (int32 k = Mid.Num() - 1; k > 0; --k) Mid.Swap(k, RandInt(0, k));
		DirBeats.Add(0); DirBeats.Append(Mid); DirBeats.Add(4); DirBeats.Add(5);
		DirPace = 0.75f + Rand() * 0.5f; DirBeatAt = T;
		FString Ord; for (int32 B : DirBeats) Ord += FString::Printf(TEXT("%d "), B);
		UE_LOG(LogTemp, Display, TEXT("HL_LIFE director order %s pace %.2f"), *Ord, DirPace);
	}
	if (!DirBeats.IsValidIndex(DirectorStep)) return;
	const int32 Beat = DirBeats[DirectorStep];
	static const float Gap[] = { 40, 100, 200, 120, 150, 0 };
	const double Since = T - DirBeatAt, Ready = Gap[Beat] * DirPace;
	auto Next = [&](const TCHAR* What) { UE_LOG(LogTemp, Display, TEXT("HL_LIFE director beat %d %s at %.0f s"), Beat, What, T); ++DirectorStep; DirBeatAt = T; };
	if (Since < Ready) return;
	const bool bGiveUp = Since > Ready + 300;
	switch (Beat)
	{
	case 0: // a local doing the same first quest invites the player
		if (P->Level <= 6 && !Party.Num() && InviteFromSim < 0 && !InCombat(*P))
		{
			const int32 i = NearestLocal([&](const FHLSim& S) { return FMath::Abs(S.Level - P->Level) <= 2 && S.P->Temperament != TEXT("quiet"); }, 45);
			if (i >= 0)
			{
				BuddySim = i; InviteFromSim = i; Sound2D(TEXT("ui_invite")); InviteUntil = Now + 40;
				SimSay(i, TEXT("whisper"), P->Level <= 1 ? TEXT("dir_invite_new") : TEXT("dir_invite"), 0, TEXT("want to group? faster together"));
				LastWhisperFrom = Sims[i].P->Handle;
				System(FString::Printf(TEXT("%s has invited you to join a group."), *Sims[i].P->Handle));
				Next(TEXT("invite"));
			}
				else if (Since > Ready + 150) Next(TEXT("skipped"));
			}
			else if (Since > Ready + 150 || P->Level > 3) Next(TEXT("skipped"));
		break;
	case 1: // a stranger heals the player in passing
		if (P->HpPct() < 0.75f && !InCombat(*P))
		{
			const int32 i = NearestLocal([](const FHLSim& S) { return S.P->Cls->Role == TEXT("healer"); }, 60);
			if (i >= 0)
			{
				FHLUnit* U = Unit(Sims[i].UnitId);
				HealUnit(U, *P, P->MaxHp * 0.6f, TEXT("Dress Wound"));
				AddFloat(U->Id, TEXT("Dress Wound"), FLinearColor(0.6f, 1, 0.6f));
				SimSay(i, TEXT("say"), TEXT("heal_passby"), 1.5f);
				Relation(Sims[i]).Familiarity++;
				Next(TEXT("heal"));
			}
		}
		if (bGiveUp && DirBeats[DirectorStep] == 1) Next(TEXT("skipped"));
		break;
	case 2: // someone trains a pack of Drones through the forecourt and dies
	{
		const FVector2D Forecourt = *Def.Pois.Find(TEXT("forecourt"));
		if (FVector2D::Distance(P->Pos, Forecourt) < 90)
		{
			const int32 i = NearestLocal([&](const FHLSim& S) { return S.Level >= 3 && int32(&S - Sims.GetData()) != BuddySim; }, 400);
			const FHLMobDef* Drone = FHLData::Get().Mob(TEXT("mob_invader_drone"));
			if (i >= 0 && Drone)
			{
				FHLSim& S = Sims[i];
				FHLUnit* U = Unit(S.UnitId);
				const FVector2D Start(148, 150);
				TeleportUnit(*U, 0, Start);
				U->Hp = U->MaxHp * 0.45f; U->TargetId = 0; U->bAutoAttack = false; U->bSitting = false;
				S.State = EHLSimState::Script; S.StateUntil = Now + 40;
				const int32 Pack = RandInt(3, 5);
				for (int32 k = 0; k < Pack; ++k)
				{
					FHLUnit* M = SpawnMob(Drone, 0, Start + FVector2D(4 + k * 2, -8 - k * 2));
					M->bEvent = true; M->Leash = 400; M->TappedBy = U->Id; M->Level = S.Level + 2; InitMobStats(*M);
					MobEnterCombat(*M, U->Id); M->Threat.Add(U->Id, 100000);
				}
				S.Dest = Forecourt + FVector2D(Rand() * 10 - 5, Rand() * 6 - 3); StopMove(*U);
				SimSay(i, TEXT("yell"), TEXT("train"), 0.5f);
				SimSay(i, TEXT("zone"), TEXT("train"), 6);
				Next(TEXT("train"));
				break;
			}
		}
		if (bGiveUp) Next(TEXT("skipped"));
		break;
	}
	case 3: // a lost newbie walks up and asks the player the way to their trainer
	{
		if (InCombat(*P)) { if (bGiveUp) Next(TEXT("skipped")); break; }
		const int32 i = NearestLocal([&](const FHLSim& S) { return S.Level <= FMath::Max(2, P->Level) && int32(&S - Sims.GetData()) != BuddySim && S.P->Temperament != TEXT("quiet"); }, 120);
		TArray<const FHLNpcDef*> Far;
		for (const FHLNpcDef& N : Def.Npcs) if (!N.Trains.IsEmpty() && FVector2D::Distance(N.Pos, P->Pos) > 40) Far.Add(&N);
		if (i >= 0 && Far.Num())
		{
			FHLSim& S = Sims[i];
			const FHLNpcDef* N = Far[RandInt(0, Far.Num() - 1)];
			for (const FHLNpcDef* F : Far) if (F->Trains == S.P->Cls->Id) N = F; // their own trainer if it is far enough
			S.State = EHLSimState::Script; S.StateUntil = Now + 60;
			S.Dest = P->Pos + FVector2D(Rand() * 4 - 2, Rand() * 4 - 2);
			ChatCtx = N->Name;
			SimSay(i, TEXT("say"), TEXT("dir_lost"), 6 + Rand() * 4, FString::Printf(TEXT("hey do you know where %s is?"), *N->Name));
			ChatCtx.Reset();
			LostSim = i; LostUntil = Now + 45 + Rand() * 15;
			Next(TEXT("lost"));
		}
		else if (bGiveUp) Next(TEXT("skipped"));
		break;
	}
	case 4: // raid talk and a red car full of raiders
	{
		const FHLExit* Car = Def.Exits.FindByPredicate([](const FHLExit& E) { return E.bRedcar; });
		int32 Made = 0;
		for (int32 i = 0; i < Sims.Num() && Made < RandInt(3, 5) && Car; ++i)
		{
			FHLSim& S = Sims[i];
			if (S.UnitId || !S.bOnline || S.Level < 40) continue;
			SpawnSim(i, EHLSimRole::Transient, Car->Pos + FVector2D(Made * 2 - 3, 4 + Rand() * 2));
			if (const FVector2D* Bank = Def.Pois.Find(TEXT("bank"))) S.Route.Add(*Bank + FVector2D(Rand() * 8 - 4, Rand() * 3));
			if (const FVector2D* Auc = Def.Pois.Find(TEXT("auction"))) S.Route.Add(*Auc + FVector2D(Rand() * 8 - 4, Rand() * 3));
			S.StateUntil = Now + 2 + Made;
			if (Made == 0)
			{
				const FHLGuild* G = FHLData::Get().Guild(S.P->Guild);
				ChatCtx = G ? G->Name : FString(TEXT("guild"));
				SimSay(i, TEXT("zone"), TEXT("dir_raid"), 4, FString::Printf(TEXT("<%s> raid tonight Mount Lee 8pm server, need 2 heals"), *ChatCtx));
				ChatCtx.Reset();
			}
			++Made;
		}
		AddChat(TEXT("emote"), FString(), TEXT("A Pacific Electric red car pulls in at Highland."));
		Next(TEXT("raid"));
		break;
	}
	case 5: // the friend from the invite asks about the Egyptian
		if (P->Level >= 5 && BuddySim >= 0 && Sims[BuddySim].bOnline)
		{
			SimSay(BuddySim, TEXT("whisper"), TEXT("dir_buddy"), 3, TEXT("hey you want to do egyptian when we hit 8? add me, ill tank or dps whatever"));
			LastWhisperFrom = Sims[BuddySim].P->Handle;
			Relation(Sims[BuddySim]).Liking++;
			Next(TEXT("buddy"));
		}
		else if (P->Level >= 5) Next(TEXT("skipped"));
		break;
	default: break;
	}
	if (InviteFromSim >= 0 && Now > InviteUntil) InviteFromSim = -1;
}
