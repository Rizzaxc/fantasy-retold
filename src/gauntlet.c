#include "global.h"
#include "battle.h"
#include "battle_setup.h"
#include "event_data.h"
#include "gauntlet.h"
#include "item.h"
#include "money.h"
#include "random.h"
#include "string_util.h"
#include "constants/battle.h"
#include "constants/gauntlet.h"
#include "constants/items.h"
#include "constants/opponents.h"

// Fantasy Retold: trainer gauntlets (design.md §2, Gauntlets). Each gauntlet is a run of trainer battles the
// map script fights back to back (no heal); any battle against one of its trainers sets every EXP award to `exp`.
// Its trainers' levels are authored for `baseBadges` badges; each badge beyond that adds `levelsPerBadge` to every
// mon's level, with the authored EVs scaled to match (GenerateMonFromTrainerMon) and the moves kept as authored.
// Clearing it pays a one-time reward: one item per slot, then money. A slot is a list of groups; the roll picks a
// group evenly, then an item in it evenly, so a fixed item is a slot with one group of one item.
// Scripts pass the gauntlet id (GAUNTLET_*) in VAR_0x8009.

struct GauntletRewardGroup
{
    const enum Item *items;
    u8 count;
};

struct GauntletRewardSlot
{
    const struct GauntletRewardGroup *groups;
    u8 count;
};

struct Gauntlet
{
    const u16 *trainers; // battle order; the last one clears the gauntlet
    u8 trainerCount;
    u8 requiredBadge;    // badge number (1-8) needed to start it, 0 = none
    u8 baseBadges;       // badge count its authored levels are tuned for
    u8 levelsPerBadge;   // levels added to every mon per badge over baseBadges
    u16 exp;             // every EXP award in its battles
    const struct GauntletRewardSlot *rewardSlots;
    u8 rewardSlotCount;  // <= GAUNTLET_MAX_REWARD_ITEMS
    u32 money;
};

#define GROUP(...) { (const enum Item[]){ __VA_ARGS__ }, ARRAY_COUNT(((const enum Item[]){ __VA_ARGS__ })) }
#define SLOT(groups) { groups, ARRAY_COUNT(groups) }

// ---- GAUNTLET_WINSTRATE ----

static const u16 sWinstrateTrainers[] = { TRAINER_VICTOR, TRAINER_VICTORIA, TRAINER_VIVI, TRAINER_VICKY };

static const struct GauntletRewardGroup sRareCandy[] = { GROUP(ITEM_RARE_CANDY) };
static const struct GauntletRewardGroup sBottleCap[] = { GROUP(ITEM_BOTTLE_CAP) };
static const struct GauntletRewardGroup sWinstrateHeldItem[] =
{
    GROUP(ITEM_CHOICE_BAND, ITEM_CHOICE_SPECS, ITEM_CHOICE_SCARF),
    GROUP(ITEM_LIFE_ORB),
    GROUP(ITEM_LEFTOVERS),
    GROUP(ITEM_ASSAULT_VEST),
    GROUP(ITEM_ROCKY_HELMET),
};

static const struct GauntletRewardSlot sWinstrateReward[] =
{
    SLOT(sRareCandy),
    SLOT(sBottleCap),
    SLOT(sWinstrateHeldItem),
};

// ---- Table ----

static const struct Gauntlet sGauntlets[GAUNTLET_COUNT] =
{
    [GAUNTLET_WINSTRATE] =
    {
        .trainers = sWinstrateTrainers,
        .trainerCount = ARRAY_COUNT(sWinstrateTrainers),
        .requiredBadge = 3,
        .baseBadges = 3,
        .levelsPerBadge = 5,
        .exp = 1,
        .rewardSlots = sWinstrateReward,
        .rewardSlotCount = ARRAY_COUNT(sWinstrateReward),
        .money = 10000,
    },
};

// The items rolled by Gauntlet_GiveRewardItems, read back by Gauntlet_GetRewardItem for the messages.
static EWRAM_DATA enum Item sRolledItems[GAUNTLET_MAX_REWARD_ITEMS] = {0};

// Gauntlet N's reward flag is FLAG_GAUNTLET_REWARD_1 + N.
static u32 GetGauntletRewardFlag(void)
{
    return FLAG_GAUNTLET_REWARD_1 + gSpecialVar_0x8009;
}

static const struct Gauntlet *GetScriptGauntlet(void)
{
    if (gSpecialVar_0x8009 >= GAUNTLET_COUNT)
        return NULL;
    return &sGauntlets[gSpecialVar_0x8009];
}

u32 GetGauntletForTrainer(u16 trainerId)
{
    u32 i, j;

    for (i = 0; i < GAUNTLET_COUNT; i++)
    {
        for (j = 0; j < sGauntlets[i].trainerCount; j++)
        {
            if (sGauntlets[i].trainers[j] == trainerId)
                return i;
        }
    }
    return GAUNTLET_NONE;
}

