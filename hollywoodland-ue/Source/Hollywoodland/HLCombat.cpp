// Combat rules (DESIGN-BIBLE §6). The player, simulated players and mobs all go through the same functions.
#include "HLGame.h"

void FHLGame::AddFloat(int32 UnitId, const FString& Text, const FLinearColor& Color, bool bCrit)
{
	if (Floats.Num() > 40) Floats.RemoveAt(0);
	FHLFloatText F; F.UnitId = UnitId; F.Text = Text; F.Color = Color; F.bCrit = bCrit;
	Floats.Add(F);
}

int32 FHLGame::OwnerOf(const FHLUnit& U) const
{
	if (U.Kind == EHLKind::Sim && U.SimIdx >= 0 && Sims[U.SimIdx].bInParty) return PlayerUnitId;
	return U.Id;
}

float FHLGame::AbilityPower(const FHLUnit& U, const FHLEffect& E) const
{
	float V = 0;
	if (U.Kind == EHLKind::Mob)
	{
		V = E.Weapon * FMath::Lerp(U.WeaponMin, U.WeaponMax, Rng.FRand()) + E.Base;
	}
	else
	{
		const float Weapon = FMath::Lerp(U.WeaponMin, U.WeaponMax, Rng.FRand()) + U.AttackPower / 14.f * U.WeaponSpeed;
		V = E.Weapon * Weapon + E.Base;
		if (U.Cls && U.Cls->bCaster) V += E.Coef * U.AbilityBonus;
		else if (E.Weapon <= 0) V += E.Coef * U.AttackPower / 14.f;
	}
	return V;
}

FString FHLGame::CanCast(const FHLUnit& U, const FHLAbility& Ab, const FHLUnit* Target) const
{
	if (U.bDead) return TEXT("You are dead.");
	if (U.Stunned()) return TEXT("You are stunned.");
	if (U.Casting) return TEXT("Another action is in progress.");
	if (Ab.bGcd && Now < U.GcdUntil) return TEXT("Not ready yet.");
	if (const float* Cd = U.CdUntil.Find(&Ab)) if (Now < *Cd) return TEXT("That ability isn't ready yet.");
	if (U.ResType != EHLRes::None && U.Res < Ab.Cost) return U.ResType == EHLRes::Power ? TEXT("Not enough Power.") : U.ResType == EHLRes::Energy ? TEXT("Not enough Energy.") : U.ResType == EHLRes::Focus ? TEXT("Not enough Focus.") : TEXT("Not enough Adrenaline.");
	if (Ab.Target == TEXT("enemy"))
	{
		if (!Target || Target->bDead || !IsHostile(U, *Target)) return TEXT("You have no target.");
		if (Dist(U, *Target) > Ab.Range + Target->Scale * 0.5f + 0.5f) return TEXT("Out of range.");
		if (Ab.Range > 6 && !Areas[U.Area].Nav.SegClear(U.Pos, Target->Pos)) return TEXT("Target not in line of sight.");
	}
	else if (Ab.Target == TEXT("friend") && Target && Target != &U)
	{
		if (Dist(U, *Target) > Ab.Range) return TEXT("Out of range.");
	}
	return FString();
}

bool FHLGame::TryCast(FHLUnit& U, const FHLAbility& Ab, int32 TargetId)
{
	FHLUnit* Target = Unit(TargetId);
	if (Ab.Target == TEXT("friend") && (!Target || Target->bDead || IsHostile(U, *Target))) { Target = &U; TargetId = U.Id; }
	if (Ab.Target == TEXT("self") || Ab.Target == TEXT("party")) { Target = &U; TargetId = U.Id; }
	const FString Why = CanCast(U, Ab, Target);
	if (!Why.IsEmpty()) { if (U.Kind == EHLKind::Player) Error(Why); return false; }
	U.bSitting = false;
	if (Ab.bGcd) U.GcdUntil = Now + 1.5f;
	if (Ab.Target == TEXT("enemy") && Target) FaceTo(U, Target->Pos);
	if (Ab.Cast > 0)
	{
		U.Casting = &Ab; U.CastStart = Now; U.CastEnd = Now + Ab.Cast; U.CastTargetId = TargetId;
		StopMove(U);
		return true;
	}
	ExecuteAbility(U, Ab, Target);
	return true;
}

