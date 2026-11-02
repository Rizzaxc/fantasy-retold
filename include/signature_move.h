#ifndef GUARD_SIGNATURE_MOVE_H
#define GUARD_SIGNATURE_MOVE_H

#include "constants/items.h"
#include "constants/moves.h"

#define SIGNATURE_MOVE_USE_THRESHOLD 100
#define SIGNATURE_MOVE_PP 8 // persists between battles; restored by healing like the slot moves' PP
#define ITEMIZED_MOVE_PP  4 // per battle; full again at the start of every battle

// Fantasy Retold (design.md §10): a move used SIGNATURE_MOVE_USE_THRESHOLD times is mastered (counted per Pokémon
// and per move ID, never per slot). On evolving into its final stage the player may commit a mastered move as its
// Signature Move, which then lives OUTSIDE the four move slots (its slot is freed) and is used from the innate menu.
bool32 IsFullyEvolved(enum Species species);
enum Move GetSignatureMove(struct Pokemon *mon);
void IncrementMoveUseCount(struct Pokemon *mon, enum Move move);
void ClearMoveUseRecord(struct Pokemon *mon);
bool32 IsMoveMastered(struct Pokemon *mon, enum Move move);
u32 GetMasteredKnownMoves(struct Pokemon *mon, enum Move *out);
enum Move PopUnannouncedMasteredMove(struct Pokemon *mon);
bool32 CanCommitSignatureMove(struct Pokemon *mon);
void CommitSignatureMove(struct Pokemon *mon, enum Move move);
void ForgetSignatureMove(struct BoxPokemon *boxMon);
void FixUpSignatureMove(struct Pokemon *mon);
void FixUpSignatureMovesForParty(void);
bool32 IsSignatureMove(struct Pokemon *mon, enum Move move);

// Fantasy Retold (design.md §10): a held TM the species can learn gives it that move outside the four slots
// (the L menu). MOVE_NONE if the item isn't a TM / HM or the species can't learn it.
enum Move GetItemizedMove(enum Species species, enum Item item);

#endif // GUARD_SIGNATURE_MOVE_H