// Called last in ApplyExperienceMultipliers. Either opponent counts, for gauntlets with double battles.
bool32 GetGauntletExpOverride(s32 *expAmount)
{
    u32 gauntlet;

    if (!(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
        return FALSE;

    gauntlet = GetGauntletForTrainer(TRAINER_BATTLE_PARAM.opponentA);
    if (gauntlet == GAUNTLET_NONE && (gBattleTypeFlags & BATTLE_TYPE_TWO_OPPONENTS))
        gauntlet = GetGauntletForTrainer(TRAINER_BATTLE_PARAM.opponentB);
    if (gauntlet == GAUNTLET_NONE)
        return FALSE;

    *expAmount = sGauntlets[gauntlet].exp;
    return TRUE;
}

static u32 CountBadges(void)
{
    u32 i, count = 0;

    for (i = 0; i < NUM_BADGES; i++)
    {
        if (FlagGet(FLAG_BADGE01_GET + i))
            count++;
    }
    return count;
}

// Levels added to the trainer's party: levelsPerBadge for each badge over the gauntlet's baseBadges, 0 for a trainer
// outside every gauntlet. Read when the party is built (CreateNPCTrainerParty) and for the prize money.
s32 GetGauntletLevelShift(u16 trainerId)
{
    u32 gauntlet = GetGauntletForTrainer(trainerId);
    u32 badges;

    if (gauntlet == GAUNTLET_NONE)
        return 0;
    badges = CountBadges();
    if (badges <= sGauntlets[gauntlet].baseBadges)
        return 0;
    return (badges - sGauntlets[gauntlet].baseBadges) * sGauntlets[gauntlet].levelsPerBadge;
}

// VAR_RESULT: whether the player has the badge the gauntlet asks for before it can be started.
bool32 Gauntlet_HasRequiredBadge(void)
{
    const struct Gauntlet *gauntlet = GetScriptGauntlet();

    if (gauntlet == NULL || gauntlet->requiredBadge == 0)
        return TRUE;
    return FlagGet(FLAG_BADGE01_GET + gauntlet->requiredBadge - 1);
}

// VAR_RESULT: whether the gauntlet's reward was already claimed.
bool32 Gauntlet_IsClaimed(void)
{
    const struct Gauntlet *gauntlet = GetScriptGauntlet();

    return gauntlet == NULL || FlagGet(GetGauntletRewardFlag());
}

// Rolls one item per slot and adds them all to the bag, or none if any doesn't fit (VAR_RESULT = FALSE).
// On success the reward is marked claimed and pays its money; the messages are left to the script.
bool32 Gauntlet_GiveRewardItems(void)
{
    const struct Gauntlet *gauntlet = GetScriptGauntlet();
    u32 i;

    if (gauntlet == NULL)
        return FALSE;

    for (i = 0; i < GAUNTLET_MAX_REWARD_ITEMS; i++)
        sRolledItems[i] = ITEM_NONE;

    for (i = 0; i < gauntlet->rewardSlotCount; i++)
    {
        const struct GauntletRewardSlot *slot = &gauntlet->rewardSlots[i];
        const struct GauntletRewardGroup *group = &slot->groups[Random() % slot->count];

        sRolledItems[i] = group->items[Random() % group->count];
        if (!AddBagItem(sRolledItems[i], 1))
        {
            // Roll back what went in, so the reward stays whole for next time.
            while (i-- > 0)
                RemoveBagItem(sRolledItems[i], 1);
            for (i = 0; i < GAUNTLET_MAX_REWARD_ITEMS; i++)
                sRolledItems[i] = ITEM_NONE;
            return FALSE;
        }
    }

    FlagSet(GetGauntletRewardFlag());
    AddMoney(&gSaveBlock1Ptr->money, gauntlet->money);
    return TRUE;
}

// VAR_RESULT: the rolled item at index VAR_0x8004, ITEM_NONE past the last one.
u16 Gauntlet_GetRewardItem(void)
{
    if (gSpecialVar_0x8004 >= GAUNTLET_MAX_REWARD_ITEMS)
        return ITEM_NONE;
    return sRolledItems[gSpecialVar_0x8004];
}

// VAR_RESULT: whether the reward pays money; buffers the amount in STR_VAR_1.
bool32 Gauntlet_BufferRewardMoney(void)
{
    const struct Gauntlet *gauntlet = GetScriptGauntlet();

    if (gauntlet == NULL || gauntlet->money == 0)
        return FALSE;
    ConvertIntToDecimalStringN(gStringVar1, gauntlet->money, STR_CONV_MODE_LEFT_ALIGN, 7);
    return TRUE;
}
