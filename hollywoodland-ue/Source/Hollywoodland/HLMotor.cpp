// The motor layer for simulated players (AI-IMPROVEMENTS-PLAN.md 4.3). The mind says what to do; this file does it the
// way a person at a keyboard would: acting on a slightly stale view of the world, starting late, walking to "about
// there", following loosely, and using their hands for one thing at a time (typing or playing, not both).
// Everything is driven by the persona's traits (FHLTraits), so one simulated player is consistently themselves.
#include "HLGame.h"

/** Seconds this persona needs to type a line (words per minute, five characters to a word). */
float FHLGame::SimTypeTime(const FHLSim& S, const FString& Text) const
{
	return FMath::Clamp(Text.Len() / (FMath::Max(15.f, S.P->Wpm) * 5.f / 60.f), 0.5f, 14.f);
}

/** How much is going on for this player right now: 0 calm, 1 a handful, more is too much. */
float FHLGame::SimLoad(const FHLSim& S, const FHLUnit& U) const
{
	float L = 0;
	for (const FHLUnit* O : UnitList)
	{
		if (O->Kind != EHLKind::Mob || O->bDead || O->Area != U.Area || O->MobState != EHLMobState::Combat) continue;
		if (O->TargetId == U.Id) L += O->Mob && O->Mob->Rank != TEXT("normal") ? 0.6f : 0.38f;
		else if (S.bInParty && FVector2D::DistSquared(O->Pos, U.Pos) < 400.f) L += 0.12f;
	}
	L += (1.f - U.HpPct()) * 0.55f;
	if (S.bInParty)
	{
		if (const FHLUnit* P = PlayerUnit()) if (!P->bDead && P->HpPct() < 0.4f) L += 0.2f;
		for (int32 SI : Party) if (const FHLUnit* M = Unit(Sims[SI].UnitId)) if (M != &U && (M->bDead || M->HpPct() < 0.35f)) L += 0.18f;
	}
	return L;
}

/** The delayed view of the player: followers and assisters act on this, never on live state. */
void FHLGame::SimSee(FHLSim& S, FHLUnit& U)
{
	if (Now < S.NextSee) return;
	const FHLUnit* P = PlayerUnit();
	if (!P) return;
	const FHLTraits& T = S.P->Tr;
	// attentive, skilled players look often; the distracted look late, and later still with a lot going on
	S.NextSee = Now + FMath::Lerp(1.15f, 0.22f, T.Attn * 0.7f + T.Skill * 0.3f) * (0.7f + Rand() * 0.6f) + FMath::Min(0.6f, S.Load * 0.25f);
	if (S.P->Archetype == TEXT("distracted") && Chance(0.12f)) S.NextSee += 1.5f + Rand() * 3.f; // looked away
	S.SeenPos = P->Pos; S.SeenArea = P->Area; S.SeenAt = Now;
	const FHLUnit* PT = Unit(P->TargetId);
	const bool bFighting = PT && !PT->bDead && PT->Kind == EHLKind::Mob && (PT->MobState == EHLMobState::Combat || P->bAutoAttack || P->Casting);
	const int32 NewTarget = bFighting ? P->TargetId : 0;
	if (NewTarget != S.SeenTarget && NewTarget && S.bInParty)
	{
		// which one did they mean? In a pack, an inattentive player picks the wrong one now and then and sticks with it
		TArray<FHLUnit*> Pack;
		for (FHLUnit* O : UnitList) if (O->Kind == EHLKind::Mob && !O->bDead && O->Area == U.Area && O->Id != NewTarget && O->MobState == EHLMobState::Combat && FVector2D::DistSquared(O->Pos, PT->Pos) < 144.f) Pack.Add(O);
		if (Pack.Num() && S.BelTarget == 0 && Chance((1.f - T.Attn) * 0.3f + FMath::Min(0.2f, S.Load * 0.1f)))
		{
			S.BelTarget = Pack[RandInt(0, Pack.Num() - 1)]->Id; ++Misreads;
			UE_LOG(LogTemp, Display, TEXT("HL_MIND %s takes the wrong target (%s, not %s)"), *S.P->Handle, *Unit(S.BelTarget)->Name, *PT->Name);
		}
	}
	S.SeenTarget = NewTarget; S.bSeenFighting = bFighting;
}

