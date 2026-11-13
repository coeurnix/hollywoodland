// Spoken dialogue: NPC barks and greetings, quest dialogue, player barks and emotes. Text comes from
// Content/Data/dialogue.json and audio from vo.json (both written by tools/dialoguegen); line n of a kind is
// audio variant n, so the subtitle always matches the voice. Everything degrades to silence when vo.json is absent.
#include "HLGame.h"
#include "HLData.h"

namespace
{
	// guards share one script in three voices: variant = line * 3 + voice
	int32 GuardVoice(const FHLNpcDef& N) { return N.Id.Len() ? (N.Id[N.Id.Len() - 1] - TEXT('0') + 30) % 3 : 0; }
	bool IsGuard(const FHLNpcDef& N) { return N.HasRole(TEXT("guard")); }
	FString Fill(FString S, const FString& Name, const FString& Target) { S.ReplaceInline(TEXT("{name}"), *Name); S.ReplaceInline(TEXT("{target}"), *Target); return S; }
}

int32 FHLGame::Say(const FHLUnit& Speaker, const FHLNpcDef& Npc, const TCHAR* Kind, int32 Line, bool bChat)
{
	const FHLLines* L = FHLData::Get().LinesFor(Npc);
	const TArray<FString>* Arr = L ? L->Get(Kind) : nullptr;
	if (!Arr) return -1;
	const int32 N = Arr->IsValidIndex(Line) ? Line : RandInt(0, Arr->Num() - 1);
	if (bChat) AddChat(TEXT("npc"), Speaker.Name, (*Arr)[N]);
	const FString Id = FString::Printf(TEXT("vo_%s_%s"), *FHLData::NpcSpeaker(Npc), Kind);
	VoiceAt(Id, IsGuard(Npc) ? N * 3 + GuardVoice(Npc) : N, &Speaker);
	return N;
}

void FHLGame::SayQuest(const FHLQuest& Q, const TCHAR* Kind)
{
	if (!Q.bVoiced) return;
	const FHLUnit* N = Unit(OpenNpcUnitId);
	VoiceAt(FString::Printf(TEXT("vo_%s_%s"), *Q.Id, Kind), 0, N);
}

FString FHLGame::PlayerVoiceKey() const
{
	return Player.Cls ? FString::Printf(TEXT("%s_%s"), *Player.Cls->Id, Player.Look.Sex ? TEXT("f") : TEXT("m")) : FString();
}

void FHLGame::PlayerBark(const TCHAR* Kind, float MinGapS)
{
	const FHLLines* L = FHLData::Get().PlayerLines.Find(PlayerVoiceKey());
	const TArray<FString>* Arr = L ? L->Get(Kind) : nullptr;
	const FHLUnit* P = PlayerUnit();
	if (!Arr || !P) return;
	double& At = BarkAt.FindOrAdd(Kind);
	if (Now - At < MinGapS) return;
	At = Now;
	VoiceAt(FString::Printf(TEXT("vo_pc_%s_%s"), *PlayerVoiceKey(), Kind), RandInt(0, Arr->Num() - 1), P);
}

void FHLGame::ErrorBark(const FString& T)
{
	// the stock error texts (HLCombat CanCast, HLPlayer) -> the character says it
	if (T == TEXT("You have no target.") || T == TEXT("You need a hostile target.")) PlayerBark(TEXT("no_target"));
	else if (T == TEXT("Out of range.") || T == TEXT("You are too far away.")) PlayerBark(TEXT("out_of_range"));
	else if (T.StartsWith(TEXT("Not enough "))) PlayerBark(TEXT("no_power"));
	else if (T == TEXT("That ability isn't ready yet.") || T.StartsWith(TEXT("Your pockets need a moment"))) PlayerBark(TEXT("not_ready"));
	else if (T == TEXT("Your bags are full.")) PlayerBark(TEXT("bags_full"));
	else if (T == TEXT("Target not in line of sight.") || T.StartsWith(TEXT("You can't")) || T.StartsWith(TEXT("You don't have enough"))) PlayerBark(TEXT("cant"));
}

bool FHLGame::DoEmote(const FString& Cmd, const FString& Rest)
{
	const FHLEmote* E = FHLData::Get().Emote(Cmd.ToLower());
	if (!E) return false;
	FHLUnit* P = PlayerUnit();
	const FHLUnit* T = nullptr;
	if (!Rest.IsEmpty()) { for (FHLUnit* U : UnitList) if (U->Name.Equals(Rest.TrimStartAndEnd(), ESearchCase::IgnoreCase) && P && U->Area == P->Area) { T = U; break; } }
	else if (P) T = Unit(P->TargetId);
	if (T == P) T = nullptr;
	const FString Line = T ? Fill(E->SelfTarget, Player.Name, T->Name) : Fill(E->SelfAlone, Player.Name, FString());
	AddChat(TEXT("emote"), FString(), Line);
	if (P)
	{
		P->bSitting = false;
		if (!E->Anim.IsEmpty()) Gesture(*P, *E->Anim);
		if (T) P->Yaw = FMath::RadiansToDegrees(FMath::Atan2(T->Pos.Y - P->Pos.Y, T->Pos.X - P->Pos.X));
	}
	if (!E->Voice.IsEmpty())
	{
		// jokes and pickup lines are also said out loud in chat; the vocal ones (laugh, train...) are just heard
		const FHLLines* L = FHLData::Get().PlayerLines.Find(PlayerVoiceKey());
		const TArray<FString>* Arr = L ? L->Get(*E->Voice) : nullptr;
		if (Arr && P)
		{
			const int32 N = RandInt(0, Arr->Num() - 1);
			if (E->Voice == TEXT("silly") || E->Voice == TEXT("flirt") || E->Voice == TEXT("charge")) AddChat(TEXT("say"), Player.Name, (*Arr)[N]);
			VoiceAt(FString::Printf(TEXT("vo_pc_%s_%s"), *PlayerVoiceKey(), *E->Voice), N, P);
		}
	}
	return true;
}

