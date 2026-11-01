#ifndef GUARD_BATTLE_AI_FR_TACTICS_H
#define GUARD_BATTLE_AI_FR_TACTICS_H

// Fantasy Retold: AI_FLAG_FR_TACTICS (Smart trainers, design.md §10). Additive move scoring on top of the
// upstream AI: it never takes score off a move that KOs, and its bonus is capped below a KO's, so lethal, type
// chart, hazard and damage logic still decide first.

bool32 AI_IsInnateKnown(enum BattlerId battler, enum BattlerId observer);
bool32 AI_IsCritImmune(enum BattlerId battler, enum BattlerId observer);
s32 AI_FrTactics(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum Move move, s32 score);
void AI_FrTacticsPostScore(enum BattlerId battlerAtk, enum BattlerId battlerDef);
void AI_RecordDodgeSpots(void);

#endif // GUARD_BATTLE_AI_FR_TACTICS_H
