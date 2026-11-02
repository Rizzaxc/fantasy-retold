#include "global.h"
#include "item.h"
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

#define MOVE_USE_COUNT_MASK     0x7F
#define MOVE_USE_ANNOUNCED_FLAG 0x80

static bool32 IsPersonalityInPlayerParty(u32 personality)
{
    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gPlayerParty[i];
        if (GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE && GetMonData(mon, MON_DATA_PERSONALITY) == personality)
            return TRUE;
    }
    return FALSE;
}

static bool32 IsMoveUseRecordEmpty(const struct MoveUseRecord *record)
{
    for (u32 i = 0; i < MAX_MON_MOVES; i++)
    {
        if (record->moves[i] != MOVE_NONE)
            return FALSE;
    }
    return TRUE;
}

static u32 GetMoveUseRecordTotal(const struct MoveUseRecord *record)
{
    u32 total = 0;
    for (u32 i = 0; i < MAX_MON_MOVES; i++)
        total += record->counts[i] & MOVE_USE_COUNT_MASK;
    return total;
}

static struct MoveUseRecord *FindMoveUseRecordByPersonality(u32 personality)
{
    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        struct MoveUseRecord *record = &gSaveBlock1Ptr->moveUseRecords[i];
        if (record->personality == personality && !IsMoveUseRecordEmpty(record))
            return record;
    }
    return NULL;
}

static struct MoveUseRecord *FindMoveUseRecord(struct Pokemon *mon)
{
    return FindMoveUseRecordByPersonality(GetMonData(mon, MON_DATA_PERSONALITY));
}

// A free record, else one of a Pokémon no longer in the party, else the one with the fewest uses.
static struct MoveUseRecord *ClaimMoveUseRecord(struct Pokemon *mon)
{
    struct MoveUseRecord *record = FindMoveUseRecord(mon);
    struct MoveUseRecord *best = NULL;
    u32 i, bestScore = UINT32_MAX;

    if (record != NULL)
        return record;

    for (i = 0; i < PARTY_SIZE; i++)
    {
        u32 score;

        record = &gSaveBlock1Ptr->moveUseRecords[i];
        if (IsMoveUseRecordEmpty(record))
            score = 0;
        else if (!IsPersonalityInPlayerParty(record->personality))
            score = 1;
        else
            score = 2 + GetMoveUseRecordTotal(record);
        if (score < bestScore)
        {
            bestScore = score;
            best = record;
        }
    }
    memset(best, 0, sizeof(*best));
    best->personality = GetMonData(mon, MON_DATA_PERSONALITY);
    return best;
}

static bool32 MonHasMoveInSlots(struct Pokemon *mon, enum Move move)
{
    for (u32 i = 0; i < MAX_MON_MOVES; i++)
    {
        if (GetMonData(mon, MON_DATA_MOVE1 + i) == move)
            return TRUE;
    }
    return FALSE;
}

static bool32 IsMoveUseEntryMastered(const struct MoveUseRecord *record, u32 entry)
{
    return record->moves[entry] != MOVE_NONE
        && (record->counts[entry] & MOVE_USE_COUNT_MASK) >= SIGNATURE_MOVE_USE_THRESHOLD;
}

void IncrementMoveUseCount(struct Pokemon *mon, enum Move move)
{
    struct MoveUseRecord *record;
    u32 i, entry = MAX_MON_MOVES, lowest = UINT32_MAX;

    if (move == MOVE_NONE || IsSignatureMove(mon, move))
        return;

    record = ClaimMoveUseRecord(mon);
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        if (record->moves[i] == move)
        {
            entry = i;
            break;
        }
    }
    // A new move takes the entry of a forgotten, unmastered move first, else the least used unmastered one.
    for (i = 0; entry == MAX_MON_MOVES && i < MAX_MON_MOVES; i++)
    {
        if (record->moves[i] == MOVE_NONE || (!MonHasMoveInSlots(mon, record->moves[i]) && !IsMoveUseEntryMastered(record, i)))
            entry = i;
    }
    for (i = 0; entry == MAX_MON_MOVES && i < MAX_MON_MOVES; i++)
    {
        u32 count = record->counts[i] & MOVE_USE_COUNT_MASK;
        if (!IsMoveUseEntryMastered(record, i) && count < lowest)
        {
            lowest = count;
            entry = i;
        }
    }
    if (entry == MAX_MON_MOVES)
        return; // four mastered moves already

    if (record->moves[entry] != move)
    {
        record->moves[entry] = move;
        record->counts[entry] = 0;
    }
    if ((record->counts[entry] & MOVE_USE_COUNT_MASK) < MOVE_USE_COUNT_MASK)
        record->counts[entry]++;
}

void ClearMoveUseRecord(struct Pokemon *mon)
{
    struct MoveUseRecord *record = FindMoveUseRecord(mon);

    if (record != NULL)
        memset(record, 0, sizeof(*record));
}

bool32 IsMoveMastered(struct Pokemon *mon, enum Move move)
{
    struct MoveUseRecord *record = FindMoveUseRecord(mon);

    if (record == NULL || move == MOVE_NONE)
        return FALSE;
    for (u32 i = 0; i < MAX_MON_MOVES; i++)
    {
        if (record->moves[i] == move)
            return IsMoveUseEntryMastered(record, i);
    }
    return FALSE;
}

