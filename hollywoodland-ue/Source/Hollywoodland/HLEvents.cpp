// World events (DESIGN-BIBLE Â§10 "Recurring world events"): hive surges on hubs, rare-spawn calls in zone chat and
// picture night at Grauman's Chinese. They run on play-time timers whether or not the player is near, and the simulated
// players join in and talk about them.
#include "HLGame.h"

namespace
{
	struct FFilm { const TCHAR* Title; int32 Year; const TCHAR* Who; };
	// Pictures from before the slice's 1930 setting (all now in the public domain).
	const FFilm Films[] = {
		{ TEXT("The General"), 1926, TEXT("Buster Keaton") }, { TEXT("Safety Last!"), 1923, TEXT("Harold Lloyd") },
		{ TEXT("The Gold Rush"), 1925, TEXT("Charlie Chaplin") }, { TEXT("Sherlock Jr."), 1924, TEXT("Buster Keaton") },
		{ TEXT("The Phantom of the Opera"), 1925, TEXT("Lon Chaney") }, { TEXT("The Thief of Bagdad"), 1924, TEXT("Douglas Fairbanks") },
		{ TEXT("The Kid"), 1921, TEXT("Charlie Chaplin") }, { TEXT("Wings"), 1927, TEXT("Clara Bow") },
		{ TEXT("Steamboat Bill, Jr."), 1928, TEXT("Buster Keaton") }, { TEXT("The Mark of Zorro"), 1920, TEXT("Douglas Fairbanks") },
	};
	const TCHAR* SurgeMobIds[] = { TEXT("mob_invader_worker"), TEXT("mob_invader_drone"), TEXT("mob_invader_spitter"), TEXT("mob_invader_broodling"), TEXT("mob_invader_soldier") };
	constexpr float WaveEvery = 35, WatchToBuff = 45;
}

void FHLGame::InitEvents()
{
	Hubs.Reset();
	const FHLAreaDef& Def = *Areas[0].Def;
	TMap<FString, TPair<FVector2D, int32>> Acc;
	for (const FHLNpcDef& N : Def.Npcs) if (!N.Hub.IsEmpty()) { auto& A = Acc.FindOrAdd(N.Hub); A.Key += N.Pos; A.Value++; }
	for (const auto& KV : Acc)
	{
		FHLHub H; H.Id = KV.Key; H.Pos = Areas[0].Nav.NearestFree(KV.Value.Key / KV.Value.Value); H.Name = SubzoneAt(0, H.Pos);
		Hubs.Add(H);
	}
	const bool bSoon = bEventsSoon = FParse::Param(FCommandLine::Get(), TEXT("HLEventsSoon")); // tests: early and short events
	NextSurgeAt = Now + (bSoon ? 25 : 600 + Rand() * 600);
	NextFilmAt = Now + (bSoon ? 150 : 240 + Rand() * 240);
}

bool FHLGame::HubClosed(const FString& HubId) const
{
	const double* T = HubClosedUntil.Find(HubId);
	return T && Now < *T;
}

int32 FHLGame::RandomLocalSim(const FVector2D& Near, float MaxD)
{
	TArray<int32> C;
	for (int32 i = 0; i < Sims.Num(); ++i)
	{
		const FHLSim& S = Sims[i];
		if (!S.UnitId || S.bInParty || S.Role == EHLSimRole::Away || S.Role == EHLSimRole::Transient) continue;
		const FHLUnit* U = Unit(S.UnitId);
		if (U && !U->bDead && U->Area == 0 && FVector2D::Distance(U->Pos, Near) <= MaxD) C.Add(i);
	}
	return C.Num() ? C[RandInt(0, C.Num() - 1)] : -1;
}

void FHLGame::EventSay(const FString& Situation, const FString& Ctx, const FString& Ch, const FVector2D& Near, float MaxD, float Delay)
{
	const int32 i = RandomLocalSim(Near, MaxD);
	if (i < 0) return;
	ChatCtx = Ctx;
	SimSay(i, Ch, Situation, Delay);
	ChatCtx.Reset();
}

