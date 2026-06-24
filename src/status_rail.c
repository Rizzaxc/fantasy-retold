#include "global.h"
#include "battle.h"
#include "battle_util.h"
#include "random.h"
#include "status_rail.h"
#include "constants/battle.h"

// Fantasy Retold: multi-stage status "rails" (design.md §6). This module owns all rail state and the
// two-way bridge to the legacy status1 bits. Only the Poison -> Toxic line (RAIL_POISON) is wired in
// this vertical slice; the remaining lines plug in here behind the same API.

// Highest level a rail can reach.
static u8 GetRailMaxLevel(enum StatusRail rail)
{
    switch (rail)
    {
    case RAIL_POISON: return 2; // Poison (Lv1) -> Toxic (Lv2)
    default:          return 1;
    }
}

u8 GetStatusRailLevel(enum BattlerId battler, enum StatusRail rail)
{
    return GetBattlerPartyState(battler)->rails[rail].level;
}

u8 GetStatusRailCounter(enum BattlerId battler, enum StatusRail rail)
{
    return GetBattlerPartyState(battler)->rails[rail].counter;
}

void SetStatusRailCounter(enum BattlerId battler, enum StatusRail rail, u8 counter)
{
    GetBattlerPartyState(battler)->rails[rail].counter = counter;
}

void ApplyStatusRail(enum BattlerId battler, enum StatusRail rail, u8 minLevel)
{
    struct StatusRailEntry *entry = &GetBattlerPartyState(battler)->rails[rail];
    u8 maxLevel = GetRailMaxLevel(rail);
    u8 newLevel = entry->level + 1; // reapplication advances one step...

    if (newLevel < minLevel) // ...but a status-move application starts no lower than minLevel.
        newLevel = minLevel;
    if (newLevel > maxLevel)
        newLevel = maxLevel;

    entry->level = newLevel;
    entry->counter = 0; // a fresh application / advance resets the rail-specific counter (e.g. Toxic escalation)

    SyncStatusRailBridge(battler);
}

void ClearVolatileStatusRails(enum BattlerId battler)
{
    struct PartyState *ps = GetBattlerPartyState(battler);
    enum StatusRail rail;

    for (rail = 0; rail < NUM_STATUS_RAILS; rail++)
    {
        if (ps->rails[rail].level == 1)
        {
            ps->rails[rail].level = 0;
            ps->rails[rail].counter = 0;
        }
    }
    SyncStatusRailBridge(battler);
}

void SyncStatusRailBridge(enum BattlerId battler)
{
    struct PartyState *ps = GetBattlerPartyState(battler);
    u32 status1 = gBattleMons[battler].status1;
    u8 poisonLevel = ps->rails[RAIL_POISON].level;

    // RAIL_POISON mirror: Lv1 -> STATUS1_POISON; Lv2 (Toxic) -> STATUS1_TOXIC_POISON (+ counter mirror
    // so the existing toxic UI/AI keep reading a sensible value); Lv0 -> clear both.
    status1 &= ~(STATUS1_POISON | STATUS1_TOXIC_POISON | STATUS1_TOXIC_COUNTER);
    if (poisonLevel == 1)
        status1 |= STATUS1_POISON;
    else if (poisonLevel >= 2)
        status1 |= STATUS1_TOXIC_POISON | STATUS1_TOXIC_TURN(min(ps->rails[RAIL_POISON].counter, 15));

    gBattleMons[battler].status1 = status1;
}

void ReconcileStatusRailsFromBridge(enum BattlerId battler)
{
    struct PartyState *ps = GetBattlerPartyState(battler);
    u32 status1 = gBattleMons[battler].status1;

    // RAIL_POISON: keep the rail consistent with whatever external code last wrote to status1.
    if (!(status1 & (STATUS1_POISON | STATUS1_TOXIC_POISON)))
    {
        // Cured (Refresh / Natural Cure / full heal / never applied) -> drop the rail.
        ps->rails[RAIL_POISON].level = 0;
        ps->rails[RAIL_POISON].counter = 0;
    }
    else if (status1 & STATUS1_TOXIC_POISON)
    {
        // Toxic from any source (Toxic Orb, Toxic Spikes, ...) -> at least Lv2; seed the escalation counter.
        if (ps->rails[RAIL_POISON].level < 2)
        {
            ps->rails[RAIL_POISON].level = 2;
            ps->rails[RAIL_POISON].counter = (status1 & STATUS1_TOXIC_COUNTER) >> 8;
        }
    }
    else if (ps->rails[RAIL_POISON].level < 1) // STATUS1_POISON only
    {
        ps->rails[RAIL_POISON].level = 1;
    }
}

bool32 PurgeRandomLevel1StatusRail(enum BattlerId battler)
{
    struct PartyState *ps = GetBattlerPartyState(battler);
    enum StatusRail rail, candidates[NUM_STATUS_RAILS];
    u32 count = 0;

    for (rail = 0; rail < NUM_STATUS_RAILS; rail++)
    {
        if (ps->rails[rail].level == 1)
            candidates[count++] = rail;
    }
    if (count == 0)
        return FALSE;

    rail = candidates[RandomUniform(RNG_STATUS_RAIL_CLEANSE, 0, count - 1)];
    ps->rails[rail].level = 0;
    ps->rails[rail].counter = 0;
    SyncStatusRailBridge(battler);
    return TRUE;
}