void FHLGame::AskDirections(const FString& Poi)
{
	const FHLUnit* G = Unit(OpenNpcUnitId);
	const FHLUnit* P = PlayerUnit();
	if (!G || !G->Npc || !P) return;
	const FHLLines* L = FHLData::Get().LinesFor(*G->Npc);
	const FString* Line = L ? L->Directions.Find(Poi) : nullptr;
	const FVector2D* At = Areas[P->Area].Def->Pois.Find(Poi);
	if (Line) AddChat(TEXT("npc"), G->Name, *Line);
	VoiceAt(FString::Printf(TEXT("vo_npc_guard_dir_%s"), *Poi), GuardVoice(*G->Npc), G);
	if (At)
	{
		SetMark(P->Area, *At, PoiName(Poi));
		System(FString::Printf(TEXT("%s is marked on your map (M)."), *PoiName(Poi)));
	}
}

FString FHLGame::PoiName(const FString& Poi)
{
	static const TMap<FString, FString> Names = { { TEXT("bank"), TEXT("First National Bank") }, { TEXT("auction"), TEXT("Auction House") }, { TEXT("inn"), TEXT("Roosevelt Hotel") },
		{ TEXT("trainers"), TEXT("Class Trainers") }, { TEXT("redcar"), TEXT("Red Car Stop") }, { TEXT("vendor"), TEXT("General Goods") }, { TEXT("mailbox"), TEXT("Mailbox") },
		{ TEXT("police"), TEXT("Police Station") }, { TEXT("repair"), TEXT("Repairs") }, { TEXT("prof"), TEXT("Profession Trainers") } };
	const FString* Nm = Names.Find(Poi);
	return Nm ? *Nm : Poi;
}

FString FHLGame::PoiQuestion(const FString& Poi)
{
	static const TMap<FString, FString> Q = { { TEXT("bank"), TEXT("Where's the bank?") }, { TEXT("auction"), TEXT("Where's the auction house?") }, { TEXT("inn"), TEXT("Where can I get a room?") },
		{ TEXT("trainers"), TEXT("Where can I find a trainer?") }, { TEXT("redcar"), TEXT("Where do I catch the red car?") }, { TEXT("vendor"), TEXT("Where can I buy supplies?") },
		{ TEXT("mailbox"), TEXT("Where's the mailbox?") }, { TEXT("police"), TEXT("Where's the police station?") } };
	const FString* S = Q.Find(Poi);
	return S ? *S : FString::Printf(TEXT("Where's the %s?"), *Poi);
}

void FHLGame::TickDialogue()
{
	// the NPC window changed: say the line that goes with what it shows now
	const FHLUnit* N = Unit(OpenNpcUnitId);
	if (OpenNpcUnitId != DlgUnit || OpenNpcMode != DlgMode)
	{
		const FHLUnit* Was = Unit(DlgUnit);
		if (!OpenNpcUnitId && Was && Was->Npc && !Was->bDead) Say(*Was, *Was->Npc, TEXT("bye"), -1, false);
		else if (N && N->Npc && OpenNpcUnitId == DlgUnit)
		{
			const FString& M = OpenNpcMode;
			if (M.StartsWith(TEXT("quest:")))
			{
				if (const FHLQuest* Q = FHLData::Get().Quest(M.Mid(6)))
					SayQuest(*Q, QuestReadyAt(*Q, N->Npc->Id) ? TEXT("complete") : QuestState(Q->Id) ? TEXT("progress") : TEXT("offer"));
			}
			else if (M == TEXT("vendor")) Say(*N, *N->Npc, TEXT("vendor"), -1, false);
			else if (M == TEXT("trainer") || M == TEXT("prof")) Say(*N, *N->Npc, TEXT("trainer"), -1, false);
			else if (M == TEXT("bank")) Say(*N, *N->Npc, TEXT("bank"), -1, false);
			else if (M == TEXT("auction")) Say(*N, *N->Npc, TEXT("auction"), -1, false);
			else if (M == TEXT("redcar")) Say(*N, *N->Npc, TEXT("redcar"), -1, false);
		}
		DlgUnit = OpenNpcUnitId; DlgMode = OpenNpcMode;
	}
	// the character notices when a fight is going badly
	if (const FHLUnit* P = PlayerUnit())
		if (!P->bDead && P->MaxHp > 0 && P->Hp < P->MaxHp * 0.25f && InCombat(*P) && Now - LowHpBarkAt > 30) { LowHpBarkAt = Now; PlayerBark(TEXT("low_health"), 0); }
}
