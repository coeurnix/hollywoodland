// Chat: ambient zone/trade/LFG lines from templates and the corpus (no model calls), the typing-style filter, slash
// commands, and the Worker client (DESIGN-BIBLE §13 protocol) used only for lines addressed to the player, party events,
// invite decisions and periodic gossip. Every Worker failure falls back to the corpus silently.
#include "HLGame.h"
#include "HLBridge.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void FHLGame::AddChat(const FString& Ch, const FString& From, const FString& Text, int32 SimIdx, float Delay)
{
	if (Text.IsEmpty()) return;
	FHLChatLine L; L.Ch = Ch; L.From = From; L.Text = Text; L.SimIdx = SimIdx; L.Time = Now + Delay;
	if (Delay > 0) { PendingChat.Add({ Now + Delay, L }); return; }
	if (SimIdx >= 0 && (Ch == TEXT("whisper") || Ch == TEXT("party"))) AwaitingReply.Remove(SimIdx);
	if (Chat.Num() >= 400) Chat.RemoveAt(0, 100);
	if (Ch.StartsWith(TEXT("whisper")) || Ch == TEXT("say") || Ch == TEXT("guild")) UE_LOG(LogTemp, Verbose, TEXT("HL_CHAT [%s] %s: %s"), *Ch, *From, *Text);
	Chat.Add(L);
	if (Ch == TEXT("whisper")) LastWhisperFrom = From;
	// speech bubbles for nearby say/yell
	if ((Ch == TEXT("say") || Ch == TEXT("yell")) && SimIdx >= 0 && Sims.IsValidIndex(SimIdx) && Sims[SimIdx].UnitId) AddFloat(Sims[SimIdx].UnitId, Text.Left(60), FLinearColor(1, 1, 1));
}