void FHLGame::ExecuteAbility(FHLUnit& U, const FHLAbility& Ab, FHLUnit* Target)
{
	if (U.ResType != EHLRes::None) U.Res = FMath::Max(0.f, U.Res - Ab.Cost);
	if (Ab.Cd > 0) U.CdUntil.Add(&Ab, Now + Ab.Cd);
	const float RankScale = U.Kind == EHLKind::Mob ? 1.f : 1.f + 0.08f * FMath::Max(0, U.Level - Ab.Level);
	const bool bPlayerSide = U.Kind == EHLKind::Player || (Target && Target->Kind == EHLKind::Player);

	TArray<FHLUnit*> Enemies, Friends;
	if (Ab.Target == TEXT("enemy") && Target && !Target->bDead && IsHostile(U, *Target)) Enemies.Add(Target);
	if (Ab.Radius > 0)
	{
		const FVector2D Center = (Ab.Target == TEXT("self") || !Target) ? U.Pos : Target->Pos;
		for (FHLUnit* O : UnitList)
		{
			if (O->bDead || O->Area != U.Area || O->Kind == EHLKind::Npc || !IsHostile(U, *O) || Enemies.Contains(O)) continue;
			if (FVector2D::Distance(O->Pos, Center) > Ab.Radius) continue;
			// Area effects only pull mobs that are already fighting or are aggressive; friendly-cast AoE never hits idle non-aggressive mobs' neighbours beyond radius.
			Enemies.Add(O);
		}
	}
	if (Ab.Target == TEXT("party"))
	{
		Friends.Add(&U);
		const bool bInPlayerParty = U.Kind == EHLKind::Player || (U.Kind == EHLKind::Sim && U.SimIdx >= 0 && Sims[U.SimIdx].bInParty);
		if (bInPlayerParty)
		{
			if (FHLUnit* P = PlayerUnit()) if (P != &U && !P->bDead && Dist(U, *P) < 30) Friends.Add(P);
			for (int32 SI : Party) if (FHLUnit* M = Unit(Sims[SI].UnitId)) if (M != &U && !M->bDead && Dist(U, *M) < 30) Friends.Add(M);
		}
	}
	else if (Ab.Target == TEXT("friend")) Friends.Add(Target && !Target->bDead && !IsHostile(U, *Target) ? Target : &U);
	else Friends.Add(&U);

	if (U.Kind == EHLKind::Player) AddChat(TEXT("combat"), FString(), FString::Printf(TEXT("You use %s."), *Ab.Name));

	const float CritChance = U.Crit + U.BuffSum(TEXT("crit"));
	for (const FHLEffect& E : Ab.Effects)
	{
		if (E.Type == TEXT("damage"))
		{
			for (FHLUnit* T : Enemies)
			{
				if (T->bDead) continue;
				const int32 LevelGap = FMath::Max(0, T->Level - U.Level);
				if (Chance(0.04f + 0.01f * LevelGap)) { if (bPlayerSide || T->Kind == EHLKind::Player) AddFloat(T->Id, TEXT("Miss"), FLinearColor::White); AddThreat(*T, U.Id, 1); continue; }
				const bool bCrit = Chance(CritChance);
				DealDamage(&U, *T, AbilityPower(U, E) * (E.Weapon > 0 ? 1.f : RankScale) * (bCrit ? 2.f : 1.f), Ab.School, bCrit, Ab.Name);
			}
		}
		else if (E.Type == TEXT("dot"))
		{
			for (FHLUnit* T : Enemies)
			{
				if (T->bDead) continue;
				T->Auras.RemoveAll([&](const FHLAura& A) { return A.Name == Ab.Name && A.SourceId == U.Id; });
				FHLAura A; A.Type = EHLAura::Dot; A.Name = Ab.Name; A.SourceId = U.Id; A.bHarmful = true; A.Stat = Ab.School;
				A.Value = U.Kind == EHLKind::Mob ? E.Weapon * (U.WeaponMin + U.WeaponMax) * 0.5f : (E.Base * RankScale + E.Coef * U.AbilityBonus);
				A.Period = E.Period; A.TickTimer = E.Period; A.Remaining = E.Period * E.Ticks + 0.05f;
				T->Auras.Add(A);
				AddThreat(*T, U.Id, 5);
				if (T->Kind == EHLKind::Mob && T->MobState == EHLMobState::Idle) MobEnterCombat(*T, U.Id);
			}
		}
		else if (E.Type == TEXT("heal"))
		{
			for (FHLUnit* T : Friends)
			{
				const bool bCrit = Chance(CritChance);
				HealUnit(&U, *T, (E.Base * RankScale + E.Coef * U.AbilityBonus) * (bCrit ? 1.5f : 1.f), Ab.Name);
			}
		}
		else if (E.Type == TEXT("hot"))
		{
			for (FHLUnit* T : Friends)
			{
				T->Auras.RemoveAll([&](const FHLAura& A) { return A.Name == Ab.Name; });
				FHLAura A; A.Type = EHLAura::Hot; A.Name = Ab.Name; A.SourceId = U.Id;
				A.Value = E.Base * RankScale + E.Coef * U.AbilityBonus; A.Period = E.Period; A.TickTimer = E.Period; A.Remaining = E.Period * E.Ticks + 0.05f;
				T->Auras.Add(A);
			}
		}
		else if (E.Type == TEXT("shield"))
		{
			for (FHLUnit* T : Friends)
			{
				T->Auras.RemoveAll([&](const FHLAura& A) { return A.Name == Ab.Name; });
				FHLAura A; A.Type = EHLAura::Shield; A.Name = Ab.Name; A.SourceId = U.Id; A.Value = E.Base * RankScale + E.Coef * U.AbilityBonus; A.Remaining = E.Dur;
				T->Auras.Add(A);
			}
		}
		else if (E.Type == TEXT("stun") || E.Type == TEXT("slow") || E.Type == TEXT("taunt"))
		{
			for (FHLUnit* T : Enemies)
			{
				if (T->bDead) continue;
				const bool bBoss = T->Mob && T->Mob->Rank == TEXT("boss");
				FHLAura A; A.Name = Ab.Name; A.SourceId = U.Id; A.bHarmful = true;
				if (E.Type == TEXT("stun")) { if (bBoss) continue; A.Type = EHLAura::Stun; A.Remaining = E.Dur; T->Casting = nullptr; }
				else if (E.Type == TEXT("slow")) { A.Type = EHLAura::Slow; A.Value = bBoss ? E.Pct * 0.4f : E.Pct; A.Remaining = E.Dur; }
				else { A.Type = EHLAura::Taunt; A.Remaining = E.Dur; }
				T->Auras.RemoveAll([&](const FHLAura& O) { return O.Name == A.Name && O.Type == A.Type; });
				T->Auras.Add(A);
				AddThreat(*T, U.Id, 10);
				if (T->Kind == EHLKind::Mob && T->MobState == EHLMobState::Idle) MobEnterCombat(*T, U.Id);
			}
		}
		else if (E.Type == TEXT("threat")) { for (FHLUnit* T : Enemies) AddThreat(*T, U.Id, E.Value); }
		else if (E.Type == TEXT("buff"))
		{
			U.Auras.RemoveAll([&](const FHLAura& A) { return A.Name == Ab.Name; });
			FHLAura A; A.Type = EHLAura::Buff; A.Name = Ab.Name; A.Stat = E.Stat; A.Value = E.Value; A.Remaining = E.Dur; A.SourceId = U.Id;
			U.Auras.Add(A);
		}
		else if (E.Type == TEXT("resource")) U.Res = FMath::Min(U.MaxRes, U.Res + E.Value);
	}
	if (Enemies.Num() || Ab.Target == TEXT("enemy")) U.CombatUntil = FMath::Max(U.CombatUntil, (float)Now + 6.f);
}

