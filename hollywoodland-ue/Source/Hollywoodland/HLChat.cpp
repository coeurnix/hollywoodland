// Chat: ambient zone/trade/LFG lines from templates and the corpus (no model calls), the typing-style filter, slash
// commands, and the Worker client (DESIGN-BIBLE §13 protocol) used only for lines addressed to the player, party events,
// invite decisions and periodic gossip. Every Worker failure falls back to the corpus silently.
#include "HLGame.h"
#include "HLBridge.h"
#include "HLSettings.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

static TArray<TSharedPtr<FJsonValue>> HLStrs(const TArray<FString>& In);

void FHLGame::AddChat(const FString& Ch, const FString& From, const FString& Text, int32 SimIdx, float Delay)
{
	if (Text.IsEmpty()) return;
	FHLChatLine L; L.Ch = Ch; L.From = From; L.Text = Text; L.SimIdx = SimIdx; L.Time = Now + Delay;
	if (Delay > 0 && SimIdx >= 0 && Sims.IsValidIndex(SimIdx))
	{
		// a line takes as long as this person types (HLMotor.cpp); while they type it, their character stands still.
		// Anything of five characters or fewer ("help", "oom", "ty") can be hit mid-action.
		FHLSim& Sm = Sims[SimIdx];
		const float Type = SimTypeTime(Sm, Text);
		if (Text.Len() > 5) { Delay = FMath::Max(Delay, Type + 0.3f); if (Sm.UnitId && Now >= Sm.TypeUntil) { Sm.TypeFrom = Now + Delay - Type; Sm.TypeUntil = Now + Delay; Sm.TypeHeld = 0; } }
		L.Time = Now + Delay;
	}
	if (Delay > 0) { PendingChat.Add({ Now + Delay, L }); return; }
	if (SimIdx >= 0 && Ch == TEXT("whisper")) NoteTalk(SimIdx, From, Text);
	if (SimIdx >= 0 && (Ch == TEXT("whisper") || Ch == TEXT("party"))) AwaitingReply.Remove(SimIdx);
	if (Chat.Num() >= 400) Chat.RemoveAt(0, 100);
	if (Ch.StartsWith(TEXT("whisper")) || Ch == TEXT("say") || Ch == TEXT("guild") || Ch == TEXT("party")) UE_LOG(LogTemp, Verbose, TEXT("HL_CHAT [%s] %s: %s"), *Ch, *From, *Text);
	Chat.Add(L);
	if (Ch == TEXT("whisper")) { LastWhisperFrom = From; Sound2D(TEXT("ui_whisper")); }
	// speech bubbles for nearby say/yell
	if ((Ch == TEXT("say") || Ch == TEXT("yell")) && SimIdx >= 0 && Sims.IsValidIndex(SimIdx) && Sims[SimIdx].UnitId) Say(Sims[SimIdx].UnitId, Text, Ch == TEXT("yell"));
		// NPCs and monsters that talk: the nearest unit of that name close to the player
		else if ((Ch == TEXT("npc") || Ch == TEXT("say") || Ch == TEXT("yell")) && !From.IsEmpty())
			if (const FHLUnit* P = PlayerUnit())
			{
				const FHLUnit* Best = nullptr; float BestD = 60;
				for (const FHLUnit* U : UnitList) if (U->Area == P->Area && U->Name == From && !U->bDead) { const float D = FVector2D::Distance(U->Pos, P->Pos); if (D < BestD) { BestD = D; Best = U; } }
				if (Best) Say(Best->Id, Text, Ch == TEXT("yell"));
			}
	}

	void FHLGame::Say(int32 UnitId, const FString& Text, bool bYell)
	{
		if (!UnitId || Text.IsEmpty()) return;
		Speech.RemoveAll([&](const FHLSpeech& S) { return S.UnitId == UnitId || S.Until < Now; });
		if (Speech.Num() > 24) Speech.RemoveAt(0);
		FHLSpeech S; S.UnitId = UnitId; S.Text = Text.Left(180); S.bYell = bYell; S.Start = Now;
		// long enough to read: 3 s plus a beat per word, stretched by the message-time setting
		S.Until = Now + FMath::Min(14.f, (3.f + Text.Len() * 0.055f) * FHLSettings::Get().TextHold);
		Speech.Add(S);
	}

FString FHLGame::CorpusLine(const FHLSim& S, const FString& Situation)
{
	const FHLData& D = FHLData::Get();
	TArray<const FHLCorpusLine*> Fit;
	for (const FHLCorpusLine& L : D.Corpus) if (L.S == Situation && (L.T == TEXT("any") || L.T == S.P->Temperament || L.T == S.P->Archetype)) Fit.Add(&L);
	if (!Fit.Num()) return FString();
	for (int32 Try = 0; Try < 10; ++Try) { const FHLCorpusLine* L = Fit[RandInt(0, Fit.Num() - 1)]; if (FreshLine(L->Text)) return FillTemplate(S, L->Text); }
	// short reactions ("gz", "ok") may repeat; a long line that has been used up is better left unsaid
	const FHLCorpusLine* L = Fit[RandInt(0, Fit.Num() - 1)];
	return L->Text.Len() <= 12 ? FillTemplate(S, L->Text) : FString();
}

bool FHLGame::FreshLine(const FString& Raw, bool bMark)
{
	const uint32 H = GetTypeHash(Raw.ToLower());
	if (const double* At = LineUsedAt.Find(H)) if (Now - *At < 1800) return false;
	if (bMark) LineUsedAt.Add(H, Now);
	return true;
}

FString FHLGame::WhoIsTyping() const
{
	// a whisper or party line on its way: the sender shows as typing for the stretch it takes to type it
	for (const FPendingChat& Pc : PendingChat)
	{
		if (Pc.Line.SimIdx < 0 || !(Pc.Line.Ch == TEXT("whisper") || Pc.Line.Ch == TEXT("party"))) continue;
		const double Typing = FMath::Clamp(Pc.Line.Text.Len() / 5.0, 1.5, 8.0);
		if (Now >= Pc.At - Typing) return Pc.Line.From;
	}
	// someone the player whispered, still composing an answer (reading first, giving up after a while)
	for (const auto& KV : AwaitingReply) if (Now > KV.Value + 2.5 && Now < KV.Value + 25 && Sims.IsValidIndex(KV.Key)) return Sims[KV.Key].P->Handle;
	return FString();
}

void FHLGame::SimSay(int32 SimIdx, const FString& Ch, const FString& Situation, float Delay, const FString& Fallback)
{
	if (!Sims.IsValidIndex(SimIdx)) return;
	const FHLSim& S = Sims[SimIdx];
	FString Line = CorpusLine(S, Situation);
	if (Line.IsEmpty()) Line = Fallback;
	if (Line.IsEmpty()) return;
	AddChat(Ch, S.P->Handle, StyleText(*S.P, Line), SimIdx, Delay);
}