u32 GetMasteredKnownMoves(struct Pokemon *mon, enum Move *out)
{
    u32 i, count = 0;

    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        enum Move move = GetMonData(mon, MON_DATA_MOVE1 + i);
        if (IsMoveMastered(mon, move))
        {
            if (out != NULL)
                out[count] = move;
            count++;
        }
    }
    return count;
}

enum Move PopUnannouncedMasteredMove(struct Pokemon *mon)
{
    struct MoveUseRecord *record = FindMoveUseRecord(mon);

    if (record == NULL)
        return MOVE_NONE;
    for (u32 i = 0; i < MAX_MON_MOVES; i++)
    {
        if (IsMoveUseEntryMastered(record, i) && !(record->counts[i] & MOVE_USE_ANNOUNCED_FLAG))
        {
            record->counts[i] |= MOVE_USE_ANNOUNCED_FLAG;
            return record->moves[i];
        }
    }
    return MOVE_NONE;
}

bool32 CanCommitSignatureMove(struct Pokemon *mon)
{
    return GetMonData(mon, MON_DATA_SIGNATURE_MOVE) == MOVE_NONE
        && IsFullyEvolved(GetMonData(mon, MON_DATA_SPECIES))
        && GetMasteredKnownMoves(mon, NULL) != 0;
}

// Removes the move in `slot` and shifts the later moves (with their PP and PP Ups) up one, leaving the last slot empty.
static void RemoveMoveSlot(struct Pokemon *mon, u8 slot)
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
    }
    move = MOVE_NONE;
    pp = 0;
    SetMonData(mon, MON_DATA_MOVE1 + MAX_MON_MOVES - 1, &move);
    SetMonData(mon, MON_DATA_PP1 + MAX_MON_MOVES - 1, &pp);

    // PP Ups: 2 bits per slot.
    for (i = 0; i < MAX_MON_MOVES; i++)
    {
        u32 src = (i < slot) ? i : i + 1;
        if (src < MAX_MON_MOVES)
            shiftedBonuses |= ((ppBonuses >> (src * 2)) & 3) << (i * 2);
    }
    SetMonData(mon, MON_DATA_PP_BONUSES, &shiftedBonuses);
}

// Takes the Signature Move out of the moveset if it is still in a slot (saves from before the slot was freed may
// still hold it).
static void RemoveSignatureFromMoveset(struct Pokemon *mon)
{
    enum Move signature = GetMonData(mon, MON_DATA_SIGNATURE_MOVE);

    if (signature == MOVE_NONE)
        return;
    for (u32 i = 0; i < MAX_MON_MOVES; i++)
    {
        if (GetMonData(mon, MON_DATA_MOVE1 + i) == signature)
        {
            RemoveMoveSlot(mon, i);
            return;
        }
    }
}

// Not called mid-battle: freeing a slot would desync the battler's moveset.
void CommitSignatureMove(struct Pokemon *mon, enum Move move)
{
    u8 pp = SIGNATURE_MOVE_PP;

    SetMonData(mon, MON_DATA_SIGNATURE_MOVE, &move);
    SetMonData(mon, MON_DATA_SIGNATURE_PP, &pp);
    RemoveSignatureFromMoveset(mon); // the signature leaves the moveset, freeing the slot
    ClearMoveUseRecord(mon);
}

// The Move Relearner's only Signature Move option: the move is gone (it isn't put back in a slot) and the Pokémon's
// move uses start over.
void ForgetSignatureMove(struct BoxPokemon *boxMon)
{
    struct MoveUseRecord *record = FindMoveUseRecordByPersonality(GetBoxMonData(boxMon, MON_DATA_PERSONALITY));
    u16 move = MOVE_NONE;
    u8 pp = SIGNATURE_MOVE_PP;

    SetBoxMonData(boxMon, MON_DATA_SIGNATURE_MOVE, &move);
    SetBoxMonData(boxMon, MON_DATA_SIGNATURE_PP, &pp);
    if (record != NULL)
        memset(record, 0, sizeof(*record));
}

// Signature Moves are never assigned automatically (the player commits one on evolving, see evolution_scene.c);
// this only fixes up saves that still hold the signature in a slot.
void FixUpSignatureMove(struct Pokemon *mon)
{
    RemoveSignatureFromMoveset(mon);
}

void FixUpSignatureMovesForParty(void)
{
    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];
        if (GetMonData(mon, MON_DATA_SPECIES) != SPECIES_NONE && !GetMonData(mon, MON_DATA_IS_EGG))
            FixUpSignatureMove(mon);
    }
}

bool32 IsSignatureMove(struct Pokemon *mon, enum Move move)
{
    if (move == MOVE_NONE)
        return FALSE;
    return GetMonData(mon, MON_DATA_SIGNATURE_MOVE) == move;
}

enum Move GetItemizedMove(enum Species species, enum Item item)
{
    enum Move move;

    if (item == ITEM_NONE || GetItemPocket(item) != POCKET_TM_HM)
        return MOVE_NONE;
    move = GetItemTMHMMoveId(item);
    if (move == MOVE_NONE || !CanLearnTeachableMove(species, move))
        return MOVE_NONE;
    return move;
}
