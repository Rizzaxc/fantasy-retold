#include "global.h"
#include "battle_main.h"
#include "event_data.h"
#include "event_object_movement.h"
#include "gym_reward.h"
#include "item.h"
#include "lab_research.h"
#include "mail.h"
#include "overworld.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "random.h"
#include "string_util.h"
#include "constants/game_stat.h"
#include "constants/items.h"
#include "constants/lab_research.h"
#include "constants/pokemon.h"
#include "constants/species.h"

// Fantasy Retold: Birch's Aide research trade (design.md §2). The player hands over two party Pokémon and gets
// back one Lv5 first-stage, non-Hoenn, non-legendary species whose line shares one of their types and whose final
// evolution has a higher BST than both of theirs. One request after the Route 103 rival, then one more after every
// badge or Elite Four clear (missed ones don't stack). From the 4th request on, one of the two must have a rolled type.
// Scripts pass the two party slots in VAR_0x8005 / VAR_0x8006.

// Type var: the requested type in the low byte (kept after the request as the last type), a bit while the open
// request has rolled its type, and a bit (kept for good) once the Aide has called about the PokéNav turn-in.
#define LAB_RESEARCH_TYPE_ROLLED (1 << 15)
#define LAB_RESEARCH_CALLED      (1 << 14)
#define LAB_RESEARCH_TYPE_MASK   0xFF

// Outdoor steps after getting the PokéNav before the Aide's one call, so it doesn't ring the moment it's received.
#define LAB_RESEARCH_CALL_STEPS 10

static EWRAM_DATA u8 sLabResearchCallSteps = 0;

#define LAB_RESEARCH_REWARD_LEVEL 5
#define EVO_LINE_DEPTH 3

// Two Pokémon, two types each, duplicates kept.
#define TYPE_POOL_SIZE 4

static u32 GetLabResearchMilestones(void)
{
    u32 count = 0;

    for (u32 i = 0; i < NUM_BADGES; i++)
    {
        if (FlagGet(FLAG_BADGE01_GET + i))
            count++;
    }
    count += min(GetGameStat(GAME_STAT_ENTERED_HOF), 0xF000);
    return count;
}

// The first request is open as soon as the Aide is reachable; later ones need a new milestone since the last.
static bool32 IsLabResearchOpen(void)
{
    if (VarGet(VAR_LAB_RESEARCH_COUNT) == 0)
        return TRUE;
    return GetLabResearchMilestones() > VarGet(VAR_LAB_RESEARCH_KEY);
}

static bool32 IsLabResearchTyped(void)
{
    return VarGet(VAR_LAB_RESEARCH_COUNT) >= LAB_RESEARCH_UNTYPED_REQUESTS;
}

static bool32 IsRequestableType(enum Type type)
{
    return type >= TYPE_NORMAL && type <= TYPE_FAIRY && type != TYPE_MYSTERY;
}

// Rolled once per request, never the same type twice in a row.
static enum Type GetRequestedType(void)
{
    u32 var = VarGet(VAR_LAB_RESEARCH_TYPE);
    enum Type last = var & LAB_RESEARCH_TYPE_MASK;
    enum Type types[NUMBER_OF_MON_TYPES];
    u32 count = 0;

    if (var & LAB_RESEARCH_TYPE_ROLLED)
        return last;
    for (enum Type type = TYPE_NORMAL; type <= TYPE_FAIRY; type++)
    {
        if (IsRequestableType(type) && type != last)
            types[count++] = type;
    }
    enum Type type = types[Random() % count];
    VarSet(VAR_LAB_RESEARCH_TYPE, (var & LAB_RESEARCH_CALLED) | LAB_RESEARCH_TYPE_ROLLED | type);
    return type;
}

static void CompleteLabResearch(void)
{
    VarSet(VAR_LAB_RESEARCH_KEY, GetLabResearchMilestones());
    VarSet(VAR_LAB_RESEARCH_COUNT, VarGet(VAR_LAB_RESEARCH_COUNT) + 1);
    VarSet(VAR_LAB_RESEARCH_TYPE, VarGet(VAR_LAB_RESEARCH_TYPE) & (LAB_RESEARCH_CALLED | LAB_RESEARCH_TYPE_MASK));
}

static u32 GetSpeciesBST(enum Species species)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];

    return info->baseHP + info->baseAttack + info->baseDefense
         + info->baseSpeed + info->baseSpAttack + info->baseSpDefense;
}

// Highest BST among the final evolutions reachable from the species (its own if it doesn't evolve). Shared with the gym reward.
u32 GetLineBST(enum Species species, u32 depth)
{
    const struct Evolution *evolutions = GetSpeciesEvolutions(species);
    u32 best = 0;

    if (depth != 0 && evolutions != NULL)
    {
        for (u32 i = 0; evolutions[i].method != EVOLUTIONS_END; i++)
        {
            u32 bst = GetLineBST(SanitizeSpeciesId(evolutions[i].targetSpecies), depth - 1);
            best = max(best, bst);
        }
    }
    return best != 0 ? best : GetSpeciesBST(species);
}