FString FHLGame::StyleText(const FHLPersona& P, const FString& In)
{
	FString Out;
	bool bInLink = false;
	const bool bShout = P.bCaps && Chance(0.3f);
	for (int32 i = 0; i < In.Len(); ++i)
	{
		TCHAR C = In[i];
		if (C == '[') bInLink = true;
		if (!bInLink && C != '<' && C != '>')
		{
			if (bShout) C = FChar::ToUpper(C);
			else if (P.bLower) C = FChar::ToLower(C);
		}
		if (C == ']') bInLink = false;
		Out.AppendChar(C);
	}
	if (!P.bPunct) { while (Out.EndsWith(TEXT(".")) && !Out.EndsWith(TEXT(".."))) Out.LeftChopInline(1); }
	else if (Out.Len() > 3 && !bShout) Out[0] = FChar::ToUpper(Out[0]);
	if (P.Abbrev.Num() >= 3)
	{
		Out = Out.Replace(TEXT(" you "), TEXT(" u "), ESearchCase::CaseSensitive).Replace(TEXT("thanks"), TEXT("ty"), ESearchCase::CaseSensitive).Replace(TEXT(" please"), TEXT(" pls"), ESearchCase::CaseSensitive).Replace(TEXT("people"), TEXT("ppl"), ESearchCase::CaseSensitive);
	}
	// typos: swap two neighbouring letters. Only the sloppier half of the town does it, and not often (one line in
	// eight or so for a long line from the sloppiest)
	if (P.TypoRate >= 0.02f && Out.Len() > 6 && Chance(FMath::Min(0.18f, P.TypoRate * Out.Len() * 0.1f)))
	{
		const int32 k = RandInt(1, Out.Len() - 3);
		if (FChar::IsAlpha(Out[k]) && FChar::IsAlpha(Out[k + 1]) && !Out.Left(k).Contains(TEXT("["))) Swap(Out[k], Out[k + 1]);
	}
	return Out;
}

FString FHLGame::FillTemplate(const FHLSim& S, const FString& In)
{
	if (!In.Contains(TEXT("{"))) return In;
	const FHLData& D = FHLData::Get();
	const FHLUnit* P = PlayerUnit();
	const FHLZoneInfo* Zone = D.Zone(S.Level <= 12 ? TEXT("zone_hollywood_blvd") : S.Zone);
	if (!Zone) Zone = &D.Zones[0];
	// one dungeon per line so {dng} and {boss} agree
	TArray<const FHLDungeonInfo*> Fit, Raids;
	for (const FHLDungeonInfo& Dn : D.Dungeons) { if (Dn.Size > 5) Raids.Add(&Dn); else if (S.Level >= Dn.LevelMin - 1 && S.Level <= Dn.LevelMax + 2) Fit.Add(&Dn); }
	if (!Fit.Num()) Fit.Add(&D.Dungeons[S.Level < 8 ? 0 : D.Dungeons.Num() - 3]);
	const FHLDungeonInfo* Dn = Fit[RandInt(0, Fit.Num() - 1)];
	const FHLDungeonInfo* Raid = Raids.Num() ? Raids[RandInt(0, Raids.Num() - 1)] : Dn;
	const bool bRaidLine = In.Contains(TEXT("{raid}"));
	auto ItemLink = [&]() -> FString
	{
		const TArray<const FHLItem*>& Pool = D.EquipPool(S.Level, Chance(0.75f) ? 2 : (S.Level >= 55 && Chance(0.4f) ? 4 : 3));
		LastLinked = Pool.Num() ? Pool[RandInt(0, Pool.Num() - 1)] : nullptr;
		return LastLinked ? FString::Printf(TEXT("[%s]"), *LastLinked->Name) : TEXT("[Scrap Metal]");
	};
	FString Out = In;
	auto Rep = [&](const TCHAR* Tok, TFunctionRef<FString()> Fn) { while (Out.Contains(Tok)) { const int32 At = Out.Find(Tok); Out = Out.Left(At) + Fn() + Out.Mid(At + FCString::Strlen(Tok)); } };
	Rep(TEXT("{dng}"), [&] { return Dn->Name; });
	Rep(TEXT("{raid}"), [&] { return Raid->Name; });
	Rep(TEXT("{boss}"), [&] { const FHLDungeonInfo* X = bRaidLine ? Raid : Dn; return X->Bosses.Num() ? X->Bosses[RandInt(0, X->Bosses.Num() - 1)] : FString(TEXT("the last boss")); });
	Rep(TEXT("{zone}"), [&] { return Zone->Name; });
	Rep(TEXT("{ozone}"), [&] { const FHLZoneInfo& Z = D.Zones[RandInt(1, D.Zones.Num() - 1)]; return Z.Name; });
	Rep(TEXT("{item}"), ItemLink);
	Rep(TEXT("{price}"), [&] { LastPrice = LastLinked ? FMath::Max(2, FMath::RoundToInt32(MarketPrice(*LastLinked) * (0.7f + Rand() * 0.6f))) : 20; return HLMoney(LastPrice); });
	Rep(TEXT("{role}"), [&] { static const TCHAR* R[] = { TEXT("tank"), TEXT("heals"), TEXT("dps"), TEXT("healer"), TEXT("tank and heals") }; return FString(R[RandInt(0, 4)]); });
	Rep(TEXT("{guild}"), [&] { const FHLGuild* G = D.Guild(S.P->Guild); return G ? G->Name : FString(TEXT("Red Car Regulars")); });
	Rep(TEXT("{mob}"), [&] { return Zone->Mobs.Num() ? Zone->Mobs[RandInt(0, Zone->Mobs.Num() - 1)] : FString(TEXT("Invader Drone")); });
	Rep(TEXT("{quest}"), [&]
	{
		if (Zone->QuestTitles.Num()) return Zone->QuestTitles[RandInt(0, Zone->QuestTitles.Num() - 1)];
		return D.Quests[RandInt(0, D.Quests.Num() - 1)].Name;
	});
	Rep(TEXT("{lvl}"), [&] { return FString::FromInt(S.Level); });
	Rep(TEXT("{class}"), [&] { return S.P->Cls ? S.P->Cls->Name.ToLower() : FString(TEXT("veteran")); });
	Rep(TEXT("{n}"), [&] { return FString::FromInt(RandInt(1, 3)); });
	Rep(TEXT("{player}"), [&] { return Player.Name; });
	Rep(TEXT("{left}"), [&] { return FString::FromInt(60 - S.Level); });
	Rep(TEXT("{plvl}"), [&] { return FString::FromInt(P ? P->Level : 1); });
	Rep(TEXT("{need}"), [&] { return FString::FromInt(FMath::Max(8, S.Level - 4)); });
	Rep(TEXT("{target}"), [&] { return Player.Name; });
	Rep(TEXT("{ctx}"), [&] { return ChatCtx; });
	return Out;
}

