// Hive spread (bible §14 world events): when the egg field is left unbroken, clusters spread to the next site and
// Broodlings thicken around it until the clusters there are destroyed (by the player, or eventually by other players).
#include "HLGame.h"
#include "HLData.h"

static bool HiveFastTest() { static const bool b = FParse::Param(FCommandLine::Get(), TEXT("HLLifeTest")) || FParse::Param(FCommandLine::Get(), TEXT("HLHiveTest")); return b; }

int32 FHLGame::HiveEggsLeft(int32 Site) const
{
	int32 N = 0;
	for (const FHLObjectState& O : Areas[0].Objects) if (O.Def->Spread == Site && O.bActive) ++N;
	return N;
}

FVector2D FHLGame::HiveSitePos(int32 Site) const
{
	FVector2D Sum = FVector2D::ZeroVector; int32 N = 0;
	for (const FHLObjectState& O : Areas[0].Objects) if (O.Def->Spread == Site) { Sum += O.Def->Pos; ++N; }
	return N ? Sum / N : FVector2D::ZeroVector;
}

FString FHLGame::HiveSiteName(int32 Site) const
{
	const FString Sz = SubzoneAt(0, HiveSitePos(Site));
	return Sz.IsEmpty() || Sz == Areas[0].Def->Name ? FString(TEXT("the southern hive grounds")) : Sz;
}

void FHLGame::HiveEggBroken(int32 Site, bool bByPlayer)
{
	if (!HiveSites.IsValidIndex(Site) || !HiveSites[Site].bActive) return;
	FHLUnit* P = PlayerUnit();
	if (bByPlayer) { HiveSites[Site].PlayerBroke++; if (P && P->Level < 60) GrantXp(FMath::Max(10, XpToNext(P->Level) / 40), true); }
	if (HiveEggsLeft(Site) > 0) return;
	HiveSites[Site].bActive = false;
	const FString Name = HiveSiteName(Site);
	System(FString::Printf(TEXT("The egg clusters near %s are destroyed. The hive pulls back."), *Name));
	UE_LOG(LogTemp, Display, TEXT("HL_LIFE hive site %d cleared (%s)"), Site, bByPlayer ? TEXT("player") : TEXT("sims"));
	if (bByPlayer && HiveSites[Site].PlayerBroke >= 2 && P && P->Level < 60)
	{
		const int32 Cents = 8 * P->Level;
		Player.Cents += Cents;
		System(FString::Printf(TEXT("The Dome Patrol pays a bounty of %s for clearing the spread."), *HLMoney(Cents)));
		GrantXp(XpToNext(P->Level) / 10, true);
	}
	EventSay(TEXT("hive_cleared"), Name, TEXT("zone"), HiveSitePos(Site), 1e9f, 2 + Rand() * 3);
}

void FHLGame::TickHive()
{
	if (Now < NextHiveTick || !Areas.Num()) return;
	NextHiveTick = Now + 5;
	FHLAreaState& A = Areas[0];
	if (HiveSites.Num() == 0)
	{
		int32 MaxSite = 0;
		for (FHLObjectState& O : A.Objects) if (O.Def->Spread > 0) { O.bActive = false; MaxSite = FMath::Max(MaxSite, O.Def->Spread); }
		HiveSites.SetNum(MaxSite + 1);
	}
	const bool bFast = HiveFastTest();
	// pressure builds while most of the egg field stands
	int32 Field = 0, FieldMax = 0;
	for (const FHLObjectState& O : A.Objects) if (O.Def->Kind == TEXT("egg")) { ++FieldMax; if (O.bActive) ++Field; }
	if (FieldMax > 0 && Field >= FieldMax * 7 / 10) HiveGrowth += 5; else HiveGrowth = FMath::Max(0.0, HiveGrowth - 10);
	if (HiveGrowth >= (bFast ? 40 : 420) && Player.PlayedS > (bFast ? 30 : 600))
	{
		HiveGrowth = 0;
		for (int32 s = 1; s < HiveSites.Num(); ++s)
		{
			if (HiveSites[s].bActive) continue;
			HiveSites[s] = FHLHiveSite(); HiveSites[s].bActive = true; HiveSites[s].Since = Now;
			for (FHLObjectState& O : A.Objects) if (O.Def->Spread == s) O.bActive = true;
			const FString Name = HiveSiteName(s);
			System(FString::Printf(TEXT("The hive is spreading. Egg clusters have appeared near %s."), *Name));
			Banner(TEXT("Hive Spread"), FString::Printf(TEXT("Egg clusters near %s"), *Name), 4);
			EventSay(TEXT("hive_spread"), Name, TEXT("zone"), HiveSitePos(s), 1e9f, 3 + Rand() * 4);
			UE_LOG(LogTemp, Display, TEXT("HL_LIFE hive spread to site %d (%s), field %d/%d"), s, *Name, Field, FieldMax);
			break;
		}
	}
	// active sites: Broodlings thicken (two per standing cluster), and other players eventually burn old sites
	const FHLMobDef* Brood = FHLData::Get().Mob(TEXT("mob_invader_broodling"));
	const FHLUnit* P = PlayerUnit();
	for (int32 s = 1; s < HiveSites.Num(); ++s)
	{
		FHLHiveSite& H = HiveSites[s];
		H.Mobs.RemoveAll([&](int32 Id) { const FHLUnit* M = Unit(Id); return !M || M->bDead; });
		if (!H.bActive) continue;
		if (HiveEggsLeft(s) == 0) { H.bActive = false; continue; } // the area was reset under us
		const FVector2D At = HiveSitePos(s);
		if (Brood && H.Mobs.Num() < HiveEggsLeft(s) * 2 && Now >= H.NextSpawn)
		{
			H.NextSpawn = Now + (bFast ? 6 : 25);
			FHLUnit* M = SpawnMob(Brood, 0, A.Nav.NearestFree(At + FVector2D(Rand() * 16 - 8, Rand() * 16 - 8)));
			M->bEvent = true; M->Leash = 60; M->Home = M->Pos; InitMobStats(*M);
			H.Mobs.Add(M->Id);
		}
		const bool bPlayerNear = P && P->Area == 0 && FVector2D::Distance(P->Pos, At) < 50;
		if (!bPlayerNear && Now - H.Since > (bFast ? 90 : 600) && Chance(0.08f))
		{
			for (FHLObjectState& O : A.Objects) if (O.Def->Spread == s && O.bActive) { O.bActive = false; break; }
			HiveEggBroken(s, false);
		}
	}
}
