#include "global.h"
#include "battle_main.h"
#include "event_data.h"
#include "gym_reward.h"
#include "item.h"
#include "lab_research.h"
#include "pokemon.h"
#include "random.h"
#include "script_pokemon_util.h"
#include "string_util.h"
#include "constants/items.h"
#include "constants/pokemon.h"
#include "constants/species.h"

// Fantasy Retold: gym leader reward (design.md §2). After a badge the leader offers one of three rolled rewards:
// a competitive held item, a stat item (Bottle Cap / Ability Capsule / Ability Patch / mint), or an Egg of a
// non-Hoenn, non-legendary species sharing the gym's type whose line reaches GYM_REWARD_EGG_MIN_BST.
// The offer is rolled once and kept in VAR_GYM_REWARD_* until claimed. Scripts pass the gym number (1-8) in VAR_0x8008.

#define GYM_COUNT 8
#define GYM_REWARD_EGG_MIN_BST 500
#define EVO_LINE_DEPTH 3

static const enum Type sGymRewardTypes[GYM_COUNT] =
{
    TYPE_ROCK,     // Roxanne
    TYPE_FIGHTING, // Brawly
    TYPE_ELECTRIC, // Wattson
    TYPE_FIRE,     // Flannery
    TYPE_NORMAL,   // Norman
    TYPE_FLYING,   // Winona
    TYPE_PSYCHIC,  // Tate & Liza
    TYPE_WATER,    // Juan
};

static const enum Item sGymRewardItems[] =
{
    // Power
    ITEM_CHOICE_BAND, ITEM_CHOICE_SPECS, ITEM_CHOICE_SCARF, ITEM_LIFE_ORB, ITEM_EXPERT_BELT,
    ITEM_MUSCLE_BAND, ITEM_WISE_GLASSES, ITEM_SCOPE_LENS, ITEM_RAZOR_CLAW, ITEM_KINGS_ROCK,
    ITEM_WIDE_LENS, ITEM_ZOOM_LENS, ITEM_METRONOME, ITEM_LOADED_DICE, ITEM_PUNCHING_GLOVE,
    ITEM_THROAT_SPRAY, ITEM_WEAKNESS_POLICY, ITEM_BLUNDER_POLICY, ITEM_POWER_HERB,
    // Bulk and sustain
    ITEM_LEFTOVERS, ITEM_BLACK_SLUDGE, ITEM_SHELL_BELL, ITEM_BIG_ROOT, ITEM_ASSAULT_VEST,
    ITEM_EVIOLITE, ITEM_ROCKY_HELMET, ITEM_FOCUS_SASH, ITEM_FOCUS_BAND, ITEM_SITRUS_BERRY, ITEM_LUM_BERRY,
    // Utility
    ITEM_QUICK_CLAW, ITEM_BRIGHT_POWDER, ITEM_AIR_BALLOON, ITEM_HEAVY_DUTY_BOOTS, ITEM_SAFETY_GOGGLES,
    ITEM_PROTECTIVE_PADS, ITEM_COVERT_CLOAK, ITEM_CLEAR_AMULET, ITEM_ABILITY_SHIELD, ITEM_UTILITY_UMBRELLA,
    ITEM_MENTAL_HERB, ITEM_WHITE_HERB, ITEM_MIRROR_HERB, ITEM_RED_CARD, ITEM_EJECT_BUTTON, ITEM_EJECT_PACK,
    ITEM_ROOM_SERVICE, ITEM_ADRENALINE_ORB, ITEM_SHED_SHELL, ITEM_TOXIC_ORB, ITEM_FLAME_ORB, ITEM_IRON_BALL,
    // Field
    ITEM_LIGHT_CLAY, ITEM_TERRAIN_EXTENDER, ITEM_BINDING_BAND, ITEM_GRIP_CLAW,
    ITEM_DAMP_ROCK, ITEM_HEAT_ROCK, ITEM_SMOOTH_ROCK, ITEM_ICY_ROCK,
};

