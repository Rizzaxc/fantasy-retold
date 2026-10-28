#ifndef GUARD_STATUS_RAIL_H
#define GUARD_STATUS_RAIL_H

// Fantasy Retold: multi-stage status "rails" (design.md §6). See enum StatusRail (constants/battle.h)
// and struct StatusRailEntry (battle.h). Rail state lives in struct PartyState and is the source of
// truth for rail mechanics (level, per-rail counter, Lv1-volatile / Lv2+-persistent lifetime). The
// legacy STATUS1_* bits (and volatiles.confusionTimer) are kept as a mirror (SyncStatusRailBridge) so
// existing animations / messages / AI / cures keep working. Unlike vanilla, several lines can be active
// at once, so several STATUS1_* bits can be set together.
//
// Every function that changes a rail first reconciles from the mirror (so a status written straight to
// status1 by vanilla code, e.g. Toxic Orb, Rest or a cure, is picked up) and then syncs back.

u8 GetStatusRailMaxLevel(enum StatusRail rail);
u8 GetStatusRailLevel(enum BattlerId battler, enum StatusRail rail);
u8 GetStatusRailCounter(enum BattlerId battler, enum StatusRail rail);
u8 GetStatusRailBranch(enum BattlerId battler, enum StatusRail rail);
void SetStatusRailCounter(enum BattlerId battler, enum StatusRail rail, u8 counter);

// Apply/advance a rail: newLevel = clamp(max(level + 1, minLevel), 1, railMax). minLevel is 2 for a
// status-move/primary application, 1 for an attacking-move secondary / ability / protect effect.
// RAIL_CHILL always starts at Lv1 (its Lv2 forms can't be applied directly). Returns the new level.
u8 ApplyStatusRail(enum BattlerId battler, enum StatusRail rail, u8 minLevel);

// Drop a rail one level (Water/Ice on Burn, Ground on the Electric line). Returns the new level.
u8 LowerStatusRail(enum BattlerId battler, enum StatusRail rail);
void ClearStatusRail(enum BattlerId battler, enum StatusRail rail);
void ClearAllStatusRails(enum BattlerId battler);

// Clear every rail currently at Level 1 (volatile). Called on switch-out.
void ClearVolatileStatusRails(enum BattlerId battler);

// Mirror rail state into the legacy status1 bits and confusionTimer (rail -> mirror).
void SyncStatusRailBridge(enum BattlerId battler);

// Seed/clear rails from the mirror (mirror -> rail) so external setters/curers that only touch status1
// or confusionTimer stay consistent with the rail.
void ReconcileStatusRailsFromBridge(enum BattlerId battler);

// Tell the controller about the new status1 mirror (keeps the party data in step).
void EmitStatusRailMirror(enum BattlerId battler);

// Recuperate: clear one randomly chosen Level 1 rail (Lv2+ is immune). Returns TRUE if one was purged.
bool32 PurgeRandomLevel1StatusRail(enum BattlerId battler);

// Effects of the active rails.
u32 GetStatusRailSpeedPercent(enum BattlerId battler);
u32 GetStatusRailStaminaSurcharge(enum BattlerId battler);
u32 GetStatusRailAccuracyPercent(enum BattlerId battler);
uq4_12_t GetStatusRailIncomingDamageModifier(enum BattlerId battlerDef, enum Type moveType);
uq4_12_t GetBurnRailPhysicalModifier(enum BattlerId battler);
bool32 IsBurnBlockedByStatusRails(enum BattlerId battler);

// End of turn: counts down SOAK / CHILL / SLEEP / CONFUSION and clears the ones that run out. Returns the
// rails that ended, as a bitmask of (1 << rail).
u32 TickStatusRailTimers(enum BattlerId battler, bool32 earlyBird);
u32 GetStatusRailEndedStringId(enum StatusRail rail);

// After a damaging move: the type interaction for a battler that used it or was hit by it (Fire thaws the
// Chill line, Water/Ice ease a Burn, Ground drains the Electric line). Returns the message, or 0.
u32 ApplyStatusRailTypeInteraction(enum BattlerId battler, enum Type moveType);
// A damaging Water hit may Soak the target. Returns TRUE if it did.
bool32 TrySoakFromHit(enum BattlerId battler, enum Type moveType);
// Sleep valve: a direct damaging hit takes a turn off the Sleep line. Returns TRUE if that woke the battler.
bool32 TrySleepRailValve(enum BattlerId battler);

#endif // GUARD_STATUS_RAIL_H
