#include "HLMusic.h"
#include "HLBridge.h"
#include "HLGame.h"
#include "HLData.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"

namespace
{
	const FHLTrack GTracks[] = {
		{ 1, TEXT("01-hollywoodland-overture.mp3"), TEXT("Hollywoodland Overture"), 145.4f },
		{ 2, TEXT("02-hollywood-boulevard-1930.mp3"), TEXT("Hollywood Boulevard, 1930"), 145.6f },
		{ 3, TEXT("03-grauman-s-chinese-theatre.mp3"), TEXT("Grauman's Chinese Theatre"), 145.6f },
		{ 4, TEXT("04-the-roosevelt-hotel.mp3"), TEXT("The Roosevelt Hotel"), 147.7f },
		{ 5, TEXT("05-hollywood-vine.mp3"), TEXT("Hollywood & Vine"), 148.6f },
		{ 6, TEXT("06-brown-derby-after-dark.mp3"), TEXT("Brown Derby After Dark"), 147.3f },
		{ 7, TEXT("07-the-red-car-express.mp3"), TEXT("The Red Car Express"), 146.1f },
		{ 8, TEXT("08-picture-night-at-the-chinese.mp3"), TEXT("Picture Night at the Chinese"), 146.5f },
		{ 9, TEXT("09-shadows-beneath-the-marquee.mp3"), TEXT("Shadows Beneath the Marquee"), 147.2f },
		{ 10, TEXT("10-the-dome-above-hollywood.mp3"), TEXT("The Dome Above Hollywood"), 148.1f },
		{ 11, TEXT("11-the-selma-avenue-alleys.mp3"), TEXT("The Selma Avenue Alleys"), 144.8f },
		{ 12, TEXT("12-the-hive-takes-root.mp3"), TEXT("The Hive Takes Root"), 146.5f },
		{ 13, TEXT("13-cahuenga-rooftops.mp3"), TEXT("Cahuenga Rooftops"), 148.8f },
		{ 14, TEXT("14-the-hollywood-hotel-ballroom.mp3"), TEXT("The Hollywood Hotel Ballroom"), 147.7f },
		{ 15, TEXT("15-the-projectionist-s-secret.mp3"), TEXT("The Projectionist's Secret"), 147.6f },
		{ 16, TEXT("16-trouble-on-the-boulevard.mp3"), TEXT("Trouble on the Boulevard"), 148.8f },
		{ 17, TEXT("17-attack-of-the-invader-drones.mp3"), TEXT("Attack of the Invader Drones"), 146.9f },
		{ 18, TEXT("18-the-bootlegger-s-run.mp3"), TEXT("The Bootlegger's Run"), 148.4f },
		{ 19, TEXT("19-the-veteran-s-last-stand.mp3"), TEXT("The Veteran's Last Stand"), 147.8f },
		{ 20, TEXT("20-lights-camera-dynamite.mp3"), TEXT("Lights, Camera, Dynamite!"), 144.8f },
		{ 21, TEXT("21-the-hive-surge.mp3"), TEXT("The Hive Surge"), 146.1f },
		{ 22, TEXT("22-the-looter-ringleader.mp3"), TEXT("The Looter Ringleader"), 148.5f },
		{ 23, TEXT("23-enter-the-egyptian.mp3"), TEXT("Enter the Egyptian"), 146.4f },
		{ 24, TEXT("24-the-brood-keeper.mp3"), TEXT("The Brood Keeper"), 146.3f },
		{ 25, TEXT("25-the-hive-warden.mp3"), TEXT("The Hive Warden"), 148.8f },
		{ 26, TEXT("26-the-spitter-matriarch.mp3"), TEXT("The Spitter Matriarch"), 146.9f },
		{ 27, TEXT("27-the-egyptian-hive-mother.mp3"), TEXT("The Egyptian Hive Mother"), 145.4f },
		{ 28, TEXT("28-the-last-reel.mp3"), TEXT("The Last Reel"), 147.8f },
		{ 29, TEXT("29-victory-at-the-egyptian.mp3"), TEXT("Victory at the Egyptian"), 145.6f },
		{ 30, TEXT("30-when-hollywood-falls-silent.mp3"), TEXT("When Hollywood Falls Silent"), 148.4f },
	};
	const TCHAR* SettingsKey = TEXT("hollywoodland_settings");

