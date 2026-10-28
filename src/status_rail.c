#include "global.h"
#include "battle.h"
#include "battle_controllers.h"
#include "battle_util.h"
#include "random.h"
#include "status_rail.h"
#include "constants/battle.h"
#include "constants/battle_string_ids.h"

// Fantasy Retold: multi-stage status "rails" (design.md §6). This module owns all rail state, the two-way
// bridge to the legacy status1 bits / confusionTimer, and the numbers behind each line's effects.

static struct StatusRailEntry *GetRail(enum BattlerId battler, enum StatusRail rail)
{
    return &GetBattlerPartyState(battler)->rails[rail];
}

// Highest level a rail can reach.
u8 GetStatusRailMaxLevel(enum StatusRail rail)
{
    switch (rail)
    {
    case RAIL_POISON:    return 2; // Poison -> Toxic
    case RAIL_BURN:      return 3; // stacks
    case RAIL_CHILL:     return 2; // Chilled -> Frozen | Frostbitten
    case RAIL_ELECTRIC:  return 3; // Dazed -> Shocked -> Paralyzed
    case RAIL_SLEEP:     return 3; // Drowsy -> Asleep -> Nightmare
    case RAIL_CONFUSION: return 3; // Confused -> Stupefied -> Frightened
    case RAIL_SOAK:      return 1;
    default:             return 1;
    }
}

u8 GetStatusRailLevel(enum BattlerId battler, enum StatusRail rail)
{
    return GetRail(battler, rail)->level;
}

u8 GetStatusRailCounter(enum BattlerId battler, enum StatusRail rail)
{
    return GetRail(battler, rail)->counter;
}

u8 GetStatusRailBranch(enum BattlerId battler, enum StatusRail rail)
{
    return GetRail(battler, rail)->branch;
}

void SetStatusRailCounter(enum BattlerId battler, enum StatusRail rail, u8 counter)
{
    GetRail(battler, rail)->counter = counter;
}

static void ResetRail(struct StatusRailEntry *entry)
{
    entry->level = 0;
    entry->counter = 0;
    entry->branch = 0;
}

u8 ApplyStatusRail(enum BattlerId battler, enum StatusRail rail, u8 minLevel)
{
    struct StatusRailEntry *entry = GetRail(battler, rail);
    u8 maxLevel = GetStatusRailMaxLevel(rail);
    u8 oldLevel, newLevel;

    ReconcileStatusRailsFromBridge(battler);
    oldLevel = entry->level;

    if (rail == RAIL_CHILL) // no direct way into Frozen / Frostbitten
        minLevel = 1;

    newLevel = oldLevel + 1;  // reapplication advances one step...
    if (newLevel < minLevel)  // ...but a status-move application starts no lower than minLevel.
        newLevel = minLevel;
    if (newLevel > maxLevel)
        newLevel = maxLevel;

    switch (rail)
    {
    case RAIL_POISON:
        entry->counter = 0; // a fresh application / advance restarts the Toxic escalation
        break;
    case RAIL_CHILL:
        entry->counter = CHILL_DURATION; // every Ice application renews it
        if (newLevel == 2 && oldLevel < 2)
            entry->branch = RandomPercentage(RNG_CHILL_BRANCH, 50) ? CHILL_BRANCH_FROSTBITTEN : CHILL_BRANCH_FROZEN;
        break;
    case RAIL_SLEEP:
        // A fresh Drowsy, or falling asleep, starts the count; Asleep -> Nightmare inherits it (no refresh).
        if (oldLevel == 0 || (oldLevel < 2 && newLevel >= 2))
            entry->counter = SLEEP_DURATION;
        break;
    case RAIL_CONFUSION:
        if (entry->counter == 0) // advancing the line keeps the remaining turns
            entry->counter = CONFUSION_DURATION;
        break;
    case RAIL_SOAK:
        entry->counter = SOAK_DURATION;
        break;
    default:
        break;
    }

    entry->level = newLevel;
    SyncStatusRailBridge(battler);
    return newLevel;
}

u8 LowerStatusRail(enum BattlerId battler, enum StatusRail rail)
{
    struct StatusRailEntry *entry = GetRail(battler, rail);

    ReconcileStatusRailsFromBridge(battler);
    if (entry->level > 1)
        entry->level--;
    else
        ResetRail(entry);
    SyncStatusRailBridge(battler);
    return entry->level;
}

void ClearStatusRail(enum BattlerId battler, enum StatusRail rail)
{
    ReconcileStatusRailsFromBridge(battler);
    ResetRail(GetRail(battler, rail));
    SyncStatusRailBridge(battler);
}

