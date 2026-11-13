// Sound effects. The sim queues sound events (FHLGame::Sound / SoundAt / Sound2D); FHLSfx drains them once per frame,
// picks a variant and pitch, attenuates and pans them against the listener (the player's head, facing the camera yaw),
// and runs the managed loops: the ambience bed for the subzone and time of day, casting loops, wings and burrowing,
// spire and egg-sac hums, the crafting bench and the player's footsteps.
// The catalogue is Content/Data/sfx.json, written by tools/gen-sfx.mjs (ElevenLabs SFX v3 through fal.ai). Files are
// <repo>/sfx/<id>_<n>.mp3: the web fetches them from the client Worker (<page>/sfx/<file>) and plays them with WebAudio
// (HLBridge op 6); native decodes them with dr_mp3 and mixes them on a waveOut thread (HLSfx.cpp).
#pragma once
#include "CoreMinimal.h"

class FHLGame;
struct FHLUnit;

struct FHLSoundEvent
{
	FString Id;
	int32 Area = -1;              // -1: 2D (UI, stingers)
	FVector2D Pos = FVector2D::ZeroVector;
	float Z = 0, Vol = 1;
	int32 UnitId = 0;             // throttles repeats per unit (hurt grunts, barks)
};

struct FHLSfxDef
{
	FString Id, Cat;
	TArray<FString> Files;
	TArray<int32> Samples;        // loops: exact 44.1 kHz frame count of each file (MP3 encoder padding is skipped)
	float Vol = 0.6f, Range = 40, Pitch = 0;
	bool bLoop = false;
};

class FHLSfx
{
public:
	void Init(bool bMuteByDefault);
	/** Listener: area, position (metres) and head height, and the camera yaw in degrees (sim axes). */
	void Tick(FHLGame& G, float Dt, int32 Area, const FVector2D& Pos, float Z, float YawDeg);
	void SetVolume(float V, bool bSave = true);
	float GetVolume() const { return Volume; }
	bool Has(const FString& Id) const { return Defs.Contains(Id); }
	/** UI sounds straight from Slate (clicks, windows): 2D, no queue. */
	void Ui(const TCHAR* Id, float Vol = 1.f);
	void StopAll();

private:
	struct FLoop { FString Id; int32 Handle = 0; float Vol = 0, Pan = 0; bool bWanted = false; };
	int32 Play(const FHLSfxDef& D, int32 Variant, float Vol, float Pan, float Rate, float FadeS);
	void Event(FHLGame& G, const FHLSoundEvent& E);
	bool Spatial(int32 Area, const FVector2D& P, float Z, float Range, float& OutGain, float& OutPan) const;
	void WantLoop(const FString& Key, const FString& Id, float Vol, float Pan);
	void WantLoopAt(const FString& Key, const FString& Id, int32 Area, const FVector2D& P, float Z, float VolMul = 1.f);
	void TickLoops(FHLGame& G, float Dt);
	void TickSteps(FHLGame& G, float Dt);
	FString AmbienceFor(FHLGame& G) const;

	TMap<FString, FHLSfxDef> Defs;
	TMap<FString, FLoop> Loops;
	TMap<uint64, double> LastByUnit;
	TMap<FString, int32> ThisFrame;
	FString Folder;              // native: absolute folder with trailing slash; web: "sfx/"
	float Volume = 0.7f;
	int32 NextHandle = 1;
	double Clock = 0;
	// listener
	int32 LArea = -1; FVector2D LPos = FVector2D::ZeroVector; float LZ = 0, LYaw = 0;
	// ambience bed (crossfaded)
	FString AmbId; int32 AmbHandle = 0; double AmbSince = 0; FString AmbWant;
	// player footsteps / jumps
	float StepT = 0; bool bWasAir = false;
	bool bReady = false;
	int32 Plays = 0; double NextStats = 10;
};