void FHLGame::AddThreat(FHLUnit& Mob, int32 SrcId, float Amount)
{
	if (Mob.Kind != EHLKind::Mob || Mob.bDead || Mob.MobState == EHLMobState::Evade) return;
	const FHLUnit* Src = Unit(SrcId);
	if (Src && Src->Cls && Src->Cls->Role == TEXT("tank")) Amount *= 1.3f;
	Mob.Threat.FindOrAdd(SrcId) += Amount;
}

void FHLGame::DealDamage(FHLUnit* Src, FHLUnit& Dst, float Amount, const FString& School, bool bCrit, const FString& What)
{
	if (Dst.bDead || Dst.Kind == EHLKind::Npc) return;
	if (Dst.Kind == EHLKind::Mob && Dst.MobState == EHLMobState::Evade) { if (Src && Src->Kind == EHLKind::Player) AddFloat(Dst.Id, TEXT("Evade"), FLinearColor::White); return; }
	if (Src) Amount *= 1.f + Src->BuffSum(TEXT("dmg_pct"));
	if (School == TEXT("physical"))
	{
		const float Armor = Dst.Armor * (1.f + Dst.BuffSum(TEXT("armor_pct")));
		const float DR = FMath::Min(0.75f, Armor / (Armor + 400.f + 85.f * (Src ? Src->Level : Dst.Level)));
		Amount *= 1.f - DR;
	}
	Amount *= FMath::Max(0.1f, 1.f + Dst.BuffSum(TEXT("taken_pct")));
	Amount = FMath::Max(1.f, FMath::RoundToFloat(Amount));
	float Absorbed = 0;
	for (int32 i = Dst.Auras.Num() - 1; i >= 0 && Amount > 0; --i)
	{
		FHLAura& A = Dst.Auras[i];
		if (A.Type != EHLAura::Shield) continue;
		const float Take = FMath::Min(A.Value, Amount);
		A.Value -= Take; Amount -= Take; Absorbed += Take;
		if (A.Value <= 0) Dst.Auras.RemoveAt(i);
	}
	Dst.Hp -= Amount;
	Dst.bSitting = false;
	Dst.CombatUntil = Now + 6.f;
	if (Src) { Src->CombatUntil = Now + 6.f; Src->bSitting = false; }
	Dst.Auras.RemoveAll([](const FHLAura& A) { return A.Name == TEXT("Eating") || A.Name == TEXT("Drinking"); });
	// adrenaline
	if (Src && Src->ResType == EHLRes::Adrenaline) Src->Res = FMath::Min(Src->MaxRes, Src->Res + 7);
	if (Dst.ResType == EHLRes::Adrenaline) Dst.Res = FMath::Min(Dst.MaxRes, Dst.Res + 4);

	const FHLUnit* P = PlayerUnit();
	const bool bSrcPlayer = Src && Src->Kind == EHLKind::Player, bDstPlayer = Dst.Kind == EHLKind::Player;
	const bool bPartyInvolved = (Src && OwnerOf(*Src) == PlayerUnitId) || OwnerOf(Dst) == PlayerUnitId;
	if (bSrcPlayer || bDstPlayer || (bPartyInvolved && P && Dist(*P, Dst) < 40))
	{
		const FString Txt = Absorbed > 0 && Amount <= 0 ? TEXT("Absorb") : FString::Printf(TEXT("%d"), (int32)Amount);
		AddFloat(Dst.Id, Txt, bDstPlayer ? FLinearColor(1, 0.25f, 0.2f) : bSrcPlayer ? (School == TEXT("physical") ? FLinearColor::White : FLinearColor(1, 0.9f, 0.3f)) : FLinearColor(0.75f, 0.75f, 0.75f), bCrit);
	}
	if (bSrcPlayer) AddChat(TEXT("combat"), FString(), FString::Printf(TEXT("Your %s hits %s for %d%s."), *What, *Dst.Name, (int32)Amount, bCrit ? TEXT(" (critical)") : TEXT("")));
	else if (bDstPlayer && Src) AddChat(TEXT("combat"), FString(), FString::Printf(TEXT("%s's %s hits you for %d."), *Src->Name, *What, (int32)Amount));

	if (Dst.Kind == EHLKind::Mob && Src)
	{
		if (Dst.TappedBy == 0 && Src->IsFriendly()) Dst.TappedBy = OwnerOf(*Src);
		AddThreat(Dst, Src->Id, Amount + Absorbed);
		if (Dst.MobState == EHLMobState::Idle || Dst.MobState == EHLMobState::Flee) MobEnterCombat(Dst, Src->Id);
	}
	if (Dst.Hp <= 0) KillUnit(Dst, Src);
}

