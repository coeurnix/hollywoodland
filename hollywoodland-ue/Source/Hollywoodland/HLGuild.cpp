// Guilds: a sim recruits the player, guild chat with member logins, grats and dungeon news, the roster's ranks.
#include "HLGame.h"
#include "HLData.h"

static bool GuildFastTest() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("HLLifeTest")); return b; }

const FHLGuild* FHLGame::PlayerGuild() const { return Player.Guild.IsEmpty() ? nullptr : FHLData::Get().Guild(Player.Guild); }

TArray<int32> FHLGame::GuildMembers(const FString& GuildId) const
{
	TArray<int32> M;
	if (GuildId.IsEmpty()) return M;
	for (int32 i = 0; i < Sims.Num(); ++i) if (Sims[i].P->Guild == GuildId) M.Add(i);
	return M;
}

FString FHLGame::GuildRank(int32 SimIdx) const
{
	// ranks follow the persona's own level so they never shuffle: the top member leads, the next two are officers
	const FHLSim& S = Sims[SimIdx];
	int32 Above = 0;
	for (int32 j : GuildMembers(S.P->Guild)) if (j != SimIdx && (Sims[j].P->Level > S.P->Level || (Sims[j].P->Level == S.P->Level && j < SimIdx))) ++Above;
	return Above == 0 ? TEXT("Guild Master") : Above <= 2 ? TEXT("Officer") : S.Level >= 30 ? TEXT("Veteran") : TEXT("Member");
}

int32 FHLGame::RandomGuildmate(int32 Except)
{
	TArray<int32> C;
	for (int32 i : GuildMembers(Player.Guild)) if (i != Except && Sims[i].bOnline && !Sims[i].bAfk) C.Add(i);
	return C.Num() ? C[RandInt(0, C.Num() - 1)] : -1;
}

void FHLGame::GuildEvent(const TCHAR* Kind, const FString& Ctx)
{
	if (Player.Guild.IsEmpty()) return;
	const FString K = Kind;
	if (K == TEXT("ding"))
	{
		const int32 N = RandInt(1, 3);
		int32 Prev = -1;
		for (int32 k = 0; k < N; ++k) { const int32 SI = RandomGuildmate(Prev); if (SI < 0) break; SimSay(SI, TEXT("guild"), TEXT("guild_gz"), 2 + k * 2.5f + Rand() * 2); Prev = SI; }
	}
	else if (K == TEXT("clear"))
	{
		// Ctx is "<sim index>|<dungeon>"
		FString A, B;
		if (!Ctx.Split(TEXT("|"), &A, &B)) return;
		const int32 SI = FCString::Atoi(*A);
		if (!Sims.IsValidIndex(SI) || Sims[SI].P->Guild != Player.Guild) return;
		ChatCtx = FString::Printf(TEXT("%s down"), *B);
		SimSay(SI, TEXT("guild"), TEXT("guild_clear"), 1.5f);
		ChatCtx.Reset();
		const int32 O = RandomGuildmate(SI);
		if (O >= 0) SimSay(O, TEXT("guild"), TEXT("gz"), 5 + Rand() * 3);
	}
}

void FHLGame::JoinGuild(const FString& GuildId, int32 Inviter)
{
	const FHLGuild* G = FHLData::Get().Guild(GuildId);
	if (!G) return;
	Player.Guild = GuildId;
	if (FHLUnit* P = PlayerUnit()) P->Title = FString::Printf(TEXT("<%s>"), *G->Name);
	System(FString::Printf(TEXT("You have joined %s."), *G->Name));
	if (!G->Motd.IsEmpty()) AddChat(TEXT("guild"), FString(), FString::Printf(TEXT("Guild Message of the Day: %s"), *G->Motd));
	GuildSeen.Reset();
	for (int32 i : GuildMembers(GuildId)) if (Sims[i].bOnline) GuildSeen.Add(i);
	int32 Prev = -1;
	const int32 N = FMath::Min(RandInt(2, 4), GuildSeen.Num());
	for (int32 k = 0; k < N; ++k) { const int32 SI = RandomGuildmate(Prev); if (SI < 0) break; SimSay(SI, TEXT("guild"), TEXT("guild_welcome"), 2 + k * 2.2f + Rand() * 2); Prev = SI; }
	if (Sims.IsValidIndex(Inviter)) Relation(Sims[Inviter]).Familiarity += 2;
	NextGuildChat = Now + 60 + Rand() * 40;
	UE_LOG(LogTemp, Display, TEXT("HL_LIFE joined guild %s (%d members, %d online)"), *G->Name, GuildMembers(GuildId).Num(), GuildSeen.Num());
	SaveNow();
}

void FHLGame::LeaveGuild()
{
	const FHLGuild* G = PlayerGuild();
	if (!G) { System(TEXT("You are not in a guild.")); return; }
	System(FString::Printf(TEXT("You have left %s."), *G->Name));
	Player.Guild.Reset(); GuildSeen.Reset();
	if (FHLUnit* P = PlayerUnit()) P->Title.Reset();
	NextGuildInvite = Now + 3600;
	SaveNow();
}

void FHLGame::AnswerGuildInvite(bool bAccept)
{
	const int32 SI = GuildFromSim;
	GuildFromSim = -1;
	if (!Sims.IsValidIndex(SI)) return;
	if (bAccept) { JoinGuild(Sims[SI].P->Guild, SI); return; }
	Player.GuildDeclines++;
	System(TEXT("You decline the guild invitation."));
	SimSay(SI, TEXT("whisper"), TEXT("guild_declined"), 2, TEXT("np"));
}

