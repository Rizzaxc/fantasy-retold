#ifndef GUARD_SIGNATURE_MOVE_H
#define GUARD_SIGNATURE_MOVE_H

#include "constants/moves.h"

#define SIGNATURE_MOVE_USE_THRESHOLD 100

bool32 IsFullyEvolved(enum Species species);
enum Move GetSignatureMove(struct Pokemon *mon);
void TryDetermineSignatureMove(struct Pokemon *mon, u8 partySlot);
void IncrementMoveUseCount(u8 partySlot, u8 moveSlot);
void ClearMoveUseTracking(u8 partySlot);
bool32 IsSignatureMove(struct Pokemon *mon, enum Move move);

#endif // GUARD_SIGNATURE_MOVE_H
