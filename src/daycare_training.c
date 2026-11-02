#include "global.h"
#include "event_data.h"
#include "money.h"
#include "party_menu.h"
#include "pokemon.h"
#include "string_util.h"
#include "constants/daycare.h"

// Fantasy Retold: the daycare man's training (design.md §10). He raises a party mon for a flat fee per level, up to
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
