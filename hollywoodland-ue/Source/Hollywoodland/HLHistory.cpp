// Server history (bible §11 census): thirty things that happened on this server before the player arrived, built from the
// census (guilds, personas, dungeons) and stable per save. The News tab prints them as the Dome Herald; sims bring them up.
#include "HLGame.h"
#include "HLData.h"

void FHLGame::BuildHistory()
{
	History.Reset();
	const FHLData& D = FHLData::Get();
	if (!D.Personas.Num() || !D.Guilds.Num()) return;
	FRandomStream R(GetTypeHash(Player.PlayerId) ^ 0x5eed);
	auto Pick = [&](auto& Arr) -> decltype(auto) { return Arr[R.RandRange(0, Arr.Num() - 1)]; };
	TArray<const FHLPersona*> Cap, Any;
	for (const FHLPersona& P : D.Personas) { Any.Add(&P); if (P.Level >= 60) Cap.Add(&P); }
	if (!Cap.Num()) Cap = Any;
	TArray<const FHLGuild*> Raid;
	for (const FHLGuild& G : D.Guilds) if (G.Kind == TEXT("raid")) Raid.Add(&G);
	if (!Raid.Num()) for (const FHLGuild& G : D.Guilds) Raid.Add(&G);
	TArray<const FHLDungeonInfo*> Big, Small;
	for (const FHLDungeonInfo& Dg : D.Dungeons) (Dg.Size > 5 ? Big : Small).Add(&Dg);
	if (!Big.Num()) for (const FHLDungeonInfo& Dg : D.Dungeons) Big.Add(&Dg);
	TArray<const FHLZoneInfo*> Zones;
	for (const FHLZoneInfo& Z : D.Zones) Zones.Add(&Z);
	static const TCHAR* Films[] = { TEXT("The Thief of Bagdad"), TEXT("The Gold Rush"), TEXT("The Phantom of the Opera"), TEXT("The Mark of Zorro"), TEXT("Robin Hood"), TEXT("The Sheik") };
	static const TCHAR* Goods[] = { TEXT("Brass Casings"), TEXT("Scrap Metal"), TEXT(".38 cartridges"), TEXT("Chitin Plates"), TEXT("bandages") };
	auto GuildName = [&](const FHLPersona& P) { const FHLGuild* G = D.Guild(P.Guild); return G ? G->Name : FString(); };
	TSet<FString> Used;
	for (int32 Tries = 0; History.Num() < 30 && Tries < 400; ++Tries)
	{
		FHLHistoryEvent E;
		E.Days = R.RandRange(1, 90);
		const int32 Kind = R.RandRange(0, 10);
		const FHLPersona& A = *Pick(Any);
		const FHLPersona& C = *Pick(Cap);
		switch (Kind)
		{
		case 0: if (Big.Num()) { const FHLGuild& G = *Pick(Raid); const FHLDungeonInfo& Dg = *Pick(Big); const FString Boss = Dg.Bosses.Num() ? Pick(Dg.Bosses) : Dg.Name;
			E.Headline = FString::Printf(TEXT("%s TOPPLES %s"), *G.Name.ToUpper(), *Boss.ToUpper()); E.Body = FString::Printf(TEXT("After a long night in %s, %s brought down %s for the first time on this server."), *Dg.Name, *G.Name, *Boss); E.Ref = FString::Printf(TEXT("%s first killed %s"), *G.Name, *Boss); } break;
		case 1: if (C.Cls) { E.Headline = FString::Printf(TEXT("%s REACHES THE TOP"), *C.Handle.ToUpper()); E.Body = FString::Printf(TEXT("%s became one of the first %ss to reach level 60."), *C.Handle, *C.Cls->Name); E.Ref = FString::Printf(TEXT("%s hit 60"), *C.Handle); } break;
		case 2: { const FHLGuild& G = Pick(D.Guilds); E.Headline = FString::Printf(TEXT("NEW SOCIETY: %s"), *G.Name.ToUpper()); E.Body = FString::Printf(TEXT("%s was founded on the steps of the bank by %s and a handful of friends."), *G.Name, *A.Handle); E.Ref = FString::Printf(TEXT("%s got started"), *G.Name); E.Days = R.RandRange(60, 90); } break;
		case 3: if (Zones.Num()) { const FHLZoneInfo& Z = *Pick(Zones); E.Headline = FString::Printf(TEXT("HIVE BREACH AT %s"), *Z.Name.ToUpper()); E.Body = FString::Printf(TEXT("The wall gave way at %s. %s rallied a line of volunteers and held it until the Dome Patrol arrived."), *Z.Name, *C.Handle); E.Ref = FString::Printf(TEXT("the breach at %s"), *Z.Name); } break;
		case 4: { const int32 N = R.RandRange(3, 9); E.Headline = FString::Printf(TEXT("%s TAKES FIGHT NIGHT AGAIN"), *A.Handle.ToUpper()); E.Body = FString::Printf(TEXT("%s won the Roosevelt bouts for the %d%s time. The house is running out of challengers."), *A.Handle, N, N == 3 ? TEXT("rd") : TEXT("th")); E.Ref = FString::Printf(TEXT("%s winning fight night %d times"), *A.Handle, N); } break;
		case 5: { const TCHAR* Gd = Goods[R.RandRange(0, UE_ARRAY_COUNT(Goods) - 1)]; E.Headline = TEXT("MARKET CORNERED"); E.Body = FString::Printf(TEXT("%s bought up every lot of %s at the Taft Building and relisted them at triple the price. The trade channel has not forgiven."), *A.Handle, Gd); E.Ref = FString::Printf(TEXT("%s cornering %s"), *A.Handle, Gd); } break;
		case 6: if (Raid.Num() > 1 && Big.Num()) { const FHLGuild& G1 = *Pick(Raid); const FHLGuild* G2 = Pick(Raid); if (G2 == &G1) break; const FHLDungeonInfo& Dg = *Pick(Big);
			E.Headline = TEXT("GUILDS FEUD OVER RAID NIGHT"); E.Body = FString::Printf(TEXT("%s and %s both claimed Thursday at %s. Words were exchanged in General; duels outside the Roosevelt followed."), *G1.Name, *G2->Name, *Dg.Name); E.Ref = FString::Printf(TEXT("the %s and %s feud"), *G1.Name, *G2->Name); } break;
		case 7: { const TCHAR* F = Films[R.RandRange(0, UE_ARRAY_COUNT(Films) - 1)]; const int32 N = R.RandRange(40, 90); E.Headline = TEXT("RECORD CROWD AT THE CHINESE"); E.Body = FString::Printf(TEXT("%d players sat through %s on picture night. Nobody was eaten."), N, F); E.Ref = FString::Printf(TEXT("the %s picture night"), F); } break;
		case 8: { const FString G1 = GuildName(A); const FHLGuild& G2 = Pick(D.Guilds); if (G1.IsEmpty() || G1 == G2.Name) break; E.Headline = FString::Printf(TEXT("%s CHANGES COLOURS"), *A.Handle.ToUpper()); E.Body = FString::Printf(TEXT("%s left %s for %s, taking two officers along. Both guilds call it amicable."), *A.Handle, *G1, *G2.Name); E.Ref = FString::Printf(TEXT("%s leaving %s"), *A.Handle, *G1); } break;
		case 9: if (Zones.Num()) { const FHLZoneInfo& Z = *Pick(Zones); E.Headline = TEXT("RED CAR SERVICE RESTORED"); E.Body = FString::Printf(TEXT("The Pacific Electric line to %s runs again after the tracks were cleared of eggs."), *Z.Name); E.Ref = FString::Printf(TEXT("the red car to %s reopening"), *Z.Name); } break;
		case 10: if (Small.Num()) { const FHLDungeonInfo& Dg = *Pick(Small); const int32 Min = R.RandRange(18, 31); E.Headline = FString::Printf(TEXT("%s RUN IN %d MINUTES"), *Dg.Name.ToUpper(), Min); E.Body = FString::Printf(TEXT("%s and four friends ran %s start to finish in %d minutes, a server record."), *C.Handle, *Dg.Name, Min); E.Ref = FString::Printf(TEXT("%s's %d minute %s run"), *C.Handle, Min, *Dg.Name); } break;
		}
		if (E.Headline.IsEmpty() || Used.Contains(E.Headline)) continue;
		Used.Add(E.Headline);
		History.Add(E);
	}
	History.Sort([](const FHLHistoryEvent& A, const FHLHistoryEvent& B) { return A.Days < B.Days; });
	NextHistoryChat = Now + 300 + Rand() * 600;
}

void FHLGame::TickHistory()
{
	if (!History.Num() || Now < NextHistoryChat) return;
	NextHistoryChat = Now + 600 + Rand() * 900;
	const FHLHistoryEvent& E = History[RandInt(0, FMath::Min(History.Num(), 12) - 1)]; // people talk about recent things
	const FHLUnit* P = PlayerUnit();
	EventSay(TEXT("history_ref"), E.Ref, TEXT("zone"), P ? P->Pos : FVector2D::ZeroVector, 1e9f, 1 + Rand() * 5);
}
