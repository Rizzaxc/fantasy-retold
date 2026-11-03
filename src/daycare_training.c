#include "global.h"
#include "event_data.h"
#include "item.h"
#include "money.h"
#include "move.h"
#include "party_menu.h"
#include "pokemon.h"
#include "signature_move.h"
#include "string_util.h"
#include "constants/daycare.h"
#include "constants/items.h"

// Fantasy Retold: the daycare man's training (design.md §2). He raises a party mon for a flat fee per level, up to
// DAYCARE_TRAINING_LEVEL_GAP below the party's highest level. Scripts pass the party slot in VAR_0x8004; the level up
// itself is DaycareTraining_LevelUpMon (party_menu.c), which reuses the Rare Candy flow.

#define DAYCARE_TRAINING_PRICE      200
#define DAYCARE_TRAINING_LEVEL_GAP  5

static u32 GetHighestPartyLevel(void)
{
    u32 highest = 0;

    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][i];

        if (GetMonData(mon, MON_DATA_SPECIES) == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG))
            continue;
        if (GetMonData(mon, MON_DATA_LEVEL) > highest)
            highest = GetMonData(mon, MON_DATA_LEVEL);
    }
    return highest;
}

static u32 GetTrainingCost(struct Pokemon *mon, u32 targetLevel)
{
    return (targetLevel - GetMonData(mon, MON_DATA_LEVEL)) * DAYCARE_TRAINING_PRICE;
}

// Checks the mon in VAR_0x8004 and returns a DAYCARE_TRAINING_* result. Always buffers its nickname (STR_VAR_1).
// AT_CAP buffers the level cap (STR_VAR_2). OK sets VAR_0x8005 to the highest level the player can afford (up to the
// cap) and VAR_0x8006 to one level up, and buffers that level (STR_VAR_2) and its price (STR_VAR_3).
u16 DaycareTraining_BufferOffer(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][gSpecialVar_0x8004];
    u32 highest = GetHighestPartyLevel();
    u32 cap = highest > DAYCARE_TRAINING_LEVEL_GAP ? highest - DAYCARE_TRAINING_LEVEL_GAP : 0;
    u32 level, target;

    if (GetMonData(mon, MON_DATA_IS_EGG))
        return DAYCARE_TRAINING_EGG;
    GetMonNickname(mon, gStringVar1);
    level = GetMonData(mon, MON_DATA_LEVEL);
    if (level >= cap)
    {
        ConvertIntToDecimalStringN(gStringVar2, cap, STR_CONV_MODE_LEFT_ALIGN, 3);
        return DAYCARE_TRAINING_AT_CAP;
    }
    target = min(cap, level + GetMoney(&gSaveBlock1Ptr->money) / DAYCARE_TRAINING_PRICE);
    if (target <= level)
        return DAYCARE_TRAINING_NO_MONEY;
    gSpecialVar_0x8005 = target;
    gSpecialVar_0x8006 = level + 1;
    ConvertIntToDecimalStringN(gStringVar2, target, STR_CONV_MODE_LEFT_ALIGN, 3);
    ConvertIntToDecimalStringN(gStringVar3, GetTrainingCost(mon, target), STR_CONV_MODE_LEFT_ALIGN, 6);
    return DAYCARE_TRAINING_OK;
}

// Takes the fee for raising the mon in VAR_0x8004 to the level in VAR_0x8005. FALSE (nothing taken) if it can't.
bool16 DaycareTraining_Charge(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][gSpecialVar_0x8004];
    u32 cost;

    if (gSpecialVar_0x8005 <= GetMonData(mon, MON_DATA_LEVEL) || gSpecialVar_0x8005 > MAX_LEVEL)
        return FALSE;
    cost = GetTrainingCost(mon, gSpecialVar_0x8005);
    if (!IsEnoughMoney(&gSaveBlock1Ptr->money, cost))
        return FALSE;
    RemoveMoney(&gSaveBlock1Ptr->money, cost);
    return TRUE;
}

// Hyper Training (design.md §2): for HYPER_TRAINING_PRICE and a Bottle Cap, the daycare man sets one stat's
// hyper-trained flag, which the stat calc reads as a perfect IV. No level requirement. Scripts pass the party slot in
// VAR_0x8004 and the stat (STAT_*) in VAR_0x8005.

STATIC_ASSERT(HYPER_TRAINING_STAT_ROWS == NUM_STATS, HyperTrainingStatRows);

// Stat list order, as on the summary screen.
static const u8 sHyperTrainingStats[NUM_STATS] = { STAT_HP, STAT_ATK, STAT_DEF, STAT_SPATK, STAT_SPDEF, STAT_SPEED };

static const u8 *const sHyperTrainingStatNames[NUM_STATS] =
{
    [STAT_HP]    = COMPOUND_STRING("HP"),
    [STAT_ATK]   = COMPOUND_STRING("ATTACK"),
    [STAT_DEF]   = COMPOUND_STRING("DEFENSE"),
    [STAT_SPATK] = COMPOUND_STRING("SP. ATK"),
    [STAT_SPDEF] = COMPOUND_STRING("STAMINA"),
    [STAT_SPEED] = COMPOUND_STRING("SPEED"),
};

static bool32 IsStatMaxed(struct Pokemon *mon, u32 stat)
{
    return GetMonData(mon, MON_DATA_HYPER_TRAINED_HP + stat) || GetMonData(mon, MON_DATA_HP_IV + stat) == MAX_PER_STAT_IVS;
}

