#include "global.h"
#include "battle_main.h"
#include "event_data.h"
#include "pokemon.h"
#include "random.h"
#include "script_pokemon_util.h"
#include "string_util.h"
#include "constants/items.h"
#include "constants/pokemon.h"
#include "constants/species.h"

// Fantasy Retold: gym leader reward (design.md §10). After a badge the leader offers a choice: a random
// competitive held item, or an Egg of a random non-Hoenn, non-legendary species sharing the gym's type.
// Scripts pass the gym number (1-8) in VAR_0x8008.

#define GYM_COUNT 8

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

static enum Type GetGymRewardType(void)
{
    u32 gym = gSpecialVar_0x8008;

    if (gym < 1 || gym > GYM_COUNT)
        gym = 1;
    return sGymRewardTypes[gym - 1];
}

static u32 GetGymRewardFlag(void)
{
    u32 gym = gSpecialVar_0x8008;

    if (gym < 1 || gym > GYM_COUNT)
        gym = 1;
    return FLAG_GYM_REWARD_1 + gym - 1;
}

bool32 GymReward_IsClaimed(void)
{
    return FlagGet(GetGymRewardFlag());
}

void GymReward_SetClaimed(void)
{
    FlagSet(GetGymRewardFlag());
}

// Buffers the gym's type name in STR_VAR_2 for the offer.
void GymReward_BufferType(void)
{
    StringCopy(gStringVar2, gTypesInfo[GetGymRewardType()].name);
}

// Rolled when the player picks it, so backing out and talking again can't fish for an item.
u16 GymReward_RollItem(void)
{
    return sGymRewardItems[Random() % ARRAY_COUNT(sGymRewardItems)];
}

// TRUE if the species or anything it evolves into has a Hoenn Pokédex number.
static bool32 IsInHoennFamily(enum Species species, u32 depth)
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
    if (IsInHoennFamily(species, 3))
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

// Gives the Egg to the party, or the PC when the party is full; returns MON_GIVEN_TO_PARTY / MON_GIVEN_TO_PC / MON_CANT_GIVE.
u16 GymReward_GiveEgg(void)
{
    enum Species species = PickEggRewardSpecies(GetGymRewardType());

    if (species == SPECIES_NONE)
        return MON_CANT_GIVE;
    return ScriptGiveEgg(species);
}
