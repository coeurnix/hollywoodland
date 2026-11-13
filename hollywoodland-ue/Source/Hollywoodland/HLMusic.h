// Music director: picks a track from game state (title, subzone, notable fights, bosses, death, victory) and drives
// HLBridge::MusicPlay. Tracks are the 30 MP3s in <repo>/music (ART-SONGS.md). Web streams them from the client Worker
// (<page>/music/<file>); native plays them from <ProjectDir>/Music or <repo>/music. Ambient tracks play once, then a
// silence gap, then the next pick (classic-MMO style); combat, boss and title tracks loop.
#pragma once
#include "CoreMinimal.h"

class FHLGame;

struct FHLTrack { int32 N; const TCHAR* File; const TCHAR* Title; float Seconds; };

class FHLMusic
{
public:
	void Init(bool bMuteByDefault);          // resolves the native folder, loads the saved volume
	void Tick(FHLGame& G, float Dt);
	void SetVolume(float V, bool bSave = true);
	float GetVolume() const { return Volume; }
	void Skip();                             // "/music next": end the current ambient track now
	FString NowPlaying() const;
	static const FHLTrack* Track(int32 N);

private:
	int32 Pick(FHLGame& G, bool& bLoop, bool& bAmbient, TArray<int32>& Ambient);
	int32 CombatTrack(FHLGame& G);
	void Play(int32 N, bool bLoop, float FadeS);
	void Stop(float FadeS);

	float Volume = 0.5f;
	FString Folder;             // native: absolute folder with trailing slash; web: "music/"
	double Clock = 0, EndsAt = 0, SilenceUntil = 0, CombatHoldUntil = 0, VictoryUntil = 0, SubzoneSince = 0, NextThink = 0;
	int32 Cur = 0, Last = 0, CombatCur = 0;
	bool bCurLoop = false, bCurAmbient = false, bWasCleared = false, bLoaded = false;
	FString StableSubzone, SeenSubzone;
};
