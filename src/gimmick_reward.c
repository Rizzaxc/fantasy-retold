#include "global.h"
#include "egg_shop.h"
#include "event_data.h"
#include "gimmick_reward.h"
#include "item.h"
#include "overworld.h"
#include "pokemon.h"
#include "random.h"
#include "starter_choose.h"
#include "string_util.h"
#include "constants/form_change_types.h"
#include "constants/gimmick_reward.h"
#include "constants/hold_effects.h"
#include "constants/items.h"

// Fantasy Retold: Mega Ring / Z-Ring and their items (design.md §2). Wattson lets the player pick one ring; Birch
// then calls them to the lab for the starter's Mega Stone or matching Z-Crystal. The Mauville Game Corner stocks
// three Mega Stones and three Z-Crystals the player doesn't have, restocked like the egg shop.

// Outdoor steps after receiving the ring before Birch calls, so it doesn't ring inside the gym's exit.
#define BIRCH_GIMMICK_CALL_STEPS 10

static EWRAM_DATA u8 sBirchGimmickCallSteps = 0;

// A Mega Stone counts only if some enabled species Mega Evolves with it. One species scan marks them all.
static void MarkUsableMegaStones(u8 *usable)
{
    for (enum Species species = 1; species < NUM_SPECIES; species++)
    {
        if (!IsSpeciesEnabled(species))
            continue;
        const struct FormChange *formChanges = GetSpeciesFormChanges(species);
        if (formChanges == NULL)
            continue;
        for (u32 i = 0; formChanges[i].method != FORM_CHANGE_TERMINATOR; i++)
        {
            u32 item = formChanges[i].param1;

            if (formChanges[i].method == FORM_CHANGE_BATTLE_MEGA_EVOLUTION_ITEM
             && item < ITEMS_COUNT
             && IsSpeciesEnabled(formChanges[i].targetSpecies))
                usable[item / 8] |= 1 << (item % 8);
        }
    }
}

static const u16 sGameCornerStockVars[][GAME_CORNER_STOCK_SLOTS] =
{
    [GAME_CORNER_STOCK_MEGA_STONE] = {VAR_GAME_CORNER_MEGA_STONE_1, VAR_GAME_CORNER_MEGA_STONE_2, VAR_GAME_CORNER_MEGA_STONE_3},
    [GAME_CORNER_STOCK_Z_CRYSTAL]  = {VAR_GAME_CORNER_Z_CRYSTAL_1, VAR_GAME_CORNER_Z_CRYSTAL_2, VAR_GAME_CORNER_Z_CRYSTAL_3},
};

static const enum HoldEffect sGameCornerStockHoldEffects[] =
{
    [GAME_CORNER_STOCK_MEGA_STONE] = HOLD_EFFECT_MEGA_STONE,
    [GAME_CORNER_STOCK_Z_CRYSTAL]  = HOLD_EFFECT_Z_CRYSTAL,
};

// More than there are Mega Stones (92) or Z-Crystals (35).
#define MAX_STOCK_CANDIDATES 128

static const u8 sText_SoldOut[] = _("SOLD OUT");

// Not in the bag yet; a Mega Stone also needs some enabled species that Mega Evolves with it.
static bool32 IsStockCandidate(enum Item item, enum HoldEffect holdEffect, const u8 *usableStones)
{
    if (GetItemHoldEffect(item) != holdEffect || CheckBagHasItem(item, 1))
        return FALSE;
    if (holdEffect == HOLD_EFFECT_MEGA_STONE && !(usableStones[item / 8] & (1 << (item % 8))))
        return FALSE;
    return TRUE;
}

// Three distinct items, uniform among the candidates. Fewer candidates leave the extra slots empty (sold out).
static void StockCategory(u32 category, const u8 *usableStones)
{
    enum HoldEffect holdEffect = sGameCornerStockHoldEffects[category];
    u16 candidates[MAX_STOCK_CANDIDATES];
    u32 count = 0;

    for (enum Item item = ITEM_NONE + 1; item < ITEMS_COUNT && count < MAX_STOCK_CANDIDATES; item++)
    {
        if (IsStockCandidate(item, holdEffect, usableStones))
            candidates[count++] = item;
    }
    for (u32 slot = 0; slot < GAME_CORNER_STOCK_SLOTS; slot++)
    {
        u16 item = ITEM_NONE;

        if (slot < count)
        {
            u32 pick = slot + Random() % (count - slot);

            item = candidates[pick];
            candidates[pick] = candidates[slot];
            candidates[slot] = item;
        }
        VarSet(sGameCornerStockVars[category][slot], item);
    }
}

static u32 GetGameCornerCategory(void)
{
    return gSpecialVar_0x8004 < ARRAY_COUNT(sGameCornerStockVars) ? gSpecialVar_0x8004 : GAME_CORNER_STOCK_MEGA_STONE;
}

static u16 GetGameCornerStockItem(void)
{
    if (gSpecialVar_0x8005 >= GAME_CORNER_STOCK_SLOTS)
        return ITEM_NONE;
    return VarGet(sGameCornerStockVars[GetGameCornerCategory()][gSpecialVar_0x8005]);
}