static bool32 DoesLineHaveType(enum Species species, enum Type type, u32 depth)
{
    if (IsSpeciesOfType(species, type))
        return TRUE;
    if (depth == 0)
        return FALSE;

    const struct Evolution *evolutions = GetSpeciesEvolutions(species);
    if (evolutions == NULL)
        return FALSE;
    for (u32 i = 0; evolutions[i].method != EVOLUTIONS_END; i++)
    {
        if (DoesLineHaveType(SanitizeSpeciesId(evolutions[i].targetSpecies), type, depth - 1))
            return TRUE;
    }
    return FALSE;
}

static bool32 IsEvolutionTarget(enum Species species, const u8 *isEvolutionTarget)
{
    return (isEvolutionTarget[species / 8] & (1 << (species % 8))) != 0;
}

static void BuildEvolutionTargets(u8 *isEvolutionTarget)
{
    for (enum Species species = 1; species < NUM_SPECIES; species++)
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
}

static bool32 IsRegionalForm(enum Species species)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];

    return info->isAlolanForm || info->isGalarianForm || info->isHisuianForm || info->isPaldeanForm;
}

static bool32 IsRewardCandidate(enum Species species, enum Type type, u32 minLineBST, const u8 *isEvolutionTarget)
{
    const struct SpeciesInfo *info = &gSpeciesInfo[species];

    if (!IsSpeciesEnabled(species))
        return FALSE;
    // First stage, in its default form or a regional form. The default form must be a first stage too,
    // so battle forms of evolved regional species (Galarian Zen Darmanitan) stay out.
    const u16 *forms = GetSpeciesFormTable(species);
    if (forms != NULL && forms[0] != species && !IsRegionalForm(species))
        return FALSE;
    if (IsEvolutionTarget(species, isEvolutionTarget) || (forms != NULL && IsEvolutionTarget(forms[0], isEvolutionTarget)))
        return FALSE;
    if (info->isRestrictedLegendary || info->isSubLegendary || info->isMythical
     || info->isUltraBeast || info->isParadox || info->isTotem)
        return FALSE;
    if (!DoesLineHaveType(species, type, EVO_LINE_DEPTH))
        return FALSE;
    if (GetLineBST(species, EVO_LINE_DEPTH) <= minLineBST)
        return FALSE;
    if (IsInHoennFamily(species, EVO_LINE_DEPTH))
        return FALSE;
    return TRUE;
}

static u32 CountRewardCandidates(enum Type type, u32 minLineBST, const u8 *isEvolutionTarget)
{
    u32 count = 0;

    for (enum Species species = 1; species < NUM_SPECIES; species++)
    {
        if (IsRewardCandidate(species, type, minLineBST, isEvolutionTarget))
            count++;
    }
    return count;
}

static enum Species PickRewardCandidate(enum Type type, u32 minLineBST, const u8 *isEvolutionTarget)
{
    u32 count = CountRewardCandidates(type, minLineBST, isEvolutionTarget);

    if (count == 0)
        return SPECIES_NONE;
    count = Random() % count;
    for (enum Species species = 1; species < NUM_SPECIES; species++)
    {
        if (IsRewardCandidate(species, type, minLineBST, isEvolutionTarget) && count-- == 0)
            return species;
    }
    return SPECIES_NONE;
}

static struct Pokemon *GetChosenMon(u32 which)
{
    u32 slot = which == 0 ? gSpecialVar_0x8005 : gSpecialVar_0x8006;

    if (slot >= PARTY_SIZE)
        slot = 0;
    return &gParties[B_TRAINER_PLAYER][slot];
}

// Rolls a type from the two Pokémon's current types (a type both carry is twice as likely), then a species from it.
// A type with no candidate is dropped and the roll repeats. SPECIES_NONE if no type has one.
static enum Species PickLabResearchReward(void)
{
    u8 isEvolutionTarget[(NUM_SPECIES + 7) / 8] = {0};
    enum Type pool[TYPE_POOL_SIZE];
    u32 poolSize = 0;
    u32 minLineBST = 0;

    for (u32 i = 0; i < 2; i++)
    {
        enum Species species = GetMonData(GetChosenMon(i), MON_DATA_SPECIES);
        enum Type type1 = GetSpeciesType(species, 0);
        enum Type type2 = GetSpeciesType(species, 1);

        pool[poolSize++] = type1;
        if (type2 != type1)
            pool[poolSize++] = type2;
        u32 lineBST = GetLineBST(species, EVO_LINE_DEPTH);
        minLineBST = max(minLineBST, lineBST);
    }

    BuildEvolutionTargets(isEvolutionTarget);
    while (poolSize != 0)
    {
        enum Type type = pool[Random() % poolSize];
        enum Species species = PickRewardCandidate(type, minLineBST, isEvolutionTarget);
        if (species != SPECIES_NONE)
            return species;

        u32 kept = 0;
        for (u32 i = 0; i < poolSize; i++)
        {
            if (pool[i] != type)
                pool[kept++] = pool[i];
        }
        poolSize = kept;
    }
    return SPECIES_NONE;
}