void FHLGame::HealUnit(FHLUnit* Src, FHLUnit& Dst, float Amount, const FString& What)
{
	if (Dst.bDead) return;
	Amount = FMath::RoundToFloat(FMath::Max(1.f, Amount));
	const float Eff = FMath::Min(Amount, Dst.MaxHp - Dst.Hp);
	Dst.Hp += Eff;
	const bool bShow = Dst.Kind == EHLKind::Player || (Src && Src->Kind == EHLKind::Player);
	if (bShow && Eff > 0) AddFloat(Dst.Id, FString::Printf(TEXT("+%d"), (int32)Eff), FLinearColor(0.3f, 1, 0.3f));
	if (Dst.Kind == EHLKind::Player && Src && Src != &Dst && Eff > 0) AddChat(TEXT("combat"), FString(), FString::Printf(TEXT("%s's %s heals you for %d."), *Src->Name, *What, (int32)Eff));
	if (Src && Eff > 0)
	{
		// healing threat: 0.5 per point, split across mobs fighting the target
		TArray<FHLUnit*> Mobs;
		for (FHLUnit* M : UnitList) if (M->Kind == EHLKind::Mob && !M->bDead && M->MobState == EHLMobState::Combat && M->Threat.Contains(Dst.Id)) Mobs.Add(M);
		for (FHLUnit* M : Mobs) AddThreat(*M, Src->Id, Eff * 0.5f / Mobs.Num());
	}
}