// Restocks both categories when a badge, rival battle or Elite Four clear has happened since the last stock
// (the egg shop's milestones). The first visit always stocks.
void GameCorner_Restock(void)
{
    u32 key = GetEggShopMilestones() + 1;

    if (VarGet(VAR_GAME_CORNER_STOCK_KEY) == key)
        return;

    u8 usableStones[(ITEMS_COUNT + 7) / 8] = {0};

    MarkUsableMegaStones(usableStones);
    for (u32 category = 0; category < ARRAY_COUNT(sGameCornerStockVars); category++)
        StockCategory(category, usableStones);
    VarSet(VAR_GAME_CORNER_STOCK_KEY, key);
}

// Buffers the category's (VAR_0x8004) three rows, item name or "SOLD OUT", in STR_VAR_1-3.
void GameCorner_BufferStock(void)
{
    u8 *const rows[GAME_CORNER_STOCK_SLOTS] = {gStringVar1, gStringVar2, gStringVar3};
    u32 category = GetGameCornerCategory();

    for (u32 slot = 0; slot < GAME_CORNER_STOCK_SLOTS; slot++)
    {
        u16 item = VarGet(sGameCornerStockVars[category][slot]);

        if (item == ITEM_NONE)
            StringCopy(rows[slot], sText_SoldOut);
        else
            CopyItemName(item, rows[slot]);
    }
}

// The item in slot VAR_0x8005 of category VAR_0x8004 (ITEM_NONE if sold out), its name in STR_VAR_1.
u16 GameCorner_BufferOffer(void)
{
    u16 item = GetGameCornerStockItem();

    if (item != ITEM_NONE)
        CopyItemName(item, gStringVar1);
    return item;
}

void GameCorner_SetSold(void)
{
    if (gSpecialVar_0x8005 < GAME_CORNER_STOCK_SLOTS)
        VarSet(sGameCornerStockVars[GetGameCornerCategory()][gSpecialVar_0x8005], ITEM_NONE);
}

#define MAX_STARTER_MEGA_STONES 4
#define EVO_LINE_DEPTH 3

// Every Mega Stone used by the species or anything it evolves into (Charizard has two).
static u32 CollectLineMegaStones(enum Species species, u32 depth, enum Item *stones, u32 count)
{
    const struct FormChange *formChanges = GetSpeciesFormChanges(species);

    if (formChanges != NULL)
    {
        for (u32 i = 0; formChanges[i].method != FORM_CHANGE_TERMINATOR && count < MAX_STARTER_MEGA_STONES; i++)
        {
            if (formChanges[i].method == FORM_CHANGE_BATTLE_MEGA_EVOLUTION_ITEM && IsSpeciesEnabled(formChanges[i].targetSpecies))
                stones[count++] = formChanges[i].param1;
        }
    }
    if (depth == 0)
        return count;

    const struct Evolution *evolutions = GetSpeciesEvolutions(species);
    if (evolutions == NULL)
        return count;
    for (u32 i = 0; evolutions[i].method != EVOLUTIONS_END; i++)
        count = CollectLineMegaStones(SanitizeSpeciesId(evolutions[i].targetSpecies), depth - 1, stones, count);
    return count;
}

// Birch's gift for the starter (VAR_STARTER_MON): its line's Mega Stone with the Mega Ring, else the Z-Crystal of
// its first type. ITEM_NONE if there is none. The name goes in STR_VAR_1.
u16 BirchGimmick_GetGift(void)
{
    enum Species starter = GetStarterPokemon(VarGet(VAR_STARTER_MON));
    enum Item gift = ITEM_NONE;

    if (CheckBagHasItem(ITEM_MEGA_RING, 1))
    {
        enum Item stones[MAX_STARTER_MEGA_STONES];
        u32 count = CollectLineMegaStones(starter, EVO_LINE_DEPTH, stones, 0);

        if (count != 0)
            gift = stones[Random() % count];
    }
    else
    {
        enum Type type = GetSpeciesType(starter, 0);

        for (enum Item item = ITEM_NORMALIUM_Z; item <= ITEM_FAIRIUM_Z; item++)
        {
            if (GetItemSecondaryId(item) == type)
            {
                gift = item;
                break;
            }
        }
    }
    CopyItemName(gift, gStringVar1);
    return gift;
}

bool32 ShouldDoBirchGimmickCall(void)
{
    if (!FlagGet(FLAG_SYS_POKENAV_GET) || VarGet(VAR_BIRCH_GIMMICK_STATE) != BIRCH_GIMMICK_RING)
        return FALSE;
    switch (gMapHeader.mapType)
    {
    case MAP_TYPE_TOWN:
    case MAP_TYPE_CITY:
    case MAP_TYPE_ROUTE:
    case MAP_TYPE_OCEAN_ROUTE:
        if (++sBirchGimmickCallSteps < BIRCH_GIMMICK_CALL_STEPS)
            return FALSE;
        sBirchGimmickCallSteps = 0;
        return TRUE;
    default:
        return FALSE;
    }
}
