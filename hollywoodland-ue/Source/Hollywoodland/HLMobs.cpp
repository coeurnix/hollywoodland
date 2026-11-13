// Mob AI (aggro, threat, leash, social pulls, flee), camps and respawns, boss scripts, loot, world objects.
#include "HLGame.h"

void FHLGame::MobEnterCombat(FHLUnit& U, int32 TargetId)
{
	if (U.bDead || U.MobState == EHLMobState::Evade) return;
	const bool bWasIdle = U.MobState == EHLMobState::Idle;
	U.MobState = EHLMobState::Combat;
	U.MoveMult = 1;
	if (!U.Threat.Contains(TargetId)) U.Threat.Add(TargetId, 1);
	if (bWasIdle)
	{
		for (int32 k = 0; k < 3; ++k) U.ScriptT[k] = Now + 6 + k * 4;
		// social pull: idle neighbours of the same family join
		if (U.Mob->Social > 0)
		{
			TArray<FHLUnit*> Join;
			for (FHLUnit* O : UnitList)
				if (O != &U && O->Kind == EHLKind::Mob && !O->bDead && O->Area == U.Area && O->MobState == EHLMobState::Idle && O->Mob->bAggressive && O->Mob->Social > 0 &&
					FVector2D::Distance(O->Pos, U.Pos) <= U.Mob->Social) Join.Add(O);
			for (FHLUnit* O : Join) { O->MobState = EHLMobState::Combat; O->MoveMult = 1; O->Threat.FindOrAdd(TargetId) += 1; for (int32 k = 0; k < 3; ++k) O->ScriptT[k] = Now + 6 + k * 4; }
		}
	}
}

void FHLGame::MobEvade(FHLUnit& U)
{
	U.MobState = EHLMobState::Evade;
	U.Threat.Reset(); U.TargetId = 0; U.bAutoAttack = false; U.Casting = nullptr; U.Auras.Reset(); U.TappedBy = 0; U.ScriptPhase = 0;
	StopMove(U);
}