FString FHLGame::CorpusLine(const FHLSim& S, const FString& Situation)
{
	const FHLData& D = FHLData::Get();
	TArray<const FHLCorpusLine*> Fit;
	for (const FHLCorpusLine& L : D.Corpus) if (L.S == Situation && (L.T == TEXT("any") || L.T == S.P->Temperament)) Fit.Add(&L);
	if (!Fit.Num()) return FString();
	return FillTemplate(S, Fit[RandInt(0, Fit.Num() - 1)]->Text);
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
	// typos: swap two neighbouring letters
	if (P.TypoRate > 0 && Out.Len() > 6 && Chance(P.TypoRate * Out.Len() * 0.5f))
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
		if (Now >= PendingChat[i].At) { const FHLChatLine L = PendingChat[i].Line; PendingChat.RemoveAt(i); AddChat(L.Ch, L.From, L.Text, L.SimIdx, 0); }
		else ++i;
	}
	if (!Session.IsEmpty() || Now < NextSessionTry) {} else WorkerSession();
	const FHLData& D = FHLData::Get();
	TickConvos();
	if (Now >= NextAmbient && Sims.Num())
	{
		// about 10 lines a minute across the capital's channels
		NextAmbient = Now + 3.5f + Rand() * 6;
		const float R = Rand();
		const FString Ch = R < 0.42f ? TEXT("zone") : R < 0.7f ? TEXT("lfg") : TEXT("trade");
		static FString LastZoneSituation;
		for (int32 Try = 0; Try < 12; ++Try)
		{
			const int32 SI = RandInt(0, Sims.Num() - 1);
			const FHLSim& S = Sims[SI];
			if (!S.bOnline || S.bInParty) continue;
			if (Ch == TEXT("zone") && !(S.UnitId || S.Level <= 14)) continue; // General is zone-wide; trade and LFG are capital-wide
			if (Ch == TEXT("lfg") && S.Level < 7) continue;
			if (Ch == TEXT("zone") && Chance(0.35f)) { const FString Line = CorpusLine(S, TEXT("idle")); if (!Line.IsEmpty()) { AddChat(Ch, S.P->Handle, StyleText(*S.P, Line), SI); LastZoneSituation = TEXT("idle"); break; } }
			TArray<const FHLChatTemplate*> Fit;
			const bool bWantAnswer = Ch == TEXT("zone") && LastZoneSituation == TEXT("question") && Chance(0.6f);
			for (const FHLChatTemplate& T : D.Templates)
			{
				if (T.Ch != Ch || S.Level < T.Min || S.Level > T.Max || (T.bGuild && S.P->Guild.IsEmpty())) continue;
				if ((T.S == TEXT("answer")) != bWantAnswer) continue;
				Fit.Add(&T);
			}
			if (!Fit.Num()) continue;
			const FHLChatTemplate& T = *Fit[RandInt(0, Fit.Num() - 1)];
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
		NextGossip = Now + 140 + Rand() * 60;
		if (bWorkerOk && CensusEvents.Num())
		{
			TArray<FString> Ev;
			for (int32 i = FMath::Max(0, CensusEvents.Num() - 3); i < CensusEvents.Num(); ++i) Ev.Add(CensusEvents[i].Text);
			TArray<int32> Sp;
			for (int32 Try = 0; Try < 60 && Sp.Num() < 2; ++Try) { const int32 SI = RandInt(0, Sims.Num() - 1); if (Sims[SI].bOnline && !Sims[SI].bInParty && Sims[SI].Level >= 10 && !Sp.Contains(SI)) Sp.Add(SI); }
			if (Sp.Num()) WorkerSim(TEXT("gossip"), Sp, TEXT(""), TEXT("zone"), FString(), Ev, [this, Sp](bool bOk, const TSharedPtr<FJsonObject>& Resp) { if (bOk && Resp.IsValid()) ApplyWorkerActions(Resp, TEXT("zone"), Sp); });
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
		else if (Cmd == TEXT("/dance") || Cmd == TEXT("/wave") || Cmd == TEXT("/cheer") || Cmd == TEXT("/laugh") || Cmd == TEXT("/bow") || Cmd == TEXT("/point") || Cmd == TEXT("/shrug"))
		{
			const FString Verb = Cmd.Mid(1);
			const FHLUnit* T = P ? Unit(P->TargetId) : nullptr;
			if (T && T != P && (Verb == TEXT("wave") || Verb == TEXT("bow") || Verb == TEXT("point") || Verb == TEXT("laugh")))
				AddChat(TEXT("emote"), FString(), FString::Printf(TEXT("%s %s%s %s."), *Player.Name, *Verb, Verb == TEXT("bow") ? TEXT("s before") : TEXT("s at"), *T->Name));
			else AddChat(TEXT("emote"), FString(), FString::Printf(TEXT("%s %ss."), *Player.Name, *Verb));
			if (P) { P->bSitting = false; FString Clip = Verb; Clip[0] = FChar::ToUpper(Clip[0]); Gesture(*P, *Clip); }
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
		else if (Cmd == TEXT("/help") || Cmd == TEXT("/?"))
		{
			System(TEXT("Chat: /s say, /y yell, /1 general, /2 trade, /4 lfg, /p party, /g guild, /w <name> <text>, /r reply. Other: /who, /gquit, /invite <name>, /friend <name>, /unfriend <name>, /duel <name>, /leave, /roll, /sit, /dance, /wave, /bow, /cheer, /laugh, /point, /shrug, /played, /music."));
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
		if (HandleWtbWhisper(SI, Text)) return;
		Speakers.Add(SI);
		if (!Sims[SI].bAfk) AwaitingReply.Add(SI, Now);
	}
	else
	{
		if (Ch == TEXT("party") && !Party.Num()) { System(TEXT("You are not in a party.")); return; }
		if (Ch == TEXT("guild") && Player.Guild.IsEmpty()) { System(TEXT("You are not in a guild.")); ChatChannel = TEXT("say"); return; }
		AddChat(Ch, Player.Name, Text);
		if (P) AddFloat(P->Id, Ch == TEXT("say") || Ch == TEXT("yell") ? Text.Left(60) : FString(), FLinearColor::White);
		if (Ch == TEXT("party")) { for (int32 SI : Party) if (Speakers.Num() < 3) Speakers.Add(SI); }
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

void FHLGame::WorkerSim(const FString& Kind, const TArray<int32>& Speakers, const FString& TrigKind, const FString& Channel, const FString& Text,
	const TArray<FString>& Events, TFunction<void(bool, const TSharedPtr<FJsonObject>&)> Done)
{
	if (!bWorkerOk || Session.IsEmpty() || WorkerInFlight >= 3 || !Speakers.Num()) { Done(false, nullptr); return; }
	const FHLData& D = FHLData::Get();
	const FHLUnit* P = PlayerUnit();
	auto Strs = [](const TArray<FString>& In) { TArray<TSharedPtr<FJsonValue>> Arr; for (const FString& S : In) Arr.Add(MakeShared<FJsonValueString>(S)); return Arr; };
	TSharedRef<FJsonObject> Req = MakeShared<FJsonObject>();
	Req->SetStringField(TEXT("session"), Session); Req->SetNumberField(TEXT("id"), NextReqId++); Req->SetStringField(TEXT("kind"), Kind);
	TArray<TSharedPtr<FJsonValue>> Sp;
	TArray<FString> Ids = { TEXT("Hollywood Boulevard"), TEXT("The Egyptian Theatre"), TEXT("Grauman's Chinese"), TEXT("Taft Building"), TEXT("Hollywood High School"), TEXT("Hollywood Hotel") };
	TArray<FString> World;
	World.Add(TEXT("Hollywood Boulevard (levels 1-12) is the capital: bank at Hollywood & Highland, auction house in the Taft Building at Vine, class trainers at Grauman's Chinese forecourt."));
	World.Add(TEXT("The Egyptian Theatre is a 5-player dungeon for levels 8-12 on Hollywood Boulevard; bosses: Brood Keeper, Hive Warden, Spitter Matriarch, The Egyptian Hive Mother."));
	World.Add(TEXT("Every road out of Hollywood Boulevard is blocked by a hive wall whose gap is guarded by elites of level 22-34."));
	for (int32 SI : Speakers)
	{
		const FHLSim& S = Sims[SI];
		const FHLPersona& Pe = *S.P;
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("persona"), Pe.Id); O->SetStringField(TEXT("handle"), Pe.Handle); O->SetStringField(TEXT("class"), Pe.Class);
		O->SetNumberField(TEXT("level"), S.Level); O->SetStringField(TEXT("temperament"), Pe.Temperament);
		O->SetStringField(TEXT("typing"), FString::Printf(TEXT("%s, %s, %s%s"), Pe.bLower ? TEXT("all lowercase") : TEXT("normal capitals"), Pe.bPunct ? TEXT("uses punctuation") : TEXT("no punctuation"),
			Pe.Abbrev.Num() ? *FString::Printf(TEXT("abbreviations like %s"), *FString::Join(Pe.Abbrev, TEXT(" "))) : TEXT("few abbreviations"), Pe.TypoRate > 0.03f ? TEXT(", occasional typos") : TEXT("")));
		O->SetStringField(TEXT("activity"), SimActivity(S));
		if (const FHLGuild* G = D.Guild(Pe.Guild)) { O->SetStringField(TEXT("guild"), G->Name); Ids.AddUnique(G->Name); }
		if (const FHLRelation* R = Player.Relations.Find(Pe.Id)) { O->SetNumberField(TEXT("liking"), R->Liking); O->SetArrayField(TEXT("memory"), Strs(R->Facts)); }
		{
			// their own last three lines, so the model keeps the voice
			TArray<FString> Recent;
			for (int32 k = Chat.Num() - 1; k >= 0 && Recent.Num() < 3; --k) if (Chat[k].SimIdx == SI && !Chat[k].Text.IsEmpty()) Recent.Insert(Chat[k].Text.Left(100), 0);
			if (Recent.Num()) O->SetArrayField(TEXT("recent"), Strs(Recent));
		}
		Sp.Add(MakeShared<FJsonValueObject>(O));
		if (const FHLZoneInfo* Z = D.Zone(S.Zone)) { World.AddUnique(FString::Printf(TEXT("%s (levels %d-%d): %s"), *Z->Name, Z->LevelMin, Z->LevelMax, *Z->Blurb)); Ids.AddUnique(Z->Name); }
		for (const FHLDungeonInfo& Dn : D.Dungeons) if (S.Level >= Dn.LevelMin - 2 && S.Level <= Dn.LevelMax + 2 && World.Num() < 9) { World.AddUnique(Dn.Blurb); Ids.AddUnique(Dn.Name); }
	}
	Req->SetArrayField(TEXT("speakers"), Sp);
	TSharedRef<FJsonObject> Trig = MakeShared<FJsonObject>();
	Trig->SetStringField(TEXT("kind"), TrigKind); Trig->SetStringField(TEXT("channel"), Channel); Trig->SetStringField(TEXT("from"), TEXT("player")); Trig->SetStringField(TEXT("text"), Text);
	Req->SetObjectField(TEXT("trigger"), Trig);
	TSharedRef<FJsonObject> Ctx = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> Pl = MakeShared<FJsonObject>();
	Pl->SetStringField(TEXT("name"), Player.Name); Pl->SetNumberField(TEXT("level"), P ? P->Level : 1); Pl->SetStringField(TEXT("class"), Player.Cls ? Player.Cls->Name : FString());
	if (const FHLGuild* Gd = PlayerGuild()) Pl->SetStringField(TEXT("guild"), Gd->Name);
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
	for (int32 i = Chat.Num() - 1; i >= 0 && Recent.Num() < 8; --i) { const FHLChatLine& L = Chat[i]; if (L.Ch == TEXT("combat") || L.Ch == TEXT("system") || L.Ch == TEXT("loot") || L.Ch == TEXT("quest") || L.From.IsEmpty()) continue; Recent.Insert(FString::Printf(TEXT("[%s] %s: %s"), *L.Ch, *L.From, *L.Text), 0); }
	Ctx->SetArrayField(TEXT("recent_chat"), Strs(Recent)); Ctx->SetArrayField(TEXT("world"), Strs(World)); Ctx->SetArrayField(TEXT("events"), Strs(Events));
	Req->SetObjectField(TEXT("context"), Ctx);
	TSharedRef<FJsonObject> Allowed = MakeShared<FJsonObject>();
	Allowed->SetArrayField(TEXT("goals"), Strs({ TEXT("GROUP"), TEXT("FOLLOW"), TEXT("IDLE") })); Allowed->SetArrayField(TEXT("biases"), Strs({ TEXT("heal_priority") })); Allowed->SetArrayField(TEXT("ids"), Strs(Ids));
	Req->SetObjectField(TEXT("allowed"), Allowed);
	FString Body;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	FJsonSerializer::Serialize(Req, Writer);
	++WorkerInFlight; ++WorkerCalls;
	HLBridge::Fetch(TEXT("POST"), ApiBase + TEXT("/v1/sim"), Body, [this, Done](const FHLBridgeResult& R)
	{
		--WorkerInFlight;
		TSharedPtr<FJsonObject> O;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(R.Body);
		if (R.Ok() && FJsonSerializer::Deserialize(Reader, O) && O.IsValid()) { Done(true, O); return; }
		++WorkerFails;
		if (R.Status == 401) { Session.Reset(); bWorkerOk = false; NextSessionTry = Now + 5; }
		else if (R.Status == 429 || R.Status == 0) { bWorkerOk = false; Session.Reset(); NextSessionTry = Now + 60; } // corpus mode, retry in a minute
		Done(false, nullptr);
	}, 6.f);
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
		if (A->TryGetArrayField(TEXT("memory_add"), Mem)) for (auto& M : *Mem) { const FString F = M->AsString(); if (!F.IsEmpty() && Rel.Facts.Num() < 20) Rel.Facts.AddUnique(F); }
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
			const float Delay = FMath::Max((float)DelayMs / 1000.f, Text.Len() * 0.06f) + Stagger;
			Stagger += 1.2f;
			AddChat(Ch, S.P->Handle, Text, SI, Delay);
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
				InviteFromSim = SI; InviteUntil = Now + 40;
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