void FHLGame::AnnounceRare(const FHLUnit& Mob)
{
	if (Mob.Area != 0 || !Chance(0.75f)) return;
	EventSay(TEXT("rare_up"), Mob.Name, TEXT("zone"), Mob.Pos, 1e9f, 15 + Rand() * 60);
}

void FHLGame::OnEventMobKilled(const FHLUnit& Mob, const FHLUnit* Killer)
{
	if (!SurgeMobs.Remove(Mob.Id)) return;
	if (Killer && OwnerOf(*Killer) == PlayerUnitId) ++SurgeKills;
}

void FHLGame::TickEvents(float Dt)
{
	const float SurgeLen = bEventsSoon ? 150 : 600, FilmLen = bEventsSoon ? 120 : 480;
	if (!Hubs.Num()) InitEvents();
	if (!Hubs.Num()) return;
	FHLUnit* P = PlayerUnit();
	if (!P) return;

	// ---------------------------------------------------------------- hive surge
	if (SurgeHub < 0 && SurgeWarnHub < 0 && Now >= NextSurgeAt)
	{
		if (P->Area != 0 || P->Level < 2) NextSurgeAt = Now + 60; // wait until the player is out on the Boulevard
		else
		{
			// usually the hub nearest the player, so the surge is seen
			int32 Best = 0; float BestD = 1e9f;
			for (int32 h = 0; h < Hubs.Num(); ++h) { const float D = FVector2D::Distance(Hubs[h].Pos, P->Pos); if (D < BestD) { BestD = D; Best = h; } }
			SurgeWarnHub = Chance(0.7f) ? Best : RandInt(0, Hubs.Num() - 1);
			SurgeStartAt = Now + 30;
			EventSay(TEXT("surge_warn"), Hubs[SurgeWarnHub].Name, TEXT("zone"), Hubs[SurgeWarnHub].Pos, 400, 1);
			Sound2D(TEXT("ev_siren"), FVector2D::Distance(Hubs[SurgeWarnHub].Pos, P->Pos) < 250 ? 1.f : 0.5f);
		}
	}
	if (SurgeWarnHub >= 0 && Now >= SurgeStartAt)
	{
		SurgeHub = SurgeWarnHub; SurgeWarnHub = -1;
		SurgeEnd = Now + SurgeLen; SurgeNextWave = Now; SurgeKills = 0; SurgeMobs.Reset();
		const FHLHub& H = Hubs[SurgeHub];
		Banner(TEXT("Hive Surge"), FString::Printf(TEXT("The hive is attacking %s. Defend it!"), *H.Name), 6);
		Sound2D(TEXT("ev_doom"));
		bBannerHostile = true;
		System(FString::Printf(TEXT("The hive is attacking %s! If the hub falls, its merchants will close."), *H.Name));
		for (int32 k = 0; k < 3; ++k) EventSay(TEXT("surge_start"), H.Name, TEXT("zone"), H.Pos, 500, 1 + k * 3 + Rand() * 3);
		// locals nearby come to help
		int32 Taken = 0;
		for (FHLSim& S : Sims)
		{
			if (Taken >= 6 || !S.UnitId || S.bInParty || S.Role != EHLSimRole::Local || S.State == EHLSimState::Script) continue;
			const FHLUnit* U = Unit(S.UnitId);
			if (U && !U->bDead && U->Area == 0 && FVector2D::Distance(U->Pos, H.Pos) < 320) { S.EventRole = 1; ++Taken; }
		}
	}
	if (SurgeHub >= 0)
	{
		const FHLHub& H = Hubs[SurgeHub];
		if (Now < SurgeEnd - 45 && Now >= SurgeNextWave)
		{
			SurgeNextWave = Now + WaveEvery;
			const FHLData& D = FHLData::Get();
			const int32 Lvl = FMath::Clamp(P->Level, 2, 11);
			const int32 N = 3 + RandInt(0, 2);
			const float Ang0 = Rand() * 2 * PI;
			for (int32 k = 0; k < N; ++k)
			{
				const int32 Pick = FMath::Clamp(Lvl <= 3 ? RandInt(0, 1) : Lvl <= 6 ? RandInt(0, 3) : RandInt(1, 4), 0, 4);
				const FHLMobDef* Def = D.Mob(SurgeMobIds[Pick]);
				if (!Def) continue;
				const float Ang = Ang0 + (Rand() - 0.5f) * 0.6f;
				const FVector2D Pos = Areas[0].Nav.NearestFree(H.Pos + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * (55 + Rand() * 10));
				FHLUnit* M = SpawnMob(Def, 0, Pos);
				M->bEvent = true; M->Leash = 400; M->Level = FMath::Clamp(Lvl + RandInt(-1, 1), 1, 12); InitMobStats(*M);
				M->Home = Areas[0].Nav.NearestFree(H.Pos + FVector2D(Rand() * 12 - 6, Rand() * 12 - 6));
				SurgeMobs.Add(M->Id);
			}
		}
		if (Now >= SurgeEnd)
		{
			int32 AtHub = 0;
			for (int32 Id : SurgeMobs) if (const FHLUnit* M = Unit(Id)) if (!M->bDead && FVector2D::Distance(M->Pos, H.Pos) < 30) ++AtHub;
			const bool bHeld = AtHub < 5;
			for (int32 Id : SurgeMobs) if (FHLUnit* M = Unit(Id)) if (!M->bDead) MobEvade(*M); // the rest withdraw (event mobs vanish)
			SurgeMobs.Reset();
			if (bHeld)
			{
				System(FString::Printf(TEXT("The surge at %s is broken. The hub held."), *H.Name));
				if (SurgeKills >= 3 && P->Level < 60)
				{
					const int32 Xp = XpToNext(P->Level) / 8, Cents = 12 * P->Level;
					Player.Cents += Cents;
					System(FString::Printf(TEXT("You defended %s (%d invaders). The merchants pay you %s."), *H.Name, SurgeKills, *HLMoney(Cents)));
					GrantXp(Xp, true);
				}
				EventSay(TEXT("surge_held"), H.Name, TEXT("zone"), H.Pos, 400, 2 + Rand() * 4);
				Sound2D(TEXT("ev_victory"));
			}
			else
			{
				HubClosedUntil.Add(H.Id, Now + 900);
				System(FString::Printf(TEXT("%s has been overrun. Its merchants have boarded up for 15 minutes."), *H.Name));
				EventSay(TEXT("surge_lost"), H.Name, TEXT("zone"), H.Pos, 1e9f, 2 + Rand() * 4);
			}
			for (FHLSim& S : Sims) if (S.EventRole == 1) S.EventRole = 0;
			SurgeHub = -1;
			NextSurgeAt = Now + 1800 + Rand() * 1200;
		}
	}

	// ---------------------------------------------------------------- picture night
	const FVector2D* Screen = Areas[0].Def->Pois.Find(TEXT("forecourt"));
	if (!Screen) return;
	if (FilmStart <= 0 && !bFilmAnnounced && Now >= NextFilmAt - 120)
	{
		bFilmAnnounced = true;
		const FFilm& F = Films[RandInt(0, UE_ARRAY_COUNT(Films) - 1)];
		FilmTitle = FString::Printf(TEXT("%s (%d), with %s"), F.Title, F.Year, F.Who);
		FilmShort = F.Title;
		System(FString::Printf(TEXT("Picture night: %s begins at Grauman's Chinese in 2 minutes."), *FilmTitle));
		EventSay(TEXT("film_soon"), FilmShort, TEXT("zone"), *Screen, 1e9f, 4 + Rand() * 8);
	}
	if (bFilmAnnounced && FilmStart <= 0 && Now >= NextFilmAt)
	{
		FilmStart = Now; FilmEnd = Now + FilmLen; FilmWatchS = 0; bFilmBuffGiven = false;
		if (P->Area == 0 && FVector2D::Distance(P->Pos, *Screen) < 150) Banner(TEXT("Picture Night"), FilmTitle + TEXT(" is showing at Grauman's Chinese"), 5);
		int32 Taken = 0;
		for (FHLSim& S : Sims)
		{
			if (Taken >= 12 || !S.UnitId || S.bInParty || S.EventRole || S.State == EHLSimState::Script || (S.Role != EHLSimRole::Local && S.Role != EHLSimRole::Idler)) continue;
			const FHLUnit* U = Unit(S.UnitId);
			if (U && !U->bDead && U->Area == 0 && !InCombat(*U) && (S.Role == EHLSimRole::Idler || Chance(0.35f))) { S.EventRole = 2; ++Taken; }
		}
	}
	if (FilmStart > 0 && Now < FilmEnd)
	{
		if (P->Area == 0 && !P->bDead && FVector2D::Distance(P->Pos, *Screen) < 28)
		{
			FilmWatchS += Dt;
			if (!bFilmBuffGiven && FilmWatchS >= WatchToBuff)
			{
				bFilmBuffGiven = true;
				P->Auras.RemoveAll([](const FHLAura& A) { return A.Name == TEXT("Picture Night"); });
				FHLAura A; A.Type = EHLAura::Buff; A.Name = TEXT("Picture Night"); A.Stat = TEXT("xp_pct"); A.Value = 0.1f; A.Remaining = 1800; A.SourceId = P->Id;
				P->Auras.Add(A);
				System(TEXT("You leave the picture in high spirits: +10% experience from kills for 30 minutes."));
			}
			if (Now >= NextFilmChat) { NextFilmChat = Now + 25 + Rand() * 30; EventSay(TEXT("film_watch"), FilmShort, TEXT("say"), *Screen, 30, 0.5f); }
		}
	}
	else if (FilmStart > 0 && Now >= FilmEnd)
	{
		for (FHLSim& S : Sims) if (S.EventRole == 2) { S.EventRole = 0; if (FHLUnit* U = Unit(S.UnitId)) U->bSitting = false; }
		FilmStart = 0; bFilmAnnounced = false;
		NextFilmAt = Now + 2400 + Rand() * 600;
	}
}