void FHLGame::TickChat(float Dt)
{
	for (int32 i = 0; i < PendingChat.Num();)
	{
		if (Now >= PendingChat[i].At)
		{
			const FHLChatLine L = PendingChat[i].Line; PendingChat.RemoveAt(i);
			// a sim that logged off while the line was "being typed" never sends it
			if (L.SimIdx >= 0 && Sims.IsValidIndex(L.SimIdx) && !Sims[L.SimIdx].bOnline) continue;
			AddChat(L.Ch, L.From, L.Text, L.SimIdx, 0);
		}
		else ++i;
	}
	if (!Session.IsEmpty() || Now < NextSessionTry) {} else WorkerSession();
	const FHLData& D = FHLData::Get();
	TickConvos();
	if (Now >= NextAmbient && Sims.Num())
	{
		// about four lines a minute across the capital's channels, mostly General; quieter still while the player is in a fight or a dungeon
		const FHLUnit* PU = PlayerUnit();
		const bool bBusy = PU && (InCombat(*PU) || PU->Area != 0);
		NextAmbient = Now + (9.f + Rand() * 11.f) * (bBusy ? 1.8f : 1.f);
		const float R = Rand();
		const FString Ch = R < 0.72f ? TEXT("zone") : R < 0.86f ? TEXT("lfg") : TEXT("trade");
		static FString LastZoneSituation;
		for (int32 Try = 0; Try < 12; ++Try)
		{
			const int32 SI = RandInt(0, Sims.Num() - 1);
			const FHLSim& S = Sims[SI];
			if (!S.bOnline || S.bInParty) continue;
			if (Ch == TEXT("zone") && !(S.UnitId || S.Level <= 14)) continue; // General is zone-wide; trade and LFG are capital-wide
			if (Ch == TEXT("lfg") && S.Level < 7) continue;
			// General is mostly people: the game as a game they found, their evening, the jam; some of it about play itself
			if (Ch == TEXT("zone") && Chance(0.72f))
			{
				const float Tp = Rand();
				const TCHAR* Sit = Tp < 0.34f ? TEXT("itch") : Tp < 0.6f ? TEXT("life") : Tp < 0.76f ? TEXT("jam") : TEXT("idle");
				const FString Line = CorpusLine(S, Sit);
				if (!Line.IsEmpty()) { AddChat(Ch, S.P->Handle, StyleText(*S.P, Line), SI); LastZoneSituation = TEXT("idle"); break; }
			}
			TArray<const FHLChatTemplate*> Fit;
			const bool bWantAnswer = Ch == TEXT("zone") && LastZoneSituation == TEXT("question") && Chance(0.6f);
			for (const FHLChatTemplate& T : D.Templates)
			{
				if (T.Ch != Ch || S.Level < T.Min || S.Level > T.Max || (T.bGuild && S.P->Guild.IsEmpty())) continue;
				if ((T.S == TEXT("answer")) != bWantAnswer) continue;
				Fit.Add(&T);
			}
			if (!Fit.Num()) continue;
			const FHLChatTemplate* Pick = nullptr;
			for (int32 k = 0; k < 8 && !Pick; ++k) { const FHLChatTemplate* Tt = Fit[RandInt(0, Fit.Num() - 1)]; if (Tt->S == TEXT("wts") || Tt->S == TEXT("wtb") || FreshLine(Tt->Text)) Pick = Tt; }
			if (!Pick) break;
			const FHLChatTemplate& T = *Pick;
			LastLinked = nullptr; LastPrice = 0;
			AddChat(Ch, S.P->Handle, StyleText(*S.P, FillTemplate(S, T.Text)), SI);
			if (T.S == TEXT("wts") && LastLinked) NoteWts(SI, LastLinked, LastPrice);
			if (Ch == TEXT("zone")) LastZoneSituation = T.S;
			break;
		}
	}
	// gossip: one batched Worker call turns recent Census events into zone chat
	if (Now >= NextGossip)
	{
		NextGossip = Now + 170 + Rand() * 130;
		const FHLUnit* PU = PlayerUnit();
		if (bWorkerOk && !(PU && PU->Area != 0))
		{
			TArray<FString> Ev;
			for (int32 i = FMath::Max(0, CensusEvents.Num() - 3); i < CensusEvents.Num(); ++i) Ev.Add(CensusEvents[i].Text);
			TArray<int32> Sp;
			for (int32 Try = 0; Try < 60 && Sp.Num() < 2; ++Try) { const int32 SI = RandInt(0, Sims.Num() - 1); if (Sims[SI].bOnline && !Sims[SI].bInParty && Sims[SI].Level >= 4 && !Sp.Contains(SI)) Sp.Add(SI); }
						// a third of the time it is the outside world or the game itself they talk about (the Worker holds the news digest)
			const float Topic = Rand();
			if (Topic < 0.66f) for (int32 Try = 0; Try < 40 && Sp.Num() < 3; ++Try) { const int32 SI = RandInt(0, Sims.Num() - 1); if (Sims[SI].bOnline && !Sims[SI].bInParty && Sims[SI].UnitId && !Sp.Contains(SI)) Sp.Add(SI); }
			if (Sp.Num()) WorkerSim(TEXT("gossip"), Sp, Topic < 0.36f ? TEXT("news") : Topic < 0.66f ? TEXT("game") : TEXT(""), TEXT("zone"), FString(), Ev, [this, Sp](bool bOk, const TSharedPtr<FJsonObject>& Resp) { if (bOk && Resp.IsValid()) ApplyWorkerActions(Resp, TEXT("zone"), Sp); });
		}
		else if (CensusEvents.Num())
		{
			// offline fallback: state the event as a brag/complaint template
			const FHLCensusEvent& E = CensusEvents.Last();
			const int32 SI = RandInt(0, Sims.Num() - 1);
			if (Sims[SI].bOnline) AddChat(TEXT("zone"), Sims[SI].P->Handle, StyleText(*Sims[SI].P, FString::Printf(TEXT("heard %s"), *E.Text)), SI);
		}
	}
}