void FHLGame::TickGuild()
{
	if (Now < NextGuildTick || !Sims.Num()) return;
	NextGuildTick = Now + 3;
	FHLUnit* P = PlayerUnit();
	if (!P || !bInWorld) return;
	const bool bFast = GuildFastTest() || bEventsSoon;
	if (GuildFromSim >= 0 && Now > GuildInviteUntil) GuildFromSim = -1;
	const FHLData& D = FHLData::Get();
	if (Player.Guild.IsEmpty())
	{
		// recruiting: once the player has found their feet, a social or levelling guild asks (a bonded sim's guild first)
		if (NextGuildInvite == 0) NextGuildInvite = Now + (bFast ? 30 : 600 + Rand() * 600);
		if (Now < NextGuildInvite || GuildFromSim >= 0 || P->Level < 4 || InCombat(*P) || P->bDead || Player.GuildDeclines >= 3 || InviteFromSim >= 0 || DuelFromSim >= 0) return;
		NextGuildInvite = Now + (bFast ? 600 : 1500 + Rand() * 1200);
		auto Fits = [&](const FHLSim& S) { const FHLGuild* G = D.Guild(S.P->Guild); return G && G->Kind != TEXT("raid") && S.bOnline && !S.bAfk && !S.bInParty && S.Level >= 6; };
		int32 Pick = -1;
		for (int32 i = 0; i < Sims.Num() && Pick < 0; ++i) { const FHLRelation* R = Player.Relations.Find(Sims[i].P->Id); if (R && !R->Bond.IsEmpty() && R->Bond != TEXT("rival") && Fits(Sims[i])) Pick = i; }
		for (int32 Try = 0; Try < 120 && Pick < 0; ++Try) { const int32 i = RandInt(0, Sims.Num() - 1); if (Fits(Sims[i]) && Sims[i].Level <= 45) Pick = i; }
		if (Pick < 0) return;
		SimSay(Pick, TEXT("whisper"), TEXT("guild_pitch"), 1);
		LastWhisperFrom = Sims[Pick].P->Handle;
		GuildFromSim = Pick; GuildInviteAt = Now + 6; GuildInviteUntil = Now + 70;
		return;
	}
	const FHLGuild* G = PlayerGuild();
	if (!G) { Player.Guild.Reset(); return; }
	// logins and logouts (the census flips bOnline; compare with what we last saw)
	const TArray<int32> Members = GuildMembers(Player.Guild);
	if (!bGuildSeenInit) { GuildSeen.Reset(); for (int32 i : Members) if (Sims[i].bOnline) GuildSeen.Add(i); bGuildSeenInit = true; }
	int32 Notices = 0;
	for (int32 i : Members)
	{
		const bool bOn = Sims[i].bOnline, bWas = GuildSeen.Contains(i);
		if (bOn == bWas) continue;
		if (bOn) GuildSeen.Add(i); else GuildSeen.Remove(i);
		if (Notices++ >= 2) continue;
		// the goodbye comes before the logoff notice, never after it
		if (!bOn && Chance(0.3f)) AddChat(TEXT("guild"), Sims[i].P->Handle, StyleText(*Sims[i].P, CorpusLine(Sims[i], TEXT("guild_bye"))), i);
		AddChat(TEXT("guild"), FString(), FString::Printf(TEXT("%s has %s."), *Sims[i].P->Handle, bOn ? TEXT("come online") : TEXT("gone offline")));
		if (bOn && Chance(0.45f))
		{
			SimSay(i, TEXT("guild"), TEXT("guild_hi"), 3 + Rand() * 4);
			const int32 O = RandomGuildmate(i);
			if (O >= 0 && Chance(0.6f)) SimSay(O, TEXT("guild"), TEXT("guild_hi_back"), 8 + Rand() * 4);
		}
	}
	// raid guilds call their raid at 19:45-20:00 server (Pacific) time
	if (G->Kind == TEXT("raid"))
	{
		const FDateTime Server = FDateTime::UtcNow() - FTimespan::FromHours(8);
		const int32 Day = Server.GetDayOfYear();
		if (Server.GetHour() == 19 && Server.GetMinute() >= 45 && GuildRaidDay != Day) { GuildRaidDay = Day; const int32 SI = RandomGuildmate(-1); if (SI >= 0) SimSay(SI, TEXT("guild"), TEXT("guild_raid"), 1); }
	}
	// ambient chatter, sometimes answered
	if (NextGuildChat == 0) NextGuildChat = Now + 40;
	if (Now >= NextGuildChat)
	{
		NextGuildChat = Now + (bFast ? 25 + Rand() * 25 : 50 + Rand() * 110);
		const int32 SI = RandomGuildmate(-1);
		if (SI >= 0)
		{
			const bool bAsk = Chance(0.45f);
			SimSay(SI, TEXT("guild"), bAsk ? TEXT("guild_ask") : TEXT("guild_idle"), 0.5f);
			const int32 O = RandomGuildmate(SI);
			if (O >= 0 && Chance(bAsk ? 0.7f : 0.35f)) SimSay(O, TEXT("guild"), bAsk ? TEXT("guild_answer") : TEXT("guild_react"), 6 + Rand() * 6);
		}
	}
}