void FHLGame::TickMob(FHLUnit& U, float Dt)
{
	if (U.bDead) return;
	const FHLMobDef& M = *U.Mob;
	const FHLNav& Nav = Areas[U.Area].Nav;
	switch (U.MobState)
	{
	case EHLMobState::Idle:
	{
		if (Now >= U.NextThink)
		{
			U.NextThink = Now + 0.4f + Rand() * 0.3f;
			if (M.bAggressive)
			{
				FHLUnit* Best = nullptr; float BestD = 1e9f;
				for (FHLUnit* O : UnitList)
				{
					if (O->bDead || O->bGhost || O->Area != U.Area || (O->Kind != EHLKind::Player && O->Kind != EHLKind::Sim)) continue;
					if (O->Kind == EHLKind::Sim && O->SimIdx >= 0 && (Sims[O->SimIdx].Role == EHLSimRole::Transient || Sims[O->SimIdx].Role == EHLSimRole::Idler)) continue;
					const float D = FVector2D::Distance(O->Pos, U.Pos);
					// classic rule: 14 m at even level, one metre more per level the mob has over its victim (so a
					// zone-wall elite twenty levels up notices you from across the street), never under 4 m
					const float Aggro = FMath::Clamp(14.f + (U.Level - O->Level), 4.f, 40.f);
					if (D < Aggro && D < BestD && Nav.SegClear(U.Pos, O->Pos)) { Best = O; BestD = D; }
				}
				if (Best) { MobEnterCombat(U, Best->Id); return; }
			}
		}
		if (U.CampIdx >= 0 && Areas[U.Area].Def->Camps[U.CampIdx].Patrol.Num() >= 2)
		{
			// patrol: walk the route's waypoints in a loop (each unit of the camp offset a little so they move as a pack)
			const FHLCamp& Camp = Areas[U.Area].Def->Camps[U.CampIdx];
			const FVector2D Off(FMath::Cos(U.CampSlot * 2.1f) * 2.5f * U.CampSlot, FMath::Sin(U.CampSlot * 2.1f) * 2.5f * U.CampSlot);
			const int32 Wp = U.ScriptPhase % Camp.Patrol.Num();
			U.Home = Nav.NearestFree(Camp.Patrol[Wp] + Off);
			U.MoveMult = 0.55f;
			if (MoveTo(U, U.Home, 1.5f)) U.ScriptPhase = (Wp + 1) % Camp.Patrol.Num();
			break;
		}
		if (U.CampIdx >= 0 && Now >= U.WanderAt && !M.bGuard && M.Rank != TEXT("boss"))
		{
			const FHLCamp& Camp = Areas[U.Area].Def->Camps[U.CampIdx];
			const float Ang = Rand() * 2 * PI, R = Camp.R * FMath::Sqrt(Rand());
			U.PathGoal = Camp.Pos; U.Home = Nav.NearestFree(Camp.Pos + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * R);
			U.WanderAt = Now + 6 + Rand() * 14;
		}
		if (FVector2D::DistSquared(U.Pos, U.Home) > 1.f) { U.MoveMult = 0.35f; MoveTo(U, U.Home, 0.6f); }
		break;
	}
	case EHLMobState::Combat:
	{
		// target: taunt source, else highest threat
		FHLUnit* T = nullptr;
		for (const FHLAura& A : U.Auras) if (A.Type == EHLAura::Taunt) { FHLUnit* S = Unit(A.SourceId); if (S && !S->bDead && S->Area == U.Area) T = S; }
		if (!T)
		{
			float Best = -1;
			for (auto It = U.Threat.CreateIterator(); It; ++It)
			{
				FHLUnit* O = Unit(It.Key());
				if (!O || O->bDead || O->Area != U.Area) { It.RemoveCurrent(); continue; }
				if (It.Value() > Best) { Best = It.Value(); T = O; }
			}
		}
		if (!T || FVector2D::Distance(U.Pos, U.Home) > U.Leash + (M.Rank == TEXT("boss") ? 60 : 0)) { MobEvade(U); return; }
		U.TargetId = T->Id; U.CombatUntil = Now + 6;
		if (U.Stunned()) return;
		if (M.bFlees && U.HpPct() < 0.3f && U.FleeUntil == 0) { U.FleeUntil = Now + 4; U.MobState = EHLMobState::Flee; U.bAutoAttack = false; return; }
		if (!M.Script.IsEmpty()) BossScript(U, Dt);
		if (U.bDead) return;
		const float D = Dist(U, *T);
		const bool bLos = Nav.SegClear(U.Pos, T->Pos);
		// abilities
		if (!U.Casting && Now >= U.GcdUntil)
		{
			for (const FHLAbility* Ab : M.Abilities)
			{
				if (Ab->Id == TEXT("ab_mob_charge") || Ab->Id == TEXT("ab_mob_acid_pool")) continue; // boss-scripted
				if (D > Ab->Range + T->Scale * 0.5f || (Ab->Range > 6 && !bLos)) continue;
				if (Ab->Range > 6 && D < 5 && !M.Script.IsEmpty()) {} // bosses keep spitting in melee
				if (CanCast(U, *Ab, T).IsEmpty()) { TryCast(U, *Ab, T->Id); break; }
			}
		}
		const float Want = M.bRanged ? 18.f : U.AutoRange - 0.6f;
		U.bAutoAttack = true;
		if (!U.Casting)
		{
			if (D > Want || !bLos) { U.MoveMult = 1; MoveTo(U, T->Pos, FMath::Max(1.5f, Want - 0.5f)); }
			else
			{
				// Spread out: a melee mob standing inside another one on the same target sidesteps around the target
				FVector2D Push = FVector2D::ZeroVector;
				if (!M.bRanged)
					for (const FHLUnit* O : UnitList)
					{
						if (O == &U || O->bDead || O->Kind != EHLKind::Mob || O->Area != U.Area || O->TargetId != T->Id) continue;
						const float MinSep = 0.4f * (U.Scale + O->Scale);
						const FVector2D Off = U.Pos - O->Pos; const float L = Off.Size();
						if (L < MinSep) Push += (L > 0.01f ? Off / L : FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f))) * (MinSep - L);
					}
				if (Push.SizeSquared() > 0.0025f)
				{
					// keep the attack distance: slide along the circle around the target
					FVector2D NewPos = U.Pos + Push.GetClampedToMaxSize(3.f * Dt);
					const FVector2D FromT = NewPos - T->Pos;
					if (FromT.Size() > 0.1f) NewPos = T->Pos + FromT.GetSafeNormal() * FMath::Min(FromT.Size(), Want);
					U.Pos = Nav.Slide(U.Pos, NewPos - U.Pos, FMath::Clamp(0.35f * U.Scale, 0.3f, 0.9f));
				}
				else StopMove(U);
				FaceTo(U, T->Pos);
			}
		}
		break;
	}
	case EHLMobState::Flee:
	{
		FHLUnit* T = Unit(U.TargetId);
		if (!T || Now >= U.FleeUntil) { U.MobState = EHLMobState::Combat; return; }
		const FVector2D Away = (U.Pos - T->Pos).GetSafeNormal();
		if (!U.Stunned()) { U.MoveMult = 0.8f; MoveTo(U, Nav.NearestFree(U.Pos + Away * 8), 0.5f); }
		break;
	}
	case EHLMobState::Evade:
	{
		if (U.bEvent || U.SummonerId) { U.bDead = true; U.CorpseUntil = 0; return; } // event mobs and adds vanish
		U.Hp = FMath::Min(U.MaxHp, U.Hp + U.MaxHp * Dt * 0.5f);
		U.MoveMult = 1.6f;
		if (MoveTo(U, U.Home, 1.f)) { U.MobState = EHLMobState::Idle; U.Hp = U.MaxHp; U.MoveMult = 1; U.FleeUntil = 0; }
		break;
	}
	}
}

