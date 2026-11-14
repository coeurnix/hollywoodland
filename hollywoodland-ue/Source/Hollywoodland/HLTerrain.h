// Rolling ground for the open lots (visual only: the simulation stays on its flat 2D map).
//
// Each outdoor area gets a height grid: zero on and next to everything the game treats as made ground (streets,
// sidewalks, building footprints, kit dressing, NPC spots, portals), rising into gentle swells a few metres high
// away from them and into real hills toward the area's outer edge, where the blight ring lies. Roads keep their
// level through the hills. AHLWorldActor::ToWorld adds the height, so people, creatures, props and nameplates all
// stand on it; the floor and the blight patches are drawn as height-mapped meshes.
#pragma once
#include "CoreMinimal.h"

struct FHLAreaDef;

struct FHLTerrain
{
	FVector2D Origin = FVector2D::ZeroVector; // metres, area-local (same space as unit positions)
	float Cell = 5.f;
	int32 NX = 0, NY = 0;                     // vertices per axis
	TArray<float> H;                          // NX * NY heights in metres
	TArray<float> Wild;                       // NX * NY: 0 on made ground .. 1 out in the open (scatter density)

	static constexpr float Margin = 600.f;    // metres the floor reaches past the area bounds (the hills out there)
	bool IsValid() const { return NX > 1 && NY > 1; }
	void Build(const FHLAreaDef& Def);
	float Height(const FVector2D& P) const { return Sample(H, P); }
	float Wildness(const FVector2D& P) const { return Sample(Wild, P); }
	FVector Normal(const FVector2D& P) const;

private:
	float Sample(const TArray<float>& F, const FVector2D& P) const;
};
