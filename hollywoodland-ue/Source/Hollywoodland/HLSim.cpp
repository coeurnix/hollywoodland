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
		if (S.bInParty) return TEXT("in your party");
		if (S.Role == EHLSimRole::Idler) return S.bAfk ? TEXT("AFK on Hollywood Boulevard") : TEXT("hanging around Hollywood Boulevard");
		if (S.Role == EHLSimRole::Transient) return TEXT("running errands on Hollywood Boulevard (bank, auction house)");
		if (S.State == EHLSimState::Grind || S.State == EHLSimState::Rest) return TEXT("questing on Hollywood Boulevard");
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
		else if (S.Level >= 30 && Idlers < MAX_IDLERS && Chance(0.08f))
		{
			++Idlers;
			FHLUnit* U = SpawnSim(i, EHLSimRole::Idler, Def.IdleSpots[RandInt(0, Def.IdleSpots.Num() - 1)] + FVector2D(Rand() * 5 - 2.5f, Rand() * 4 - 2));
			S.bAfk = Chance(0.4f); S.StateUntil = Now + 200 + Rand() * 900;
			if (U && !S.P->Mount.IsEmpty() && Chance(0.5f)) U->bMounted = true;
		}
	}
	NextCensus = Now + 5; NextPopCheck = Now + 8; NextDirector = Now + 5;
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
			if (Pct > 0.72f) continue;
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
	if (Now >= S.NextAct && Now >= U.GcdUntil && !U.Casting && !U.Stunned())
	{
		FHLUnit* AT = nullptr;
		if (const FHLAbility* Ab = SimPickAbility(S, U, AT))
		{
			if (Chance(S.P->MistakeRate)) S.NextAct = Now + 1.5f + Rand() * 2; // a mistake: hesitates
			else { TryCast(U, *Ab, AT ? AT->Id : 0); S.NextAct = Now + S.P->ReactionS * (0.6f + Rand() * 0.8f); }
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
		if (U.Area != P->Area) { TeleportUnit(U, P->Area, P->Pos + FVector2D(Rand() * 4 - 2, Rand() * 4 - 2)); }
		if (Now >= S.StateUntil && !InCombat(U) && !Areas[P->Area].Def->bDungeon) { SimLeaveParty(SimIdx, true); return; }
		// pick an enemy: whatever is attacking the party, preferring the player's target
		FHLUnit* PT = Unit(P->TargetId);
		FHLUnit* Enemy = nullptr;
		const bool bTank = S.P->Cls->Role == TEXT("tank");
		if (PT && !PT->bDead && PT->Kind == EHLKind::Mob && (PT->MobState == EHLMobState::Combat || P->bAutoAttack || P->Casting)) Enemy = PT;
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
		const int32 Slot = Party.IndexOfByKey(SimIdx);
		const float Rad = FMath::DegreesToRadians(P->Yaw + 140 + Slot * 27);
		const FVector2D Spot = P->Pos + FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)) * (3.f + Slot * 0.6f);
		if (Dist(U, *P) > 50) TeleportUnit(U, P->Area, Spot);
		else if (FVector2D::Distance(U.Pos, Spot) > 3.5f && !U.Casting) MoveTo(U, Spot, 1.f);
		else if (P->bSitting && U.HpPct() < 0.9f) U.bSitting = true;
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
			if (!S.bAfk && Now >= S.NextEmote) { S.NextEmote = Now + 8 + Rand() * 30; if (Chance(0.5f)) U.HopT = 0.5f; else U.Yaw += Rand() * 120 - 60; }
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
		// local: go and grind
		S.CampIdx = PickCampFor(S.Level); S.Kills = 0; S.KillGoal = RandInt(6, 16);
		S.Dest = Def.Camps[S.CampIdx].Pos + FVector2D(Rand() * 10 - 5, Rand() * 10 - 5);
		S.State = EHLSimState::Travel;
		break;
	}
	case EHLSimState::Travel:
	{
		if (MoveTo(U, S.Dest, 2.f))
		{
			if (S.Role == EHLSimRole::Local && S.CampIdx >= 0 && !S.Route.Num()) S.State = EHLSimState::Grind;
			else { S.State = EHLSimState::Idle; S.StateUntil = Now + 8 + Rand() * 30; if (Chance(0.3f)) U.HopT = 0.4f; SimArrived(S, U); }
		}
		break;
	}
	case EHLSimState::Leaving:
	{
		if (MoveTo(U, S.Dest, 3.f)) { DespawnSim(SimIdx); return; }
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
				// back to town: vendor/trainer, then out again
				static const TCHAR* Stops[] = { TEXT("vendor"), TEXT("trainers"), TEXT("inn"), TEXT("mailbox"), TEXT("auction"), TEXT("forecourt") };
				S.Route.Reset(); S.RouteIdx = 0; S.Kills = 0;
				const FString Stop = Stops[RandInt(0, 5)];
				if (const FVector2D* Poi = Def.Pois.Find(Stop)) S.Route.Add(Stop == TEXT("forecourt") ? *Poi + FVector2D(Rand() * 6 - 3, Rand() * 4 - 1) : ErrandSpot(Stop, S, *Poi));
				S.State = EHLSimState::Idle; S.StateUntil = Now; S.CampIdx = -1;
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
			else { if (MoveTo(U, S.Dest, 2.f)) { S.Dest = Camp.Pos + FVector2D(Rand() * 2 - 1, Rand() * 2 - 1) * Camp.R; S.StateUntil = Now + 2 + Rand() * 4; } break; }
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
	if (Transients < MAX_TRANSIENTS && Chance(0.6f))
	{
		int32 i = Chance(0.15f) ? BondedAwaySim(13, 60) : -1;
		if (i < 0) i = RandomAway(13, 60);
		if (i >= 0)
		{
			const FHLExit& Ex = Def.Exits[RandInt(0, Def.Exits.Num() - 1)];
			FHLUnit* U = SpawnSim(i, EHLSimRole::Transient, Ex.Pos + FVector2D(Rand() * 4 - 2, Rand() * 4 - 2));
			FHLSim& S = Sims[i];
			if (U && !S.P->Mount.IsEmpty() && !Ex.bRedcar) U->bMounted = true;
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
		const int32 i = RandomAway(20, 60);
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
	if (R.Facts.Num() < 20 && !R.Facts.ContainsByPredicate([](const FString& F) { return F.StartsWith(TEXT("grouped")); })) R.Facts.Add(FString::Printf(TEXT("grouped with the player in %s"), *CurSubzone));
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
	if (P->Area != 0 || P->bDead) return;
	switch (DirectorStep)
	{
	case 0: // a local doing the same first quest invites the player
		if (T > 40 && P->Level <= 3 && !Party.Num() && InviteFromSim < 0 && !InCombat(*P))
		{
			const int32 i = NearestLocal([&](const FHLSim& S) { return FMath::Abs(S.Level - P->Level) <= 2 && S.P->Temperament != TEXT("quiet"); }, 45);
			if (i >= 0)
			{
				BuddySim = i; InviteFromSim = i; InviteUntil = Now + 40;
				AddChat(TEXT("whisper"), Sims[i].P->Handle, StyleText(*Sims[i].P, P->Level <= 1 ? TEXT("doing the worker quest too? inv, faster together") : TEXT("want to group? faster together")), i);
				LastWhisperFrom = Sims[i].P->Handle;
				System(FString::Printf(TEXT("%s has invited you to join a group."), *Sims[i].P->Handle));
				DirectorStep = 1;
			}
			else if (T > 150) DirectorStep = 1;
		}
		break;
	case 1: // a stranger heals the player in passing
		if (T > 150 && P->HpPct() < 0.75f && !InCombat(*P))
		{
			const int32 i = NearestLocal([](const FHLSim& S) { return S.P->Cls->Role == TEXT("healer"); }, 60);
			if (i >= 0)
			{
				FHLUnit* U = Unit(Sims[i].UnitId);
				HealUnit(U, *P, P->MaxHp * 0.6f, TEXT("Dress Wound"));
				AddFloat(U->Id, TEXT("Dress Wound"), FLinearColor(0.6f, 1, 0.6f));
				SimSay(i, TEXT("say"), TEXT("heal_passby"), 1.5f);
				Relation(Sims[i]).Familiarity++;
				DirectorStep = 2;
			}
			else if (T > 420) DirectorStep = 2;
		}
		else if (T > 480) DirectorStep = 2;
		break;
	case 2: // someone trains a pack of Drones through the forecourt and dies
		if (T > 420)
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
					for (int32 k = 0; k < 4; ++k)
					{
						FHLUnit* M = SpawnMob(Drone, 0, Start + FVector2D(4 + k * 2, -8 - k * 2));
						M->bEvent = true; M->Leash = 400; M->TappedBy = U->Id; M->Level = S.Level + 2; InitMobStats(*M);
						MobEnterCombat(*M, U->Id); M->Threat.Add(U->Id, 100000);
					}
					S.Dest = Forecourt + FVector2D(4, 2); StopMove(*U);
					SimSay(i, TEXT("yell"), TEXT("train"), 0.5f);
					SimSay(i, TEXT("zone"), TEXT("train"), 6);
					DirectorStep = 3;
				}
			}
			else if (T > 700) DirectorStep = 3;
		}
		break;
	case 3: // raid talk and a red car full of raiders
		if (T > 620)
		{
			const FHLExit* Car = Def.Exits.FindByPredicate([](const FHLExit& E) { return E.bRedcar; });
			int32 Made = 0;
			for (int32 i = 0; i < Sims.Num() && Made < 4 && Car; ++i)
			{
				FHLSim& S = Sims[i];
				if (S.UnitId || !S.bOnline || S.Level < 60 || S.P->Guild.IsEmpty()) continue;
				SpawnSim(i, EHLSimRole::Transient, Car->Pos + FVector2D(Made * 2 - 3, 4 + Rand() * 2));
				if (const FVector2D* Bank = Def.Pois.Find(TEXT("bank"))) S.Route.Add(*Bank + FVector2D(Rand() * 8 - 4, Rand() * 3));
				if (const FVector2D* Auc = Def.Pois.Find(TEXT("auction"))) S.Route.Add(*Auc + FVector2D(Rand() * 8 - 4, Rand() * 3));
				S.StateUntil = Now + 2 + Made;
				if (Made == 0) { const FHLGuild* G = FHLData::Get().Guild(S.P->Guild); AddChat(TEXT("zone"), S.P->Handle, StyleText(*S.P, FString::Printf(TEXT("<%s> raid tonight Mount Lee 8pm server, need 2 heals"), G ? *G->Name : TEXT("guild"))), i, 4); }
				++Made;
			}
			AddChat(TEXT("emote"), FString(), TEXT("A Pacific Electric red car pulls in at Highland."));
			DirectorStep = 4;
		}
		break;
	case 4: // the friend from step 0 asks about the Egyptian
		if (P->Level >= 5 && BuddySim >= 0 && Sims[BuddySim].bOnline)
		{
			AddChat(TEXT("whisper"), Sims[BuddySim].P->Handle, StyleText(*Sims[BuddySim].P, TEXT("hey you want to do egyptian when we hit 8? add me, ill tank or dps whatever")), BuddySim, 3);
			LastWhisperFrom = Sims[BuddySim].P->Handle;
			Relation(Sims[BuddySim]).Liking++;
			DirectorStep = 5;
		}
		else if (P->Level >= 5) DirectorStep = 5;
		break;
	default: break;
	}
	if (InviteFromSim >= 0 && Now > InviteUntil) InviteFromSim = -1;
}