void FHLGame::BossScript(FHLUnit& U, float Dt)
{
	const FHLMobDef& M = *U.Mob;
	FHLAreaState& Area = Areas[U.Area];
	auto RandomVictim = [&](bool bNotTarget) -> FHLUnit*
	{
		TArray<FHLUnit*> C;
		for (auto& KV : U.Threat) { FHLUnit* O = Unit(KV.Key); if (O && !O->bDead && O->Area == U.Area && !(bNotTarget && O->Id == U.TargetId)) C.Add(O); }
		if (!C.Num()) return Unit(U.TargetId);
		return C[RandInt(0, C.Num() - 1)];
	};
	auto CountAdds = [&]() { int32 N = 0; for (FHLUnit* O : UnitList) if (O->SummonerId == U.Id && !O->bDead) ++N; return N; };
	auto SpawnAdd = [&](const TCHAR* MobId, const FVector2D& Pos)
	{
		const FHLMobDef* Def = FHLData::Get().Mob(MobId);
		if (!Def) return;
		FHLUnit* A = SpawnMob(Def, U.Area, Pos + FVector2D(Rand() * 4 - 2, Rand() * 4 - 2));
		A->SummonerId = U.Id; A->Home = U.Home; A->Leash = 200; A->TappedBy = U.TappedBy;
		if (FHLUnit* V = RandomVictim(false)) MobEnterCombat(*A, V->Id);
	};
	if (M.Script == TEXT("brood_keeper"))
	{
		if (Now >= U.ScriptT[0])
		{
			U.ScriptT[0] = Now + 20;
			int32 Hatched = 0;
			for (FHLObjectState& O : Area.Objects)
				if (O.bActive && O.Def->Boss == M.Id && CountAdds() < 8) { SpawnAdd(TEXT("mob_egy_broodling"), O.Def->Pos); SpawnAdd(TEXT("mob_egy_broodling"), O.Def->Pos); ++Hatched; }
			if (Hatched) AddChat(TEXT("emote"), FString(), TEXT("The egg clusters hatch! Break them to stop the Broodlings."));
		}
	}
	else if (M.Script == TEXT("hive_warden"))
	{
		if (Now >= U.ScriptT[0] && !U.Casting)
		{
			U.ScriptT[0] = Now + 15;
			if (FHLUnit* V = RandomVictim(true))
			{
				AddChat(TEXT("emote"), FString(), FString::Printf(TEXT("Hive Warden charges %s!"), *V->Name));
				U.Pos = Area.Nav.NearestFree(V->Pos + (U.Pos - V->Pos).GetSafeNormal() * 3);
				if (const FHLAbility* Ab = FHLData::Get().Ability(TEXT("ab_mob_charge"))) ExecuteAbility(U, *Ab, V);
			}
		}
	}
	else if (M.Script == TEXT("matriarch"))
	{
		if (Now >= U.ScriptT[0] && !U.Casting)
		{
			U.ScriptT[0] = Now + 12;
			if (FHLUnit* V = RandomVictim(false))
			{
				AddChat(TEXT("emote"), FString(), TEXT("Spitter Matriarch sprays a pool of acid!"));
				// Simulated players "break line of sight" unless they make a mistake; the human has to move behind the seats.
				const FHLAbility* Ab = FHLData::Get().Ability(TEXT("ab_mob_acid_pool"));
				if (Ab)
				{
					TArray<FHLUnit*> Hit;
					for (FHLUnit* O : UnitList) if (!O->bDead && O->Area == U.Area && O->IsFriendly() && O->Kind != EHLKind::Npc && FVector2D::Distance(O->Pos, V->Pos) <= Ab->Radius) Hit.Add(O);
					for (FHLUnit* O : Hit)
					{
						if (O->Kind == EHLKind::Sim && O->SimIdx >= 0 && !Chance(Sims[O->SimIdx].P->MistakeRate * 3)) continue;
						if (!Area.Nav.SegClear(U.Pos, O->Pos)) continue;
						DealDamage(&U, *O, AbilityPower(U, Ab->Effects[0]), TEXT("acid"), false, Ab->Name);
					}
				}
			}
		}
	}
	else if (M.Script == TEXT("hive_mother"))
	{
		const float Pct = U.HpPct();
		if ((U.ScriptPhase == 0 && Pct < 0.66f) || (U.ScriptPhase == 1 && Pct < 0.33f))
		{
			U.ScriptPhase++;
			AddChat(TEXT("emote"), FString(), TEXT("The Egyptian Hive Mother calls her Drones!"));
			for (int32 i = 0; i < 4; ++i) SpawnAdd(TEXT("mob_egy_drone"), U.Pos + FVector2D((i % 2 ? 14 : -14), -10));
		}
		if (Now >= U.ScriptT[1])
		{
			U.ScriptT[1] = Now + 2;
			for (FHLObjectState& O : Area.Objects) if (O.bActive && O.Def->Boss == M.Id) { U.Hp = FMath::Min(U.MaxHp, U.Hp + U.MaxHp * 0.012f); if (Chance(0.15f)) AddChat(TEXT("emote"), FString(), TEXT("The egg sac on the stage feeds the Hive Mother. Burn it!")); }
		}
	}
}

