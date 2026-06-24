#ifndef GUARD_STATUS_RAIL_H
#define GUARD_STATUS_RAIL_H

// Fantasy Retold: multi-stage status "rails" (design.md §6). See enum StatusRail (constants/battle.h)
// and struct StatusRailEntry (battle.h). Rail state lives in struct PartyState and is the source of
// truth for rail mechanics (level, per-rail counter, Lv1-volatile / Lv2+-persistent lifetime). The
// legacy STATUS1_* bits are kept only as a read-mirror (SyncStatusRailBridge) so existing status
// icons / animations / messages / AI keep working untouched.

u8 GetStatusRailLevel(enum BattlerId battler, enum StatusRail rail);
u8 GetStatusRailCounter(enum BattlerId battler, enum StatusRail rail);
void SetStatusRailCounter(enum BattlerId battler, enum StatusRail rail, u8 counter);

// Apply/advance a rail: newLevel = clamp(max(level + 1, minLevel), 1, railMax). minLevel is 2 for a
// status-move/primary application, 1 for an attacking-move secondary / ability / protect effect.
void ApplyStatusRail(enum BattlerId battler, enum StatusRail rail, u8 minLevel);

// Clear every rail currently at Level 1 (volatile). Called on switch-out.
void ClearVolatileStatusRails(enum BattlerId battler);

// Mirror rail state into the legacy status1 bits (rail -> status1).
void SyncStatusRailBridge(enum BattlerId battler);

// Seed/clear rails from the legacy status1 bits (status1 -> rail) so external setters/curers that
// only touch status1 (Toxic Orb, Synchronize, Toxic Spikes, Refresh, Natural Cure, full heal, a mon
// entering battle already statused, ...) stay consistent with the rail. Called at defined sync points.
void ReconcileStatusRailsFromBridge(enum BattlerId battler);

// Recuperate: clear one randomly chosen Level 1 rail (Lv2+ is immune). Returns TRUE if one was purged.
bool32 PurgeRandomLevel1StatusRail(enum BattlerId battler);

#endif // GUARD_STATUS_RAIL_H