void ClearAllStatusRails(enum BattlerId battler)
{
    enum StatusRail rail;

    for (rail = 0; rail < NUM_STATUS_RAILS; rail++)
        ResetRail(GetRail(battler, rail));
    SyncStatusRailBridge(battler);
}

void ClearVolatileStatusRails(enum BattlerId battler)
{
    enum StatusRail rail;

    ReconcileStatusRailsFromBridge(battler);
    for (rail = 0; rail < NUM_STATUS_RAILS; rail++)
    {
        if (GetRail(battler, rail)->level == 1)
            ResetRail(GetRail(battler, rail));
    }
    SyncStatusRailBridge(battler);
}

#define STATUS1_RAIL_MIRROR (STATUS1_PSN_ANY | STATUS1_TOXIC_COUNTER | STATUS1_BURN | STATUS1_ICY_ANY | STATUS1_PARALYSIS | STATUS1_SLEEP)

void SyncStatusRailBridge(enum BattlerId battler)
{
    struct PartyState *ps = GetBattlerPartyState(battler);
    u32 status1 = gBattleMons[battler].status1 & ~STATUS1_RAIL_MIRROR;

    // Poison: Lv1 -> STATUS1_POISON; Lv2 (Toxic) -> STATUS1_TOXIC_POISON + counter mirror.
    if (ps->rails[RAIL_POISON].level == 1)
        status1 |= STATUS1_POISON;
    else if (ps->rails[RAIL_POISON].level >= 2)
        status1 |= STATUS1_TOXIC_POISON | STATUS1_TOXIC_TURN(min(ps->rails[RAIL_POISON].counter, 15));

    if (ps->rails[RAIL_BURN].level != 0)
        status1 |= STATUS1_BURN;

    // Chill: only the Lv2 forms have a vanilla bit.
    if (ps->rails[RAIL_CHILL].level >= 2)
        status1 |= (ps->rails[RAIL_CHILL].branch == CHILL_BRANCH_FROSTBITTEN) ? STATUS1_FROSTBITE : STATUS1_FREEZE;

    // Electric: only Paralyzed (Lv3) has a vanilla bit.
    if (ps->rails[RAIL_ELECTRIC].level >= 3)
        status1 |= STATUS1_PARALYSIS;

    // Sleep: Asleep / Nightmare -> the vanilla sleep bits, holding the turns left (at least 1).
    if (ps->rails[RAIL_SLEEP].level >= 2)
        status1 |= STATUS1_SLEEP_TURN(min(max(ps->rails[RAIL_SLEEP].counter, 1), 7));

    gBattleMons[battler].status1 = status1;

    // Confusion: the whole line reads as "confused" to vanilla code; the timer holds the turns left.
    if (ps->rails[RAIL_CONFUSION].level != 0)
        gBattleMons[battler].volatiles.confusionTimer = max(ps->rails[RAIL_CONFUSION].counter, 1);
    else
        gBattleMons[battler].volatiles.confusionTimer = 0;
}

void ReconcileStatusRailsFromBridge(enum BattlerId battler)
{
    struct PartyState *ps = GetBattlerPartyState(battler);
    u32 status1 = gBattleMons[battler].status1;
    u32 confusionTimer = gBattleMons[battler].volatiles.confusionTimer;
    struct StatusRailEntry *rail;

    // Poison
    rail = &ps->rails[RAIL_POISON];
    if (!(status1 & STATUS1_PSN_ANY))
    {
        ResetRail(rail); // cured (Refresh / Natural Cure / full heal / Rest) or never applied
    }
    else if (status1 & STATUS1_TOXIC_POISON)
    {
        if (rail->level < 2) // Toxic Orb, Toxic Spikes, ...
        {
            rail->level = 2;
            rail->counter = (status1 & STATUS1_TOXIC_COUNTER) >> 8;
        }
    }
    else if (rail->level < 1)
    {
        rail->level = 1;
    }

    // Burn
    rail = &ps->rails[RAIL_BURN];
    if (!(status1 & STATUS1_BURN))
        ResetRail(rail);
    else if (rail->level == 0) // Flame Orb, Synchronize, ...
        rail->level = 1;

    // Chill (only Lv2 is mirrored, so a missing bit only clears Lv2)
    rail = &ps->rails[RAIL_CHILL];
    if (!(status1 & STATUS1_ICY_ANY))
    {
        if (rail->level >= 2) // thawed
            ResetRail(rail);
    }
    else if (rail->level < 2)
    {
        rail->level = 2;
        rail->counter = CHILL_DURATION;
        rail->branch = (status1 & STATUS1_FROSTBITE) ? CHILL_BRANCH_FROSTBITTEN : CHILL_BRANCH_FROZEN;
    }

    // Electric (only Lv3 is mirrored)
    rail = &ps->rails[RAIL_ELECTRIC];
    if (!(status1 & STATUS1_PARALYSIS))
    {
        if (rail->level >= 3) // paralysis cured
            ResetRail(rail);
    }
    else if (rail->level < 3)
    {
        rail->level = 3;
    }

    // Sleep (Lv2+ is mirrored)
    rail = &ps->rails[RAIL_SLEEP];
    if (!(status1 & STATUS1_SLEEP))
    {
        if (rail->level >= 2) // woken (Uproar, cure, ...)
            ResetRail(rail);
    }
    else if (rail->level < 2) // Rest, Yawn, Effect Spore, ...
    {
        rail->level = 2;
        rail->counter = min(status1 & STATUS1_SLEEP, SLEEP_DURATION);
    }

    // Confusion
    rail = &ps->rails[RAIL_CONFUSION];
    if (confusionTimer == 0)
    {
        ResetRail(rail); // Persim Berry, Own Tempo, ... (or never confused)
    }
    else if (rail->level == 0) // Outrage fatigue, confusing berries, ...
    {
        rail->level = 1;
        rail->counter = min(confusionTimer, CONFUSION_DURATION);
    }
}