// Sims with an event role. Returns true when it drove the unit this tick.
bool FHLGame::SimEventBrain(FHLSim& S, FHLUnit& U)
{
	const int32 SimIdx = int32(&S - Sims.GetData());
	if (S.EventRole == 1)
	{
		if (SurgeHub < 0) { S.EventRole = 0; return false; }
		const FHLHub& H = Hubs[SurgeHub];
		FHLUnit* Best = nullptr; float BestD = 45;
		for (int32 Id : SurgeMobs) if (FHLUnit* M = Unit(Id)) if (!M->bDead)
		{
			// hold the line: only what reaches the hub, or whatever is already on this sim
			if (M->TargetId != U.Id && FVector2D::Distance(M->Pos, H.Pos) > 22) continue;
			const float D = Dist(U, *M); if (D < BestD) { BestD = D; Best = M; }
		}
		if (Best) { U.TargetId = Best->Id; U.bSitting = false; SimCombat(S, U); return true; }
		U.bAutoAttack = false;
		const float A = SimIdx * 2.39996f;
		MoveTo(U, Areas[0].Nav.NearestFree(H.Pos + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 9.f), 2.f);
		return true;
	}
	if (S.EventRole == 2)
	{
		if (FilmStart <= 0 || Now >= FilmEnd) { S.EventRole = 0; return false; }
		if (InCombat(U)) return false;
		const FVector2D* Screen = Areas[0].Def->Pois.Find(TEXT("forecourt"));
		if (!Screen) { S.EventRole = 0; return false; }
		// rows in the forecourt, facing the theatre front (north, -y)
		const int32 Seat = SimIdx % 24;
		const FVector2D Spot = Areas[0].Nav.NearestFree(*Screen + FVector2D(((Seat % 6) - 2.5f) * 2.6f, -13.f + (Seat / 6) * 2.2f));
		if (MoveTo(U, Spot, 1.f)) { FaceTo(U, *Screen + FVector2D(0, -40)); U.bSitting = true; }
		return true;
	}
	return false;
}