void FHLGame::GrantXp(int32 Amount, bool bQuest)
{
	FHLUnit* U = PlayerUnit();
	if (!U || Amount <= 0) return;
	Player.Xp += Amount;
	AddChat(TEXT("system"), FString(), bQuest ? FString::Printf(TEXT("Experience gained: %d."), Amount) : FString::Printf(TEXT("You gain %d experience."), Amount));
	if (!bQuest) AddFloat(U->Id, FString::Printf(TEXT("+%d XP"), Amount), FLinearColor(0.8f, 0.5f, 1.f));
	while (U->Level < 60 && Player.Xp >= XpToNext(U->Level))
	{
		Player.Xp -= XpToNext(U->Level);
		U->Level++;
		RecalcPlayerStats();
		U->Hp = U->MaxHp; if (U->ResType != EHLRes::Adrenaline) U->Res = U->MaxRes;
		Banner(FString::Printf(TEXT("Level %d"), U->Level), TEXT("You have reached a new level."), 5);
		System(FString::Printf(TEXT("Congratulations, you have reached level %d!"), U->Level));
		bool bNew = false;
		for (const FHLAbility* Ab : Player.Cls->Abilities) if (Ab->Level == U->Level && !Player.Known.Contains(Ab)) bNew = true;
		if (bNew) System(TEXT("New abilities are available at your class trainer in the Chinese forecourt."));
		if (U->Level == 10) System(TEXT("You have unlocked talents (N)."));
		// people nearby and in the party say grats
		int32 Said = 0;
		for (int32 SI : Party) { if (Chance(0.8f)) SimSay(SI, TEXT("party"), TEXT("gz"), 1.5f + Rand() * 3); }
		for (FHLSim& S : Sims) if (Said < 2 && S.UnitId && !S.bInParty) if (FHLUnit* SU = Unit(S.UnitId)) if (Dist(*U, *SU) < 25 && Chance(0.5f)) { SimSay(int32(&S - Sims.GetData()), TEXT("say"), TEXT("gz"), 1.5f + Rand() * 3); ++Said; }
		SaveNow();
	}
}

