// Player settings: key bindings, interface and graphics. One global instance, saved as JSON in browser storage
// (native: Saved/HLStorage) under "hl_settings". Audio volumes live in FHLMusic / FHLSfx, which save themselves.
#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"

enum class EHLAct : uint8
{
	Forward, Back, TurnLeft, TurnRight, StrafeLeft, StrafeRight, Jump,
	Bar1, Bar2, Bar3, Bar4, Bar5, Bar6, Bar7, Bar8, Bar9, Bar10, Bar11, Bar12,
	Target, Interact, Sit, AutoAttack, Reply,
	Character, Bags, Quests, Spellbook, Map, Social, Talents, Workbench, Help,
	Count
};

struct FHLBind { const TCHAR* Label; FKey Key, Alt; };

struct FHLSettings
{
	// keys (two per action; Esc, Enter and / are fixed)
	FHLBind Binds[(int32)EHLAct::Count];
	// interface
	float UiScale = 1.f;          // 0.8 .. 1.3, on top of the fit-to-window scale
	bool bNameplates = true;      // names over other units (target always shown)
	bool bFloatText = true;       // floating combat numbers
	bool bHelpOnStart = true;
	// graphics
	int32 Quality = 1;            // 0 low, 1 medium, 2 high, 3 ultra (presets for the four below)
	int32 Shadows = 1;            // 0 off, 1 characters, 2 characters and the world
	float ViewDist = 1.f;         // 0.6 .. 1.5 x prop/kit draw distance
	int32 ResScale = 100;         // render resolution %, 50 .. 100
	bool bEffects = true;         // combat effects (projectiles always draw)
	int32 Version = 0;            // bumps on every change; the world actor applies graphics when it moves

	FHLSettings();
	static FHLSettings& Get();
	bool Is(const FKey& K, EHLAct A) const { const FHLBind& B = Binds[(int32)A]; return K.IsValid() && (K == B.Key || K == B.Alt); }
	bool Held(const TSet<FKey>& Down, EHLAct A) const { const FHLBind& B = Binds[(int32)A]; return (B.Key.IsValid() && Down.Contains(B.Key)) || (B.Alt.IsValid() && Down.Contains(B.Alt)); }
	FString KeyName(EHLAct A) const; // short label for the micro menu / tooltips ("C", "Shift+1"...)
	void Bind(EHLAct A, const FKey& K, bool bAlt); // clears the key from any other action
	void ResetKeys();
	void ApplyPreset(int32 Q);
	void Changed() { ++Version; Save(); }
	void Load();  // async: storage answers later, then Version moves
	void Save() const;
};