// ------------------------------------------------------------------------------------------------------------- player input
void FHLGame::PlayerChat(const FString& Raw)
{
	FString Text = Raw.TrimStartAndEnd();
	if (Text.IsEmpty()) return;
	FHLUnit* P = PlayerUnit();
	FString Ch = ChatChannel, Target = WhisperTarget;
	if (Text.StartsWith(TEXT("/")))
	{
		FString Cmd, Rest;
		if (!Text.Split(TEXT(" "), &Cmd, &Rest)) { Cmd = Text; Rest.Reset(); }
		Cmd = Cmd.ToLower(); Rest = Rest.TrimStartAndEnd();
		if (Cmd == TEXT("/s") || Cmd == TEXT("/say")) Ch = TEXT("say");
		else if (Cmd == TEXT("/y") || Cmd == TEXT("/yell")) Ch = TEXT("yell");
		else if (Cmd == TEXT("/p") || Cmd == TEXT("/party")) Ch = TEXT("party");
		else if (Cmd == TEXT("/1") || Cmd == TEXT("/general") || Cmd == TEXT("/zone")) Ch = TEXT("zone");
		else if (Cmd == TEXT("/g") || Cmd == TEXT("/gu") || Cmd == TEXT("/guild")) Ch = TEXT("guild");
		else if (Cmd == TEXT("/gquit")) { LeaveGuild(); return; }
		else if (Cmd == TEXT("/2") || Cmd == TEXT("/trade")) Ch = TEXT("trade");
		else if (Cmd == TEXT("/4") || Cmd == TEXT("/lfg")) Ch = TEXT("lfg");
		else if (Cmd == TEXT("/w") || Cmd == TEXT("/t") || Cmd == TEXT("/whisper") || Cmd == TEXT("/tell"))
		{
			FString Name, Msg;
			if (!Rest.Split(TEXT(" "), &Name, &Msg)) { Name = Rest; Msg.Reset(); }
			Ch = TEXT("whisper"); Target = Name; Rest = Msg.TrimStartAndEnd();
		}
		else if (Cmd == TEXT("/r") || Cmd == TEXT("/reply")) { Ch = TEXT("whisper"); Target = LastWhisperFrom; }
		else if (Cmd == TEXT("/who")) { WhoQuery = Rest; bWhoOpen = true; return; }
		else if (Cmd == TEXT("/inv") || Cmd == TEXT("/invite"))
		{
			int32 SI = Rest.IsEmpty() && P && Unit(P->TargetId) ? Unit(P->TargetId)->SimIdx : FindSimByHandle(Rest);
			if (SI >= 0) InviteSim(SI); else System(TEXT("No player by that name is currently playing."));
			return;
		}
		else if (Cmd == TEXT("/leave")) { LeaveParty(); return; }
		else if (Cmd == TEXT("/friend") || Cmd == TEXT("/unfriend") || Cmd == TEXT("/duel"))
		{
			const int32 SI = Rest.IsEmpty() && P && Unit(P->TargetId) ? Unit(P->TargetId)->SimIdx : FindSimByHandle(Rest);
			if (SI < 0) { System(TEXT("No player by that name.")); return; }
			if (Cmd == TEXT("/friend")) AddFriend(SI); else if (Cmd == TEXT("/unfriend")) RemoveFriend(SI); else ChallengeDuel(SI);
			return;
		}
		else if (Cmd == TEXT("/roll")) { AddChat(TEXT("system"), FString(), FString::Printf(TEXT("%s rolls %d (1-100)"), *Player.Name, RandInt(1, 100))); return; }
		else if (Cmd == TEXT("/played")) { System(FString::Printf(TEXT("Total time played: %d minutes."), (int32)(Player.PlayedS / 60))); return; }
		else if (Cmd == TEXT("/sit")) { if (P) P->bSitting = !P->bSitting; return; }
		else if (DoEmote(Cmd.Mid(1), Rest)) return; // dialogue.json emotes (/train /silly /flirt /lol ...)
		else if (Cmd == TEXT("/dance") || Cmd == TEXT("/wave") || Cmd == TEXT("/cheer") || Cmd == TEXT("/laugh") || Cmd == TEXT("/bow") || Cmd == TEXT("/point") || Cmd == TEXT("/shrug"))
		{
			const FString Verb = Cmd.Mid(1);
			const FHLUnit* T = P ? Unit(P->TargetId) : nullptr;
			if (T && T != P && (Verb == TEXT("wave") || Verb == TEXT("bow") || Verb == TEXT("point") || Verb == TEXT("laugh")))
				AddChat(TEXT("emote"), FString(), FString::Printf(TEXT("%s %s%s %s."), *Player.Name, *Verb, Verb == TEXT("bow") ? TEXT("s before") : TEXT("s at"), *T->Name));
			else AddChat(TEXT("emote"), FString(), FString::Printf(TEXT("%s %ss."), *Player.Name, *Verb));
			FString Clip = Verb; Clip[0] = FChar::ToUpper(Clip[0]);
			if (P) { P->bSitting = false; Gesture(*P, *Clip); }
			Notice(*FString::Printf(TEXT("did /%s"), *Verb), FString::Printf(TEXT("%s did the /%s emote%s"), *Player.Name, *Verb, T && T->Kind == EHLKind::Sim ? TEXT(", aimed at the speaker") : TEXT("")), T && T->Kind == EHLKind::Sim ? T->Id : 0, T ? 0.85f : 0.45f, *Clip);
			return;
		}
		else if (Cmd == TEXT("/music"))
		{
			const FString A = Rest.ToLower();
			if (A == TEXT("off") || A == TEXT("mute")) Music.SetVolume(0);
			else if (A == TEXT("on")) Music.SetVolume(Music.GetVolume() > 0 ? Music.GetVolume() : 0.5f);
			else if (A == TEXT("next") || A == TEXT("skip")) Music.Skip();
			else if (!A.IsEmpty() && A.IsNumeric()) Music.SetVolume(FCString::Atof(*A) / 100.f);
			const FString Playing = Music.NowPlaying();
			System(FString::Printf(TEXT("Music volume %d%%.%s Usage: /music <0-100> | on | off | next."), FMath::RoundToInt(Music.GetVolume() * 100),
				Playing.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" Now playing: %s."), *Playing)));
			return;
		}
		else if (Cmd == TEXT("/sfx") || Cmd == TEXT("/sound"))
		{
			const FString A = Rest.ToLower();
			if (A == TEXT("off") || A == TEXT("mute")) Sfx.SetVolume(0);
			else if (A == TEXT("on")) Sfx.SetVolume(Sfx.GetVolume() > 0 ? Sfx.GetVolume() : 0.7f);
			else if (A.StartsWith(TEXT("play ")))
			{
				const FString Id = A.Mid(5).TrimStartAndEnd();
				if (!Sfx.Has(Id)) { Error(FString::Printf(TEXT("No sound called %s."), *Id)); return; }
				Sound2D(Id);
				return;
			}
			else if (!A.IsEmpty() && A.IsNumeric()) Sfx.SetVolume(FCString::Atof(*A) / 100.f);
			System(FString::Printf(TEXT("Sound effects volume %d%%. Usage: /sfx <0-100> | on | off | play <id>."), FMath::RoundToInt(Sfx.GetVolume() * 100)));
			return;
		}
		else if (Cmd == TEXT("/help") || Cmd == TEXT("/?"))
		{
			System(TEXT("Chat: /s say, /y yell, /1 general, /2 trade, /4 lfg, /p party, /g guild, /w <name> <text>, /r reply. Other: /who, /gquit, /invite <name>, /friend <name>, /unfriend <name>, /duel <name>, /leave, /roll, /sit, /played, /music, /sfx."));
			System(TEXT("Emotes: /dance, /wave, /bow, /cheer, /laugh (/lol), /silly, /flirt, /train, /chicken, /rasp, /charge, /cry, /spit, /rude, /point, /shrug. Target someone or add a name to aim it."));
			System(TEXT("Keys: WASD move, right-drag turn, Tab target, 1-0 abilities, right-click interact, C character, B bags, L quests, M map, P abilities, O who, N talents, Esc close."));
			return;
		}
		else { System(TEXT("Unknown command. Type /help.")); return; }
		ChatChannel = Ch; WhisperTarget = Target;
		Text = Rest;
		if (Text.IsEmpty()) return;
	}
	Text = Text.Left(240);
	TArray<int32> Speakers;
	if (Ch == TEXT("whisper"))
	{
		const int32 SI = FindSimByHandle(Target);
		if (SI < 0 || !Sims[SI].bOnline) { System(FString::Printf(TEXT("No player named '%s' is currently playing."), *Target)); return; }
		WhisperTarget = Sims[SI].P->Handle;
		AddChat(TEXT("whisper_to"), Sims[SI].P->Handle, Text);
		Relation(Sims[SI]).Familiarity++;
		NoteTalk(SI, Player.Name, Text);
		if (HandleWtbWhisper(SI, Text)) return;
		Speakers.Add(SI);
		if (!Sims[SI].bAfk) AwaitingReply.Add(SI, Now);
	}
	else
	{
		if (Ch == TEXT("party") && !Party.Num()) { System(TEXT("You are not in a party.")); return; }
		if (Ch == TEXT("guild") && Player.Guild.IsEmpty()) { System(TEXT("You are not in a guild.")); ChatChannel = TEXT("say"); return; }
		AddChat(Ch, Player.Name, Text);
		if (P && (Ch == TEXT("say") || Ch == TEXT("yell"))) Say(P->Id, Text, Ch == TEXT("yell"));
		if (Ch == TEXT("party"))
		{
			for (int32 SI : Party) if (Speakers.Num() < 3) Speakers.Add(SI);
			// what did the player mean? (Worker kind "interpret": typed questions, no writing model.) Each member then
			// hears it, or not, and takes it right, or not (HLMind.cpp PartyHeard).
			TArray<FHLUnit*> Mobs;
			if (P) for (FHLUnit* O : UnitList) if (O->Kind == EHLKind::Mob && !O->bDead && O->Area == P->Area && Dist(*O, *P) < 45 && Mobs.Num() < 8 && !Mobs.ContainsByPredicate([&](const FHLUnit* M) { return M->Name == O->Name; })) Mobs.Add(O);
			const FString Low = Text.ToLower();
			auto Local = [this, Low, P]()
			{
				// corpus mode: the obvious words
				const TCHAR* I = Low.Contains(TEXT("wait")) || Low.Contains(TEXT("stop")) || Low.Contains(TEXT("hold")) || Low.Contains(TEXT("dont pull")) ? TEXT("wait") : Low.Contains(TEXT("heal")) ? TEXT("heal") : Low.Contains(TEXT("follow")) || Low.Contains(TEXT("come")) ? TEXT("follow")
					: Low.Contains(TEXT("oom")) || Low.Contains(TEXT("drink")) || Low.Contains(TEXT("rest")) || Low.Contains(TEXT("brb")) ? TEXT("rest") : Low.Contains(TEXT("run")) ? TEXT("run") : Low.Contains(TEXT("ty")) || Low.Contains(TEXT("thank")) || Low.Contains(TEXT("gj")) || Low.Contains(TEXT("nice")) ? TEXT("thanks")
					: Low.Contains(TEXT("sry")) || Low.Contains(TEXT("sorry")) ? TEXT("sorry") : Low.Contains(TEXT("kill")) || Low.Contains(TEXT("attack")) || Low.Contains(TEXT("focus")) ? TEXT("attack") : Low == TEXT("go") || Low.Contains(TEXT("rdy")) || Low.Contains(TEXT("ready")) || Low.Contains(TEXT("pull")) ? TEXT("go") : TEXT("banter");
				PartyHeard(-1, I, P ? P->TargetId : 0);
			};
			TSharedRef<FJsonObject> Req = MakeShared<FJsonObject>();
			Req->SetArrayField(TEXT("speakers"), { MakeShared<FJsonValueObject>(SpeakerJson(Party[0])) });
			TSharedRef<FJsonObject> Trig = MakeShared<FJsonObject>(); Trig->SetStringField(TEXT("text"), Text); Req->SetObjectField(TEXT("trigger"), Trig);
			TSharedRef<FJsonObject> Ctx = MakeShared<FJsonObject>();
			TArray<FString> Members, Targets; TArray<int32> TargetIds;
			for (int32 SI : Party) Members.Add(Sims[SI].P->Handle);
			for (const FHLUnit* M : Mobs) { Targets.Add(M->Name); TargetIds.Add(M->Id); }
			Ctx->SetArrayField(TEXT("members"), HLStrs(Members)); Ctx->SetArrayField(TEXT("targets"), HLStrs(Targets)); Ctx->SetBoolField(TEXT("in_combat"), P && InCombat(*P)); Ctx->SetStringField(TEXT("hub"), CurSubzone);
			Req->SetObjectField(TEXT("context"), Ctx);
			WorkerPost(TEXT("interpret"), Req, [this, Local, Targets, TargetIds](bool bOk, const TSharedPtr<FJsonObject>& Resp)
			{
				const TSharedPtr<FJsonObject>* In = nullptr;
				if (!bOk || !Resp.IsValid() || !Resp->TryGetObjectField(TEXT("intent"), In)) { Local(); return; }
				FString Best; double BestP = 0;
				for (const auto& KV : (*In)->Values) { const double Pr = KV.Value->AsNumber(); if (Pr > BestP) { BestP = Pr; Best = FString(KV.Key); } }
				FString Tn; int32 Tid = 0;
				if (Resp->TryGetStringField(TEXT("target"), Tn)) { const int32 k = Targets.IndexOfByKey(Tn); if (k >= 0) Tid = TargetIds[k]; }
				if (!Tid) if (const FHLUnit* PU = PlayerUnit()) Tid = PU->TargetId;
				UE_LOG(LogTemp, Display, TEXT("HL_MIND the player's party line means \"%s\" (%.2f)%s"), *Best, BestP, Tid ? TEXT(" with a target") : TEXT(""));
				PartyHeard(-1, BestP > 0.45 ? Best : FString(TEXT("banter")), Tid);
			}, 5.f);
		}
		else if (Ch == TEXT("guild")) { for (int32 k = 0; k < 2; ++k) { const int32 SI = RandomGuildmate(Speakers.Num() ? Speakers[0] : -1); if (SI >= 0 && !Speakers.Contains(SI)) Speakers.Add(SI); } }
		else if (Ch == TEXT("say") || Ch == TEXT("yell"))
		{
			const float Range = Ch == TEXT("yell") ? 60.f : 20.f;
			TArray<TPair<float, int32>> Near;
			for (int32 i = 0; i < Sims.Num(); ++i) if (Sims[i].UnitId && P) if (const FHLUnit* U = Unit(Sims[i].UnitId)) if (!U->bDead && Dist(*P, *U) < Range) Near.Add({ Dist(*P, *U), i });
			Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
			for (int32 i = 0; i < Near.Num() && i < 2; ++i) Speakers.Add(Near[i].Value);
		}
		else
		{
			for (int32 Try = 0; Try < 80 && Speakers.Num() < 2; ++Try)
			{
				const int32 SI = RandInt(0, Sims.Num() - 1);
				const FHLSim& S = Sims[SI];
				if (!S.bOnline || S.bAfk || Speakers.Contains(SI)) continue;
				if (Ch == TEXT("zone") && !(S.UnitId || S.Level <= 14)) continue;
				Speakers.Add(SI);
			}
		}
	}
	if (!Speakers.Num()) return;
	const FString Lower = Text.ToLower();
	const bool bAsksAi = Lower.Contains(TEXT("are you a bot")) || Lower.Contains(TEXT("are you real")) || Lower.Contains(TEXT("are you human")) || Lower.Contains(TEXT("are you an ai")) || Lower.Contains(TEXT("r u a bot")) || Lower.Contains(TEXT("real person")) || Lower.Contains(TEXT("are you ai"));
	const bool bDirect = Ch == TEXT("whisper") || Ch == TEXT("party");
	WorkerSim(TEXT("reply"), Speakers, TEXT("chat"), Ch, Text, {}, [this, Speakers, Ch, bAsksAi, bDirect, Len = Text.Len()](bool bOk, const TSharedPtr<FJsonObject>& Resp)
	{
		if (bOk && Resp.IsValid()) { ApplyWorkerActions(Resp, Ch, Speakers); return; }
		// corpus mode: the game stays playable with the Worker unreachable
		const int32 SI = Speakers[0];
		const float Delay = 1.5f + Rand() * 2.5f;
		if (bAsksAi) SimSay(SI, Ch, TEXT("who_ai"), Delay);
		else if (bDirect) SimSay(SI, Ch, TEXT("whisper_fallback"), Delay);
		else if (Ch == TEXT("guild")) { if (Chance(0.65f)) SimSay(SI, Ch, TEXT("guild_reply"), Delay); }
		else if (Chance(0.3f)) SimSay(SI, Ch, TEXT("whisper_fallback"), Delay);
	});
}