void FHLGame::KillUnit(FHLUnit& U, FHLUnit* Killer)
{
	if (U.bDead) return;
	U.bDead = true; U.Hp = 0; U.DeadAt = Now; U.Auras.Reset(); U.Casting = nullptr; U.bAutoAttack = false; U.TargetId = 0; StopMove(U); U.bSitting = false;
	if (U.Kind == EHLKind::Mob)
	{
		const FHLMobDef& M = *U.Mob;
		U.MobState = EHLMobState::Idle;
		float RankXp = M.Rank == TEXT("elite") ? 2.f : M.Rank == TEXT("rare") ? 3.f : M.Rank == TEXT("boss") ? 4.f : 1.f;
		if (U.SummonerId) RankXp *= 0.3f;
		if (U.TappedBy == PlayerUnitId && PlayerUnit())
		{
			FHLUnit& P = *PlayerUnit();
			const int32 Diff = U.Level - P.Level;
			float Xp = (45 + 5 * U.Level) * RankXp * FMath::Clamp(1.f + 0.05f * Diff, 0.f, 1.5f);
			if (Diff <= -6) Xp = 0;
			const int32 N = 1 + Party.Num();
			Xp = Xp / N * (1.f + 0.15f * (N - 1));
			if (!P.bDead && Dist(P, U) < 80)
			{
				GrantXp(FMath::RoundToInt32(Xp), false);
				Player.Kills++;
				QuestEvent(TEXT("kill"), M.Id, 1, &U);
			}
			for (int32 SI : Party) Sims[SI].Xp += Xp;
			GenerateLoot(U);
			if (M.Rank == TEXT("boss")) { PartyEvent(FString::Printf(TEXT("the party killed the boss %s"), *U.Name)); if (M.Script == TEXT("hive_mother")) bDungeonCleared = true; }
		}
		else if (FHLUnit* Owner = Unit(U.TappedBy))
		{
			if (Owner->Kind == EHLKind::Sim && Owner->SimIdx >= 0)
			{
				FHLSim& S = Sims[Owner->SimIdx];
				S.Xp += (45 + 5 * U.Level) * RankXp * 1.5f; // simulated players level a little faster than a human would
				S.Kills++;
			}
		}
		U.CorpseUntil = Now + (U.bLootable ? 120.f : 12.f);
		if (U.CampIdx >= 0 && U.CampSlot >= 0)
			Areas[U.Area].Camps[U.CampIdx].RespawnAt[U.CampSlot] = M.Respawn > 0 ? Now + M.Respawn * (0.8f + Rand() * 0.4f) : 1e18f;
		U.Threat.Reset();
	}
	else if (U.Kind == EHLKind::Player)
	{
		Player.Deaths++;
		System(TEXT("You have died. Release to the nearest aid station."));
		if (Party.Num()) PartyEvent(TEXT("the player died"));
		InteractObj = -1; OpenNpcUnitId = 0; OpenNpcMode.Reset();
	}
	else if (U.Kind == EHLKind::Sim && U.SimIdx >= 0)
	{
		FHLSim& S = Sims[U.SimIdx];
		if (S.State != EHLSimState::Script) { S.State = EHLSimState::Dead; S.StateUntil = Now + 12 + Rand() * 14; }
		const FHLUnit* P = PlayerUnit();
		if (P && Dist(*P, U) < 40 && Chance(S.bInParty ? 0.6f : 0.35f)) SimSay(U.SimIdx, S.bInParty ? TEXT("party") : TEXT("say"), TEXT("death"), 2 + Rand() * 3);
	}
}

