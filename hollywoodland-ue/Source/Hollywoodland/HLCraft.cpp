// Gunsmithing (recipes, crafting cast, skill-ups) and escort quests (the escorted NPC follows the player, ambushes, failure).
#include "HLGame.h"

// ------------------------------------------------------------------------------------------------------------- gunsmithing
const TArray<FHLGame::FHLRecipe>& FHLGame::GunRecipes()
{
	static TArray<FHLRecipe> R;
	if (R.Num() == 0)
	{
		auto Add = [](const TCHAR* Product, int32 Yield, int32 Skill, int32 Scrap, int32 Brass) { FHLRecipe X; X.Product = Product; X.Yield = Yield; X.Skill = Skill; X.Mat1 = TEXT("Scrap Metal"); X.N1 = Scrap; X.Mat2 = TEXT("Brass Casings"); X.N2 = Brass; R.Add(X); };
		Add(TEXT(".38 Special Lead Round-Nose"), 20, 1, 1, 1);
		Add(TEXT("12-Gauge Birdshot Shells"), 20, 15, 1, 2);
		Add(TEXT("Zip Gun"), 1, 25, 6, 2);
		Add(TEXT(".30-30 Winchester Soft-Point"), 20, 35, 2, 2);
		Add(TEXT(".45 ACP Full Metal Jacket"), 20, 50, 2, 3);
		Add(TEXT("Sawed-off Coach Gun"), 1, 65, 10, 4);
	}
	return R;
}

int32 FHLGame::RecipeColour(const FHLRecipe& R) const
{
	const int32 S = Player.Guns;
	if (S < R.Skill) return -1; // not yet learnable
	if (S < R.Skill + 10) return 3; // orange: always a skill-up
	if (S < R.Skill + 20) return 2; // yellow: usually
	if (S < R.Skill + 30) return 1; // green: sometimes
	return 0; // grey: never
}

int32 FHLGame::CraftableCount(const FHLRecipe& R) const
{
	const FHLData& D = FHLData::Get();
	const FHLItem* A = D.ItemByName(R.Mat1);
	const FHLItem* B = D.ItemByName(R.Mat2);
	int32 N = 999;
	if (A && R.N1 > 0) N = FMath::Min(N, CountItem(A) / R.N1);
	if (B && R.N2 > 0) N = FMath::Min(N, CountItem(B) / R.N2);
	return N;
}

void FHLGame::StartCraft(int32 Idx, int32 Times)
{
	const TArray<FHLRecipe>& Rs = GunRecipes();
	FHLUnit* U = PlayerUnit();
	if (!U || U->bDead || !Rs.IsValidIndex(Idx)) return;
	if (Player.Guns <= 0) { Error(TEXT("You have not learned Gunsmithing.")); return; }
	if (RecipeColour(Rs[Idx]) < 0) { Error(TEXT("Your Gunsmithing skill is too low.")); return; }
	if (CraftableCount(Rs[Idx]) <= 0) { Error(TEXT("Missing materials.")); return; }
	if (U->Casting || InteractObj >= 0) { Error(TEXT("You are busy.")); return; }
	CraftIdx = Idx; CraftLeft = FMath::Max(1, Times); CraftStart = Now; CraftEnd = Now + 2.5;
	U->bSitting = false;
}

