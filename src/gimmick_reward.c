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

static bool32 IsLegendarySpecies(enum Species species)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];

    return info->isRestrictedLegendary || info->isSubLegendary || info->isMythical;
}

// Species Z-Crystals of legendary / mythical Pokémon, left out of the Game Corner like their Mega Stones.
static bool32 IsLegendaryZCrystal(enum Item item)
{
    switch (item)
    {
    case ITEM_MEWNIUM_Z:
    case ITEM_TAPUNIUM_Z:
    case ITEM_SOLGANIUM_Z:
    case ITEM_LUNALIUM_Z:
    case ITEM_MARSHADIUM_Z:
    case ITEM_ULTRANECROZIUM_Z:
        return TRUE;
    default:
        return FALSE;
    }
}

// A Mega Stone counts only if some enabled, non-legendary species Mega Evolves with it. One species scan marks
// them all.
static void MarkUsableMegaStones(u8 *usable)
{
    for (enum Species species = 1; species < NUM_SPECIES; species++)
    {
        if (!IsSpeciesEnabled(species) || IsLegendarySpecies(species))
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

// Not in the bag yet; a Mega Stone also needs some enabled, non-legendary species that Mega Evolves with it, and a
// Z-Crystal must not be a legendary's.
static bool32 IsStockCandidate(enum Item item, enum HoldEffect holdEffect, const u8 *usableStones)
{
    if (GetItemHoldEffect(item) != holdEffect || CheckBagHasItem(item, 1))
        return FALSE;
    if (holdEffect == HOLD_EFFECT_MEGA_STONE && !(usableStones[item / 8] & (1 << (item % 8))))
        return FALSE;
    if (holdEffect == HOLD_EFFECT_Z_CRYSTAL && IsLegendaryZCrystal(item))
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

// Overworld Mega Stones / Z-Crystals (design.md §2). Every Hoenn vitamin, X-item and repel pick-up (item ball or
// hidden item) gives one instead. The table is rolled from the trainer ID, so it is fixed per save and stored nowhere.

#define OW_GIMMICK_SEED      0x4F574D5A // "OWMZ"
#define OW_GIMMICK_MEGA_ODDS 2          // Mega Stone 2 times in 3, else a type Z-Crystal

// Spots by pick-up flag, in rough story order.
static const u16 sOverworldGimmickSpots[] =
{
    FLAG_ITEM_ROUTE_103_GUARD_SPEC,
    FLAG_ITEM_ROUTE_104_X_ACCURACY,
    FLAG_ITEM_PETALBURG_WOODS_X_ATTACK,
    FLAG_ITEM_RUSTBORO_CITY_X_DEFEND,
    FLAG_ITEM_ROUTE_116_X_SPECIAL,
    FLAG_ITEM_ROUTE_116_HP_UP,
    FLAG_ITEM_ROUTE_116_REPEL,
    FLAG_ITEM_ROUTE_105_IRON,
    FLAG_ITEM_ROUTE_106_PROTEIN,
    FLAG_ITEM_GRANITE_CAVE_B2F_REPEL,
    FLAG_ITEM_ROUTE_110_DIRE_HIT,
    FLAG_ITEM_MAUVILLE_CITY_X_SPEED,
    FLAG_HIDDEN_ITEM_ROUTE_117_REPEL,
    FLAG_ITEM_ROUTE_111_HP_UP,
    FLAG_HIDDEN_ITEM_ROUTE_111_PROTEIN,
    FLAG_ITEM_ROUTE_113_SUPER_REPEL,
    FLAG_ITEM_ROUTE_114_PROTEIN,
    FLAG_HIDDEN_ITEM_ROUTE_114_CARBOS,
    FLAG_ITEM_ROUTE_115_IRON,
    FLAG_HIDDEN_ITEM_ROUTE_118_IRON,
    FLAG_ITEM_ROUTE_119_ZINC,
    FLAG_HIDDEN_ITEM_ROUTE_119_CALCIUM,
    FLAG_ITEM_ROUTE_119_SUPER_REPEL,
    FLAG_HIDDEN_ITEM_ROUTE_120_ZINC,
    FLAG_ITEM_ROUTE_121_CARBOS,
    FLAG_ITEM_ROUTE_121_ZINC,
    FLAG_HIDDEN_ITEM_ROUTE_121_HP_UP,
    FLAG_ITEM_LILYCOVE_CITY_MAX_REPEL,
    FLAG_ITEM_SAFARI_ZONE_NORTH_CALCIUM,
    FLAG_HIDDEN_ITEM_SAFARI_ZONE_NORTH_EAST_ZINC,
    FLAG_ITEM_ROUTE_123_CALCIUM,
    FLAG_HIDDEN_ITEM_ROUTE_123_SUPER_REPEL,
    FLAG_ITEM_MT_PYRE_3F_SUPER_REPEL,
    FLAG_HIDDEN_ITEM_MT_PYRE_SUMMIT_ZINC,
    FLAG_HIDDEN_ITEM_UNDERWATER_124_CARBOS,
    FLAG_HIDDEN_ITEM_UNDERWATER_124_CALCIUM,
    FLAG_HIDDEN_ITEM_UNDERWATER_126_IRON,
    FLAG_HIDDEN_ITEM_UNDERWATER_127_HP_UP,
    FLAG_HIDDEN_ITEM_UNDERWATER_128_PROTEIN,
    FLAG_ITEM_ROUTE_127_ZINC,
    FLAG_ITEM_ROUTE_127_CARBOS,
    FLAG_ITEM_ROUTE_132_PROTEIN,
    FLAG_ITEM_ROUTE_134_CARBOS,
    FLAG_HIDDEN_ITEM_VICTORY_ROAD_B2F_MAX_REPEL,
    FLAG_ITEM_ARTISAN_CAVE_1F_CARBOS,
    FLAG_ITEM_ARTISAN_CAVE_B1F_HP_UP,
    FLAG_HIDDEN_ITEM_ARTISAN_CAVE_B1F_ZINC,
    FLAG_HIDDEN_ITEM_ARTISAN_CAVE_B1F_CALCIUM,
    FLAG_HIDDEN_ITEM_ARTISAN_CAVE_B1F_PROTEIN,
    FLAG_HIDDEN_ITEM_ARTISAN_CAVE_B1F_IRON,
};

#define OW_GIMMICK_SPOTS ARRAY_COUNT(sOverworldGimmickSpots)

enum
{
    OW_GIMMICK_MEGA,
    OW_GIMMICK_Z,
    OW_GIMMICK_CATEGORIES,
};

struct OverworldGimmickPools
{
    u16 items[OW_GIMMICK_CATEGORIES][MAX_STOCK_CANDIDATES];
    u8 counts[OW_GIMMICK_CATEGORIES];
};

// Mega side: the Game Corner's Mega Stones. Z side: the 18 type Z-Crystals only.
static void BuildOverworldGimmickPools(struct OverworldGimmickPools *pools)
{
    u8 usableStones[(ITEMS_COUNT + 7) / 8] = {0};

    MarkUsableMegaStones(usableStones);
    pools->counts[OW_GIMMICK_MEGA] = 0;
    for (enum Item item = ITEM_NONE + 1; item < ITEMS_COUNT && pools->counts[OW_GIMMICK_MEGA] < MAX_STOCK_CANDIDATES; item++)
    {
        if (GetItemHoldEffect(item) == HOLD_EFFECT_MEGA_STONE && (usableStones[item / 8] & (1 << (item % 8))))
            pools->items[OW_GIMMICK_MEGA][pools->counts[OW_GIMMICK_MEGA]++] = item;
    }
    pools->counts[OW_GIMMICK_Z] = 0;
    for (enum Item item = ITEM_NORMALIUM_Z; item <= ITEM_FAIRIUM_Z; item++)
        pools->items[OW_GIMMICK_Z][pools->counts[OW_GIMMICK_Z]++] = item;
}

static void ShuffleItems(u16 *items, u32 count, rng_value_t *rng)
{
    for (u32 i = count; i > 1; i--)
    {
        u32 j = LocalRandom(rng) % i;
        u16 temp = items[i - 1];

        items[i - 1] = items[j];
        items[j] = temp;
    }
}

// Each spot flips Mega / Z, then takes the next item of that category's shuffled pool, so no item is seeded twice.
// A spent category falls back to the other one. Shuffles the pools in place (the reroll doesn't care about order).
static void RollOverworldGimmickTable(struct OverworldGimmickPools *pools, u16 *table)
{
    u32 next[OW_GIMMICK_CATEGORIES] = {0};
    rng_value_t rng = LocalRandomSeed(READ_OTID_FROM_SAVE ^ OW_GIMMICK_SEED);

    for (u32 category = 0; category < OW_GIMMICK_CATEGORIES; category++)
        ShuffleItems(pools->items[category], pools->counts[category], &rng);
    for (u32 spot = 0; spot < OW_GIMMICK_SPOTS; spot++)
    {
        u32 category = (LocalRandom(&rng) % 3 < OW_GIMMICK_MEGA_ODDS) ? OW_GIMMICK_MEGA : OW_GIMMICK_Z;

        if (next[category] >= pools->counts[category])
            category ^= 1;
        if (next[category] < pools->counts[category])
            table[spot] = pools->items[category][next[category]++];
        else
            table[spot] = ITEM_RARE_CANDY;
    }
}

// In the bag or held by a party Pokémon.
static bool32 IsGimmickItemOwned(u16 item)
{
    if (CheckBagHasItem(item, 1))
        return TRUE;
    for (u32 i = 0; i < PARTY_SIZE; i++)
    {
        if (GetMonData(&gPlayerParty[i], MON_DATA_HELD_ITEM) == item)
            return TRUE;
    }
    return FALSE;
}

// Seeded on a spot other than this one that hasn't been picked up yet.
static bool32 IsSeededOnOtherSpot(u16 item, const u16 *table, u32 spot)
{
    for (u32 other = 0; other < OW_GIMMICK_SPOTS; other++)
    {
        if (other != spot && table[other] == item && !FlagGet(sOverworldGimmickSpots[other]))
            return TRUE;
    }
    return FALSE;
}

static bool32 IsRerollCandidate(u16 item, bool32 seededElsewhere, const u16 *table, u32 spot)
{
    return !IsGimmickItemOwned(item) && IsSeededOnOtherSpot(item, table, spot) == seededElsewhere;
}

// The item at a replaced pick-up, by its flag; ITEM_NONE if the flag isn't one of the spots. A seeded item the
// player already owns is rerolled, uniformly within the first non-empty tier: same category not seeded on another
// unpicked spot, other category likewise, same category seeded elsewhere, other category seeded elsewhere. The
// reroll is seeded by the trainer ID and the spot, so it only changes with what the player owns. Nothing left:
// Rare Candy.
u16 GetOverworldGimmickItem(u32 flag)
{
    struct OverworldGimmickPools pools;
    u16 table[OW_GIMMICK_SPOTS];
    u32 spot;

    for (spot = 0; spot < OW_GIMMICK_SPOTS; spot++)
    {
        if (sOverworldGimmickSpots[spot] == flag)
            break;
    }
    if (spot == OW_GIMMICK_SPOTS)
        return ITEM_NONE;

    BuildOverworldGimmickPools(&pools);
    RollOverworldGimmickTable(&pools, table);

    u16 seeded = table[spot];
    if (seeded == ITEM_RARE_CANDY || !IsGimmickItemOwned(seeded))
        return seeded;

    rng_value_t rng = LocalRandomSeed(READ_OTID_FROM_SAVE ^ (OW_GIMMICK_SEED + flag));
    u32 seededCategory = (GetItemHoldEffect(seeded) == HOLD_EFFECT_MEGA_STONE) ? OW_GIMMICK_MEGA : OW_GIMMICK_Z;

    for (u32 tier = 0; tier < 4; tier++)
    {
        u32 category = (tier % 2 == 0) ? seededCategory : (seededCategory ^ 1);
        bool32 seededElsewhere = (tier >= 2);
        u32 count = 0;

        for (u32 i = 0; i < pools.counts[category]; i++)
        {
            if (IsRerollCandidate(pools.items[category][i], seededElsewhere, table, spot))
                count++;
        }
        if (count == 0)
            continue;

        u32 pick = LocalRandom(&rng) % count;
        for (u32 i = 0; i < pools.counts[category]; i++)
        {
            u16 item = pools.items[category][i];

            if (IsRerollCandidate(item, seededElsewhere, table, spot) && pick-- == 0)
                return item;
        }
    }
    return ITEM_RARE_CANDY;
}