void FHLGame::TickCombat(FHLUnit& U, float Dt)
{
	if (U.bDead) return;
	// auras
	for (int32 i = U.Auras.Num() - 1; i >= 0; --i)
	{
		FHLAura& A = U.Auras[i];
		A.Remaining -= Dt;
		if (A.Type == EHLAura::Dot || A.Type == EHLAura::Hot)
		{
			A.TickTimer -= Dt;
			if (A.TickTimer <= 0)
			{
				A.TickTimer += A.Period;
				const FHLAura Copy = A; // DealDamage may modify the aura array
				if (Copy.Type == EHLAura::Dot) { DealDamage(Unit(Copy.SourceId), U, Copy.Value, Copy.Stat.IsEmpty() ? TEXT("acid") : Copy.Stat, false, Copy.Name); if (U.bDead) return; }
				else if (Copy.Stat == TEXT("res")) U.Res = FMath::Min(U.MaxRes, U.Res + Copy.Value);
				else HealUnit(Unit(Copy.SourceId), U, Copy.Value, Copy.Name);
				if (i >= U.Auras.Num()) continue;
			}
		}
		if (U.Auras[i].Remaining <= 0) U.Auras.RemoveAt(i);
	}
	const bool bCombat = InCombat(U);
	// regeneration
	if (U.ResType == EHLRes::Energy) U.Res = FMath::Min(U.MaxRes, U.Res + 10 * Dt);
	else if (U.ResType == EHLRes::Focus) U.Res = FMath::Min(U.MaxRes, U.Res + 5 * Dt);
	else if (U.ResType == EHLRes::Adrenaline) { if (!bCombat) U.Res = FMath::Max(0.f, U.Res - 2 * Dt); }
	else if (U.ResType == EHLRes::Power) U.Res = FMath::Min(U.MaxRes, U.Res + (5 + U.Stats[HL_SPI] * 0.5f) / 5.f * Dt * (bCombat ? 0.35f : (U.bSitting ? 3.f : 1.f)));
	if (!bCombat && U.Kind != EHLKind::Mob && U.Hp < U.MaxHp)
		U.Hp = FMath::Min(U.MaxHp, U.Hp + (U.Stats[HL_SPI] * 0.5f + U.MaxHp * 0.02f) / 5.f * Dt * (U.bSitting ? 4.f : 1.f));
	if (U.Stunned()) return;
	// cast completion
	if (U.Casting && Now >= U.CastEnd)
	{
		const FHLAbility* Ab = U.Casting;
		U.Casting = nullptr;
		FHLUnit* T = Unit(U.CastTargetId);
		if (Ab->Target == TEXT("enemy") && (!T || T->bDead || Dist(U, *T) > Ab->Range * 1.4f + 3)) { if (U.Kind == EHLKind::Player) Error(TEXT("Your target is gone.")); }
		else if (U.ResType != EHLRes::None && U.Res < Ab->Cost) {}
		else ExecuteAbility(U, *Ab, T);
	}
	// auto attack
	if (U.SwingTimer > 0) U.SwingTimer -= Dt;
	if (U.bAutoAttack && !U.Casting)
	{
		FHLUnit* T = Unit(U.TargetId);
		if (!T || T->bDead || !IsHostile(U, *T) || T->Area != U.Area) { U.bAutoAttack = false; return; }
		const float D = Dist(U, *T);
		const float Reach = U.AutoRange + (U.bRangedAuto ? 0.f : T->Scale * 0.4f);
		if (D <= Reach && U.SwingTimer <= 0 && (!U.bRangedAuto || Areas[U.Area].Nav.SegClear(U.Pos, T->Pos)))
		{
			U.SwingTimer = U.WeaponSpeed;
			if (U.Kind != EHLKind::Player || !Input.bMouseSteer) FaceTo(U, T->Pos);
			const bool bShow = U.Kind == EHLKind::Player || T->Kind == EHLKind::Player;
			const int32 Gap = FMath::Max(0, T->Level - U.Level);
			const float R = Rand();
			// one-roll attack table (classic order): miss, dodge, parry, block, crit, hit. Parry and block need the
			// defender to face the attacker; nobody parries a bullet.
			const FVector2D ToAtt = (U.Pos - T->Pos).GetSafeNormal();
			const float TRad = FMath::DegreesToRadians(T->Yaw);
			const bool bFacing = FVector2D::DotProduct(ToAtt, FVector2D(FMath::Cos(TRad), FMath::Sin(TRad))) > 0.f || T->Kind == EHLKind::Mob;
			const float Miss = 0.05f + 0.01f * Gap, Dodge = T->Dodge + T->BuffSum(TEXT("dodge"));
			const float Parry = (bFacing && !U.bRangedAuto) ? T->Parry : 0.f, Block = bFacing ? T->Block : 0.f;
			auto Avoided = [&](const TCHAR* What)
			{
				if (bShow) AddFloat(T->Id, What, FLinearColor::White);
				if (T->Kind == EHLKind::Player) AddChat(TEXT("combat"), FString(), FString::Printf(TEXT("%s attacks. You %s."), *U.Name, *FString(What).ToLower()));
				AddThreat(*T, U.Id, 1); U.CombatUntil = T->CombatUntil = Now + 6;
				if (T->Kind == EHLKind::Mob && T->MobState == EHLMobState::Idle) MobEnterCombat(*T, U.Id);
			};
			if (R < Miss) Avoided(TEXT("Miss"));
			else if (R < Miss + Dodge) Avoided(TEXT("Dodge"));
			else if (R < Miss + Dodge + Parry) Avoided(TEXT("Parry"));
			else
			{
				const bool bBlocked = R < Miss + Dodge + Parry + Block;
				const bool bCrit = !bBlocked && Chance(U.Crit + U.BuffSum(TEXT("crit")));
				float Dmg = FMath::Lerp(U.WeaponMin, U.WeaponMax, Rand()) + U.AttackPower / 14.f * U.WeaponSpeed;
				if (bBlocked) { Dmg = FMath::Max(0.f, Dmg - T->BlockValue); if (bShow) AddFloat(T->Id, TEXT("Block"), FLinearColor::White); }
				if (Dmg > 0) DealDamage(&U, *T, Dmg * (bCrit ? 2.f : 1.f), TEXT("physical"), bCrit, U.bRangedAuto ? TEXT("shot") : TEXT("attack"));
				// Dazed: a mob far above its victim slows it on every hit, so nobody outruns a zone-wall elite
				if (U.Kind == EHLKind::Mob && !T->bDead && T->Kind != EHLKind::Mob && U.Level - T->Level >= 5)
				{
					const bool bNew = !T->HasAuraNamed(TEXT("Dazed"));
					T->Auras.RemoveAll([](const FHLAura& A) { return A.Name == TEXT("Dazed"); });
					FHLAura A; A.Type = EHLAura::Slow; A.Name = TEXT("Dazed"); A.Value = 0.5f; A.Remaining = 4; A.bHarmful = true; A.SourceId = U.Id;
					T->Auras.Add(A);
					if (bNew && T->Kind == EHLKind::Player) AddChat(TEXT("combat"), FString(), TEXT("You are Dazed."));
				}
			}
		}
	}
}