void FHLGame::TickCraft()
{
	if (CraftIdx < 0 || Now < CraftEnd) return;
	const TArray<FHLRecipe>& Rs = GunRecipes();
	const FHLData& D = FHLData::Get();
	FHLUnit* U = PlayerUnit();
	if (!U || U->bDead || !Rs.IsValidIndex(CraftIdx)) { CraftIdx = -1; return; }
	const FHLRecipe& R = Rs[CraftIdx];
	const FHLItem* Out = D.ItemByName(R.Product);
	if (!Out || CraftableCount(R) <= 0) { Error(TEXT("Missing materials.")); CraftIdx = -1; return; }
	if (!AddItem(Out, R.Yield, false)) { CraftIdx = -1; return; } // bags full (AddItem reports)
	if (const FHLItem* A = D.ItemByName(R.Mat1)) RemoveItem(A, R.N1);
	if (const FHLItem* B = D.ItemByName(R.Mat2)) RemoveItem(B, R.N2);
	AddChat(TEXT("loot"), FString(), R.Yield > 1 ? FString::Printf(TEXT("You create: [%s] x%d."), *Out->Name, R.Yield) : FString::Printf(TEXT("You create: [%s]."), *Out->Name));
	QuestEvent(TEXT("craft"), Out->Name, R.Yield);
	const int32 Col = RecipeColour(R);
	const float UpChance = Col >= 3 ? 1.f : Col == 2 ? 0.75f : Col == 1 ? 0.25f : 0.f;
	if (Player.Guns < 75 && Chance(UpChance)) { ++Player.Guns; System(FString::Printf(TEXT("Your skill in Gunsmithing has increased to %d."), Player.Guns)); }
	if (--CraftLeft > 0 && CraftableCount(R) > 0) { CraftStart = Now; CraftEnd = Now + 2.5; }
	else CraftIdx = -1;
}

// ------------------------------------------------------------------------------------------------------------- escorts
static const FHLObjective* EscortObjective(const FHLQuest& Q)
{
	for (const FHLObjective& O : Q.Objectives) if (O.Type == TEXT("escort")) return &O;
	return nullptr;
}

bool FHLGame::CanStartEscort(const FHLQuest& Q, bool bReport)
{
	const FHLObjective* O = EscortObjective(Q);
	if (!O) return true;
	if (bEscorting) { if (bReport) Error(TEXT("You are already escorting someone.")); return false; }
	for (FHLUnit* U : UnitList)
		if (U->Kind == EHLKind::Npc && U->Npc && U->Npc->Id == O->Npc)
		{
			if (U->bDead || (EscortUnitId == U->Id && EscortResetAt > 0)) { if (bReport) Error(FString::Printf(TEXT("%s is in no state to travel."), *U->Name)); return false; }
			return true;
		}
	return false;
}

void FHLGame::StartEscort(const FHLQuest& Q)
{
	const FHLObjective* O = EscortObjective(Q);
	if (!O) return;
	FHLUnit* W = nullptr;
	for (FHLUnit* U : UnitList) if (U->Kind == EHLKind::Npc && U->Npc && U->Npc->Id == O->Npc) { W = U; break; }
	if (!W) return;
	W->Level = Q.Level; W->MaxHp = W->Hp = 60.f + 45.f * Q.Level; W->Armor = 20.f * Q.Level; W->Speed = 7.3f; W->MoveMult = 1;
	EscortUnitId = W->Id; EscortQuest = &Q; EscortStart = W->Pos; EscortAmbush = 0; bEscorting = true; EscortResetAt = 0;
	AddChat(TEXT("npc"), W->Name, TEXT("Lead on. I'll be right behind you."));
}

void FHLGame::EndEscort(bool bFailed, const FString& Why)
{
	if (!bEscorting) return;
	bEscorting = false;
	FHLUnit* W = Unit(EscortUnitId);
	if (bFailed && EscortQuest)
	{
		if (!Why.IsEmpty()) Error(Why);
		System(FString::Printf(TEXT("Quest failed: %s"), *EscortQuest->Name));
		Player.Quests.RemoveAll([&](const FHLQuestState& S) { return S.Q == EscortQuest; });
	}
	EscortResetAt = Now + (W && W->bDead ? 30.0 : bFailed ? 4.0 : 12.0);
	EscortQuest = nullptr;
}

void FHLGame::EscortDied(FHLUnit& W)
{
	W.Hp = 0; W.bDead = true; W.DeadAt = Now; StopMove(W);
	for (FHLUnit* M : UnitList) if (M->Kind == EHLKind::Mob && M->bEvent && M->TargetId == W.Id && !M->bDead) M->Threat.Remove(W.Id);
	EndEscort(true, FString::Printf(TEXT("%s has died."), *W.Name));
}

