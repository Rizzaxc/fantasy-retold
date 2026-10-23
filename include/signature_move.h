#ifndef GUARD_SIGNATURE_MOVE_H
#define GUARD_SIGNATURE_MOVE_H

#include "constants/moves.h"

#define SIGNATURE_MOVE_USE_THRESHOLD 100

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

#endif // GUARD_SIGNATURE_MOVE_H