// Returns a LAB_RESEARCH_* state; for a typed request the type's name is in STR_VAR_1.
u16 LabResearch_GetState(void)
{
    if (!IsLabResearchOpen())
        return LAB_RESEARCH_CLOSED;
    if (!IsLabResearchTyped())
        return LAB_RESEARCH_OPEN;
    StringCopy(gStringVar1, gTypesInfo[GetRequestedType()].name);
    return LAB_RESEARCH_OPEN_TYPED;
}

// Checked before anything is taken. Returns a LAB_RESEARCH_PAIR_* result.
u16 LabResearch_CheckPair(void)
{
    if (IsLabResearchTyped())
    {
        enum Type type = GetRequestedType();
        bool32 found = FALSE;

        for (u32 i = 0; i < 2; i++)
        {
            if (IsSpeciesOfType(GetMonData(GetChosenMon(i), MON_DATA_SPECIES), type))
                found = TRUE;
        }
        if (!found)
            return LAB_RESEARCH_PAIR_NEEDS_TYPE;
    }
    // Each type is tried in turn, so a roll can only fail if every type fails.
    if (PickLabResearchReward() == SPECIES_NONE)
        return LAB_RESEARCH_PAIR_NO_REWARD;
    return LAB_RESEARCH_PAIR_OK;
}

// Held items go back to the bag (with the Pokémon if the bag is full; Mail is never kept on a Pokémon that leaves).
static void ReturnHeldItem(struct Pokemon *mon)
{
    enum Item item = GetMonData(mon, MON_DATA_HELD_ITEM);

    if (item == ITEM_NONE)
        return;
    if (ItemIsMail(item))
    {
        AddBagItem(item, 1);
        TakeMailFromMon(mon);
    }
    else if (AddBagItem(item, 1))
    {
        item = ITEM_NONE;
        SetMonData(mon, MON_DATA_HELD_ITEM, &item);
    }
}

// Takes the two Pokémon and gives the reward. Returns the reward's party slot (also in VAR_0x8004 for the
// nickname prompt) with its species name in STR_VAR_2, or PARTY_SIZE if there is no reward (nothing is taken).
u16 LabResearch_Trade(void)
{
    enum Species species = PickLabResearchReward();
    struct Pokemon reward;

    if (species == SPECIES_NONE)
        return PARTY_SIZE;

    for (u32 i = 0; i < 2; i++)
    {
        ReturnHeldItem(GetChosenMon(i));
        ZeroMonData(GetChosenMon(i));
    }
    CompactPartySlots();
    CalculatePlayerPartyCount();

    // Random nature and (non-hidden) ability; each IV is rolled twice and keeps the higher roll.
    CreateRandomMon(&reward, species, LAB_RESEARCH_REWARD_LEVEL);
    for (u32 stat = 0; stat < NUM_STATS; stat++)
    {
        u8 iv = Random() % (MAX_PER_STAT_IVS + 1);
        iv = max(iv, GetMonData(&reward, MON_DATA_HP_IV + stat));
        SetMonData(&reward, MON_DATA_HP_IV + stat, &iv);
    }
    CalculateMonStats(&reward);
    GiveScriptedMonToPlayer(&reward, PARTY_SIZE);
    UpdateFollowingPokemon();

    CompleteLabResearch();
    gSpecialVar_0x8004 = gPartiesCount[B_TRAINER_PLAYER] - 1;
    StringCopy(gStringVar2, GetSpeciesName(species));
    return gSpecialVar_0x8004;
}

// Rare Candy instead of a trade when nothing can beat the pair; uses up the request like a trade.
void LabResearch_Skip(void)
{
    CompleteLabResearch();
}

// Once the player has the PokéNav, the Aide calls once to tell them about the Select menu turn-in.
bool32 ShouldDoLabResearchCall(void)
{
    if (!FlagGet(FLAG_SYS_POKENAV_GET) || VarGet(VAR_BIRCH_LAB_STATE) < 5)
        return FALSE;
    if (VarGet(VAR_LAB_RESEARCH_TYPE) & LAB_RESEARCH_CALLED)
        return FALSE;
    switch (gMapHeader.mapType)
    {
    case MAP_TYPE_TOWN:
    case MAP_TYPE_CITY:
    case MAP_TYPE_ROUTE:
    case MAP_TYPE_OCEAN_ROUTE:
        if (++sLabResearchCallSteps < LAB_RESEARCH_CALL_STEPS)
            return FALSE;
        sLabResearchCallSteps = 0;
        return TRUE;
    default:
        return FALSE;
    }
}

void LabResearch_SetCalled(void)
{
    VarSet(VAR_LAB_RESEARCH_TYPE, VarGet(VAR_LAB_RESEARCH_TYPE) | LAB_RESEARCH_CALLED);
}