// Mints by the innate their nature grants (combat.md §5). Serious stands in for the neutral natures.
static const enum Item sMintsStandardOffense[] = { ITEM_ADAMANT_MINT, ITEM_MODEST_MINT, ITEM_JOLLY_MINT, ITEM_TIMID_MINT };
static const enum Item sMintsSuperLuck[] =
{
    ITEM_LONELY_MINT, ITEM_BRAVE_MINT, ITEM_NAUGHTY_MINT, ITEM_MILD_MINT,
    ITEM_QUIET_MINT, ITEM_RASH_MINT, ITEM_HASTY_MINT, ITEM_NAIVE_MINT,
};
static const enum Item sMintsBattleArmor[] = { ITEM_BOLD_MINT, ITEM_IMPISH_MINT };
static const enum Item sMintsResilient[] = { ITEM_CALM_MINT, ITEM_CAREFUL_MINT };
static const enum Item sMintsRegenerator[] = { ITEM_RELAXED_MINT, ITEM_LAX_MINT };
static const enum Item sMintsNaturalCure[] = { ITEM_SASSY_MINT, ITEM_GENTLE_MINT };

static const struct { const enum Item *mints; u32 count; } sMintGroups[] =
{
    { sMintsStandardOffense, ARRAY_COUNT(sMintsStandardOffense) },
    { sMintsSuperLuck,       ARRAY_COUNT(sMintsSuperLuck) },
    { sMintsBattleArmor,     ARRAY_COUNT(sMintsBattleArmor) },
    { sMintsResilient,       ARRAY_COUNT(sMintsResilient) },
    { sMintsRegenerator,     ARRAY_COUNT(sMintsRegenerator) },
    { sMintsNaturalCure,     ARRAY_COUNT(sMintsNaturalCure) },
};

#define SERIOUS_MINT_PERCENT 10

static u32 GetGymRewardGym(void)
{
    u32 gym = gSpecialVar_0x8008;

    if (gym < 1 || gym > GYM_COUNT)
        gym = 1;
    return gym;
}

static enum Type GetGymRewardType(void)
{
    return sGymRewardTypes[GetGymRewardGym() - 1];
}

static u32 GetGymRewardFlag(void)
{
    return FLAG_GYM_REWARD_1 + GetGymRewardGym() - 1;
}

bool32 GymReward_IsClaimed(void)
{
    return FlagGet(GetGymRewardFlag());
}

void GymReward_SetClaimed(void)
{
    FlagSet(GetGymRewardFlag());
    VarSet(VAR_GYM_REWARD_GYM, 0);
}

// Even among Bottle Cap, Ability Capsule, Ability Patch and a mint. The mint is Serious SERIOUS_MINT_PERCENT of the
// time, otherwise an even pick of the groups, then of the group's mints.
static enum Item RollStatItem(void)
{
    switch (Random() % 4)
    {
    case 0:
        return ITEM_BOTTLE_CAP;
    case 1:
        return ITEM_ABILITY_CAPSULE;
    case 2:
        return ITEM_ABILITY_PATCH;
    default:
    {
        if (Random() % 100 < SERIOUS_MINT_PERCENT)
            return ITEM_SERIOUS_MINT;
        u32 group = Random() % ARRAY_COUNT(sMintGroups);
        return sMintGroups[group].mints[Random() % sMintGroups[group].count];
    }
    }
}

// TRUE if the species or anything it evolves into has a Hoenn Pokédex number. Shared with the egg shop.
bool32 IsInHoennFamily(enum Species species, u32 depth)
{
    if (SpeciesToHoennPokedexNum(species) != 0)
        return TRUE;
    if (depth == 0)
        return FALSE;

    const struct Evolution *evolutions = GetSpeciesEvolutions(species);
    if (evolutions == NULL)
        return FALSE;
    for (u32 i = 0; evolutions[i].method != EVOLUTIONS_END; i++)
    {
        if (IsInHoennFamily(SanitizeSpeciesId(evolutions[i].targetSpecies), depth - 1))
            return TRUE;
    }
    return FALSE;
}