void FHLGame::GenerateLoot(FHLUnit& Mob)
{
	const FHLMobDef& M = *Mob.Mob;
	Mob.Loot.Reset(); Mob.LootCents = 0;
	if (Mob.SummonerId) return;
	const FHLData& D = FHLData::Get();
	if (M.bMoney && Chance(0.7f)) Mob.LootCents = FMath::Max(1, FMath::RoundToInt32(1.5f * Mob.Level * (0.6f + Rand() * 0.8f) * (M.Rank == TEXT("normal") ? 1 : 3)));
	if (M.Junk.Num() && Chance(0.4f)) Mob.Loot.Add({ M.Junk[RandInt(0, M.Junk.Num() - 1)], 1 });
	for (const auto& T : M.Trade) if (Chance(T.Value)) Mob.Loot.Add({ T.Key, 1 });
	auto FromPool = [&](int32 Q) { const TArray<const FHLItem*>& Pool = D.EquipPool(Mob.Level, Q); if (Pool.Num()) Mob.Loot.Add({ Pool[RandInt(0, Pool.Num() - 1)], 1 }); };
	if (Chance(0.06f)) FromPool(1);
	// Greens: the bible's 1.5% is tuned for a 120-hour game; the slice uses 4% so a one-hour session sees a few.
	float Green = 0.04f;
	if (M.Rank == TEXT("elite")) Green = 0.08f; else if (M.Rank == TEXT("rare")) Green = 1.f;
	if (Chance(Green)) FromPool(2);
	if (M.Rank == TEXT("boss") && M.BossLoot.Num())
	{
		TArray<const FHLItem*> Table = M.BossLoot;
		for (int32 i = 0; i < 2 && Table.Num(); ++i) { const int32 k = RandInt(0, Table.Num() - 1); Mob.Loot.Add({ Table[k], 1 }); Table.RemoveAt(k); }
	}
	Mob.bLootable = Mob.Loot.Num() > 0 || Mob.LootCents > 0;
}