	// Ambient playlists by subzone (day / night). Unlisted subzones use the zone default.
	struct FPlaylist { const TCHAR* Subzone; TArray<int32> Day, Night; };
	const TArray<FPlaylist>& Playlists()
	{
		static const TArray<FPlaylist> L = {
			{ TEXT("Grauman's Chinese Forecourt"), { 3, 4, 2 }, { 8, 3, 4 } },
			{ TEXT("Hollywood & Highland"), { 2, 4 }, { 4, 8, 30 } },
			{ TEXT("Hollywood & Vine"), { 5, 2 }, { 6, 5 } },
			{ TEXT("Hollywood Police Station"), { 9 }, { 9, 30 } },
			{ TEXT("Franklin Slopes"), { 2, 11, 10 }, { 30, 9, 10 } },
			{ TEXT("Selma Avenue Alleys"), { 11, 9 }, { 11, 9 } },
			{ TEXT("Cahuenga Rooftops"), { 13 }, { 13, 9 } },
			{ TEXT("Taft Back Lots"), { 15, 20 }, { 15 } },
			{ TEXT("Hollywood High School"), { 12 }, { 12 } },
			{ TEXT("Hollywood Hotel Ballroom"), { 14 }, { 14 } },
			{ TEXT("The Egyptian Theatre"), { 23 }, { 23, 28 } },
			{ TEXT("Sunset Boulevard"), { 2, 7 }, { 6, 30 } },
			{ TEXT("The Hive Wall"), { 10, 12 }, { 10, 12 } },
			// Egyptian Theatre dungeon
			{ TEXT("Forecourt"), { 23 }, { 23 } },
			{ TEXT("Lobby"), { 23 }, { 23 } },
			{ TEXT("Auditorium"), { 23, 28 }, { 23, 28 } },
			{ TEXT("Projection Booth"), { 28 }, { 28 } },
			{ TEXT("Stage"), { 23, 28 }, { 23, 28 } },
		};
		return L;
	}
	const TArray<int32> ZoneDay = { 2, 11, 9, 10 }, ZoneNight = { 30, 6, 9, 10 }, DungeonDefault = { 23, 28 };

	int32 Prio(int32 N) { return N >= 24 && N <= 27 ? 3 : (N == 22 || N == 18 || N == 14) ? 2 : N ? 1 : 0; }
}

const FHLTrack* FHLMusic::Track(int32 N) { return N >= 1 && N <= UE_ARRAY_COUNT(GTracks) ? &GTracks[N - 1] : nullptr; }

void FHLMusic::Init(bool bMuteByDefault)
{
#if PLATFORM_WASM
	Folder = HLBridge::AudioBase() + TEXT("music/");
#else
	for (const FString& Dir : { FPaths::ProjectDir() / TEXT("Music"), FPaths::ProjectDir() / TEXT("../music") })
	{
		FString Full = FPaths::ConvertRelativePathToFull(Dir) + TEXT("/");
		if (FPaths::FileExists(Full + GTracks[0].File)) { FPaths::MakePlatformFilename(Full); Folder = Full; break; }
	}
	if (Folder.IsEmpty()) HLBridge::Log(TEXT("HL music: no music folder found (expected <ProjectDir>/Music or <repo>/music)"));
#endif
	if (bMuteByDefault) { Volume = 0; bLoaded = true; return; }
	HLBridge::StorageGet(SettingsKey, [this](const FHLBridgeResult& R)
	{
		TSharedPtr<FJsonObject> O;
		if (R.Ok() && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(R.Body), O) && O.IsValid())
		{
			double V = 0.5;
			if (O->TryGetNumberField(TEXT("music"), V)) Volume = FMath::Clamp((float)V, 0.f, 1.f);
		}
		bLoaded = true;
	});
}

