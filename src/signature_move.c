#include "global.h"
#include "pokemon.h"
#include "signature_move.h"
#include "constants/species.h"

bool32 IsFullyEvolved(enum Species species)
{
    const struct Evolution *evolutions = GetSpeciesEvolutions(species);
    if (evolutions == NULL)
        return TRUE;
    for (u32 i = 0; evolutions[i].method != EVOLUTIONS_END; i++)
    {
        if (evolutions[i].method && SanitizeSpeciesId(evolutions[i].targetSpecies) != SPECIES_NONE)
            return FALSE;
    }
    return TRUE;
}

enum Move GetSignatureMove(struct Pokemon *mon)
{
    return (enum Move)GetMonData(mon, MON_DATA_SIGNATURE_MOVE);
}

void IncrementMoveUseCount(u8 partySlot, u8 moveSlot)
{
    if (partySlot >= PARTY_SIZE || moveSlot >= MAX_MON_MOVES)
        return;
    if (gSaveBlock1Ptr->moveUseTracking[partySlot][moveSlot] < 255)
        gSaveBlock1Ptr->moveUseTracking[partySlot][moveSlot]++;
}

void ClearMoveUseTracking(u8 partySlot)
{
    if (partySlot >= PARTY_SIZE)
        return;
    memset(gSaveBlock1Ptr->moveUseTracking[partySlot], 0, MAX_MON_MOVES);
}

void TryDetermineSignatureMove(struct Pokemon *mon, u8 partySlot)
{
    if (GetMonData(mon, MON_DATA_SIGNATURE_MOVE) != MOVE_NONE)
        return;

    enum Species species = GetMonData(mon, MON_DATA_SPECIES);
    if (!IsFullyEvolved(species))
        return;

    if (partySlot >= PARTY_SIZE)
        return;

    u8 bestSlot = MAX_MON_MOVES;
    u8 bestCount = 0;
    for (u8 i = 0; i < MAX_MON_MOVES; i++)
    {
        u8 count = gSaveBlock1Ptr->moveUseTracking[partySlot][i];
        if (count >= SIGNATURE_MOVE_USE_THRESHOLD && count > bestCount)
        {
            bestCount = count;
            bestSlot = i;
        }
    }

    if (bestSlot < MAX_MON_MOVES)
    {
        enum Move move = GetMonData(mon, MON_DATA_MOVE1 + bestSlot);
        if (move != MOVE_NONE)
            SetMonData(mon, MON_DATA_SIGNATURE_MOVE, &move);
    }
}

bool32 IsSignatureMove(struct Pokemon *mon, enum Move move)
{
    if (move == MOVE_NONE)
        return FALSE;
    return GetMonData(mon, MON_DATA_SIGNATURE_MOVE) == move;
}