// ------------------------------------------------------------------------------------------------------------- worker
void FHLGame::WorkerSession()
{
	NextSessionTry = Now + 60;
	const FString Body = FString::Printf(TEXT("{\"player\":\"%s\"}"), *Player.PlayerId);
	HLBridge::Fetch(TEXT("POST"), ApiBase + TEXT("/v1/session"), Body, [this](const FHLBridgeResult& R)
	{
		TSharedPtr<FJsonObject> O;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(R.Body);
		if (R.Ok() && FJsonSerializer::Deserialize(Reader, O) && O.IsValid() && O->TryGetStringField(TEXT("token"), Session)) { bWorkerOk = true; HLBridge::Log(TEXT("HL worker session ok")); }
		else { bWorkerOk = false; Session.Reset(); HLBridge::Log(FString::Printf(TEXT("HL worker session failed status=%d %s"), R.Status, *R.Error)); }
	});
}

static TArray<TSharedPtr<FJsonValue>> HLStrs(const TArray<FString>& In) { TArray<TSharedPtr<FJsonValue>> Arr; for (const FString& S : In) Arr.Add(MakeShared<FJsonValueString>(S)); return Arr; }

// The persona card (workers/server/src/prompt.js speakerCard): who they are, how they type, where they are from, what
// they say about the jam, how well they know the game, and what they remember of the player.
TSharedRef<FJsonObject> FHLGame::SpeakerJson(int32 SI)
{
	const FHLData& D = FHLData::Get();
	const FHLSim& S = Sims[SI];
	const FHLPersona& Pe = *S.P;
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	O->SetStringField(TEXT("persona"), Pe.Id); O->SetStringField(TEXT("handle"), Pe.Handle); O->SetStringField(TEXT("class"), Pe.Class);
	O->SetNumberField(TEXT("level"), S.Level); O->SetStringField(TEXT("temperament"), Pe.Temperament); O->SetStringField(TEXT("archetype"), Pe.Archetype);
	O->SetStringField(TEXT("typing"), FString::Printf(TEXT("%s, %s, %s%s%s"), Pe.bLower ? TEXT("all lowercase") : TEXT("normal capitals"), Pe.bPunct ? TEXT("uses punctuation") : TEXT("no punctuation"),
		Pe.Abbrev.Num() ? *FString::Printf(TEXT("abbreviations like %s"), *FString::Join(Pe.Abbrev, TEXT(" "))) : TEXT("few abbreviations"), Pe.TypoRate > 0.03f ? TEXT(", occasional typos") : TEXT(""), Pe.bCaps ? TEXT(", shouts in caps when excited") : TEXT("")));
	O->SetStringField(TEXT("english"), Pe.English); O->SetStringField(TEXT("lang"), Pe.Lang); O->SetStringField(TEXT("country"), Pe.Country); O->SetNumberField(TEXT("tz"), Pe.Tz);
	if (Pe.Bits.Num()) O->SetArrayField(TEXT("bits"), HLStrs(Pe.Bits));
	O->SetStringField(TEXT("jam"), Pe.Jam); O->SetStringField(TEXT("found"), Pe.Found); O->SetStringField(TEXT("ai_stance"), Pe.AiStance);
	O->SetNumberField(TEXT("know"), Pe.Tr.Know); O->SetNumberField(TEXT("mmo"), Pe.Tr.Mmo); O->SetNumberField(TEXT("attention"), Pe.Tr.Attn);
	O->SetStringField(TEXT("activity"), SimActivity(S));
	const FString Doing = SimDoing(S);
	if (!Doing.IsEmpty()) O->SetStringField(TEXT("doing"), Doing);
	if (!S.Mood.IsEmpty()) O->SetStringField(TEXT("mood"), S.Mood);
	if (const FHLGuild* G = D.Guild(Pe.Guild)) O->SetStringField(TEXT("guild"), G->Name);
	if (const FHLRelation* R = Player.Relations.Find(Pe.Id))
	{
		O->SetNumberField(TEXT("liking"), R->Liking);
		if (R->Frustration > 0 || S.Frust > 1) O->SetNumberField(TEXT("frustration"), FMath::Max(R->Frustration, FMath::RoundToInt32(S.Frust)));
		if (!R->Bond.IsEmpty()) O->SetStringField(TEXT("bond"), R->Bond);
		if (!R->Gist.IsEmpty()) O->SetStringField(TEXT("gist"), R->Gist);
		// the strongest memories; faded ones are marked so they come out hazy
		TArray<const FHLEpisode*> Eps;
		for (const FHLEpisode& E : R->Episodes) Eps.Add(&E);
		Eps.Sort([](const FHLEpisode& A, const FHLEpisode& B) { return A.W > B.W; });
		const int32 Today = (int32)(RealNow() / 86400);
		TArray<FString> Mem;
		for (int32 i = 0; i < Eps.Num() && Mem.Num() < 6; ++i)
		{
			const int32 Ago = FMath::Max(0, Today - Eps[i]->Day);
			Mem.Add(FString::Printf(TEXT("%s%s (%s)"), Eps[i]->W < 0.3f ? TEXT("(hazy) ") : TEXT(""), *Eps[i]->Text, Ago == 0 ? TEXT("today") : Ago == 1 ? TEXT("yesterday") : *FString::Printf(TEXT("%d days ago"), Ago)));
		}
		for (const FString& F : R->Facts) if (Mem.Num() < 8) Mem.AddUnique(F);
		if (!R->Promise.IsEmpty()) Mem.Add(TEXT("still open: ") + R->Promise);
		if (Mem.Num()) O->SetArrayField(TEXT("memory"), HLStrs(Mem));
		if (R->Lines.Num()) O->SetArrayField(TEXT("last_talk"), HLStrs(R->Lines));
	}
	{
		// their own last three lines, so the model keeps the voice
		TArray<FString> Recent;
		for (int32 k = Chat.Num() - 1; k >= 0 && Recent.Num() < 3; --k) if (Chat[k].SimIdx == SI && !Chat[k].Text.IsEmpty()) Recent.Insert(Chat[k].Text.Left(100), 0);
		if (Recent.Num()) O->SetArrayField(TEXT("recent"), HLStrs(Recent));
	}
	return O;
}

