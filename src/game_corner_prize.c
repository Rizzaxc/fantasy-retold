#include "global.h"
#include "egg_shop.h"
#include "event_data.h"
#include "gym_reward.h"
#include "item.h"
#include "pokemon.h"
#include "random.h"
#include "script_pokemon_util.h"
#include "string_util.h"
#include "text.h"
#include "constants/characters.h"
#include "constants/game_corner_prize.h"
#include "constants/items.h"
#include "constants/species.h"

// Fantasy Retold: design.md §2, Game Corner Prizes. Five rows, each rolled as a category (evenly), then an item or a
// species. They restock every day and after every badge / rival battle / Elite Four clear (the egg shop milestones).
// Each row is one var: category in bits 14-15, item or species id below; 0 = sold. While the player has locked them
// (FLAG_GAME_CORNER_PRIZES_LOCKED) the rows never restock.

#define PRIZE_CATEGORY_SHIFT 14
#define PRIZE_ID_MASK        ((1 << PRIZE_CATEGORY_SHIFT) - 1)

static const u16 sPrizeVars[GAME_CORNER_PRIZE_ROWS] =
{
    VAR_GAME_CORNER_PRIZE_1,
    VAR_GAME_CORNER_PRIZE_2,
    VAR_GAME_CORNER_PRIZE_3,
    VAR_GAME_CORNER_PRIZE_4,
    VAR_GAME_CORNER_PRIZE_5,
};

static const u16 sPrizeCoins[GAME_CORNER_PRIZE_CATEGORIES] =
{
    [GAME_CORNER_PRIZE_SPECIES_ITEM] = GAME_CORNER_SPECIES_ITEM_COINS,
    [GAME_CORNER_PRIZE_HELD_ITEM]    = GAME_CORNER_HELD_ITEM_COINS,
    [GAME_CORNER_PRIZE_STAT_ITEM]    = GAME_CORNER_STAT_ITEM_COINS,
    [GAME_CORNER_PRIZE_EGG]          = GAME_CORNER_EGG_COINS,
};

// Items for one species line; the legendaries' ones (Soul Dew, the orbs, Plates...) are left out.
static const enum Item sSpeciesItems[] =
{
    ITEM_LIGHT_BALL,
    ITEM_THICK_CLUB,
    ITEM_LEEK,
    ITEM_LUCKY_PUNCH,
    ITEM_METAL_POWDER,
    ITEM_QUICK_POWDER,
    ITEM_DEEP_SEA_TOOTH,
    ITEM_DEEP_SEA_SCALE,
};

static u32 RollPrize(void)
{
    u32 category = Random() % GAME_CORNER_PRIZE_CATEGORIES;
    u32 id;

    switch (category)
    {
    case GAME_CORNER_PRIZE_SPECIES_ITEM:
        id = sSpeciesItems[Random() % ARRAY_COUNT(sSpeciesItems)];
        break;
    case GAME_CORNER_PRIZE_HELD_ITEM:
        id = RollGymRewardHeldItem();
        break;
    case GAME_CORNER_PRIZE_STAT_ITEM:
        id = RollGymRewardStatItem();
        break;
    default:
        id = PickEggShopPowerhouseSpecies();
        if (id == SPECIES_NONE) // no Powerhouse species enabled in this build
        {
            category = GAME_CORNER_PRIZE_HELD_ITEM;
            id = RollGymRewardHeldItem();
        }
        break;
    }
    return (category << PRIZE_CATEGORY_SHIFT) | (id & PRIZE_ID_MASK);
}

static u32 GetPrizeRow(void)
{
    return gSpecialVar_0x8005 < GAME_CORNER_PRIZE_ROWS ? gSpecialVar_0x8005 : 0;
}

