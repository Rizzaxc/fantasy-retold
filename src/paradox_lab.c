#include "global.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "party_menu.h"
#include "pokemon.h"
#include "random.h"
#include "signature_move.h"
#include "string_util.h"
#include "constants/abilities.h"
#include "constants/paradox_lab.h"
#include "constants/pokedex.h"
#include "constants/species.h"

// Fantasy Retold: Lanette's Paradox Lab (design.md §2). After badge 5 Lanette gives one random Lv10 paradox (once per
// save), then converts a party Pokémon of a canon source species into its paradox, for free, one-way. The converted
// Pokémon keeps level, nature, EVs, IVs and everything else stored on it; its ability becomes the paradox's, and its
// moves, Signature Move and move records are wiped (the four slots get the paradox's level-up moves at its level).
// Scripts pass the party slot in VAR_0x8004 and the ancient / future pick in VAR_0x8005.

struct ParadoxSource
{
    u16 source;
    u16 paradox[2]; // PARADOX_LAB_ANCIENT, PARADOX_LAB_FUTURE; SPECIES_NONE if there is none
};

// Only the exact canon species convert (not the rest of their lines). The legendary-based paradoxes (Walking Wake,
// Gouging Fire, Raging Bolt, Iron Leaves, Iron Boulder, Iron Crown) have no source here: their sources can't be caught.
static const struct ParadoxSource sParadoxSources[] =
{
    { SPECIES_DONPHAN,    { SPECIES_GREAT_TUSK,   SPECIES_IRON_TREADS  } },
    { SPECIES_VOLCARONA,  { SPECIES_SLITHER_WING, SPECIES_IRON_MOTH    } },
    { SPECIES_JIGGLYPUFF, { SPECIES_SCREAM_TAIL,  SPECIES_NONE         } },
    { SPECIES_AMOONGUSS,  { SPECIES_BRUTE_BONNET, SPECIES_NONE         } },
    { SPECIES_MISDREAVUS, { SPECIES_FLUTTER_MANE, SPECIES_NONE         } },
    { SPECIES_MAGNETON,   { SPECIES_SANDY_SHOCKS, SPECIES_NONE         } },
    { SPECIES_SALAMENCE,  { SPECIES_ROARING_MOON, SPECIES_NONE         } },
    { SPECIES_DELIBIRD,   { SPECIES_NONE,         SPECIES_IRON_BUNDLE  } },
    { SPECIES_HARIYAMA,   { SPECIES_NONE,         SPECIES_IRON_HANDS   } },
    { SPECIES_HYDREIGON,  { SPECIES_NONE,         SPECIES_IRON_JUGULIS } },
    { SPECIES_TYRANITAR,  { SPECIES_NONE,         SPECIES_IRON_THORNS  } },
    { SPECIES_GARDEVOIR,  { SPECIES_NONE,         SPECIES_IRON_VALIANT } },
    { SPECIES_GALLADE,    { SPECIES_NONE,         SPECIES_IRON_VALIANT } },
};

// The gift: the 14 non-legendary paradoxes, evenly.
static const u16 sParadoxGiftPool[] =
{
    SPECIES_GREAT_TUSK,
    SPECIES_SCREAM_TAIL,
    SPECIES_BRUTE_BONNET,
    SPECIES_FLUTTER_MANE,
    SPECIES_SLITHER_WING,
    SPECIES_SANDY_SHOCKS,
    SPECIES_ROARING_MOON,
    SPECIES_IRON_TREADS,
    SPECIES_IRON_BUNDLE,
    SPECIES_IRON_HANDS,
    SPECIES_IRON_JUGULIS,
    SPECIES_IRON_MOTH,
    SPECIES_IRON_THORNS,
    SPECIES_IRON_VALIANT,
};

static const struct ParadoxSource *GetParadoxSource(enum Species species)
{
    for (u32 i = 0; i < ARRAY_COUNT(sParadoxSources); i++)
    {
        if (sParadoxSources[i].source == species)
            return &sParadoxSources[i];
    }
    return NULL;
}

static enum Species GetParadoxTarget(enum Species species, u32 choice)
{
    const struct ParadoxSource *source = GetParadoxSource(species);

    if (source == NULL || choice > PARADOX_LAB_FUTURE || !IsSpeciesEnabled(source->paradox[choice]))
        return SPECIES_NONE;
    return source->paradox[choice];
}

static struct Pokemon *GetChosenMon(void)
{
    return &gParties[B_TRAINER_PLAYER][gSpecialVar_0x8004];
}

// Rolls the gift species (returned, and its name in STR_VAR_1 for gText_NicknameThisPokemon). The script gives it with givemon.
u16 ParadoxLab_RollGift(void)
{
    u16 pool[ARRAY_COUNT(sParadoxGiftPool)];
    u32 count = 0;
    enum Species species;

    for (u32 i = 0; i < ARRAY_COUNT(sParadoxGiftPool); i++)
    {
        if (IsSpeciesEnabled(sParadoxGiftPool[i]))
            pool[count++] = sParadoxGiftPool[i];
    }
    species = count != 0 ? pool[Random() % count] : SPECIES_GREAT_TUSK;
    StringCopy(gStringVar1, GetSpeciesName(species));
    return species;
}