// Checks the mon in VAR_0x8004 and returns a HYPER_TRAINING_* result. Buffers its nickname (STR_VAR_1).
u16 HyperTraining_CheckMon(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][gSpecialVar_0x8004];

    if (GetMonData(mon, MON_DATA_IS_EGG))
        return HYPER_TRAINING_EGG;
    GetMonNickname(mon, gStringVar1);
    for (u32 i = 0; i < NUM_STATS; i++)
    {
        if (!IsStatMaxed(mon, i))
            return HYPER_TRAINING_OK;
    }
    return HYPER_TRAINING_ALL_MAXED;
}

// Buffers row VAR_0x8005 of the stat list (STR_VAR_2): the stat and its IV, or MAX. Returns the row's stat.
u16 HyperTraining_BufferStatRow(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][gSpecialVar_0x8004];
    u32 stat = sHyperTrainingStats[gSpecialVar_0x8005 % NUM_STATS];
    u8 *end = StringCopy(gStringVar2, sHyperTrainingStatNames[stat]);

    end = StringCopy(end, COMPOUND_STRING(": "));
    if (IsStatMaxed(mon, stat))
        StringCopy(end, COMPOUND_STRING("MAX"));
    else
        ConvertIntToDecimalStringN(end, GetMonData(mon, MON_DATA_HP_IV + stat), STR_CONV_MODE_LEFT_ALIGN, 2);
    return stat;
}

// Buffers the name of stat VAR_0x8005 (STR_VAR_2); FALSE if that stat is already maxed.
bool16 HyperTraining_CanTrainStat(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][gSpecialVar_0x8004];
    u32 stat = gSpecialVar_0x8005;

    if (stat >= NUM_STATS)
        return FALSE;
    StringCopy(gStringVar2, sHyperTrainingStatNames[stat]);
    return !IsStatMaxed(mon, stat);
}

// Takes the fee and a Bottle Cap and maxes stat VAR_0x8005 of the mon in VAR_0x8004. FALSE (nothing taken) if it can't.
bool16 HyperTraining_Train(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][gSpecialVar_0x8004];
    u32 stat = gSpecialVar_0x8005;
    bool32 trained = TRUE;

    if (stat >= NUM_STATS || IsStatMaxed(mon, stat)
     || !IsEnoughMoney(&gSaveBlock1Ptr->money, HYPER_TRAINING_PRICE) || !CheckBagHasItem(ITEM_BOTTLE_CAP, 1))
        return FALSE;
    RemoveMoney(&gSaveBlock1Ptr->money, HYPER_TRAINING_PRICE);
    RemoveBagItem(ITEM_BOTTLE_CAP, 1);
    SetMonData(mon, MON_DATA_HYPER_TRAINED_HP + stat, &trained);
    CalculateMonStats(mon);
    return TRUE;
}

// Signature Training (design.md §2): for SIGNATURE_TRAINING_PRICE the daycare man commits a mastered move as the
// Signature Move, under the same rule as on evolving into the final stage (CanCommitSignatureMove). It's the way to
// earn one for a Pokémon that mastered the move after its final evolution, never evolves, refused, or forgot one.
// Scripts pass the party slot in VAR_0x8004 and the row (index among the mastered moves) in VAR_0x8005.

STATIC_ASSERT(SIGNATURE_TRAINING_CANCEL == MAX_MON_MOVES, SignatureTrainingCancel);

// Checks the mon in VAR_0x8004 and returns a SIGNATURE_TRAINING_* result. Buffers its nickname (STR_VAR_1);
// HAS_SIGNATURE buffers that move (STR_VAR_2). OK sets VAR_0x8006 to the number of mastered moves it knows.
u16 SignatureTraining_CheckMon(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][gSpecialVar_0x8004];
    enum Move signature;

    if (GetMonData(mon, MON_DATA_IS_EGG))
        return SIGNATURE_TRAINING_EGG;
    GetMonNickname(mon, gStringVar1);
    signature = GetSignatureMove(mon);
    if (signature != MOVE_NONE)
    {
        StringCopy(gStringVar2, GetMoveName(signature));
        return SIGNATURE_TRAINING_HAS_SIGNATURE;
    }
    if (!IsFullyEvolved(GetMonData(mon, MON_DATA_SPECIES)))
        return SIGNATURE_TRAINING_NOT_FINAL;
    gSpecialVar_0x8006 = GetMasteredKnownMoves(mon, NULL);
    if (gSpecialVar_0x8006 == 0)
        return SIGNATURE_TRAINING_NO_MASTERED;
    return SIGNATURE_TRAINING_OK;
}

// Buffers mastered move VAR_0x8005 of the mon in VAR_0x8004 (STR_VAR_2). FALSE if there's no such move.
bool16 SignatureTraining_BufferMove(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][gSpecialVar_0x8004];
    enum Move mastered[MAX_MON_MOVES];

    if (gSpecialVar_0x8005 >= GetMasteredKnownMoves(mon, mastered))
        return FALSE;
    StringCopy(gStringVar2, GetMoveName(mastered[gSpecialVar_0x8005]));
    return TRUE;
}

// Takes the fee and commits mastered move VAR_0x8005 of the mon in VAR_0x8004. FALSE (nothing taken) if it can't.
bool16 SignatureTraining_Commit(void)
{
    struct Pokemon *mon = &gParties[B_TRAINER_PLAYER][gSpecialVar_0x8004];
    enum Move mastered[MAX_MON_MOVES];

    if (!CanCommitSignatureMove(mon) || gSpecialVar_0x8005 >= GetMasteredKnownMoves(mon, mastered)
     || !IsEnoughMoney(&gSaveBlock1Ptr->money, SIGNATURE_TRAINING_PRICE))
        return FALSE;
    RemoveMoney(&gSaveBlock1Ptr->money, SIGNATURE_TRAINING_PRICE);
    CommitSignatureMove(mon, mastered[gSpecialVar_0x8005]);
    return TRUE;
}
