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

// Removes the move in `slot` and shifts the later moves (with their PP, PP Ups and use counts) up one,
// leaving the last slot empty.
static void RemoveMoveSlot(struct Pokemon *mon, u8 partySlot, u8 slot)
{
    u32 ppBonuses = GetMonData(mon, MON_DATA_PP_BONUSES);
    u32 shiftedBonuses = 0;
    enum Move move;
    u32 pp, i;

    for (i = slot; i < MAX_MON_MOVES - 1; i++)
    {
        move = GetMonData(mon, MON_DATA_MOVE1 + i + 1);
        pp = GetMonData(mon, MON_DATA_PP1 + i + 1);
        SetMonData(mon, MON_DATA_MOVE1 + i, &move);
        SetMonData(mon, MON_DATA_PP1 + i, &pp);
        if (partySlot < PARTY_SIZE)
            gSaveBlock1Ptr->moveUseTracking[partySlot][i] = gSaveBlock1Ptr->moveUseTracking[partySlot][i + 1];
    }
    move = MOVE_NONE;
    pp = 0;
    SetMonData(mon, MON_DATA_MOVE1 + MAX_MON_MOVES - 1, &move);
    SetMonData(mon, MON_DATA_PP1 + MAX_MON_MOVES - 1, &pp);
    if (partySlot < PARTY_SIZE)
        gSaveBlock1Ptr->moveUseTracking[partySlot][MAX_MON_MOVES - 1] = 0;

    // PP Ups: 2 bits per slot.
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        u32 src = (i < slot) ? i : i + 1;
        if (src < MAX_MON_MOVES)
            shiftedBonuses |= ((ppBonuses >> (src * 2)) & 3) << (i * 2);
    }
    SetMonData(mon, MON_DATA_PP_BONUSES, &shiftedBonuses);
}

// Takes the Signature Move out of the moveset if it is still in a slot (it was picked from there, and saves
// from before the slot was freed may still hold it).
static void RemoveSignatureFromMoveset(struct Pokemon *mon, u8 partySlot)
{
    enum Move signature = GetMonData(mon, MON_DATA_SIGNATURE_MOVE);
    u32 i;

    if (signature == MOVE_NONE)
        return;
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (GetMonData(mon, MON_DATA_MOVE1 + i) == signature)
        {
            RemoveMoveSlot(mon, partySlot, i);
            return;
        }
    }
}

// Not called mid-battle: freeing a slot would desync the battler's moveset. Battles run
// TryDetermineSignatureMovesForParty once they end.
void TryDetermineSignatureMove(struct Pokemon *mon, u8 partySlot)
{
    if (GetMonData(mon, MON_DATA_SIGNATURE_MOVE) != MOVE_NONE)
    {
        RemoveSignatureFromMoveset(mon, partySlot);
        return;
    }

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
        {
            SetMonData(mon, MON_DATA_SIGNATURE_MOVE, &move);
            RemoveMoveSlot(mon, partySlot, bestSlot); // the signature leaves the moveset, freeing the slot
        }
    }
}

void TryDetermineSignatureMovesForParty(void)
{
    u32 i;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE && !GetMonData(mon, MON_DATA_IS_EGG))
            TryDetermineSignatureMove(mon, i);
    }
}

bool32 IsSignatureMove(struct Pokemon *mon, enum Move move)
{
    if (move == MOVE_NONE)
        return FALSE;
    return GetMonData(mon, MON_DATA_SIGNATURE_MOVE) == move;
}