static bool32 IsEggRewardCandidate(enum Species species, enum Type type, const u8 *isEvolutionTarget)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];

    if (!IsSpeciesEnabled(species) || !IsSpeciesOfType(species, type))
        return FALSE;
    // Only the first stage of a line, in its default form (no regional / battle forms).
    if (isEvolutionTarget[species / 8] & (1 << (species % 8)))
        return FALSE;
    const u16 *forms = GetSpeciesFormTable(species);
    if (forms != NULL && forms[0] != species)
        return FALSE;
    if (info->isRestrictedLegendary || info->isSubLegendary || info->isMythical
     || info->isUltraBeast || info->isParadox)
        return FALSE;
    // Can't breed and never evolves: Unown-likes (babies can't breed either, but they evolve).
    const struct Evolution *evolutions = GetSpeciesEvolutions(species);
    if (info->eggGroups[0] == EGG_GROUP_NO_EGGS_DISCOVERED && (evolutions == NULL || evolutions[0].method == EVOLUTIONS_END))
        return FALSE;
    if (IsInHoennFamily(species, EVO_LINE_DEPTH))
        return FALSE;
    if (GetLineBST(species, EVO_LINE_DEPTH) < GYM_REWARD_EGG_MIN_BST)
        return FALSE;
    return TRUE;
}

static enum Species PickEggRewardSpecies(enum Type type)
{
    u8 isEvolutionTarget[(NUM_SPECIES + 7) / 8] = {0};
    u32 count = 0;
    enum Species species;

    for (species = 1; species < NUM_SPECIES; species++)
    {
        if (!IsSpeciesEnabled(species))
            continue;
        const struct Evolution *evolutions = GetSpeciesEvolutions(species);
        if (evolutions == NULL)
            continue;
        for (u32 i = 0; evolutions[i].method != EVOLUTIONS_END; i++)
        {
            enum Species target = SanitizeSpeciesId(evolutions[i].targetSpecies);
            isEvolutionTarget[target / 8] |= 1 << (target % 8);
        }
    }

    for (species = 1; species < NUM_SPECIES; species++)
    {
        if (IsEggRewardCandidate(species, type, isEvolutionTarget))
            count++;
    }
    if (count == 0)
        return SPECIES_NONE;

    count = Random() % count;
    for (species = 1; species < NUM_SPECIES; species++)
    {
        if (IsEggRewardCandidate(species, type, isEvolutionTarget) && count-- == 0)
            return species;
    }
    return SPECIES_NONE;
}

// Rolls this gym's offer unless it is already the stored one, so backing out and talking again shows the same three.
// Buffers the held item (STR_VAR_1), the stat item (STR_VAR_2) and the Egg's species (STR_VAR_3) for the menu.
void GymReward_RollOffer(void)
{
    u32 gym = GetGymRewardGym();

    if (VarGet(VAR_GYM_REWARD_GYM) != gym)
    {
        VarSet(VAR_GYM_REWARD_HELD_ITEM, sGymRewardItems[Random() % ARRAY_COUNT(sGymRewardItems)]);
        VarSet(VAR_GYM_REWARD_STAT_ITEM, RollStatItem());
        VarSet(VAR_GYM_REWARD_EGG_SPECIES, PickEggRewardSpecies(GetGymRewardType()));
        VarSet(VAR_GYM_REWARD_GYM, gym);
    }
    CopyItemName(VarGet(VAR_GYM_REWARD_HELD_ITEM), gStringVar1);
    CopyItemName(VarGet(VAR_GYM_REWARD_STAT_ITEM), gStringVar2);
    StringCopy(gStringVar3, GetSpeciesName(VarGet(VAR_GYM_REWARD_EGG_SPECIES)));
}

u16 GymReward_GetHeldItem(void)
{
    return VarGet(VAR_GYM_REWARD_HELD_ITEM);
}

u16 GymReward_GetStatItem(void)
{
    return VarGet(VAR_GYM_REWARD_STAT_ITEM);
}

// Gives the Egg to the party, or the PC when the party is full; returns MON_GIVEN_TO_PARTY / MON_GIVEN_TO_PC / MON_CANT_GIVE.
u16 GymReward_GiveEgg(void)
{
    enum Species species = VarGet(VAR_GYM_REWARD_EGG_SPECIES);

    if (species == SPECIES_NONE)
        return MON_CANT_GIVE;
    return ScriptGiveEgg(species);
}