/** MoveTo, the way this person walks: a beat before setting off, a pace of their own, "about there" on arrival. */
bool FHLGame::SimMove(FHLSim& S, FHLUnit& U, const FVector2D& Goal, float StopDist)
{
	const FHLTraits& T = S.P->Tr;
	if (FVector2D::DistSquared(S.MoveGoal, Goal) > 9.f)
	{
		S.MoveGoal = Goal;
		// deciding takes a moment (longer for the slow and the distracted); nobody turns on the spot the instant a goal changes
		S.MoveStartAt = Now + S.P->ReactionS * (0.4f + Rand() * 0.9f) + (1.f - T.Attn) * Rand() * 0.9f;
		// people stop near a spot, not on it
		const float R = FMath::Min(StopDist * 0.7f, 0.3f + (1.f - T.Attn) * 1.4f);
		const float A = Rand() * 2 * PI;
		const FVector2D Sc(FMath::Cos(A) * R * Rand(), FMath::Sin(A) * R * Rand());
		S.Scatter = Areas[U.Area].Nav.BlockedAt(Goal + Sc) ? FVector2D::ZeroVector : Sc;
	}
	const FVector2D Aim = Goal + S.Scatter;
	if (FVector2D::Distance(U.Pos, Aim) <= StopDist) { StopMove(U); return true; }
	if (Now < S.MoveStartAt || Now < S.PauseUntil) return false;
	if (Now >= S.NextPace)
	{
		S.NextPace = Now + 2.f + Rand() * 4.f;
		const bool bStroller = (S.P->Archetype == TEXT("lore") || S.P->Archetype == TEXT("tourist")) && !InCombat(U) && !S.bInParty;
		S.Pace = bStroller && S.State != EHLSimState::Leaving ? 0.5f + Rand() * 0.2f : 0.92f + Rand() * 0.12f + (S.P->Archetype == TEXT("speedrunner") ? 0.05f : 0.f);
		// the half-attentive stop to read something, check a map, answer a message somewhere else
		if (!InCombat(U) && !S.bInParty && Chance((1.f - T.Attn) * 0.1f)) { S.PauseUntil = Now + 1.2f + Rand() * 3.5f; StopMove(U); if (Chance(0.5f)) U.Yaw += Rand() * 140 - 70; return false; }
	}
	U.MoveMult = S.Pace;
	// some people cannot run anywhere without jumping
	if (Now >= S.NextHop && !InCombat(U))
	{
		const bool bHopper = S.P->Archetype == TEXT("leeroy") || S.P->Archetype == TEXT("hype") || S.P->Archetype == TEXT("speedrunner") || S.P->Archetype == TEXT("jamdev");
		S.NextHop = Now + (bHopper ? 2.5f + Rand() * 7.f : 14.f + Rand() * 40.f);
		if (U.Z <= 0.01f && !U.bMounted) U.VelZ = 6.2f;
	}
	return MoveTo(U, Aim, StopDist);
}

/** Party members keep up with the player the way people do: each at their own distance, a beat late, bunching up and
 *  falling behind, saying so when they lose sight of the leader. */
