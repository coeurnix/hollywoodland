#include "HLTerrain.h"
#include "HLData.h"

// FHLTerrain::Margin: how far the floor reaches past the area bounds (AHLWorldActor::BuildWorld draws it that wide).
static constexpr float FloorMargin = FHLTerrain::Margin;

void FHLTerrain::Build(const FHLAreaDef& Def)
{
	H.Reset(); Wild.Reset(); NX = NY = 0;
	if (Def.bDungeon) return;
	Origin = Def.Min - FVector2D(FloorMargin);
	const FVector2D Ext = Def.Size + FVector2D(FloorMargin * 2);
	// at most ~380 vertices a side (a 2.7 km floor at about 7 m)
	Cell = FMath::Max(5.f, FMath::Max(Ext.X, Ext.Y) / 380.f);
	NX = FMath::CeilToInt32(Ext.X / Cell) + 1;
	NY = FMath::CeilToInt32(Ext.Y / Cell) + 1;
	const int32 N = NX * NY;

	// ---- made ground stays level: mark it, then measure every vertex's distance to it
	TArray<uint8> Flat; Flat.SetNumZeroed(N);
	auto MarkRect = [&](float X0, float Y0, float X1, float Y1)
	{
		const int32 I0 = FMath::Clamp(FMath::FloorToInt32((X0 - Origin.X) / Cell), 0, NX - 1), I1 = FMath::Clamp(FMath::CeilToInt32((X1 - Origin.X) / Cell), 0, NX - 1);
		const int32 J0 = FMath::Clamp(FMath::FloorToInt32((Y0 - Origin.Y) / Cell), 0, NY - 1), J1 = FMath::Clamp(FMath::CeilToInt32((Y1 - Origin.Y) / Cell), 0, NY - 1);
		for (int32 j = J0; j <= J1; ++j) for (int32 i = I0; i <= I1; ++i) Flat[j * NX + i] = 1;
	};
	auto MarkAt = [&](const FVector2D& P, float R) { MarkRect(P.X - R, P.Y - R, P.X + R, P.Y + R); };
	for (const FHLBox& B : Def.Ground)
		if (B.Kind != TEXT("Blight") && B.Kind != TEXT("Grass") && B.Kind != TEXT("Lot")) MarkRect(B.X - 1, B.Y - 1, B.X + B.W + 1, B.Y + B.D + 1);
	for (const FHLBox& B : Def.Boxes) MarkRect(B.X - 3, B.Y - 3, B.X + B.W + 3, B.Y + B.D + 3);
	for (const FBox2f& C : Def.Cols) MarkRect(C.Min.X - 1.5f, C.Min.Y - 1.5f, C.Max.X + 1.5f, C.Max.Y + 1.5f);
	// dressing out past the bounds (the HOLLYWOODLAND sign on the north ridge) stands on the hills instead of levelling them
	const FVector2D Lo = Def.Min, Hi = Def.Min + Def.Size;
	for (const FHLPropPlace& P : Def.Props) if (P.Pos.X >= Lo.X && P.Pos.Y >= Lo.Y && P.Pos.X <= Hi.X && P.Pos.Y <= Hi.Y) MarkAt(P.Pos, 2.5f);
	for (const FHLNpcDef& Np : Def.Npcs) MarkAt(Np.Pos, 5.f);
	for (const FHLPortal& Po : Def.Portals) MarkAt(Po.Pos, 6.f);
	for (const FHLAidStation& A : Def.Aid) MarkAt(A.Pos, 6.f);
	for (const auto& KV : Def.Pois) MarkAt(KV.Value, 5.f);
	MarkAt(Def.Spawn, 8.f);

	// two-pass chamfer distance (3 / 4 steps), in vertex units * 3
	TArray<int32> Dist; Dist.SetNumUninitialized(N);
	for (int32 k = 0; k < N; ++k) Dist[k] = Flat[k] ? 0 : 1 << 28;
	auto Relax = [&](int32 i, int32 j, int32 di, int32 dj, int32 W)
	{
		const int32 a = i + di, b = j + dj;
		if (a < 0 || b < 0 || a >= NX || b >= NY) return;
		Dist[j * NX + i] = FMath::Min(Dist[j * NX + i], Dist[b * NX + a] + W);
	};
	for (int32 j = 0; j < NY; ++j) for (int32 i = 0; i < NX; ++i) { Relax(i, j, -1, 0, 3); Relax(i, j, 0, -1, 3); Relax(i, j, -1, -1, 4); Relax(i, j, 1, -1, 4); }
	for (int32 j = NY - 1; j >= 0; --j) for (int32 i = NX - 1; i >= 0; --i) { Relax(i, j, 1, 0, 3); Relax(i, j, 0, 1, 3); Relax(i, j, 1, 1, 4); Relax(i, j, -1, 1, 4); }

	// ---- heights: low swells in the lots, hills toward the edge (the blight ring and past the bounds)
	H.SetNumUninitialized(N); Wild.SetNumUninitialized(N);
	for (int32 j = 0; j < NY; ++j)
		for (int32 i = 0; i < NX; ++i)
		{
			const int32 k = j * NX + i;
			const float X = Origin.X + i * Cell, Y = Origin.Y + j * Cell;
			const float D = Dist[k] >= (1 << 27) ? 1e4f : Dist[k] * Cell / 3.f; // metres to made ground
			const float Amp = FMath::SmoothStep(3.f, 28.f, D);
			const float N1 = 0.5f + 0.5f * (0.6f * FMath::Sin(0.041f * X + 0.3f) * FMath::Cos(0.035f * Y + 1.1f) + 0.4f * FMath::Sin(0.067f * X - 0.052f * Y + 2.f));
			const float N2 = 0.5f + 0.5f * (0.5f * FMath::Sin(0.13f * X + 0.11f * Y + 0.5f) + 0.5f * FMath::Cos(0.11f * X - 0.15f * Y + 1.7f));
			// distance inside the area bounds (negative past them)
			const float E = FMath::Min(FMath::Min(X - Lo.X, Hi.X - X), FMath::Min(Y - Lo.Y, Hi.Y - Y));
			const float Edge = 1.f - FMath::SmoothStep(-120.f, 230.f, E);
			const float Base = 3.2f * (N1 - 0.3f) + 0.7f * (N2 - 0.5f);
			// past the hive ring the ground climbs into a ridge of hills that closes the horizon, highest in the north
			// (the Hollywood Hills: the sign stands up there)
			const float Ridge = FMath::SmoothStep(60.f, 380.f, -E) * (22.f + 30.f * N1 + 10.f * N2) * (Y < Lo.Y ? 1.5f : 1.f);
			H[k] = Amp * (Base + Edge * (5.f + 9.f * N1) + Ridge);
			Wild[k] = FMath::SmoothStep(1.f, 8.f, D); // scatter density: grass reaches nearly to the kerb, ahead of the swells
		}
}

float FHLTerrain::Sample(const TArray<float>& F, const FVector2D& P) const
{
	if (!IsValid() || F.Num() != NX * NY) return 0.f;
	const float Fx = FMath::Clamp((P.X - Origin.X) / Cell, 0.f, NX - 1.001f), Fy = FMath::Clamp((P.Y - Origin.Y) / Cell, 0.f, NY - 1.001f);
	const int32 I = FMath::FloorToInt32(Fx), J = FMath::FloorToInt32(Fy);
	const float A = Fx - I, B = Fy - J;
	const float H00 = F[J * NX + I], H10 = F[J * NX + I + 1], H01 = F[(J + 1) * NX + I], H11 = F[(J + 1) * NX + I + 1];
	return FMath::Lerp(FMath::Lerp(H00, H10, A), FMath::Lerp(H01, H11, A), B);
}

FVector FHLTerrain::Normal(const FVector2D& P) const
{
	const float E = Cell;
	const float Dx = Height(P + FVector2D(E, 0)) - Height(P - FVector2D(E, 0));
	const float Dy = Height(P + FVector2D(0, E)) - Height(P - FVector2D(0, E));
	return FVector(-Dx, -Dy, 2.f * E).GetSafeNormal();
}