void FHLGame::TickCamps(float Dt)
{
	// corpses
	TArray<int32> Remove;
	for (FHLUnit* U : UnitList) if (U->Kind == EHLKind::Mob && U->bDead && Now >= U->CorpseUntil) Remove.Add(U->Id);
	for (int32 Id : Remove) { if (PlayerUnit() && PlayerUnit()->TargetId == Id) PlayerUnit()->TargetId = 0; RemoveUnit(Id); }
	// respawns
	static float NextCheck = 0;
	if (Now < NextCheck) return;
	NextCheck = Now + 1.f;
	const FHLUnit* P = PlayerUnit();
	for (int32 a = 0; a < Areas.Num(); ++a)
	{
		FHLAreaState& A = Areas[a];
		for (int32 c = 0; c < A.Camps.Num(); ++c)
		{
			const FHLCamp& Camp = A.Def->Camps[c];
			for (int32 s = 0; s < Camp.Count; ++s)
			{
				if (A.Camps[c].UnitIds[s] != 0 || Now < A.Camps[c].RespawnAt[s]) continue;
				const float Ang = Rand() * 2 * PI, R = Camp.R * FMath::Sqrt(Rand());
				const FVector2D Pos = Camp.Pos + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * R;
				if (P && P->Area == a && FVector2D::Distance(P->Pos, Pos) < 8) continue; // not on top of the player
				A.Camps[c].UnitIds[s] = SpawnMob(Camp.Mob, a, Pos, c, s)->Id;
			}
		}
		for (FHLObjectState& O : A.Objects) if (!O.bActive && O.Def->Respawn > 0 && Now >= O.RespawnAt) O.bActive = true;
	}
}

bool FHLGame::ObjectInteract(int32 AreaIdx, int32 ObjIdx)
{
	FHLUnit* P = PlayerUnit();
	if (!P || P->bDead || AreaIdx != P->Area || !Areas[AreaIdx].Objects.IsValidIndex(ObjIdx)) return false;
	FHLObjectState& O = Areas[AreaIdx].Objects[ObjIdx];
	const FHLObjectDef& Def = *O.Def;
	if (!O.bActive) return false;
	if (FVector2D::Distance(P->Pos, Def.Pos) > (Def.bBig ? 9.f : 4.5f)) { Error(TEXT("You are too far away.")); return false; }
	if (Def.Kind == TEXT("mailbox")) { System(TEXT("You have no mail.")); return true; }
	if (Def.Kind == TEXT("redcar")) { OpenNpcUnitId = 0; OpenNpcMode = TEXT("redcar"); return true; }
	if (Def.bSalvage && Player.Salvage <= 0) { Error(TEXT("Requires Salvaging. Ike Brandt at Hollywood & Vine can teach you.")); return false; }
	if (!Def.Quest.IsEmpty() && Def.Boss.IsEmpty())
	{
		const FHLQuestState* QS = QuestState(Def.Quest);
		bool bNeeded = false;
		if (QS) for (int32 i = 0; i < QS->Q->Objectives.Num(); ++i) { const FHLObjective& Oj = QS->Q->Objectives[i]; if (Oj.Kind == Def.Kind && QS->Progress[i] < Oj.Count) bNeeded = true; }
		if (!bNeeded) { Error(TEXT("You don't need that right now.")); return false; }
	}
	if (InCombat(*P) && Def.Boss.IsEmpty()) { Error(TEXT("You can't do that while in combat.")); return false; }
	InteractArea = AreaIdx; InteractObj = ObjIdx; InteractStart = Now; InteractEnd = Now + (Def.bUse ? 2.5f : 1.5f);
	InteractLabel = Def.bSalvage ? TEXT("Salvaging") : Def.bUse ? TEXT("Using") : TEXT("Collecting");
	StopMove(*P); P->bSitting = false;
	return true;
}