void FHLGame::TickEscort(float Dt)
{
	// reset the escorted NPC to its post some time after the escort ends
	if (!bEscorting && EscortUnitId && EscortResetAt > 0 && Now >= EscortResetAt)
	{
		if (FHLUnit* W = Unit(EscortUnitId))
			if (W->Npc)
			{
				W->Pos = W->Npc->Pos; W->Yaw = W->Npc->Yaw; W->bDead = false; W->MaxHp = W->Hp = 5000; W->Level = 30; StopMove(*W); W->Path.Reset();
			}
		EscortUnitId = 0; EscortResetAt = 0;
	}
	if (!bEscorting)
	{
		// an escort quest without a live escort (e.g. restored from a save) cannot continue
		for (int32 i = Player.Quests.Num() - 1; i >= 0; --i)
		{
			const FHLQuestState& QS = Player.Quests[i];
			if (QS.bComplete || !EscortObjective(*QS.Q)) continue;
			System(FString::Printf(TEXT("Quest failed: %s. Ask again to retry."), *QS.Q->Name));
			Player.Quests.RemoveAt(i);
		}
		return;
	}
	FHLUnit* W = Unit(EscortUnitId);
	const FHLUnit* P = PlayerUnit();
	const FHLQuestState* QS = EscortQuest ? QuestState(EscortQuest->Id) : nullptr;
	if (!W || !P || !QS) { EndEscort(false, FString()); return; } // abandoned
	const FHLObjective* O = EscortObjective(*EscortQuest);
	if (!O) { EndEscort(false, FString()); return; }
	if (W->bDead) return;
	if (W->Area != P->Area || Dist(*W, *P) > 60) { EndEscort(true, FString::Printf(TEXT("You left %s behind."), *W->Name)); return; }
	// follow a few metres behind the player
	if (Dist(*W, *P) > 3.5f) { W->MoveMult = Dist(*W, *P) > 10 ? 1.25f : 1.f; MoveTo(*W, P->Pos, 2.8f); }
	else { StopMove(*W); FaceTo(*W, P->Pos); }
	// ambushes on the way
	const float Total = FMath::Max(1.f, (float)FVector2D::Distance(EscortStart, O->To));
	const float Prog = 1.f - FVector2D::Distance(W->Pos, O->To) / Total;
	static const float At[] = { 0.2f, 0.55f, 0.85f };
	if (EscortAmbush < 3 && Prog > At[EscortAmbush])
	{
		const FHLData& D = FHLData::Get();
		const int32 N = EscortAmbush == 1 ? 3 : 2;
		const FVector2D Fwd = (O->To - W->Pos).GetSafeNormal();
		const FVector2D Side(-Fwd.Y, Fwd.X);
		for (int32 k = 0; k < N; ++k)
		{
			const FHLMobDef* Def = D.Mob(k == 0 ? TEXT("mob_invader_spitter") : TEXT("mob_invader_drone"));
			if (!Def) continue;
			const FVector2D Pos = Areas[W->Area].Nav.NearestFree(W->Pos + Fwd * (10 + Rand() * 6) + Side * ((k % 2 ? 1 : -1) * (12 + Rand() * 5)));
			FHLUnit* M = SpawnMob(Def, W->Area, Pos);
			M->bEvent = true; M->Leash = 200; M->Level = FMath::Max(1, EscortQuest->Level + RandInt(-1, 0)); InitMobStats(*M);
			M->Home = W->Pos;
			MobEnterCombat(*M, W->Id);
			M->Threat.FindOrAdd(W->Id) += 20;
		}
		static const TCHAR* Shouts[] = { TEXT("Up on the roof! Here they come!"), TEXT("More of them! Don't let them get at me!"), TEXT("Nearly there. Keep them off me!") };
		AddChat(TEXT("npc"), W->Name, Shouts[EscortAmbush]);
		AddFloat(W->Id, TEXT("Ambush!"), FLinearColor(1, 0.4f, 0.3f));
		++EscortAmbush;
	}
	// arrival
	if (FVector2D::Distance(W->Pos, O->To) < 10.f)
	{
		QuestEvent(TEXT("escort"), O->Npc);
		AddChat(TEXT("npc"), W->Name, TEXT("The Pantages loading door. I never thought I'd be glad to see it. Thanks, friend."));
		StopMove(*W);
		EndEscort(false, FString());
	}
}