void EmitStatusRailMirror(enum BattlerId battler)
{
    BtlController_EmitSetMonData(battler, B_COMM_TO_CONTROLLER, REQUEST_STATUS_BATTLE, 0, sizeof(gBattleMons[battler].status1), &gBattleMons[battler].status1);
    MarkBattlerForControllerExec(battler);
}

bool32 PurgeRandomLevel1StatusRail(enum BattlerId battler)
{
    struct PartyState *ps = GetBattlerPartyState(battler);
    enum StatusRail rail, candidates[NUM_STATUS_RAILS];
    u32 count = 0;

    ReconcileStatusRailsFromBridge(battler);
    for (rail = 0; rail < NUM_STATUS_RAILS; rail++)
    {
        if (ps->rails[rail].level == 1)
            candidates[count++] = rail;
    }
    if (count == 0)
        return FALSE;

    rail = candidates[RandomUniform(RNG_STATUS_RAIL_CLEANSE, 0, count - 1)];
    ResetRail(&ps->rails[rail]);
    SyncStatusRailBridge(battler);
    return TRUE;
}

// Soaked, Chilled and Drowsy each slow the battler; they stack multiplicatively. (Paralysis is applied
// separately, in GetBattlerTotalSpeedStat.)
u32 GetStatusRailSpeedPercent(enum BattlerId battler)
{
    u32 percent = 100;

    if (GetStatusRailLevel(battler, RAIL_SOAK) != 0)
        percent = percent * STATUS_RAIL_SLOW_PERCENT / 100;
    if (GetStatusRailLevel(battler, RAIL_CHILL) == 1)
        percent = percent * STATUS_RAIL_SLOW_PERCENT / 100;
    if (GetStatusRailLevel(battler, RAIL_SLEEP) == 1)
        percent = percent * STATUS_RAIL_SLOW_PERCENT / 100;
    return percent;
}

// Chilled and Drowsy each add a flat amount to every move / Dodge (not Recuperate).
u32 GetStatusRailStaminaSurcharge(enum BattlerId battler)
{
    u32 surcharge = 0;

    if (GetStatusRailLevel(battler, RAIL_CHILL) == 1)
        surcharge += STATUS_RAIL_STAMINA_SURCHARGE;
    if (GetStatusRailLevel(battler, RAIL_SLEEP) == 1)
        surcharge += STATUS_RAIL_STAMINA_SURCHARGE;
    return surcharge;
}

// The attacker's accuracy on the Electric line: Dazed, then Shocked / Paralyzed.
u32 GetStatusRailAccuracyPercent(enum BattlerId battler)
{
    switch (GetStatusRailLevel(battler, RAIL_ELECTRIC))
    {
    case 0:  return 100;
    case 1:  return DAZED_ACCURACY_PERCENT;
    default: return SHOCKED_ACCURACY_PERCENT;
    }
}

static uq4_12_t PercentToUQ412(u32 percent)
{
    return (percent * UQ_4_12(1.0)) / 100;
}

// Incoming damage on the defender: Soaked (Fire x0.5, Electric / Ice x1.5) and Shocked+ (Electric x1.25).
uq4_12_t GetStatusRailIncomingDamageModifier(enum BattlerId battlerDef, enum Type moveType)
{
    u32 percent = 100;

    if (GetStatusRailLevel(battlerDef, RAIL_SOAK) != 0)
    {
        if (moveType == TYPE_FIRE)
            percent = percent * 50 / 100;
        else if (moveType == TYPE_ELECTRIC || moveType == TYPE_ICE)
            percent = percent * 150 / 100;
    }
    if (moveType == TYPE_ELECTRIC && GetStatusRailLevel(battlerDef, RAIL_ELECTRIC) >= 2)
        percent = percent * SHOCKED_ELECTRIC_DAMAGE_PERCENT / 100;

    return PercentToUQ412(percent);
}