TSharedRef<FJsonObject> FHLGame::WorkerContext()
{
	const FHLUnit* P = PlayerUnit();
	TSharedRef<FJsonObject> Ctx = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> Pl = MakeShared<FJsonObject>();
	Pl->SetStringField(TEXT("name"), Player.Name); Pl->SetNumberField(TEXT("level"), P ? P->Level : 1); Pl->SetStringField(TEXT("class"), Player.Cls ? Player.Cls->Name : FString());
	if (const FHLGuild* Gd = PlayerGuild()) Pl->SetStringField(TEXT("guild"), Gd->Name);
	if (P) Pl->SetStringField(TEXT("doing"), P->bDead ? TEXT("dead") : InCombat(*P) ? TEXT("fighting") : P->bSitting ? TEXT("sitting down") : P->Gesture == TEXT("Dance") && P->HopT > 0 ? TEXT("dancing") : TEXT("standing or walking about"));
	Ctx->SetObjectField(TEXT("player"), Pl);
	Ctx->SetStringField(TEXT("zone"), P ? Areas[P->Area].Def->Name : FString()); Ctx->SetStringField(TEXT("hub"), CurSubzone); Ctx->SetBoolField(TEXT("in_combat"), P && InCombat(*P));
	if (Party.Num() && P)
	{
		TArray<TSharedPtr<FJsonValue>> Pa;
		auto Mem = [&](const FString& Name, const FHLUnit& U) { TSharedRef<FJsonObject> M = MakeShared<FJsonObject>(); M->SetStringField(TEXT("actor"), Name); M->SetNumberField(TEXT("hp"), U.bDead ? 0 : U.HpPct()); if (U.ResType == EHLRes::Power) M->SetNumberField(TEXT("power"), U.MaxRes > 0 ? U.Res / U.MaxRes : 0); Pa.Add(MakeShared<FJsonValueObject>(M)); };
		Mem(TEXT("player"), *P);
		for (int32 SI : Party) if (const FHLUnit* U = Unit(Sims[SI].UnitId)) Mem(Sims[SI].P->Handle, *U);
		Ctx->SetArrayField(TEXT("party"), Pa);
	}
	TArray<FString> Recent;
	for (int32 i = Chat.Num() - 1; i >= 0 && Recent.Num() < 8; --i)
	{
		const FHLChatLine& L = Chat[i];
		if (L.Ch == TEXT("combat") || L.Ch == TEXT("system") || L.Ch == TEXT("loot") || L.Ch == TEXT("quest") || L.From.IsEmpty()) continue;
		// NPC speech is labelled as such: unlabelled, the model took quest givers for players and answered them
		Recent.Insert(L.Ch == TEXT("npc") ? FString::Printf(TEXT("[npc] %s (NPC, not a player): %s"), *L.From, *L.Text) : FString::Printf(TEXT("[%s] %s: %s"), *L.Ch, *L.From, *L.Text), 0);
	}
	if (P)
	{
		// the game's NPCs around the player, by name, so the speakers never mistake them for players
		TArray<TPair<float, FString>> Near;
		for (const FHLUnit* N : UnitList) if (N->Kind == EHLKind::Npc && N->Npc && N->Area == P->Area && Dist(*N, *P) < 60) Near.Add({ Dist(*N, *P), N->Npc->Title.IsEmpty() ? N->Name : FString::Printf(TEXT("%s (%s)"), *N->Name, *N->Npc->Title) });
		Near.Sort([](const TPair<float, FString>& A, const TPair<float, FString>& B) { return A.Key < B.Key; });
		TArray<FString> Names;
		for (const auto& KV : Near) if (Names.Num() < 10) Names.AddUnique(KV.Value);
		if (Names.Num()) Ctx->SetArrayField(TEXT("npcs"), HLStrs(Names));
	}
	Ctx->SetArrayField(TEXT("recent_chat"), HLStrs(Recent));
	return Ctx;
}