void FHLMusic::SetVolume(float V, bool bSave)
{
	Volume = FMath::Clamp(V, 0.f, 1.f);
	HLBridge::MusicVolume(Volume);
	if (bSave) HLBridge::StorageSet(SettingsKey, FString::Printf(TEXT("{\"music\":%.2f}"), Volume));
}

FString FHLMusic::NowPlaying() const
{
	const FHLTrack* T = Track(Cur);
	return T ? T->Title : FString();
}

void FHLMusic::Skip()
{
	if (!Cur) return;
	Stop(2);
	SilenceUntil = Clock + 2;
}

void FHLMusic::Play(int32 N, bool bLoop, float FadeS)
{
	const FHLTrack* T = Track(N);
	if (!T || Folder.IsEmpty()) return;
	HLBridge::MusicPlay(Folder + T->File, bLoop, FadeS, Volume);
	HLBridge::Log(FString::Printf(TEXT("HL music: %s%s"), T->Title, bLoop ? TEXT(" (loop)") : TEXT("")));
	Cur = N; Last = N; bCurLoop = bLoop;
	EndsAt = Clock + T->Seconds - 3.0;
}

void FHLMusic::Stop(float FadeS)
{
	if (Cur) HLBridge::MusicStop(FadeS);
	Cur = 0;
}

int32 FHLMusic::CombatTrack(FHLGame& G)
{
	const FHLUnit* P = G.PlayerUnit();
	if (!P || P->bDead || !G.InCombat(*P)) return 0;
	TArray<int32, TInlineAllocator<6>> Friends = { P->Id };
	for (int32 SI : G.Party) if (G.Sims.IsValidIndex(SI) && G.Sims[SI].UnitId) Friends.Add(G.Sims[SI].UnitId);
	int32 Insects = 0, Others = 0, Named = 0;
	bool bEliteInsect = false, bEliteOther = false;
	for (const FHLUnit* U : G.UnitList)
	{
		if (U->Kind != EHLKind::Mob || U->bDead || !U->Mob || U->Area != P->Area || !G.InCombat(*U)) continue;
		if (FVector2D::Distance(U->Pos, P->Pos) > 50) continue;
		bool bEngaged = Friends.Contains(U->TargetId);
		for (int32 F : Friends) if (!bEngaged && U->Threat.Contains(F)) bEngaged = true;
		if (!bEngaged) continue;
		const FHLMobDef& M = *U->Mob;
		if (M.Rank == TEXT("boss"))
		{
			if (M.Script == TEXT("brood_keeper")) return 24;
			if (M.Script == TEXT("hive_warden")) return 25;
			if (M.Script == TEXT("matriarch")) return 26;
			return 27;
		}
		if (M.Id == TEXT("mob_looter_ringleader")) Named = 22;
		else if (M.Family == TEXT("dragna_enforcer")) Named = 18;
		else if (M.Id == TEXT("mob_ballroom_brute")) Named = 14;
		const bool bInsect = M.Family.StartsWith(TEXT("invader"));
		const bool bElite = M.Rank == TEXT("elite") || M.Rank == TEXT("rare");
		(bInsect ? Insects : Others)++;
		if (bElite) (bInsect ? bEliteInsect : bEliteOther) = true;
	}
	if (Named) return Named;
	if (G.Areas[P->Area].Def->bDungeon) return (bEliteInsect || bEliteOther || Insects + Others >= 2) ? 19 : 0;
	if (Insects >= 5) return 21;
	if (bEliteInsect || Insects >= 3) return 17;
	if (bEliteOther || Insects + Others >= 3) return G.CurSubzone == TEXT("Taft Back Lots") ? 20 : 16;
	return 0;
}

