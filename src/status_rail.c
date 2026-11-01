#include "global.h"
#include "battle.h"
#include "battle_controllers.h"
#include "battle_dynamax.h"
#include "battle_util.h"
#include "random.h"
#include "status_rail.h"
#include "constants/battle.h"
#include "constants/battle_script_commands.h"
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
    entry->keepOnSwitch = FALSE;
}

// Timers for a rail that just reached newLevel from oldLevel.
static void StartStatusRailLevel(struct StatusRailEntry *entry, enum StatusRail rail, u8 oldLevel, u8 newLevel)
{
    switch (rail)
    {
    case RAIL_POISON:
        entry->counter = 0; // a fresh application / advance restarts the Toxic escalation
        break;
    case RAIL_CHILL:
        entry->counter = CHILL_DURATION;
        break;
    case RAIL_SLEEP:
        // A fresh Drowsy, or falling asleep, starts the count; Asleep -> Nightmare inherits it (no refresh).
        if (oldLevel == 0 || (oldLevel < 2 && newLevel == 2))
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
    entry->keepOnSwitch = FALSE;
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
    if (rail == RAIL_ELECTRIC && oldLevel == 1 && GetRail(battler, RAIL_SOAK)->level != 0) // Dazed + Soaked skips to Paralyzed
        newLevel = maxLevel;
    if (newLevel > maxLevel)
        newLevel = maxLevel;

    if (rail == RAIL_CHILL && newLevel == 2 && oldLevel < 2)
        entry->branch = RandomPercentage(RNG_CHILL_BRANCH, 50) ? CHILL_BRANCH_FROSTBITTEN : CHILL_BRANCH_FROZEN;
    StartStatusRailLevel(entry, rail, oldLevel, newLevel);
    SyncStatusRailBridge(battler);
    return newLevel;
}

u8 RaiseStatusRailTo(enum BattlerId battler, enum StatusRail rail, u8 level, u8 branch)
{
    struct StatusRailEntry *entry = GetRail(battler, rail);

    ReconcileStatusRailsFromBridge(battler);
    if (level > GetStatusRailMaxLevel(rail))
        level = GetStatusRailMaxLevel(rail);
    if (entry->level < level)
    {
        if (rail == RAIL_CHILL && level == 2)
            entry->branch = branch;
        StartStatusRailLevel(entry, rail, entry->level, level);
    }
    SyncStatusRailBridge(battler);
    return entry->level;
}

u8 ApplyStatusRailOrSynchronize(enum BattlerId battler, enum StatusRail rail, u8 minLevel)
{
    u8 syncLevel = gBattleStruct->synchronizeRailLevel;

    if (syncLevel == 0
     || gBattleStruct->synchronizeState != SYNCH_STATE_SET_STATUS
     || battler != gBattleScripting.savedBattler)
        return ApplyStatusRail(battler, rail, minLevel);

    gBattleStruct->synchronizeRailLevel = 0;
    if (GetStatusRailLevel(battler, rail) >= syncLevel) // already that deep: advance one step instead
        return ApplyStatusRail(battler, rail, minLevel);
    return RaiseStatusRailTo(battler, rail, syncLevel, gBattleStruct->synchronizeRailBranch);
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

// A cure clears a rail below Lv3; a Lv3 drops to Lv1. keepOnSwitch: that Lv1 survives the next switch-out
// (a cure that happens on switch-out, i.e. Natural Cure, mustn't save the turn the Lv1 would cost).
static void CureRail(struct StatusRailEntry *entry, bool32 keepOnSwitch)
{
    if (entry->level >= 3)
    {
        entry->level = 1;
        entry->keepOnSwitch = keepOnSwitch;
    }
    else
    {
        ResetRail(entry);
    }
}

static bool32 CureStatusRailEntries(struct StatusRailEntry *rails, u32 railMask, bool32 keepOnSwitch)
{
    enum StatusRail rail;
    bool32 cured = FALSE;

    for (rail = 0; rail < NUM_STATUS_RAILS; rail++)
    {
        if (!(railMask & STATUS_RAIL_BIT(rail)))
            continue;
        if (rails[rail].level != 0)
            cured = TRUE;
        CureRail(&rails[rail], keepOnSwitch);
    }
    return cured;
}

bool32 CurePartyStatusRails(struct StatusRailEntry *rails, bool32 keepOnSwitch)
{
    return CureStatusRailEntries(rails, STATUS_RAILS_ALL, keepOnSwitch);
}

void CureAllStatusRails(enum BattlerId battler, bool32 keepOnSwitch)
{
    // No reconcile: callers may already have zeroed status1, which would wipe the Lv3s this keeps.
    CurePartyStatusRails(GetBattlerPartyState(battler)->rails, keepOnSwitch);
    SyncStatusRailBridge(battler);
}

bool32 CureStatusRails(enum BattlerId battler, u32 railMask)
{
    bool32 cured;

    ReconcileStatusRailsFromBridge(battler);
    cured = CureStatusRailEntries(GetBattlerPartyState(battler)->rails, railMask, FALSE);
    SyncStatusRailBridge(battler);
    return cured;
}

bool32 HasAnyStatusRail(enum BattlerId battler, u32 railMask)
{
    enum StatusRail rail;

    for (rail = 0; rail < NUM_STATUS_RAILS; rail++)
    {
        if ((railMask & STATUS_RAIL_BIT(rail)) && GetStatusRailLevel(battler, rail) != 0)
            return TRUE;
    }
    return FALSE;
}

// The cure message (B_MSG_CURED_*) for the first active line in railMask, in vanilla's order.
u32 GetStatusRailCureMessage(enum BattlerId battler, u32 railMask)
{
    if ((railMask & STATUS_RAIL_BIT(RAIL_ELECTRIC)) && GetStatusRailLevel(battler, RAIL_ELECTRIC) != 0)
        return B_MSG_CURED_PARALYSIS;
    if ((railMask & STATUS_RAIL_BIT(RAIL_POISON)) && GetStatusRailLevel(battler, RAIL_POISON) != 0)
        return B_MSG_CURED_POISON;
    if ((railMask & STATUS_RAIL_BIT(RAIL_BURN)) && GetStatusRailLevel(battler, RAIL_BURN) != 0)
        return B_MSG_CURED_BURN;
    if ((railMask & STATUS_RAIL_BIT(RAIL_SLEEP)) && GetStatusRailLevel(battler, RAIL_SLEEP) != 0)
        return B_MSG_CURED_SLEEP;
    if ((railMask & STATUS_RAIL_BIT(RAIL_CHILL)) && GetStatusRailLevel(battler, RAIL_CHILL) != 0)
        return GetStatusRailBranch(battler, RAIL_CHILL) == CHILL_BRANCH_FROSTBITTEN ? B_MSG_CURED_FROSTBITE : B_MSG_CURED_FREEZE;
    if ((railMask & STATUS_RAIL_BIT(RAIL_CONFUSION)) && GetStatusRailLevel(battler, RAIL_CONFUSION) != 0)
        return B_MSG_CURED_CONFUSION;
    return B_MSG_CURED_PROBLEM; // Soaked only
}

bool32 HasStatusRailCondition(enum BattlerId battler)
{
    enum StatusRail rail;

    if (GetStatusRailLevel(battler, RAIL_SOAK) != 0)
        return TRUE;
    for (rail = 0; rail < NUM_STATUS_RAILS; rail++)
    {
        if (GetStatusRailLevel(battler, rail) >= 2)
            return TRUE;
    }
    return FALSE;
}

enum StatusRail GetStatusRailFromStatus1(u32 status1)
{
    if (status1 & STATUS1_PSN_ANY)   return RAIL_POISON;
    if (status1 & STATUS1_BURN)      return RAIL_BURN;
    if (status1 & STATUS1_ICY_ANY)   return RAIL_CHILL;
    if (status1 & STATUS1_PARALYSIS) return RAIL_ELECTRIC;
    if (status1 & STATUS1_SLEEP)     return RAIL_SLEEP;
    return NUM_STATUS_RAILS;
}

bool32 HasStatusRailConditionForStatus1(enum BattlerId battler, u32 status1Mask)
{
    if ((status1Mask & STATUS1_ANY) == STATUS1_ANY)
        return HasStatusRailCondition(battler);
    if ((status1Mask & STATUS1_PSN_ANY) && GetStatusRailLevel(battler, RAIL_POISON) >= 2)
        return TRUE;
    if ((status1Mask & STATUS1_BURN) && GetStatusRailLevel(battler, RAIL_BURN) >= 2)
        return TRUE;
    if ((status1Mask & STATUS1_ICY_ANY) && GetStatusRailLevel(battler, RAIL_CHILL) >= 2)
        return TRUE;
    if ((status1Mask & STATUS1_PARALYSIS) && GetStatusRailLevel(battler, RAIL_ELECTRIC) >= 2)
        return TRUE;
    if ((status1Mask & STATUS1_SLEEP) && GetStatusRailLevel(battler, RAIL_SLEEP) >= 2)
        return TRUE;
    return FALSE;
}

bool32 CureStatusRailWithBerry(enum BattlerId battler, enum StatusRail rail)
{
    ReconcileStatusRailsFromBridge(battler);
    if (GetRail(battler, rail)->level < 2) // berries ignore Lv1
        return FALSE;
    CureRail(GetRail(battler, rail), FALSE);
    SyncStatusRailBridge(battler);
    return TRUE;
}

void ClearVolatileStatusRails(enum BattlerId battler)
{
    enum StatusRail rail;

    ReconcileStatusRailsFromBridge(battler);
    for (rail = 0; rail < NUM_STATUS_RAILS; rail++)
    {
        struct StatusRailEntry *entry = GetRail(battler, rail);

        if (entry->level == 1 && entry->keepOnSwitch)
            entry->keepOnSwitch = FALSE; // survives this one switch-out only
        else if (entry->level == 1)
            ResetRail(entry);
    }
    SyncStatusRailBridge(battler);
}

#define STATUS1_RAIL_MIRROR (STATUS1_PSN_ANY | STATUS1_TOXIC_COUNTER | STATUS1_BURN | STATUS1_ICY_ANY | STATUS1_PARALYSIS | STATUS1_SLEEP)

// The status1 bits standing for a set of rails.
static u32 BuildStatusRailMirror(const struct StatusRailEntry *rails)
{
    u32 status1 = 0;

    // Poison: Lv1 -> STATUS1_POISON; Lv2 (Toxic) -> STATUS1_TOXIC_POISON + counter mirror.
    if (rails[RAIL_POISON].level == 1)
        status1 |= STATUS1_POISON;
    else if (rails[RAIL_POISON].level >= 2)
        status1 |= STATUS1_TOXIC_POISON | STATUS1_TOXIC_TURN(min(rails[RAIL_POISON].counter, 15));

    if (rails[RAIL_BURN].level != 0)
        status1 |= STATUS1_BURN;

    // Chill: only the Lv2 forms have a vanilla bit.
    if (rails[RAIL_CHILL].level >= 2)
        status1 |= (rails[RAIL_CHILL].branch == CHILL_BRANCH_FROSTBITTEN) ? STATUS1_FROSTBITE : STATUS1_FREEZE;

    // Electric: only Paralyzed (Lv3) has a vanilla bit.
    if (rails[RAIL_ELECTRIC].level >= 3)
        status1 |= STATUS1_PARALYSIS;

    // Sleep: Asleep / Nightmare -> the vanilla sleep bits, holding the turns left (at least 1).
    if (rails[RAIL_SLEEP].level >= 2)
        status1 |= STATUS1_SLEEP_TURN(min(max(rails[RAIL_SLEEP].counter, 1), 7));

    return status1;
}

// The saved-level bits (STATUS1_SAVED_*) for the lines that vanilla status1 can't hold at their level.
static u32 BuildStatusRailSavedBits(const struct StatusRailEntry *rails)
{
    u32 saved = 0;

    if (rails[RAIL_ELECTRIC].level >= 2)
        saved |= rails[RAIL_ELECTRIC].level << __builtin_ctz(STATUS1_SAVED_ELECTRIC);
    if (rails[RAIL_SLEEP].level >= 2)
        saved |= rails[RAIL_SLEEP].level << __builtin_ctz(STATUS1_SAVED_SLEEP);
    if (rails[RAIL_CONFUSION].level >= 2)
        saved |= rails[RAIL_CONFUSION].level << __builtin_ctz(STATUS1_SAVED_CONFUSION);
    if (rails[RAIL_BURN].level >= 2)
        saved |= rails[RAIL_BURN].level << __builtin_ctz(STATUS1_SAVED_BURN);
    return saved;
}

u32 GetStatusRailPartyStatusAfterBattle(const struct StatusRailEntry *rails)
{
    struct StatusRailEntry kept[NUM_STATUS_RAILS];
    enum StatusRail rail;

    for (rail = 0; rail < NUM_STATUS_RAILS; rail++)
    {
        kept[rail] = rails[rail];
        if (kept[rail].level == 1) // Lv1 is volatile: the battle ending drops it like a switch-out
            kept[rail].level = 0;
    }
    return BuildStatusRailMirror(kept) | BuildStatusRailSavedBits(kept);
}

u32 GetStatusRailSavedBitsToCure(u32 healMask)
{
    u32 saved = 0;

    if (healMask & STATUS1_BURN)
        saved |= STATUS1_SAVED_BURN;
    if (healMask & STATUS1_SLEEP)
        saved |= STATUS1_SAVED_SLEEP;
    if (healMask & STATUS1_PARALYSIS)
        saved |= STATUS1_SAVED_ELECTRIC;
    if ((healMask & STATUS1_ANY) == STATUS1_ANY) // a full cure also takes the confusion line
        saved |= STATUS1_SAVED_CONFUSION;
    return saved;
}

void SeedStatusRailsFromSavedStatus(enum BattlerId battler)
{
    struct PartyState *ps = GetBattlerPartyState(battler);
    u32 status1 = gBattleMons[battler].status1;
    u32 level;

    level = (status1 & STATUS1_SAVED_ELECTRIC) >> __builtin_ctz(STATUS1_SAVED_ELECTRIC);
    if (level > ps->rails[RAIL_ELECTRIC].level)
        ps->rails[RAIL_ELECTRIC].level = level;

    level = (status1 & STATUS1_SAVED_SLEEP) >> __builtin_ctz(STATUS1_SAVED_SLEEP);
    if (level > ps->rails[RAIL_SLEEP].level && ps->rails[RAIL_SLEEP].level >= 2) // the sleep turns are still mirrored
        ps->rails[RAIL_SLEEP].level = level;

    level = (status1 & STATUS1_SAVED_BURN) >> __builtin_ctz(STATUS1_SAVED_BURN);
    if (level > ps->rails[RAIL_BURN].level && ps->rails[RAIL_BURN].level >= 1) // the burn bit is still mirrored
        ps->rails[RAIL_BURN].level = level;

    level = (status1 & STATUS1_SAVED_CONFUSION) >> __builtin_ctz(STATUS1_SAVED_CONFUSION);
    if (level > ps->rails[RAIL_CONFUSION].level)
    {
        ps->rails[RAIL_CONFUSION].level = level;
        ps->rails[RAIL_CONFUSION].counter = CONFUSION_DURATION; // the timer isn't kept; it restarts
    }
}

void SyncStatusRailBridge(enum BattlerId battler)
{
    struct PartyState *ps = GetBattlerPartyState(battler);

    gBattleMons[battler].status1 = (gBattleMons[battler].status1 & ~(STATUS1_RAIL_MIRROR | STATUS1_SAVED_ANY)) | BuildStatusRailMirror(ps->rails);

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

// Soaked, the Chill line and Drowsy each slow the battler; they stack multiplicatively. (Paralysis is
// applied separately, in GetBattlerTotalSpeedStat.)
u32 GetStatusRailSpeedPercent(enum BattlerId battler)
{
    u32 percent = 100;

    if (GetStatusRailLevel(battler, RAIL_SOAK) != 0)
        percent = percent * SOAK_SPEED_PERCENT / 100;
    if (GetStatusRailLevel(battler, RAIL_CHILL) != 0) // Frozen / Frostbitten keep Chilled's effects
        percent = percent * STATUS_RAIL_SLOW_PERCENT / 100;
    if (GetStatusRailLevel(battler, RAIL_SLEEP) == 1)
        percent = percent * STATUS_RAIL_SLOW_PERCENT / 100;
    return percent;
}

// The Chill line and Drowsy each add a flat amount to every move / Dodge (not Recuperate).
u32 GetStatusRailStaminaSurcharge(enum BattlerId battler)
{
    u32 surcharge = 0;

    if (GetStatusRailLevel(battler, RAIL_CHILL) != 0)
        surcharge += STATUS_RAIL_STAMINA_SURCHARGE;
    if (GetStatusRailLevel(battler, RAIL_SLEEP) == 1)
        surcharge += STATUS_RAIL_STAMINA_SURCHARGE;
    return surcharge;
}

// Every level of the Electric line keeps Dazed's effects: its own moves are less accurate...
u32 GetStatusRailAccuracyPercent(enum BattlerId battler)
{
    return GetStatusRailLevel(battler, RAIL_ELECTRIC) != 0 ? DAZED_ACCURACY_PERCENT : 100;
}

// ...and moves aimed at it are more accurate.
u32 GetStatusRailIncomingAccuracyPercent(enum BattlerId battlerDef)
{
    return GetStatusRailLevel(battlerDef, RAIL_ELECTRIC) != 0 ? DAZED_INCOMING_ACCURACY_PERCENT : 100;
}

// The Poison line blocks Recuperate's heal and any heal of maxHP/16 or less (Leftovers, a weak drain, ...).
bool32 IsHealBlockedByStatusRails(enum BattlerId battler, u32 healAmount)
{
    ReconcileStatusRailsFromBridge(battler);
    return GetStatusRailLevel(battler, RAIL_POISON) != 0
        && healAmount <= max(GetNonDynamaxMaxHP(battler) / POISON_HEAL_BLOCK_DIVISOR, 1);
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
    case TYPE_ICE: // Water only eases a Burn through a Soak (TrySoakFromHit)
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

u32 TrySoakFromHit(enum BattlerId battler, enum Type moveType)
{
    if (moveType != TYPE_WATER
     || IS_BATTLER_OF_TYPE(battler, TYPE_WATER)
     || !RandomPercentage(RNG_STATUS_RAIL_SOAK, SOAK_CHANCE))
        return 0;

    ReconcileStatusRailsFromBridge(battler);
    if (GetStatusRailLevel(battler, RAIL_BURN) != 0) // the Soak takes a Burn stack off instead
        return LowerStatusRail(battler, RAIL_BURN) == 0 ? STRINGID_FRBURNDOUSED : STRINGID_FRBURNEASED;
    if (GetStatusRailLevel(battler, RAIL_SOAK) != 0) // already Soaked: start the count again
    {
        GetRail(battler, RAIL_SOAK)->counter = SOAK_DURATION;
        return 0;
    }
    ApplyStatusRail(battler, RAIL_SOAK, 1);
    return STRINGID_FRSOAKEDHIT;
}

void TryRenewChillFromHit(enum BattlerId battler, enum Type moveType)
{
    struct StatusRailEntry *entry = GetRail(battler, RAIL_CHILL);

    if (moveType == TYPE_ICE && entry->level != 0
     && !(entry->level >= 2 && entry->branch == CHILL_BRANCH_FROZEN)) // Frozen is never renewed
        entry->counter = CHILL_DURATION;
}

// Whether an attack may push battlerDef further down a line: the same immunities as a normal application
// (abilities, types, Safeguard, terrains, ...).
static bool32 CanAdvanceStatusRail(enum BattlerId battlerAtk, enum BattlerId battlerDef, enum StatusRail rail)
{
    enum Ability abilityAtk = GetBattlerAbility(battlerAtk);

    if (rail == RAIL_CONFUSION)
        return CanBeConfused(battlerAtk, battlerDef);
    if (IsSafeguardProtected(battlerAtk, battlerDef, abilityAtk))
        return FALSE;
    return CanSetNonVolatileStatus(battlerAtk, battlerDef, abilityAtk, GetBattlerAbility(battlerDef),
                                   rail == RAIL_SLEEP ? MOVE_EFFECT_SLEEP : MOVE_EFFECT_PARALYSIS, CHECK_TRIGGER);
}

bool32 TryShockedParalysisFromHit(enum BattlerId battlerAtk, enum BattlerId battler, enum Type moveType)
{
    if (moveType != TYPE_ELECTRIC
     || GetStatusRailLevel(battler, RAIL_ELECTRIC) != 2
     || !CanAdvanceStatusRail(battlerAtk, battler, RAIL_ELECTRIC)
     || !RandomPercentage(RNG_SHOCKED_PARALYSIS, SHOCKED_PARALYSIS_CHANCE))
        return FALSE;

    ApplyStatusRail(battler, RAIL_ELECTRIC, 3);
    return TRUE;
}

u32 TryCritStatusRailUpgrade(enum BattlerId battlerAtk, enum BattlerId battler, enum Type moveType)
{
    enum StatusRail rail;
    u32 stringId;

    if (moveType == TYPE_GHOST)
    {
        rail = RAIL_SLEEP;
        stringId = STRINGID_FRGHOSTNIGHTMARE;
    }
    else if (moveType == TYPE_PSYCHIC)
    {
        rail = RAIL_CONFUSION;
        stringId = STRINGID_FRPSYCHICFRIGHT;
    }
    else
    {
        return 0;
    }

    ReconcileStatusRailsFromBridge(battler);
    if (GetStatusRailLevel(battler, rail) == 0 || GetStatusRailLevel(battler, rail) >= 3
     || (gBattleStruct->statusRailsAppliedByMove[battler] & STATUS_RAIL_BIT(rail))
     || !CanAdvanceStatusRail(battlerAtk, battler, rail))
        return 0;
    RaiseStatusRailTo(battler, rail, 3, 0); // keeps the turns left
    return stringId;
}

u32 GetStatusRailAppliedStringId(enum StatusRail rail, u8 level, u8 branch)
{
    switch (rail)
    {
    case RAIL_POISON:
        return level >= 2 ? STRINGID_PKMNBADLYPOISONED : STRINGID_PKMNWASPOISONED;
    case RAIL_BURN:
        return STRINGID_PKMNWASBURNED;
    case RAIL_CHILL:
        if (level < 2)
            return STRINGID_FRCHILLED;
        return branch == CHILL_BRANCH_FROSTBITTEN ? STRINGID_PKMNGOTFROSTBITE : STRINGID_PKMNWASFROZEN;
    case RAIL_ELECTRIC:
        return level == 1 ? STRINGID_FRDAZED : level == 2 ? STRINGID_FRSHOCKED : STRINGID_PKMNWASPARALYZED;
    case RAIL_SLEEP:
        return level == 1 ? STRINGID_FRDROWSY : level == 2 ? STRINGID_PKMNFELLASLEEP : STRINGID_FRNIGHTMARE;
    case RAIL_CONFUSION:
        return level == 1 ? STRINGID_PKMNWASCONFUSED : level == 2 ? STRINGID_FRSTUPEFIED : STRINGID_FRFRIGHTENED;
    case RAIL_SOAK:
        return STRINGID_FRSOAKEDHIT;
    default:
        return 0;
    }
}

bool32 TrySleepRailValve(enum BattlerId battler, enum Type moveType)
{
    struct StatusRailEntry *entry = GetRail(battler, RAIL_SLEEP);

    ReconcileStatusRailsFromBridge(battler);
    if (entry->level == 0
     || moveType == TYPE_GHOST
     || !RandomPercentage(RNG_SLEEP_VALVE, SLEEP_VALVE_CHANCE))
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