// Checks the mon in VAR_0x8004 and returns a PARADOX_LAB_MON_* result. Buffers its nickname (STR_VAR_1).
// ONE: VAR_0x8005 = the pick and STR_VAR_2 = the paradox. TWO: STR_VAR_2 = ancient, STR_VAR_3 = future.
u16 ParadoxLab_CheckMon(void)
{
    struct Pokemon *mon = GetChosenMon();
    enum Species species = GetMonData(mon, MON_DATA_SPECIES);
    enum Species ancient, future;

    if (GetMonData(mon, MON_DATA_IS_EGG))
        return PARADOX_LAB_MON_EGG;
    GetMonNickname(mon, gStringVar1);
    if (gSpeciesInfo[species].isParadox)
        return PARADOX_LAB_MON_PARADOX;
    ancient = GetParadoxTarget(species, PARADOX_LAB_ANCIENT);
    future = GetParadoxTarget(species, PARADOX_LAB_FUTURE);
    if (ancient != SPECIES_NONE && future != SPECIES_NONE)
    {
        StringCopy(gStringVar2, GetSpeciesName(ancient));
        StringCopy(gStringVar3, GetSpeciesName(future));
        return PARADOX_LAB_MON_TWO;
    }
    if (ancient == SPECIES_NONE && future == SPECIES_NONE)
        return PARADOX_LAB_MON_NONE;
    gSpecialVar_0x8005 = ancient != SPECIES_NONE ? PARADOX_LAB_ANCIENT : PARADOX_LAB_FUTURE;
    StringCopy(gStringVar2, GetSpeciesName(ancient != SPECIES_NONE ? ancient : future));
    return PARADOX_LAB_MON_ONE;
}

// The paradox the mon in VAR_0x8004 becomes with pick VAR_0x8005, into STR_VAR_2 (nickname in STR_VAR_1).
void ParadoxLab_BufferTarget(void)
{
    struct Pokemon *mon = GetChosenMon();

    GetMonNickname(mon, gStringVar1);
    StringCopy(gStringVar2, GetSpeciesName(GetParadoxTarget(GetMonData(mon, MON_DATA_SPECIES), gSpecialVar_0x8005)));
}

// Converts the mon in VAR_0x8004 with pick VAR_0x8005. STR_VAR_1 = its nickname before, STR_VAR_2 = the paradox,
// VAR_0x8006 = the paradox species (for ParadoxLab_IsQuarkDrive). FALSE (nothing changed) if it can't.
bool16 ParadoxLab_Convert(void)
{
    struct Pokemon *mon = GetChosenMon();
    enum Species species = GetMonData(mon, MON_DATA_SPECIES);
    enum Species target = GetParadoxTarget(species, gSpecialVar_0x8005);
    u32 level, exp;
    u8 zero = 0;

    if (target == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG))
        return FALSE;

    GetMonNickname(mon, gStringVar1);
    StringCopy(gStringVar2, GetSpeciesName(target));
    gSpecialVar_0x8006 = target;
    level = GetMonData(mon, MON_DATA_LEVEL);

    // Signature Move and move records go first: they are keyed by personality, which doesn't change.
    ForgetSignatureMove(&mon->box);

    SetMonData(mon, MON_DATA_SPECIES, &target);
    // Same level on the paradox's growth rate.
    exp = gExperienceTables[gSpeciesInfo[target].growthRate][level];
    SetMonData(mon, MON_DATA_EXP, &exp);
    // Paradoxes have one ability (Protosynthesis / Quark Drive); the nature innate is unchanged.
    SetMonData(mon, MON_DATA_ABILITY_NUM, &zero);
    // Every slot move goes; the four slots get the paradox's level-up moves at this level.
    SetMonData(mon, MON_DATA_PP_BONUSES, &zero);
    GiveMonInitialMoveset(mon);
    CalculateMonStats(mon);

    EvolutionRenameMon(mon, species, target);
    HandleSetPokedexFlagFromMon(mon, FLAG_SET_SEEN);
    HandleSetPokedexFlagFromMon(mon, FLAG_SET_CAUGHT);
    UpdateFollowingPokemon();
    return TRUE;
}

// Whether the species in VAR_0x8006 has Quark Drive: Lanette adds a Booster Energy whenever she hands one over.
bool16 ParadoxLab_IsQuarkDrive(void)
{
    return GetSpeciesAbility(gSpecialVar_0x8006, 0) == ABILITY_QUARK_DRIVE;
}

// Whether the species in VAR_0x8005 is a paradox: the Day-Care Lady won't raise one (design.md §2, Daycare Training).
bool16 ParadoxLab_IsParadox(void)
{
    return gSpeciesInfo[gSpecialVar_0x8005].isParadox;
}