void FHLGame::SimFollow(FHLSim& S, FHLUnit& U)
{
	FHLUnit* P = PlayerUnit();
	if (!P) return;
	const int32 SimIdx = int32(&S - Sims.GetData());
	const FHLTraits& T = S.P->Tr;
	if (S.FollowDist <= 0)
	{
		// a distance and a side of their own: the cautious hang back, the sociable crowd you
		S.FollowDist = 3.f + T.Caut * 4.f + (1.f - T.Soc) * 2.f + Rand() * 1.5f - (S.P->Archetype == TEXT("flirt") ? 2.f : 0.f);
		S.FollowSide = Chance(0.5f) ? 1.f : -1.f;
	}
	// a door or a stairway between us: the leader went somewhere I cannot walk to
	if (U.Area != P->Area) { if (Now - S.SeenAt < 3.f && S.SeenArea == P->Area) { TeleportUnit(U, P->Area, P->Pos + FVector2D(Rand() * 4 - 2, Rand() * 4 - 2)); S.bFollowMoving = false; } return; }
	const bool bTight = Now < S.TightUntil;
	const float Want = bTight ? 2.6f : S.FollowDist;
	const float D = FVector2D::Distance(U.Pos, S.SeenPos);
	const float Real = Dist(U, *P);
	if (Real > 150.f) { TeleportUnit(U, P->Area, P->Pos + FVector2D(Rand() * 6 - 3, Rand() * 6 - 3)); S.bFollowMoving = false; return; } // hopelessly lost: the game's leash
	if (Real > 55.f && Now >= S.NextLost && !InCombat(U))
	{
		S.NextLost = Now + 25.f + Rand() * 20.f;
		SimPartySay(SimIdx, TEXT("lost"), TEXT("party_lost"), Chance(0.5f) ? TEXT("where did you go") : TEXT("wait up"));
	}
	if (!S.bFollowMoving)
	{
		if (D > Want + (bTight ? 1.5f : 3.f))
		{
			// notice, then go; not to the leader's feet but to a spot of my own near them
			S.bFollowMoving = true;
			S.FollowGoAt = Now + S.P->ReactionS * (0.3f + Rand() * 0.8f) + (Chance((1.f - T.Attn) * 0.35f) ? 1.f + Rand() * 2.5f : 0.f);
			const FVector2D Away = (U.Pos - S.SeenPos).GetSafeNormal();
			const FVector2D Side(-Away.Y * S.FollowSide, Away.X * S.FollowSide);
			S.FollowAnchor = S.SeenPos + Away * Want * (0.6f + Rand() * 0.4f) + Side * Want * (Rand() * 0.8f);
			if (Areas[U.Area].Nav.BlockedAt(S.FollowAnchor)) S.FollowAnchor = S.SeenPos + Away * 2.f;
		}
		else if (P->bSitting && U.HpPct() < 0.9f && !U.bSitting && Chance(0.02f)) U.bSitting = true;
		else if (!U.bSitting && Chance(0.004f)) FaceTo(U, S.SeenPos + FVector2D(Rand() * 16 - 8, Rand() * 16 - 8)); // looks around
		return;
	}
	if (Now < S.FollowGoAt || U.Casting) return;
	// the leader kept going: re-aim (at the pace I look up)
	if (FVector2D::Distance(S.FollowAnchor, S.SeenPos) > Want + 5.f)
	{
		const FVector2D Away = (U.Pos - S.SeenPos).GetSafeNormal();
		S.FollowAnchor = S.SeenPos + Away * Want * 0.7f + FVector2D(-Away.Y, Away.X) * S.FollowSide * Want * 0.4f * Rand();
	}
	U.bSitting = false;
	// falling behind: hurry (a mounted or hasted leader still outruns them, as it should)
	S.Pace = D > Want + 14.f ? 1.08f : D > Want + 6.f ? 1.f : 0.9f; S.NextPace = Now + 1.f;
	U.MoveMult = S.Pace;
	if (MoveTo(U, S.FollowAnchor, 1.2f) || FVector2D::Distance(U.Pos, S.SeenPos) < Want * 0.75f) { S.bFollowMoving = false; StopMove(U); if (Chance(0.6f)) FaceTo(U, S.SeenPos); }
}