int32 FHLMusic::Pick(FHLGame& G, bool& bLoop, bool& bAmbient, TArray<int32>& Ambient)
{
	bLoop = true; bAmbient = false;
	const FHLUnit* P = G.PlayerUnit();
	if (!G.bInWorld || !P) return 1;
	// victory fanfare once per clear
	if (G.bDungeonCleared && !bWasCleared) VictoryUntil = Clock + Track(29)->Seconds - 4; // then crossfade into ambient
	bWasCleared = G.bDungeonCleared;
	if (Clock < VictoryUntil) { bLoop = false; return 29; }
	if (P->bDead) { CombatCur = 0; return 30; }
	const int32 C = CombatTrack(G);
	if (C && Prio(C) > Prio(CombatCur)) CombatCur = C;
	if (C) CombatHoldUntil = Clock + 6;
	if (CombatCur && Clock < CombatHoldUntil) return CombatCur;
	CombatCur = 0;

	bAmbient = true; bLoop = false;
	const int32 Hour = FDateTime::Now().GetHour();
	const bool bNight = Hour >= 20 || Hour < 6;
	const FHLAreaState& A = G.Areas[P->Area];
	for (const FHLObjectState& O : A.Objects)
		if (O.Def && O.Def->Kind == TEXT("redcar") && FVector2D::Distance(O.Def->Pos, P->Pos) < 18) { Ambient = { 7 }; return 0; }
	const FString Sz = StableSubzone.Mid(StableSubzone.Find(TEXT("|")) + 1);
	for (const FPlaylist& L : Playlists()) if (Sz == L.Subzone) { Ambient = bNight ? L.Night : L.Day; return 0; }
	Ambient = A.Def->bDungeon ? DungeonDefault : (bNight ? ZoneNight : ZoneDay);
	return 0;
}

void FHLMusic::Tick(FHLGame& G, float Dt)
{
	Clock += Dt;
	HLBridge::MusicTick(Dt);
	if (!bLoaded || Clock < NextThink) return;
	NextThink = Clock + 0.5;
	if (Volume <= 0.001f || Folder.IsEmpty()) { Stop(0.5f); return; }

	const FHLUnit* P = G.PlayerUnit();
	const FString Seen = G.bInWorld && P ? FString::Printf(TEXT("%d|%s"), P->Area, *G.CurSubzone) : FString();
	if (Seen != SeenSubzone) { SeenSubzone = Seen; SubzoneSince = Clock; }
	const bool bZoneChanged = StableSubzone != SeenSubzone && (Clock - SubzoneSince >= 3 || StableSubzone.IsEmpty() || Seen.IsEmpty());
	if (bZoneChanged) StableSubzone = SeenSubzone;

	bool bLoop = true, bAmbient = false;
	TArray<int32> Ambient;
	const int32 Want = Pick(G, bLoop, bAmbient, Ambient);
	if (!bAmbient)
	{
		if (Want != Cur) { Play(Want, bLoop, Prio(Want) >= 2 || Want == 29 ? 1.0f : 1.5f); bCurAmbient = false; }
		return;
	}
	if (Cur && bCurAmbient && Ambient.Contains(Cur))
	{
		if (Clock >= EndsAt) { Stop(3); SilenceUntil = Clock + 3 + FMath::FRandRange(20.f, 45.f); }
		return;
	}
	// Coming from combat/title, or this subzone doesn't play the current track, or a silence gap: start the next pick.
	const bool bSwitch = Cur != 0 || bZoneChanged || Clock >= SilenceUntil;
	if (!bSwitch) return;
	TArray<int32> Choices = Ambient;
	if (Choices.Num() > 1) Choices.Remove(Last);
	const int32 N = Choices[FMath::RandRange(0, Choices.Num() - 1)];
	Play(N, false, Cur ? 3.0f : 1.5f);
	bCurAmbient = true;
}