void GameCornerPrize_Restock(void)
{
    u16 key = GetEggShopMilestones() + 1;

    if (FlagGet(FLAG_GAME_CORNER_PRIZES_LOCKED))
        return;
    if (FlagGet(FLAG_DAILY_GAME_CORNER_PRIZES) && VarGet(VAR_GAME_CORNER_PRIZE_KEY) == key)
        return;
    for (u32 row = 0; row < GAME_CORNER_PRIZE_ROWS; row++)
        VarSet(sPrizeVars[row], RollPrize());
    VarSet(VAR_GAME_CORNER_PRIZE_KEY, key);
    FlagSet(FLAG_DAILY_GAME_CORNER_PRIZES);
}

// A row's prize name ("<SPECIES> EGG" for an Egg); the row must not be sold.
static u8 *BufferPrizeName(u8 *dest, u16 value)
{
    u32 id = value & PRIZE_ID_MASK;

    if ((value >> PRIZE_CATEGORY_SHIFT) == GAME_CORNER_PRIZE_EGG)
        return StringCopy(StringCopy(dest, GetSpeciesName(id)), COMPOUND_STRING(" EGG"));
    CopyItemName(id, dest);
    return dest + StringLength(dest);
}

// Row VAR_0x8005's label in STR_VAR_2, for dynmultipush: the prize, then its coin price in a column after the
// widest prize name of all rows (so the rows line up); or SOLD OUT.
void GameCornerPrize_BufferRow(void)
{
    u16 value = VarGet(sPrizeVars[GetPrizeRow()]);
    u8 name[32];
    u32 priceX = 0;
    u8 *end;

    if (value == 0)
    {
        StringCopy(gStringVar2, COMPOUND_STRING("SOLD OUT"));
        return;
    }
    for (u32 row = 0; row < GAME_CORNER_PRIZE_ROWS; row++)
    {
        u16 rowValue = VarGet(sPrizeVars[row]);

        if (rowValue == 0)
            continue;
        BufferPrizeName(name, rowValue);
        priceX = max(priceX, (u32)GetStringWidth(FONT_NORMAL, name, 0));
    }
    end = BufferPrizeName(gStringVar2, value);
    *end++ = EXT_CTRL_CODE_BEGIN;
    *end++ = EXT_CTRL_CODE_CLEAR_TO;
    *end++ = priceX + 8;
    ConvertIntToDecimalStringN(end, sPrizeCoins[value >> PRIZE_CATEGORY_SHIFT], STR_CONV_MODE_LEFT_ALIGN, 4);
}

// Row VAR_0x8005: name in STR_VAR_1, coin price in VAR_0x8006, item id in VAR_0x8007 (an Egg: ITEM_NONE).
// Returns GAME_CORNER_OFFER_SOLD / _ITEM / _EGG.
u16 GameCornerPrize_BufferOffer(void)
{
    u16 value = VarGet(sPrizeVars[GetPrizeRow()]);
    u32 category = value >> PRIZE_CATEGORY_SHIFT;
    u32 id = value & PRIZE_ID_MASK;

    gSpecialVar_0x8007 = ITEM_NONE;
    if (value == 0)
        return GAME_CORNER_OFFER_SOLD;
    gSpecialVar_0x8006 = sPrizeCoins[category];
    if (category == GAME_CORNER_PRIZE_EGG)
    {
        StringCopy(StringCopy(gStringVar1, GetSpeciesName(id)), COMPOUND_STRING(" EGG"));
        return GAME_CORNER_OFFER_EGG;
    }
    gSpecialVar_0x8007 = id;
    CopyItemName(id, gStringVar1);
    return GAME_CORNER_OFFER_ITEM;
}

// Gives row VAR_0x8005's Egg (party, else PC). Returns MON_GIVEN_TO_PARTY / MON_GIVEN_TO_PC / MON_CANT_GIVE.
u16 GameCornerPrize_GiveEgg(void)
{
    u16 value = VarGet(sPrizeVars[GetPrizeRow()]);

    if (value == 0 || (value >> PRIZE_CATEGORY_SHIFT) != GAME_CORNER_PRIZE_EGG)
        return MON_CANT_GIVE;
    return ScriptGiveEgg(value & PRIZE_ID_MASK);
}

void GameCornerPrize_SetSold(void)
{
    VarSet(sPrizeVars[GetPrizeRow()], 0);
}
