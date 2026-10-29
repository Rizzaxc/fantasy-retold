#ifndef GUARD_SIGNATURE_MOVE_H
#define GUARD_SIGNATURE_MOVE_H

#include "constants/items.h"
#include "constants/moves.h"

#define SIGNATURE_MOVE_USE_THRESHOLD 100
#define SIGNATURE_MOVE_PP 8 // persists between battles; restored by healing like the slot moves' PP
#define ITEMIZED_MOVE_PP  4 // per battle; full again at the start of every battle

// Fantasy Retold (design.md §10): a fully evolved mon's most-used move (>= SIGNATURE_MOVE_USE_THRESHOLD uses)
// becomes its Signature Move. It then lives OUTSIDE the four move slots — the slot it came from is freed —
// and is used in battle from the innate menu.
bool32 IsFullyEvolved(enum Species species);
enum Move GetSignatureMove(struct Pokemon *mon);
void TryDetermineSignatureMove(struct Pokemon *mon, u8 partySlot);
void TryDetermineSignatureMovesForParty(void);
void IncrementMoveUseCount(u8 partySlot, u8 moveSlot);
void ClearMoveUseTracking(u8 partySlot);
bool32 IsSignatureMove(struct Pokemon *mon, enum Move move);

// Fantasy Retold (design.md §10): a held TM the species can learn gives it that move outside the four slots
// (the L menu). MOVE_NONE if the item isn't a TM / HM or the species can't learn it.
enum Move GetItemizedMove(enum Species species, enum Item item);

#endif // GUARD_SIGNATURE_MOVE_H