void FHLGame::WorkerPost(const FString& Kind, const TSharedRef<FJsonObject>& Req, TFunction<void(bool, const TSharedPtr<FJsonObject>&)> Done, float TimeoutS)
{
	if (!bWorkerOk || Session.IsEmpty() || WorkerInFlight >= 4) { Done(false, nullptr); return; }
	Req->SetStringField(TEXT("session"), Session); Req->SetNumberField(TEXT("id"), NextReqId++); Req->SetNumberField(TEXT("v"), 2); Req->SetStringField(TEXT("kind"), Kind);
	FString Body;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	FJsonSerializer::Serialize(Req, Writer);
	++WorkerInFlight; ++WorkerCalls;
	const double T0 = FPlatformTime::Seconds();
	HLBridge::Fetch(TEXT("POST"), ApiBase + TEXT("/v1/sim"), Body, [this, Done, Kind, T0](const FHLBridgeResult& R)
	{
		--WorkerInFlight;
		TSharedPtr<FJsonObject> O;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(R.Body);
		if (R.Ok() && FJsonSerializer::Deserialize(Reader, O) && O.IsValid())
		{
			UE_LOG(LogTemp, Display, TEXT("HL_AI %s %.0f ms%s"), *Kind, (FPlatformTime::Seconds() - T0) * 1000.0, O->HasField(TEXT("fallback")) ? TEXT(" fallback") : TEXT(""));
			Done(!O->HasField(TEXT("fallback")) || O->HasField(TEXT("actions")), O); return;
		}
		++WorkerFails;
		if (R.Status == 401) { Session.Reset(); bWorkerOk = false; NextSessionTry = Now + 5; }
		else if (R.Status == 429 || R.Status == 0) { bWorkerOk = false; Session.Reset(); NextSessionTry = Now + 60; } // corpus mode, retry in a minute
		Done(false, nullptr);
	}, TimeoutS);
}

void FHLGame::WorkerSim(const FString& Kind, const TArray<int32>& Speakers, const FString& TrigKind, const FString& Channel, const FString& Text,
	const TArray<FString>& Events, TFunction<void(bool, const TSharedPtr<FJsonObject>&)> Done)
{
	if (!bWorkerOk || Session.IsEmpty() || !Speakers.Num()) { Done(false, nullptr); return; }
	TSharedRef<FJsonObject> Req = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Sp;
	for (int32 SI : Speakers) Sp.Add(MakeShared<FJsonValueObject>(SpeakerJson(SI)));
	Req->SetArrayField(TEXT("speakers"), Sp);
	TSharedRef<FJsonObject> Trig = MakeShared<FJsonObject>();
	// gossip topics travel in the trigger kind ("news", "game"); everything else is a trigger kind as before
	if (Kind == TEXT("gossip")) Trig->SetStringField(TEXT("topic"), TrigKind); else Trig->SetStringField(TEXT("kind"), TrigKind);
	Trig->SetStringField(TEXT("channel"), Channel); Trig->SetStringField(TEXT("from"), TEXT("player")); Trig->SetStringField(TEXT("text"), Text);
	Req->SetObjectField(TEXT("trigger"), Trig);
	TSharedRef<FJsonObject> Ctx = WorkerContext();
	Ctx->SetArrayField(TEXT("events"), HLStrs(Events));
	Req->SetObjectField(TEXT("context"), Ctx);
	TSharedRef<FJsonObject> Allowed = MakeShared<FJsonObject>();
	Allowed->SetArrayField(TEXT("goals"), HLStrs({ TEXT("GROUP"), TEXT("FOLLOW"), TEXT("IDLE") })); Allowed->SetArrayField(TEXT("biases"), HLStrs({ TEXT("heal_priority") }));
	Req->SetObjectField(TEXT("allowed"), Allowed);
	WorkerPost(Kind, Req, MoveTemp(Done), 12.f);
}

