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

#define STATUS_RAIL_BIT(rail)        (1u << (rail))
#define STATUS_RAILS_ALL             ((1u << NUM_STATUS_RAILS) - 1)
#define STATUS_RAILS_NON_VOLATILE    (STATUS_RAILS_ALL & ~STATUS_RAIL_BIT(RAIL_CONFUSION))

u8 GetStatusRailMaxLevel(enum StatusRail rail);
u8 GetStatusRailLevel(enum BattlerId battler, enum StatusRail rail);
u8 GetStatusRailCounter(enum BattlerId battler, enum StatusRail rail);
u8 GetStatusRailBranch(enum BattlerId battler, enum StatusRail rail);
void SetStatusRailCounter(enum BattlerId battler, enum StatusRail rail, u8 counter);

// Apply/advance a rail: newLevel = clamp(max(level + 1, minLevel), 1, railMax). minLevel is 2 for a
// status-move/primary application (see below for stacking), 1 for an attacking-move secondary / ability / protect effect.
// RAIL_CHILL always starts at Lv1 (its Lv2 forms can't be applied directly). Returns the new level.
// A direct application (minLevel 2) on a line that is already there stacks: Lv1 + Lv2 -> Lv3. With
// STATUS_RAIL_APPLY_FLOOR ORed in, minLevel is only a floor (a secondary whose >100% excess rolled): max(level + 1, 2).
#define STATUS_RAIL_APPLY_FLOOR 0x80
u8 ApplyStatusRail(enum BattlerId battler, enum StatusRail rail, u8 minLevel);
// ApplyStatusRail, unless this is a Synchronize passing a status back: then the rail jumps to the
// synchronizer's level.
u8 ApplyStatusRailOrSynchronize(enum BattlerId battler, enum StatusRail rail, u8 minLevel);
// Raise a rail straight to level (Synchronize, crit upgrades); never lowers it. branch is the Chill Lv2 form.
u8 RaiseStatusRailTo(enum BattlerId battler, enum StatusRail rail, u8 level, u8 branch);

// Drop a rail one level (Water/Ice on Burn, Ground on the Electric line). Returns the new level.
u8 LowerStatusRail(enum BattlerId battler, enum StatusRail rail);
void ClearStatusRail(enum BattlerId battler, enum StatusRail rail);
void ClearAllStatusRails(enum BattlerId battler);

// Full cures (Full Heal, Heal Bell, Natural Cure): every rail below Lv3 clears, a Lv3 drops to Lv1. That Lv1 is
// volatile as usual, except with keepOnSwitch (Natural Cure, which cures on switch-out): then it survives that
// switch-out. The party form works on a benched mon's rails.
void CureAllStatusRails(enum BattlerId battler, bool32 keepOnSwitch);
bool32 CurePartyStatusRails(struct StatusRailEntry *rails, bool32 keepOnSwitch); // TRUE if any line was active
// Other status cures (Shed Skin, Hydration, Healer, Purify, Jungle Healing, Smelling Salts, ...): the lines in
// railMask clear below Lv3, a Lv3 drops to a volatile Lv1. Returns TRUE if any of them was active.
bool32 CureStatusRails(enum BattlerId battler, u32 railMask);
bool32 HasAnyStatusRail(enum BattlerId battler, u32 railMask);
// B_MSG_CURED_* for the first active line in railMask (B_MSG_CURED_PROBLEM for Soaked alone).
u32 GetStatusRailCureMessage(enum BattlerId battler, u32 railMask);
// "Has a status" for Guts, Facade, Hex, catch bonuses, ...: Lv2+ of any line, or Soaked.
bool32 HasStatusRailCondition(enum BattlerId battler);
// The same for the lines behind some vanilla status1 bits (Lv2+ of those lines; STATUS1_ANY = any line or Soaked).
bool32 HasStatusRailConditionForStatus1(enum BattlerId battler, u32 status1Mask);
enum StatusRail GetStatusRailFromStatus1(u32 status1);
// The party status1 to keep after battle: the mirror and saved-level bits of the Lv2+ lines (Lv1 drops).
u32 GetStatusRailPartyStatusAfterBattle(const struct StatusRailEntry *rails);
// The STATUS1_SAVED_* bits a cure with healMask (the vanilla bits it clears) also takes off a party mon's status.
u32 GetStatusRailSavedBitsToCure(u32 healMask);
// On a mon's first send-out: raise the rails from the STATUS1_SAVED_* bits in its incoming status1 (after
// ReconcileStatusRailsFromBridge, before SyncStatusRailBridge strips them).
void SeedStatusRailsFromSavedStatus(enum BattlerId battler);
// A cleansing berry's line: only acts on Lv2+ (Lv3 drops to a volatile Lv1). Returns TRUE if it did.
bool32 CureStatusRailWithBerry(enum BattlerId battler, enum StatusRail rail);

// Clear every rail currently at Level 1 (volatile), except a Natural Cure leftover (kept this once). Called on
// switch-out.
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
u32 GetStatusRailIncomingAccuracyPercent(enum BattlerId battlerDef);
bool32 IsHealBlockedByStatusRails(enum BattlerId battler, u32 healAmount);
uq4_12_t GetStatusRailIncomingDamageModifier(enum BattlerId battlerDef, enum Type moveType);
uq4_12_t GetBurnRailPhysicalModifier(enum BattlerId battler);
bool32 IsBurnBlockedByStatusRails(enum BattlerId battler);

// End of turn: counts down SOAK / CHILL / SLEEP / CONFUSION and clears the ones that run out. Returns the
// rails that ended, as a bitmask of (1 << rail).
u32 TickStatusRailTimers(enum BattlerId battler, bool32 earlyBird);
u32 GetStatusRailEndedStringId(enum StatusRail rail);

// After a damaging move: the type interaction for a battler that used it or was hit by it (Fire thaws the
// Chill line, Ice eases a Burn, Ground drains the Electric line). Returns the message, or 0.
u32 ApplyStatusRailTypeInteraction(enum BattlerId battler, enum Type moveType);
// A damaging Water hit may Soak the target (or, on a burned target, take a Burn stack off). Returns the message, or 0.
u32 TrySoakFromHit(enum BattlerId battler, enum Type moveType);
// Ice damage renews the Chill line's timer (not Frozen's).
void TryRenewChillFromHit(enum BattlerId battler, enum Type moveType);
// Both respect the usual status immunities (abilities, Safeguard, terrains).
// An Electric hit may paralyze a Shocked battler. Returns TRUE if it did.
bool32 TryShockedParalysisFromHit(enum BattlerId battlerAtk, enum BattlerId battler, enum Type moveType);
// A critical Ghost hit turns Drowsy / Asleep into Nightmare, a critical Psychic hit turns Confused / Stupefied
// into Frightened. Returns the message, or 0.
u32 TryCritStatusRailUpgrade(enum BattlerId battlerAtk, enum BattlerId battler, enum Type moveType);
// The "X got <status>!" message (B_EFF_NAME) for a line reaching level.
u32 GetStatusRailAppliedStringId(enum StatusRail rail, u8 level, u8 branch);
// Sleep valve: a direct non-Ghost damaging hit may take a turn off the Sleep line. Returns TRUE if that woke
// the battler.
bool32 TrySleepRailValve(enum BattlerId battler, enum Type moveType);

#endif // GUARD_STATUS_RAIL_H