// Burn weakens physical damage by a fixed step per stack (replaces vanilla's flat halving).
uq4_12_t GetBurnRailPhysicalModifier(enum BattlerId battler)
{
    u32 stacks = GetStatusRailLevel(battler, RAIL_BURN);

    if (stacks == 0) // burned outside the rail; treat as one stack
        stacks = 1;
    return PercentToUQ412(100 - BURN_PHYSICAL_PENALTY_PER_STACK * stacks);
}

// Burn can't take hold on a Soaked or chilled battler (and the reverse is checked by the callers).
bool32 IsBurnBlockedByStatusRails(enum BattlerId battler)
{
    return GetStatusRailLevel(battler, RAIL_SOAK) != 0 || GetStatusRailLevel(battler, RAIL_CHILL) != 0;
}

u32 TickStatusRailTimers(enum BattlerId battler, bool32 earlyBird)
{
    static const enum StatusRail sTimedRails[] = { RAIL_SOAK, RAIL_CHILL, RAIL_SLEEP, RAIL_CONFUSION };
    u32 i, endedMask = 0;

    ReconcileStatusRailsFromBridge(battler);
    for (i = 0; i < ARRAY_COUNT(sTimedRails); i++)
    {
        struct StatusRailEntry *entry = GetRail(battler, sTimedRails[i]);
        u32 step = (sTimedRails[i] == RAIL_SLEEP && earlyBird) ? 2 : 1;

        if (entry->level == 0)
            continue;
        if (entry->counter > step)
        {
            entry->counter -= step;
        }
        else
        {
            ResetRail(entry);
            endedMask |= 1u << sTimedRails[i];
        }
    }
    SyncStatusRailBridge(battler);
    return endedMask;
}

u32 GetStatusRailEndedStringId(enum StatusRail rail)
{
    switch (rail)
    {
    case RAIL_SOAK:      return STRINGID_FRDRIEDOFF;
    case RAIL_CHILL:     return STRINGID_FRWARMEDUP;
    case RAIL_SLEEP:     return STRINGID_FRWOKEUP;
    case RAIL_CONFUSION: return STRINGID_FRCALMEDDOWN;
    default:             return 0;
    }
}

u32 ApplyStatusRailTypeInteraction(enum BattlerId battler, enum Type moveType)
{
    ReconcileStatusRailsFromBridge(battler);

    switch (moveType)
    {
    case TYPE_FIRE:
        if (GetStatusRailLevel(battler, RAIL_CHILL) != 0)
        {
            ClearStatusRail(battler, RAIL_CHILL);
            return STRINGID_FRWARMEDUP;
        }
        break;
    case TYPE_WATER:
    case TYPE_ICE:
        if (GetStatusRailLevel(battler, RAIL_BURN) != 0)
        {
            LowerStatusRail(battler, RAIL_BURN);
            return STRINGID_FRBURNEASED;
        }
        break;
    case TYPE_GROUND:
        if (GetStatusRailLevel(battler, RAIL_ELECTRIC) != 0)
        {
            LowerStatusRail(battler, RAIL_ELECTRIC);
            return STRINGID_FRSTATICFADED;
        }
        break;
    default:
        break;
    }
    return 0;
}

bool32 TrySoakFromHit(enum BattlerId battler, enum Type moveType)
{
    if (moveType != TYPE_WATER
     || IS_BATTLER_OF_TYPE(battler, TYPE_WATER)
     || GetStatusRailLevel(battler, RAIL_SOAK) != 0
     || GetStatusRailLevel(battler, RAIL_BURN) != 0 // Burn and Soaked are mutually exclusive
     || !RandomPercentage(RNG_STATUS_RAIL_SOAK, SOAK_CHANCE))
        return FALSE;

    ApplyStatusRail(battler, RAIL_SOAK, 1);
    return TRUE;
}

bool32 TrySleepRailValve(enum BattlerId battler)
{
    struct StatusRailEntry *entry = GetRail(battler, RAIL_SLEEP);

    ReconcileStatusRailsFromBridge(battler);
    if (entry->level == 0)
        return FALSE;

    if (entry->counter > 1)
    {
        entry->counter--;
        SyncStatusRailBridge(battler);
        return FALSE;
    }

    ResetRail(entry);
    SyncStatusRailBridge(battler);
    return TRUE;
}