void FHLGame::ApplyWorkerActions(const TSharedPtr<FJsonObject>& Resp, const FString& DefaultChannel, const TArray<int32>& Speakers)
{
	const TArray<TSharedPtr<FJsonValue>>* Acts = nullptr;
	for (int32 SI : Speakers) AwaitingReply.Remove(SI); // a reply (if any) is now queued with its own typing time
	if (!Resp.IsValid() || !Resp->TryGetArrayField(TEXT("actions"), Acts)) { UE_LOG(LogTemp, Display, TEXT("HL_SYS worker reply without actions")); return; }
	UE_LOG(LogTemp, Display, TEXT("HL_SYS worker reply: %d actions on %s"), Acts->Num(), *DefaultChannel);
	float Stagger = 0;
	for (auto& V : *Acts)
	{
		const TSharedPtr<FJsonObject> A = V->AsObject();
		if (!A.IsValid()) continue;
		const FString PersonaId = A->GetStringField(TEXT("speaker"));
		int32 SI = -1;
		for (int32 Cand : Speakers) if (Sims[Cand].P->Id == PersonaId) SI = Cand;
		if (SI < 0) continue;
		FHLSim& S = Sims[SI];
		FHLRelation& Rel = Relation(S);
		double Num = 0;
		if (A->TryGetNumberField(TEXT("liking_delta"), Num)) Rel.Liking = FMath::Clamp(Rel.Liking + (int32)Num, -10, 10);
		const TArray<TSharedPtr<FJsonValue>>* Mem = nullptr;
		if (A->TryGetArrayField(TEXT("memory_add"), Mem)) for (auto& M : *Mem) { const FString F = M->AsString(); if (!F.IsEmpty()) Remember(SI, TEXT("talk"), F, 0.5f); }
		const TSharedPtr<FJsonObject>* Bias = nullptr;
		if (A->TryGetObjectField(TEXT("bias"), Bias) && (*Bias)->GetStringField(TEXT("type")) == TEXT("heal_priority")) S.HealBiasUntil = Now + FMath::Clamp((float)(*Bias)->GetNumberField(TEXT("ttl_s")), 5.f, 60.f);
		const TSharedPtr<FJsonObject>* ChatObj = nullptr;
		if (A->TryGetObjectField(TEXT("chat"), ChatObj))
		{
			const FString Text = (*ChatObj)->GetStringField(TEXT("text"));
			FString Ch = DefaultChannel;
			if (Ch == TEXT("whisper_to")) Ch = TEXT("whisper");
			double DelayMs = 1200;
			A->TryGetNumberField(TEXT("delay_ms"), DelayMs);
			// a reply appears no earlier than its typing time
			// reading what was said, then typing the answer at their own speed (AddChat stretches the delay to the typing time)
			// the public channels move quickly: a reply there waits at most 3 s before typing starts (6 s elsewhere)
			const bool bPublic = Ch == TEXT("general") || Ch == TEXT("trade") || Ch == TEXT("lfg") || Ch == TEXT("zone");
			const float Delay = FMath::Max(FMath::Min((float)DelayMs / 1000.f, bPublic ? 3.f : 6.f), 0.4f + S.P->ReactionS * 0.6f) + Stagger;
			Stagger += 0.8f;
			AddChat(Ch, S.P->Handle, Text, SI, Delay);
			if (Ch == TEXT("party") || Ch == TEXT("say")) NoteTalk(SI, S.P->Handle, Text);
		}
		FString Emote;
		if (A->TryGetStringField(TEXT("emote"), Emote) && !Emote.IsEmpty())
			if (FHLUnit* U = Unit(S.UnitId))
			{
				const FString E = Emote.ToLower();
				static const TCHAR* Keys[][2] = { { TEXT("danc"), TEXT("Dance") }, { TEXT("wave"), TEXT("Wave") }, { TEXT("cheer"), TEXT("Cheer") }, { TEXT("clap"), TEXT("Cheer") }, { TEXT("laugh"), TEXT("Laugh") }, { TEXT("chuckl"), TEXT("Laugh") }, { TEXT("bow"), TEXT("Bow") }, { TEXT("point"), TEXT("Point") }, { TEXT("shrug"), TEXT("Shrug") } };
				const TCHAR* Clip = TEXT("Talk");
				for (const auto& K : Keys) if (E.Contains(K[0])) { Clip = K[1]; break; }
				Gesture(*U, Clip);
			}
		// goals the model may set (allowed: GROUP, FOLLOW, IDLE); the game decides whether they are possible
		const TSharedPtr<FJsonObject>* GoalObj = nullptr;
		if (A->TryGetObjectField(TEXT("goal"), GoalObj) && GoalObj->IsValid())
		{
			const FString Goal = (*GoalObj)->GetStringField(TEXT("type"));
			FHLUnit* PU = PlayerUnit();
			FHLUnit* SU = Unit(S.UnitId);
			if (Goal == TEXT("GROUP") && PU && !S.bInParty && Party.Num() < 4 && InviteFromSim < 0 && FMath::Abs(S.Level - PU->Level) <= 5 && S.State != EHLSimState::Script)
			{
				InviteFromSim = SI; InviteUntil = Now + 40; Sound2D(TEXT("ui_invite"));
				System(FString::Printf(TEXT("%s has invited you to join a group."), *S.P->Handle));
				UE_LOG(LogTemp, Display, TEXT("HL_LIFE worker goal GROUP from %s"), *S.P->Handle);
			}
			else if (Goal == TEXT("FOLLOW") && PU && SU && !SU->bDead && SU->Area == PU->Area && !S.bInParty && Dist(*SU, *PU) < 120 && S.State != EHLSimState::Script)
			{
				// they come over to the player (the Director's script state walks them to Dest)
				S.State = EHLSimState::Script; S.StateUntil = Now + 30; SU->TargetId = 0; SU->bSitting = false;
				S.Dest = PU->Pos + FVector2D(Rand() * 4 - 2, Rand() * 4 - 2);
				UE_LOG(LogTemp, Display, TEXT("HL_LIFE worker goal FOLLOW from %s"), *S.P->Handle);
			}
			else if (Goal == TEXT("IDLE") && SU && !SU->bDead && !S.bInParty && !InCombat(*SU) && S.State != EHLSimState::Script)
			{
				S.State = EHLSimState::Idle; S.StateUntil = Now + 40 + Rand() * 40; StopMove(*SU); SU->bSitting = Chance(0.5f);
			}
		}
	}
}
